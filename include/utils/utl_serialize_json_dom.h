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
#include "utils/utl_serialize_json.h"
#include "utils/utl_i3s_resource_defines.h"
#include "utils/utl_basic_tracker_api.h"
#include <optional>
#ifdef __EMSCRIPTEN__
#include <string_view>
#endif
namespace i3slib
{

namespace utl
{

class Archive_in_json_dom_impl;

//! a DOM implementation of the JSON archiveIN. 
class  Archive_in_json_dom final: public Archive_in
  {
  public:
    I3S_EXPORT explicit Archive_in_json_dom(const std::string& doc, int version = 0);
#ifdef __EMSCRIPTEN__
    I3S_EXPORT explicit Archive_in_json_dom(const std::string_view& doc, int version = 0);
#endif
    I3S_EXPORT virtual ~Archive_in_json_dom();
    I3S_EXPORT virtual void begin_obj(const char*) override;
    I3S_EXPORT virtual void end_obj(const char*) override;
    virtual int version() const override         { return m_version; }
    I3S_EXPORT void rewind(int new_version); // allows to start reading from the very beginning
    I3S_EXPORT void rewind(); // allows to start reading from the very beginning

  protected:
    virtual bool    _open_tag_name(const char* name)  override; // nullptr as a name means the first entry whatever name it has
    virtual bool    _try_open_tag_name(const char* name)  override; // nullptr as a name means the first entry whatever name it has
    virtual bool    _close_tag_name(const char* name) override;
    virtual int     _open_sequence() override;
    virtual int     _read_seq_separator() override;
    virtual void    _load_variant(Variant& v) override;
    //virtual int     _load_array_size()  override;
    //virtual void    _load_array(char* ptr, int nBytes) override;
    virtual void    _load_binary_blob(Binary_blob& blob) override;

    virtual std::string     _get_locator() const override;
    virtual void   _load_unparsed_node(Unparsed_field& node) override;
    virtual int    _get_rtti_code() override;
  private:
    Archive_in_json_dom(const Archive_in_json_dom&); //disallow
    Archive_in_json_dom& operator=(const Archive_in_json_dom&); //disallow
    int m_version;
    std::unique_ptr< Archive_in_json_dom_impl  > m_impl;
  };

class Json_input  // allows reusing the same stream/Archive for multiple passes
{
public:
  explicit Json_input(const std::string& str, int version = 0) :
    m_ar(str, version) {}
#ifdef __EMSCRIPTEN__
  explicit Json_input(const std::string_view& str, int version = 0) :
    m_ar(str, version) {}
#endif
  void rewind(int new_version) { m_ar.rewind(new_version); need_rewind = false; }
  void rewind() { if (need_rewind) { m_ar.rewind(); need_rewind = false; } }

  bool has_parse_error() const
  {
    return m_ar.has_parse_error();
  }

  std::string get_parse_error_string() const
  {
    if (!m_ar.has_parse_error())
      return "";
    return m_ar.get_parse_error_string();
  }

  // Clear any unsupressed logic errors (e.g. missing something that we say is required on an object).
  // The JSON can then be reparsed, generally in a different context (e.g. expecting a different object
  // than the one we originally tried).
  void clear_unsuppressed_error()
  {
    m_ar.clear_unsuppressed_error();
  }

  template< class T > void read(T& obj)
  {
    static_assert(has_serialize<T>::value, "Object is missing SERIALIZABLE Macro");
    I3S_ASSERT(!m_ar.has_parse_error()); // the basic JSON parse should have succeeded if we're going to proceed w/ parsing our objects
    check_rewind();
    m_ar & obj;
  }
  
  template< class T > void read_array(std::vector<T>& arr)
  {
    //static_assert(has_serialize<T>::value, "Object is missing SERIALIZABLE Macro");
    I3S_ASSERT(!m_ar.has_parse_error()); // the basic JSON parse should have succeeded if we're going to proceed w/ parsing our objects
    check_rewind();
    auto s = seq(arr);
    m_ar & s;
  }

  template< class T > void read(const char* key, std::vector<T>& arr)
  {
    //static_assert(has_serialize<T>::value, "Object is missing SERIALIZABLE Macro");
    I3S_ASSERT(!m_ar.has_parse_error()); // the basic JSON parse should have succeeded if we're going to proceed w/ parsing our objects
    check_rewind();
    m_ar.begin_obj(nullptr);
    if (!m_ar.has_parse_error())
    {
      m_ar& nvp(key, seq(arr));
      m_ar.end_obj(nullptr);
    }
  }
  template< class T > void read(T& obj, std::vector< utl::Json_parse_error >& log)
  {
    I3S_ASSERT(!m_ar.has_parse_error()); // the basic JSON parse should have succeeded if we're going to proceed w/ parsing our objects
    read(obj);
    m_ar.pop_suppressed_log(&log);
  }  

  template< class T > [[nodiscard]]
  bool read(T& obj, utl::Basic_tracker* trk, const std::string& ref_document_for_error_reporting)
  {
    I3S_ASSERT(!m_ar.has_parse_error()); // the basic JSON parse should have succeeded if we're going to proceed w/ parsing our objects

    m_log.clear();
    read(obj, m_log);
    if (m_ar.has_parse_error())
    {
      return utl::log_error(trk, IDS_I3S_JSON_PARSING_ERROR, ref_document_for_error_reporting, m_ar.get_parse_error_string());
    }
    if (m_log.size() && trk)
    {
      //warnings 
      for (const auto& l : m_log)
        utl::log_warning(trk, IDS_I3S_JSON_PARSING_ERROR, ref_document_for_error_reporting, std::string(l.what()));
    }
    return true;
  }
  
  [[nodiscard]]
  const std::vector< utl::Json_parse_error >& get_error_log()const noexcept
  {
    return m_log;
  }
private:
  void check_rewind() { I3S_ASSERT(!need_rewind); need_rewind = true; }
  std::vector< utl::Json_parse_error > m_log;
  Archive_in_json_dom m_ar;
  bool need_rewind = false;
};

 //! ---------- simple helper function: --------------- 

template< class T > inline bool from_json_with_log(const std::string& str, T* obj, std::vector< Json_parse_error >* errors, int version=0)
{
  Json_input in(str, version);
  if (in.has_parse_error())
    return false;
  if (errors)
    in.read(*obj, *errors);
  else
    in.read(*obj);
  return !in.has_parse_error();
}

 template< class T > inline bool from_json(const std::string& str, T* obj, int version=0, std::string* pErrStr=nullptr )
 {
   Json_input in(str, version);
   if (in.has_parse_error())
     return false;
   in.read(*obj);
   const auto has_parse_error = in.has_parse_error();
   if (has_parse_error && pErrStr)
   {
     *pErrStr = in.get_parse_error_string();
   }
   return !has_parse_error;
 }

#ifdef __EMSCRIPTEN__
 template< class T > inline bool from_json(const std::string_view& str_view, T* obj, int version = 0, std::string* pErrStr = nullptr)
 {
   Json_input in(str_view, version);
   if (in.has_parse_error())
     return false;
   in.read(*obj);
   const auto has_parse_error = in.has_parse_error();
   if (has_parse_error && pErrStr)
   {
     *pErrStr = in.get_parse_error_string();
   }
   return !has_parse_error;
 }
#endif

 template< class T > inline bool from_json_array(const std::string& json,  const char* key, std::vector<T>* arr, int version=0 )
 {
   Json_input in(json, version);
   if (in.has_parse_error())
     return false;
   in.read(key, *arr);
   return !in.has_parse_error();
 }

 template< class T > inline bool from_json_array(const std::string& json, std::vector<T>* arr, int version = 0)
 {
   Json_input in(json, version);
   if (in.has_parse_error())
     return false;
   in.read_array(*arr);
   return !in.has_parse_error();
 }

}

} // namespace i3slib
