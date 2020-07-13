/*
Copyright 2020 Esri

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
#include "utils/utl_i3s_resource_defines.h"
#include <unordered_map>
#include <string>

namespace i3slib
{
namespace utl
{

namespace
{
const std::unordered_map<int, std::string> c_status_strings =
  {
#include "utl_resource_strings.inc"
  };
}

std::string get_message_string(int string_id)
{
  return c_status_strings.at(string_id);
}

bool get_message_string(int string_id, std::string& string)
{
  const auto it = c_status_strings.find(string_id);
  if (it == c_status_strings.cend())
    return false;

  string = it->second;
  return true;
}

} // namespace utl
} // namespace i3slib
