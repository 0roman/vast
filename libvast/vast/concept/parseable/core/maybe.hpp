// SPDX-FileCopyrightText: (c) 2016 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/concept/parseable/core/parser.hpp"

namespace vast {

/// Like ::optional_parser, but exposes `T` instead of `optional<T>` as
/// attribute.
template <class Parser>
class maybe_parser : public parser<maybe_parser<Parser>> {
public:
  using attribute = typename Parser::attribute;

  constexpr explicit maybe_parser(Parser p) : parser_{std::move(p)} {
  }

  template <class Iterator, class Attribute>
  bool parse(Iterator& f, const Iterator& l, Attribute& a) const {
    parser_(f, l, a);
    return true;
  }

private:
  Parser parser_;
};

} // namespace vast
