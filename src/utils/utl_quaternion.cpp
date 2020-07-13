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
#include "utils/utl_quaternion.h"

namespace i3slib
{
namespace utl
{

Vec4d rotation_matrix_to_quaternion(const Mat4d& rotation_matrix)
{
  const auto& m = rotation_matrix.m;
  if (m[2][2] >= 0.0)
  {
    const auto s = m[1][1] + m[0][0];
    if (s >= 0.0)
    {
      const auto w_sqr_4 = 1.0 + m[2][2] + s;
      const auto w_2 = std::sqrt(w_sqr_4);
      const auto inv = 0.5 / w_2;
      return { (m[1][2] - m[2][1]) * inv, (m[2][0] - m[0][2]) * inv, (m[0][1] - m[1][0]) * inv, 0.5 * w_2 };
    }
    else
    {
      const auto z_sqr_4 = 1.0 + m[2][2] - s;
      const auto z_2 = std::sqrt(z_sqr_4);
      const auto inv = 0.5 / z_2;
      return { (m[0][2] + m[2][0]) * inv, (m[1][2] + m[2][1]) * inv, 0.5 * z_2, (m[0][1] - m[1][0]) * inv };
    }
  }
  else
  {
    const auto d = m[1][1] - m[0][0];
    if (d >= 0.0)
    {
      const auto y_sqr_4 = 1.0 - m[2][2] + d;
      const auto y_2 = std::sqrt(y_sqr_4);
      const auto inv = 0.5 / y_2;
      return { (m[0][1] + m[1][0]) * inv, 0.5 * y_2, (m[1][2] + m[2][1]) * inv, (m[2][0] - m[0][2]) * inv };
    }
    else
    {
      const auto x_sqr_4 = 1.0 - m[2][2] - d;
      const auto x_2 = std::sqrt(x_sqr_4);
      const auto inv = 0.5 / x_2;
      return { 0.5 * x_2, (m[0][1] + m[1][0]) * inv, (m[0][2] + m[2][0]) * inv, (m[1][2] - m[2][1]) * inv };
    }
  }
}

} // namespace utl
} // namespace i3slib
