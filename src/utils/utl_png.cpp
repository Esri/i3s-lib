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
#include "utils/utl_png.h"
#include "utils/utl_colors.h"
#include <stdint.h>
#include <cstring>

#include <iostream>
#include <sstream>
#include <stdio.h>

#define PNG_DEBUG 3
#include "libpng/png.h"

namespace i3slib
{

namespace utl
{

std::vector< char > r8_to_rgba8(const uint8_t* src, int nbytes)
{
  std::vector< char > ret(nbytes * 4);
  for (int i = 0; i < nbytes; ++i)
  {
    ret[i * 4] = src[i];
    ret[i * 4 + 1] = src[i];
    ret[i * 4 + 2] = src[i];
    ret[i * 4 + 3] = src[i];// (uint8)255; //opaque
  }
  return ret;
}


//! zero-copy wrapper around a buffer view (with a std::istream-like api)
class Buffer_view_stream
{
public:
  explicit Buffer_view_stream(const Buffer_view<const char>& in) : m_view(in) {}

  Buffer_view_stream&    read(char* dst, size_t count)
  {
    if (!fail())
    {
      if (m_pos + count > (size_t)m_view.size())
        m_pos = c_invalid_pos;
      else
      {
        std::memcpy(dst, &m_view[(int)m_pos], count);
        m_pos += count;
      }
    }
    return *this;
  }
  
  bool fail() const { return m_pos == c_invalid_pos; }

private:
  Buffer_view<const char> m_view;
  static const size_t c_invalid_pos = ~(size_t)0;
  size_t                       m_pos = 0;
};


typedef void(*read_png_custom_fct)(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count_to_read);

template< class T > 
void read_png_from_buffer_tpl(png_structp png_ptr, png_bytep out_bytes,
                              png_size_t byte_count_to_read)
{
  png_voidp io_ptr = png_get_io_ptr(png_ptr);
  if (io_ptr == NULL)
  {
    I3S_ASSERT(false);
    return;   // add custom error handling here
  }
  T & iss = *(T*)io_ptr;
  iss.read((char*)out_bytes, (size_t)byte_count_to_read);
  if (iss.fail())
  {
#if PNG_LIBPNG_VER_MAJOR >= 1 && PNG_LIBPNG_VER_MINOR > 2  //see deprication: http://www.libpng.org/pub/png/src/libpng-1.2.x-to-1.4.x-summary.txt
    png_error(png_ptr, "Unexpected end-of-stream");
#else
    png_error(png_ptr, fileReadingError, "Unexpected end-of-stream");
#endif
  }
}

//bool read_png_from_buffer(const std::string& input, int* out_w, int* out_h, std::vector<char>* out) 


static bool decode_png_internal(void* src_object, read_png_custom_fct read_png_fct,  Buffer_view<char>* out
                           , int* out_w, int* out_h, bool* has_alpha=nullptr
                           , int output_channel_count=4
                           , int dst_byte_alignment=0
                           )
{
  I3S_ASSERT_EXT(output_channel_count == 4); //TODO

  png_structp png_ptr;
  png_infop info_ptr;
  unsigned int sig_read = 0;
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;


  /* Create and initialize the png_struct with the desired error handler
  * functions.  If you want to use the default stderr and longjump method,
  * you can supply NULL for the last three parameters.  We also supply the
  * the compiler header file version, so that we know if the application
  * was compiled with a compatible version of the library.  REQUIRED
  */
  png_voidp user_error_ptr = nullptr;
  png_error_ptr user_error_fn = nullptr;
  png_error_ptr user_warning_fn = nullptr;
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                   /*png_voidp*/user_error_ptr, user_error_fn, user_warning_fn);

  if (png_ptr == NULL)
  {
    //fclose(fp);
    return false;
  }

  /* Allocate/initialize the memory for image information.  REQUIRED. */
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
  {
    //fclose(fp);
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    return false;
  }

  /* Set error handling if you are using the setjmp/longjmp method (this is
  * the normal method of doing things with libpng).  REQUIRED unless you
  * set up your own error handlers in the png_create_read_struct() earlier.
  */

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    /* Free all of the memory associated with the png_ptr and info_ptr */
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    //fclose(fp);
    /* If we get here, we had a problem reading the file */
    return false;
  }

  /* One of the following I/O initialization methods is REQUIRED */
  //#ifdef streams /* PNG file I/O method 1 */
  ///* Set up the input control if you are using standard C streams */
  //png_init_io(png_ptr, fp);

  //#else no_streams /* PNG file I/O method 2 */
    /* If you are using replacement read functions, instead of calling
    * png_init_io() here you would call:
    */
    png_set_read_fn(png_ptr, src_object, read_png_fct);
    /* where user_io_ptr is a structure you want available to the callbacks */
  //#endif no_streams /* Use only one I/O method! */

  /* If we have already read some of the signature */
  png_set_sig_bytes(png_ptr, sig_read);

//#define hilevel 1;
#ifdef hilevel
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, nullptr, nullptr);
  //copy to output:
  if (out_h)
    *out_h = height;
  if (out_w)

    *out_w = width;

  if (has_alpha)
    *has_alpha = true;
  /*
  * If you have enough memory to read in the entire image at once,
  * and you need to specify only transforms that can be controlled
  * with one of the PNG_TRANSFORM_* bits (this presently excludes
  * dithering, filling, setting background, and doing gamma
  * adjustment), then you can read the entire image (including
  * pixels) into the info structure with this call:
  */
  auto png_transforms = PNG_TRANSFORM_IDENTITY;
  png_read_png(png_ptr, info_ptr, png_transforms, nullptr/*png_voidp_NULL*/);
#else
  /* OK, you're doing it the hard way, with the lower-level functions */

  /* The call to png_read_info() gives us all of the information from the
  * PNG file before the first IDAT (image data chunk).  REQUIRED
  */
  png_read_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, nullptr, nullptr);
  //copy to output:
  if (out_h)
    *out_h = height;
  if (out_w)
    *out_w = width;

  if (has_alpha)
  {
    //TBD:
    //PNG supports transparency in two(or three) quite different ways :
    //Truecolor or grayscale images with a separated alpha channel(RGBA or GA)
    //  Transparency extra info in the(optional) tRNS chunk.Which has two different flavors :
    //2a.For indexed images : the tRNS chunk specifies a transparency value("alpha") for one, several or all the palette indexes.
    //  2b.For truecolor or grayscale images : the tRNS chunk specifies a single color value(RGB or Gray) that should be considered as fully transparent.
    //  If you are interested in case 2a, and if you are using libpng, you should look at the function png_get_tRNS()

    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GA)
      *has_alpha = true;
    else
    {
      png_bytep trans_alpha = NULL;
      int num_trans = 0;
      png_color_16p trans_color = NULL;

      png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);
      if (trans_alpha != NULL)
        *has_alpha = true;
      else
        *has_alpha = false;
    }
  }

  if (!out)
  {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr); // clean-up
    return true; //just getting the size...
  }
  /* Set up the data transformations you want.  Note that these are all
  * optional.  Only call them if you want/need them.  Many of the
  * transformations only work on specific types of images, and many
  * are mutually exclusive.
  */

  /* tell libpng to strip 16 bit/color files down to 8 bits/color */
  if (color_type != PNG_COLOR_TYPE_GRAY)
    png_set_strip_16(png_ptr);

  /* Strip alpha bytes from the input data without combining with the
  * background (not recommended).
  */
  // this would set alpha=1, but this is not what we would want (i.e. hidden pixels (alpha=0) will show up)
  //png_set_strip_alpha(png_ptr); 

  /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
  * byte into separate bytes (useful for paletted and grayscale images).
  */
  png_set_packing(png_ptr);

  /* Change the order of packed pixels to least significant bit first
  * (not useful if you are using png_set_packing). */
  //png_set_packswap(png_ptr);

  /* Expand paletted colors into true RGB triplets */
  if (color_type == PNG_COLOR_TYPE_PALETTE)
  {
    png_set_palette_to_rgb(png_ptr);
  }
  /* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
  {
#if PNG_LIBPNG_VER_MAJOR >= 1 && PNG_LIBPNG_VER_MINOR > 2  //see decprication: http://www.libpng.org/pub/png/src/libpng-1.2.x-to-1.4.x-summary.txt
      png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
      png_set_gray_1_2_4_to_8(png_ptr);
#endif
  }
  /* Expand paletted or RGB images with transparency to full alpha channels
  * so the data will be available as RGBA quartets.
  */
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);

  /* Set the background color to draw transparent and alpha images over.
  * It is possible to set the red, green, and blue components directly
  * for paletted images instead of supplying a palette index.  Note that
  * even if the PNG file supplies a background, you are not required to
  * use it - you should use the (solid) application background if it has one.
  */
#if 0 // no alpha blending with background.
  png_color_16 my_background, *image_background;

  if (png_get_bKGD(png_ptr, info_ptr, &image_background))
    png_set_background(png_ptr, image_background,
                       PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
  else
    png_set_background(png_ptr, &my_background,
                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
#endif

  /* Some suggestions as to how to get a screen gamma value */

  /* Note that screen gamma is the display_exponent, which includes
  * the CRT_exponent and any correction for viewing conditions */
  //if (/* We have a user-defined screen gamma value */)
  //{
  //  screen_gamma = user - defined screen_gamma;
  //}
  ///* This is one way that applications share the same screen gamma value */
  //else if ((gamma_str = getenv("SCREEN_GAMMA")) != NULL)
  //{
  //  screen_gamma = atof(gamma_str);
  //}
  ///* If we don't have another value */
  //else
  //{
  //  screen_gamma = 2.2;  /* A good guess for a PC monitors in a dimly
  //                       lit room */
  //  screen_gamma = 1.7 or 1.0;  /* A good guess for Mac systems */
  //}

  double screen_gamma = 2.2;

  /* Tell libpng to handle the gamma conversion for you.  The final call
  * is a good guess for PC generated images, but it should be configurable
  * by the user at run time by the user.  It is strongly suggested that
  * your application support gamma correction.
  */
#if 1
  int intent;

  if (png_get_sRGB(png_ptr, info_ptr, &intent))
    png_set_gamma(png_ptr, screen_gamma, 0.45455);
  else
  {
    double image_gamma;
    if (png_get_gAMA(png_ptr, info_ptr, &image_gamma))
      png_set_gamma(png_ptr, screen_gamma, image_gamma);
    else
      png_set_gamma(png_ptr, screen_gamma, 0.45455);
  }
#endif

  /* Dither RGB files down to 8 bit palette or reduce palettes
  * to the number of colors available on your screen.
  */
#if 0 
  if (color_type & PNG_COLOR_MASK_COLOR)
  {
    int num_palette;
    png_colorp palette;

    /* This reduces the image to the application supplied palette */
    if (/* we have our own palette */)
    {
      /* An array of colors to which the image should be dithered */
      png_color std_color_cube[MAX_SCREEN_COLORS];

      png_set_dither(png_ptr, std_color_cube, MAX_SCREEN_COLORS,
                     MAX_SCREEN_COLORS, png_uint_16p_NULL, 0);
    }
    /* This reduces the image to the palette supplied in the file */
    else if (png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette))
    {
      png_uint_16p histogram = NULL;

      png_get_hIST(png_ptr, info_ptr, &histogram);

      png_set_dither(png_ptr, palette, num_palette,
                     max_screen_colors, histogram, 0);
    }
  }
#endif

  /* invert monochrome files to have 0 as white and 1 as black */
  png_set_invert_mono(png_ptr);

  /* If you want to shift the pixel values from the range [0,255] or
  * [0,65535] to the original [0,7] or [0,31], or whatever range the
  * colors were originally in:
  */
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT))
  {
    png_color_8p sig_bit;

    png_get_sBIT(png_ptr, info_ptr, &sig_bit);
    png_set_shift(png_ptr, sig_bit);
  }
  
#if 0
  /* flip the RGB pixels to BGR (or RGBA to BGRA) */
  if (color_type & PNG_COLOR_MASK_COLOR)
    png_set_bgr(png_ptr);

  /* swap the RGBA or GA data to ARGB or AG (or BGRA to ABGR) */
  png_set_swap_alpha(png_ptr);
#endif

  /* swap bytes of 16 bit files to least significant byte first */
  png_set_swap(png_ptr);

  /* Add filler (or alpha) byte (before/after each RGB triplet) */
  if (color_type != PNG_COLOR_TYPE_GRAY)
    png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

  /* Turn on interlace handling.  REQUIRED if you are not using
  * png_read_image().  To see how to handle interlacing passes,
  * see the png_read_row() method below:
  */
  /*int number_passes =*/ png_set_interlace_handling(png_ptr);

  /* Optional call to gamma correct and add the background to the palette
  * and update info structure.  REQUIRED if you are expecting libpng to
  * update the palette for you (ie you selected such a transform above).
  */
  png_read_update_info(png_ptr, info_ptr);

  /* Allocate the memory to hold the image using the fields of info_ptr. */

  /* The easiest way to read the image: */
  //png_bytep row_pointers[height];
  std::vector< png_voidp > vec_row_pointers(height);
  //png_bytep* row_pointers = vec_row_pointers.data(); // [height];

  const auto row_size = png_get_rowbytes(png_ptr, info_ptr);
  for (int row = 0; row < (int)height; row++)
    vec_row_pointers[row] = png_malloc(png_ptr, row_size);

#define entire 1
  /* Now it's time to read the image.  One of these methods is REQUIRED */
#ifdef entire /* Read the entire image in one go */
  png_read_image(png_ptr, (png_bytepp)(vec_row_pointers.data()));
 
  *out = std::make_shared< Buffer >(nullptr, (int)(height * row_size), Buffer::Memory::Deep, dst_byte_alignment)->create_writable_view();
  //out->resize(height * width * sizeof(uint));
  
  for (int y = 0; y < (int)height; ++y)
  {
    //copy line-by-line:
    std::memcpy(&((*out)[y * static_cast<int>(row_size)]), vec_row_pointers[y], row_size);
    png_free(png_ptr, vec_row_pointers[y]);
  }

#else // no_entire /* Read the image one or more scanlines at a time */
  /* The other way to read images - deal with interlacing: */

  for (int pass = 0; pass < number_passes; pass++)
  {
#ifdef single /* Read the image a single row at a time */
    for (y = 0; y < height; y++)
    {
      png_read_rows(png_ptr, &row_pointers[y], png_bytepp_NULL, 1);
    }

#else // no_single /* Read the image several rows at a time */
    for (int y = 0; y < height; y += number_of_rows)
    {
#ifdef sparkle /* Read the image using the "sparkle" effect. */
      png_read_rows(png_ptr, &row_pointers[y], png_bytepp_NULL,
                    number_of_rows);
#else // no_sparkle /* Read the image using the "rectangle" effect */
      png_read_rows(png_ptr, png_bytepp_NULL, &row_pointers[y],
                    number_of_rows);
#endif // no_sparkle /* use only one of these two methods */
    }

    /* if you want to display the image after every pass, do
    so here */
#endif // no_single /* use only one of these two methods */
  }
#endif // no_entire /* use only one of these two methods */

  /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
  png_read_end(png_ptr, info_ptr);
#endif // hilevel

  /* At this point you have read the entire image */

  /* clean up after the read, and free any memory allocated - REQUIRED */
  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

  /* close the file */
  //fclose(fp);

  /* that's it */
  return true;
}

bool decode_png(const char* src, int bytes, Buffer_view<char>* out
                     , int* out_w, int* out_h, bool* has_alpha
                     , int output_channel_count
                     , int dst_byte_alignment)
{
  //wrap input buffer:
  auto src_view = std::make_shared<Buffer>(src, bytes, Buffer::Memory::Shallow)->create_view();

  Buffer_view_stream bvs(src_view);

  return decode_png_internal( &bvs, read_png_from_buffer_tpl< Buffer_view_stream>, out, out_w, out_h, has_alpha, output_channel_count, dst_byte_alignment);
}

bool read_png_from_file(const std::filesystem::path& file_name, int* out_w, int* out_h, std::vector<char>* out)
{
  std::ifstream iss(file_name, std::ios::binary);
  if (!iss.good())
    return false;

  Buffer_view<char> tmp_copy; //TBD: avoid this extra copy
  if (decode_png_internal(&iss, read_png_from_buffer_tpl< std::ifstream >, &tmp_copy, out_w, out_h))
  {
    out->resize(tmp_copy.size());
    std::memcpy(out->data(), tmp_copy.data(), out->size());
    return true;
  }
  return false;
}

bool decode_png(const std::string& input, int* out_w, int* out_h, std::vector<char>* out, bool* has_alpha)
{
  const int c_png_hdr_magic_size = 8;
  if (input.size() < c_png_hdr_magic_size)
    return false;
  std::istringstream iss(input, std::ios::binary);

  Buffer_view<char> tmp_copy; //TBD: avoid this extra copy
  if (decode_png_internal(&iss, read_png_from_buffer_tpl< std::ifstream >, &tmp_copy, out_w, out_h, has_alpha))
  {
    out->resize(tmp_copy.size());
    std::memcpy(out->data(), tmp_copy.data(), out->size());
    return true;
  }
  return false;

}


bool get_png_size(const char* src, int bytes, int* out_w, int* out_h)
{
  //wrap input buffer:
  auto src_view = std::make_shared<Buffer>(src, bytes, Buffer::Memory::Shallow)->create_view();

  Buffer_view_stream bvs(src_view);
  return decode_png_internal(&bvs, read_png_from_buffer_tpl<Buffer_view_stream>, nullptr, out_w, out_h);
}

namespace
{

int fopen(FILE*& file, const std::filesystem::path& file_name, const std::string& mode)
{
#ifdef _WIN32
  std::wstring wmode(std::cbegin(mode), std::cend(mode));
  return ::_wfopen_s(&file, file_name.c_str(), wmode.c_str());
#else
  file = ::fopen(file_name.c_str(), mode.c_str());
  return file ? 0 : errno;
#endif
}

}

class Png_writer_impl
{
public:
  Png_writer_impl() = default;
  ~Png_writer_impl() { if (m_file) _finalize_write(); }
  bool create_rgba32(const std::filesystem::path& fn, int w, int h);

  bool      add_rows(int n_rows, const char* buffer, int n_bytes);

  static bool     write_file(const std::filesystem::path& path, int w, int h, const char* buffer, int n_bytes);

private:
  bool      _finalize_write();

  FILE* m_file = nullptr;
  int m_w = 0;
  int m_h = 0;
  int m_y0 = 0;
  png_struct_def* m_png_ptr = nullptr;
  png_infop m_info_ptr = nullptr;
};

//! Creates RGBA 8bits per channel PNG file:
bool Png_writer_impl::create_rgba32(const std::filesystem::path& file_name, int w, int h)
{
  m_w = w;
  m_h = h;
  /* create file */
  auto err = fopen(m_file, file_name, "wb");
  if (err)
  {
    std::wcerr << L"PngWriter::CreateRGBA32(): failed to create file " 
      << file_name.generic_wstring() << "\n";
    return false;
  }
  m_png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!m_png_ptr)
  {
    std::cerr << "PngWriter::CreateRGBA32():Failed to create PNG write struct\n";
    return false;
  }
  m_info_ptr = png_create_info_struct(m_png_ptr);
  if (!m_info_ptr)
  {
    std::cerr << "PngWriter::CreateRGBA32():Failed to create PNG info struct\n";
    return false;
  }
  if (setjmp(png_jmpbuf(m_png_ptr)))
  {
    std::cerr << "PngWriter::CreateRGBA32():Error during init_io\n";
    return false;
  }
  png_init_io(m_png_ptr, m_file);

  //write header:
  if (setjmp(png_jmpbuf(m_png_ptr)))
  {
    std::cerr << "PngWriter::CreateRGBA32():Failed to write header\n";
    return false;
  }
  const int bits_per_channel = 8;
  png_set_IHDR(m_png_ptr, m_info_ptr, m_w, m_h,
               bits_per_channel, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_write_info(m_png_ptr, m_info_ptr);

  //ready to write:
  if (setjmp(png_jmpbuf(m_png_ptr)))
  {
    std::cerr << "PngWriter::CreateRGBA32():Failed to setup write data\n";
    return false;
  }
  m_y0 = 0;
  return true;
}

// A a (contiguous) buffer of rows to the PNG file. Rows must be complete ( num columns = image With ).
bool  Png_writer_impl::add_rows(int n_rows, const char* buffer, int n_bytes)
{
  //check param:
  I3S_ASSERT_EXT(m_y0 + n_rows <= m_h);
  const int bytes_per_pix = n_bytes / n_rows / m_w;
  I3S_ASSERT_EXT(bytes_per_pix == 4);

  // create pointer on each row:
  int bytes_per_row = m_w * bytes_per_pix;
  std::vector< png_bytep > rows(n_rows);
  for (int i = 0; i < (int)rows.size(); i++)
    rows[i] = (png_bytep)buffer + i * bytes_per_row;

  //write the rows:
  png_write_rows(m_png_ptr, rows.data(), (png_uint_32)rows.size());

  //move our cursor:
  m_y0 += n_rows;
  return true;
}

//! Write PNG meta-data and close file.
bool Png_writer_impl::_finalize_write()
{
  //end write:
  if (setjmp(png_jmpbuf(m_png_ptr)))
  {
    std::cerr << "PngWriter::CreateRGBA32():Failed to to finalze write\n";
    return false;
  }
  png_write_end(m_png_ptr, NULL);

  //free mem:
  png_destroy_write_struct(&m_png_ptr, &m_info_ptr);
  m_png_ptr = nullptr;
  m_info_ptr = nullptr;

  fclose(m_file);
  m_file = nullptr;
  return true;
}

bool Png_writer_impl::write_file(const std::filesystem::path& path, int w, int h, const char* buffer, int n_bytes)
{
  Png_writer_impl writer;
  if (!writer.create_rgba32(path.c_str(), w, h))
    return false;
  if (!writer.add_rows(h, buffer, n_bytes))
    return false;
  return writer._finalize_write();
}


Png_writer::Png_writer()
{
  m_pimpl = std::make_unique <Png_writer_impl>();
}

Png_writer::~Png_writer()
{
  m_pimpl.reset(nullptr);
}

bool Png_writer::create_rgba32(const std::filesystem::path& file_name, int w, int h)
{
  return m_pimpl->create_rgba32(file_name, w, h);
}

bool Png_writer::add_rows(int n_rows, const char* buffer, int n_bytes)
{
  return m_pimpl->add_rows(n_rows, buffer, n_bytes);
}

bool Png_writer::write_file(const std::filesystem::path& path, int w, int h, const char* buffer, int n_bytes)
{
  return Png_writer_impl::write_file(path, w, h, buffer, n_bytes);
}

// A simple test function to create a gradient image:
namespace test
{
void png_test()
{
  {
    int w = 128;
    int h = 64;
    Png_writer png;
    /*bool ret =*/ png.create_rgba32(I3S_T("c:/temp/~test.png"), 128, 64);
    //create a gradient:
    std::vector< Rgba8 > gradient;
    Rgba8 red(255, 0, 0, 255), green(0, 255, 0, 255);
    create_gradient(h, red, green, &gradient);
    std::vector< Rgba8  > img(w * h);
    // write a vertical gradient in the image:
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        img[y * w + x] = x == y ? Rgba8() : gradient[y];

    //write the png:
    png.add_rows(h, (char*)img.data(), (int)img.size() * sizeof(Rgba8));
  }
}

}  //endof ::test

namespace
{

struct Png_write_struct_deleter
{
  void operator()(png_structp png_write_struct) const
  {
    png_destroy_write_struct(&png_write_struct, NULL);
  }
};

struct Png_info_struct_deleter
{
  explicit Png_info_struct_deleter(png_structp png_write_struct) :
    m_png_write_struct(png_write_struct)
  {}

  void operator()(png_infop png_info_struct) const
  {
    png_destroy_info_struct(m_png_write_struct, &png_info_struct);
  }

private:
  png_structp m_png_write_struct = nullptr;
};

}

I3S_EXPORT bool encode_png(const uint8_t* raw_bytes, int w, int h, bool has_alpha, std::vector<uint8_t>& png_bytes)
{
  png_bytes.clear();

  std::unique_ptr<png_struct, Png_write_struct_deleter> png(
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL));

  std::unique_ptr<png_info, Png_info_struct_deleter> info(
    png_create_info_struct(png.get()), Png_info_struct_deleter(png.get()));

  std::vector<png_bytep> rows(h);
  {
    uint8_t* row = const_cast<uint8_t*>(raw_bytes);
    const auto stride = w * (has_alpha ? 4 : 3);
    for (size_t i = 0; i < static_cast<size_t>(h); i++, row += stride)
      rows[i] = reinterpret_cast<png_bytep>(row);
  }

  const auto write_callback = [](png_structp png, png_bytep bytes, png_size_t count)
  {
    auto& v = *reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png));
    const auto* b = reinterpret_cast<uint8_t*>(bytes);
    v.insert(v.end(), b, b + count);
  };

  // You probably should not declare any C++ objects after the setjump. 
  if (setjmp(png_jmpbuf(png.get())))
    return false;

  png_set_IHDR(png.get(), info.get(), w, h, 8,
    has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_set_rows(png.get(), info.get(), rows.data());
  png_set_write_fn(png.get(), &png_bytes, write_callback, NULL);
  png_write_png(png.get(), info.get(), PNG_TRANSFORM_IDENTITY, NULL);
  return true;
}

} // namespace utl

} // namespace i3slib
