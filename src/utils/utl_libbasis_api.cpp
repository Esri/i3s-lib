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
#include <memory>
#include "utils/utl_libbasis_api.h"

// SLL
#include "utils/dxt/dds.h"
#include "utils/utl_i3s_assert.h"

#if defined (NO_BASIS_ENCODER_SUPPORT) && defined (NO_BASIS_TRANSCODER_SUPPORT)
#define NO_BASIS_SUPPORT
#endif

// Basis
#ifndef NO_BASIS_SUPPORT

#include <encoder/basisu_comp.h>

#ifndef NO_BASIS_ENCODER_SUPPORT
#include <encoder/basisu_gpu_texture.h>
#include <encoder/basisu_enc.h>
#endif

#ifndef NO_BASIS_TRANSCODER_SUPPORT
#include <transcoder/basisu_transcoder.h>
#endif

namespace
{
void add_alpha_channel(const uint8_t* rgb, size_t pixel_count, uint8_t* rgba, uint8_t alpha)
{
  const auto size = pixel_count * 3;
  uint32_t out = 0;
  for (uint32_t in = 0; in < size; in += 3, out += 4)
  {
    rgba[out] = rgb[in];
    rgba[out + 1] = rgb[in + 1];
    rgba[out + 2] = rgb[in + 2];
    rgba[out + 3] = alpha;
  }
  I3S_ASSERT(out == pixel_count * 4);
}
}

namespace i3slib
{
  namespace utl
  {
  void basis_init()
  {
    static std::once_flag flag;
#ifndef NO_BASIS_ENCODER_SUPPORT
    std::call_once(flag, []() {basisu::basisu_encoder_init(); });
#else
    std::call_once(flag, []() {basist::basisu_transcoder_init(); });
#endif
  }

#ifndef NO_BASIS_ENCODER_SUPPORT
    bool compress_to_basis_with_mips(
      const char* data,
      int w, int h,
      int component_count, // must be 3 or 4 (rgb8 or rgba8)
      std::string& basis_out,
      i3s::Texture_semantic sem
    )
    {
      if (component_count != 3 && component_count != 4)
      {
        I3S_ASSERT(false);
        return false;
      }
      basis_init();
      // set up params for compressor
      basisu::basis_compressor_params comp_params;
      comp_params.m_status_output = false; // don't print informational messages to console.
      comp_params.m_mip_gen = true;
      comp_params.m_quality_level = 128;
      comp_params.m_compression_level = 1;
      comp_params.m_multithreading = 0;
      comp_params.m_create_ktx2_file = true;

      if (sem == i3s::Texture_semantic::Normal_map)
      {
        comp_params.m_uastc = true;
        // additional options used by basis command line tool when using the -normal-map option
        comp_params.m_perceptual = false;
        comp_params.m_mip_srgb = false;
        comp_params.m_no_selector_rdo = true;
        comp_params.m_no_endpoint_rdo = true;
      }

      // Note: basisu::image constructor adds opaque alpha for rgb input
      basisu::image img;
      comp_params.m_source_images.push_back(basisu::image{ (uint8_t*)data, (uint32_t)w, (uint32_t)h, (uint32_t)component_count });
      // threads
      constexpr auto num_threads = 16;
      basisu::job_pool jpool(num_threads);
      comp_params.m_pJob_pool = &jpool;

      // set up compressor
      basisu::basis_compressor basisu_comp;
      if (!basisu_comp.init(comp_params))
      {
        //basisu::error_printf("Failed to init compressor!\n");
        return false;
      }

      if (basisu_comp.process() != basisu::basis_compressor::cECSuccess)
      {
        //basisu::error_printf("Compression failed!\n");
        return false;
      }

      // output

      //if ((int)out_fmt == 1)
      //{
      basis_out.resize(basisu_comp.get_output_ktx2_file().size());
      memcpy(basis_out.data(), basisu_comp.get_output_ktx2_file().data(), basisu_comp.get_output_ktx2_file().size());
      //}
      //else if ((int)out_fmt == 2)
      //{
      //  basis_out->resize(basisu_comp.get_basis_file_size());
      //  memcpy(basis_out->data(), basisu_comp.get_output_basis_file().data(), basisu_comp.get_output_basis_file().size());
      //}
      //else
      //{
      //  //future format... (for example if we support BASISD_SUPPORT_KTX2_ZSTD)
      //  return false;
      //}

      return true;
    }
#endif // NO_BASIS_ENCODER_SUPPORT

#ifndef NO_BASIS_TRANSCODER_SUPPORT
    static bool is_basis_file(const char* basis, int size)
    {
      constexpr uint8_t c_ktx2_file_identifier[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
      if (size >= sizeof(c_ktx2_file_identifier))
      {
        return (memcmp(basis, c_ktx2_file_identifier, sizeof(c_ktx2_file_identifier)) != 0);
      }
      return false;
    }

    static bool get_image_info(const char* basis, int bytes, int* mip0_w, int* mip0_h, int* num_mipmaps)
    {
#ifdef ENABLE_BASIS_FILE_SUPPORT
      // remove once Basis support is dropped. will be ktx2 only
      bool is_ktx2 = !is_basis_file(basis, bytes);
      if (!is_ktx2)
      {
        basist::basisu_transcoder dec;

        basist::basisu_image_info image_info;
        constexpr int image_index = 0; // only using 1 image per basis file
        if (!dec.get_image_info(basis, bytes, image_info, image_index))
        {
          //basisu::error_printf("Failed retrieving Basis image information!\n");
          return false;
        }
        if (mip0_w)
          *mip0_w = image_info.m_orig_width;
        if (mip0_h)
          *mip0_h = image_info.m_orig_height;
        if (num_mipmaps)
          *num_mipmaps = image_info.m_total_levels;

        return true;
      }
#endif
      basist::ktx2_transcoder dec;
      if (!dec.init(basis, bytes))
        return false;

      if (mip0_w)
        *mip0_w = dec.get_width();
      if (mip0_h)
        *mip0_h = dec.get_height();
      if (num_mipmaps)
        *num_mipmaps = dec.get_levels();

      return true;
    }

    basist::transcoder_texture_format to_basis_transcoder_tex_fmt(Transcoder_format fmt)
    {
      switch (fmt)
      {
      case Transcoder_format::BC1:
          return basist::transcoder_texture_format::cTFBC1_RGB;
        case Transcoder_format::BC3:
          return basist::transcoder_texture_format::cTFBC3_RGBA;
        case Transcoder_format::BC7:
          return basist::transcoder_texture_format::cTFBC7_RGBA;
        case Transcoder_format::RGBA:
          return basist::transcoder_texture_format::cTFRGBA32;
        case Transcoder_format::ETC2:
          return basist::transcoder_texture_format::cTFETC2_RGBA;
        default:
          return basist::transcoder_texture_format::cTFTotalTextureFormats;
      }
    }

    // for error message
    std::string to_string(Transcoder_format fmt)
    {
      switch (fmt)
      {
      case Transcoder_format::BC1:
        return "BC1";
      case Transcoder_format::BC3:
        return "BC3";
      case Transcoder_format::BC7:
        return "BC7";
      default:
        return "unsupported format";
      }
    }

    bool transcode_mip_level(const char* basis, int bytes, int mip_level, std::vector<uint8_t>* out, Transcoder_format fmt)
    {
      basis_init();
      if (basis && out)
      {
        basist::ktx2_transcoder dec;

        if (!dec.init(basis, bytes))
          return false;
        if (!dec.start_transcoding())
        {
          //basisu::error_printf("ktx2_transcoder::start_transcoding() failed! File either uses an unsupported feature, is invalid, was corrupted, or this is a bug.\n");
          return false;
        }

        basist::ktx2_image_level_info level_info;

        if (!dec.get_image_level_info(level_info, mip_level, 0, 0))
        {
          //basisu::error_printf("Failed retrieving image: %u, level: %u information!\n", image_index, mip_level);
          return false;
        }

        const auto basis_transcoder_fmt = to_basis_transcoder_tex_fmt(fmt);
        const auto basis_tex_format = basist::basis_get_basisu_texture_format(basis_transcoder_fmt);

        if (basis_tex_format == basisu::texture_format::cInvalidTextureFormat)
          return false;

        if (!basist::basis_transcoder_format_is_uncompressed(basis_transcoder_fmt))
        {
          basisu::gpu_image gi;
          gi.init(basis_tex_format, level_info.m_orig_width, level_info.m_orig_height);

#ifndef NO_BASIS_ENCODER_SUPPORT
          // Fill the buffer with psuedo-random bytes, to help more visibly detect cases where the transcoder fails to write to part of the output.
          basisu::fill_buffer_with_random_bytes(gi.get_ptr(), gi.get_size_in_bytes());
#endif

          if (!dec.transcode_image_level(mip_level, 0, 0, gi.get_ptr(), gi.get_total_blocks(), basis_transcoder_fmt))
          {
            //basisu::error_printf("Failed transcoding image %u, level: %u, format: %s!\n", image_index, mip_level, to_string(fmt));
            return false;
          }

          auto mip_size_in_bytes = gi.get_size_in_bytes();
          out->resize(mip_size_in_bytes);
          memcpy(out->data(), (uint8_t*)gi.get_ptr(), mip_size_in_bytes);

          return true;
        }
        else
        {
          out->resize(level_info.m_orig_width * level_info.m_orig_height * 4);
          return dec.transcode_image_level(mip_level, 0, 0, out->data(), level_info.m_orig_width * level_info.m_orig_height, basis_transcoder_fmt);
        }
      }
      return false; // basis image file missing or no output specified
    }

#ifdef ENABLE_BASIS_FILE_SUPPORT
    // Keeping this around until runtime updates to support for ktx2
    static bool transcode_mip_level_basis(const char* basis, int bytes, int mip_level, std::vector<uint8_t>* out, Transcoder_format fmt)
    {
      if (basis && out)
      {
        static basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);
        basist::basisu_transcoder dec(&sel_codebook);

        constexpr int image_index = 0; // single image at index 0

        basist::basisu_image_level_info level_info;
        if (!dec.get_image_level_info(basis, bytes, level_info, image_index, mip_level))
        {
          //basisu::error_printf("Failed retrieving image: %u, level: %u information!\n", image_index, mip_level);
          return false;
        }

        const auto basis_transcoder_fmt = to_basis_transcoder_tex_fmt(fmt);
        const auto basis_tex_format = basist::basis_get_basisu_texture_format(basis_transcoder_fmt);

        if (basis_tex_format == basisu::texture_format::cInvalidTextureFormat)
          return false;

        basisu::gpu_image gi;
        gi.init(basis_tex_format, level_info.m_orig_width, level_info.m_orig_height);

#ifndef NO_BASIS_ENCODER_SUPPORT
        // Fill the buffer with psuedo-random bytes, to help more visibly detect cases where the transcoder fails to write to part of the output.
        basisu::fill_buffer_with_random_bytes(gi.get_ptr(), gi.get_size_in_bytes());
#endif

        // start transcoding
        dec.start_transcoding(basis, bytes);
        if (!dec.transcode_image_level(basis, bytes, image_index, mip_level, gi.get_ptr(), gi.get_total_blocks(), basis_transcoder_fmt))
        {
          //basisu::error_printf("Failed transcoding image %u, level: %u, format: %s!\n", image_index, mip_level, to_string(fmt));
          return false;
        }
        dec.stop_transcoding();

        auto mip_size_in_bytes = gi.get_size_in_bytes();
        out->resize(mip_size_in_bytes);
        memcpy(out->data(), (uint8_t*)gi.get_ptr(), mip_size_in_bytes);

        return true;
      }
      return false; // basis image file missing or no output specified
    }
#endif

    // transcodes all mip levels of an image
    static std::vector< std::vector<uint8_t> > transcode_image(const char* basis, int bytes, Transcoder_format fmt)
    {
      int w = 0, h = 0, num_mipmaps = 0;
      if (!get_image_info(basis, bytes, &w, &h, &num_mipmaps))
        return {};

      std::vector< std::vector<uint8_t> > mipmaps;
      mipmaps.resize(num_mipmaps);
      bool is_ktx2 = !is_basis_file(basis, bytes);

      for (int level_index = 0; level_index < num_mipmaps; level_index++)
      {
        auto& gi = mipmaps[level_index];
#ifdef ENABLE_BASIS_FILE_SUPPORT
        // remove once Basis support is dropped. will be ktx2 only
        if (!is_ktx2 && !transcode_mip_level_basis(basis, bytes, level_index, &gi, fmt))
          return {};
#endif
        if (is_ktx2 && !transcode_mip_level(basis, bytes, level_index, &gi, fmt))
          return {};
      }
      return mipmaps;
    }


    static bool copy_mipmaps_to_dds(const std::vector< std::vector<uint8_t > >& gpu_images, std::vector<uint8_t>* dds_out, int width, int height, int mipmap_count, bool has_alpha)
    {
      static constexpr int c_dxt5_block_size = 16; // BC3
      static constexpr int c_dxt1_block_size = 8;  // BC1
      // DDS header
      DirectX::DDS_HEADER hdr;
      memset(&hdr, 0, sizeof(DirectX::DDS_HEADER));
      // --- set the header:
      hdr.size = 124;
      hdr.flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP | DDS_HEADER_FLAGS_LINEARSIZE; // compressed texture with mipmaps
      hdr.height = height;
      hdr.width = width;
      hdr.pitchOrLinearSize = std::max(1, ((width + 3) / 4)) * std::max(1, ((height + 3) / 4)) * (has_alpha ? c_dxt5_block_size : c_dxt1_block_size);
      hdr.mipMapCount = mipmap_count;
      hdr.ddspf = has_alpha ? DirectX::DDSPF_DXT5 : DirectX::DDSPF_DXT1;
      hdr.caps = DDS_SURFACE_FLAGS_MIPMAP | DDS_SURFACE_FLAGS_TEXTURE;

      int offset = 0;
      // add magic
      std::vector<uint8_t> dds(sizeof(uint32_t));
      memcpy(dds.data(), &DirectX::DDS_MAGIC, sizeof(uint32_t));
      offset += sizeof(uint32_t);
      // copy header
      dds.resize(dds.size() + sizeof(DirectX::DDS_HEADER));
      memcpy(dds.data() + offset, &hdr, sizeof(DirectX::DDS_HEADER));
      offset += sizeof(DirectX::DDS_HEADER);

      // copy mipmaps
      for (int mip_index = 0; mip_index < mipmap_count; mip_index++)
      {
        const auto& img = gpu_images[mip_index];
        const auto img_size_in_bytes = img.size();
        // copy data
        const auto dds_end = dds.size();
        dds.resize(dds_end + img_size_in_bytes);
        memcpy(dds.data() + dds_end, img.data(), img_size_in_bytes);
      } // mip_index

      *dds_out = std::move(dds);
      return true;

    }

    bool transcode_basis_to_dds(const char* basis, int bytes, std::vector<uint8_t>* dds_out, bool has_alpha)
    {
      if (basis && dds_out)
      {
        int width = 0, height = 0, mipmap_count = 0;
        if (!get_image_info(basis, bytes, &width, &height, &mipmap_count))
          return false;

        auto fmt = has_alpha ? Transcoder_format::BC3 : Transcoder_format::BC1;

        auto gpu_images = transcode_image(basis, bytes, fmt);
        if (gpu_images.empty())
          return false; // transcoding failed

        copy_mipmaps_to_dds(gpu_images, dds_out, width, height, mipmap_count, has_alpha);

        return true;
      }
      return false; // mising input or output

    }

    static bool create_ktx_from_etc2_mipmaps(const std::vector< std::vector<uint8_t > >& gpu_images, std::vector<uint8_t>& ktx_out, int width, int height, int mipmap_count, bool has_alpha)
    {
      struct KTX_header
      {
        uint8_t identifier[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
        uint32_t endianness{ 0 };
        uint32_t gl_type{ 0 };
        uint32_t gl_type_size{ 0 };
        uint32_t gl_format{ 0 };
        uint32_t gl_internal_format{ 0 };
        uint32_t gl_base_internal_format{ 0 };
        uint32_t pixel_width{ 0 };
        uint32_t pixel_height{ 0 };
        uint32_t pixel_depth{ 0 };
        uint32_t number_of_array_elements{ 0 };
        uint32_t number_of_faces{ 0 };
        uint32_t number_of_mipmap_levels{ 0 };
        uint32_t bytes_of_key_value_data{ 0 };
      };

      KTX_header hdr;
      hdr.endianness = 0x04030201;
      hdr.gl_type_size = 1;
      hdr.gl_internal_format = 0x9278; // GL_COMPRESSED_RGBA8_ETC2_EAC;
      hdr.gl_base_internal_format = has_alpha ? 0x1908 : 0x1907; // GL_RGBA: GL_RGB;
      hdr.number_of_faces = 1;
      hdr.pixel_height = height;
      hdr.pixel_width = width;
      hdr.number_of_mipmap_levels = mipmap_count;

      // Determine total size in bytes and allocate once
      auto total_size_in_bytes = sizeof(hdr) + static_cast<size_t>(4 * mipmap_count);
      for (auto const& gpu_image : gpu_images)
      {
        total_size_in_bytes += gpu_image.size();
      }

      try
      {
        ktx_out.resize(total_size_in_bytes);
      }
      catch (std::bad_alloc const&)
      {
        return false;
      }

      // copy header
      memcpy(ktx_out.data(), &hdr, sizeof(hdr));

      ptrdiff_t offset = sizeof(hdr);
      // copy mipmaps
      for (int mip_index = 0; mip_index < mipmap_count; mip_index++)
      {
        const auto& img = gpu_images[mip_index];
        const auto img_size_in_bytes = img.size();
        // copy data
        memcpy(ktx_out.data() + offset, &img_size_in_bytes, 4);
        memcpy(ktx_out.data() + offset + 4, img.data(), img_size_in_bytes);

        offset += 4 + img_size_in_bytes;
      } // mip_index

      return true;
    }

    bool transcode_basis_to_ktx(const char* basis, int bytes, std::vector<uint8_t>* ktx_out, bool has_alpha)
    {
      if (basis == nullptr || ktx_out == nullptr)
      {
        return false; // mising input or output
      }

      int width = 0, height = 0, mipmap_count = 0;
      if (!get_image_info(basis, bytes, &width, &height, &mipmap_count))
        return false;

      auto fmt = Transcoder_format::ETC2;

      auto gpu_images = transcode_image(basis, bytes, fmt);
      if (gpu_images.empty())
      {
        return false; // transcoding failed
      }

      return create_ktx_from_etc2_mipmaps(gpu_images, *ktx_out, width, height, mipmap_count, has_alpha);
    }

    bool get_basis_image_info(const char* basis, int bytes, int* mip0_w, int* mip0_h, int* mipmap_count)
    {
      return get_image_info(basis, bytes, mip0_w, mip0_h, mipmap_count);
    }
#endif // NO_BASIS_TRANSCODER_SUPPORT
  } // end ::utl
} // end ::i3slib
#else
// BASIS SUPPORT IS DISABLED
namespace i3slib
{
  namespace utl
  {
    // ENCODING
    bool compress_to_basis_with_mips(
      const char* data,
      int w, int h,
      int component_count, // must be 3 or 4 (rgb8 or rgba8)
      std::string& basis_out
    )
    {
      if (component_count != 3 && component_count != 4)
      {
        I3S_ASSERT(false);
        return false;
      }
      return true;
    }
    // TRANSCODING
    bool transcode_mip_level(const char* basis_img, int num_bytes, int mip_level, std::vector<uint8_t>* out, Transcoder_format fmt) { return true; }
    bool get_basis_image_info(const char* basis_img, int num_bytes, int* mip0_w, int* mip0_h, int* mipmap_count) { return true; }
    bool transcode_basis_to_dds(const char* basis_img, int num_bytes, std::vector<uint8_t>* out, bool has_alpha) { return true; }
    }
  }
#endif
