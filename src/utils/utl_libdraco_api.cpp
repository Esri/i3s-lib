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
#include "utils/utl_libdraco_api.h"
#include "utils/utl_i3s_assert.h"
#include "draco/draco_features.h"
#include "draco/mesh/mesh.h"
#include "draco/compression/encode.h"
#include "draco/compression/decode.h"
#include "draco/mesh/triangle_soup_mesh_builder.h"
#include "draco/attributes/attribute_quantization_transform.h"
#include "draco/attributes/attribute_octahedron_transform.h"
#include "draco/point_cloud/point_cloud_builder.h"

namespace i3slib
{

namespace utl
{

namespace
{

using namespace draco;

static const char* c_key_scale_x = "i3s-scale_x";
static const char* c_key_scale_y = "i3s-scale_y";
static const char* c_key_feature_ids = "i3s-feature-ids";
static const char* c_key_feature_ids_high = "i3s-feature-ids-high";
static const char* c_anchor_point_fid_indices = "i3s-anchor-point-fid-indices";
static const char* c_anchor_point_positions = "i3s-anchor-point-positions";
static const char* c_key_attrib_type = "i3s-attribute-type";
static const char* c_metadata_type_fid = "feature-index";
static const char* c_metadata_type_region = "uv-region";

typedef std::array<float, 3> vec3f_t;
typedef std::array<float, 2> vec2f_t;
typedef std::array<uint8_t, 4> rgba_t;
typedef std::array<uint16_t, 4> uv_region_t;
typedef std::array<uint32_t, 2> face_range_t;
typedef std::array<uint32_t, 1> anchor_point_fid_idx_t;

GeometryAttribute::Type to_draco_type(draco_attrib_type_t type)
{
  switch (type)
  {
    case draco_attrib_type_t::Pos:
    case draco_attrib_type_t::Normal:
    case draco_attrib_type_t::Uv:
    case draco_attrib_type_t::Color:
      return (GeometryAttribute::Type)type;
    case draco_attrib_type_t::Fid_index:
    case draco_attrib_type_t::Fid:
      return GeometryAttribute::Type::GENERIC;
    case draco_attrib_type_t::Region:
      return GeometryAttribute::Type::GENERIC;
    default:
      I3S_ASSERT(false);
      return GeometryAttribute::Type::GENERIC;
  }
}

const PointAttribute * get_draco_attribute(const draco::PointCloud* mesh, draco_attrib_type_t type)
{
  const PointAttribute * att=nullptr;
  
  switch (type)
  {
  case draco_attrib_type_t::Fid_index:
  {
    int id = mesh->GetAttributeIdByMetadataEntry(c_key_attrib_type, c_metadata_type_fid);
    att = mesh->GetAttributeByUniqueId(id);
    if (att && (att->num_components() != 1 || att->attribute_type() != GeometryAttribute::Type::GENERIC))
    {
      I3S_ASSERT(false);
      att = nullptr;
    }
    break;
  }
  case draco_attrib_type_t::Region:
  {
    int id = mesh->GetAttributeIdByMetadataEntry(c_key_attrib_type, c_metadata_type_region);
    att = mesh->GetAttributeByUniqueId(id);
    if (att && (att->num_components() != 4 || att->attribute_type() != GeometryAttribute::Type::GENERIC))
    {
      I3S_ASSERT(false);
      return nullptr;
    }
    break;
  }
  case draco_attrib_type_t::Anchor_points:
  case draco_attrib_type_t::Anchor_point_fid_index:
  {
    // It should not get here.
    I3S_ASSERT(false);
    break;
  }
  default:
    auto dtype = to_draco_type(type);
    att = mesh->GetNamedAttribute(dtype, 0);
  }

  return att;
}

//! this class is only used for debugging
template< class T >
struct Attribute_tmp_info
{

  const T& operator[](int  i) const { return values[indices[i] ]; }

  int value_count=0;
  int index_count=0;
  T*    values=nullptr;
  uint32_t*  indices=nullptr;

};

//! read the feature_index as an vertex attribute and the feature_ids as metadata
static bool read_fids(const draco::Mesh* mesh, draco_mesh_handle_t mhdl, draco_create_mesh_attribute_t create_attrib_fct, Has_fids & fids)
{
  fids = Has_fids::No;
  auto src_att = get_draco_attribute(mesh, draco_attrib_type_t::Fid_index);
  if (!src_att)
    return true; //not there.
  const int byte_per_item = DataTypeLength(src_att->data_type()) * src_att->num_components();
  if (byte_per_item != sizeof(uint32_t))
    return false;

  //fetch the expected fids from metadata:
  auto dmeta = mesh->GetMetadata()->GetAttributeMetadataByUniqueId(src_att->unique_id());
  std::vector< int32_t> fids_lowbits;
  std::vector< int32_t> fids_highbits;
  if (!dmeta || !dmeta->GetEntryIntArray(c_key_feature_ids, &fids_lowbits))
  {
    return false; //missing FID metadata
  }
  if (!dmeta->GetEntryIntArray(c_key_feature_ids_high, &fids_highbits))
  {
    // fids are 32bits
    fids = Has_fids::Yes_32;
    fids_highbits.clear();
  }
  else
  {
    // fids are 64bits
    fids = Has_fids::Yes_64;
    if (fids_highbits.size() != fids_lowbits.size())
    {
      return false; // number of lowbit fids != number of highbit fids
    }
  }

  //alloc output:
  const int value_count = static_cast<int>(fids_lowbits.size());
  const int index_count = mesh->num_faces() * 3;
  uint32_t* dst_indices = nullptr;
  char* dst_values_raw = nullptr;
  if (!(*create_attrib_fct)(mhdl, draco_attrib_type_t::Fid, value_count, sizeof(int64_t), index_count, &dst_values_raw, &dst_indices))
    return false;

  //copy FID over:
  const bool has_high_bits = !fids_highbits.empty();
  for (size_t i = 0; i < value_count; ++i)
  {
    // In 'draco_compress', uint32_t values have been encoded on int32_t values (fids_lowbits[i] and fids_highbits[i]) via memcpy.
    // We first cast to uint32_t to recover the original unsigned values, using the right conversion for negative numbers.
    const uint64_t low = static_cast<uint64_t>(static_cast<uint32_t>(fids_lowbits[i]));
    const uint64_t high = has_high_bits ? (static_cast<uint64_t>(static_cast<uint32_t>(fids_highbits[i])) << 32ull) : 0ull;
    uint64_t val = low | high;
    if (!fid_in_range(val))
    {
      I3S_ASSERT_EXT(false);
      return false;
    }
    static_assert(sizeof(char) == 1);
    memcpy(dst_values_raw + sizeof(uint64_t) * i, &val, sizeof(uint64_t));
  }

  //read the attribute index:
  for (FaceIndex i = FaceIndex(0); i < mesh->num_faces(); ++i)
  {
    auto& face = mesh->face(i);
    for (int k = 0; k < 3; ++k)
    {
      //TODO: support ConvertValue too !
      src_att->GetMappedValue( face[k], &(dst_indices[i.value()*3+k]));
    }
  }

  std::vector<int32_t> anchor_fids_index;
  std::vector<uint8_t> anchor_points_data;

  if (dmeta->GetEntryIntArray(c_anchor_point_fid_indices, &anchor_fids_index)
    && dmeta->GetEntryBinary(c_anchor_point_positions, &anchor_points_data))
  {
    //alloc output:
    int value_count = static_cast<int>(anchor_fids_index.size());
    const int index_count = 0;
    uint32_t* dst_indices = nullptr;
    char* dst_values_raw = nullptr;
    if (!(*create_attrib_fct)(mhdl, draco_attrib_type_t::Anchor_point_fid_index, value_count, sizeof(decltype(anchor_fids_index)::value_type), index_count, &dst_values_raw, &dst_indices))
      return false;

    I3S_ASSERT(dst_indices == nullptr);
    std::memcpy(dst_values_raw, anchor_fids_index.data(), anchor_fids_index.size() * sizeof(decltype(anchor_fids_index)::value_type));
    
    value_count = static_cast<int>(anchor_points_data.size() / sizeof(utl::Vec3f));
    dst_values_raw = nullptr;

    if (!(*create_attrib_fct)(mhdl, draco_attrib_type_t::Anchor_points, value_count, sizeof(decltype(anchor_points_data)::value_type), index_count, &dst_values_raw, &dst_indices))
      return false;

    I3S_ASSERT(dst_indices == nullptr);
    std::memcpy(dst_values_raw, anchor_points_data.data(), anchor_points_data.size() * sizeof(decltype(anchor_points_data)::value_type));
    static_assert(sizeof(std::remove_pointer<decltype(dst_values_raw)>::type) == sizeof(decltype(anchor_points_data)::value_type));
  }

  return true;
}

void apply_position_scale(const PointCloud& mesh, char* raw, int count)
{
  vec3f_t* xyz = reinterpret_cast<vec3f_t*>(raw);
  //apply any XY scale:
  if (count)
  {  //add scale to the position attribute meta-data :
    auto pos_att = mesh.GetAttributeMetadataByAttributeId(mesh.GetNamedAttributeId(GeometryAttribute::POSITION));
    if (pos_att)
    {
      double scale_x = 1.0, scale_y = 1.0;
      pos_att->GetEntryDouble(c_key_scale_x, &scale_x);
      pos_att->GetEntryDouble(c_key_scale_y, &scale_y);
      if (scale_x != 1.0 || scale_y != 1.0)
      {
        for (int i = 0; i < count; ++i)
        {
          xyz[i][0] = (float)(scale_x * (double)xyz[i][0]);
          xyz[i][1] = (float)(scale_y * (double)xyz[i][1]);
        }
      }
    }
  }
}

template< class T >
static bool read_attribute(const draco::Mesh* mesh, draco_attrib_type_t type, draco_mesh_handle_t mhdl
  , draco_create_mesh_attribute_t create_attrib_fct, Attribute_tmp_info<T>* info =nullptr)
{
  auto src_att = get_draco_attribute( mesh, type );
  if (!src_att)
    return true; //not there.

  const int byte_per_item = DataTypeLength(src_att->data_type()) * src_att->num_components();

  if (byte_per_item != sizeof(T))
    return false;

  // for positions, use the global face index instead of the mapped attribute index
  const bool use_face_index = type == draco_attrib_type_t::Pos;
  int value_count = use_face_index ? mesh->num_points() : static_cast<int>(src_att->size()); //number of unique values.
  int index_count = mesh->num_faces() * 3; // 0 if not indexed. 

  //alloc destination:
  uint32_t* dst_indices = nullptr;
  char* dst_values_raw = nullptr;
  if (!(*create_attrib_fct)(mhdl, type, value_count, sizeof(T), index_count, &dst_values_raw, &dst_indices))
    return false;

  T* dst_values0 = reinterpret_cast<T*>(dst_values_raw);
  T* dst_values = dst_values0;

  if (info)
  {
    info->value_count = value_count;
    info->index_count = index_count;
    info->values  = dst_values;
    info->indices = dst_indices;
  }
  //read the attribute index:
  for (FaceIndex i = FaceIndex(0); i < mesh->num_faces(); ++i)
  {
    auto& face = mesh->face(i);
    for (int k = 0; k < 3; ++k)
    {
      dst_indices[i.value() * 3 + k ]= use_face_index ? face[k].value() : src_att->mapped_index(face[k]).value();
    }
  }
  //read the data:
  I3S_ASSERT(src_att->num_components() == dst_values->size());
  const auto dst_size = static_cast<int8_t>(dst_values->size());
  
  if (use_face_index) {
    for (PointIndex i = PointIndex(0); i < PointIndex(value_count); ++i, ++dst_values)
    {
      auto& dst = *dst_values;
      bool ret = src_att->ConvertValue(src_att->mapped_index(i), dst_size, &dst[0]);
      if (!ret)
      {
        I3S_ASSERT(ret);
        return false;
      }
    }
  }
  else {
    for (AttributeValueIndex i = AttributeValueIndex(0); i < AttributeValueIndex(value_count); ++i, ++dst_values)
    {
      auto& dst = *dst_values;
      bool ret = src_att->ConvertValue(i, dst_size, &dst[0]);
      if (!ret)
      {
        I3S_ASSERT(ret);
        return false;
      }
    }
  }
  if (type == draco_attrib_type_t::Pos) {
    apply_position_scale(*mesh, dst_values_raw, value_count);
  }

  return true;
}

// Position and FID only.
// Points will be decoded sequentially, in FID order.
static bool read_attributes_from_point_cloud(const draco::PointCloud* pc, draco_mesh_handle_t mhdl, draco_create_mesh_attribute_t create_attrib_fct, Has_fids & fids)
{
  fids = Has_fids::No;
  // set up fid
  auto fid_att = get_draco_attribute(pc, draco_attrib_type_t::Fid);
  if (!fid_att)
    return true; //not there.
  const int byte_per_fid = DataTypeLength(fid_att->data_type()) * fid_att->num_components();
  if (byte_per_fid != sizeof(uint32_t))
    return false;

  //fetch the expected fids from metadata:
  auto dmeta = pc->GetMetadata()->GetAttributeMetadataByUniqueId(fid_att->unique_id());
  std::vector< int32_t> fids_lowbits;
  std::vector< int32_t> fids_highbits;
  if (!dmeta || !dmeta->GetEntryIntArray(c_key_feature_ids, &fids_lowbits))
  {
    return false; //missing FID metadata
  }
  if (!dmeta->GetEntryIntArray(c_key_feature_ids_high, &fids_highbits))
  {
    // fids are 32bits
    fids = Has_fids::Yes_32;
    fids_highbits.clear();
  }
  else
  {
    // fids are 64bits
    fids = Has_fids::Yes_64;
    if (fids_highbits.size() != fids_lowbits.size())
    {
      return false; // number of lowbit fids != number of highbit fids
    }
  }

  //alloc fids:
  auto value_count = static_cast<int>(fids_lowbits.size());
  uint32_t* fid_indices = nullptr; // will not be used
  char* fid_values_raw = nullptr;
  if (!(*create_attrib_fct)(mhdl, draco_attrib_type_t::Fid, value_count, sizeof(int64_t), value_count, &fid_values_raw, &fid_indices))
    return false;

  //copy FID over:
  const bool has_high_bits = !fids_highbits.empty();
  for (size_t i = 0; i < value_count; ++i)
  {
    // In 'draco_compress', uint32_t values have been encoded on int32_t values (fids_lowbits[i] and fids_highbits[i]) via memcpy.
    // We first cast to uint32_t to recover the original unsigned values, using the right conversion for negative numbers.
    const uint64_t low = static_cast<uint64_t>(static_cast<uint32_t>(fids_lowbits[i]));
    const uint64_t high = has_high_bits ? (static_cast<uint64_t>(static_cast<uint32_t>(fids_highbits[i])) << 32ull) : 0ull;
    uint64_t val = low | high;
    if (!fid_in_range(val))
    {
      I3S_ASSERT_EXT(false);
      return false;
    }
    static_assert(sizeof(char) == 1);
    memcpy(fid_values_raw + sizeof(uint64_t) * i, &val, sizeof(uint64_t));
  }

  // implicit index
  for (int i = 0; i < value_count; ++i)
    fid_indices[i] = i;

  // set up position
  auto pos_att = get_draco_attribute(pc, draco_attrib_type_t::Pos);
  if (!pos_att)
    return true; //not there.

  const int byte_per_pos = DataTypeLength(pos_att->data_type()) * pos_att->num_components();

  if (byte_per_pos != sizeof(utl::vec3f_t))
    return false;

  // check number of encoded points is equal to number of fids
  if (value_count != static_cast<int>(pos_att->size()))
    return false;

  //alloc position:
  uint32_t* pos_indices = nullptr; // will not be used
  char* pos_values_raw = nullptr;
  if (!(*create_attrib_fct)(mhdl, draco_attrib_type_t::Pos, value_count, sizeof(vec3f_t), 0, &pos_values_raw, &pos_indices))
    return false;

  vec3f_t* dst_values0 = reinterpret_cast<vec3f_t*>(pos_values_raw);
  vec3f_t* dst_values = dst_values0;

  //read the position data:
  I3S_ASSERT(pos_att->num_components() == dst_values->size());

  for (AttributeValueIndex i = AttributeValueIndex(0); i < AttributeValueIndex(value_count); ++i)
  {
    // get position idx from fid
    uint32_t idx;
    fid_att->GetMappedValue(PointIndex(i.value()), &idx);
    auto& dst = dst_values[idx];

    // The explicit specification of GetValue template args in necessary to make Clang and GCC happy.
    // For some reason if no template args are provided they prefer the non-template overload
    //      void GeometryAttribute::GetValue(AttributeValueIndex, void*)
    // which causes compilation error.
    bool ret = pos_att->GetValue<float, 3>(i, &dst);
    if (!ret)
    {
      I3S_ASSERT(ret);
      return false;
    }
  }

  apply_position_scale(*pc, pos_values_raw, value_count);

  return true;
}

bool read_attributes_from_mesh(const draco::Mesh* mesh, draco_mesh_handle_t mhdl, draco_create_mesh_attribute_t create_attrib_fct, Has_fids & fids)
{
  // --- convert the draco mesh back to i3s:
  read_attribute<vec3f_t>(mesh, draco_attrib_type_t::Pos, mhdl, create_attrib_fct);
  read_attribute<vec3f_t>(mesh, draco_attrib_type_t::Normal, mhdl, create_attrib_fct);

  read_attribute<vec2f_t>(mesh, draco_attrib_type_t::Uv, mhdl, create_attrib_fct);
  read_attribute< rgba_t>(mesh, draco_attrib_type_t::Color, mhdl, create_attrib_fct);
  read_attribute<uv_region_t>(mesh, draco_attrib_type_t::Region, mhdl, create_attrib_fct);

  // FIDs are special, since Draco re-order vertices, we need to keep the original order of the
  // fids since they are used to indexed (i.e. lookup) attribute in the Attribute buffers (i.e. "Fields" buffers)
  // The anchor points if they exist are also read here.
  read_fids(mesh, mhdl, create_attrib_fct, fids);

  return true;
}


//! returns the attribute ID
template< class T >int set_mesh_attribute(int num_faces, TriangleSoupMeshBuilder& mb,  const T* src, GeometryAttribute::Type att, int8_t ncomp, DataType dt)
{
  int att_id = -1;
  if (src)
  {
    att_id = mb.AddAttribute(att, ncomp, dt);
    //FaceIndex idx;
    for (int f = 0; f < num_faces; ++f, src += 3)
    {
      mb.SetAttributeValuesForFace(att_id, FaceIndex(f), src, src+1, src+2);
    }
  }
  return att_id;
};

//! returns the attribute ID
template< class T >int set_mesh_attribute(int num_points, PointCloudBuilder& pcb, const T* src, GeometryAttribute::Type att, int8_t ncomp, DataType dt)
{
  int att_id = -1;
  if (src)
  {
    att_id = pcb.AddAttribute(att, ncomp, dt);
    // Add attribute values.
    for (PointIndex i(0); i < num_points; ++i) {
      pcb.SetAttributeValueForPoint(att_id, i, src+i.value());
    }
  }
  return att_id;
}

typedef decltype(*draco_i3s_mesh::fid) fid_t;
static_assert(sizeof(fid_t) == sizeof(uint64_t)); // we encode fids using 2 32bit metadata arrays, one for low bits, one for high bits.

} // namespace


static std::unique_ptr<draco::Mesh> finalize_builder(TriangleSoupMeshBuilder& builder)
{
  return builder.Finalize();
}

static std::unique_ptr<draco::PointCloud> finalize_builder(PointCloudBuilder& builder)
{
  constexpr bool duplicate_points = false;
  return builder.Finalize(duplicate_points);
}

static bool encode_to_buffer(Encoder& encoder, draco::Mesh* mesh, EncoderBuffer* buffer)
{
  try
  {
    auto status = encoder.EncodeMeshToBuffer(*mesh, buffer);
    return (status.code() == Status::Code::OK);
  }
  catch (...)
  {
    return false;
  }
}

static bool encode_to_buffer(Encoder& encoder, draco::PointCloud* mesh, EncoderBuffer* buffer)
{
  auto status = encoder.EncodePointCloudToBuffer(*mesh, buffer);
  return (status.code() == Status::Code::OK);
}

template<class T>
static bool draco_compress(T& builder, int size, const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, Has_fids & fids)
{
  fids = Has_fids::No;
  if (size < 1)
  {
    I3S_ASSERT(false);
    return false;
  }
  builder.Start(size);
  // --- position:
  const int pos_id      = set_mesh_attribute(size, builder, (const vec3f_t*)src->position, GeometryAttribute::Type::POSITION, 3, DT_FLOAT32);
  if (pos_id < 0)
  {
    I3S_ASSERT(false);
    return false;
  }

  /*const int normal_id   =*/ set_mesh_attribute(size, builder, (const vec3f_t*)src->normal, GeometryAttribute::Type::NORMAL, 3, DT_FLOAT32);
  /*const int uv_id       =*/ set_mesh_attribute(size, builder, (const vec2f_t*)src->uv, GeometryAttribute::Type::TEX_COORD, 2, DT_FLOAT32);
  /*const int color_id    =*/ set_mesh_attribute(size, builder, (const rgba_t*)src->rgba, GeometryAttribute::Type::COLOR, 4, DT_UINT8);
  const int region_id   = set_mesh_attribute(size, builder, (const uv_region_t*)src->uv_region, GeometryAttribute::Type::GENERIC, 4, DT_UINT16);

  int fid_idx_id = -1;

  // feature id: store original feature id order (i.e. index)
  //             as vertex attribute + feature_id has a metadata array
  if (src->fid_count)
  {
    std::unique_ptr< uint32_t[] > implicit_index; //for scoped lifetime
    I3S_ASSERT( src->fid );
    if (!src->fid_index)
    {
      // implicit index case:
      implicit_index.reset( new uint32_t[ src->fid_count] );
      auto iter = implicit_index.get();
      for (uint32_t i = 0; i < (uint32_t)src->fid_count; ++i, ++iter)
        *iter = i;
    }
    fid_idx_id = set_mesh_attribute(size, builder, src->fid_index  ? src->fid_index : implicit_index.get()
                                    , GeometryAttribute::Type::GENERIC, 1, DT_UINT32);
  }

  auto mesh = finalize_builder(builder);

  // Set up the encoder.
  draco::Encoder encoder;

  if (src->bits_pos)
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, src->bits_pos);
  if (src->bits_normal)
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, src->bits_normal);
  if (src->bits_uv)
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, src->bits_uv);

  encoder.SetSpeedOptions(3,3); //(best compression) TBD!

  if (src->pos_scale_x != 1.0 || src->pos_scale_y != 1.0)
  {
    //add scale to the position attribute meta-data :
    std::unique_ptr<AttributeMetadata> att_meta(new AttributeMetadata());
    att_meta->set_att_unique_id( pos_id );
    att_meta->AddEntryDouble( c_key_scale_x, 1.0/ src->pos_scale_x);
    att_meta->AddEntryDouble(c_key_scale_y, 1.0 / src->pos_scale_y);

    mesh->AddAttributeMetadata(pos_id, std::move(att_meta));
  }
  if (fid_idx_id>=0)
  {
    // we add the FID as the meta-data
    std::unique_ptr<AttributeMetadata> fid_meta(new AttributeMetadata());
    fid_meta->set_att_unique_id(fid_idx_id);
    std::vector< int32_t > fid_low_bits(src->fid_count);
    std::vector< int32_t > fid_high_bits(src->fid_count);
    fids = Has_fids::Yes_32;
    // unfortunate copy
    for (size_t i = 0; i < src->fid_count; ++i)
    {
      const uint64_t fid = src->fid[i];

      if (!fid_in_range(fid))
      {
        I3S_ASSERT_EXT(false);
        return false;
      }

      const uint32_t low = static_cast<uint32_t>(fid);
      const uint32_t high = static_cast<uint32_t>(fid >> 32ull);

      fid_low_bits[i] = low;
      fid_high_bits[i] = high;
      if (high)
        fids = Has_fids::Yes_64;
    }
    fid_meta->AddEntryIntArray(c_key_feature_ids, fid_low_bits);
    // only store high bits if some are non-zero
    if (fids == Has_fids::Yes_64)
    {
      fid_meta->AddEntryIntArray(c_key_feature_ids_high, fid_high_bits);
    }

    if (src->anchor_point_count)
    {
      std::vector<int> anchor_point_fid_indices(src->anchor_point_count);
      const size_t points_size = src->anchor_point_count * sizeof(typename std::remove_pointer<decltype(src->anchor_points)>::type) * 3; // We have a position defined by three floats.
      std::vector<uint8_t> anchor_point_positions(points_size);
      for (size_t i = 0, num = src->anchor_point_count; i < num; ++i)
        anchor_point_fid_indices[i] = src->anchor_point_fid_indices[i];
      std::memcpy(anchor_point_positions.data(), src->anchor_points, points_size);

      fid_meta->AddEntryIntArray(c_anchor_point_fid_indices, anchor_point_fid_indices);
      fid_meta->AddEntryBinary(c_anchor_point_positions, anchor_point_positions);
    }

    fid_meta->AddEntryString(c_key_attrib_type, c_metadata_type_fid);
    mesh->AddAttributeMetadata(fid_idx_id, std::move( fid_meta ));
  }
  if (region_id>=0)
  {
    std::unique_ptr<AttributeMetadata> region_meta(new AttributeMetadata());
    region_meta->set_att_unique_id(region_id);
    region_meta->AddEntryString(c_key_attrib_type, c_metadata_type_region);
    mesh->AddAttributeMetadata(region_id, std::move(region_meta));
  }

  EncoderBuffer buffer;
  if (!encode_to_buffer(encoder, mesh.get(), &buffer))
    return false;
  // alloc the dest buffer:
  *dst = (*alloc)(static_cast<int>(buffer.size()));
  if (!*dst)
    return false;

  //copy draco compressed buffer:
  memcpy(*dst, buffer.data(), buffer.size());
  *bytes = static_cast<int>(buffer.size());

  return true;
}

bool draco_compress_mesh(const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, Has_fids & fids)
{
  // Build the mesh.
  I3S_ASSERT(src->vtx_count % 3 ==0);
  const int num_faces = src->vtx_count / 3;
  TriangleSoupMeshBuilder mb;
  return  draco_compress(mb, num_faces, src,alloc, dst, bytes, fids);
}

bool draco_compress_point_cloud(const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, Has_fids & fids)
{
  // Build the point cloud.
  const int num_points = src->vtx_count;
  PointCloudBuilder pcb;
  return draco_compress(pcb, num_points, src, alloc, dst, bytes, fids);
}

bool draco_compress_mesh(const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, Has_fids &fids, bool is_mesh)
{
  // testing
  *bytes = 0;
  *dst = nullptr;
  I3S_ASSERT(src->vtx_count > 0);
  return is_mesh ? draco_compress_mesh(src, alloc, dst, bytes, fids) : draco_compress_point_cloud(src, alloc, dst, bytes, fids);
}

bool draco_decompress_mesh(const char* src, int src_bytes, draco_mesh_handle_t hdl, draco_create_mesh_attribute_t create_attrib_fct, Has_fids & fids)
{
  fids = Has_fids::No;

  draco::DecoderBuffer buffer;
  buffer.Init(src, src_bytes);

  // Decode the input data into a geometry.
  
  auto type_statusor = draco::Decoder::GetEncodedGeometryType(&buffer);
  if (!type_statusor.ok()) {
    I3S_ASSERT(false);
    return false;
  }
  const draco::EncodedGeometryType geom_type = type_statusor.value();

  draco::Decoder decoder;
  if (geom_type == draco::TRIANGULAR_MESH)
  {
    if (auto statusor = decoder.DecodeMeshFromBuffer(&buffer); statusor.ok())
    {
      if (auto in_mesh = std::move(statusor).value())
        return read_attributes_from_mesh(in_mesh.get(), hdl, create_attrib_fct, fids);
    }
    I3S_ASSERT(false); // statusor not ok
    return false;
  }
  else if (geom_type == draco::POINT_CLOUD) {
    if (auto statusor = decoder.DecodePointCloudFromBuffer(&buffer); statusor.ok())
    {
      if (auto in_mesh = std::move(statusor).value())
        return read_attributes_from_point_cloud(in_mesh.get(), hdl, create_attrib_fct, fids);
    }
    I3S_ASSERT(false); // statusor not ok
    return false;
  }
  else
  {
    return false; // not Mesh or PointCloud
  }
}

} // namespace utl 

} // namespace i3slib
