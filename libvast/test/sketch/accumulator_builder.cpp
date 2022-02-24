//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2022 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/sketch/accumulator_builder.hpp"

#include "vast/sketch/min_max_accumulator.hpp"

#define SUITE sketch

#include "vast/test/fixtures/events.hpp"
#include "vast/test/test.hpp"

#include <caf/test/dsl.hpp>

using namespace vast;
using namespace vast::sketch;

FIXTURE_SCOPE(partition_sketch_tests, fixtures::events)

TEST(min_max accumulator) {
  auto builder = accumulator_builder<min_max_accumulator>{};
  auto err = builder.add(zeek_conn_log[0], offset{0});
  CHECK_EQUAL(err, caf::none);
}

FIXTURE_SCOPE_END()
