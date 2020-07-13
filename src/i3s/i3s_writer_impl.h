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
#include <map>
#include <atomic>
#include <mutex>
#include <memory>
#include "i3s/i3s_index_dom.h"
#include "utils/utl_basic_tracker_api.h" //TBD

namespace i3slib
{

namespace i3s
{

struct Perf_stats
{
  double    get_ratio() const { return (double)draco_size / (double)(legacy_size); }

  int64_t   draco_size=0;
  int64_t   legacy_size=0;
};

class Stats_attribute;

namespace detail
{ 
  class Material_helper;
  struct Node_io;
}

class Layer_writer_impl : public Layer_writer
{
public: 
  enum Stats_mode { Create_stats, Dont_create_stats };
  DECL_PTR(Layer_writer_impl);
  Layer_writer_impl(utl::Slpk_writer::Ptr slpk, Writer_context::Ptr ctx);
  virtual ~Layer_writer_impl() override = default;
  //bool create_slpk(const std::string& path_utf8, Stats_mode mode);
  
  // --- Layer_writer:
  virtual void       set_layer_meta(const Layer_meta& meta) override { m_layer_meta = meta; }
  virtual status_t   set_attribute_meta( Attrib_index idx, const Attribute_meta& def, Attrib_schema_id sid=0) override;
  virtual status_t   set_attribute_stats( Attrib_index idx, std::shared_ptr<const Stats_attribute> stats, Attrib_schema_id sid=0) override; //TBD
  virtual status_t   create_output_node(const Simple_node_data& mesh, Node_id id) override;
  virtual status_t   process_children(const Simple_node_data& mesh, Node_id id) override;
  virtual status_t   create_node(const Simple_node_data& mesh, Node_id id) override;
  virtual status_t   save() override;
  virtual status_t   create_mesh_from_raw(const Simple_raw_mesh& src, Mesh_data& dst) const override;
  virtual status_t   create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const override;
private:
  void                  _write_node( detail::Node_io& d);
  status_t              _save_paged_index( int root_index );
  void                  _encode_geometry_to_legacy(detail::Node_io& nio, const Geometry_buffer& src);
private:

  static const int        c_default_page_size; 
  Layer_meta              m_layer_meta;
  Writer_context::Ptr     m_ctx;
  utl::Slpk_writer::Ptr   m_slpk;
  std::mutex              m_mutex;
  struct Node_brief
  {
    utl::Obb_abs obb;
    utl::Vec4d  mbs;
    int level = -1; //so we can identify the root.
    std::unique_ptr<detail::Node_io> node;
  };
  std::map< Node_id, Node_brief > m_working_set;

  Attrib_flags          m_vb_attribs=0;               // actual non-null vertex attributes 
  Attrib_flags          m_vb_attribs_mask_legacy = 0; // padded vertex attributes written in legacy buffers (Zero-value added for padded attrib) 
  struct Attrb_info
  {
    Attribute_definition      def;
    std::shared_ptr<const Stats_attribute> stats;
  };
  std::vector< std::vector< Attrb_info> > m_attrib_metas; //per definition set.
  Attrb_info& _get_attrib_meta(Attrib_schema_id sid, Attrib_index idx) ;

  std::vector< Node_desc_v17 > m_nodes17;
  std::array<std::atomic<int>, 8> m_geometry_defs{{{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}}};

  Perf_stats m_perf; //TBD: move to builder_context ?
  std::unique_ptr< detail::Material_helper  > m_mat_helper;
};

}

} // namespace i3slib
