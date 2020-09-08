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

#define SUITE data

#include "vast/data.hpp"

#include "vast/test/test.hpp"

#include "vast/concept/convertible/to.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/data.hpp"
#include "vast/concept/printable/stream.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/data.hpp"
#include "vast/concept/printable/vast/json.hpp"
#include "vast/json.hpp"
#include "vast/load.hpp"
#include "vast/save.hpp"

#include <caf/test/dsl.hpp>

using namespace vast;

TEST(list) {
  REQUIRE(std::is_same_v<std::vector<data>, list>);
}

TEST(tables) {
  map ports{{"ssh", 22u}, {"http", 80u}, {"https", 443u}, {"imaps", 993u}};
  CHECK(ports.size() == 4);
  auto i = ports.find("ssh");
  REQUIRE(i != ports.end());
  CHECK(i->second == 22u);
  i = ports.find("imaps");
  REQUIRE(i != ports.end());
  CHECK(i->second == 993u);
  CHECK(ports.emplace("telnet", 23u).second);
  CHECK(!ports.emplace("http", 8080u).second);
}

TEST(flatten) {
  // clang-format off
  auto rt = record_type{
    {"a", string_type{}},
    {"b", record_type{
      {"c", integer_type{}},
      {"d", list_type{integer_type{}}}
    }},
    {"e", record_type{
      {"f", address_type{}},
      {"g", port_type{}}
    }},
    {"h", bool_type{}}
  };
  auto xs = record{
    {"a", "foo"},
    {"b", record{
      {"c", -42},
      {"d", list{1, 2, 3}}
    }},
    {"e", record{
      {"f", caf::none},
      {"g", caf::none},
    }},
    {"h", true}
  };
  // clang-format on
  auto values
    = std::vector<data>{"foo", -42, list{1, 2, 3}, caf::none, caf::none, true};
  // Because list is exactly std::vector<data>, and data can be constructed from
  // list, we must use () over {} for explicitly copying values here. This
  // happened to work with brace-initialization using libc++, because libc++
  // does not properly implement deduction guides for std::vector.
  auto r = unbox(make_record(rt, std::vector<data>(values)));
  REQUIRE_EQUAL(r, xs);
  MESSAGE("flatten");
  auto fr = flatten(r);
  auto ftr = unbox(flatten(r, rt));
  CHECK_EQUAL(fr, ftr);
  REQUIRE_EQUAL(fr.size(), values.size());
  CHECK_EQUAL(fr["b.c"], -42);
  MESSAGE("unflatten");
  auto ur = unflatten(fr);
  CHECK_EQUAL(ur, xs);
  auto utr = unbox(unflatten(fr, rt));
  CHECK_EQUAL(utr, xs);
}

TEST(construction) {
  CHECK(caf::holds_alternative<caf::none_t>(data{}));
  CHECK(caf::holds_alternative<bool>(data{true}));
  CHECK(caf::holds_alternative<bool>(data{false}));
  CHECK(caf::holds_alternative<integer>(data{0}));
  CHECK(caf::holds_alternative<integer>(data{42}));
  CHECK(caf::holds_alternative<integer>(data{-42}));
  CHECK(caf::holds_alternative<count>(data{42u}));
  CHECK(caf::holds_alternative<real>(data{4.2}));
  CHECK(caf::holds_alternative<std::string>(data{"foo"}));
  CHECK(caf::holds_alternative<std::string>(data{std::string{"foo"}}));
  CHECK(caf::holds_alternative<pattern>(data{pattern{"foo"}}));
  CHECK(caf::holds_alternative<address>(data{address{}}));
  CHECK(caf::holds_alternative<subnet>(data{subnet{}}));
  CHECK(caf::holds_alternative<port>(data{port{53, port::udp}}));
  CHECK(caf::holds_alternative<list>(data{list{}}));
  CHECK(caf::holds_alternative<map>(data{map{}}));
}

TEST(relational_operators) {
  data d1;
  data d2;
  CHECK(d1 == d2);
  CHECK(!(d1 < d2));
  CHECK(d1 <= d2);
  CHECK(d1 >= d2);
  CHECK(!(d1 > d2));

  d2 = 42;
  CHECK(d1 != d2);
  CHECK(d1 < d2);
  CHECK(d1 <= d2);
  CHECK(!(d1 >= d2));
  CHECK(!(d1 > d2));

  d1 = 42;
  d2 = caf::none;
  CHECK(d1 != d2);
  CHECK(!(d1 < d2));
  CHECK(!(d1 <= d2));
  CHECK(d1 >= d2);
  CHECK(d1 > d2);

  d2 = 1377;
  CHECK(d1 != d2);
  CHECK(d1 < d2);
  CHECK(d1 <= d2);
  CHECK(!(d1 >= d2));
  CHECK(!(d1 > d2));
}

TEST(evaluation) {
  MESSAGE("in");
  data lhs{"foo"};
  data rhs{"foobar"};
  CHECK(evaluate(lhs, in, rhs));
  CHECK(evaluate(rhs, not_in, lhs));
  CHECK(evaluate(rhs, ni, lhs));
  CHECK(evaluate(rhs, not_in, lhs));
  MESSAGE("equality");
  lhs = count{42};
  rhs = count{1337};
  CHECK(evaluate(lhs, less_equal, rhs));
  CHECK(evaluate(lhs, less, rhs));
  CHECK(evaluate(lhs, not_equal, rhs));
  CHECK(!evaluate(lhs, equal, rhs));
  MESSAGE("network types");
  lhs = *to<address>("10.0.0.1");
  rhs = *to<subnet>("10.0.0.0/8");
  CHECK(evaluate(lhs, in, rhs));
  lhs = *to<subnet>("10.0.42.0/16");
  CHECK(evaluate(lhs, in, rhs));
  rhs = *to<subnet>("10.0.42.0/17");
  CHECK(!evaluate(lhs, in, rhs));
  MESSAGE("mixed types");
  rhs = real{4.2};
  CHECK(!evaluate(lhs, equal, rhs));
  CHECK(evaluate(lhs, not_equal, rhs));
}

TEST(evaluation - pattern matching) {
  CHECK(evaluate(pattern{"f.*o"}, equal, "foo"));
  CHECK(evaluate("foo", equal, pattern{"f.*o"}));
  CHECK(evaluate("foo", match, pattern{"f.*o"}));
}

TEST(serialization) {
  list xs;
  xs.emplace_back(port{80, port::tcp});
  xs.emplace_back(port{53, port::udp});
  xs.emplace_back(port{8, port::icmp});
  auto x0 = data{xs};
  std::vector<char> buf;
  CHECK_EQUAL(save(nullptr, buf, x0), caf::none);
  data x1;
  CHECK_EQUAL(load(nullptr, buf, x1), caf::none);
  CHECK(x0 == x1);
}

TEST(printable) {
  // Ensure that we don't produce trailing zeros for floating point data.
  auto x = data{-4.2};
  CHECK_EQUAL(to_string(x), "-4.2");
  x = 3.14;
  CHECK_EQUAL(to_string(x), "3.14");
}

TEST(parseable) {
  auto p = make_parser<data>();
  data d;
  MESSAGE("bool");
  auto str = "T"s;
  auto f = str.begin();
  auto l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == true);
  MESSAGE("numbers");
  str = "+1001"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == 1001);
  str = "1001"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == 1001u);
  str = "10.01"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == 10.01);
  MESSAGE("string");
  str = R"("bar")";
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == "bar");
  MESSAGE("pattern");
  str = "/foo/"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == pattern{"foo"});
  MESSAGE("address");
  str = "10.0.0.1"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == *to<address>("10.0.0.1"));
  MESSAGE("port");
  str = "22/tcp"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == port{22, port::tcp});
  MESSAGE("list");
  str = "[42,4.2,nil]"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == list{42u, 4.2, caf::none});
  MESSAGE("map");
  str = "{T->1,F->0}"s;
  f = str.begin();
  l = str.end();
  CHECK(p(f, l, d));
  CHECK(f == l);
  CHECK(d == map{{true, 1u}, {false, 0u}});
}

// clang-format on
TEST(json) {
  MESSAGE("plain");
  data x = record{
    {"x", "foo"},
    {"r", record{
      {"i", -42},
      {"r", record{
        {"u", 1001u}
      }},
    }},
    {"str", "x"},
    {"port", port{443, port::tcp}}
  };
  auto expected = json{json::object{
    {"x", json{"foo"}},
    {"r", json{json::object{
      {"i", json{-42}},
      {"r", json{json::object{
        {"u", json{1001}}
      }}},
    }}},
    {"str", json{"x"}},
    {"port", json{443}}
  }};
  CHECK_EQUAL(to_json(x), expected);
  MESSAGE("zipped");
  type t = record_type{
    {"x", string_type{}},
    {"r", record_type{
      {"i", integer_type{}},
      {"r", record_type{
        {"u", count_type{}}
      }},
    }},
    {"str", string_type{}},
    {"port", port_type{}}
  };
  CHECK(type_check(t, x));
  expected = R"__({
  "x": "foo",
  "r": {
    "i": -42,
    "r": {
      "u": 1001
    }
  },
  "str": "x",
  "port": 443
})__";
  CHECK_EQUAL(to_string(to_json(x, t)), expected);
}
// clang-format on
