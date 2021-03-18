// SPDX-FileCopyrightText: (c) 2019 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "vast/concept/parseable/core/ignore.hpp"
#include "vast/concept/parseable/core/parser.hpp"
#include "vast/concept/parseable/string/char.hpp"
#include "vast/concept/parseable/string/string.hpp"

namespace vast {

// -- unary -------------------------------------------------------------------

constexpr auto to_parser(char c) {
  return ignore(parsers::chr{c});
}

inline auto to_parser(std::string str) {
  return ignore(parsers::str{std::move(str)});
}

template <class T>
constexpr auto to_parser(T x)
  -> std::enable_if_t<std::conjunction_v<std::is_arithmetic<T>,
                                         std::negation<std::is_same<T, bool>>>,
                      decltype(ignore(string_parser{""}))> {
  return ignore(parsers::str{std::to_string(x)});
}

template <class T>
constexpr auto to_parser(T x) -> std::enable_if_t<is_parser_v<T>, T> {
  return x; // A good compiler will elide the copy.
}

// -- binary ------------------------------------------------------------------

template <class T>
constexpr bool is_convertible_to_unary_parser_v
  = std::is_convertible_v<
      T, std::string> || (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>);

template <class T, class U>
constexpr bool is_convertible_to_binary_parser_v
  = (is_parser_v<T> && is_parser_v<U>)
    || (is_parser_v<T> && is_convertible_to_unary_parser_v<U>)
    || (is_convertible_to_unary_parser_v<T> && is_parser_v<U>);

// clang-format off
template <
  template <class, class> class BinaryParser,
  class T,
  class U
>
using make_binary_parser =
  std::conditional_t<
    is_parser_v<T> && is_parser_v<U>,
    BinaryParser<T, U>,
    std::conditional_t<
      is_parser_v<T> && is_convertible_to_unary_parser_v<U>,
      BinaryParser<T, decltype(to_parser(std::declval<U>()))>,
      std::conditional_t<
        is_convertible_to_unary_parser_v<T> && is_parser_v<U>,
        BinaryParser<decltype(to_parser(std::declval<T>())), U>,
        std::false_type
      >
    >
  >;
// clang-format on

template <template <class, class> class BinaryParser, class T, class U>
constexpr auto to_parser(T&& x, U&& y) -> std::enable_if_t<
  is_convertible_to_binary_parser_v<std::decay_t<T>, std::decay_t<U>>,
  make_binary_parser<BinaryParser, decltype(to_parser(std::forward<T>(x))),
                     decltype(to_parser(std::forward<U>(y)))>> {
  return {to_parser(std::forward<T>(x)), to_parser(std::forward<U>(y))};
}

} // namespace vast
