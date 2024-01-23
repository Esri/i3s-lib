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
#include "i3s/i3s_writer_impl.h"
#include "i3s/i3s_index_dom.h"
#include "utils/utl_prohull.h"
#include "i3s/i3s_legacy_mesh.h"
#include "utils/utl_gzip.h"
#include "utils/utl_jpeg.h"
#include "utils/utl_box.h"
#include "utils/utl_bitstream.h"
#include "i3s/i3s_legacy_shared_dom.h"
#include "utils/utl_i3s_resource_defines.h"
#include "utils/utl_fs.h"
#include "utils/utl_mime.h"
#include "utils/utl_lock.h"
#include "utils/utl_image_resize.h"
#include "utils/utl_json_helper.h"
#include "utils/utl_geom.h"
#include "utils/utl_slpk_writer_api.h"
#include "utils/utl_envelope.h"
#include "utils/utl_string.h"
#include "i3s/i3s_pages_localsubtree.h"
#include "i3s/i3s_pages_breadthfirst.h"
#include "i3s/i3s_attribute_buffer_encoder.h"

#include <stdint.h>
#include <set>
#include <deque>
#include <ctime>

namespace i3slib
{

namespace i3s
{


// ---------------------------------------------------------------------------------------------
//        Helpers:
// ---------------------------------------------------------------------------------------------
  namespace
  {
    template<typename... Args>
    status_t log_error_s(utl::Basic_tracker* tracker, int code, Args&&... args)
    {
#if defined(__EMSCRIPTEN__) && defined(_DEBUG)
      printf("log_error CRITICAL code=%d\n", code);
#else
      utl::Basic_tracker::log(tracker, utl::Log_level::Critical, code, std::forward<Args>(args)...);
#endif
      return status_t(code);
    }
  }
  enum Tri_state : int { True, False, Not_set };

static std::string to_epsg_url(const Spatial_reference_desc& def)
{
  int id = def.latest_wkid != Spatial_reference::c_wkid_not_set ? def.latest_wkid : def.wkid;
  if (id == Spatial_reference::c_wkid_not_set)
    return std::string();
  auto epsg = "http://www.opengis.net/def/crs/EPSG/0/" + std::to_string(id);
  return epsg;
}

/*
'to_full_extent' computes a full extent from an obb.

Under the assumption that a line segment of the obb in cartesian space
is very close to a line segment when projected in the spatial reference space,
we can limit ourselves to taking into account corners of the obb.

But this assumption breaks as the obb dimensions grow.

For example, consider this obb containing all of earth:

 ---------  <- obb
 |       |
 |  /\   |
 |  \/   |
 | earth |
 ---------

 all of earth is contained in the obb, so the theoreticall full extent should be :
 lon : [-180 , 180]
 lat : [-90 , 90]
 z   : [0 (or -radius of earth), max altitude of an obb corner]

However if we zoom-in on earth, we see that the projection of corners (x) might be:

     North pole
        __
      /x  x\ <- latitude 60
     /      \
    |        | <- equator
     \      /
      \x  x/ <- latitude -60
        --
     South pole

Or, using an equirectangular projection:

   _________________________
  |                         |
  |  x      x     x      x  | <- latitude 60
  |                         |
  |                         | <- equator
  |                         |
  |  x      x     x      x  | <- latitude -60
  |                         |
   -------------------------
-180           0          180  (longitude)

 so if we only take corners of the obb into account, we will have a full extent resembling:

 lon : -140 140
 lat : -60 60
 z   : [min altitude of an obb corner, max altitude of an obb corner]

 As a stop-gap measure, if the aabb in cartesian space also contains the center of the earth,
 we hardcode the full extent to be the full earth.

 (TODO) But to fix this accurately, in the general case, we should not only consider the corners of the obb,
 but also every point on the surface of the obb.
*/

//! TODO: this function won't work well for [0,360] type of GCS data
Full_extent to_full_extent(const Spatial_reference_xform& xform, const utl::Obb_abs& dst_obb_)
{
  auto obb = dst_obb_;
  to_dst_cartesian(xform, &obb.center);

  std::array< utl::Vec3d, 8  > corners;
  obb.get_corners(&corners[0], 8);

  utl::Boxd aabb;
  for (const utl::Vec3d& corner : corners)
  {
    aabb.expand(corner);
  }
  const bool aabb_contains_center_of_earth = aabb.is_inside(utl::Vec3d{ 0. });

  from_dst_cartesian(xform, &corners[0], 8);

  utl::Vec3d pt[2] = { corners[0], corners[0] };
  for (auto p : corners)
  {
    pt[0] = min(pt[0], p);
    pt[1] = max(pt[1], p);
  }
  Full_extent full_extent;
  if (aabb_contains_center_of_earth)
  {
    full_extent.xmin = -180.;
    full_extent.xmax = 180.;
    full_extent.ymin = -90.;
    full_extent.ymax = 90.;
    full_extent.zmin = 0.; // TODO - radius of earth?
    full_extent.zmax = pt[1].z;
  }
  else
  {
    full_extent.xmin = pt[0].x;
    full_extent.ymin = pt[0].y;
    full_extent.zmin = pt[0].z;
    full_extent.xmax = pt[1].x;
    full_extent.ymax = pt[1].y;
    full_extent.zmax = pt[1].z;
  }
  return full_extent;
}

static Mime_image_format to_mime_type(Image_format format)
{
  switch (format)
  {
    case    Image_format::Jpg: return Mime_image_format::Jpg;
    case    Image_format::Png: return Mime_image_format::Png;
    case    Image_format::Dds: return Mime_image_format::Dds;
    case    Image_format::Ktx: return Mime_image_format::Ktx;
    case    Image_format::Basis: return Mime_image_format::Basis;
    case    Image_format::Ktx2: return Mime_image_format::Ktx2;
    default:
      I3S_ASSERT(false);
      return Mime_image_format::Not_set;
  }
}

static utl::Mime_type  to_utl_mime_type( Mime_image_format format)
{
  switch (format)
  {
    case    Mime_image_format::Jpg: return utl::Mime_type::Jpeg;
    case    Mime_image_format::Png: return utl::Mime_type::Png;
    case    Mime_image_format::Dds: return utl::Mime_type::Dds_proper;
    case    Mime_image_format::Ktx: return utl::Mime_type::Ktx;
    case    Mime_image_format::Basis: return utl::Mime_type::Basis;
    case    Mime_image_format::Ktx2: return utl::Mime_type::Ktx2;
    default:
      I3S_ASSERT(false);
      return utl::Mime_type::Not_set;
  }
}


std::string to_compatibility_tex_name(Image_format f, Texture_semantic sem)
{
  // jpg, png
  if ((Image_formats)Image_format::Default & (Image_formats)f)
  {
    switch (sem)
    {
    case i3slib::i3s::Texture_semantic::Normal_map:
      return "2";
    case i3slib::i3s::Texture_semantic::Metallic_roughness:
      // same as occlusion map. metallic roughness data in channels GB
    case i3slib::i3s::Texture_semantic::Occlusion_map:
      return "3";
    case i3slib::i3s::Texture_semantic::Emissive_texture:
      return "4";
    //case i3slib::i3s::Texture_semantic::Diffuse_texture: // not yet supported
    default:
      break;
    }
  }
  // DDS. same as City Engine
  else if ((Image_formats)Image_format::Dds & (Image_formats)f)
  {
    switch (sem)
    {
    case i3slib::i3s::Texture_semantic::Normal_map:
      return "8";
    case i3slib::i3s::Texture_semantic::Metallic_roughness:
      // same as occlusion map.
    case i3slib::i3s::Texture_semantic::Occlusion_map:
      return "9";
    case i3slib::i3s::Texture_semantic::Emissive_texture:
      return "10";
    //case i3slib::i3s::Texture_semantic::Diffuse_texture: // not yet supported
    default:
      break;
    }
  }
  // Basis
  else if ((Image_formats)Image_format::Ktx2 & (Image_formats)f)
  {
    switch (sem)
    {
    case i3slib::i3s::Texture_semantic::Normal_map:
      return "5";
    case i3slib::i3s::Texture_semantic::Metallic_roughness:
      // same as occlusion map. metallic roughness data in channels GB
    case i3slib::i3s::Texture_semantic::Occlusion_map:
      return "6";
    case i3slib::i3s::Texture_semantic::Emissive_texture:
      return "7";
    //case i3slib::i3s::Texture_semantic::Diffuse_texture: // not yet supported
    default:
      break;
    }
  }
  return to_compatibility_tex_name(f); // Base color tex
}

std::string to_compatibility_tex_name(Image_format f)
{
  switch (f)
  {
    case Image_format::Dds:
      return "0_0_1";
    case Image_format::Ktx:
      return "0_0_2"; // not tested. Hope that the old portal will be let it go. 
    case Image_format::Basis: //remove after run time is done removing basis file support
      return "2";
    case Image_format::Ktx2:
        return "1";
    default:
      return "0";
  }
}


static std::vector< std::string > get_texture_mime_types(Image_formats flags)
{
  std::vector< std::string > ret;
  utl::for_each_bit_set_conditional(flags, [&](int bit_number)
  {
    Image_format f = (Image_format)(1 << bit_number);
    ret.emplace_back(to_string(to_mime_type(f)));
    return true;
  });
  return ret;
}

void compute_obb(const Spatial_reference_xform& xform,
    std::vector<utl::Vec3d>& points, utl::Pro_hull& hull, utl::Obb_abs& obb, utl::Vec4d& mbs)
{
  //to cartesian space...
  [[maybe_unused]] bool ret = to_dst_cartesian(xform, points.data(), static_cast<int>(points.size()));
  I3S_ASSERT(ret);
  // compute OBB:
  double radius;
  hull.get_ball_box(points, obb, radius);
  //enforce a minimum extent for single points and points located at the same location:
  if (radius == 0. && !points.empty())
  {
    radius = 1.0;
    obb.extents = { 1.0f, 1.0f, 1.0f };
  }
  //convert center back:
  from_dst_cartesian(xform, &obb.center, 1);

  mbs = utl::Vec4d(obb.center, radius);
}

void compute_aabb(const Spatial_reference_xform& xform,
  utl::Vec3d * points, size_t count_points, utl::Box<double> & aabb, double & radius)
{
  using utl::Vec3d;

  //to cartesian space...
  [[maybe_unused]] bool ret = to_dst_cartesian(xform, points, static_cast<int>(count_points));
  I3S_ASSERT(ret);

  aabb.set_empty();
  for(size_t i=0; i<count_points; ++i)
    aabb.expand(points[i]);

  radius = 0.;
  const auto center = aabb.center();
  for (size_t i = 0; i < count_points; ++i)
    radius = std::max(radius, center.distance_sqr(points[i]));
  radius = std::sqrt(radius);
}

namespace
{

void compute_obb(const Spatial_reference_xform& xform
  , const utl::Vec3d* points, int count, utl::Obb_abs& obb, utl::Vec4d& mbs)
{
  utl::Pro_hull hull(utl::Pro_set::get_default());

  //deep copy:
  std::vector< utl::Vec3d > points_copy(count);
  copy_elements(points_copy.data(), points, count);

  compute_obb(xform, points_copy, hull, obb, mbs);
}


} // namespace

void compute_obb(
  const Spatial_reference_xform& xform,
  const std::vector< utl::Obb_abs>& child_obbs,
  utl::Pro_hull & hull,
  std::vector<utl::Vec3d> & scratch_corners,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs)
{
  scratch_corners.clear();

  size_t corner_count = child_obbs.size() * 8;
  if (obb.is_valid())
    corner_count += 8;

  scratch_corners.resize(corner_count);

  size_t offset = 0;
  for (const auto& child_obb : child_obbs)
  {
    //get corners in cartesian space:
    auto center = child_obb.center;
    to_dst_cartesian(xform, &center);
    utl::Obb_abs(center, child_obb.extents, child_obb.orientation).get_corners(&scratch_corners[offset], 8);
    offset += 8;
  }

  if (offset < corner_count)
  {
    to_dst_cartesian(xform, &obb.center);
    obb.get_corners(&scratch_corners[offset], 8);
  }

  // compute OBB:
  double radius;
  hull.get_ball_box(scratch_corners, obb, radius);

  //convert center back:
  from_dst_cartesian(xform, &obb.center);
  mbs = utl::Vec4d(obb.center, radius);
}

void compute_obb(
  const Spatial_reference_xform& xform,
  const std::vector< utl::Obb_abs>& child_obbs,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs)
{
  utl::Pro_hull hull(utl::Pro_set::get_default());
  std::vector<utl::Vec3d> corners;

  compute_obb(xform, child_obbs, hull, corners, obb, mbs);
}

status_t project_update_mesh_origin_and_obb(
  utl::Basic_tracker* trk,
  Layer_type layer_type,
  const Spatial_reference_xform& xform,
  Mesh_abstract& mesh,
  const std::vector<utl::Obb_abs>& ch_obbs,
  utl::Obb_abs& obb,
  utl::Vec4d& mbs)
{
  std::vector<utl::Vec3d> abs_pos_values;
  utl::Vec3d* abs_positions = nullptr;
  int abs_positions_count = 0;
  if (layer_type == Layer_type::Point)
  {
    auto& mesh_abs_positions = mesh.get_absolute_positions();
    abs_positions = const_cast<utl::Vec3d*>(mesh_abs_positions.data());
    abs_positions_count = mesh_abs_positions.size();
  }
  else
  {
    auto& mesh_rel_positions = mesh.get_relative_positions();
    abs_positions_count = mesh_rel_positions.values.size();
    abs_pos_values.reserve(abs_positions_count);
    auto origin = mesh.get_origin();
    for (auto& p : mesh_rel_positions.values)
      abs_pos_values.push_back(utl::Vec3d(p) + origin);
    abs_positions = abs_pos_values.data();
  }
  // "project" to destination SR. Successful No-op if Src_sr == Dst_sr. 
  auto hr = xform.transform(
    Spatial_reference_xform::Sr_type::Src_sr, 
    Spatial_reference_xform::Sr_type::Dst_sr, 
    abs_positions, abs_positions_count);
  if (hr != Spatial_reference_xform::Status_t::Ok)
    return log_error_s(trk, IDS_I3S_PROJ_ENGINE_TRANS_ERROR);
  compute_obb(xform, abs_positions, abs_positions_count, obb, mbs);

  // update obb, including children
  if (ch_obbs.size())
    compute_obb(xform, ch_obbs, obb, mbs);

  // Update relative positions to reflect abs positions.
  if (!mesh.update_positions(obb.center, abs_positions, abs_positions_count))
  {
    return log_error_s(trk, IDS_I3S_INTERNAL_ERROR, std::string("position update"));
  }
  return IDS_I3S_OK;
}

utl::Vec3f get_somewhat_anisotropic_scale(Writer_context& ctx, const Spatial_reference_xform& xform, const Mesh_abstract& mesh)
{
  //estimate X/Y scale if globe mode:
  utl::Vec3d frame[3];
  frame[0] = mesh.get_origin();
  frame[1] = frame[0] + utl::Vec3d(1.0, 0.0, 0.0);
  frame[2] = frame[1] + utl::Vec3d(0.0, 1.0, 0.0);

  to_dst_cartesian(xform, frame, 3);

  double scale_x = (frame[1] - frame[0]).length();
  double scale_y = (frame[2] - frame[0]).length();

  const double c_epsi = 10.0;
  return (std::abs(scale_x - 1.0) > c_epsi || std::abs(scale_x - 1.0) > c_epsi) ? 
    utl::Vec3f((float)scale_x, (float)scale_y, 1.0)
    : utl::Vec3f(1.0);
}

Esri_field_type to_esri_type(Type type)
{
  switch (type)
  {
    case Type::Float32: return Esri_field_type::Single;
    case Type::Float64: return Esri_field_type::Double;
    case Type::UInt16:
    case Type::Int16:
    case Type::UInt32:
    case Type::Int32: return Esri_field_type::Integer;
    case Type::UInt8:
    case Type::Int8: return Esri_field_type::Small_integer;
    case Type::Oid64:
    case Type::Oid32: return Esri_field_type::Oid;
    case Type::String_utf8:
      return Esri_field_type::String;
    case Type::Date_iso_8601:
      return Esri_field_type::Date;
    case Type::Global_id:
      return Esri_field_type::Global_id;
    case Type::Guid:
      return Esri_field_type::Guid;
    case Type::UInt64:  // TODO : document why 64bit integers are mapped to doubles
    case Type::Int64:   // TODO : document why 64bit integers are mapped to doubles
      return Esri_field_type::Double;
    default:
      I3S_ASSERT(false);
      return Esri_field_type::Double;
  }
}

std::vector< Field_desc > create_fields(const std::vector<Attribute_definition>& descs)
{
  std::vector< Field_desc > ret(descs.size());
  for (int i = 0; i < ret.size(); ++i)
  {
    Field_desc& f = ret[i];

    f.name = descs[i].meta.name;
    f.type = (descs[i].esri_type_overload != Esri_field_type::Not_set) ? descs[i].esri_type_overload : to_esri_type(descs[i].type);
    f.alias = descs[i].meta.alias;

    if (descs[i].coded_values)
    {
      f.domain.type = Domain_type::CodedValue;
      f.domain.name = descs[i].meta.name;
      f.domain.field_type = f.type;

      f.domain.coded_values.clear();

      for (const Domain_coded_value_definition& c : *descs[i].coded_values)
      {
        f.domain.coded_values.emplace_back();
        f.domain.coded_values.back().name = c.name;
        f.domain.coded_values.back().code = c.code;
      }
    }
  }
  return ret;
}

static std::string  get_res_path(const std::string& name, const std::string& ref_path)
{
  return utl::to_string(utl::make_path(utl::u8path(ref_path), utl::u8path(name)));
}

bool _pack(utl::Basic_tracker* trk, std::string* buf, utl::Mime_type type, utl::Mime_encoding* encoding
               , Gzip_context& gzip)
{
  bool uncompressed_texture_fmt = type == utl::Mime_type::Jpeg || type == utl::Mime_type::Png || type == utl::Mime_type::Basis || type == utl::Mime_type::Ktx2;
  if (*encoding == utl::Mime_encoding::Not_set && !uncompressed_texture_fmt)
  {
    if (!gzip.compress_inplace(buf))
      return false;
    *encoding = utl::Mime_encoding::Gzip;
  }
  return true;
}

static utl::Mime_type hack_mime_type_for_legacy(utl::Mime_type type)
{
  switch (type)
  {
    //case utl::Mime_type::Jpeg: //try to keep .jpg extension
    case utl::Mime_type::Png: return utl::Mime_type::Binary;
    case utl::Mime_type::Dds_proper: return utl::Mime_type::Dds_legacy;
    default: return type;
  }
}

status_t append_to_slpk_(utl::Basic_tracker* trk, utl::Slpk_writer* out, const std::string& name, const std::string& ref_path, const char* buf, int n_bytes,
                         utl::Mime_type type, const utl::Mime_encoding encoding, int* out_size)
{
  auto path = get_res_path(name, ref_path);

  //hack mime-type for legacy:
  type = hack_mime_type_for_legacy(type); // :sad: 

 // utl::add_slpk_extension_to_path(&path, type, *encoding);

  auto ret = out->append_file(path, buf, n_bytes, type, encoding);
  if (!ret)
  {
    return log_error_s(trk, IDS_I3S_IO_WRITE_FAILED, std::string("SLPK://" + path));
  }
  if (out_size)
    *out_size = n_bytes;
  return IDS_I3S_OK;
}

static status_t append_to_slpk(utl::Basic_tracker* trk, utl::Slpk_writer* out, const std::string& name, const std::string& ref_path,
  std::string* buf, utl::Mime_type type, utl::Mime_encoding* encoding, Gzip_context& gzip, int* out_size)
{
  const bool uncompressed_texture_fmt = type == utl::Mime_type::Jpeg || type == utl::Mime_type::Png || type == utl::Mime_type::Basis || type == utl::Mime_type::Ktx2;
  if (*encoding == utl::Mime_encoding::Not_set && !uncompressed_texture_fmt)
  {
    if (!gzip.compress_inplace(buf))
    {
      auto res_name_for_error_report = get_res_path(name, ref_path);
      log_error_s(trk, IDS_I3S_COMPRESSION_ERROR, res_name_for_error_report, std::string("GZIP"));
      return IDS_I3S_COMPRESSION_ERROR;
    }
  }
  *encoding = utl::Mime_encoding::Gzip;
  if (uncompressed_texture_fmt)
    *encoding = utl::Mime_encoding::Not_set;

  return append_to_slpk_(trk, out, name, ref_path, buf->data(), static_cast<int>(buf->size()), type, *encoding, out_size);
}

enum class Use_gzip { No, Yes };

static status_t append_to_slpk(utl::Basic_tracker* trk, utl::Slpk_writer* out, const std::string& name, const std::string& ref_path,
                               const utl::Raw_buffer_view& buf, utl::Mime_type type, utl::Mime_encoding* encoding, Gzip_context& gzip, int* out_size = nullptr, const Use_gzip gz = Use_gzip::Yes)
{
  if (gz == Use_gzip::Yes)
  {
    thread_local std::string tmp;
    tmp.resize(buf.size());
    memcpy(&tmp[0], buf.data(), tmp.size());
    return append_to_slpk(trk, out, name, ref_path, &tmp, type, encoding, gzip, out_size);
  }
  else
  {
    return append_to_slpk_(trk, out, name, ref_path, buf.data(), buf.size(), type, *encoding, out_size);
  }
}

static status_t save_json(utl::Basic_tracker* trk, utl::Slpk_writer* out, std::string& json_content, const std::string& name, const std::string& ref, Gzip_context& gzip)
{
  auto encoding = utl::Mime_encoding::Not_set;
  return append_to_slpk(trk, out, name, ref, &json_content, utl::Mime_type::Json, &encoding, gzip, nullptr);
}

template< class T > static status_t save_json(utl::Basic_tracker* trk, utl::Slpk_writer* out, const T& obj, const std::string& name, const std::string& ref, Gzip_context & gzip)
{
  //to json:
  auto json_content = utl::to_json(obj);
  if (json_content.empty())
    return log_error_s(trk, IDS_I3S_INTERNAL_ERROR, std::string("Empty JSON"));

  auto encoding = utl::Mime_encoding::Not_set;
  return append_to_slpk(trk, out, name, ref, &json_content, utl::Mime_type::Json, &encoding, gzip, nullptr);
}

static std::optional<size_t> find_tex(const Multi_format_texture_buffer& src, Image_format format)
{
  auto found = std::find_if(src.begin(), src.end(), [format](const Texture_buffer& t) { return t.meta.format == format; });
  if (found == src.end())
    return std::nullopt;
  return std::distance(src.begin() , found);
}

static int get_alpha_bits(const Texture_buffer& img)
{
  switch (img.meta.format)
  {
    case Image_format::Raw_rgb8:
      return 0;
    case Image_format::Raw_rgba8:
    {
      I3S_ASSERT_EXT((img.data.size() % 4) == 0);
      const uint32_t* pix32 = reinterpret_cast<const uint32_t*>(img.data.data());
      const uint32_t* end = reinterpret_cast<const uint32_t*>(img.data.data() + img.data.size());
      //uint32_t n = img.data.size();
      uint32_t lo = 256;
      while (pix32 < end)
      {
        uint32_t a = ((*(pix32++)) >> 24) & 0xFF;
        if (a > 0 && a < 255)
          return 8;
        lo = std::min(lo, a);
      }
      return lo == 255 ?
        0 : //opaque image
        1; // 1-bit mask image ( 0 or 255 only)
    }
    default:
      I3S_ASSERT_EXT(false);
      break;
  }
  return 0;
}

namespace
{

std::pair<int, int> apply_texture_size_constraints(
  const Texture_buffer& raw_img,
  int max_dimension,
  const std::string& res_id_for_error_only,
  utl::Basic_tracker* trk)
{
  auto w = raw_img.width();
  if (w > max_dimension)
  {
    utl::log_info(trk, IDS_I3S_IMAGE_TOO_LARGE, res_id_for_error_only, max_dimension, w);
    w = max_dimension;
  }

  auto h = raw_img.height();
  if (h > max_dimension)
  {
    utl::log_info(trk, IDS_I3S_IMAGE_TOO_LARGE, res_id_for_error_only, max_dimension, h);
    h = max_dimension;
  }

  return { w, h };
}

std::pair<int, int> apply_pot_texture_size_constraints(
  const Texture_buffer& raw_img,
  int max_dimension,
  const std::string& res_id_for_error_only,
  utl::Basic_tracker* trk)
{
  auto w = raw_img.width();
  if (w > max_dimension)
  {
    utl::log_info(trk, IDS_I3S_IMAGE_TOO_LARGE, res_id_for_error_only, max_dimension, w);
    w = utl::round_down_power_of_two(max_dimension);
  }
  else if (!utl::is_power_of_two(w))
  {
    utl::log_info(trk, IDS_I3S_DXT_NPOT_IMAGE, res_id_for_error_only, w);
    w = utl::round_up_power_of_two(w);
    if (w > max_dimension)
      w >>= 1;
  }

  auto h = raw_img.height();
  if (h > max_dimension)
  {
    utl::log_info(trk, IDS_I3S_IMAGE_TOO_LARGE, res_id_for_error_only, max_dimension, h);
    h = utl::round_down_power_of_two(max_dimension);
  }
  else if (!utl::is_power_of_two(h))
  {
    utl::log_info(trk, IDS_I3S_DXT_NPOT_IMAGE, res_id_for_error_only, h);
    h = utl::round_up_power_of_two(h);
    if (h > max_dimension)
      h >>= 1;
  }

  return { w, h };
}

void scale_texture(const Texture_buffer& img, int w, int h, Texture_buffer& out)
{
  const int pixel_size = img.meta.format == Image_format::Raw_rgb8 ? 3 : 4;
  auto resampled_buffer = std::make_shared<utl::Buffer>(w * h * pixel_size);

  utl::resample_2d_uint8(img.width(), img.height(), w, h, img.data.data(),
    resampled_buffer->create_writable_view().data(), pixel_size, utl::Alpha_mode::Pre_mult);

  out.data = resampled_buffer->create_view();
  out.meta = img.meta;
  out.meta.mip0_width = w;
  out.meta.mip0_height = h;
}

}

//! generate the missing texture format based on what texture encoder have been provided in the context
static status_t create_texture_set(const Writer_context& builder_ctx, Multi_format_texture_buffer& tex_set, const std::string& res_id_for_error_only)
{
  if (!builder_ctx.decoder)
    return IDS_I3S_INTERNAL_ERROR;

  Image_formats desired_tex_out = (Image_formats)Image_format::Default;

  // If we do not wish to use the original src texture set(default)
  // Depending on the different supported encoders, new textures will be generated.
  if (!builder_ctx.use_src_tex_set)
  {
    if (builder_ctx.decoder->encode_to_dxt_with_mips)
      desired_tex_out |= (Image_formats)Image_format::Dds;

    if (builder_ctx.encode_to_etc2_with_mips)
      desired_tex_out |= (Image_formats)Image_format::Ktx;

    if (builder_ctx.encode_to_basis_with_mips)
      desired_tex_out |= (Image_formats)Image_format::Basis;

    if (builder_ctx.encode_to_basis_ktx2_with_mips)
      desired_tex_out |= (Image_formats)Image_format::Ktx2;
  }
  
  const std::map<Image_format, Writer_context::Encode_img_fct> encoder_funcs =
  {
    { Image_format::Jpg, builder_ctx.encode_to_jpeg },
    { Image_format::Png, builder_ctx.encode_to_png },
    { Image_format::Dds, builder_ctx.decoder->encode_to_dxt_with_mips },
    { Image_format::Ktx, builder_ctx.encode_to_etc2_with_mips },
    { Image_format::Basis, builder_ctx.encode_to_basis_with_mips },
    { Image_format::Ktx2, builder_ctx.encode_to_basis_ktx2_with_mips}
  };

  //From option above, set desired textures to image formats.
  //Get rid of jpg or png option, whichever is not included
  const std::optional<size_t> png_index = find_tex(tex_set, Image_format::Png);
  const auto img_fmts = desired_tex_out &
    ~(png_index.has_value() ? (Image_formats)Image_format::Jpg : (Image_formats)Image_format::Png);

  std::vector<Image_format> dst_item;
  Multi_format_texture_buffer out;
  utl::for_each_bit_set(img_fmts, [&desired_tex_out,&tex_set,&out,&dst_item](const int i)
    {
      const auto format = static_cast<Image_format>(1 << i);
      if (const std::optional<size_t> idx_buffer = find_tex(tex_set, format))
      {
        out.push_back(tex_set[*idx_buffer]);
        desired_tex_out &= ~static_cast<Image_formats>(format);
      }
      else
        dst_item.push_back(format);
    });

  if (!builder_ctx.use_src_tex_set)
  {
    //If the desired_tex_out has been cleared of all bits, nothing to decompress.
    if (dst_item.empty())
    {
      tex_set.swap(out);
      return IDS_I3S_OK;
    }
  }
  else
    // Using the original src texture set.
    return IDS_I3S_OK;
  
  auto trk = builder_ctx.tracker();

  Texture_buffer raw_img;
  Texture_buffer pot_img; // scaled to power-of-two dimensions, only used for DDS/DXT
  {
    // Check if there's raw texture in the input.
    std::optional<size_t> raw_idx = find_tex(tex_set, Image_format::Raw_rgba8);
    if (!raw_idx)
      raw_idx = find_tex(tex_set, Image_format::Raw_rgb8);
    
    if (raw_idx)
      raw_img = tex_set[*raw_idx];
    else if (png_index)
    {
      // There's no raw texture readily available, have to decompress from PNG.
      if (
        !builder_ctx.decoder->decode_png ||
        !builder_ctx.decoder->decode_png(tex_set[*png_index].data, &raw_img))
      {
        return log_error_s(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, std::string("PNG"));
      }
    }
    else if (const std::optional<size_t> pre_raw_idx = find_tex(tex_set, Image_format::Jpg))
    {
      if (
        !builder_ctx.decoder->decode_jpeg ||
        !builder_ctx.decoder->decode_jpeg(tex_set[*pre_raw_idx].data, &raw_img))
      {
        return log_error_s(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, std::string("JPEG"));
      }
    }
    else
      return log_error_s(trk, IDS_I3S_MISSING_JPG_OR_PNG, res_id_for_error_only);

    I3S_ASSERT_EXT(!raw_img.empty());
    if ((int)raw_img.meta.alpha_status <= 0)
      raw_img.meta.alpha_status = (Texture_meta::Alpha_status)get_alpha_bits(raw_img);

    const auto max_texture_size = builder_ctx.decoder->m_prop.max_write_texture_size;
 
    if((desired_tex_out & static_cast<Image_formats>(Image_format::Dds)) != 0)
    {
      // We need to produce DXT with mipmaps, and this demands a power-of-two texture.
      // Also we may need to downsample to fit the size limit setting.
      const auto [w, h] = apply_pot_texture_size_constraints(raw_img, max_texture_size, res_id_for_error_only, trk);
      I3S_ASSERT_EXT(w <= max_texture_size);
      I3S_ASSERT_EXT(h <= max_texture_size);
      I3S_ASSERT_EXT(utl::is_power_of_two(w));
      I3S_ASSERT_EXT(utl::is_power_of_two(h));
      if(w != raw_img.width() || h != raw_img.height())
        scale_texture(raw_img, w, h, pot_img);
    }

    if ((desired_tex_out & ~static_cast<Image_formats>(Image_format::Dds)) != 0)
    {
      const auto [w, h] = apply_texture_size_constraints(raw_img, max_texture_size, res_id_for_error_only, trk);
      I3S_ASSERT_EXT(w <= max_texture_size);
      I3S_ASSERT_EXT(h <= max_texture_size);
      if (w != raw_img.width() || h != raw_img.height())
      {
        if(!pot_img.empty() && w == pot_img.width() && h == pot_img.height())
          raw_img = pot_img;
        else
          scale_texture(raw_img, w, h, raw_img);
      }
    }
  }

  for (auto& format : dst_item)
  {
    const auto encoder = encoder_funcs.at(format);
    if (!encoder)
      continue;

    Texture_buffer tex;
    if (!encoder(format == Image_format::Dds && !pot_img.empty() ? pot_img : raw_img, &tex))
      return log_error_s(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, to_string(format));

    if (!tex.empty())
      out.push_back(tex);
  }

  tex_set.swap(out);

  return IDS_I3S_OK;
}

static Image_formats get_formats(const Multi_format_texture_buffer& set)
{
  Image_formats ret = 0;
  for (auto& s : set)
    if (!s.empty())
      ret |= (Image_formats)s.meta.format;

  return ret;
}

bool Layer_writer_impl::remap_geometry_ids(std::map<int, int>* out, const std::atomic<int>* geometry_ids, int size)
{
  int next_id = 0;
  for (int i = 0; i != size; ++i)
  {
    if (geometry_ids[i])
      out->operator[](i) = next_id++;
  }
  return true;
}
template< class T > utl::Vec3<T> abs3(const utl::Vec3<T>& v) { return utl::Vec3<T>( std::abs(v.x), std::abs(v.y), std::abs(v.z) ); }

bool dbg_compare_points(const i3s::Mesh_abstract& legacy_mesh, const i3s::Mesh_abstract& decoded_mesh, bool is_gcs
                        , int& estimate_lowest_encoding_accuracy_in_bits)
{
  std::map< uint32_t, utl::Vec3d > src, dst;
  if (legacy_mesh.get_feature_count() != legacy_mesh.get_vertex_count())
    return false;
  for (int i = 0; i < legacy_mesh.get_vertex_count(); ++i)
  {
    bool is_ok = src.insert({ (uint32_t)legacy_mesh.get_feature_ids()[i],
      legacy_mesh.get_origin() + utl::Vec3d(legacy_mesh.get_relative_positions()[i]) }).second;
    if (!is_ok) // fid<=>point must be 1:1
      return false;
  }
  utl::Vec3d max_error(0.0);
  //destination:
  for (int i = 0; i < decoded_mesh.get_vertex_count(); ++i)
  {
    bool is_ok = dst.insert({ (uint32_t)decoded_mesh.get_feature_ids()[i], 
                            decoded_mesh.get_origin()+utl::Vec3d(decoded_mesh.get_relative_positions()[i]) }).second;
    if (!is_ok) // fid<=>point must be 1:1
      return false;
  }
  if (src.size() != dst.size())
    return false;
  auto si = src.begin();
  auto di = dst.begin();
  utl::Vec3d lo(std::numeric_limits<double>::max()), hi(std::numeric_limits<double>::lowest());
  for (; si != src.end(); ++si, ++di)
  {
    if (si->first != di->first)
    {
      I3S_ASSERT(false ); // fid must be identical
      break;
    }
    lo = min(lo, si->second);
    hi = max(hi, si->second);
    //compare accuracy:
    max_error = max(max_error, abs3(di->second - si->second));
  }
  auto extent = (hi - lo);
  for(int k=0; k < 3; ++k )
    if (extent[k] == 0.0)
      extent[k] = 1.0;

  auto max_normalized_error = (max_error / extent).max_any();
  //auto norm_err = 100.0 * (max_error / extent);
  auto bit =  utl::first_bit_set( utl::nearest_power_of_two( (int)(1.0 / max_normalized_error)) );

  estimate_lowest_encoding_accuracy_in_bits = bit;

  //std::cout << "Max Error: " << bit << " bits\n";

  //std::cout << "Max Error: (" << norm_err.x <<", " << norm_err.y << ", " << norm_err.z <<" ) %\n";
  return true;
}


namespace detail
{
int Material_helper::get_or_create_texture_set(Image_formats f, bool is_atlas, Texture_semantic sem)
{
  {
    utl::Lock_guard lk(m_mutex);
    auto found = std::find_if(m_tex_def.begin(), m_tex_def.end(), [f, is_atlas, sem](const Texture_definition_desc& desc)
    {
      if (desc.is_atlas != is_atlas)
        return false;
      Image_formats mask = 0;
      for (auto& f : desc.formats)
        mask |= (Image_formats)f.format;
      if (mask != f)
        return false;
      return sem == desc.sem;
    });

    if (found != m_tex_def.end())
      return (int)(found - m_tex_def.begin());
    Texture_definition_desc tex_def;
    utl::for_each_bit_set(f, [&tex_def, sem](int bit_number)
    {
      Texture_format_desc fd;
      fd.format = (Image_format)(1 << bit_number);
      fd.name = to_compatibility_tex_name(fd.format, sem);
     
      tex_def.formats.push_back( fd );
    });
    tex_def.is_atlas = is_atlas;
    tex_def.sem = sem;
    m_tex_def.push_back(tex_def);
    return (int)m_tex_def.size() - 1;
  } // -> unlock
}

int Material_helper::get_or_create_material(const Material_desc& data)
{
  //clean it up:
  auto copy = data;
  if (copy.alpha_mode == Alpha_mode::Opaque)
    copy.alpha_cutoff = Material_desc::c_default_alpha_cutoff; //suppress it.

  {
    utl::Lock_guard lk(m_mutex);
    auto found = std::find(m_mat_def.begin(), m_mat_def.end(), copy);
    if (found != m_mat_def.end())
      return (int)(found - m_mat_def.begin());

    m_mat_def.push_back(copy);
    return (int)m_mat_def.size() - 1;
  } // -> unlock
}

std::vector< std::string >  Material_helper::get_legacy_texture_mime_types() const
{
  Image_formats mask = 0;
  for (auto& def : m_tex_def)
  {
    for (auto& f : def.formats)
    {

      if (f.format != Image_format::Dds)
        mask |= (Image_formats)f.format;
    }
  }
  return get_texture_mime_types(mask);
}


// ---------------------------------------------------------------------------------------------
//        class:      Node_io
// ---------------------------------------------------------------------------------------------

// staging structure to write a node out :
struct Node_io
{
  bool        is_root() const { return legacy_desc.level == 0; }
  status_t    set_parent(const Node_io& parent);
  [[nodiscard]]
  status_t    save(utl::Basic_tracker* trk, const Write_legacy, utl::Slpk_writer* slpk, Perf_stats& perf, Gzip_context &, Gzip_draco, i3s::Layer_type layer_type, int sublayer_id = -1);

  Node_desc_v17                       desc;
  utl::Raw_buffer_view                simple_geom;
  utl::Raw_buffer_view                draco_geom;
  std::vector<std::optional<utl::Raw_buffer_view>> attribute_buffers;
  Multi_format_texture_buffer         base_color_texs;
  Tri_state                           has_region = Tri_state::Not_set;
  Attrib_flags                        vb_attribs_mask_legacy;
  // --- legacy:
  Legacy_node_desc                    legacy_desc;
  Legacy_shared_desc                  legacy_shared;
  Legacy_feature_desc                 legacy_feature;

private:
  i3s::status_t _write_geometry(utl::Basic_tracker* trk, utl::Slpk_writer* slpk, Perf_stats& perf
                                , Gzip_context& gzip, Gzip_draco, Write_legacy, const std::string& legacy_res_path, i3s::Layer_type layer_type);
  
};


status_t Node_io::set_parent(const Node_io& parent)
{
  //connect to parent:
  //desc.parent_index = parent.desc.index; // It will be overwritten when we organize the page_page. 
  legacy_desc.parent_node.href = "../" + parent.legacy_desc.id;
  legacy_desc.parent_node.id = parent.legacy_desc.id;
  legacy_desc.parent_node.mbs = legacy_desc.mbs;
  legacy_desc.parent_node.obb = legacy_desc.obb;

  // TODO: 
  // add code to check that children are inside parents.

  return IDS_I3S_OK;
}

i3s::status_t Node_io::_write_geometry(utl::Basic_tracker* trk, utl::Slpk_writer* slpk
                                      , Perf_stats& perf, Gzip_context& gzip, Gzip_draco g, Write_legacy l, const std::string& legacy_res_path,  i3s::Layer_type layer_type)
{
  int out_size = 0;
  status_t status = IDS_I3S_OK; // exit if not ok
  if (layer_type == i3s::Layer_type::Point)
  {
    if (l == Write_legacy::Yes)
    {
      //Legacy geometry is stored in JSON featureData ...
      auto enc = utl::Mime_encoding::Not_set;
      status = append_to_slpk(trk, slpk, "features/0", legacy_res_path, simple_geom, utl::Mime_type::Json, &enc, gzip, &out_size);
      if (status != IDS_I3S_OK)
        return status;
      legacy_desc.geometry_data.clear();
      legacy_desc.feature_data.clear();
      legacy_desc.feature_data.push_back({ "./features/0" });
    }
  }
  else
  {
    // Contrary to the Layer_type::Point branch, not implementing the filter on (l == Write_legacy::Yes) here, because of the comment
    // on top of struct Legacy_feature_desc saying "featureData document is deprecated, but for 3DObject, Pro will read the OID from it (for no good reason)"
    auto enc = utl::Mime_encoding::Not_set;
    status = append_to_slpk(trk, slpk, "geometries/0", legacy_res_path, simple_geom, utl::Mime_type::Binary, &enc, gzip, &out_size);
    if (status != IDS_I3S_OK)
      return status;
    perf.legacy_size += out_size;
    legacy_desc.geometry_data.clear();
    legacy_desc.geometry_data.push_back({ "./geometries/0" });
  }
  if (draco_geom.size())
  {
    out_size = 0;
    const Use_gzip gz = (g == Gzip_draco::Yes) ? Use_gzip::Yes : Use_gzip::No;
    auto enc = utl::Mime_encoding::Not_set;
    if (layer_type == i3s::Layer_type::Point)
      status = append_to_slpk(trk, slpk, "geometries/0", legacy_res_path, draco_geom, utl::Mime_type::Binary, &enc, gzip, &out_size, gz);
    else
      status = append_to_slpk(trk, slpk, "geometries/1", legacy_res_path, draco_geom, utl::Mime_type::Binary, &enc, gzip, &out_size, gz);
    if (status != IDS_I3S_OK)
      return status;
    perf.draco_size += out_size;  
  }

  return IDS_I3S_OK;
}

status_t Node_io::save(utl::Basic_tracker* trk, const Write_legacy legacy, utl::Slpk_writer* slpk, Perf_stats& perf, Gzip_context& gzip, Gzip_draco g, i3s::Layer_type layer_type, int sublayer_id)
{
  status_t status;
  //create the legacy hrefs:
  std::vector< std::string > legacy_node_ids = { std::to_string(desc.index) };
  if (is_root())
    legacy_node_ids.push_back("root"); //we have to duplicate.

  std::string layer_prefix = (sublayer_id >= 0 ? "sublayers/" + std::to_string(sublayer_id) + "/" : "");

  for (const std::string & legacy_id : legacy_node_ids)
  {
    const bool write_legacy = (legacy == Write_legacy::Yes) && (legacy_id == legacy_node_ids.back());

    std::string legacy_res_path = layer_prefix + "nodes/" + legacy_id + "/";
    if (simple_geom.size())
    {
      // --- write the textures:
      legacy_desc.texture_data.clear(); //the last one (if node is "root") will override.
      for (auto& tex : base_color_texs)
      {
        auto mime_type = to_utl_mime_type(to_mime_type(tex.meta.format));
        if (mime_type == utl::Mime_type::Dds_legacy)
          mime_type = utl::Mime_type::Dds_proper;
        auto tex_name = "textures/" + to_compatibility_tex_name(tex.meta.format, tex.meta.semantic);
        auto enc = utl::Mime_encoding::Not_set;
        status = append_to_slpk(trk, slpk, tex_name, legacy_res_path, tex.data, mime_type, &enc, gzip);
        if (status != IDS_I3S_OK)
          return status;
        legacy_desc.texture_data.push_back({ "./" + tex_name });
      }

      // --- write the geometries:
      status = _write_geometry(trk, slpk, perf, gzip, g, legacy, legacy_res_path, layer_type);
      if (status != IDS_I3S_OK)
        return status;

      // --- write the attributes:
      int loop = 0;
      legacy_desc.attribute_data.clear();
      for (auto& attrib : attribute_buffers)
      {
        if (attrib.has_value())
        {
          auto enc = utl::Mime_encoding::Not_set;
          std::string name = "attributes/f_" + std::to_string(loop) + "/0";
          status = append_to_slpk(trk, slpk, name, legacy_res_path, *attrib, utl::Mime_type::Binary, &enc, gzip);
          if (status != IDS_I3S_OK)
            return status;
          legacy_desc.attribute_data.push_back({ "./" + name });
        }
        ++loop;
      }
      if (write_legacy && layer_type != Layer_type::Point)
      {
        //write legacy sharedResource:
        status = save_json(trk, slpk, legacy_shared, "shared/sharedResource", legacy_res_path, gzip);
        if (status != IDS_I3S_OK)
          return status;
        legacy_desc.shared_resource.href = "./shared";

        //"copy-over" feature data:
        // TODO: create one if missing ?
        legacy_desc.feature_data.push_back({ "./features/0" });
        if (legacy_feature.feature_data.raw.empty())
          legacy_feature.feature_data.raw = "[]";
        if (legacy_feature.geometry_data.raw.empty())
          legacy_feature.geometry_data.raw = "[]";
        status = save_json(trk, slpk, legacy_feature, "features/0", legacy_res_path, gzip);
        if (status != IDS_I3S_OK)
          return status;
      }
    }
    if (write_legacy)
    {
      //write the legacy node index document:
      status = save_json(trk, slpk, legacy_desc, "3dNodeIndexDocument", legacy_res_path, gzip);
      if (status != IDS_I3S_OK)
        return status;
    }
  }
  return IDS_I3S_OK;
}

static void set_non_texture_material_properties(Material_texture_desc& mat_tex_desc, int tex_def_id)
{
  if (tex_def_id != c_invalid_id)
  {
    mat_tex_desc.tex_coord_set = 0;
    mat_tex_desc.tex_def_id = tex_def_id;
  }
}

static status_t set_material(const Writer_context& builder_ctx, detail::Material_helper& helper,
                             const Material_data_multitex& mat_, const std::string legacy_mat_json, Node_io& nio)
{
  auto trk = builder_ctx.tracker();

  std::array< int, (int)Texture_semantic::_count > tex_set_id_by_semantic = { -1,-1,-1,-1,-1,-1 };

  //int actual_alpha_bits = 0;
  auto mat_copy = mat_;

  auto create_missing_tex = [&tex_set_id_by_semantic, &mat_copy, &helper, &nio, &builder_ctx]
    (Multi_format_texture_buffer& tex_set, Texture_semantic sem, bool use_atlas = false )
  {
    if (tex_set.size())
    {
      auto st = create_texture_set(builder_ctx, tex_set, nio.legacy_desc.id);
      if (st != IDS_I3S_OK)
        return st;
      I3S_ASSERT(tex_set.empty() || tex_set.front().width() > 0 && tex_set.front().height() > 0);
      tex_set_id_by_semantic[(int)sem] = helper.get_or_create_texture_set(get_formats(tex_set), use_atlas, sem);
      for (auto& tex : tex_set)
      {
        tex.meta.semantic = sem;
        nio.base_color_texs.push_back(tex);
      }
    }
    return status_t(IDS_I3S_OK);
  };

  //write the legacy shared resource:
  if (legacy_mat_json.size())
  {
    if (!utl::from_json_safe(legacy_mat_json, &nio.legacy_shared, trk, "nodes/" + nio.legacy_desc.id + std::string("/sharedResource")))
      return IDS_I3S_JSON_PARSING_ERROR;
  }

  // for WSV pre-10.8
  // materialDefinitions must be written to sharedResources
  if (nio.legacy_shared.material.unnamed == Legacy_material_desc())
  {
    nio.legacy_shared.material.unnamed.param.vertex_region = nio.vb_attribs_mask_legacy & (Attrib_flags)(Attrib_flag::Region);
    nio.legacy_shared.material.unnamed.param.vertex_colors = nio.vb_attribs_mask_legacy & (Attrib_flags)(Attrib_flag::Color);
    nio.legacy_shared.material.unnamed.param.shininess = 1.f;
    nio.legacy_shared.material.unnamed.param.specular = { 0.0979999974f, 0.0979999974f, 0.0979999974f };
    nio.legacy_shared.material.unnamed.param.diffuse = { 1.f, 1.f, 1.f };
    // all other properties will use defaults
  }

  //setup region (mandatory for WSV):
  I3S_ASSERT_EXT(nio.has_region != Tri_state::Not_set);
  nio.legacy_shared.material.unnamed.param.vertex_region = nio.has_region == Tri_state::True;
  nio.legacy_shared.texture.unnamed.atlas = nio.legacy_shared.material.unnamed.param.vertex_region;
  // base color:
  if( mat_copy.has_texture( i3s::Texture_semantic::Base_color))
  {
    auto& tex_set = mat_copy.get_texture(i3s::Texture_semantic::Base_color);
    auto st = create_texture_set(builder_ctx, tex_set, nio.legacy_desc.id);
    if (st != IDS_I3S_OK)
      return st;
    // rewrite the texture set:
    nio.legacy_shared.texture.unnamed.encoding.clear();
    Legacy_texture_image_desc img_desc;
    if (nio.legacy_shared.texture.unnamed.images.size())
    {
      auto& src_img = nio.legacy_shared.texture.unnamed.images.front();
      img_desc.id = src_img.id;
      img_desc.size = src_img.size;
      img_desc.pixel_in_world_units = src_img.pixel_in_world_units;
    }
    nio.legacy_shared.texture.unnamed.images.clear();
    for (int i = 0; i < tex_set.size(); ++i)
    {
      auto& tex = tex_set[i];
      auto tex_name = to_compatibility_tex_name(tex.meta.format);

      if (!tex.empty())
      {
        //ASSUMES that tex are ordered in the img_format enum order: (so to match the legacy layer desc)
        nio.legacy_shared.texture.unnamed.encoding.push_back(to_mime_type(tex.meta.format));

        img_desc.byte_offset.push_back(0);
        img_desc.length.push_back(tex.data.size());
        img_desc.href.push_back("../textures/" + tex_name);
        img_desc.id = std::to_string(nio.desc.index);
      }
    }
    nio.legacy_shared.texture.unnamed.images.push_back(img_desc);
    nio.legacy_shared.set_material_id(nio.desc.index);

    bool use_atlas = nio.legacy_shared.material.unnamed.param.vertex_region;
    tex_set_id_by_semantic[(int)Texture_semantic::Base_color] = helper.get_or_create_texture_set(get_formats(tex_set), use_atlas, Texture_semantic::Base_color);
    nio.base_color_texs = tex_set;
    I3S_ASSERT(tex_set.empty() || tex_set.front().width() > 0 && tex_set.front().height() > 0);
    nio.desc.mesh.material.texel_count_hint = tex_set.size() ? tex_set.front().width() * tex_set.front().height() : 0;
  }
  constexpr std::array< Texture_semantic, 3 > c_sems = 
  {Texture_semantic::Metallic_roughness,Texture_semantic::Normal_map,Texture_semantic::Emissive_texture};
  for( auto& sem : c_sems )
    create_missing_tex(mat_copy.get_texture(sem), sem);

  //create the v17 material:
  Material_desc mat17;
  mat17.alpha_cutoff = (float)mat_copy.properties.alpha_cut_off / 255.0f;
  mat17.alpha_mode = mat_copy.properties.alpha_mode;
  mat17.emissive_factor = mat_copy.properties.emissive_factor;
  mat17.is_double_sided = mat_copy.properties.double_sided;
  mat17.cull_face = mat_copy.properties.cull_face;
  mat17.metal.base_color_factor = mat_copy.metallic_roughness.base_color_factor;
  if (auto tex_id = tex_set_id_by_semantic[(int)Texture_semantic::Base_color]; tex_id != -1)
  {
    mat17.metal.base_color_tex.tex_coord_set = 0;
    mat17.metal.base_color_tex.tex_def_id = tex_set_id_by_semantic[(int)Texture_semantic::Base_color];
  }
  if (auto tex_id = tex_set_id_by_semantic[(int)Texture_semantic::Metallic_roughness]; tex_id != -1)
  {
    mat17.metal.metal_tex.tex_coord_set = 0;
    mat17.metal.metal_tex.tex_def_id = tex_id;
    mat17.metal.metallic_factor = mat_copy.metallic_roughness.metallic_factor;

    if (mat_copy.has_texture( i3s::Texture_semantic::Occlusion_map ) )
      set_non_texture_material_properties(mat17.occlusion_tex, tex_id);
  }

  set_non_texture_material_properties(mat17.normal_tex, tex_set_id_by_semantic[(int)Texture_semantic::Normal_map]);
  set_non_texture_material_properties(mat17.emissive_tex, tex_set_id_by_semantic[(int)Texture_semantic::Emissive_texture]);

  //write it out:
  nio.desc.mesh.material.definition_id = helper.get_or_create_material(mat17);
  return IDS_I3S_OK;
}


//-----------------------------------------------------------
} //endof ::detail





// ---------------------------------------------------------------------------------------------
//        class:      Layer_writer_impl
// ---------------------------------------------------------------------------------------------

Layer_writer_impl::Layer_writer_impl(utl::Slpk_writer::Ptr slpk, Writer_context::Ptr ctx, int sublayer_id)
  : m_sublayer_id(sublayer_id)
  , m_ctx(ctx)
  , m_slpk(slpk)
  , m_gzip(ctx->gzip_option)
{
}


//Must be call first ( so that projection could be setup)
void Layer_writer_impl::set_layer_meta(
  const Layer_meta&                     meta
  , const i3s::Spatial_reference_desc*  dst_sr
  , const Normal_reference_frame*       nrf_override
  , const Height_unit                   dst_vert_linear_unit
  , const std::string&                  dst_vert_crs_name
  , const Height_model                  dst_height_model)
{
  m_layer_meta = meta;
  m_xform = m_ctx->sr_helper_factory(meta.sr, dst_sr);
  m_layer_meta.sr = m_xform->get_dst_sr(); //in case we are reprojecting.
  if (nrf_override) // For re-projecting, the normal reference frame might differ from the src NRF.
    m_layer_meta.normal_reference_frame = *nrf_override;
  if (dst_vert_linear_unit != Height_unit::Not_set)
    m_layer_meta.height_model_info.height_unit = dst_vert_linear_unit;
  if (dst_vert_crs_name != "")
    m_layer_meta.height_model_info.vert_crs = dst_vert_crs_name;
  if (dst_height_model != Height_model::Not_set)
    m_layer_meta.height_model_info.height_model = dst_height_model;
}



static Lod_selection_desc convert_lod_selection_to_size(const Lod_selection_desc & input)
{
  //*s *= (*s) * utl::c_pi * 0.25;
  I3S_ASSERT(input.max_error >= 0.0);
  Lod_selection_desc out;
  out.metric_type = Lod_metric_type::Max_screen_size;

  switch (input.metric_type)
  {
  case Lod_metric_type::Max_screen_area:
    // see screen_size_to_area() function for the reverse.
    out.max_error = 2.0 * std::sqrt(input.max_error / utl::c_pi);
    break;
  case Lod_metric_type::Max_screen_size:
    out.max_error = input.max_error;
    break;
  default:
    out.max_error = 0.0; // To make Coverity happy.
    // TODO handle other cases, if / when needed
    I3S_ASSERT_EXT(false);
    break;
  }
  return out;
}

static bool all_faces_degenerate(const Mesh_attrb<utl::Vec3f>& positions)
{
  constexpr float c_epsi = 1e-3f;

  const auto count = positions.size();
  I3S_ASSERT(count % 3 == 0);

  for (int i = 0; i < count; i += 3)
  {
    //for each triangle:
    const auto& v1 = positions[i];
    const auto& v2 = positions[i + 1];
    const auto& v3 = positions[i + 2];

    const auto a = utl::Vec3f::l1_distance(v1, v2);  // side v1v2
    const auto b = utl::Vec3f::l1_distance(v2, v3);  // side v2v3
    const auto c = utl::Vec3f::l1_distance(v3, v1);  // side v3v1

    // if true, all faces are not degenerate
    if (a > c_epsi && b > c_epsi && c > c_epsi)
      return false;
  }

  return true;
}


static bool cartesian_to_enu(utl::Vec3d* vtx, const utl::Vec3d& ref, const utl::Vec3d& origin)
{
  const auto c_cos_lat = std::cos(utl::radians(origin.x));
  const auto c_sin_lat = std::sin(utl::radians(origin.x));
  const auto c_cos_lon = std::cos(utl::radians(origin.y));
  const auto c_sin_lon = std::sin(utl::radians(origin.y));

  const auto dx = vtx->x - ref.x;
  const auto dy = vtx->y - ref.y;
  const auto dz = vtx->z - ref.z;

  vtx->x = (-c_sin_lon * dx) + (c_cos_lon * dy);
  vtx->y = (-c_sin_lat * c_cos_lon * dx) - (c_sin_lat * c_sin_lon * dy) + (c_cos_lat * dz);
  vtx->z = (c_cos_lat * c_cos_lon * dx) + (c_cos_lat * c_sin_lon * dy) + (c_sin_lat * dz);

  return true;
}


// --- convert vertices to correct reference frame

// WARNING: assumes that dst SR is WGS84.
static bool convert_vtx_to_normal_frame(std::vector<utl::Vec3d>* vtx, int count,  const Writer_context& ctx, const Spatial_reference_xform& xform
                        , Normal_reference_frame nrf, const utl::Vec3d& origin)
{
  switch (nrf)
  {
    case Normal_reference_frame::Earth_centered:
      to_dst_cartesian( xform, vtx->data(), count);
      return true;
    case Normal_reference_frame::East_north_up:
    {
      // to cartesian
      to_dst_cartesian(xform, vtx->data(), count);
      // to ENU
      auto ref{ origin };
      to_dst_cartesian(xform, &ref, 1);
      for (auto& v : *vtx)
        cartesian_to_enu(&v, ref, origin);
      return true;
    }
    break;
    case Normal_reference_frame::Vertex_reference_frame:
      // nothing to do
      return true;
    default:
      I3S_ASSERT(false);
      return false;
  }
}

// true -> ALL normals were implicit. can be droppped.
// false -> found smooth normal. don't drop.
static bool is_implicit_normals(std::shared_ptr<Mesh_abstract> mesh, const Writer_context& ctx, const Spatial_reference_xform& xform, Normal_reference_frame nrf)
{
  const auto& normals = mesh->get_normals();
  if (normals.values.size() == 0)
    return true;

  const auto& vtx = mesh->get_absolute_positions();
  const auto count = vtx.size();
  I3S_ASSERT(normals.size() == count);
  I3S_ASSERT(count % 3 == 0);
  // convert points to cartesian
  std::vector<utl::Vec3d> cartesian(count);
  copy_elements(cartesian.data(), vtx.data(), count);

  convert_vtx_to_normal_frame(&cartesian, count, ctx, xform,  nrf, mesh->get_origin());

  const float c_epsi = 1.0f;// 1 degree

  // for each triangle
  for (int i = 0; i < count; i += 3)
  {
    const auto& c_v1 = cartesian[i];
    const auto& c_v2 = cartesian[i + 1];
    const auto& c_v3 = cartesian[i + 2];
    // check is triangle
    if (c_v1 != c_v2 && c_v1 != c_v3 && c_v2 != c_v3) {
      const auto c_side1 = c_v2 - c_v1; // vector v1 -> v2
      const auto c_side2 = c_v3 - c_v2; // vector v2 -> v3

      auto angle_with_face_normal = [face_normal{ utl::Vec3f(utl::Vec3d::cross(c_side1, c_side2)) }](const auto& vtx_normal)
      {
        double val = face_normal.dot(vtx_normal) / (vtx_normal.length() * face_normal.length());
        if (val > 1.0) val = 1.0; // floating point error? 1.0000001...
        return std::acos(val);
      };

      // angle between vertex normals and face normal, in degrees
      auto angle_v1 = utl::degrees(angle_with_face_normal(normals[i]));
      auto angle_v2 = utl::degrees(angle_with_face_normal(normals[i + 1]));
      auto angle_v3 = utl::degrees(angle_with_face_normal(normals[i + 2]));
      if (angle_v1 > c_epsi || angle_v2 > c_epsi || angle_v3 > c_epsi)
        return false; // smooth normal. can't drop
    }
  }
  return true; // all implicit
}

void Layer_writer_impl::_encode_geometry_to_legacy(detail::Node_io& nio, const Geometry_buffer& src)
{
  if (m_layer_meta.type == i3s::Layer_type::Point)
  {
    //let's create the legacy JSON for it:
    std::string json_content;
    auto mesh = src.get_mesh();
    if (mesh->get_feature_ids().values.size() == mesh->get_absolute_positions().size())
      encode_points_to_i3s(mesh->get_vertex_count(), mesh->get_absolute_positions().data(), mesh->get_feature_ids().values.data()
        , &json_content,m_ctx->tracker());
    else
    {
      I3S_ASSERT( "fids / xyz size mismatch for Points Scene layer" ); //TODO: report error failure.
    }

    nio.simple_geom = utl::Buffer::create_deep_copy(json_content.data(), (int)json_content.size());

    m_vb_attribs_mask_legacy |= (Attrib_flags)Attrib_flag::Pos | (Attrib_flags)Attrib_flag::Feature_id;
  }
  else
  {
    Attrib_flags vb_attribs_mask_legacy = 0;
    nio.simple_geom = src.to_legacy_buffer(&vb_attribs_mask_legacy);

    [[maybe_unused]] constexpr Attrib_flags c_exclude_region = ~(Attrib_flags)Attrib_flag::Region;
    I3S_ASSERT(m_vb_attribs_mask_legacy == 0 ||
      (vb_attribs_mask_legacy & c_exclude_region) == (m_vb_attribs_mask_legacy & c_exclude_region));

    m_vb_attribs_mask_legacy |= vb_attribs_mask_legacy;
  }
}

std::string   Layer_writer_impl::_layer_path(const std::string& resource) const
{
  return (m_sublayer_id >= 0 ? "sublayers/" + std::to_string(m_sublayer_id) + "/" + resource : resource);
}

static std::optional<utl::Raw_buffer_view> try_convert_date_attribs_to_iso8601(
  const utl::Datetime_meta& meta
  , Attribute_buffer* attrib,
  utl::Histo_datetime<std::string>& histo
  , const std::string& key_for_error_report
  , utl::Basic_tracker* trk)
{
  // try convert to iso
  const int sz = attrib->get_count();
  i3s::Attribute_buffer_encoder encoder;
  encoder.init_buffer(sz, Type::Date_iso_8601);
  std::string iso_date;
  for (int idx = 0; idx != sz; ++idx)
  {
    const std::string date = attrib->get_as_string(idx);
    if (date.size())
    {
      if (date.front() == '\0')
        encoder.push_back(date); // empty string
      else if (!utl::convert_date_to_iso8601(meta, date, &iso_date))
      {
        log_error_s(trk, IDS_I3S_EXPECTS, "layer.attributeStorageInfo." + key_for_error_report,
          meta.datetime_format, date);
        return std::nullopt;
      }
      encoder.push_back(iso_date);
      histo.add_value(iso_date);
    }
    else
      encoder.push_null();
  }
  auto data = encoder.get_raw_bytes();
  auto buff = utl::Buffer::create_deep_copy<char>(data.data(), (int)data.size());
  return buff;
}

namespace
{

utl::Boxd compute_mesh_envelope(const Mesh_abstract& mesh, const Spatial_reference_desc& sr, Layer_type layer_type)
{
  const auto vertices = mesh.get_absolute_positions().data();
  const auto vertex_count = mesh.get_vertex_count();

  if (Spatial_reference::is_well_known_gcs(sr))
  {
    if (layer_type == Layer_type::Point)
      return utl::compute_points_geo_envelope(vertices, vertex_count);
    else
    {
      I3S_ASSERT(vertex_count % 3 == 0);
      return utl::compute_mesh_geo_envelope(vertices, vertex_count / 3);
    }
  }

  utl::Boxd box;
  for (size_t i = 0; i < vertex_count; i++)
    box.expand(vertices[i]);

  return box;
}

utl::Boxd merge_envelopes(const std::vector<utl::Boxd>& envelopes, const Spatial_reference_desc& sr)
{
  I3S_ASSERT(!envelopes.empty());

  if (envelopes.size() == 1)
    return envelopes.front();

  if (Spatial_reference::is_well_known_gcs(sr))
    return utl::merge_geo_envelopes(envelopes.data(), envelopes.size());

  utl::Boxd box;
  for (const auto& envelope : envelopes)
    box.expand(envelope);

  return box;
}

} // namespace

status_t Layer_writer_impl::create_output_node(const Simple_node_data& node, Node_id node_id)
{
  std::string scratch;
  status_t status{ IDS_I3S_OK };

  auto trk = m_ctx->tracker();

  auto nio = std::make_unique<detail::Node_io>();
  //*node_id = c_invalid_id;
  I3S_ASSERT(node.node_depth >= 0);
  //const bool is_root = node.node_depth == 0;
  nio->legacy_desc.level = node.node_depth;
  nio->desc.index = static_cast<decltype(nio->desc.index)>(node_id);
  I3S_ASSERT(static_cast<decltype(node_id)>(nio->desc.index) == node_id);
  nio->legacy_desc.id = nio->is_root() ? "root" : std::to_string(nio->desc.index);
  //const int c_single_attrib_set = 0;

  // --- Lod selection: 
  Lod_selection_desc lod;
  lod.max_error = node.lod_threshold;
  lod.metric_type = m_layer_meta.lod_metric_type;

  nio->legacy_desc.lod_selection.push_back(lod);

  // write an older metrics too for older clients:
  nio->legacy_desc.lod_selection.push_back(convert_lod_selection_to_size(lod));
  // v1.7+:
  nio->desc.lod_threshold = lod.max_error; //TODO: select the same one for the layer.

  bool degenerated_mesh = false;

  const bool has_precomputed_obb = node.precomputed_obb.extent.x >= 0.0;

  // get obb of children
  std::vector<utl::Obb_abs> ch_obbs;

  // Get envelopes of children as well.
  std::vector<utl::Boxd> envelopes;

  if (!node.children.empty())
  {
    // allocate memory before locking.
    envelopes.reserve(node.children.size() + 1);
    if (!has_precomputed_obb)
    {
      ch_obbs.reserve(node.children.size());
    }

    utl::Lock_guard lk(m_mutex);

    for (const Node_id & ch_id : node.children)
    {
      auto iter = m_working_set.find(ch_id);
      if (iter == m_working_set.end())
      {
        // all children must have been completed (in working_set) before creating parent, in order to create obb
        return log_error_s(trk, IDS_I3S_INVALID_TREE_TOPOLOGY, ch_id);
      }
      if (iter->second.envelope)
        envelopes.push_back(*iter->second.envelope);
      if (!has_precomputed_obb)
      {
        ch_obbs.push_back(iter->second.obb);
      }
    }
  } // --> unlock

  // --- Geometry:
  if (node.mesh.geometries.size())
  {
    // --- create single mesh (with 2 geometry buffers : simple i3s and draco compressed):
    Mesh_desc_v17& m17 = nio->desc.mesh;
    m17.geometry.resource_id = nio->desc.index;
    //m17.attribute.definition_id = c_single_attrib_set; // ONLY one attribute definition for v1.7
    if (node.mesh.attribs.size())
      m17.attribute.resource_id = nio->desc.index;
    if (node.mesh.material.has_texture( i3s::Texture_semantic::Base_color))
      m17.material.resource_id = nio->desc.index;

    auto legacy_mesh_buffer = node.mesh.geometries.front();
    auto legacy_mesh = legacy_mesh_buffer->get_mesh();

    auto mask = legacy_mesh->get_available_attrib_mask();

#if 0
    if (is_set(mask, Attrib_flag::Region) && m_ctx->is_drop_region_if_not_repeated && (legacy_mesh->get_wrap_mode(0) & Texture_meta::Wrap_xy) == 0)
    {
      //Drop regions and re-encode UV as absolute:
      legacy_mesh->drop_regions();
      mask = legacy_mesh->get_available_attrib_mask();
    }
#endif
    nio->has_region = is_set(mask, Attrib_flag::Region) ? Tri_state::True : Tri_state::False;

    // 1.6 can only have a single geometry definiton, 
    // so we will need to make sure all legacy geometry buffers contain the same attributes. 
    m_vb_attribs |= mask;

    if(has_precomputed_obb)
    {
      const auto &obb = node.precomputed_obb;
      nio->legacy_desc.obb = utl::Obb_abs(obb.center, obb.extent, obb.orientation);
      nio->legacy_desc.mbs = node.precomputed_mbs;
    }
    else
    {
      // Project if necessary, then compute OBB and shift to new center:
      status = project_update_mesh_origin_and_obb(m_ctx->tracker(), m_layer_meta.type, *m_xform,
        *legacy_mesh, ch_obbs, nio->legacy_desc.obb, nio->legacy_desc.mbs);
      if (status != IDS_I3S_OK)
        return status;
    }

    nio->desc.obb = nio->legacy_desc.obb;

    //
    envelopes.push_back(compute_mesh_envelope(*legacy_mesh, m_layer_meta.sr, m_layer_meta.type));

    bool drop_normals = true; // will be false if node contains >= 1 smooth normal
    bool drop_colors = false; // true if all color values set to [255,255,255, 255] or all colors are set to [0,0,0,0]
    bool is_mesh_3d_IM_point = m_layer_meta.type == i3s::Layer_type::Mesh_3d || m_layer_meta.type == i3s::Layer_type::Mesh_IM || m_layer_meta.type == i3s::Layer_type::Point;

    if (is_mesh_3d_IM_point)
    {
      if (!m_ctx->decoder->m_prop.is_drop_normals)
        drop_normals = is_implicit_normals(legacy_mesh, *m_ctx, *m_xform, m_layer_meta.normal_reference_frame);

      if (drop_normals)
        legacy_mesh->drop_normals(); // client will recompute them.

      // Some applications write transparent black or opaque white for all colors. An example is nFrames SURE.
      constexpr Rgba8 opaque_white{ 0xFF, 0xFF, 0xFF, 0xFF };
      constexpr Rgba8 transparent_black{ 0x00, 0x00, 0x00, 0x00 };
      const auto& colors = legacy_mesh->get_colors();
      if (!colors.values.size())
        drop_colors = true;
      else if (const auto color = colors[0]; color == opaque_white || color == transparent_black)
      {
        const auto iter = std::find_if_not(colors.values.begin() + 1, colors.values.end(),
          [color](auto& a) {return a == color; });
        drop_colors = iter == colors.values.end();
      }

      if (drop_colors)
        legacy_mesh->drop_colors();
    }

    // "convert" to legacy:
    _encode_geometry_to_legacy(*nio, *legacy_mesh_buffer);

    if (m_layer_meta.type == Layer_type::Point)
    {
      auto abs_points = legacy_mesh->get_absolute_positions();
      auto& feature_ids = legacy_mesh->get_feature_ids();
      int num = abs_points.size();
      Point_feature_data_desc feature_data;
      auto& points = feature_data.points;
      points.resize(num);
      for (int i = 0; i < num; ++i)
      {
        auto& desc = points[i];
        desc.fid = feature_ids[i];
        desc.position = abs_points[i];
      }

      nio->legacy_feature.feature_data.raw = utl::to_json(feature_data);
    }
    
    // This is deprecated.
    //nio->legacy_feature.geometry_data.raw = node.mesh.legacy_feature_data_geom_json;

    if (m_ctx->encode_to_draco && is_mesh_3d_IM_point)
    {
      int bad_uv_count= legacy_mesh->sanitize_uvs(); //Draco doesn't like garbage UV
      if (bad_uv_count && m_ctx->tracker())
        utl::log_warning(trk, (int)IDS_I3S_BAD_UV, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"), bad_uv_count);
  
      //scale it:
      utl::Vec3f scale;
      auto src_pos = legacy_mesh->get_relative_positions();

      scale = get_somewhat_anisotropic_scale(*m_ctx, *m_xform, *legacy_mesh);

      // create a Draco version for it:
      Has_fids has_fids = Has_fids::No;
      if (!m_ctx->encode_to_draco(*legacy_mesh, &nio->draco_geom, has_fids, (double)scale.x, (double)scale.y))
      {
        // DRACO will fail on degenerated mesh ( all faces are degenerated)
        // need to add the node ID to help with error reporting.
        if (!all_faces_degenerate(src_pos))
          return log_error_s(trk, IDS_I3S_COMPRESSION_ERROR, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"), std::string("DRACO"));
        else {
          // all faces were degenerate
          degenerated_mesh = true;
          utl::log_warning(trk, IDS_I3S_DEGENERATED_MESH, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"));
        }
      }

      if (has_fids == Has_fids::Yes_64 && !m_ctx->draco_allow_large_fids)
      {
        return log_error_s(trk, IDS_I3S_INTERNAL_ERROR, std::string("fid out of range"));
      }

      if (m_ctx->decoder->m_prop.verify_compressed_geometry && legacy_mesh->get_topology() == i3s::Mesh_topology::Points && m_ctx->decoder->decode_draco )
      {
        //decompress draco and check that we get the "same" mesh. check accuracy:
        auto decoded_mesh = m_ctx->decoder->decode_draco(legacy_mesh->get_origin(), nio->draco_geom);
        int lowest_encoding_accuracy = 0;
        dbg_compare_points(*legacy_mesh, *decoded_mesh,Spatial_reference::is_well_known_gcs(m_layer_meta.sr), lowest_encoding_accuracy);
        if (lowest_encoding_accuracy <= 16)
        {
          utl::log_warning(trk, IDS_I3S_LOW_ENCODING_PRECISION, std::string("/nodes/" + nio->legacy_desc.id + "/geometry")
                           , lowest_encoding_accuracy, 20);
        }
      }
    }


    if (!degenerated_mesh)
    {
      // need to OR these properties to get correct node definition id
      uint32_t drop_normal = drop_normals ? 1 : 0;
      uint32_t drop_color = drop_colors ? 2 : 0;
      uint32_t drop_region = nio->has_region == Tri_state::False ? 4 : 0;

      m17.geometry.definition_id = drop_normal | drop_color | drop_region;
      m17.geometry.feature_count = legacy_mesh->get_feature_count();
      m17.geometry.vertex_count = legacy_mesh->get_vertex_count();

      //--- compute the OBB:
      //compute_obb( *m_ctx, m_layer_meta.sr, legacy_mesh->get_origin(), legacy_mesh->get_relative_positions(), &desc.obb, &desc.mbs);

      // --- textures: (only support base color texture )
      //int mat_def_id = c_invalid_id;
      nio->vb_attribs_mask_legacy = legacy_mesh->get_available_attrib_mask();
      if (m_layer_meta.type != i3s::Layer_type::Point)
        status = set_material(*m_ctx, m_mat_helper, node.mesh.material, node.mesh.legacy_mat_json, *nio);
      if (status != IDS_I3S_OK)
        return status;

      // --- attribute data:
      if (node.mesh.attribs.size())
      {
        constexpr int c_single_schema = 0;
        {
          utl::Lock_guard lk(m_mutex_attr);

          if (m_attrib_metas.empty())
          {
            m_attrib_metas.resize(1);
            // set it up:
            m_attrib_metas[0].resize(node.mesh.attribs.size());
          }
          auto& single_schema_attrs = m_attrib_metas[c_single_schema];
          const int single_schema_attrs_sz = static_cast<int>(single_schema_attrs.size());
          nio->attribute_buffers.resize(node.mesh.attribs.size());
          for (int i = 0; i < node.mesh.attribs.size(); ++i)
          {
            //type checking:
            if (i >= single_schema_attrs_sz)
              return log_error_s(trk, IDS_I3S_OUT_OF_RANGE_ID, std::string("attribute"), i, single_schema_attrs_sz);
            auto& attrib = node.mesh.attribs[i];
            if (attrib)
            {
              const auto attrib_type = attrib->get_type();
              if (single_schema_attrs[i].def.type == Type::Not_set)
                single_schema_attrs[i].def.type = attrib_type;
              if (attrib_type != single_schema_attrs[i].def.type)
              {
                return log_error_s(trk, IDS_I3S_TYPE_MISMATCH, std::string("attribute")
                  , to_string(attrib_type), to_string(single_schema_attrs[i].def.type));
              }
              // if date attrib, try to convert to ECMA ISO8601
              if (attrib_type == Type::Date_iso_8601 && m_ctx->decoder->datetime_meta)
              {
                if (auto buff = try_convert_date_attribs_to_iso8601(*m_ctx->decoder->datetime_meta,
                  attrib.get(), m_datetime_stats[i], m_attrib_metas[0][i].def.meta.key, trk))
                {
                  nio->attribute_buffers[i] = std::move(buff);
                  m_attrib_metas[0][i].def.time_encoding = Time_encoding::Ecma_iso_8601;
                }
                else
                  return IDS_I3S_INTERNAL_ERROR;
              }
              else
              {
                nio->attribute_buffers[i] = attrib->get_raw_data();
              }
            }
            else
            {
              nio->attribute_buffers[i].reset();
            }
          }
        } // unlock m_mutex_attr
      }
    } // end of node with mesh
    else
    {
      m17.attribute = Mesh_attribute_ref_desc();
      m17.geometry = Mesh_geometry_ref_desc();
      m17.material = Mesh_material_ref_desc();
      nio->simple_geom = utl::Raw_buffer_view();

      if (node.children.empty())
      {
        // Treat leaf node with degenerate mesh as empty.
        // already warned about degenerate mesh.
        return IDS_I3S_EMPTY_LEAF_NODE;
      }
    }

  } //end of node with geometry
  else
  {
    // This node has no geometry.
  
    nio->desc.mesh.geometry = Mesh_geometry_ref_desc();//no mesh for you.
    
    // Empty leaf nodes are allowed for layers that have the "Label" semantic because
    //   some labels should disappear when zooming in close enough.
    //
    // It would be tempting to write an empty OBB (i.e with strictly negative extent values) for empty nodes.
    //
    // Unfortunately clients don't support empty OBBs well.
    //   For example in Pro, 'ExtractNodeProperties' should be modified to support empty obbs
    //   (see https://devtopia.esri.com/ArcGISPro/ArcGISPro/blob/552ebe4eba573d7202238a390416ce311afcb862/BeagleGraphics/Graphics/GraphicsDisplayInteraction/SceneServiceNodeConverter.cpp)
    //
    // So instead, we let the caller precompute the OBB for the leaf node.
    // This OBB must be non-empty, and contained in the OBB of the parent node.

    if (has_precomputed_obb)
    {
      if ((m_ctx->semantic != Semantic::Labels) && (node.children.empty()))
      {
        utl::log_warning(trk, IDS_I3S_EMPTY_LEAF_NODE, node_id);
        return IDS_I3S_EMPTY_LEAF_NODE;
      }
      // The precomputed obb must be s.t the parent node obb contains the obb
      const auto& obb = node.precomputed_obb;
      nio->legacy_desc.obb = utl::Obb_abs(obb.center, obb.extent, obb.orientation);
      nio->legacy_desc.mbs = node.precomputed_mbs;
    }
    else
    {
      if (node.children.empty())
      {
        utl::log_warning(trk, IDS_I3S_EMPTY_LEAF_NODE, node_id);
        return IDS_I3S_EMPTY_LEAF_NODE;
      }
      compute_obb(*m_xform, ch_obbs, nio->legacy_desc.obb, nio->legacy_desc.mbs);
    }

    nio->desc.obb = nio->legacy_desc.obb;
  }

  Node_brief brief;
  brief.obb = nio->legacy_desc.obb;
  brief.mbs = nio->legacy_desc.mbs;

  if(!envelopes.empty())
    brief.envelope = merge_envelopes(envelopes, m_layer_meta.sr);

  brief.level = nio->legacy_desc.level;
  brief.node = std::move(nio);
  {
    utl::Lock_guard lk(m_mutex);
    m_working_set.emplace(node_id, std::move(brief));
  } // -> unlock
  return IDS_I3S_OK;
}

status_t Layer_writer_impl::process_children(const Simple_node_data& node, Node_id node_id)
{

  std::map<Node_id, Node_brief>::iterator node_brief;
  {
    utl::Lock_guard lk(m_mutex);
    node_brief = m_working_set.find(node_id);
    I3S_ASSERT_EXT(node_brief != m_working_set.end());
    if (node_brief == m_working_set.end())
      return IDS_I3S_INTERNAL_ERROR;
  } // -> unlock;

  auto& nio = node_brief->second.node;
  auto trk = m_ctx->tracker();
  //process children:
  std::vector< std::unique_ptr< detail::Node_io> > children_to_write;
  for (int i = 0; i < node.children.size(); ++i)
  {
    auto ch_id = node.children[i];
    if (ch_id == c_invalid_id)
      continue; // empty leaf nodes are skipped.
    Node_brief ch_node_brief;
    Node_id id;
    {    
      utl::Lock_guard lk(m_mutex);
      auto found = m_working_set.find(ch_id);
      if (found == m_working_set.end())
        return log_error_s(m_ctx->tracker(), IDS_I3S_INVALID_TREE_TOPOLOGY, ch_id); // Node can only have one parent.
      ch_node_brief = std::move(found->second);
      id = found->first;
      m_working_set.erase(found);
    }
    Node_ref_desc ch_desc;
    ch_desc.href = "../" + std::to_string(id);
    ch_desc.id = std::to_string(ch_id);
    ch_desc.mbs = ch_node_brief.mbs;
    ch_desc.obb = ch_node_brief.obb;
    nio->legacy_desc.children.push_back(ch_desc);
    //connect to parent:
    nio->desc.children.push_back(static_cast<decltype(nio->desc.children)::value_type>(ch_id));
    I3S_ASSERT_EXT(static_cast<decltype(nio->desc.children)::value_type>(ch_id) == ch_id);
    //connect the parent:
    children_to_write.push_back(std::move(ch_node_brief.node));
  }

  if (nio->legacy_desc.obb.is_valid())
  {
    //We can now connect to the children  and dump them:
    for (auto& ch : children_to_write)
    {
      ch->set_parent(*nio);
      //Once, the parent is set, we can commit each child to SLPK:
      if (auto status = _write_node(*ch, &nio->desc); status != IDS_I3S_OK)
        return status;
    }

    // update node_brief
    auto& brief = node_brief->second;
    //add to working set:
    //Node_brief brief;
    brief.obb = nio->legacy_desc.obb;
    brief.mbs = nio->legacy_desc.mbs;
    brief.level = nio->legacy_desc.level;
  }
  else
  {
    auto id_for_log = nio->legacy_desc.id;
    {
      utl::Lock_guard lk(m_mutex);
      m_working_set.erase(node_id);
    }
    utl::log_warning(trk, IDS_I3S_EMPTY_LEAF_NODE, id_for_log);
    return IDS_I3S_EMPTY_LEAF_NODE;
  }
  return IDS_I3S_OK;
}

status_t Layer_writer_impl::create_node(const Simple_node_data& node, Node_id node_id)
{
  auto status = create_output_node(node, node_id);
  if (status == IDS_I3S_OK)
    status = process_children(node, node_id);
  return status;
}


status_t Layer_writer_impl::_write_node( detail::Node_io& nio, Node_desc_v17* maybe_parent)
{
  auto status = nio.save(m_ctx->tracker(), m_ctx->write_legacy, m_slpk.get(), m_perf, m_gzip, m_ctx->gzip_draco, m_layer_meta.type, m_sublayer_id);
  if (status != IDS_I3S_OK)
    return status;

  ++m_node_count;

  if (nio.desc.mesh.geometry.definition_id >= 0)
    m_geometry_defs[nio.desc.mesh.geometry.definition_id]++;

  return _on_node_written(nio.desc, maybe_parent);
}

status_t Layer_writer_impl::_on_node_written(Node_desc_v17& desc, Node_desc_v17* maybe_parent)
{
  {
    utl::Lock_guard lk(m_mutex_nodes17);

    if (desc.index >= m_nodes17.size())
      m_nodes17.resize(desc.index + 1);
    m_nodes17[desc.index] = desc;
  }
  return IDS_I3S_OK;
}

status_t Layer_writer_impl::_save_paged_index(uint32_t root_id, std::map<int, int>& geometry_ids)
{
  auto trk = m_ctx->tracker();

  I3S_ASSERT(root_id < m_nodes17.size());
  remap_geometry_ids(&geometry_ids, m_geometry_defs.data(), static_cast<int>(m_geometry_defs.size()));

  {
    auto update_geometry_id = [&geometry_ids](int* inout)
    {
      auto found = geometry_ids.find(*inout);
      if (found != geometry_ids.end())
        *inout = found->second;
    };
    for (auto& n : m_nodes17)
      update_geometry_id(&n.mesh.geometry.definition_id);
  }

  const std::string node_page_path = _layer_path("nodepages");
  const uint32_t page_size = static_cast<uint32_t>(_get_page_size());

  auto on_page = [&](const Node_page_desc_v17& page, const size_t page_id) -> status_t {
    return save_json(trk, m_slpk.get(), page, std::to_string(page_id), node_page_path, m_gzip);
  };

  switch(m_ctx->pages_construction)
  {
  case Pages_construction::Local_sub_tree:
    return Page_builder_localsubtree{ Max_count_sibling_local_subtrees {1} }.build_pages(m_node_count, root_id, page_size, on_page, m_nodes17);
  case Pages_construction::Local_sub_tree_all_siblings:
    return Page_builder_localsubtree{ Max_count_sibling_local_subtrees {std::numeric_limits<int>::max()} }.build_pages(m_node_count, root_id, page_size, on_page, m_nodes17);
  case Pages_construction::Breadth_first:
    return Page_builder_breadthfirst{}.build_pages(m_node_count, root_id, page_size, on_page, m_nodes17);
  default:
    I3S_ASSERT_EXT(false);
    return IDS_I3S_INTERNAL_ERROR;
  }
}



static Geometry_schema_desc create_legacy_geometry_schema(uint32_t attrib_mask)
{
  Geometry_schema_desc desc;
  desc.geometry_type = Mesh_topology::Triangles;
  desc.topology = Legacy_topology::Per_attribute_array;
  desc.hdrs.push_back({ Geometry_header_property::Vertex_count, Type::UInt32 });
  desc.hdrs.push_back({ Geometry_header_property::Feature_count, Type::UInt32 });
  desc.orderings = { Vertex_attrib_ordering::Position };
  desc.vertex_attributes.position = { Type::Float32, 3 };

  if (is_set(attrib_mask, Attrib_flag::Normal))
  {
      desc.orderings.push_back(Vertex_attrib_ordering::Normal);
      desc.vertex_attributes.normal = { Type::Float32, 3 };
  }
  if (is_set(attrib_mask, Attrib_flag::Uv0))
  {
      desc.orderings.push_back(Vertex_attrib_ordering::Uv0);
      desc.vertex_attributes.uv0 = { Type::Float32, 2 };
  }
  if (is_set(attrib_mask, Attrib_flag::Color))
  {
      desc.orderings.push_back(Vertex_attrib_ordering::Color);
      desc.vertex_attributes.color = { Type::UInt8, 4 };
  }
  if (is_set(attrib_mask, Attrib_flag::Region))
  {
    desc.orderings.push_back(Vertex_attrib_ordering::Region);
    desc.vertex_attributes.region = { Type::UInt16, 4 };
  }

  desc.feature_attrib_order       = { Feature_attrib_ordering::Fid, Feature_attrib_ordering::Face_range };
  desc.feature_attrib.id          = { Type::UInt64, 1 };
  desc.feature_attrib.face_range  = { Type::UInt32, 2 };
  return desc;
}

static std::vector< Attribute_storage_info_desc > create_legacy_attrib_info(const std::vector< Attribute_definition >& attrib_defs)
{
  std::vector< Attribute_storage_info_desc > ret(attrib_defs.size());
  for( int id =0 ; id < attrib_defs.size(); ++id)
  {
    const auto& src = attrib_defs[id];
    auto& dst = ret[id];
    dst.key = "f_" + std::to_string(id);
    dst.name = src.meta.name;
    dst.header.push_back({ Attrib_header_property::Count, Type::UInt32 });
    bool is_str_type = src.type == Type::String_utf8 || src.type == Type::Date_iso_8601;
    if (is_str_type)
    {
      dst.header.push_back({ Attrib_header_property::Attribute_values_byte_count, Type::UInt32 });
      dst.ordering = { Attrib_ordering::Attribute_byte_counts, Attrib_ordering::Attribute_values };
      dst.attribute_values = { Type::String_utf8, 1, Value_encoding::Utf8 };
      dst.attribute_byte_counts = { Type::UInt32, 1 };
      dst.attribute_values.time_encoding = src.time_encoding;
    }
    else
    {
      dst.ordering = { Attrib_ordering::Attribute_values };
      dst.attribute_values = { src.type,  1 };
    }
    dst.attribute_values.kv_encoding = src.key_value_encoding;
  }
  return ret;
} 

Layer_writer_impl::Attrb_info& Layer_writer_impl::_get_attrib_meta_nolock(Attrib_schema_id sid, Attrib_index idx)
{
  if (m_attrib_metas.size() <= sid)
    m_attrib_metas.resize(sid + 1);
  auto& schema = m_attrib_metas[sid];
  if (schema.size() <= idx)
    schema.resize(idx + 1);
  return schema[idx];
}


status_t Layer_writer_impl::set_attribute_meta(
  Attrib_index idx, const Attribute_definition& attrib_def,
  Attrib_schema_id sid
)
{
  status_t status = IDS_I3S_OK;
  {
    std::unique_lock l(m_mutex_attr);
    auto& def = _get_attrib_meta_nolock(sid, idx).def;
    def = attrib_def;
    // check vs the current type since psl_writer calls the function AFTER populating attributes!!
    const i3s::Type& expected_type = attrib_def.type;
    if (expected_type != Type::Not_set)
    {
      if (def.type != Type::Not_set && def.type != expected_type)
      {
        status = log_error_s(m_ctx->tracker(), IDS_I3S_TYPE_MISMATCH, std::string("attribute")
          , to_string(def.type), to_string(expected_type));
      }
      def.type = expected_type;
    }
  } // unlock m_mutex_attr
  return status;
}

status_t   Layer_writer_impl::set_attribute_stats( Attrib_index idx, Stats_attribute::ConstPtr stats, Attrib_schema_id sid)
{
  {
    std::unique_lock l(m_mutex_attr);
    _get_attrib_meta_nolock(sid, idx).stats = stats;
  } // unlock m_mutex_attr
  return IDS_I3S_OK;
}

static void create_mesh_desc(Attrib_flags vb_attrib_mask, Attrib_flags vb_attribs_mask_legacy, Geometry_definition_desc* gd, bool has_draco, bool has_legacy = true)
{
  gd->topo = Mesh_topology::Triangles;

  Geometry_buffer_desc gd_draco;

  Geometry_buffer_desc gd_legacy;
  //gd.id = 0; //legacy must always be zero.
  gd_legacy.offset = 2 * sizeof(int); //skip legacy header

  gd_draco.compressed.encoding = Compressed_geometry_format::Draco;
  utl::for_each_bit_set_conditional(vb_attrib_mask, [&](int bit_number)
  {
    Attrib_flag f = (Attrib_flag)(1 << bit_number);
    switch (f)
    {
    case Attrib_flag::Pos: gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Position); break;
    case Attrib_flag::Normal: gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Normal); break;
    case Attrib_flag::Uv0: gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Uv0); break;
    case Attrib_flag::Color: gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Color); break;
    case Attrib_flag::Region: break; // See below
    case Attrib_flag::Feature_id: gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Feature_index); break;
    default:
      I3S_ASSERT(false);
    }
    return true; // for lambda
  });
  utl::for_each_bit_set_conditional(vb_attribs_mask_legacy, [&](int bit_number)
  {
    Attrib_flag f = (Attrib_flag)(1<<bit_number);
    switch(f)
    {
    case Attrib_flag::Pos:
      // push as "regular" attribute:
      gd_legacy.position.binding = VB_Binding::Per_vertex;
      gd_legacy.position.component = 3;
      gd_legacy.position.type = Type::Float32;
      break;
    case Attrib_flag::Normal:
      gd_legacy.normal.binding = VB_Binding::Per_vertex;
      gd_legacy.normal.component = 3;
      gd_legacy.normal.type = Type::Float32;
      break;
    case Attrib_flag::Uv0:
      gd_legacy.uv0.binding = VB_Binding::Per_vertex;
      gd_legacy.uv0.component = 2;
      gd_legacy.uv0.type = Type::Float32;
      break;
    case Attrib_flag::Color:
      gd_legacy.color.binding = VB_Binding::Per_vertex;
      gd_legacy.color.component = 4;
      gd_legacy.color.type = Type::UInt8;
      break;
    case Attrib_flag::Feature_id:
      gd_legacy.feature_id.binding = VB_Binding::Per_feature;
      gd_legacy.feature_id.component = 1;
      gd_legacy.feature_id.type = Type::UInt64;
      gd_legacy.face_range.binding = VB_Binding::Per_feature;
      gd_legacy.face_range.component = 2;
      gd_legacy.face_range.type = Type::UInt32;
      break;
    case Attrib_flag::Region:
      break; // will be added later if needed (see below)
    default:
      I3S_ASSERT(false);
    }
    return true; // foreach loop
  });

  // some IM don't have featureID or faceRange.
  // always written out and legacy buffer needs to match
  if (!(vb_attrib_mask & (Attrib_flags)Attrib_flag::Feature_id))
  {
    gd_legacy.feature_id.binding = VB_Binding::Per_feature;
    gd_legacy.feature_id.component = 1;
    gd_legacy.feature_id.type = Type::UInt64;
    gd_legacy.face_range.binding = VB_Binding::Per_feature;
    gd_legacy.face_range.component = 2;
    gd_legacy.face_range.type = Type::UInt32;
  }

  if (is_set(vb_attrib_mask, Attrib_flag::Region))
  {
    gd_legacy.uv_region.binding = VB_Binding::Per_vertex;
    gd_legacy.uv_region.component = 4;
    gd_legacy.uv_region.type = Type::UInt16;
    gd_draco.compressed.attributes.push_back(Compressed_mesh_attribute::Uv_region);
  }
  if (has_legacy)
    gd->geoms.push_back(gd_legacy);
  if (has_draco)
    gd->geoms.push_back(gd_draco);
}

namespace
{

const std::string c_metadata_json_path{"metadata"};

Full_extent to_full_extent(const utl::Boxd& box)
{
  Full_extent extent;
  extent.xmin = box.left();
  extent.xmax = box.right();
  extent.ymin = box.bottom();
  extent.ymax = box.top();
  extent.zmin = box.front();
  extent.zmax = box.back();
  return extent;
}

}

status_t Layer_writer_impl::save(utl::Boxd* extent /*= nullptr*/)
{
  auto trk = m_ctx->tracker();
  if (m_working_set.size() != 1)
  {
    //return status_t::Unconnected_nodes;
    return log_error_s(trk, IDS_I3S_INVALID_TREE_TOPOLOGY, m_working_set.size() - 1);
  }

  const auto found = std::cbegin(m_working_set);
  const auto root_id = found->second.node->desc.index;

  //save the root:
  if (auto status = _write_node(*found->second.node, nullptr); status != IDS_I3S_OK)
    return status;

  std::map<int, int> geometry_ids;  // only want geometry defs that are actually used
  if (auto status = _save_paged_index(root_id, geometry_ids); status != IDS_I3S_OK)
    return status;

  Layer_desc desc;
  desc.id = (m_sublayer_id == -1 ? 0 : m_sublayer_id);
  desc.name = m_layer_meta.name;
  desc.version = m_layer_meta.uid;
  desc.time_stamp.last_update = m_layer_meta.timestamp == 0 ? std::time(nullptr) * 1000ull : m_layer_meta.timestamp;
  //desc.href = "./0"; //makes no sense...
  desc.layer_type = m_layer_meta.type;
  desc.priority = m_ctx->priority;
  desc.semantic = m_ctx->semantic;
  //static_assert(sizeof(i3s::Spatial_reference_desc) == sizeof(m_layer_meta.sr), "unexpected size");
  desc.spatial_ref = (const Spatial_reference_desc&)m_layer_meta.sr;
  
  I3S_ASSERT(found->second.envelope.has_value());
  if (!found->second.envelope.has_value())
    return log_error_s(trk, IDS_I3S_EMPTY_FULL_EXTENT);

  desc.full_extent = to_full_extent(found->second.envelope.value());
  
  desc.full_extent.spatial_reference = desc.spatial_ref;
  desc.alias = m_layer_meta.alias.size() ? m_layer_meta.alias : m_layer_meta.name;
  desc.description = m_layer_meta.desc;
  desc.copyright = m_layer_meta.copyright;
  desc.capabilities = m_layer_meta.capabilities;
  desc.drawing_info.raw = m_layer_meta.drawing_info;
  desc.elevation_info.raw = m_layer_meta.elevation_info;
  desc.popup_info.raw = m_layer_meta.popup_info;

  desc.store.id = "0"; //useless but required.
  desc.store.root_node = "./nodes/root";// +std::to_string(root_id);
  desc.store.index_crs = to_epsg_url(desc.spatial_ref);
  desc.store.vertex_crs = to_epsg_url(desc.spatial_ref);
  desc.store.extent = { desc.full_extent.xmin, desc.full_extent.ymin, desc.full_extent.xmax, desc.full_extent.ymax };

  int min_major_version = 1;
  if (m_ctx->write_legacy == Write_legacy::No)
    min_major_version = 2;

  if (m_layer_meta.type != Layer_type::Point)
  {
    desc.store.geometry_schema = create_legacy_geometry_schema(m_vb_attribs_mask_legacy);
    desc.store.normal_reference_frame = m_layer_meta.normal_reference_frame;
    desc.store.profile = "meshpyramids";
    desc.store.resource_pattern = { "3dNodeIndexDocument", "SharedResource", "Geometry", "Attributes" }, // required but useless..
    //nidencoding, featureencoding, attributeEncoding // obsolete.
      desc.store.lod_type = "MeshPyramid"; //useless
    desc.store.lod_model = "node-switching"; //useless
    //I3S_ASSERT(m_tex_defs.size() < 2); //What to do when different type of "sets" are used ?
    //desc.store.texture_encodings = get_texture_mime_types(m_tex_defs.size() ? m_tex_defs.front().tex_format_set : 0 );
    desc.store.texture_encodings = m_mat_helper.get_legacy_texture_mime_types();
  }

  static_assert(sizeof(Height_model_info_desc) == sizeof(m_layer_meta.height_model_info), "unexpected size");
  desc.height_model_info = reinterpret_cast<const Height_model_info_desc&>(m_layer_meta.height_model_info);
  if (m_attrib_metas.size() > 0)
  {
    std::vector< Attribute_definition > attr_defs(m_attrib_metas.front().size());
    for (int i = 0; i < attr_defs.size(); ++i)
    {
      attr_defs[i] = m_attrib_metas.front()[i].def;
      switch (attr_defs[i].key_value_encoding.type)
      {
      case Key_value_encoding_type::Not_set:
        break;
      case Key_value_encoding_type::Separated_key_values:
        min_major_version = std::max(min_major_version, 3);
        break;
      default:
        I3S_ASSERT_EXT(false);
        break;
      }
    }
    desc.attribute_storage_info = create_legacy_attrib_info(attr_defs);
    desc.fields = create_fields(attr_defs);
  }

  switch (min_major_version)
  {
  case 3:
    desc.store.version = "3.0";
    break;
  case 2:
    desc.store.version = "2.0";
    break;
  case 1:
    desc.store.version = "1.9";
    break;
  default:
    I3S_ASSERT_EXT(false);
    break;
  }

  // --- v17 Node page:
  desc.node_pages.nodes_per_page = static_cast<int>(_get_page_size());
  desc.node_pages.lod_metric_type = m_layer_meta.lod_metric_type;
  //---- v17 material / texture sets
  desc.material_defs = m_mat_helper.get_material_defs();
  desc.tex_defs = m_mat_helper.get_texture_defs();
  //---- v17 geometry buffers:
  std::vector<Geometry_definition_desc> draco_all_defs(8);
  for (uint32_t i = 0; i < 8; ++i)
  {
    auto udpated_vb_attribs = m_vb_attribs;// vb_mask_update(m_vb_attribs, i);

    utl::for_each_bit_set_conditional(i, [&udpated_vb_attribs](int bit_number)
      {
        // map the optionally compressed attributes to corresponding Attrib_flag
        //{ Normal = 1, Color = 2, Region = 4 }
        static const std::map < uint32_t, Attrib_flag> attrib_flag
        {
          { 1, Attrib_flag::Normal },
          { 2, Attrib_flag::Color },
          { 4, Attrib_flag::Region }
        };
        udpated_vb_attribs &= ~(Attrib_flags)attrib_flag.at(0x01 << bit_number);
        return true;
      });
    create_mesh_desc(udpated_vb_attribs, m_vb_attribs_mask_legacy, &draco_all_defs[i], (bool)m_ctx->encode_to_draco, desc.layer_type != Layer_type::Point);
  }

  desc.geom_defs.resize(geometry_ids.size());

  int cur_id = 0;
  for (auto& id : geometry_ids)
  {
    desc.geom_defs[cur_id++] = std::move(draco_all_defs[id.first]);
  }

  //if (!is_set(m_vb_attribs, Attrib_flag::Region))
  //  desc.geom_defs.resize(4);


  // --- V17 Attribute buffers:
  //int j = 0; 
  //for( int i=0 ; i < m_attrib_metas.size(); ++i)
  //{
  //  for (const auto& src : m_attrib_metas[i])
  //  {
  //    Attribute_buffer_desc tmp;
  //    tmp.alias = src.def.meta.alias;
  //    tmp.name = src.def.meta.name;
  //    tmp.binding = VB_Binding::Per_feature;
  //    tmp.id = j++;
  //    tmp.type = src.def.type;
  //    tmp.offset = (int) std::max( sizeof(int), size_of(src.def.type) );
  //    desc.attrib_defs.push_back(tmp);
  //  }
  //}

  // --- stats: 
  // write the href:
  for (int sid = 0; sid < m_attrib_metas.size(); ++sid)
    for (int i = 0; i < m_attrib_metas[sid].size(); ++i)
    {
      if (m_attrib_metas[sid][i].stats)
      {
        //create stats info entry:
        Statistics_href_desc shd;
        auto name = "f_" + std::to_string(i) + "/0";
        shd.href = "./statistics/" + name; //Pro 2.3- expects this. 
        shd.key = "f_" + std::to_string(i);
        shd.name = m_attrib_metas[sid][i].def.meta.name;
        desc.statistics_info.push_back(shd);
        std::string json_stats;
        // stats for datetime attributes may have been updated
        if (m_attrib_metas[sid][i].def.type == Type::Date_iso_8601 && m_ctx->decoder->datetime_meta)
        {
          utl::Attribute_stats_desc<utl::Atrb_stats_datetime<std::string> > stats;
          m_datetime_stats[i].get_stats(&stats.stats);
          json_stats = utl::to_json(stats);
        }
        else
        {
          json_stats = m_attrib_metas[sid][i].stats->to_json();
        }
        auto st = save_json(trk, m_slpk.get(), json_stats, name, _layer_path("statistics"), m_gzip);
        if (st != IDS_I3S_OK)
          return st;
      }
      else if (m_attrib_metas[sid][i].def.type != Type::Oid32 && m_attrib_metas[sid][i].def.type != Type::Oid64) // don't expect stats for objectId
        utl::log_warning(trk, IDS_I3S_MISSING_ATTRIBUTE_STATS, m_attrib_metas[sid][i].def.meta.name);

    }
  if (auto status = save_json(trk, m_slpk.get(), desc, _layer_path("3dSceneLayer"), "", m_gzip); status != IDS_I3S_OK)
    return status;

  // Add metadata.json to the slpk.
  const auto metadata_json =
    "{\n  \"I3SVersion\": \"" + desc.store.version + "\",\n  \"nodeCount\": " + std::to_string(m_node_count.load()) + "\n}";

  if (!m_slpk->append_file(_layer_path(c_metadata_json_path), metadata_json.data(), static_cast<int>(metadata_json.size()), utl::Mime_type::Json))
  {
    return log_error_s(trk, IDS_I3S_IO_WRITE_FAILED, "SLPK://" + c_metadata_json_path);
  }

  //print compression ratio:
  utl::log_debug(trk, IDS_I3S_GEOMETRY_COMPRESSION_RATIO, std::to_string( m_perf.get_ratio()));
  if (m_ctx->finalization_mode == Writer_finalization_mode::Finalize_output_stream)
  {
    if (m_slpk->finalize())
      return IDS_I3S_OK;
    return log_error_s(trk, IDS_I3S_IO_WRITE_FAILED, std::string("output SLPK"));
  }

  if (extent != nullptr)
    *extent = found->second.envelope.value();

  return IDS_I3S_OK;
}


Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, const std::filesystem::path& path)
{
  // Since create_mesh_layer_builder() accepts both filesystem paths and URIs
  // for the path parameter, the parameter should be UTF-8 std::string.
  // However, the existing API has already been adopted by clients.
  utl::Slpk_writer::Ptr slpk_writer(utl::create_slpk_writer(utl::to_string(path)));

  if (slpk_writer && slpk_writer->create_archive(path))
    return new Layer_writer_impl(slpk_writer, ctx);

  utl::log_error(ctx->tracker(), IDS_I3S_IO_OPEN_FAILED, path);
  return nullptr;
}

Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, utl::Slpk_writer::Ptr writer, int sublayer_id)
{
  return (writer ? new Layer_writer_impl(writer, ctx, sublayer_id) : nullptr);
}

bool  create_texture_from_image(int width, int height, int channel_count, const char* data, Texture_buffer& out)
{
  I3S_ASSERT(channel_count == 3 || channel_count == 4);

  out.meta.alpha_status = channel_count == 3 ? Texture_meta::Alpha_status::Opaque : Texture_meta::Alpha_status::Mask_or_blend;
  out.meta.wrap_mode = Texture_meta::Wrap_mode::None;
  out.meta.is_atlas = false;
  out.meta.mip0_width = width;
  out.meta.mip0_height = height;
  out.meta.mip_count = 1; //no mip
  out.meta.format = channel_count == 3 ? Image_format::Raw_rgb8 : Image_format::Raw_rgba8;

  //deep copy the image. 16 byte aligned in case we need to use SSE DXT compressor on it:
  int size = width * height* channel_count;
  auto buff = std::make_shared< utl::Buffer >(data, size, utl::Buffer::Memory::Deep_aligned);
  out.data = buff->create_writable_view();
  return true;
}


// More of wrapper around  Mesh_abstract::Ptr. Maybe Geometry_buffer needs refactoring. 
class Geometry_buffer_simple_impl : public Geometry_buffer
{
public:
  explicit Geometry_buffer_simple_impl(Mesh_abstract::Ptr m) : m_mesh(m) {}
  // --- Geometry_buffer:
  virtual Mesh_abstract::Ptr        get_mesh() const override { return m_mesh; }
  virtual Geometry_format           get_format() const override { return Geometry_format::Legacy; } //not completly true. TBD
  virtual utl::Raw_buffer_view      to_legacy_buffer(Attrib_flags* actualy_written_attrib) const override { return encode_legacy_buffer(*m_mesh, actualy_written_attrib); }
  virtual Modification_state        get_modification_state() const override { return Modification_state(); };
private:
  Mesh_abstract::Ptr m_mesh;
};


//! Create Mesh_data from src mesh description. Vertex data will be deep-copied, but Texture_buffer will be shallow-copied.
status_t  Layer_writer_impl::create_mesh_from_raw(const Simple_raw_mesh& src, Mesh_data& dst) const
{
  I3S_ASSERT(src.abs_xyz && src.vertex_count);

  if (src.index_count == 0)
  {
    if (src.vertex_count % 3 != 0)
    {
      I3S_ASSERT(false);
      return IDS_I3S_DEGENERATED_MESH;
    }
  }
  else if (src.index_count % 3 != 0)
  {
    I3S_ASSERT(false);
    return IDS_I3S_DEGENERATED_MESH;
  }
  //take care of the material:
  dst.material = Material_data_multitex(); //clear
  if (!src.img.empty())
    dst.material.add_texture( src.img, i3s::Texture_semantic::Base_color); // shallow copy

  utl::Obb_abs obb;
  utl::Vec4d mbs;

  // create bulk data to assign:
  compute_obb(*m_xform, src.abs_xyz, src.vertex_count, obb, mbs);
  auto rel_pos = utl::Buffer::create_writable_typed_view< utl::Vec3f>(src.vertex_count);
  for (int i = 0; i < rel_pos.size(); ++i)
  {
    rel_pos[i] = utl::Vec3f(src.abs_xyz[i] - obb.center);
  }
  //create the bulk:
  Mesh_bulk_data bulk;
  bulk.origin = obb.center;
  bulk.rel_pos.values = rel_pos;
  int triangle_corner_count = (src.index_count ? src.index_count : src.vertex_count);

  auto color_values = utl::Buffer::create_writable_typed_view<Rgba8>(triangle_corner_count);
  std::fill_n(color_values.data(), triangle_corner_count, src.default_color);
  bulk.colors.values = color_values;

  if (src.uv)
    bulk.uvs.values = utl::Buffer::create_deep_copy(src.uv, src.vertex_count);
  if (src.indices)
  {
    auto indices = utl::Buffer::create_deep_copy(src.indices, src.index_count);
    bulk.rel_pos.index = indices;
    if (src.uv)
      bulk.uvs.index = indices;
  }

  if (src.fid_value_count)
  {
    I3S_ASSERT(src.fid_values);
    if (!src.fid_values)
    {
      return IDS_I3S_DEGENERATED_MESH;
    }

    bulk.fids.values = utl::Buffer::create_deep_copy(src.fid_values, src.fid_value_count);
    if (src.fids_indices)
    {
      I3S_ASSERT(src.index_count);
      bulk.fids.index = utl::Buffer::create_deep_copy(src.fids_indices, src.index_count);      
    }
    else
    {
      I3S_ASSERT(src.fid_value_count == src.vertex_count);
      if(src.fid_value_count != src.vertex_count)
        return IDS_I3S_DEGENERATED_MESH;
    }
  }

  Mesh_abstract::Ptr mesh(parse_mesh_from_bulk(bulk));
  I3S_ASSERT(mesh);
  dst.geometries = { std::make_shared< Geometry_buffer_simple_impl>( mesh ) };
  return IDS_I3S_OK;
}

void create_mesh_from_raw_and_origin(const Simple_raw_points& src, const utl::Vec3d& origin, Mesh_data& dst)
{
  auto rel_pos = utl::Buffer::create_writable_typed_view< utl::Vec3f>(src.count);
  for (int i = 0; i < rel_pos.size(); ++i)
  {
    rel_pos[i] = utl::Vec3f(src.abs_xyz[i] - origin);
  }
  //create the bulk:
  Mesh_bulk_data bulk;
  bulk.origin = origin;
  bulk.rel_pos.values = rel_pos;

  if (src.fids)
  {
    bulk.fids.values = utl::Buffer::create_deep_copy(src.fids, src.count);
  }
  i3s::Mesh_abstract::Ptr mesh(parse_mesh_from_bulk(bulk));
  I3S_ASSERT(mesh);
  mesh->set_topology(i3s::Mesh_topology::Points);
  dst.geometries = { std::make_shared< Geometry_buffer_simple_impl>(mesh) };
}

status_t Layer_writer_impl::create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const
{
  I3S_ASSERT(src.abs_xyz && src.count);

  utl::Obb_abs obb;
  utl::Vec4d mbs;

  // create bulk data to assign:
  compute_obb(*m_xform, src.abs_xyz, src.count, obb, mbs);
  create_mesh_from_raw_and_origin(src, obb.center, dst);
  return IDS_I3S_OK;
}


size_t Layer_writer_impl::_get_page_size() const
{
  return m_ctx->max_page_size;
}

} // namespace i3s
} // namespace i3slib
