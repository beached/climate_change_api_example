// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <daw/daw_not_null.h>

#include <cstddef>
#include <gumbo.h>
#include <iterator>

namespace daw::gumbo {
	template<typename ChildType = GumboNode *>
	struct GumboVectorIterator {
		using difference_type = std::ptrdiff_t;
		using size_type = unsigned int;
		using value_type = ChildType;
		using pointer = value_type *;
		using const_pointer = value_type const *;
		using iterator_category = std::random_access_iterator_tag;
		using reference = value_type &;
		using const_reference = value_type const &;
		using values_type = daw::not_null<GumboVector *>;

	private:
		values_type m_vector;
		size_type m_index;

	public:
		GumboVectorIterator( GumboVector &vect )
		  : m_vector( &vect )
		  , m_index( 0 ) {}

		[[nodiscard]] GumboVectorIterator begin( ) const {
			return *this;
		}

		[[nodiscard]] GumboVectorIterator end( ) const {
			auto result = *this;
			result.m_index = m_vector->length;
			return result;
		}

		[[nodiscard]] pointer data( ) const {
			return reinterpret_cast<ChildType *>( m_vector->data );
		}

		[[nodiscard]] reference operator*( ) const {
			return data( )[m_index];
		}

		[[nodiscard]] pointer operator->( ) const {
			return data( );
		}

		GumboVectorIterator &operator++( ) {
			++m_index;
			return *this;
		}

		GumboVectorIterator operator++( int ) & {
			GumboVectorIterator result = *this;
			++m_index;
			return result;
		}

		GumboVectorIterator &operator--( ) {
			--m_index;
			return *this;
		}

		GumboVectorIterator operator--( int ) & {
			GumboVectorIterator result = *this;
			--m_index;
			return result;
		}

		GumboVectorIterator &operator+=( difference_type n ) {
			m_index += n;
			return *this;
		}

		GumboVectorIterator &operator-=( difference_type n ) {
			m_index -= n;
			return *this;
		}

		[[nodiscard]] GumboVectorIterator
		operator+( difference_type n ) const noexcept {
			GumboVectorIterator result = *this;
			result.m_index += n;
			return result;
		}

		[[nodiscard]] GumboVectorIterator
		operator-( difference_type n ) const noexcept {
			GumboVectorIterator result = *this;
			result.m_index -= n;
			return result;
		}

		[[nodiscard]] difference_type
		operator-( GumboVectorIterator const &rhs ) const noexcept {
			return static_cast<difference_type>( m_index ) -
			       static_cast<difference_type>( rhs.m_index );
		}

		[[nodiscard]] daw::not_null<ChildType> operator[]( size_type n ) noexcept {
			return static_cast<ChildType>( m_vector->data[m_index + n] );
		}

		[[nodiscard]] const_reference operator[]( size_type n ) const noexcept {
			return static_cast<ChildType>( m_vector->data[m_index + n] );
		}

		[[nodiscard]] friend bool operator==( GumboVectorIterator const &lhs,
		                                      GumboVectorIterator const &rhs ) {
			return lhs.m_index == rhs.m_index;
		}

		[[nodiscard]] friend bool operator!=( GumboVectorIterator const &lhs,
		                                      GumboVectorIterator const &rhs ) {
			return lhs.m_index != rhs.m_index;
		}

		[[nodiscard]] friend bool operator<( GumboVectorIterator const &lhs,
		                                     GumboVectorIterator const &rhs ) {
			return lhs.m_index < rhs.m_index;
		}

		[[nodiscard]] friend bool operator<=( GumboVectorIterator const &lhs,
		                                      GumboVectorIterator const &rhs ) {
			return lhs.m_index <= rhs.m_index;
		}

		[[nodiscard]] friend bool operator>( GumboVectorIterator const &lhs,
		                                     GumboVectorIterator const &rhs ) {
			return lhs.m_index > rhs.m_index;
		}

		[[nodiscard]] friend bool operator>=( GumboVectorIterator const &lhs,
		                                      GumboVectorIterator const &rhs ) {
			return lhs.m_index >= rhs.m_index;
		}
	};

	GumboVectorIterator( GumboVector const & ) -> GumboVectorIterator<>;
} // namespace daw::gumbo
