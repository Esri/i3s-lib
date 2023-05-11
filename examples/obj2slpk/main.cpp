/*
Copyright 2021 Tisham Dhar

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of
the License at http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

For additional information, contact:
Tisham <whatnick> Dhar
email: tisham@whanick.com
*/

#include "i3s/i3s_writer.h"
#include "utils/utl_png.h"
#include "utils/utl_geom.h"
#include "utils/utl_i3s_resource_defines.h"
#include "fastobj/fast_obj.h"
#include <iostream>
#include <vector>

#include <filesystem>
namespace stdfs = std::filesystem;

using i3slib::utl::Vec2i;
using i3slib::utl::Vec2f;
using i3slib::utl::Vec2d;
using i3slib::utl::Vec3f;
using i3slib::utl::Vec3d;

namespace
{

struct CS_transformation
{
  DECL_PTR(CS_transformation);
  virtual bool transform(Vec3d* points, size_t count) = 0;
};

i3slib::i3s::Layer_writer::Var create_writer(const stdfs::path& slpk_path)
{
  i3slib::i3s::Ctx_properties ctx_props;
  // OBJ's will require Geometry/Texture compression in the writer
  i3slib::i3s::set_geom_compression(ctx_props.geom_encoding_support, i3slib::i3s::Geometry_compression::Draco, true);
  i3slib::i3s::set_gpu_compression(ctx_props.gpu_tex_encoding_support, i3slib::i3s::GPU_texture_compression::ETC_2, true);
  auto writer_context = i3slib::i3s::create_i3s_writer_context(ctx_props);

  i3slib::i3s::Layer_meta meta;
  meta.type = i3slib::i3s::Layer_type::Mesh_IM;
  meta.name = slpk_path.stem().u8string();
  meta.desc = "Generated with obj2slpk";
  meta.sr.wkid = 4326;
  meta.uid = meta.name;
  meta.normal_reference_frame = i3slib::i3s::Normal_reference_frame::Not_set;

  std::unique_ptr<i3slib::i3s::Layer_writer> writer(
    i3slib::i3s::create_mesh_layer_builder(writer_context, slpk_path));

  if (writer)
    writer->set_layer_meta(meta);
  return writer;
}

// Recursive node addition function for various LOD's
bool process(
  i3slib::i3s::Layer_writer& writer,
  CS_transformation* transformation = nullptr)
{
  return false;
}

// OBJ textured mesh to SLPK mesh conversion
bool build_mesh(
  const i3slib::i3s::Layer_writer& writer,
  i3slib::i3s::Mesh_data& mesh,
  fastObjMesh* mesh_lod,
  CS_transformation* transformation = nullptr
)
{
  // TODO: Load vertices from mesh struct
  // TODO: Load MTL + Textures from mesh struct
  // TODO: Load UV from mesh struct
  // TODO: Reproject vertices via ENU transform as necessary
  // TODO: Add vertices to raw_mesh
  // TODO: Add UV's to raw mesh
  // TODO: Assign n-textures from OBJ to raw mesh
  // TODO: Attach raw mesh to writer

  std::vector<Vec3d> verts;
  std::vector<Vec2f> uvs;
  i3slib::i3s::Simple_raw_mesh raw_mesh;

  return false;
}

}

int main(int argc, char* argv[])
{
  if (argc != 6)
  {
    std::cout << "Usage:" << std::endl
      << "obj2slpk <full_res_obj> <lod1_obj> <lod2_obj> <ref_xml> <output_slpk_file>" << std::endl;

    return 1;
  }

  const stdfs::path full_res_obj_path(argv[1]);
  const stdfs::path lod1_obj_path(argv[2]);
  const stdfs::path lod2_obj_path(argv[3]);
  const stdfs::path reference_xml(argv[4]);
  const stdfs::path slpk_file_path(argv[5]);

  // Read the source obj files into structs, this is memory intensive
  fastObjMesh* mesh_full = fast_obj_read(full_res_obj_path.c_str());
  fastObjMesh* mesh_lod_1 = fast_obj_read(lod1_obj_path.c_str());
  fastObjMesh* mesh_lod_2 = fast_obj_read(lod2_obj_path.c_str());

  auto writer = create_writer(slpk_file_path);
  if (!writer)
    return 1;
}