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

namespace i3slib
{

namespace utl
{
//! only use for DDS. 
class I3S_EXPORT Image_2d
{
public:
  typedef unsigned int pixel_t;
  Image_2d() = default;
  ~Image_2d();
  Image_2d& operator=(Image_2d&& src);
  Image_2d(Image_2d&& src);

  Image_2d& operator=(Image_2d& src) =delete;
  Image_2d(Image_2d& src) = delete;
  
  int                width() const  { return m_w; }
  int                height() const { return m_h; }
  void*              data() { return m_data; }
  const void*        data() const { return m_data; }
  int                size_in_bytes() const { return m_w * m_h * sizeof( pixel_t); }
  void               swap(Image_2d& src);
  void               resize( int w, int h, bool dont_free_on_min = true );
  void               fill(pixel_t rgba);
public:
  //static void        mipmap(const Image_2d& src, Image_2d* dst );
  static void        mipmap_blocked4x4(const Image_2d& src, Image_2d* dst);
  static Image_2d    create_aligned(int w, int h, const void* src = nullptr, int src_bytes = 0);
  static Image_2d    wrap_aligned(int w, int h, void* data, int src_bytes);
  //static Image_2d    copy_or_wrap_aligned(int w, int h, void* src, int src_bytes );

private:
  static constexpr int c_byte_alignment=16;
  int         m_w = 0;
  int         m_h =0;
  pixel_t*    m_data=nullptr;
  bool        m_is_owned=false;
  bool m_blocked = false;
  int m_capacity_in_bytes = 0;
};

}

} // namespace i3slib
