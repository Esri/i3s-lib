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
#include "utils/utl_slpk_writer_factory.h"
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
// Free  functions
//-----------------------------------------------------------------------

//! stream-to-stream copy using a temp buffer
template< class Output_stream_t >
static bool  copy_stream(std::istream* src, Output_stream_t* dest, uint64_t n_bytes)
{
  std::vector< char > buff(1024 * 1024);
  while (n_bytes > 0 && src->good() && dest->good())
  {
    auto n = std::min((uint64_t)buff.size(), n_bytes);
    src->read(buff.data(), n);
    dest->write(buff.data(), n);
    n_bytes -= n;
  }
  return !src->fail() && dest->good() && !dest->fail();
}

//-----------------------------------------------------------------------
// class Scoped_temp_folder
//-----------------------------------------------------------------------

class Scoped_temp_folder
{
public:
  Scoped_temp_folder() {}
  ~Scoped_temp_folder()
  {
    _delete_all();
  }
  Scoped_temp_folder& operator=(const Scoped_temp_folder&) = delete;
  Scoped_temp_folder(const Scoped_temp_folder&) = delete;
  void clear() { _delete_all(); }
  bool create(std::filesystem::path prefix)
  {
    _delete_all();
    m_path = create_temporary_folder_path(prefix);
    return !m_path.empty();
  }
  const std::filesystem::path& get_path() const { return m_path; }
  void _delete_all()
  {
    if (m_path.empty())
      return;
    std::error_code ec;
    stdfs::remove_all(m_path, ec);
    m_path.clear();
    I3S_ASSERT(!ec);
  }
  operator bool() const { return !m_path.empty(); }
private:
  std::filesystem::path m_path;
};

//-----------------------------------------------------------------------
// class Output_stream_std
//-----------------------------------------------------------------------
class  Output_stream_std : public Output_stream
{
public:
  DECL_PTR(Output_stream);
  Output_stream_std(std::filesystem::path path, std::ios_base::openmode mode) : m_ofs(path, mode) {}
  virtual int64_t  tellp() override { return m_ofs.tellp(); }
  virtual void     close() noexcept override { return m_ofs.close(); }
  virtual bool     fail() const override { return m_ofs.fail(); }
  virtual bool     good() const override { return m_ofs.good(); }
  virtual Output_stream& write(const char* raw_bytes, std::streamsize count) override { m_ofs.write(raw_bytes, count); return *this; }
private:
  std::ofstream m_ofs;
};
//-----------------------------------------------------------------------
// class        Slpk_writer_index
//-----------------------------------------------------------------------

//! Store the expected state of an unfinalized SLPK. ( used for sanity check on re-open)
struct Sanity_state
{
  static const int64_t c_magic = 0x77665533;
  int64_t magic = c_magic;
  int64_t slpk_pending_size = 0;
  int64_t cd_pending_size = 0;
  int64_t entry_count = 0;
};

//! Helper class that store the index. Part of the index may on on disk if archive is unfinalized. 
class Slpk_writer_index
{
public:
  typedef int64_t Locator;
  bool    init_index(const std::filesystem::path& path = {});
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


bool Slpk_writer_index::init_index(const std::filesystem::path& path)
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

int64_t Slpk_writer_index::find_file(std::string* path_in_archive)
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
  m_stored.seekg(-(int64_t)sizeof(Sanity_state), std::ios::end);
  read_it(&m_stored, &m_state);
  if (m_stored.fail() || m_state.cd_pending_size != cd_pending_size || m_state.slpk_pending_size != slpk_pending_size
    || m_state.magic != Sanity_state::c_magic)
  {
    init_index();
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
  if (!m_index_path.empty())
  {
    //delete artifact on disk if any.
    m_stored.close();
    std::error_code whatnow;
    stdfs::remove(m_index_path, whatnow);
    init_index();
  }
  //else
  //  I3S_ASSERT(!m_stored.is_open() && m_pending.empty() && m_is_sorted == false);
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
    for (size_t i = 1, sz = m_pending.size(); i < sz; ++i)
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
class Slpk_writer_impl final : public Slpk_writer
{
public:
  DECL_PTR(Slpk_writer_impl);
  Slpk_writer_impl();
  ~Slpk_writer_impl() override;
  // --- Slpk_writer: ---
  virtual bool    create_archive(const std::filesystem::path& path, Create_flags flags) override;
  virtual bool    create_stream(Output_stream::ptr strm) override;
  virtual bool    append_file(const std::string& path_in_archive, const char* buffer, int n_bytes, Mime_type type, Mime_encoding pack) override;
#if 0 // deprecated TBD
  virtual bool    get_file(const std::string& path_in_archive, std::string* content) override;
#endif
  virtual bool    finalize()  override { Lock_guard lk(m_mutex); return _finalize_no_lock(); }
  virtual bool    cancel() noexcept override { Lock_guard lk(m_mutex); return _cancel_no_lock(); }
  virtual bool    close_unfinalized() noexcept override { Lock_guard lk(m_mutex); return _close_unfinalized_no_lock(); }
private:
  [[ nodiscard ]]
  bool    _is_io_fail();
  // returns an offset to the file in archive.
  [[ nodiscard ]]
  uint64_t _append_file_no_lock(std::string && clean_path_in_archive, const char* buffer, size_t n_bytes, const uint32_t crc)noexcept;

  void    _init() { m_path.clear(); m_ar = nullptr; m_tmp = std::fstream(); m_index.init_index(); m_flags = Create_flag::Overwrite_if_exists_and_cancel_in_destructor; }
  bool    _finalize_no_lock() noexcept;
  bool    _cancel_no_lock() noexcept;
  bool    _close_unfinalized_no_lock() noexcept;
  bool    _create_archive_no_lock(const std::filesystem::path& path, Create_flags flags, Output_stream::ptr strm);

  std::filesystem::path     m_path; //Path of the SLPK as provided by caller
  //std::fstream              m_ar;
  Output_stream::ptr        m_ar;
  std::fstream              m_tmp;
  mutable Slpk_writer_index m_index;
  mutable std::mutex        m_mutex;
  Create_flags              m_flags = Create_flag::Overwrite_if_exists_and_cancel_in_destructor;
  // must be last member:
  Scoped_temp_folder        m_temp_folder_for_stream_artifacts; // not use for regular filesystem slpk archives.
};



Slpk_writer_impl::Slpk_writer_impl()
{}

Slpk_writer_impl::~Slpk_writer_impl()
{
  Lock_guard lk(m_mutex);
  if (m_path.empty())
    return;

  if (m_flags & Create_flag::On_destruction_keep_unfinalized_to_reopen)
  {
    _close_unfinalized_no_lock();
  }
  else
    _cancel_no_lock();
}

#if 0 //deprecated TBD
bool Slpk_writer_impl::get_file(const std::string& path_in_archive, std::string* content)
{
  Lock_guard lk(m_mutex);
  bool is_found = false;
  auto actual_path = path_in_archive;
  int64_t offset = m_index.find_file(&actual_path);
  if (offset >= 0)
  {
    m_ar->flush();
    // read header::
    detail::Local_file_hdr hdr;
    uint64_t actual_packed_size = 0;
    if (hdr.read(&m_ar, offset, &actual_packed_size, &actual_path) && (hdr.packed_size == hdr.unpacked_size))
    {
      //read content:
      content->resize((size_t)actual_packed_size);
      m_ar->read(content->data(), content->size());
      if (!m_ar->fail())
      {
        //check CRC:
        auto actual_crc = ~crc32_buf(content->data(), content->size());
        is_found = actual_crc == hdr.crc32 || hdr.crc32 == 0;
      }
    }
  }
  //back to the end of the file, ready to append
  m_ar->seekp(0, std::ios::end);
  return is_found;
}
#endif

bool  Slpk_writer_impl::_is_io_fail()
{
  if (!m_ar)
  {
    I3S_ASSERT(m_ar);
    return false;
  }
  return !m_ar->good() || m_ar->fail() || !m_tmp.good() || m_tmp.fail();
}

bool Slpk_writer_impl::create_stream(Output_stream::ptr strm)
{
  Lock_guard lk(m_mutex);
  // reset:
  _init();

  if (!m_temp_folder_for_stream_artifacts.create("slpk_writer"))
    return false;

  auto path = m_temp_folder_for_stream_artifacts.get_path() / "placeholder.slpk";

  return _create_archive_no_lock(path, Create_flag::Overwrite_if_exists_and_cancel_in_destructor, strm);
}

bool  Slpk_writer_impl::create_archive(const std::filesystem::path& path, Create_flags flags)
{
  Lock_guard lk(m_mutex);
  // reset:
  _init();
  m_temp_folder_for_stream_artifacts.clear();

  return _create_archive_no_lock(path, flags, nullptr);
}

bool  Slpk_writer_impl::_create_archive_no_lock(const std::filesystem::path& path, Create_flags flags, Output_stream::ptr strm)
{

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
    I3S_ASSERT(!strm);
    //look for an "unfinalized" SLPK:
    //m_ar = std::ofstream(slpk_path, std::ios::binary | std::ios::out | std::ios::in |std::ios::ate);
    m_ar = std::make_shared< Output_stream_std>(slpk_path, std::ios::binary | std::ios::out | std::ios::ate | std::ios::app);
    m_tmp = std::fstream(tmp_cd_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::ate);
    if (_is_io_fail() || !m_index.load(tmp_index_path, m_ar->tellp(), m_tmp.tellp()))
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

    //m_ar = std::fstream(slpk_path,    std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
    if (strm)
      m_ar = strm;
    else
      m_ar = std::make_shared< Output_stream_std>(slpk_path, std::ios::binary | std::ios::out | std::ios::trunc);
    m_tmp = std::fstream(tmp_cd_path, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
    if (file_exists(tmp_index_path))
    {
      stdfs::remove(tmp_index_path, err_code);
      if (err_code)
      {
        _init();
        return false;
      }
    }
    if (_is_io_fail() || ((m_flags & Create_flag::On_destruction_keep_unfinalized_to_reopen) && !m_index.init_index(tmp_index_path)))
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
  if (path_in_archive_.empty() || n_bytes <= 0 || !buffer)
  {
    return false;
  }
  std::string path_in_archive = path_in_archive_;
  add_slpk_extension_to_path(&path_in_archive, type, pack);
  const auto path_hash = detail::hash_path(&path_in_archive);
  const uint32_t crc = ~crc32_buf(buffer, n_bytes);

  Lock_guard lk(m_mutex);
  if (m_path.empty())
  {
    return false; //try to append to an already finalized file ?
  }
  auto offset = _append_file_no_lock(std::move(path_in_archive), buffer, n_bytes, crc);
  if (_is_io_fail())
    return false;
  m_index.add(path_hash, offset);
  return true;
}

uint64_t Slpk_writer_impl::_append_file_no_lock(
  std::string && clean_file_path_in_archive, const char* buffer, size_t n_bytes, const uint32_t crc) noexcept
{
  I3S_ASSERT(n_bytes > 0 && buffer);
  //write to archive file:
  uint64_t offset = m_ar->tellp();
  //fill in the header:
  detail::Local_file_hdr loc_hdr(n_bytes, crc, (uint16_t)clean_file_path_in_archive.size());
  write_it(m_ar.get(), loc_hdr);
  //fill in the file name
  m_ar->write(clean_file_path_in_archive.data(), clean_file_path_in_archive.size());
  // write extra info
  if (loc_hdr.packed_size == detail::c_ones_32)
  {
    auto extra = detail::Extra_field_64bit_file_size(n_bytes);
    write_it(m_ar.get(), extra);
  }
  //write the data:
  m_ar->write(buffer, n_bytes);
  //fill in the CD record:
  detail::Cd_hdr cd_hdr;
  cd_hdr.offset = offset;
  cd_hdr.size64 = n_bytes;
  cd_hdr.path = std::move(clean_file_path_in_archive);
  cd_hdr.raw = detail::Cd_hdr_raw(loc_hdr, n_bytes, offset);
  cd_hdr.write(&m_tmp);
  return offset;
}

bool Slpk_writer_impl::_cancel_no_lock()noexcept
{
  if (m_path.empty())
    return true;
  m_index.destroy();

  if(m_tmp.is_open())
    m_tmp.close();
  auto tmp_file_path = m_path;
  tmp_file_path += I3S_T(".tmp");
  bool is_deleted = utl::remove_file(tmp_file_path);
  I3S_ASSERT(is_deleted);

  if (m_ar)
  {
    m_ar->close();
    m_ar = nullptr;
  }
  auto pending_file_path = m_path;
  pending_file_path += I3S_T(".pending");
  is_deleted = utl::remove_file(pending_file_path);
  I3S_ASSERT(is_deleted);

  if (m_temp_folder_for_stream_artifacts)
    m_temp_folder_for_stream_artifacts.clear();
  _init();
  return true;
}

bool Slpk_writer_impl::_close_unfinalized_no_lock() noexcept
{
  if (m_path.empty())
    return false;
  try
  {
    if (!m_index.save(m_ar->tellp(), m_tmp.tellp()))
    {
      // uh-oh... 
      m_index.destroy();
      return false;
    }
  }
  catch (const std::ios_base::failure&)
  {
    // uh-oh...last hope call
    try {
      m_index.destroy();
    }
    catch (const std::ios_base::failure&)
    {
      I3S_ASSERT(false);
      return false;
    }
  }
  m_tmp.close();
  m_ar->close();
  auto res = !_is_io_fail();
  _init();
  return res;
}

bool Slpk_writer_impl::_finalize_no_lock()noexcept
{
  bool is_ok = false;
  if (!m_path.empty())
  {
    const char* src_ptr;
    int64_t size;
    m_index.finalize_index(&src_ptr, &size);
    {
      const uint32_t crc = ~crc32_buf(src_ptr, static_cast<size_t>(size));
      [[maybe_unused]]
      auto offset = _append_file_no_lock(detail::c_hash_table_file_name, src_ptr, static_cast<size_t>(size), crc);
    }
    if (!_is_io_fail())
    {
      auto total_file_count = m_index.get_count() + 1; // "+1" is for index itself that is not in the index

      uint64_t offset_to_cd = m_ar->tellp();
      uint64_t cd_size = m_tmp.tellp();
      m_tmp.flush();
      m_tmp.seekg(0);
      //copy the tmp CD to the ZIP archive:
      if (copy_stream(&m_tmp, m_ar.get(), cd_size))
      {
        //add [zip64 end of central directory record]
        uint64_t offset_to_eocd64 = m_ar->tellp();
        detail::End_of_cd_64 eocd64(total_file_count, cd_size, offset_to_cd);
        write_it(m_ar.get(), eocd64);
        //[zip64 end of central directory locator]
        detail::End_of_cd_locator_64 the_end_64(offset_to_eocd64);
        write_it(m_ar.get(), the_end_64);
        //[end of central directory record]
        detail::End_of_cd_legacy the_end(total_file_count, cd_size, offset_to_cd);
        write_it(m_ar.get(), the_end);
        if (m_ar->good() && !m_ar->fail())
        {
          m_tmp.close();
          std::error_code err_code;

          auto tmp_file_path = m_path;
          tmp_file_path += I3S_T(".tmp");
          {
            [[maybe_unused]]
            auto is_deleted = utl::remove_file(tmp_file_path);
            I3S_ASSERT(is_deleted);
          }

          auto pending_file_path = m_path;
          pending_file_path += I3S_T(".pending");

          m_index.destroy();
          //rename:
          m_ar->close();
          m_ar = nullptr;
          if (!m_temp_folder_for_stream_artifacts)
          {
            // if **not** in user-provided stream mode, rename the final SLPK:
            err_code = {};
            stdfs::rename(pending_file_path, m_path, err_code);
            is_ok = !err_code;
          }
          else
            is_ok = true;
        }
        m_path.clear();
      }
    }
  }
  if (!is_ok)
    _cancel_no_lock();
  else
    _init();
  return is_ok;
}


class Extracted_slpk_writer final : public Slpk_writer
{
public:
  Extracted_slpk_writer() = default;
  ~Extracted_slpk_writer(); // N.B.: by default removes all files and subdirectories in the destination folder
  // --- Abstract_i3s_writer ---
  virtual bool    create_archive(const std::filesystem::path& path, Create_flags flags = Overwrite_if_exists_and_cancel_in_destructor) override;
#if 0
  virtual bool    get_file(const std::string& archivePath, std::string* content) override { return false; }
#endif
  virtual bool    append_file(const std::string& archivePath, const char* buffer, int nBytes, Mime_type type = Mime_type::Not_set, Mime_encoding pack = Mime_encoding::Not_set) override;
  virtual bool    finalize() override { m_dest.clear();  return true; }
  virtual bool    cancel() noexcept override ; // clears the destination folder
  virtual bool    close_unfinalized() noexcept override  { m_dest.clear();  return true; }
private:
  stdfs::path m_dest;
  Create_flags m_openning_flags = Overwrite_if_exists_and_cancel_in_destructor;
};

Extracted_slpk_writer::~Extracted_slpk_writer()
{
  if (m_dest.empty())
    return;
  if (m_openning_flags & Unfinalized_only)
    close_unfinalized();
  cancel();
}

bool Extracted_slpk_writer::create_archive(const std::filesystem::path& path, Create_flags flags)
{
  if (flags & Unfinalized_only)
  {
    if (!folder_exists(m_dest))
      return false;
  }
  else
  {
    // if destination exists, it must be empty.
    // directories will be created when extracting.
    std::error_code e;
    if (folder_exists(m_dest) && (!std::filesystem::is_empty(m_dest, e) || e))
      return false;
  }
  m_dest = path;
  m_openning_flags = flags;
  return true;
}

bool Extracted_slpk_writer::append_file(const std::string& archivePath, const char* buffer, int nBytes, Mime_type type, Mime_encoding pack)
{
  const auto path_out = m_dest / archivePath;        // nodes/0/textures/0.jpg
  const auto dir = std::filesystem::path(path_out).parent_path(); // nodes/0/textures
  // check directory exists. Create if it doesn't.
  if (!folder_exists(dir))
  {
    // if create_directory fails, check if it's because directory already exsists (created in another thread)
    if (!create_directory_recursively(dir) && !folder_exists(dir))
      return false;
  }
  // append the file
  return write_file(path_out, buffer, nBytes);
}

bool Extracted_slpk_writer::cancel() noexcept
{
  if (m_dest.empty())
    return true;
  m_dest.clear();
  std::error_code ec;
  stdfs::remove_all(m_dest, ec);
  return !ec;
}

}
Output_stream* create_output_stream_std(std::filesystem::path path, std::ios_base::openmode mode)
{
  return new detail::Output_stream_std(path, mode);
}

}//endof ::utl::detail


utl::Slpk_writer* utl::create_file_slpk_writer(const std::filesystem::path& dst_path)
{
  if (dst_path.extension() == ".eslpk")
    return new detail::Extracted_slpk_writer(); // extracted slpk
  else
    return new detail::Slpk_writer_impl();    // slpk
}

utl::Slpk_writer* utl::create_slpk_writer(const std::string& dst_path)
{
  using namespace std::string_view_literals;
  const auto pos = dst_path.find("://"sv);

  // If no :// marker is found in the url_or_path, we assume this is a filesystem
  // path, which we designate with empty scheme tag.
  const auto scheme = pos == std::string::npos ?
    std::string{} :
    utl::to_lower(dst_path.substr(0, pos));

  const auto slpk_writer_factory = utl::Slpk_writer_factory::get(scheme);
  if (!slpk_writer_factory)
    return nullptr; // unknown URI scheme

  return slpk_writer_factory->create_writer(dst_path);
}

} // namespace i3slib
