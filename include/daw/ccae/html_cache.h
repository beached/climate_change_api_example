// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include "cached_value.h"
#include "filter_config.h"
#include "link_search.h"
#include "url.h"

#include <daw/curl_wrapper.h>
#include <daw/gumbo_pp/gumbo_handle.h>
#include <daw/utf8/unchecked.h>

#include <algorithm>
#include <functional>
#include <gumbo.h>
#include <iterator>
#include <string>
#include <vector>

namespace daw::ccae {
	namespace ccae_details {
		template<typename StringView>
		std::string shrink_ws( StringView &&source ) {
			std::string result{ };
			result.reserve( source.size( ) );
			auto first = daw::utf8::unchecked::iterator( std::data( source ) );
			auto last = daw::utf8::unchecked::iterator( daw::data_end( source ) );
			while( first != last ) {
				if( daw::parser::is_unicode_whitespace( *first ) ) {
					result += ' ';
					++first;
					while( first != last and
					       daw::parser::is_unicode_whitespace( *first ) ) {
						++first;
					}
				} else {
					utf8::utf32to8( first,
					                std::next( first ),
					                std::back_inserter( result ) );
					++first;
				}
			}
			return result;
		}
	} // namespace ccae_details
	struct html_cache_t {
		using func_t = std::function<std::vector<Url>( )>;
		using cache_t = daw::ccae::cached_value_t<std::vector<Url>, func_t>;
		std::unordered_map<std::string, cache_t>
		operator( )( daw::ccae::filter_config_t const &filter_config ) const {
			auto result = std::unordered_map<std::string, cache_t>{ };
			std::transform(
			  std::begin( filter_config.urls ),
			  std::end( filter_config.urls ),
			  std::inserter( result, std::end( result ) ),
			  [&filter_config]( daw::ccae::url_source_t const &n ) {
				  return std::pair<std::string, cache_t>(
				    n.name,
				    cache_t( func_t( [n, keywords = filter_config.keywords] {
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
					      keywords,
					      [&]( auto &&uri, auto &&title ) {
						      std::string u{ };
						      u.reserve( std::size( n.base ) + std::size( uri ) );
						      u.append( std::data( n.base ), std::size( n.base ) );
						      u.append( std::data( uri ), std::size( uri ) );
						      auto t = ccae_details::shrink_ws( title );
						      auto tsv = daw::parser::trim( daw::string_view( t ) );
						      r.push_back( Url{ DAW_MOVE( u ),
						                        static_cast<std::string>( tsv ),
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

} // namespace daw::ccae
