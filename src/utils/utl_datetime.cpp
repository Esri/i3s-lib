/*
Copyright 2020-2023 Esri

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of
the License at http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

For additional information, contact:
Environmental Systems Research Institute, Inc.
Attn: Contracts Dept
380 New York Street
Redlands, California, USA 92373
email: contracts@esri.com
*/

#include "pch.h"
#include "utils/utl_datetime.h"
#include "utils/utl_i3s_assert.h"
#include <array>
#include <charconv>

namespace i3slib
{
namespace utl
{
constexpr int c_sec_per_min = 60;
constexpr int c_millisec_per_min = c_sec_per_min * 1'000;
constexpr int c_sec_per_hour = 3'600;
constexpr int c_millisec_per_hour = c_sec_per_hour * 1'000;
constexpr int c_sec_per_day = 86'400;
constexpr int c_sec_per_year = 31'536'000;
constexpr int c_epoch_year = 1970;
constexpr int c_month_february = 2; // for handling leap days
constexpr int c_months_in_year = 13;
constexpr std::array<int, c_months_in_year> c_days_per_month = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

struct Datetime
{
  int year{ 0 };
  int month{ 0 };
  int day{ 0 };
  int hour{ 0 };
  int minute{ 0 };
  int second{ 0 };
  int millisecond{ 0 };
  int utc_offset_hour{ 0 };
  int utc_offset_min{ 0 };
  bool utc_offset_positive{ true };
};

constexpr bool is_leap_year(int date)
{
  return date % 4 == 0 && (date % 100 != 0 || date % 400 == 0);
}

// does not include end year
constexpr int leap_days_between(int start, int end)
{
  I3S_ASSERT(start <= end);
  auto num_leap_years = [](int year)
  {
    return (year / 4) - (year / 100) + (year / 400);
  };
  return num_leap_years(end - 1) - num_leap_years(start - 1);
}

static bool get_datetime(const Datetime_meta& datetime_meta, std::string_view date, Datetime* datetime)
{
  using Element = Time_element;
  if (datetime_meta.parsed_format.empty())
    return false;

  const char* start = date.data();
  const char* end = date.data() + date.size();
  
  auto get_value_and_validate = [&start, end](int* value, int min_v, int max_v)
  {
    if (end <= start)
      return false;
    if (auto res = std::from_chars(start, end, *value); res.ec == std::errc{})
    {
      if (*value < min_v || *value > max_v)
        return false;
      start = res.ptr + 1;
      if ((start < end) && std::isdigit(*start) == 0)
        return false; //only 1 character between digits
      return true;
    }
    return false;
  };

  for (const auto& element : datetime_meta.parsed_format)
  {
    I3S_ASSERT(end <= date.data() + date.size());
    switch (element)
    {
    case Element::Year:
    {
      // only supporting positive 4-digit years
      if (!get_value_and_validate(&datetime->year, 0, 9999)) 
      return false;
      break;
    }
    case Element::Month:
    {
      if (!get_value_and_validate(&datetime->month, 1, 12))
        return false;
      break;
    }
    case Element::Day:
    {
      if (!get_value_and_validate(&datetime->day, 1, 31))
        return false;
      break;
    }
    case Element::Hour:
    {
      if (!get_value_and_validate(&datetime->hour, 0, 24))
        return false;
      break;
    }
    case Element::Min:
    {
      if (!get_value_and_validate(&datetime->minute, 0, 59))
        return false;
      break;
    }
    case Element::Sec:
    {
      if (!get_value_and_validate(&datetime->second, 0, 59))
        return false;
      break;
    }
    case Element::Millisec: //
    {
      if (!get_value_and_validate(&datetime->millisecond, 0, 999))
        return false;
      break;
    }
    case Element::Utc_offset:
    {
      if (*(start - 1) == 'Z') // == +00:00
      {
        ; // do nothing
      }
      else
      {
        // [+/-][h]h:mm
        if (*(start - 1) == '-')
          datetime->utc_offset_positive = false;

        if (!get_value_and_validate(&datetime->utc_offset_hour, 0, 14))
          return false;
        if (!get_value_and_validate(&datetime->utc_offset_min, 0, 59))
          return false;
      }

      // offsets must be at the end
      if (start < end)
        return false;
      return true;
    }
    default:
      return false;
    }
  }
  return true;
}

static constexpr int64_t to_unix_timestamp(const Datetime_meta& datetime_meta, const Datetime& datetime)
{
  const int cur_year = datetime.year;
  int64_t ts = 0;
  ts = 0; // 1970-01-01T00:00:00Z
  // add utc offset
  int utc_offset_ms = (datetime_meta.utc_offset_hour * c_sec_per_hour)
    + (datetime_meta.utc_offset_min * c_sec_per_min);
  if (datetime_meta.utc_offset_positive)
    utc_offset_ms *= -1;
  ts += utc_offset_ms;

  // add the time
  ts += datetime.second;
  ts += (datetime.minute * c_sec_per_min);
  ts += (datetime.hour * c_sec_per_hour);
  // add years
  ts += (int64_t)(cur_year - c_epoch_year) * c_sec_per_year;
  // add previous days
  ts += ((datetime.day - 1) * c_sec_per_day); // -1: don't add the current day
  // add days for all previous months
  for (int month = 1; month != datetime.month; ++month)
  {
    ts += (c_days_per_month[month] * c_sec_per_day);
  }
  // add leap day for current year
  if (datetime.month > c_month_february && is_leap_year(cur_year))
    ts += c_sec_per_day;
  // leap days for previous years
  int start_date = c_epoch_year;
  int end_date = cur_year;
  if (start_date > end_date)
    std::swap(start_date, end_date);
  int sign = cur_year >= c_epoch_year ? 1 : -1;
  if (auto n_leap_days = leap_days_between(start_date, end_date))
  {
    ts += sign * (n_leap_days * c_sec_per_day);
  }

  // to millsecs
  ts *= 1000;
  ts += sign * datetime.millisecond;
  return ts;

}

std::optional<int64_t> to_unix_timestamp(const Datetime_meta& datetime_meta, std::string_view date)
{
  Datetime datetime;
  if (!get_datetime(datetime_meta, date, &datetime))
    return std::nullopt;

  return to_unix_timestamp(datetime_meta, datetime);
}

std::string timestamp_to_iso8601(int64_t ts,
  const Datetime_meta& datetime_meta)
{
  auto update = [](auto& what, int n)
  {
    int out = static_cast<int>(what / n);
    what -= static_cast<int64_t>(n) * out;
    return out;
  };

  // remove UTC offset from timestamp
  int utc_offset_ms = (datetime_meta.utc_offset_hour * c_millisec_per_hour)
    + (datetime_meta.utc_offset_min * c_millisec_per_min);
  if (datetime_meta.utc_offset_positive)
    utc_offset_ms *= -1;

  ts -= utc_offset_ms;

  // date
  int year, month, day;
  year = c_epoch_year;
  month = 1;
  day = 1;
  // time
  int hour, minute, second, millisec;
  hour = minute = second = millisec = 0;

  // get millisecs and convert to seconds
  millisec = std::abs(static_cast<int>(ts % 1000));
  ts /= 1000;

  // get time of day
  int tod_sec = ts % c_sec_per_day;
  if (tod_sec < 0)
  {
    tod_sec += c_sec_per_day;
  }
  ts -= tod_sec;
  hour = update(tod_sec, c_sec_per_hour);
  minute = update(tod_sec, c_sec_per_min);
  second = tod_sec;
  I3S_ASSERT(tod_sec <= c_sec_per_min);

  // remainder is date
  int n_years = static_cast<int>(ts / c_sec_per_year);

  // get leap days
  int start_year = c_epoch_year;
  int end_year = c_epoch_year + n_years;
  if (start_year > end_year)
    std::swap(start_year, end_year);
  int n_leap_days_sec = leap_days_between(start_year, end_year) * c_sec_per_day;
  ts -= ts > 0 ? n_leap_days_sec : -n_leap_days_sec;

  year += update(ts, c_sec_per_year);
  int n_days = update(ts, c_sec_per_day);
  if (n_days < 0)
  {
    n_days += 365;
    --year;
  }
  // check if year shifted due to leap days
  if ((year != c_epoch_year + n_years) && is_leap_year(year))
  {
    ++n_days; // a missing leap day
  }
  // get days into current year
  I3S_ASSERT(n_days >= 0 && n_days <= 365);
  if (n_days)
  {
    for (int i = 1; i != c_months_in_year; ++i)
    {
      int days_in_month = c_days_per_month[i];
      if (i == c_month_february && is_leap_year(year))
      {
        ++days_in_month;
      }
      if (n_days >= days_in_month)
      {
        ++month;
        n_days -= days_in_month;
      }
      else
        break;
    }
    day += n_days;
  }
  I3S_ASSERT(month > 0 && month <= 12);

  constexpr int c_buff_size = 50; // large enough to hold any time str
  char timeStringBuffer[c_buff_size] = { '\0' };
  if (datetime_meta.utc_offset_hour || datetime_meta.utc_offset_min)
  {
    // local time + utc offset
    constexpr auto positive_offset_format = "%04d-%02d-%02dT%02d:%02d:%02d.%03d+%02d:%02d";
    constexpr auto negative_offset_format = "%04d-%02d-%02dT%02d:%02d:%02d.%03d-%02d:%02d";
    auto the_format = datetime_meta.utc_offset_positive ? positive_offset_format : negative_offset_format;
    snprintf(timeStringBuffer, c_buff_size, the_format
     ,year, month, day, hour, minute, second, millisec
      , datetime_meta.utc_offset_hour, datetime_meta.utc_offset_min);
  }
  else
  {
    // utc time
    constexpr auto format = "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ";
    snprintf(timeStringBuffer, c_buff_size, format
      , year, month, day, hour, minute, second, millisec);
  }
  return timeStringBuffer;
}

bool parse_date_format(std::string_view expected_format, Datetime_meta* datetime_meta)
{
  using Element = Time_element;
  utl::static_vector<Time_element, (int)Time_element::_count > parsed_format;
  int format_pos{ 0 };
  const auto format_sz{ expected_format.size() };
  bool time_set = false;
  while (format_pos < format_sz)
  {
    const auto& c = expected_format[format_pos];
    int num = 1; // number of characters eg. YYYY==4, dd==2 
    switch (c)
    {
    case 'Y':
    {
      // year. only 4 digit years supported. 2-digit is ambiguous, e.g 22 (1922 or 2022?)
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 'Y')
          break;
        ++num;
      }
      switch (num)
      {
      case (4):
        parsed_format.push_back(Element::Year);
        break;
      default:
        return false;;
      }
      time_set = false;
      break;
    }
    case 'M':
    {
      // month
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 'M')
          break;
        ++num;
      }
      switch (num)
      {
      case(1):  // 'm'
      case (2): // 'mm'
        parsed_format.push_back(Element::Month);
        break;
      default:  // 'mmm' (Oct), 'mmmm' (October)
        return false;;
        //*parsed_format += "%b";
      }
      time_set = false;
      break;
    }
    case 'D':
    {
      // day
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 'D')
          break;
        ++num;
      }
      switch (num)
      {
      case (1):
      case(2):
        parsed_format.push_back(Element::Day);
        break;
      default:
        return false;;
      }
      time_set = false;
      break;
    }
    case 'h':
    {
      // hours
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 'h')
          break;
        ++num;
      }
      switch (num)
      {
      case (1):
      case (2):
        parsed_format.push_back(Element::Hour);
        break;
      default:
        return false;;
      }
      time_set = true;
      break;
    }
    case 'm':
    {
      // minutes
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 'm')
          break;
        ++num;
      }
      parsed_format.push_back(Element::Min);
      break;
    }
    case 's':
    {
      // seconds
      while (++format_pos < format_sz)
      {
        if (expected_format[format_pos] != 's')
          break;
        ++num;
      }
      switch (num)
      {
      case (2):
        parsed_format.push_back(Element::Sec);
        break;
      case (3):
      {
        // millisec. check dot separator is used.
        if (expected_format[format_pos - 4] != '.')
          return false;;
        // Datetime doesn't support millisecs. custom sequence to remove millisecs
        parsed_format.push_back(Element::Millisec);
        break;
      }
      default:
        return false;;
      }
      break;
    }
    case '-':
    {
      // if time not set, this is a sepeator, e.g 1-1-2020
      if (!time_set)
      {
        ++format_pos;
        break;
      }
    }
      // else
    [[fallthrough]];
    case 'Z': // = "utc
    case '+':
    {
      // UTC offset must be at end of string
      int leftover = (int)format_sz - format_pos;
      if (leftover > 6) // e.g '+hh:mm'
        return false;;
      parsed_format.push_back(Element::Utc_offset);
      format_pos += leftover;
      break;
    }
    case 'T': // indicates time in ISO 8601 string.
      ++format_pos;
      break;
    default:
      // no other characters allowed. only symbols (-,/,.)
      if (std::isalpha(c))
        return false;
      ++format_pos;
      break;
    }
  }
  I3S_ASSERT(parsed_format.size());
  datetime_meta->parsed_format = std::move(parsed_format);
  return datetime_meta->parsed_format.size();
}

bool convert_date_to_iso8601(const Datetime_meta& datetime_meta  , std::string_view date, std::string* date_iso8601)
{
  Datetime datetime;
  if (!get_datetime(datetime_meta, date, &datetime))
    return false;
  int64_t timestamp = to_unix_timestamp(datetime_meta, datetime);
  *date_iso8601 = timestamp_to_iso8601(timestamp, datetime_meta);
  return true;
}

} // end ::utl
} // end ::i3slib
