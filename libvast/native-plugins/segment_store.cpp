//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include <vast/atoms.hpp>
#include <vast/detail/overload.hpp>
#include <vast/detail/zip_iterator.hpp>
#include <vast/fbs/partition.hpp>
#include <vast/fbs/utils.hpp>
#include <vast/ids.hpp>
#include <vast/logger.hpp>
#include <vast/plugin.hpp>
#include <vast/query.hpp>
#include <vast/segment_store.hpp>
#include <vast/system/actors.hpp>
#include <vast/system/node_control.hpp>
#include <vast/system/report.hpp>
#include <vast/system/status.hpp>
#include <vast/table_slice.hpp>

#include <caf/settings.hpp>
#include <caf/typed_event_based_actor.hpp>
#include <flatbuffers/flatbuffers.h>
#include <fmt/format.h>

#include <chrono>
#include <span>
#include <vector>

namespace vast::plugins::segment_store {

namespace {

/// The STORE BUILDER actor interface.
using local_store_actor
  = system::typed_actor_fwd<caf::reacts_to<atom::internal, atom::persist>>::
    extend_with<system::store_builder_actor>::unwrap;

struct active_store_state {
  /// Defaulted constructor to make this a non-aggregate.
  active_store_state() = default;

  /// A pointer to the hosting actor.
  //  We intentionally store a strong pointer here: The store lifetime is
  //  ref-counted, it should exit after all currently active queries for this
  //  store have finished, its partition has dropped out of the cache, and it
  //  received all data from the incoming stream. This pointer serves to keep
  //  the ref-count alive for the last part, and is reset after the data has
  //  been written to disk.
  local_store_actor self = {};

  /// Actor handle of the accountant.
  system::accountant_actor accountant = {};

  /// Actor handle of the filesystem.
  system::filesystem_actor fs = {};

  /// The path to where the store will be written.
  std::filesystem::path path = {};

  /// The builder preparing the store.
  //  TODO: Store a vector<table_slice> here and implement
  //  store/lookup/.. on that.
  std::unique_ptr<vast::segment_builder> builder = {};

  /// The serialized segment.
  std::optional<vast::segment> segment = {};

  /// Number of events in this store.
  size_t events = {};

  /// The name for this type of CAF actor.
  static constexpr const char* name = "active-local-store";
};

struct passive_store_state {
  /// Defaulted constructor to make this a non-aggregate.
  passive_store_state() = default;

  /// Holds requests that did arrive while the segment data
  /// was still being loaded from disk.
  using request
    = std::tuple<vast::query, caf::typed_response_promise<uint64_t>>;
  std::vector<request> deferred_requests = {};

  /// Holds erase requests that did arrive while the segment data
  /// was still being loaded from disk.
  using erasure = std::tuple<vast::ids, caf::typed_response_promise<uint64_t>>;
  std::vector<erasure> deferred_erasures = {};

  /// Actor handle of the accountant.
  system::accountant_actor accountant = {};

  /// The actor handle of the filesystem actor.
  system::filesystem_actor fs = {};

  /// The path where the segment is stored.
  std::filesystem::path path = {};

  /// The segment corresponding to this local store.
  caf::optional<vast::segment> segment = {};

  /// The name for this type of CAF actor.
  static constexpr const char* name = "passive-local-store";

private:
  caf::result<atom::done> erase(const vast::ids&);
};

// Handler for `vast::query` that is shared between active and passive stores.
// Returns a the number of events that match the query.
// Precondition: Query type is either `count` or `extract`.
template <typename Actor>
caf::expected<uint64_t> handle_lookup(Actor& self, const vast::query& query,
                                      const std::vector<table_slice>& slices) {
  const auto& ids = query.ids;
  std::vector<expression> checkers;
  uint64_t num_hits = 0ull;
  for (const auto& slice : slices) {
    if (query.expr == expression{}) {
      checkers.emplace_back();
    } else {
      auto c = tailor(query.expr, slice.layout());
      if (!c)
        return c.error();
      checkers.emplace_back(prune_meta_predicates(std::move(*c)));
    }
  }
  auto handle_query = detail::overload{
    [&](const query::count& count) {
      if (count.mode == query::count::estimate)
        die("logic error detected");
      for (size_t i = 0; i < slices.size(); ++i) {
        const auto& slice = slices.at(i);
        const auto& checker = checkers.at(i);
        auto result = count_matching(slice, checker, ids);
        num_hits += result;
        self->send(count.sink, result);
      }
    },
    [&](const query::extract& extract) {
      VAST_ASSERT(slices.size() == checkers.size());
      for (size_t i = 0; i < slices.size(); ++i) {
        const auto& slice = slices[i];
        const auto& checker = checkers[i];
        if (extract.policy == query::extract::preserve_ids) {
          for (auto& sub_slice : select(slice, ids)) {
            if (query.expr == expression{}) {
              self->send(extract.sink, sub_slice);
              num_hits += sub_slice.rows();
            } else {
              auto hits = evaluate(checker, sub_slice);
              num_hits += rank(hits);
              for (auto& final_slice : select(sub_slice, hits))
                self->send(extract.sink, final_slice);
            }
          }
        } else {
          auto final_slice = filter(slice, checker, ids);
          if (final_slice) {
            num_hits += final_slice->rows();
            self->send(extract.sink, *final_slice);
          }
        }
      }
    },
  };
  caf::visit(handle_query, query.cmd);
  return num_hits;
}

std::filesystem::path
store_path_from_header(std::span<const std::byte> header) {
  std::string_view sv{reinterpret_cast<const char*>(header.data()),
                      header.size()};
  return std::filesystem::path{sv};
}

system::store_actor::behavior_type passive_local_store(
  system::store_actor::stateful_pointer<passive_store_state> self,
  system::accountant_actor accountant, system::filesystem_actor fs,
  const std::filesystem::path& path) {
  self->state.accountant = std::move(accountant);
  self->state.fs = std::move(fs);
  self->state.path = path;
  self->set_exit_handler([self](const caf::exit_msg&) {
    for (auto&& [expr, rp] : std::exchange(self->state.deferred_requests, {}))
      rp.deliver(caf::make_error(ec::lookup_error, "partition store shutting "
                                                   "down"));
    for (auto&& [expr, rp] : std::exchange(self->state.deferred_erasures, {}))
      rp.deliver(caf::make_error(ec::lookup_error, "partition store shutting "
                                                   "down"));
  });
  VAST_DEBUG("{} loads passive store from path {}", *self, path);
  self->request(self->state.fs, caf::infinite, atom::mmap_v, path)
    .then(
      [self](chunk_ptr chunk) {
        auto seg = segment::make(std::move(chunk));
        if (!seg) {
          VAST_ERROR("couldnt create segment from chunk: {}", seg.error());
          self->send_exit(self, caf::exit_reason::unhandled_exception);
          return;
        }
        self->state.segment = std::move(*seg);
        // Delegate all deferred evaluations now that we have the partition chunk.
        VAST_DEBUG("{} delegates {} deferred evaluations", *self,
                   self->state.deferred_requests.size());
        for (auto&& [query, rp] :
             std::exchange(self->state.deferred_requests, {})) {
          VAST_TRACE("{} delegates {} (pending: {})", *self, query,
                     rp.pending());
          rp.delegate(static_cast<system::store_actor>(self), std::move(query));
        }
        for (auto&& [ids, rp] :
             std::exchange(self->state.deferred_erasures, {})) {
          VAST_TRACE("{} delegates erase (pending: {})", *self, rp.pending());
          rp.delegate(static_cast<system::store_actor>(self), atom::erase_v,
                      std::move(ids));
        }
      },
      [self](caf::error& err) {
        VAST_ERROR("{} could not map passive store segment into memory: {}",
                   *self, render(err));
        for (auto&& [_, rp] : std::exchange(self->state.deferred_requests, {}))
          rp.deliver(err);
        for (auto&& [_, rp] : std::exchange(self->state.deferred_erasures, {}))
          rp.deliver(err);
        self->quit(std::move(err));
      });
  return {
    [self](query query) -> caf::result<uint64_t> {
      VAST_DEBUG("{} handles new query", *self);
      if (!self->state.segment) {
        auto rp = self->make_response_promise<uint64_t>();
        self->state.deferred_requests.emplace_back(query, rp);
        return rp;
      }
      auto start = std::chrono::steady_clock::now();
      auto slices = self->state.segment->lookup(query.ids);
      if (!slices)
        return slices.error();
      auto num_hits = handle_lookup(self, query, *slices);
      if (!num_hits)
        return num_hits.error();
      duration runtime = std::chrono::steady_clock::now() - start;
      auto id_str = fmt::to_string(query.id);
      self->send(
        self->state.accountant, "segment-store.lookup.runtime", runtime,
        system::metrics_metadata{{"query", id_str}, {"store-type", "passive"}});
      self->send(self->state.accountant, "segment-store.lookup.hits", *num_hits,
                 system::metrics_metadata{{"query", id_str},
                                          {"store-type", "passive"}});
      return *num_hits;
    },
    [self](atom::erase, ids xs) -> caf::result<uint64_t> {
      if (!self->state.segment) {
        // Treat this as an "erase" query for the purposes of storing it
        // until the segment is loaded.
        auto rp = self->make_response_promise<uint64_t>();
        self->state.deferred_erasures.emplace_back(std::move(xs), rp);
        return rp;
      }
      auto segment_ids = self->state.segment->ids();
      auto segment_size = rank(segment_ids);
      auto intersection = segment_ids & xs;
      auto intersection_size = rank(intersection);
      VAST_DEBUG("{} erases {} of {} events", *self, intersection_size,
                 segment_size);
      if (is_subset(self->state.segment->ids(), xs)) {
        VAST_VERBOSE("{} gets wholly erased from {}", *self, self->state.path);
        // There is a (small) chance one or more lookups are currently still in
        // progress, so we dont call `self->quit()` here but instead rely on
        // ref-counting. The lookups can still finish normally because the
        // `mmap()` is still valid even after the underlying segment file was
        // removed.
        auto rp = self->make_response_promise<uint64_t>();
        self
          ->request(self->state.fs, caf::infinite, atom::erase_v,
                    self->state.path)
          .then(
            [rp, intersection_size](atom::done) mutable {
              rp.deliver(intersection_size);
            },
            [rp](caf::error& err) mutable {
              rp.deliver(std::move(err));
            });
        return rp;
      }
      auto new_segment = segment::copy_without(*self->state.segment, xs);
      if (!new_segment) {
        VAST_ERROR("could not remove ids from segment {}: {}",
                   self->state.segment->id(), render(new_segment.error()));
        return new_segment.error();
      }
      VAST_ASSERT(self->state.path.has_filename());
      auto old_path = self->state.path;
      auto new_path = self->state.path.replace_extension("next");
      auto rp = self->make_response_promise<uint64_t>();
      self
        ->request(self->state.fs, caf::infinite, atom::write_v, new_path,
                  new_segment->chunk())
        .then(
          [seg = std::move(*new_segment), self, old_path, new_path, rp,
           intersection_size](atom::ok) mutable {
            std::error_code ec;
            // Re-use the old filename so that we don't have to write a new
            // partition flatbuffer with the changed store header as well.
            std::filesystem::rename(new_path, old_path, ec);
            if (ec)
              VAST_ERROR("failed to erase old data {}", seg.id());
            self->state.segment = std::move(seg);
            rp.deliver(intersection_size);
          },
          [rp](caf::error& err) mutable {
            VAST_ERROR("failed to flush archive {}", to_string(err));
            rp.deliver(err);
          });
      return rp;
    },
  };
}

local_store_actor::behavior_type
active_local_store(local_store_actor::stateful_pointer<active_store_state> self,
                   system::accountant_actor accountant,
                   system::filesystem_actor fs,
                   const std::filesystem::path& path) {
  VAST_DEBUG("spawning active local store");
  self->state.self = self;
  self->state.accountant = std::move(accountant);
  self->state.fs = std::move(fs);
  self->state.path = path;
  self->state.builder
    = std::make_unique<segment_builder>(defaults::system::max_segment_size);
  self->set_exit_handler([self](const caf::exit_msg&) {
    VAST_DEBUG("active local store exits");
    // TODO: We should save the finished segment in the state, so we can
    //       answer queries that arrive after the stream has ended.
    self->quit();
  });
  auto result = local_store_actor::behavior_type{
    // store api
    [self](const query& query) -> caf::result<uint64_t> {
      auto start = std::chrono::steady_clock::now();
      caf::expected<std::vector<table_slice>> slices = caf::error{};
      if (self->state.builder) {
        slices = self->state.builder->lookup(query.ids);
      } else {
        VAST_ASSERT(self->state.segment.has_value());
        slices = self->state.segment->lookup(query.ids);
      }
      if (!slices)
        return slices.error();
      auto num_hits = handle_lookup(self, query, *slices);
      if (!num_hits)
        return num_hits.error();
      duration runtime = std::chrono::steady_clock::now() - start;
      auto id_str = fmt::to_string(query.id);
      self->send(
        self->state.accountant, "segment_store.lookup.runtime", runtime,
        system::metrics_metadata{{"query", id_str}, {"store_type", "active"}});
      self->send(self->state.accountant, "segment_store.lookup.hits", *num_hits,
                 system::metrics_metadata{{"query", id_str},
                                          {"store_type", "active"}});
      return *num_hits;
    },
    [self](const atom::erase&, const ids& ids) -> caf::result<uint64_t> {
      // TODO: There is a race here when ids are erased while we're waiting
      // for the filesystem actor to finish.
      auto seg = self->state.builder->finish();
      auto id = seg.id();
      auto slices = seg.erase(ids);
      if (!slices)
        return slices.error();
      self->state.builder->reset(id);
      for (auto&& slice : std::exchange(*slices, {}))
        self->state.builder->add(std::move(slice));
      return rank(self->state.builder->ids());
    },
    // store builder
    [self](
      caf::stream<table_slice> in) -> caf::inbound_stream_slot<table_slice> {
      return self
        ->make_sink(
          in, [=](caf::unit_t&) {},
          [=](caf::unit_t&, std::vector<table_slice>& batch) {
            VAST_TRACE("{} gets batch of {} table slices", *self, batch.size());
            for (auto& slice : batch) {
              if (auto error = self->state.builder->add(slice))
                VAST_ERROR("{} failed to add table slice to store {}", *self,
                           render(error));
              self->state.events += slice.rows();
            }
          },
          [=](caf::unit_t&, const caf::error&) {
            VAST_DEBUG("{} stream shuts down", *self);
            self->send(self, atom::internal_v, atom::persist_v);
          })
        .inbound_slot();
    },
    // Conform to the protocol of the STATUS CLIENT actor.
    [self](atom::status, system::status_verbosity) -> record {
      auto result = record{};
      auto store = record{};
      store["events"] = count{self->state.events};
      store["path"] = self->state.path.string();
      result["segment-store"] = std::move(store);
      return result;
    },
    // internal handlers
    [self](atom::internal, atom::persist) {
      self->state.segment = self->state.builder->finish();
      VAST_DEBUG("persisting segment {}", self->state.segment->id());
      self
        ->request(self->state.fs, caf::infinite, atom::write_v,
                  self->state.path, self->state.segment->chunk())
        .then(
          [self](atom::ok) {
            self->state.self = nullptr;
          },
          [self](caf::error& err) {
            VAST_ERROR("failed to flush archive {}", to_string(err));
            self->state.self = nullptr;
          });
      self->state.fs = nullptr;
    },
  };
  return result;
}

class plugin final : public virtual store_plugin {
public:
  using store_plugin::builder_and_header;

  // plugin API
  caf::error initialize(data) override {
    return {};
  }

  [[nodiscard]] const char* name() const override {
    return "segment-store";
  };

  // store plugin API
  [[nodiscard]] caf::expected<builder_and_header>
  make_store_builder(system::accountant_actor accountant,
                     system::filesystem_actor fs,
                     const vast::uuid& id) const override {
    auto path = store_path_for_partition(id);
    std::string path_str = path.string();
    auto header = chunk::make(std::move(path_str));
    auto builder
      = fs->home_system().spawn(active_local_store, accountant, fs, path);
    return builder_and_header{
      static_cast<system::store_builder_actor&&>(builder), std::move(header)};
  }

  [[nodiscard]] caf::expected<system::store_actor>
  make_store(system::accountant_actor accountant, system::filesystem_actor fs,
             std::span<const std::byte> header) const override {
    auto path = store_path_from_header(header);
    return fs->home_system().spawn<caf::lazy_init>(passive_local_store,
                                                   accountant, fs, path);
  }
};

} // namespace

} // namespace vast::plugins::segment_store

VAST_REGISTER_PLUGIN(vast::plugins::segment_store::plugin)
