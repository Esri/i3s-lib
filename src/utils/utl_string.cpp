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
#include "utils/utl_string.h"
#include "utils/utl_i3s_assert.h"
#include <locale>
#include <codecvt>
#include <cstring>
#include <algorithm>

#ifdef PCSL_WIDE_STRING_OS
#include "utils/win/utl_windows.h"
#endif

namespace i3slib
{

namespace utl
{

namespace
{

template< class String_t >
String_t trim_quotes_(const String_t& s)
{
  typedef decltype(s[0]) My_char;
  const My_char c_quote('\"');
  if (s.size() == 0)
    return s;
  int beg = s.front() == c_quote ? 1 : 0;
  int end = (int)s.size() - 1;
  while ((end > beg && s[end] == c_quote) || (s[end] == My_char('\0')))
    end--;
  auto size = end + 1 - beg;
  String_t ret;
  ret.resize(size);
  std::memcpy(&ret[0], &s[beg], size * sizeof(My_char));
  return ret;
}

}

// trim from start (in place)
static void ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    return !std::isspace(ch);
  }));
}

// trim from end (in place)
static void rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
    return !std::isspace(ch);
  }).base(), s.end());
}

// trim from both ends (in place)
void trim_in_place(std::string& s) {
  ltrim(s);
  rtrim(s);
}

std::string trim_quotes(const std::string& s)
{
  return trim_quotes_(s);
}

#ifdef _WIN32
std::string utf16_to_utf8(const std::wstring& s)
{
  if (s.size() == 0)
    return std::string();
  std::string out;
  //let's get the size of the output string:
  int n_bytes = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, 0, 0, NULL, NULL);
  if (n_bytes == 0)
  {
    I3S_ASSERT_EXT(false);
    return std::string();
  }
  out.resize(n_bytes);
  n_bytes = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], (int)out.size(), NULL, NULL);
  if (n_bytes == 0)
    I3S_ASSERT_EXT(false);
  //let's pop the null-terminated char:
  out.pop_back();
  return out;
}

std::wstring utf8_to_utf16(std::string_view s)
{
  std::wstring ret;
  if (s.size() == 0)
    return ret;

  int n_char = ::MultiByteToWideChar(CP_UTF8, 0, std::data(s), static_cast<int>(std::size(s)), nullptr, 0);
  ret.resize(static_cast<size_t>(n_char));

  if (n_char)
  {
    n_char = ::MultiByteToWideChar(CP_UTF8, 0, std::data(s), static_cast<int>(std::size(s)), std::data(ret), n_char);
    I3S_ASSERT_EXT(n_char);
  }
  return ret;
}

#else
// NOTE:
// using std::wstring_convert is about 100 SLOWER than using WideCharToMultiByte
// converting std::wstring(30, L'a') 1,000,000 times:
// std::wstring_convert   ~26000  ms
// WideCharToMultiByte    ~150    ms
std::string utf16_to_utf8(const std::wstring& s)
{
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(s);
}
#endif


#ifdef PCSL_WIDE_STRING_OS

std::string os_to_utf8(const String_os& s)
{
  return utf16_to_utf8(s);
}

String_os utf8_to_os(std::string_view s)
{
  return utf8_to_utf16(s);
}

String_os utf8_to_os(std::string&& s)
{
  return utf8_to_utf16(s);
}

String_os trim_quotes(const String_os& s)
{
  return trim_quotes_(s);
}

int stricmp(const char* a, const char* b)
{
  return _stricmp(a, b);
}

#else

String_os utf8_to_os(std::string_view s)
{
  return std::string(s);
}

String_os utf8_to_os(std::string&& s)
{
  return s;
}

#endif

void replace_all(std::string& str, const std::string& from, const std::string& to)
{
  if (from.empty())
    return;

  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos)
  {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
  }
}

} // namespace utl

} // namespace i3slib
