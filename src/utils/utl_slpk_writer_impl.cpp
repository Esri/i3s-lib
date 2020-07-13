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
#include "utils/utl_slpk_writer_api.h"
#include "utils/utl_zip_archive_impl.h"
#include "utils/utl_lock.h"
#include "utils/utl_fs.h"
#include "utils/utl_crc32.h"
#include "utils/utl_io.h"
#include <stdint.h>
#include <vector>
#include <fstream>
#include <algorithm>

namespace i3slib
{

namespace utl
{

namespace detail
{
//-----------------------------------------------------------------------
// class        Slpk_writer_index
//-----------------------------------------------------------------------

//! Store the expected state of an unfinalized SLPK. ( used for sanity check on re-open)
struct Sanity_state
{
  static const int64_t c_magic = 0x77665533;
  int64_t magic = c_magic;
  int64_t slpk_pending_size=0;
  int64_t cd_pending_size=0;
  int64_t entry_count=0;
};

//! Helper class that store the index. Part of the index may on on disk if archive is unfinalized. 
class Slpk_writer_index
{
public:
  typedef int64_t Locator;
  bool    init(const std::filesystem::path& path = {});
  bool    load(const std::filesystem::path& index_path, int64_t slpk_pending_size, int64_t cd_pending_size);
  int64_t   get_count() const { return m_state.entry_count + m_pending.size(); }
  void    add(const Md5::Digest& h, int64_t offset);
  bool    save(int64_t slpk_pending_size, int64_t cd_pending_size);
  void    destroy();
  bool    finalize_index(const char** ptr, int64_t* size);
  int64_t   find_file(std::string* path_in_archive); // returns -1 on failure.
private:
  bool _load_to_sysmem();

private:
  std::fstream                  m_stored;
  std::vector< Hashed_offset >  m_pending;
  Sanity_state                  m_state;
  std::filesystem::path         m_index_path;
  bool                          m_is_sorted = false;
};


bool Slpk_writer_index::init(const std::filesystem::path& path)
{
  m_pending.clear();
  m_state = Sanity_state();
  m_index_path = path;
  m_is_sorted = false;

  if (path.empty())
    return true;

  m_stored = std::fstream(m_index_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
  return m_stored.good();
}

int64_t Slpk_writer_index::find_file( std::string* path_in_archive)
{
  if (!_load_to_sysmem() || m_pending.empty())
    return -1;
  //find it using binary seach:
  //hash the path:
  auto path_hash = detail::hash_path(path_in_archive);

  // do in-place binary search:
  const Hashed_offset* idx0 = &m_pending[0];
  const Hashed_offset* end = m_pending.data() + m_pending.size();
  auto found = end;
  auto key = Hashed_offset(path_hash, ~(uint64_t)0);
  found = std::lower_bound(idx0, end, key);

  if (found == end || found->path_key != path_hash)
    return -1;
  return  found->offset;
}

bool Slpk_writer_index::load(const std::filesystem::path& index_path, int64_t slpk_pending_size, int64_t cd_pending_size)
{
  //open stream:
  m_stored = std::fstream(index_path, std::ios::binary | std::ios::out | std::ios::in);
  // read the sanity state and compare:
  m_stored.seekg(-(int64_t)sizeof(Sanity_state), std::ios::end );
  read_it(&m_stored, &m_state);
  if (m_stored.fail() || m_state.cd_pending_size != cd_pending_size || m_state.slpk_pending_size != slpk_pending_size
      || m_state.c_magic != Sanity_state::c_magic)
  {
    init();
    return false;
  }
  m_stored.seekg(-(int64_t)sizeof(Sanity_state), std::ios::end);
  m_index_path = index_path;
  m_is_sorted = false;
  return m_stored.good() && !m_stored.fail();
}

void Slpk_writer_index::add(const Md5::Digest& h, int64_t offset)
{
  m_pending.emplace_back(h, offset);
  m_is_sorted = false;
}


bool Slpk_writer_index::save(int64_t slpk_pending_size, int64_t cd_pending_size)
{
  if (!m_index_path.empty())
  {
    I3S_ASSERT(m_stored.good());
    if (m_stored.good())
    {
      //update state:
      m_state.cd_pending_size = cd_pending_size;
      m_state.slpk_pending_size = slpk_pending_size;
      m_state.entry_count += m_pending.size();
      m_stored.write(reinterpret_cast<char*>(&m_pending[0]), m_pending.size() * sizeof(Hashed_offset));
      write_it(&m_stored, m_state);
      m_pending.clear();
      return m_stored.good() && !m_stored.fail();
    }
  }
  return false;
}

void Slpk_writer_index::destroy()
{
  //delete artifact on disk if any.
  m_stored.close();
  std::error_code whatnow;
  stdfs::remove(m_index_path, whatnow);
  init();
}

bool Slpk_writer_index::_load_to_sysmem()
{
  if (m_state.entry_count)
  {
    //load it all in sysmem:
    m_stored.seekg(0);
    auto pos = m_pending.size();
    m_pending.resize(pos + m_state.entry_count);
    m_stored.read(reinterpret_cast<char*>(&m_pending[pos]), m_state.entry_count * sizeof(Hashed_offset));
    if (m_stored.fail())
      return false;
    //reset the store:
    m_stored.seekp(0);
    m_stored.seekg(0);
    m_state = Sanity_state();
  }
  if (!m_is_sorted)
  {
    //sort it by hash:
    std::sort(m_pending.begin(), m_pending.end()); //may take a while...

    //check for collision:
    for (size_t i = 1; i < m_pending.size(); ++i)
    {
      if (!(m_pending[i - 1] < m_pending[i]))
      {
        I3S_ASSERT_EXT(false);
        return false;
      }
    }
    m_is_sorted = true;
  }

  return true;
}


bool Slpk_writer_index::finalize_index(const char** ptr, int64_t* size)
{
  *ptr = nullptr;
  *size = 0;

  if (!_load_to_sysmem())
    return false;

  *size = m_pending.size() * sizeof(Hashed_offset);
  if (*size)
    *ptr = reinterpret_cast<char*>(&m_pending[0]);
  return true;
}

//-----------------------------------------------------------------------
// class        Slpk_writer_impl
//-----------------------------------------------------------------------

//! Write a ZIP64 UNCOMPRESSED archive
//! WARNING: Will fail on hash collision on file path. (2 different paths hashing to the same 128bit MD5)
//! NOT optimized for memory usage. will use  24 bytes per file written (16byte hash, 8byte offset). so 1 M files -> 24MB of RAM used.
//! In our use case:
//! - files added to archive are small. ( < few MB )
//! - file paths in archive are ascii.
//! this class is  thread-safe .
class Slpk_writer_impl : public Slpk_writer
{
public:
  DECL_PTR(Slpk_writer_impl);
  Slpk_writer_impl();
  ~Slpk_writer_impl() override;
  // --- Slpk_writer: ---
  virtual bool    create_archive(const std::filesystem::path& path, Create_flags flags) override;
  virtual bool    append_file(const std::string& path_in_archive, const char* buffer, int n_bytes, Mime_type type, Mime_encoding pack) override;
  virtual bool    get_file(const std::string& path_in_archive, std::string* content) override;
  virtual bool    finalize()  override { Lock_guard lk(m_mutex); return _finalize_no_lock(); }
private:
  bool    _is_io_fail();
  bool    _append_file_no_lock(const std::string& path_in_archive, const char* buffer, int n_bytes);
  void    _init() { m_path.clear(); m_ar = std::fstream(); m_tmp = std::fstream(); m_index.init(); m_flags = Create_flag::Overwrite_if_exists_and_auto_finalize; }
  bool    _finalize_no_lock()  ;

  std::filesystem::path     m_path; //Path of the SLPK as provided by caller
  std::fstream              m_ar;
  std::fstream              m_tmp;
  mutable Slpk_writer_index m_index;
  mutable std::mutex        m_mutex;
  Create_flags              m_flags = Create_flag::Overwrite_if_exists_and_auto_finalize;
};



Slpk_writer_impl::Slpk_writer_impl()
{}

Slpk_writer_impl::~Slpk_writer_impl()
{ 
  
  Lock_guard lk(m_mutex);
  if (!m_path.empty())
  {
    if (m_flags & Create_flag::Disable_auto_finalize)
    {
      //need to save the index:
      if (!m_index.save(m_ar.tellp(), m_tmp.tellp()))
      {
        // uh-oh...
        m_index.destroy();
      }
    }
    else
      _finalize_no_lock();
  }
}

bool Slpk_writer_impl::get_file(const std::string& path_in_archive, std::string* content)
{
  Lock_guard lk(m_mutex);
  bool is_found = false;
  auto actual_path = path_in_archive;
  int64_t offset = m_index.find_file( &actual_path);
  if (offset >= 0)
  {
    m_ar.flush();
    // read header::
    detail::Local_file_hdr hdr;
    uint64_t actual_packed_size = 0;
    if (hdr.read(&m_ar, offset, actual_path, &actual_packed_size) && (hdr.packed_size == hdr.unpacked_size))
    {
      //read content:
      content->resize((size_t)actual_packed_size);
      m_ar.read(content->data(), content->size());
      if (!m_ar.fail())
      {    
        //check CRC:
        auto actual_crc = ~crc32_buf(content->data(), content->size());
        is_found = actual_crc == hdr.crc32 || hdr.crc32 == 0;
      }
    }
  }
  //back to the end of the file, ready to append
  m_ar.seekp(0, std::ios::end);
  return is_found;
}

bool  Slpk_writer_impl::_is_io_fail()
{
  return !m_ar.good() || m_ar.fail() || !m_tmp.good() || m_tmp.fail();
}

bool  Slpk_writer_impl::create_archive(const std::filesystem::path& path, Create_flags flags)
{
  Lock_guard lk(m_mutex);
  // reset:
  _init();
  m_flags = flags;
  m_path = path;

  // There's no operator+ for std::filesystem::path, huh. Add one to utl_fs?
  auto slpk_path = path;
  slpk_path += I3S_T(".pending");

  auto tmp_cd_path = path;
  tmp_cd_path += I3S_T(".tmp");

  auto tmp_index_path = path;
  tmp_index_path += I3S_T(".tmpx");

  if (flags & Create_flag::Unfinalized_only)
  {
    //look for an "unfinalized" SLPK:
    m_ar = std::fstream(slpk_path, std::ios::binary | std::ios::out | std::ios::in |std::ios::ate);
    m_tmp = std::fstream(tmp_cd_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::ate);
    if (_is_io_fail() || !m_index.load(tmp_index_path, m_ar.tellp(), m_tmp.tellp()))
    {
      _init();
      return false;
    }
  }
  else
  {
    std::error_code err_code;
    if (file_exists(path))
    {
      // if file exists, attempt to rename it:
      stdfs::remove(path, err_code); // so we can rename it.
      if (err_code)
      {
        _init();
        return false;
      }
    }

    m_ar = std::fstream(slpk_path,    std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
    m_tmp = std::fstream(tmp_cd_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
    if (file_exists(tmp_index_path))
    {
      stdfs::remove(tmp_index_path, err_code);
      if(err_code)
      {
        _init();
        return false;
      }
    }
    if (_is_io_fail() || ( ( m_flags & Create_flag::Disable_auto_finalize ) &&  !m_index.init(tmp_index_path) ))
    {
      _init();
      return false;
    }
  }
  return true;
}


//! WARNING: This function will NOT check for collision here (for performance) so adding the same path multiple times will not fail until FinalizeArchive(). 
//! File extension will be added if type and/or pack are provided.
bool  Slpk_writer_impl::append_file(const std::string& path_in_archive_, const char* buffer, int n_bytes, Mime_type type, Mime_encoding pack)
{
  Lock_guard lk(m_mutex);
  if (m_path.empty() || path_in_archive_.empty())
  {
    return false; //try to append to an already finalized file ?
  }
  auto path_with_ext = path_in_archive_;
  add_slpk_extension_to_path(&path_with_ext, type, pack);
  return _append_file_no_lock(path_with_ext, buffer, n_bytes);
}

bool  Slpk_writer_impl::_append_file_no_lock(const std::string& file_path_raw, const char* buffer, int n_bytes)
{
  auto path_in_archive = file_path_raw;
  const auto path_hash = detail::hash_path(&path_in_archive);

  if (n_bytes <= 0 || !buffer)
    return false;
  //write to archive file:
  uint64_t offset = m_ar.tellp();
  //fill in the header:
  uint32_t crc = ~crc32_buf(buffer, n_bytes);
  detail::Local_file_hdr loc_hdr(n_bytes, crc, (uint16_t)path_in_archive.size());
  write_it(&m_ar, loc_hdr);
  m_ar.write(path_in_archive.data(), path_in_archive.size());
  //write the data:
  m_ar.write(buffer, n_bytes);
  //fill in the CD record:
  detail::Cd_hdr cd_hdr;
  cd_hdr.offset = offset;
  cd_hdr.path = path_in_archive;
  cd_hdr.raw = detail::Cd_hdr_raw(loc_hdr, offset);
  cd_hdr.write(&m_tmp);

  if (_is_io_fail())
    return false;

  m_index.add(path_hash, offset);
  return true;
}

bool Slpk_writer_impl::_finalize_no_lock()
{
  bool is_ok = false;
  if (!m_path.empty())
  {
    const char* src_ptr;
    int64_t size;
    m_index.finalize_index(&src_ptr, &size);
    //if (!_append_file_no_lock(detail::c_hash_table_file_name, reinterpret_cast<const char*>(m_index.data()), (int)(m_index.size() * sizeof(Hashed_offset))))
    if (_append_file_no_lock(detail::c_hash_table_file_name, src_ptr, (int)size))
    {

      uint64_t offset_to_cd = m_ar.tellp();
      uint64_t cd_size = m_tmp.tellp();
      m_tmp.flush();
      m_tmp.seekg(0);
      //copy the tmp CD to the ZIP archive:
      if (copy_stream(&m_tmp, &m_ar, cd_size))
      {
        //add [zip64 end of central directory record]
        uint64_t offset_to_eocd64 = m_ar.tellp();
        detail::End_of_cd_64 eocd64(m_index.get_count(), cd_size, offset_to_cd);
        write_it(&m_ar, eocd64);
        //[zip64 end of central directory locator]
        detail::End_of_cd_locator_64 the_end_64(offset_to_eocd64);
        write_it(&m_ar, the_end_64);
        //[end of central directory record]
        detail::End_of_cd_legacy the_end(m_index.get_count(), cd_size, offset_to_cd);
        write_it(&m_ar, the_end);
        //bool ret = m_ar.good() && !m_ar.fail();
        m_tmp.close();
        std::error_code err_code;

        auto tmp_file_path = m_path;
        tmp_file_path += I3S_T(".tmp");
        stdfs::remove(tmp_file_path);

        auto pending_file_path = m_path;
        pending_file_path += I3S_T(".pending");

        m_index.destroy();
        //rename:
        m_ar.close();
        err_code = {};
        stdfs::rename(pending_file_path, m_path, err_code);
        is_ok = !err_code;
        m_path.clear();
      }
    }
  }
  _init();
  return is_ok;
}

}
}//endof ::utl::detail


utl::Slpk_writer* utl::create_slpk_writer()
{
  return new  detail::Slpk_writer_impl();
}

} // namespace i3slib
