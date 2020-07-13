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

#include "utils/utl_i3s_export.h"
#include <vector>
#include "utils/utl_buffer.h"

namespace i3slib
{

namespace utl
{

I3S_EXPORT bool decompress_jpeg(const char* src, int bytes, Buffer_view<char>* out
                                , int* out_w=nullptr, int* out_h=nullptr, bool* has_alpha = nullptr
                                , int output_channel_count=4
                                , int dst_byte_alignment = 0);

I3S_EXPORT bool compress_jpeg( int w, int h, const char* src, int bytes, Buffer_view<char>* out, int channel_count, int quality=75, int src_row_stride=0);

I3S_EXPORT bool get_jpeg_size(const char* src, int bytes, int* w, int* h);

}

} // namespace i3slib
