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
#include "utils/utl_obb.h"
#include "utils/utl_box.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_prohull.h"
#include "utils/utl_quaternion.h"
#include <array>

namespace i3slib
{

namespace utl
{

Obb_abs::Obb_abs(const Boxd& box)
{ 
  center = box.center(); 
  extents = Vec3f(0.5 * Vec3d(box.width(), box.height(), box.depth()));
  orientation = identity_quaternion<double>();
}

//! inefficient implementation.
Boxd Obb_abs::to_aabb() const
{
  if(!is_valid())
    return {};

  Vec3d x_axis, y_axis, z_axis;
  quaternion_axes(orientation, x_axis, y_axis, z_axis);

  const auto dx = std::abs(x_axis.x * extents.x) + std::abs(y_axis.x * extents.y) + std::abs(z_axis.x * extents.z);
  const auto dy = std::abs(x_axis.y * extents.x) + std::abs(y_axis.y * extents.y) + std::abs(z_axis.y * extents.z);
  const auto dz = std::abs(x_axis.z * extents.x) + std::abs(y_axis.z * extents.y) + std::abs(z_axis.z * extents.z);
  return Boxd(center.x - dx, center.y - dy, center.x + dx, center.y + dy, center.z - dz, center.z + dz);
} 

 namespace
{

// m = diagonal scaling matrix * m
template<typename T>
void scale_before(Mat4x4<T>& m, const Vec3<T>& scale)
{
  m.m[0][0] *= scale.x; m.m[0][1] *= scale.x; m.m[0][2] *= scale.x;
  m.m[1][0] *= scale.y; m.m[1][1] *= scale.y; m.m[1][2] *= scale.y;
  m.m[2][0] *= scale.z; m.m[2][1] *= scale.z; m.m[2][2] *= scale.z;
}

}

void Obb_abs::get_transform_matrix(Mat4d& mat) const
{
  mat = quaternion_to_rotation_matrix(orientation);
  scale_before(mat, 2.0 * Vec3d(extents));
  mat.translate(center);
}

Vec3d Obb_abs::axis_z() const
{
  // This is suboptimal, it's possible to compute only the 3 components needed.
  const auto m = quaternion_to_rotation_matrix(orientation);
  return { m._31, m._32, m._33 };
}

bool Obb_abs::contains(const Vec3d& point) const
{
  const auto p = rotate_vec3_by_quaternion_inverse(point - center, orientation);
  return 
    -extents.x <= p.x && p.x <= extents.x &&
    -extents.y <= p.y && p.y <= extents.y &&
    -extents.z <= p.z && p.z <= extents.z;
}

//! corner order is (--+)(+-+)(+++)(-++)(---)(+--)(++-)(-+-)
void Obb_abs::get_corners(Vec3d* corners, int count) const
{
  I3S_ASSERT(is_valid());
  I3S_ASSERT(count == 8);

  // Would it be more efficient to get obb axes, multiply by extents and sum up? 
  const Vec2d ext_neg(-extents.x, -extents.y);
  const auto delta0 = rotate_vec3_by_quaternion({ ext_neg.x, ext_neg.y, extents.z }, orientation);
  const auto delta1 = rotate_vec3_by_quaternion({ extents.x, ext_neg.y, extents.z }, orientation);
  const auto delta2 = rotate_vec3_by_quaternion({ extents.x, extents.y, extents.z }, orientation);
  const auto delta3 = rotate_vec3_by_quaternion({ ext_neg.x, extents.y, extents.z }, orientation);

  corners[0] = center + delta0;
  corners[1] = center + delta1;
  corners[2] = center + delta2;
  corners[3] = center + delta3;
  corners[4] = center - delta2;
  corners[5] = center - delta3;
  corners[6] = center - delta0;
  corners[7] = center - delta1;
}

// static
Obb_abs Obb_abs::from_corners(const Vec3d* corners, int count)
{
  I3S_ASSERT(count == 8);

  // Average side vectors 01, 32, 45, 76 to find X axes of the obb
  // Average side vectors 03, 12, 47, 56 to find Y axes of the obb
  // Average side vectors 40, 51, 62, 73 to find Z axes of the obb
  std::array<Vec3d, 3> axes =
  {
    (corners[1] - corners[0]) + (corners[2] - corners[3]) + (corners[5] - corners[4]) + (corners[6] - corners[7]),
    (corners[3] - corners[0]) + (corners[2] - corners[1]) + (corners[7] - corners[4]) + (corners[6] - corners[5]),
    (corners[0] - corners[4]) + (corners[1] - corners[5]) + (corners[2] - corners[6]) + (corners[3] - corners[7])
  };

  std::array<double, 3> lengths = { axes[0].length(), axes[1].length(), axes[2].length() };

  auto ind0 = 0;
  if (lengths[1] > lengths[0])
    ind0 = 1;
  if (lengths[2] > lengths[ind0])
    ind0 = 2;

  if (lengths[ind0] < 1e-8)
  {
    // The obb is totally degenerate, it's basically a point.
    Obb_abs obb;
    obb.center = corners[0];   // average all corners?
    obb.extents = { 0.0, 0.0, 0.0 };
    obb.orientation = { 0.0, 0.0, 0.0, 1.0 };  // identity quaternion
    return obb;
  }

  int parity = ind0 % 2;
  auto ind1 = (ind0 + 1) % 3;
  auto ind2 = (ind0 + 2) % 3;
  if (lengths[ind1] < lengths[ind2])
    std::swap(ind1, ind2);
  if (ind1 > ind2)
    parity = 1 - parity;

  Vec4d orientation;
  if (lengths[ind1] < 1e-8)
  {
    // The obb has two dimensions collapsed, so it's basically a line segment.
    if (std::abs(axes[ind0].x) > (1.0 - 1e-8)* lengths[ind0])
      orientation = identity_quaternion<double>();
    else
    {
      orientation = { 0.0, -axes[ind0].z, axes[ind0].y, lengths[ind0] + axes[ind0].x };
      orientation = orientation / orientation.length();
    }
  }
  else
  {
    // The obb has at least two non-degenerate dimensions.
    // Make sure the axes are orthogonal, normalized and right-hand oriented.
    axes[ind0] = axes[ind0].normalized();
    axes[ind1] -= axes[ind1].dot(axes[ind0]) * axes[ind0];
    axes[ind1] = axes[ind1].normalized();
    axes[ind2] = Vec3d::cross(axes[ind0], axes[ind1]);
    if (parity == 1)
      axes[ind2] = -axes[ind2];

    // Create the rotation matrix and convert it to a quaternion.
    Mat4d rot(
      axes[0].x, axes[0].y, axes[0].z, 0.0,
      axes[1].x, axes[1].y, axes[1].z, 0.0,
      axes[2].x, axes[2].y, axes[2].z, 0.0,
      0.0, 0.0, 0.0, 1.0);

    orientation = rotation_matrix_to_quaternion(rot);
  }

  // Expand the box to make sure the corners are included.
  return from_points(corners, count, orientation);
}

// static
Obb_abs Obb_abs::from_points(const Vec3d* points, int count, const Vec4d& orientation)
{
  Extent_calculator ext_calc(orientation);
  ext_calc.expand(points, count);
  return ext_calc.calc_extent();
}

// static
Obb_abs Obb_abs::from_points_ex(const Vec3d* points, int count)
{
  Pro_set base;
  Pro_hull hull(&base);
  for (int i = 0; i < count; i++)
    hull.add(points[i]);

  Obb_abs obb;
  hull.get_bounding_box(obb);
  return obb;
}


//------------------------------------------------------------------------------------
//  class Extent_calculator
//------------------------------------------------------------------------------------
void Extent_calculator::expand(const Vec3d* points, int count)
{
  I3S_ASSERT(count >= 0);
  for (int i = 0; i < count; i++)
  {
    const auto p = rotate_vec3_by_quaternion_inverse(points[i], quaternion_);
    min_ = min(min_, p);
    max_ = max(max_, p);
  }
}

void Extent_calculator::expand(const Obb_abs& obb)
{
  std::array<Vec3d, 8> corners;
  obb.get_corners(corners.data(), 8);
  expand(corners.data(), 8);
}

Obb_abs Extent_calculator::calc_extent() const
{
  I3S_ASSERT(min_.x <= max_.x);
  I3S_ASSERT(min_.y <= max_.y);
  I3S_ASSERT(min_.z <= max_.z);

  const auto center = rotate_vec3_by_quaternion((min_ + max_) * 0.5, quaternion_);
  return { center, Vec3f(0.5 * (max_ - min_)), quaternion_ };
}

} // namespace utl

} // namespace i3slib
