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
#include "utils/utl_fs.h"
#include "utils/utl_platform_def.h"
#include <fstream>
#include <array>
#include <ctime>
#include <iostream>

#ifdef _WIN32
#include "utils/win/utl_windows.h"
#else
#include <stdlib.h>
#endif

namespace i3slib
{

namespace utl
{

bool create_directory_recursively(stdfs::path path)
{
  std::error_code ec;
  return stdfs::create_directories(path.make_preferred(), ec);
}

bool file_exists(const stdfs::path& path)
{
  std::error_code ec;
  const auto status = stdfs::status(path, ec);
  return !ec && stdfs::exists(status) && stdfs::is_regular_file(status);
}

bool folder_exists(const stdfs::path& path)
{
  std::error_code ec;
  const auto status = stdfs::status(path, ec);
  return stdfs::exists(status) && stdfs::is_directory(status);
}
bool remove_file(const stdfs::path& path)
{
  std::error_code ec;
  return stdfs::remove(path,ec) && !ec;
}

bool write_file(const stdfs::path& path, const char* data, size_t bytes)
{
  std::ofstream t(path, std::ios::binary);
  t.write(data, bytes);
  return t.good() && !t.fail();
}

std::string read_file(const stdfs::path& path)
{
  std::ifstream t(path, std::ios::binary | std::ios::ate);
  size_t size = (size_t)t.tellg();
  t.seekg(0);
  if (!size || t.fail() || !t.good())
    return {};

  std::string buffer;
  buffer.resize(size);
  t.read(buffer.data(), size);
  return buffer;
}

namespace
{
template<typename Char> struct Generic_string_method_selector;
template<> struct Generic_string_method_selector<char> { static std::basic_string<char> invoke(const stdfs::path& path) { return path.generic_string(); } };
template<> struct Generic_string_method_selector<wchar_t> { static std::basic_string<wchar_t> invoke(const stdfs::path& path) { return path.generic_wstring(); } };
template<> struct Generic_string_method_selector<char16_t> { static std::basic_string<char16_t> invoke(const stdfs::path& path) { return path.generic_u16string(); } };
template<> struct Generic_string_method_selector<char32_t> { static std::basic_string<char32_t> invoke(const stdfs::path& path) { return path.generic_u32string(); } };
};

stdfs::path::string_type get_generic_path_name(const stdfs::path& path)
{
  return Generic_string_method_selector<stdfs::path::value_type>::invoke(path);
}

namespace
{
std::string get_file_name_safe_timestamp()
{
  std::array<char, 32> timestamp; // to fit "2018-12-31-12-59-59\0"
  const auto t = std::time(nullptr);
  std::tm timeinfo;
  localtime_s(&timeinfo, &t);
  [[maybe_unused]] auto count = std::strftime(timestamp.data(), timestamp.size(), "%Y-%m-%d-%H-%M-%S", &timeinfo);
  I3S_ASSERT(count > 0);
  return timestamp.data();
}
}

bool Scoped_folder::delete_folder(
  std::ostream& error_message_destination, 
  bool also_report_deleted_file_count) noexcept
{
  try
  {
    if (!path_.empty())
    {
      std::error_code ec;
      auto deleted_file_count = stdfs::remove_all(path_, ec);
      if (!ec)
      {
        if (also_report_deleted_file_count)
        {
          error_message_destination << "Info: successfully deleted " << deleted_file_count 
            << " in " << path_ << std::endl;
        }
        path_.clear();
        return true;
      }
      else
      {
        std::error_code ec1;
        auto dir_it = stdfs::directory_iterator(path_, ec1);
        if (!ec1)
        {
          for (auto& dir_entry: dir_it)
          {
            if (dir_entry.is_regular_file())
            {
              stdfs::remove(dir_entry.path(), ec1);
              if (ec1)
              {
                error_message_destination << "Error: can't delete file '" << dir_entry.path().generic_u8string()
                  << "'. Reason: " << ec.message() << std::endl;

              }
            }
            else if (dir_entry.is_directory())
            {
              stdfs::remove_all(dir_entry.path(), ec1);
              if (ec1)
              {
                error_message_destination << "Error: can't delete sub-folder '" << dir_entry.path().generic_u8string()
                  << "'. Reason: " << ec.message() << std::endl;
              }
            }
          }
        }

        error_message_destination << "Error: can't delete folder '" << path_.generic_u8string()
          << "'. Reason: " << ec.message() << std::endl;
        path_.clear();
        return false;
      }
    }
    return true;
  }
  catch (const std::bad_alloc & e)
  {
    error_message_destination << "Error: can't delete folder '" << path_
      << "'. Reason: allocation failure (" << e.what() << ")" << std::endl;
    return false;
  }
  catch (const std::exception & e)
  {
    error_message_destination << "Error: can't delete folder '" << path_.generic_u8string()
      << "Reason: " << e.what() << std::endl;
    return false;
  }
}

void Scoped_folder::delete_folder()
{
  if (!path_.empty())
  {
    stdfs::remove_all(path_);
    path_.clear();
  }
}

Scoped_folder::~Scoped_folder() noexcept
{
  delete_folder(std::cerr, false);
}

Scoped_folder create_temporary_folder(const stdfs::path& prefix)
{
  auto temp_path = stdfs::temp_directory_path();
  temp_path /= prefix;
  temp_path += get_file_name_safe_timestamp();
  stdfs::create_directories(temp_path);
  return Scoped_folder(temp_path);
}

} // namespace utl

} // namespace i3slib
