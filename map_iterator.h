// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/
//

#pragma once

#include <cstdint>
#include <iterator>
#include <type_traits>

namespace daw::iterator {
	template<typename T>
	struct arrow_proxy {
		T value;

		T *operator->( ) const {
			return &value;
		}
	};

	template<typename Iterator, typename Function>
	struct random_map_iterator {
		using iterator_category = std::random_access_iterator_tag;
		using value_type =
		  std::invoke_result_t<Function,
		                       typename std::iterator_traits<Iterator>::value_type>;
		using reference = value_type;
		using pointer = arrow_proxy<value_type>;
		using difference_type = std::ptrdiff_t;
		using size_type = std::size_t;

	private:
		Iterator m_iter;
		Function m_func;

	public:
		constexpr random_map_iterator( ) = default;
		constexpr random_map_iterator( Iterator it, Function f )
		  : m_iter( it )
		  , m_func( f ) {}

		constexpr reference operator[]( size_type n ) const {
			return m_func( *( m_iter + static_cast<difference_type>( n ) ) );
		}

		constexpr reference operator*( ) const {
			return operator[]( 0U );
		}

		constexpr pointer operator->( ) const {
			return pointer( operator[]( 0U ) );
		}

		constexpr random_map_iterator &operator++( ) {
			++m_iter;
			return *this;
		}

		constexpr random_map_iterator operator++( int ) {
			random_map_iterator result = *this;
			++m_iter;
			return result;
		}

		constexpr random_map_iterator &operator--( ) {
			--m_iter;
			return *this;
		}

		constexpr random_map_iterator operator--( int ) {
			random_map_iterator result = *this;
			--m_iter;
			return result;
		}

		constexpr random_map_iterator &operator+=( difference_type n ) {
			m_iter += n;
			return *this;
		}

		constexpr random_map_iterator &operator-=( difference_type n ) {
			m_iter -= n;
			return *this;
		}

		constexpr random_map_iterator
		operator+( difference_type n ) const noexcept {
			random_map_iterator result = *this;
			m_iter += n;
			return result;
		}

		constexpr random_map_iterator
		operator-( difference_type n ) const noexcept {
			random_map_iterator result = *this;
			m_iter -= n;
			return result;
		}

		constexpr difference_type operator-( random_map_iterator const &rhs ) {
			return m_iter - rhs.m_iter;
		}

		constexpr friend bool operator==( random_map_iterator const &lhs,
		                                  random_map_iterator const &rhs ) {
			return lhs.m_iter == rhs.m_iter;
		}

		constexpr friend bool operator!=( random_map_iterator const &lhs,
		                                  random_map_iterator const &rhs ) {
			return lhs.m_iter != rhs.m_iter;
		}

		constexpr friend bool operator<( random_map_iterator const &lhs,
		                                 random_map_iterator const &rhs ) {
			return lhs.m_iter < rhs.m_iter;
		}

		constexpr friend bool operator<=( random_map_iterator const &lhs,
		                                  random_map_iterator const &rhs ) {
			return lhs.m_iter <= rhs.m_iter;
		}

		constexpr friend bool operator>( random_map_iterator const &lhs,
		                                 random_map_iterator const &rhs ) {
			return lhs.m_iter > rhs.m_iter;
		}

		constexpr friend bool operator>=( random_map_iterator const &lhs,
		                                  random_map_iterator const &rhs ) {
			return lhs.m_iter >= rhs.m_iter;
		}
	};

	template<typename Iterator, typename Function>
	struct bidirectional_map_iterator {
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type =
		  std::invoke_result_t<Function,
		                       typename std::iterator_traits<Iterator>::value_type>;
		using reference = value_type;
		using pointer = arrow_proxy<value_type>;
		using difference_type = std::ptrdiff_t;
		using size_type = std::size_t;

	private:
		Iterator m_iter;
		Function m_func;

	public:
		constexpr bidirectional_map_iterator( ) = default;
		constexpr bidirectional_map_iterator( Iterator it, Function f )
		  : m_iter( it )
		  , m_func( f ) {}

		constexpr reference operator[]( size_type n ) const {
			return m_func( *( m_iter + static_cast<difference_type>( n ) ) );
		}

		constexpr reference operator*( ) const {
			return m_func( *m_iter );
		}

		constexpr pointer operator->( ) const {
			return pointer( operator*( ) );
		}

		constexpr bidirectional_map_iterator &operator++( ) {
			++m_iter;
			return *this;
		}

		constexpr bidirectional_map_iterator operator++( int ) {
			bidirectional_map_iterator result = *this;
			++m_iter;
			return result;
		}

		constexpr bidirectional_map_iterator &operator--( ) {
			--m_iter;
			return *this;
		}

		constexpr bidirectional_map_iterator operator--( int ) {
			bidirectional_map_iterator result = *this;
			--m_iter;
			return result;
		}

		constexpr friend bool operator==( bidirectional_map_iterator const &lhs,
		                                  bidirectional_map_iterator const &rhs ) {
			return lhs.m_iter == rhs.m_iter;
		}

		constexpr friend bool operator!=( bidirectional_map_iterator const &lhs,
		                                  bidirectional_map_iterator const &rhs ) {
			return lhs.m_iter != rhs.m_iter;
		}
	};

	template<typename Iterator, typename Function>
	struct input_map_iterator {
		using iterator_category = std::input_iterator_tag;
		using value_type =
		  std::invoke_result_t<Function,
		                       typename std::iterator_traits<Iterator>::value_type>;
		using reference = value_type;
		using pointer = arrow_proxy<value_type>;
		using difference_type = std::ptrdiff_t;
		using size_type = std::size_t;

	private:
		Iterator m_iter;
		Function m_func;

	public:
		constexpr input_map_iterator( ) = default;
		constexpr input_map_iterator( Iterator it, Function f )
		  : m_iter( it )
		  , m_func( f ) {}

		constexpr reference operator*( ) const {
			return m_func( *m_iter );
		}

		constexpr pointer operator->( ) const {
			return pointer( operator*( ) );
		}

		constexpr input_map_iterator &operator++( ) {
			++m_iter;
			return *this;
		}

		constexpr input_map_iterator operator++( int ) {
			input_map_iterator result = *this;
			++m_iter;
			return result;
		}

		constexpr friend bool operator==( input_map_iterator const &lhs,
		                                  input_map_iterator const &rhs ) {
			return lhs.m_iter == rhs.m_iter;
		}

		constexpr friend bool operator!=( input_map_iterator const &lhs,
		                                  input_map_iterator const &rhs ) {
			return lhs.m_iter != rhs.m_iter;
		}
	};

	template<typename Iterator, typename Function>
	using base_map_iterator_type = std::conditional_t<
	  std::is_same_v<std::random_access_iterator_tag,
	                 typename std::iterator_traits<Iterator>::iterator_category>,
	  random_map_iterator<Iterator, Function>,
	  std::conditional_t<std::is_same_v<std::bidirectional_iterator_tag,
	                                    typename std::iterator_traits<
	                                      Iterator>::iterator_category>,
	                     bidirectional_map_iterator<Iterator, Function>,
	                     input_map_iterator<Iterator, Function>>>;

	template<typename Iterator, typename Function>
	class map_iterator : public base_map_iterator_type<Iterator, Function> {
		using base = base_map_iterator_type<Iterator, Function>;

	public:
		using iterator_category = typename base::iterator_category;
		using value_type = typename base::value_type;
		using reference = typename base::reference;
		using pointer = typename base::pointer;
		using difference_type = typename base::difference_type;

		using base::base;
	};
} // namespace daw::iterator
