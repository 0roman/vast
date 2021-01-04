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

#include <unordered_map>

#include "vast/format/reader.hpp"
#include "vast/table_slice_builder.hpp"

namespace vast::format {

/// Base class for readers that deal with multiple layouts.
class multi_layout_reader : public reader {
public:
  multi_layout_reader(const caf::settings& options);

  ~multi_layout_reader() override;

protected:
  /// Convenience function for finishing the current table slice in
  /// `builder_ptr` before reporting an error. Usually simply returns `result`
  /// after finishing the slice, however, an error in finishing the slice
  /// "overrides" `result`.
  /// @param f Consumer for the finished slice.
  /// @param builder_ptr Pointer to the table slice builder we want to "flush".
  /// @param result Current status of the parent context, usually returned
  ///               unmodified.
  /// @returns `result`, unless `builder_ptr->finish()` fails.
  caf::error finish(consumer& f, table_slice_builder_ptr& builder_ptr,
                    caf::error result = caf::none);

  /// Convenience function for finishing all table slices in all table slice
  /// builders before reporting an error. Usually simply returns `result` after
  /// finishing the slice, however, an error in finishing the slice "overrides"
  /// `result`.
  /// @param f Consumer for the finished slice.
  /// @param result Current status of the parent context, usually returned
  ///               unmodified.
  /// @returns `result`, unless any `finish()` call fails.
  caf::error finish(consumer& f, caf::error result = caf::none);

  /// @returns a table slice builder for given type, creating it on-the-fly if
  ///          necessary.
  table_slice_builder_ptr builder(const type& t);

  std::unordered_map<type, table_slice_builder_ptr> builders_;
};

} // namespace vast::format
