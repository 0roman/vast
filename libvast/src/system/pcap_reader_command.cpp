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

#include "vast/system/pcap_reader_command.hpp"

#include <string>
#include <string_view>

#include <caf/event_based_actor.hpp>
#include <caf/scoped_actor.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

#include "vast/defaults.hpp"
#include "vast/detail/assert.hpp"
#include "vast/error.hpp"
#include "vast/format/pcap.hpp"
#include "vast/logger.hpp"
#include "vast/scope_linked.hpp"
#include "vast/system/signal_monitor.hpp"
#include "vast/system/source.hpp"
#include "vast/system/source_command.hpp"
#include "vast/system/spawn_or_connect_to_node.hpp"

namespace vast::system {

caf::message pcap_reader_command(const command& cmd, caf::actor_system& sys,
                                 caf::settings& options,
                                 command::argument_iterator first,
                                 command::argument_iterator last) {
  VAST_TRACE(VAST_ARG(options), VAST_ARG("args", first, last));
  using reader_t = format::pcap::reader;
  using defaults_t = defaults::import::pcap;
  std::string category = defaults_t::category;
  reader_t reader{defaults::import::table_slice_type(sys, options),
                  options,
                  get_or(options, category + ".read", defaults_t::read),
                  get_or(options, category + ".cutoff", defaults_t::cutoff),
                  get_or(options, category + ".max-flows",
                         defaults_t::max_flows),
                  get_or(options, category + ".max-flow-age",
                         defaults_t::max_flow_age),
                  get_or(options, category + ".flow-expiry",
                         defaults_t::flow_expiry),
                  get_or(options, category + ".pseudo-realtime-factor",
                         defaults_t::pseudo_realtime_factor)};
  auto src = sys.spawn(default_source<format::pcap::reader>, std::move(reader));
  return source_command(cmd, sys, std::move(src), options, first, last);
}

} // namespace vast::system
