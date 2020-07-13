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

namespace i3slib
{

namespace utl
{

class Archive_in_json_dom_impl;

//! a DOM implementation of the JSON archiveIN. 
class  Archive_in_json_dom : public Archive_in
  {
  public:
    I3S_EXPORT explicit Archive_in_json_dom(const std::string& doc, int version = 0);
    I3S_EXPORT virtual ~Archive_in_json_dom();
    I3S_EXPORT virtual void              begin_obj(const char*)  override;
    I3S_EXPORT virtual void              end_obj(const char*) override;
    virtual int version() const override         { return m_version; }
    I3S_EXPORT void rewind(int new_version); // allows to start reading from the very beginning
    I3S_EXPORT void rewind(); // allows to start reading from the very beginning

  protected:
    virtual bool    _open_tag_name(const char* name)  override;
    virtual bool    _try_open_tag_name(const char* name)  override;
    virtual bool    _close_tag_name(const char* name) override;
    virtual bool    _read_seq_separator(int c) override;
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
  static std::optional<Json_input> create(const std::string& str, 
    utl::Basic_tracker* trk, const std::string& ref_document_for_error_reporting, int version = 0)
  {
    try
    {
      return std::make_optional<Json_input>( str, version );
    }
    catch (utl::Json_exception & err)
    {
      auto what = err.what();
      utl::log_error(trk, IDS_I3S_JSON_PARSING_ERROR, ref_document_for_error_reporting, std::string(what));
      return {};
    }
  }

  explicit Json_input(const std::string& str, int version = 0) :
    m_ar(str, version) {}
  void rewind(int new_version) { m_ar.rewind(new_version); need_rewind = false; }
  void rewind() { if (need_rewind) { m_ar.rewind(); need_rewind = false; } }

  template< class T > void read(T& obj)
  {
    static_assert(has_serialize<T>::value, "Object is missing SERIALIZABLE Macro");
    check_rewind();
    m_ar & obj;
  }
  template< class T > void read(const char* key, std::vector<T>& arr)
  {
    //static_assert(has_serialize<T>::value, "Object is missing SERIALIZABLE Macro");
    check_rewind();
    m_ar & nvp(key, seq(arr));
  }
  template< class T > void read(T& obj, std::vector< utl::Json_exception >& log)
  {
    read(obj);
    m_ar.pop_suppressed_log(&log);
  }  

  template< class T > [[nodiscard]]
  bool read(T& obj, utl::Basic_tracker* trk, const std::string& ref_document_for_error_reporting)
  {
    try
    {
      m_log.clear();
      read(obj, m_log);
    }
    catch (utl::Json_exception & err)
    {
      auto what = err.what();
      return utl::log_error(trk, IDS_I3S_JSON_PARSING_ERROR, ref_document_for_error_reporting, std::string(what));
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
  const std::vector< utl::Json_exception >& get_error_log()const noexcept
  {
    return m_log;
  }
private:
  void check_rewind() { I3S_ASSERT(!need_rewind); need_rewind = true; }
  std::vector< utl::Json_exception > m_log;
  Archive_in_json_dom m_ar;
  bool need_rewind = false;
};

 //! ---------- simple helper function: --------------- 

 //! Use DOM. MAY THROW Json_exception !
template< class T > inline void from_json_with_log(const std::string& str, T* obj, std::vector< Json_exception >* errors, int version=0)
{
  Json_input in(str, version);
  if (errors)
    in.read(obj, *errors);
  else
    in.read(obj);
}

 //! Use DOM. MAY THROW Json_exception !
 template< class T > inline void from_json(const std::string& str, T* obj, int version=0 )
 {
   Json_input(str, version).read(*obj);
 }

 template< class T > inline bool from_json_no_except(const std::string& str, T* obj, int version=0)
 {
   try
   {
     from_json(str, obj, version);
   }
   catch (Json_exception& /*err*/)
   {
     //auto what = err.what();
     return false;
   }
   return true;
 }


 //! Use DOM. MAY THROW Json_exception !
 template< class T > inline void from_json_array(const std::string& json,  const char* key, std::vector<T>* arr, int version=0 )
 {
   Json_input(json, version).read(key, *arr);
 }

}

} // namespace i3slib
