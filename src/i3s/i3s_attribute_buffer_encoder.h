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

#include "utils/utl_i3s_assert.h"
#include "utils/utl_geom.h"
#include "utils/utl_i3s_export.h"
#include "i3s/i3s_enums.h"

//#include "psl_builder/psl_types.h"
#include <string>
#include <cstring>

namespace i3slib
{

namespace i3s
{

//! Attribute buffer - WARNING: type cannot be mixed! 
//! Binary layout is compatible with i3s buffer ( no copy will be required)
//! for Strings, Binary Layout is : (all integers are *little endian*)
//!     U32  number_of_string
//!     U32 number_of_bytes_all_strings
//!     U32 string_sizes[number_of_string]
//!     char   all_strings_buffer_utf8[number_of_bytes_all_strings]


class Attribute_buffer_encoder
{
public:
  Attribute_buffer_encoder() : m_capacity(0), m_type(Type::Not_set) {}
  void        init_buffer(int capacity, Type type)
  {
    m_capacity = capacity;
    m_buffer.clear();
    m_type = type;
  }
  template< class T >
  void        push_back_pod(T val); // Plain-old-data ONLY!
  void        push_back(const utl::Vec3d& xyz) { push_back_pod(xyz); }
  I3S_EXPORT void        push_back(const std::string& utf8_string);
  I3S_EXPORT void        push_null();
  int         size() const { return (m_buffer.size() ? *reinterpret_cast<const int*>(&m_buffer[0]): 0 ); }

  const std::string& get_raw_bytes() const { return m_buffer; }

  // Not in usable state after this was called!
  const std::string release_finalized_buffer()
  {
    m_capacity = 0;
    return std::move(m_buffer);
  }

  template< class T > const T* cast_as_array() const;
  template< class T > T* cast_as_array_non_const();

  template< class T > Type _as_type() const; //must be specialized.

  // Predicate in the form: []( const string& str )-> bool 
  template< class Pred > void for_each_string(Pred pred) const;


private:
  void        _push_back(const char* utf_8, int bytes);
private:
  Type    m_type;
  int               m_capacity; //in items (*not* bytes)
  std::string       m_buffer;

};

//templated type assignment:
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<bool>() const { return Type::Int8; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<char>() const { return Type::Int8; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<int8_t>() const { return Type::Int8; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<uint8_t>() const { return Type::UInt8; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<int16_t>() const { return Type::Int16; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<uint16_t>() const { return Type::UInt16; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<int32_t>() const { return Type::Int32; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<uint32_t>() const { return Type::UInt32; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<int64_t>() const { return Type::Int64; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<uint64_t>() const { return Type::UInt64; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<float>() const { return Type::Float32; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<double>() const { return Type::Float64; }
template<> inline constexpr Type Attribute_buffer_encoder::_as_type<utl::Vec3d>() const { return Type::Float64; }


// ---------------------------- inline definitions: ---------------


template< class T >
inline const T* Attribute_buffer_encoder::cast_as_array() const
{
  static_assert(sizeof(T) <= 8 || sizeof(T) % sizeof(double) == 0, "Vector types not supported except for utl::Vec3d");
  constexpr size_t c_hdr_and_pad_size = sizeof(int) + (sizeof(T) > sizeof(int) ? 8 - sizeof(int) : 0); // double must be 8-byte align despite the 4 byte 'count' header
  I3S_ASSERT((m_buffer.size() - c_hdr_and_pad_size) % sizeof(T) == 0);
  I3S_ASSERT(m_type != Type::String_utf8 && (sizeof(T)* size() + c_hdr_and_pad_size <= m_buffer.size()));
  return reinterpret_cast<const T*>(&m_buffer[c_hdr_and_pad_size]);
}

template< class T >
inline T* Attribute_buffer_encoder::cast_as_array_non_const()
{
  static_assert(sizeof(T) <= 8 || sizeof(T) % sizeof(double) == 0, "Vector types not supported except for utl::Vec3d");
  constexpr size_t c_hdr_and_pad_size = sizeof(int) + (sizeof(T) > sizeof(int) ? 8 - sizeof(int) : 0); // double must be 8-byte align despite the 4 byte 'count' header
  I3S_ASSERT((m_buffer.size() - c_hdr_and_pad_size) % sizeof(T) == 0);
  //I3S_ASSERT(m_type != Type::Str_utf_8 && (sizeof(T)* size() + c_hdr_and_pad_size < m_buffer.size()));
  I3S_ASSERT(m_type != Type::String_utf8 && (sizeof(T)* size() + c_hdr_and_pad_size <= m_buffer.size()));
  return reinterpret_cast<T*>(&m_buffer[c_hdr_and_pad_size]);
}

// Predicate in the form: []( const string& str )-> bool 
template< class Pred >
inline void Attribute_buffer_encoder::for_each_string(Pred pred) const
{
  int count = size(); // count of strings in the buffer
  int head = sizeof(int) * (2 + count);
  // loop over strings
  std::string staging;
  for (int i = 0; i < count; i++)
  {
    int str_size = (int&)m_buffer[sizeof(int) * (2 + i)];
    staging.resize( std::max(0, str_size-1) );
    // WARNING: string in the buffer are always null-terminated (or size=0 -> null)
    I3S_ASSERT_EXT(staging.size() == 0 || m_buffer[head + staging.size()] == '\0');
    I3S_ASSERT_EXT(head + staging.size() <= m_buffer.size());
    if( staging.size() )
      std::memcpy(&staging[0], &m_buffer[head], staging.size());
    head += str_size;
    if (!pred(staging)) 
      return;
  }
}

template< class T >
inline void Attribute_buffer_encoder::push_back_pod(T val)
{
  static_assert(sizeof(T) <= 8 || sizeof(T) % sizeof(double) == 0, "Vector types not supported except for utl::Vec3d");
  constexpr size_t c_hdr_and_pad_size = sizeof(int) + (sizeof(T) > sizeof(int) ? 8 - sizeof(int) : 0); // double must be 8-byte align despite the 4 byte 'count' header
  auto arg_type = _as_type<T>();
  if (m_buffer.empty())
  {
    I3S_ASSERT(m_capacity >0);
    m_type = arg_type;

    m_buffer.resize(m_capacity * sizeof(T) + c_hdr_and_pad_size);
    memset(&m_buffer[0], 0x00, c_hdr_and_pad_size); //zero the count
  }
  int& count = *reinterpret_cast<int*>(&m_buffer[0]);
  if (count == m_capacity || m_type != arg_type)
  {
    I3S_ASSERT(false); //Out-of-bound or type mixing.
    return;
  }
  T* items = reinterpret_cast<T*>(&m_buffer[c_hdr_and_pad_size]);
  items[count++] = val;
}


}//endof ::i3s
} // namespace i3slib

