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

[[ nodiscard ]] inline String_os to_string_os(const stdfs::path& path) {
#ifdef PCSL_WIDE_STRING_OS
  return path.generic_wstring();
#else
  return utl::to_string(path.generic_u8string());
#endif
}

// This allows passing std::filesystem::path arguments to Basic_tracker logging funcs.
template<>
[[ nodiscard ]] inline std::string to_string(const stdfs::path& path) {
  return to_string(path.generic_u8string());
}

#ifdef __cpp_char8_t
template <class Source>
std::filesystem::path u8path(const Source& source)
{
  return std::filesystem::path(source);
}

template <>
inline std::filesystem::path u8path(const std::string& source)
{
  return std::filesystem::path(as_u8string_view(source));
}

inline std::filesystem::path u8path(std::string_view source)
{
  return std::filesystem::path(as_u8string_view(source));
}

template< class InputIt >
std::filesystem::path u8path(InputIt first, InputIt last)
{
  return std::filesystem::path(first, last);
}
#else
using std::filesystem::u8path;
#endif

inline std::string to_url(const stdfs::path& p)
{
  auto r = std::string("file://");
  r.append(as_string_view(p.generic_u8string()));
  return r;
}

[[ nodiscard ]]
I3S_EXPORT bool create_directory_recursively(stdfs::path path)noexcept;
[[ nodiscard ]]
I3S_EXPORT bool file_exists(const stdfs::path& path)noexcept;
[[ nodiscard ]]
I3S_EXPORT bool folder_exists(const stdfs::path& path)noexcept;

[[ nodiscard ]]
I3S_EXPORT bool remove_file(const stdfs::path& path)noexcept; // returns true if file was deleted or if it did not exist

// Read all bytes from file into string.
[[ nodiscard ]]
I3S_EXPORT std::string read_file(const stdfs::path& path)noexcept;

// Write byte array to file.
[[ nodiscard ]]
I3S_EXPORT bool write_file(const stdfs::path& path, const char* data, size_t bytes)noexcept;

// Write string to file.
[[ nodiscard ]]
inline bool write_file(const stdfs::path& path, const std::string& content)noexcept
{ return write_file(path, content.data(), content.size()); }

[[ nodiscard ]] inline
stdfs::path make_path(const stdfs::path& ref_path, const stdfs::path& res_path) // can throw bad_alloc or bad_array_new_length
{
  return (ref_path / res_path).lexically_normal();
}

[[ nodiscard ]]
I3S_EXPORT stdfs::path::string_type get_generic_path_name(const stdfs::path& path) noexcept;

class Scoped_folder
{
public:
  I3S_EXPORT explicit Scoped_folder(stdfs::path folder_path) noexcept:
    path_(std::move(folder_path))
  {}
  Scoped_folder() noexcept = default;
  Scoped_folder(Scoped_folder&&) noexcept = default;

  Scoped_folder(const Scoped_folder&) = delete;
  Scoped_folder& operator=(const Scoped_folder&) = delete;

  [[ nodiscard ]]
  I3S_EXPORT bool delete_folder(
    std::ostream& utf8_error_message_destination, 
    bool also_report_deleted_file_count = false) noexcept;
  
  I3S_EXPORT void delete_folder() noexcept;

  I3S_EXPORT ~Scoped_folder() noexcept;

  [[ nodiscard ]]
  const stdfs::path& path() const { return path_; }

private:
  stdfs::path path_;
};

// in case of an error returns Scoped_folder(). Call folder.path().empty() to test that the folder was not created.
[[ nodiscard ]]
Scoped_folder I3S_EXPORT create_temporary_folder(stdfs::path prefix) noexcept;
[[ nodiscard ]]
stdfs::path create_temporary_folder_path(stdfs::path prefix) noexcept;

} // namespace utl

} // namespace i3slib
