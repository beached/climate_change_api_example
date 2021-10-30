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

namespace daw::climate {
	struct Newspaper {
		std::string name;
		std::string address;
		std::string base;
	};
} // namespace daw::climate

namespace daw::json {
	template<>
	struct json_data_contract<::daw::climate::Newspaper> {
		static constexpr char const name[] = "name";
		static constexpr char const address[] = "address";
		static constexpr char const base[] = "base";
		using type = json_member_list<json_string<name>,
		                              json_string<address>,
		                              json_string<base>>;
	};
} // namespace daw::json
