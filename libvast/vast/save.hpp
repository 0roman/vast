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

#include "vast/compression.hpp"
#include "vast/detail/compressedbuf.hpp"
#include "vast/detail/type_traits.hpp"
#include "vast/error.hpp"
#include "vast/filesystem.hpp"
#include "vast/logger.hpp"

#include <caf/actor_system.hpp>
#include <caf/expected.hpp>
#include <caf/stream_serializer.hpp>

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <streambuf>
#include <type_traits>

namespace vast {

/// Serializes a sequence of objects.
/// @param sys An (optional) actor system.
/// @param out The sink to serialize into.
/// @param xs The instances to serialize.
/// @see load
template <compression Method = compression::null, class Sink, class... Ts>
caf::error save(caf::actor_system* sys, Sink&& out, const Ts&... xs) {
  static_assert(sizeof...(Ts) > 0);
  using sink_type = std::decay_t<Sink>;
  if constexpr (detail::is_streambuf_v<sink_type>) {
    auto ctx = sys ? sys->dummy_execution_unit() : nullptr;
    if constexpr (Method == compression::null) {
      caf::stream_serializer<sink_type&> s{ctx, out};
      if (auto err = s(xs...))
        return err;
    } else {
      detail::compressedbuf compressed{out, Method};
      caf::stream_serializer<detail::compressedbuf&> s{ctx, compressed};
      if (auto err = s(xs...))
        return err;
      compressed.pubsync();
    }
    return caf::none;
  } else if constexpr (std::is_base_of_v<std::ostream, sink_type>) {
    auto sb = out.rdbuf();
    return save<Method>(sys, *sb, xs...);
  } else if constexpr (detail::is_contiguous_byte_container_v<sink_type>) {
    caf::containerbuf<sink_type> sink{out};
    return save<Method>(sys, sink, xs...);
  } else if constexpr (std::is_same_v<sink_type, path>) {
    if (auto dir = out.parent(); !exists(dir)) {
      VAST_DEBUG_ANON(__func__, "creates directory", dir);
      if (auto res = mkdir(dir); !res) {
        VAST_DEBUG_ANON(__func__, "failed to create directory", dir);
        return res.error();
      }
    }
    auto tmp = out + ".tmp";
    if (exists(tmp))
      VAST_WARNING_ANON(__func__, "discovered temporary file:", tmp);
    std::ofstream fs{tmp.str()};
    if (!fs)
      return make_error(ec::filesystem_error, "failed to create filestream",
                        out, std::strerror(errno));
    if (auto err = save<Method>(sys, *fs.rdbuf(), xs...))
      return err;
    if (std::rename(tmp.str().c_str(), out.str().c_str()) != 0)
      return make_error(ec::filesystem_error, "failed to rename to", out);
    return caf::none;
  } else {
    static_assert(!std::is_same_v<Sink, Sink>, "unexpected Sink type");
  }
}

} // namespace vast
