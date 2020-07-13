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
#include "utils/utl_serialize_json_dom.h"
#include "utils/utl_obb.h"
#include "i3s_layer_dom.h"

namespace i3slib
{

namespace i3s
{

//! a mistake.
struct Lod_selection_desc
{
  // --- fields:
  Lod_metric_type   metric_type;
  double            max_error;
  // --- serialization:
  SERIALIZABLE(Lod_selection_desc);
  //friend bool operator==(const Node_desc& a, const Node_desc& b) { return a.offset_z == b.offset_z; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("metricType", utl::enum_str(metric_type));
    ar & utl::nvp("maxError", max_error);
  }
};

// featureData document is deprecated, but for 3DObject, Pro will read the OID from it (for no good reason)
// For Point it actually contains the "geometry" 
struct Legacy_feature_desc
{
  // --- serialization:
  utl::Unparsed_field feature_data;
  utl::Unparsed_field geometry_data;
  SERIALIZABLE(Legacy_feature_desc);
  template< class Ar > void serialize(Ar& ar)
  { 
    ar & utl::nvp("featureData", feature_data);
    ar & utl::nvp("geometryData", geometry_data);
  }
};

struct Feature_ref_desc
{
  // --- fields:
  std::string         href;
  utl::Vec2i          feature_range;

  // --- serialization:
  SERIALIZABLE(Feature_ref_desc);
  friend bool operator==(const Feature_ref_desc& a, const Feature_ref_desc& b) { return a.href == b.href && a.feature_range == b.feature_range; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("href", href);
    ar & utl::opt("featureRange", utl::seq(feature_range));
  }
};

struct Attribute_data_ref_desc
{
  // --- fields:
  std::string         href;

  // --- serialization:
  SERIALIZABLE(Attribute_data_ref_desc);
  friend bool operator==(const Attribute_data_ref_desc& a, const Attribute_data_ref_desc& b) { return a.href == b.href; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("href", href);
  }
};
// looks the same ( AFAIK)
typedef  Attribute_data_ref_desc Geometry_data_ref_desc;
typedef  Attribute_data_ref_desc Texture_data_ref_desc;
typedef  Attribute_data_ref_desc Shared_resource_ref_desc;


//! used for parent, children
struct Node_ref_desc
{
  // --- fields:
  std::string           id;
  std::string           version;
  std::string           href;
  utl::Vec4d            mbs;
  utl::Obb_abs          obb;

  //default:
  Node_ref_desc() : id("-1") {}
  // --- serialization:x
  SERIALIZABLE(Node_ref_desc);

  friend bool operator==(const Node_ref_desc& a, const Node_ref_desc& b) { 
    return a.id == b.id && a.version == b.version && a.href == b.href && a.mbs==b.mbs && a.obb == b.obb; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("id", id);
    ar & utl::opt("version", version, std::string());
    ar & utl::nvp("href", href);
    ar & utl::opt("mbs", utl::seq(mbs));
    ar & utl::opt("obb", obb, utl::Obb_abs());
  }
};

//! 3dNodeIndexDocument.json
struct Legacy_node_desc
{
  // --- fields:
  std::string id;
  int     level = 0;
  std::string version;
  utl::Vec4d  mbs;
  utl::Obb_abs obb;
  std::string created; // obsolete ?
  std::string expires; // obsolete ?
  std::vector< double> transform; //obsolete
  std::vector<Lod_selection_desc> lod_selection;
  std::vector<Feature_ref_desc>  feature_data;
  std::vector<Geometry_data_ref_desc>  geometry_data;
  std::vector<Texture_data_ref_desc>  texture_data;
  Shared_resource_ref_desc      shared_resource;
  Node_ref_desc   parent_node;
  std::vector< Node_ref_desc > children;
  std::vector< Attribute_data_ref_desc> attribute_data;

  // ---- default:
  Legacy_node_desc() {}
  // --- API:
  // has_texture() is not reliable since older (buggy) 1.4 services may not declare it here, but in the sharedResource JSON. 
  // Pls use get_mesh_texture_info() from the layer instead.
  //bool  has_texture() const {    return texture_data.size() && texture_data.front().href.size();  }
  // --- serialization:
  SERIALIZABLE(Legacy_node_desc);
  //friend bool operator==(const Node_desc& a, const Node_desc& b) { return a.offset_z == b.offset_z; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("id", id); //required
    ar & utl::nvp("level", level); // Pro expects it.
    ar & utl::opt("version", version, std::string());
    ar & utl::opt("mbs", seq(mbs));
    ar & utl::opt("obb", obb, utl::Obb_abs());
    ar & utl::opt("created", created, std::string());
    ar & utl::opt("expires", expires, std::string());
    ar & utl::opt("transform", utl::seq(transform));
#if 0 //_DEBUG
    try {
      ar & utl::nvp("lodSelection", utl::seq(lod_selection)); //required 
    }
    catch (utl::Json_exception&)
    {
      //trying to work around invalid LodDescription
      //only in "forgiving" read mode:
      lod_selection.resize(1);
      ar & utl::nvp("lodSelection", lod_selection[0]); //required 
    }
#else
    ar & utl::nvp("lodSelection", utl::seq(lod_selection)); //required 
#endif
    ar & utl::opt("featureData", utl::seq(feature_data));
    ar & utl::opt("geometryData", utl::seq(geometry_data));
    ar & utl::opt("textureData", utl::seq(texture_data));
    ar & utl::opt("sharedResource", shared_resource, Shared_resource_ref_desc());
    ar & utl::opt("parentNode", parent_node, Node_ref_desc());
    ar & utl::opt("children", utl::seq( children));
    ar & utl::opt("attributeData", utl::seq(attribute_data));
  }
};

typedef Node_id  Node_id_v2;

//! Mesh 1.7
struct Mesh_material_ref_desc
{
  // --- Fields:
  int definition_id=-1;
  int resource_id=-1;
  int texel_count_hint=0;
  // --- 
  SERIALIZABLE(Mesh_material_ref_desc);
  friend bool operator==(const Mesh_material_ref_desc& a, const Mesh_material_ref_desc& b) {
    return a.definition_id == b.definition_id && a.resource_id == b.resource_id && a.texel_count_hint == b.texel_count_hint;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("definition", definition_id);
    ar & utl::nvp("resource", resource_id);
    ar & utl::opt("texelCountHint", texel_count_hint, 0);
  }
};


struct Mesh_geometry_ref_desc
{
  // --- Fields:
  int definition_id = -1;
  int resource_id = -1;
  int vertex_count = 0;
  int feature_count = 0;
  // --- 
  SERIALIZABLE(Mesh_geometry_ref_desc);
  friend bool operator==(const Mesh_geometry_ref_desc& a, const Mesh_geometry_ref_desc& b) { 
    return a.definition_id == b.definition_id && a.resource_id == b.resource_id && a.vertex_count == b.vertex_count && a.feature_count == b.feature_count;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("definition", definition_id);
    ar & utl::nvp("resource", resource_id);
    ar & utl::nvp("vertexCount", vertex_count);
    ar & utl::opt("featureCount", feature_count, 0);
  }
};

struct Mesh_attribute_ref_desc
{
  // --- Fields:
  //int definition_id = -1;
  int resource_id = -1;
  // --- 
  SERIALIZABLE(Mesh_attribute_ref_desc);
  friend bool operator==(const Mesh_attribute_ref_desc& a, const Mesh_attribute_ref_desc& b) {return a.resource_id == b.resource_id; }
  template< class Ar > void serialize(Ar& ar)
  {
    //ar & utl::nvp("definition", definition_id);
    ar & utl::nvp("resource", resource_id);
  }
};

struct Mesh_desc_v17
{
  // --- Fields:
  Mesh_material_ref_desc  material;
  Mesh_geometry_ref_desc  geometry;
  Mesh_attribute_ref_desc attribute;
  // --- 
  SERIALIZABLE(Mesh_desc_v17);
  friend bool operator==(const Mesh_desc_v17& a, const Mesh_desc_v17& b) {
    return a.material == b.material && a.geometry == b.geometry && a.attribute == b.attribute;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("material", material, Mesh_material_ref_desc());
    ar & utl::opt("geometry", geometry, Mesh_geometry_ref_desc());
    ar & utl::opt("attribute", attribute, Mesh_attribute_ref_desc());
  }
};

struct Node_desc_v17
{
  // --- fields:
  int                       index=-1;
  double                    lod_threshold=0.0;
  utl::Obb_abs              obb;
  Mesh_desc_v17             mesh; //only one at version v17
  Shared_resource_ref_desc  shared_resource; // Only used for validation.
  int                       parent_index=-1;
  std::vector< int >        children;

  // --- 
  SERIALIZABLE(Node_desc_v17);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("index", index); //required (only id)
    ar & utl::opt("parentIndex", parent_index, -1);
    ar & utl::opt("lodThreshold", lod_threshold, 0.0);
    ar & utl::nvp("obb", obb );
    //ar & utl::opt("meshes", utl::seq(meshes), std::vector<Mesh_desc_v17>());
    ar & utl::opt("mesh", mesh, Mesh_desc_v17());
    ar & utl::opt("children", utl::seq(children));
    ar& utl::opt("sharedResource", shared_resource, Shared_resource_ref_desc()); // legacy, only used for validation. 
  }
};

struct Node_page_desc_v17
{
  // --- fields:
  std::vector< Node_desc_v17 > nodes;
  //----
  SERIALIZABLE(Node_page_desc_v17);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("nodes", utl::seq(nodes));
  }
};

}//endof i3s

} // namespace i3slib
