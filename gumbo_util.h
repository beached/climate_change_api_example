// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <daw/daw_not_null.h>

#include <gumbo.h>

namespace daw::gumbo {
	template<typename Visitor>
	inline decltype( auto ) visit( daw::not_null<GumboNode *> node,
	                               Visitor vis ) {
		switch( node->type ) {
		case GumboNodeType::GUMBO_NODE_DOCUMENT:
			return vis( node->v.document );
		case GumboNodeType::GUMBO_NODE_ELEMENT:
			return vis( node->v.element );
		case GumboNodeType::GUMBO_NODE_TEXT:
		case GumboNodeType::GUMBO_NODE_CDATA:
		case GumboNodeType::GUMBO_NODE_COMMENT:
		case GumboNodeType::GUMBO_NODE_WHITESPACE:
		case GumboNodeType::GUMBO_NODE_TEMPLATE:
			return vis( node->v.text );
		}
	}

} // namespace daw::gumbo
