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

#pragma once
#ifndef __I3SLIB_UTL_STRING_H__
#define __I3SLIB_UTL_STRING_H__

#include "utils/utl_i3s_export.h"
#include <string>
#include <sstream>

#ifdef _WIN32
#define PCSL_WIDE_STRING_OS
#endif

namespace i3slib
{

namespace utl
{

#ifdef PCSL_WIDE_STRING_OS

//! represent a "buffer" for a OS api string (path, resource strings, etc.). 
//! On Windows encoding is UTF-16LE, so a wstring buffer is preferred. 
//! Other OS may use UTF-8, so we would  define String_os as std::string instead. 
using String_os = std::wstring;
using String_os_ostream = std::wostringstream;
using String_os_istream = std::wistringstream;
using String_view_os = std::wstring_view;

#define I3S_T(x) L##x
#define I3S_CHAR_T wchar_t

I3S_EXPORT std::string os_to_utf8(const String_os& s);
I3S_EXPORT String_os   utf8_to_os(const std::string& s);
I3S_EXPORT String_os   trim_quotes(const String_os& s);
I3S_EXPORT int         stricmp(const char* a, const char* b);

#else // PCSL_WIDE_STRING_OS

//TODO: for now, we assume UTF-8 same path/names
using String_os = std::string;
using String_os_ostream = std::ostringstream;
using String_os_istream = std::istringstream;
using String_view_os = std::string_view;  // it can be std::u8string_view in C++20

#define I3S_T(x) x
#define I3S_CHAR_T char

inline std::string os_to_utf8(const String_os& s) { return s; }
inline String_os   utf8_to_os(const std::string& s) { return s; }

#endif // PCSL_WIDE_STRING_OS

I3S_EXPORT std::string utf16_to_utf8(const std::wstring& s);
I3S_EXPORT std::string trim_quotes(const std::string& s);
I3S_EXPORT std::string encode_double_quote(const std::string& s);
I3S_EXPORT std::string decode_double_quote(const std::string& s);
I3S_EXPORT void replace_all(std::string& str, const std::string& from, const std::string& to);

//
template<class T> std::string to_string(const T& p) { return std::to_string(p); }
template<> inline std::string to_string(const std::string& p) { return p; }
#ifdef PCSL_WIDE_STRING_OS
template<> inline std::string to_string(const std::wstring& p) { return os_to_utf8(p); }
#endif
// till C++20 where it will be part of standard
inline constexpr bool starts_with(std::string_view str,
  std::string_view prefix) noexcept
{
  auto l = prefix.length();
  return str.length() >= l && str.substr(0, l) == prefix;
}

inline constexpr bool ends_with(std::string_view str, std::string_view suffix)
{
  return str.size() >= suffix.size() &&
    0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

// copy/paste from the above since deduction works 
// before casting/conversion and can't be used here:
// "Attempting to generalize functions to different character types 
// by accepting basic_string_view instead of string_view or 
// wstring_view prevents the intended use of implicit conversion."
inline constexpr bool ends_with(std::wstring_view str, std::wstring_view suffix)
{
  return str.size() >= suffix.size() &&
    0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

template<typename Char>
std::basic_string<Char> to_lower(std::basic_string<Char> str)
{
  auto &f = std::use_facet<std::ctype<Char>>(std::locale());
  f.tolower(str.data(), str.data() + str.size());
  return str;
}

} // namespace utl

} // namespace i3slib

#endif //__I3SLIB_UTL_STRING_H__
