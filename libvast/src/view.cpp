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

#include "vast/view.hpp"

#include <regex>

#include "vast/detail/narrow.hpp"
#include "vast/detail/overload.hpp"
#include "vast/type.hpp"

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

// -- default_vector_view -----------------------------------------------------

default_vector_view::default_vector_view(const vector& xs) : xs_{xs} {
  // nop
}

default_vector_view::value_type default_vector_view::at(size_type i) const {
  return make_data_view(xs_[i]);
}

default_vector_view::size_type default_vector_view::size() const noexcept {
  return xs_.size();
}

// -- default_set_view --------------------------------------------------------

default_set_view::default_set_view(const set& xs) : xs_{xs} {
  // nop
}

default_set_view::value_type default_set_view::at(size_type i) const {
  return make_data_view(*std::next(xs_.begin(), i));
}

default_set_view::size_type default_set_view::size() const noexcept {
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

// -- make_view ---------------------------------------------------------------

data_view make_view(const data& x) {
  return caf::visit([](const auto& z) { return make_data_view(z); }, x);
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

template <class Result, class T>
Result materialize_container(const T& xs) {
  Result result;
  if (xs)
    for (auto x : *xs)
      result.insert(result.end(), materialize(x));
  return result;
}

} // namespace <anonymous>

vector materialize(vector_view_handle xs) {
  return materialize_container<vector>(xs);
}

set materialize(set_view_handle xs) {
  return materialize_container<set>(xs);
}

map materialize(map_view_handle xs) {
  return materialize_container<map>(xs);
}

data materialize(data_view x) {
  return caf::visit([](auto y) { return data{materialize(y)}; }, x);
}

// WARNING: making changes to the logic of this function requires adapting the
// companion overload in type.cpp.
bool type_check(const type& t, const data_view& x) {
  auto f = detail::overload(
    [&](const auto& u) {
      using data_type = type_to_data<std::decay_t<decltype(u)>>;
      return caf::holds_alternative<view<data_type>>(x);
    },
    [&](const none_type&) {
      // Cannot determine data type since data may always be null.
      return true;
    },
    [&](const enumeration_type& u) {
      auto e = caf::get_if<view<enumeration>>(&x);
      return e && *e < u.fields.size();
    },
    [&](const vector_type& u) {
      auto v = caf::get_if<view<vector>>(&x);
      if (!v)
        return false;
      auto& xs = **v;
      return xs.empty() || type_check(u.value_type,  xs.at(0));
    },
    [&](const set_type& u) {
      auto v = caf::get_if<view<set>>(&x);
      if (!v)
        return false;
      auto& xs = **v;
      return xs.empty() || type_check(u.value_type, xs.at(0));
    },
    [&](const map_type& u) {
      auto v = caf::get_if<view<map>>(&x);
      if (!v)
        return false;
      auto& xs = **v;
      if (xs.empty())
        return true;
      auto [key, value] = xs.at(0);
      return type_check(u.key_type, key) && type_check(u.value_type, value);
    },
    [&](const record_type& u) {
      // Until we have a separate data type for records we treat them as vector.
      auto v = caf::get_if<view<vector>>(&x);
      if (!v)
        return false;
      auto& xs = **v;
      if (xs.size() != u.fields.size())
        return false;
      for (size_t i = 0; i < xs.size(); ++i)
        if (!type_check(u.fields[i].type, xs.at(i)))
          return false;
      return true;
    },
    [&](const alias_type& u) {
      return type_check(u.value_type, x);
    }
  );
  return caf::holds_alternative<caf::none_t>(x) || caf::visit(f, t);
}

namespace {

// Checks whether the left-hand side is contained in the right-hand side.
struct contains_predicate {
  template <class T, class U>
  bool operator()(const T& lhs, const U& rhs) const {
    if constexpr (detail::is_any_v<U, view<vector>, view<set>>) {
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

  bool operator()(const view<std::string>& lhs,
                  const view<std::string>& rhs) const {
    return rhs.find(lhs) != std::string::npos;
  }

  bool operator()(const view<std::string>& lhs,
                  const view<pattern>& rhs) const {
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
    return caf::visit(detail::overload([](auto, auto) { return false; },
                                       [](view<std::string>& lhs,
                                          view<pattern> rhs) {
                                         return rhs.match(lhs);
                                       }),
                      x, y);
  };
  auto check_in = [](const auto& x, const auto& y) {
    return caf::visit(contains_predicate{}, x, y);
  };
  switch (op) {
    default:
      VAST_ASSERT(!"missing case");
      return false;
    case match:
      return check_match(lhs, rhs);
    case not_match:
      return !check_match(lhs, rhs);
    case in:
      return check_in(lhs, rhs);
    case not_in:
      return !check_in(lhs, rhs);
    case ni:
      return check_in(rhs, lhs);
    case not_ni:
      return !check_in(rhs, lhs);
    case equal:
      return lhs == rhs;
    case not_equal:
      return lhs != rhs;
    case less:
      return lhs < rhs;
    case less_equal:
      return lhs <= rhs;
    case greater:
      return lhs > rhs;
    case greater_equal:
      return lhs >= rhs;
  }
}

data_view to_canonical(const type& t, const data_view& x) {
  auto v = detail::overload(
    [](const view<enumeration>& x, const enumeration_type& t) -> data_view {
      if (materialize(x) >= t.fields.size())
        return caf::none;
      return make_view(t.fields[materialize(x)]);
    },
    [&](auto&, auto&) { return x; });
  return caf::visit(v, x, t);
}

data_view to_internal(const type& t, const data_view& x) {
  auto v = detail::overload(
    [](const view<std::string>& s, const enumeration_type& t) -> data_view {
      auto i = std::find(t.fields.begin(), t.fields.end(), s);
      if (i == t.fields.end())
        return caf::none;
      return detail::narrow_cast<enumeration>(
        std::distance(t.fields.begin(), i));
    },
    [&](auto&, auto&) { return x; });
  return caf::visit(v, x, t);
}

} // namespace vast
