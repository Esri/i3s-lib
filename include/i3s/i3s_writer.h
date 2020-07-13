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
#include "i3s/i3s_common.h"
#include "utils/utl_i3s_export.h"
#include "utils/utl_geom.h"
#include "utils/utl_declptr.h"
#include <stdint.h>
#include <vector>
#include <functional>
#include <filesystem>

namespace i3slib
{

namespace utl { class Slpk_writer; }

namespace i3s
{

struct Simple_node_data
{
  Mesh_data                 mesh;
  double                    lod_threshold = 0.0;
  std::vector< Node_id>     children;
  int                       node_depth = -1;
};

struct Simple_raw_mesh
{
  Texture_buffer      img;
  int                 vertex_count = 0;
  const utl::Vec3d*   abs_xyz=nullptr;
  const utl::Vec2f*   uv=nullptr;
  int                 index_count = 0;   // 0 if mesh in not indexed. 
  const uint32_t*         indices = nullptr; // optional. must be a multiple of 3. 
};

struct Simple_raw_points
{
  int count{ 0 };
  const utl::Vec3d* abs_xyz{ nullptr };
  const uint32_t* fids{ nullptr };
};

//! Create a opaque texture from a image buffer, no UV wrapping, not mipmap, no atlasing
//! channel_count must be 3 or 4
//! Byte order must be RGB(A) with R is LSB. 
//! source data will be deep-copied.
I3S_EXPORT bool  create_texture_from_image(int width, int height, int channel_count, const char* data, Texture_buffer& out);

struct Writer_context
{
  DECL_PTR(Writer_context);

  typedef std::function< bool(const Texture_buffer& img, Texture_buffer* dst)> Encode_img_fct;
  typedef std::function< bool(const Mesh_abstract& src, utl::Raw_buffer_view* out, double scale_x, double scale_y)> Encode_geometry_fct;
  typedef std::function< bool( const Spatial_reference& sr, utl::Vec3d* xyz, int count)> Geo_xform_fct;
  Encode_img_fct        encode_to_jpeg;
  Encode_img_fct        encode_to_png;
  Encode_img_fct        encode_to_etc2_with_mips;

  Encode_geometry_fct   encode_to_draco;
  Geo_xform_fct         to_cartesian_space;
  Geo_xform_fct         from_cartesian_space;

  bool                  is_drop_region_if_not_repeated= true;

  utl::Basic_tracker*   tracker() { return decoder ? decoder->tracker() : nullptr; }

  Context::Ptr          decoder;
};

typedef int status_t; //TBD!

class Stats_attribute;

class Layer_writer
{
public:
  DECL_PTR(Layer_writer);
  virtual ~Layer_writer() = default;
  virtual void       set_layer_meta(const Layer_meta& meta) = 0;
  virtual status_t   set_attribute_meta( Attrib_index idx, const Attribute_meta& def, Attrib_schema_id sid = 0) = 0;
  virtual status_t   set_attribute_stats( Attrib_index idx, std::shared_ptr<const Stats_attribute> stats, Attrib_schema_id sid = 0) = 0; //TBD
  virtual status_t   create_output_node(const Simple_node_data& mesh, Node_id id) = 0;
  virtual status_t   process_children(const Simple_node_data& mesh, Node_id id) = 0;
  virtual status_t   create_node(const Simple_node_data& mesh, Node_id node_id) = 0;
  virtual status_t   save() = 0;

  //! Create Mesh_data from src mesh description. Vertex data will be deep-copied, but Texture_buffer will be shallow-copied.
  virtual status_t   create_mesh_from_raw(const Simple_raw_mesh& src, Mesh_data& dst) const = 0;
  virtual status_t   create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const = 0;
};

struct Cartesian_transformation
{
  DECL_PTR(Cartesian_transformation);

  virtual bool to_cartesian(const Spatial_reference& sr, utl::Vec3d* xyz, int count) = 0;
  virtual bool from_cartesian(const Spatial_reference& sr, utl::Vec3d* xyz, int count) = 0;
  virtual ~Cartesian_transformation() {}
};

I3S_EXPORT Writer_context::Ptr create_i3s_writer_context(
  const Ctx_properties& prop,
  Cartesian_transformation::Ptr cs_transform = nullptr);

I3S_EXPORT Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, const std::filesystem::path& path);
I3S_EXPORT Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, std::shared_ptr<utl::Slpk_writer> slpk);

// this is exposed for psl cooker for now. Will be made private once cooker is ported to use this
I3S_EXPORT Esri_field_type to_esri_type(Type type);

}

} // namespace i3slib
