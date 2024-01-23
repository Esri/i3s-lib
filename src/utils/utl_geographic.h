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
#include <cmath>

namespace i3slib
{

namespace utl
{

//! assumed size of the earth sphere:
static constexpr double c_wgs84_equatorial_radius = 6378137.0; //semi-major 
static constexpr double c_wgs84_polar_radius      = 6356752.314245; //semi-minor
static constexpr double c_wgs84_eccentricity      = 0.08181919092890624;
static constexpr double c_max_coordinate_epsg3857 = 20037508.34;
// assume x: lon=la=0, y = east and z = north

void  geocentric2cartesian(const Vec3d& llz, Vec3d* xyz) noexcept;
void  geocentric2cartesian(Vec3d* in_out, int count=1 ) noexcept;
void  cartesian2geocentric(const Vec3d& llz, Vec3d* xyz) noexcept;
void  cartesian2geocentric(const Vec3d& llz, Vec3d* xyz) noexcept;
void  geodetic2ECEF(const Vec3d& llz, Vec3d* xyz)noexcept;
void  geodetic2ECEF(Vec3d* in_out, int count=1) noexcept;

void  ECEF2geodetic(const Vec3d& xyz, Vec3d* llz)noexcept;
void  ECEF2geodetic(Vec3d* in_out, int count = 1) noexcept;

//-----

inline void  geocentric2cartesian(const Vec3d& llz, Vec3d* xyz) noexcept
{
  double d = c_wgs84_equatorial_radius + llz.z;
  double lon_rad = radians(llz.x);
  double lat_rad = radians(llz.y);
  double cos_lon = cos( lon_rad );
  double sin_lon = sin( lon_rad );
  double cos_lat = cos( lat_rad );
  double sin_lat = sin(lat_rad);
  double proj = cos_lat * d;
  xyz->x = cos_lon * proj;
  xyz->y = sin_lon * proj;
  xyz->z = sin_lat * d;
}

inline void  geocentric2cartesian(Vec3d* in_out, int count) noexcept
{
  Vec3d tmp;
  for (int i = 0; i < count; i++)
  {
    geocentric2cartesian(in_out[i], &tmp);
    in_out[i] = tmp;
  }
}


inline void  cartesian2geocentric(const Vec3d& xyz, Vec3d* llz) noexcept
{
  double epsi = 1e-6;

  double d = xyz.length();
  if (d < epsi)
  {
    *llz = Vec3d(0.0);
    return;
  }

  //calc longitude
  llz->x = degrees( std::atan2(xyz.y, xyz.x) ); // if x=y=0 -> longitude is set to zero, this is true for VS2013+ and IEEE floating-point compliant compilers.
  //calc latitude:
  llz->y = degrees( asin( xyz.z / d ) );
  llz->z = d - c_wgs84_equatorial_radius; 
}
inline void  cartesian2geocentric(Vec3d* in_out, int count=1)
{
  Vec3d tmp;
  for (int i = 0; i < count; i++)
  {
    cartesian2geocentric(in_out[i], &tmp);
    in_out[i] = tmp;
  }
}

inline void  geodetic2ECEF(const Vec3d& llz, Vec3d* xyz) noexcept
{
  double lon_rad = radians(llz.x);
  double lat_rad = radians(llz.y);
  double cos_lon = cos(lon_rad);
  double sin_lon = sin(lon_rad);
  double cos_lat = cos(lat_rad);
  double sin_lat = sin(lat_rad);

  double e2 = c_wgs84_eccentricity * c_wgs84_eccentricity;
  double norm = c_wgs84_equatorial_radius / sqrt(1.0 - e2 * sin_lat * sin_lat);

  xyz->x = (norm + llz.z) * cos_lat * cos_lon; //Pro Y
  xyz->y = (norm + llz.z) * cos_lat * sin_lon; //Pro Z
  xyz->z = (norm *(1.0 - e2) + llz.z) * sin_lat; //Pro X
    
  //double norm = sqrt(1.0 - (e2 * sin_lat * sin_lat));
  //pts[i].x = (a / norm + h) * cos_lat * cos(lon); //pro Y
  //pts[i].y = (a / norm + h) * cos_lat * sin(lon); //pro Z
  //pts[i].z = (a * (1 - e2) / norm + h) * sin_lat;   //pro X
}

inline void  ECEF2geodetic( const Vec3d& xyz, Vec3d* llz ) noexcept
{
  double a    = c_wgs84_equatorial_radius; // spheroid.semi_major_axis;
  double b    = c_wgs84_polar_radius; //  spheroid.semi_minor_axis;
  double e2   = c_wgs84_eccentricity * c_wgs84_eccentricity; //spheroid.eccentricity_sqrd;
  double ep2  = (a*a- b*b) / (b*b); // spheroid.eccentricity_prime_sqrd;

  double in_x = xyz.y; //y
  double in_y = xyz.z; //z
  double in_z = xyz.x; //x
  //double in_x = xyz.x;
  //double in_y = xyz.y;
  //double in_z = xyz.z;

  double p = sqrt(in_z * in_z + in_x * in_x);
  double theta = atan(in_y * a / (p * b));
  double sin_theta3 = sin(theta);
  sin_theta3 = sin_theta3 * sin_theta3 * sin_theta3;
  double cos_theta3 = cos(theta);
  cos_theta3 = cos_theta3 * cos_theta3 * cos_theta3;
  double lon = atan2(in_x, in_z);
  double lat = atan((in_y + ep2 * b * sin_theta3) / (p - e2 * a * cos_theta3));
  double sinLat = sin(lat);
  double n = a / sqrt(1.0 - (e2 * sinLat * sinLat));

  llz->z = (p / cos(lat)) - n;
  double const c_180_div_pi = 180.0 / c_pi;
  llz->y = lat * c_180_div_pi;
  llz->x = lon * c_180_div_pi;
}

inline void  geodetic2ECEF(Vec3d* in_out, int count) noexcept
{
  Vec3d tmp;
  for (int i = 0; i < count; i++)
  {
    geodetic2ECEF(in_out[i], &tmp);
    //geodetic2ECEF2(in_out[i], &tmp);
    in_out[i] = tmp;
  }
}


inline void  ECEF2geodetic(Vec3d* in_out, int count ) noexcept
{
  Vec3d tmp;
  for (int i = 0; i < count; i++)
  {
    ECEF2geodetic(in_out[i], &tmp);
    in_out[i] = tmp;
  }
}

}

} // namespace i3slib
