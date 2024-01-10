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

#include "utils/utl_jpeg.h"
#include "utils/utl_png.h"
#include "i3s/i3s_writer.h"
#include "utils/utl_image_2d.h"
#include "i3s/i3s_legacy_mesh.h"
#include "lepcc_tpl_api.h"
#include "utils/utl_geographic.h"

#include <stdint.h>

#ifndef NO_ETC2_SUPPORT
#include "utils/utl_etc2comp_c_api.h"
#endif

#ifndef NO_DRACO_SUPPORT
#include "utils/utl_libdraco_api.h"
#endif

#ifndef NO_DXT_ENCODER_SUPPORT
#include "utils/dxt/utl_dxt_mipmap_dds.h"
#endif

#if !defined (NO_BASIS_ENCODER_SUPPORT) || !defined (NO_BASIS_TRANSCODER_SUPPORT)
#include "utils/utl_libbasis_api.h"
#endif

namespace i3slib
{

namespace i3s
{

#ifndef NO_DXT_ENCODER_SUPPORT

namespace
{



void add_alpha_to_rgb(const uint8_t* rgb, uint32_t pixel_count, uint8_t* rgba, uint8_t alpha)
{
  const auto size = pixel_count * 3;
  uint32_t out = 0;
  for (uint32_t in = 0; in < size; in += 3, out += 4)
  {
    rgba[out] = rgb[in];
    rgba[out + 1] = rgb[in + 1];
    rgba[out + 2] = rgb[in + 2];
    rgba[out + 3] = alpha;
  }
  I3S_ASSERT(out == pixel_count * 4);
}

}

//----- experimental: -------------------
static bool  compress_to_dds_with_mipmaps(const Texture_buffer& img, Texture_buffer* dds_buffer)
{
  const auto w = img.width();
  const auto h = img.height();
  utl::Image_2d src;

  if (img.meta.format == Image_format::Raw_rgb8)
  {
    src = utl::Image_2d::create_aligned(w, h);
    if (!src.data())
      return false;

    add_alpha_to_rgb(reinterpret_cast<const uint8_t*>(img.data.data()), w* h, static_cast<uint8_t*>(src.data()), 255);
  }
  else
  {
    I3S_ASSERT_EXT(img.meta.format == Image_format::Raw_rgba8);

    //deep copy, since dds compressor will modify the input ( in-place mipmap)
    src = utl::Image_2d::create_aligned(w, h, img.data.data(), img.data.size());
    if (!src.data())
      return false;
  }

  utl::Buffer_view< char > dds;
  I3S_ASSERT_EXT((int)img.meta.alpha_status != -1); // ALPHA status must have been set at this point.
  if (!compress_to_dds_with_mips(src, img.meta.alpha_status != Texture_meta::Alpha_status::Opaque, &dds))
    return false;

  dds_buffer->data = dds;
  dds_buffer->meta = img.meta;
  dds_buffer->meta.mip_count = -1; //dunno. 
  dds_buffer->meta.format = Image_format::Dds;

  return true;
}
#endif // --------------------------- end DXT encoder support -----

void _to_relative(const utl::Vec3d& origin, const  utl::Vec3d* src, utl::Vec3f* dst, size_t count)
{
  for (size_t i = 0; i < count; ++i)
    dst[i] = utl::Vec3f(src[i] - origin);
}

i3s::Mesh_abstract::Ptr decode_lepcc(const utl::Vec3d& origin, const utl::Raw_buffer_view& src)
{
  std::vector< utl::Vec3d > abs_points;
  if (lepcc::decodeXYZ(src, &abs_points) != lepcc::ErrCode::Ok)
    return nullptr;

  // to relative position (in place: )
  auto rel_pos_ptr = reinterpret_cast<utl::Vec3f*>(abs_points.data());
  _to_relative(origin, abs_points.data(), rel_pos_ptr, abs_points.size());

  i3s::Mesh_bulk_data::Var dst = std::make_unique<i3s::Mesh_bulk_data>();
  dst->origin = origin;
  dst->rel_pos.values = utl::Buffer::create_deep_copy(rel_pos_ptr, (int)abs_points.size());
  return i3s::Mesh_abstract::Ptr(i3s::parse_mesh_from_bulk(*dst));
}

#ifndef NO_DRACO_SUPPORT

extern "C"
{
  static  bool draco_create_mesh_attribute_impl(utl::draco_mesh_handle_t hdl, utl::draco_attrib_type_t type, int value_count,
                                                int  value_stride, int index_count, char** val_out, uint32_t** idx_out)
  {
    Mesh_bulk_data& dst = *reinterpret_cast<Mesh_bulk_data*>(hdl);
    switch (type)
    {
      case utl::draco_attrib_type_t::Pos:
      {
        I3S_ASSERT(value_stride == sizeof(utl::Vec3f));
        dst.rel_pos.values = utl::Buffer::create_writable_typed_view<utl::Vec3f>(value_count);
        dst.rel_pos.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.rel_pos.values.data());
        *idx_out = (uint32_t*)dst.rel_pos.index.data();
        break;
      }
      case utl::draco_attrib_type_t::Normal:
        I3S_ASSERT(value_stride == sizeof(utl::Vec3f));
        dst.normals.values = utl::Buffer::create_writable_typed_view<utl::Vec3f>(value_count);
        dst.normals.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.normals.values.data());
        *idx_out = (uint32_t*)dst.normals.index.data();
        break;
      case utl::draco_attrib_type_t::Uv:
        I3S_ASSERT(value_stride == sizeof(utl::Vec2f));
        dst.uvs.values = utl::Buffer::create_writable_typed_view<utl::Vec2f>(value_count);
        dst.uvs.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.uvs.values.data());
        *idx_out = (uint32_t*)dst.uvs.index.data();
        break;
      case utl::draco_attrib_type_t::Color:
        I3S_ASSERT(value_stride == sizeof(Rgba8));
        dst.colors.values = utl::Buffer::create_writable_typed_view<Rgba8>(value_count);
        dst.colors.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.colors.values.data());
        *idx_out = (uint32_t*)dst.colors.index.data();
        break;
      case utl::draco_attrib_type_t::Fid:
        I3S_ASSERT(value_stride == sizeof(uint64_t));
        dst.fids.values = utl::Buffer::create_writable_typed_view<uint64_t>(value_count);
        dst.fids.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.fids.values.data());
        *idx_out = (uint32_t*)dst.fids.index.data();
        break;
      case utl::draco_attrib_type_t::Region:
        I3S_ASSERT(value_stride == sizeof(Uv_region));
        dst.uv_region.values = utl::Buffer::create_writable_typed_view<Uv_region>(value_count);
        dst.uv_region.index = utl::Buffer::create_writable_typed_view<uint32_t>(index_count);
        *val_out = (char*)(dst.uv_region.values.data());
        *idx_out = (uint32_t*)dst.uv_region.index.data();
        break;
      case utl::draco_attrib_type_t::Anchor_point_fid_index:
        I3S_ASSERT(value_stride == sizeof(Anchor_point_fid_index));
        dst.anchor_point_fid_indices.values = utl::Buffer::create_writable_typed_view<Anchor_point_fid_index>(value_count);
        *val_out = (char*)(dst.anchor_point_fid_indices.values.data());
        break;
      case utl::draco_attrib_type_t::Anchor_points:
        dst.rel_anchor_points.values = utl::Buffer::create_writable_typed_view<utl::Vec3f>(value_count);
        *val_out = (char*)(dst.rel_anchor_points.values.data());
        break;
      case utl::draco_attrib_type_t::Fid_index:
        I3S_ASSERT(false); //should not call.
                           //dst.fids = utl::Buffer::create_writable_typed_view<uint32_t>(value_count);
                           //*val_out = (char*)(dst.fids.data());
                           //*idx_out = nullptr;
        break;
      default:
        I3S_ASSERT(false);
        return false;
    }
    return true;
  }
}

Mesh_abstract::Ptr decode_draco(const utl::Vec3d& origin, const utl::Raw_buffer_view& draco)
{
  auto src_data = const_cast<char*>(draco.data());
  Mesh_bulk_data::Var dst = std::make_unique<Mesh_bulk_data>();
  dst->origin = origin;
  Has_fids fids{ Has_fids::No };
  if (utl::draco_decompress_mesh(src_data, draco.size(), (utl::draco_mesh_handle_t)dst.get(), draco_create_mesh_attribute_impl, fids))
  {
    return Mesh_abstract::Ptr(parse_mesh_from_bulk(*dst));
  }
  return nullptr;
}

char* draco_create_buffer_impl(int size)
{
  return new char[size];
}

template< class T >
utl::Buffer_view< const T >  to_unindexed(const Mesh_attrb<T>& src, T scale = T(1))
{
  const bool no_scale = scale == T(1);
  if (src.index.size() == 0 && no_scale)
    return src.values;

  const int n = src.size();
  auto ret = utl::Buffer::create_writable_typed_view<T>(n);

  if( no_scale )
    for (int i = 0; i<n; ++i)
      ret[i] = src[i];
  else
    for (int i = 0; i < n; ++i)
      ret[i] = src[i] * scale;

  utl::Buffer_view< const T > what;
  what.operator=(ret);
  return what;
}

struct Unindexed_mesh
{
  utl::Buffer_view< const utl::Vec3f > rel_pos;
  utl::Buffer_view< const utl::Vec3f > normals;
  utl::Buffer_view< const utl::Vec2f > uvs;
  utl::Buffer_view< const Rgba8 > colors;
  //utl::Buffer_view< const uint32_t > fids;
  utl::Buffer_view< const uint32_t > fid_indices;
  utl::Buffer_view< const Uv_region > uv_regions;

  template< class T, class Y > const Y* as_draco(utl::Buffer_view< const T >& src) { return src.size() ? reinterpret_cast< const Y*>(src.data()) : nullptr; }

  utl::draco_i3s_mesh    as_draco_struct()
  {
    utl::draco_i3s_mesh dm;
    dm.vtx_count = rel_pos.size();
    //dm.fid_count = src.get_feature_ids().
    dm.position = as_draco< utl::Vec3f, float >(rel_pos);
    dm.normal = as_draco< utl::Vec3f, float >(normals);
    dm.uv = as_draco< utl::Vec2f, float >(uvs);
    dm.rgba = as_draco< Rgba8, uint8_t >(colors);
    //dm.fid_index = as_draco< uint32_t, uint32_t >(fid_indices);
    dm.uv_region = as_draco< Uv_region, uint16_t >(uv_regions);
    return dm;
  }
};
// Note: scalex/y will be applied to src.relative_position .
static bool compress_to_draco(const Mesh_abstract& src, utl::Raw_buffer_view* out, Has_fids & fids, double scale_x, double scale_y)
{
  //draco_compressed_buffer dst;
  Unindexed_mesh um;
  um.rel_pos = to_unindexed(src.get_relative_positions(), utl::Vec3f((float)scale_x, (float)scale_y, 1.0f));
  um.normals = to_unindexed(src.get_normals());
  um.uvs = to_unindexed(src.get_uvs(0));
  um.colors = to_unindexed(src.get_colors());
  um.uv_regions = to_unindexed(src.get_regions());

  utl::draco_i3s_mesh  dm = um.as_draco_struct();
  // Add the FID:
  dm.fid_index = src.get_feature_ids().index.data();
  dm.fid = src.get_feature_ids().values.data();
  dm.fid_count = src.get_feature_count();

  dm.pos_scale_x = scale_x;
  dm.pos_scale_y = scale_y;

  char* ptr_out = nullptr;
  int  size = 0;
  if (utl::draco_compress_mesh(&dm, draco_create_buffer_impl, &ptr_out, &size, fids, src.get_topology() == i3slib::i3s::Mesh_topology::Triangles))
  {
    *out = utl::Buffer::move_exising(&ptr_out, size);
    return true;
  }
  return false;
}

#endif // -------------------------- end draco----------------------


#ifndef NO_ETC2_SUPPORT

static bool compress_to_ktx_with_mipmaps(const Texture_buffer& img, Texture_buffer* ktx_buffer/*, int* encoding_time_ms_p*/)
{
  /* if (!m_mod)
  return false;*/
  const unsigned char* src_ptr = (const unsigned char*)img.data.data();

  if (((Image_formats)img.meta.format & (Image_formats)Image_format::Raw_uncompressed) == 0)
  {
    I3S_ASSERT(false); //unsupported input format
    return false;
  }

  //sanity:
  if (img.empty() || img.meta.format == Image_format::Not_set)
    return false; //invalid_arg
  const int alpha_bits = (int)img.meta.alpha_status;
  if (alpha_bits <0 || alpha_bits > 8)
  {
    I3S_ASSERT_EXT(false); // must have been set!
    return false;
  }
  std::vector< float >  m_float_img;
  const int c_component_count = 4; //rgba
  m_float_img.resize(img.width()*img.height() *c_component_count);
  const float c_scale = 1.0f / 255.0f;
  //bool has_alpha = img.meta.format == Image_format::Raw_rgba8;
  I3S_ASSERT_EXT(alpha_bits >= 0 && alpha_bits <= 8); // must have been set!
  etc_img_format etc_format;
  if (img.meta.format == Image_format::Raw_rgba8)
  {
    etc_format = alpha_bits == 0 ? etc_img_format::RGB8 : (alpha_bits == 1 ? etc_img_format::RGB8A1 : etc_img_format::RGBA8);
    for (int i = 0; i < m_float_img.size(); ++i)
      m_float_img[i] = (float)src_ptr[i] * c_scale;
  }
  else
  {
    etc_format = RGB8;
    //RGB -> RGBA
    const int pix_count = img.height() * img.width();
    for (int i = 0; i < pix_count; ++i)
    {
      m_float_img[4 * i] = (float)src_ptr[3 * i] * c_scale;
      m_float_img[4 * i + 1] = (float)src_ptr[3 * i + 1] * c_scale;
      m_float_img[4 * i + 2] = (float)src_ptr[3 * i + 2] * c_scale;
      m_float_img[4 * i + 3] = 1.0f;// float((i % img.w)) / (float)(img.w - 1);
    }
  }
  const auto  c_defaut_error_metric = etc_error_metric::BT709;

  const float c_default_effort = 40.0f;
  const int c_max_thread = 8; // TBD: this is so slow it needs to be more threads...

  int encoding_time_ms;
  auto mip_img_hdl = etc_encode_mipmaps(
    m_float_img.data(),
    img.width(), img.height(),
    etc_format,
    c_defaut_error_metric,
    c_default_effort,
    c_max_thread,
    (int)img.meta.wrap_mode & 0b11, //matches ETC enum
    &encoding_time_ms);

  if (!mip_img_hdl)
    return false;

  //if (encoding_time_ms_p)
  //  *encoding_time_ms_p = encoding_time_ms;

  const int ktx_size = etc_get_ktx_size(mip_img_hdl);
  auto dst = utl::Buffer::create_writable_view(nullptr, ktx_size);
  if (etc_write_ktx(mip_img_hdl, dst.data(), ktx_size))
  {
    ktx_buffer->data = dst;
    ktx_buffer->meta = img.meta;
    ktx_buffer->meta.format = Image_format::Ktx;
    ktx_buffer->meta.mip_count = -1; // Don't know the number of mips, but we may not care...
  }
  else
  {
    ktx_buffer->data = utl::Raw_buffer_view();
  }
  etc_free_mip_image(mip_img_hdl);
  return !ktx_buffer->empty();
}
#endif // --------------------------- end ETC2 ----------------------


#ifndef NO_BASIS_ENCODER_SUPPORT

static bool compress_to_basis_ktx2_with_mipmaps(const i3s::Texture_buffer& img, i3s::Texture_buffer* basis_buffer)
{
  if (img.meta.format != i3s::Image_format::Raw_rgba8
    && img.meta.format != i3s::Image_format::Raw_rgb8)
  {
    I3S_ASSERT(false);
    return false;
  }

  std::string basis;
  if (!utl::compress_to_basis_with_mips(img.data.data()
    , img.width(), img.height()
    , img.meta.format == i3s::Image_format::Raw_rgb8 ? 3 : 4
    , basis)
    )
    return false;

  basis_buffer->data = utl::Buffer::create_deep_copy(basis.data(), (int)basis.size());
  basis_buffer->meta = img.meta;
  basis_buffer->meta.mip_count = -1; // Don't know yet
  basis_buffer->meta.format = i3s::Image_format::Ktx2;

  return true;
}

// TODO: eliminate i3s::Image_format::Basis and the function
static bool compress_to_basis_with_mipmaps(const i3s::Texture_buffer& img, i3s::Texture_buffer* basis_buffer)
{
  bool res = compress_to_basis_ktx2_with_mipmaps(img, basis_buffer);
  basis_buffer->meta.format = i3s::Image_format::Basis;
  return res;
}

#endif // NO_BASIS_ENCODER_SUPPORT

#ifndef NO_BASIS_TRANSCODER_SUPPORT
static bool transcode_basis(const Texture_buffer& img, Texture_buffer* dds_buffer, Image_format to_img_fmt)
{
  if (img.meta.format == Image_format::Basis || img.meta.format == Image_format::Ktx2)
  {
    I3S_ASSERT_EXT((int)img.meta.alpha_status != -1); // ALPHA status must have been set at this point.
    std::vector<uint8_t> out_buffer;

    if (to_img_fmt == Image_format::Dds)
    {
      utl::transcode_basis_to_dds(img.data.data(), img.data.size(), &out_buffer, img.meta.alpha_status != Texture_meta::Alpha_status::Opaque);

      dds_buffer->data = utl::Buffer::create_deep_copy((const char*)out_buffer.data(), (int)out_buffer.size());
      dds_buffer->meta = img.meta;
      dds_buffer->meta.mip_count = -1; //dunno. 
      dds_buffer->meta.format = to_img_fmt;

      return true;
    }
    else if (to_img_fmt == Image_format::Ktx)
    {
      utl::transcode_basis_to_ktx(img.data.data(), img.data.size(), &out_buffer, img.meta.alpha_status != Texture_meta::Alpha_status::Opaque);

      dds_buffer->data = utl::Buffer::create_deep_copy((const char*)out_buffer.data(), (int)out_buffer.size());
      dds_buffer->meta = img.meta;
      dds_buffer->meta.mip_count = -1; //dunno. 
      dds_buffer->meta.format = to_img_fmt;

      return true;
    }
  }
  return false;
}

static bool get_basis_info(const utl::Raw_buffer_view& basis, int* mip0_w, int* mip0_h, int* mipmap_count)
{
  return utl::get_basis_image_info(basis.data(), basis.size(), mip0_w, mip0_h, mipmap_count);
}
#endif // NO_BASIS_TRANSCODER_SUPPORT

#ifndef WASM
static bool decode_jpeg(const utl::Raw_buffer_view& jpeg, Texture_buffer* img_out)
{
  bool has_alpha;
  std::string output;
  utl::Buffer_view<char> raw_rgba;
  if (!utl::decompress_jpeg(jpeg.data(), jpeg.size(), &raw_rgba, &img_out->meta.mip0_width, &img_out->meta.mip0_height, &has_alpha))
    return false;

  if (!raw_rgba.is_valid())
  {
    I3S_ASSERT(false);
    return false;
  }

  img_out->meta.format = Image_format::Raw_rgba8;
  img_out->data = raw_rgba;
  img_out->meta.mip_count = 1;
  //TODO: check is it's mask or full transparency !
  img_out->meta.alpha_status = has_alpha ? Texture_meta::Alpha_status::Mask_or_blend : Texture_meta::Alpha_status::Opaque;
  return true;
}

bool  get_jpeg_size(const utl::Raw_buffer_view& jpeg, int* w, int* h, int max_tex_dim = -1)
{
  return utl::get_jpeg_size(jpeg.data(), jpeg.size(), w, h, max_tex_dim);
}

static bool  decode_png(const utl::Raw_buffer_view& jpeg, Texture_buffer* img_out)
{
  bool has_alpha;
  std::string output;
  utl::Buffer_view<char> raw_rgba;
  if (utl::decode_png(jpeg.data(), jpeg.size(), &raw_rgba, &img_out->meta.mip0_width, &img_out->meta.mip0_height, &has_alpha))
  {
    img_out->meta.format = Image_format::Raw_rgba8;
    img_out->data = raw_rgba;
    img_out->meta.mip_count = 1;
    //TODO: check is it's mask or full transparency !
    img_out->meta.alpha_status = has_alpha ? Texture_meta::Alpha_status::Mask_or_blend : Texture_meta::Alpha_status::Opaque;
    return true;
  }
  return false;
}

bool  get_png_size(const utl::Raw_buffer_view& jpeg, int* w, int* h, int max_tex_dim)
{
  // TODO max_tex_dim is ignored for now.
  return utl::get_png_size(jpeg.data(), jpeg.size(), w, h);
}
#endif

Context::Ptr     create_minimal_reader_context(const Ctx_properties& prop)
{
  Context::Ptr ctx = std::make_shared< Context >(prop);

#ifndef WASM
  //decoder:
  ctx->decode_jpeg = decode_jpeg;
  ctx->get_jpeg_size = get_jpeg_size;
  ctx->decode_png = decode_png;
  ctx->get_png_size = get_png_size;
  ctx->m_tracker = prop.tracker;

  ctx->decode_lepcc = decode_lepcc;
#endif

#ifndef NO_DXT_ENCODER_SUPPORT
  if (ctx->m_prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::DXT_BC_ALL)
    ctx->encode_to_dxt_with_mips = compress_to_dds_with_mipmaps;
#endif
  set_gpu_compression(ctx->m_prop.gpu_tex_encoding_support, GPU_texture_compression::DXT_BC_ALL, (bool)ctx->encode_to_dxt_with_mips);

  bool outputing_basis = false; //if for testing purpose outputing basis is still needed set this flag true.
#ifndef NO_BASIS_TRANSCODER_SUPPORT
  if (ctx->m_prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::Basis)
    outputing_basis = true;
  if (ctx->m_prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::KTX2 || outputing_basis)
      ctx->transcode_basis = transcode_basis;

  ctx->get_basis_info = get_basis_info;
  if ((ctx->m_prop.gpu_tex_rendering_support & (GPU_texture_compression_flags)GPU_texture_compression::Basis) ||
    (ctx->m_prop.gpu_tex_rendering_support & (GPU_texture_compression_flags)GPU_texture_compression::KTX2))
  {
    ctx->transcode_basis = transcode_basis;
  }
#endif

  set_gpu_compression(ctx->m_prop.gpu_tex_encoding_support, GPU_texture_compression::Basis, (bool)ctx->transcode_basis && outputing_basis);
  set_gpu_compression(ctx->m_prop.gpu_tex_encoding_support, GPU_texture_compression::KTX2, (bool)ctx->transcode_basis);
  
#ifndef NO_DRACO_SUPPORT
  if (ctx->m_prop.geom_decoding_support & (Geometry_compression_flags)Geometry_compression::Draco)
    ctx->decode_draco = decode_draco;
#endif

  set_geom_compression(ctx->m_prop.geom_decoding_support, Geometry_compression::Draco, (bool)ctx->decode_draco);

  //always support lepcc for now (build-in):
  set_geom_compression(ctx->m_prop.geom_decoding_support, Geometry_compression::Lepcc, true);

  return ctx;
}

static bool raw_to_jpg(const Texture_buffer& img, Texture_buffer* dst)
{
  I3S_ASSERT(img.meta.format == Image_format::Raw_rgb8 || img.meta.format == Image_format::Raw_rgba8);
  I3S_ASSERT(img.meta.mip_count == 1 && img.width() && img.height());
  const int nchan = img.meta.format == Image_format::Raw_rgb8 ? 3 : 4;
  utl::Buffer_view<char> out;
  if (utl::compress_jpeg(img.width(), img.height(), img.data.data(), img.data.size(), &out, nchan))
  {
    dst->data = out;
    dst->meta = img.meta;
    dst->meta.format = Image_format::Jpg;
    return true;
  }
  return false;

}

static bool raw_to_png(const Texture_buffer& img, Texture_buffer* dst)
{
  I3S_ASSERT(img.meta.format == Image_format::Raw_rgb8 || img.meta.format == Image_format::Raw_rgba8);
  I3S_ASSERT(img.meta.mip_count == 1 && img.width() > 0 && img.height() > 0);

  std::vector<uint8_t> png_blob;
  if (!utl::encode_png(
        reinterpret_cast<const uint8_t*>(img.data.data()),
        img.width(),
        img.height(),
        img.meta.format == Image_format::Raw_rgba8,
        png_blob))
    return false;

  dst->data =
    utl::Buffer::create_deep_copy<char>(
      reinterpret_cast<const char*>(png_blob.data()),
      static_cast<int>(png_blob.size()));

  dst->meta = img.meta;
  dst->meta.format = Image_format::Png;
  return true;
}

// -----------------------------------------------------------------------------
//    class     Spatial_reference_helper_default
// -----------------------------------------------------------------------------

Spatial_reference_xform::Status_t Spatial_reference_xform_cartesian_only::transform(Spatial_reference_xform::Sr_type src, Spatial_reference_xform::Sr_type dst, utl::Vec3d* xyz, int count) const
{
  if ((src == Sr_type::Src_cartesian && dst == Sr_type::Src_sr)
      || (src == Sr_type::Dst_cartesian && dst == Sr_type::Dst_sr))
  {
    if (Spatial_reference::is_well_known_gcs(m_src))
      utl::ECEF2geodetic(xyz, count);
    return Status_t::Ok;
  }
  if ((src == Sr_type::Src_sr && dst == Sr_type::Src_cartesian)
      || (src == Sr_type::Dst_sr && dst == Sr_type::Dst_cartesian))
  {
    if (Spatial_reference::is_well_known_gcs(m_src))
      utl::geodetic2ECEF(xyz, count);
    return Status_t::Ok;
  }
  if (src == Sr_type::Src_sr && dst == Sr_type::Dst_sr)
    return Status_t::Ok; // no xform sonce src == dst

  return Status_t::No_implementation;
}

// -----------------------------------------------------------------------------
//    class     BuilderContext
// -----------------------------------------------------------------------------
struct Builder_context_impl : public Writer_context
{
  DECL_PTR(Builder_context_impl);

  explicit Builder_context_impl(const Ctx_properties& prop)
  {
    decoder = create_minimal_reader_context(prop);

    //typedef std::function< bool(const Texture_buffer& img, Texture_buffer* dst)> Encode_img_fct;
  #ifndef WASM
    encode_to_jpeg = raw_to_jpg;
    encode_to_png = raw_to_png;
  #endif
  }

private:
};


Writer_context::Ptr create_i3s_writer_context(
  const Ctx_properties& _prop, Writer_finalization_mode finalization_mode)
{
  auto builder_ctx = std::make_shared<Builder_context_impl>(_prop);
  builder_ctx->finalization_mode = finalization_mode;
  auto& prop = builder_ctx->decoder->m_prop;

#ifndef NO_ETC2_SUPPORT
  if (prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::ETC_2)
    builder_ctx->encode_to_etc2_with_mips = compress_to_ktx_with_mipmaps;
#endif
  set_gpu_compression(prop.gpu_tex_encoding_support, GPU_texture_compression::ETC_2, (bool)builder_ctx->encode_to_etc2_with_mips);

#ifndef NO_BASIS_ENCODER_SUPPORT
  if (prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::Basis)
    builder_ctx->encode_to_basis_with_mips = compress_to_basis_with_mipmaps;
  if (prop.gpu_tex_encoding_support & (GPU_texture_compression_flags)GPU_texture_compression::KTX2)
      builder_ctx->encode_to_basis_ktx2_with_mips = compress_to_basis_ktx2_with_mipmaps;
#endif
  set_gpu_compression(prop.gpu_tex_encoding_support, GPU_texture_compression::Basis, (bool)builder_ctx->encode_to_basis_with_mips);
  set_gpu_compression(prop.gpu_tex_encoding_support, GPU_texture_compression::KTX2, (bool)builder_ctx->encode_to_basis_ktx2_with_mips);

#ifndef NO_DRACO_SUPPORT
  if (prop.geom_encoding_support & (Geometry_compression_flags)Geometry_compression::Draco)
    builder_ctx->encode_to_draco = compress_to_draco;
#endif
  set_geom_compression(prop.gpu_tex_encoding_support, Geometry_compression::Draco, (bool)builder_ctx->encode_to_draco);

  //always on (build)
  set_geom_compression(prop.geom_encoding_support, Geometry_compression::Lepcc, true);

  // default SR helper doesn't project: 
  builder_ctx->sr_helper_factory = [](const i3s::Spatial_reference_desc& layer_sr, const i3s::Spatial_reference_desc* dst_sr)
  { return std::make_shared< Spatial_reference_xform_cartesian_only>(layer_sr); };
  
  builder_ctx->gzip_option = prop.gzip_option;

  return builder_ctx;
}

}//endof ::i3s

} // namespace i3slib
