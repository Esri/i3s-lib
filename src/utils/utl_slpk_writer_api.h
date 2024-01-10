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
#include "utils/utl_i3s_export.h"
#include "utils/utl_declptr.h"
#include "utils/utl_mime.h"
#include <stdint.h>
#include <filesystem>

namespace i3slib
{

namespace utl
{

class Output_stream
{
public:
  DECL_PTR(Output_stream);
  virtual ~Output_stream() = default;
  virtual int64_t  tellp() = 0;
  virtual void     close() noexcept = 0;
  virtual bool     fail() const = 0;
  virtual bool     good() const = 0;
  virtual Output_stream& write(const char* raw_bytes, std::streamsize count) = 0;
};

/*!
Write a ZIP64 UNCOMPRESSED archive
WARNING: Will fail on MD5 collision on file path. (2 different paths hashing to the same 128bit MD5)
Not optimized for memory usage. will use  24 bytes per file written. so 1 M files -> 24MB of RAM used.
In our use case, files added to archive are small.
this class is  thread-safe .
*/
class Slpk_writer
{
public:
  typedef uint32_t Create_flags;
  enum  Create_flag : Create_flags {
    Unfinalized_only = 1, // Open an **unfinalized** SLPK.
                          // Will fail if SLPK has been finalized already or doesn't exist.
    On_destruction_keep_unfinalized_to_reopen = 2, // In destructor temporary files will be preserved.
                                                   // Allows SLPK being re-opened
                                                   // with Unfinalized_only flag to keep adding files later on. 
    Overwrite_if_exists_and_cancel_in_destructor = 0 // default.
  };
  DECL_PTR(Slpk_writer);
  virtual ~Slpk_writer() = default;
  // --- Slpk_writer: ---
  virtual bool    create_archive(const std::filesystem::path& path, Create_flags flags = Create_flag::Overwrite_if_exists_and_cancel_in_destructor) = 0;
  virtual bool    create_stream(Output_stream::ptr strm) { return false; }

  virtual bool append_file(
    const std::string& archive_Path,
    const char* buffer, int n_bytes,
    utl::Mime_type type = utl::Mime_type::Not_set,
    utl::Mime_encoding pack = utl::Mime_encoding::Not_set) = 0;
  
#if 0 // deprecated TBD
  virtual bool    get_file(const std::string& archive_Path, std::string* content) =0;
#endif
  virtual bool    finalize()  = 0;
  virtual bool    cancel() noexcept = 0; // removes all temporary files or returns false if failed or unsupported
  
  virtual bool    close_unfinalized() noexcept = 0; // closes preserving all appended data in temporary files.
                                                    // Allows SLPK being re-opened
                                                    // with Unfinalized_only flag to keep adding files later on. 
};

Slpk_writer* create_file_slpk_writer(const std::filesystem::path& path);

I3S_EXPORT Slpk_writer*   create_slpk_writer(const std::string& dst_path = std::string());
I3S_EXPORT Output_stream* create_output_stream_std(std::filesystem::path path, std::ios_base::openmode mode); //for unit-testing.

} // namespace utl

} // namespace i3slib
