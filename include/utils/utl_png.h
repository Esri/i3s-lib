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

//#ifdef USE_LIB_PNG
#pragma once

#include "utils/utl_geom.h"
#include "utils/utl_string.h"
#include "utils/utl_i3s_export.h"
#include "utils/utl_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fstream>
#include <vector>
#include <filesystem>

namespace i3slib
{

namespace utl 
{

I3S_EXPORT bool decode_png(const std::string& input, int* out_w, int* out_h, std::vector<char>* out, bool* has_alpha = nullptr);

I3S_EXPORT bool decode_png(const char* src, int bytes, Buffer_view<char>* out
                                , int* out_w = nullptr, int* out_h = nullptr, bool* has_alpha = nullptr
                                , int output_channel_count = 4
                                , int dst_byte_alignment = 0);
 
I3S_EXPORT bool get_png_size(const char* src, int bytes, int* w, int* h);



I3S_EXPORT std::vector< char > r8_to_rgba8(const uint8_t* src, int nbytes);

I3S_EXPORT bool  read_png_from_file(const std::filesystem::path& file_name, int* out_w, int* out_h, std::vector<char>* out);
//I3S_EXPORT bool  read_png_from_buffer(const std::string& input, int* out_w, int* out_h, std::vector<char>* out);

//! util class to write (sequentially) a PNG file. File is finalized on object destruction
class Png_writer_impl;
class Png_writer
{
public:
  I3S_EXPORT Png_writer();
  I3S_EXPORT ~Png_writer();
  I3S_EXPORT bool create_rgba32( const std::filesystem::path& file_name, int w, int h );

  I3S_EXPORT bool      add_rows( int n_rows, const char* buffer, int n_bytes );

  I3S_EXPORT static bool write_file( const std::filesystem::path& path, int w, int h, const char* buffer, int n_bytes );

private:
  std::unique_ptr<Png_writer_impl> m_pimpl;
};

I3S_EXPORT bool encode_png(const uint8_t* raw_bytes, int w, int h, bool has_alpha, std::vector<uint8_t>& png_bytes);

}//endof ::utl

} // namespace i3slib
