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
#include <stdint.h>

namespace i3slib
{
namespace utl
{

enum Alpha_mode { Default, Pre_mult };

void resample_2d_uint8(
  uint32_t src_width, uint32_t src_height, uint32_t dst_width, uint32_t dst_height,
  const void* src, void* dst, uint32_t channels, Alpha_mode alpha_mode);

} // namespace utl 
} // namespace i3slib
