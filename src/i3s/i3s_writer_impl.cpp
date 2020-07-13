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
#include "i3s_writer_impl.h"
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
enum Tri_state : int { True, False, Not_set };

static std::string to_epsg_url(const geo::SR_def& def)
{
  int id = def.latest_wkid != geo::SR_def::c_wkid_not_set ? def.latest_wkid : def.wkid;
  if (id == geo::SR_def::c_wkid_not_set)
    return std::string();
  auto epsg = "http://www.opengis.net/def/crs/EPSG/0/" + std::to_string(id);
  return epsg;
}

//! TODO: this function won't work well for [0,360] type of GCS data
static utl::Vec4d to_extent(Writer_context& ctx, const Spatial_reference& sr, const utl::Obb_abs& obb_)
{
  auto obb = obb_;
  ctx.to_cartesian_space(sr, &obb.center, 1);

  std::array< utl::Vec3d, 8  > corners;
  obb.get_corners(&corners[0], 8);
  ctx.from_cartesian_space(sr, &corners[0], 8);

  utl::Vec3d pt[2] = { corners[0], corners[0] };
  for (auto p : corners)
  {
    pt[0] = min(pt[0], p);
    pt[1] = max(pt[1], p);
  }
  return utl::Vec4d(pt[0].x, pt[0].y, pt[1].x, pt[1].y);
}

static Mime_image_format to_mime_type(Image_format format)
{
  switch (format)
  {
    case    Image_format::Jpg: return Mime_image_format::Jpg;
    case    Image_format::Png: return Mime_image_format::Png;
    case    Image_format::Dds: return Mime_image_format::Dds;
    case    Image_format::Ktx: return Mime_image_format::Ktx;
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
    default:
      I3S_ASSERT(false);
      return utl::Mime_type::Not_set;
  }
}


std::string to_compatibility_tex_name(Image_format f)
{
  switch (f)
  {
    case Image_format::Dds:
      return "0_0_1";
    case Image_format::Ktx:
      return "0_0_2"; // not tested. Hope that the old portal will be let it go. 
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

static void compute_obb(const Writer_context& ctx, const Spatial_reference& sr
                        , utl::Vec3d* points, int count, utl::Obb_abs* obb, utl::Vec4d* mbs)
{
  //to cartesian space...
  I3S_ASSERT(ctx.to_cartesian_space);
  I3S_ASSERT(ctx.from_cartesian_space);
  [[maybe_unused]] bool ret = ctx.to_cartesian_space(sr, points, count);
  I3S_ASSERT(ret);
  // compute OBB:
  utl::Pro_hull hull(utl::Pro_set::get_default());
  double radius;
  hull.get_ball_box(points, count, *obb, radius, utl::Pro_hull::Method::Minimal_surface_area);
  //enforce a minimum extent for single points:
  if (count == 1)
  {
    radius = 1.0;
    obb->extents = { 1.0f, 1.0f, 1.0f };
  }
  //convert center back:
  ctx.from_cartesian_space(sr, &obb->center, 1);

  *mbs = utl::Vec4d(obb->center, radius);
}

static void compute_obb(const Writer_context& ctx, const Spatial_reference& sr
                        , std::vector< utl::Obb_abs >& src, utl::Obb_abs* obb, utl::Vec4d* mbs)
{
  //get the corners:
  std::vector< utl::Vec3d > tmp(src.size() * 8);
  // to absolute coordinates:
  for (int i = 0; i < src.size(); ++i)
  {
    //convert center:
    ctx.to_cartesian_space(sr, &src[i].center, 1);
    //get the corner in cartesian space:
    src[i].get_corners(&tmp[i * 8], 8);
  }
  // compute OBB:
  utl::Pro_hull hull(utl::Pro_set::get_default());
  double radius;
  hull.get_ball_box(tmp, *obb, radius, utl::Pro_hull::Method::Minimal_surface_area);

  //convert center back:
  ctx.from_cartesian_space(sr, &obb->center, 1);

  *mbs = utl::Vec4d(obb->center, radius);
}

status_t  convert_geometry(Writer_context& ctx, const Spatial_reference& sr, Mesh_abstract* mesh, utl::Obb_abs* obb, utl::Vec4d* mbs)
{
  std::vector< utl::Vec3d > pos; // absolute
  utl::Vec3d origin = mesh->get_origin();
  auto src_pos = mesh->get_relative_positions();
  // Legacy_point uses absolute position
  if (mesh->get_topology() == Mesh_topology::Points)
  {
    auto abs_points = mesh->get_absolute_positions();
    pos.resize(abs_points.size());
    std::memcpy(pos.data(), abs_points.data(), sizeof(utl::Vec3d) * abs_points.size());
  }
  else
  {
    // convert to absolute:
    pos.resize(src_pos.size());
    for (int i = 0; i < pos.size(); ++i)
    {
      pos[i] = origin + utl::Vec3d(src_pos[i]);
    }
  }
  compute_obb(ctx, sr, pos.data(), (int)pos.size(), obb, mbs);
  // make the geometry relative to the new center:
  std::vector< utl::Vec3f > rel_pos(src_pos.size());
  utl::Vec3f shift = utl::Vec3f(origin - obb->center);
  for (int i = 0; i < pos.size(); ++i)
  {
    rel_pos[i] = src_pos[i] + shift;
  }
  if (!mesh->update_positions(obb->center, rel_pos.data(), (int)rel_pos.size()))
    return utl::log_error(ctx.tracker(), IDS_I3S_INTERNAL_ERROR, std::string("position update"));

  return IDS_I3S_OK;
}


status_t scale_to_somewhat_anisotropic(Writer_context& ctx, const Spatial_reference& sr, Mesh_abstract* mesh, utl::Vec3f* scale)
{
  //estimate X/Y scale if globe mode:
  utl::Vec3d frame[3];
  frame[0] = mesh->get_origin();
  frame[1] = frame[0] + utl::Vec3d(1.0, 0.0, 0.0);
  frame[2] = frame[1] + utl::Vec3d(0.0, 1.0, 0.0);

  ctx.to_cartesian_space(sr, frame, 3);

  double scale_x = (frame[1] - frame[0]).length();
  double scale_y = (frame[2] - frame[0]).length();

  const double c_epsi = 10.0;
  if (std::abs(scale_x - 1.0) > c_epsi || std::abs(scale_x - 1.0) > c_epsi)
  {
    //scale coordinates:
    *scale = utl::Vec3f((float)scale_x, (float)scale_y, 1.0);
    std::vector<  utl::Vec3f > pos(mesh->get_relative_positions().size());
    //scale the relative position then:
    const auto& src_pos = mesh->get_relative_positions();
    for (int i = 0; i < pos.size(); ++i)
    {
      pos[i] = (*scale) * src_pos[i];
    }
    mesh->update_positions(mesh->get_origin(), pos.data(), (int)pos.size());
  }
  else
    *scale = utl::Vec3f(1.0);

  return IDS_I3S_OK;
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
    ret[i].name = descs[i].meta.name;
    ret[i].type = to_esri_type(descs[i].type);
    ret[i].alias = descs[i].meta.alias;
  }
  return ret;
}

static std::string  get_res_path(const std::string& name, const std::string& ref_path)
{
  return utl::make_path(ref_path, name).u8string();
}

status_t _pack(utl::Basic_tracker* trk, std::string* buf, utl::Mime_type type, utl::Mime_encoding* encoding
               , std::string* scratch, const std::string& res_name_for_error_report)
{
  if (*encoding == utl::Mime_encoding::Not_set && (type != utl::Mime_type::Jpeg && type != utl::Mime_type::Png))
  {
    I3S_ASSERT(buf->data() != scratch->data());
    if (!utl::compress_gzip(buf->data(), (int)buf->size(), scratch))
    {
      return utl::log_error(trk, IDS_I3S_COMPRESSION_ERROR, res_name_for_error_report, std::string("GZIP"));
    }
    buf->swap(*scratch);
    *encoding = utl::Mime_encoding::Gzip;
  }
  return IDS_I3S_OK;
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

static status_t append_to_slpk(utl::Basic_tracker* trk, utl::Slpk_writer* out, const std::string& name, const std::string& ref_path,
                               std::string* buf, utl::Mime_type type, utl::Mime_encoding* encoding, std::string* scratch, int* out_size)
{
  auto path = get_res_path(name, ref_path);
  auto status = _pack(trk, buf, type, encoding, scratch, path);
  if (status != IDS_I3S_OK)
    return status;
  *encoding = utl::Mime_encoding::Gzip;
  if (type == utl::Mime_type::Jpeg || type == utl::Mime_type::Png)
    * encoding = utl::Mime_encoding::Not_set;

  //hack mime-type for legacy:
  type = hack_mime_type_for_legacy(type); // :sad: 

  utl::add_slpk_extension_to_path(&path, type, *encoding);
  
  auto ret = out->append_file(path, buf->data(), (int)buf->size());
  if (!ret)
    return utl::log_error(trk, IDS_I3S_IO_WRITE_FAILED, std::string("SLPK://" + path));
  if (out_size)
    * out_size = (int)buf->size();
  return IDS_I3S_OK;
}

static status_t append_to_slpk(utl::Basic_tracker* trk, utl::Slpk_writer* out, const std::string& name, const std::string& ref_path,
                               const utl::Raw_buffer_view& buf, utl::Mime_type type, utl::Mime_encoding* encoding, std::string* scratch, int* out_size = nullptr)
{
  //TODO: unnecessary alloc+copy....
  std::string tmp;
  tmp.resize(buf.size());
  memcpy(&tmp[0], buf.data(), tmp.size());
  return append_to_slpk(trk, out, name, ref_path, &tmp, type, encoding, scratch, out_size);
}

static status_t save_json(utl::Basic_tracker* trk, utl::Slpk_writer* out, std::string& json_content, const std::string& name, const std::string& ref, std::string* scratch)
{
  auto encoding = utl::Mime_encoding::Not_set;
  return append_to_slpk(trk, out, name, ref, &json_content, utl::Mime_type::Json, &encoding, scratch, nullptr);
}

template< class T > static status_t save_json(utl::Basic_tracker* trk, utl::Slpk_writer* out, const T& obj, const std::string& name, const std::string& ref, std::string* scratch)
{
  //to json:
  auto json_content = utl::to_json(obj);
  if (json_content.empty())
    return utl::log_error(trk, IDS_I3S_INTERNAL_ERROR, std::string("Empty JSON"));

  auto encoding = utl::Mime_encoding::Not_set;
  return append_to_slpk(trk, out, name, ref, &json_content, utl::Mime_type::Json, &encoding, scratch, nullptr);
}

static int find_tex(const Multi_format_texture_buffer& src, Image_format format)
{
  auto found = std::find_if(src.begin(), src.end(), [format](const Texture_buffer& t) { return t.meta.format == format; });
  return found != src.end() ? (int)(found - src.begin()) : c_invalid_id;
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

//! generate the missing texture format based on what texture encoder have been provided in the context
static status_t create_texture_set(Writer_context& builder_ctx, Multi_format_texture_buffer& tex_set, const std::string& res_id_for_error_only)
{
  Multi_format_texture_buffer out;
  Image_formats desired_tex_out = (Image_formats)Image_format::Default;
  if (builder_ctx.decoder->encode_to_dxt_with_mips) 
      desired_tex_out |= (Image_formats)Image_format::Dds;
  
  if (builder_ctx.encode_to_etc2_with_mips) 
      desired_tex_out |= (Image_formats)Image_format::Ktx;
  

  auto trk = builder_ctx.tracker();
  struct Item
  {
    Image_format format;
    Writer_context::Encode_img_fct encoder;
    Texture_buffer tex;
  };
  std::vector<Item> dst_item;
  bool has_png = find_tex(tex_set,Image_format::Png) != c_invalid_id;

  int raw_idx = find_tex(tex_set, Image_format::Raw_rgba8);
  if (raw_idx == c_invalid_id)
      raw_idx = find_tex(tex_set, Image_format::Raw_rgb8);

  Texture_buffer raw_img;
  if (raw_idx != c_invalid_id)
      raw_img = tex_set[raw_idx];

  std::map<Image_formats, Writer_context::Encode_img_fct> encoder_funcs = {
      {(Image_formats)Image_format::Jpg, builder_ctx.encode_to_jpeg},
      {(Image_formats)Image_format::Png, builder_ctx.encode_to_png},
      {(Image_formats)Image_format::Dds, builder_ctx.decoder->encode_to_dxt_with_mips},
      {(Image_formats)Image_format::Ktx, builder_ctx.encode_to_etc2_with_mips} };


  //From option above, set desired textures to image formats.
  auto img_fmts = (Image_formats)Image_format::Default;
  img_fmts |= (Image_formats)desired_tex_out;

  //Get rid of jpg or png option, whichever is not included
  if (has_png) 
      img_fmts &= ~((Image_formats)Image_format::Jpg);
  else 
      img_fmts &= ~((Image_formats)Image_format::Png);

  utl::for_each_bit_set_conditional(img_fmts, [&desired_tex_out,&tex_set,&out,&encoder_funcs,&dst_item](const int i)
      {
          int idx_buffer = find_tex(tex_set, static_cast<Image_format>(1<<i));
          if (idx_buffer != c_invalid_id)
          {
              out.push_back(tex_set[idx_buffer]);
              if ((int)desired_tex_out & (1 << i))
                  desired_tex_out ^= (Image_formats)(1 << i); 
          }
          else dst_item.push_back(Item{ static_cast<Image_format>(1<<i),encoder_funcs[1<<i],Texture_buffer() });
          return true;
      });

  //If the desired_tex_out has been cleared of all bits, nothing to decompress.
  if (desired_tex_out == (Image_formats)Image_format::Not_set) 
      dst_item.clear();

  std::string scratch;
  for (auto& item : dst_item)
  {
    int idx = find_tex(tex_set, item.format);
    if (idx != c_invalid_id)
      item.tex = tex_set[idx];
    if (item.tex.empty())
    {
      if (item.encoder)
      {
        //need to create it then:
        //int vanilla_idx = c_invalid_id;
        if (raw_img.empty())
        {
          int pre_raw_idx = find_tex(tex_set, has_png ? Image_format::Png : Image_format::Jpg);
          if (has_png)
          {
              if (!builder_ctx.decoder || !builder_ctx.decoder->decode_png || !builder_ctx.decoder->decode_png(tex_set[pre_raw_idx].data, &raw_img))
                return utl::log_error(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, std::string("PNG"));
            //return status_t::Png_decoding_error;
          }
          else if (pre_raw_idx != c_invalid_id)
          {
            if (!builder_ctx.decoder || !builder_ctx.decoder->decode_jpeg || !builder_ctx.decoder->decode_jpeg(tex_set[pre_raw_idx].data, &raw_img))
              return utl::log_error(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, std::string("JPEG"));
            //return status_t::Jpg_decoding_error;
          }
          else
            return utl::log_error(trk, IDS_I3S_MISSING_JPG_OR_PNG, res_id_for_error_only);
          //return status_t::Jpeg_or_png_texture_required;
        }
        //at this point we must have a raw image we can use to check the alpha-channel:
        if ((int)raw_img.meta.alpha_status <= 0)
        {
          raw_img.meta.alpha_status = (Texture_meta::Alpha_status)get_alpha_bits(raw_img);
        }

        // check image dimensions aren't too large. using 4k as max dimension in height or width
        const auto max_dimension = builder_ctx.decoder->m_prop.max_texture_size;
        int w = raw_img.width();
        int h = raw_img.height();
        if (w > max_dimension || h > max_dimension)
        {
          utl::log_info(trk, IDS_I3S_IMAGE_TOO_LARGE, res_id_for_error_only, max_dimension, std::max(w, h));

          int w_res, h_res;
          if (w >= h)
          {
            w_res = max_dimension;
            h_res = static_cast<int>(h * static_cast<int64_t>(max_dimension) / w);
          }
          else
          {
            w_res = static_cast<int>(w * static_cast<int64_t>(max_dimension) / h);
            h_res = max_dimension;
          }

          const int pixel_size = raw_img.meta.format == Image_format::Raw_rgb8 ? 3 : 4;
          auto resampled_buffer = std::make_shared<utl::Buffer>(w_res * h_res * pixel_size);

          utl::resample_2d_uint8(w, h, w_res, h_res, raw_img.data.data(),
            resampled_buffer->create_writable_view().data(), pixel_size, utl::Alpha_mode::Pre_mult);

          raw_img.data = resampled_buffer->create_view();
          raw_img.meta.mip0_width = w_res;
          raw_img.meta.mip0_height = h_res;
        }

        if (!item.encoder(raw_img, &item.tex))
          return utl::log_error(trk, IDS_I3S_IMAGE_ENCODING_ERROR, res_id_for_error_only, to_string(item.format));
        //return status_t::Texture_compression_failed;
      }
      else
        continue; // skip this one.
    }
  }

  //copy back to input:
  for (auto& d : dst_item)
    if (!d.tex.empty())
      out.push_back(d.tex);

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

static bool remap_geometry_ids(std::map<int, int>* out, const std::atomic<int>* geometry_ids, int size)
{
  int next_id = 0;
  for (int i = 0; i != size; ++i)
  {
    if (geometry_ids[i])
      out->operator[](i) = next_id++;
  }
  return true;
}


namespace detail
{
class Material_helper
{
public:
  int     get_or_create_texture_set(Image_formats f, bool is_atlas);
  int     get_or_create_material(const Material_desc& data);
  const std::vector< Texture_definition_desc >&  get_texture_defs() const { return m_tex_def; }
  const std::vector< Material_desc >&            get_material_defs() const { return m_mat_def; }
  std::vector< std::string >                          get_legacy_texture_mime_types() const;
  //std::vector< Texture_data_ref_desc>                 get_legacy_texture_hrefs(int mat_id) const;

  //template< class Pred > void                for_each_base_color_tex_formats(int mat_id, Pred pred) const;
private:
  std::vector< Material_desc > m_mat_def;
  std::vector< Texture_definition_desc > m_tex_def;
  std::mutex                             m_mutex;
 
};

int Material_helper::get_or_create_texture_set(Image_formats f, bool is_atlas)
{
  {
    utl::Lock_guard lk(m_mutex);
    auto found = std::find_if(m_tex_def.begin(), m_tex_def.end(), [f, is_atlas](const Texture_definition_desc& desc)
    {
      if (desc.is_atlas != is_atlas)
        return false;
      Image_formats mask = 0;
      for (auto& f : desc.formats)
        mask |= (Image_formats)f.format;
      return mask == f;
    });

    if (found != m_tex_def.end())
      return (int)(found - m_tex_def.begin());
    Texture_definition_desc tex_def;
    utl::for_each_bit_set(f, [&tex_def](int bit_number)
    {
      Texture_format_desc fd;
      fd.format = (Image_format)(1 << bit_number);
      fd.name = to_compatibility_tex_name(fd.format);
      tex_def.formats.push_back( fd );
    });
    tex_def.is_atlas = is_atlas;
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
  status_t    save(utl::Basic_tracker* trk, utl::Slpk_writer* slpk, Perf_stats& perf, i3s::Layer_type layer_type );

  Node_desc_v17                       desc;
  utl::Raw_buffer_view                simple_geom;
  utl::Raw_buffer_view                draco_geom;
  std::vector< utl::Raw_buffer_view > attribute_buffers;
  Multi_format_texture_buffer         base_color_texs;
  Tri_state                           has_region = Tri_state::Not_set;
  Attrib_flags                        vb_attribs_mask_legacy;
  // --- legacy:
  Legacy_node_desc                    legacy_desc;
  Legacy_shared_desc                  legacy_shared;
  Legacy_feature_desc                 legacy_feature;

private:
  i3s::status_t _write_legacy_geometry(utl::Basic_tracker* trk, utl::Slpk_writer* slpk, Perf_stats& perf
                                       , std::string& scratch, const std::string& legacy_res_path, i3s::Layer_type layer_type);
  
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

i3s::status_t Node_io::_write_legacy_geometry(utl::Basic_tracker* trk, utl::Slpk_writer* slpk,
                                              Perf_stats& perf, std::string& scratch, const std::string& legacy_res_path,  i3s::Layer_type layer_type)
{
  auto enc = utl::Mime_encoding::Not_set;
  int out_size = 0;
  auto status = IDS_I3S_OK; // exit if not ok
  if (layer_type == i3s::Layer_type::Point)
  {
    //Legacy geometry is stored in JSON featureData ...
    //auto enc = utl::Mime_encoding::Not_set;
    //int out_size = 0;
    status = append_to_slpk(trk, slpk, "features/0", legacy_res_path, simple_geom, utl::Mime_type::Json, &enc, &scratch, &out_size);
    if (status != IDS_I3S_OK)
      return status;

    legacy_desc.geometry_data.clear();
    legacy_desc.feature_data.clear();
    legacy_desc.feature_data.push_back({ "./features/0" });
  }
  else
  {
    status = append_to_slpk(trk, slpk, "geometries/0", legacy_res_path, simple_geom, utl::Mime_type::Binary, &enc, &scratch, &out_size);
    if (status != IDS_I3S_OK)
      return status;
    perf.legacy_size += out_size;
    legacy_desc.geometry_data.clear();
    legacy_desc.geometry_data.push_back({ "./geometries/0" });
  }

  if (draco_geom.size())
  {
    out_size = 0;
    enc = utl::Mime_encoding::Not_set;
    status = append_to_slpk(trk, slpk, "geometries/1", legacy_res_path, draco_geom, utl::Mime_type::Binary, &enc, &scratch, &out_size);
    if (status != IDS_I3S_OK)
      return status;
    perf.draco_size += out_size;
  }
  return IDS_I3S_OK;
}

status_t Node_io::save(utl::Basic_tracker* trk, utl::Slpk_writer* slpk, Perf_stats& perf, i3s::Layer_type layer_type)
{
  std::string scratch;
  status_t status;
  //create the legacy hrefs:
  std::vector< std::string > legacy_node_ids = { std::to_string(desc.index) };
  if (is_root())
    legacy_node_ids.push_back("root"); //we have to duplicate.

  for (auto legacy_id : legacy_node_ids)
  {
    std::string legacy_res_path = "nodes/" + legacy_id + "/";
    if (simple_geom.size())
    {
      // --- write the textures:
      legacy_desc.texture_data.clear(); //the last one (if node is "root") will override.
      for (auto& tex : base_color_texs)
      {
        auto mime_type = to_utl_mime_type(to_mime_type(tex.meta.format));
        if (mime_type == utl::Mime_type::Dds_legacy)
          mime_type = utl::Mime_type::Dds_proper;
        auto tex_name = "textures/" + to_compatibility_tex_name(tex.meta.format);
        auto enc = utl::Mime_encoding::Not_set;
        status = append_to_slpk(trk, slpk, tex_name, legacy_res_path, tex.data, mime_type, &enc, &scratch);
        if (status != IDS_I3S_OK)
          return status;
        legacy_desc.texture_data.push_back({ "./" + tex_name });
      }

      // --- write the geometries:
      status = _write_legacy_geometry( trk, slpk, perf, scratch, legacy_res_path, layer_type);
      if (status != IDS_I3S_OK)
        return status;

      // --- write the attributes:
      int loop = 0;
      legacy_desc.attribute_data.clear();
      for (auto& attrib : attribute_buffers)
      {
        auto enc = utl::Mime_encoding::Not_set;
        std::string name = "attributes/f_" + std::to_string(loop) + "/0";
        status = append_to_slpk(trk, slpk, name, legacy_res_path, attrib, utl::Mime_type::Binary, &enc, &scratch);
        if (status != IDS_I3S_OK)
          return status;
        legacy_desc.attribute_data.push_back({ "./" + name });
        ++loop;
      }
      if (legacy_id == legacy_node_ids.back() && layer_type != Layer_type::Point)
      {
        //write legacy sharedResource:
        status = save_json(trk, slpk, legacy_shared, "shared/sharedResource", legacy_res_path, &scratch);
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
        status = save_json(trk, slpk, legacy_feature, "features/0", legacy_res_path, &scratch);
        if (status != IDS_I3S_OK)
          return status;
      }
    }
    if (legacy_id == legacy_node_ids.back())
    {
      //write the legacy node index document:
      status = save_json(trk, slpk, legacy_desc, "3dNodeIndexDocument", legacy_res_path, &scratch);
      if (status != IDS_I3S_OK)
        return status;
    }
  }
  return IDS_I3S_OK;
}


static status_t set_material(Writer_context& builder_ctx, detail::Material_helper& helper,
                             const Material_data& mat_, Node_io& nio)
{
  auto trk = builder_ctx.tracker();

  std::array< int, (int)Texture_semantic::_count > tex_set_id_by_semantic = { -1,-1,-1,-1,-1,-1 };

  //int actual_alpha_bits = 0;
  auto mat_copy = mat_;

  //write the legacy shared resource:
  if (mat_copy.legacy_json.size())
  {
    if (!utl::from_json_safe(mat_copy.legacy_json, &nio.legacy_shared, trk, "nodes/" + nio.legacy_desc.id + std::string("/sharedResource")))
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
  if (mat_copy.metallic_roughness.base_color_tex.size())
  {
    auto& tex_set = mat_copy.metallic_roughness.base_color_tex;
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
    tex_set_id_by_semantic[(int)Texture_semantic::Base_color] = helper.get_or_create_texture_set(get_formats(tex_set), use_atlas);
    nio.base_color_texs = tex_set;
    I3S_ASSERT(tex_set.empty() || tex_set.front().width() > 0 && tex_set.front().height() > 0);
    nio.desc.mesh.material.texel_count_hint = tex_set.size() ? tex_set.front().width() * tex_set.front().height() : 0;
  }
  // TODO: other texture semantics

  //create the v17 material:
  Material_desc mat17;
  mat17.alpha_cutoff = (float)mat_copy.properties.alpha_cut_off / 255.0f;
  mat17.alpha_mode = mat_copy.properties.alpha_mode;
  mat17.emissive_factor = mat_copy.properties.emissive_factor;
  mat17.is_double_sided = mat_copy.properties.double_sided;
  mat17.cull_face = mat_copy.properties.cull_face;
  mat17.metal.base_color_factor = mat_copy.metallic_roughness.base_color_factor;
  if (tex_set_id_by_semantic[(int)Texture_semantic::Base_color] != -1)
  {
    mat17.metal.base_color_tex.tex_coord_set = 0;
    mat17.metal.base_color_tex.tex_def_id = tex_set_id_by_semantic[(int)Texture_semantic::Base_color];
  }
  //write it out:
  nio.desc.mesh.material.definition_id = helper.get_or_create_material(mat17);
  return IDS_I3S_OK;
}


//-----------------------------------------------------------
} //endof ::detail





// ---------------------------------------------------------------------------------------------
//        class:      Layer_writer_impl
// ---------------------------------------------------------------------------------------------
const int Layer_writer_impl::c_default_page_size = 64;

Layer_writer_impl::Layer_writer_impl(utl::Slpk_writer::Ptr slpk, Writer_context::Ptr ctx)
  : m_ctx(ctx)
  , m_slpk(slpk)
  , m_mat_helper(std::make_unique<detail::Material_helper>())
{
}

// see screen_size_to_area() function for the reverse.
static Lod_selection_desc convert_lod_selection_from_SQ_to_size(double v)
{
  //*s *= (*s) * 3.1415927 * 0.25;
  I3S_ASSERT(v >= 0.0);
  Lod_selection_desc out;
  out.metric_type = Lod_metric_type::Max_screen_size;
  out.max_error = 2.0 * std::sqrt(v / 3.1415927);
  return out;
}

static bool all_faces_degenerate(const std::vector< utl::Vec3f>& vtx, int count)
{
  const float c_epsi = 1e-3f;
  I3S_ASSERT(count % 3 == 0);

  for (int i = 0; i < count; i += 3)
  {
    //for each triangle:
    auto& v1 = vtx[i];
    auto& v2 = vtx[i + 1];
    auto& v3 = vtx[i + 2];

    auto a = utl::Vec3f::l1_distance(v1, v2);  // side v1v2
    auto b = utl::Vec3f::l1_distance(v2, v3);  // side v2v3
    auto c = utl::Vec3f::l1_distance(v3, v1);  // side v3v1

    // if true, all faces are not degenerate
    if (a > c_epsi && b > c_epsi && c > c_epsi) {
      return false;
    }
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
static bool convert_vtx(std::vector<utl::Vec3d>* vtx, int count,  const Writer_context& ctx, Normal_reference_frame nrf, const utl::Vec3d& origin)
{
  auto to_cartesian{ [&ctx](auto* vtx, int count) { ctx.to_cartesian_space({ 4326 }, vtx, count); } };
  switch (nrf)
  {
    case Normal_reference_frame::Earth_centered:
      to_cartesian(vtx->data(), count);
      return true;
    case Normal_reference_frame::East_north_up:
    {
      // to cartesian
      to_cartesian(vtx->data(), count);
      // to ENU
      auto ref{ origin };
      to_cartesian(&ref, 1);
      for (auto& v : *vtx) cartesian_to_enu(&v, ref, origin);
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
static bool is_implicit_normals(std::shared_ptr<Mesh_abstract> mesh, const Writer_context& ctx, Normal_reference_frame nrf)
{
  const auto& normals = mesh->get_normals();
  if (normals.values.size() == 0)
    return true;

  const auto& vtx = mesh->get_absolute_positions();
  const auto count = vtx.size();
  I3S_ASSERT(normals.values.size() == count);
  I3S_ASSERT(count % 3 == 0);
  // convert points to cartesian
  std::vector<utl::Vec3d> cartesian(count);
  memcpy(cartesian.data(), vtx.data(), sizeof(utl::Vec3d) * count);

  convert_vtx(&cartesian, count, ctx, nrf, mesh->get_origin());

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

status_t Layer_writer_impl::create_output_node(const Simple_node_data& node, Node_id node_id)
{
  std::string scratch;
  status_t status;

  auto trk = m_ctx->tracker();

  auto nio = std::make_unique<detail::Node_io>();
  //*node_id = c_invalid_id;
  I3S_ASSERT(node.node_depth >= 0);
  //const bool is_root = node.node_depth == 0;
  nio->legacy_desc.level = node.node_depth;
  nio->desc.index = static_cast<int>(node_id);
  nio->legacy_desc.id = nio->is_root() ? "root" : std::to_string(nio->desc.index);
  //const int c_single_attrib_set = 0;

  // --- Lod selection: 
  Lod_selection_desc lod;
  lod.max_error = node.lod_threshold;
  lod.metric_type = Lod_metric_type::Max_screen_area; //all converted to that...

  nio->legacy_desc.lod_selection.push_back(lod);
  // write an older metrics too for older clients:
  nio->legacy_desc.lod_selection.push_back(convert_lod_selection_from_SQ_to_size(lod.max_error));
  // v1.7+:
  nio->desc.lod_threshold = lod.max_error; //TODO: select the same one for the layer.

  bool degenerated_mesh = false;

  // --- Geometry:
  if (node.mesh.geometries.size())
  {
    // --- create single mesh (with 2 geometry buffers : simple i3s and draco compressed):
    Mesh_desc_v17& m17 = nio->desc.mesh;
    //m17.attribute.definition_id = c_single_attrib_set; // ONLY one attribute definition for v1.7
    m17.attribute.resource_id = nio->desc.index;
    m17.geometry.resource_id = nio->desc.index;
    m17.material.resource_id = nio->desc.index;

    auto legacy_mesh_buffer = node.mesh.geometries.front();
    //bool is_legacy_format = legacy_mesh_buffer->get_format() == Geometry_format::Legacy || legacy_mesh_buffer->get_format() == Geometry_format::Point_legacy;
    auto legacy_mesh = node.mesh.geometries.front()->get_mesh();

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

    //Compute OBB and shift to new center:
    convert_geometry(*m_ctx, m_layer_meta.sr, legacy_mesh.get(), &nio->legacy_desc.obb, &nio->legacy_desc.mbs);
    nio->desc.obb = nio->legacy_desc.obb;

    // "convert" to legacy:
    _encode_geometry_to_legacy(*nio, *legacy_mesh_buffer);

    // copy the feature data over:
    nio->legacy_feature.feature_data.raw = node.mesh.legacy_feature_data_json;
    nio->legacy_feature.geometry_data.raw = node.mesh.legacy_feature_data_geom_json;

    bool drop_normals = true; // will be false if node contains >= 1 smooth normal
    bool drop_colors = false; // true if all color values set to [255,255,255, 255]

    if (m_ctx->encode_to_draco && (m_layer_meta.type == i3s::Layer_type::Mesh_3d || m_layer_meta.type == i3s::Layer_type::Mesh_IM || m_layer_meta.type == i3s::Layer_type::Point) )
    {
      if (!m_ctx->decoder->m_prop.is_drop_normals)
        drop_normals = is_implicit_normals(legacy_mesh, *m_ctx, m_layer_meta.normal_reference_frame);

      if (drop_normals)
        legacy_mesh->drop_normals(); // client will recompute them.

      static const Rgba8 opaque_white{ 0xFF, 0xFF, 0xFF, 0xFF };
      const auto &colors = legacy_mesh->get_colors();
      const auto iter = std::find_if_not(colors.values.begin(), colors.values.end(), [](auto &a) {return a == opaque_white; });
      drop_colors = iter == colors.values.end(); // drop if all opaque white

      if (drop_colors) 
        legacy_mesh->drop_colors();

      int bad_uv_count= legacy_mesh->sanitize_uvs(); //Draco doesn't like garbage UV
      if (bad_uv_count && m_ctx->tracker())
        utl::log_warning(trk, (int)IDS_I3S_BAD_UV, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"), bad_uv_count);
  
      //scale it:
      utl::Vec3f scale;
      auto src_pos = legacy_mesh->get_relative_positions();
      I3S_ASSERT(!src_pos.index.is_valid());
      std::vector< utl::Vec3f> backup_pos(src_pos.values.size());
      memcpy(backup_pos.data(), src_pos.values.data(), sizeof(utl::Vec3f) * backup_pos.size());

      scale_to_somewhat_anisotropic(*m_ctx, m_layer_meta.sr, legacy_mesh.get(), &scale);

      // create a Draco version for it:
      if (!m_ctx->encode_to_draco(*legacy_mesh, &nio->draco_geom, (double)scale.x, (double)scale.y))
      {
        // DRACO will fail on degenerated mesh ( all faces are degenerated)
        // need to add the node ID to help with error reporting.
        std::vector< utl::Vec3f> scaled_pos(src_pos.values.size());
        memcpy(scaled_pos.data(), src_pos.values.data(), sizeof(utl::Vec3f) * scaled_pos.size());
        if (!all_faces_degenerate(scaled_pos, (int)scaled_pos.size()))
          return utl::log_error(trk, IDS_I3S_COMPRESSION_ERROR, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"), std::string("DRACO"));
        else {
          // all faces were degenerate
          degenerated_mesh = true;
          utl::log_warning(trk, IDS_I3S_DEGENERATED_MESH, std::string("/nodes/" + nio->legacy_desc.id + "/geometry"));
        }
      }
      //restore un-scaled position:
      legacy_mesh->update_positions(legacy_mesh->get_origin(), backup_pos.data(), (int)backup_pos.size());
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
      status = set_material(*m_ctx, *m_mat_helper, node.mesh.material, *nio);
      if (status != IDS_I3S_OK)
        return status;

      // --- attribute data:
      if (node.mesh.attribs.size())
      {
        int c_single_schema = 0;
        if (m_attrib_metas.empty())
        {
          m_attrib_metas.resize(1);
          // set it up:
          m_attrib_metas[0].resize(node.mesh.attribs.size());
        }
        std::vector< Type*  > expected_types(m_attrib_metas[c_single_schema].size());
        for (int i = 0; i < expected_types.size(); ++i)
          expected_types[i] = &m_attrib_metas[c_single_schema][i].def.type;

        nio->attribute_buffers.resize(node.mesh.attribs.size());
        for (int i = 0; i < node.mesh.attribs.size(); ++i)
        {
          //type checking:
          if (i >= (int)expected_types.size())
            return utl::log_error(trk, IDS_I3S_OUT_OF_RANGE_ID, std::string("attribute"), i, (int)expected_types.size());
          auto& attrib = node.mesh.attribs[i];
          if (*expected_types[i] == Type::Not_set)
            *expected_types[i] = attrib->get_type();
          if (attrib->get_type() != *expected_types[i])
            return utl::log_error(trk, IDS_I3S_TYPE_MISMATCH, std::string("attribute")
              , to_string(attrib->get_type()), to_string(*expected_types[i]));
          nio->attribute_buffers[i] = attrib->get_raw_data();
        }
        //status = write_attributes(trk, node_res_id, node.mesh.attribs.data(), (int)node.mesh.attribs.size(), m_slpk.get()
        //                          , &legacy_desc.attribute_data, &scratch, expected_types);
        if (status != IDS_I3S_OK)
          return status;
      }
    } // end of node with mesh
    else
    {
      m17.attribute = Mesh_attribute_ref_desc();
      m17.geometry = Mesh_geometry_ref_desc();
      m17.material = Mesh_material_ref_desc();
      nio->simple_geom = utl::Raw_buffer_view();
    }

  } //end of node with geometry
  else
  {
    nio->desc.mesh.geometry = Mesh_geometry_ref_desc();//no mesh for you.
    //nio->desc.meshes.clear(); 
    //if (node.children.empty())
    //  return status_t::Leaf_node_must_have_data; //more of a warning. Node could be simply ignored.
  }

  Node_brief brief;
  brief.obb = nio->legacy_desc.obb;
  brief.mbs = nio->legacy_desc.mbs;
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
      return false;
  } // -> unlock;

  auto& nio = node_brief->second.node;
  auto trk = m_ctx->tracker();
  //process children:
  bool is_no_data_node = node.mesh.geometries.empty();
  std::vector< utl::Obb_abs> ch_obbs;
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
        return utl::log_error(m_ctx->tracker(), IDS_I3S_INVALID_TREE_TOPOLOGY, ch_id); // Node can only have one parent.
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
    nio->desc.children.push_back((int)ch_id);
    if (is_no_data_node)
      ch_obbs.push_back(ch_desc.obb);
    //connect the parent:
    children_to_write.push_back(std::move(ch_node_brief.node));
  }
  if (ch_obbs.size())
  {
    compute_obb(*m_ctx, m_layer_meta.sr, ch_obbs, &nio->legacy_desc.obb, &nio->legacy_desc.mbs);
    nio->desc.obb = nio->legacy_desc.obb;
  }

  if (nio->legacy_desc.obb.is_valid())
  {
    //We can now connect to the children  and dump them:
    for (auto& ch : children_to_write)
    {
      ch->set_parent(*nio);
      //Once, the parent is set, we can commit each child to SLPK:
      _write_node(*ch);
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


void Layer_writer_impl::_write_node( detail::Node_io& nio)
{
  nio.save(m_ctx->tracker(), m_slpk.get(), m_perf, m_layer_meta.type);

  //add to page v17:
  {
    utl::Lock_guard lk(m_mutex);
    if (nio.desc.index >= m_nodes17.size())
      m_nodes17.resize(nio.desc.index + 1);
    m_nodes17[nio.desc.index] = nio.desc;
  } // -> unlock

  if (nio.desc.mesh.geometry.definition_id >= 0)
    m_geometry_defs[nio.desc.mesh.geometry.definition_id]++;
}

status_t Layer_writer_impl::_save_paged_index( int root_id )
{
  auto trk = m_ctx->tracker();

  I3S_ASSERT(root_id < m_nodes17.size());
  // we need to re-order the tree breadth first:
  struct Item
  {
    Item(Node_desc_v17* n, uint32_t d) : node(n), depth(d) {}
    Node_desc_v17* node;
    uint32_t  depth;
  };

  std::map<int, int> geometry_ids; // only want geometry defs that are actually used
  remap_geometry_ids(&geometry_ids, m_geometry_defs.data(), (int)m_geometry_defs.size());

  auto update_geometry_id = [&geometry_ids](int* inout)
  {
    auto found = geometry_ids.find(*inout);
    if (found == geometry_ids.end())
      return false;
    *inout = found->second;
    return true;
  };

  std::string scratch;
  std::deque<Item > queue;
  m_nodes17[root_id].index = 0; //update the root_id
  update_geometry_id(&m_nodes17[root_id].mesh.geometry.definition_id);
  queue.push_back({ &m_nodes17[root_id], 0 }); //root is node 0
  int visit_count = 0;
  Node_page_desc_v17 current_page;
  int page_id = 0;
  current_page.nodes.resize(c_default_page_size);
  while (queue.size())
  {
    Item item = queue.front();
    queue.pop_front();
    ++visit_count;
    int ch0 = visit_count + (int)queue.size();
    int node_id = (visit_count - 1) % c_default_page_size;
    //get the children:
    for ( int& ch_id : item.node->children )
    { 
      int updated_id = ch0++;
      //rewrite childen index:
      m_nodes17[ch_id].index = updated_id;
      m_nodes17[ch_id].parent_index = node_id;
      update_geometry_id(&m_nodes17[ch_id].mesh.geometry.definition_id);
      queue.push_back({ &m_nodes17[ ch_id ], item.depth + 1 });
      //update childen in parent:
      ch_id = updated_id;
    }
    current_page.nodes[node_id] = *item.node;
    //write the page out ?
    if(node_id == c_default_page_size-1)
    {
      auto status = save_json(trk, m_slpk.get(), current_page, std::to_string(page_id++), "nodepages", &scratch);
      if (status != IDS_I3S_OK)
        return status;
    }
  }
  int left_over = visit_count % c_default_page_size;
  //write the last (incomplete?) page :
  if (left_over)
  {
    current_page.nodes.resize(left_over);
    auto status = save_json(trk, m_slpk.get(), current_page, std::to_string(page_id++), "nodepages", &scratch);
    if (status != IDS_I3S_OK)
      return status;
  }

  return IDS_I3S_OK;
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
    if (src.type == Type::String_utf8)
    {
      dst.header.push_back({ Attrib_header_property::Attribute_values_byte_count, Type::UInt32 });
      dst.ordering = { Attrib_ordering::Attribute_byte_counts, Attrib_ordering::Attribute_values };
      dst.attribute_values = { Type::String_utf8, 1, Value_encoding::Utf8 };
      dst.attribute_byte_counts= { Type::UInt32, 1 };
    }
    else
    {
      dst.ordering = { Attrib_ordering::Attribute_values };
      dst.attribute_values = { src.type,  1 };
    }
  }
  return ret;
} 

Layer_writer_impl::Attrb_info& Layer_writer_impl::_get_attrib_meta(Attrib_schema_id sid, Attrib_index idx)
{
  if (m_attrib_metas.size() <= sid)
    m_attrib_metas.resize(sid + 1);
  auto& schema = m_attrib_metas[sid];
  if (schema.size() <= idx)
    schema.resize(idx + 1);
  return schema[idx];
}


status_t Layer_writer_impl::set_attribute_meta(Attrib_index idx, const Attribute_meta& meta, Attrib_schema_id sid )
{
  _get_attrib_meta(sid, idx).def.meta = meta;
  return IDS_I3S_OK;
}

status_t   Layer_writer_impl::set_attribute_stats( Attrib_index idx, Stats_attribute::ConstPtr stats, Attrib_schema_id sid)
{
  _get_attrib_meta(sid, idx).stats = stats;
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
const std::string c_metadata_json_path{"metadata.json"};
}

bool create_point_store_desc(Layer_desc* desc, const utl::Vec4d& extent)
{
  desc->store = Store_desc();
  desc->store.resource_pattern = { "3dNodeIndexDocument", "Attributes", "Geometry", "featureData" };
  desc->store.id = "0"; //useless but required.
  desc->store.index_crs = to_epsg_url(desc->spatial_ref);
  desc->store.vertex_crs = to_epsg_url(desc->spatial_ref);
  desc->store.extent = extent;  // to_extent(*m_ctx, m_layer_meta.sr, root_obb);
  desc->store.version = "1.7"; //redundant and confusing...
  return true;
}

status_t Layer_writer_impl::save()
{
  auto trk = m_ctx->tracker();
  if (m_working_set.size() != 1)
  {
    //return status_t::Unconnected_nodes;
    return utl::log_error(trk, IDS_I3S_INVALID_TREE_TOPOLOGY, m_working_set.size() - 1);
  }

  const auto found = std::cbegin(m_working_set);
  const auto root_id = found->second.node->desc.index;

  //save the root:
  _write_node(*found->second.node);

  auto root_obb = found->second.obb;

  auto status = _save_paged_index( (int)root_id);
  if (status != IDS_I3S_OK)
    return status;


  Layer_desc desc;
  desc.id = 0;
  desc.name = m_layer_meta.name;
  desc.version = m_layer_meta.uid;
  desc.time_stamp.last_update = m_layer_meta.timestamp == 0 ? std::time(nullptr) * 1000ull : m_layer_meta.timestamp;
  //desc.href = "./0"; //makes no sense...
  desc.layer_type = m_layer_meta.type;
  static_assert(sizeof(geo::SR_def) == sizeof(m_layer_meta.sr), "unexpected size");
  desc.spatial_ref = (const geo::SR_def&)m_layer_meta.sr;
  desc.alias = m_layer_meta.name;
  desc.description = m_layer_meta.desc;
  desc.capabilities = { "View", "Query" };
  desc.drawing_info.raw = m_layer_meta.drawing_info;
  desc.elevation_info.raw = m_layer_meta.elevation_info;
  desc.popup_info.raw = m_layer_meta.popup_info;

  if (m_layer_meta.type != Layer_type::Point)
  {
    desc.store.id = "0"; //useless but required.
    desc.store.profile =  "meshpyramids";
    desc.store.resource_pattern = { "3dNodeIndexDocument", "SharedResource", "Geometry", "Attributes" }, // required but useless..
      desc.store.root_node = "./nodes/root";// +std::to_string(root_id);
    desc.store.index_crs = to_epsg_url(desc.spatial_ref);
    desc.store.vertex_crs = to_epsg_url(desc.spatial_ref);
    //nidencoding, featureencoding, attributeEncoding // obsolete.
    desc.store.lod_type= "MeshPyramid"; //useless
    desc.store.lod_model = "node-switching"; //useless
    //I3S_ASSERT(m_tex_defs.size() < 2); //What to do when different type of "sets" are used ?
    //desc.store.texture_encodings = get_texture_mime_types(m_tex_defs.size() ? m_tex_defs.front().tex_format_set : 0 );
    desc.store.texture_encodings = m_mat_helper->get_legacy_texture_mime_types();
    desc.store.extent = to_extent(*m_ctx, m_layer_meta.sr, root_obb);
    desc.store.version = "1.7"; //redundant and confusing...
    desc.store.geometry_schema = create_legacy_geometry_schema(m_vb_attribs_mask_legacy);
    desc.store.normal_reference_frame = m_layer_meta.normal_reference_frame;
  }
  else
  {
    create_point_store_desc(&desc, to_extent(*m_ctx, m_layer_meta.sr, root_obb));
  }

  static_assert( sizeof(Height_model_info_desc) == sizeof(m_layer_meta.height_model_info), "unexpected size" );
  desc.height_model_info = reinterpret_cast<const Height_model_info_desc&>(m_layer_meta.height_model_info);
  if (m_attrib_metas.size() > 0)
  {
    std::vector< Attribute_definition > attr_defs(m_attrib_metas.front().size());
    for (int i = 0; i < attr_defs.size(); ++i)
      attr_defs[i] = m_attrib_metas.front()[i].def;
    desc.attribute_storage_info = create_legacy_attrib_info(attr_defs);
    desc.fields = create_fields(attr_defs);
  }
  // --- v17 Node page:
  desc.node_pages.nodes_per_page = c_default_page_size;
  desc.node_pages.lod_metric_type = Lod_metric_type::Max_screen_area; //TODO!!!!
  //---- v17 material / texture sets
  desc.material_defs = m_mat_helper->get_material_defs();
  desc.tex_defs = m_mat_helper->get_texture_defs();
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
      udpated_vb_attribs &= ~(Attrib_flags)attrib_flag.at(0x01<<bit_number);
      return true;
    });
    create_mesh_desc(udpated_vb_attribs, m_vb_attribs_mask_legacy, &draco_all_defs[i], (bool)m_ctx->encode_to_draco, desc.layer_type != Layer_type::Point);
  }

  std::map<int, int> geometry_ids;
  remap_geometry_ids(&geometry_ids, m_geometry_defs.data(), (int)m_geometry_defs.size());

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
  std::string scratch;
  // write the href:
  for (int sid = 0; sid < m_attrib_metas.size(); ++sid)
    for (int i = 0; i < m_attrib_metas[sid].size(); ++i)
    {
      if (m_attrib_metas[sid][i].stats)
      {
        //create stats info entry:
        Statistics_href_desc shd;
        auto name = "f_" + std::to_string(i) + "/0";
        shd.href = "./statistics/" + name ; //Pro 2.3- expects this. 
        shd.key = "f_" + std::to_string(i);
        shd.name = m_attrib_metas[sid][i].def.meta.name;
        desc.statistics_info.push_back(shd); 
        auto json_stats = m_attrib_metas[sid][i].stats->to_json();
        auto st = save_json(trk, m_slpk.get(), json_stats, name, "statistics", &scratch);
        if (st != IDS_I3S_OK)
          return st;
      }
      else if(m_attrib_metas[sid][i].def.type != Type::Oid32) // don't expect stats for objectId
        utl::log_warning(trk, IDS_I3S_MISSING_ATTRIBUTE_STATS, m_attrib_metas[sid][i].def.meta.name);

    }
  status = save_json(trk, m_slpk.get(), desc, "3dSceneLayer", "", &scratch);
  if (status != IDS_I3S_OK)
    return status;

  // Add metadata.json to the slpk.
  const auto metadata_json =
    "{\n  \"I3SVersion\": \"1.7\",\n  \"nodeCount\": " + std::to_string(m_nodes17.size()) + "\n}";

  if (!m_slpk->append_file(c_metadata_json_path, metadata_json.data(), static_cast<int>(metadata_json.size())))
    return utl::log_error(trk, IDS_I3S_IO_WRITE_FAILED, "SLPK://" + c_metadata_json_path);

  //print compression ratio:
  utl::log_debug(trk, IDS_I3S_GEOMETRY_COMPRESSION_RATIO, std::to_string( m_perf.get_ratio()));
  return IDS_I3S_OK;
}


Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, const std::filesystem::path& path)
{
  utl::Slpk_writer::Ptr writer(utl::create_slpk_writer());
  if (writer && writer->create_archive(path))
    return new Layer_writer_impl(writer, ctx);

  utl::log_error(ctx->tracker(), IDS_I3S_IO_OPEN_FAILED, path);
  return nullptr;
}

Layer_writer* create_mesh_layer_builder(Writer_context::Ptr ctx, utl::Slpk_writer::Ptr writer)
{
    return ( writer ? new Layer_writer_impl(writer, ctx) : nullptr );
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
  //take care of the material:
  dst.material = Material_data(); //clear
  if(!src.img.empty())
    dst.material.metallic_roughness.base_color_tex.push_back(src.img); // shallow copy

  utl::Obb_abs obb;
  utl::Vec4d mbs;

  std::vector< utl::Vec3d> scratch( src.vertex_count);
  memcpy(scratch.data(), src.abs_xyz, scratch.size() * sizeof(utl::Vec3d));

  // create bulk data to assign:
  compute_obb((Writer_context&)(*m_ctx), m_layer_meta.sr, scratch.data(), (int)scratch.size(), &obb, &mbs);
  auto rel_pos = utl::Buffer::create_writable_typed_view< utl::Vec3f>(src.vertex_count);
  for (int i = 0; i < rel_pos.size(); ++i)
  {
    rel_pos[i] = utl::Vec3f(src.abs_xyz[i] - obb.center);
  }
  //create the bulk:
  Mesh_bulk_data bulk;
  bulk.origin = obb.center;
  bulk.rel_pos.values = rel_pos;

  auto color_values = utl::Buffer::create_writable_typed_view<Rgba8>(src.vertex_count);
  memset(color_values.data(), 0xFF, src.vertex_count * sizeof(Rgba8));
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

  Mesh_abstract::Ptr mesh(parse_mesh_from_bulk(bulk));
  I3S_ASSERT(mesh);
  dst.geometries = { std::make_shared< Geometry_buffer_simple_impl>( mesh ) };
  return IDS_I3S_OK;
}

status_t Layer_writer_impl::create_mesh_from_raw(const Simple_raw_points& src, Mesh_data& dst) const
{
  I3S_ASSERT(src.abs_xyz && src.count);

  utl::Obb_abs obb;
  utl::Vec4d mbs;

  std::vector< utl::Vec3d> scratch(src.count);
  memcpy(scratch.data(), src.abs_xyz, scratch.size() * sizeof(utl::Vec3d));

  // create bulk data to assign:
  compute_obb((Writer_context&)(*m_ctx), m_layer_meta.sr, scratch.data(), (int)scratch.size(), &obb, &mbs);
  auto rel_pos = utl::Buffer::create_writable_typed_view< utl::Vec3f>(src.count);
  for (int i = 0; i < rel_pos.size(); ++i)
  {
    rel_pos[i] = utl::Vec3f(src.abs_xyz[i] - obb.center);
  }
  //create the bulk:
  Mesh_bulk_data bulk;
  bulk.origin = obb.center;
  bulk.rel_pos.values = rel_pos;

  if (src.fids)
  {
    bulk.fids.values = utl::Buffer::create_deep_copy( src.fids, src.count);
  }
  i3s::Mesh_abstract::Ptr mesh(parse_mesh_from_bulk(bulk));
  I3S_ASSERT(mesh);
  mesh->set_topology( i3s::Mesh_topology::Points);
  dst.geometries = { std::make_shared< Geometry_buffer_simple_impl>(mesh) };
  return IDS_I3S_OK;
}

}

} // namespace i3slib
