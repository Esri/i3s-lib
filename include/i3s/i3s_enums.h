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
#include "utils/utl_i3s_export.h"
#include <string>

//! -------------------------------------------------------------------------------
//!      Formatting is used to generate to enum <--> string conversion code. 
//! i.e. :
//!    Int8 = 0, // => Int8
//!      Int8 = 0, // => Int8 => integer8 
//! -------------------------------------------------------------------------------

namespace i3slib
{

namespace i3s
{
  // We need the struct until all compilers support https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2593r0.html
  template<class T> struct dependent_false : std::false_type {};

enum class  Alpha_mode : int {
  Opaque = 0, 
  Mask, 
  Blend,
  Not_set,
  _count=Not_set
};

enum class Face_culling_mode : int {
  None = 0,
  Front, 
  Back,
  _count
};

enum class Type : int {
  Int8 = 0, // => Int8
  UInt8, // => UInt8
  Int16, // => Int16
  UInt16, // => UInt16
  Int32, // => Int32
  UInt32, // => UInt32
  Oid32, // => Oid32
  Int64, // => Int64
  UInt64, // => UInt64
  Oid64, // => Oid64
  Float32, // => Float32
  Float64, // => Float64
  String_utf8, // => String
  Date_iso_8601, // => Date
  Global_id, // => GlobalID
  Guid, // => GUID
  Not_set,
  _count = Not_set
};

enum class Domain_type : int {
  CodedValue = 0,// => codedValue
  Range,// => range
  Not_set,
  _count = Not_set
};

enum class Esri_field_type : int {
  Date = 0,// => esriFieldTypeDate
  Single,// => esriFieldTypeSingle
  Double, // => esriFieldTypeDouble
  Guid, // => esriFieldTypeGUID
  Global_id,// => esriFieldTypeGlobalID
  Integer, // => esriFieldTypeInteger => FieldTypeInteger
  Oid, // => esriFieldTypeOID
  Small_integer,// => esriFieldTypeSmallInteger
  String, // => esriFieldTypeString
  Xml, // => esriFieldTypeXML
  Not_set ,
  _count=Not_set
};


enum class Key_value_encoding_type : int {
  Separated_key_values, // => SeparatedKeyValues

  Not_set,
  _count = Not_set
};


enum class Legacy_topology : int { 
  Per_attribute_array = 0, // => PerAttributeArray
  _count 
};

enum class Vertex_attrib_ordering : int { 
  Position = 0, 
  Normal, 
  Uv0, 
  Color, 
  Region, 
  _count 
};
enum class Feature_attrib_ordering : int {
  Fid = 0,  // => id
  Face_range,  // => faceRange
  _count 
};

enum class Geometry_header_property : int { 
  Vertex_count = 0, // => vertexCount
  Feature_count, // => featureCount  => faceCount
  _count 
};

enum class Mesh_topology :int {
  Triangles = 0, // => triangles => triangle
  Lines, 
  Points, 
  _count 
};

enum class Encoding : int { 
  None = 0, 
  String_utf8, 
  _count 
};

enum class Lod_metric_type : int { 
  Max_screen_area, // => maxScreenThresholdSQ
  Max_screen_size, // =>  maxScreenThreshold
  Distance, // => distanceRangeFromDefaultCamera
  Effective_density, // => density-threshold
  Screen_space_relative, // =>  screenSpaceRelative
  Not_set, 
  _count = Not_set 
};

enum class Layer_type :int {
  Mesh_3d = 0, // => 3DObject
  Mesh_IM, // => IntegratedMesh
  Point, // => Point
  Point_cloud,  // => PointCloud
  Building, // => Building
  Voxel, // => Voxel
  Group, // => group
  Not_set,
  _count = Not_set
};

enum class VB_Binding :int { 
  Per_vertex = 0, 
  Per_uv_region,
  Per_feature, 
  _count 
};

//! not used
enum class Texture_filtering_mode : int {
  Nearest = 0, 
  Linear, 
  Nearest_mipmap_nearest, 
  Linear_mipmap_nearest, 
  Nearest_mipmap_linear, 
  Linear_mipmap_linear, 
  Not_set,
  _count = Not_set
};

//! not used
enum class Texture_wrap_mode : int {
  Clamp = 0, 
  Repeat, 
  _count 
};

enum class Normal_reference_frame {
  East_north_up = 0, //normals are stored in a node local reference frame defined by the easting, northing and up directions at the OBB/MBS center, and is only valid for geographic(WGS84)
  Earth_centered, //normals are stored in a global earth-centered, earth-fixed (ECEF) reference frame where the x-axis points towards Prime meridian (lon = 0°) and Equator (lat = 0°), the y-axis points East towards lon = +90 and lat = 0 and the z-axis points North. It is only valid for geographic 
  Vertex_reference_frame, //normals are stored in the same reference frame as vertices and is only valid for projected spatial reference
  Not_set,
  _count = Not_set
};

enum class Compressed_mesh_attribute : int { 
  Position = 0, 
  Normal, 
  Uv0, 
  Color, 
  Uv_region, 
  Feature_index, 
  Flag, 
  _count 
};

enum class Attrib_header_property : int {
  Count = 0,
  Attribute_values_byte_count, // => attributeValuesByteCount
  _count
};

enum class Attrib_ordering : int {
  Attribute_values = 0, // => attributeValues
  Attribute_byte_counts, // =>  attributeByteCounts
  Object_ids, // => ObjectIds => objectIds
  _count
};


enum class Compressed_geometry_format : int {
  Not_init = 0, // => Not_set
  Draco = 1,
  Lepcc = 2
};

enum class Legacy_image_channel : int {
  Rgba = 0,
  Rgb, // => rgb => 
  Not_set,
  _count = Not_set
};

enum class Legacy_wrap_mode : int {
  None,
  Repeat,
  Mirror,
  _count
};

enum class Mime_image_format {
  Jpg = 0, // => image/jpeg => data:image/jpeg
  Png, // => image/png => data:image/png
  Dds, // => image/vnd-ms.dds => data:image/vnd-ms.dds
  Ktx, // => image/ktx => data:image/ktx
  Basis, // => image/basis
  Ktx2, // => image/ktx2
  Not_set,
  _count = Not_set,
};

enum class Legacy_uv_set : int {
  Uv0,
  _count
};

enum class Value_encoding : int {
  Utf8, // => UTF-8
  Not_set,
  _count = Not_set,
};

enum class Time_encoding : int {
  Ecma_iso_8601, // => ECMA_ISO8601
  Not_set,
  _count = Not_set,
};

enum class Attribute_storage_info_encoding : int {
  Embedded_elevation, // => embedded-elevation
  Lepcc_xyz, // => lepcc-xyz
  Lepcc_rgb, // => lepcc-rgb
  Lepcc_intensity, // => lepcc-intensity
  Not_set,
  _count = Not_set,
};

enum  class Bsl_filter_mode : int {
  Solid, // => solid
  Wireframe, // => wireFrame
  Not_set,
  _count = Not_set,
};

enum class Height_model : int {
  Gravity_related, // => gravity_related_height
  Ellipsoidal, // => ellipsoidal
  Orthometric, // => orthometric
  Not_set,
  _count = Not_set
};

enum class Height_unit : int {
  Meter, // => meter => Meter => Meters 
  Us_foot, // => us-foot => foot_us
  Foot, // => foot
  Clarke_foot, // => clarke-foot => foot_clarke
  Clarke_yard, // => clarke-yard => yard_clarke
  Clarke_link, // => clarke-link => link_clarke
  Sears_yard, // => sears-yard => yard_sears
  Sears_foot, // => sears-foot => foot_sears
  Sears_chain, // => sears-chain => chain_sears
  Benoit_chain, // => benoit-1895-b-chain // => chain_benoit_1895_b
  Indian_yard, // => indian-yard => yard_indian
  Indian_1937_yard, // => indian-1937-yard => yard_indian_1937
  Gold_coast_foot, // => gold-coast-foot => foot_gold_coast
  Sears_trunc_chain, // => sears-1922-trunctuated-chain => chain_sears_1922_trunctuated
  Us_inch, // => us-inch => inch_us => inch
  Us_mile, // => us-mile => mile_us
  Us_yard, // => us-yard => yard_us
  Millimeter, // => millimeter
  Decimeter, // => decimeter
  Centimeter, // => centimeter
  Kilometer, // => kilometer
  Not_set,
  _count = Not_set
};

enum class Continuity : int {
  Continuous,
  Discrete, 
  Not_set,
  _count = Not_set
};

enum class Base_quantity : int {
  Horizontal_coordinate,
  Vertical_coordinate,
  Time,
  None,
  Not_set,
  _count = Not_set
};


/*
// keeping these Height_units here in case they're needed in the future
Fathom, // => Fathom
Nautical_mile, // => nautical_mile
German_meter, // => meter_german
Us_chain, // => chain_us
Us_link, // => link_us
Clarke_chain, // => chain_clarke
Sears_link, // => link_sears
Benoit_yard_a, // => yard_benoit_1895_a
Benoit_foot_a, // => foot_benoit_1895_a
Benoit_chain_a, // => chain_benoit_1895_a
Benoit_link_a, // => link_benoit_1895_a
Benoit_yard_b, // => yard_benoit_1895_b
Benoit_foot_b, // => foot_benoit_1895_b
Benoit_link_b, // => link_benoit_1895_b
Foot_1865, // => foot_1865
Indian_foot, // => foot_indian
Indian_1937_foot, // => foot_indian_1937
Indian_1962_foot, // => foot_indian_1962
Indian_1975_foot, // => foot_indian_1975
Indian_1975_yard, // => yard_indian_1975
Statute_mile, // => statute_mile
British_foot_1936, // => foot_british_1936
Chain, // => chain
Link, // => link
Sears_trunc_yard, // => yard_sears_1922_trunctuated
Sears_trunc_foot, // => foot_sears_1922_trunctuated
Sears_trunc_link, // => link_sears_1922_trunctuated
Us_rod, // => rod_us => rod
Us_nautical_mile, // => nautical_mile_us
Uk_nautical_mile, // => nautical_mile_uk
Smoot, // => smoot
Tx_vara, // => vara_tx
Point, // => point
Micrometer, // => micrometer
Nanometer, // => nanometer
Kilometer_50, // => 50_kilometers
Kilometer_150, // => 150_kilometers
*/
enum class Pcsl_attribute_buffer_type : int {
  Elevation = 1,// => ELEVATION
  Intensity = 2,// => INTENSITY
  Color_rgb = 4,// => RGB
  Class_code = 8,// => CLASS_CODE
  Flags = 16,// => FLAGS
  Returns = 32,// => RETURNS
  Point_id = 64,// => POINTID
  User_data = 128,// => USER_DATA
  Point_source_id = 256,// => POINT_SRC_ID
  Gps_time = 512,// => GPS_TIME
  Scan_angle_rank = 1024,// => SCAN_ANGLE
  Near_infrared = 2048,// => NEAR_INFRARED
  Not_set = 0// => INVALID
};

enum class Bounding_volume_type : int {
  Obb,
  Mbs,
  Not_set,
  _count = Not_set
};

enum class Vxl_variable_semantic : int {
  Stc_hot_spot_results,
  Stc_cluster_outlier_results,
  Stc_estimated_bin,
  Generic_nearest_interpolated,
  Not_set,
  _count = Not_set
};

enum class Vertical_exag_mode : int {
  Scale_position,
  Scale_height,
  Not_set,
  _count = Not_set 
};

enum class Rendering_quality : int {
  Low = 0, 
  Medium, 
  High, 
  Custom
};

enum class Interpolation : int {
  Linear = 0, 
  Nearest, 
  Not_set 
};
enum class Vxl_render_mode : int {
  Volume, 
  Surfaces
};

enum class Vxl_rw_stats_status : int {
  Partial = 0,
  Final,
  Not_set, 
};



/*
* If the client has enough resources to do so, it could chose to show more details for high priority content.
*/
enum class Priority : int {
  High,  // => High
  Low  // => Low
};


/*
* For Semantic::Labels, the client is not allowed to scale the LoD metrics
*/
enum class Semantic : int {
  None,  // => None
  Labels // => Labels
};

constexpr Priority c_default_priority{ Priority::High };
constexpr Semantic c_default_semantic{ Semantic::None };

enum class Capability : int {
  View, // => View
  Query, // => Query
  Edit, // => Edit
  Extract, // => Extract
};

} // namespace i3s

namespace utl
{
class Archive_in;
class Archive_out;
}

namespace i3s
{
#include "i3s/i3s_enums_generated.h"
}

} // namespace i3slib
