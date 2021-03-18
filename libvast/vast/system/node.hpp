// SPDX-FileCopyrightText: (c) 2016 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/aliases.hpp"
#include "vast/command.hpp"
#include "vast/error.hpp"
#include "vast/path.hpp"
#include "vast/system/actors.hpp"
#include "vast/system/component_registry.hpp"
#include "vast/system/spawn_arguments.hpp"

#include <caf/actor.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

#include <chrono>
#include <map>
#include <string>

namespace vast::system {

/// State of the node actor.
struct node_state {
  // -- remote command infrastructure ------------------------------------------

  /// Command callback for spawning components in the node.
  static caf::message
  spawn_command(const invocation& inv, caf::actor_system& sys);

  /// Spawns a component for the NODE with given spawn arguments.
  using component_factory_fun
    = caf::expected<caf::actor> (*)(node_actor::stateful_pointer<node_state>,
                                    spawn_arguments&);

  /// Maps command names to a component factory.
  using named_component_factory = std::map<std::string, component_factory_fun>;

  /// Maps command names (including parent command) to spawn functions.
  inline static named_component_factory component_factory = {};

  /// Maps command names to functions.
  inline static command::factory command_factory = {};

  // -- actor facade -----------------------------------------------------------

  /// The name of the NODE actor.
  std::string name = "node";

  /// A pointer to the NODE actor handle.
  node_actor::pointer self = {};

  // -- member types -----------------------------------------------------------

  /// Stores the base directory for persistent state.
  path dir = {};

  /// The component registry.
  component_registry registry = {};

  /// Counters for multi-instance components.
  std::unordered_map<std::string, uint64_t> label_counters = {};
};

/// Spawns a node.
/// @param self The actor handle
/// @param name The unique name of the node.
/// @param dir The directory where to store persistent state.
/// @param shutdown_grace_period Time to give components to shutdown cleanly.
node_actor::behavior_type
node(node_actor::stateful_pointer<node_state> self, std::string name, path dir,
     std::chrono::milliseconds shutdown_grace_period);

} // namespace vast::system
