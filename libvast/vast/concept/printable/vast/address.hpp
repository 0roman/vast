//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstring>

#include "vast/address.hpp"
#include "vast/concept/printable/core/printer.hpp"
#include "vast/concept/printable/string/string.hpp"

namespace vast {

struct address_printer : vast::printer<address_printer> {
  using attribute = address;

  template <class Iterator>
  bool print(Iterator& out, const address& a) const {
    char buf[INET6_ADDRSTRLEN];
    std::memset(buf, 0, sizeof(buf));
    auto result = a.is_v4()
      ? inet_ntop(AF_INET, &a.data()[12], buf, INET_ADDRSTRLEN)
      : inet_ntop(AF_INET6, &a.data(), buf, INET6_ADDRSTRLEN);
    return result != nullptr && printers::str.print(out, result);
  }
};

template <>
struct printer_registry<address> {
  using type = address_printer;
};

namespace printers {

auto const addr = address_printer{};

} // namespace printers
} // namespace vast
