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
#include "i3s/i3s_common.h"

namespace i3slib
{

namespace i3s
{

// position, normal, uv, color, uv_Region, feature_id, face_range 
struct VB_attribute_desc
{
  //--- fields:
  //utl::Scalar_format  type = utl::Scalar_format::Any;
  Type  type = Type::Not_set;
  int                 component = 1;
  VB_Binding          binding = VB_Binding::Per_vertex;

  // --- 
  bool                is_valid() const { return type != Type::Not_set; }
  SERIALIZABLE(VB_attribute_desc);
  friend bool operator==(const VB_attribute_desc& a, const VB_attribute_desc& b) {
    return a.type == b.type && a.component == b.component && a.binding == b.binding;
  }

  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("type", utl::enum_str(type)); //required
    ar & utl::nvp("component", component);
    ar & utl::opt("binding", utl::enum_str(binding), VB_Binding::Per_vertex);
  };
};


struct Compressed_attrb_desc
{
  bool is_valid() const { return attributes.size() && encoding != Compressed_geometry_format::Not_init; }
  // ---- fields:
  Compressed_geometry_format     encoding = Compressed_geometry_format::Not_init; //"draco" or "lepcc"
  std::vector< Compressed_mesh_attribute > attributes;
  // ---- 
  SERIALIZABLE(Compressed_attrb_desc);
  friend bool operator==(const Compressed_attrb_desc& a, const Compressed_attrb_desc& b)
  {
    return a.encoding == b.encoding && a.attributes == b.attributes;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("encoding", encoding);
    ar & utl::nvp("attributes", utl::seq(attributes));
  }
};

struct Geometry_buffer_desc
{
  //--- fields:
  //int                   id = 0;
  int                   offset = 0;
  VB_attribute_desc     position;
  VB_attribute_desc     normal;
  VB_attribute_desc     uv0;
  VB_attribute_desc     uv1;
  VB_attribute_desc     color;
  VB_attribute_desc     uv_region;
  VB_attribute_desc     feature_id;
  VB_attribute_desc     face_range;
  Compressed_attrb_desc compressed;
  // --- 
  friend bool operator==(const Geometry_buffer_desc& a, const Geometry_buffer_desc& b)
  {
    return /*a.id == b.id &&*/ a.offset == b.offset && a.position == b.position && a.normal == b.normal
      && a.uv0 == b.uv0 && a.uv0 == b.uv0 && a.color == b.color && a.uv_region == b.uv_region
      && a.feature_id == b.feature_id && a.face_range == b.face_range && a.compressed == b.compressed;
  }
  SERIALIZABLE(Geometry_buffer_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    //ar & utl::nvp("id", id );
    ar & utl::opt("offset", offset, 0);
    ar & utl::opt("position", position, VB_attribute_desc());
    ar & utl::opt("normal", normal, VB_attribute_desc());
    ar & utl::opt("uv0", uv0, VB_attribute_desc());
    ar & utl::opt("uv1", uv1, VB_attribute_desc());
    ar & utl::opt("color", color, VB_attribute_desc());
    ar & utl::opt("uvRegion", uv_region, VB_attribute_desc());
    ar & utl::opt("featureId", feature_id, VB_attribute_desc());
    ar & utl::opt("faceRange", face_range, VB_attribute_desc());
    ar & utl::opt("compressedAttributes", compressed, Compressed_attrb_desc());
  };
};

// v1.7+ only
struct Geometry_definition_desc
{
  //--- fields:
  Mesh_topology  topo = Mesh_topology::Triangles;
  std::vector< Geometry_buffer_desc > geoms;
  // --- 
  friend bool operator==(const Geometry_definition_desc& a, const Geometry_definition_desc& b) { return a.topo == b.topo && a.geoms == b.geoms; }
  SERIALIZABLE(Geometry_definition_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("topology", utl::enum_str(topo), Mesh_topology::Triangles);
    ar & utl::nvp("geometryBuffers", utl::seq(geoms));
    //ar & utl::opt("attributeBuffers", utl::seq(attribs));
  };
};

//! v1.7+ only
struct Attribute_buffer_desc
{
  //--- fields:
  int                 id=0;
  std::string         name;
  std::string         alias;
  Type           type = Type::Not_set;
  int                 offset = 0;
  Encoding            encoding = Encoding::None;
  VB_Binding          binding = VB_Binding::Per_feature;
  bool                has_null_mask = false;

  // --- 
  SERIALIZABLE(Attribute_buffer_desc);
  friend bool operator==(const Attribute_buffer_desc& a, const Attribute_buffer_desc& b)
  {
    return a.id == b.id && a.name == b.name && a.alias == b.alias && a.type == b.type && a.offset == b.offset 
      && a.encoding == b.encoding && a.binding == b.binding && a.has_null_mask == b.has_null_mask;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("id", id);
    ar & utl::nvp("name", name);
    ar & utl::opt("alias", alias, name); // this line must be *after* the "name" nvp line
    ar & utl::nvp("type", utl::enum_str(type));
    ar & utl::opt("encoding", utl::enum_str(encoding), Encoding::None);
    ar & utl::nvp("binding", utl::enum_str(binding));
    ar & utl::opt("offset", offset, 0);
    ar & utl::opt("hasNullMask", has_null_mask, false);
  };
};

}//endof of msh

} // namespace i3slib
