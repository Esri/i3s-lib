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
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <charconv>

#if defined(_MSC_VER) && _MSC_VER >= 1900
#include <algorithm>
#include <string_view>
#include <type_traits>
#define HAS_FP_CHARCONV
#else
#include <sstream>
#endif

namespace
{
  template<typename Char>
  std::basic_string_view<Char> trim_whitespaces(const std::basic_string_view<Char> sv)
  {
    auto it = sv.begin();
    auto end_it = sv.end();
    for (;;)
    {
      if (it == end_it)
        return std::basic_string_view<Char>();
      if (!std::isspace(*it)) break;
      ++it;
    }

    for (;;)
    {
      auto prev_end_it = std::prev(end_it);
      if (!std::isspace(*prev_end_it))
        return std::basic_string_view<Char>(&*it, std::distance(it, end_it));
      end_it = prev_end_it;
    }    
  }

  std::optional<double> to_double(const std::string& s)
  {
#ifdef HAS_FP_CHARCONV
    double value = 0.0;
    auto sv = trim_whitespaces(std::string_view(s));
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
    {
      I3S_ASSERT(false);
      return {};
    }
    return value;
#else
    std::istringstream iss(s);
    iss.imbue(std::locale::classic());
    double val;
    iss >> val;
    if (iss.fail())
      return {};
    else
      return val;
    //return std::stod(_get_string()); //WARNING: This function depends on the local which could be a problem.
#endif

  }

  std::optional<double> to_double(const std::wstring& ws)
  {
#ifdef HAS_FP_CHARCONV
    double value = 0.0;
    auto wv = trim_whitespaces(std::wstring_view(ws));
    constexpr auto buffer_size = 2 * std::numeric_limits<double>::max_digits10;
    std::array<char, buffer_size> buffer;
    std::string s;
    char* b = nullptr;
    if (wv.size() <= buffer_size)
    {
      b = buffer.data();
    }
    else
    {
      s.resize(wv.size());
      b = s.data();
    }
    std::transform(wv.begin(), wv.end(), b, [](auto c) { return static_cast<char>(c); });
    auto [ptr, ec] = std::from_chars(b, b + wv.size(), value);
    if (ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range)
    {
      I3S_ASSERT(false);
      return {};
    }
    return value;
#else
    std::wistringstream iss(ws);
    iss.imbue(std::locale::classic());
    double val;
    iss >> val;
    if (iss.fail())
      return {};
    else
      return val;
    //return std::stod(_get_string()); //WARNING: This function depends on the local which could be a problem. 
#endif
  }

}

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

namespace
{

// We would like to use to_chars whenever possible, but its support is poor on some platforms.
// Microsoft's standard library has to_chars implemented for both integer and floating-point types.
// On Linux libstdc++ only provides to_chars for integer types.
// XCode's <charconv> only has to_chars for integer types, but even those may be disabled depending
// on the target OS / version.
// The code below tries to detect whether there's to_chars overload for particular types.
// If to_chars is not available, we have to use to_string or stringstream.

[[maybe_unused]] constexpr bool has_to_chars_integral_(...) { return false; }

template<
  typename T,
  std::to_chars_result(*F)(char*, char*, T, int) = std::to_chars,
  typename = std::void_t<decltype(std::to_chars(nullptr, nullptr, T{}, 10))>
>
constexpr bool has_to_chars_integral_(T) { return true; }

[[maybe_unused]] constexpr bool has_to_chars_fp_(...) { return false; }

template<
  typename T,
  std::to_chars_result(*F)(char*, char*, T) = std::to_chars,
  typename = std::void_t<decltype(std::to_chars(nullptr, nullptr, T{}))>
>
constexpr bool has_to_chars_fp_(T) { return true; }

template<typename T>
constexpr size_t get_buffer_size()
{
  if constexpr (std::is_floating_point<T>::value)
  {
    // Besides max digits we may need space for sign, decimal dot, what else?
    return std::numeric_limits<T>::max_digits10 + 16;
  }
  else
    return std::numeric_limits<T>::digits10 + 1 + static_cast<size_t>(std::is_signed<T>::value);
};

template<typename T>
std::string to_chars_(T value)
{
  std::array<char, get_buffer_size<T>()> buf;
  const auto res = std::to_chars(buf.data(), buf.data() + buf.size(), value);
  I3S_ASSERT(res.ec == std::errc());
  return std::string(buf.data(), res.ptr);
}

template<typename T>
std::string to_string_integral_(T value)
{
  if constexpr (has_to_chars_integral_(T{}))
    return to_chars_(value);
  else
    return std::to_string(value);
}

template<typename T>
std::string to_string_fp_(T value)
{
  if constexpr (has_to_chars_fp_(T{}))
    return to_chars_(value);
  else
  {
    std::stringstream str;
    str.imbue(std::locale::classic());
    str.precision(std::numeric_limits<T>::max_digits10);
    str << value;
    return str.str();
  }
}

}

std::string Variant::to_string() const
{
  switch (m_type)
  {
  case Variant_trait::Type::Bool:    return _get_scalar_value<bool>() ? "true" : "false";
  case Variant_trait::Type::Int8:    return to_string_integral_(_get_scalar_value<int>());
  case Variant_trait::Type::Uint8:   return to_string_integral_(_get_scalar_value<uint8_t>());
  case Variant_trait::Type::Int16:   return to_string_integral_(_get_scalar_value<int16_t>());
  case Variant_trait::Type::Uint16:  return to_string_integral_(_get_scalar_value<uint16_t>());
  case Variant_trait::Type::Int32:   return to_string_integral_(_get_scalar_value<int32_t>());
  case Variant_trait::Type::Uint32:  return to_string_integral_(_get_scalar_value<uint32_t>());
  case Variant_trait::Type::Int64:   return to_string_integral_(_get_scalar_value<int64_t>());
  case Variant_trait::Type::Uint64:  return to_string_integral_(_get_scalar_value<uint64_t>());
  case Variant_trait::Type::Float:   return to_string_fp_(_get_scalar_value<float>());
  case Variant_trait::Type::Double:  return to_string_fp_(_get_scalar_value<double>());
  case Variant_trait::Type::String:  return _get_string();
  case Variant_trait::Type::WString: return utf16_to_utf8(_get_wstring());
  case Variant_trait::Type::Not_set: return "<Null>"; //TBD
  default:
    I3S_ASSERT_EXT(false);
    return {};
  }
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
      return ::to_double(_get_string()).value_or(0.0);
    }
    case Variant_trait::Type::WString:
    {
      return ::to_double(_get_wstring()).value_or(0.0);
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

std::any Variant::to_any() const noexcept
{
  switch (m_type)
  {
    case Variant_trait::Type::String:
    {
      return {_get_string()};
    }
    case Variant_trait::Type::WString:
    {
      return {utf16_to_utf8(_get_wstring())};
    }
    case Variant_trait::Type::Bool:  return {_get_scalar_value<bool>()};
    case Variant_trait::Type::Int8:  return {_get_scalar_value<int8_t>()};
    case Variant_trait::Type::Uint8: return {_get_scalar_value<uint8_t>()};
    case Variant_trait::Type::Int16: return {_get_scalar_value<int16_t>()};
    case Variant_trait::Type::Uint16:return {_get_scalar_value<uint16_t>()};
    case Variant_trait::Type::Int32: return {_get_scalar_value<int32_t>()};
    case Variant_trait::Type::Uint32:return {_get_scalar_value<uint32_t>()};
    case Variant_trait::Type::Int64: return {_get_scalar_value<int64_t>()};
    case Variant_trait::Type::Uint64:return {_get_scalar_value<uint64_t>()};
    case Variant_trait::Type::Float: return {_get_scalar_value<float>()};
    case Variant_trait::Type::Double:return {_get_scalar_value<double>()};
    default:
      I3S_ASSERT(false);
      return {};
  }
}

} // namespace utl

} // namespace i3slib
