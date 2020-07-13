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
#include "i3s/i3s_layer_dom.h"
#include "utils/utl_i3s_resource_defines.h"
#include "utils/utl_basic_tracker_api.h"
#include <filesystem>
#include <stdint.h>
#include <algorithm>
#include <ios>
#include <set>
#include <map>


// ----------------- Validation MACROS: 

#define I3S_EXPECTS( obj, expr, obj_path, expected, trk ) \
if (obj.expr != expected) \
  return utl::log_error(trk, IDS_I3S_EXPECTS, std::string( std::string(obj_path)+ "."+ #expr ), expected, obj.expr); \


#define I3S_EXPECTS_ENUM( obj, expr, obj_path, expected, trk ) \
if (obj.expr != expected) \
  return utl::log_error(trk, IDS_I3S_EXPECTS, (std::string( obj_path )+ "."+ #expr ), to_string( expected ), to_string(obj.expr)); \

#define I3S_EXPECTS_NULL( obj, expr, obj_path, trk ) \
if (obj.expr != decltype(obj.expr)::Not_set )\
  return utl::log_error(trk, IDS_I3S_EXPECTS, (std::string( obj_path )+ "."+ #expr  ), std::string("<null>"), to_string(obj.expr)); \

//--------------------------------------------

namespace i3slib
{

namespace i3s
{
const char* Elevation_info::c_abs_height = "absoluteHeight";
//const char* Elevation_info::c_abs_height = "relativeToGround"; // testing only
const char* Elevation_info::c_on_the_ground = "onTheGround";

size_t          size_of(Type f)
{
  static const int c_bytes[(int)Type::_count] =
  {
    1, //"int8",
    1, //"uint8",
    2, //"int16",
    2, //"uint16",
    4, //"int32",
    4, //"uint32",
    4, //"oid32",
    8, //"int64",
    8, //"uint64",
    8, //"oid64",
    4,//"float32",
    8,//"float64"
    0, //string
    0, //date
  };
  if ((uint32_t)f < (uint32_t)Type::_count)
    return c_bytes[(int)f];
  else
  {
    I3S_ASSERT(false);
    return  0;
  }
}

std::string     to_string(Image_format enc)
{
  //static const int c_count = 6;
  //static const char* c_txt[c_count] = { "jpg", "png", "dds", "ktx" };
  switch (enc)
  {
    case Image_format::Jpg: return "jpg";
    case Image_format::Png: return "png";
    case Image_format::Ktx: return "ktx-etc2";
    case Image_format::Dds: return "dds";
    default:
      I3S_ASSERT(false);
      return "";
  }
}

bool            from_string(const std::string& txt_utf8, Image_format* out)
{
  static const int c_count = 5;
  static const char*  c_txt[c_count] = { "jpg", "png", "dds", "ktx", "ktx-etc2" };
  static const Image_format c_val[c_count] = { Image_format::Jpg,Image_format::Png, Image_format::Dds, Image_format::Ktx, Image_format::Ktx };
  for (int i = 0; i < c_count; ++i)
  {
    if (strcmp(txt_utf8.c_str(), c_txt[i]) == 0)
    {
      *out = c_val[i];
      return true;
    }
  }
  return false;
}
//! we only support a subset:
//! - Normal, color, region may be missing, but feature_id/face range must be there. 
bool validate_geometry_def(Layer_type layer_type, const Geometry_schema_desc& desc, Version ver, const std::vector< Geometry_definition_desc>& v17, utl::Basic_tracker* trk)
{
  bool has_point_topology = (layer_type == Layer_type::Point) || (layer_type == Layer_type::Point_cloud);
  if (desc.geometry_type == Mesh_topology::Triangles && has_point_topology)
  {
    return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("defaultGeometrySchema.topology"), to_string(Mesh_topology::Points)
                                     , to_string(desc.geometry_type));
  }
  else if(desc.geometry_type == Mesh_topology::Points && !has_point_topology)
  {
    return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("defaultGeometrySchema.topology"), to_string(Mesh_topology::Triangles)
      , to_string(desc.geometry_type));
  }

  if (layer_type != Layer_type::Point)
  {
    I3S_EXPECTS_ENUM(desc, topology, "defaultGeometrySchema", Legacy_topology::Per_attribute_array, trk);
    if (desc.feature_attrib.id.values_per_element)
    {
      const auto base = "defaultGeometrySchema.featureAttributes";
      const auto& obj = desc.feature_attrib;
      I3S_EXPECTS_ENUM(obj, id.value_type, base, Type::UInt64, trk);
      I3S_EXPECTS(obj, id.values_per_element, base, 1, trk);
      I3S_EXPECTS_ENUM(obj, face_range.value_type, base, Type::UInt32, trk);
      I3S_EXPECTS(obj, face_range.values_per_element, base, 2, trk);
      //if (desc.vertexAttributes.region.values_per_element != 4)
      //  return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("defaultGeometrySchema.vertexAttributes.region"), 4, desc.vertexAttributes.region.values_per_element);
      //if (desc.vertexAttributes.region.value_type != Type::UInt16)
      //  return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("defaultGeometrySchema.vertexAttributes.region"), to_string(Type::UInt16), to_string(desc.vertexAttributes.region.value_type));
    }

    if (layer_type != Layer_type::Point_cloud)
    {
      I3S_EXPECTS(desc.hdrs, size(), "defaultGeometrySchema.headers", 2, trk);
      I3S_EXPECTS_ENUM(desc.hdrs[0], prop, "defaultGeometrySchema.headers[0]", Geometry_header_property::Vertex_count, trk);
      I3S_EXPECTS_ENUM(desc.hdrs[1], prop, "defaultGeometrySchema.headers[1]", Geometry_header_property::Feature_count, trk);
      {
        const auto base = "defaultGeometrySchema.vertexAttributes.position";
        I3S_EXPECTS_ENUM(desc.vertex_attributes.position, value_type, base, Type::Float32, trk);
        I3S_EXPECTS(desc.vertex_attributes.position, values_per_element, base, 3, trk);
      }
      if (desc.vertex_attributes.normal.values_per_element)
      {
        const auto base = "defaultGeometrySchema.vertexAttributes.normal";
        const auto& obj = desc.vertex_attributes.normal;
        I3S_EXPECTS_ENUM(obj, value_type, base, Type::Float32, trk);
        I3S_EXPECTS(obj, values_per_element, base , 3, trk);
      }
      else
      {
        utl::log_warning( trk,  IDS_I3S_MISSING_PROPERTY_COMPATIBILITY_WARNING
          , std::string("[geometries/0] : vertex normals"));
      }
      if (desc.vertex_attributes.uv0.values_per_element)
      {
        const auto base = "defaultGeometrySchema.vertexAttributes.uv0";
        const auto& obj = desc.vertex_attributes.uv0;
        I3S_EXPECTS_ENUM(obj, value_type, base, Type::Float32, trk);
        I3S_EXPECTS(obj, values_per_element, base, 2, trk);
      }
      else
      {
        utl::log_warning(trk,  IDS_I3S_MISSING_PROPERTY_COMPATIBILITY_WARNING
          , std::string("[geometries/0] : texture coordinates"));
      }
      if (desc.vertex_attributes.color.values_per_element) {
        const auto base = "defaultGeometrySchema.vertexAttributes.color";
        const auto& obj = desc.vertex_attributes.color;
        I3S_EXPECTS_ENUM(obj, value_type, base, Type::UInt8, trk);
        I3S_EXPECTS(obj, values_per_element, base, 4, trk);
      }
      else
      {
        utl::log_warning(trk,  IDS_I3S_MISSING_PROPERTY_COMPATIBILITY_WARNING
          , std::string("[geometries/0] : Vertex colors"));
      }
      if (desc.vertex_attributes.region.values_per_element) {
        const auto base = "defaultGeometrySchema.vertexAttributes.region";
        const auto& obj = desc.vertex_attributes.region;
        I3S_EXPECTS_ENUM(obj, value_type, base, Type::UInt16, trk);
        I3S_EXPECTS(obj, values_per_element, base, 4, trk);
      }
    }
    else 
    {
      //Point cloud
      const auto base = "defaultGeometrySchema.vertexAttributes.position";
      I3S_EXPECTS_ENUM(desc.vertex_attributes.position, value_type, base, (ver == Version(2,0) ? Type::Float64 : Type::Int32), trk);
      I3S_EXPECTS(desc.vertex_attributes.position, values_per_element, base, 3, trk);
    }
  }

  if (Version(1, 6) < ver && ver < Version(2, 0))
  {
    //Buffer 0 description must match:
    if( v17.size() <1 )
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("length( layer.geometryDefinitions)"), 1, 0);
    int loop = 0;
    for (auto& def : v17)
    {
      std::string base = "layer.geometryDefinitions[" + std::to_string(loop) + "]";
      if( def.geoms.size() < 1 )
        return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("length(" + base+")"), 1, 0);
      
      if (layer_type != Layer_type::Point)
      {
        for (int k = 0; k < def.geoms.size();++k)
        {
          base = "layer.geometryDefinitions[" + std::to_string(loop) + "].geometryBuffers[" + std::to_string(k) + "]";
          auto& bd = def.geoms[k];
          if (bd.compressed.is_valid() && k != 1) // Compressed geometry buffer must be on index 1
            return utl::log_error(trk, IDS_I3S_INVALID_COMPRESSED_GEOMETRY_INDEX,1 , k );

          if (!bd.compressed.is_valid() && k != 0) // Uncompressed geometry buffer must be on index 0
            return utl::log_error(trk, IDS_I3S_INVALID_UNCOMPRESSED_GEOMETRY_INDEX, 0, k);

          if (bd.compressed.is_valid())
          {
            //compressed must be the only attribute:
            //I3S_EXPECTS(bd, offset, base, 0, trk);
            I3S_EXPECTS_NULL(bd, position.type, base, trk);
            I3S_EXPECTS_NULL(bd, normal.type, base, trk);
            I3S_EXPECTS_NULL(bd, uv0.type, base, trk);
            I3S_EXPECTS_NULL(bd, color.type, base, trk);
            I3S_EXPECTS_NULL(bd, uv_region.type, base, trk);
            I3S_EXPECTS_NULL(bd, feature_id.type, base, trk);
            I3S_EXPECTS_NULL(bd, face_range.type, base, trk);
          }
          else
          {
            I3S_EXPECTS_ENUM(bd, compressed.encoding, base, Compressed_geometry_format::Not_init, trk);
            //must match the legacy definitions:
            I3S_EXPECTS_ENUM(bd, position.type, base, Type::Float32, trk);
            I3S_EXPECTS(bd, offset, base, 8, trk);
            I3S_EXPECTS(bd, position.component, base, 3, trk);
            I3S_EXPECTS_ENUM(bd, position.binding, base, VB_Binding::Per_vertex, trk);

            if (bd.normal.component > 1)
            {
              I3S_EXPECTS_ENUM(bd, normal.type, base, desc.vertex_attributes.normal.value_type, trk);
              I3S_EXPECTS(bd, normal.component, base, desc.vertex_attributes.normal.values_per_element, trk);
              I3S_EXPECTS_ENUM(bd, normal.binding, base, VB_Binding::Per_vertex, trk);
            }
            if (bd.uv0.component > 1)
            {
              I3S_EXPECTS_ENUM(bd, uv0.type, base, desc.vertex_attributes.uv0.value_type, trk);
              I3S_EXPECTS(bd, uv0.component, base, desc.vertex_attributes.uv0.values_per_element, trk);
              I3S_EXPECTS_ENUM(bd, uv0.binding, base, VB_Binding::Per_vertex, trk);
            }
            if (bd.color.component > 1)
            {
              I3S_EXPECTS_ENUM(bd, color.type, base, desc.vertex_attributes.color.value_type, trk);
              I3S_EXPECTS(bd, color.component, base, desc.vertex_attributes.color.values_per_element, trk);
              I3S_EXPECTS_ENUM(bd, color.binding, base, VB_Binding::Per_vertex, trk);
            }
            if (bd.uv_region.component > 1)
            {
              I3S_EXPECTS_ENUM(bd, uv_region.type, base, desc.vertex_attributes.region.value_type, trk);
              I3S_EXPECTS(bd, uv_region.component, base, desc.vertex_attributes.region.values_per_element, trk);
              I3S_EXPECTS_ENUM(bd, uv_region.binding, base, VB_Binding::Per_vertex, trk);
            }
            I3S_EXPECTS_ENUM(bd, feature_id.type, base, desc.feature_attrib.id.value_type, trk);
            I3S_EXPECTS(bd, feature_id.component, base, desc.feature_attrib.id.values_per_element, trk);
            I3S_EXPECTS_ENUM(bd, face_range.type, base, desc.feature_attrib.face_range.value_type, trk);
            I3S_EXPECTS(bd, face_range.component, base, desc.feature_attrib.face_range.values_per_element, trk);
            if (bd.feature_id.is_valid())
            {
              I3S_EXPECTS_ENUM(bd, feature_id.binding, base, VB_Binding::Per_feature, trk);
              I3S_EXPECTS_ENUM(bd, face_range.binding, base, VB_Binding::Per_feature, trk);
            }
          }
        }
        ++loop;
      }
      else
      {
        // Point layer
        // Compressed geometry buffer must be only buffer, at index 0
        if (def.geoms.size() != 1)
          return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("length(" + base + ")"), 1, def.geoms.size());
        if (!def.geoms[0].compressed.is_valid())
          return utl::log_error(trk, IDS_I3S_EXPECTS, base, std::string("Compressed geometry buffer"), std::string("Uncompressed geometry buffer"));
      }
    }
  }
  return true;
}

bool validate_attrib_storage_info(const Attribute_storage_info_desc& desc, int index, Version ver, const Attribute_buffer_desc& v17,  utl::Basic_tracker* trk)
{
  std::string base = "layer.attributeStorageInfo[" + std::to_string(index) + "]";
  const bool is_string = desc.attribute_values.value_type == Type::String_utf8;
  bool is_pcsl = false; // TODO; layer type should be pass as argument
  if (desc.header.size())
  {
    if (desc.header.size() != (is_string ? 2 : 1))
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string("length( " + base + ".header)"), (is_string ? 2 : 1), desc.header.size());
    if (desc.header[0].property != Attrib_header_property::Count)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".header[0].count")
                                           , to_string(desc.header[0].property), to_string(Attrib_header_property::Count));
    if (desc.header[0].value_type != Type::UInt32 && desc.header[0].value_type != Type::Int32)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".header[0].valueType")
                                           , to_string(desc.header[0].value_type), to_string(Type::UInt32));
  }
  else
  {
    //TODO  must be point-cloud.
    is_pcsl = true;
  }
  if (is_string)
  {

    if (desc.header[1].property != Attrib_header_property::Attribute_values_byte_count)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".header[1].property")
                                              , to_string(desc.header[1].property), to_string(Attrib_header_property::Attribute_values_byte_count));

    if (desc.header[1].value_type != Type::UInt32 && desc.header[1].value_type != Type::Int32)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".header[1].valueType")
                                              , to_string(desc.header[1].value_type), to_string(Type::UInt32));

    if (desc.ordering.size() != 2)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".ordering"), 2, desc.ordering.size());
    if (desc.ordering[0] != Attrib_ordering::Attribute_byte_counts)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".ordering[0]"),
                                              to_string(desc.ordering[0]), to_string(Attrib_ordering::Attribute_byte_counts));
    if (desc.ordering[1] != Attrib_ordering::Attribute_values)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".ordering[1]"),
                                              to_string(desc.ordering[1]), to_string(Attrib_ordering::Attribute_values));

    if (desc.attribute_byte_counts.values_per_element != 1)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".attributeByteCounts.values_per_element"), 1, desc.attribute_byte_counts.values_per_element);
    if (desc.attribute_byte_counts.value_type != Type::UInt32  && desc.attribute_byte_counts.value_type != Type::Int32)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".attributeByteCounts.valueType")
                                              , to_string(desc.attribute_byte_counts.value_type), desc.attribute_byte_counts.values_per_element);
    if (desc.attribute_values.values_per_element != 1)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".attributeValues.values_per_element"), 1, desc.attribute_values.values_per_element);

    if (desc.attribute_values.encoding != Value_encoding::Utf8)
      utl::log_warning(trk,  IDS_I3S_EXPECTS, std::string(base + ".attributeValues.encoding"), to_string(Value_encoding::Utf8), to_string(desc.attribute_values.encoding));
  }
  else if(!is_pcsl)
  {
    // non-string scalar type:
    if (desc.ordering.size() != 1)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".ordering"), 1, desc.ordering.size());
    if (desc.ordering[0] != Attrib_ordering::Attribute_values && desc.ordering[0] != Attrib_ordering::Object_ids)
      return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".ordering[0]")
                                              , to_string(Attrib_ordering::Attribute_values), to_string(desc.ordering[0]));
    if (desc.ordering[0] == Attrib_ordering::Object_ids)
    {
      if (desc.object_ids.values_per_element != 1)
      {
        if (Version(1, 6) <= ver) // workaround for some v14 SLPK that can't be added to Pro
          return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".objectIds.valuesPerElement"), 1, desc.object_ids.values_per_element);
      }
      if (!is_integer(desc.object_ids.value_type))
        return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".objectIds.valueType"), std::string("INTEGER_TYPE"), to_string(desc.object_ids.value_type));
    }
    else
    {
      if (desc.attribute_values.values_per_element != 1)
        return utl::log_error(trk, IDS_I3S_EXPECTS, std::string(base + ".attributeValues.valuesPerElement"), 1, desc.attribute_values.values_per_element);
    }
  }
  //if (Version(1, 6) < ver && ver < Version(2, 0))
  //{
  //  base = "attributeDefinitions[" + std::to_string(index) + "]";
  //  I3S_EXPECTS(v17, name, base, desc.name, trk);
  //  I3S_EXPECTS_ENUM(v17, type, base, desc.attribute_values.value_type, trk);
  //  I3S_EXPECTS_ENUM(v17, binding, base, VB_Binding::Per_feature, trk);
  //  int expected_offset = 4 + std::max( 0 , (int)size_of(desc.attribute_values.value_type)-4 );
  //  I3S_EXPECTS(v17, offset, base, expected_offset, trk);
  //}
  // 

  return true;
}

bool check_resource_path_equal(const std::string& what, const std::string& expected)
{
  return utl::to_lower(std::filesystem::path(what).lexically_normal().generic_string()) == expected;
}

//! 
bool validate_legacy_attribute_def(const std::vector< Field_desc>& fields, const std::vector< Attribute_storage_info_desc>& info,
                                   const std::vector< Statistics_href_desc>& stats, Version version, const std::vector<Attribute_buffer_desc>& v17, utl::Basic_tracker* trk)
{
  std::map< std::string, const Attribute_storage_info_desc* > attrib_by_key;
  std::map< std::string, const Attribute_storage_info_desc* > attrib_by_name;
  int loop = 0;

  //bool has_asd = Version(1, 6) < version && version < Version(2, 0);
  //if( has_asd )
  //  I3S_EXPECTS(v17, size(), "attributeDefinitions[]", info.size(), trk);

  for (auto& item : info)
  {
    //check for uniqueness:
    if (!attrib_by_key.emplace(item.key, &item).second)
      return utl::log_error(trk, IDS_I3S_DUPLICATE_ATTRIBUTE_KEY, item.key);
    if (!attrib_by_name.emplace(item.name, &item).second)
      return utl::log_error(trk, IDS_I3S_DUPLICATE_ATTRIBUTE_NAME, item.name);
    //check the stats:
    if (std::find_if(stats.begin(), stats.end(), [&item](const Statistics_href_desc& d) { return d.key == item.key; }) == stats.end()
        && !(item.ordering.size() && item.ordering[0] == Attrib_ordering::Object_ids))
        utl::log_warning(trk, IDS_I3S_MISSING_ATTRIBUTE_STATS_DECL, item.name);

    ++loop;
  }
  loop = 0; 
  Attribute_buffer_desc matching_v17;

  for (auto& item : info)
  {
    //if (has_asd)
    //{
    //  auto found = std::find_if(v17.begin(), v17.end(), [&](const Attribute_buffer_desc& d) { return std::to_string(d.id) == item.key; });
    //  if( found == v17.end())
    //    return utl::log_error(trk, IDS_I3S_MISSING_ATTRIBUTE_SET_DECL, item.key);
    //  matching_v17 = *found;
    //}
    //else
    //  matching_v17 = Attribute_buffer_desc();

    if (!validate_attrib_storage_info(item, loop, version, matching_v17, trk))
      return false;
    ++loop;
  }
  for (auto& st : stats)
  {
    if (attrib_by_key.find(st.key) == attrib_by_key.end() || attrib_by_name.find(st.name) == attrib_by_name.end())
      utl::log_warning(trk, IDS_I3S_STATS_DECL_UNKNOWN_ATTRIBUTE, st.key, st.name);
    std::string expected_paths[2] = { "statistics/" + st.key, "statistics/" + st.key + "/0" };
    if (!check_resource_path_equal(st.href, expected_paths[0]) && !check_resource_path_equal(st.href, expected_paths[1]))
      utl::log_warning(trk, IDS_I3S_PATH_COMPATIBILITY_WARNING, std::string("layer.statisticsInfo[]"), st.href, expected_paths[1]);
  }
  //check fields:
  for (auto& fd : fields)
  {
    if (attrib_by_name.find(fd.name) == attrib_by_name.end())
      utl::log_warning(trk, IDS_I3S_MISSING_ATTRIBUTE_STORAGE_DECL, fd.name);
  }
  return true;
}


bool Version::parse(const std::string& v)
{
  int major = 0;
  int minor = 0;
  int* what = &major;
  for (int i = 0; i < v.size(); ++i)
  {
    if (v[i] == '.')
    {
      if (what != &major)
        return false; // two dots!
      what = &minor;
      continue;
    }
    if (v[i] < '0' || v[i] > '9')
      return false; // not an number. (don't support exponent notation for version ;)
    *what = (*what) * 10 + (v[i] - '0');
  }
  if (major > 255 || minor > 255)
    return false;
  m_code = (major << 8) | minor;
  return true;
}

std::string   Layer_desc::to_json() const
{
  return utl::to_json(*this);
}

}//endof ::i3s

} // namespace i3slib
