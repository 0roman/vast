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

#include <unordered_map>

#include <caf/config_value.hpp>
#include <caf/none.hpp>

#include "vast/data.hpp"

#include "vast/system/key_value_store.hpp"

namespace vast::system {

template <class Key, class Value>
struct data_store_state {
  std::unordered_map<Key, Value> store;
  static inline const char* name = "data-store";
};

/// A key-value store that stores its data in a `std::unordered_map`.
/// @param self The actor handle.
template <class Key, class Value>
typename key_value_store_type<Key, Value>::behavior_type
data_store(
  typename key_value_store_type<Key, Value>::template stateful_pointer<
    data_store_state<Key, Value>
  > self) {
  return {
    [=](atom::put, const Key& key, Value& value) {
      self->state.store[key] = std::move(value);
      return atom::ok_v;
    },
    [=](atom::add, const Key& key, const Value& value) -> caf::result<Value> {
      auto& v = self->state.store[key];
      auto old = v;
      v += value;
      return old;
    },
    [=](atom::erase, const Key& key) {
      self->state.store.erase(key);
      return atom::ok_v;
    },
    [=](atom::get, const Key& key) -> caf::result<optional<Value>> {
      auto i = self->state.store.find(key);
      if (i == self->state.store.end())
        return caf::none;
      return i->second;
    },
    [=](atom::status) -> caf::config_value::dictionary {
      caf::dictionary<caf::config_value> result;
      result.emplace("store-size", self->state.store.size());
      return result;
    },
  };
}

} // namespace vast::system


