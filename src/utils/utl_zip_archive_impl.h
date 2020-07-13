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
#include <ios>
#include <string>
#include <ios>
#include <stdint.h>
#include "utils/utl_md5.h"

namespace i3slib
{

namespace utl
{

namespace detail
{

I3S_EXPORT Md5::Digest hash_path(std::string* path);

bool _parse_extra_record(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64, uint32_t offset32, uint64_t* out_offset64);

bool parse_extra_record_local_hdr(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64);
bool parse_extra_record_cd_hdr(const char* extra_rec, int extra_rec_size, uint32_t file_size32, uint64_t* out_file_size64, uint32_t offset32, uint64_t* out_offset64);

static const uint32_t c_ones_32 = 0xFFFFFFFF;

static const char* c_hash_table_file_name = "@specialIndexFileHASH128@";

I3S_EXPORT bool reverse_seek(std::istream* in, uint32_t pattern, uint32_t tmp_buff_size = 1024, uint32_t max_retrial = 1);

// --------------------------------------------------------------------------
//    class Hashed_offset
// --------------------------------------------------------------------------

struct Hashed_offset
{
  Hashed_offset() :offset(0) {};
  Hashed_offset(const Md5::Digest& k, uint64_t off) : path_key(k), offset(off) {}
  bool operator<(const Hashed_offset& b) const noexcept;
  Md5::Digest    path_key;
  uint64_t       offset;
};
static_assert(sizeof(Hashed_offset) == 24, "Unexpected size");
//
#ifdef _MSC_VER 
__forceinline 
#else
inline 
#endif
bool Hashed_offset::operator<(const Hashed_offset& other) const noexcept
{
  const uint64_t* a = reinterpret_cast<const uint64_t*>(path_key.data());
  const uint64_t* b = reinterpret_cast<const uint64_t*>(other.path_key.data());
  return a[0] == b[0] ? a[1] < b[1] : a[0] < b[0];
}

class Stream_like
{
public:
  virtual void        seekg(int64_t where, std::ios_base::seekdir = std::ios_base::beg) = 0;
  virtual int64_t     tellg() const = 0;
  virtual bool        fail() const = 0;
  virtual bool        good() const = 0;
  virtual void        read(void* dst, size_t nbytes) = 0;
  virtual int64_t     gcount()const = 0;
};

#pragma pack( push )
#pragma pack( 2 )
struct I3S_EXPORT Local_file_hdr
{
  Local_file_hdr(uint32_t size = 0, uint32_t crc = 0, uint16_t fn_length_ = 0) :
    sig(0x04034b50),
    ver_to_extract(45),
    general_bits(0),
    compression_type(0),
    last_mod_file_time(0),
    last_mod_file_date(0),
    crc32(crc),
    packed_size(size),
    unpacked_size(size),
    fn_length(fn_length_),
    extra_length(0)
    //TODO: date/time 
  {}
  
  bool read(std::istream* in, uint64_t offset, const std::string& actual_path, uint64_t* actual_packed_size);
  bool read(Stream_like* in, uint64_t offset, const std::string& actual_path, uint64_t* actual_packed_size);
  bool write(std::ostream* out, uint64_t offset, const std::string& path, const char* content, int content_size);

  uint32_t sig;
  uint16_t ver_to_extract;
  uint16_t general_bits;
  uint16_t compression_type;
  uint16_t last_mod_file_time;
  uint16_t last_mod_file_date;
  uint32_t crc32;
  uint32_t packed_size;
  uint32_t unpacked_size;
  uint16_t fn_length;
  uint16_t extra_length;
  // ... filename and extra field to follow
};

/*

Zip64 Extended Information Extra Field (0x0001):

The following is the layout of the zip64 extended
information "extra" block. If one of the size or
offset fields in the Local or Central directory
is too small to hold the required data,
a Zip64 extended information record is created.
The order of the fields in the zip64 extended
information record is fixed, but the fields will
only appear if the corresponding Local or Central
directory record field is set to 0xFFFF or 0xFFFFFFFF.

Note: all fields stored in Intel low-byte/high-byte order.

Value             Size       Description
-----             ----       -----------
(ZIP64) 0x0001            2 bytes    Tag for this "extra" block type
Size              2 bytes    Size of this "extra" block

Size Original     8 bytes    Original uncompressed file size

Size Compressed   8 bytes    Size of compressed data
Rel. Hdr Offset   8 bytes    Offset of local header record
Disk Start#       4 bytes    Number of the disk on which this file starts

This entry in the Local header must include BOTH original
and compressed file size fields.
*/
struct Extra_field_64
{
  explicit Extra_field_64(uint64_t n_bytes) : id(1), n_bytes_to_follow(8), size(n_bytes) {}
  uint16_t id;
  uint16_t n_bytes_to_follow;
  uint64_t size;
};

/*
central file header signature      4 bytes  (0x02014b50)
version made by                 2 bytes
version needed to extract       2 bytes
general purpose bit flag        2 bytes
compression method              2 bytes
last mod file time              2 bytes
last mod file date              2 bytes
crc-32                          4 bytes
compressed size                 4 bytes
uncompressed size               4 bytes
file name length                2 bytes
extra field length              2 bytes
file comment length             2 bytes
disk number start               2 bytes
internal file attributes        2 bytes
external file attributes        4 bytes
relative offset of local header 4 bytes

file name (variable size)
extra field (variable size)
file comment (variable size)
*/
struct Cd_hdr_raw
{
  static const uint32_t  k_magic = 0x02014b50;
  Cd_hdr_raw() {}
  explicit Cd_hdr_raw(const Local_file_hdr& h, uint64_t offset) : sig(k_magic), writer_ver(45), ver_to_extract(h.ver_to_extract), general_bits(h.general_bits), compression_type(h.compression_type)
    , last_mod_file_time(h.last_mod_file_time), last_mod_file_date(h.last_mod_file_date), crc32(h.crc32), packed_size(h.packed_size), unpacked_size(h.unpacked_size), fn_length(h.fn_length)
    , extra_length((uint16_t)sizeof(Extra_field_64))
    , comment_length(0)
    , disk0(0)
    , int_attrib(0)
    , ext_attrib(0)
    , rel_offset(detail::c_ones_32) //ALWAYS write offset in extra-field to simplify CDHdr update (re-writting file content)
  {
  }
  bool is_valid() const { return sig == k_magic; }
  uint32_t sig;
  uint16_t writer_ver;
  uint16_t ver_to_extract;
  uint16_t general_bits;
  uint16_t compression_type;
  uint16_t last_mod_file_time;
  uint16_t last_mod_file_date;
  uint32_t crc32;
  uint32_t packed_size;
  uint32_t unpacked_size;
  uint16_t fn_length;
  uint16_t extra_length;
  uint16_t comment_length;
  uint16_t disk0;
  uint16_t int_attrib;
  uint32_t ext_attrib;
  uint32_t rel_offset;
  // .. fileName, extra field and comment to follow.

};

struct Cd_hdr
{
  Cd_hdr_raw raw;
  uint64_t        offset;
  uint64_t        size64;
  std::string     path;
  operator bool() const { return raw.is_valid(); }
  I3S_EXPORT bool read_it(std::istream* in);
  I3S_EXPORT bool read_it(Stream_like* in);
  I3S_EXPORT void write(std::ostream* out);
};

/*
zip64 end of central dir
signature                       4 bytes  (0x06064b50)
size of zip64 end of central
directory record                8 bytes
version made by                 2 bytes
version needed to extract       2 bytes
number of this disk             4 bytes
number of the disk with the
start of the central directory  4 bytes
total number of entries in the
central directory on this disk  8 bytes
total number of entries in the
central directory               8 bytes
size of the central directory   8 bytes
offset of start of central
directory with respect to
the starting disk number        8 bytes
zip64 extensible data sector    (variable size)*/

struct End_of_cd_64
{
  static const uint32_t k_magic = 0x06064b50;
  End_of_cd_64() {}

  End_of_cd_64(uint64_t count, uint64_t cd_size_, uint64_t offset) :
    sig(k_magic), size_this_packet(sizeof(End_of_cd_64) - 12), writer_ver(45), ver_to_extract(45), num_disk(0), disk0(0),
    num_entries_this_disk(count), num_entries_total(count), cd_size(cd_size_), offset_cd(offset)
  {}

  uint32_t sig;
  uint64_t size_this_packet;
  uint16_t writer_ver;
  uint16_t ver_to_extract;
  uint32_t num_disk;
  uint32_t disk0;
  uint64_t num_entries_this_disk;
  uint64_t num_entries_total;
  uint64_t cd_size;
  uint64_t offset_cd;
  //... followed byt extensible data sector
};
/*
4.3.15 Zip64 end of central directory locator

zip64 end of central dir locator
signature                       4 bytes  (0x07064b50)
number of the disk with the
start of the zip64 end of
central directory               4 bytes
relative offset of the zip64
end of central directory record 8 bytes
total number of disks           4 bytes
*/
struct End_of_cd_locator_64
{
  static const uint32_t k_magic = 0x07064b50;
  End_of_cd_locator_64() {}
  explicit End_of_cd_locator_64(uint64_t offset) : sig(k_magic), disk0_cd64(0), offset_to_eocd64(offset), num_disk(1) {}
  uint32_t sig;
  uint32_t disk0_cd64;
  uint64_t offset_to_eocd64;
  uint32_t num_disk;
};
static_assert(sizeof(End_of_cd_locator_64) == 20, "Check packing");
/*
4.3.16  End of central directory record :

end of central dir signature    4 bytes(0x06054b50)
number of this disk             2 bytes
number of the disk with the
start of the central directory  2 bytes
total number of entries in the
central directory on this disk  2 bytes
total number of entries in
the central directory           2 bytes
size of the central directory   4 bytes
offset of start of central
directory with respect to
the starting disk number        4 bytes
.ZIP file comment length        2 bytes
.ZIP file comment(variable size)
*/
struct End_of_cd_legacy
{
  static const uint32_t k_magic = 0x06054b50;
  
  End_of_cd_legacy() {}

  End_of_cd_legacy(uint64_t count, uint64_t cd_size, uint64_t offset) :
    sig(k_magic), num_disk(0), disk0(0), num_entries_this_disk(count > 0xFFFF ? 0xFFFF : (uint16_t)count)
    , num_entries(count > 0xFFFF ? 0xFFFF : (uint16_t)count)
    , size_of_cd(cd_size > (uint64_t)c_ones_32 ? c_ones_32 : (uint32_t)cd_size)
    , offset_to_cd(offset > (uint64_t)c_ones_32 ? c_ones_32 : (uint32_t)offset)
    , comment_length(0) {}

  uint32_t sig;
  uint16_t num_disk;
  uint16_t disk0;
  uint16_t num_entries_this_disk; //or FFFF
  uint16_t num_entries;
  uint32_t size_of_cd;
  uint32_t offset_to_cd;
  uint16_t comment_length;
  // ... comment to follow

};
#pragma pack(pop)

}

}

} // namespace i3slib
