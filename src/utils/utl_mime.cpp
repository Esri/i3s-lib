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
#include "utils/utl_mime.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_bitstream.h"
#include <cstring>

namespace i3slib
{

namespace utl
{

namespace
{

static_assert((Mime_type_set)Mime_type::_next_ == 1024, "Please update constants 'string' below");
constexpr int c_type_ext_count = 9;
const char* const c_type_ext[c_type_ext_count]
{
  //MUST BE IN POWER OF 2 ORDER ( to_known_mime_type() expects this)
  "json", // = 2,
  "bin",// = 4,
  "jpg", //= 8,
  "png", // = 16,
  "dds",// = 32,
  "bin.dds",// = 64,
  "bin.ktx",// = 128,
  "basis",// = 256,
  "ktx2" // = 1024,
};

static_assert((Mime_encoding_set)Mime_encoding::_next_ == 32, "Please update constants 'string' below");
constexpr int c_encoding_ext_count = 4;
const char* const c_encoding_ext[c_encoding_ext_count]
{
  "gz", //= 2,
  "pccxyz", // = 4,
  "pccrgb",// = 8,
  "pccint"// = 16,
};

}

void add_slpk_extension_to_path(std::string* path, const Mime_type& type, const Mime_encoding& pack)
{
  I3S_ASSERT((Mime_type_set)type && (Mime_encoding_set)pack);
  int bit = first_bit_set((Mime_type_set)type) - 2;
  if (bit >= 0 && bit < c_type_ext_count)
  {
    path->append(".").append(c_type_ext[bit]);
  }

  bit = first_bit_set((Mime_encoding_set)pack) - 2;
  if (bit >= 0 && bit < c_encoding_ext_count)
    path->append(".").append(c_encoding_ext[bit]);
}

//! returns Not_set if not known. This function is case-sensitive
Mime_type to_known_mime_type(const char* str, int len)
{
  for (int i = 0; i < c_type_ext_count; ++i)
  {
    int expected_len = (int)std::strlen(c_type_ext[i]);
    if (expected_len == len && std::memcmp(c_type_ext[i], str, expected_len) == 0)
      return (Mime_type)(1 << (i + 1));
  }
  return Mime_type::Not_set;
}

} // namespace utl

} // namespace i3slib
