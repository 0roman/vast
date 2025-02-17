//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/fwd.hpp"

#include "vast/defaults.hpp"
#include "vast/format/writer.hpp"
#include "vast/type.hpp"

#include <arrow/io/api.h>
#include <arrow/ipc/writer.h>
#include <caf/error.hpp>
#include <caf/expected.hpp>

#include <memory>
#include <vector>

namespace vast::format::arrow {

/// An Arrow writer.
class writer : public format::writer {
public:
  using output_stream_ptr = std::shared_ptr<::arrow::io::OutputStream>;

  using batch_writer_ptr = std::shared_ptr<::arrow::ipc::RecordBatchWriter>;

  writer();
  writer(writer&&) = default;
  writer& operator=(writer&&) = default;
  ~writer() override;

  explicit writer(const caf::settings& options);

  caf::error write(const table_slice& x) override;

  const char* name() const override;

  void out(output_stream_ptr ptr) {
    out_ = std::move(ptr);
  }

  bool layout(const std::shared_ptr<::arrow::Schema>& schema);

private:
  output_stream_ptr out_;
  type current_layout_;
  table_slice_builder_ptr current_builder_;
  batch_writer_ptr current_batch_writer_;
};

} // namespace vast::format::arrow
