// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#include "daw/ccae/html_cache.h"
#include "daw/ccae/newspaper.h"
#include "daw/ccae/url.h"

#include <daw/daw_memory_mapped_file.h>
#include <daw/json/daw_json_link.h>

#define CROW_MAIN

#include <algorithm>
#include <crow.h>
#include <future>
#include <iterator>
#include <unordered_map>
#include <vector>

int main( int argc, char **argv ) {
	if( argc < 2 ) {
		std::cerr << "Must supply a newspaper.json file\n";
		exit( EXIT_FAILURE );
	}
	auto html_cache = [&] {
		auto json_data = daw::filesystem::memory_mapped_file_t<char>( argv[1] );
		auto newspapers =
		  daw::json::from_json<std::vector<daw::ccae::Newspaper>>( json_data );
		return daw::ccae::html_cache_t{ }( newspapers );
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
		try {
			std::vector<std::future<std::vector<daw::ccae::Url>>> urls_fut{ };
			urls_fut.reserve( html_cache.size( ) );
			for( auto &c : html_cache ) {
				urls_fut.push_back( c.second.get( ) );
			}
			std::vector<daw::ccae::Url> all_urls{ };
			for( auto &f : urls_fut ) {
				auto u = f.get( );
				all_urls.insert( std::end( all_urls ), std::begin( u ), std::end( u ) );
			}
			auto resp = crow::response( daw::json::to_json( all_urls ) );
			resp.add_header( "Content-Type", "application/json" );
			return resp;
		} catch( ... ) { return crow::response( 500 ); }
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
	if( char const *ptr = std::getenv( "PORT" ); ptr ) {
		int port = std::atof( ptr );
		app.port( port ).multithreaded( ).run( );
	} else {
		app.port( 8080 ).multithreaded( ).run( );
	}
}
