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

#include "etc2comp_c_api.h"
#include "Etc.h"
#include "EtcImage.h"
#include <sstream>
#include <cstring>

static_assert((int)etc_error_metric::RGBA == (int)Etc::ErrorMetric::RGBA, "Mismatch between C and CPP enums");
static_assert((int)etc_img_format::UNKNOWN == (int)Etc::Image::Format::UNKNOWN, "Mismatch between C and CPP enums");

// ------------------------------------------------------------------------------
//      Ktx header 
// ------------------------------------------------------------------------------
class Ktx_header
{
public:

  typedef struct
  {
    uint32_t  u32KeyAndValueByteSize;
  } KeyValuePair;

  typedef struct
  {
    uint8_t  m_au8Identifier[12];
    uint32_t m_u32Endianness;
    uint32_t m_u32GlType;
    uint32_t m_u32GlTypeSize;
    uint32_t m_u32GlFormat;
    uint32_t m_u32GlInternalFormat;
    uint32_t m_u32GlBaseInternalFormat;
    uint32_t m_u32PixelWidth;
    uint32_t m_u32PixelHeight;
    uint32_t m_u32PixelDepth;
    uint32_t m_u32NumberOfArrayElements;
    uint32_t m_u32NumberOfFaces;
    uint32_t m_u32NumberOfMipmapLevels;
    uint32_t m_u32BytesOfKeyValueData;
  } Data;

  enum class InternalFormat
  {
    ETC1_RGB8 = 0x8D64,
    ETC1_ALPHA8 = ETC1_RGB8,
    //
    ETC2_R11 = 0x9270,
    ETC2_SIGNED_R11 = 0x9271,
    ETC2_RG11 = 0x9272,
    ETC2_SIGNED_RG11 = 0x9273,
    ETC2_RGB8 = 0x9274,
    ETC2_SRGB8 = 0x9275,
    ETC2_RGB8A1 = 0x9276,
    ETC2_SRGB8_PUNCHTHROUGH_ALPHA1 = 0x9277,
    ETC2_RGBA8 = 0x9278
  };

  enum class BaseInternalFormat
  {
    ETC2_R11 = 0x1903,
    ETC2_RG11 = 0x8227,
    ETC1_RGB8 = 0x1907,
    ETC1_ALPHA8 = ETC1_RGB8,
    //
    ETC2_RGB8 = 0x1907,
    ETC2_RGB8A1 = 0x1908,
    ETC2_RGBA8 = 0x1908,
  };

  //FileHeader_Ktx(File *a_pfile);
  Ktx_header(int img_w, int img_h, Etc::Image::Format format, int mip_count = 1);

  bool Write(std::ostream& out);
  
  //void AddKeyAndValue(KeyValuePair *a_pkeyvaluepair);

private:

  Data m_data;
  //KeyValuePair *m_pkeyvaluepair;

  //uint32_t m_u32Images;
  //uint32_t m_u32KeyValuePairs;
};

using namespace Etc;
//m_pfile = a_pfile;

static const uint8_t s_au8Itentfier[12] =
{
  0xAB, 0x4B, 0x54, 0x58, // first four bytes of Byte[12] identifier
  0x20, 0x31, 0x31, 0xBB, // next four bytes of Byte[12] identifier
  0x0D, 0x0A, 0x1A, 0x0A  // final four bytes of Byte[12] identifier
};

Ktx_header::Ktx_header(int img_w, int img_h, Etc::Image::Format format, int mip_count )
{
  for (unsigned int ui = 0; ui < sizeof(s_au8Itentfier); ui++)
  {
    m_data.m_au8Identifier[ui] = s_au8Itentfier[ui];
  }

  m_data.m_u32Endianness = 0x04030201;
  m_data.m_u32GlType = 0;
  m_data.m_u32GlTypeSize = 1;
  m_data.m_u32GlFormat = 0;

  switch (format)
  {
    case Image::Format::RGB8:
    case Image::Format::SRGB8:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_RGB8;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_RGB8;
      break;

    case Image::Format::RGBA8:
    case Image::Format::SRGBA8:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_RGBA8;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_RGBA8;
      break;

    case Image::Format::RGB8A1:
    case Image::Format::SRGB8A1:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_RGB8A1;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_RGB8A1;
      break;

    case Image::Format::R11:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_R11;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_R11;
      break;

    case Image::Format::SIGNED_R11:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_SIGNED_R11;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_R11;
      break;

    case Image::Format::RG11:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_RG11;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_RG11;
      break;

    case Image::Format::SIGNED_RG11:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC2_SIGNED_RG11;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC2_RG11;
      break;

    default:
      m_data.m_u32GlInternalFormat = (unsigned int)InternalFormat::ETC1_RGB8;
      m_data.m_u32GlBaseInternalFormat = (unsigned int)BaseInternalFormat::ETC1_RGB8;
      break;
  }

  m_data.m_u32PixelWidth = 0;
  m_data.m_u32PixelHeight = 0;
  m_data.m_u32PixelDepth = 0;
  m_data.m_u32NumberOfArrayElements = 0;
  m_data.m_u32NumberOfFaces = 0;
  m_data.m_u32BytesOfKeyValueData = 0;

  //m_pkeyvaluepair = nullptr;

  //m_u32Images = 0;
  //m_u32KeyValuePairs = 0;

  m_data.m_u32PixelWidth = img_w;
  m_data.m_u32PixelHeight = img_h;
  m_data.m_u32PixelDepth = 0;
  m_data.m_u32NumberOfArrayElements = 0;
  m_data.m_u32NumberOfFaces = 1;
  m_data.m_u32NumberOfMipmapLevels = mip_count;
}

bool Ktx_header::Write(std::ostream& out)
{
  //size_t szBytesWritten;

  // Write header
  //szBytesWritten = fwrite(&m_data, 1, sizeof(Data), a_pfile);
  //assert(szBytesWritten == sizeof(Data));
  out.write(reinterpret_cast<const char*>(&m_data), sizeof(Data));

  // Write KeyAndValuePairs
  //if (m_u32KeyValuePairs)
  //{
  //  //fwrite(m_pkeyvaluepair, m_pkeyvaluepair->u32KeyAndValueByteSize, 1, a_pfile);
  //  out.write(reinterpret_cast<const char*>(m_pkeyvaluepair), m_pkeyvaluepair->u32KeyAndValueByteSize);
  //}
  return out.good() && !out.fail();
}

// ------------------------------------------------------------------------------
//     C API
// ------------------------------------------------------------------------------


struct Hdl
{
  Etc::RawImage* mips=nullptr;
  int mip_count=0;
  int img_w=0, img_h=0;
  Etc::Image::Format img_format;
};

void  etc_free_mip_image(mip_image_hdl_t hdl)
{
  Hdl& inner = *reinterpret_cast<Hdl*>(hdl);
  delete[] inner.mips;
  std::memset(&inner, 0, sizeof(inner));
}

int   etc_get_ktx_size(mip_image_hdl_t hdl)
{
  static_assert(sizeof(Ktx_header) == sizeof(Ktx_header::Data), "Unexpected size");
  int size = sizeof(Ktx_header);
  Hdl& inner = *reinterpret_cast<Hdl*>(hdl);
  for( int i=0; i < inner.mip_count; ++i )
  {
    size += sizeof(int);
    size += inner.mips[i].uiEncodingBitsBytes;
  }
  return size;
}


bool etc_write_ktx(mip_image_hdl_t hdl, char* out_ptr, int max_byte_out)
{
  Hdl& inner = *reinterpret_cast<Hdl*>(hdl);
  if (inner.mip_count == 0)
    return false; //invalid arg


  Ktx_header hdr(inner.img_w, inner.img_h, inner.img_format, inner.mip_count);

  std::ostringstream out;

  //write the header:
  hdr.Write(out);
  //write the data:

  for ( int mip = 0; mip < inner.mip_count; mip++)
  {
    // Write u32 image size
    out.write(reinterpret_cast<const char*>(&(inner.mips[mip].uiEncodingBitsBytes)), sizeof(uint32_t));
    //uint32_t szBytesWritten = fwrite(&u32ImageSize, 1, sizeof(u32ImageSize), pfile);

    out.write(reinterpret_cast<const char*>(inner.mips[mip].paucEncodingBits.get()), inner.mips[mip].uiEncodingBitsBytes);
    //unsigned int iResult = (int)fwrite(m_pMipmapImages[mip].paucEncodingBits.get(), 1, m_pMipmapImages[mip].uiEncodingBitsBytes, pfile);
  }
  if (!out.good() || out.fail())
    return false;
  auto str = out.str(); //unfortunate copy...
  if (max_byte_out < str.size())
    return false;
  std::memcpy(out_ptr, str.data(), str.size());
  return true;
}


mip_image_hdl_t            etc_encode_mipmaps(float *a_pafSourceRGBA,
                                   unsigned int img_w,
                                   unsigned int img_h,
                                   etc_img_format a_format,
                                   etc_error_metric a_eErrMetric,
                                   float a_fEffort,
                                   unsigned int a_uiMaxJobs,
                                   unsigned int a_uiMipFilterFlags,
                                   int *a_piEncodingTime_ms)
{


  //compute number of mips:
  int dim = img_w < img_h ? img_w : img_h;
  int maxMips = 0;
  while (dim >= 1)
  {
    maxMips++;
    dim >>= 1;
  }

  //Populate or C-api handle:
  Hdl* inner = new Hdl();
  inner->mips = new Etc::RawImage[maxMips];
  inner->mip_count = maxMips;
  inner->img_w = img_w;
  inner->img_h = img_h;
  inner->img_format = (Etc::Image::Format)a_format;
  
  Etc::EncodeMipmaps(
    a_pafSourceRGBA,
    img_w,
    img_h,
    (Etc::Image::Format)a_format,
    (Etc::ErrorMetric)a_eErrMetric,
    a_fEffort,
    a_uiMaxJobs,
    a_uiMaxJobs,
    maxMips,
    a_uiMipFilterFlags,
    inner->mips,
    a_piEncodingTime_ms,
    false
  );

  return inner;
}


