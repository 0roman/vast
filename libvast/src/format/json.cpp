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

#include <caf/detail/pretty_type_name.hpp>
#include <caf/expected.hpp>
#include <caf/none.hpp>

#include "vast/concept/parseable/vast/address.hpp"
#include "vast/concept/parseable/vast/json.hpp"
#include "vast/concept/parseable/vast/port.hpp"
#include "vast/concept/parseable/vast/subnet.hpp"
#include "vast/concept/parseable/vast/time.hpp"
#include "vast/concept/printable/to_string.hpp"
#include "vast/concept/printable/vast/json.hpp"
#include "vast/data.hpp"
#include "vast/detail/unbox_var.hpp"
#include "vast/format/json.hpp"
#include "vast/logger.hpp"
#include "vast/table_slice.hpp"
#include "vast/table_slice_builder.hpp"
#include "vast/type.hpp"
#include "vast/view.hpp"

namespace vast::format::json {
namespace {

struct convert {
  template <class T>
  using expected = caf::expected<T>;
  using json = vast::json;

  expected<data> operator()(json::boolean b, const bool_type&) const {
    return b;
  }

  expected<data> operator()(json::number n, const integer_type&) const {
    return detail::narrow_cast<integer>(n);
  }

  expected<data> operator()(json::number n, const count_type&) const {
    return detail::narrow_cast<count>(n);
  }

  expected<data> operator()(json::number n, const real_type&) const {
    return detail::narrow_cast<real>(n);
  }

  expected<data> operator()(json::number n, const port_type&) const {
    return port{detail::narrow_cast<port::number_type>(n)};
  }

  expected<data> operator()(json::number s, const timestamp_type&) const {
    auto secs = std::chrono::duration<json::number>(s);
    auto since_epoch = std::chrono::duration_cast<timespan>(secs);
    return timestamp{since_epoch};
  }

  expected<data> operator()(json::number s, const timespan_type&) const {
    auto secs = std::chrono::duration<json::number>(s);
    return std::chrono::duration_cast<timespan>(secs);
  }

  expected<data> operator()(json::string s, const string_type&) const {
    return s;
  }

  template <class T,
            typename std::enable_if_t<has_parser_v<type_to_data<T>>, int> = 0>
  expected<data> operator()(const json::string& s, const T&) const {
    using value_type = type_to_data<T>;
    value_type x;
    if (!make_parser<value_type>{}(s, x))
      return make_error(ec::parse_error, "unable to parse",
                        caf::detail::pretty_type_name(typeid(value_type)), ":",
                        s);
    return x;
  }

  expected<data> operator()(const json::string& s,
                            const enumeration_type& e) const {
    auto i = std::find(e.fields.begin(), e.fields.end(), s);
    if (i == e.fields.end())
      return make_error(ec::parse_error, "invalid:", s);
    return std::distance(e.fields.begin(), i);
  }

  expected<data> operator()(const json::array& a, const set_type& s) const {
    set xs;
    xs.reserve(a.size());
    for (auto& x : a) {
      if (auto elem = caf::visit(*this, x, s.value_type))
        xs.insert(*std::move(elem));
      else
        return elem;
    }
    return xs;
  }

  expected<data> operator()(const json::array& a, const vector_type& v) const {
    vector xs;
    xs.reserve(a.size());
    for (auto& x : a) {
      if (auto elem = caf::visit(*this, x, v.value_type))
        xs.push_back(*std::move(elem));
      else
        return elem;
    }
    return xs;
  }

  expected<data> operator()(const json::object& o, const map_type& m) const {
    map xs;
    xs.reserve(o.size());
    for (auto& [k, v] : o) {
      VAST_UNBOX_VAR(key, this->operator()(k, m.key_type));
      VAST_UNBOX_VAR(val, caf::visit(*this, v, m.value_type));
      xs[key] = val;
    }
    return xs;
  }

  template <class T, class U>
  expected<data> operator()(T, U) const {
    VAST_ERROR_ANON("json-reader cannot convert from",
                    caf::detail::pretty_type_name(typeid(T)), "to",
                    caf::detail::pretty_type_name(typeid(U)));
    return false;
  }
};

const vast::json* lookup(std::string_view field, const vast::json::object& xs) {
  VAST_ASSERT(!field.empty());
  auto lookup_flat = [&]() {
    auto r = xs.find(field);
    return r == xs.end() ? nullptr : &r->second;
  };
  auto i = field.find('.');
  if (i == std::string_view::npos)
    return lookup_flat();
  // We have to deal with a nested field name in a potentially nested JSON
  // object.
  auto r = xs.find(field.substr(0, i));
  if (r == xs.end())
    // Attempt to access JSON field with flattened name.
    return lookup_flat();
  auto obj = caf::get_if<vast::json::object>(&r->second);
  if (obj == nullptr)
    return nullptr;
  field.remove_prefix(i + 1);
  return lookup(field, *obj);
}

} // namespace

caf::error writer::write(const table_slice& x) {
  json_printer<policy::oneline> printer;
  return print<true>(printer, x, "{", ", ", "}");
}

const char* writer::name() const {
  return "json-writer";
}

caf::error add(table_slice_builder& builder, const vast::json::object& xs,
               const record_type& layout) {
  for (auto& field : layout.fields) {
    auto i = lookup(field.name, xs);
    // Non-existing fields are treated as empty (unset).
    if (!i) {
      builder.add(make_data_view(caf::none));
      continue;
    }
    auto x = caf::visit(convert{}, *i, field.type);
    if (!x)
      return make_error(ec::convert_error, x.error().context(),
                        "could not convert", field.name, ":", to_string(*i));
    if (!builder.add(make_data_view(*x)))
      return make_error(ec::type_clash, "unexpected type", field.name, ":",
                        to_string(*i));
  }
  return caf::none;
}

} // namespace vast::format::json
