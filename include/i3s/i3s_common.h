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
#include "i3s/i3s_common_dom.h"
#include "i3s/i3s_material_dom.h"
#include "utils/utl_i3s_export.h"
#include "utils/utl_buffer.h"
#include "utils/utl_geom.h"
#include "utils/utl_gzip_context.h"
#include "utils/utl_datetime.h"
#include "utils/utl_i3s_assert.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>

namespace i3slib
{

namespace utl { class Basic_tracker; }


enum class Has_fids
{
  No,  // no fids are present
  Yes_32,  // fids are encoded in a single 32bit array (hence fids are <2^32)
  Yes_64   // fids are encoded using 2 32bit arrays, one for low bits, one for high bits (hence fids are <2^64)
};


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
typedef uint32_t Anchor_point_fid_index;

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
  Basis = 16,
  Raw_rgba8 = 32,
  Raw_rgb8 = 64,
  Ktx2 = 128,
  Default = Jpg | Png,
  Raw_uncompressed = Raw_rgba8 | Raw_rgb8,
  Desktop = Default | Dds,
  All_compressed = Desktop | Ktx | Ktx2,
  Not_gpu_compressed = Raw_uncompressed | Jpg | Png
};


I3S_EXPORT bool            from_string(const std::string& txt_utf8, Image_format* out);
I3S_EXPORT std::string     to_string(Image_format enc);

struct Obb
{
  utl::Vec3d center;
  utl::Vec3f extent;
  utl::Vec4d orientation;
};

struct Attribute_meta
{
  std::string       key; //<- used to find the attribute buffer resources.
  std::string       name;
  std::string       alias;
};

struct Domain_coded_value_definition
{
  std::string name;
  utl::Variant code;
};

struct Attribute_definition
{
  Attribute_meta    meta;
  Type              type = Type::Not_set;
  Esri_field_type   esri_type_overload = Esri_field_type::Not_set; // if it is Esri_field_type::Not_set, to_esri_type(type) will be used to deduce an Esri_field_type.
  Key_value_encoding key_value_encoding;
  int               component_count{ 1 }; 
  Attribute_storage_info_encoding encoding = Attribute_storage_info_encoding::Not_set;
  Time_encoding     time_encoding = Time_encoding::Not_set;
  std::optional<std::vector<Domain_coded_value_definition>> coded_values;
};

namespace Spatial_reference
{
inline constexpr bool is_well_known_gcs(const Spatial_reference_desc& d) { return (d.latest_wkid == 4326 
                                                                  || d.wkid == 4326) != (d.latest_wkid == 4490 || d.wkid == 4490); }

inline constexpr bool is_null(const Spatial_reference_desc& d) { return d.wkid < 0 && d.latest_wkid < 0 && d.wkt.empty(); }
inline constexpr int32_t c_wkid_not_set = -1;
}

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
  std::string alias;
  std::string copyright;
  Spatial_reference_desc sr;
  std::string uid;
  std::string drawing_info, elevation_info, popup_info;
  uint64_t timestamp = 0;
  Normal_reference_frame normal_reference_frame;
  Height_model_info height_model_info;
  Lod_metric_type lod_metric_type = Lod_metric_type::Max_screen_area;
  std::vector<Capability> capabilities;
  std::vector<Image_format> tex_formats;
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
  Texture_semantic semantic = Texture_semantic::Not_set;
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
  float       metallic_factor = 1.0f;
  float       roughness_factor = 1.0f;
};

struct Material_properties
{
  Alpha_mode                    alpha_mode = Alpha_mode::Opaque;
  int                           alpha_cut_off = 255; //in [0, 255], below cut-off, pixels are dropped.
  bool                          double_sided = true;
  utl::Vec3f                    emissive_factor = utl::Vec3f(0.0); //TBD
  Face_culling_mode             cull_face = Face_culling_mode::None;
  Metallic_roughness            metal;
  float                         occlusion_strength{ 1.0f };
  float                         normal_scale{ 1.0f };
  uint32_t                      avail_tex_semantic_mask{ 0 }; // 1 bit per semantic.
};

// This struct contains at most one texture per semantic. (rendering)
struct Material_data
{
  DECL_PTR(Material_data);
  // --- fields:
  Material_properties           properties;
  template< class T> using Tex_array = std::array<T, (uint32_t)Texture_semantic::_count >;
  Tex_array<Texture_buffer> texs;
  //Tex_array<Material_texture_desc> tex_refs;
  // --- 
  bool has_texture(i3s::Texture_semantic sem) const { return texs[(uint32_t)sem].data.size(); }
  void set_texture(const Texture_buffer& buf, i3s::Texture_semantic sem = i3s::Texture_semantic::Base_color) { texs[(uint32_t)sem] = buf; } //shallow copy
  Texture_buffer& get_texture(i3s::Texture_semantic sem) { return texs[(uint32_t)sem]; }
  const Texture_buffer& get_texture(i3s::Texture_semantic sem)const { return texs[(uint32_t)sem]; }
};

// May contain multiple image format per texture semantic ( useful for conversion, layer creation, etc.)
struct Material_data_multitex
{
  DECL_PTR(Material_data);
  // --- fields:
  Material_properties           properties;
  Metallic_roughness            metallic_roughness;
  template< class T> using Tex_array = std::array<T, (uint32_t)Texture_semantic::_count >;
  Tex_array<Multi_format_texture_buffer> texs;
  // --- 
  bool has_texture(i3s::Texture_semantic sem) const { return texs[(uint32_t)sem].size(); }
  void add_texture( const Texture_buffer& buf, i3s::Texture_semantic sem = i3s::Texture_semantic::Base_color) { texs[(uint32_t)sem].push_back(buf); } 
  Multi_format_texture_buffer& get_texture(i3s::Texture_semantic sem)  { return texs[(uint32_t)sem]; }
};

class Attribute_buffer;
class Geometry_buffer;

struct Mesh_data
{
  std::vector<std::shared_ptr<Attribute_buffer>> attribs;      // in order of the attribute schema for this mesh.
  std::vector<std::shared_ptr<Geometry_buffer>> geometries;    // v1.x: only single geometry supported.
  Material_data_multitex                        material;
  // The following fields are only useful for version upgrade workflow:
  std::string                          legacy_feature_data_json; 
  std::string                          legacy_feature_data_geom_json; 
  std::string                          legacy_mat_json; // serialized Legacy_material_desc object from legacy "shared" resource.
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
  Basis = (uint32_t)Image_format::Basis, //we probably shoudl remove basis from here (as its same content in ktx2) ??
  KTX2 = (uint32_t)Image_format::Ktx2,
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

/*
* Defines max major version supported when _reading_ a layer.
*/
struct Max_major_versions
{
public:
  explicit Max_major_versions(const std::vector<std::pair<Layer_type, int> >& supported_major_versions)
  {
    for (const auto& layer_ver : supported_major_versions)
    {
      I3S_ASSERT_EXT(layer_ver.first < Layer_type::_count);
      m_values[static_cast<int>(layer_ver.first)] = layer_ver.second;
    }
  }
  int get_max_major_version(Layer_type type) const { return type < Layer_type::_count ? m_values[static_cast<int>(type)] : 0; }
private:
  std::array<int, static_cast<int>(Layer_type::_count)> m_values{};
};

struct Ctx_properties
{
  explicit Ctx_properties(const Max_major_versions& max_ver) : m_max_ver_read(max_ver) {}
  const Max_major_versions& get_max_versions_for_reading_layers() const { return m_max_ver_read; }
  std::shared_ptr<utl::Basic_tracker> tracker;
  uint16_t max_write_texture_size     = 4096;
  uint16_t max_read_texture_size      = 8192;
  bool     is_drop_normals            = false;
  bool     verify_compressed_geometry = false; //PSL writer only.
  Geometry_compression_flags     geom_encoding_support{ (Geometry_compression_flags)Geometry_compression::Lepcc };
  Geometry_compression_flags     geom_decoding_support{ (Geometry_compression_flags)Geometry_compression::Lepcc };
  GPU_texture_compression_flags  gpu_tex_encoding_support{ (GPU_texture_compression_flags)GPU_texture_compression::None }; // writer contex only. What to encode.
  GPU_texture_compression_flags  gpu_tex_rendering_support{ (GPU_texture_compression_flags)GPU_texture_compression::Desktop }; // rendering only (reader context only)
  Gzip_with_monotonic_allocator  gzip_option{ Gzip_with_monotonic_allocator::Yes };
private:
  Max_major_versions m_max_ver_read;
};

class Mesh_abstract;

struct Context
{
  DECL_PTR(Context);
  explicit Context(const Ctx_properties& prop) : m_prop(prop) {}
  typedef std::function<bool(const utl::Raw_buffer_view& raw_img, Texture_buffer* out)> Decode_img_fct;
  typedef std::function<std::shared_ptr<Mesh_abstract>(const utl::Vec3d & origin, const utl::Raw_buffer_view & buffer)> Decode_mesh_fct;
  typedef std::function<bool(const utl::Raw_buffer_view& jpeg, int* w, int* h, int max_tex_dim)> Get_img_size_fct;
  typedef std::function<bool(const utl::Raw_buffer_view& basis, int* w, int* h, int* num_mips)> Get_compressed_img_info_fct;
  typedef std::function<bool(const Texture_buffer& img, Texture_buffer* dst)> Encode_img_fct;
  typedef std::function<bool(const Texture_buffer& img, Texture_buffer* dst, Image_format to_img_fmt)> Transcode_img_fct;

  Decode_img_fct      decode_jpeg;
  Decode_img_fct      decode_png;
  Get_img_size_fct    get_jpeg_size;
  Get_img_size_fct    get_png_size;
  Decode_mesh_fct     decode_draco;
  Decode_mesh_fct     decode_lepcc;
  Encode_img_fct      encode_to_dxt_with_mips; //client side DDS recompression
  Transcode_img_fct   transcode_basis;  // client side transcoding of Basis to DDS or KTX
  Get_compressed_img_info_fct get_basis_info;
  std::optional<utl::Datetime_meta>   datetime_meta; // try convert date attributes if set.
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
