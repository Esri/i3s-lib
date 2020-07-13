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
#include "utils/utl_i3s_assert.h"
#include <cmath> //sqrt
#include <algorithm> //just for min/max...
#include <tuple>

#pragma warning(push)
#pragma warning(disable:4251)
#ifdef foreach
#undef foreach
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace i3slib
{

namespace utl
{

//! gcc has issue with abs()... 
template< class T > inline T my_abs(T v) noexcept { return v < T(0) ? -v : v; }

template< class X, class Y > X lerp( const X& a, const X& b, Y u ) { return ( Y(1)-u )*a + u*b ; } 

template< class T > struct Vec3;

// ---------------------------------------------------------
//                 Vec2<>
// ---------------------------------------------------------

template< class T >
struct Vec2
{
  //SERIALIZABLE( Vec2<T> );
  Vec2():x((T)0), y((T)0){}
  Vec2(T _x, T _y) : x(_x), y(_y){}
  explicit Vec2(T _v) : x(_v), y(_v){}
  template< class Y > Vec2(const Vec2<Y>& src) : x((T)src.x), y((T)src.y) {}
  template< class Y > Vec2( const Vec3<Y>& src ) : x( (T)src.x ), y( (T)src.y ) {}

  T x, y;
  friend Vec2<T>   operator*( T a, const Vec2<T>& b )                  { return Vec2<T>( a * b.x, a* b.y ); }
  friend Vec2<T>   operator*( const Vec2<T>& b, T a)                   { return Vec2<T>( a * b.x, a* b.y ); }
  friend Vec2<T>   operator*( const Vec2<T>& a, const Vec2<T>& b )     { return Vec2<T>( a.x * b.x, a.y* b.y ); }
  friend Vec2<T>   operator/(const Vec2<T>& a, T b)                    { return Vec2<T>(a.x / b, a.y / b); }
  friend Vec2<T>   operator/(T a, const Vec2<T>& b )                   { return Vec2<T>(a / b.x, a / b.y); }
  friend Vec2<T>   operator/(const Vec2<T>& a, const Vec2<T>& b)       { return Vec2<T>(a.x / b.x, a.y / b.y); }
  friend Vec2<T>   operator-(const Vec2<T>& a, const Vec2<T>& b)       { return Vec2<T>(a.x - b.x, a.y - b.y); }
  friend Vec2<T>   operator-(const Vec2<T>& a, T b)                    { return Vec2<T>(a.x - b, a.y - b); }
  friend Vec2<T>   operator-(T a, const Vec2<T>& b)                    { return Vec2<T>(a -b.x, a - b.y); }
  friend Vec2<T>   operator+(T a, const Vec2<T>& b)                    { return Vec2<T>(a + b.x, a + b.y); }
  friend Vec2<T>   operator+(const Vec2<T>& b, T a )                   { return Vec2<T>(a + b.x, a + b.y); }
  friend Vec2<T>   operator+(const Vec2<T>& a, const Vec2<T>& b)       { return Vec2<T>(a.x + b.x, a.y + b.y); }
  Vec2<T>&         operator+=(const Vec2<T>& b) noexcept               { x += b.x; y += b.y; return *this; }
  Vec2<T>&         operator-=(const Vec2<T>& b) noexcept               { x -= b.x; y -= b.y; return *this; }
  Vec2<T>&         operator/=(const Vec2<T>& b) noexcept               { x /= b.x; y /= b.y; return *this; }
  Vec2<T>&         operator/=( T b) noexcept                           { x /= b;   y /= b; return *this; }
  Vec2<T>&         operator*=(const Vec2<T>& b) noexcept               { x *= b.x; y *= b.y; return *this; }
  Vec2<T>&         operator*=( T b) noexcept                           { x *= b;   y *= b; return *this; }

  Vec2 <T>         operator-() const                    { return Vec2<T>( -x, -y ); }
  T                length() const                       { return (T)sqrt( x*x + y*y ); }
  T                length_sqr() const noexcept { return  x * x + y * y; }

  //template< class Ar > void serialize( Ar& ar )         { ar & utl::nvp( "x",x ) & utl::nvp( "y", y); }


  T                dot(const Vec2<T>& v) const noexcept { return v.x * x + v.y * y; }
  constexpr static int              size()  noexcept                 { return 2; }
  T&               operator[](size_t i) noexcept                     { I3S_ASSERT(i < 2); return reinterpret_cast<T*>(this)[i]; }
  const T&         operator[](size_t i) const noexcept               { I3S_ASSERT(i < 2); return reinterpret_cast<const T*>(this)[i]; }

  static T         cross(const Vec2<T>& a, const Vec2<T>& b)  noexcept { return a.x*b.y - a.y*b.x; }
  static T         l1_distance(const Vec2<T>& v, const Vec2<T>& w)noexcept { return my_abs(v.x - w.x) + my_abs(v.y - w.y) ; }
  static T         l1_distance(const Vec2<T>& v)noexcept { return my_abs(v.x) + my_abs(v.y); }

  friend bool operator==(const Vec2<T>& a, const Vec2<T>& b)         { return a.x == b.x && a.y == b.y; }
  friend bool operator!=(const Vec2<T>& a, const Vec2<T>& b)         { return !(a == b); }
  friend bool operator<(const Vec2<T>& a, const Vec2<T>& b)          { return a.x < b.x || (a.x == b.x && (a.y < b.y)); }

  friend Vec2<T>   min(const Vec2<T>& a, const Vec2<T>& b) noexcept  { return Vec2<T>(std::min(a.x, b.x), std::min(a.y, b.y)); }
  friend Vec2<T>   max(const Vec2<T>& a, const Vec2<T>& b) noexcept  { return Vec2<T>(std::max(a.x, b.x), std::max(a.y, b.y)); }

  Vec3<T>          insert(size_t i, T v) const noexcept { I3S_ASSERT(i < 3); return Vec3<T>(i==0?v:x, i==1?v:(i==0?x:y), i==2?v:y); }  // insert a coordinate
  Vec2<T>          ortho() const  { return Vec2<T>(-y, x); }   // orthogonal vector
  T* begin() noexcept { return &x; }
  const T* begin() const noexcept { return &x; }
  const T* cbegin() const noexcept { return &x; }
  T* end() noexcept { return &y + 1; }
  const T* end() const noexcept { return &y + 1; }
  const T* cend() const noexcept { return &y + 1; }

};
typedef Vec2<int> Vec2i;
typedef Vec2<unsigned int> Vec2u;
typedef Vec2<double> Vec2d;
typedef Vec2<float> Vec2f;

// ---------------------------------------------------------
//                 Vec3 
// ---------------------------------------------------------

template< class T > struct Vec4;

template< class T >
struct Vec3
{
  T x,y,z;  
  typedef T value_type;
  //SERIALIZABLE( Vec3<T> );
  constexpr Vec3() : x((T)0), y((T)0), z((T)0) {}
  constexpr Vec3( T _x, T _y, T _z ) : x( _x ), y( _y ), z( _z ){}
  explicit Vec3(T v) noexcept : x(v), y(v), z(v) {}
  explicit Vec3(const Vec4<T>& v) noexcept;
  template<class Y> explicit Vec3(const Vec4<Y>& v) noexcept;
  template< class Y > explicit Vec3( const Vec3<Y>& src ) noexcept: x( (T)src.x ), y( (T)src.y ), z( (T)src.z ) {}
  template< class Y > explicit Vec3(const Vec2<Y>& src, T _z = T(0))noexcept : x((T)src.x), y((T)src.y), z(_z) {}
  T   distance( const Vec3<T>& p ) const noexcept                           { return (p-*this).length(); }
  T   distance_sqr( const Vec3<T>& p ) const noexcept                        { return (p-*this).length_sqr(); }
  T   length() const    noexcept                                            { return std::sqrt( x*x + y*y+z*z ); }
  Vec3<T> normalized() const  noexcept                                      { T len= length(); if( len==0 ) return Vec3<T>(0.0); len = T(1.0) / len; return Vec3<T>(x*len, y*len, z*len); }
  T   length_sqr() const noexcept                                            { return  x*x + y*y+z*z; }
  T   dot( const Vec3<T>& v ) const noexcept                                { return v.x * x + v.y * y + v.z * z;}
  static T   dot( const Vec3<T>& v, const Vec3<T>& w )noexcept              { return v.x * w.x + v.y * w.y + v.z * w.z;}
  static T   l1_distance(const Vec3<T>& v, const Vec3<T>& w)noexcept         { return my_abs(v.x - w.x) + my_abs(v.y - w.y) + my_abs(v.z - w.z); }
  T   min_distance_from_segment( const Vec3<T>& a, const Vec3<T>& b ) const;

  //template< class Ar > void serialize( Ar& ar )                             { ar & utl::nvp("x",x) & utl::nvp("y", y ) & utl::nvp("z", z); }
  friend constexpr Vec3<T>   operator-(const Vec3<T>& a, T b) noexcept                { return Vec3<T>(a.x - b, a.y - b, a.z - b); }
  friend constexpr Vec3<T>   operator-(const Vec3<T>& a, const Vec3<T>& b)noexcept    { return Vec3<T>(a.x - b.x, a.y - b.y, a.z - b.z); }
  friend constexpr Vec3<T>   operator-(T a, const Vec3<T>& b)noexcept                 { return Vec3<T>(a - b.x, a - b.y, a - b.z); }
  friend constexpr Vec3<T>   operator+(const Vec3<T>& a, const Vec3<T>& b) noexcept { return Vec3<T>(a.x + b.x, a.y + b.y, a.z + b.z); }
  friend constexpr Vec3<T>   operator*(const Vec3<T>& a, const Vec3<T>& b)noexcept { return Vec3<T>(a.x * b.x, a.y * b.y, a.z * b.z); }
  friend constexpr Vec3<T>   operator*(T a, const Vec3<T>& b) noexcept                { return Vec3<T>(a * b.x, a* b.y, a * b.z); }
  friend constexpr Vec3<T>   operator*(const Vec3<T>& b, T a) noexcept { return Vec3<T>(a * b.x, a* b.y, a * b.z); }
  friend constexpr Vec3<T>   operator/(const Vec3<T>& a, const Vec3<T>& b) noexcept { return Vec3<T>(a.x / b.x, a.y / b.y, a.z / b.z); }
  friend constexpr Vec3<T>   operator/(T a, const Vec3<T>& b) noexcept                { return Vec3<T>(a / b.x, a / b.y, a / b.z); }
  friend constexpr Vec3<T>   operator/(const Vec3<T>& a, T b) noexcept { return Vec3<T>(a.x / b, a.y / b, a.z / b); }
  friend constexpr Vec3<T>   operator+(T a, const Vec3<T>& b)  noexcept { return Vec3<T>(a + b.x, a + b.y, a + b.z); }
  friend constexpr Vec3<T>   operator+( const Vec3<T>& b, T a ) noexcept              { return a + b; }
  Vec3<T>          operator&(T b) const noexcept; // must be specialized for integer types
  Vec3<T>&         operator&=(T b) noexcept; // must be specialized for integer types
  Vec3<T>          operator>>(T b) const noexcept; // must be specialized for integer types
  Vec3<T>&         operator>>=(T b) noexcept; // must be specialized for integer types
  Vec3<T>          operator<<(T b) const noexcept; // must be specialized for integer types
  Vec3<T>&         operator<<=(T b) noexcept; // must be specialized for integer types
  Vec3<T>&         operator+=(const Vec3<T>& b) noexcept                    { x += b.x; y += b.y; z += b.z; return *this; }
  Vec3<T>&         operator+=( T b) noexcept                                { x += b; y += b; z += b; return *this; }
  Vec3<T>&         operator-=( const Vec3<T>& b ) noexcept                  { x -= b.x; y -= b.y; z -= b.z; return *this; }
  Vec3<T>&         operator/=(T b) noexcept                                 { x /= b; y /= b; z /= b; return *this; }
  Vec3<T>&         operator/=(const Vec3<T>& b) noexcept                    { x /= b.x; y /= b.y; z /= b.z; return *this; }
  Vec3<T>&         operator*=(const Vec3<T>& b) noexcept                    { x *= b.x; y *= b.y; z *= b.z; return *this; }
  Vec3<T>&         operator*=( T b ) noexcept                               { x *= b; y *= b; z *= b; return *this; }
  Vec3<T>          operator-() const noexcept                               { return Vec3<T>( -x, -y, -z); }
  T                max_any() const noexcept                                     { return std::max( x, std::max( y, z ) );} // warning: renamed to avoid name conflict with friend max()
  T                min_any() const noexcept                                     { return std::min( x, std::min( y, z ) );}// warning: renamed to avoid name conflict with friend min()
  T                sum() const noexcept                                     { return x+y+z;}
  template<class Y > Y product() const noexcept                             { return (Y)x*(Y)y*(Y)z; }
  static Vec3<T>   cross( const Vec3<T>& a, const Vec3<T>& b) noexcept      { return Vec3<T>( a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x ); }
  friend bool operator==( const Vec3<T>& a, const Vec3<T>& b ) noexcept     { return a.x ==b.x && a.y == b.y && a.z == b.z; }
  friend bool operator!=( const Vec3<T>& a, const Vec3<T>& b ) noexcept     { return !(a==b); }
  friend Vec3<T>   min(const Vec3<T>& a, const Vec3<T>& b) noexcept         { return Vec3<T>(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
  friend Vec3<T>   max(const Vec3<T>& a, const Vec3<T>& b) noexcept         { return Vec3<T>(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }
  friend bool      is_equal(const Vec3<T>& a, const Vec3<T>& b, T epsi) noexcept       { return my_abs( a.x-b.x) < epsi && my_abs(a.y - b.y) < epsi && my_abs(a.z - b.z) < epsi; }
  T&               operator[](size_t i) noexcept                            { I3S_ASSERT(i < 3); return reinterpret_cast<T*>(this)[i]; }
  const T&         operator[](size_t i) const noexcept                      { I3S_ASSERT(i < 3); return reinterpret_cast<const T*>(this)[i]; }
  void             flip()noexcept                                           { std::swap(x, z);}
  Vec3<T>          flipped() const noexcept                                 { return Vec3<T>(z,y,x); }

  Vec2<T>          erase(size_t i) const noexcept                            { I3S_ASSERT(i < 3); return Vec2<T>( i==0? y:x, i==2? y:z); }  // erase a coordinate, project to a plane
  //experimental:
  Vec2<T>&         xy()  noexcept                                            { return *reinterpret_cast< Vec2<T>*>(this); }
  const Vec2<T>&   xy() const  noexcept                                      { return *reinterpret_cast< const Vec2<T>*>(this); }

  // total order comparaison:
  friend bool      operator<(const Vec3<T>& a, const Vec3<T>& b) {
    return a.x < b.x || (a.x == b.x && (a.y < b.y || (a.y == b.y && a.z < b.z)));
  }
  bool             all_less(const Vec3<T>& a) const    { return x < a.x && y < a.y && z < a.z; }
  bool             all_greater(const Vec3<T>& a) const { return x > a.x&& y > a.y&& z > a.z; }
  bool             all_gr_eq(const Vec3<T>& a) const   { return x >= a.x&& y >= a.y&& z >= a.z; }
  static constexpr int              size()  noexcept                        { return 3; }

  T* begin() noexcept { return &x; }
  const T* begin() const noexcept { return &x; }
  const T* cbegin() const noexcept { return &x; }
  T* end() noexcept { return &z + 1; }
  const T* end() const noexcept { return &z + 1; }
  const T* cend() const noexcept { return &z + 1; }
};
typedef Vec3< double > Vec3d;
typedef Vec3< float > Vec3f;
typedef Vec3< int > Vec3i;
typedef Vec3< unsigned int > Vec3u;
typedef Vec3< unsigned char > Rgb8;
typedef Vec3< unsigned short > Rgb16;
static_assert( sizeof( Rgb8 )  == 3, "Unexpected size" );
static_assert( sizeof( Rgb16 ) == 6, "Unexpected size" );

template< class T >
inline T  Vec3<T>::min_distance_from_segment(const Vec3<T>& a, const Vec3<T>& b) const
{
  auto AB = a - b;
  const T l2 = AB.length_sqr();
  if (l2 == 0.0)
    return distance(a);
  // Consider the line extending the segment, parameterized as a + t (b - a).
  // We find projection of point p onto the line. 
  // It falls where t = [(p-a) . (b-a)] / |b-a|^2
  const T t = dot(*this - a, AB) / l2;
  if (t < 0.0)
    return distance(a);       // Beyond the 'a' end of the segment
  else if (t > 1.0)
    return distance(b);  // Beyond the 'b' end of the segment
  return distance(a + t * AB);  // Projection falls on the segment
}

template< class T >
inline Vec3<T>::Vec3(const Vec4<T>& v) noexcept : x(v.x), y(v.y), z(v.z) {}
template<class T> template< class Y > Vec3<T>::Vec3(const Vec4<Y>& v) noexcept: x((T)v.x), y((T)v.y), z((T)v.z) {}

// --- bit shift specialization for (some) interger types;
//template<> inline  Vec3<int>      Vec3<int>::operator>>(int b) noexcept { return Vec3<int>( x >> b, y >> b, z >> b ); }
template<> inline  Vec3<uint32_t> Vec3<uint32_t>::operator>>(uint32_t b) const noexcept { return Vec3<uint32_t>(x >> b, y >> b, z >> b); }
//template<> inline  Vec3<int64_t>  Vec3<int64_t>::operator>>(int64_t b) noexcept { return Vec3<int64_t>(x >> b, y >> b, z >> b); }
//template<> inline  Vec3<uint64_t> Vec3<uint64_t>::operator>>(uint64_t b) noexcept { return Vec3<uint64_t>(x >> b, y >> b, z >> b); }

//template<> inline  Vec3<int>      Vec3<int>::operator<<(int b) noexcept { return Vec3<int>(x << b, y << b, z << b); }
template<> inline  Vec3<uint32_t> Vec3<uint32_t>::operator<<(uint32_t b) const noexcept { return Vec3<uint32_t>(x << b, y << b, z << b); }
//template<> inline  Vec3<int64_t>  Vec3<int64_t>::operator<<(int64_t b) noexcept { return Vec3<int64_t>(x << b, y << b, z << b); }
//template<> inline  Vec3<uint64_t> Vec3<uint64_t>::operator<<(uint64_t b) noexcept { return Vec3<uint64_t>(x << b, y << b, z << b); }

template<> inline  Vec3<uint32_t>& Vec3<uint32_t>::operator>>=(uint32_t b) noexcept { x >>= b; y >>= b; z >>= b; return *this; }
template<> inline  Vec3<uint32_t>& Vec3<uint32_t>::operator<<=(uint32_t b) noexcept { x <<= b; y <<= b; z <<= b; return *this; }

template<> inline  Vec3<uint32_t>  Vec3<uint32_t>::operator&(uint32_t b) const noexcept  { return Vec3<uint32_t>(x & b, y & b, z & b); }
template<> inline  Vec3<uint32_t>& Vec3<uint32_t>::operator&=(uint32_t b) noexcept { x &= b; y &= b; z &= b; return *this; }


// ---------------------------------------------------------
//                 Vec4
// ---------------------------------------------------------
template< class T >
struct Vec4
{
  //SERIALIZABLE( Vec4<T> );
  typedef T value_type;
  Vec4(): x((T)0), y((T)0), z((T)0), w((T)0) {}
  explicit Vec4(T v) : x(v), y(v), z(v), w(v){}
  Vec4( T _x, T _y, T _z, T _w ) : x( _x ), y( _y ), z( _z ), w( _w ){}
  Vec4( const Vec3<T>& xyz, T _w ) : x( xyz.x ), y( xyz.y ), z( xyz.z ), w( _w ){}
  template< class Y > explicit Vec4(const Vec4<Y>& src) noexcept : x((T)src.x), y((T)src.y), z((T)src.z), w((T)src.w) {}

  T   length() const      { return sqrt( x*x + y*y+ z*z + w*w ); }

  T x,y,z,w;  
  //template< class Ar > void serialize( Ar& ar )                           { ar & utl::nvp("x", x) & utl::nvp("y", y) & utl::nvp("z", z) & utl::nvp("w", w); }
  friend Vec4<T>   operator-( const Vec4<T>& a, const Vec4<T>& b )noexcept{ return Vec4<T>( a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w );}
  friend Vec4<T>   operator-(const Vec4<T>& a, T b) noexcept              { return Vec4<T>(a.x - b, a.y - b, a.z - b, a.w-b); }
  friend Vec4<T>   operator-(T a, const Vec4<T>& b)noexcept               { return Vec4<T>(a - b.x, a - b.y, a - b.z, a - b.w); }

  friend Vec4<T>   operator+( const Vec4<T>& a, const Vec4<T>& b )noexcept{ return Vec4<T>( a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w );}
  Vec4<T>& operator+=(const Vec4<T>& b) noexcept { x += b.x; y += b.y; z += b.z; w += b.w; return *this; }
  friend Vec4<T>   operator*(T a, const Vec4<T>& b)noexcept               { return Vec4<T>(a * b.x, a* b.y, a * b.z, a * b.w); }
  friend Vec4<T>   operator*(const Vec4<T>& b, T a)noexcept { return Vec4<T>(a * b.x, a * b.y, a * b.z, a * b.w); }
  friend Vec4<T>   operator*(const Vec4<T>& a, const Vec4<T>& b)noexcept  { return Vec4<T>(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
  Vec4<T>&         operator*=(const Vec4<T>& b) noexcept                  { x *= b.x; y *= b.y; z *= b.z; w *= b.w; return *this; }
  Vec4<T>&         operator*=(T b) noexcept                               { x *= b; y *= b; z *= b; w *= b; return *this; }
  friend constexpr Vec4<T>   operator/(const Vec4<T>& a, const Vec4<T>& b) noexcept { return Vec4<T>(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
  friend constexpr Vec4<T>   operator/(T a, const Vec4<T>& b) noexcept { return Vec4<T>(a / b.x, a / b.y, a / b.z, a / b.w); }
  friend constexpr Vec4<T>   operator/(const Vec4<T>& a, T b) noexcept { return Vec4<T>(a.x / b, a.y / b, a.z / b, a.w / b); }

  template< class Y > friend Vec4<T>   operator*(Y a, const Vec4<T>& b)noexcept { return Vec4<T>(a * b.x, a * b.y, a * b.z, a * b.w); }
  template< class Y > friend Vec4<T>   operator*(const Vec4<T>& b, Y a)noexcept { return Vec4<T>(a * b.x, a * b.y, a * b.z, a * b.w); }
  friend bool operator==(const Vec4<T>& a, const Vec4<T>& b)noexcept      { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
  friend bool operator!=(const Vec4<T>& a, const Vec4<T>& b)noexcept      { return !(a == b); }
  friend bool is_equal(const Vec4<T>& a, const Vec4<T>& b, T epsi)  noexcept   {  return my_abs(a.x - b.x) < epsi && my_abs(a.y - b.y) < epsi && my_abs(a.z - b.z) < epsi && my_abs(a.w - b.w) < epsi; }
  template< class Y > Y product() const noexcept                          { return (Y)x * (Y)y * (Y)z * (T)w; }
  T&               operator[](size_t i) noexcept                          { I3S_ASSERT(i < 4); return reinterpret_cast<T*>(this)[i]; }
  const T&         operator[](size_t i) const noexcept                    { I3S_ASSERT(i < 4); return reinterpret_cast<const T*>(this)[i]; }
  void             flip()noexcept                                         { std::swap(x, w); std::swap(y, z); }
  Vec4<T>          flipped() const noexcept                               { return { w, z, y, x }; }
  T                dot(const Vec4<T>& v) const noexcept                   { return v.x * x + v.y * y + v.z * z + v.w * w; }
  static constexpr int              size()  noexcept                      { return 4;}
  friend bool      operator<(const Vec4<T>& a, const Vec4<T>& b) noexcept
  {  //Define a strick weak ordering:
    return std::tie(a.x, a.y, a.z, a.w) < std::tie(b.x, b.y, b.z, b.w );
  }

  T* begin() noexcept { return &x; }
  const T* begin() const noexcept { return &x; }
  const T* cbegin() const noexcept { return &x; }
  T* end() noexcept { return &w + 1; }
  const T* end() const noexcept { return &w + 1; }
  const T* cend() const noexcept { return &w + 1; }
  //experimental:
  Vec2<T>&         xy()  noexcept                                         { return *reinterpret_cast< Vec2<T>*>(this); }
  Vec3<T>&         xyz()  noexcept                                        { return *reinterpret_cast< Vec3<T>*>(this); }
  const Vec2<T>&   xy() const noexcept                                    { return *reinterpret_cast< const Vec2<T>*>(this); }
  const Vec3<T>&   xyz() const noexcept                                   { return *reinterpret_cast< const Vec3<T>*>(this); }

};
typedef Vec4< double > Vec4d;
typedef Vec4< float > Vec4f;
typedef Vec4< int > Vec4i;
typedef Vec4< unsigned int > Vec4u;
//typedef Vec4< unsigned char > Rgba8; ->let's specialize this one
typedef Vec4< unsigned short > Rgba16;
//static_assert( sizeof( Rgba8 )  == 4, "Unexpected size" );
static_assert( sizeof( Rgba16 ) == 8, "Unexpected size" );

// ---------------------------------------------------------
//                 Mat3X3
// ---------------------------------------------------------

template< class T >
struct Mat3x3
{
#pragma warning( push)
#pragma warning( disable:4201)
  union
  {
    struct
    {
      T _11, _12, _13;
      T _21, _22, _23;
      T _31, _32, _33;
    };
    T m[3][3];
  };
#pragma warning( pop)

  Mat3x3() {}
  Mat3x3(T m00, T m01, T m02,
         T m10, T m11, T m12,
         T m20, T m21, T m22);
  explicit Mat3x3(T diag) { memset(this, 0, sizeof(Mat3x3<T>)); _11 = (T)1; _22 = (T)1; _33 = (T)1;}
  T           operator() (size_t Row, size_t Column) const { return m[Row][Column]; }
  T&          operator() (size_t Row, size_t Column) { return m[Row][Column]; }

  Mat3x3<T>&        inverse();
  Mat3x3<T>         transposed() const;
  T                 det() const;
  Vec3<T>           mult(const Vec3<T>& v) const;
  static Mat3x3<T>  mult(const Mat3x3<T>& a, const Mat3x3<T>& b) ;
};

// constructor:
template< class T >
inline Mat3x3<T>::Mat3x3
(
  T m00, T m01, T m02, 
  T m10, T m11, T m12, 
  T m20, T m21, T m22
)
{
  m[0][0] = m00;
  m[0][1] = m01;
  m[0][2] = m02;

  m[1][0] = m10;
  m[1][1] = m11;
  m[1][2] = m12;

  m[2][0] = m20;
  m[2][1] = m21;
  m[2][2] = m22;
}

template< class T >
inline T Mat3x3<T>::det() const
{
  const Mat3x3<T>& src = *this;
  T det = src._11 * (src._22 * src._33 - src._23 * src._32)
        - src._12 * (src._21 * src._33 - src._23 * src._31)
        + src._13 * (src._21 * src._32 - src._22 * src._31);
  return det;
}

template< class T >
inline Mat3x3<T>& Mat3x3<T>::inverse()
{
  const Mat3x3<T>& src = *this;
  Mat3x3<T>  dst;
  T det = src._11 * (src._22 * src._33 - src._23 * src._32) - src._12 * (src._21 * src._33 - src._23 * src._31) + src._13 * (src._21 * src._32 - src._22 * src._31);
  if (det)
  {
    T det_1 = T(1) / det;
    dst._11 = (src._22 * src._33 - src._23 * src._32) * det_1;
    dst._21 = -(src._21 * src._33 - src._23 * src._31) * det_1;
    dst._31 = (src._21 * src._32 - src._22 * src._31) * det_1;

    dst._12 = -(src._12 * src._33 - src._32 * src._13) * det_1;
    dst._22 = (src._11 * src._33 - src._13 * src._31) * det_1;
    dst._32 = -(src._11 * src._32 - src._12 * src._31) * det_1;

    dst._13 = (src._12 * src._23 - src._13 * src._22) * det_1;
    dst._23 = -(src._11 * src._23 - src._13 * src._21) * det_1;
    dst._33 = (src._11 * src._22 - src._12 * src._21) * det_1;
  }
  else
  {
    dst = Mat3x3<T>(1);
  }
  *this = dst;
  return *this;
}

template< class T >
Vec3<T> Mat3x3<T>::mult(const Vec3<T>& v) const
{
  return Vec3<T>(
    _11 * v.x + _12 * v.y + _13 * v.z,
    _21 * v.x + _22 * v.y + _23 * v.z,
    _31 * v.x + _32 * v.y + _33 * v.z
    );
}
template< class T >
Mat3x3<T>  Mat3x3<T>::transposed() const
{
  return Mat3x3<T>(
    _11, _21, _31,
    _12, _22, _32,
    _13, _23, _33
    );
}

template< class T >
Mat3x3<T> Mat3x3<T>::mult(const Mat3x3<T>& m1, const Mat3x3<T>& m2)
{
  // Cache the invariants in registers
  T x = m1.m[0][0];
  T y = m1.m[0][1];
  T z = m1.m[0][2];
  Mat3x3<T> result;
  // Perform the operation on the first row
  result.m[0][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z);
  result.m[0][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z);
  result.m[0][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z);
  // Repeat for all the other rows
  x = m1.m[1][0];
  y = m1.m[1][1];
  z = m1.m[1][2];
  result.m[1][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z);
  result.m[1][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z);
  result.m[1][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z);
  x = m1.m[2][0];
  y = m1.m[2][1];
  z = m1.m[2][2];
  result.m[2][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z);
  result.m[2][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z);
  result.m[2][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z);
  return result;
}

// ---------------------------------------------------------
//                 Mat4x4 
// ---------------------------------------------------------

// a "plain" 4x4 matrix used for interop when a known binary layout is required.
// Naming follows DirectXMath
// *NOT* SSE 16 byte aligned.
// NOTE: matrix operations are in utl_matrix.h
template< class T >
struct Mat4x4
{
#pragma warning( push)
#pragma warning( disable:4201)
  union
  {
    struct
    {
      T _11, _12, _13, _14;
      T _21, _22, _23, _24;
      T _31, _32, _33, _34;
      T _41, _42, _43, _44;
    };
    T m[4][4];
  };
#pragma warning( pop)

  Mat4x4() {}
  Mat4x4(T m00, T m01, T m02, T m03,
         T m10, T m11, T m12, T m13,
         T m20, T m21, T m22, T m23,
         T m30, T m31, T m32, T m33);
  Mat4x4(const Vec3<T>& vx,
         const Vec3<T>& vy,
         const Vec3<T>& vz
         );
  explicit Mat4x4(T diag);
  template< class Y > explicit Mat4x4(const Mat4x4<Y>& src); //type conversion...
  T           operator() (size_t Row, size_t Column) const { return m[Row][Column]; }
  T&          operator() (size_t Row, size_t Column) { return m[Row][Column]; }
  Mat4x4<T>&  translate(const Vec3<T>& v) { _41 += v.x; _42 += v.y; _43 += v.z; return *this; }
  Mat3x3<T>   to_mat3() const;
  friend  Mat4x4<T>   operator*(const Mat4x4<T>& a, const Mat4x4<T>& b) {  return mult(a, b);}
  Vec3<T>     transform_point(const Vec3<T>& p) const;
  Vec3<T>     transform_normal(const Vec3<T>& p) const;
  Mat4x4<T>   transposed() const;
  Mat4x4<T>&  transpose() { *this = std::move(transposed()); return *this; }
  Mat4x4<T>&  inverse();
  T           det() const;
  Mat4x4<T>&  scale(const Vec3f& scale) { _11*=scale.x; _22 *= scale.y; _33 *=scale.z; return *this; } 

  Mat4x4<T>&  operator+=(const Mat4x4<T>& m);
  friend Mat4x4<T>   operator*(T v, const Mat4x4<T>& m) { return mult(v, m); } 

  static Mat4x4<T>  mult(const Mat4x4<T>& m1, const Mat4x4<T>& m2);
  static Mat4x4<T>  mult(const Vec4<T>& a, const Vec4<T>& b);
  static Mat4x4<T>  mult( T a, const Mat4x4<T>& b);
};


typedef Mat4x4< float >  FLoat4x4;
typedef Mat4x4< double > Double4x4;
typedef Mat4x4< double > Mat4d;
typedef Mat4x4< float  > Mat4f;
typedef Mat3x3< double > Mat3d;
typedef Mat3x3< float  > Mat3f;


// constructor:
template< class T >
inline Mat4x4<T>::Mat4x4
(
  T m00, T m01, T m02, T m03,
  T m10, T m11, T m12, T m13,
  T m20, T m21, T m22, T m23,
  T m30, T m31, T m32, T m33
)
{
  m[0][0] = m00;
  m[0][1] = m01;
  m[0][2] = m02;
  m[0][3] = m03;

  m[1][0] = m10;
  m[1][1] = m11;
  m[1][2] = m12;
  m[1][3] = m13;

  m[2][0] = m20;
  m[2][1] = m21;
  m[2][2] = m22;
  m[2][3] = m23;

  m[3][0] = m30;
  m[3][1] = m31;
  m[3][2] = m32;
  m[3][3] = m33;
}

template< class T >
inline Mat4x4<T>::Mat4x4(const Vec3<T>& vx,
                         const Vec3<T>& vy,
                         const Vec3<T>& vz
                         )
{
  memcpy(&m[0][0], &vx.x, sizeof(vx));
  memcpy(&m[1][0], &vy.x, sizeof(vx));
  memcpy(&m[2][0], &vz.x, sizeof(vx));
  m[3][3] = (T)1;
  m[0][3] = (T)0;
  m[1][3] = (T)0;
  m[2][3] = (T)0;

  m[3][0] = (T)0;
  m[3][1] = (T)0;
  m[3][2] = (T)0;
}

// constructor: create diagonal matrix ( diag=1 -> identity matrix)
template< class T >
inline Mat4x4<T>::Mat4x4(T diag)
  : Mat4x4<T>(
    diag, T(0), T(0), T(0),
    T(0), diag, T(0), T(0),
    T(0), T(0), diag, T(0),
    T(0), T(0), T(0), diag
    )
{
}

template< class T >
template< class Y > inline Mat4x4<T>::Mat4x4(const Mat4x4<Y>& m)
  : Mat4x4<T>(
      (T)m._11, (T)m._12, (T)m._13, (T)m._14,
      (T)m._21, (T)m._22, (T)m._23, (T)m._24,
      (T)m._31, (T)m._32, (T)m._33, (T)m._34,
      (T)m._41, (T)m._42, (T)m._43, (T)m._44
      )
{
}

template< class T >
inline Mat4x4<T>  Mat4x4<T>::mult(const Mat4x4<T>& m1, const Mat4x4<T>& m2)
{
  // Cache the invariants in registers
  T x = m1.m[0][0];
  T y = m1.m[0][1];
  T z = m1.m[0][2];
  T w = m1.m[0][3];
  Mat4x4<T> result;
  // Perform the operation on the first row
  result.m[0][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z) + (m2.m[3][0] * w);
  result.m[0][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z) + (m2.m[3][1] * w);
  result.m[0][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z) + (m2.m[3][2] * w);
  result.m[0][3] = (m2.m[0][3] * x) + (m2.m[1][3] * y) + (m2.m[2][3] * z) + (m2.m[3][3] * w);
  // Repeat for all the other rows
  x = m1.m[1][0];
  y = m1.m[1][1];
  z = m1.m[1][2];
  w = m1.m[1][3];
  result.m[1][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z) + (m2.m[3][0] * w);
  result.m[1][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z) + (m2.m[3][1] * w);
  result.m[1][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z) + (m2.m[3][2] * w);
  result.m[1][3] = (m2.m[0][3] * x) + (m2.m[1][3] * y) + (m2.m[2][3] * z) + (m2.m[3][3] * w);
  x = m1.m[2][0];
  y = m1.m[2][1];
  z = m1.m[2][2];
  w = m1.m[2][3];
  result.m[2][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z) + (m2.m[3][0] * w);
  result.m[2][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z) + (m2.m[3][1] * w);
  result.m[2][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z) + (m2.m[3][2] * w);
  result.m[2][3] = (m2.m[0][3] * x) + (m2.m[1][3] * y) + (m2.m[2][3] * z) + (m2.m[3][3] * w);
  x = m1.m[3][0];
  y = m1.m[3][1];
  z = m1.m[3][2];
  w = m1.m[3][3];
  result.m[3][0] = (m2.m[0][0] * x) + (m2.m[1][0] * y) + (m2.m[2][0] * z) + (m2.m[3][0] * w);
  result.m[3][1] = (m2.m[0][1] * x) + (m2.m[1][1] * y) + (m2.m[2][1] * z) + (m2.m[3][1] * w);
  result.m[3][2] = (m2.m[0][2] * x) + (m2.m[1][2] * y) + (m2.m[2][2] * z) + (m2.m[3][2] * w);
  result.m[3][3] = (m2.m[0][3] * x) + (m2.m[1][3] * y) + (m2.m[2][3] * z) + (m2.m[3][3] * w);
  return result;
}

template< class T >
inline Mat4x4<T>   Mat4x4<T>::mult(const Vec4<T>& a, const Vec4<T>& b)
{
  return   Mat4x4<T>(
    a.x * b.x, a.x * b.y, a.x * b.z, a.x * b.w,
    a.y * b.x, a.y * b.y, a.y * b.z, a.y * b.w,
    a.z * b.x, a.z * b.y, a.z * b.z, a.z * b.w,
    a.w * b.x, a.w * b.y, a.w * b.z, a.w * b.w
    );
}


template< class T >
inline Vec3<T>  Mat4x4<T>::transform_point(const Vec3<T>& p) const
{
  const T x = p.x, y = p.y, z = p.z;
  const T inv_w = T(1.0) / ((m[0][3] * x) + (m[1][3] * y) + (m[2][3] * z) + (m[3][3]));
  return Vec3<T>(
    ((m[0][0] * x) + (m[1][0] * y) + (m[2][0] * z) + (m[3][0])) * inv_w, //first column
    ((m[0][1] * x) + (m[1][1] * y) + (m[2][1] * z) + (m[3][1])) * inv_w,
    ((m[0][2] * x) + (m[1][2] * y) + (m[2][2] * z) + (m[3][2])) * inv_w);
}


template< class T >
inline Vec3<T>  Mat4x4<T>::transform_normal( const Vec3<T>& p) const
{
  const auto& M = *this;
  const T x = p.x, y = p.y, z = p.z;
  return Vec3<T>(
    (M.m[0][0] * x) + (M.m[1][0] * y) + (M.m[2][0] * z),
    (M.m[0][1] * x) + (M.m[1][1] * y) + (M.m[2][1] * z),
    (M.m[0][2] * x) + (M.m[1][2] * y) + (M.m[2][2] * z)
  );
}

template< class T > 
inline Mat4x4<T>  Mat4x4<T>::transposed() const
{
  const auto& m = *this; 
  return Mat4x4<T>(
    m._11, m._21, m._31, m._41,
    m._12, m._22, m._32, m._42,
    m._13, m._23, m._33, m._43,
    m._14, m._24, m._34, m._44
    );
}


template< class T >
Mat4x4<T>& Mat4x4<T>::inverse()
{
  T m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
  T m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
  T m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
  T m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

  T v0 = m20 * m31 - m21 * m30;
  T v1 = m20 * m32 - m22 * m30;
  T v2 = m20 * m33 - m23 * m30;
  T v3 = m21 * m32 - m22 * m31;
  T v4 = m21 * m33 - m23 * m31;
  T v5 = m22 * m33 - m23 * m32;

  T t00 = +(v5 * m11 - v4 * m12 + v3 * m13);
  T t10 = -(v5 * m10 - v2 * m12 + v1 * m13);
  T t20 = +(v4 * m10 - v2 * m11 + v0 * m13);
  T t30 = -(v3 * m10 - v1 * m11 + v0 * m12);

  T inv_det = (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);
  if (inv_det)
  {
    inv_det = (T)1.0 / inv_det;

    m[0][0] = t00 * inv_det;
    m[1][0] = t10 * inv_det;
    m[2][0] = t20 * inv_det;
    m[3][0] = t30 * inv_det;

    m[0][1] = -(v5 * m01 - v4 * m02 + v3 * m03) * inv_det;
    m[1][1] = +(v5 * m00 - v2 * m02 + v1 * m03) * inv_det;
    m[2][1] = -(v4 * m00 - v2 * m01 + v0 * m03) * inv_det;
    m[3][1] = +(v3 * m00 - v1 * m01 + v0 * m02) * inv_det;

    v0 = m10 * m31 - m11 * m30;
    v1 = m10 * m32 - m12 * m30;
    v2 = m10 * m33 - m13 * m30;
    v3 = m11 * m32 - m12 * m31;
    v4 = m11 * m33 - m13 * m31;
    v5 = m12 * m33 - m13 * m32;

    m[0][2] = +(v5 * m01 - v4 * m02 + v3 * m03) * inv_det;
    m[1][2] = -(v5 * m00 - v2 * m02 + v1 * m03) * inv_det;
    m[2][2] = +(v4 * m00 - v2 * m01 + v0 * m03) * inv_det;
    m[3][2] = -(v3 * m00 - v1 * m01 + v0 * m02) * inv_det;

    v0 = m21 * m10 - m20 * m11;
    v1 = m22 * m10 - m20 * m12;
    v2 = m23 * m10 - m20 * m13;
    v3 = m22 * m11 - m21 * m12;
    v4 = m23 * m11 - m21 * m13;
    v5 = m23 * m12 - m22 * m13;

    m[0][3] = -(v5 * m01 - v4 * m02 + v3 * m03) * inv_det;
    m[1][3] = +(v5 * m00 - v2 * m02 + v1 * m03) * inv_det;
    m[2][3] = -(v4 * m00 - v2 * m01 + v0 * m03) * inv_det;
    m[3][3] = +(v3 * m00 - v1 * m01 + v0 * m02) * inv_det;
    return *this;
  }
  else
  {
    // not invertible.
    return *this;
  }
}

template< class T >
inline T Mat4x4<T>::det() const
{
  T det = _11 * (_22 * _33 - _23 * _32) - _12 * (_21 * _33 - _23 * _31) + _13 * (_21 * _32 - _22 * _31);
  return det;
}

template< class T >
inline Mat4x4<T>&  Mat4x4<T>::operator+=(const Mat4x4<T>& m)
{
  T* dst = &_11;
  const T* src = &m._11;
  for (size_t i = 0; i < 16; ++i)
    dst[i] += src[i];
  return *this;
}

template< class T >
Mat4x4<T> Mat4x4<T>::mult(T v, const Mat4x4<T>& m)
{
  Mat4x4<T> ret( m );
  T* dst = &ret._11;
  for (size_t i = 0; i < 16; ++i)
    dst[i] *= v;
  return ret;
}



template< class T >
inline Mat3x3<T> Mat4x4<T>::to_mat3() const
{
  return Mat3x3<T>(
    _11, _12, _13,
    _21, _22, _23,
    _31, _32, _33
    );
}



// ----------------------- util functions: -----------------------

template< class T >
inline T clamp( const T& v, const T& a, const T& b ) noexcept
{
  return std::min( b, std::max( a, v ) );
}

// usefull functions:
template< class T >
inline Vec2< T > clamp( const Vec2<T>& v, const Vec2<T>& a, const Vec2<T>& b ) noexcept
{
  return Vec2<T>( std::min( b.x, std::max( a.x, v.x ) ), std::min( b.y, std::max( a.y, v.y ) ) );
}

template< class T >
inline Vec3< T > clamp( const Vec3<T>& v, const Vec3<T>& a, const Vec3<T>& b )
{
  return Vec3<T>(  std::min( b.x, std::max( a.x, v.x ) ),  std::min( b.y, std::max( a.y, v.y ) ),  std::min( b.z, std::max( a.z, v.z ) ) );
}

//template< class T >
//inline Vec4< T > clamp(const Vec4<T>& v, const Vec4<T>& a, const Vec4<T>& b)
//{
//  return Vec4<T>(std::min(b.x, std::max(a.x, v.x)), std::min(b.y, std::max(a.y, v.y)), std::min(b.z, std::max(a.z, v.z), , std::min(b.w, std::max(a.w, v.w)));
//}

//radian conversion for float types only:
template< class T > inline T radians(T v) noexcept;
template<> inline double radians(double v) noexcept { return v *  0.01745329251994329576923690768489; }
template<> inline float  radians(float v)  noexcept { return v *  0.01745329251994329576923690768489f; }

template< class T > inline T degrees(T v) noexcept;
template<> inline double degrees(double v) noexcept { return v *  57.295779513082320876798154814105; }
template<> inline float  degrees(float v)  noexcept { return v *  57.295779513082320876798154814105f; }

namespace detail
{
// "Trait" function for sequence-like containers (Serializable framework):
// e.g. In json, vec3d -> "position" : [ 1.0, 2.0, 3.0 ]
template < class T>                    void clear_vec(Vec2<T>& /*vec*/) {  } //do nothing
template < class T>                    bool resize_vec(Vec2<T>& /*vec*/, size_t n) { return n <= 2; } //do nothing
template < class T>                    void clear_vec(Vec3<T>& /*vec*/) {  } //do nothing
template < class T>                    bool resize_vec(Vec3<T>& /*vec*/, size_t n) { return n <= 3; } //do nothing
template < class T>                    void clear_vec(Vec4<T>& /*vec*/) {  } //do nothing
template < class T>                    bool resize_vec(Vec4<T>& /*vec*/, size_t n) { return n <= 4; } //do nothing
}

//! template to clear vector and release memory:
template< class V > inline void clear_and_free_vector(V& v) { V tmp; v.swap(tmp); } //avoid C4239 

}//end of ::utl

} // namespace i3slib

#pragma warning(pop)
