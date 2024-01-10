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
#include "i3s/i3s_writer.h"
#include "utils/utl_slpk_writer_api.h"
#include "utils/utl_obb.h"
#include "utils/utl_gzip_context.h"
#include "utils/utl_prohull.h"
#include "utils/utl_box.h"
#include <map>
#include <atomic>
#include <mutex>
#include <memory>
#include "i3s/i3s_index_dom.h"
#include "utils/utl_basic_tracker_api.h" //TBD
#include "utils/utl_stats.h"

namespace i3slib
{

namespace i3s
{

struct Perf_stats
{
  double    get_ratio() const { return (double)draco_size / (double)(legacy_size); }

  int64_t   draco_size = 0;
  int64_t   legacy_size = 0;
};

class Stats_attribute;

namespace detail
{
class Material_helper
{
public:
  int     get_or_create_texture_set(Image_formats f, bool is_atlas, Texture_semantic sem);
  int     get_or_create_material(const Material_desc& data);
  const std::vector< Texture_definition_desc >& get_texture_defs() const { return m_tex_def; }
  const std::vector< Material_desc >& get_material_defs() const { return m_mat_def; }
  std::vector< std::string >                          get_legacy_texture_mime_types() const;

private:
  std::vector< Material_desc > m_mat_def;
  std::vector< Texture_definition_desc > m_tex_def;
  std::mutex                             m_mutex;
};

struct Node_io;
}

class Layer_writer_impl final : public Layer_writer
{
public:
  enum Stats_mode { Create_stats, Dont_create_stats };
  static constexpr int c_count_geometry_defs = 8;
  DECL_PTR(Layer_writer_impl);
  Layer_writer_impl(utl::Slpk_writer::Ptr slpk, Writer_context::Ptr ctx, int sublayer_id = -1);
  virtual ~Layer_writer_impl() override = default;

  // --- Layer_writer:
  virtual void       set_layer_meta(
    const Layer_meta&                     meta
    , const i3s::Spatial_reference_desc*  dst_sr
    , const Normal_reference_frame*       nrf_override
    , const Height_unit                   dst_vert_linear_unit // If Height_unit::Not_set then the current unit will be left the same and not set to Not_set.
    , const std::string&                  dst_vert_crs_name // If the empty string, then the vert cs name will not be overridden.
    , const Height_model                  dst_height_model = Height_model::Not_set // If `Not_set` then the height model will not be overridden.
    ) override; //Must be call first ( so that projection could be setup)
  virtual status_t   set_attribute_meta(
    Attrib_index idx,
    const Attribute_definition& attrib_def,
    Attrib_schema_id sid = 0) override;
  virtual status_t   set_attribute_stats(Attrib_index idx, std::shared_ptr<const Stats_attribute> stats, Attrib_schema_id sid = 0) override; //TBD
  [[nodiscard]]
  virtual status_t   create_output_node(const Simple_node_data& mesh, Node_id id) override;
  [[nodiscard]]
  virtual status_t   process_children(const Simple_node_data& mesh, Node_id id) override;
  [[nodiscard]]
  virtual status_t   create_node(const Simple_node_data& mesh, Node_id id) override;
  [[nodiscard]]
  virtual status_t   save(utl::Boxd* extent = nullptr) override;
  virtual status_t   create_mesh_from_raw(const Simple_raw_mesh& src, Mesh_data& dst) const override;
  virtual status_t   create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const override;
  Spatial_reference_xform::cptr get_xform() const { return m_xform; }
private:
  [[nodiscard]]
  status_t              _write_node(detail::Node_io& d, Node_desc_v17* maybe_parent);
  size_t        _get_page_size() const;
  // This method is called when a node has been written, with the guarantee that
  // calls corresponding to sibling nodes will be done in the same thread, and are contiguous in time.
  status_t      _on_node_written(Node_desc_v17&, Node_desc_v17* maybe_parent);
  [[nodiscard]]
  status_t      _save_paged_index(uint32_t root_index, std::map<int, int>& geometry_ids_mapping);
  void                  _encode_geometry_to_legacy(detail::Node_io& nio, const Geometry_buffer& src);
protected:
  std::string           _layer_path(const std::string& resource = std::string()) const;

protected:
  Layer_meta              m_layer_meta;
  int                     m_sublayer_id; // >= 0 means this layer is a sublayer
  Writer_context::Ptr     m_ctx;
  utl::Slpk_writer::Ptr   m_slpk;
  std::mutex              m_mutex;
  struct Node_brief
  {
    utl::Obb_abs obb;
    utl::Vec4d  mbs;
    std::optional<utl::Boxd> envelope;  // empty leaf nodes don't have an envelope.
    int level = -1; //so we can identify the root.
    std::unique_ptr<detail::Node_io> node;
  };
  std::map< Node_id, Node_brief > m_working_set;

  Attrib_flags          m_vb_attribs = 0;               // actual non-null vertex attributes 
  Attrib_flags          m_vb_attribs_mask_legacy = 0; // padded vertex attributes written in legacy buffers (Zero-value added for padded attrib) 
  struct Attrb_info
  {
    Attribute_definition      def;
    std::shared_ptr<const Stats_attribute> stats;
  };
  std::mutex                              m_mutex_attr;  // synchronizes accesses to m_attrib_metas
  std::vector< std::vector< Attrb_info> > m_attrib_metas; //per definition set.
  Attrb_info& _get_attrib_meta_nolock(Attrib_schema_id sid, Attrib_index idx);

  std::map<int, utl::Histo_datetime<std::string> > m_datetime_stats;
  std::array<std::atomic<int>, c_count_geometry_defs> m_geometry_defs{ {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}} };
  std::atomic<size_t> m_node_count = 0;

  Perf_stats m_perf; //TBD: move to builder_context ?
  detail::Material_helper m_mat_helper;

  Spatial_reference_xform::cptr m_xform;

  Gzip_context m_gzip;

  std::mutex                   m_mutex_nodes17;  // synchronizes accesses to m_nodes17
  std::vector< Node_desc_v17 > m_nodes17; // implicitely indexed by Node_id

  static bool remap_geometry_ids(std::map<int, int>* out, const std::atomic<int>* geometry_ids, int size);
};

I3S_EXPORT void create_mesh_from_raw_and_origin(
  const Simple_raw_points& src,
  const utl::Vec3d& origin,
  Mesh_data& dst);

// These 'compute_obb' overloads are not in i3s_writer.h because Pro_hull is not part of the SDK.
I3S_EXPORT void compute_obb(
  const Spatial_reference_xform& xform,
  std::vector<utl::Vec3d>& points,
  utl::Pro_hull& hull,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs);
I3S_EXPORT void compute_obb(
  const Spatial_reference_xform& xform,
  const std::vector< utl::Obb_abs>& child_obbs,
  utl::Pro_hull& hull,
  std::vector<utl::Vec3d>& scratch_corners,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs);

I3S_EXPORT void compute_aabb(
  const Spatial_reference_xform& xform,
  utl::Vec3d* points,
  const size_t count_points,
  utl::Box<double>& aabb,
  double & radius);

inline bool to_dst_cartesian(const Spatial_reference_xform& xform, utl::Vec3d* xyz, int count = 1)
{
  return xform.transform(Spatial_reference_xform::Sr_type::Dst_sr, Spatial_reference_xform::Sr_type::Dst_cartesian, xyz, count)
    == Spatial_reference_xform::Status_t::Ok;
}
inline bool from_dst_cartesian(const Spatial_reference_xform& xform, utl::Vec3d* xyz, int count = 1)
{
  return xform.transform(Spatial_reference_xform::Sr_type::Dst_cartesian, Spatial_reference_xform::Sr_type::Dst_sr, xyz, count)
    == Spatial_reference_xform::Status_t::Ok;
}

} // namespace i3s
} // namespace i3slib
