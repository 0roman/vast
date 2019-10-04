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

#include "vast/system/start_command.hpp"

#include <csignal>
#include <iostream>
#include <thread>

#include "vast/config.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>
#ifdef VAST_USE_OPENSSL
#include <caf/openssl/all.hpp>
#endif // VAST_USE_OPENSSL

#include "vast/concept/parseable/vast/endpoint.hpp"
#include "vast/defaults.hpp"
#include "vast/endpoint.hpp"
#include "vast/error.hpp"
#include "vast/logger.hpp"
#include "vast/scope_linked.hpp"
#include "vast/system/atoms.hpp"
#include "vast/system/signal_monitor.hpp"
#include "vast/system/spawn_node.hpp"

namespace vast::system {

using namespace std::chrono_literals;

caf::message start_command_impl(start_command_extra_steps extra_steps,
                                const command::invocation& invocation,
                                caf::actor_system& sys) {
  VAST_TRACE(VAST_ARG(invocation.options),
             VAST_ARG("args", invocation.arguments.begin(),
                      invocation.arguments.end()));
  // Fetch SSL settings from config.
  auto& sys_cfg = sys.config();
  auto use_encryption = !sys_cfg.openssl_certificate.empty()
                        || !sys_cfg.openssl_key.empty()
                        || !sys_cfg.openssl_passphrase.empty()
                        || !sys_cfg.openssl_capath.empty()
                        || !sys_cfg.openssl_cafile.empty();
  // Construct an endpoint.
  endpoint node_endpoint;
  auto str
    = get_or(invocation.options, "system.endpoint", defaults::system::endpoint);
  if (!parsers::endpoint(str, node_endpoint))
    return caf::make_message(
      make_error(ec::parse_error, "invalid endpoint", str));
  // Get a convenient and blocking way to interact with actors.
  caf::scoped_actor self{sys};
  // Spawn our node.
  auto node_opt = spawn_node(self, content(sys.config()));
  if (!node_opt)
    return caf::make_message(std::move(node_opt.error()));
  auto& node = node_opt->get();
  // Publish our node.
  auto host = node_endpoint.host.empty() ? nullptr : node_endpoint.host.c_str();
  auto publish = [&]() -> caf::expected<uint16_t> {
    if (use_encryption)
#ifdef VAST_USE_OPENSSL
      return caf::openssl::publish(node, node_endpoint.port.number(), host);
#else
      return make_error(ec::unspecified, "not compiled with OpenSSL support");
#endif
    auto& mm = sys.middleman();
    auto reuse_address = true;
    return mm.publish(node, node_endpoint.port.number(), host, reuse_address);
  };
  auto bound_port = publish();
  if (!bound_port)
    return caf::make_message(std::move(bound_port.error()));
  VAST_INFO_ANON("VAST node is listening on", (host ? host : "")
                 << ':' << *bound_port);
  // Run user-defined extra code.
  if (extra_steps != nullptr)
    if (auto err = extra_steps(self, invocation.options, node))
      return caf::make_message(std::move(err));
  // Start signal monitor.
  std::thread sig_mon_thread;
  auto guard = signal_monitor::run_guarded(sig_mon_thread, sys, 750ms, self);
  // Run main loop.
  caf::error err;
  auto stop = false;
  self->monitor(node);
  self->do_receive(
    [&](caf::down_msg& msg) {
      VAST_ASSERT(msg.source == node);
      VAST_DEBUG_ANON("... received DOWN from node");
      stop = true;
      if (msg.reason != caf::exit_reason::user_shutdown)
        err = std::move(msg.reason);
    },
    [&](system::signal_atom, int signal) {
      VAST_DEBUG_ANON("... got " << ::strsignal(signal));
      if (signal == SIGINT || signal == SIGTERM)
        self->send_exit(node, caf::exit_reason::user_shutdown);
      else
        self->send(node, system::signal_atom::value, signal);
    }
  ).until([&] { return stop; });
  return caf::make_message(std::move(err));
}

caf::message
start_command(const command::invocation& invocation, caf::actor_system& sys) {
  VAST_TRACE(VAST_ARG(invocation.options),
             VAST_ARG("args", invocation.arguments.begin(),
                      invocation.arguments.end()));
  return start_command_impl(nullptr, invocation, sys);
}

} // namespace vast::system
