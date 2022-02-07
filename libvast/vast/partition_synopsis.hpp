//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "vast/detail/friend_attribute.hpp"
#include "vast/fbs/partition_synopsis.hpp"
#include "vast/qualified_record_field.hpp"
#include "vast/synopsis.hpp"
#include "vast/table_slice.hpp"
#include "vast/uuid.hpp"

namespace caf {

// Forward declaration to be able to befriend the unshare implementation.
template <typename T>
T* default_intrusive_cow_ptr_unshare(T*&);

} // namespace caf

namespace vast {

/// Contains one synopsis per partition column.
struct partition_synopsis final : public caf::ref_counted {
  partition_synopsis() = default;
  partition_synopsis(partition_synopsis&&) = default;

  partition_synopsis& operator=(partition_synopsis&&) = default;

  /// Add data to the synopsis.
  void add(const table_slice& slice, const caf::settings& synopsis_options);

  /// Optimizes the partition synopsis contents for size.
  /// @related buffered_synopsis
  void shrink();

  /// Estimate the memory footprint of this partition synopsis.
  /// @returns A best-effort estimate of the amount of memory used by this
  ///          synopsis.
  size_t memusage() const;

  /// Id of the first event in the partition.
  uint64_t offset = invalid_id;

  // Number of events in the partition.
  uint64_t events = 0;

  /// The minimum import timestamp of all contained table slices.
  time min_import_time = time::max();

  /// The maximum import timestamp of all contained table slices.
  time max_import_time = time::min();

  /// Synopsis data structures for types.
  std::unordered_map<type, synopsis_ptr> type_synopses_;

  /// Synopsis data structures for individual columns.
  std::unordered_map<qualified_record_field, synopsis_ptr> field_synopses_;

  // -- flatbuffer -------------------------------------------------------------

  FRIEND_ATTRIBUTE_NODISCARD friend caf::expected<
    flatbuffers::Offset<fbs::partition_synopsis::LegacyPartitionSynopsis>>
  pack(flatbuffers::FlatBufferBuilder& builder, const partition_synopsis&);

  FRIEND_ATTRIBUTE_NODISCARD friend caf::error
  unpack(const fbs::partition_synopsis::LegacyPartitionSynopsis&,
         partition_synopsis&);

  FRIEND_ATTRIBUTE_NODISCARD friend caf::error
  unpack(const fbs::partition_synopsis::LegacyPartitionSynopsis& x,
         partition_synopsis& ps, uint64_t offset, uint64_t events);

private:
  // Returns a raw pointer to a deep copy of this partition synopsis.
  // For use by the `caf::intrusive_cow_ptr`.
  friend partition_synopsis* ::caf::default_intrusive_cow_ptr_unshare<
    partition_synopsis>(partition_synopsis*& ptr);
  partition_synopsis* copy() const;
};

struct partition_synopsis_pair {
  vast::uuid uuid;
  partition_synopsis_ptr synopsis;
};

} // namespace vast
