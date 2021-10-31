// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <daw/daw_logic.h>
#include <daw/daw_parser_helper_sv.h>
#include <daw/daw_string_view.h>
#include <daw/gumbo_pp/gumbo_algorithms.h>
#include <daw/gumbo_pp/gumbo_node_iterator.h>
#include <daw/gumbo_pp/gumbo_util.h>
#include <daw/utf8.h>

#include <gumbo.h>
#include <initializer_list>

namespace daw::ccae::details {
	/// Do a case insensitive(for ASCII) compare
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
			                    return lhs == rhs or
			                           ( lhs < 128U and rhs < 128U and
			                             ( lhs | 32U ) == ( rhs | 32U ) );
		                    } ) != haystack_last;
	}

	constexpr bool starts_with( daw::string_view haystack,
	                            daw::string_view needle ) {
		return haystack.substr( 0, needle.length( ) ) == needle;
	}
} // namespace daw::ccae::details

namespace daw::ccae {
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
			  if( not details::starts_with( uri, "http" ) ) {
				  return;
			  }
			  auto title = daw::parser::trim( daw::gumbo::node_text( node ) );
			  for( auto q : queries ) {
				  if( daw::nsc_or( details::Contains( title, q ),
				                   details::Contains( uri, q ) ) ) {
					  (void)onEach( uri, title );
					  return;
				  }
			  }
		  } );
	}
} // namespace daw::ccae
