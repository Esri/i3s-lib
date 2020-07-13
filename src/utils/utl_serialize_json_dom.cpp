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
#include "utils/utl_serialize_json_dom.h"
#include "utils/utl_serialize_json.h" //just for the exception
#include "utils/json/json.h"
#include "utils/utl_base64.h"

namespace i3slib
{

// ------------------------------------------------------------
// class          Archive_in_json_dom_impl
// ------------------------------------------------------------
namespace utl
{

class Archive_in_json_dom_impl
{
public:
  friend class Archive_in_json_dom;
  Archive_in_json_dom_impl(const std::string& doc, Archive_in* base);
  void              begin_obj(const char*);
  void              end_obj(const char*);
  std::string       get_locator() const;
  void rewind() 
  { 
    I3S_ASSERT(m_stack.size() >= 1);
    m_stack.resize(1); 
    m_iter = Json::Value();
    m_locators.clear();
  }

protected:
  bool    _open_tag_name(const char* name);
  bool    _try_open_tag_name(const char* name);
  bool    _close_tag_name(const char* name);
  bool    _read_seq_separator(int c);
  void    _load_variant(Variant& v);
  //int     _load_array_size() override  { return -1; } //not supported. use utl::seq instead.
  //void    _load_array(char* ptr, int n_bytes) override { I3S_ASSERT_EXT(false); } //implement it if you need it
  void    _load_binary_blob(Binary_blob& blob)
  {
    //read as a string:
    std::string str64;
    Variant vstring(&str64, utl::Variant::Memory::Shared);
    _load_variant(vstring);
    //decode base 64:
    std::string data = base64_decode(str64);
    //copy to blob:
    blob.resize(data.size());
    memcpy(blob.data(), data.data(), data.size());
  }

  void    _load_unparsed_node(Unparsed_field& node);
  int     _get_rtti_code();

private:
  void parse_document(const std::string& doc);
  Json::Value m_root, m_iter;
  std::string m_err;
  std::vector< Json::Value > m_stack;
  std::vector< std::string > m_locators;
  Archive_in* m_base;
};

Archive_in_json_dom_impl::Archive_in_json_dom_impl(const std::string& doc, Archive_in* base)
  : m_base(base)
{
  I3S_ASSERT(m_base);
  parse_document(doc);
}

void Archive_in_json_dom_impl::parse_document(const std::string& doc)
{
  try
  {
    std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder().newCharReader() };
    if (!reader->parse(doc.c_str(), doc.c_str() + doc.length(), &m_root, &m_err))
      throw Json_exception(m_err);
    m_stack.push_back(m_root);
  }
  catch (Json::Exception & err)
  {
    throw Json_exception(err);
  }
}


void  Archive_in_json_dom_impl::begin_obj(const char*)
{
  try
  {
    if (!m_iter.isNull())
      m_stack.push_back(m_iter);
  }
  catch (Json::Exception & err)
  {
    throw Json_exception(err);
  }

}
void  Archive_in_json_dom_impl::end_obj(const char*)
{
  if(m_stack.size() > 1)
    m_stack.pop_back();
};


std::string Archive_in_json_dom_impl::get_locator() const
{
  std::ostringstream oss;
  int loop = 0;
  for (auto& item : m_locators)
  {
    if (loop++)
      oss << ".";
    oss << item;
  }
  return oss.str();
}

bool Archive_in_json_dom_impl::_open_tag_name(const char* name)
{
  //bool is_ok = false;
  try
  {
    m_locators.push_back(name ? name : "<any>");
    if (!m_stack.size())
    {
      m_base->report_parsing_error(Json_exception::Error::Scope_error, name);
      return false;
    }
    if (name)
      m_iter = m_stack.back().get(name, Json::Value());
    else
    {
      if (m_stack.back().type() != Json::ValueType::objectValue || m_stack.back().begin() == m_stack.back().end()) //|| m_stack.back().type() != Json::ValueType::arrayValue
        m_base->report_parsing_error(Json_exception::Error::Unnamed_field_not_object, name ? name : "<any>");
      m_iter = *m_stack.back().begin();
    }
    if (m_iter.isNull())
    {
      m_base->report_parsing_error(Json_exception::Error::Not_found, name);
      return false;
    }
    return true;
  }
  catch (Json::Exception&)
  {
    m_base->report_parsing_error(Json_exception::Error::Object_expected, name); //TBD: but could be other errors ?
    return false;
  }
}

int Archive_in_json_dom_impl::_get_rtti_code()
{
  // read it:
  if (m_iter.isNull())
  {
    I3S_ASSERT(false);
    throw Json_exception("Unexpected"); // must be in an object already 
  }
  auto found = m_iter.get("_rtti_code_", Json::Value());
  if (!found.isNull())
  {
    return found.asInt();
  }
  I3S_ASSERT(false);
  return -1;
}

//! the not-throw version:
bool Archive_in_json_dom_impl::_try_open_tag_name(const char* name)
{
  try
  {
    if (!m_stack.size())
      return false;
    if (name)
      m_iter = m_stack.back().get(name, Json::Value());
    else if (m_stack.back().type() == Json::ValueType::objectValue && m_stack.back().begin() != m_stack.back().end())
      m_iter = *m_stack.back().begin();
    else
      m_iter = Json::Value();
    if (!m_iter.isNull())
      m_locators.push_back(name ? name : "<any>");
    return !m_iter.isNull();
  }
  catch (Json::Exception&)
  {
    //throw Json_exception(err);
    m_base->report_parsing_error(Json_exception::Error::Object_expected, name); //TBD: but could be other errors ?
    return false;
  }
}


bool Archive_in_json_dom_impl::_close_tag_name(const char* name)
{
  if (m_locators.size())
    m_locators.pop_back();
  else
    I3S_ASSERT(false);
  return true;
}

Variant variant_to_variant(const Json::Value& val)
{
  switch (val.type())
  {
  case Json::ValueType::booleanValue: return Variant(val.asBool()); // deep copy
  case Json::ValueType::intValue: return Variant(val.asInt()); // deep copy
  case Json::ValueType::uintValue: return Variant(val.asUInt()); // deep copy
  case Json::ValueType::realValue: return Variant(val.asDouble()); // deep copy
  case Json::ValueType::stringValue: return Variant(val.asString()); // deep copy
  case Json::ValueType::nullValue:  return Variant();
  case Json::ValueType::arrayValue: throw Json_exception(Json_exception::Error::Unexpected_array, val.asString()); break;
  case Json::ValueType::objectValue: throw Json_exception(Json_exception::Error::Unexpected_object, val.asString()); break;
  default:
    throw Json_exception("Unknown JSON variant type"); break;
  }
}

void Archive_in_json_dom_impl::_load_variant(Variant& v)
{
  try
  {
    if (m_iter.isNull())
      throw Json_exception("Expected a Variant");
    //I3S_ASSERT_EXT(!m_iter.isNull());
    switch (v.get_type())
    {
    case Variant_trait::Type::Bool:       v.set(m_iter.asBool());  break;
    case Variant_trait::Type::Int8:       v.set((int8_t)m_iter.asInt());  break;
    case Variant_trait::Type::Uint8:      v.set((uint8_t)m_iter.asUInt());  break;
    case Variant_trait::Type::Int16:      v.set((int16_t)m_iter.asInt());  break;
    case Variant_trait::Type::Uint16:     v.set((uint16_t)m_iter.asUInt());  break;
    case Variant_trait::Type::Int32:      v.set(m_iter.asInt());  break;
    case Variant_trait::Type::Uint32:     v.set(m_iter.asUInt());  break;
    case Variant_trait::Type::Int64:      v.set(m_iter.asInt64());  break;
    case Variant_trait::Type::Uint64:     v.set(m_iter.asUInt64());  break;
    case Variant_trait::Type::Float:      v.set(m_iter.asFloat()); break;
    case Variant_trait::Type::Double:     v.set(m_iter.asDouble()); break;
    case Variant_trait::Type::String:     v.set(m_iter.asString()); break;
    case Variant_trait::Type::WString:    v.set(m_iter.asString()); break;
    case Variant_trait::Type::Not_set:    v = variant_to_variant(m_iter); break;
    default:
      I3S_ASSERT_EXT(false);
    }
  }
  catch (Json::Exception & err)
  {
    throw Json_exception(err);
  }
}

void Archive_in_json_dom_impl::_load_unparsed_node(Unparsed_field& node)
{
  //we need to turn the current node into a JSON representation:
  Json::StreamWriterBuilder wbuilder;
  node.raw = Json::writeString(wbuilder, m_iter);
}

bool Archive_in_json_dom_impl::_read_seq_separator(int c)
{
  try
  {
    if (c == 0)
    {
      if (!m_iter.isArray())
      {
        auto what = m_iter.isObject() ? std::string("<JSON object>") : m_iter.asString();
        m_base->report_parsing_error(Json_exception::Error::Array_expected, what);
      }
      m_stack.push_back(m_iter);
    }

    if (!m_stack.size())
    {
      auto what = m_iter.isObject() ? std::string("<JSON object>") : m_iter.asString();
      m_base->report_parsing_error(Json_exception::Error::Array_expected, what);
    }
    if (c >= (int)m_stack.back().size())
    {
      m_stack.pop_back();
      m_iter = Json::Value();
      return false;
    }
    else
    {
      m_iter = m_stack.back()[c];
      return true;
    }
  }
  catch (Json::Exception & err)
  {
    throw Json_exception(err);
  }
}


// ------------------------------------------------------------
// class          Archive_in_json_dom
// ------------------------------------------------------------

Archive_in_json_dom::Archive_in_json_dom(const std::string& doc, int version)
  : m_version(version), m_impl(new Archive_in_json_dom_impl(doc, this))
{}

Archive_in_json_dom::~Archive_in_json_dom()
{}

void  Archive_in_json_dom::begin_obj(const char* c) { return m_impl->begin_obj(c); }
void  Archive_in_json_dom::end_obj(const char* c) { return m_impl->end_obj(c); }

void Archive_in_json_dom::rewind(int new_version)
{
  m_version = new_version;
  m_impl->rewind();
}

void Archive_in_json_dom::rewind()
{
  m_impl->rewind();
}

std::string  Archive_in_json_dom::_get_locator()const { return m_impl->get_locator(); }
bool  Archive_in_json_dom::_open_tag_name(const char* name) { return m_impl->_open_tag_name(name); }
bool  Archive_in_json_dom::_try_open_tag_name(const char* name) { return m_impl->_try_open_tag_name(name); }
bool  Archive_in_json_dom::_close_tag_name(const char* name) { return  m_impl->_close_tag_name(name); }
bool  Archive_in_json_dom::_read_seq_separator(int c) { return m_impl->_read_seq_separator(c); }
void  Archive_in_json_dom::_load_variant(Variant& v) { return m_impl->_load_variant(v); }
//int   Archive_in_json_dom::_load_array_size() { I3S_ASSERT_EXT(false);  return -1; } //not supported. use utl::seq instead.
//void  Archive_in_json_dom::_loadArray(char* ptr, int nBytes) { I3S_ASSERT_EXT(false); } //implement it if you need it
void  Archive_in_json_dom::_load_binary_blob(Binary_blob& blob) { m_impl->_load_binary_blob(blob); }
void  Archive_in_json_dom::_load_unparsed_node(Unparsed_field& node) { m_impl->_load_unparsed_node(node); }
int   Archive_in_json_dom::_get_rtti_code() { return m_impl->_get_rtti_code(); }

} // namespace utl

} // namespace i3slib
