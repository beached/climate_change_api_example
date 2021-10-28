
#include <daw/curl_wrapper.h>
#include <daw/daw_logic.h>
#include <daw/daw_move.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/json/daw_json_link.h>
#include <daw/parallel/daw_latch.h>
#include <daw/utf8.h>

#define CROW_MAIN

#include <atomic>
#include <chrono>
#include <crow.h>
#include <future>
#include <gumbo.h>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <vector>

struct Newspaper {
	std::string name;
	std::string address;
	std::string base;
};

static const Newspaper newspapers[] = {
  { "cityam",
    "https://www.cityam.com/"
    "london-must-become-a-world-leader-on-climate-change-action/",
    "" },
  { "thetimes", "https://www.thetimes.co.uk/environment/climate-change", "" },
  {
    "guardian",
    "https://www.theguardian.com/environment/climate-crisis",
    "",
  },
  {
    "telegraph",
    "https://www.telegraph.co.uk/climate-change",
    "https://www.telegraph.co.uk",
  },
  {
    "nyt",
    "https://www.nytimes.com/international/section/climate",
    "",
  },
  {
    "latimes",
    "https://www.latimes.com/environment",
    "",
  },
  {
    "smh",
    "https://www.smh.com.au/environment/climate-change",
    "https://www.smh.com.au",
  },
  {
    "un",
    "https://www.un.org/climatechange",
    "",
  },
  {
    "bbc",
    "https://www.bbc.co.uk/news/science_and_environment",
    "https://www.bbc.co.uk",
  },
  { "es",
    "https://www.standard.co.uk/topic/climate-change",
    "https://www.standard.co.uk" },
  { "sun", "https://www.thesun.co.uk/topic/climate-change-environment/", "" },
  { "dm",
    "https://www.dailymail.co.uk/news/climate_change_global_warming/index.html",
    "" },
  { "nyp", "https://nypost.com/tag/climate-change/", "" } };

struct GumboDeleter {
	constexpr GumboDeleter( ) = default;

	inline void operator( )( GumboOutput *output ) const {
		if( not output ) {
			return;
		}
		gumbo_destroy_output( &kGumboDefaultOptions, output );
	}
};

struct GumboHandle : std::unique_ptr<GumboOutput, GumboDeleter> {
	using base = std::unique_ptr<GumboOutput, GumboDeleter>;
	inline GumboHandle( GumboOutput *ptr )
	  : base( ptr ) {}
};

struct Url {
	std::string uri;
	std::string title;
	std::string source;
};

template<typename T, typename Retriever>
struct CachedValue : private Retriever {
	static_assert( std::is_invocable_r_v<T, Retriever> );
	using type = T;
	using retriever_t = Retriever;

private:
	struct state_t {
		std::chrono::seconds m_ttl;
		std::optional<type> m_value{ };
		std::optional<std::chrono::time_point<std::chrono::system_clock>>
		  m_time_of_retrieval{ };
		std::mutex m_mut = std::mutex{ };
		std::atomic_bool m_working = false;
		daw::latch m_latch = daw::latch( 0 );

		inline state_t( std::chrono::seconds ttl )
		  : m_ttl( ttl ) {}
	};
	std::unique_ptr<state_t> m_state;

public:
	CachedValue( retriever_t const &retriever )
	  : Retriever( retriever )
	  , m_state( std::make_unique<state_t>( std::chrono::seconds( 3600 ) ) ) {}

	CachedValue( retriever_t const &retriever, std::chrono::seconds ttl )
	  : retriever_t( retriever )
	  , m_state( std::make_unique<state_t>( ttl ) ) {}

	void clear( ) {
		auto const lck = std::unique_lock( m_state->m_mut );
		m_state->m_value.reset( );
		m_state->m_time_of_retrieval.reset( );
	}

	std::future<type> get( ) {
		auto const lck = std::unique_lock( m_state->m_mut );
		(void)lck;
		if( m_state->m_time_of_retrieval ) {
			// We have previously retrieved data
			if( m_state->m_working.load( ) or
			    std::chrono::system_clock::now( ) <
			      ( *m_state->m_time_of_retrieval + m_state->m_ttl ) ) {
				// We are updating existing data or it has not yet expired
				return std::async( std::launch::deferred,
				                   [value = *m_state->m_value] { return value; } );
			}
		}
		if( m_state->m_working.load( ) ) {
			// We have not yet retrieved data and another thread has started the load.
			// Wait for it and then return the new data
			return std::async( std::launch::async, [&] {
				m_state->m_latch.wait( );
				auto const local_lck = std::unique_lock( m_state->m_mut );
				(void)local_lck;
				auto result = *m_state->m_value;
				return result;
			} );
		}
		// No data has yet been retrieved and no other thread is loading it
		m_state->m_latch.add_notifier( );
		m_state->m_working = true;
		return std::async( std::launch::async, [&] {
			type new_value = ( *this )( );
			{
				auto const local_lck = std::unique_lock( m_state->m_mut );
				(void)local_lck;
				m_state->m_value = new_value;
				m_state->m_time_of_retrieval = std::chrono::system_clock::now( );
				m_state->m_working = false;
			}
			m_state->m_latch.notify( );
			return new_value;
		} );
	}
};
template<typename Ret>
CachedValue( Ret ) -> CachedValue<std::invoke_result_t<Ret>, Ret>;

template<typename Ret>
CachedValue( Ret, std::chrono::seconds )
  -> CachedValue<std::invoke_result_t<Ret>, Ret>;

namespace daw::json {
	template<>
	struct json_data_contract<Url> {
		static constexpr char const uri[] = "uri";
		static constexpr char const title[] = "title";
		static constexpr char const source[] = "source";
		using type = json_member_list<json_string<uri>,
		                              json_string<title>,
		                              json_string<source>>;

		static inline auto to_json_data( Url const &u ) {
			return std::forward_as_tuple( u.uri, u.title, u.source );
		}
	};
} // namespace daw::json

daw::string_view get_tag_text( GumboNode *node ) {
	if( not node or node->type != GUMBO_NODE_ELEMENT ) {
		return { };
	}
	GumboVector *children = &node->v.element.children;
	if( children ) {
		for( unsigned i = 0; i < children->length; ++i ) {
			auto *child = static_cast<GumboNode *>( children->data[i] );
			if( child->type == GUMBO_NODE_TEXT ) {
				return { child->v.text.text };
			}
		}
	}
	return { };
}

template<typename StringView>
constexpr bool Contains( StringView const &sv, daw::string_view key ) {
	auto haystack_first = daw::utf8::unchecked::iterator( std::data( sv ) );
	auto haystack_last = daw::utf8::unchecked::iterator( daw::data_end( sv ) );
	auto needle_first = daw::utf8::unchecked::iterator( std::data( key ) );
	auto needle_last = daw::utf8::unchecked::iterator( daw::data_end( key ) );
	return std::search( haystack_first,
	                    haystack_last,
	                    needle_first,
	                    needle_last,
	                    []( std::uint32_t lhs, std::uint32_t rhs ) {
		                    return lhs == rhs or ( lhs < 128U and rhs < 128U and
		                                           ( lhs | 32U ) == ( rhs | 32U ) );
	                    } ) != haystack_last;
}

enum class filter_action : int { exclude = 0, include = 1, stop = -1 };

template<typename Filter, typename Callback>
static void
filter_gumbo_nodes( GumboNode *root, Filter filter, Callback onEach ) {
	if( not root ) {
		return;
	}
	auto to_visit = std::vector<GumboNode *>( );
	to_visit.reserve( 1024UL );
	to_visit.push_back( root );
	while( not to_visit.empty( ) ) {
		auto cur_node = to_visit.back( );
		to_visit.pop_back( );
		auto const fres = static_cast<filter_action>( filter( cur_node ) );
		switch( fres ) {
		case filter_action::include:
			(void)onEach( cur_node );
			break;
		case filter_action::stop:
			return;
		case filter_action::exclude:
		default:
			break;
		}
		if( cur_node->type == GUMBO_NODE_ELEMENT ) {
			GumboVector *children = &cur_node->v.element.children;
			if( children ) {
				for( unsigned i = 0; i < children->length; ++i ) {
					auto *child = static_cast<GumboNode *>( children->data[i] );
					if( not child ) {
						continue;
					}
					to_visit.push_back( child );
				}
			}
		}
	}
}

template<typename Callback>
static std::vector<Url> search_for_links( GumboNode *node, Callback onEach ) {
	auto result = std::vector<Url>{ };
	if( not node or node->type != GUMBO_NODE_ELEMENT ) {
		return result;
	}
	filter_gumbo_nodes(
	  node,
	  []( GumboNode *n ) {
		  return n->type == GUMBO_NODE_ELEMENT and n->v.element.tag == GUMBO_TAG_A;
	  },
	  [&]( GumboNode *n ) {
		  if( GumboAttribute *href =
		        gumbo_get_attribute( &n->v.element.attributes, "href" );
		      href != nullptr ) {
			  auto title = daw::parser::trim( get_tag_text( n ) );
			  auto uri = daw::string_view( href->value );
			  if( daw::nsc_or( uri.empty( ),
			                   title.empty( ),
			                   not ::Contains( title, "climate" ),
			                   not uri.starts_with( "http" ),
			                   uri.find( "climate" ) == daw::string_view::npos ) ) {
				  return;
			  }
			  (void)onEach( uri, title );
		  }
	  } );
	return result;
}

struct HtmlCache {
	using func_t = std::function<std::vector<Url>( )>;
	using cache_t = CachedValue<std::vector<Url>, func_t>;
	std::unordered_map<std::string, cache_t> operator( )( ) const {
		auto result = std::unordered_map<std::string, cache_t>{ };
		result.reserve( std::size( newspapers ) );
		for( Newspaper const &n : newspapers ) {
			result.emplace(
			  n.name,
			  cache_t{ func_t{ [n]( ) {
				  auto curl = daw::curl_wrapper( );
				  auto const html = curl.get_string( n.address );
				  GumboHandle output = gumbo_parse_with_options( &kGumboDefaultOptions,
				                                                 html.c_str( ),
				                                                 html.size( ) );
				  std::vector<Url> r{ };
				  r.reserve( 128 );
				  search_for_links(
				    output->root,
				    [&]( std::string uri, std::string title ) {
					    uri = n.base + uri;
					    r.push_back( Url{ DAW_MOVE( uri ), DAW_MOVE( title ), n.name } );
				    } );
				  std::sort( r.begin( ),
				             r.end( ),
				             []( auto const &lhs, auto const &rhs ) {
					             return lhs.uri < rhs.uri;
				             } );
				  r.erase( std::unique( r.begin( ),
				                        r.end( ),
				                        []( auto const &lhs, auto const &rhs ) {
					                        return lhs.uri == rhs.uri;
				                        } ),
				           r.end( ) );
				  return r;
			  } } } );
		}
		return result;
	}
};

int main( ) {
	static auto html_cache = HtmlCache{ }( );
	auto app = crow::SimpleApp{ };
	CROW_ROUTE( app, "/news/" ).methods( crow::HTTPMethod::GET )( []( ) {
		std::vector<std::future<std::vector<Url>>> urls_fut{ };
		for( auto &c : html_cache ) {
			urls_fut.push_back( c.second.get( ) );
		}
		std::vector<Url> all_urls{ };
		for( auto &f : urls_fut ) {
			auto u = f.get( );
			all_urls.insert( all_urls.end( ), u.begin( ), u.end( ) );
		}
		auto resp = crow::response( daw::json::to_json( all_urls ) );
		resp.add_header( "Content-Type", "application/json" );
		return resp;
	} );
	CROW_ROUTE( app, "/news/<string>" )
	  .methods( crow::HTTPMethod::GET )( []( std::string which_source ) {
		  auto it = html_cache.find( which_source );
		  if( it == html_cache.end( ) ) {
			  return crow::response( 404U, "Unable to find data" );
		  }
		  auto resp =
		    crow::response( daw::json::to_json( it->second.get( ).get( ) ) );
		  resp.add_header( "Content-Type", "application/json" );
		  return resp;
	  } );
	app.port( 8080 ).multithreaded( ).run( );
}
