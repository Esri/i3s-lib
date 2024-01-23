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
#include "utils/utl_i3s_export.h"
#include <cinttypes>

namespace i3slib
{

namespace utl
{

struct draco_i3s_mesh
{
  int                   vtx_count=0;
  int                   fid_count = 0;
  const float*          position=nullptr; //float3
  const float*          normal = nullptr; //float3
  const float*          uv = nullptr;  //float2
  const unsigned char*  rgba = nullptr; //uchar4  
  const unsigned short* uv_region = nullptr; //ushort4
  const uint64_t*       fid = nullptr;
  const unsigned int*   fid_index = nullptr;
  int                   bits_pos = 20, bits_normal = 20, bits_uv = 20;
  double                pos_scale_x=1.0, pos_scale_y=1.0; //already applied to vertex positions.
  uint32_t              anchor_point_count = 0;
  const uint32_t*       anchor_point_fid_indices = nullptr;
  const float*          anchor_points = nullptr; // float3
}; 

enum draco_attrib_type_t { Pos=0, Normal, Color, Uv, Region, Fid_index, Fid, Anchor_point_fid_index, Anchor_points };
typedef void* draco_mesh_handle_t;

//! WARNING: returned pointer must be byte-aligned on value_stride;
typedef bool (*draco_create_mesh_attribute_t)(draco_mesh_handle_t hdl, draco_attrib_type_t type, int value_count, int  value_stride, int index_count, char** val_out, unsigned int** idx_out);

typedef char* (*draco_create_buffer_t)( int size );

I3S_EXPORT bool draco_compress_mesh(const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, Has_fids&, bool is_mesh);
bool draco_decompress_mesh(const char* src, int src_bytes, draco_mesh_handle_t hdl, draco_create_mesh_attribute_t create_attrib_fct, Has_fids&);

inline static bool fid_in_range(uint64_t fid)
{
  // At some point (in Pro and also in the JS API) the integral fids are cast to floating point doubles, so we use a range of fids s.t
  // each individual fid in that range can be cast to a double and back to a uint64_t without changing its value.
  //
  // 2^53 is the limit according to: https://stackoverflow.com/questions/1848700/biggest-integer-that-can-be-stored-in-a-double

  return fid < (1ull << 53ull);
}

} // namespace utl 

} // namespace i3slib
