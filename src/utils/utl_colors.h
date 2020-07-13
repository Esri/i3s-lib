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
#include "utils/utl_geom.h"
#include "utils/utl_serialize.h"
#include <stdint.h>
#include <vector>

namespace i3slib
{

namespace utl
{

//!Helper function to make sure 8 bit operation do not overflow (saturate instead)
inline uint8_t clamp_sum(uint8_t a, uint32_t b)  { uint32_t r = ((uint32_t)a + (uint32_t)b);   return (uint8_t)(r > 0xFF ? 0xFF : r); }
inline uint8_t clamp_mul(uint8_t a, float b) { uint32_t r = (uint32_t)((float)a * b); return (uint8_t)(r > 0xFF ? 0xFF : r); }


//generic clamp function
//template< class T > inline T clamp(const T& v, const T& a, const T& b)   { return std::min(std::max(v, a), b); }
//! Linear interpolation function:
template< class T > inline T mix(const T& a, const T& b, float v)       { I3S_ASSERT(v >= 0.0f && v <= 1.0f); return a * (1.0f - v) + b*v; }


//! a RGBA 8bit pixel:
struct Rgba8
{
  Rgba8() : r(0), g(0), b(0), a(255) {}
  Rgba8(uint32_t _r, uint32_t _g, uint32_t _b, uint32_t _a = 255) : r((uint8_t)_r), g((uint8_t)_g), b((uint8_t)_b), a((uint8_t)_a) {}
  explicit Rgba8(uint32_t v) { memcpy(this, &v, sizeof(uint32_t)); }
  explicit Rgba8(Rgb8 v, uint8_t alpha=255) { memcpy(this, &v, sizeof(uint32_t)); a = alpha; }
  Rgba8&    operator=(const Rgb8& src) noexcept { memcpy(this, &src, 3); a = 255; return *this; }
  template< class T > explicit Rgba8(const Vec4<T>& v) : r((uint8_t)clamp(v.x,(T)0, (T)255)), g((uint8_t)clamp(v.y, (T)0, (T)255)), b((uint8_t)clamp(v.z, (T)0, (T)255)), a((uint8_t)clamp(v.w, (T)0, (T)255)) {}
  Rgba8              operator+(const Rgba8& v) { return Rgba8(clamp_sum(r, v.r), clamp_sum(g, v.g), clamp_sum(b, v.b), clamp_sum(a, v.a)); }
  friend Rgba8       operator*(const Rgba8& c, float v)           { return Rgba8(clamp_mul(c.r, v), clamp_mul(c.g, v), clamp_mul(c.b, v), clamp_mul(c.a, v)); }
  constexpr int       size() const noexcept { return 4; }
  uint8_t              operator[](size_t i) const noexcept { I3S_ASSERT(i >= 0 && i < 4); return *(&r + i); }
  uint8_t&             operator[](size_t i)  noexcept      { I3S_ASSERT(i >= 0 && i < 4); return *(&r + i); }
  Rgba8              mixed(const Rgba8& w, float v) {
    return Rgba8(
      (uint8_t)clamp(lerp((float)r, (float)w.r, v), 0.0f, 255.0f),
      (uint8_t)clamp(lerp((float)g, (float)w.g, v), 0.0f, 255.0f),
      (uint8_t)clamp(lerp((float)b, (float)w.b, v), 0.0f, 255.0f),
      (uint8_t)clamp(lerp((float)a, (float)w.a, v), 0.0f, 255.0f));
  }
  operator uint32_t() const   { return *reinterpret_cast<const uint32_t*>(this); } //may regret this (unexpected implicit conversion ?)
  uint8_t r, g, b, a;
  friend bool operator==(const Rgba8& a, const Rgba8& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
  }
  SERIALIZABLE(Rgba8);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("r", r);
    ar& utl::nvp("g", g);
    ar& utl::nvp("b", b);
    ar& utl::nvp("a", a);
  }
};

I3S_EXPORT void  create_gradient(int size, Rgba8 c1, Rgba8 c2, std::vector< Rgba8 >* out);
I3S_EXPORT void rgba2bgra(uint32_t* rgba, int n);
I3S_EXPORT Vec4f to_color4f(const Rgba8& v);
I3S_EXPORT Rgba8 from_color4f(const Vec4f& v);
I3S_EXPORT Vec4f lod_to_color4f(int lod);



struct Alpha_stop
{
  Alpha_stop() = default;
  Alpha_stop(float a, float p) : alpha(a), position(p) {}
  float       alpha; // in [0,1]
  float       position; // in [0,1] if stretch mode, voxel value otherwise
  // --- 
  bool        operator<(const Alpha_stop& st) const { return position < st.position; }
  friend bool operator==(const Alpha_stop& a, const Alpha_stop& b) { return a.alpha == b.alpha && a.position == b.position; }
  SERIALIZABLE(Alpha_stop);
  template< class Ar >  void serialize(Ar& ar)
  {
    ar& utl::nvp("position", position);
    ar& utl::nvp("alpha", alpha);
  }

};


struct Color_stop
{
  Color_stop() = default;
  Color_stop(const utl::Rgba8& c, float p) : color(c), position(p) {}
  utl::Rgba8  color = { 0,0,0,255 };
  float       position = 0.0f; // in [0,1] if stretch mode, voxel value otherwise
  // --- 
  friend bool operator==(const Color_stop& a, const Color_stop& b) { return a.color == b.color && a.position == b.position; }
  bool operator<(const Color_stop& b) const { return position < b.position; }
  SERIALIZABLE(Color_stop);
  template< class Ar >  void serialize(Ar& ar)
  {
    ar& utl::nvp("position", position);
    ar& utl::nvp("color", utl::seq((utl::Vec4< uint8_t >&)color));
  }
};

I3S_EXPORT std::vector< utl::Color_stop > get_spectrum_colormap();

I3S_EXPORT void rasterize_colormap(const std::vector< utl::Color_stop >& stops, std::vector< utl::Rgba8 >& colors, int count);

}//endof ::utl

} // namespace i3slib
