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

#include "vast/defaults.hpp"
#include "vast/fwd.hpp"
#include "vast/logger.hpp"
#include "vast/system/terminator.hpp"

#include <caf/actor_cast.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/fwd.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/stateful_actor.hpp>

#include <vector>

namespace vast::policy {

struct sequential;
struct parallel;

} // namespace vast::policy

namespace vast::system {

/// Performs an asynchronous shutdown of a set of actors by sending an EXIT
/// message, configurable either in sequential or parallel mode of operation.
/// As soon as all actors have terminated, the returned promise gets fulfilled.
/// This function is the lower-level interface for bringing down actors. The
/// function `shutdown` uses this function internally to implement a more
/// convenient one-stop solution.
/// @param self The actor to terminate.
/// @param xs The actors to terminate.
/// @param grace_period The amount of time to wait until all actors terminated
///        cleanly.
/// @param kill_timeout The timeout befor giving and delivering an error to the
///        response promise.
/// @returns A response promise to be fulfilled when all *xs* terminated.
/// @relates shutdown
template <class Policy, class Actor>
auto terminate(Actor&& self, std::vector<caf::actor> xs,
               std::chrono::seconds grace_period
               = defaults::system::shutdown_grace_period,
               std::chrono::seconds kill_timeout
               = defaults::system::shutdown_kill_timeout) {
  auto t = self->spawn(terminator<Policy>, grace_period, kill_timeout);
  auto request_timeout = grace_period + kill_timeout;
  if (request_timeout > std::chrono::seconds::zero())
    return self->request(std::move(t), request_timeout, std::move(xs));
  else
    return self->request(std::move(t), caf::infinite, std::move(xs));
}

template <class Policy, class Actor>
auto terminate(Actor&& self, caf::actor x,
               std::chrono::seconds grace_period
               = defaults::system::shutdown_grace_period,
               std::chrono::seconds kill_timeout
               = defaults::system::shutdown_kill_timeout) {
  return terminate<Policy>(std::forward<Actor>(self),
                           std::vector<caf::actor>{std::move(x)}, grace_period,
                           kill_timeout);
}

} // namespace vast::system
