// SPDX-FileCopyrightText: (c) 2016 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/system/signal_monitor.hpp"

#include "vast/fwd.hpp"

#include "vast/atoms.hpp"
#include "vast/logger.hpp"

#include <caf/actor.hpp>
#include <caf/send.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

using namespace caf;

namespace {

// Keeps track of all signals by their value from 1 to 31. The flag at index 0
// is used to tell whether a signal has been raised or not.
std::atomic<bool> signals[32];

extern "C" void signal_monitor_handler(int sig) {
  // Catch termination signals only once to allow forced termination by the OS
  // upon sending the signal a second time.
  if (sig == SIGINT || sig == SIGTERM) {
    std::cerr << "\rinitiating graceful shutdown... (repeat request to "
                 "terminate immediately)\n";
    std::signal(sig, SIG_DFL);
  }
  signals[0] = true;
  signals[sig] = true;
}

} // namespace <anonymous>

namespace vast::system {

std::atomic<bool> signal_monitor::stop;

void signal_monitor::run(std::chrono::milliseconds monitoring_interval,
                         actor receiver) {
  [[maybe_unused]] static constexpr auto class_name = "signal_monitor";
  VAST_DEBUG("{} sends signals to {}", class_name, receiver);
  for (auto s : {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2}) {
    VAST_DEBUG("{} registers signal handler for {}", class_name, strsignal(s));
    std::signal(s, &signal_monitor_handler);
  }
  while (!stop) {
    std::this_thread::sleep_for(monitoring_interval);
    if (signals[0]) {
      // TODO: this handling of singals is fundamentally unsafe, because we
      //       always have a race between the singal handler and this loop on
      //       singals[0]. This needs to be re-implemented in a truly atomic
      //       fashion, probably via CAS operaions and a single 32-bit integer.
      signals[0] = false;
      for (int i = 1; i < 32; ++i) {
        if (signals[i]) {
          VAST_DEBUG("{} caught signal {}", class_name, strsignal(i));
          signals[i] = false;
          caf::anon_send(receiver, atom::signal_v, i);
        }
      }
    }
  }
}

} // namespace vast::system
