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

#include "vast/system/task.hpp"

#include "vast/error.hpp"
#include "vast/logger.hpp"

#include <caf/event_based_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

using namespace caf;

namespace vast::system {

namespace {

template <class Actor>
void notify(Actor self) {
  for (auto& s : self->state.subscribers)
    self->send(s, atom::progress_v, uint64_t{self->state.workers.size()},
               self->state.total);
  if (self->state.workers.empty()) {
    for (auto& s : self->state.supervisors)
      self->send(s, self->state.done_msg);
    self->quit();
  }
}

template <class Actor>
void complete(Actor self, const actor_addr& a) {
  auto w = self->state.workers.find(a);
  if (w == self->state.workers.end()) {
    VAST_ERROR(self, "got completion signal from unknown actor:", a);
    self->quit(caf::make_error(ec::unspecified, "got DONE from unknown actor"));
  } else if (--w->second == 0) {
    self->demonitor(a);
    self->state.workers.erase(w);
    notify(self);
  }
}

} // namespace <anonymous>

behavior task_impl(stateful_actor<task_state>* self, message done_msg) {
  self->state.done_msg = std::move(done_msg);
  self->set_exit_handler(
    [=](const exit_msg& msg) {
      self->state.subscribers.clear();
      notify(self);
      self->quit(msg.reason);
    }
  );
  self->set_down_handler(
    [=](const down_msg& msg) {
      if (self->state.workers.erase(msg.source) == 1)
        notify(self);
    }
  );
  return {
    [=](const actor& a) {
      VAST_TRACE(self, "registers actor", a);
      self->monitor(a);
      if (++self->state.workers[a.address()] == 1)
        ++self->state.total;
    },
    [=](const actor& a, uint64_t n) {
      VAST_TRACE(self, "registers actor", a, "for", n, "sub-tasks");
      self->monitor(a);
      self->state.workers[a.address()] += n;
      ++self->state.total;
    },
    [=](atom::done, const actor_addr& addr) {
      VAST_TRACE(self, "manually completed actor with address", addr);
      complete(self, addr);
    },
    [=](atom::done) {
      VAST_TRACE(self, "completed actor", self->current_sender());
      complete(self, actor_cast<actor_addr>(self->current_sender()));
    },
    [=](atom::supervisor, const actor& a) {
      VAST_TRACE(self, "notifies actor", a, "about task completion");
      self->state.supervisors.insert(a);
    },
    [=](atom::subscriber, const actor& a) {
      VAST_TRACE(self, "notifies actor", a, "on task status change");
      self->state.subscribers.insert(a);
    },
    [=](atom::progress) {
      auto num_workers = uint64_t{self->state.workers.size()};
      return make_message(num_workers, self->state.total);
    },
  };
}

} // namespace vast::system
