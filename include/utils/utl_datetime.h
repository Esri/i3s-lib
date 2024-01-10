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

#pragma once

#include "utils/utl_i3s_export.h"
#include <optional>
#include <string>
#include "utils/utl_static_vector.h"

namespace i3slib
{

namespace utl
{
// only used as part of a parsed date-format.
enum class Time_element { Year, Month, Day, Hour, Min, Sec, Millisec, Utc_offset, _count, Not_set = _count };

struct Datetime_meta
{
  std::string datetime_format; // e.g MM/DD/YYYY hh:mm:ss, YYYY-MM-DD
  utl::static_vector<Time_element, (int)Time_element::_count > parsed_format;
  unsigned int utc_offset_hour{ 0 };
  unsigned int utc_offset_min{ 0 };
  bool utc_offset_positive{ true };
};

// must call this first before attempting to convert a date.
[[nodiscard]]
I3S_EXPORT bool parse_date_format(std::string_view expected_format, Datetime_meta* datetime_meta);
[[nodiscard]]
I3S_EXPORT bool convert_date_to_iso8601(const Datetime_meta& datetime_meta, std::string_view date, std::string* date_iso8601);
I3S_EXPORT std::optional<int64_t> to_unix_timestamp(const Datetime_meta& datetime_meta, std::string_view date);

// if datetime_meta utc offsets = 0, returns UTC time, else local time + utc offset
// e.g timestamp = 0:
// 1970-01-01T:00:00:00.000Z
// 1969-12-31T16:00:00:00.000-08:00 (utc_offset_hours = -8);
I3S_EXPORT std::string timestamp_to_iso8601(int64_t ts, const Datetime_meta& datetime_meta);

} // end ::utl
} // end ::i3slib