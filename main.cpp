// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#include "cached_value.h"
#include "newspaper.h"

#include <daw/curl_wrapper.h>
#include <daw/daw_logic.h>
#include <daw/daw_move.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/json/daw_json_link.h>
#include <daw/utf8.h>

#define CROW_MAIN

#include <crow.h>
#include <future>
#include <gumbo.h>
#include <tuple>
#include <unordered_map>
#include <vector>

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
	using cache_t = daw::CachedValue<std::vector<Url>, func_t>;
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
