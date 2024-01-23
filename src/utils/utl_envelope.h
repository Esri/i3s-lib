/*
Copyright 2022 Esri

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
#include "utils/utl_box.h"
#include <utility>

namespace i3slib
{

namespace utl 
{

I3S_EXPORT Boxd compute_points_geo_envelope(const Vec3d* points, size_t point_count);

I3S_EXPORT Boxd compute_mesh_geo_envelope(const Vec3d* triangles, size_t triangle_count);

I3S_EXPORT Boxd merge_geo_envelopes(const Boxd* aabbs, size_t count);

I3S_EXPORT void inflate_geo_envelope(utl::Boxd& box, double margin_factor);

I3S_EXPORT double get_longitude_range_size(double west, double east);

I3S_EXPORT std::pair<double, double> expand_longitude_range(double west, double east, double dw, double de);

I3S_EXPORT bool longitude_ranges_overlap(double west1, double east1, double west2, double east2);

} // namespace utl

} // namespace i3slib
