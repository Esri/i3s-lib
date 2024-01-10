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

#include "i3s/i3s_common_.h"
#include "utils/utl_i3s_export.h"
#include "utils/utl_serialize.h"
#include <stdint.h>

namespace i3slib
{

namespace i3s
{
struct Geometry_schema_desc;
I3S_EXPORT Mesh_abstract*   parse_mesh_from_i3s( const  utl::Vec3d& origin, utl::Raw_buffer_view& buff, bool has_feature_attrib
                                          , Attrib_flags expected_vb_attrib, const std::string& path, const i3s::Geometry_schema_desc* desc, utl::Basic_tracker* trk);

I3S_EXPORT Mesh_abstract*   parse_mesh_from_bulk(const Mesh_bulk_data& data);
I3S_EXPORT Mesh_abstract::Ptr create_empty_mesh();

I3S_EXPORT Mesh_abstract*   parse_points_from_i3s(const  utl::Vec3d& origin,const std::string&  feature_data_json, const std::string& path, utl::Basic_tracker* trk);
void             encode_points_to_i3s(int count, const utl::Vec3d* xyz, const uint64_t* fids, std::string* feature_data_json, utl::Basic_tracker*);

I3S_EXPORT utl::Raw_buffer_view encode_legacy_buffer(const Mesh_abstract&  mesh, Attrib_flags * out_actual_attributes_mask=nullptr);

// -----------------------------------------------------------------------------
//      Legacy point feature : XYZ + fid from featureData JSON resource
// -----------------------------------------------------------------------------

struct Point_feature_desc
{
  // --- fields:
  utl::Vec3d position;
  int64_t    fid;
  SERIALIZABLE(Point_feature_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("position", utl::seq(position));
    ar & utl::nvp("id", fid);
  }
};

struct Point_feature_data_desc
{
  // --- fields:
  std::vector< Point_feature_desc > points;

  SERIALIZABLE(Point_feature_data_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("featureData", utl::seq(points));
  }
};

}

} // namespace i3slib
