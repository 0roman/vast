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

#include "vast/system/dummy_consensus.hpp"

#include <caf/all.hpp>
#include <caf/config_value.hpp>
#include <caf/none.hpp>

#include "vast/filesystem.hpp"
#include "vast/load.hpp"
#include "vast/save.hpp"

#include "vast/detail/fill_status_map.hpp"

namespace vast::system {

dummy_consensus_state::dummy_consensus_state(actor_ptr self) : self{self} {
  // nop
}

caf::error dummy_consensus_state::init(path dir) {
  file = std::move(dir) / "store";
  if (exists(file)) {
    if (auto err = vast::load(&self->system(), file, store)) {
      VAST_WARNING_ANON(name, "unable to load state file:", file);
      return err;
    }
  }
  return caf::none;
}

caf::error dummy_consensus_state::save() {
  return vast::save(&self->system(), file, store);
}

caf::dictionary<caf::config_value> dummy_consensus_state::status() const {
  caf::dictionary<caf::config_value> result;
  // Misc parameters.
  result.emplace("store-size", store.size());
  result.emplace("store-file", file.str());
  // General state such as open streams.
  detail::fill_status_map(result, self);
  return result;
}

/// A key-value store that stores its data in a `std::unordered_map`.
/// @param self The actor handle.
consensus_type::behavior_type
dummy_consensus(dummy_consensus_state::actor_ptr self, path dir) {
  using behavior_type = consensus_type::behavior_type;
  if (auto err = self->state.init(std::move(dir))) {
    self->quit(std::move(err));
    return behavior_type::make_empty_behavior();
  }
  return {[=](atom::put, const std::string& key,
              data& value) -> caf::result<atom::ok> {
            self->state.store[key] = std::move(value);
            if (auto err = self->state.save())
              return err;
            return atom::ok_v;
          },
          [=](atom::add, const std::string& key,
              const data& value) -> caf::result<data> {
            auto& v = self->state.store[key];
            auto old = v;
            v += value;
            if (auto err = self->state.save())
              return err;
            return old;
          },
          [=](atom::erase, const std::string& key) -> caf::result<atom::ok> {
            self->state.store.erase(key);
            if (auto err = self->state.save())
              return err;
            return atom::ok_v;
          },
          [=](atom::get,
              const std::string& key) -> caf::result<optional<data>> {
            auto i = self->state.store.find(key);
            if (i == self->state.store.end())
              return caf::none;
            return i->second;
          },
          [=](atom::status) -> caf::config_value::dictionary {
            return self->state.status();
          }};
}

} // namespace vast::system
