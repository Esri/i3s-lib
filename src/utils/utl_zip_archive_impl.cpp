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
#include "utils/utl_zip_archive_impl.h"
#include "utils/utl_io.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_crc32.h"
#include <stdint.h>
#include <cstring>
#include <vector>

//-----------------------------------------------------------------------
// definitions:   ::detail
// @see https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
//-----------------------------------------------------------------------

namespace i3slib
{

namespace utl
{

namespace detail
{
static void clean_path(std::string*); //forward decl

//static const char* kIndexFileName = "@specialIndexFileHASH128@";

// ---- utilities:

//helper fcts: (implement your own java version). Zip specs requires little-endianness for the extra-header fields.
uint16_t  _read_uint16(const char* bytes) { return *reinterpret_cast<const uint16_t*>(bytes); }
uint64_t  _read_uint64(const char* bytes) { return *reinterpret_cast<const uint64_t*>(bytes); }


//! written to be easily ported to Java :)
bool _parse_extra_record(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64, uint32_t offset32, uint64_t* out_offset64)
{
  //constants:
  const uint32_t c_overflow = 0xFFFFFFFF;
  const int c_block_hdr_size = 4;
  const int c_zip_64_block_type = 1;

  *out_offset64 = offset32;
  *out_file_size64 = file_size32;
  if (offset32 != c_overflow && file_size32 != c_overflow)
  {
    return true; // no extra block needed. both < 2^32-1
  }

  int iter = 0;
  while (iter < extra_rec_size - c_block_hdr_size)
  {
    //read the block header:
    uint16_t block_type = _read_uint16(&extra_rec[iter]);
    iter += 2;
    uint16_t block_size = _read_uint16(&extra_rec[iter]);
    iter += 2;
    if (block_type == c_zip_64_block_type)
    {
      if (file_size32 == c_overflow)
      {
        //size if over 4GB, so block must be at least 8+8=16 bytes:
        if (block_size < 16)
          return false;
        uint64_t uncompressed_size = _read_uint64(&extra_rec[iter]);
        iter += 8;
        uint64_t compressed_size = _read_uint64(&extra_rec[iter]);
        iter += 8;
        if (uncompressed_size != compressed_size)
          return false; // ZIP archive MUST be STORE only (in our case)
        *out_file_size64 = compressed_size;
      }
      if (offset32 == c_overflow)
      {
        //offset if >= 2^32, need to read it now:
        if (block_size < 8)
          return false; // block must at least be 8 byte long.
        *out_offset64 = _read_uint64(&extra_rec[iter]);
      }
      //we found our record(s). we're done:
      return true;
    }
    else
    {
      //not the block we're looking for. let's skip it:
      iter += block_size;
    }
  }
  return false; //zip-64 block is missing but needed: error.
}

//! use this fct for "local header" where there is no offset:
bool parse_extra_record_local_hdr(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64)
{
  uint64_t dont_care;
  return _parse_extra_record(extra_rec, extra_rec_size, file_size32, out_file_size64, 0, &dont_care);
}

//! use this fct for "Central-directory" header:
bool parse_extra_record_cd_hdr(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64, uint32_t offset32, uint64_t* out_offset64)
{
  return _parse_extra_record(extra_rec, extra_rec_size, file_size32, out_file_size64, offset32, out_offset64);
}

template< class Stream >
bool read_tpl(Local_file_hdr& hdr, Stream* in, uint64_t offset, const std::string& actual_path, uint64_t* actual_packed_size)
{
  in->seekg(offset);
  //read the file hdr:
  in->read(reinterpret_cast<char*>(&hdr), sizeof(Local_file_hdr));
  if( in->fail() || hdr.fn_length > 2048)
    return false;
  //read the file path for sanity:
  std::string path;
  path.resize(hdr.fn_length);

  in->read(path.data(), path.size());
  detail::clean_path(&path);
  if (actual_path != path)
  {
    I3S_ASSERT_EXT(false); //corrupted/invalid hash file ? 
    return false;
  }
  *actual_packed_size = hdr.packed_size;
  if (hdr.packed_size == detail::c_ones_32)
  {
    //parse the extra record (in case file size > 4GB )
    std::vector<char> extra_buffer(hdr.extra_length);
    in->read(extra_buffer.data(), extra_buffer.size());
    detail::parse_extra_record_local_hdr(extra_buffer.data(), (int)extra_buffer.size(), hdr.packed_size, actual_packed_size);
  }
  else if (hdr.extra_length)
  {
    //skip extra:
    in->seekg((uint64_t)in->tellg() + (uint64_t)hdr.extra_length); //skip
  }

  return !in->fail();
}

bool Local_file_hdr::read(std::istream* in, uint64_t offset, const std::string& actual_path, uint64_t* actual_packed_size) 
{ 
  return read_tpl( *this, in, offset, actual_path, actual_packed_size); 
}

bool Local_file_hdr::read(Stream_like* in, uint64_t offset, const std::string& actual_path, uint64_t* actual_packed_size)
{
  return read_tpl(*this, in, offset, actual_path, actual_packed_size); 
}

bool Local_file_hdr::write(std::ostream* out, uint64_t offset, const std::string& path, const char* content, int content_size)
{
  out->seekp(offset);
  this->fn_length = (uint16_t)path.size();
  this->packed_size = content_size;
  this->unpacked_size = content_size;
  this->extra_length = 0;
  this->crc32 = ~crc32_buf(content, content_size);

  //read the file hdr:
  write_it(out, *this);
  //write the file path 
  out->write(path.data(), path.size());
  //write the content:
  out->write(content, content_size);
  return !out->fail();
}


template< class Stream_t > 
bool  read_it_tpl(Stream_t& in, Cd_hdr& out )
{
  if (!utl::read_it(&in, &out.raw) || !out.raw.is_valid())
    return false;
  int64_t to_skip = out.raw.extra_length + out.raw.comment_length;
  //get filename:
  out.path.resize(out.raw.fn_length);
  in.read(&out.path[0], out.raw.fn_length);
  out.offset = out.raw.rel_offset;
  out.size64 = out.raw.packed_size;
  if (out.raw.rel_offset == detail::c_ones_32 || out.raw.packed_size == detail::c_ones_32)
  {
    if (!out.raw.extra_length)
      return false; //expect extra record!
    std::vector<char> extra_buffer(out.raw.extra_length);
    in.read(extra_buffer.data(), extra_buffer.size());
    to_skip -= out.raw.extra_length; //just skip "comment" now.
    if (!detail::parse_extra_record_cd_hdr(extra_buffer.data(), (int)extra_buffer.size()
                                           , out.raw.packed_size, &out.size64, out.raw.rel_offset, &out.offset))
      return false;
  }

  //skip the rest:
  if(to_skip)
    in.seekg(to_skip, std::ios_base::cur);
  return true;
}

bool   Cd_hdr::read_it(std::istream* in) { return read_it_tpl(*in, *this); }
bool   Cd_hdr::read_it(Stream_like* in) { return read_it_tpl(*in, *this); }


void Cd_hdr::write(std::ostream* out)
{
  //detail::Cd_hdr_raw cdHdr(locHdr, offset);
  write_it(out, raw);
  out->write(path.data(), path.size());
  if (raw.rel_offset == detail::c_ones_32)
  {
    // write length64 extra record:
    auto extra = detail::Extra_field_64(offset);
    write_it(out, extra);
  }
}




static void  clean_path(std::string* path)
{
  //convert slashes:
  std::replace(path->begin(), path->end(), '\\', '/');
  //strip front '/':
  (*path) = (*path).size() && (*path)[0] == '/' ? (*path).substr(1) : (*path);
  // --- ISSUE: For legacy reason, we can't create hash with lower-case. 
  // TODO: when "upgrating" the hash file -> use lower-case. (writer/reader have to be consistent !)
  //to lower case ( path may only be ASCII!)
  //std::transform(path->begin(), path->end(), path->begin(), ::tolower);
}

//! Archive Path are ASCII only and case insensitive and do not start with a '/'. 
//! WARNING: This fct doesn't check for relative path substring ( '..', './') nor invalid path characters.
Md5::Digest hash_path(std::string* path)
{
  clean_path(path);

  Md5::Digest hash;
  Md5::hash(reinterpret_cast<const uint8_t*>(path->data()), path->size(), hash);
  return hash;
}

}

}//endof ::utl::Detail

} // namespace i3slib
