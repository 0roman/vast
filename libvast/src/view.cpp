//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2018 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/view.hpp"

#include "vast/detail/narrow.hpp"
#include "vast/detail/overload.hpp"
#include "vast/operator.hpp"
#include "vast/type.hpp"

#include <algorithm>
#include <regex>

namespace vast {

// -- pattern_view ------------------------------------------------------------

pattern_view::pattern_view(const pattern& x) : pattern_{x.string()} {
  // nop
}

pattern_view::pattern_view(std::string_view str) : pattern_{str} {
  // nop
}

std::string_view pattern_view::string() const {
  return pattern_;
}

bool pattern_view::match(std::string_view x) const {
  return std::regex_match(x.begin(), x.end(),
                          std::regex{pattern_.begin(), pattern_.end()});
}

bool pattern_view::search(std::string_view x) const {
  return std::regex_search(x.begin(), x.end(),
                           std::regex{pattern_.begin(), pattern_.end()});
}

bool operator==(pattern_view x, pattern_view y) noexcept {
  return x.string() == y.string();
}

bool operator<(pattern_view x, pattern_view y) noexcept {
  return x.string() < y.string();
}

bool is_equal(const data& x, const data_view& y) {
  auto pred = [](const auto& lhs, const auto& rhs) {
    return is_equal(lhs, rhs);
  };
  auto f = detail::overload{
    [&](const auto& lhs, const auto& rhs) {
      using lhs_type = std::decay_t<decltype(lhs)>;
      using rhs_type = std::decay_t<decltype(rhs)>;
      if constexpr (std::is_same_v<view<lhs_type>, rhs_type>)
        return lhs == rhs;
      else
        return false;
    },
    [&](const pattern& lhs, const view<pattern>& rhs) {
      return lhs.string() == rhs.string();
    },
    [&](const list& lhs, const view<list>& rhs) {
      return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), pred);
    },
    [&](const map& lhs, const view<map>& rhs) {
      auto f = [](const auto& xs, const auto& ys) {
        return is_equal(xs.first, ys.first) && is_equal(xs.second, ys.second);
      };
      return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), f);
    },
    [&](const record& lhs, const view<record>& rhs) {
      auto f = [](const auto& xs, const auto& ys) {
        return xs.first == ys.first && is_equal(xs.second, ys.second);
      };
      return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), f);
    },
  };
  return caf::visit(f, x, y);
}

bool is_equal(const data_view& x, const data& y) {
  return is_equal(y, x);
}

// -- default_list_view -----------------------------------------------------

default_list_view::default_list_view(const list& xs) : xs_{xs} {
  // nop
}

default_list_view::value_type default_list_view::at(size_type i) const {
  return make_data_view(xs_[i]);
}

default_list_view::size_type default_list_view::size() const noexcept {
  return xs_.size();
}

// -- default_map_view --------------------------------------------------------

default_map_view::default_map_view(const map& xs) : xs_{xs} {
  // nop
}

default_map_view::value_type default_map_view::at(size_type i) const {
  auto& [key, value] = *std::next(xs_.begin(), i);
  return {make_data_view(key), make_data_view(value)};
}

default_map_view::size_type default_map_view::size() const noexcept {
  return xs_.size();
}

// -- default_record_view --------------------------------------------------------

default_record_view::default_record_view(const record& xs) : xs_{xs} {
  // nop
}

default_record_view::value_type default_record_view::at(size_type i) const {
  VAST_ASSERT(i < xs_.size());
  auto& [key, value] = *std::next(xs_.begin(), i);
  return {key, make_data_view(value)};
}

default_record_view::size_type default_record_view::size() const noexcept {
  return xs_.size();
}

// -- make_view ---------------------------------------------------------------

data_view make_view(const data& x) {
  return caf::visit(
    [](const auto& z) {
      return make_data_view(z);
    },
    x);
}

// -- materialization ----------------------------------------------------------

std::string materialize(std::string_view x) {
  return std::string{x};
}

pattern materialize(pattern_view x) {
  return pattern{std::string{x.string()}};
}

namespace {

auto materialize(std::pair<data_view, data_view> x) {
  return std::pair(materialize(x.first), materialize(x.second));
}

auto materialize(std::pair<std::string_view, data_view> x) {
  return std::pair(std::string{x.first}, materialize(x.second));
}

template <class Result, class T>
Result materialize_container(const T& xs) {
  Result result;
  if (xs)
    for (auto x : *xs)
      result.insert(result.end(), materialize(x));
  return result;
}

} // namespace

list materialize(list_view_handle xs) {
  return materialize_container<list>(xs);
}

map materialize(map_view_handle xs) {
  return materialize_container<map>(xs);
}

record materialize(record_view_handle xs) {
  return materialize_container<record>(xs);
}

data materialize(data_view x) {
  return caf::visit(
    [](auto y) {
      return data{materialize(y)};
    },
    x);
}

// WARNING: making changes to the logic of this function requires adapting the
// companion overload in type.cpp.
bool type_check(const type& x, const data_view& y) {
  auto f = detail::overload{
    [&](const auto&, const view<caf::none_t>&) {
      // Every type can be assigned nil.
      return true;
    },
    [&](const enumeration_type& t, const view<enumeration>& u) {
      return !t.field(u).empty();
    },
    [&](const list_type& t, const view<list>& u) {
      if (u.empty())
        return true;
      const auto vt = t.value_type();
      auto it = u.begin();
      const auto check = [&](const auto& d) noexcept {
        return type_check(vt, d);
      };
      if (check(*it)) {
        // Technically lists can contain heterogenous data,
        // but for optimization purposes we only check the
        // first element when assertions are disabled.
        VAST_ASSERT(std::all_of(it + 1, u.end(), check), //
                    "expected a homogenous list");
        return true;
      }
      return false;
    },
    [&](const map_type& t, const view<map>& u) {
      if (u.empty())
        return true;
      const auto kt = t.key_type();
      const auto vt = t.value_type();
      auto it = u.begin();
      const auto check = [&](const auto& d) noexcept {
        return type_check(kt, d.first) && type_check(vt, d.second);
      };
      if (check(*it)) {
        // Technically maps can contain heterogenous data,
        // but for optimization purposes we only check the
        // first element when assertions are disabled.
        VAST_ASSERT(std::all_of(it + 1, u.end(), check), //
                    "expected a homogenous map");
        return true;
      }
      return false;
    },
    [&](const record_type& t, const view<record>& u) {
      if (u.size() != t.num_fields())
        return false;
      for (size_t i = 0; const auto& [k, v] : u) {
        const auto field = t.field(i++);
        if (field.name != k || type_check(field.type, v))
          return false;
      }
      return true;
    },
    [&]<basic_type T, class U>(const T&, const U&) {
      // For basic types we can solely rely on the result of
      // construct.
      return std::is_same_v<view<type_to_data_t<T>>, U>;
    },
    [&]<complex_type T, class U>(const T&, const U&) {
      // We don't have a matching overload.
      static_assert(!std::is_same_v<view<type_to_data_t<T>>, U>, //
                    "missing type check overload");
      return false;
    },
  };
  return caf::visit(f, x, y);
}

namespace {

// Checks whether the left-hand side is contained in the right-hand side.
struct contains_predicate {
  template <class T, class U>
  bool operator()(const T& lhs, const U& rhs) const {
    if constexpr (std::is_same_v<U, view<list>>) {
      auto equals_lhs = [&](const auto& y) {
        if constexpr (std::is_same_v<T, std::decay_t<decltype(y)>>)
          return lhs == y;
        else
          return false;
      };
      auto pred = [&](const auto& rhs_element) {
        return caf::visit(equals_lhs, rhs_element);
      };
      return std::find_if(rhs->begin(), rhs->end(), pred) != rhs->end();
      return false;
    } else {
      // Default case.
      return false;
    }
  }

  bool
  operator()(const view<std::string>& lhs, const view<std::string>& rhs) const {
    return rhs.find(lhs) != std::string::npos;
  }

  bool
  operator()(const view<std::string>& lhs, const view<pattern>& rhs) const {
    return rhs.search(lhs);
  }

  bool operator()(const view<address>& lhs, const view<subnet>& rhs) const {
    return rhs.contains(lhs);
  }

  bool operator()(const view<subnet>& lhs, const view<subnet>& rhs) const {
    return rhs.contains(lhs);
  }
};

} // namespace

bool evaluate_view(const data_view& lhs, relational_operator op,
                   const data_view& rhs) {
  auto check_match = [](const auto& x, const auto& y) {
    return caf::visit(detail::overload{
                        [](auto, auto) {
                          return false;
                        },
                        [](view<std::string>& lhs, view<pattern> rhs) {
                          return rhs.match(lhs);
                        },
                      },
                      x, y);
  };
  auto check_in = [](const auto& x, const auto& y) {
    return caf::visit(contains_predicate{}, x, y);
  };
  switch (op) {
    default:
      VAST_ASSERT(!"missing case");
      return false;
    case relational_operator::match:
      return check_match(lhs, rhs);
    case relational_operator::not_match:
      return !check_match(lhs, rhs);
    case relational_operator::in:
      return check_in(lhs, rhs);
    case relational_operator::not_in:
      return !check_in(lhs, rhs);
    case relational_operator::ni:
      return check_in(rhs, lhs);
    case relational_operator::not_ni:
      return !check_in(rhs, lhs);
    case relational_operator::equal:
      return lhs == rhs;
    case relational_operator::not_equal:
      return lhs != rhs;
    case relational_operator::less:
      return lhs < rhs;
    case relational_operator::less_equal:
      return lhs <= rhs;
    case relational_operator::greater:
      return lhs > rhs;
    case relational_operator::greater_equal:
      return lhs >= rhs;
  }
}

data_view to_canonical(const type& t, const data_view& x) {
  auto v = detail::overload{
    [](const view<enumeration>& x, const enumeration_type& t) -> data_view {
      if (auto result = t.field(materialize(x)); !result.empty())
        return result;
      return caf::none;
    },
    [&](auto&, auto&) {
      return x;
    },
  };
  return caf::visit(v, x, t);
}

data_view to_internal(const type& t, const data_view& x) {
  auto v = detail::overload{
    [](const view<std::string>& s, const enumeration_type& t) -> data_view {
      if (auto key = t.resolve(s))
        return detail::narrow_cast<enumeration>(*key);
      return caf::none;
    },
    [&](auto&, auto&) {
      return x;
    },
  };
  return caf::visit(v, x, t);
}

} // namespace vast
