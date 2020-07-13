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
#include "utils/utl_jpeg.h"
#include <cstdlib>
#include <functional>
#include <csetjmp>
#include "utils/utl_gzip.h"
#include "utils/utl_fs.h"
#include "utils/utl_lock.h"
#include "utils/utl_colors.h"

// Need to define this before including jpeglib.h to gain access to RGB_* definitions.
#define JPEG_INTERNAL_OPTIONS

extern "C"
{
#include <jpeglib.h>
#include <jerror.h>
}

namespace i3slib
{

static void init_source(j_decompress_ptr cinfo) {}
static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
  ERREXIT(cinfo, JERR_INPUT_EMPTY);
  return TRUE;
}
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  struct jpeg_source_mgr* src = (struct jpeg_source_mgr*) cinfo->src;

  if (num_bytes > 0) {
    src->next_input_byte += (size_t)num_bytes;
    src->bytes_in_buffer -= (size_t)num_bytes;
  }
}
static void term_source(j_decompress_ptr cinfo) {}

// this function is now part of libjpeg 8+, but older version may not have it:
[[maybe_unused]]
static void jpeg_mem_src(j_decompress_ptr cinfo, const void* buffer, long nbytes)
{
  struct jpeg_source_mgr* src;

  if (cinfo->src == NULL) {   /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_PERMANENT,
                                  sizeof(jpeg_source_mgr));
  }

  src = (struct jpeg_source_mgr*) cinfo->src;
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;
  src->bytes_in_buffer = nbytes;
  src->next_input_byte = (const JOCTET*)buffer;

}

typedef std::function< unsigned char* (int w, int h) > Alloc_rgba_buffer_fct;
enum class jpeg_status : int { ok, error_dst_buffer_too_small, error_jpeg_lib, error_unsupported_pixel_format };


struct Jpeg_error_manager {
  /* "public" fields */
  struct jpeg_error_mgr pub;
  /* for return to caller */
  jmp_buf setjmp_buffer;
};

char jpeg_last_rrror_msg[JMSG_LENGTH_MAX];
static void jpeg_error_exit(j_common_ptr cinfo)
{
  /* cinfo->err actually points to a Jpeg_error_manager struct */
  Jpeg_error_manager* myerr = (Jpeg_error_manager*)cinfo->err;
  /* note : *(cinfo->err) is now equivalent to myerr->pub */

  /* output_message is a method to print an error message */
  /*(* (cinfo->err->output_message) ) (cinfo);*/

  /* Create the message */
  (*(cinfo->err->format_message)) (cinfo, jpeg_last_rrror_msg);

  /* Jump to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

//static std::mutex g_zlib_lock;

namespace
{
  void remap_rgb(int w, int channel_count, const unsigned char* src, unsigned char* dst)
  {
    for (int i = 0; i < w; i++, src += channel_count, dst += RGB_PIXELSIZE)
    {
      dst[RGB_RED] = src[0];
      dst[RGB_GREEN] = src[1];
      dst[RGB_BLUE] = src[2];
    }
  }

} // namespace


enum class Jpeg_format : int { rgba, rgb };

jpeg_status decompress_jpeg_internal(const char* src, int jpg_size, int* w, int* h
                                     ,  Alloc_rgba_buffer_fct allocator = [](int, int) { return nullptr; }
                                     ,  Jpeg_format requested= Jpeg_format::rgba, bool* has_alpha=nullptr )
{
  if (has_alpha)
    *has_alpha = false;
  //I3S_ASSERT_EXT( w * h * (int)sizeof(int) <= buffer_size_in_bytes);

  // Variables for the decompressor itself
  struct jpeg_decompress_struct cinfo;

  // Variables for the output buffer, and how long each row is
  int row_stride, width, height, pixel_size;

  const unsigned char* jpg_buffer = (const unsigned char*)src;//reinterpret_cast<const unsigned char*>(buffer.data());

  //int jpg_size = (int)buffer.size();

#if 1
  Jpeg_error_manager jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error. */
    //std::cerr << jpeg_last_rrror_msg << std::endl;
    jpeg_destroy_decompress(&cinfo);
    //fclose(fileHandler);
    return jpeg_status::error_jpeg_lib;
  }
#else
  struct jpeg_error_mgr jerr;

  // Allocate a new decompress struct, with the default error handler.
  // The default error handler will exit() on pretty much any issue,
  // so it's likely you'll want to replace it or supplement it with
  // your own.
  cinfo.err = jpeg_std_error(&jerr);
#endif
  jpeg_create_decompress(&cinfo);

  // Configure this decompressor to read its data from a memory 
  // buffer starting at unsigned char *jpg_buffer, which is jpg_size
  // long, and which must contain a complete jpg already.
  //
  // If you need something fancier than this, you must write your 
  // own data source manager, which shouldn't be too hard if you know
  // what it is you need it to do. See jpeg-8d/jdatasrc.c for the 
  // implementation of the standard jpeg_mem_src and jpeg_stdio_src 
  // managers as examples to work from.
  jpeg_mem_src(&cinfo, jpg_buffer, jpg_size);


  // Have the decompressor scan the jpeg header. This won't populate
  // the cinfo struct output fields, but will indicate if the
  // jpeg is valid.
  int rc = jpeg_read_header(&cinfo, TRUE);

  if (rc != 1) {
    I3S_ASSERT(false);
    return jpeg_status::error_jpeg_lib;
    //syslog(LOG_ERR, "File does not seem to be a normal JPEG");
    //exit(EXIT_FAILURE);
  }

  //syslog(LOG_INFO, "Proc: Initiate JPEG decompression");
  // By calling jpeg_start_decompress, you populate cinfo
  // and can then allocate your output bitmap buffers for
  // each scanline.
  jpeg_start_decompress(&cinfo);

  width = cinfo.output_width;
  height = cinfo.output_height;
  *w = width;
  *h = height;

  pixel_size = cinfo.output_components;

  //syslog(LOG_INFO, "Proc: Image is %d by %d with %d components",
  //       width, height, pixel_size);
  //bmp_size = width * height * pixel_size;
  //bmp_buffer = (unsigned char*)malloc(bmp_size);
  if (pixel_size != 3)
  {
    I3S_ASSERT(false);
    return jpeg_status::error_unsupported_pixel_format;
  }
  //alloc the dst buffer:
  unsigned char* dst_ptr = allocator(width, height);
  if (!dst_ptr)
  {
    jpeg_destroy_decompress(&cinfo);
    //jpeg_abort((j_common_ptr)&cinfo); // merely clean up all the nonpermanent memory pools
    return jpeg_status::error_dst_buffer_too_small; // caller will only get the size of the image.
  }

  //to avoid extra mem alloc, use output buffer in place: ( since source is 3 byte per pix and dest is 4 byte per pix )
  utl::Rgba8*       dst_rgba = reinterpret_cast<utl::Rgba8*>(dst_ptr);
  const utl::Rgb8*  src_rgb = reinterpret_cast<utl::Rgb8*>(dst_ptr);  //same if RGB mode
  if (requested == Jpeg_format::rgba)
  {
    const auto quarter_shift = width * height;
    dst_ptr += quarter_shift;
    // we start at 1/4 of the buffer. (we'll need to add the opaque channel in place)
    src_rgb = reinterpret_cast<utl::Rgb8*>(dst_ptr); 
  }
  // entire scanline (row). 
  row_stride = width * pixel_size;


  //syslog(LOG_INFO, "Proc: Start reading scanlines");
  //
  // Now that you have the decompressor entirely configured, it's time
  // to read out all of the scanlines of the jpeg.
  //
  // By default, scanlines will come out in RGBRGBRGB...  order, 
  // but this can be changed by setting cinfo.out_color_space
  //
  // jpeg_read_scanlines takes an array of buffers, one for each scanline.
  // Even if you give it a complete set of buffers for the whole image,
  // it will only ever decompress a few lines at a time. For best 
  // performance, you should pass it an array with cinfo.rec_outbuf_height
  // scanline buffers. rec_outbuf_height is typically 1, 2, or 4, and 
  // at the default high quality decompression setting is always 1.
  while (cinfo.output_scanline < cinfo.output_height) {
    unsigned char *buffer_array[1];
    buffer_array[0] = dst_ptr + (cinfo.output_scanline) * row_stride;

    // This code may run with two flavors of jpeglib:
    //  - when compiled as stand-alone i3slib it links with vanilla jpeglib, which returns RGB as output
    //  - when compiled for AcrGIS Pro it links with ImageAccessLib.dll, which returns BGR as output.
    // We can distinguish between these two cases by looking at the values of RGB_* constants.
    if (RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2)
    {
      JSAMPROW row_pointer;
      std::vector<JSAMPLE> row(width * RGB_PIXELSIZE);
      row_pointer = row.data();
      jpeg_read_scanlines(&cinfo, &row_pointer, 1);
      remap_rgb(width, pixel_size, row_pointer, buffer_array[0]);
    }
    else
    {
      jpeg_read_scanlines(&cinfo, buffer_array, 1);
    }

  }
  //syslog(LOG_INFO, "Proc: Done reading scanlines");

  // keeps number of bytes remaining in buffer
  size_t remaining_bytes = cinfo.src->bytes_in_buffer;

  // Once done reading *all* scanlines, release all internal buffers,
  // etc by calling jpeg_finish_decompress. This lets you go back and
  // reuse the same cinfo object with the same settings, if you
  // want to decompress several jpegs in a row.
  //
  // If you didn't read all the scanlines, but want to stop early,
  // you instead need to call jpeg_abort_decompress(&cinfo)
  jpeg_finish_decompress(&cinfo);

  // At this point, optionally go back and either load a new jpg into
  // the jpg_buffer, or define a new jpeg_mem_src, and then start 
  // another decompress operation.

  // Once you're really really done, destroy the object to free everything
  jpeg_destroy_decompress(&cinfo);

  if (requested == Jpeg_format::rgb)
  {
    //return and ignore extra hack alpha channel:
    return  jpeg_status::ok;;
  }

  // Any (hacked) alpha channel ?
  // extract the alpha channel
  const int count = width * height;
  if (remaining_bytes > 0)
  {
    //last 4 byte would be i32 offset to alpha channel:
    int offset = *reinterpret_cast<const int*>(&src[jpg_size - sizeof(int)]);
    if (offset >= (int)(jpg_size - remaining_bytes) && offset < (int)(jpg_size - sizeof(int)))
    {
      //need to alloc:
      std::vector< unsigned char > alpha_channel(count);
      //utl::Lock_guard lk(g_zlib_lock);
      int status = utl::zlib_uncompress(alpha_channel.data(), (int)alpha_channel.size()
                                        , reinterpret_cast<const unsigned char*>(&src[offset]), (int)(jpg_size - sizeof(int) - offset));

      static const int c_z_ok = 0; // Z_OK
      if (status >= c_z_ok) // stat contains zlib error number
      {
        for (size_t i = 0; i < count - 1; ++i)
        {
          dst_rgba[i] = src_rgb[i];
          dst_rgba[i].a = alpha_channel[i];
        }
        // last one overlap in memory, so do a copy:
        dst_rgba[count - 1] = utl::Rgb8(src_rgb[count - 1]);
        dst_rgba[count - 1].a = alpha_channel[count - 1];
        if (has_alpha)
          *has_alpha = true;
        return jpeg_status::ok;
      }
      else
      {
        I3S_ASSERT(false);
      }
    }
  }
  //now, we need to write opaque alpha channel:
  for (size_t i = 0; i < count - 1; ++i)
    dst_rgba[i] = src_rgb[i];
  // last one overlap in memory, so do a copy:
  dst_rgba[count-1] = utl::Rgb8( src_rgb[count-1] );

  return jpeg_status::ok;
}

bool utl::get_jpeg_size(const char* src, int bytes, int* w, int* h)
{
  auto status = decompress_jpeg_internal(src, bytes, w, h);
  return status == jpeg_status::error_dst_buffer_too_small;
}

bool utl::decompress_jpeg(const char* src, int bytes, Buffer_view<char>* out
                                     , int* out_w, int* out_h, bool* has_alpha, int output_channel_count, int dst_byte_alignment)
{
  I3S_ASSERT(output_channel_count == 3 || output_channel_count == 4);

  auto alloc = [out, output_channel_count, dst_byte_alignment](int w, int h)
  {
    int size = w * h* output_channel_count;
    if (dst_byte_alignment)
    {
      auto buff = std::make_shared< Buffer >(nullptr, size, Buffer::Memory::Deep_aligned, dst_byte_alignment);
      *out = buff->create_writable_view();
    }
    else
    {
      *out = Buffer::create_writable_view(nullptr, size);
    }
    return reinterpret_cast<unsigned char*>(out->data());
  };

  int w, h;
  auto jformat = output_channel_count == 3 ? Jpeg_format::rgb : Jpeg_format::rgba;
  auto ret = decompress_jpeg_internal(src, bytes, &w, &h, alloc, jformat, has_alpha) == jpeg_status::ok;
  if (out_w)
    *out_w = w;
  if (out_h)
    *out_h = h;

  return ret;
}



// compress to jpeg:

/* setup the buffer but we did that in the main function */
static void init_buffer(jpeg_compress_struct* cinfo) {}

/* what to do when the buffer is full; this should almost never
* happen since we allocated our buffer to be big to start with
*/
static boolean empty_buffer(jpeg_compress_struct* cinfo) {
  return TRUE;
}

/* finalize the buffer and do any cleanup stuff */
void term_buffer(jpeg_compress_struct* cinfo) {}


static jpeg_status compress_jpeg_internal(int w, int h, const char* src, int bytes, utl::Buffer_view<char>* out, int channel_count
                              , int quality, int row_stride )
{
  if (channel_count != 3)
  {
    I3S_ASSERT(channel_count != 4); //RGBA is TODO (encode alpha at the end...)
    // no impl. not supported.
    return jpeg_status::error_unsupported_pixel_format;
  }

  struct jpeg_compress_struct cinfo;
  //struct jpeg_error_mgr       jerr;
  struct jpeg_destination_mgr dmgr;

  // create our in-memory output buffer to hold the jpeg
  // It's not actually trivial to estimate the sufficient buffer size.
  // Resulting JPEG may be larger than the input data - a small enough texture
  // may occupy less bytes than just the minimally possible JPEG header.
  // See https://stackoverflow.com/questions/2734678/jpeg-calculating-max-size
  // for some truly pathological examples.

  const auto out_buf_size = w * h * 3 + 1024;
  *out = utl::Buffer::create_writable_typed_view<char>(out_buf_size);
  JOCTET * out_buffer = reinterpret_cast<JOCTET*>(out->data());

  /* here is the magic */
  dmgr.init_destination = init_buffer;
  dmgr.empty_output_buffer = empty_buffer;
  dmgr.term_destination = term_buffer;
  dmgr.next_output_byte = out_buffer;
  dmgr.free_in_buffer = out->size();

#if 1
  Jpeg_error_manager jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error_exit;
  // Establish the setjmp return context for my_error_exit to use. 
  if (setjmp(jerr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error. 
    //std::cerr << jpeg_last_rrror_msg << std::endl;
    jpeg_destroy_compress(&cinfo);
    //fclose(fileHandler);
    return jpeg_status::error_jpeg_lib;
  }
#else
  cinfo.err = jpeg_std_error(&jerr);
#endif
  jpeg_create_compress(&cinfo);

  // make sure we tell it about our manager 
  cinfo.dest = &dmgr;

  cinfo.image_width = w;
  cinfo.image_height = h;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, true); // limit to baseline JPEG values
  jpeg_start_compress(&cinfo, true);

  JSAMPROW row_pointer;
  const uint8_t *buffer = (const uint8_t*)src;
  const int pitch = row_stride ? row_stride :  w*channel_count;

  // This code may run with two flavors of jpeglib:
  //  - when compiled as stand-alone i3slib it links with vanilla jpeglib, which accepts RGB as input
  //  - when compiled for AcrGIS Pro it links with ImageAccessLib.dll, which accepts BGR as input.
  // We can distinguish between these two cases by looking at the values of RGB_* constants.

  if (RGB_RED != 0 || RGB_GREEN != 1 || RGB_BLUE != 2 || channel_count != RGB_PIXELSIZE)
  {
    std::vector<JSAMPLE> row(w * RGB_PIXELSIZE);
    row_pointer = row.data();
    while (cinfo.next_scanline < cinfo.image_height)
    {
      remap_rgb(w, channel_count, buffer + cinfo.next_scanline * pitch, row_pointer);
      jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }
  }
  else
  {
    while (cinfo.next_scanline < cinfo.image_height)
    {
      row_pointer = (JSAMPROW)&buffer[cinfo.next_scanline * pitch];
      jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }
  }

  jpeg_finish_compress(&cinfo);

  /* write the buffer to disk so you can see the image */
  //fwrite(out_buffer, cinfo.dest->next_output_byte - out_buffer, 1, outfile);
  
  //Shrink to actual encoding stream size:
  size_t out_size = cinfo.dest->next_output_byte - out_buffer;
  I3S_ASSERT( cinfo.dest->free_in_buffer == out_buf_size - out_size );
  out->shrink((int)out_size);
  return jpeg_status::ok;
}

bool utl::compress_jpeg(int w, int h, const char* src, int bytes, Buffer_view<char>* out, int channel_count, int quality, int src_row_stride)
{
  return compress_jpeg_internal(w, h, src, bytes, out, channel_count, quality, src_row_stride) == jpeg_status::ok;
}

} // namespace i3slib
