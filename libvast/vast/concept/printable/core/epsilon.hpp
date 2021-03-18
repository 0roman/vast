// SPDX-FileCopyrightText: (c) 2016 Tenzir GmbH <info@tenzir.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/concept/printable/core/printer.hpp"

namespace vast {

class epsilon_printer : public printer<epsilon_printer> {
public:
  using attribute = unused_type;

  template <class Iterator>
  bool print(Iterator&, unused_type) const {
    return true;
  }
};

namespace printers {

auto const eps = epsilon_printer{};

} // namespace printers
} // namespace vast


