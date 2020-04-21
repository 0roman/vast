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

#include <memory>

#include <caf/fwd.hpp>

#include "vast/config.hpp"

namespace vast::system {

// -- classes ------------------------------------------------------------------

class application;
class configuration;
class default_application;
class export_command;
class import_command;
class node_command;
class pcap_reader_command;
class pcap_writer_command;
class remote_command;
class sink_command;
class source_command;
class start_command;

// -- structs ------------------------------------------------------------------

#ifdef VAST_MEASUREMENT_MUTEX_WORKAROUND
struct atomic_measurement;
#endif
struct measurement;
struct node_state;
struct query_status;
struct spawn_arguments;

// -- aliases ------------------------------------------------------------------

#ifndef VAST_MEASUREMENT_MUTEX_WORKAROUND
using atomic_measurement = std::atomic<measurement>;
#endif
using node_actor = caf::stateful_actor<node_state>;


} // namespace vast::system
