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

#include "vast/concept/printable/core/printer.hpp"
#include "vast/concept/printable/string/string.hpp"
#include "vast/path.hpp"

namespace vast {

struct path_printer : printer<path_printer> {
  using attribute = path;

  template <class Iterator>
  bool print(Iterator& out, const path& p) const {
    return printers::str.print(out, p.str());
  }
};

template <>
struct printer_registry<path> {
  using type = path_printer;
};

} // namespace vast

