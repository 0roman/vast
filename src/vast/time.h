#ifndef VAST_TIME_H
#define VAST_TIME_H

#include <chrono>
#include <string>

#include "vast/fwd.h"
#include "vast/convert.h"
#include "vast/parse.h"
#include "vast/print.h"
#include "vast/util/operators.h"

namespace vast {

struct access;

namespace time {

// The main reason why we try to shoehorn std::chrono into the two types
// *duration* and *point* is so that we can offer two simple types to the query
// language. We may switch to more types in the future, i.e., nanoseconds,
// microseconds, etc.

class point;
class duration;

using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;
using double_seconds = std::chrono::duration<double, std::ratio<1>>;

// Short idiomatic names for working with timers.
using stopwatch = std::chrono::steady_clock;
using moment = stopwatch::time_point;
using extent = stopwatch::duration;

inline moment snapshot()
{
  return stopwatch::now();
}

// Currently unused.
class interval;
class perdiod;

/// Constructs a time point with the current system time.
point now();

/// A time duration with nanosecond granularity.
class duration : util::totally_ordered<duration>
{
  friend access;
  friend class point;

public:
  using rep = int64_t;
  using duration_type = std::chrono::duration<rep, std::nano>;

  static duration zero();
  static duration min();
  static duration max();

  /// Constructs a zero duration.
  duration() = default;

  /// Constructs a duration from a `std::chrono::duration`.
  /// @tparam Rep The representation type of the duration.
  /// @tparam Period The period of the duration.
  /// @param dur A `std::chrono::duration<Rep, Period>`.
  template <typename Rep, typename Period>
  duration(std::chrono::duration<Rep, Period> dur)
    : duration_{std::chrono::duration_cast<duration_type>(dur).count()}
  {
  }

  // Arithmetic operators.
  duration operator+() const;
  duration operator-() const;
  duration& operator++();
  duration operator++(int);
  duration& operator--();
  duration operator--(int);
  duration& operator+=(duration const& r);
  duration& operator-=(duration const& r);
  duration& operator*=(rep const& r);
  duration& operator/=(rep const& r);
  friend duration operator+(duration const& x, duration const& y);
  friend duration operator-(duration const& x, duration const& y);
  friend point operator+(point const& x, duration const& y);
  friend point operator-(point const& x, duration const& y);
  friend point operator+(duration const& x, point const& y);

  // Relational operators.
  friend bool operator==(duration const& x, duration const& y);
  friend bool operator<(duration const& x, duration const& y);

  /// Lifts `std::chrono::duration::count`.
  rep count() const;

  // Convert this duration to minute resolution.
  // @returns This duration in minutes or 0 if too fine-grained.
  rep minutes() const;

  // Convert this duration to seconds resolution.
  // @returns This duration in seconds or 0 if too fine-grained.
  rep seconds() const;

  // Convert this duration to seconds resolution with double precision.
  // @returns This duration in seconds plus a fractional part.
  double double_seconds() const;

  // Convert this duration to milliseconds resolution.
  // @returns This duration in milliseconds or 0 if too fine-grained.
  rep milliseconds() const;

  // Convert this duration to microseconds resolution.
  // @returns This duration in microseconds or 0 if too fine-grained.
  rep microseconds() const;

  // Convert this duration to nanoseconds resolution.
  // @returns This duration in nanoseconds.
  rep nanoseconds() const;

  friend trial<void> convert(duration tr, double& d);
  friend trial<void> convert(duration tr, duration_type& dur);
  friend trial<void> convert(duration tr, util::json& j);

private:
  duration_type duration_{0};
};

/// Constructs a second duration.
/// @param f The number of fractional seconds.
/// @returns A duration of *f* fractional seconds.
inline duration fractional(double f)
{
  return double_seconds{f};
}

/// An absolute point in time having UTC time zone.
class point : util::totally_ordered<point>
{
  friend access;

public:
  using duration_type = duration::duration_type;
  using clock = std::chrono::system_clock;
  using time_point_type = std::chrono::time_point<clock, duration_type>;

  // The default format string used to convert time points into calendar types.
  static constexpr char const* format = "%Y-%m-%d+%H:%M:%S";

  /// Constructs a time point from a `std::tm` structure.
  static point from_tm(std::tm const& tm);

  /// Constructs a UTC time point.
  static point utc(int year,
                   int month = 0,
                   int day = 0,
                   int hour = 0,
                   int min = 0,
                   int sec = 0);

  /// Constructs a time point with the UNIX epoch.
  point() = default;

  /// Constructs a time point from a `std::chrono::time_point`.
  template <typename Clock, typename Duration>
  point(std::chrono::time_point<Clock, Duration> tp)
    : time_point_{std::chrono::time_point_cast<duration_type>(tp)}
  {
  }

  /// Creates a time point from a duration.
  /// @param d The duration.
  point(duration d);

  // Arithmetic operators.
  point& operator+=(duration const& r);
  point& operator-=(duration const& r);
  friend point operator+(point const& x, duration const& y);
  friend point operator-(point const& x, duration const& y);
  friend point operator+(duration const& x, point const& y);
  friend duration operator-(point const& x, point const& y);

  // Relational operators.
  friend bool operator==(point const& x, point const& y);
  friend bool operator<(point const& x, point const& y);

  /// Computes the relative time with respect to this time point. Underflows
  /// and overflows behave intuitively for seconds, minutes, hours, and days.
  /// For months, a delta of *x* months means the same day of the current month
  /// shifted by *x* months. That is, *x* represents the number of days of the
  /// respective months, as opposed to always 30 days. Year calculations follow
  /// the style.
  /// @param secs The seconds to add/subtract.
  /// @param mins The minutes to add/subtract.
  /// @param hours The hours to add/subtract.
  /// @param days The days to to add/subtract.
  /// @param months The months to to add/subtract.
  /// @param years The years to to add/subtract.
  /// @returns The relative time from now according to the unit specifications.
  point delta(int secs = 0,
              int mins = 0,
              int hours = 0,
              int days = 0,
              int months = 0,
              int years = 0);

  /// Returns a duration representing the duration since the UNIX epoch.
  duration time_since_epoch() const;

  friend trial<void> convert(point p, double& d);
  friend trial<void> convert(point p, std::tm& tm);
  friend trial<void> convert(point p, util::json& j);

private:
  time_point_type time_point_;
};

// This remains outside because friend functions with default arguments must
// come with a direct definition, no declarations allowed.
trial<void> convert(point p, std::string& str,
                    char const* fmt = point::format);

/// Determines whether a given year is a leap year.
/// @param year The year to check.
/// @returns `true` iff *year* is a leap year.
bool is_leap_year(int year);

/// Retrieves the number days in a given month of a particular year.
/// @param year The year.
/// @param month The month.
/// @returns The number of days that *month* has in *year*.
/// @pre `month >= 0 && month < 12`
int days_in_month(int year, int month);

/// Computes the number of days relative to a given year and month.
/// @param year The starting year.
/// @param month The starting month.
/// @param n The number of months to convert to days relative to *year* and
///          *month*.
/// @pre `month >= 0 && month < 12`
int days_from(int year, int month, int n);

/// Converts a `std::tm` structure to `time_t`.
/// @param tm The `std::tm` structure to convert.
/// @returns *tm* as `time_t`
time_t to_time_t(std::tm const& t);

/// Creates a new `std::tm` initialized to the 1970 epoch.
/// @returns The `std::tm`.
std::tm make_tm();

/// Propagates underflowed and overflowed values up to the next higher unit.
void propagate(std::tm &t);

/// Parses a string into a `std::tm` structure.
/// @param str The string to parse.
/// @param fmt The format string to use.
/// @param local The locale to use.
/// @returns The `std::tm` structure corresponding to *str*.
std::tm to_tm(std::string const& str, char const* fmt, char const* locale);

//
// Concepts
//

template <typename Iterator>
trial<void> print(duration d, Iterator&& out, bool adaptive = true)
{
  using util::print;
  if (adaptive)
  {
    auto cnt = d.count();
    if (cnt > 1000000000)
    {
      print_numeric(cnt / 1000000000, out);
      print('.', out);
      print_numeric((cnt % 1000000000) / 10000000, out);
      print("s", out);
    }
    else if (cnt > 1000000)
    {
      print_numeric(cnt / 1000000, out);
      print('.', out);
      print_numeric((cnt % 1000000) / 10000, out);
      print("ms", out);
    }
    else if (cnt > 1000)
    {
      print_numeric(cnt / 1000, out);
      print("us", out);
    }
    else
    {
      print_numeric(cnt, out);
      print("ns", out);
    }
  }
  else
  {
    print(d.double_seconds(), out);
    print('s', out);
  }
  return nothing;
}

template <typename Iterator>
trial<void> print(point p, Iterator&& out, char const* fmt = point::format)
{
  using util::print;
  std::string str;
  auto t = convert(p, str, fmt);
  if (t)
    return print(str, out);
  else
    return t.error();
}

template <typename Iterator>
trial<void> parse(duration& dur, Iterator& begin, Iterator end)
{
  bool is_double;
  auto d = util::parse<double>(begin, end, &is_double);
  if (! d)
    return d.error();
  if (is_double)
  {
    dur = fractional(*d);
    return nothing;
  }
  duration::rep i = *d;
  if (begin == end)
  {
    dur = seconds(i);
    return nothing;
  }
  switch (*begin++)
  {
    default:
      return error{"invalid unit: ", *begin};
    case 'n':
      if (begin != end && *begin++ == 's')
        dur = nanoseconds(i);
      break;
    case 'u':
      if (begin != end && *begin++ == 's')
        dur = microseconds(i);
      break;
    case 'm':
      if (begin != end && *begin++ == 's')
        dur = milliseconds(i);
      else
        dur = minutes(i);
      break;
    case 's':
      dur = seconds(i);
      break;
    case 'h':
      dur = hours(i);
      break;
  }
  return nothing;
}

template <typename Iterator>
trial<void> parse(point& p, Iterator& begin, Iterator end,
                  char const* fmt = nullptr,
                  char const* locale = nullptr)
{
  if (fmt)
  {
    std::string str{begin, end};
    begin = end;
    p = std::chrono::system_clock::from_time_t(
        to_time_t(to_tm(str, fmt, locale)));
  }
  else
  {
    duration d;
    auto t = parse(d, begin, end);
    if (! t)
      return t.error();
    p = d;
  }
  return nothing;
}

} // namespace time
} // namespace vast

// We put this overload in namespace std so that ADL can find it.
namespace std {

template <typename Rep, typename Period, typename Iterator>
vast::trial<void> print(std::chrono::duration<Rep, Period> d, Iterator&& out)
{
  return print(vast::time::duration{d}, out);
}

} // namespace std

#endif
