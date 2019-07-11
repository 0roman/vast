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

#include <tuple>

#include "vast/event.hpp"
#include "vast/json.hpp"

namespace vast {

event::event(caf::none_t) {
  // nop
}

event::event(value v) : value{std::move(v)} {
  // nop
}

bool event::id(vast::id i) {
  if (i <= max_id) {
    id_ = i;
    return true;
  }
  return false;
}

id event::id() const {
  return id_;
}

void event::timestamp(time ts) {
  timestamp_ = ts;
}

time event::timestamp() const {
  return timestamp_;
}

event flatten(const event& e) {
  event result = flatten(static_cast<const value&>(e));
  result.id(e.id());
  result.timestamp(e.timestamp());
  return result;
}

bool operator==(const event& x, const event& y) {
  return x.id() == y.id() &&
    x.timestamp() == y.timestamp() &&
    static_cast<const value&>(x) == static_cast<const value&>(y);
}

bool operator<(const event& x, const event& y) {
  return std::tie(x.id_, x.timestamp_, static_cast<const value&>(x)) <
    std::tie(y.id_, y.timestamp_, static_cast<const value&>(y));
}

bool convert(const event& e, json& j) {
  json::object o;
  o["id"] = e.id();
  o["timestamp"] = e.timestamp().time_since_epoch().count();
  if (!convert(static_cast<const value&>(e), o["value"]))
    return false;
  j = std::move(o);
  return true;
}
} // namespace vast
