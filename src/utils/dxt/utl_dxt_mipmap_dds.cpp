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

#include "utl_dxt_mipmap_dds.h"
#include "utils/utl_jpeg.h"
#include "utils/utl_bitstream.h"
#include "IntelDXTCompressor.h"
#include "dds.h"
#include "utils/utl_fs.h"
#include <array>
#include <stdint.h>

namespace i3slib
{

namespace utl
{

// -------------------------------------------------------------------------
//        DDS_buffer
// -------------------------------------------------------------------------
class DDS_buffer
{
public:
  DDS_buffer(const DDS_buffer&) = delete;
  DDS_buffer& operator=(const DDS_buffer&) = delete;
  DDS_buffer() = default;
  ~DDS_buffer() = default;

  BYTE*               get_mip_ptr(int lod) { auto ret = m_data.data() + m_mip_offsets[lod]; /*I3S_ASSERT((size_t)ret % 16 == 0); */return (BYTE*)ret; }
  int                 get_lod_count()const { return _hdr().mipMapCount; }
  void                copy_to_buffer(std::string* buffer) const;

  //bool              load_from_dds_file(const utl::String_os& path);
  utl::Buffer_view<char>&  get_buffer() { return m_data; }
public:
  static void create_with_mips_aligned(int w, int h, DDS_buffer* buffer, bool has_alpha);
private:
  static const int c_magic_and_header_size = sizeof(int) + sizeof(DirectX::DDS_HEADER);
  DirectX::DDS_HEADER& _hdr() { return *reinterpret_cast< DirectX::DDS_HEADER*>(m_data.data() + sizeof(int)); }
  const DirectX::DDS_HEADER& _hdr() const { return *reinterpret_cast< const DirectX::DDS_HEADER*>(m_data.data() + sizeof(int)); }
  //BYTE*   m_data =nullptr; //size is equal to offsets[ mips_count];
  utl::Buffer_view<char> m_data;//size is equal to offsets[ mips_count] (or 0 is not init yet);
  std::array< int, 16 > m_mip_offsets; //texture are always smaller that 2^16.
};
//
//bool DDS_buffer::load_from_dds_file(const std::filesystem::path& path)
//{
//  auto tmp    = utl::read_file(L"k:/tools/dds_viewer/1_0.dds");
//  const int& magic    = *(uint*)tmp.data();
//  DirectX::DDS_HEADER* hdr = (DirectX::DDS_HEADER*)(tmp.data() + 4);
//  const uint* p1 = (const uint*)(tmp.data() + c_magic_and_header_size);
//  return false;
//}  

void DDS_buffer::create_with_mips_aligned(int w, int h, DDS_buffer* ret_p, bool has_alpha)
{

  static const int c_dxt1_block_size = 8;
  static const int c_dxt5_block_size = 16;
  DDS_buffer& ret = *ret_p;

  //ret.load_from_dds_file(L"");

  //compute the mips size:
  int mip_w=w, mip_h=h;
  int mips_count = utl::first_bit_set(std::max(mip_w, mip_h));
  ret.m_mip_offsets[0] = c_magic_and_header_size;

  for (int i= 0; i <= mips_count; i++)
  {
    int bytes = std::max(1, ((mip_w + 3) / 4)) * std::max(1, ((mip_h + 3) / 4)) * (has_alpha ? c_dxt5_block_size: c_dxt1_block_size );
    I3S_ASSERT(( bytes % c_sse_alignment ) == 0 || bytes == 8); //so all MIP buffer are 16 bytes align if the base address is.
    ret.m_mip_offsets[i+1] = ret.m_mip_offsets[i] + bytes;
    mip_w >>= 1;
    mip_h >>= 1;
  }

  // --- alloc the DDS buffer with magix, header and mips:
  //ret.m_data = (BYTE*)_aligned_malloc(ret.m_mip_offsets[mips_count], c_sse_alignment);
  auto buff = std::make_shared< Buffer >(nullptr, ret.m_mip_offsets[mips_count], utl::Buffer::Memory::Deep_aligned, c_sse_alignment);
  ret.m_data = buff->create_writable_view();
  memset(ret.m_data.data(), 0, c_magic_and_header_size);
  // --- set magic:
  *reinterpret_cast<uint32_t*>(ret.m_data.data()) = DirectX::DDS_MAGIC; //DDS
  // --- set the header:
  DirectX::DDS_HEADER& hdr = ret._hdr();
  hdr.size = 124;
  hdr.flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP | DDS_HEADER_FLAGS_LINEARSIZE; // compressed texture with mipmaps
  hdr.height = h;
  hdr.width = w;
  //from the MSFT docs, it could be both...
  //hdr.pitchOrLinearSize = std::max(1, ((w + 3) / 4)) * c_dxt1_block_size;
  hdr.pitchOrLinearSize = std::max(1, ((w + 3) / 4)) * std::max(1, ((h + 3) / 4)) * (has_alpha ? c_dxt5_block_size : c_dxt1_block_size);
  hdr.mipMapCount = mips_count;
  hdr.ddspf = has_alpha ? DirectX::DDSPF_DXT5 : DirectX::DDSPF_DXT1;
  hdr.caps = DDS_SURFACE_FLAGS_MIPMAP | DDS_SURFACE_FLAGS_TEXTURE;
}

void  DDS_buffer::copy_to_buffer(std::string* buffer) const
{

  //const DirectX::DDS_HEADER& hdr = _hdr();

  buffer->resize( m_mip_offsets[get_lod_count()] );
  memcpy(buffer->data(), m_data.data(), buffer->size() );
}



// -------------------------------------------------------------------------



bool    compress_to_dds_with_mips( Image_2d& src, bool has_alpha, utl::Buffer_view<char>* dds_out )
{

  if (!utl::is_power_of_two(src.width()) || !utl::is_power_of_two(src.height()) || src.width() < 4 || src.height() < 4)
  {
    I3S_ASSERT(false);
    return false;//ONLY support power-of-2 image for now, but we could use bgl_render::pixel_utils for that.
  }

  I3S_ASSERT(((size_t)src.data() % c_sse_alignment) == 0);

  //src.fill( 0xFF0000FF); //red
  //create the mips:
  Image_2d dst;
  DDS_buffer dds;
  DDS_buffer::create_with_mips_aligned(src.width(), src.height(), &dds, has_alpha);
  int lod = 0;
  while (true)
  {
    //write the src to dds:
    auto dst_ptr = dds.get_mip_ptr(lod);
    if (((size_t)dst_ptr % c_sse_alignment) ==0 )
    {
      //Use SSE version of the compressor. 
      I3S_ASSERT(((size_t)src.data() % c_sse_alignment) == 0);
      if(has_alpha)
        DXTC::CompressImageDXT5SSE2((const BYTE*)src.data(), dst_ptr, src.width(), src.height());
      else
        DXTC::CompressImageDXT1SSE2((const BYTE*)src.data(), dst_ptr, src.width(), src.height());
    }
    else
    {
      if (has_alpha)
        DXTC::CompressImageDXT5((const BYTE*)src.data(), dst_ptr, src.width(), src.height());
      else
        DXTC::CompressImageDXT1((const BYTE*)src.data(), dst_ptr, src.width(), src.height());
    }

    if (lod + 1 == dds.get_lod_count())
      break;
    //create the mipmap:
    Image_2d::mipmap_blocked4x4(src, &dst);
    src.swap(dst);
    ++lod;
  }

  //copy back:
  //dds.copy_to_buffer(dds_out);
  *dds_out = dds.get_buffer();
  
  //static int s_counter = 0;
  //utl::write_file( L"k:/tools/dds_viewer/tex_" + std::to_wstring( s_counter++ ) + L".dds", *dds_out);

  return true;
}
bool    convert_to_dds_with_mips(const std::string& jpeg, utl::Buffer_view<char>* dds_out, int)
{
  //create 
  utl::Buffer_view<char > raw_data;
  bool has_alpha;
  int w, h;
  const int c_rgba = 4;
  if (!utl::decompress_jpeg(jpeg.data(), (int)jpeg.size(), &raw_data, &w, &h, &has_alpha, c_rgba, c_sse_alignment))
    return false;
  I3S_ASSERT(((size_t)raw_data.data() % c_sse_alignment) == 0);

  //wrap into an image:
  Image_2d src = Image_2d::wrap_aligned(w, h, raw_data.data(), raw_data.size());
  return compress_to_dds_with_mips(src, has_alpha, dds_out);
}

} // namespace utl

} // namespace i3slib
