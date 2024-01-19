/*
Copyright 2020 - 2023 Esri

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

#include "i3s/i3s_writer.h"
#include "utils/utl_png.h"
#include "utils/utl_geom.h"
#include "utils/utl_i3s_resource_defines.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include <filesystem>
namespace stdfs = std::filesystem;

using i3slib::utl::Vec2i;
using i3slib::utl::Vec2f;
using i3slib::utl::Vec2d;
using i3slib::utl::Vec3f;
using i3slib::utl::Vec3d;

namespace
{

constexpr double c_pi = 3.14159265358979323846; // M_PI;
constexpr double c_wgs84_equatorial_radius = 6378137.0; //semi-major 
constexpr double c_wgs84_polar_radius = 6356752.314245; //semi-minor
constexpr double c_degrees_per_meter = 180.0 / (c_wgs84_polar_radius * c_pi);

struct CS_transformation
{
  DECL_PTR(CS_transformation);
  virtual bool transform(Vec3d* points, size_t count) = 0;
};

class ENU_to_WGS_transformation : public CS_transformation
{
public:

  DECL_PTR(ENU_to_WGS_transformation);

  explicit ENU_to_WGS_transformation(const Vec2d& origin) :
    origin_(origin),
    lon_degrees_per_meter_(180.0 / (c_wgs84_equatorial_radius * c_pi * std::cos(origin.y * c_pi / 180.0)))
  {}

  virtual bool transform(Vec3d* points, size_t count) override
  {
    std::transform(points, points + count, points, [this](const Vec3d& p)
    {
      return Vec3d(origin_.x + p.x * lon_degrees_per_meter_, origin_.y + p.y * c_degrees_per_meter, p.z);
    });

    return true;
  }

private:

  const Vec2d origin_;
  const double lon_degrees_per_meter_;
};

double screen_size_to_area(double pixels)
{
  constexpr double c_pi_over_4 = c_pi * 0.25;
  return pixels * pixels * c_pi_over_4;
}

i3slib::i3s::Layer_writer::Var create_writer(const stdfs::path& slpk_path)
{
  i3slib::i3s::Ctx_properties ctx_props(i3slib::i3s::Max_major_versions({}));
  //i3slib::i3s::set_geom_compression(ctx_props.geom_encoding_support, i3slib::i3s::Geometry_compression::Draco, true);
  //i3slib::i3s::set_gpu_compression(ctx_props.gpu_tex_encoding_support, i3slib::i3s::GPU_texture_compression::ETC_2, true);
  auto writer_context = i3slib::i3s::create_i3s_writer_context(ctx_props);

  i3slib::i3s::Layer_meta meta;
  meta.type = i3slib::i3s::Layer_type::Mesh_IM;
  meta.name = i3slib::utl::to_string(slpk_path.stem().generic_u8string());
  meta.desc = "Generated with raster2slpk";
  meta.sr.wkid = 4326;
  meta.uid = meta.name;
  meta.normal_reference_frame = i3slib::i3s::Normal_reference_frame::Not_set;

  // Temp hack to make the output SLPKs be exactly the same in different runs on the same input.
  // TODO: add command line parameter for the timestamp?
  meta.timestamp = 1;

  std::unique_ptr<i3slib::i3s::Layer_writer> writer(
    i3slib::i3s::create_mesh_layer_builder(writer_context, slpk_path));

  if (writer)
    writer->set_layer_meta(meta);
  return writer;
}

template<typename T>
size_t remove_alpha_channel(T* data, size_t size)
{
  I3S_ASSERT(size % 4 == 0);
  size_t out = 3;
  for (size_t in = 4; in < size; in += 4, out += 3)
  {
    data[out] = data[in];
    data[out + 1] = data[in + 1];
    data[out + 2] = data[in + 2];
  }

  I3S_ASSERT(out * 4 == size * 3);
  return out;
}

bool load_color_data(const stdfs::path& path, int& size, std::vector<char>& data)
{
  data.clear();

  int h;
  if (!i3slib::utl::read_png_from_file(path, &size, &h, &data))
  {
    std::cout << "Failed to load color bitmap from file." << std::endl;
    return false;
  }

  if (size != h)
  {
    std::cout << "Color bitmap must have equal width and height." << std::endl;
    return false;
  }

  if ((size & (size - 1)) != 0)
  {
    std::cout << "Color bitmap size must be a power of 2." << std::endl;
    return false;
  }

  // The current png reader implementation always produces RGBA output for color images.
  // We have to strip alpha channel here.
  I3S_ASSERT(data.size() == size * size * 4);
  data.resize(remove_alpha_channel(data.data(), data.size()));
  I3S_ASSERT(data.size() == size * size * 3);
  return true;
}

bool load_elevation_data(const stdfs::path& path, int size, std::vector<double>& data, double unit)
{
  data.clear();

  int w, h;
  std::vector<char> grayscale_data;
  if (!i3slib::utl::read_png_from_file(path, &w, &h, &grayscale_data))
  {
    std::cout << "Failed to load elevation image from file." << std::endl;
    return false;
  }

  if (w != size + 1 || h != size + 1)
  {
    std::cout << "Invalid elevation image dimensions." << std::endl;
    return false;
  }

  if (grayscale_data.size() != w * w * 2)
  {
    std::cout << "Elevation image is not a 16-bit grayscale." << std::endl;
    return false;
  }

  data.reserve(grayscale_data.size() / 2);
  auto p = reinterpret_cast<const unsigned char*>(grayscale_data.data());
  auto end = p + grayscale_data.size();
  for (; p != end; p += 2)
  {
    // PNG files store 16-bit pixels in network byte order (that is big-endian, most significant bits
    // first). See http://www.libpng.org/pub/png/libpng-manual.txt
    // However, implementation of read_png_from_file() sets png_set_invert_mono() and png_set_swap()
    // modes for libpng, and we have to deal with this here.

    const auto e = 0xffffu - (p[0] + (p[1] << 8));
    data.push_back(e * unit);
  }

  return true;
}

uint8_t avg4(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4)
{
  return static_cast<uint8_t>((c1 + c2 + c3 + c4 + 2) / 4);
}

void build_downsampled_textures(int size, const char* data, int min_size, std::vector<std::vector<char>>& textures)
{
  while(size > min_size)
  {
    I3S_ASSERT((size % 2) == 0);
    const auto s = size / 2;
    const auto stride = size * 3;

    std::vector<char> texture;
    texture.reserve(s * s * 3);

    const uint8_t *p = reinterpret_cast<const uint8_t*>(data);
    for (int y = 0; y < s; y++)
    {
      auto p1 = p + stride;
      for (int x = 0; x < s; x++, p += 6 , p1 += 6)
      {
        // Get average R,G,B values over 2 * 2 block of pixels.
        texture.push_back(avg4(p[0], p[3], p1[0], p1[3]));
        texture.push_back(avg4(p[1], p[4], p1[1], p1[4]));
        texture.push_back(avg4(p[2], p[5], p1[2], p1[5]));
      }
      p = p1;
    }

    textures.emplace_back(std::move(texture));
    data = textures.back().data();
    size = s;
  }
}

void build_downsampled_grids(int size, const double* data, int min_size, std::vector<std::vector<double>>& grids)
{
  while (size > min_size)
  {
    I3S_ASSERT((size % 2) == 0);
    const auto s = size / 2;
    I3S_ASSERT((s % 2) == 0);

    std::vector<double> grid;
    grid.reserve((s + 1) * (s + 1));

    auto p = data;
    auto pn = p + size + 1;

    // Corner vertex of the first row.
    grid.push_back((p[0] + p[1] + pn[0]) / 3.0);
    p += 2;
    pn += 2;

    // Internal vertices of the first row.
    for (int x = 1; x < s; x++, p += 2, pn += 2)
      grid.push_back((p[-1] + p[0] + p[1] + pn[0]) * 0.25);

    // Corner vertex of the first row.
    grid.push_back((p[-1] + p[0] + pn[0]) / 3.0);

    //
    auto pp = p + 1;
    p = pn + 1;
    pn += (size + 2);

    for (int y = 1; y < s; y++)
    {
      // First vertex of the row.
      grid.push_back((pp[0] + p[0] + p[1] + pn[0]) * 0.25);
      pp += 2;
      p += 2;
      pn += 2;

      // Internal vertices of the row.
      for (int x = 1; x < s; x++, pp += 2, p += 2, pn += 2)
        grid.push_back((pp[0] + p[-1] + p[0] + p[1] + pn[0]) * 0.2);

      // Last vertex of the row.
      grid.push_back((pp[0] + p[-1] + p[0] + pn[0]) * 0.25);

      pp = p + 1;
      p = pn + 1;
      pn += (size + 2);
    }

    // Corner vertex of the last row.
    grid.push_back((pp[0] + p[0] + p[1]) / 3.0);
    pp += 2;
    p += 2;

    // Internal vertices of the last row.
    for (int x = 1; x < s; x++, pp += 2, p += 2, pn += 2)
      grid.push_back((pp[0] + p[-1] + p[0] + p[1]) * 0.25);

    // Corner vertex of the last row.
    grid.push_back((pp[0] + p[-1] + p[0]) / 3.0);

    grids.emplace_back(std::move(grid));
    data = grids.back().data();
    size = s;
  }
}

Vec2f clamp_uv(const Vec2f& uv)
{
  return
  {
    std::clamp(uv.x, 0.01f, 0.99f),
    std::clamp(uv.y, 0.01f, 0.99f)
  };
}

void add_quad(
  std::vector<Vec3d>& verts,
  std::vector<Vec2f>& uvs,
  const Vec3d& v0, 
  const Vec2f& uv0,
  const Vec3d& v1,
  const Vec2f& uv1,
  const Vec3d& v2,
  const Vec2f& uv2,
  const Vec3d& v3,
  const Vec2f& uv3)
{
  verts.push_back(v0);
  verts.push_back(v1);
  verts.push_back(v2);
  verts.push_back(v2);
  verts.push_back(v3);
  verts.push_back(v0);

  uvs.push_back(clamp_uv(uv0));
  uvs.push_back(clamp_uv(uv1));
  uvs.push_back(clamp_uv(uv2));
  uvs.push_back(clamp_uv(uv2));
  uvs.push_back(clamp_uv(uv3));
  uvs.push_back(clamp_uv(uv0));
}

i3slib::utl::Rgb8 avg2(const i3slib::utl::Rgb8& rgb1, const i3slib::utl::Rgb8& rgb2)
{
  return i3slib::utl::Rgb8(
    static_cast<unsigned char>((rgb1.x + rgb2.x) / 2),
    static_cast<unsigned char>((rgb1.y + rgb2.y) / 2),
    static_cast<unsigned char>((rgb1.z + rgb2.z) / 2));
};

i3slib::utl::Rgb8 avg4(
  const i3slib::utl::Rgb8& rgb1,
  const i3slib::utl::Rgb8& rgb2,
  const i3slib::utl::Rgb8& rgb3,
  const i3slib::utl::Rgb8& rgb4)
{
  return i3slib::utl::Rgb8(
    avg4(rgb1.x, rgb2.x, rgb3.x, rgb4.x),
    avg4(rgb1.y, rgb2.y, rgb3.y, rgb4.y),
    avg4(rgb1.z, rgb2.z, rgb3.z, rgb4.z));
};

// Extracts a square fragment of given size at a given offset from a square texture.
// Inputs: src_data is raw RGB, src_size, start and size are in pixels.
// The output texture is PNG-compressed.
bool extract_texture_fragment(
  const char* src_data,
  int src_size,
  const Vec2i& start,
  int size,
  i3slib::i3s::Texture_buffer& texture_buffer)
{
  std::vector<char> texture(static_cast<size_t>(size) * size * 3);

  const auto* src =
    reinterpret_cast<const i3slib::utl::Rgb8*>(src_data) +
    (start.y * src_size + start.x);

  auto* dst = reinterpret_cast<i3slib::utl::Rgb8*>(texture.data());
  const auto end = start + Vec2i(size, size);
  size_t y = 0;

  if (start.y > 0)
  {
    const auto prev_row = src - src_size;
    for (size_t x = 0; x < size; x++)
      dst[x] = avg2(src[x], prev_row[x]);

    if (start.x > 0)
      dst[0] = avg4(src[0], src[-1], prev_row[0], prev_row[-1]);

    if (end.x < src_size)
      dst[size - 1] = avg4(src[size - 1], src[size], prev_row[size - 1], prev_row[size]);

    y = 1;
    src += src_size;
    dst += size;
  }

  const bool fix_last_row = end.y < src_size;
  const size_t y_end = fix_last_row ? size - 1 : size;

  for (; y < y_end; y++, src += src_size, dst += size)
  {
    std::copy(src, src + size, dst);

    if (start.x > 0)
      dst[0] = avg2(src[0], src[-1]);

    if (end.x < src_size)
      dst[size - 1] = avg2(src[size - 1], src[size]);
  }

  if (fix_last_row)
  {
    const auto next_row = src + src_size;
    for (size_t x = 0; x < size; x++)
      dst[x] = avg2(src[x], next_row[x]);

    if (start.x > 0)
      dst[0] = avg4(src[0], src[-1], next_row[0], next_row[-1]);

    if (end.x < src_size)
      dst[size - 1] = avg4(src[size - 1], src[size], next_row[size - 1], next_row[size]);
  }

  // Encode to PNG.
  std::vector<uint8_t> png_bytes;
  if (!i3slib::utl::encode_png(
        reinterpret_cast<uint8_t*>(texture.data()), size, size, false, png_bytes))
    return false;

  texture_buffer.meta.alpha_status = i3slib::i3s::Texture_meta::Alpha_status::Opaque;
  texture_buffer.meta.wrap_mode = i3slib::i3s::Texture_meta::Wrap_mode::None;
  texture_buffer.meta.is_atlas = false;
  texture_buffer.meta.mip0_width = size;
  texture_buffer.meta.mip0_height = size;
  texture_buffer.meta.mip_count = 1; //no mip
  texture_buffer.meta.format = i3slib::i3s::Image_format::Png;

  texture_buffer.data = i3slib::utl::Buffer::create_deep_copy<char>(
    reinterpret_cast<const char*>(png_bytes.data()),
    static_cast<int>(png_bytes.size()));

  return true;
}

// Extracts the fragment of the elevation grid starting at _start_ and spanning _size_ cells 
// (i.e, size + 1 nodes) and extracts the fragment of color_data starting at texture_start
// with texture_size * texture_size pixels.

bool build_mesh(
  const i3slib::i3s::Layer_writer& writer,
  const Vec2d& cell_size,
  int cell_factor,
  const double* grid_data,
  int grid_size,
  const char* color_data,
  int color_size,
  const Vec2i& start,
  int size,
  const Vec2i& texture_start,
  int texture_size,
  i3slib::i3s::Mesh_data& mesh,
  CS_transformation* transformation = nullptr)
{
  I3S_ASSERT(start.x + size <= grid_size);
  I3S_ASSERT(start.y + size <= grid_size);
  I3S_ASSERT(texture_start.x + texture_size <= color_size);
  I3S_ASSERT(texture_start.y + texture_size <= color_size);

  const auto end = start + Vec2i(size, size);
  const double h = cell_factor * cell_size.x;

  const auto get_elevation = [&grid_data, grid_size](int x, int y)
  {
    return grid_data[y * (grid_size + 1) + x];
  };

  std::vector<Vec3d> verts;
  std::vector<Vec2f> uvs;
  verts.reserve(static_cast<size_t>(size + 3) * (size + 3) * 6);
  uvs.reserve(verts.capacity());

  if (start.y != 0)
  {
    // Add a row of skirt quads.
    Vec2f uv(0, 0);
    Vec2f uv_next(0, 0);
    for (int dx = 0; dx < size; dx++, uv.x = uv_next.x)
    {
      auto ind_x = start.x + dx;
      const Vec3d vtx0((ind_x * cell_factor) * cell_size.x, -(start.y * cell_factor) * cell_size.y, get_elevation(ind_x, start.y));
      const Vec3d vtx1(((ind_x + 1) * cell_factor) * cell_size.x, vtx0.y, get_elevation(ind_x + 1, start.y));
      const Vec3d vtx2(vtx1.x, vtx1.y, vtx1.z - h);
      const Vec3d vtx3(vtx0.x, vtx0.y, vtx0.z - h);
      uv_next.x = static_cast<float>(dx + 1) / size;
      add_quad(verts, uvs, vtx0, uv, vtx1, uv_next, vtx2, uv_next, vtx3, uv);
    }
  }

  for (int dy = 0; dy < size; dy++)
  {
    const auto ind_y = start.y + dy;
    const auto ind_y1 = ind_y + 1;
    const auto y = -(ind_y * cell_factor) * cell_size.y;
    const auto y1 = -(ind_y1 * cell_factor) * cell_size.y;
    const auto v = static_cast<float>(dy) / size;
    const auto v1 = static_cast<float>(dy + 1) / size;

    if (start.x != 0)
    {
      // Add skirt quad at the beginning of the row.
      const Vec3d vtx0((start.x * cell_factor) * cell_size.x, y1, get_elevation(start.x, ind_y1));
      const Vec3d vtx1(vtx0.x, y, get_elevation(start.x, ind_y));
      const Vec3d vtx2(vtx1.x, vtx1.y, vtx1.z - h);
      const Vec3d vtx3(vtx0.x, vtx0.y, vtx0.z - h);
      add_quad(verts, uvs, vtx0, {0, v1}, vtx1, {0, v}, vtx2, {0, v}, vtx3, {0, v1});
    }

    for (int dx = 0; dx < size; dx++)
    {
      const auto ind_x = start.x + dx;
      const auto ind_x1 = ind_x + 1;
      const Vec3d v00((ind_x * cell_factor) * cell_size.x, y, get_elevation(ind_x, ind_y));
      const Vec3d v11((ind_x1 * cell_factor) * cell_size.x, y1, get_elevation(ind_x1, ind_y1));
      const Vec3d v10(v11.x, v00.y, get_elevation(ind_x1, ind_y));
      const Vec3d v01(v00.x, v11.y, get_elevation(ind_x, ind_y1));
      const Vec2f uv00(static_cast<float>(dx) / size, v);
      const Vec2f uv11(static_cast<float>(dx + 1) / size, v1);
      const Vec2f uv01(uv00.x, v1);
      const Vec2f uv10(uv11.x, v);
      add_quad(verts, uvs, v10, uv10, v00, uv00, v01, uv01, v11, uv11);
    }

    if (end.x != grid_size)
    {
      // Add skirt quad at the end of the row.
      const Vec3d vtx0((end.x * cell_factor) * cell_size.x, y, get_elevation(end.x, ind_y));
      const Vec3d vtx1(vtx0.x, y1, get_elevation(end.x, ind_y1));
      const Vec3d vtx2(vtx0.x, y1, vtx1.z - h);
      const Vec3d vtx3(vtx0.x, y, vtx0.z - h);

      const Vec2f uv0(1.0f, v);
      const Vec2f uv1(1.0f, v1);

      // The 0.9999 hack is due to a probable bug in Pro SLPK renderer.
      // If a triangle has all its u texture coordinates equal to 1.0, it is textured incorrectly.
      // Web Scene Viewer does not have this issue.
      add_quad(verts, uvs, vtx0, uv0, vtx1, uv1, vtx2, /*uv1*/ { 0.9999f, v1 }, vtx3, /*uv0*/{ 0.9999f, v });
    }
  }

  if (end.y != grid_size)
  {
    // Add a row of skirt quads.
    Vec2f uv(0, 1.0f);
    Vec2f uv_next(0, 1.0f);
    for (int dx = 0; dx < size; dx++, uv.x = uv_next.x)
    {
      auto ind_x = start.x + dx;
      const Vec3d vtx0(((ind_x + 1) * cell_factor) * cell_size.x, -(end.y * cell_factor) * cell_size.y, get_elevation(ind_x + 1, end.y));
      const Vec3d vtx1((ind_x * cell_factor) * cell_size.x, vtx0.y, get_elevation(ind_x, end.y));
      const Vec3d vtx2(vtx1.x, vtx1.y, vtx1.z - h);
      const Vec3d vtx3(vtx0.x, vtx0.y, vtx0.z - h);
      uv_next.x = static_cast<float>(dx + 1) / size;

      // The 0.9999 hack is due to a probable bug in Pro SLPK renderer.
      // If a triangle has all its v texture coordinates equal to 1.0, it is textured incorrectly.
      // Web Scene Viewer does not have this issue.
      add_quad(verts, uvs, vtx0, uv_next, vtx1, uv, vtx2, /*uv*/ { uv.x, 0.9999f }, vtx3, /*uv_next*/{ uv_next.x, 0.9999f });
    }
  }

  if (transformation)
    transformation->transform(verts.data(), verts.size());
  
  i3slib::i3s::Simple_raw_mesh raw_mesh;
  raw_mesh.vertex_count = static_cast<int>(verts.size());
  raw_mesh.abs_xyz = verts.data();
  raw_mesh.uv = uvs.data();

  if (!extract_texture_fragment(
        color_data, color_size, texture_start, texture_size, raw_mesh.img))
    return false;

  return writer.create_mesh_from_raw(raw_mesh, mesh) == IDS_I3S_OK;
}

bool process(
  i3slib::i3s::Layer_writer& writer,
  const int input_size,
  const Vec2d& cell_size,
  const std::vector<std::vector<double>>& grids,
  const std::vector<std::vector<char>>& textures,
  int node_tris_size,
  int node_texture_size,
  int depth,
  int grid_size,
  const Vec2i& start,
  int texture_size,
  const Vec2i& texture_start,
  i3slib::i3s::Node_id& node_id,
  CS_transformation* transformation = nullptr)
{
  std::vector<i3slib::i3s::Node_id> node_ids;

  if (depth + 1 < textures.size())
  {
    for (int i : { 0, 1 })
    {
      for (int j : { 0, 1 })
      {
        const auto status = process(
          writer, input_size, cell_size, grids, textures, node_tris_size, node_texture_size, depth + 1,
          grid_size * 2, 2 * start + node_tris_size * Vec2i(j, i),
          texture_size * 2, 2 * texture_start + node_texture_size * Vec2i(j, i),
          node_id, transformation);

        if (!status)
          return false;

        node_ids.push_back(node_id++);
      }
    }
  }
  else if (depth + 1 < grids.size())
  {
    for (int i : { 0, 1 })
    {
      for (int j : { 0, 1 })
      {
        const auto status = process(
          writer, input_size, cell_size, grids, textures, node_tris_size, node_texture_size / 2, depth + 1,
          grid_size * 2, 2 * start + node_tris_size * Vec2i(j, i),
          texture_size, texture_start + node_texture_size / 2 * Vec2i(j, i),
          node_id, transformation);

        if (!status)
          return false;

        node_ids.push_back(node_id++);
      }
    }
  }

  //
  i3slib::i3s::Simple_node_data node_data;
  node_data.node_depth = depth + 1;
  node_data.children = std::move(node_ids);
  node_data.lod_threshold = screen_size_to_area(500);

  const std::vector<char>& texture = depth < textures.size() ? textures[depth] : textures.back();

  const auto status = build_mesh(
    writer, cell_size, input_size / grid_size,
    grids[depth].data(), grid_size, texture.data(), texture_size,
    start, node_tris_size, texture_start, node_texture_size, node_data.mesh, transformation);

  if (!status)
    return false;

  return writer.create_node(node_data, node_id) == IDS_I3S_OK;
}

}

int main(int argc, char* argv[])
{
  /*if (argc != 7)
  {
    std::cout << "Usage:" << std::endl
      << "raster2slpk <elevation_png> <color_png> <output_slpk_file> <x_step> <y_step> <z_unit>" << std::endl;

    return 1;
  }*/
 
  const stdfs::path elevation_file_path("C:\\esri\\sdk\\data\\ps-e.lg+1.png");// argv[1]);
  const stdfs::path color_file_path("C:\\esri\\sdk\\data\\ps-t.lg.png");//argv[2]);
  const stdfs::path slpk_file_path("C:\\esri\\sdk\\data\\slpk\\sample.slpk"); // argv[3]);

  const Vec2d cell_size(10, 10);//(std::stod(argv[4]), std::stod(argv[5]));
  const double elevation_unit = 0.1;//std::stod(argv[6]);

  //
  int size;
  std::vector<char> color_data;
  if (!load_color_data(color_file_path, size, color_data))
    return 1;

  std::vector<double> elevation_data;
  if (!load_elevation_data(elevation_file_path, size, elevation_data, elevation_unit))
    return 1;

  std::vector<std::vector<char>> textures;
  textures.emplace_back(std::move(color_data));
  build_downsampled_textures(size, textures.front().data(), 128, textures);
  std::reverse(std::begin(textures), std::end(textures));

  std::vector<std::vector<double>> grids;
  grids.emplace_back(std::move(elevation_data));
  build_downsampled_grids(size, grids.front().data(), 32, grids);
  std::reverse(std::begin(grids), std::end(grids));

  //
  auto writer = create_writer(slpk_file_path);
  if (!writer)
    return 1;

  ENU_to_WGS_transformation transformation({ -123.4583943, 47.6204856 });
  i3slib::i3s::Node_id node_id = 0;

  if(!process(*writer, size, cell_size, grids, textures, 32, 128, 0, 32, { 0, 0 }, 128, { 0, 0 }, node_id, &transformation))
    return 1;

  // Add a root node on top of everything.
  i3slib::i3s::Simple_node_data node_data;
  node_data.node_depth = 0;
  node_data.children.push_back(node_id++);
  if (writer->create_node(node_data, node_id) != IDS_I3S_OK)
    return 1;

  if (writer->save() != IDS_I3S_OK)
    return 1;

  return 0;
}
