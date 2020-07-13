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

#include "utils/utl_string.h"
#include "utils/utl_i3s_assert.h"
#include <vector>
#include <fstream>

#include <filesystem>
namespace stdfs = std::filesystem;

namespace i3slib
{

namespace utl 
{

// This allows passing std::filesystem::path arguments to Basic_tracker logging funcs.
template<> inline std::string to_string(const stdfs::path& path) { return path.u8string(); }

I3S_EXPORT bool create_directory_recursively(stdfs::path path);
I3S_EXPORT bool file_exists(const stdfs::path& path);
I3S_EXPORT bool folder_exists(const stdfs::path& path);

I3S_EXPORT bool remove_file(const stdfs::path& path);

// Read all bytes from file into string.
I3S_EXPORT std::string read_file(const stdfs::path& path);

// Write byte array to file.
I3S_EXPORT bool write_file(const stdfs::path& path, const char* data, size_t bytes);

// Write string to file.
inline bool write_file(const stdfs::path& path, const std::string& content)
{ return write_file(path, content.data(), content.size()); }

inline stdfs::path make_path(const stdfs::path& ref_path, const stdfs::path& res_path)
{
  return (ref_path / res_path).lexically_normal();
}


stdfs::path::string_type get_generic_path_name(const stdfs::path& path);

class Scoped_folder
{
public:
  I3S_EXPORT explicit Scoped_folder(stdfs::path folder_path):
    path_(std::move(folder_path))
  {}

  Scoped_folder(Scoped_folder&&) = default;

  Scoped_folder(const Scoped_folder&) = delete;
  Scoped_folder& operator=(const Scoped_folder&) = delete;

  I3S_EXPORT bool delete_folder(
    std::ostream& utf8_error_message_destination, 
    bool also_report_deleted_file_count = false) noexcept;
  
  I3S_EXPORT void delete_folder();

  I3S_EXPORT ~Scoped_folder() noexcept;

  const stdfs::path& path() const { return path_; }

private:
  stdfs::path path_;
};

Scoped_folder I3S_EXPORT create_temporary_folder(const stdfs::path& prefix);

} // namespace utl

} // namespace i3slib
