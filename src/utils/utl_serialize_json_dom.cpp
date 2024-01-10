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
#include "utils/utl_base64.h"
#define RAPIDJSON_ASSERT(exp) I3S_ASSERT(exp)
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <string_view>
#endif

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
#ifdef __EMSCRIPTEN__
    Archive_in_json_dom_impl(const std::string_view& doc, Archive_in* base);
#endif
    void              begin_obj(const char*);
    void              end_obj(const char*);
    std::string       get_locator() const;
    void rewind()
    {
      m_current = &m_root;
      m_stack.clear();
      m_locators.clear();
    }

  protected:
    bool    _open_tag_name(const char* name);
    bool    _try_open_tag_name(const char* name);
    bool    _close_tag_name(const char* name);
    int     _open_sequence(); // returns the number of values in the array
    int     _read_seq_separator(); // you can be used to iterate over both arrays and dictionaries
    void    _load_variant(Variant& v);
    //int     _load_array_size() override  { return -1; } //not supported. use utl::seq instead.
    //void    _load_array(char* ptr, int n_bytes) override { I3S_ASSERT(false); } //implement it if you need it
    void    _load_binary_blob(Binary_blob& blob)
    {
      //read as a string:
      std::string str64;
      Variant vstring(&str64, utl::Variant::Memory::Shared);
      _load_variant(vstring);
      if (m_base->has_parse_error())
      {
        return;
      }
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
#ifdef __EMSCRIPTEN__
    void parse_document(const std::string_view& doc);
#endif
    rapidjson::Document m_root;
    const rapidjson::Value m_null;
    const rapidjson::Value* m_current;
    std::string m_err;
    std::vector<std::pair<const rapidjson::Value*, rapidjson::Value::ConstValueIterator> > m_stack;
    std::vector< std::string > m_locators;
    Archive_in* m_base;
    bool m_has_basic_parse_error = false; // true if provided JSON doesn't adhere to JSON spec, as opposed to adhering to our i3s spec
  };

  Archive_in_json_dom_impl::Archive_in_json_dom_impl(const std::string& doc, Archive_in* base)
    : m_base(base)
  {
    I3S_ASSERT(m_base);
    parse_document(doc);
  }

#ifdef __EMSCRIPTEN__
  Archive_in_json_dom_impl::Archive_in_json_dom_impl(const std::string_view& doc, Archive_in* base)
    : m_base(base)
  {
    I3S_ASSERT(m_base);
    parse_document(doc);
  }
#endif

  void Archive_in_json_dom_impl::parse_document(const std::string& doc)
  {
    rapidjson::ParseResult result = m_root.Parse<rapidjson::kParseFullPrecisionFlag>(doc.c_str(), doc.length());
    if (result)
    {
      rewind();
    }
    else
    {
      std::string error_str = rapidjson::GetParseError_En(result.Code());
      if (!error_str.empty() && error_str.back() == '.')
        error_str.resize(error_str.length() - 1);
      error_str.append(" at position ");
      error_str.append(std::to_string(result.Offset()));
      error_str.append(".");
      m_base->set_basic_parse_error_string(error_str);
    }
  }

#ifdef __EMSCRIPTEN__
  void Archive_in_json_dom_impl::parse_document(const std::string_view& doc)
  {
    rapidjson::ParseResult result = m_root.Parse<rapidjson::kParseFullPrecisionFlag>(doc.data(), doc.length());
    if (result)
    {
      rewind();
    }
    else
    {
      std::string error_str = rapidjson::GetParseError_En(result.Code());
      if (!error_str.empty() && error_str.back() == '.')
        error_str.resize(error_str.length() - 1);
      error_str.append(" at position ");
      error_str.append(std::to_string(result.Offset()));
      error_str.append(".");
      m_base->set_basic_parse_error_string(error_str);
    }
  }
#endif

  void  Archive_in_json_dom_impl::begin_obj([[ maybe_unused ]] const char* serialized_object_name)
  {
    I3S_ASSERT(!m_base->has_parse_error());
    if (!m_current->IsObject())
    {
      m_base->report_parsing_error(Json_parse_error::Error::Object_expected, serialized_object_name);
      return;
    }

    m_stack.emplace_back(m_current, nullptr); // TODO
    m_current = &m_null;
  }

  void  Archive_in_json_dom_impl::end_obj([[ maybe_unused ]] const char* serialized_object_name)
  {
    I3S_ASSERT(!m_base->has_parse_error());
    m_current = m_stack.back().first;
    m_stack.pop_back();
  };

  std::string Archive_in_json_dom_impl::get_locator() const
  {
    size_t res_len = 0;
    int loop = 0;
    for (auto& item : m_locators)
    {
      if (loop++)
        ++res_len;
      res_len += item.length();
    }
    std::string res;
    res.reserve(res_len);
    loop = 0;
    for (auto& item : m_locators)
    {
      if (loop++)
        res.push_back('.');
      res.append(item);
    }
    return res;
  }

  int Archive_in_json_dom_impl::_get_rtti_code()
  {
    I3S_ASSERT(!m_base->has_parse_error());
    // read it:
    if (m_current->IsNull())
    {
      I3S_ASSERT(false);
      m_base->report_parsing_error(Json_parse_error::Error::Object_expected, "Unexpected");
      return 0;
    }
    auto it = m_current->FindMember("_rtti_code_");
    if (it != m_current->MemberEnd() && it->value.IsInt())
    {
      return it->value.GetInt();
    }
    I3S_ASSERT(false);
    return -1;
  }

  bool Archive_in_json_dom_impl::_open_tag_name(const char* name)
  {
    I3S_ASSERT(!m_base->has_parse_error());
    if (m_stack.empty())
    {
      m_base->report_parsing_error(Json_parse_error::Error::Scope_error, name);
      return false;
    }
    m_locators.push_back(name ? name : "<any>");
    if (name)
    {
      auto parent = m_stack.back().first;
      auto it = parent->FindMember(name);
      if (it != parent->MemberEnd())
        m_current = &(it->value);
      else
        m_current = &m_null;// nullptr;
    }
    else
    {
      auto& parent = *m_stack.back().first;
      auto parentType = parent.GetType();
      if (parentType != rapidjson::Type::kObjectType || parent.MemberBegin() == parent.MemberEnd())
      {
        m_base->report_parsing_error(Json_parse_error::Error::Unnamed_field_not_object, "<any>");
        return false;
      }
      m_current = &parent.MemberBegin()->value;
    }
    if (m_current->IsNull())
    {
      m_base->report_parsing_error(Json_parse_error::Error::Not_found, name);
      return false;
    }
    return true;
  }

  //! the not-throw version:
  bool Archive_in_json_dom_impl::_try_open_tag_name(const char* name)
  {
    I3S_ASSERT(!m_base->has_parse_error());
    if (m_stack.empty())
      return false;
    if (name)
    {
      //m_current = &m_stack.back().first->get(name, Json::Value::null);
      auto parent = m_stack.back().first;
      auto it = parent->FindMember(name);
      if (it != parent->MemberEnd())
        m_current = &(it->value);
      else
        m_current = &m_null;// nullptr;
    }
    else
    {
      auto& parent = *m_stack.back().first;
      auto parentType = parent.GetType();
      if (parentType != rapidjson::Type::kObjectType || parent.MemberBegin() == parent.MemberEnd())
      {
        m_base->report_parsing_error(Json_parse_error::Error::Unnamed_field_not_object, "<any>");
        return false;
      }
      m_current = &parent.MemberBegin()->value;
    }
    if (!m_current->IsNull())
      m_locators.push_back(name ? name : "<any>");
    return !m_current->IsNull();
  }


  bool Archive_in_json_dom_impl::_close_tag_name(const char* name)
  {
    // NOTE: we may have parse errors at this point if a child item had errors
    if (!m_locators.empty())
      m_locators.pop_back();
    else
      I3S_ASSERT(false);
    return true;
  }

  Variant variant_to_variant(const rapidjson::Value& val, Archive_in* pArchive)
  {
    switch (val.GetType())
    {
    case rapidjson::Type::kFalseType: return Variant(val.GetBool()); // deep copy
    case rapidjson::Type::kTrueType: return Variant(val.GetBool()); // deep copy
    case rapidjson::Type::kNumberType:
    {
      if (val.IsUint64())
        return Variant(val.GetUint64());
      else if (val.IsUint())
        return Variant(val.GetUint());
      else if (val.IsInt())
        return Variant(val.GetInt());
      else
        return Variant(val.GetDouble());
    }
    case rapidjson::Type::kStringType: return Variant(std::string(val.GetString())); // deep copy
    case rapidjson::Type::kNullType:  return Variant();
    case rapidjson::Type::kArrayType:
    {
      if (pArchive)
        pArchive->report_parsing_error(Json_parse_error::Error::Unexpected_array, "kArrayType");
      return Variant(); 
      break;
    }
    case rapidjson::Type::kObjectType:
    {
      if (pArchive)
        pArchive->report_parsing_error(Json_parse_error::Error::Unexpected_object, "kObjectType");
      return Variant();
      break;
    }
    default:
      if (pArchive)
        pArchive->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Unknown JSON variant type");
      return Variant();
    }
  }

  template<typename T>
  std::string stringify(const T& o)
  {
    auto type = o.GetType();
    const bool supported = type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType;
    I3S_ASSERT(supported);
    if (!supported)
      return "";
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    o.Accept(writer);
    return sb.GetString();
  }

  void Archive_in_json_dom_impl::_load_variant(Variant& v)
  {
    if (m_current->IsNull())
    {
      m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Variant");
      return;
    }
    switch (v.get_type())
    {
    case Variant_trait::Type::Bool:       
    {
      if (!m_current->IsBool())
      {
        auto type = m_current->GetType();
        if (type == rapidjson::Type::kNullType)
        {
          v.set(false);
        }
        else if (type == rapidjson::Type::kNumberType && m_current->IsInt())
        {
          auto asInt = m_current->GetInt();
          v.set(asInt ? true : false);
        }
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Bool.");
        return;
      }
      v.set(m_current->GetBool());
      break;
    }
    case Variant_trait::Type::Int8:       
    {
      if (!m_current->IsInt())
      {        
#ifdef _DEBUG
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
        }
#endif
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set((int8_t)m_current->GetInt());
      break;
    }
    case Variant_trait::Type::Uint8:      
    {
      if (!m_current->IsUint())
      {
#ifdef _DEBUG
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
        }
#endif
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set((uint8_t)m_current->GetUint());
      break;
    }
    case Variant_trait::Type::Int16:      
    {
      if (!m_current->IsInt())
      {
#ifdef _DEBUG
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
        }
#endif
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set((int16_t)m_current->GetInt());
      break;
    }
    case Variant_trait::Type::Uint16:     
    {
      if (!m_current->IsUint())
      {
#ifdef _DEBUG
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
        }
#endif
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set((uint16_t)m_current->GetUint());
      break;
    }
    case Variant_trait::Type::Int32:      
    {
      if (!m_current->IsInt())
      {
        auto type = m_current->GetType();
        if (type == rapidjson::Type::kNumberType)
        {
          auto asDouble = m_current->GetDouble();
          if (asDouble >= std::numeric_limits<int32_t>::lowest() && asDouble <= std::numeric_limits<int32_t>::max())
          {
            v.set(static_cast<int32_t>(asDouble));
            return;
          }
#ifdef _DEBUG
          else if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
          {
            auto s = stringify(*m_current);
          }
#endif
        }
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set(m_current->GetInt());
      break;
    }
    case Variant_trait::Type::Uint32:     
    {
      if (!m_current->IsUint())
      {
        auto type = m_current->GetType();
        if (type == rapidjson::Type::kNumberType)
        {
          auto asDouble = m_current->GetDouble();
          if (asDouble >= std::numeric_limits<uint32_t>::lowest() && asDouble <= std::numeric_limits<uint32_t>::max())
          {
            v.set(static_cast<uint32_t>(asDouble));
            return;
          }
#ifdef _DEBUG
          else if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
          {
            auto s = stringify(*m_current);
          }
#endif
        }
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set(m_current->GetUint());
      break;
    }
    case Variant_trait::Type::Int64:      
    {
      if (!m_current->IsInt64())
      {
        auto type = m_current->GetType();
        if (type == rapidjson::Type::kNumberType)
        {
          auto asDouble = m_current->GetDouble();
          if (asDouble >= std::numeric_limits<int64_t>::lowest() && asDouble <= std::numeric_limits<int64_t>::max())
          {
            v.set(static_cast<int64_t>(asDouble));
            return;
          }
#ifdef _DEBUG
          else if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
          {
            auto s = stringify(*m_current);
          }
#endif
        }        
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set(m_current->GetInt64());
      break;
    }
    case Variant_trait::Type::Uint64:     
    {
      if (!m_current->IsUint64())
      {
        auto type = m_current->GetType();
        if (type == rapidjson::Type::kNumberType)
        {
          auto asDouble = m_current->GetDouble();
          if (asDouble >= std::numeric_limits<uint64_t>::lowest() && asDouble <= std::numeric_limits<uint64_t>::max())
          {
            v.set(static_cast<uint64_t>(asDouble));
            return;
          }
#ifdef _DEBUG
          else if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
          {
            auto s = stringify(*m_current);
          }
#endif
        }  
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Int.");
        return;
      }
      v.set(m_current->GetUint64());
      break;
    }
    case Variant_trait::Type::Float:      
    {
      if (!m_current->IsFloat())
      {
        auto type = m_current->GetType();
        if (!m_current->IsLosslessFloat())
        {
          if (m_current->IsDouble())
          {
            const auto asDouble = m_current->GetDouble();
            if (asDouble > std::numeric_limits<float>::max())
            {
              v.set(std::numeric_limits<float>::max());
              return;
            }
            else if (asDouble < -std::numeric_limits<float>::max())
            {
              v.set(-std::numeric_limits<float>::max());
              return;
            }
            else
            {
              v.set(static_cast<float>(asDouble));
              return;
            }
          }
          m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Float.");
          return;
        }
      }
      v.set(m_current->GetFloat());
      break;
    }
    case Variant_trait::Type::Double:     
    {
      if (!m_current->IsDouble())
      {
        auto type = m_current->GetType();
        if (!m_current->IsLosslessDouble())
        {
          m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Double.");
          return;
        }
      }
      v.set(m_current->GetDouble());
      break;
    }
    case Variant_trait::Type::String:     
    {
      if (!m_current->IsString())
      {
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
          v.set(s);
          return;
        }
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to String.");
        return;
      }

      v.set(std::string(m_current->GetString(), m_current->GetStringLength()));
      break;
    }
    case Variant_trait::Type::WString:    
    {
      if (!m_current->IsString())
      {
        auto type = m_current->GetType();
        if (type != rapidjson::Type::kArrayType && type != rapidjson::Type::kObjectType)
        {
          auto s = stringify(*m_current);
          v.set(s);
          return;
        }
        m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to String.");
        return;
      }

#ifdef PCSL_WIDE_STRING_OS
      v.set(utf8_to_os(std::string_view(m_current->GetString(), m_current->GetStringLength())));
#else
      // TODO
      // On Linux wchar_t is usually 32-bit, so we're going to need something
      // like utf8_to_utf32() that we don't have now.
      // It could be implemented with std::wstring_convert, but it's been deprecated in C++17.
      I3S_ASSERT_EXT(false);
#endif
      break;
    }
    case Variant_trait::Type::Not_set:    
    {
      v = variant_to_variant(*m_current, m_base);
      break;
    }
    default:
      I3S_ASSERT(false);
      m_base->report_parsing_error(Json_parse_error::Error::Variant_conversion, "Value is not convertible to Variant.");
      break;
    }
  }

  void Archive_in_json_dom_impl::_load_unparsed_node(Unparsed_field& node)
  {
    //we need to turn the current node into a JSON representation:
    //Json::StreamWriterBuilder wbuilder;
    //node.raw = Json::writeString(wbuilder, *m_current);
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    m_current->Accept(writer);
    node.raw = s.GetString();
  }

  int Archive_in_json_dom_impl::_open_sequence()
  {
    if (!m_current->IsArray())
    {
      m_base->report_parsing_error(Json_parse_error::Error::Array_expected, "_open_sequence");
      return 0;
    }
    int array_size = static_cast<int>(m_current->Size());
    if (array_size)
    {
      m_stack.emplace_back(m_current, m_current->Begin());
      m_current = &*m_current->Begin();
    }
    else
      m_current = &m_null;

    return array_size;
  }

  int Archive_in_json_dom_impl::_read_seq_separator()
  {
    auto& [arr, it] = m_stack.back();
    if (it == arr->End())
    {
      I3S_ASSERT(false);
      return 0;
    }
    ++it;
    auto dist = std::distance(it, arr->End());
    if (dist)
    {
      m_current = &*it;
    }
    else
    {
      m_current = &m_null;
      m_stack.pop_back();
    }
    return static_cast<int>(dist);
  }

// ------------------------------------------------------------
// class          Archive_in_json_dom
// ------------------------------------------------------------

Archive_in_json_dom::Archive_in_json_dom(const std::string& doc, int version)
  : m_version(version), m_impl(new Archive_in_json_dom_impl(doc, this))

{}

#ifdef __EMSCRIPTEN__
Archive_in_json_dom::Archive_in_json_dom(const std::string_view& doc, int version)
  : m_version(version), m_impl(new Archive_in_json_dom_impl(doc, this))

{}
#endif

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
int   Archive_in_json_dom::_open_sequence() { return m_impl->_open_sequence(); }
int   Archive_in_json_dom::_read_seq_separator() { return m_impl->_read_seq_separator(); }
void  Archive_in_json_dom::_load_variant(Variant& v) { return m_impl->_load_variant(v); }
//int   Archive_in_json_dom::_load_array_size() { I3S_ASSERT_EXT(false);  return -1; } //not supported. use utl::seq instead.
//void  Archive_in_json_dom::_loadArray(char* ptr, int nBytes) { I3S_ASSERT_EXT(false); } //implement it if you need it
void  Archive_in_json_dom::_load_binary_blob(Binary_blob& blob) { m_impl->_load_binary_blob(blob); }
void  Archive_in_json_dom::_load_unparsed_node(Unparsed_field& node) { m_impl->_load_unparsed_node(node); }
int   Archive_in_json_dom::_get_rtti_code() { return m_impl->_get_rtti_code(); }

} // namespace utl

} // namespace i3slib
