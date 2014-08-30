#include "vast/exporter.h"

#include <caf/all.hpp>
#include "vast/event.h"

using namespace caf;

namespace vast {

message_handler exporter::act()
{
  attach_functor(
      [=](uint32_t reason)
      {
        for (auto& s : sinks_)
          send_exit(s, reason);

        sinks_.clear();
      });

  return
  {
    [=](down_msg const&)
    {
      VAST_LOG_ACTOR_ERROR("got DOWN from " << last_sender());
      for (auto& s : sinks_)
        if (s == last_sender())
        {
          sinks_.erase(s);
          break;
        }
    },
    on(atom("add"), arg_match) >> [=](actor const& snk)
    {
      monitor(snk);
      sinks_.insert(snk);
    },
    on(atom("limit"), arg_match) >> [=](uint64_t max)
    {
      VAST_LOG_ACTOR_DEBUG("caps event export at " << max << " events");

      if (processed_ < max)
        limit_ = max;
      else
        VAST_LOG_ACTOR_ERROR("ignores new limit of " << max <<
                             ", already processed " << processed_ << " events");
    },
    [=](event const&)
    {
      for (auto& s : sinks_)
        forward_to(s);

      if (++processed_ == limit_)
      {
        VAST_LOG_ACTOR_DEBUG("reached maximum event limit: " << limit_);
        quit(exit::done);
      }
    }
  };
}

std::string exporter::describe() const
{
  return "exporter";
}

} // namespace vast
