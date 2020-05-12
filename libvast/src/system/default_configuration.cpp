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

#include "vast/system/default_configuration.hpp"

#include "vast/defaults.hpp"

#include <caf/atom.hpp>
#include <caf/config_value.hpp>

namespace vast::system {

default_configuration::default_configuration() {
  // Tweak default logging options.
  using caf::atom;
  set("logger.component-blacklist",
      caf::make_config_value_list(atom("caf"), atom("caf_flow"),
                                  atom("caf_stream")));
  set("logger.console", atom("COLORED"));
  set("logger.console-format", defaults::logger::console_format);
  set("logger.console-verbosity", defaults::logger::console_verbosity);
  set("logger.file-verbosity", defaults::logger::file_verbosity);
  // Allow VAST clusters to form a mesh.
  set("middleman.enable-automatic-connections", true);
}

} // namespace vast::system
