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
#include "utils/utl_spatial_reference.h"
#include "i3s/i3s_material_dom.h"
#include "i3s/i3s_mesh_dom.h"
#include <stdint.h>

namespace i3slib
{

namespace i3s
{
struct Version;
struct Store_desc;
struct Field_desc;
struct Attribute_storage_info_desc;
struct Header_desc;
struct Typed_array_desc;
struct Statistics_href_desc;
struct Height_model_info_desc;


struct Last_update_desc
{
  // --- fields:
  int64_t last_update=0;
  // --- serialization:
  SERIALIZABLE(Last_update_desc);
  friend bool operator==(const Last_update_desc& a, const Last_update_desc&b) { return a.last_update == b.last_update; }
  template< class Ar> void serialize(Ar& ar)
  {
    ar & utl::nvp("lastUpdate", last_update);
  }
};

struct Elevation_info
{
  // --- fields:
  double offset_z = 0.0;
  std::string mode = c_abs_height;
  // --- serialization:
  SERIALIZABLE(Elevation_info);
  static const char* c_abs_height, *c_on_the_ground;
  friend bool operator==(const Elevation_info& a, const Elevation_info& b) { return a.offset_z == b.offset_z && a.mode == b.mode; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("mode", mode, std::string(c_abs_height));
    ar & utl::opt("offset", offset_z, 0.0);
    //TODO: There's more to parse. see webspecs. Since we don't use it here for now, let's use a Unparsed_node instead.
  }
};

struct Field_desc
{
  // --- fields:
  std::string name;
  Esri_field_type type{ Esri_field_type::Not_set };
  std::string alias;
  // --- serialization:
  SERIALIZABLE(Field_desc);
  friend bool operator==(const Field_desc& a, const Field_desc& b) { return a.name == b.name && a.type == b.type && a.alias == b.alias; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("name", name); //required
    ar & utl::nvp("type", utl::enum_str(type)); //required
    ar & utl::opt("alias", alias, std::string()); //required ?
  }
};



struct Typed_array_desc
{
  // --- Fields:
  Type       value_type{ Type::Not_set };
  int             values_per_element{ 0 };
  Value_encoding  encoding = Value_encoding::Not_set;
  // --- serialization:
  SERIALIZABLE(Typed_array_desc);
  friend bool operator==(const Typed_array_desc& a, const Typed_array_desc& b) { return a.value_type == b.value_type && a.encoding == b.encoding && a.values_per_element == b.values_per_element; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("valueType", utl::enum_str(value_type)); 
    ar & utl::nvp("valuesPerElement", values_per_element);
    ar & utl::opt("encoding", encoding, Value_encoding::Not_set);
  }
};


struct Header_desc
{
  // --- fields:
  Attrib_header_property property;
  Type value_type;
  // --- serialization:
  SERIALIZABLE(Header_desc);
  friend bool operator==(const Header_desc& a, const Header_desc& b) { return a.property == b.property && a.value_type == b.value_type; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("property", property); //required
    ar & utl::nvp("valueType", utl::enum_str(value_type)); //required
  }
};


struct Attribute_storage_info_desc
{
  // --- fields:
  std::string key;
  std::string name;
  std::vector< Header_desc > header;
  std::vector< Attrib_ordering > ordering;
  Typed_array_desc           object_ids;
  Typed_array_desc           attribute_byte_counts;
  Typed_array_desc           attribute_values;
  //std::string                encoding; //pcsl only
  Attribute_storage_info_encoding encoding = Attribute_storage_info_encoding::Not_set;
  // ----
  friend bool operator==(const Attribute_storage_info_desc& a, const Attribute_storage_info_desc& b) 
  {
    return a.key == b.key && a.name == b.name && a.header == b.header && a.ordering == b.ordering && a.object_ids == b.object_ids 
      && a.attribute_byte_counts == b.attribute_byte_counts && a.attribute_values == b.attribute_values && a.encoding == b.encoding; 
  }
  SERIALIZABLE(Attribute_storage_info_desc)
    template< class Ar >void serialize(Ar& ar) {
    ar & utl::nvp("key", key); //required
    ar & utl::nvp("name", name); //required
    ar & utl::opt("header", utl::seq(header));
    ar & utl::opt("ordering", utl::seq(ordering)); //required
    ar & utl::opt("objectIds", object_ids, Typed_array_desc());
    ar & utl::opt("attributeByteCounts", attribute_byte_counts, Typed_array_desc());
    ar & utl::opt("attributeValues", attribute_values, Typed_array_desc());
    ar & utl::opt("encoding", utl::enum_str(encoding), Attribute_storage_info_encoding::Not_set);
  }
};


struct Height_model_info_desc
{
  // --- fields:
  Height_model height_model = Height_model::Not_set;
  std::string vert_crs;
  Height_unit height_unit = Height_unit::Not_set;

  // --- serialization:
  SERIALIZABLE(Height_model_info_desc);
  friend bool operator==(const Height_model_info_desc& a, const Height_model_info_desc& b) {
    return a.height_model == b.height_model && a.vert_crs == b.vert_crs && a.height_unit == b.height_unit; 
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("heightModel", utl::enum_str(height_model), Height_model::Not_set);
    ar & utl::opt("vertCRS", vert_crs, std::string()); 
    ar & utl::opt("heightUnit", utl::enum_str(height_unit), Height_unit::Not_set);
    // --- deal with osolete field name aliases:
    std::string obsolete;
    ar & utl::opt("geoid", obsolete, std::string());
    if (obsolete.size() && vert_crs.empty())
      vert_crs = obsolete;
    obsolete.clear();
    ar & utl::opt("ellipsoid", obsolete, std::string());
    if (obsolete.size() && vert_crs.empty())
      vert_crs = obsolete;
  }
};


struct Statistics_href_desc
{
  // --- Fields
  std::string key;
  std::string name;
  std::string href;
  // ---
  // --- serialization:
  SERIALIZABLE(Statistics_href_desc);
  friend bool operator==(const Statistics_href_desc& a, const Statistics_href_desc& b) { return a.key == b.key && a.name == b.name && a.href == b.href; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("key", key); //required
    ar & utl::nvp("name", name); //required
    ar & utl::nvp("href", href); //required
  }
};


struct Binary_header_desc {
  // --- fields:
  Geometry_header_property prop = Geometry_header_property::Vertex_count;
  Type type = Type::UInt32;
  // ---- 
  friend bool operator==(const Binary_header_desc& a, const Binary_header_desc& b) {
    return a.prop == b.prop && a.type == b.type;
  }
  SERIALIZABLE(Binary_header_desc);
  template< class Ar> void serialize(Ar& ar)
  {
    ar & utl::nvp("property", utl::enum_str( prop));
    ar & utl::nvp("type", utl::enum_str(type));
  }
};
struct Vertex_attributes_desc
{
  // --- Fields:
  Typed_array_desc position;
  Typed_array_desc normal;
  Typed_array_desc uv0;
  Typed_array_desc color;
  Typed_array_desc region;
  //---- 
  friend bool operator==(const Vertex_attributes_desc& a, const Vertex_attributes_desc& b) {
    return a.position == b.position && a.normal == b.normal && a.uv0 == b.uv0 && a.color == b.color && a.region == b.region;  
  }
  SERIALIZABLE(Vertex_attributes_desc);
  template< class Ar >void serialize(Ar& ar) {
    using namespace utl;
    ar & nvp("position", position);
    ar & opt("normal", normal, Typed_array_desc());
    ar & opt("uv0", uv0, Typed_array_desc());
    ar & opt("color", color, Typed_array_desc());
    ar & opt("region", region, Typed_array_desc());
  }
};

struct Feature_attributes_desc
{
  // --- fields:
  Typed_array_desc id = { Type::UInt64, 1 };
  Typed_array_desc face_range = { Type::UInt32, 2};
  //---- 
  SERIALIZABLE(Feature_attributes_desc);
  friend bool operator==(const Feature_attributes_desc& a, const Feature_attributes_desc& b) {
    return a.id == b.id && a.face_range == b.face_range;
  }

  template< class Ar >void serialize(Ar& ar) {
    using namespace utl;
    ar & utl::nvp("id", id);//TBD
    ar & utl::nvp("faceRange", face_range); //TBD
  }
};

struct Geometry_schema_desc
{
  // --- Fields:
  Mesh_topology geometry_type= Mesh_topology::Triangles;
  Legacy_topology topology = Legacy_topology::Per_attribute_array;
  //std::string encoding; //TBD: what enum is supported ?
  std::vector< Binary_header_desc > hdrs;
  Vertex_attributes_desc vertex_attributes;
  std::vector< Vertex_attrib_ordering > orderings;
  std::vector< Feature_attrib_ordering > feature_attrib_order = { Feature_attrib_ordering::Fid, Feature_attrib_ordering::Face_range };
  Feature_attributes_desc   feature_attrib;

  friend bool operator==(const Geometry_schema_desc& a, const Geometry_schema_desc& b) {
    return a.geometry_type == b.geometry_type && a.topology == b.topology && a.hdrs == b.hdrs && a.vertex_attributes == b.vertex_attributes
      && a.orderings == b.orderings && a.feature_attrib_order == b.feature_attrib_order && a.feature_attrib == b.feature_attrib;
  }
  // --- 
  SERIALIZABLE(Geometry_schema_desc);
  template< class Ar >void serialize(Ar& ar)
  {
    ar & utl::opt("geometryType", utl::enum_str(geometry_type), Mesh_topology::Triangles);
    ar & utl::nvp("header", utl::seq(hdrs));
    ar & utl::nvp("topology", utl::enum_str(topology));
    ar & utl::nvp("ordering", utl::seq(orderings));
    //ar & utl::opt("encoding", encoding, std::string());
    ar & utl::nvp("vertexAttributes", vertex_attributes);
    // NOTE: the following 2 fields should be required, but some 1.6 SLPK do not and we need to keep this behavior
    ar& utl::opt("featureAttributeOrder", utl::seq(feature_attrib_order), std::vector< Feature_attrib_ordering >({ Feature_attrib_ordering::Fid, Feature_attrib_ordering::Face_range })
    , utl::Serialize_field_mode::Optional_always_write);
    ar & utl::opt("featureAttributes", feature_attrib, Feature_attributes_desc(), utl::Serialize_field_mode::Optional_always_write); //Pro requires at least one feature
  }
};


struct Pcsl_paged_index_desc
{
  //--- fields:
  uint32_t        nodes_per_page{0}; //always 64!
  Bounding_volume_type  bounding_volume_type{ Bounding_volume_type::Obb};
  int         node_version{ 1 };
  Lod_metric_type lod_mode{ Lod_metric_type::Effective_density};
  // ---- 
  friend bool operator==(const Pcsl_paged_index_desc& a, const Pcsl_paged_index_desc& b) {
    return a.nodes_per_page == b.nodes_per_page && a.bounding_volume_type == b.bounding_volume_type && a.node_version == b.node_version && a.lod_mode == b.lod_mode;
  }
  SERIALIZABLE(Pcsl_paged_index_desc);
  //Pcsl_paged_index_desc() : nodes_per_page(64), bounding_volume_type("obb"), node_version(1), lod_mode("density-threshold") {}
  template< class Ar >void  serialize(Ar& ar) {
    ar& utl::nvp("nodeVersion", node_version);
    ar& utl::opt("boundingVolumeType", utl::enum_str( bounding_volume_type ), Bounding_volume_type::Obb, utl::Serialize_field_mode::Optional_always_write);
    if ((ar.version() & 0xff00) == 0x0100)
    {
      ar& utl::nvp("nodePerIndexBlock", nodes_per_page);
      std::string href("./nodepages");
      ar& utl::opt("href", href, std::string());
    }
    else
    {
      ar& utl::nvp("nodesPerPage", nodes_per_page);
      ar& utl::opt("lodSelectionMetricType", utl::enum_str(lod_mode), Lod_metric_type::Effective_density, utl::Serialize_field_mode::Optional_always_write);
    }
  };
};

struct Store_desc
{
  // --- fields:
  std::string id;
  std::string profile;
  std::vector< std::string > resource_pattern;
  std::string root_node;
  utl::Vec4d  extent;
  std::string index_crs;
  std::string vertex_crs;
  Normal_reference_frame normal_reference_frame= Normal_reference_frame::Not_set;
  std::string nid_encoding; //obsolete?
  std::string feature_encoding; //obsolete?
  std::string attribute_encoding;
  std::vector< std::string > texture_encodings;
  std::string lod_type;
  std::string lod_model;
  std::string version;
  int  index_page_size=1;
  Geometry_schema_desc geometry_schema;
  double z_factor_internal=1.0;
  // --- fields (PCSL specific):
  Pcsl_paged_index_desc     pcsl_paged_index_desc;

  //--- default:
  Store_desc() : profile("points"), resource_pattern({ "3dNodeIndexDocument", "Attributes", "featureData" }), root_node("./nodes/root"), attribute_encoding("application/octet-stream; version=1.7")
    , lod_type("AutoThinning"), lod_model("node-switching") {}
  // --- serialization:
  SERIALIZABLE(Store_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("id", id, std::string());
    ar & utl::nvp("profile", profile); //required
    ar & utl::nvp("version", version); //required
    ar & utl::opt("resourcePattern", utl::seq(resource_pattern));
    ar & utl::opt("rootNode", root_node, std::string());
    ar & utl::opt("extent", seq(extent));
    ar & utl::opt("indexCRS", index_crs, std::string());
    ar & utl::opt("vertexCRS", vertex_crs, std::string());
    ar & utl::opt("zFactor", z_factor_internal, 1.0);
    ar & utl::opt("nidEncoding", nid_encoding, std::string());
    //some SLPKs write out normalReferenceFrame: "null"
    ar.push_suppressed_error_mask((utl::Json_exception::Error_flags_t)utl::Json_exception::Error::Unknown_enum);
    ar & utl::opt("normalReferenceFrame", utl::enum_str(normal_reference_frame), Normal_reference_frame::Not_set);
    if (ar.pop_suppressed_error_mask())
      normal_reference_frame = Normal_reference_frame::Not_set;
    ar & utl::opt("featureEncoding", feature_encoding, std::string()); //obsolete!
    ar & utl::opt("attributeEncoding", attribute_encoding, std::string()); //obsolete!
    ar & utl::opt("textureEncoding", utl::seq(texture_encodings));
    ar & utl::opt("lodType", lod_type, std::string());
    ar & utl::opt("lodModel", lod_model, std::string());
    ar & utl::opt("defaultGeometrySchema", geometry_schema, Geometry_schema_desc());
    ar& utl::opt("nodesPerIndexPage", index_page_size, 1);
    ar& utl::opt("index", pcsl_paged_index_desc, Pcsl_paged_index_desc());
  }

};

//! Mesh v1.7:
struct Node_pages_desc
{
  // --- fields:
  int             nodes_per_page=1;
  int             root_index=0;
  Lod_metric_type lod_metric_type = Lod_metric_type::Max_screen_size; //TODO: reduce to v2 supported  enums (ideally ONE!)
  // --- serialization:
  friend bool operator==(const Node_pages_desc& a, const Node_pages_desc& b) { return a.nodes_per_page == b.nodes_per_page && a.lod_metric_type== b.lod_metric_type; }
  SERIALIZABLE(Node_pages_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("nodesPerPage", nodes_per_page); //required
    ar & utl::nvp("lodSelectionMetricType", utl::enum_str( lod_metric_type ) ); //required
    ar & utl::opt("rootIndex", root_index, 0);
  }
};

// v1.7+ only
struct Texture_format_desc
{
  // --- field:
  //int           _index=0;
  std::string   name;
  Image_format  format= Image_format::Not_set;
  friend bool   operator==(const Texture_format_desc& a, const Texture_format_desc& b) { return a.name == b.name && a.format == b.format; }

  //std::string   get_name() const { return name.empty() ? std::to_string(_index) : name; }
  

  SERIALIZABLE(Texture_format_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    int index=-1;
    ar & utl::opt("index", index, -1); //deprecated
    ar & utl::opt("name", name, std::string());  // the forward compatible alternative...
    ar & utl::nvp("format", utl::enum_str(format));
    if (index != -1 && name.empty())
      name = to_compatibility_tex_name(format);
  }
};

// Shelved for now. not in v1.7
struct Texture_sampler_desc
{
  // --- fields:
  Texture_filtering_mode min_filter= Texture_filtering_mode::Not_set;
  Texture_filtering_mode mag_filter = Texture_filtering_mode::Not_set;
  Texture_wrap_mode   wrap_u = Texture_wrap_mode::Repeat;
  Texture_wrap_mode   wrap_v = Texture_wrap_mode::Repeat;
  friend bool operator==(const Texture_sampler_desc& a, const Texture_sampler_desc& b) {
    return a.min_filter == b.min_filter && a.mag_filter == b.mag_filter && a.wrap_u == b.wrap_u && a.wrap_v == b.wrap_v; }
  SERIALIZABLE(Texture_sampler_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("magFilter", utl::enum_str(min_filter), Texture_filtering_mode::Not_set);
    ar & utl::opt("magFilter", utl::enum_str(mag_filter), Texture_filtering_mode::Not_set);
    ar & utl::opt("wrapU", utl::enum_str(wrap_u), Texture_wrap_mode::Repeat);
    ar & utl::opt("wrapV", utl::enum_str(wrap_v), Texture_wrap_mode::Repeat);
  }
};


// v1.7+ only
struct Texture_definition_desc
{
  // --- fields:
  std::vector< Texture_format_desc > formats;
  bool is_atlas = false;
  //Texture_sampler_desc  sampler; //NO sampler support yet.
  //Texture_binding binding= Texture_binding::Per_node;
  // --- serialization:
  friend bool operator==(const Texture_definition_desc& a, const Texture_definition_desc& b) 
  {
    return a.formats == b.formats/* && a.sampler == b.sampler*/;
  }
  SERIALIZABLE(Texture_definition_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("formats", utl::seq(formats)); //required
    //ar & utl::opt("sampler", sampler, Texture_sampler_desc()); 
    ar & utl::opt("atlas", is_atlas, false);
  }
};

//! 3DSceneLayer.json (including v17 fields) 
struct Layer_desc
{
  // --- fields:
  int id=-1; // so we know if the layer is valid or not.
  std::string version; //should be UID. 
  std::string name;
  std::string href;
  std::string copyright;
  Layer_type  layer_type =Layer_type::_count;
  geo::SR_def spatial_ref;
  std::string alias;
  std::string description;
  Last_update_desc time_stamp;
  std::vector< std::string > capabilities;
  //Elevation_info elevation_info;
  bool        disable_popup = false;
  Store_desc  store;
  std::vector< Field_desc > fields;
  std::vector< Attribute_storage_info_desc > attribute_storage_info;
  std::vector< Statistics_href_desc>   statistics_info;
  Height_model_info_desc height_model_info;
  utl::Unparsed_field           drawing_info; //pass-thru. don't interpret
  utl::Unparsed_field           popup_info;   //pass-thru. don't interpret
  utl::Unparsed_field           elevation_info;   //pass-thru. don't interpret
  utl::Unparsed_field           cached_drawing_info;   //pass-thru. don't interpret (seems obsolete anyway)

  // mesh v17:
  Node_pages_desc                       node_pages;
  std::vector<Material_desc>            material_defs;
  std::vector< Texture_definition_desc> tex_defs;
  std::vector< Geometry_definition_desc> geom_defs;
  //std::vector< Attribute_set_definition_desc> attrib_defs;
  //Attribute_set_definition_desc  attrib_def;
  std::vector< Attribute_buffer_desc > attrib_defs;
  // --helper api:
  int     get_nodes_per_page() const { return store.index_page_size; }
  I3S_EXPORT std::string   to_json() const;
  // ---- defaults:
  //Layer_desc() : id(0), href("./layers/0"), layer_type("Point"), capabilities({ "View", "Query" }), disable_popup(false) {}

  // --- serialization:
  SERIALIZABLE(Layer_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    static const   geo::SR_def c_sr_wgs84 = { 4326, 4326, -1,-1,std::string() };
    ar & utl::nvp("id", id); // won't publish otherwise.
    I3S_ASSERT(id >= 0);
    ar & utl::opt("version", version, std::string());
    ar & utl::opt("name", name, std::string());
    ar & utl::opt("href", href, std::string());
    ar & utl::nvp("layerType", utl::enum_str(layer_type)); //required
    ar & utl::opt("spatialReference", spatial_ref, c_sr_wgs84, utl::Serialize_field_mode::Optional_always_write); //not required, assumes wgs84 ( see indexCRS, vertexCRS)
    ar & utl::opt("alias", alias, std::string());
    ar & utl::opt("description", description, std::string());
    ar & utl::opt("copyrightText", copyright, std::string());
    ar & utl::opt("serviceUpdateTimeStamp", time_stamp, Last_update_desc());
    ar & utl::nvp("capabilities", utl::seq(capabilities)); //required
    //ar & utl::opt("elevationInfo", elevation_info, Elevation_info());
    ar & utl::opt("disablePopup", disable_popup, false);
    ar & utl::nvp("store", store); //required
    ar & utl::opt("fields", utl::seq(fields)); // optional too(IM may not have it ) -> but specs says its required...
    ar & utl::opt("attributeStorageInfo", utl::seq(attribute_storage_info)); // optional too (IM doesn't have it )
    ar & utl::opt("statisticsInfo", utl::seq(statistics_info)); // TODO: required ?

    // heightModelInfo workaround. some pointcloud slpk write out "heightModelInfo": ""
    if (layer_type == Layer_type::Point_cloud)
    {
      utl::Unparsed_field test;
      ar & utl::opt("heightModelInfo", test, utl::Unparsed_field());
      if (test.raw != "\"\"")
        ar & utl::opt("heightModelInfo", height_model_info, Height_model_info_desc());
    }
    else
      ar & utl::opt("heightModelInfo", height_model_info, Height_model_info_desc());

    ar & utl::opt("cachedDrawingInfo", cached_drawing_info, utl::Unparsed_field());
    ar & utl::opt("drawingInfo", drawing_info, utl::Unparsed_field());
    ar & utl::opt("popupInfo", popup_info, utl::Unparsed_field());
    ar & utl::opt("elevationInfo", elevation_info, utl::Unparsed_field());

    // mesh v.1.7:
    ar & utl::opt("nodePages", node_pages, Node_pages_desc());
    ar & utl::opt("materialDefinitions", utl::seq(material_defs));
    ar & utl::opt("textureSetDefinitions", utl::seq(tex_defs));
    ar & utl::opt("geometryDefinitions", utl::seq(geom_defs)); //required if 1.7 version
    ar & utl::opt("attributeDefinitions", utl::seq( attrib_defs ));
  }
};

struct Service_desc
{
  // --- fields:
  std::string   service_name;
  std::string   service_version;
  std::vector< Layer_desc> layers;
  std::vector< int > layer_ids;
  // --- api:
  SERIALIZABLE(Service_desc);
  template< class Ar > void serialize(Ar& ar)
  { 
    ar & utl::nvp("serviceName", service_name);
    ar & utl::nvp("serviceVersion", service_version);
    ar & utl::nvp("layers", utl::seq(layers)); //should be "opt" TODO: define operator==()
    ar & utl::opt( "layerIds", utl::seq(layer_ids));
  }

};

struct Full_extent
{
  // -- fields:
  geo::SR_def spatial_ref;
  double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0, zmin = 0.0, zmax = 0.0;
  // --- 
  SERIALIZABLE(Full_extent);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("spatialReference", spatial_ref, geo::SR_def());
    ar & utl::nvp("xmin", xmin);
    ar & utl::nvp("xmax", xmax);
    ar & utl::nvp("ymin", ymin);
    ar & utl::nvp("ymax", ymax);
    ar & utl::nvp("zmin", zmin);
    ar & utl::nvp("zmax", zmax);
  };
};

struct Bsl_sublayer_desc
{
  // --- fields:
  // --- fields:
  int id;
  std::string name;
  std::string alias;
  std::string discipline;
  std::string model_name;
  Layer_type layer_type;
  bool    visibility=true;
  std::vector< Bsl_sublayer_desc > sublayers;
  // --- 
  friend bool operator==(const Bsl_sublayer_desc& a, const Bsl_sublayer_desc& b) {
    return a.id == b.id && a.name == b.name && a.discipline == b.discipline && a.model_name == b.model_name && a.layer_type == b.layer_type
      && a.visibility == b.visibility && a.sublayers == b.sublayers;
  }
  SERIALIZABLE(Bsl_sublayer_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("id", id);
    ar & utl::nvp("name", name);
    ar & utl::opt("alias", alias, std::string());
    ar & utl::opt("discipline", discipline, std::string());
    ar & utl::opt("modelName", model_name, std::string());
    ar & utl::nvp("layerType", utl::enum_str(layer_type));
    ar & utl::opt("visibility", visibility, true);
    ar & utl::opt("sublayers", utl::seq(sublayers), std::vector< Bsl_sublayer_desc >());
  }
};

struct Bsl_filter_mode_desc
{
  Bsl_filter_mode type = Bsl_filter_mode::Not_set;
  utl::Unparsed_field edges; //WebScene specs. unparsed for now
  // --- 
  friend bool operator==(const Bsl_filter_mode_desc& a, const Bsl_filter_mode_desc& b) {
    return a.type == b.type && a.edges == b.edges;
  }
  SERIALIZABLE(Bsl_filter_mode_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("type", utl::enum_str(type), Bsl_filter_mode::Not_set);
    ar & utl::opt("edges", edges, utl::Unparsed_field());
  }
};

struct Bsl_filter_block_desc
{
  // --- fields:
  std::string title;
  Bsl_filter_mode_desc filter_mode;
  std::string filter_expr;
  // --- 
  friend bool operator==(const Bsl_filter_block_desc& a, const Bsl_filter_block_desc& b)
  {
    return a.title == b.title && a.filter_expr == b.filter_expr && a.filter_mode == b.filter_mode;
  }
  SERIALIZABLE(Bsl_filter_block_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("title", title);
    ar & utl::nvp("filterMode", filter_mode);
    ar & utl::nvp("filterExpression", filter_expr);
  }
};

struct Bsl_filter_authoring_filter_type_desc
{
  // --- fields:
  std::string filter_type;
  std::vector< std::string > filter_values;
  // --- 
  friend bool operator==(const Bsl_filter_authoring_filter_type_desc& a, const Bsl_filter_authoring_filter_type_desc& b)
  {
    return a.filter_type == b.filter_type && a.filter_values == b.filter_values;
  }
  SERIALIZABLE(Bsl_filter_authoring_filter_type_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("filterType", filter_type);
    ar& utl::nvp("filterValues", utl::seq(filter_values));
  }
};

struct Bsl_filter_authoring_blocks_desc
{
  // --- fields:
  std::vector< Bsl_filter_authoring_filter_type_desc > filter_types;

  // --- 
  friend bool operator==(const Bsl_filter_authoring_blocks_desc& a, const Bsl_filter_authoring_blocks_desc& b)
  {
    return a.filter_types == b.filter_types;
  }
  SERIALIZABLE(Bsl_filter_authoring_blocks_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("filterTypes", utl::seq(filter_types));
  }
};


struct Bsl_filter_authoring_desc
{
  // --- fields:
  std::string type;
  std::vector< Bsl_filter_authoring_blocks_desc > filter_blocks;

  // --- 
  friend bool operator==(const Bsl_filter_authoring_desc& a, const Bsl_filter_authoring_desc& b)
  {
    return a.type == b.type && a.filter_blocks == b.filter_blocks;
  }
  SERIALIZABLE(Bsl_filter_authoring_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("type", type);
    ar& utl::nvp("filterBlocks", utl::seq(filter_blocks));
  }
};

struct Bsl_filter_desc
{
  // --- fields:
  std::string id;
  std::string name;
  std::string description;
  std::vector< Bsl_filter_block_desc > filter_blocks;
  Bsl_filter_authoring_desc filter_authoring_info;
 
  // --- 
  friend bool operator==(const Bsl_filter_desc& a, const Bsl_filter_desc& b)
  {
    return a.id == b.id && a.name == b.name && a.description == b.description && a.filter_blocks == b.filter_blocks && a.filter_authoring_info == b.filter_authoring_info;
  }
  SERIALIZABLE(Bsl_filter_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("id", id);
    ar& utl::nvp("name", name);
    ar& utl::nvp("description", description);
    ar& utl::nvp("filterBlocks", utl::seq(filter_blocks));
    ar& utl::opt("filterAuthoringInfo", filter_authoring_info, Bsl_filter_authoring_desc());
  }
};

struct Bsl_layer_desc
{
  // --- fields:
  int id=-1;
  std::string name;
  std::string version;
  std::string alias;
  Layer_type layer_type = Layer_type::Building;
  std::string description;
  std::string copyright;
  Full_extent extent;
  geo::SR_def spatial_ref;
  Height_model_info_desc height_model_info;
  std::string active_filter_id;
  std::string stats_href;
  std::vector< Bsl_sublayer_desc > sublayers;
  std::vector< Bsl_filter_desc > filters;
  // ---
  bool is_valid() const { return id != -1; }
  SERIALIZABLE(Bsl_layer_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("id", id);
    ar & utl::nvp("name", name);
    ar & utl::nvp("version", version);
    ar & utl::opt("alias", alias, std::string());
    ar & utl::nvp("layerType", layer_type);
    ar & utl::opt("description", description, std::string());
    ar & utl::opt("copyright", copyright, std::string());
    ar & utl::nvp("spatialReference", spatial_ref);
    ar & utl::opt("heightModelInfo", height_model_info, Height_model_info_desc());
    ar & utl::opt("activeFilterId", active_filter_id, std::string());
    ar & utl::opt("statisticsHRef", stats_href, std::string());
    ar & utl::nvp("sublayers", utl::seq(sublayers));
    ar& utl::opt("filters", utl::seq(filters), std::vector< Bsl_filter_desc >());
  }
};

// --------------------------- validation helper: 

struct I3S_EXPORT Version
{
  Version() = default;
  Version(int major, int minor) : m_code((major << 8) | minor) {}
  uint32_t get_code() const { return m_code; }
  bool is_valid() const { return m_code != 0; }
  friend bool operator<(const Version& a, const Version& b) { return a.m_code < b.m_code; }
  friend bool operator<=(const Version& a, const Version& b) { return a.m_code <= b.m_code; }
  friend bool operator>(const Version& a, const Version& b) { return a.m_code > b.m_code; }
  friend bool operator>=(const Version& a, const Version& b) { return a.m_code >= b.m_code; }
  friend bool operator==(const Version& a, const Version& b) { return a.m_code == b.m_code; }
  friend bool operator!=(const Version& a, const Version& b) { return a.m_code != b.m_code; }
  bool parse(const std::string& v);
private:
  uint32_t m_code{ 0 };
};
I3S_EXPORT bool validate_geometry_def(Layer_type layer_type, const Geometry_schema_desc& desc, Version ver, const std::vector< Geometry_definition_desc>& v17def, utl::Basic_tracker* trk);
//bool validate_attrib_storage_info(const Attribute_storage_info_desc& desc, int index, Version version, const std::vector<Attribute_buffer_desc>& v17, utl::Basic_tracker* trk);
I3S_EXPORT bool check_resource_path_equal(const std::string& what, const std::string& expected);
I3S_EXPORT bool validate_legacy_attribute_def(const std::vector< Field_desc>& fields,  const std::vector< Attribute_storage_info_desc>& info,
                                   const std::vector< Statistics_href_desc>& stats, Version version, const std::vector<Attribute_buffer_desc>& v17,  utl::Basic_tracker* trk);


}//endof ::i3s

} // namespace i3slib
