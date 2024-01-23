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

#include <vector>
#include <string>
#include <stdint.h>
#include "utils/utl_i3s_export.h"
#include "i3s/i3s_common.h"

namespace i3slib
{
  namespace utl
  {
    // Encoding
    //enum class Basis_output_format { KTX2 = 1, BASIS = 2 };

    // For normal maps(Texture_semmantic::Normal_map) UASTC  mode will be enabled.
    // This will create higher quality a texture, similar to BC7, which is suitable for normal maps,
    // but will result in a larger file size.
    // Everything else will use ETC1S.
    I3S_EXPORT bool compress_to_basis_with_mips(
      const char* data,
      int w, int h,
      int component_count, // must be 3 or 4 (rgb8 or rgba8)
      std::string& basis_out,
      i3s::Texture_semantic sem = i3s::Texture_semantic::Base_color
    ); // on conversion to 1.8+. Creates a ktx2 file.

    // TRANSCODING
    enum class Transcoder_format { BC1 = 1, BC3 = 2, BC7 = 3, RGBA = 4, ETC2 = 5};

    I3S_EXPORT bool transcode_mip_level(const char* basis_img, int num_bytes, int mip_level, std::vector<uint8_t>* out, Transcoder_format fmt);
    I3S_EXPORT bool get_basis_image_info(const char* basis_img, int num_bytes, int* mip0_w, int* mip0_h, int* mipmap_count);

    // for SLL test viewer. On-the-fly transcoding to dds
    // trancodes basis file to BC3 (if has alpha), BC1 (no alpha)
    // includes mipmaps
    bool transcode_basis_to_dds(const char* basis_img, int num_bytes, std::vector<uint8_t>* out, bool has_alpha);

    // trancodes basis file to ETC2
    // includes mipmaps
    bool transcode_basis_to_ktx(const char* basis_img, int num_bytes, std::vector<uint8_t>* out, bool has_alpha);

  } // namespace ::utl 

} // namespace ::i3slib
