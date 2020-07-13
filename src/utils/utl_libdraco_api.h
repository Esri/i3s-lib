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
  const unsigned int*   fid = nullptr;
  const unsigned int*   fid_index = nullptr;
  int                   bits_pos = 20, bits_normal = 20, bits_uv = 20;
  double                pos_scale_x=1.0, pos_scale_y=1.0; //already applied to vertex positions.
}; 

enum draco_attrib_type_t { Pos=0, Normal, Color, Uv, Region, Fid_index, Fid };
typedef void* draco_mesh_handle_t;

//! WARNING: returned pointer must be byte-aligned on value_stride;
typedef bool (*draco_create_mesh_attribute_t)(draco_mesh_handle_t hdl, draco_attrib_type_t type, int value_count, int  value_stride, int index_count, char** val_out, unsigned int** idx_out);

typedef char* (*draco_create_buffer_t)( int size );

bool draco_compress_mesh(const draco_i3s_mesh* src, draco_create_buffer_t alloc, char** dst, int* bytes, bool is_mesh);
bool draco_decompress_mesh(const char* src, int src_bytes, draco_mesh_handle_t hdl, draco_create_mesh_attribute_t create_attrib_fct);

} // namespace utl 

} // namespace i3slib
