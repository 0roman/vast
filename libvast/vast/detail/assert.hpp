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

#include "vast/config.hpp"

#if VAST_ENABLE_ASSERTIONS
#  if __has_include(<execinfo.h>)
#    include <cstdio>
#    include <cstdlib>
#    include <execinfo.h>
#    define VAST_ASSERT(expr)                                                  \
      if (static_cast<bool>(expr) == false) {                                  \
        ::printf("%s:%u: assertion failed '%s'\n", __FILE__, __LINE__, #expr); \
        void* vast_array[10];                                                  \
        auto vast_bt_size = ::backtrace(vast_array, 10);                       \
        ::backtrace_symbols_fd(vast_array, vast_bt_size, 2);                   \
        ::abort();                                                             \
      }                                                                        \
      static_cast<void>(0)
#  else
#    include <cstdio>
#    include <cstdlib>
#    define VAST_ASSERT(expr)                                                  \
      if (static_cast<bool>(expr) == false) {                                  \
        ::printf("%s:%u: assertion failed '%s'\n", __FILE__, __LINE__, #expr); \
        ::abort();                                                             \
      }                                                                        \
      static_cast<void>(0)
#  endif
#else
#  define VAST_ASSERT(expr) static_cast<void>(expr)
#endif
