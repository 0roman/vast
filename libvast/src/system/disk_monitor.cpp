#include "vast/system/disk_monitor.hpp"

#include "vast/fwd.hpp"

#include "vast/concept/parseable/from_string.hpp"
#include "vast/concept/parseable/vast/uuid.hpp"
#include "vast/detail/recursive_size.hpp"
#include "vast/error.hpp"
#include "vast/logger.hpp"
#include "vast/system/archive.hpp"
#include "vast/uuid.hpp"

#include <caf/detail/scope_guard.hpp>
#include <caf/settings.hpp>
#include <caf/typed_event_based_actor.hpp>

#include <filesystem>
#include <system_error>

namespace vast::system {

namespace {

struct partition_diskstate {
  uuid id;
  std::uintmax_t filesize;
  std::filesystem::file_time_type mtime;
};

template <typename Fun>
std::shared_ptr<caf::detail::scope_guard<Fun>> make_shared_guard(Fun f) {
  return std::make_shared<caf::detail::scope_guard<Fun>>(std::forward<Fun>(f));
}

} // namespace

disk_monitor_actor::behavior_type
disk_monitor(disk_monitor_actor::stateful_pointer<disk_monitor_state> self,
             size_t hiwater, size_t lowater, std::chrono::seconds scan_interval,
             const std::filesystem::path& dbdir, archive_actor archive,
             index_actor index) {
  VAST_TRACE_SCOPE("{} {} {}", VAST_ARG(hiwater), VAST_ARG(lowater),
                   VAST_ARG(dbdir));
  using namespace std::string_literals;
  self->state.high_water_mark = hiwater;
  self->state.low_water_mark = lowater;
  self->state.archive = archive;
  self->state.index = index;
  self->state.dbdir = dbdir;
  self->state.scan_interval = scan_interval;
  self->send(self, atom::ping_v);
  return {
    [self](atom::ping) {
      self->delayed_send(self, self->state.scan_interval, atom::ping_v);
      if (self->state.purging) {
        VAST_DEBUG("{} ignores ping because a deletion is still in "
                   "progress",
                   self);
        return;
      }
      // TODO: This is going to do one syscall per file in the database
      // directory. This feels a bit wasteful, but in practice we didn't
      // see noticeable overhead even on large-ish databases.
      // Nonetheless, if this becomes relevant we should switch to using
      // `inotify()` or similar to do real-time tracking of the db size.
      if (const auto size = detail::recursive_size(self->state.dbdir); !size) {
        VAST_WARN("{} failed to calculate recursive size of {}: {}", self,
                  self->state.dbdir, size.error());
      } else {
        VAST_VERBOSE("{} checks db-directory of size {} bytes", self, *size);
        if (*size > self->state.high_water_mark && !self->state.purging) {
          self->state.purging = true;
          // TODO: Remove the static_cast when switching to CAF 0.18.
          self
            ->request(static_cast<disk_monitor_actor>(self), caf::infinite,
                      atom::erase_v)
            .then(
              [] {
                // nop
              },
              [=](const caf::error& err) {
                VAST_ERROR("{} failed to purge db-directory: {}", self, err);
              });
        }
      }
    },
    [self](atom::erase) -> caf::result<void> {
      // Make sure the `purging` state will be reset once all continuations
      // have finished or we encountered an error.
      auto shared_guard
        = make_shared_guard([=] { self->state.purging = false; });
      auto err = std::error_code{};
      const auto index_dir
        = std::filesystem::directory_iterator(self->state.dbdir / "index", err);
      if (err)
        return caf::make_error(ec::filesystem_error, //
                               fmt::format("failed to find index in "
                                           "db-directory at {}: {}",
                                           self->state.dbdir, err));
      // TODO(ch20006): Add some check on the overall structure on the db dir.
      std::vector<partition_diskstate> partitions;
      for (const auto& entry : index_dir) {
        auto partition = entry.path().stem().string();
        if (partition == "index.bin")
          continue;
        uuid id;
        if (!parsers::uuid(partition, id)) {
          VAST_VERBOSE("{} failed to find partition {}", self, partition);
          continue;
        }
        if (entry.is_regular_file()) {
          std::error_code err{};
          const auto file_size = entry.file_size(err);
          const auto mtime = entry.last_write_time(err);
          if (!err && file_size != static_cast<std::uintmax_t>(-1))
            partitions.push_back({id, file_size, mtime});
          else
            VAST_WARN("{} failed to get file size and last write time for "
                      "partition {}",
                      self, partition);
        }
      }
      if (partitions.empty()) {
        VAST_VERBOSE("{} failed to find any partitions to delete", self);
        return {};
      }
      VAST_DEBUG("{} found {} partitions on disk", self, partitions.size());
      std::sort(partitions.begin(), partitions.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.mtime < rhs.mtime;
                });
      auto oldest = partitions.front();
      VAST_VERBOSE("{} erases partition {} from index", self, oldest.id);
      self->request(self->state.index, caf::infinite, atom::erase_v, oldest.id)
        .then(
          [=, sg = shared_guard](ids erased_ids) {
            // TODO: It would be more natural if we could chain these futures,
            // instead of nesting them.
            VAST_VERBOSE("{} erases removed ids from archive", self);
            self
              ->request(self->state.archive, caf::infinite, atom::erase_v,
                        erased_ids)
              .then(
                [=, sg = shared_guard](atom::done) {
                  // TODO: There's a race condition here: We calculate the size
                  // of the database directory while we might be deleting files
                  // from it.
                  if (const auto size
                      = detail::recursive_size(self->state.dbdir);
                      !size) {
                    VAST_WARN("{} failed to calculate recursive size of {}: {}",
                              self, self->state.dbdir, size.error());
                  } else {
                    VAST_VERBOSE("{} erased ids from index; {} bytes "
                                 "left on disk",
                                 self, *size);
                    if (*size > self->state.low_water_mark) {
                      // Repeat until we're below the low water mark
                      self->send(self, atom::erase_v);
                    }
                  }
                },
                [=, sg = shared_guard](caf::error err) {
                  VAST_WARN("{} failed to erase from archive: {}", self, err);
                });
          },
          [=, sg = shared_guard](caf::error e) {
            VAST_WARN("{} failed to erase from index: {}", self, render(e));
          });
      return {};
    },
    [](atom::status, status_verbosity) {
      // TODO: Return some useful information here.
      return caf::settings{};
    },
  };
}

} // namespace vast::system
