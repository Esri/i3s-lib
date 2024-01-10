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
#include "i3s/i3s_attribute_buffer_encoder.h"
#include <cstring>

namespace i3slib
{

namespace i3s
{

void Attribute_buffer_encoder::push_back(const std::string& utf8_string)
{
  // In I3S string must be null terminated (to distinguish between empty vs. null)
  _push_back(utf8_string.c_str(), (int)utf8_string.size()+1);
}

//! WARNING: "null str" != "empty string" in I3S
void Attribute_buffer_encoder::_push_back(const char* utf_8, int bytes)
{
  if (m_buffer.empty())
  {
    // init to string buffer :
    I3S_ASSERT(m_capacity > 0);
    m_type = Type::String_utf8;
    m_buffer.resize(sizeof(int)*(m_capacity + 2)); //reserve space for meta only. (header +size array)
    memset(&m_buffer[0], 0x00, m_buffer.size());
  }
  struct Meta
  {
    int string_count;
    int bytes_all_strings;
  };
  //update meta-data:
  Meta* meta = reinterpret_cast<Meta*>(&m_buffer[0]);
  if (m_type != Type::String_utf8 || meta->string_count == m_capacity)
  {
    I3S_ASSERT(false); //Out-of-bound or type mixing.
    return;
  }
  meta->bytes_all_strings += bytes;
  *reinterpret_cast<int*>(&m_buffer[sizeof(int) * (2 + meta->string_count++)]) = bytes;

  //append the string
  if (bytes)
  {
    auto i = m_buffer.size();
    m_buffer.resize(m_buffer.size() + bytes);
    std::memcpy(&m_buffer[i], utf_8, bytes);
  }
}

void Attribute_buffer_encoder::push_null()
{
  constexpr int c_hdr_size = 8;
  I3S_ASSERT(m_type != Type::Not_set);
  switch (m_type)
  {
    case Type::Int8: return push_back_pod(std::numeric_limits<int8_t>::lowest());
    case Type::UInt8: return push_back_pod(std::numeric_limits<uint8_t>::max());
    case Type::Int16: return push_back_pod(std::numeric_limits<int16_t>::lowest());
    case Type::UInt16: return push_back_pod(std::numeric_limits<uint16_t>::max());
    case Type::Int32: return push_back_pod(std::numeric_limits<int32_t>::lowest());
    case Type::Oid32:
    case Type::UInt32: return push_back_pod(std::numeric_limits<uint32_t>::max());
    case Type::Int64: return push_back_pod(std::numeric_limits<int64_t>::lowest());
    case Type::Oid64:
    case Type::UInt64: return push_back_pod(std::numeric_limits<uint64_t>::max());
    case Type::Float32:
      static_assert (std::numeric_limits<float>::has_quiet_NaN);
      push_back_pod(std::numeric_limits<float>::quiet_NaN());
      I3S_ASSERT(m_capacity * sizeof(float) + c_hdr_size == m_buffer.size()); // this code wouldn't work with multi-component type ( i.e. vec3d ) . XYZ Can't be null, so it's ok.
      return;
    case Type::Float64:
      static_assert(std::numeric_limits<double>::has_quiet_NaN);
      push_back_pod(std::numeric_limits<double>::quiet_NaN());
      I3S_ASSERT(m_capacity * sizeof(double) + c_hdr_size == m_buffer.size()); // this code wouldn't work with multi-component type ( i.e. vec3d ) . XYZ Can't be null, so it's ok.
      return;
    case Type::Date_iso_8601:
    case Type::Global_id:
    case Type::Guid:
    case Type::String_utf8: return _push_back( nullptr, 0 ); // push a null string ( **not** empty str)
    default:
      I3S_ASSERT(false); //something new ?
      return;
  }

}

}//endof ::i3s
} // namespace i3slib

