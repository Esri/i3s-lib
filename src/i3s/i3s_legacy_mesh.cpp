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
#include <iostream>
#include "i3s/i3s_legacy_mesh.h"
#include "i3s/i3s_layer_dom.h"
#include "utils/utl_i3s_resource_defines.h"
#include <unordered_map>
#include "utils/utl_bitstream.h"
#include "utils/utl_json_helper.h"
#include <optional>
#include <algorithm>

template<> struct std::hash<i3slib::utl::Vec4<uint16_t>>
{
  size_t operator()(const i3slib::utl::Vec4<uint16_t>& v) const { return reinterpret_cast<const size_t&>(v); }
};

namespace i3slib
{

namespace i3s
{

template< class T > Mesh_attrb< T > index_values(const utl::Buffer_view<const T>& src)
{
  int src_count = src.size();
  utl::Buffer_view< uint32_t > indices = utl::Buffer::create_writable_typed_view<uint32_t>(src_count);
  std::unordered_map<T, uint32_t > map(src_count * 2);
  std::vector< T > values;
  for (int i = 0; i < src_count; ++i)
  {
    auto iter = map.insert({ src[i], (int)values.size() });
    if (iter.second)
      values.push_back(src[i]);

    indices[i] = iter.first->second;
  }
  auto val = utl::Buffer::create_writable_typed_view<T>((int)values.size());
  copy_elements(val.data(), values.data(), values.size());
  Mesh_attrb< T > ret;
  ret.values = val;
  ret.index = indices;
  return ret;
}

//re-index the mesh_attrib to have unique values (i.e. draco indexed attribute may not be unique )
template< class T > Mesh_attrb< T > index_values(const Mesh_attrb<const T>& src)
{
  int src_count = src.size();
  utl::Buffer_view< uint32_t > indices = utl::Buffer::create_writable_typed_view<uint32_t>(src_count);
  std::unordered_map<T, uint32_t > map(src.values.size());
  std::vector< T > unique_vals;
  unique_vals.reserve(src.values.size());
  for (int i = 0; i < src.index.size(); ++i)
  {
    const auto& val = src[i];
    auto iter = map.insert({ val, (int)unique_vals.size() });
    if (iter.second)
      unique_vals.push_back(val);
    indices[i] = iter.first->second;
  }
  auto val = utl::Buffer::create_writable_typed_view<T>((int)unique_vals.size());
  copy_elements(val.data(), unique_vals.data(), unique_vals.size());
  Mesh_attrb< T > ret;
  ret.values = val;
  ret.index = indices;
  return ret;
}

// -----------------------------------------------------------------------------
//        class         Mesh_buffer_impl
// -----------------------------------------------------------------------------


class Mesh_buffer_impl : public Mesh_abstract
{
public:
  DECL_PTR(Mesh_buffer_impl);
  // --- Mesh_abstract :
  virtual bool          update_positions(const utl::Vec3d& origin, const utl::Vec3d* abs_pos, int count) override;
  virtual void          drop_normals() override { m_normals = Mesh_attrb<utl::Vec3f>(); m_attrib_mask &= ~(Attrib_flags)Attrib_flag::Normal; }
  virtual void          drop_colors() override { m_colors = Mesh_attrb<Rgba8>(); m_attrib_mask &= ~(Attrib_flags)Attrib_flag::Color; }
  virtual void          drop_regions() override;
  virtual void          drop_feature_ids() override{ m_fids = Mesh_attrb<uint64_t>(); m_attrib_mask &= ~(Attrib_flags)Attrib_flag::Feature_id; }
  virtual int           sanitize_uvs( float max_val) override;
  virtual utl::Vec3d    get_origin() const override { return m_origin; }
  virtual uint32_t      get_available_attrib_mask() const override { return m_attrib_mask; }
  virtual int           get_vertex_count() const override { return std::max(m_abs_pos.size(), m_rel_pos.size() ); }
  virtual int           get_face_count() const override { return get_vertex_count() / 3; }
  virtual int           get_region_count() const override { return (int)m_uv_region.values.size(); }
  virtual int           get_feature_count() const override { return m_fids.values.size(); }
  virtual Texture_meta::Wrap_mode            get_wrap_mode(int uv_set) const override;
  virtual const Mesh_attrb<utl::Vec3f>&      get_relative_positions() const override;
  virtual const Mesh_attrb<utl::Vec3f>&      get_normals() const override { return m_normals; }
  virtual bool                               create_normals(const i3s::Mesh_attrb< utl::Vec3f>& rel_positions, bool left_handed_reference_frame) override;
  virtual bool                               xform_normals(const std::function<bool(utl::Vec3f*, int count)>& proj) override;
  virtual const Mesh_attrb<utl::Vec2f>&      get_uvs(int uvset) const override { return m_uvs; }
  virtual const Mesh_attrb<Rgba8>&           get_colors() const override { return m_colors; };
  virtual const Mesh_attrb<Uv_region>&       get_regions() const override { return m_uv_region; }
  virtual const Mesh_attrb<uint64_t>&        get_feature_ids() const override { return m_fids; }
  virtual const Mesh_attrb<uint32_t>&         get_anchor_point_fid_indices() const override { return m_anchor_point_fid_indices; }
  virtual const Mesh_attrb<utl::Vec3f>&       get_relative_anchor_points() const override { return m_rel_anchor_points; }
  virtual const utl::Buffer_view<utl::Vec3d>& get_absolute_anchor_points() const override;
  virtual const utl::Buffer_view<utl::Vec3d>& get_absolute_positions() const override;
  virtual i3s::Mesh_topology                  get_topology() const override { return m_topo; }
  virtual void                                set_topology(i3s::Mesh_topology t) override {  m_topo = t; }
  // --- Mesh_buffer_impl:
  bool                  assign(const utl::Vec3d & origin, utl::Raw_buffer_view& buffer, bool has_attrib, Attrib_flags vb_attrib, const std::string& buffer_path, const i3s::Geometry_schema_desc* desc, utl::Basic_tracker* trk);
  bool                  assign(const Mesh_bulk_data& src);
  bool                  assign_points(const utl::Buffer_view<utl::Vec3d>& abs_pos, const utl::Buffer_view<int64_t>& fid, const utl::Vec3d & origin_just_in_case);
private:
  void                  _update_available();
  void                  _update_to_absolute_uv();
private:
  Attrib_flags                      m_attrib_mask = 0;
  utl::Vec3d                        m_origin = utl::Vec3d(0.0);
  mutable Mesh_attrb<utl::Vec3f>    m_rel_pos;
  Mesh_attrb<utl::Vec3f>            m_normals;
  Mesh_attrb<utl::Vec2f>            m_uvs;
  Mesh_attrb<Rgba8>                 m_colors;
  Mesh_attrb<Uv_region>             m_uv_region;
  Mesh_attrb<uint64_t>              m_fids;
  Mesh_attrb<uint32_t>              m_anchor_point_fid_indices;
  Mesh_attrb<utl::Vec3f>                m_rel_anchor_points;
  mutable utl::Buffer_view<utl::Vec3d>  m_abs_anchor_points;
  mutable Texture_meta::Wrap_mode       m_uv_wrap_mode = Texture_meta::Wrap_mode::Not_set; 
  mutable utl::Buffer_view<utl::Vec3d>  m_abs_pos;
  i3s::Mesh_topology                    m_topo{ i3s::Mesh_topology::Triangles };
};

const Mesh_attrb<utl::Vec3f>& Mesh_buffer_impl::get_relative_positions() const 
{ 
  if( m_abs_pos.size() && m_rel_pos.size() ==0 )
  {
    //build from absolute position
    auto rpos = utl::Buffer::create_writable_typed_view< utl::Vec3f>( m_abs_pos.size() );
    for (int i = 0; i < m_abs_pos.size(); ++i)
      rpos[i] = utl::Vec3f( m_abs_pos[i] - m_origin );
    m_rel_pos.values = rpos;
  }
  return m_rel_pos;
}

static void ensure_absolute(const utl::Vec3d& origin, const Mesh_attrb<utl::Vec3f>& rel_pos, utl::Buffer_view<utl::Vec3d>& abs_pos)
{
  if (abs_pos.size() == 0 && rel_pos.size())
  {
    //create from relative:
    abs_pos = utl::Buffer::create_writable_typed_view< utl::Vec3d>(rel_pos.size());
    for (int i = 0; i < rel_pos.size(); ++i)
      abs_pos[i] = utl::Vec3d(rel_pos[i]) + origin;
  }
}

const utl::Buffer_view<utl::Vec3d>& Mesh_buffer_impl::get_absolute_positions() const
{
  ensure_absolute(m_origin, m_rel_pos, m_abs_pos);
  return m_abs_pos;
}

const utl::Buffer_view<utl::Vec3d>& Mesh_buffer_impl::get_absolute_anchor_points() const
{
  ensure_absolute(m_origin, m_rel_anchor_points, m_abs_anchor_points);
  return m_abs_anchor_points;
}

utl::Buffer_view<utl::Vec3f> create_flat_normals(const i3s::Mesh_attrb<  utl::Vec3f>& pt, bool left_handed_reference_frame)
{
  I3S_ASSERT(pt.size() % 3 == 0);
  auto normals = utl::Buffer::create_writable_typed_view< utl::Vec3f>(pt.size());
  for (int i = 0; i < pt.size(); i += 3)
  {
    auto& p0 = pt[i];
    auto& p1 = pt[i + 1];
    auto& p2 = pt[i + 2];
    if (left_handed_reference_frame)
    {
      (normals)[i] = utl::Vec3f::cross(p2 - p1, p1 - p0).normalized();
    }
    else
    {
      (normals)[i] = utl::Vec3f::cross(p1 - p0, p2 - p1).normalized();
    }

    (normals)[i + 1] = (normals)[i];
    (normals)[i + 2] = (normals)[i];
  }
  return normals;
}

bool Mesh_buffer_impl::create_normals(const i3s::Mesh_attrb< utl::Vec3f>& rel_positions, bool left_handed_reference_frame)
{
  if (rel_positions.size() != get_vertex_count())
    return false;
  //TBD: shouldn't we index the normal per face at least ? 
  m_normals.values = create_flat_normals(rel_positions, left_handed_reference_frame);
  m_normals.index = decltype(m_normals.index){};
  return true;
}

bool Mesh_buffer_impl::xform_normals(const std::function<bool(utl::Vec3f*, int count)>& proj)
{
  // there is currently not an interface to get a mutable pointer to the data 
  // so resort to const_cast for now to tranform normals without copying buffer
  auto mut_normal_data = const_cast<utl::Vec3f*>(m_normals.values.data());
  if (proj(mut_normal_data, m_normals.values.size()))
  {
    return true;
  }
  return false;
}

//! origin is provided just in case caller request "relative" positions instead of absolute. 
bool Mesh_buffer_impl::assign_points(const utl::Buffer_view<utl::Vec3d>& abs_pos, const utl::Buffer_view<int64_t>& fid64, const utl::Vec3d & origin_just_in_case)
{
  m_abs_pos         = abs_pos;
  auto fids = utl::Buffer::create_writable_typed_view<uint64_t>(fid64.size());
  for (int i = 0; i < fids.size(); ++i)
    fids[i] = (uint64_t)fid64[i];

  m_fids.values = fids;

  m_topo = i3s::Mesh_topology::Points;
  m_origin = origin_just_in_case;
  m_rel_pos = i3s::Mesh_attrb<utl::Vec3f>();
  _update_available();
  return true;
}



//! WARNING: is_uv_repeated(0) == false -> otherwise this function will corrupt UVs
void  Mesh_buffer_impl::drop_regions()
{
  if (m_uv_region.size() && m_uvs.size())
  {
    //convert to absolute tex coordinate:
    I3S_ASSERT(m_uv_region.size() == m_uvs.size());
    auto abs_uv = utl::Buffer::create_writable_typed_view< utl::Vec2f >(m_uvs.values.size());
    int n = m_uvs.size();
    const float to_norm = 1.0f / (float)0xFFFF;
    for (int i = 0; i <n; ++i)
    {
      //get index of UV value:
      int k = m_uvs.get_mapped_index(i);
      // region:
      utl::Vec4f rg(m_uv_region[i]);
      rg *= to_norm;
      // compute absolute UV:
      I3S_ASSERT(m_uvs.values[k].x <= 1.0 && m_uvs.values[k].y <= 1.0);
      abs_uv[k].x = m_uvs.values[k].x * (rg.z - rg.x) + rg.x;
      abs_uv[k].y = m_uvs.values[k].y * (rg.w - rg.y) + rg.y;
    }
    //assign:
    m_uvs.values = abs_uv;
  }
  m_uv_region = Mesh_attrb<Uv_region>();
  m_attrib_mask &= ~(Attrib_flags)Attrib_flag::Region;
}

//! Some input have FLOAT_MAX UV coordinates which throw off the draco encoder. Will clamp them to 1.0
int Mesh_buffer_impl::sanitize_uvs(float max_val)
{
  int fixes= 0;
  float* val = const_cast< float*>( reinterpret_cast< const float*>(m_uvs.values.data()) );
  float* end = val + 2 * m_uvs.values.size();
  for(; val != end; ++val)
  {
    if (std::abs(*val) > max_val || !std::isfinite(*val))
    {
      *val = 1.0f;
      ++fixes;
    }
  }
  return fixes;
}


Mesh_abstract* parse_mesh_from_i3s( const utl::Vec3d& origin, utl::Raw_buffer_view& buff, bool has_feature_attrib
                                   , Attrib_flags vb_attrib, const std::string& path, const i3s::Geometry_schema_desc* desc, utl::Basic_tracker* trk)
{
  std::unique_ptr< Mesh_buffer_impl > ret(new Mesh_buffer_impl());
  if (ret->assign(origin, buff, has_feature_attrib, vb_attrib, path, desc, trk))
    return ret.release();

  return nullptr;
 }

//! WARNING: Point featureData stores position as ABSOLUTE (not like meshes). 
//! So, it the cooker didn't partition the data correctly, we could get a single tile covering 
//! a very large area where origin (f64) + delta (f32) position encoding won't be accurate enough.
//! TO AVOID this accuracy issue, Mesh_abstract_impl supports both relative and absolute position storage. 
//!  but caller must be aware that using relative positions may result in loss of accuracy 
Mesh_abstract*   parse_points_from_i3s(const utl::Vec3d& origin, const std::string& feature_data_json, const std::string& path, utl::Basic_tracker* trk)
{
  //construct the json string:
  //auto tmp = "{\"featureData\":" + feature_data_json  + "}";
  Point_feature_data_desc obj;
  if (utl::from_json_safe(feature_data_json, &obj, trk, path))
  {
    utl::Buffer_view< utl::Vec3d > pos = utl::Buffer::create_writable_typed_view< utl::Vec3d >((int)obj.points.size());
    utl::Buffer_view< int64_t > fid =  utl::Buffer::create_writable_typed_view< int64_t >((int)obj.points.size());
    for (int i = 0; i < obj.points.size(); ++i)
    {
      pos[i] = obj.points[i].position;
      fid[i] = obj.points[i].fid;
    }
    std::unique_ptr< Mesh_buffer_impl > ret(new Mesh_buffer_impl());
    if (ret->assign_points(pos, fid, origin))
      return ret.release();
  }
  return nullptr;
}

void   encode_points_to_i3s(int count, const utl::Vec3d* xyz, const uint64_t* fids, std::string* feature_data_json, utl::Basic_tracker* )
{
  Point_feature_data_desc desc;
  desc.points.resize(count);
  for (int i = 0; i < count; ++i)
  {
    desc.points[i].position = xyz[i];
    desc.points[i].fid = fids[i];
  }
  *feature_data_json = utl::to_json(desc);
}


Mesh_abstract* parse_mesh_from_bulk(const Mesh_bulk_data& data)
{
  std::unique_ptr< Mesh_buffer_impl > ret(new Mesh_buffer_impl());
  if (ret->assign(data))
    return ret.release();
  return nullptr;
}

I3S_EXPORT Mesh_abstract::Ptr create_empty_mesh()
{
  return std::make_shared< Mesh_buffer_impl>();
}

template <class T > static bool is_all_zero(const utl::Buffer_view<const T>& src)
{
  const T zero(0);
  int count = src.size();
  for (int i = 0; i < count; ++i)
    if (src[i] != zero)
      return false;

  return true;
}

template <class T > static  utl::Buffer_view<const T> create_attrib_view(const utl::Raw_buffer_view& src, int byte_offset, int count, bool* is_ok)
{
  
  utl::Buffer_view<const T> ret;
  if (*is_ok && byte_offset != -1)
  {
    ret = src.get_buffer()->create_typed_view<const T>(count, byte_offset);
    if (ret.is_valid())
    {
      //*byte_iter += count * sizeof(T);
      if (is_all_zero(src))
        ret = utl::Buffer_view<const T>();
    }
    else
      *is_ok = false;
  }
  return  ret;
}

void _set_flag(Attrib_flags* flags, Attrib_flag what, int count)
{
  if (count)
    *flags |= (uint32_t)what;
}

bool Mesh_buffer_impl::update_positions(const utl::Vec3d& origin, const utl::Vec3d* abs_pos, int count)
{
  if (m_rel_pos.size())
  {
    if (count != m_rel_pos.values.size())
    {
      I3S_ASSERT(false);
      return false;
    }
    // Casting away the cost should be fine because elements are not being added or removed. 
    auto rel_pos = const_cast<utl::Vec3f*>(m_rel_pos.values.data());
    // make the geometry relative to the new center:
    for (int i = 0; i < count; ++i)
      rel_pos[i] = utl::Vec3f(abs_pos[i] - origin);
  }

  m_origin = origin;

  return true;
}

bool Mesh_buffer_impl::assign(const Mesh_bulk_data& src)
{
  m_rel_pos = src.rel_pos;
  m_normals = src.normals;
  m_colors = src.colors;
  m_uvs = src.uvs;
  m_fids = src.fids;
  //m_fid_indices = src.fid_indices;
  m_uv_region = src.uv_region;
  m_origin = src.origin;
  m_anchor_point_fid_indices = src.anchor_point_fid_indices;
  m_rel_anchor_points = src.rel_anchor_points;
  m_uv_wrap_mode = Texture_meta::Wrap_mode::Not_set;

  //Index the regions:
  if (m_uv_region.size())
  {
    m_uv_region = index_values(m_uv_region);
  }
  _update_available();
  return true;
}

void Mesh_buffer_impl::_update_available()
{
  //compute the mask:
  struct Item {
    Attrib_flag what; int size;
  };
  std::vector< Item  > attribs = {
    { i3s::Attrib_flag::Pos, m_rel_pos.size() || m_abs_pos.size() },
  { i3s::Attrib_flag::Normal, m_normals.size() },
  { i3s::Attrib_flag::Uv0, m_uvs.size() },
  { i3s::Attrib_flag::Color,  m_colors.size() },
  { i3s::Attrib_flag::Region,  m_uv_region.size() },
  //{ i3s::Attrib_flag::Region_index,  m_uv_region.size() },
  { i3s::Attrib_flag::Feature_id,  m_fids.size() },
  //{ i3s::Attrib_flag::Feature_index, m_fids.size() }
  };
  for (auto& att : attribs)
    _set_flag(&m_attrib_mask, att.what, att.size);
}

//! Dragan is against it. 
void Mesh_buffer_impl::_update_to_absolute_uv()
{
  if (m_uv_region.size() && get_wrap_mode(0) != Texture_meta::Wrap_mode::None)
  {
    //TODO ?
  }
}

//enum class Legacy_vertex_atrb : int { Position = 0, Normal, Uv0, Color, Region, _count };


bool parse_legacy_geometry(
   utl::Raw_buffer_view& i3s_buffer
  , bool has_feature_attrib
  , Attrib_flags vb_attrib
  , const std::string& buffer_path //for warning.
  , Mesh_bulk_data& out
  , const i3s::Geometry_schema_desc* desc
  , utl::Basic_tracker* trk
)
{

  // --- Only support "standard" 8 byte header:
  // we assume  layer validation step would have rejected unsupported headers
  const int* hdr = reinterpret_cast<const int*>(i3s_buffer.data());
  int vtx_count = hdr[0];
  int fid_count = hdr[1];
  if ((vtx_count % 3) != 0 || vtx_count <= 0 || !is_set(vb_attrib, Attrib_flag::Pos))
    return utl::log_warning(trk, IDS_I3S_INVALID_VERTEX_COUNT_IN_BUFFER, buffer_path, vtx_count);


  static const Attrib_flag c_to_atrb_flag[(int)Vertex_attrib_ordering::_count] =
  { Attrib_flag::Pos, Attrib_flag::Normal, Attrib_flag::Uv0, Attrib_flag::Color, Attrib_flag::Color };

  static const int c_default_atrb_stride[(int)Vertex_attrib_ordering::_count] =
  { sizeof(utl::Vec3f),sizeof(utl::Vec3f),sizeof(utl::Vec2f), sizeof(Rgba8), sizeof(Uv_region) };


  std::array< int, (int)Vertex_attrib_ordering::_count > atrb_offsets;
  atrb_offsets.fill(-1);

  int offset = 2 * sizeof(int); // skip header

  const bool use_default_ordering = !desc || desc->orderings.empty();
  int m = (int)Vertex_attrib_ordering::_count - 1;
  if (!use_default_ordering)
  {
    m = (int)desc->orderings.size();
    if (desc->orderings.back() == Vertex_attrib_ordering::Region)
      --m; // we'll process check for region later (if last, it may not be there, actually )
  }
  for (int i = 0; i < m; ++i)
  {
    auto code = use_default_ordering ? i : (int)desc->orderings[i];
    if (is_set(vb_attrib, c_to_atrb_flag[code]))
    {
      atrb_offsets[code] = offset;
      offset += vtx_count * c_default_atrb_stride[code];
    }
  }
  if (offset > i3s_buffer.size())
    return utl::log_warning(trk, IDS_I3S_INVALID_BINARY_BUFFER_SIZE, buffer_path, i3s_buffer.size(), offset);

  // fix erronous fid_count (older CE exporter) 
  if (offset == i3s_buffer.size())
  {
    //no feature, actually.
    vb_attrib &= ~(Attrib_flags)Attrib_flag::Feature_id;
    fid_count = 0;
  }
  // region may be declared, but actually missing from some nodes...
  if (is_set(vb_attrib, Attrib_flag::Region))
  {
    int fid_size = fid_count * (sizeof(int64_t) + sizeof(utl::Vec2i));
    auto left_over = i3s_buffer.size() - offset - fid_size;
    bool does_make_sense = left_over == 0 || left_over == sizeof(utl::Vec4<uint16_t>) * vtx_count;
    if (!does_make_sense)
      return utl::log_warning(trk, IDS_I3S_INVALID_FEATURE_COUNT_IN_BUFFER, buffer_path, fid_count);

    if (left_over == 0)
      vb_attrib &= ~(Attrib_flags)Attrib_flag::Region; //no region for this buffer
    else
    {
      atrb_offsets[(int)Vertex_attrib_ordering::Region] = offset; // ok, regions
      offset += left_over;
    }
  }
  bool is_ok = true;

  // --- shallow views on vertex attributes (if present):
  out.rel_pos.values = create_attrib_view< utl::Vec3f >(i3s_buffer, atrb_offsets[(int)Vertex_attrib_ordering::Position], vtx_count, &is_ok);
  out.normals.values = create_attrib_view< utl::Vec3f >(i3s_buffer, atrb_offsets[(int)Vertex_attrib_ordering::Normal], vtx_count, &is_ok);
  out.uvs.values = create_attrib_view< utl::Vec2f >(i3s_buffer, atrb_offsets[(int)Vertex_attrib_ordering::Uv0], vtx_count, &is_ok);
  out.colors.values = create_attrib_view< Rgba8 >(i3s_buffer, atrb_offsets[(int)Vertex_attrib_ordering::Color], vtx_count, &is_ok);
  out.uv_region.values = create_attrib_view< Uv_region>(i3s_buffer, atrb_offsets[(int)Vertex_attrib_ordering::Region], vtx_count, &is_ok);

  if (!is_ok)
  {
    return utl::log_warning(trk, IDS_I3S_INVALID_BINARY_BUFFER_SIZE, buffer_path, i3s_buffer.size(), offset);
  }

  // Feature Id parsing:

  utl::Buffer_view< const utl::Vec2i> face_range;
  utl::Buffer_view< const int64_t> fid64;

  //feature stuff:
  if ((has_feature_attrib || fid_count > 0) && is_set(vb_attrib, Attrib_flag::Feature_id))
  {
    fid64 = create_attrib_view< int64_t >(i3s_buffer, offset, fid_count, &is_ok);
    offset += sizeof(int64_t) * fid_count;
    face_range = create_attrib_view< utl::Vec2i >(i3s_buffer, offset, fid_count, &is_ok);
    if (fid64.size() != face_range.size() || !is_ok)
    {
      I3S_ASSERT(false); //expects face_range 
      return false;
    }
    //index the fids using the face range:
    auto fid_indices = utl::Buffer::create_writable_typed_view<uint32_t>(vtx_count);
    //must zero the indices in case face-range are invalid (to prevent OOR lookup)
    memset(fid_indices.data(), 0x00, fid_indices.size() * sizeof(uint32_t));
    utl::Vec2i raw_vtx_range, clip_vtx_range, lo(0), hi(vtx_count - 1);
    int oor_warn_count = 0;
    for (int i = 0; i < fid_count; ++i)
    {
      raw_vtx_range = utl::Vec2i(face_range[i].x * 3, (face_range[i].y + 1) * 3 - 1);
      if (face_range[i].y < 0)
      {
        //workaround cooker bug:
        raw_vtx_range.y = hi.y;
        oor_warn_count++;
      }
      clip_vtx_range = utl::clamp(raw_vtx_range, lo, hi);
      if (clip_vtx_range != raw_vtx_range)
        ++oor_warn_count;
      for (int k = clip_vtx_range.x; k <= clip_vtx_range.y; ++k)
        fid_indices[k] = i;
    }
    if (oor_warn_count)
      return utl::log_warning(trk, IDS_I3S_INVALID_FEATURE_FACE_RANGE, buffer_path, oor_warn_count, fid_count);

    auto fids = utl::Buffer::create_writable_typed_view<uint64_t>(fid64.size());
    for (int i = 0; i < fids.size(); ++i)
      fids[i] = (uint64_t)fid64[i];

    out.fids.values = fids;
    out.fids.index = fid_indices;
  }
  else
  {
    //integrated mesh declares a "dummy" fid that we should ignore.
  }

  if (!is_ok)
    return false;

  //Index the regions:
  if (out.uv_region.values.size())
  {
    out.uv_region = index_values(out.uv_region.values);
  }
  return is_ok;
}


bool Mesh_buffer_impl::assign(
                                const utl::Vec3d & origin, utl::Raw_buffer_view& i3s_buffer
                              , bool has_feature_attrib, Attrib_flags vb_attrib
                              , const std::string& buffer_path
                              , const i3s::Geometry_schema_desc* desc
                              , utl::Basic_tracker* trk)
{
  Mesh_bulk_data bulk;
  if (parse_legacy_geometry( i3s_buffer, has_feature_attrib, vb_attrib, buffer_path, bulk, desc,trk))
  {
    bulk.origin = origin;
    assign(bulk);
    return true;
  }
  return false;
}


Texture_meta::Wrap_mode Mesh_buffer_impl::get_wrap_mode(int uv_set) const
{
  if (m_uvs.size() && m_uv_wrap_mode == Texture_meta::Wrap_mode::Not_set)
  {
    m_uv_wrap_mode = Texture_meta::Wrap_mode::None;
    for (int i = 0; i < m_uvs.size() && m_uv_wrap_mode != (int)Texture_meta::Wrap_mode::Wrap_xy; ++i)
    {
      using wm = Texture_meta::Wrap_mode;
      using ut = std::underlying_type< Texture_meta::Wrap_mode>::type;

      if (m_uvs[i].x > 1.0 || m_uvs[i].x < 0.0)
        m_uv_wrap_mode = static_cast<wm>(static_cast<ut>(m_uv_wrap_mode) | static_cast<ut>(wm::Wrap_x));
      if (m_uvs[i].y > 1.0 || m_uvs[i].y < 0.0)
        m_uv_wrap_mode = static_cast<wm>(static_cast<ut>(m_uv_wrap_mode) | static_cast<ut>(wm::Wrap_y));
    }
  }
  return m_uv_wrap_mode;
}


// -----------------------------------------------------------------------------------
//            struct Legacy_geometry_buffer_encoder
// -----------------------------------------------------------------------------------
struct Legacy_geometry_buffer_encoder
{
private:
  enum class Legacy_attr : int { pos = 0, normal, uv, color, region, fid, face_range, _count };
  template< class T, class Y = T > bool _encode(Legacy_attr what, const Mesh_attrb<T>& val);
public:
  Legacy_geometry_buffer_encoder(int vtx_count, int fid_count, uint32_t attrib_mask, int face_range_count);

  bool encode_positions(const Mesh_attrb<  utl::Vec3f>& val) { return _encode(Legacy_attr::pos, val); }
  bool encode_normals(const Mesh_attrb<  utl::Vec3f>& val) { return _encode(Legacy_attr::normal, val); }
  bool encode_uvs(const Mesh_attrb<  utl::Vec2f>& val) { return _encode(Legacy_attr::uv, val); }
  bool encode_colors(const Mesh_attrb<  utl::Vec4<uint8_t>>& val);
  bool encode_regions(const Mesh_attrb< Uv_region>& val) { return _encode(Legacy_attr::region, val); }
  bool encode_feature_indices(const Mesh_attrb< uint64_t>& val);

  utl::Raw_buffer_view finalize() const { utl::Raw_buffer_view  tmp; tmp.operator=(m_buffer); return tmp; }

private:
  static const int c_bpv[(int)Legacy_attr::_count];
  std::array< int, (int)Legacy_attr::_count> m_offsets;
  int m_n, m_m;
  int fr_count;
  //bool m_has_region;
  utl::Buffer_view<char> m_buffer;
};

constexpr const int Legacy_geometry_buffer_encoder::c_bpv[(int)Legacy_attr::_count] = { 12, 12, 8, 4, 8, 8, 8 };


Legacy_geometry_buffer_encoder::Legacy_geometry_buffer_encoder(int vtx_count, int fid_count, Attrib_flags attrib_mask, int face_range_count)
  : m_n(vtx_count), m_m(fid_count), fr_count(face_range_count) //, m_has_region()
{
  if (m_m < 1)
    m_m = 1; // PRO up to v2.3 expects at least one feature, otherwise won't draw.
  const int hdr_size = 2 * sizeof(int);
  // 3 + 3 + 2 + 1    pos, normal, uv0, color
  const int attr_size = (is_set(attrib_mask, Attrib_flag::Pos) ? 3 : 0) + (is_set(attrib_mask, Attrib_flag::Normal) ? 3 : 0) + (is_set(attrib_mask, Attrib_flag::Uv0) ? 2 : 0) +
    (is_set(attrib_mask, Attrib_flag::Color) ? 1 : 0) + (is_set(attrib_mask, Attrib_flag::Region) ? 2 : 0);
  const int expected_size = hdr_size + m_n * sizeof(float) * attr_size + m_m * sizeof(int64_t) + fr_count * 2 * sizeof(int);
  m_buffer = utl::Buffer::create_writable_view(nullptr, expected_size);
  memset(m_buffer.data(), 0x00, expected_size); //some attribute may not get written, so we should zero the buffer first.
  int iter = hdr_size;

  for (int i = 0; i < (int)Legacy_attr::_count; ++i)
  {
    m_offsets[i] = iter;

    if (i == (int)Legacy_attr::normal && !is_set(attrib_mask, Attrib_flag::Normal) ||
        i == (int)Legacy_attr::uv && !is_set(attrib_mask, Attrib_flag::Uv0) ||
        i == (int)Legacy_attr::color && !is_set(attrib_mask, Attrib_flag::Color) ||
        i == (int)Legacy_attr::region && !is_set(attrib_mask, Attrib_flag::Region))
    {
      continue;
    }

    iter += c_bpv[i] * (i < (int)Legacy_attr::fid ? m_n : (i < (int)Legacy_attr::face_range ? m_m : fr_count));
  }
  int* hdr = reinterpret_cast<int*>(m_buffer.data());
  hdr[0] = m_n;
  hdr[1] = m_m;
  I3S_ASSERT(iter == expected_size);
}

template< class T, class Y > bool Legacy_geometry_buffer_encoder::_encode(Legacy_attr what, const Mesh_attrb<T>& val)
{
  I3S_ASSERT((int)what < (int)Legacy_attr::_count);
  I3S_ASSERT_EXT(c_bpv[(int)what] == sizeof(Y)); //static assert
  const int count = val.size();
  if (!count)
    return true;
  if (count != (what < Legacy_attr::fid ? m_n : m_m))
  {
    I3S_ASSERT(false);
    return false;
  }
  int expected_bytes = m_offsets[(int)what + 1] - m_offsets[(int)what];
  if (expected_bytes > 0 && expected_bytes != sizeof(Y) * count)
  {
    I3S_ASSERT(false);
    return false;
  }
  Y* dst = reinterpret_cast<Y*>(m_buffer.data() + m_offsets[(int)what]);

  if (!val.index.size() && sizeof(T) == sizeof(Y))
  {
    memcpy(dst, val.values.data(), val.values.size() * sizeof(T));
  }
  else
  {
    for (int i = 0; i < count; ++i)
    {
      dst[i] = static_cast<Y>(val[i]);
    }
  }
  return true;
}

bool Legacy_geometry_buffer_encoder::encode_colors(const Mesh_attrb<utl::Vec4<uint8_t>>& val)
{
  constexpr auto attr = static_cast<int>(Legacy_attr::color);
  if (m_offsets[attr + 1] != m_offsets[attr] + m_n * sizeof(Rgba8))
  {
    // encode_colors should only be called if Legacy_attr::color was set for the constructor
    I3S_ASSERT(false);
    return false;
  }

  auto dst = reinterpret_cast<Rgba8*>(m_buffer.data() + m_offsets[attr]);
  const auto count = val.size();
  if (count == 0)
  {
    // Fill with default color.
    constexpr Rgba8 default_color(0xff, 0xff, 0xff, 0xff);
    std::fill_n(dst, m_n, default_color);
    return true;
  }

  if (count != m_n)
  {
    I3S_ASSERT(false);
    return false;
  }

  if (val.index.size() == 0)
    std::copy_n(val.values.data(), m_n, dst);
  else
  {
    for (int i = 0; i < count; ++i)
      dst[i] = val.values[val.index[i]];
  }

  return true;
}

bool Legacy_geometry_buffer_encoder::encode_feature_indices(const Mesh_attrb< uint64_t>& val)
{
  int64_t* dst_fid = reinterpret_cast<int64_t*>(m_buffer.data() + m_offsets[(int)Legacy_attr::fid]);
  utl::Vec2i* dst_fr = reinterpret_cast<utl::Vec2i*>(m_buffer.data() + m_offsets[(int)Legacy_attr::face_range]);

  if (m_m == 1 && val.index.size() == 0)
  {
    //this is a special case where we create a dummy feature to keep Pro 2.3- happy:
    int64_t tmp = 0;
    memcpy(dst_fid, &tmp, sizeof(tmp));
    I3S_ASSERT(m_n % 3 == 0);
    utl::Vec2i face_range_all(0, m_n / 3-1); //inclusive range.
    memcpy(dst_fr, &face_range_all, sizeof(face_range_all));
    return true;
  }
  if (!val.index.size() || val.values.size() != m_m || val.size() != m_n)
  {
    I3S_ASSERT_EXT(false); //not implemented.
    return false;
  }

  // this one is different, we have to rebuild the face-range:
  std::vector< utl::Vec2i> fr;
  fr.reserve(m_m);
  const int* i0 = reinterpret_cast<const int*>(val.index.data());
  const int* i1 = i0;
  const int* i2 = i0;
  const int* end = i1 + val.index.size();
  do
  {
    while (i2 < end && *i1 == *i2)
    {
      if (*i1 > *i2)
      {
        I3S_ASSERT(false); //not a valid face range...
        return false;
      }
      ++i2;
    }
    if (((i1 - i0) % 3) != 0 || ((i2 - i0) % 3) != 0)
    {
      I3S_ASSERT(false); //not a valid face range...
      return false;

    }
    auto tri_a = (i1 - i0) / 3;
    auto tri_b = (i2 - 1 - i0) / 3;
    fr.push_back(utl::Vec2i((int)tri_a, (int)tri_b));
    i1 = i2;
    //++i2;
  } while (i2 < end);
  if (fr.size() != fr_count)
  {
    I3S_ASSERT_EXT(false); //not a valid face range
    return false;
  }
  //write the fid:
  //Note: this pointer could be misaligned:
  for (int i = 0; i < val.values.size(); i++)
  {
    dst_fid[i] = static_cast<int64_t>(val.values[i]);
  }
  copy_elements(dst_fr, fr.data(), fr.size());
  return true;
}


template< class T > int compare_mesh_attribute(const T* a, const T* b, int count)
{
  double c_epsi = 1e-4;
  int diff = 0;
  for (int i = 0; i < count; ++i)
  {
    if (!is_equal(a[i], b[i], c_epsi))
    {
      ++diff;
    }
  }
  return diff;
}

//! For compatibility reason, we may need to add dummy vertex attributes if missing:
utl::Raw_buffer_view encode_legacy_buffer(const Mesh_abstract&  mesh, uint32_t* out_actual_attributes_mask )
{
  if (out_actual_attributes_mask)
    *out_actual_attributes_mask = 0;
  uint32_t attrib_mask = mesh.get_available_attrib_mask();
  // pad with "default" 1.6 attributes:
  // NOTE:
  //      If all nodes were to not have a specific attribute, we wouldn't need to include it
  //      but since we write geometry buffers as we go, we can't predict this.
  attrib_mask |= (Attrib_flags)Attrib_flag::Legacy_no_region;

  // If there are no normals, then the normals were dropped, or they do not exist in the source data.
  if (!mesh.get_normals().size())
    attrib_mask &= ~(Attrib_flags)Attrib_flag::Normal;

  // If there are no colors, then the colors were dropped, or they do not exist in the source data.
  if(!mesh.get_colors().size())
    attrib_mask &= ~(Attrib_flags)Attrib_flag::Color;

  // if this mesh came from Draco, the number of face ranges may not equal the number of fids,
  // due to Draco re-ordering of vertices
  // e.g # fids = 2. indices: 0, 0, 0,..., 0, 1, 1, 1, ..., 1, 0, 0, 0 (3 face ranges)
  int fr_size = 0;
  if (mesh.get_feature_ids().size() > 1)
  {
    auto fid_indices = mesh.get_feature_ids().index;
    uint32_t last_fid_idx = std::numeric_limits<uint32_t>::max();
    for (auto idx : fid_indices)
    {
      if (last_fid_idx != idx)
      {
        ++fr_size;
        last_fid_idx = idx;
      }
    }
  }
  else
  {
    // at least 1 fid is expected
    fr_size = 1;
  }

  Legacy_geometry_buffer_encoder encoder(mesh.get_vertex_count(), mesh.get_feature_count(), attrib_mask, fr_size);
  bool successful_encoding = true;

  utl::for_each_bit_set_conditional(attrib_mask, [&](int bit_number)
  {
    Attrib_flag f = (Attrib_flag)(1 << bit_number);
    switch (f) {
      case Attrib_flag::Pos:
        successful_encoding = encoder.encode_positions(mesh.get_relative_positions());
        break;
      case Attrib_flag::Normal:
        successful_encoding = encoder.encode_normals(mesh.get_normals());
        break;
      case Attrib_flag::Uv0:
        successful_encoding = encoder.encode_uvs(mesh.get_uvs(0));
        break;
      case Attrib_flag::Color:
        successful_encoding = encoder.encode_colors(mesh.get_colors());
        break;
      case Attrib_flag::Region:
        successful_encoding = encoder.encode_regions(mesh.get_regions());
        break;
      case Attrib_flag::Feature_id:
        successful_encoding = encoder.encode_feature_indices(mesh.get_feature_ids());
        break;
      default:
        I3S_ASSERT(false);
    }

    if (!successful_encoding)
      return false;
    *out_actual_attributes_mask = attrib_mask;
    return true;
  });

  if (successful_encoding)
  {
    auto ret = encoder.finalize();
#if 0 
    bool c_has_attrib = true;
    Mesh_abstract::Ptr sanity(msh::parse_mesh_from_i3s(mesh.get_origin(), ret, c_has_attrib));
    int n = mesh.get_colors().size();
    for (int i = 0; i < n; ++i)
    {
      const uint32_t& a = (const uint32_t&)(mesh.get_colors()[i]);
      const uint32_t& b = (const uint32_t&)(sanity->get_colors()[i]);
      std::cout << "Color[" << i << "]" << (a - b) << std::endl;
    }
#endif

    return ret;
  }
  else
  {
    I3S_ASSERT(false);
    return utl::Raw_buffer_view();
  }
}

}

} // namespace i3slib
