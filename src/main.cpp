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
#include <daw/daw_memory_mapped_file.h>
#include <daw/daw_move.h>
#include <daw/daw_not_null.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/gumbo_pp/gumbo_algorithms.h>
#include <daw/gumbo_pp/gumbo_handle.h>
#include <daw/gumbo_pp/gumbo_node_iterator.h>
#include <daw/json/daw_json_link.h>
#include <daw/utf8.h>

#define CROW_MAIN

#include <crow.h>
#include <future>
#include <gumbo.h>
#include <initializer_list>
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

constexpr bool starts_with( daw::string_view haystack,
                            daw::string_view needle ) {
	return haystack.substr( 0, needle.length( ) ) == needle;
}

template<typename Callback>
static void
search_for_links_with_text( GumboNode *root_node,
                            std::initializer_list<daw::string_view> queries,
                            Callback onEach ) {
	(void)daw::gumbo::find_all_oneach(
	  daw::gumbo::gumbo_node_iterator_t( root_node ),
	  daw::gumbo::gumbo_node_iterator_t( ),
	  GUMBO_TAG_A,
	  [&]( GumboNode const &node ) {
		  auto uri = daw::gumbo::node_attribute_value( node, "href" );
		  if( not starts_with( uri, "http" ) ) {
			  return;
		  }
		  auto title = daw::parser::trim( daw::gumbo::node_text( node ) );
		  for( auto q : queries ) {
			  if( daw::nsc_or( ::Contains( title, q ), ::Contains( uri, q ) ) ) {
				  (void)onEach( uri, title );
				  return;
			  }
		  }
	  } );
}

struct HtmlCache {
	using func_t = std::function<std::vector<Url>( )>;
	using cache_t = daw::CachedValue<std::vector<Url>, func_t>;
	std::unordered_map<std::string, cache_t>
	operator( )( std::vector<daw::climate::Newspaper> const &newspapers ) const {
		auto result = std::unordered_map<std::string, cache_t>{ };
		std::transform(
		  std::begin( newspapers ),
		  std::end( newspapers ),
		  std::inserter( result, std::begin( result ) ),
		  []( daw::climate::Newspaper const &n ) {
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
				    search_for_links_with_text(
				      output->root,
				      { "climate" },
				      [&]( auto &&uri, auto &&title ) {
					      std::string u{ };
					      u.reserve( std::size( n.base ) + std::size( uri ) );
					      u.append( std::data( n.base ), std::size( n.base ) );
					      u.append( std::data( uri ), std::size( uri ) );
					      std::string t( std::data( title ), std::size( title ) );
					      r.push_back( Url{ DAW_MOVE( u ), DAW_MOVE( t ), n.name } );
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

int main( int argc, char **argv ) {
	if( argc < 2 ) {
		std::cerr << "Must supply a newspaper.json file\n";
		exit( EXIT_FAILURE );
	}
	auto html_cache = [&] {
		auto json_data = daw::filesystem::memory_mapped_file_t<char>( argv[1] );
		auto newspapers =
		  daw::json::from_json<std::vector<daw::climate::Newspaper>>( json_data );
		return HtmlCache{ }( newspapers );
	}( );
	auto app = crow::SimpleApp{ };
	CROW_ROUTE( app, "/sources/" ).methods( crow::HTTPMethod::GET )( [&]( ) {
		std::vector<std::string_view> result{ };
		result.reserve( html_cache.size( ) );
		std::transform(
		  std::begin( html_cache ),
		  std::end( html_cache ),
		  std::back_inserter( result ),
		  []( auto const &kv ) { return std::string_view( kv.first ); } );
		return crow::response( daw::json::to_json( result ) );
	} );
	CROW_ROUTE( app, "/news/" ).methods( crow::HTTPMethod::GET )( [&]( ) {
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
	  .methods( crow::HTTPMethod::GET )( [&]( std::string const &which_source ) {
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
	app.port( 8080 )./*multithreaded( ).*/ run( );
}
