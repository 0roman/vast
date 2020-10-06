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

#include <utility>

namespace vast::detail {

/// Creates a set of overloaded functions. This utility struct allows for
/// writing inline visitors without having to result to inversion of control.
template <class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};

/// Explicit deduction guide for overload (not needed as of C++20).
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

} // namespace vast::detail

