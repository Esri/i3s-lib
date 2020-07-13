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
#include "utils/utl_gzip.h"
#include "utils/utl_geom.h"
#include <stdint.h>
#include <vector>
#include <cstring>

// This code is used by several projects, will try to make their build configs coherent on this.
#if __has_include(<zlib.h>)
#include <zlib.h>
#else
#include <zlib/zlib.h>
#endif

namespace i3slib
{

namespace utl
{
template< class X, class Y > bool compress_gzip(const X* bytes, int n_bytes, Y *dest, int level = MY_DEFAULT_COMPRESSION);
template< class X, class Y > bool uncompress_gzip(const X* bytes, int n_bytes, Y *dest);

template< class T > bool compress_gzip(std::vector<T>& source, std::vector<T>* dest, int level = MY_DEFAULT_COMPRESSION);
template< class T > bool uncompress_gzip(std::vector<T>& source, std::vector<T>* dest);

template< class Source, class Sink > bool compress_gzip2(Source* source, Sink* dest, int level = MY_DEFAULT_COMPRESSION);
template< class Source, class Sink > bool uncompress_gzip2(Source* source, Sink* dest);

int zlib_uncompress(unsigned char* dst, unsigned int dst_size, const unsigned char* src, unsigned int src_size)
{
  unsigned long dst_len = dst_size;
  return uncompress(dst, &dst_len, src, src_size);
}


struct Source_mem
{
  Source_mem(const void* buff, int n_bytes) :
    m_curr(reinterpret_cast< const unsigned char*>(buff)),
    m_end(reinterpret_cast< const unsigned char*>(buff) + n_bytes)
  {}
 
  bool      read_some(unsigned char** cur, unsigned int* n_bytes, bool* is_finish)
  {
    if (m_curr >= m_end)
      return false;
    *cur = const_cast< unsigned char* >(m_curr);
    *n_bytes = (uint32_t)(m_end - m_curr);
    *is_finish = true;
    m_curr += *n_bytes;
    return  true;
  }
  const unsigned char* m_curr, *m_end;
};

//! WARNING cont_t must be a container of contiguous elements ( i.e. vector<T> or basic_string<T> )
template< class Cont_t >
struct Sink_vector
{
  typedef typename Cont_t::value_type T;
  Sink_vector(Cont_t* vec, unsigned int chunk_size = 16 * 1024) :m_vec(vec), m_chunk_size(chunk_size / sizeof(T)), m_curr(0)
  {
    I3S_ASSERT_EXT(m_chunk_size > 16);
  }

  bool      reserve_some(unsigned char** cur, unsigned int* n_bytes)
  {
    m_vec->resize(m_vec->size() + m_chunk_size);
    I3S_ASSERT_EXT(m_curr < sizeof(T)*m_vec->size());
    *cur = byte_at(m_curr);
    *n_bytes = (uint32_t)(m_vec->size() * sizeof(T) - m_curr);
    m_curr += *n_bytes;
    return true; //until memory blows up ;)
  }

  void      rewind(unsigned int n_bytes)
  {
    m_curr -= n_bytes;
    I3S_ASSERT_EXT(n_bytes % sizeof(T) == 0); //otherwise, something's fishy...
    size_t n_elem = n_bytes / sizeof(T); //floor. 
    m_vec->resize(m_vec->size() - n_elem);
  }
  
private:
  unsigned char*         byte_at(size_t i) { return reinterpret_cast<unsigned char*>(&((*m_vec)[0])) + i; }
  Cont_t* m_vec;
  size_t m_chunk_size;
  size_t m_curr; //in bytes
};

template< class T >      bool compress_gzip(std::vector<T>& source, std::vector<T> *dest, int level)
{
  Source_mem src(source.data(), (int)(source.size() * sizeof(T)));
  Sink_vector< std::vector<T> > dst(dest);
  return compress_gzip2(&src, &dst);
}
template< class T >      bool uncompress_gzip(std::vector<T>& source, std::vector<T> *dest)
{
  Source_mem src(source.data(), (uint32_t)(source.size() * sizeof(T)));
  Sink_vector< std::vector<T> > dst(dest);
  return uncompress_gzip2(&src, &dst);
}


template< class T, class Y >      bool compress_gzip(const T* bytes, int n_bytes, Y *dest, int level)
{
  Source_mem src(bytes, n_bytes);
  Sink_vector< Y > dst(dest);
  return compress_gzip2(&src, &dst);
}

template< class T, class Y >      bool uncompress_gzip(const T* bytes, int n_bytes, Y *dest)
{
  Source_mem src(bytes, n_bytes);
  Sink_vector< Y >  dst(dest);
  return uncompress_gzip2(&src, &dst);
}


template< class Source, class Sink > bool compress_gzip2(Source *source, Sink *dest, int level)
{
  int ret, flush;
  z_stream strm;
  std::memset(&strm, 0x00, sizeof(z_stream)); //coverty init. 
  //unsigned char in[CHUNK];
  //unsigned char out[CHUNK];

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  //ret = deflateInit(&strm, level);
  const int mem_level = 8; //the default;
  ret = deflateInit2(&strm, level, Z_DEFLATED, 16 + MAX_WBITS, mem_level, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK)
    return false;

  /* compress until end of file */
  bool is_finish;
  while (source->read_some(&strm.next_in, &strm.avail_in, &is_finish))
  {
    //strm.avail_in = fread(in, 1, CHUNK, source);
    //flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    flush = is_finish ? Z_FINISH : Z_NO_FLUSH;
    //strm.next_in = in;

    /* run deflate() on input until output buffer not full, finish
    compression if all of source has been read in */
    //do {
    //  strm.avail_out = CHUNK;
    //  strm.next_out = out;

    //while (dest->Next( &strm.next_out, &strm.avail_out ))
    do
    {
      dest->reserve_some(&strm.next_out, &strm.avail_out);
      ret = deflate(&strm, flush);    /* no bad return value */
      I3S_ASSERT_EXT(ret != Z_STREAM_ERROR);  /* state not clobbered */
                                              //I3S_ASSERT_EXT((int)avail - (int)strm.avail_out >= 0);
                                              //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                                              //  (void)deflateEnd(&strm);
                                              //  return Z_ERRNO;
                                              //}
    } while (strm.avail_out == 0);
    //rewind so we don't loose the left over:
    dest->rewind(strm.avail_out);

    /* done when last data in file processed */
  };
  I3S_ASSERT_EXT(ret == Z_STREAM_END || ret == Z_OK);        /* stream will be complete */

                                                             /* clean up and return */
  (void)deflateEnd(&strm);
  return true;
}

bool compress_gzip(const std::string& source, std::string* out, int level)
{
  Source_mem src(source.data(), (int)(source.size()));
  Sink_vector< std::string > dst(out);
  return compress_gzip2(&src, &dst);
}

bool compress_gzip(const char* ptr, int src_size, std::string* out, int level)
{
  Source_mem src(ptr, src_size);
  Sink_vector< std::string > dst(out);
  return compress_gzip2(&src, &dst);
}


bool uncompress_gzip(const std::string& source, std::string* out)
{
  Source_mem src(source.data(), (int)(source.size()));
  Sink_vector< std::string > dst(out);
  auto ret = uncompress_gzip2(&src, &dst);
  //uint32_t s1 = *reinterpret_cast<const uint32_t*>( &source[ source.size() - sizeof(int)] );
  return ret;
}

/* Decompress from file source to file dest until stream ends or EOF.
inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
allocated for processing, Z_DATA_ERROR if the deflate data is
invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
the version of the library linked do not match, or Z_ERRNO if there
is an error reading or writing the files. */

template< class Source, class Sink > bool uncompress_gzip2(Source *source, Sink *dest)
{
  int ret;
  z_stream strm;
  //unsigned char in[CHUNK];
  //unsigned char out[CHUNK];

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit2(&strm, 16 + MAX_WBITS);
  if (ret != Z_OK)
    return false;

  /* decompress until deflate stream ends or end of file */
  //do {
  //  strm.avail_in = fread(in, 1, CHUNK, source);
  //  if (ferror(source)) {
  //    (void)inflateEnd(&strm);
  //    return Z_ERRNO;
  //  }
  //  if (strm.avail_in == 0)
  //    break;
  //  strm.next_in = in;
  bool is_finish;
  while (source->read_some(&strm.next_in, &strm.avail_in, &is_finish))
  {
    /* run inflate() on input until output buffer not full */
    //do {
    //  strm.avail_out = CHUNK;
    //  strm.next_out = out;
    do
    {
      dest->reserve_some(&strm.next_out, &strm.avail_out);
      ret = inflate(&strm, Z_NO_FLUSH);
      I3S_ASSERT_EXT(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;     /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void)inflateEnd(&strm);
          return false/*ret*/;
      }
      //have = CHUNK - strm.avail_out;
      //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
      //  (void)inflateEnd(&strm);
      //  return Z_ERRNO;
      //}
    } while (strm.avail_out == 0);
    //rewind so we don't loose the left over:
    dest->rewind(strm.avail_out);
    if ((ret == Z_STREAM_END) != is_finish)
    {
      return false; //stream truncated !?
    }
  }
  //  /* done when inflate() says it's done */
  //} while (ret != Z_STREAM_END);
  I3S_ASSERT_EXT(ret == Z_STREAM_END);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return ret == Z_STREAM_END;
}

}

} // namespace i3slib
