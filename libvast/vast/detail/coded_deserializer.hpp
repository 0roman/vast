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
#include <string>
#include <type_traits>
#include <utility>

#include <caf/stream_deserializer.hpp>

#include "vast/error.hpp"

#include "vast/detail/assert.hpp"
#include "vast/detail/zigzag.hpp"

namespace vast::detail {

template <class Streambuf>
class coded_deserializer : public caf::stream_deserializer<Streambuf> {
  using super = caf::stream_deserializer<Streambuf>;
  using builtin = typename super::builtin;

public:
  using super::super;

protected:
  template <class T>
  error zig_zag_varbyte_decode(T& x) {
    static_assert(std::is_signed_v<T>, "T must be an signed type");
    auto u = std::make_unsigned_t<T>{0};
    if (auto e = this->varbyte_decode(u))
      return e;
    x = zigzag::decode(u);
    return caf::none;
  }

  error apply_builtin(builtin type, void* val) override {
    VAST_ASSERT(val != nullptr);
    switch (type) {
      default:
        VAST_ASSERT(type == builtin::i8_v || type == builtin::u8_v);
        return this->apply_raw(sizeof(uint8_t), val);
      case builtin::i16_v:
        return zig_zag_varbyte_decode(
          *reinterpret_cast<int16_t*>(std::launder(val)));
      case builtin::i32_v:
        return zig_zag_varbyte_decode(
          *reinterpret_cast<int32_t*>(std::launder(val)));
      case builtin::i64_v:
        return zig_zag_varbyte_decode(
          *reinterpret_cast<int64_t*>(std::launder(val)));
      case builtin::u16_v:
        return this->varbyte_decode(
          *reinterpret_cast<uint16_t*>(std::launder(val)));
      case builtin::u32_v:
        return this->varbyte_decode(
          *reinterpret_cast<uint32_t*>(std::launder(val)));
      case builtin::u64_v:
        return this->varbyte_decode(
          *reinterpret_cast<uint64_t*>(std::launder(val)));
      case builtin::float_v:
        return this->apply_float(*reinterpret_cast<float*>(std::launder(val)));
      case builtin::double_v:
        return this->apply_float(*reinterpret_cast<double*>(std::launder(val)));
      case builtin::ldouble_v: {
        // the IEEE-754 conversion does not work for long double
        // => fall back to string serialization (even though it sucks)
        std::string tmp;
        auto e = this->apply(tmp);
        if (e)
          return e;
        std::istringstream iss{std::move(tmp)};
        iss >> *reinterpret_cast<long double*>(std::launder(val));
        return caf::none;
      }
      case builtin::string8_v: {
        auto& str = *reinterpret_cast<std::string*>(std::launder(val));
        size_t size;
        return error::eval([&] { return this->begin_sequence(size); },
                           [&] { str.resize(size);
                                 return this->apply_raw(size, &str[0]); },
                           [&] { return this->end_sequence(); });
      }
      case builtin::string16_v: {
        auto& str = *reinterpret_cast<std::u16string*>(std::launder(val));
        str.clear();
        size_t ns;
        return error::eval([&] { return this->begin_sequence(ns); },
                           [&] { return this->template
                                   fill_range_c<uint16_t>(str, ns); },
                           [&] { return this->end_sequence(); });
      }
      case builtin::string32_v: {
        auto& str = *reinterpret_cast<std::u32string*>(std::launder(val));
        str.clear();
        size_t ns;
        return error::eval([&] { return this->begin_sequence(ns); },
                           [&] { return this->template
                                   fill_range_c<uint32_t>(str, ns); },
                           [&] { return this->end_sequence(); });
      }
    }
  }
};

} // namespace vast::detail

