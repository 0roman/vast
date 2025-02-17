//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#define SUITE chunk

#include "vast/chunk.hpp"

#include "vast/detail/legacy_deserialize.hpp"
#include "vast/detail/serialize.hpp"
#include "vast/test/fixtures/filesystem.hpp"
#include "vast/test/test.hpp"

#include <cstddef>
#include <span>

using namespace vast;

TEST(deleter) {
  char buf[100] = {};
  auto i = 42;
  MESSAGE("owning chunk");
  auto deleter = [&]() noexcept {
    i = 0;
  };
  auto x = chunk::make(buf, sizeof(buf), std::move(deleter));
  CHECK_EQUAL(i, 42);
  x = nullptr;
  CHECK_EQUAL(i, 0);
  i = 42;
}

TEST(deletion_step) {
  char buf[100] = {};
  auto i = 0;
  MESSAGE("owning chunk");
  auto x = chunk::copy(buf);
  x->add_deletion_step([&]() noexcept {
    i = 42;
  });
  auto y = x->slice(1);
  auto z = y->slice(2);
  CHECK_EQUAL(i, 0);
  x = nullptr;
  CHECK_EQUAL(i, 0);
  y = nullptr;
  CHECK_EQUAL(i, 0);
  z = nullptr;
  CHECK_EQUAL(i, 42);
}

TEST(access) {
  auto xs = std::vector<char>{'f', 'o', 'o'};
  auto chk = chunk::make(std::move(xs));
  REQUIRE_NOT_EQUAL(chk, nullptr);
  auto& x = *chk;
  CHECK_EQUAL(x.size(), 3u);
  CHECK_EQUAL(*x.begin(), static_cast<std::byte>('f'));
}

TEST(slicing) {
  std::array<char, 100> buf = {};
  auto x = chunk::copy(buf);
  auto y = x->slice(50);
  auto z = y->slice(40, 5);
  CHECK_EQUAL(y->size(), 50u);
  CHECK_EQUAL(z->size(), 5u);
}

TEST(serialization) {
  std::string str = "foobarbaz";
  auto x = chunk::make(std::move(str));
  std::vector<char> buf;
  CHECK_EQUAL(detail::serialize(buf, x), caf::none);
  chunk_ptr y;
  CHECK_EQUAL(detail::legacy_deserialize(buf, y), true);
  REQUIRE_NOT_EQUAL(y, nullptr);
  CHECK(std::equal(x->begin(), x->end(), y->begin(), y->end()));
}

TEST(as_bytes) {
  std::string str = "foobarbaz";
  auto copy = str;
  auto bytes
    = std::span{reinterpret_cast<const std::byte*>(copy.data()), copy.size()};
  auto x = chunk::make(std::move(str));
  CHECK_EQUAL(bytes, as_bytes(x));
}

namespace {

struct fixture : public fixtures::filesystem {
  fixture() : fixtures::filesystem(VAST_PP_STRINGIFY(SUITE)) {
  }
};

} // namespace

FIXTURE_SCOPE(chunk_tests, fixture)

TEST(read / write) {
  std::string str = "foobarbaz";
  auto x = chunk::make(std::move(str));
  const auto filename = directory / "chunk";
  auto err = write(filename, x);
  CHECK_EQUAL(err, caf::none);
  chunk_ptr y;
  err = read(filename, y);
  CHECK_EQUAL(err, caf::none);
  CHECK_EQUAL(as_bytes(x), as_bytes(y));
}

FIXTURE_SCOPE_END()
