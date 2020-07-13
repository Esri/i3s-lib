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
#include "i3s/i3s_enums.h"
#include "utils/utl_i3s_export.h"
#include "utils/utl_buffer.h"
#include "utils/utl_geom.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace i3slib
{

namespace utl { class Basic_tracker; }

namespace i3s
{

static const int c_invalid_id = -1;

typedef size_t Node_id;
typedef size_t Layer_id;


typedef int Attrib_index;
typedef int Attrib_schema_id; //always 0 at v1.7
typedef int Texture_id;
typedef int Geometry_id;
typedef utl::Vec4<uint8_t> Rgba8;
typedef utl::Vec4<uint16_t> Uv_region;

I3S_EXPORT size_t     size_of(Type  t);


typedef int Geometry_formats;
enum class Geometry_format : Geometry_formats {
  Not_set = 0,
  Legacy = 1,
  Point_legacy = 2,     // from featureData JSON
  Draco = 4,
  Lepcc = 8
};

typedef uint32_t Image_formats;
// WARNING: do not change the values ( the texture_def index is derived for it to avoid collision when new format are added)
enum class Image_format : Image_formats {
  Not_set = 0,
  Jpg = 1,
  Png = 2,
  Dds = 4,
  Ktx = 8,
  Raw_rgba8 = 16,
  Raw_rgb8 = 32,
  Default = Jpg | Png,
  Raw_uncompressed = Raw_rgba8 | Raw_rgb8,
  Desktop = Default | Dds,
  All_compressed = Desktop | Ktx,
  Not_gpu_compressed = Raw_uncompressed | Jpg | Png
};


I3S_EXPORT bool            from_string(const std::string& txt_utf8, Image_format* out);
I3S_EXPORT std::string     to_string(Image_format enc);

struct Attribute_meta
{
  std::string       key; //<- used to find the attribute buffer resources.
  std::string       name;
  std::string       alias;
};

struct Attribute_definition
{
  Attribute_meta    meta;
  Type              type = Type::Not_set;
  Attribute_storage_info_encoding encoding = Attribute_storage_info_encoding::Not_set;
};


struct Spatial_reference
{
  //! In theory, we should also parse for WTK matching WGS84
  bool is_vanilla_wgs64() const  {  return latest_wkid == 4326 || wkid == 4326; }

  int wkid = -1;
  int latest_wkid = -1;
  int vcs_id = -1;
  int latest_vcs_id = -1;
  std::string wkt;
};

struct Height_model_info
{
  Height_model height_model = Height_model::Not_set;
  std::string vert_crs;
  Height_unit height_unit = Height_unit::Not_set;
};

struct Layer_meta
{
  Layer_type type;
  std::string name;
  std::string desc;
  std::string copyright;
  Spatial_reference sr;
  std::string uid;
  std::string drawing_info, elevation_info, popup_info;
  uint64_t timestamp = 0;
  Normal_reference_frame normal_reference_frame;
  Height_model_info height_model_info;
};


//TODO: rename to avoid confusion with feature fields
typedef  uint32_t Attrib_flags;
enum class Attrib_flag : Attrib_flags {
  Pos = 1, Normal = 2, Uv0 = 4, Uv1 = 16, Color = 32, Region = 64, Feature_id = 128,
  //, Face_range = 256, // Feature_index = 512, //, Region_index = 1024,
  Legacy_no_region = Pos | Normal | Uv0 | Color | Feature_id,
  Legacy_with_region = Legacy_no_region | Region
};

inline bool is_set(Attrib_flags m, Attrib_flag f) { return (m & static_cast<Attrib_flags>(f)) != 0; }

enum class Texture_semantic : int
{
  Base_color = 0,
  Metallic_roughness,
  Diffuse_texture,
  Emissive_texture,
  Normal_map,
  Occlusion_map,
  _count,
  Not_set = _count
};

struct Texture_meta
{
  enum class Alpha_status : int { 
    Not_set = -1, 
    Opaque = 0, 
    Mask = 1, // 1 bit alpha channel 
    Blend = 8,  // 8 bit alpha channel
    Mask_or_blend = -2 };
  //TBD: sRGB, etc ?
  //TBD: Image origin ?
  typedef int wrap_flags_t;
  enum Wrap_mode : wrap_flags_t {
    Not_set = 4,
    None = 0,  //! Regular, not-repeated texture (TBD :means "clamp" )
    Wrap_x = 1,  //! Texture repeats horizontally
    Wrap_y = 2,  //! Texture repeats vertically
    Wrap_xy = Wrap_x | Wrap_y  
  };
  int           mip0_width = -1, mip0_height = -1;
  int           mip_count = -1;
  int           uv_set = 0;
  Alpha_status  alpha_status = Alpha_status::Not_set;
  Wrap_mode     wrap_mode = Wrap_mode::Not_set;
  Image_format  format = Image_format::Not_set;
  bool          is_atlas=false;
  //Texture_semantic semantic = Texture_semantic::Not_set;
};

I3S_EXPORT std::string to_compatibility_tex_name(Image_format f);

struct Texture_buffer
{
  DECL_PTR(Texture_buffer);

  operator bool() const { return  !empty(); }
  bool        empty() const { return  data.data() == nullptr || /*meta.mip0_width <= 0 || meta.mip0_height <= 0 || */data.size() == 0; }
  int         width() const noexcept { return meta.mip0_width; }
  int         height() const noexcept { return meta.mip0_height; }
  utl::Raw_buffer_view    data;
  Texture_meta            meta;
};

typedef std::vector<Texture_buffer> Multi_format_texture_buffer;


// based of GLTF pbr metallic roughness 
struct Metallic_roughness
{
  utl::Vec4f  base_color_factor = utl::Vec4f(1.0);
  float       metallic_factor=1.0f;
  float       roughness_factor=1.0f;
  Multi_format_texture_buffer   base_color_tex; 
  Multi_format_texture_buffer   metallic_roughness_tex; 
};

#if 0
// based of: 
// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness
struct Specular_glossiness
{
  utl::Vec4f diffuse_factor  = { 1.0, 1.0, 1.0, 1.0 };
  utl::Vec3f specular_factor = { 1.0, 1.0, 1.0 };
  float glossiness_factor = 1.0;
  Multi_format_texture_buffer   specular_glossiness_tex;
  Multi_format_texture_buffer   diffuse_texture_tex;
};
#endif

struct Material_properties
{
  //enum class Alpha_mode : int { Opaque, Mask, Alpha };
  Alpha_mode                    alpha_mode = Alpha_mode::Opaque;
  int                           alpha_cut_off = 255; //in [0, 255], below cut-off, pixels are dropped.
  bool                          double_sided = true;
  utl::Vec3f                    emissive_factor = utl::Vec3f(0.0); //TBD...
  Face_culling_mode             cull_face = Face_culling_mode::None;
};

struct Material_data
{
  DECL_PTR(Material_data);
  Material_properties           properties;
  Metallic_roughness            metallic_roughness;
  //Specular_glossiness           specular_glossiness;
  Multi_format_texture_buffer   normal_map_tex;
  std::string                   legacy_json;  // ONLY for conversion.  TBD
};

class Attribute_buffer;
class Geometry_buffer;

struct Mesh_data
{
  std::vector<std::shared_ptr<Attribute_buffer>> attribs;      // in order of the attribute schema for this mesh.
  std::vector<std::shared_ptr<Geometry_buffer>> geometries;    // v1.x: only single geometry supported.
  Material_data                        material;
  std::string                          legacy_feature_data_json;  // ONLY for conversion.  TBD
  std::string                          legacy_feature_data_geom_json;  // ONLY for conversion.  TBD
};

typedef uint32_t Geometry_compression_flags;
//enum class Geometry_compression : Geometry_compression_flags { None = 0, Draco = 1, Lepcc = 2, Default=Draco|Lepcc };
typedef Compressed_geometry_format Geometry_compression;

typedef Image_formats GPU_texture_compression_flags;
enum class GPU_texture_compression : GPU_texture_compression_flags
{
  None = 0,
  DXT_BC_ALL = (uint32_t)Image_format::Dds,
  ETC_2 = (uint32_t)Image_format::Ktx,
  Desktop = DXT_BC_ALL
};

inline void set_gpu_compression(GPU_texture_compression_flags& in_out, GPU_texture_compression flag, bool is_on)
{
  if (is_on)
    in_out |= (GPU_texture_compression_flags)flag;
  else
    in_out &= ~((GPU_texture_compression_flags)flag);
}
inline void set_geom_compression(Geometry_compression_flags& in_out, Geometry_compression flag, bool is_on)
{
  if (is_on)
    in_out |= (Geometry_compression_flags)flag;
  else
    in_out &= ~((Geometry_compression_flags)flag);
}

struct Ctx_properties
{
  std::shared_ptr<utl::Basic_tracker> tracker;
  uint16_t max_texture_size = 4096;
  bool is_drop_normals = false;
  Geometry_compression_flags     geom_encoding_support{ (Geometry_compression_flags)Geometry_compression::Lepcc };
  Geometry_compression_flags     geom_decoding_support{ (Geometry_compression_flags)Geometry_compression::Lepcc };
  GPU_texture_compression_flags  gpu_tex_encoding_support{ (GPU_texture_compression_flags)GPU_texture_compression::None }; // writer contex only. What to encode.
  GPU_texture_compression_flags  gpu_tex_rendering_support{ (GPU_texture_compression_flags)GPU_texture_compression::Desktop }; // rendering only (reader context only)
};

class Mesh_abstract;

struct Context
{
  DECL_PTR(Context);
  typedef std::function<bool(const utl::Raw_buffer_view& raw_img, Texture_buffer* out)> Decode_img_fct;
  typedef std::function<std::shared_ptr<Mesh_abstract>(const utl::Vec3d & origin, const utl::Raw_buffer_view & buffer)> Decode_mesh_fct;
  typedef std::function<bool(const utl::Raw_buffer_view& jpeg, int* w, int* h)> Get_img_size_fct;
  typedef std::function<bool(const Texture_buffer& img, Texture_buffer* dst)> Encode_img_fct;

  Decode_img_fct      decode_jpeg;
  Decode_img_fct      decode_png;
  Get_img_size_fct    get_jpeg_size;
  Get_img_size_fct    get_png_size;
  Decode_mesh_fct     decode_draco;
  Decode_mesh_fct     decode_lepcc;
  Encode_img_fct      encode_to_dxt_with_mips; //client side DDS recompression
  std::shared_ptr<utl::Basic_tracker> m_tracker; //tbd

  utl::Basic_tracker*   tracker() const { return m_tracker.get(); } //constness TBD
  int                 num_threads = 1;
  // TBD: private?
  Ctx_properties      m_prop;
};

inline bool is_integer(Type type) { return type < Type::Float32; }

I3S_EXPORT Context::Ptr create_minimal_reader_context(const Ctx_properties& prop);

} //end of i3s

} // namespace i3slib 
