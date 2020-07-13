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
#include "i3s/i3s_common.h"
#include "i3s/i3s_enums.h"
#include "utils/utl_buffer.h"
#include "utils/utl_geom.h"
#include "utils/utl_variant.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace i3slib
{

namespace utl { class Basic_tracker; }

namespace i3s
{

template< class T, class Y >
struct Indexed_attrb
{
  typedef std::remove_const_t< T > Mutable_T;
  typedef std::remove_const_t< Y > Mutable_Y;
  Indexed_attrb() = default;
  Indexed_attrb(utl::Buffer_view<T>&& a, utl::Buffer_view< Y  >&& b) : values(std::move(a)), index(std::move(b)) {}
  Indexed_attrb<T, Y>& operator=(const Indexed_attrb<Mutable_T, Mutable_Y>& src)
  {
    if (this != &src)
    {
      this->values = src.values;
      this->index = src.index;
    }
    return *this;
  }

  //Indexed_attrb<Mutable_T, Mutable_Y> deep_copy() const;

  int size() const noexcept { return index.size() ? index.size() : values.size(); }

  const T&  operator[](int i) const noexcept { i = index.size() ? index[i] : i; return values[i]; }

  Y         get_mapped_index(int i)const noexcept { return index.size() ? index[i] : i; }

  void     deep_copy() { values.deep_copy(); index.deep_copy(); }

  utl::Buffer_view< T >    values;
  utl::Buffer_view< Y  >   index; //if any
};

template<class T>
using Mesh_attrb = Indexed_attrb< const T, const uint32_t >;

//! NOTE: Mesh may also contains point features ( xyz + fid) in which case xyz are available as "absolute" positions
//! please note that in this particular case, absolute position (float64) may be **more accurate** than relative position (float32) + origin (float64).
//! notably when the point data is poorly partitionned in very large tiles that exceed float accuracy ( e.g. single tile covering the Earth).
class Mesh_abstract
{
public:
  DECL_PTR(Mesh_abstract);
  virtual ~Mesh_abstract() = default;

  virtual bool          update_positions( const utl::Vec3d& origin, const utl::Vec3f* rel_pos, int count )=0;
  virtual void          drop_normals() = 0;
  virtual void          drop_colors() = 0;
  virtual void          drop_regions() = 0;
  virtual int           sanitize_uvs(float max_val = 16365.0f)=0;
  virtual utl::Vec3d    get_origin() const = 0;
  virtual Attrib_flags  get_available_attrib_mask() const = 0;
  virtual int           get_vertex_count() const = 0;
  virtual int           get_face_count() const = 0;
  virtual int           get_region_count() const = 0;
  virtual int           get_feature_count() const = 0;
  virtual Texture_meta::Wrap_mode          get_wrap_mode(int uv_set) const = 0;
  virtual const Mesh_attrb<utl::Vec3f>&  get_relative_positions() const = 0;
  virtual const Mesh_attrb<utl::Vec3f>&  get_normals() const = 0;
  virtual bool                           create_normals(const i3s::Mesh_attrb< utl::Vec3f>& rel_positions) =0;
  virtual const Mesh_attrb<utl::Vec2f>&  get_uvs(int uvset) const = 0;
  virtual const Mesh_attrb<Rgba8>&  get_colors() const = 0;
  virtual const Mesh_attrb<Uv_region>&  get_regions() const = 0;
  virtual const Mesh_attrb<uint32_t>&        get_feature_ids() const = 0;
  virtual const utl::Buffer_view<utl::Vec3d>&     get_absolute_positions() const =0;
  virtual i3s::Mesh_topology    get_topology() const = 0;
  virtual void                  set_topology(i3s::Mesh_topology t) = 0;

};

struct Mesh_bulk_data
{
  DECL_PTR(Mesh_bulk_data);
  utl::Vec3d                       origin = utl::Vec3d(0.0);
  Mesh_attrb<utl::Vec3f>      rel_pos;
  Mesh_attrb<utl::Vec3f>      normals;
  Mesh_attrb<utl::Vec2f>      uvs;
  Mesh_attrb<Rgba8>      colors;
  Mesh_attrb<Uv_region>  uv_region;
  Mesh_attrb<uint32_t>            fids;
  //utl::Buffer_view<const uint32_t>     fids;
};

//I3S_EXPORT Mesh_abstract*   parse_mesh_from_bulk(Mesh_bulk_data& data);


enum class Modification_status
{
  Discarded, // all triangles were discarded
  Unchanged, // no triangles were changed or discarded
  Modified, // some triangles were clipped or discarded
  Failed // a fatal internal error occurred, e.g. Modification_controller was already deleted 
         // or initial modification polygon editing was not finished yet (end_polygon_editing was not called)
};

struct Modification_revision_stamp
{
  using Stamp = std::array<std::uint64_t, 2>;
  Stamp stamp;
  Modification_revision_stamp() = default;
  template <typename T>
  explicit Modification_revision_stamp(const T& data) 
    : stamp(reinterpret_cast<const Stamp&>(data))
  {
    static_assert(sizeof(T) == sizeof(Stamp));
  }
  bool operator == (const Modification_revision_stamp& rhs)const
  {
    return stamp == rhs.stamp;
  }
};

struct Modification_state
{
  Modification_revision_stamp revision;
  Modification_status status{ Modification_status::Unchanged };
};

enum class Estimated_modification_status
{
  Will_be_discarded, // it means that there is no need to load the data at all since all of them will be discarded
  Will_be_returned_unchanged,
  Maybe_modified,
  Failed // a fatal internal error occurred, e.g. Modification_controller was already deleted 
         // or initial modification polygon editing was not finished yet (end_polygon_editing was not called)
};

class Geometry_buffer
{
public:
  DECL_PTR(Geometry_buffer);
  virtual ~Geometry_buffer() = default;
  virtual Mesh_abstract::Ptr          get_mesh() const = 0;
  virtual Geometry_format             get_format() const = 0;
  virtual utl::Raw_buffer_view        to_legacy_buffer(Attrib_flags* out_actual_attributes_mask = nullptr) const = 0;
  virtual Modification_state          get_modification_state() const = 0;
};

class Attribute_buffer
{
public:
  DECL_PTR(Attribute_buffer);
  virtual ~Attribute_buffer() = default;
  // --- Attribute_buffer:
  virtual const utl::Raw_buffer_view&  get_raw_data() const = 0;
  //virtual const char*           get_raw_data() const = 0;
  //virtual int                   get_size_in_bytes() const = 0;
  virtual Attrib_index            get_attribute_index() const = 0;
  virtual Type                    get_type() const = 0;
  virtual int                     get_count() const = 0;
  virtual std::string             get_as_string(int index) const = 0;
  virtual double                  get_as_double(int index) const = 0;
  virtual utl::Variant            get_as_variant(int index) const = 0;
};

//I3S_EXPORT    Attribute_buffer::Ptr create_from_buffer( const std::string& buffer, Type type);

struct Basic_stats
{
  double get_average() const {  return count != 0.0 ? sum / count : 0.0;  }
  double minimum = 0.0, maximum = 0.0, stddev = 0.0, sum = 0.0, count=0.0;
};

class Stats_attribute
{
public:
  DECL_PTR(Stats_attribute);
  virtual ~Stats_attribute() = default;
  // --- Stats_attribute:
  virtual void    get_basic(Basic_stats* out) const = 0;
  virtual int     get_histogram_size() const = 0;
  virtual bool    get_histogram(double* lo, double* hi, int64_t* out, int max_out_count) const = 0;
  virtual bool    get_most_frequent_numbers(int* size_in_out, int64_t* counts, double* values) const = 0;
  virtual bool    get_most_frequent_strings(int* size_in_out, int64_t* counts, std::string* values) const = 0;
  virtual std::string to_json() const = 0;
  //virtual bool    get_stats(Stats_simple* out) const = 0;
};

} //end of i3s

} // namespace i3slib 
