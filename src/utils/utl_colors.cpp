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
#include "utils/utl_colors.h"
#include <stdint.h>

namespace i3slib
{

namespace utl
{

//! function to create a bi-color gradient:  
void  create_gradient(int size, Rgba8 c1, Rgba8 c2, std::vector< Rgba8 >* out)
{
  out->resize(size);
  for (int i = 0; i < (int)out->size(); i++)
  {
    float alpha = (float)i / (float)(out->size() - 1);
    (*out)[i] = mix(c1, c2, alpha);
  }
}

void  create_step_colors(std::vector< Rgba8 >* out)
{
  const uint32_t colors[6] = { 0x0000FF, 0x00FF00, 0xFF0000, 0xFF00FF, 0xFFFF00, 0x00FFFF };
  out->resize(6);
  for (size_t i = 0; i < out->size(); i++)
    (*out)[i] = Rgba8(colors[i] | 0xFF000000);
}


void rgba2bgra(uint32_t* rgba, int n)
{
  Rgba8* p = reinterpret_cast<Rgba8*>(rgba);
  uint8_t tmp;
  Rgba8* end = p + n;
  while (p < end)
  {
    tmp = p->r;
    p->r = p->b;
    p->b = tmp;
    p++;
  }
}

//for testing/debugging
Rgba8 lod_to_color(int lod)
{
  static const uint32_t n_colors = 10;
  static const Rgba8 colors[n_colors] =
  { 
    { 0xFF, 0x00, 0x00, 0xFF },
    { 0x00, 0xFF, 0x00, 0xFF },
    { 0x00, 0x00, 0xFF, 0xFF },
    { 0xFF, 0x00, 0xFF, 0xFF },
    { 0x00, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0x00, 0xFF },
    { 0x66, 0x00, 0x66, 0xFF },
    { 0x00, 0x99, 0x00, 0xFF },
    { 0x00, 0x00, 0x99, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
  };
  //I3S_ASSERT( lod < n_colors );
  return colors[lod % n_colors];
}

Vec4f to_color4f(const Rgba8& v)
{
  return Vec4f((float)v.r / 255.0f, (float)v.g / 255.0f, (float)v.b / 255.0f, (float)v.a / 255.0f);
}

static uint8_t _clamp8(float v) noexcept { return (uint8_t)std::max(0, std::min(255, (int)(v * 255.0f))); }

Rgba8 from_color4f(const Vec4f& v)
{

  return Rgba8(_clamp8(v.x), _clamp8(v.y), _clamp8(v.z), _clamp8(v.w));
}


Vec4f lod_to_color4f(int lod) { return to_color4f(lod_to_color(lod)); }


void rasterize_colormap(const std::vector< utl::Color_stop >& stops, std::vector< utl::Rgba8 >& colors, int count)
{
  I3S_ASSERT(stops.front().position == 0.0f);
  I3S_ASSERT(stops.back().position == 1.0f);

  //Texture_item tex;
  colors.resize(count);

  auto i1 = stops.begin();
  auto i2 = i1 + 1;
  for (int i = 0; i < colors.size(); ++i)
  {
    float alpha = (float)i / (float)(colors.size() - 1);
    while (alpha > i2->position)
    {
      i1 = i2;
      ++i2;
      I3S_ASSERT(i1 != stops.end());
    }
    I3S_ASSERT(i1->position <= alpha && alpha <= i2->position);
    float beta = (alpha - i1->position) / (i2->position - i1->position);
    colors[i] = utl::mix(i1->color, i2->color, beta);
  }
}

std::vector< utl::Color_stop > get_spectrum_colormap()
{
  return
  {
      { { 255,  0,   0, 255 }, 0.0f },
      { { 255, 255,  0, 255 }, 0.2f },
      { { 0,  255,  0, 255 }, 0.4f },
      { { 0,  255, 255, 255 }, 0.6f },
      { { 0,  0,  255, 255 }, 0.8f },
      { { 255,  0,  255, 255 }, 1.0f }
  };
}

}

} // namespace i3slib
