//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2020 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "vast/fwd.hpp"

#include "vast/span.hpp"

#include <cstddef>
#include <filesystem>

namespace vast::io {

/// Performs a one-shot write of an immutable buffer into a file.
/// @param filename The file to write to.
/// @param xs The buffer to read from.
/// @returns An error if the operation failed.
caf::error
write(const std::filesystem::path& filename, span<const std::byte> xs);

} // namespace vast::io
