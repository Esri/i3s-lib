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
// Direct method from zlib (which uses different stream headers). 
// This is use by decode Jpeg "shadow" alpha channel for instance. 
// **WARNING**: newer code should not use this method.
int zlib_uncompress(unsigned char* dst, uint32_t dst_size, const unsigned char* src, uint32_t src_size)
{
  unsigned long dst_len = dst_size;
  return uncompress(dst, &dst_len, src, src_size);
}


struct Source_mem
{
  Source_mem(const void* buff, int nbytes)
    : m_size(nbytes)
    , m_curr(reinterpret_cast<const unsigned char*>(buff))
    , m_end(reinterpret_cast<const unsigned char*>(buff) + nbytes)
  {}

  int     total_size() const { return m_size; }
  bool    read_some(unsigned char** cur, uint32_t* nBytes, bool* isFinish)
  {
    if (m_curr >= m_end)
      return false;
    *cur = const_cast<unsigned char*>(m_curr);
    *nBytes = (uint32_t)(m_end - m_curr);
    *isFinish = true;
    m_curr += *nBytes;
    return  true;
  }
  int m_size;
  const unsigned char* m_curr, * m_end;
};

struct Fixed_size_sink_trait
{
  explicit Fixed_size_sink_trait(int& size_in_out) : m_size_in_out(size_in_out) {}
  bool  reserve_some(char* dst, unsigned char** cur, uint32_t* nbytes, int )
  {
    if (reserve_count)
      return false; // no more.
    // all in:
    *cur = reinterpret_cast<unsigned char*>(dst);
    *nbytes = m_size_in_out;
    ++reserve_count;
    return true;
  }
  void  rewind(char* dst, uint32_t nbytes)
  {
    m_size_in_out -= std::min((int)nbytes, m_size_in_out);
  }
  int& m_size_in_out;
  int  reserve_count{ 0 };
};

namespace gzip
{

namespace
{

struct Monotonic_allocator
{
  // no copy
  Monotonic_allocator(Monotonic_allocator const&) = delete;
  void operator=(Monotonic_allocator const&) = delete;

  Monotonic_allocator() = default;

  // Must be called once at most
  void set_mem(std::vector<uint8_t>& mem)
  {
    I3S_ASSERT_EXT(!m_mem);  // because set_mem is called once at most

    if (mem.capacity() == 0)
    {
      m_next = nullptr;
      m_begin = nullptr;
      m_end = nullptr;
    }
    else
    {
      m_next = mem.data();
      m_begin = mem.data();
      m_end = mem.data() + mem.capacity();
    }
    m_mem = &mem;
  }

  ~Monotonic_allocator()
  {
    // To benefit use cases where we repeatedly compress / uncompress the same amount of data,
    // we adjust the size of m_mem based on the value of m_sz_failed_allocs.

    try
    {
      if (size_failed_allocs())
      {
        I3S_ASSERT_EXT(m_mem);
        const size_t ideal_capacity = count_used_bytes() + size_failed_allocs();
        m_mem->reserve(ideal_capacity);
      }
    }
    catch (const std::exception &)
    {
      // coverity: reserve could throw : https://en.cppreference.com/w/cpp/memory/new/bad_array_new_length
      I3S_ASSERT_EXT(false);
    }
  }

  uint8_t* try_alloc(const size_t requested_bytes)
  {
    if (m_mem)
    {
      if (m_next + requested_bytes <= m_end)
      {
        uint8_t* ret = m_next;
        m_next += requested_bytes;
        return ret;
      }
      m_sz_failed_allocs += requested_bytes;
    }
    return nullptr;
  }

  bool owns(void* ptr) const { 
    return ptr >= m_begin && ptr < m_end;
  }

private:
  uint8_t* m_next = nullptr;
  const uint8_t* m_begin = nullptr;
  const uint8_t* m_end = nullptr;
  std::vector<uint8_t>* m_mem = nullptr;

  size_t m_sz_failed_allocs = 0;

  size_t size_failed_allocs() const
  {
    return m_sz_failed_allocs;
  }

  size_t count_used_bytes() const
  {
    return m_next - m_begin;
  }

  size_t total_bytes() const
  {
    return m_end - m_begin;
  }
};

}

void setup_stream_allocator(z_stream & strm, Monotonic_allocator & alloc, std::vector<uint8_t> & scratch_mem)
{
  alloc.set_mem(scratch_mem);

  strm.opaque = &alloc;
  strm.zalloc = [](void* q, unsigned n, unsigned m) -> void*
  {
    Monotonic_allocator* alloc = static_cast<Monotonic_allocator*>(q);
    const size_t count_needed_bytes = (size_t)n * m;
    if (uint8_t* ptr = alloc->try_alloc(count_needed_bytes))
    {
      I3S_ASSERT(alloc->owns(ptr));
      return ptr;
    }
    return new uint8_t[count_needed_bytes];
  };
  strm.zfree = [](void* q, void* ptr) {
    const Monotonic_allocator* alloc = static_cast<const Monotonic_allocator*>(q);
    if (!alloc->owns(ptr))
      delete [] static_cast<uint8_t*>(ptr);
  };
}
}

template< class Sink, class Sink_trait > bool encode_gzip_tpl(std::vector<uint8_t> * scratch_alloc, Source_mem&& src, Sink* dst, Sink_trait&& trait, int level)
{
  using gzip::Monotonic_allocator;
  using gzip::setup_stream_allocator;

  int ret;
  z_stream strm;
  std::memset(&strm, 0x00, sizeof(z_stream)); //coverty init. 

  Monotonic_allocator alloc;
  if (scratch_alloc)
    setup_stream_allocator(strm, alloc, *scratch_alloc);

  // allocate deflate state
  const int memLevel = 8; //the default;
  ret = deflateInit2(&strm, level, Z_DEFLATED, 16 + MAX_WBITS, memLevel, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK)
    return false;

  bool is_finish=false;
  //set the entire input:
  src.read_some(&strm.next_in, &strm.avail_in, &is_finish);
  I3S_ASSERT(is_finish);
  {
    do
    {
      if (strm.avail_out == 0)
      {
        if (!trait.reserve_some(dst, &strm.next_out, &strm.avail_out, src.total_size()))
        {
          break; //alloc failure.
        }
      }
      ret = deflate(&strm, Z_FINISH);    /* no bad return value */
      I3S_ASSERT_EXT(ret != Z_STREAM_ERROR);  /* state not clobbered */
    } while (ret == Z_OK || ret == Z_BUF_ERROR);
  };
  if (ret == Z_STREAM_END)
    trait.rewind(dst, strm.avail_out);
  // clean up and return
  (void)deflateEnd(&strm);
  return ret == Z_STREAM_END;
}

struct String_sink_trait
{
  static bool  reserve_some(std::string* str, unsigned char** cur, uint32_t* nbytes, int src_size_hint)
  {
    auto pos = str->size();

    size_t chunk_size = std::max( src_size_hint -(int)pos, 4096 );
    str->resize(str->size() + chunk_size);
    *cur = reinterpret_cast<unsigned char*>(&str->at(pos)); 
    *nbytes = (uint32_t)str->size() - (uint32_t)pos;
    return true; //until memory blows up ;)
  }
  static void  rewind(std::string* str, uint32_t nbytes)
  {
    str->resize(str->size() >= nbytes ? str->size() - nbytes : 0);
  }
};
bool compress_gzip(const std::string& in, std::string* out, int level)
{
  out->resize(0); 
  return encode_gzip_tpl(nullptr, Source_mem(in.data(), (int)in.size()), out, String_sink_trait(), level);
}
bool compress_gzip(const char* src, int src_size, std::string* out, int level)
{
  out->resize(0);
  return encode_gzip_tpl(nullptr, Source_mem(src, src_size), out, String_sink_trait(), level);
}

bool compress_gzip(const char* src, int src_size, char* dst, int& dst_size_in_out,  int level)
{
  return encode_gzip_tpl(nullptr, Source_mem(src, src_size), dst, Fixed_size_sink_trait(dst_size_in_out), level);
}

bool compress_gzip(const std::string& in, std::string* out, std::vector<uint8_t>& tmp_buffer, int level)
{
  out->resize(0);
  return encode_gzip_tpl(&tmp_buffer, Source_mem(in.data(), (int)in.size()), out, String_sink_trait(), level);
}

/* Decompress from file source to file dest until stream ends or EOF.
inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
allocated for processing, Z_DATA_ERROR if the deflate data is
invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
the version of the library linked do not match, or Z_ERRNO if there
is an error reading or writing the files. */

template< class Sink, class Sink_trait > bool decode_gzip_tpl(std::vector<uint8_t>* scratch_alloc, Source_mem& src, Sink* dst, Sink_trait& trait)
{
  using gzip::Monotonic_allocator;
  using gzip::setup_stream_allocator;

  int ret;
  z_stream strm;
  memset(&strm, 0, sizeof(z_stream));

  Monotonic_allocator alloc;
  if (scratch_alloc)
    setup_stream_allocator(strm, alloc, *scratch_alloc);

  ret = inflateInit2(&strm, 16 + MAX_WBITS);
  if (ret != Z_OK)
    return false;

  bool is_finish=false;

  //set the entire input:
  src.read_some(&strm.next_in, &strm.avail_in, &is_finish);
  I3S_ASSERT(is_finish);
  do
  {
    if (strm.avail_out == 0)
    {
      if (!trait.reserve_some(dst, &strm.next_out, &strm.avail_out, src.total_size()))
      {
        break; //alloc failure
      } 
    }
    ret = inflate(&strm, Z_NO_FLUSH);
    I3S_ASSERT_EXT(ret != Z_STREAM_ERROR);  /* state not clobbered */
  } while (ret == Z_OK);
  if (ret == Z_STREAM_END)
    trait.rewind(dst, strm.avail_out);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return ret == Z_STREAM_END;
}

bool uncompress_gzip_maybe_monotonic(const std::string& in, std::string* out, std::vector<uint8_t>* ptr_tmp_buffer)
{
  out->clear();
  Source_mem srcmem(in.data(), (int)in.size());
  String_sink_trait trait;
  return decode_gzip_tpl(ptr_tmp_buffer, srcmem, out, trait);
}

bool uncompress_gzip(const std::string& in, std::string* out)
{
  return uncompress_gzip_maybe_monotonic(in, out, nullptr);
}

bool uncompress_gzip_monotonic(const std::string& in, std::string* out, std::vector<uint8_t>& tmp_buffer)
{
  return uncompress_gzip_maybe_monotonic(in, out, &tmp_buffer);
}


bool uncompress_gzip_maybe_monotonic(const char* src, int src_size, char* dst, int& dst_size_in_out, std::vector<uint8_t>* ptr_tmp_buffer)
{
  Source_mem srcmem(src, src_size);
  Fixed_size_sink_trait trait(dst_size_in_out);
  return decode_gzip_tpl(ptr_tmp_buffer, srcmem, dst, trait);
}

bool uncompress_gzip(const char* src, int src_size, char* dst, int& dst_size_in_out)
{
  return uncompress_gzip_maybe_monotonic(src, src_size, dst, dst_size_in_out, nullptr);
}

bool uncompress_gzip_monotonic(const char* src, int src_size, char* dst, int& dst_size_in_out, std::vector<uint8_t>& tmp_buffer)
{
  return uncompress_gzip_maybe_monotonic(src, src_size, dst, dst_size_in_out, &tmp_buffer);
}


}// utl
} // namespace i3slib
