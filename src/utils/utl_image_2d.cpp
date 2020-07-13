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
#include "utils/utl_image_2d.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_bitstream.h"
#include "utils/utl_platform_def.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace i3slib
{
namespace utl
{

Image_2d::~Image_2d()
{
  if( m_is_owned && m_data)
    _aligned_free(m_data);
}

Image_2d& Image_2d::operator=(Image_2d&& src)
{
  std::memcpy(this, &src, sizeof(Image_2d));
  src.m_data = nullptr;
  return *this;
} //shallow copy

Image_2d::Image_2d(Image_2d&& src)
{
  std::memcpy(this, &src, sizeof(Image_2d));
  src.m_data = nullptr;
} //shallow copy


void Image_2d::resize(int w, int h, bool dont_free_on_min)
{
  int bytes = w * h * sizeof(pixel_t);
  if (bytes > m_capacity_in_bytes || !dont_free_on_min)
  {
    I3S_ASSERT_EXT(m_data == nullptr || m_is_owned == true); // Not supported yet. (since wrapped buffer *must* have been allocated with _aligned_malloc() )
    //re-alloc:
    if (m_is_owned)
      _aligned_free( m_data );
    m_data = (pixel_t*)_aligned_malloc( bytes, c_byte_alignment);
    m_is_owned = true;
    m_capacity_in_bytes = bytes;
  }
  m_w = w;
  m_h = h;
}


void  Image_2d::swap(Image_2d& src)
{
  std::swap(*this, src);
}

__forceinline uint32_t average_rgba(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept
{
  static const uint32_t m0 = 0x000000FFu;
  static const uint32_t m1 = 0x0000FF00u;
  static const uint32_t m2 = 0x00FF0000u;

  uint32_t _0 = ((a & m0) + (b & m0) + (c & m0) + (d & m0)) >> 2u;
  uint32_t _1 = ((a & m1) + (b & m1) + (c & m1) + (d & m1)) >> 2u;
  uint32_t _2 = ((a & m2) + (b & m2) + (c & m2) + (d & m2)) >> 2u;
  //last one may overflow, so don't sum in-place:
  uint32_t _3 = (((a >> 24u) & m0) + ((b >> 24u) & m0) + ((c >> 24u) & m0) + ((d >> 24u) & m0)) >> 2u;
  return _0 | (_1 &m1)  | (_2 & m2)  | (_3 << 24u);
}

void Image_2d::mipmap_blocked4x4(const Image_2d& src, Image_2d* dst)
{
  I3S_ASSERT_EXT(src.width() >= 1 && src.height() >= 1);
  I3S_ASSERT_EXT(is_power_of_two(src.width()) && is_power_of_two(src.height()));

  // we assume that if width < 4 or/and height < 4 then src was produced by the method as well and so is also blocked
  I3S_ASSERT_EXT((src.width() >= 4 && src.height() >= 4) || src.m_blocked);
  auto src_w = src.width();
  auto new_w = std::max(1, src_w / 2);
  auto new_h = std::max(1, src.height() / 2);

  if (new_w >= 4 && new_h >= 4)
  {
    dst->resize(new_w, new_h);
    auto* dst_it = dst->m_data;
    auto two_src_w = 2 * src_w;
    auto* src_row = src.m_data;
    for (int y = 0; y < new_h; ++y, src_row += two_src_w)
    {
      auto* s0 = src_row;
      auto * s1 = s0 + src_w;
      for (int x = 0; x < new_w; ++x, ++dst_it)
      {
        *dst_it = average_rgba(s0[0], s0[1], s1[0], s1[1]);
        s0 += 2;
        s1 += 2;
      }
    }
  }
  else
  {
    dst->m_blocked = true;
    dst->m_h = new_h;
    dst->m_w = new_w;
    auto block_w = (new_w + 3) / 4;
    auto block_h = (new_h + 3) / 4;
    auto padded_w = block_w * 4;
    auto padded_h = block_h * 4;
    auto new_size_in_bytes = padded_w * padded_h  * sizeof(pixel_t);
    if (new_size_in_bytes > dst->m_capacity_in_bytes)
    {
      if (dst->m_is_owned)
        _aligned_free(dst->m_data);

      dst->m_data = (pixel_t*)_aligned_malloc(new_size_in_bytes, c_byte_alignment);
      dst->m_is_owned = true;
      dst->m_capacity_in_bytes = (int)new_size_in_bytes;
    }
    auto pad_pixel_count = padded_w - new_w;
    auto pad_row_count = padded_h - new_h;
    auto* dst_it = dst->m_data;
    auto padded_src_w = ((src_w + 3) / 4) * 4;
    auto two_padded_src_w = 2 * padded_src_w;
    auto* src_row = src.m_data;
    for (int y = 0; y < new_h; ++y, src_row += two_padded_src_w)
    {
      auto* s0 = src_row;
      auto* s1 = s0 + padded_src_w;
      for (int x = 0; x < new_w; ++x, ++dst_it)
      {
        *dst_it = average_rgba(s0[0], s0[1], s1[0], s1[1]);
        s0 += 2;
        s1 += 2;
      }
      dst_it = std::fill_n(dst_it, pad_pixel_count, dst_it[-1]);
    }
    auto fill_row_begin = dst_it - padded_w;
    auto fill_row_end = dst_it;

    for (int i = 0; i < pad_row_count; ++i)
    {
      dst_it = std::copy(fill_row_begin, fill_row_end, dst_it);
    }
  }
}

Image_2d Image_2d::create_aligned(int w, int h, const void* src, int src_bytes)
{
  Image_2d ret;
  ret.m_w = w;
  ret.m_h = h;
  ret.m_data = (pixel_t*)_aligned_malloc(w * h * sizeof(pixel_t), c_byte_alignment);
  ret.m_is_owned = true;
  if (src && src_bytes)
  {
    if ( w * h * sizeof(pixel_t) > src_bytes)
    {
      I3S_ASSERT(false);
      return Image_2d();
    }
    std::memcpy(ret.m_data, src, w*h * sizeof(pixel_t));
  }
  ret.m_capacity_in_bytes = src_bytes;
  return ret;
}


Image_2d Image_2d::wrap_aligned(int w, int h, void* data, int src_bytes)
{
  I3S_ASSERT_EXT(src_bytes == w * h * sizeof(pixel_t));
  Image_2d ret;
  ret.m_w = w;
  ret.m_h = h;
  ret.m_data = (pixel_t*)data;
  ret.m_is_owned = false;
  ret.m_capacity_in_bytes = static_cast<size_t>(src_bytes);
  return ret;
}

void Image_2d::fill(pixel_t rgba)
{
  for (int i = 0; i < m_w*m_h; ++i)
  {
    int y =(int)( double(i / m_w) * 255.0 / double(m_h ) );

    m_data[i] = (rgba & 0xFFFFFF00 ) | y;
  }
}

} // namespace utl
} // namespace i3slib
