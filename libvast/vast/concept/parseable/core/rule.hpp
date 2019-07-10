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

#pragma once

#include <memory>
#include <utility>

#include "vast/concept/parseable/core/parser.hpp"
#include "vast/detail/assert.hpp"
#include "vast/detail/type_traits.hpp"

namespace vast {
namespace detail {

template <class Iterator, class Attribute>
struct abstract_rule {
  virtual ~abstract_rule() = default;
  virtual bool parse(Iterator& f, const Iterator& l, unused_type) const = 0;
  virtual bool parse(Iterator& f, const Iterator& l, Attribute& a) const = 0;
};

template <class Parser, class Iterator, class Attribute>
class rule_definition : public abstract_rule<Iterator, Attribute> {
public:
  explicit rule_definition(Parser p) : parser_(std::move(p)) {
  }

  bool parse(Iterator& f, const Iterator& l, unused_type) const override {
    return parser_(f, l, unused);
  }

  bool parse(Iterator& f, const Iterator& l, Attribute& a) const override {
    return parser_(f, l, a);
  }

private:
  Parser parser_;
};

} // namespace detail

/// A type-erased parser which can store any other parser.
template <class Iterator, class Attribute = unused_type>
class rule : public parser<rule<Iterator, Attribute>> {
  using abstract_rule_type = detail::abstract_rule<Iterator, Attribute>;
  using rule_pointer = std::unique_ptr<abstract_rule_type>;

  template <class RHS>
  void make_parser(RHS&& rhs) {
    // TODO:
    // static_assert(is_compatible_attribute<RHS, typename RHS::attribute>{},
    //              "incompatible parser attributes");
    using rule_type = detail::rule_definition<RHS, Iterator, Attribute>;
    *parser_ = std::make_unique<rule_type>(std::forward<RHS>(rhs));
  }

public:
  using attribute = Attribute;

  rule() : parser_{std::make_shared<rule_pointer>()} {
  }

  template <
    class RHS,
    class = std::enable_if_t<
      is_parser_v<std::decay_t<RHS>> && !detail::is_same_or_derived_v<rule, RHS>
    >
  >
  rule(RHS&& rhs)
    : rule{} {
    make_parser<RHS>(std::forward<RHS>(rhs));
  }

  template <class RHS>
  auto operator=(RHS&& rhs)
    -> std::enable_if_t<is_parser_v<std::decay_t<RHS>>
                        && !detail::is_same_or_derived_v<rule, RHS>> {
    make_parser<RHS>(std::forward<RHS>(rhs));
  }

  bool parse(Iterator& f, const Iterator& l, unused_type) const {
    VAST_ASSERT(*parser_ != nullptr);
    return (*parser_)->parse(f, l, unused);
  }

  bool parse(Iterator& f, const Iterator& l, Attribute& a) const {
    VAST_ASSERT(*parser_ != nullptr);
    return (*parser_)->parse(f, l, a);
  }

  const std::shared_ptr<rule_pointer>& parser() const {
    return parser_;
  }

private:
  std::shared_ptr<rule_pointer> parser_;
};

/// A type-erased, non-owning reference to a parser.
template <class Iterator, class Attribute = unused_type>
class rule_ref : public parser<rule_ref<Iterator, Attribute>> {
  using abstract_rule_type = detail::abstract_rule<Iterator, Attribute>;
  using rule_pointer = std::unique_ptr<abstract_rule_type>;

  template <class RHS>
  void make_parser(RHS&& rhs) {
    // TODO:
    // static_assert(is_compatible_attribute<RHS, typename RHS::attribute>{},
    //              "incompatible parser attributes");
    using rule_type = detail::rule_definition<RHS, Iterator, Attribute>;
    *parser_ = std::make_unique<rule_type>(std::forward<RHS>(rhs));
  }

public:
  using attribute = Attribute;

  explicit rule_ref(const rule<Iterator, Attribute>& x) : parser_(x.parser()) {
    // nop
  }

  rule_ref(rule_ref&&) = default;

  rule_ref(const rule_ref&) = default;

  rule_ref& operator=(rule_ref&&) = default;

  rule_ref& operator=(const rule_ref&) = default;

  bool parse(Iterator& f, const Iterator& l, unused_type x) const {
    auto ptr = parser_.lock();
    VAST_ASSERT(ptr != nullptr);
    return (*ptr)->parse(f, l, x);
  }

  bool parse(Iterator& f, const Iterator& l, Attribute& x) const {
    auto ptr = parser_.lock();
    VAST_ASSERT(ptr != nullptr);
    return (*ptr)->parse(f, l, x);
  }

private:
  std::weak_ptr<rule_pointer> parser_;
};

template <class Iterator, class Attribute>
auto ref(const rule<Iterator, Attribute>& x) {
  return rule_ref<Iterator, Attribute>{x};
}

template <class Iterator, class Attribute>
auto ref(rule<Iterator, Attribute>& x) {
  return rule_ref<Iterator, Attribute>{x};
}

} // namespace vast

