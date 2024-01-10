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
#include "utils/utl_gzip_context.h"
#include "utils/utl_box.h"
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <filesystem>
#include <optional>

namespace i3slib
{

namespace utl
{
class Slpk_writer;
struct Obb_abs;
}

namespace i3s
{

struct Simple_node_data
{
  Mesh_data                 mesh;
  double                    lod_threshold = 0.0;
  std::vector< Node_id>     children;
  int                       node_depth = -1;

  // The following two values must be provided together.
  // They are considered effective if precomputed_obb.extent.x >= 0.
  // No coordinate system transformation is applied to vertex coordinates
  // if precomputed obb is provided, this feature should be used in scenarios
  // where an SLPK is partially copied to another SLPK (i.e. clipping).
  Obb                       precomputed_obb = { {}, { -1.0, -1.0, -1.0 } };
  utl::Vec4d                precomputed_mbs;
};

// given maximal screen extent in pixels calculates the corresponding area threshold 
// to be used for Simple_node_data::lod_threshold
[[nodiscard]]
inline constexpr double screen_size_to_area(double pixels)noexcept
{
  constexpr double c_pi_over_4 = utl::c_pi * 0.25;
  return pixels * pixels * c_pi_over_4;
}

struct Simple_raw_mesh
{
  Texture_buffer      img;
  int                 vertex_count = 0;
  const utl::Vec3d*   abs_xyz=nullptr;
  const utl::Vec2f*   uv=nullptr;
  int                 index_count = 0;   // 0 if mesh in not indexed. 
  const uint32_t*     indices = nullptr; // optional. must be a multiple of 3. 
  const uint64_t* fid_values{ nullptr };
  int fid_value_count = 0;
  const uint32_t* fids_indices{ nullptr }; // if not nullptr then it's size equals to 
                                           // (index_count ? index_count : vertex_count)
  i3s::Rgba8 default_color{0xff};
};

struct Simple_raw_points
{
  int count{ 0 };
  const utl::Vec3d* abs_xyz{ nullptr };
  const uint64_t* fids{ nullptr };
};

//! Create a opaque texture from a image buffer, no UV wrapping, not mipmap, no atlasing
//! channel_count must be 3 or 4
//! Byte order must be RGB(A) with R is LSB. 
//! source data will be deep-copied.
I3S_EXPORT bool  create_texture_from_image(int width, int height, int channel_count, const char* data, Texture_buffer& out);


//! Src/Dst_cartesian represents an cartesian reference frame which is "associated" with the SR. 
//! such a "cartesian" space is required to compute bounding volume. 
//! in practice:
//!    - if SR is a PCS, then no changes are required (i.e. Src_sr == Src_cartesian) and transform is no-op. 
//!    - if SR is a GCS, convertion to (cartesian) ECEF must be provided by the implementation.
//! In addition, the implementation may also support true re-projection ( e.g UTM_11N -> WGS84 ), but this is not required if Src_sr == Dst_sr.
//! **warning**:  implementation must be thread-safe.
class Spatial_reference_xform
{
public:
  DECL_PTR(Spatial_reference_xform);
  enum class Sr_type { Src_sr, Dst_sr, Src_cartesian, Dst_cartesian };
  enum class Status_t { Ok, Failed, No_implementation};

  virtual ~Spatial_reference_xform() = default;
  virtual Status_t                    transform(Sr_type src, Sr_type dst, utl::Vec3d* xyz, int count = 1) const = 0;
  virtual const Spatial_reference_desc&    get_dst_sr() const = 0;
};

class Spatial_reference_xform_cartesian_only : public Spatial_reference_xform
{
public:
  DECL_PTR(Spatial_reference_xform_cartesian_only);
  explicit Spatial_reference_xform_cartesian_only(const Spatial_reference_desc& sr) : m_src(sr) {}
  I3S_EXPORT virtual Status_t           transform(Sr_type src, Sr_type dst, utl::Vec3d* xyz, int count) const override;
  virtual const Spatial_reference_desc& get_dst_sr() const override { return m_src; }
private:
  Spatial_reference_desc m_src; //src==dst. no projection
};

//typedef int status_t; //TBD!
class status_t
{
public:
  status_t() : m_c(8000) {} // IDS_I3S_OK is defined as 8000 in "utils/utl_i3s_resource_defines.h"
  status_t(bool) = delete;
  status_t(int c) : m_c(c) {}

  operator bool() = delete;
  operator int() = delete;
  bool operator !() = delete;
  bool operator == (status_t rhs) const { return m_c == rhs.m_c; }
  bool operator != (status_t rhs) const { return !(*this == rhs); }
  bool operator == (int rhs) const { return m_c == rhs; }
  bool operator != (int rhs) const { return !(*this == rhs); }
  int get_code()const { return m_c; }
private:
  int m_c;
};


I3S_EXPORT void compute_obb(
  const Spatial_reference_xform& xform,
  const std::vector< utl::Obb_abs>& child_obbs,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs);

I3S_EXPORT status_t project_update_mesh_origin_and_obb(
  utl::Basic_tracker* tracker,
  Layer_type layer_type,
  const Spatial_reference_xform& xform,
  Mesh_abstract& mesh,
  const std::vector<utl::Obb_abs>& child_obbs,
  utl::Obb_abs& obb,   // in/out
  utl::Vec4d& mbs);    // out

enum class Writer_finalization_mode
{
  Finalize_output_stream, Leave_shared_output_stream_open
};

enum class Pages_construction
{
  Breadth_first, // legacy
  Local_sub_tree,
  Local_sub_tree_all_siblings, // works best for deep trees (minimizes the number of pages traversed from root to leaves)
};

// Whether draco geometries should be gzipped
enum class Gzip_draco { No, Yes };

// Whether to write legacy documents
enum class Write_legacy { No, Yes };

struct Writer_context
{
  DECL_PTR(Writer_context);

  typedef std::function< bool(const Texture_buffer& img, Texture_buffer* dst)> Encode_img_fct;
  //typedef std::function< bool(const Texture_buffer& img, Texture_buffer* dst, i3slib::utl::Basis_output_format basis_out_fmt)> Encode_img_fct_basis;
  typedef std::function< bool(const Mesh_abstract& src, utl::Raw_buffer_view* out, Has_fids&, double scale_x, double scale_y)> Encode_geometry_fct;
  typedef std::function< Spatial_reference_xform::ptr(const i3s::Spatial_reference_desc& layer_sr, const i3s::Spatial_reference_desc* dst_sr )> Spatial_ref_factory_fct;

  Encode_img_fct                      encode_to_jpeg;
  Encode_img_fct                      encode_to_png;
  Encode_img_fct                      encode_to_etc2_with_mips;
  Encode_img_fct                      encode_to_basis_with_mips;
  Encode_img_fct                      encode_to_basis_ktx2_with_mips;

  Encode_geometry_fct                 encode_to_draco;
  bool                                is_drop_region_if_not_repeated= true;
  bool                                draco_allow_large_fids = false;  // whether fid values >= 2^32 are allowed in draco metadata
  Write_legacy                        write_legacy = Write_legacy::Yes;

  utl::Basic_tracker*                 tracker() const { return decoder ? decoder->tracker() : nullptr; }

  Context::Ptr                          decoder;
  Spatial_ref_factory_fct               sr_helper_factory;
  std::optional<Normal_reference_frame> dst_nrf;
  Height_unit                           dst_vert_linear_unit { Height_unit::Not_set };
  std::string                           dst_vert_crs_name;
  Height_model                          dst_height_model { Height_model::Not_set };
  bool                                  use_src_tex_set{ false };

  Pages_construction                    pages_construction = Pages_construction::Breadth_first;
  size_t                                max_page_size = 64;

  i3s::Writer_finalization_mode         finalization_mode = i3s::Writer_finalization_mode::Finalize_output_stream;
  Gzip_with_monotonic_allocator         gzip_option = Gzip_with_monotonic_allocator::Yes;
  Gzip_draco                            gzip_draco{ Gzip_draco::Yes };
  Priority                              priority{ c_default_priority };
  Semantic                              semantic{ c_default_semantic };
};

class Stats_attribute;

class Layer_writer
{
public:
  DECL_PTR(Layer_writer);
  virtual ~Layer_writer() = default;
  virtual void       set_layer_meta(
                                    const Layer_meta&                     meta
                                    , const i3s::Spatial_reference_desc*  dst_sr = nullptr
                                    , const Normal_reference_frame*       nrf_override = nullptr
                                    , const Height_unit                   dst_vert_linear_unit = Height_unit::Not_set // If Height_unit::Not_set then the current unit will be left the same and not set to Not_set.
                                    , const std::string&                  dst_vert_crs_name = "" // If the empty string, then the vert cs name will not be overridden.
                                    , const Height_model                  dst_height_model = Height_model::Not_set // If `Not_set` then the height model will not be overridden.
                                    ) = 0; // Must be call before :create_node()
  virtual status_t   set_attribute_meta(
    Attrib_index idx,
    const Attribute_definition& attrib_def,
    Attrib_schema_id sid = 0) = 0;
  virtual status_t   set_attribute_stats( Attrib_index idx, std::shared_ptr<const Stats_attribute> stats, Attrib_schema_id sid = 0) = 0; //TBD
  [[nodiscard]]
  virtual status_t   create_output_node(const Simple_node_data& mesh, Node_id id) = 0;
  [[nodiscard]]
  virtual status_t   process_children(const Simple_node_data& mesh, Node_id id) = 0;
  [[nodiscard]]
  virtual status_t   create_node(const Simple_node_data& mesh, Node_id node_id) = 0;
  [[nodiscard]]
  virtual status_t   save(utl::Boxd* extent = nullptr) = 0;

  //! Create Mesh_data from src mesh description. Vertex data will be deep-copied, but Texture_buffer will be shallow-copied.
  virtual status_t   create_mesh_from_raw(const Simple_raw_mesh& src, Mesh_data& dst) const = 0;
  virtual status_t   create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const = 0;
};

I3S_EXPORT Writer_context::Ptr create_i3s_writer_context(const Ctx_properties& prop, 
  Writer_finalization_mode finalization_mode = Writer_finalization_mode::Finalize_output_stream);

I3S_EXPORT Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, const std::filesystem::path& path);
I3S_EXPORT Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, std::shared_ptr<utl::Slpk_writer> slpk, int sublayer_id = -1);

}

} // namespace i3slib
