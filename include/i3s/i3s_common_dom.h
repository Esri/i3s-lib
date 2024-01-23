/*
Copyright 2020-2023 Esri

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
#include "i3s/i3s_enums.h"

// -----------------------------------------------------------------------------------
// WARNING: **Do not Edit** - this file is auto-generated. Manual changes will be lost
// -----------------------------------------------------------------------------------
namespace i3slib::i3s
{
    using rgba8_t = utl::Vec4< uint8_t >;

// Spatial reference declaration
struct Spatial_reference_desc
{
  // --- fields:
  int32_t wkid {-1}; //Well-know ID
  int32_t latest_wkid {-1}; //Latest Well-know ID
  int32_t vcs_wkid {-1}; //vertical coordinate system ID
  int32_t latest_vcs_wkid {-1}; //Latest vertical coordinate system ID
  std::string wkt ; //Well-Known-Text description of the VCS ( take precedence over IDs)
  // --- 
  SERIALIZABLE(Spatial_reference_desc);
  friend bool operator==(const Spatial_reference_desc& a, const Spatial_reference_desc& b) { return a.wkid==b.wkid && a.latest_wkid==b.latest_wkid && a.vcs_wkid==b.vcs_wkid && a.latest_vcs_wkid==b.latest_vcs_wkid && a.wkt==b.wkt; }
  friend bool operator!=(const Spatial_reference_desc& a, const Spatial_reference_desc& b) { return !(a==b); }
  template< class Ar > void serialize(Ar& ar)
  {
  ar & utl::opt("wkid", wkid, int32_t(-1));
  ar & utl::opt("latestWkid", latest_wkid, int32_t(-1));
  ar & utl::opt("vcsWkid", vcs_wkid, int32_t(-1));
  ar & utl::opt("latestVcsWkid", latest_vcs_wkid, int32_t(-1));
  ar & utl::opt("wkt", wkt, std::string()); ;
  }
};
// A 3d extend . used in BSL v1.7+ and v1.8+ layers
struct Full_extent_desc
{
  // --- fields:
  Spatial_reference_desc spatial_reference ; //_deprecated_ use layer-level `Spatial_reference` object instead
  double xmin {0}; //Left
  double xmax {0}; //Right
  double ymin {0}; //bottom
  double ymax {0}; //top
  double zmin {0}; //Min elevation
  double zmax {0}; //Max elevation
  // --- 
  SERIALIZABLE(Full_extent_desc);
  friend bool operator==(const Full_extent_desc& a, const Full_extent_desc& b) { return a.spatial_reference==b.spatial_reference && a.xmin==b.xmin && a.xmax==b.xmax && a.ymin==b.ymin && a.ymax==b.ymax && a.zmin==b.zmin && a.zmax==b.zmax; }
  friend bool operator!=(const Full_extent_desc& a, const Full_extent_desc& b) { return !(a==b); }
  template< class Ar > void serialize(Ar& ar)
  {
  ar & utl::opt("spatialReference", spatial_reference, Spatial_reference_desc());
  ar & utl::nvp("xmin", xmin);
  ar & utl::nvp("xmax", xmax);
  ar & utl::nvp("ymin", ymin);
  ar & utl::nvp("ymax", ymax);
  ar & utl::nvp("zmin", zmin);
  ar & utl::nvp("zmax", zmax); ;
  }
};

struct Compressed_key
{
  explicit Compressed_key() = default;

  explicit Compressed_key(std::string && uncompressed, std::string && compressed)
    : key(std::move(uncompressed))
    , compressed_key(std::move(compressed))
  {}

  std::string key;
  std::string compressed_key;

  SERIALIZABLE(Compressed_key);
  friend bool operator==(const Compressed_key& a, const Compressed_key& b) { return a.key == b.key && a.compressed_key == b.compressed_key; }
  friend bool operator!=(const Compressed_key& a, const Compressed_key& b) { return !(a == b); }
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::nvp("key", key);
    ar& utl::nvp("compressedKey", compressed_key);
  }
};

struct Key_value_encoding
{
  Key_value_encoding_type type = Key_value_encoding_type::Not_set;
  std::vector<Compressed_key> compressed_keys;

  SERIALIZABLE(Key_value_encoding);
  friend bool operator==(const Key_value_encoding& a, const Key_value_encoding& b) { return a.type == b.type && a.compressed_keys == b.compressed_keys; }
  friend bool operator!=(const Key_value_encoding& a, const Key_value_encoding& b) { return !(a == b); }
  template< class Ar > void serialize(Ar& ar)
  {
    ar& utl::opt("type", type, Key_value_encoding_type::Not_set);
    ar& utl::opt("compressedKeys", utl::seq(compressed_keys), std::vector<Compressed_key>{});
  }

};
}// endof i3slib::i3s