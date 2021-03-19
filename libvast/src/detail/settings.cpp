//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2020 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/detail/settings.hpp"

#include "vast/concept/parseable/vast/si.hpp"
#include "vast/detail/type_traits.hpp"
#include "vast/logger.hpp"

namespace vast::detail {

namespace {

template <class Policy>
void merge_settings_impl(const caf::settings& src, caf::settings& dst,
                         Policy policy, size_t depth = 0) {
  if (depth > 100) {
    VAST_ERROR("Exceeded maximum nesting depth in settings.");
    return;
  }
  for (auto& [key, value] : src) {
    if (caf::holds_alternative<caf::settings>(value)) {
      merge_settings_impl(caf::get<caf::settings>(value),
                          dst[key].as_dictionary(), policy, depth + 1);
    } else {
      if constexpr (std::is_same_v<Policy, policy::merge_lists_tag>) {
        if (caf::holds_alternative<caf::config_value::list>(value)
            && caf::holds_alternative<caf::config_value::list>(dst[key])) {
          const auto& src_list = caf::get<caf::config_value::list>(value);
          auto& dst_list = dst[key].as_list();
          dst_list.insert(dst_list.end(), src_list.begin(), src_list.end());
        } else {
          dst.insert_or_assign(key, value);
        }
      } else if constexpr (std::is_same_v<Policy, policy::overwrite_lists_tag>) {
        dst.insert_or_assign(key, value);
      } else {
        static_assert(detail::always_false_v<Policy>, "unsupported merge "
                                                      "policy");
      }
    }
  }
}

} // namespace

void merge_settings(const caf::settings& src, caf::settings& dst,
                    policy::overwrite_lists_tag policy) {
  return merge_settings_impl(src, dst, policy);
}

void merge_settings(const caf::settings& src, caf::settings& dst,
                    policy::merge_lists_tag policy) {
  return merge_settings_impl(src, dst, policy);
}

bool strip_settings(caf::settings& xs) {
  auto& m = xs.container();
  for (auto it = m.begin(); it != m.end();) {
    if (auto x = caf::get_if<caf::settings>(&it->second)) {
      if (x->empty()) {
        it = m.erase(it);
      } else {
        if (strip_settings(*x))
          it = m.erase(it);
        else
          ++it;
      }
    } else
      ++it;
  }
  return m.empty();
}

caf::expected<uint64_t>
get_bytesize(caf::settings opts, std::string_view key, uint64_t defval) {
  // Note that there's no `caf::has_key()` and e.g. `caf::get_or<std::string>`
  // would silently take the default value if the key exists but is not a
  // string, so we have to make a copy of `opts` and use `caf::put_missing()`
  // as a workaround.
  size_t result = 0;
  caf::put_missing(opts, key, defval);
  if (caf::holds_alternative<size_t>(opts, key)) {
    result = caf::get<size_t>(opts, key);
  } else if (caf::holds_alternative<std::string>(opts, key)) {
    auto result_str = caf::get<std::string>(opts, key);
    if (!parsers::bytesize(result_str, result))
      return caf::make_error(ec::parse_error, "could not parse '" + result_str
                                                + "' as valid byte size");
  } else {
    return caf::make_error(ec::invalid_argument,
                           "invalid value for key '" + std::string{key} + "'");
  }
  return result;
}

} // namespace vast::detail
