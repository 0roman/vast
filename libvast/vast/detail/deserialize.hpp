// SPDX-FileCopyrightText: (c) 2020 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/as_bytes.hpp"
#include "vast/detail/type_traits.hpp"
#include "vast/span.hpp"

#include <caf/binary_deserializer.hpp>
#include <caf/error.hpp>

namespace vast::detail {

/// Deserializes a sequence of objects from a byte buffer.
/// @param buffer The vector of bytes to read from.
/// @param xs The object to deserialize.
/// @returns The status of the operation.
/// @relates detail::serialize
template <class Buffer, class... Ts,
          class = std::enable_if_t<detail::is_byte_container_v<Buffer>>>
caf::error deserialize(const Buffer& buffer, Ts&&... xs) {
  auto bytes = as_bytes(buffer);
  auto data = reinterpret_cast<const char*>(bytes.data());
  caf::binary_deserializer deserializer{nullptr, data, bytes.size()};
  return deserializer(xs...);
}

} // namespace vast::detail
