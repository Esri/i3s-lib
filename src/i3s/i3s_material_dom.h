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
#include "utils/utl_serialize_json_dom.h"
#include "utils/utl_geom.h"
#include "i3s/i3s_enums.h"

namespace i3slib
{

namespace i3s
{

struct Material_texture_desc
{
  // --- fields:
  int   tex_def_id = -1;
  int   tex_coord_set = -1;
  float  factor = 1.0f;
  friend bool operator==(const Material_texture_desc& a, const Material_texture_desc& b) {
    return a.tex_def_id == b.tex_def_id && a.tex_coord_set == b.tex_coord_set && a.factor == b.factor;
  }
  SERIALIZABLE(Material_texture_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp("textureSetDefinitionId", tex_def_id);
    ar & utl::opt("texCoord", tex_coord_set, 0);
    ar & utl::opt("factor", factor, 1.0f);
  }
};

struct Pbr_metallic_roughness_desc
{
  // --- fields:
  utl::Vec4f  base_color_factor = utl::Vec4f(1.0f);
  Material_texture_desc base_color_tex;
  float metallic_factor = 0.0f; //1.0f;
  float roughness_factor = 1.0f;
  Material_texture_desc metal_tex;

  friend bool operator==(const Pbr_metallic_roughness_desc& a, const Pbr_metallic_roughness_desc& b) { 
    return a.base_color_factor == b.base_color_factor && a.base_color_tex == b.base_color_tex && a.metallic_factor == b.metallic_factor && a.metal_tex == b.metal_tex;
  }
  SERIALIZABLE(Pbr_metallic_roughness_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("baseColorFactor", utl::seq(base_color_factor), utl::Vec4f(1.0f));
    ar & utl::opt("baseColorTexture", base_color_tex, Material_texture_desc());
    ar & utl::opt("metallicFactor", metallic_factor, 1.0f);
    ar & utl::opt("roughnessFactor", roughness_factor, 1.0f);
    ar & utl::opt("metallicRoughnessTexture", metal_tex, Material_texture_desc());
  }
};

struct Material_desc
{
  // --- fields:
  static constexpr float c_default_alpha_cutoff=0.25;
  Alpha_mode  alpha_mode = Alpha_mode::Opaque;
  float       alpha_cutoff = c_default_alpha_cutoff;
  bool        is_double_sided = false;
  Face_culling_mode cull_face = Face_culling_mode::None;
  Material_texture_desc normal_tex;
  Material_texture_desc occlusion_tex;
  Material_texture_desc emissive_tex;
  utl::Vec3f      emissive_factor = utl::Vec3f(0.0);
  Pbr_metallic_roughness_desc metal;

  friend bool operator==(const Material_desc& a, const Material_desc& b) {
    return a.alpha_mode == b.alpha_mode && a.alpha_cutoff == b.alpha_cutoff && a.is_double_sided == b.is_double_sided && a.cull_face == b.cull_face
      && a.normal_tex == b.normal_tex && a.occlusion_tex == b.occlusion_tex && a.emissive_tex == b.emissive_tex && a.emissive_factor == b.emissive_factor
      && a.metal == b.metal;
  }
  SERIALIZABLE(Material_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("alphaMode", utl::enum_str(alpha_mode), Alpha_mode::Opaque);
    ar & utl::opt("alphaCutoff", alpha_cutoff, c_default_alpha_cutoff);
    ar & utl::opt("doubleSided", is_double_sided, false);
    ar & utl::opt("cullFace", utl::enum_str(cull_face), Face_culling_mode::None);
    ar & utl::opt("normalTexture", normal_tex, Material_texture_desc());
    ar & utl::opt("occlusionTexture", occlusion_tex, Material_texture_desc());
    ar & utl::opt("emissiveTexture", emissive_tex, Material_texture_desc());
    ar & utl::opt("emissiveFactor", utl::seq(emissive_factor), utl::Vec3f(0.0));
    ar & utl::opt("pbrMetallicRoughness", metal, Pbr_metallic_roughness_desc());
  }
};

}

} // namespace i3slib
