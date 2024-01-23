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
#include <cmath>

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


double Obb_abs::get_squared_distance_to(const Vec3d& point) const
{
  const auto p = rotate_vec3_by_quaternion_inverse(point - center, orientation).abs() - Vec3d(extents);
  return Vec3d{ std::max(p.x, 0.0), std::max(p.y, 0.0), std::max(p.z, 0.0) }.length_sqr();
}

std::pair<double, double> Obb_abs::get_minmax_squared_distance_to(const Vec3d& point) const
{
  const auto p = rotate_vec3_by_quaternion_inverse(point - center, orientation).abs();
  double d_min_squared = 0;
  double d_max_squared = 0;
  for (int i = 0; i < 3; ++i)
  {
    double c = p[i];
    double e = extents[i];
    double s = c + e;
    double d = c - e;
    if ( d > 0.0)
      d_min_squared += d * d;
    d_max_squared += s * s;    
  }
  return {d_min_squared, d_max_squared};
}

bool Obb_abs::contains(const Obb_abs& obb) const
{
  I3S_ASSERT(extents.x >= 0.0);
  I3S_ASSERT(extents.y >= 0.0);
  I3S_ASSERT(extents.z >= 0.0);

  // Transform center of the arg OBB into the CS of this OBB.
  const auto inv_quat = quaternion_conjugate(orientation);
  const auto c = rotate_vec3_by_quaternion(obb.center - center, inv_quat);

  // Get axes of the arg OBB in the local CS of this OBB.
  std::array<Vec3d, 3> axes;
  quaternion_axes(quaternion_product(inv_quat, obb.orientation), axes[0], axes[1], axes[2]);

  const auto& e = obb.extents;
  using std::abs;

  // For every coordinate axis of the local CS of this OBB, we need to check if
  // the span of the parameter OBB along the axis is within this OBB extent
  // along the axis.
  //
  // Let's consider the X axis, for Y and Z everything is just the same. 
  //
  // An OBB is a convex body, so its projection on a coordinate axis is bounded
  // by projections of two of its corners.
  //
  // Positions of the corners of the OBB with given center, extents and axes
  // can be described by the following formula:
  //   c +- e.x * axes[0] +- e.y * axes[1] +- e.z * axes[2] ,
  // Every +- sign above can be replaced with + or -, so there are eight possible
  // combinations describing all eight corners.
  //
  // Positions of the projections of the corners on the X axis are:
  //   c.x +- e.x * axes[0].x +- e.y * axes[1].x +- e.z * axes[2].x.
  //
  // The maximum of this expression is achieved when every +- sign is chosen
  // to match the sign of the corresponding member, resuting in a positive
  // value, and the resulting value is equal to
  //   c.x + abs(e.x * axes[0].x) + abs(e.y * axes[1].x) + abs(e.z * axes[2].x).
  //
  // The minimum of the expression is achived when +- sign is chosen to counter
  // the sign of the corresponding member and equals to
  //   c.x - (abs(e.x * axes[0].x) + abs(e.y * axes[1].x) + abs(e.z * axes[2].x)).
  //
  // This OBB span along the X axis is [-extent.x, extent.x], so We need to check
  // if the min value above is g.e. than -extent.x and the max value is l.e.
  // than extent.x. Using obvious symmetry considerations this can be done
  // with a single test:
  //  abs(c.x) + abs(e.x * axes[0].x) + abs(e.y * axes[1].x) + abs(e.z * axes[2].x) <= extents.x
  //
  // Repeating the same logic for the Y and Z axes results in the test below.

  return
    abs(c.x) + abs(e.x * axes[0].x) + abs(e.y * axes[1].x) + abs(e.z * axes[2].x) <= extents.x &&
    abs(c.y) + abs(e.x * axes[0].y) + abs(e.y * axes[1].y) + abs(e.z * axes[2].y) <= extents.y &&
    abs(c.z) + abs(e.x * axes[0].z) + abs(e.y * axes[1].z) + abs(e.z * axes[2].z) <= extents.z;
}

// NB: this implementation is not perfectly symmetric - it's possible that for some
// obb1 and obb2
//       obb1.intersects(obb2) != obb2.intersects(obb1).
// This may happen if obb1 and obb2 are "almost intersecting" or "barely intersecting".
// It's possible to make this func strictly symmetric, but for the price of essential
// performance loss.

bool Obb_abs::intersects(const Obb_abs& obb) const
{
  const Vec3d ext1(extents);
  I3S_ASSERT(ext1.x >= 0.0);
  I3S_ASSERT(ext1.y >= 0.0);
  I3S_ASSERT(ext1.z >= 0.0);

  // Transform center of the arg OBB into the CS of this OBB.
  // If the center of arg OBB is inside this OBB, they definitely intersect.
  const auto inv_quat = quaternion_conjugate(orientation);
  const auto delta = obb.center - center;
  const auto center2 = rotate_vec3_by_quaternion(delta, inv_quat);
  const Vec3d center_abs2(std::abs(center2.x), std::abs(center2.y), std::abs(center2.z));
  if (center_abs2.x <= ext1.x && center_abs2.y <= ext1.y && center_abs2.z <= ext1.z)
    return true;

  const Vec3d ext2(obb.extents);
  I3S_ASSERT(ext2.x >= 0.0);
  I3S_ASSERT(ext2.y >= 0.0);
  I3S_ASSERT(ext2.z >= 0.0);

  // Get axes of the arg OBB in the local CS of this OBB.
  // Note that transposing these axes (as matrix) will give us axes of this OBB in the CS of the arg OBB.
  std::array<Vec3d, 3> axes;
  quaternion_axes(quaternion_product(inv_quat, obb.orientation), axes[0], axes[1], axes[2]);

  // Try axes of this OBB for separation direction. 
  std::array<Vec3d, 3> axes_abs;
  axes_abs[0].x = std::abs(axes[0].x);
  axes_abs[1].x = std::abs(axes[1].x);
  axes_abs[2].x = std::abs(axes[2].x);
  if (center_abs2.x > ext1.x + ext2.x * axes_abs[0].x + ext2.y * axes_abs[1].x + ext2.z * axes_abs[2].x)
    return false;

  axes_abs[0].y = std::abs(axes[0].y);
  axes_abs[1].y = std::abs(axes[1].y);
  axes_abs[2].y = std::abs(axes[2].y);
  if (center_abs2.y > ext1.y + ext2.x * axes_abs[0].y + ext2.y * axes_abs[1].y + ext2.z * axes_abs[2].y)
    return false;

  axes_abs[0].z = std::abs(axes[0].z);
  axes_abs[1].z = std::abs(axes[1].z);
  axes_abs[2].z = std::abs(axes[2].z);
  if (center_abs2.z > ext1.z + ext2.x * axes_abs[0].z + ext2.y * axes_abs[1].z + ext2.z * axes_abs[2].z)
    return false;

  // Transform center of this OBB into the local CS of the arg OBB.
  // If the center of arg OBB is inside this OBB, they definitely intersect.
  const auto center1 = rotate_vec3_by_quaternion_inverse(-delta, obb.orientation);
  const Vec3d center_abs1(std::abs(center1.x), std::abs(center1.y), std::abs(center1.z));
  if (center_abs1.x <= ext2.x && center_abs1.y <= ext2.y && center_abs1.z <= ext2.z)
    return true;

  // Try axes of the arg OBB for separation direction. 
  if (center_abs1.x > ext2.x + ext1.x * axes_abs[0].x + ext1.y * axes_abs[0].y + ext1.z * axes_abs[0].z ||
      center_abs1.y > ext2.y + ext1.x * axes_abs[1].x + ext1.y * axes_abs[1].y + ext1.z * axes_abs[1].z ||
      center_abs1.z > ext2.z + ext1.x * axes_abs[2].x + ext1.y * axes_abs[2].y + ext1.z * axes_abs[2].z)
    return false;

  // Now we need to test for separation all possible directions of the kind cross(e1[i], e2[j]),
  // where e1[i] are direction of the edges of this obb, and e2[j] are directions of the edge
  // of the arg obb.
  // If we make e1 and e2 as orthonormal and in the right-handed order, computing projection
  // spans of the obbs on the directions in question can be done as follows: 
  //  (e1[i] x e2[j]) * e1[k] == -(e1[i] x e1[k]) * e2[j] == (-1)^(i + j + 1) * e1[3 - i - k] * e2[j] if k != i else 0
  //  |(e1[i] x e2[j]) * e2[k]| == |e1[i] * (e2[j] x e2[k])| == |e1[i] * e2[3 - j - k]| if k != j else 0
  // (The second formula is used to compute "radiuses" of bounding boxes, so we only need
  // absolute values there, no need to bother with the sign).
  // If we operate in the local CS of this obb, we can just use axis unit vectors u[i] for e1[i],
  // while axes[j] computed above can be used for e2[j]. The above formulae become:
  //   (u[i] x axes[j]) * u[k] == (-1)^(i + j + 1) * axes[j][3 - i - k] if k != i else 0
  //  |(u[i] x axes[j]) * axes[k]| ==  |u[i] * (axes[j] x axes[k])| ==  |axes[3 - j - k][i]| if k != j else 0

  // Try cross((1, 0, 0), axes[0]) for separation direction.
  //  (u[0] x axes[0]) * u[k] == -axes[0][3 - k] if k != 0 else 0
  //  |(u[0] x axes[0]) * axes[k]| == |axes[3 - k][0]| if k != 0 else 0
  auto c2 = std::abs(center2.z * axes[0].y - center2.y * axes[0].z);
  auto r1 = ext1.y * axes_abs[0].z + ext1.z * axes_abs[0].y;
  auto r2 = ext2.y * axes_abs[2].x + ext2.z * axes_abs[1].x;
  if (r1 + r2 < c2)
    return false;

  // Try cross((1, 0, 0), axes[1]) for separation direction.
  //  (u[0] x axes[1]) * u[k] == -axes[1][3 - k] if k != 0 else 0
  //  |(u[0] x axes[1]) * axes[k]| == |axes[2 - k][0]| if k != 1 else 0
  c2 = std::abs(center2.z * axes[1].y - center2.y * axes[1].z);
  r1 = ext1.y * axes_abs[1].z + ext1.z * axes_abs[1].y;
  r2 = ext2.x * axes_abs[2].x + ext2.z * axes_abs[0].x;
  if (r1 + r2 < c2)
    return false;

  // Try cross((1, 0, 0), axes[2]) for separation direction.
  //  (u[0] x axes[2]) * u[k] == -axes[2][3 - k] if k != 0 else 0
  //  |(u[0] x axes[2]) * axes[k]| == |axes[1 - k][0]| if k != 2 else 0
  c2 = std::abs(center2.z * axes[2].y - center2.y * axes[2].z);
  r1 = ext1.y * axes_abs[2].z + ext1.z * axes_abs[2].y;
  r2 = ext2.x * axes_abs[1].x + ext2.y * axes_abs[0].x;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 1, 0), axes[0]) for separation direction.
  //  (u[1] x axes[0]) * u[k] == axes[0][2 - k] if k != 1 else 0
  //  |(u[1] x axes[0]) * axes[k]| == |axes[3 - k][1]| if k != 0 else 0
  c2 = std::abs(center2.x * axes[0].z - center2.z * axes[0].x);
  r1 = ext1.x * axes_abs[0].z + ext1.z * axes_abs[0].x;
  r2 = ext2.y * axes_abs[2].y + ext2.z * axes_abs[1].y;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 1, 0), axes[1]) for separation direction.
  //  (u[1] x axes[1]) * u[k] == axes[1][2 - k] if k != 1 else 0
  //  |(u[1] x axes[1]) * axes[k]| == |axes[2 - k][1]| if k != 1 else 0
  c2 = std::abs(center2.x * axes[1].z - center2.z * axes[1].x);
  r1 = ext1.x * axes_abs[1].z + ext1.z * axes_abs[1].x;
  r2 = ext2.x * axes_abs[2].y + ext2.z * axes_abs[0].y;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 1, 0), axes[2]) for separation direction.
  //  (u[1] x axes[2]) * u[k]| == axes[2][2 - k] if k != 1 else 0
  //  |(u[1] x axes[2]) * axes[k]| == |axes[1 - k][1]| if k != 2 else 0
  c2 = std::abs(center2.x * axes[2].z - center2.z * axes[2].x);
  r1 = ext1.x * axes_abs[2].z + ext1.z * axes_abs[2].x;
  r2 = ext2.x * axes_abs[1].y + ext2.y * axes_abs[0].y;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 0, 1), axes[0]) for separation direction.
  //  (u[2] x axes[0]) * u[k] == -axes[0][1 - k] if k != 2 else 0
  //  |(u[2] x axes[0]) * e2[k]| == |axes[3 - k][2]| if k != 0 else 0
  c2 = std::abs(center2.y * axes[0].x - center2.x * axes[0].y);
  r1 = ext1.x * axes_abs[0].y + ext1.y * axes_abs[0].x;
  r2 = ext2.y * axes_abs[2].z + ext2.z * axes_abs[1].z;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 0, 1), axes[1]) for separation direction.
  //  (u[2] x axes[1]) * u[k] == -axes[1][1 - k] if k != 2 else 0
  //  |(u[2] x axes[1]) * e2[k]| == |axes[2 - k][2]| if k != 1 else 0
  c2 = std::abs(center2.y * axes[1].x - center2.x * axes[1].y);
  r1 = ext1.x * axes_abs[1].y + ext1.y * axes_abs[1].x;
  r2 = ext2.x * axes_abs[2].z + ext2.z * axes_abs[0].z;
  if (r1 + r2 < c2)
    return false;

  // Try cross((0, 0, 1), axes[2]) for separation direction.
  //  (u[2] x axes[2]) * u[k] == -axes[2][1 - k] if k != 2 else 0
  //  |(u[2] x axes[2]) * axes[k]| == |axes[1 - k][2]| if k != 2 else 0
  c2 = std::abs(center2.y * axes[2].x - center2.x * axes[2].y);
  r1 = ext1.x * axes_abs[2].y + ext1.y * axes_abs[2].x;
  r2 = ext2.x * axes_abs[1].z + ext2.y * axes_abs[0].z;
  if (r1 + r2 < c2)
    return false;

  // There's no separation direction, hence the obbs intersect.
  return true;
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
  Obb_abs obb;
  double radius = 0.0; // to make static analysis happy
  Pro_hull hull;
  hull.get_ball_box(points, count, obb, radius);
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
