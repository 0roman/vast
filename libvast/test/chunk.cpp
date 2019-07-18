/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#define SUITE chunk

#include "vast/chunk.hpp"

#include "vast/test/test.hpp"

#include "vast/load.hpp"
#include "vast/save.hpp"

#include "vast/span.hpp"

using namespace vast;

TEST(deleter) {
  char buf[100];
  auto i = 42;
  MESSAGE("owning chunk");
  auto deleter = [&]() { i = 0; };
  auto x = chunk::make(sizeof(buf), buf, deleter);
  CHECK_EQUAL(i, 42);
  x = nullptr;
  CHECK_EQUAL(i, 0);
  i = 42;
}

TEST(access) {
  auto xs = std::vector<char>{'f', 'o', 'o'};
  auto chk = chunk::make(std::move(xs));
  REQUIRE_NOT_EQUAL(chk, nullptr);
  auto& x = *chk;
  CHECK_EQUAL(x.size(), 3u);
  CHECK_EQUAL(x[0], 'f');
  CHECK_EQUAL(*x.begin(), 'f');
}

TEST(slicing) {
  char buf[100];
  auto x = chunk::make(as_bytes(span{buf, sizeof(buf)}));
  auto y = x->slice(50);
  auto z = y->slice(40, 5);
  CHECK_EQUAL(y->size(), 50u);
  CHECK_EQUAL(z->size(), 5u);
}

TEST(serialization) {
  std::string_view str = "foobarbaz";
  auto x = chunk::make(as_bytes(span{str.data(), str.size()}));
  std::vector<char> buf;
  CHECK_EQUAL(save(nullptr, buf, x), caf::none);
  chunk_ptr y;
  CHECK_EQUAL(load(nullptr, buf, y), caf::none);
  REQUIRE_NOT_EQUAL(y, nullptr);
  CHECK(std::equal(x->begin(), x->end(), y->begin(), y->end()));
}
