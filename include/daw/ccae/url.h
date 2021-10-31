// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <daw/json/daw_json_link.h>

#include <string>

namespace daw::ccae {
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
} // namespace daw::ccae

namespace daw::json {
	template<>
	struct json_data_contract<daw::ccae::Url> {
		static constexpr char const uri[] = "uri";
		static constexpr char const title[] = "title";
		static constexpr char const source[] = "source";
		using type = json_member_list<json_string<uri>,
		                              json_string<title>,
		                              json_string<source>>;

		static inline auto to_json_data( daw::ccae::Url const &u ) {
			return std::forward_as_tuple( u.uri, u.title, u.source );
		}
	};
} // namespace daw::json
