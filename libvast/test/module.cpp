//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2022 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#define SUITE module

#include "vast/module.hpp"

#include "vast/aliases.hpp"
#include "vast/data.hpp"
#include "vast/test/test.hpp"

using namespace vast;

TEST(multiple members) {
  auto x = module_ng{};
  type y = type{};
  auto table = symbol_table_ng{};
  auto r = record{
    {"module", std::string{"foo"}},
    {"description", std::string{"blab"}},
    {"references", list{{std::string{"http://foo.com"}},
                        {std::string{"https://www.google.com/search?q=foo"}}}},
    {"types",
     record{
       {"id", record{{"type", std::string{"string"}},
                     {"description", std::string{"A random unique ID with..."}},
                     {"attributes", record{{"index", std::string{"has"
                                                                 "h"}}}}}}}}};
  REQUIRE_SUCCESS(convert(r, y, table)); // FIXME: x instead of y
}
