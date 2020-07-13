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
#include "utils/utl_variant.h"
#include <stdint.h>
#include <cstring>
#include <limits>
#include <cstring>

namespace i3slib
{

namespace utl
{

Variant::Variant(const Variant& src)
{
  _copy_from(src);
}

Variant& Variant::operator=(const Variant& src)
{
  if (&src != this)
  {
    if (m_memory == Memory::Copy)
    {
      if (m_type == Variant_trait::Type::String)
        delete m_string_ptr; //this could be optimized out in certain cases.
      if (m_type == Variant_trait::Type::WString)
        delete m_wstring_ptr; //this could be optimized out in certain cases.
      m_string_ptr = nullptr;
    }
    _copy_from(src);
  }
  return *this;
}

std::string   escape_special_char_for_json(const std::string& src)
{
  static const int  c_count = 6;
  static const char c_special[c_count] = { '\b', '\t', '\n','\r', '"', '\\' }; //in c
  static const char c_escaped[c_count] = { 'b', 't', 'n','r', '"', '\\' }; // in json
  std::string out;
  out.reserve(src.size());
  bool is_special;
  for (auto &c : src)
  {
    is_special = false;
    for (int i = 0; i < c_count; ++i)
      if (c_special[i] == c)
      {
        out.push_back('\\');
        out.push_back(c_escaped[i]);
        is_special = true;
        break;
      }
    if (!is_special)
      out.push_back(c);
  }
  return out;
}

 
std::ostream& Variant::_to_string_internal(std::ostream& out, const Variant& v, Variant::format_str_fct format_fct)
{
  std::ios old_state(nullptr);
  old_state.copyfmt(out);
  switch (v.m_type)
  {
    case Variant_trait::Type::Bool:  out << v._get_scalar_value< bool >(); break;
    case Variant_trait::Type::Int8:  out << (int)v._get_scalar_value< int8_t >(); break;
    case Variant_trait::Type::Uint8: out << (int)v._get_scalar_value< uint8_t >(); break;
    case Variant_trait::Type::Int16: out << v._get_scalar_value< int16_t >(); break;
    case Variant_trait::Type::Uint16:out << v._get_scalar_value< uint16_t >(); break;
    case Variant_trait::Type::Int32: out << v._get_scalar_value< int32_t >(); break;
    case Variant_trait::Type::Uint32:out << v._get_scalar_value< uint32_t >(); break;
    case Variant_trait::Type::Int64: out << v._get_scalar_value< int64_t >(); break;
    case Variant_trait::Type::Uint64:out << v._get_scalar_value< uint64_t >(); break;
    case Variant_trait::Type::Float: out << std::setprecision(std::numeric_limits<float>::max_digits10)
      << v._get_scalar_value< float >(); break;
    case Variant_trait::Type::Double:out << std::setprecision(std::numeric_limits<double>::max_digits10)
      << v._get_scalar_value< double >(); break;
    case Variant_trait::Type::String:out << (*format_fct)(v._get_string()); break;
    case Variant_trait::Type::WString:out << (*format_fct)(utf16_to_utf8(v._get_wstring())); break;
    case Variant_trait::Type::Not_set:
      out << (*format_fct)("<Null>"); //TBD
    default:
      I3S_ASSERT_EXT(false);
  }
  out.copyfmt(old_state);

  return out;
}

//! this function is used in the context where 'out' is UTF-8 text stream (not binary stream)
//! for string, JSON special char as escaped and quotes are added.
std::ostream& operator<<(std::ostream& out, const Variant& v)
{
  out << std::boolalpha; // "true"/ "false", not "0"/"1"

  return Variant::_to_string_internal(out, v, escape_special_char_for_json);
}

std::string  Variant::to_string() const
{
std::ostringstream oss;
Variant::_to_string_internal(oss, *this, [](const std::string& src) { return src; });
return oss.str();
}

bool  operator==(const Variant& a, const Variant& b)
{
  if (a.m_type != b.m_type)
    return false;
  switch (a.m_type)
  {
    case Variant_trait::Type::String:
      return a._get_string() == b._get_string();
    case Variant_trait::Type::WString:
      return a._get_wstring() == b._get_wstring();
    default:
      return std::memcmp(a._get_scalar_ptr(), b._get_scalar_ptr(), sizeof(int64_t)) == 0;
  }
}

bool  operator<(const Variant& a, const Variant& b)
{
  if (a.m_type != b.m_type)
    return a.m_type < b.m_type;

  switch (a.m_type)
  {
    case Variant_trait::Type::String:
      return a._get_string() < b._get_string();
    case Variant_trait::Type::WString:
      return a._get_wstring() < b._get_wstring();
    default:
      return a.to_double() < b.to_double();
  }
}


double  Variant::to_double() const noexcept
{
  switch (m_type)
  {
    case Variant_trait::Type::String:
    {
      std::istringstream iss(_get_string());
      iss.imbue(std::locale::classic());
      double val;
      iss >> val;
      return val;
      //return std::stod(_get_string()); //WARNING: This function depends on the local which could be a problem. 
    }
    case Variant_trait::Type::WString:
    {
      std::wistringstream iss(_get_wstring());
      iss.imbue(std::locale::classic());
      double val;
      iss >> val;
      return val;
      //return std::stod(_get_string()); //WARNING: This function depends on the local which could be a problem. 
    }
    case Variant_trait::Type::Bool:  return (double)_get_scalar_value< bool >();
    case Variant_trait::Type::Int8:  return (double)_get_scalar_value< int8_t >();
    case Variant_trait::Type::Uint8: return (double)_get_scalar_value< uint8_t >();
    case Variant_trait::Type::Int16: return (double)_get_scalar_value< int16_t >();
    case Variant_trait::Type::Uint16:return (double)_get_scalar_value< uint16_t >();
    case Variant_trait::Type::Int32: return (double)_get_scalar_value< int32_t >();
    case Variant_trait::Type::Uint32:return (double)_get_scalar_value< uint32_t >();
    case Variant_trait::Type::Int64: return (double)_get_scalar_value< int64_t >();
    case Variant_trait::Type::Uint64:return (double)_get_scalar_value< uint64_t >();
    case Variant_trait::Type::Float: return (double)_get_scalar_value< float >();
    case Variant_trait::Type::Double:return (double)_get_scalar_value< double >();
    default:
      I3S_ASSERT(false);
      return 0.0;
  }
}

void Variant::_copy_from(const Variant& src) noexcept
{
  m_type = src.m_type;
  m_memory = Memory::Copy;
  switch (m_type)
  {
    case Variant_trait::Type::String:
      m_string_ptr = new std::string(src._get_string());
      break;

    case Variant_trait::Type::WString:
      m_string_ptr = new std::string(src._get_string());
      break;
    default:
      if (src.m_memory == Memory::Shared)
      {
        m_scalar_copy = 0;
        memcpy(&m_scalar_copy, _get_scalar_ptr(), Variant_trait::size_in_bytes(m_type));
      }
      m_scalar_copy = src.m_scalar_copy;
      break;
  }
}

} // namespace utl

} // namespace i3slib
