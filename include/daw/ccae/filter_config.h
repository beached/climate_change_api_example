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
#include <vector>

namespace daw::ccae {
	struct url_source_t {
		std::string name;
		std::string address;
		std::string base;
	};

	struct filter_config_t {
		std::vector<std::string> keywords;
		std::vector<url_source_t> urls;
	};
} // namespace daw::ccae

namespace daw::json {
	template<>
	struct json_data_contract<daw::ccae::url_source_t> {
		static constexpr char const name[] = "name";
		static constexpr char const address[] = "address";
		static constexpr char const base[] = "base";
		using type = json_member_list<json_string<name>,
		                              json_string<address>,
		                              json_string<base>>;

		static inline auto to_json_data( daw::ccae::url_source_t const &value ) {
			return std::forward_as_tuple( value.name, value.address, value.base );
		}
	};

	template<>
	struct json_data_contract<daw::ccae::filter_config_t> {
		static constexpr char const keywords[] = "keywords";
		static constexpr char const urls[] = "urls";
		using type = json_member_list<json_array<keywords, std::string>,
		                              json_array<urls, daw::ccae::url_source_t>>;
		static inline auto to_json_data( daw::ccae::filter_config_t const &value ) {
			return std::forward_as_tuple( value.keywords, value.urls );
		}
	};
} // namespace daw::json
