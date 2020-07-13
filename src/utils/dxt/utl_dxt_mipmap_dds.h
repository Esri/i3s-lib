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
#include <string>
#include "utils/utl_buffer.h"
#include "utils/utl_image_2d.h"

namespace i3slib
{

namespace utl
{

static const int c_sse_alignment = 16;
bool convert_to_dds_with_mips(const std::string& jpeg, utl::Buffer_view<char>* out, int not_used = 0);
bool compress_to_dds_with_mips(Image_2d& src, bool has_alpha, utl::Buffer_view<char>* dds_out);

}

} // namespace i3slib
