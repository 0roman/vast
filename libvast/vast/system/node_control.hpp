// SPDX-FileCopyrightText: (c) 2019 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/atoms.hpp"
#include "vast/command.hpp"
#include "vast/detail/actor_cast_wrapper.hpp"
#include "vast/detail/assert.hpp"
#include "vast/detail/tuple_map.hpp"
#include "vast/error.hpp"
#include "vast/logger.hpp"
#include "vast/system/actors.hpp"

#include <caf/actor.hpp>
#include <caf/expected.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/typed_actor.hpp>

#include <array>
#include <string>
#include <string_view>

namespace vast::system {

caf::expected<caf::actor>
spawn_at_node(caf::scoped_actor& self, node_actor node, invocation inv);

/// Look up components by their typed actor interfaces. Returns the first actor
/// of each type passed as template parameter.
template <class... Actors>
caf::expected<std::tuple<Actors...>>
get_node_components(caf::scoped_actor& self, const node_actor& node) {
  using result_t = std::tuple<Actors...>;
  auto result = caf::expected{result_t{}};
  auto normalize = [](std::string in) {
    // Remove the uninteresting parts of the name:
    //   vast::system::type_registry_actor -> type_registry
    in.erase(0, sizeof("vast::system::") - 1);
    in.erase(in.size() - (sizeof("_actor") - 1));
    // Replace '_' with '-': type_registry -> type-registry
    std::replace(in.begin(), in.end(), '_', '-');
    return in;
  };
  auto labels = std::vector<std::string>{
    normalize(caf::type_name_by_id<caf::type_id<Actors>::value>::value)...};
  self
    ->request(node, caf::infinite, atom::get_v, atom::label_v,
              std::move(labels))
    .receive(
      [&](std::vector<caf::actor> components) {
        result = detail::tuple_map<result_t>(std::move(components),
                                             detail::actor_cast_wrapper{});
      },
      [&](caf::error e) { //
        result = std::move(e);
      });
  return result;
}

} // namespace vast::system
