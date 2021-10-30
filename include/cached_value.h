// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <daw/daw_random.h>
#include <daw/parallel/daw_latch.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

namespace daw {
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
		  , m_state( std::make_unique<state_t>(
		      std::chrono::seconds( 3600 + daw::randint<int>( -100, 100 ) ) ) ) {
		}

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
				// We have not yet retrieved data and another thread has started the
				// load. Wait for it and then return the new data
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
			return std::async( std::launch::async, [&]( ) -> type {
				try {
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
				} catch( ... ) {
					m_state->m_working = false;
					m_state->m_latch.notify( );
					return type{ };
				}
			} );
		}
	};
	template<typename Ret>
	CachedValue( Ret ) -> CachedValue<std::invoke_result_t<Ret>, Ret>;

	template<typename Ret>
	CachedValue( Ret, std::chrono::seconds )
	  -> CachedValue<std::invoke_result_t<Ret>, Ret>;
} // namespace daw
