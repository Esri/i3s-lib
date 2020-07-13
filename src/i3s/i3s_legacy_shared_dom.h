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
#include "utils/utl_serialize.h"
#include "utils/utl_geom.h"


namespace i3slib
{

namespace i3s
{

struct Legacy_material_param_desc
{
  // ---field: 
  bool vertex_region = false; // for WSV: this *MUST* match the geometry buffer ( if true -> region must be in geometry buffer)
  bool vertex_colors = false; 
  bool use_vertex_color_alpha = false;
  float transparency = 0.0f;
  float reflectivity = 0.0f;
  float shininess = 0.0f;
  utl::Vec3f ambient = { .0f,.0f,.0f };
  utl::Vec3f diffuse = { .0f,.0f,.0f };
  utl::Vec3f specular = { .0f,.0f,.0f };
  bool cast_shadows = true;
  bool receive_shadows = true;
  std::string  render_mode = "solid"; //ignored by WSV
  Face_culling_mode cull_face = Face_culling_mode::None; 

  SERIALIZABLE(Legacy_material_param_desc);
  friend bool  operator==(const Legacy_material_param_desc& a, const Legacy_material_param_desc& b)
  {
    return memcmp(&a, &b, offsetof(Legacy_material_param_desc, render_mode)) == 0 && a.render_mode == b.render_mode && a.cull_face == b.cull_face;
  }
  template< class Ar > void serialize(Ar& ar)
  {

    auto c_yes = utl::Serialize_field_mode::Optional_always_write;
    ar & utl::opt("vertexRegions", vertex_region, false, c_yes);
    ar & utl::opt("vertexColors", vertex_colors, false, c_yes);
    ar & utl::opt("useVertexColorAlpha", use_vertex_color_alpha, false, c_yes);
    ar & utl::opt("transparency", transparency, 0.0f); // TBD: check that opaque = 0 
    ar & utl::opt("reflectivity", reflectivity, 0.0f, c_yes);
    ar & utl::opt("shininess", shininess, 0.0f, c_yes);
    // some (invalid) data contains a 4th component. Not a critical failure though, so we suppress it (and print a warning later):
    ar.push_suppressed_error_mask(utl::Json_exception::Error::Fixed_array_out_of_bound );
    ar & utl::opt("ambient", utl::seq(ambient), utl::Vec3f(0.0f), c_yes);
    ar & utl::opt("diffuse", utl::seq(diffuse), utl::Vec3f(0.0f), c_yes);
    ar & utl::opt("specular", utl::seq(specular), utl::Vec3f(0.0f), c_yes);
    ar.pop_suppressed_error_mask();
    ar & utl::opt("renderMode", render_mode, std::string("solid"), c_yes);
    ar & utl::opt("castShadows", cast_shadows, true);
    ar & utl::opt("receiveShadows", receive_shadows, true);
    ar & utl::opt("cullFace", utl::enum_str(cull_face), Face_culling_mode::None, c_yes);
  }
};

struct Legacy_material_desc
{
  // ---field: 
  std::string type = "standard"; //ignored, but specs call for some "type"
  std::string name = "standard";
  Legacy_material_param_desc param;

  SERIALIZABLE(Legacy_material_desc);
  friend bool  operator==(const Legacy_material_desc& a, const Legacy_material_desc& b) { 
    return a.type == b.type && a.name == b.name && a.param == b.param;  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("type", type, std::string());
    ar & utl::opt("name", name, std::string());
    ar & utl::opt("params", param, Legacy_material_param_desc());
  }
};

struct Legacy_texture_image_desc
{
  // ---field:  
  std::string id= "0"; // not used for anything, apparently. 

  int size = 0;        // seems to be irrelevant. 
  int pixel_in_world_units=0;  //ignored by WSV 
  std::vector<std::string> href;
  std::vector<int> byte_offset;
  std::vector<int> length;
  SERIALIZABLE(Legacy_texture_image_desc);
  friend bool  operator==(const Legacy_texture_image_desc& a, const Legacy_texture_image_desc& b)
  {
    return a.id == b.id && a.size == b.size && a.pixel_in_world_units == b.pixel_in_world_units && a.href == b.href && a.byte_offset == b.byte_offset && a.length == b.length;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    //auto c_yes = utl::Optional_always_write;
    ar & utl::nvp("id", id);
    ar & utl::nvp("size", size);
    ar & utl::opt("pixelInWorldUnits", pixel_in_world_units, 0);

    Version version_16(1, 6);
    if (!ar.version() || ar.version() >= (int)version_16.get_code())
    {
      ar & utl::nvp("href", utl::seq(href));
      ar & utl::opt("byteOffset", utl::seq(byte_offset));
      ar & utl::opt("length", utl::seq(length));
    }
    else
    {
      // some v1.4/1.5 SLPKs use a scalar instead of array. Test for array first
      ar.push_suppressed_error_mask(utl::Json_exception::Error::Array_expected);
      ar & utl::nvp("href", utl::seq(href));
      ar & utl::opt("byteOffset", utl::seq(byte_offset));
      ar & utl::opt("length", utl::seq(length));
      if (ar.pop_suppressed_error_mask())
      {
        std::vector<utl::Json_exception> errors;
        ar.pop_suppressed_log(&errors);
        //on reading v1.5 from Nearmap Reality Model: uses scalar instead of array...
        std::string ss;
        ar & utl::nvp("href", ss);
        href = { ss };
        int scalar;
        ar & utl::opt("byteOffset", scalar, 0);
        byte_offset = { scalar };
        scalar = 0;
        ar & utl::opt("length", scalar, 0);
        length = { scalar };
      }
    }
  }
};

struct Legacy_texture_desc
{
  // ---field:  
  std::vector< Mime_image_format> encoding;
  std::vector< Legacy_wrap_mode > wrap;
  Legacy_uv_set uv_set = Legacy_uv_set::Uv0;
  Legacy_image_channel channels= Legacy_image_channel::Not_set;//supposed to be an array...
  bool atlas = false;
  std::vector< Legacy_texture_image_desc > images;
  // ----
  bool is_atlas() const { return atlas && images.size(); }
  SERIALIZABLE(Legacy_texture_desc);
  friend bool  operator==(const Legacy_texture_desc& a, const Legacy_texture_desc& b)
  {
    return a.encoding == b.encoding && a.wrap == b.wrap && a.uv_set == b.uv_set && a.channels == b.channels && a.atlas == b.atlas;
  }
  template< class Ar > void serialize(Ar& ar)
  {
    auto c_yes = utl::Serialize_field_mode::Optional_always_write;

    Version version_16(1, 6);
    if (!ar.version() || ar.version() >= (int)version_16.get_code() ) // 1.5 => 1<<8 | 5
      ar & utl::opt("encoding", utl::seq(encoding), std::vector< Mime_image_format >(), c_yes);
    else
    {
      // some v1.4/1.5 SLPKs use a scalar instead of array. Test for array first
      ar.push_suppressed_error_mask(
        (utl::Json_exception::Error_flags_t)utl::Json_exception::Error::Array_expected
      );
      ar & utl::opt("encoding", utl::seq(encoding), std::vector< Mime_image_format >(), c_yes);
      if (ar.pop_suppressed_error_mask())
      {
        std::vector<utl::Json_exception> errors;
        ar.pop_suppressed_log(&errors);
        Mime_image_format scalar;
        ar & utl::opt("encoding", scalar, Mime_image_format::Not_set);
        encoding.resize(1);
        encoding[0] = scalar;
      }
      //if (what & (utl::Json_exception::Error_flags_t)utl::Json_exception::Error::Unknown_enum)
      //{
      //  encoding = i3s::Mime_image_format::Jpg;
      //}
    }
    ar & utl::opt("wrap", utl::seq(wrap), std::vector< Legacy_wrap_mode>(), c_yes);
    ar & utl::opt("atlas", atlas, false, c_yes);
    ar & utl::opt("uvSet", utl::enum_str( uv_set ), Legacy_uv_set::Uv0, c_yes);
    ar & utl::opt("channels", utl::enum_str(channels), Legacy_image_channel::Not_set, c_yes);
    ar & utl::nvp("images", utl::seq( images)); // WSV doesn't seem to work if there are not there...
  }

};
template<class T>
struct Unnamed_placeholder
{
  // ---field: 
  T unnamed;
  SERIALIZABLE(Unnamed_placeholder);
  friend bool  operator==(const Unnamed_placeholder<T>& a, const Unnamed_placeholder<T>& b) { return a.unnamed == b.unnamed; }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::nvp(name_hint.empty() ? nullptr : name_hint.c_str(), unnamed);
  }
  std::string name_hint;
};


struct Legacy_shared_desc
{
  void set_material_id(int id ) { material.name_hint = "mat_" + std::to_string(id); texture.name_hint = std::to_string(id);  }
  // ---field: 
  Unnamed_placeholder<Legacy_material_desc> material; //! WARNING: WSV expects this to be a unique name. See set_material_id()
  Unnamed_placeholder<Legacy_texture_desc> texture;
  SERIALIZABLE(Legacy_shared_desc);
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("materialDefinitions", material, Unnamed_placeholder<Legacy_material_desc>());
    ar & utl::opt("textureDefinitions", texture, Unnamed_placeholder<Legacy_texture_desc>());
  }
};

}

} // namespace i3slib
