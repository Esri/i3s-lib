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
#include "utils/utl_serialize.h"
#include "utils/utl_geom.h"
#include <vector>

namespace i3slib
{

namespace utl 
{
template<typename T> class Box;

#pragma warning(push)
#pragma warning(disable: 4251)

struct I3S_EXPORT Obb_abs
{
  SERIALIZABLE(Obb_abs);

  Obb_abs() { extents.x = std::numeric_limits< float >::max(); }

  Obb_abs( const Vec3d& c, const Vec3f& e, const Vec4d& o = { 0.0, 0.0, 0.0, 1.0 }) :
    center(c), extents(e), orientation(o) {} 

  explicit Obb_abs(const Box<double>& box);

  template<typename Ar> void serialize(Ar& ar);

  bool  is_valid() const { return  extents.x != std::numeric_limits< float >::max() && extents != Vec3f(0.0); }
  bool  is_empty() const { return extents.x <= 0.0f || extents.y <= 0.0f || extents.z <= 0.0f; }
  float radius() const   { return extents.length(); }

  Box<double> to_aabb() const;

  //! matrix to be applied to a unit box centered at the origin.
  // NOTE that the unit box is NOT [0, 1] * [0, 1] * [0, 1], 
  // it is [-0.5, 0.5] * [-0.5, 0.5] * [-0.5, 0.5].
  void get_transform_matrix(Mat4d& mat) const;

  Vec3d axis_z() const;

  bool contains(const Vec3d& point) const;

  // count should always be set to 8.
  // corner order is (--+)(+-+)(+++)(-++)(---)(+--)(++-)(-+-)
  void get_corners(Vec3d* corners, int count) const;

  // count should always be set to 8.
  // corner order should be (--+)(+-+)(+++)(-++)(---)(+--)(++-)(-+-)
  static Obb_abs from_corners(const Vec3d* points, int count);

  static Obb_abs from_points(const Vec3d* points, int count, const Vec4d& orientation);
  static Obb_abs from_points_ex(const Vec3d* points, int count);

  Vec3d center;            // Center of the box.
  Vec3f extents;           // Distance from the center to each side.
  Vec4d orientation;       // Unit quaternion representing rotation (box -> world).
};

#pragma warning(pop)

inline bool operator==(const Obb_abs& a, const Obb_abs& b) noexcept
{
  return a.center == b.center && a.extents == b.extents && a.orientation == b.orientation;
}

template< class Ar > 
inline void Obb_abs::serialize( Ar& ar ) 
{ 
  ar & nvp("center", seq(center));
  ar & nvp("halfSize", seq(extents));
  ar & nvp("quaternion", seq(orientation));
}

class Extent_calculator
{
public:

  Extent_calculator()
  {
    init({ 0.0, 0.0, 0.0, 1.0 });
  }

  explicit Extent_calculator(const Vec4d& quaternion) 
  {
    init(quaternion);
  }

  void init(const Vec4d& quaternion)
  {
    quaternion_ = quaternion;
    min_ = Vec3d(std::numeric_limits<double>::max());
    max_ = Vec3d(std::numeric_limits<double>::lowest());
  }

  I3S_EXPORT void expand(const Vec3d* points, int count);
  I3S_EXPORT void expand(const Obb_abs& box);
  I3S_EXPORT Obb_abs calc_extent() const;

private:

  Vec4d quaternion_;
  Vec3d min_;
  Vec3d max_;
};

}//endof ::utl

} // namespace i3slib
