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
#include "utils/utl_geom.h"
#include "utils/utl_i3s_export.h"
#include <cmath>

namespace i3slib
{
namespace utl
{

template<typename T>
constexpr Vec4<T> identity_quaternion()
{
  constexpr auto c_0 = static_cast<T>(0);
  return { c_0, c_0, c_0, static_cast<T>(1) } ;
}

template<typename T>
Vec4<T> get_quaternion_from_axis_and_angle(const Vec3<T>& axis, T angle_in_radians)
{
  const auto angle = angle_in_radians * T(0.5);
  return Vec4<T>(axis.normalized() * std::sin(angle), std::cos(angle));
}

template<typename T>
Vec4<T> quaternion_conjugate(const Vec4<T>& quaternion)
{
  return { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };
}

template <typename T>
Vec4<T> quaternion_product(const Vec4<T>& q1, const Vec4<T>& q2)
{
  /*
  // I wonder whether this implementation is faster or more accurate.
  return
  {
    q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
    q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
    q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w,
    q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
  };*/

  const Vec3<T> v1(q1);
  const Vec3<T> v2(q2);
  return Vec4<T>(Vec3<T>::cross(v1, v2) + v2 * q1.w + v1 * q2.w, q1.w * q2.w - v1.dot(v2));
}

template <typename T>
Vec3<T> rotate_vec3_by_quaternion(const Vec3<T>& v, const Vec4<T>& quaternion)
{
  // p' = q x p x q-1
  //
  // p'  = rotated vector as pure quaternion
  // q   = unit quaternion
  // q-1 = quaternion conjugate (-x,-y,-z, w)
  // x   = quaternion multiplication

  // The math can be simplified as follows:
  const Vec3<T> u(quaternion); // quaternion vector part
  const auto s = quaternion.w;
  return (T(2.0) * u.dot(v)) * u + (s * s - u.dot(u)) * v + (T(2.0) * s) * Vec3<T>::cross(u, v);
}

template <typename T>
Vec3<T> rotate_vec3_by_quaternion_inverse(const Vec3<T>& v, const Vec4<T>& quaternion)
{
  const Vec3<T> u(quaternion);
  const auto s = quaternion.w;
  constexpr auto c_2 = static_cast<T>(2);
  return (c_2 * u.dot(v)) * u + (s * s - u.dot(u)) * v - (c_2 * s) * Vec3<T>::cross(u, v);
}

namespace detail
{

template<typename T>
void quaternion_axes_(const Vec4<T>& quaternion, T* x_axis, T* y_axis, T* z_axis)
{
  const auto x2 = quaternion.x + quaternion.x;
  const auto y2 = quaternion.y + quaternion.y;
  const auto z2 = quaternion.z + quaternion.z;

  const auto xx = x2 * quaternion.x;
  const auto xy = x2 * quaternion.y;
  const auto xz = x2 * quaternion.z;
  const auto xw = x2 * quaternion.w;
  const auto yy = y2 * quaternion.y;
  const auto yz = y2 * quaternion.z;
  const auto yw = y2 * quaternion.w;
  const auto zz = z2 * quaternion.z;
  const auto zw = z2 * quaternion.w;

  constexpr auto c_1 = static_cast<T>(1);
  x_axis[0] = c_1 - (yy + zz);
  x_axis[1] = xy + zw;
  x_axis[2] = xz - yw;
  y_axis[0] = xy - zw;
  y_axis[1] = c_1 - (xx + zz);
  y_axis[2] = yz + xw;
  z_axis[0] = xz + yw;
  z_axis[1] = yz - xw; 
  z_axis[2] = c_1 - (xx + yy);
}

}

template <typename T>
Mat4x4<T> quaternion_to_rotation_matrix(const Vec4<T>& quaternion)
{
  Mat4x4<T> rot;
  detail::quaternion_axes_(quaternion, rot.m[0], rot.m[1], rot.m[2]);

  constexpr auto c_0 = static_cast<T>(0);
  rot.m[0][3] = c_0;
  rot.m[1][3] = c_0;
  rot.m[2][3] = c_0;
  rot.m[3][0] = c_0;
  rot.m[3][1] = c_0;
  rot.m[3][2] = c_0;
  rot.m[3][3] = static_cast<T>(1);
  return rot;
}

template<typename T>
void quaternion_axes(const Vec4<T>& quaternion, Vec3<T>& x_axis, Vec3<T>& y_axis, Vec3<T>& z_axis)
{
  detail::quaternion_axes_(quaternion, x_axis.begin(), y_axis.begin(), z_axis.begin());
}

I3S_EXPORT Vec4d rotation_matrix_to_quaternion(const Mat4d& rotation_matrix);

} // namespace utl
} // namespace i3slib
