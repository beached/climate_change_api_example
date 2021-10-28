// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/climate_change_api_example
//

#pragma once

#include <string>

struct Newspaper {
	std::string name;
	std::string address;
	std::string base;
};

static const Newspaper newspapers[] = {
  { "cityam",
    "https://www.cityam.com/"
    "london-must-become-a-world-leader-on-climate-change-action/",
    "" },
  { "thetimes", "https://www.thetimes.co.uk/environment/climate-change", "" },
  {
    "guardian",
    "https://www.theguardian.com/environment/climate-crisis",
    "",
  },
  {
    "telegraph",
    "https://www.telegraph.co.uk/climate-change",
    "https://www.telegraph.co.uk",
  },
  {
    "nyt",
    "https://www.nytimes.com/international/section/climate",
    "",
  },
  {
    "latimes",
    "https://www.latimes.com/environment",
    "",
  },
  {
    "smh",
    "https://www.smh.com.au/environment/climate-change",
    "https://www.smh.com.au",
  },
  {
    "un",
    "https://www.un.org/climatechange",
    "",
  },
  {
    "bbc",
    "https://www.bbc.co.uk/news/science_and_environment",
    "https://www.bbc.co.uk",
  },
  { "es",
    "https://www.standard.co.uk/topic/climate-change",
    "https://www.standard.co.uk" },
  { "sun", "https://www.thesun.co.uk/topic/climate-change-environment/", "" },
  { "dm",
    "https://www.dailymail.co.uk/news/climate_change_global_warming/index.html",
    "" },
  { "nyp", "https://nypost.com/tag/climate-change/", "" } };
