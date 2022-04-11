//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2022 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/module.hpp"

#include "vast/test/test.hpp"

TEST(multiple members) {
  auto x = module_ng{};
  auto table = symbol_table{};
  auto r = record{
    {"module", string{"foo"}},
    {"description", string{"blab"}},
    {"references", list{{string{"http://foo.com"}},
                        {string{"https://www.google.com/search?q=foo"}}}},
    {"types",
     record{{"id", record{{"type", string{"string"}},
                          {"description", string{"A random unique ID with..."}},
                          {"attributes", record{{"index", string{"has"
                                                                 "h"}}}}}}}}};
  REQUIRE_SUCCESS(convert(r, x, table));
}
