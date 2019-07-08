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

#include <cstddef>
#include <cstdint>

#include "caf/meta/type_name.hpp"

#include "vast/time.hpp"

namespace vast::system {

/// Statistics about a query.
struct query_status {
  duration runtime;            ///< Current runtime.
  size_t expected = 0;         ///< Expected ID sets from INDEX.
  size_t scheduled = 0;        ///< Scheduled partitions (ID sets) at INDEX.
  size_t received = 0;         ///< Received ID sets from INDEX.
  size_t lookups_issued = 0;   ///< Number of lookups sent to the ARCHIVE.
  size_t lookups_complete = 0; ///< Number of lookups returned by the ARCHIVE.
  uint64_t processed = 0;      ///< Processed candidates from ARCHIVE.
  uint64_t shipped = 0;        ///< Shipped results to the SINK.
  uint64_t requested = 0;      ///< User-requested pending results to extract.
  uint64_t cached = 0;         ///< Currently available results for the SINK.
};

template <class Inspector>
auto inspect(Inspector& f, query_status& qs) {
  return f(caf::meta::type_name("query_status"), qs.runtime, qs.expected,
           qs.scheduled, qs.received, qs.lookups_issued, qs.lookups_complete,
           qs.processed, qs.shipped, qs.requested);
}

} // namespace vast::system
