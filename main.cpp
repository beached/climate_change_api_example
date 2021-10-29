// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#include "cached_value.h"
#include "gumbo_handle.h"
#include "gumbo_node_iterator.h"
#include "gumbo_vector_iterator.h"
#include "newspaper.h"

#include <daw/curl_wrapper.h>
#include <daw/daw_logic.h>
#include <daw/daw_move.h>
#include <daw/daw_not_null.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/json/daw_json_link.h>
#include <daw/utf8.h>

#define CROW_MAIN

#include <crow.h>
#include <future>
#include <gumbo.h>
#include <iterator>
#include <tuple>
#include <unordered_map>
#include <vector>

struct Url {
	std::string uri;
	std::string title;
	std::string source;

	friend inline bool operator==( Url const &lhs, Url const &rhs ) {
		return lhs.uri == rhs.uri;
	}

	friend inline bool operator<( Url const &lhs, Url const &rhs ) {
		return lhs.uri < rhs.uri;
	}
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

daw::string_view get_tag_text( daw::not_null<GumboNode *> node ) {
	if( node->type != GUMBO_NODE_ELEMENT ) {
		return { };
	}
	for( auto child :
	     daw::gumbo::GumboVectorIterator( node->v.element.children ) ) {
		if( child->type == GUMBO_NODE_TEXT ) {
			return { child->v.text.text };
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
		                    // Attempt to do an inexpensive case-insensitive match
		                    return lhs == rhs or ( lhs < 128U and rhs < 128U and
		                                           ( lhs | 32U ) == ( rhs | 32U ) );
	                    } ) != haystack_last;
}

enum class filter_action : int { exclude = 0, include = 1, stop = -1 };

template<typename Filter, typename Callback>
static void filter_gumbo_nodes( daw::not_null<GumboNode *> root,
                                Filter filter,
                                Callback onEach ) {
	auto to_visit = std::vector<daw::not_null<GumboNode *>>( );
	to_visit.reserve( 1024UL );
	to_visit.push_back( root );
	while( not to_visit.empty( ) ) {
		auto cur_node = to_visit.back( );
		to_visit.pop_back( );
		auto const fres = static_cast<filter_action>( filter( cur_node.get( ) ) );
		switch( fres ) {
		case filter_action::include:
			(void)onEach( cur_node.get( ) );
			break;
		case filter_action::stop:
			return;
		case filter_action::exclude:
		default:
			break;
		}
		if( cur_node->type == GUMBO_NODE_ELEMENT ) {
			auto it = daw::gumbo::GumboVectorIterator( cur_node->v.element.children );
			to_visit.insert( to_visit.end( ), it, std::end( it ) );
		}
	}
}

template<typename Callback>
static void search_for_links( GumboNode *root_node, Callback onEach ) {
	auto first = daw::gumbo::gumbo_node_iterator_t( root_node );
	auto const last = daw::gumbo::gumbo_node_iterator_t( );
	auto last_node = daw::gumbo::gumbo_node_iterator_t( );
	while( first != last ) {
		daw::not_null<GumboNode *> node = &( *first );
		if( node->type != GUMBO_NODE_ELEMENT or
		    node->v.element.tag != GUMBO_TAG_A ) {
			last_node = first;
			++first;
			continue;
		}
		GumboAttribute *href =
		  gumbo_get_attribute( &node->v.element.attributes, "href" );
		if( href == nullptr ) {
			// Error in document probably, but just ignore
			last_node = first;
			++first;
			continue;
		}
		auto title = daw::parser::trim( get_tag_text( node ) );
		auto uri = daw::string_view( href->value );
		if( daw::nsc_or( uri.empty( ),
		                 title.empty( ),
		                 not ::Contains( title, "climate" ),
		                 not uri.starts_with( "http" ),
		                 uri.find( "climate" ) == daw::string_view::npos ) ) {
			last_node = first;
			++first;
			continue;
		}
		(void)onEach( uri, title );
		last_node = first;
		++first;
	}
	(void)last_node;
}

struct HtmlCache {
	using func_t = std::function<std::vector<Url>( )>;
	using cache_t = daw::CachedValue<std::vector<Url>, func_t>;
	std::unordered_map<std::string, cache_t> operator( )( ) const {
		auto result = std::unordered_map<std::string, cache_t>{ };
		std::transform(
		  std::begin( newspapers ),
		  std::end( newspapers ),
		  std::inserter( result, std::begin( result ) ),
		  []( Newspaper const &n ) {
			  return std::pair<std::string, cache_t>(
			    n.name,
			    cache_t( func_t( [n]( ) {
				    auto curl = daw::curl_wrapper( );
				    curl_easy_setopt( static_cast<CURL *>( curl ),
				                      CURLOPT_FOLLOWLOCATION,
				                      1L );
				    auto const html = curl.get_string( n.address );

				    daw::gumbo::GumboHandle output =
				      gumbo_parse_with_options( &kGumboDefaultOptions,
				                                html.c_str( ),
				                                html.size( ) );
				    std::vector<Url> r{ };
				    r.reserve( 128 ); // Get past small allocations
				    search_for_links( output->root,
				                      [&]( std::string uri, std::string title ) {
					                      uri = n.base + uri;
					                      r.push_back( Url{ DAW_MOVE( uri ),
					                                        DAW_MOVE( title ),
					                                        n.name } );
				                      } );
				    std::sort( std::begin( r ), std::end( r ) );
				    r.erase( std::unique( std::begin( r ), std::end( r ) ),
				             std::end( r ) );
				    return r;
			    } ) ) );
		  } );
		return result;
	}
};

int main( ) {
	static auto html_cache = HtmlCache{ }( );
	/*
	auto app = crow::SimpleApp{ };
	CROW_ROUTE( app, "/sources/" ).methods( crow::HTTPMethod::GET )( []( ) {
	  std::vector<std::string_view> result{ };
	  result.reserve( html_cache.size( ) );
	  std::transform(
	    std::begin( html_cache ),
	    std::end( html_cache ),
	    std::back_inserter( result ),
	    []( auto const &kv ) { return std::string_view( kv.first ); } );
	  return crow::response( daw::json::to_json( result ) );
	} );
	CROW_ROUTE( app, "/news/" ).methods( crow::HTTPMethod::GET )( []( ) {
	  std::vector<std::future<std::vector<Url>>> urls_fut{ };
	  urls_fut.reserve( html_cache.size( ) );
	  for( auto &c : html_cache ) {
	    urls_fut.push_back( c.second.get( ) );
	  }
	  std::vector<Url> all_urls{ };
	  for( auto &f : urls_fut ) {
	    auto u = f.get( );
	    all_urls.insert( std::end( all_urls ), std::begin( u ), std::end( u ) );
	  }
	  auto resp = crow::response( daw::json::to_json( all_urls ) );
	  resp.add_header( "Content-Type", "application/json" );
	  return resp;
	} );
	CROW_ROUTE( app, "/news/<string>" )
	  .methods( crow::HTTPMethod::GET )( []( std::string const &which_source ) {
	    auto it = html_cache.find( which_source );
	    if( it == std::end( html_cache ) ) {
	      return crow::response( 404U );
	    }
	    auto resp =
	      crow::response( daw::json::to_json( it->second.get( ).get( ) ) );
	    resp.add_header( "Content-Type", "application/json" );
	    return resp;
	  } );
	app.loglevel( crow::LogLevel::Error );
	app.port( 8080 ).multithreaded( ).run( );
	 */
	return html_cache.size( );
}
