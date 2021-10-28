
#include <daw/curl_wrapper.h>
#include <daw/daw_logic.h>
#include <daw/daw_move.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/daw_visit.h>
#include <daw/json/daw_json_link.h>
#include <daw/utf8.h>

#define CROW_MAIN

#include <chrono>
#include <crow.h>
#include <future>
#include <gumbo.h>
#include <iostream>
#include <mutex>
#include <tuple>

struct GumboDeleter {
	constexpr GumboDeleter( ) = default;

	inline void operator( )( GumboOutput *output ) const {
		if( not output ) {
			return;
		}
		gumbo_destroy_output( &kGumboDefaultOptions, output );
	}
};

struct GumboHandle : std::shared_ptr<GumboOutput> {
	inline GumboHandle( GumboOutput *ptr )
	  : std::shared_ptr<GumboOutput>( ptr, GumboDeleter{ } ) {}
};

struct Url {
	std::string uri;
	std::string title;
};

template<typename T, typename Retriever>
struct CachedValue : private Retriever {
	static_assert( std::is_invocable_r_v<T, Retriever> );
	using type = T;
	using retriever_t = Retriever;

private:
	std::chrono::seconds m_ttl;
	std::optional<type> m_value{ };
	std::optional<std::chrono::time_point<std::chrono::system_clock>>
	  m_time_of_retrieval{ };
	std::mutex m_mut{ };

public:
	CachedValue( retriever_t const &retriever )
	  : Retriever( retriever )
	  , m_ttl( std::chrono::seconds( 3600 ) ) {}

	CachedValue( retriever_t const &retriever, std::chrono::seconds ttl )
	  : retriever_t( retriever )
	  , m_ttl( ttl ) {}

	void clear( ) {
		auto const lck = std::unique_lock( m_mut );
		m_value.reset( );
		m_time_of_retrieval.reset( );
	}

	std::future<type> get( ) {
		auto const lck = std::unique_lock( m_mut );
		if( m_time_of_retrieval and
		    std::chrono::system_clock::now( ) < ( *m_time_of_retrieval + m_ttl ) ) {
			return std::async( std::launch::deferred,
			                   [value = *m_value] { return value; } );
		}
		return std::async( std::launch::async, [&] {
			type new_value = ( *this )( );
			auto const lck = std::unique_lock( m_mut );
			m_value = new_value;
			m_time_of_retrieval = std::chrono::system_clock::now( );
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
		using type = json_member_list<json_string<uri>, json_string<title>>;

		static inline auto to_json_data( Url const &u ) {
			return std::forward_as_tuple( u.uri, u.title );
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
			  (void)onEach( Url{ uri, title } );
		  }
	  } );
	return result;
}

int main( ) {
	auto app = crow::SimpleApp{ };
	CROW_ROUTE( app, "/news" )
	  .methods( crow::HTTPMethod::GET )(
	    []( crow::request const &req, crow::response &resp ) {
		    std::cout << "Request for " << req.url
		              << " from: " << req.remoteIpAddress << '\n';
		    static auto html_cache = CachedValue( [] {
			    auto curl = daw::curl_wrapper( );
			    auto const html = curl.get_string(
			      "https://www.theguardian.com/environment/climate-crisis" );
			    GumboHandle output = gumbo_parse_with_options( &kGumboDefaultOptions,
			                                                   html.c_str( ),
			                                                   html.size( ) );
			    std::vector<Url> result{ };
			    result.reserve( 128 );
			    search_for_links( output->root, [&]( Url &&url ) {
				    result.push_back( DAW_MOVE( url ) );
			    } );
			    return result;
		    } );
		    auto urls_fut = html_cache.get( );
		    resp.add_header( "Content-Type", "application/json" );
		    resp.body = daw::json::to_json( urls_fut.get( ) );
		    resp.end( );
	    } );

	app.port( 8080 ).multithreaded( ).run( );
}
