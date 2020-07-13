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

#include "utils/utl_serialize.h"

namespace i3slib
{

namespace utl
{
  
//! "key" string must be ASCII encoded. "Value" string can be UTF-8 encoded  ( String_os (UTF-16LE, usually) are converted to UTF-8 on read/write
class Archive_out_json: public Archive_out
{
public:
  I3S_EXPORT explicit Archive_out_json(std::ostream* out, int version = 0);
  I3S_EXPORT virtual ~Archive_out_json();
  I3S_EXPORT virtual void    begin_obj(const char*)override;
  I3S_EXPORT virtual void    end_obj(const char*)override;

  I3S_EXPORT virtual int version() const override  { return m_version; }

protected:
  I3S_EXPORT virtual bool    _open_tag_name(const char* name) override;
  I3S_EXPORT virtual bool    _close_tag_name(const char* name) override;
  I3S_EXPORT virtual void    _begin_seq(int) override;
  I3S_EXPORT virtual void    _write_seq_separator() override;
  I3S_EXPORT virtual void    _close_seq() override;
  I3S_EXPORT virtual void    _save_variant(const Variant& v) override;
  //virtual bool    _saveArraySize(int s)   override;
  //virtual void    _saveArray(const char* ptr, int nBytes) override;
  I3S_EXPORT virtual void    _save_binary_blob(const Binary_blob_const& blob) override;
  I3S_EXPORT virtual void    _save_unparsed_node(utl::Unparsed_field& node) override;
  I3S_EXPORT virtual void    _set_rtti_code(int code) override { m_next_obj_rtti_code = code; };
private:
  Archive_out_json(const Archive_out_json&); //disallow
  Archive_out_json& operator=(const Archive_out_json&); //disallow
  int m_version;
  std::ostream* m_out;
  struct State
  {
    State() :n_field(0)/*, nSeq( 0)*/ {}
    int n_field;
    //int nSeq;
  };
  std::vector< State > m_states;
  int       m_next_obj_rtti_code = -1;
};

//! ---------- simple helper function: --------------- 


//! Use non-dom version of the writer
template< class T > inline std::string to_json(const T& val, int version=0)
{
  static_assert(has_serialize<T>::value, "Type T is missing SERIALIZABLE Macro");
  std::ostringstream out;
  Archive_out_json ar(&out, version);
  ar & const_cast<T&>(val);
  return out.str();
}

namespace detail
{
template< class T >
struct Wrap_array
{
  SERIALIZABLE(Wrap_array);
  explicit Wrap_array(const char* key, const std::vector<T>& a) : m_vec(a), m_key(key) {}

  template< class Ar > void serialize(Ar& ar)
  {
    ar & nvp(m_key, seq(const_cast<std::vector<T>&>(m_vec)));
  }
  const std::vector<T>& m_vec;
  const char* m_key;
};
}
template< class T > inline std::string to_json_array(const char* key, const std::vector<T>& vec, int version=0)
{

  //static_assert(has_serialize<T>::value, "Type T is missing SERIALIZABLE Macro");
  std::ostringstream out;
  Archive_out_json ar(&out, version);
  detail::Wrap_array< T > wrap(key, vec);
  ar & wrap;
  return out.str();
}

} // namespace utl

} // namespace i3slib
