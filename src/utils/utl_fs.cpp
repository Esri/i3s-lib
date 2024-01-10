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

namespace i3slib::utl
{

bool create_directory_recursively(stdfs::path path) noexcept
{
  std::error_code ec;
  stdfs::create_directories(path.make_preferred(), ec);
  return !ec;
}

bool file_exists(const stdfs::path& path)noexcept
{
  std::error_code ec;
  const auto status = stdfs::status(path, ec);
  return !ec && stdfs::exists(status) && stdfs::is_regular_file(status);
}

bool folder_exists(const stdfs::path& path)noexcept
{
  std::error_code ec;
  const auto status = stdfs::status(path, ec);
  return stdfs::exists(status) && stdfs::is_directory(status);
}
bool remove_file(const stdfs::path& path)noexcept
{
  std::error_code ec;
  stdfs::remove(path, ec);
  return !ec;
}

bool write_file(const stdfs::path& path, const char* data, size_t bytes) noexcept
{
  std::ofstream t(path, std::ios::binary);
  return t.is_open() && t.write(data, bytes);
}

std::string read_file(const stdfs::path& path) noexcept
{
  if (std::ifstream t{ path, std::ios::binary | std::ios::ate }; t.is_open())
  {
    size_t size = (size_t)t.tellg();
    t.seekg(0);
    if (size && t.good())
    {
      std::string buffer;
      buffer.resize(size);
      if (t.read(buffer.data(), size))
        return buffer;
    }
  }
  return {};
}

namespace
{
template<typename Char> struct Generic_string_method_selector;
template<> struct Generic_string_method_selector<char> { static std::basic_string<char> invoke(const stdfs::path& path) { return path.generic_string(); } };
template<> struct Generic_string_method_selector<wchar_t> { static std::basic_string<wchar_t> invoke(const stdfs::path& path) { return path.generic_wstring(); } };
template<> struct Generic_string_method_selector<char16_t> { static std::basic_string<char16_t> invoke(const stdfs::path& path) { return path.generic_u16string(); } };
template<> struct Generic_string_method_selector<char32_t> { static std::basic_string<char32_t> invoke(const stdfs::path& path) { return path.generic_u32string(); } };
};

stdfs::path::string_type get_generic_path_name(const stdfs::path& path) noexcept
{
  return Generic_string_method_selector<stdfs::path::value_type>::invoke(path);
}

namespace
{
std::string get_file_name_safe_timestamp() noexcept
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
                error_message_destination << "Error: can't delete file '" 
                  << utl::to_string(dir_entry.path())
                  << "'. Reason: " << ec.message() << std::endl;

              }
            }
            else if (dir_entry.is_directory())
            {
              stdfs::remove_all(dir_entry.path(), ec1);
              if (ec1)
              {
                error_message_destination << "Error: can't delete sub-folder '" 
                  << utl::to_string(dir_entry.path())
                  << "'. Reason: " << ec.message() << std::endl;
              }
            }
          }
        }

        error_message_destination << "Error: can't delete folder '" 
          << utl::to_string(path_)
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
    error_message_destination << "Error: can't delete folder '" 
      << utl::to_string(path_)
      << "Reason: " << e.what() << std::endl;
    return false;
  }
}

void Scoped_folder::delete_folder() noexcept
{
  if (!path_.empty())
  {
    std::error_code ec;
    stdfs::remove_all(path_, ec);
    path_.clear();
    I3S_ASSERT(!ec);
  }
}

Scoped_folder::~Scoped_folder() noexcept
{
  [[maybe_unused]] auto res = delete_folder(std::cerr, false);
}


stdfs::path create_temporary_folder_path(stdfs::path prefix) noexcept
{
  I3S_ASSERT(!prefix.has_root_path());
  std::error_code ec;
  auto temp_path = stdfs::temp_directory_path(ec);
  I3S_ASSERT(!ec);
  if (ec)
    return {};
  if (prefix.has_parent_path())
  {
    temp_path /= prefix.parent_path();
    prefix = prefix.filename();
  }
  [[maybe_unused]]
  bool base_folder_creation_result_to_ignore = stdfs::create_directory(temp_path, ec);
  I3S_ASSERT(!ec);
  if (ec) // only error code need to be checked. It's ok and even expected that the directory already 
    return {};
  temp_path /= prefix;
  stdfs::path full_path = temp_path;
  auto timestamp = get_file_name_safe_timestamp();
  full_path += timestamp;
  constexpr int num_attempts = 1024;
  for (int i = 0; i < num_attempts; ++i)
  {
    if (stdfs::create_directory(full_path, ec))
      return full_path;
    I3S_ASSERT(!ec);
    full_path = temp_path;
    full_path += timestamp;
    full_path += '_';
    full_path += std::to_string(i);
  }
  return {};
}

Scoped_folder create_temporary_folder(stdfs::path prefix) noexcept
{
  auto path = create_temporary_folder_path(prefix);

  return path.empty() ? Scoped_folder() : Scoped_folder(path);

}


} // namespace utl
