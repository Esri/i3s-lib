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
#include "utils/utl_serialize_json.h"
#include "utils/utl_variant.h"
#include "utils/utl_base64.h"

namespace i3slib
{

namespace utl
{
// --------------------------------------------------------------------
// class      Json_exception
// --------------------------------------------------------------------
Json_exception::Json_exception(Error what, const std::string& where, const std::string& add)
  : m_type(what)
  , m_where(where)
{
  switch (what)
  {
  case Error::Not_set:
    m_generic_what = "Unspecified error:\"" + where + "\"";
    break;
  case Error::Not_found:
    m_generic_what = "JSON node not found:\"" + where + "\"";
    break;
  case Error::Scope_error:
    m_generic_what = "JSON Object scope error: \"" + where + "\"";
    break;
    break;
  case Error::Unnamed_field_not_object:
    m_generic_what = "JSON Object is expected for anonynous field:\"" + where + "\"";
    break;
  case Error::Unknown_enum:
    m_generic_what = "JSON string \"" + where + "\" is not a known value for this enumeration (" + add + ")";
    break;
  case Error::Unexpected_array:
    m_generic_what = "\"Unexpected JSON array: \"" + where + "\"";
    break;
  case Error::Unexpected_object:
    m_generic_what = "\"Unexpected JSON object: \"" + where + "\"";
    break;
  case Error::Array_expected:
    m_generic_what = "\"JSON array expected: \"" + where + "\"";
    break;
  case Error::Fixed_array_out_of_bound:
    m_generic_what = "\"JSON array \"" + where + "\" must be of size \"" + add + "\"";
    break;
  case Error::Object_expected:
    m_generic_what = "\"Object expected: \"" + where + "\"";
    break;
  }
}

// --------------------------------------------------------------------
// class      Archive_out_json
// --------------------------------------------------------------------
Archive_out_json::Archive_out_json(std::ostream* out, int version)
  : m_version(version)
  , m_out(out)
{
  if (m_out)
  {
    *m_out << std::boolalpha; // "true"/ "false", not "0"/"1"
    // always use the shortest floating point value representation
    m_out->unsetf(std::ios::floatfield);
  }
  //NOTE: we're not restoring the stream state.
}


Archive_out_json::~Archive_out_json() = default;

void Archive_out_json::begin_obj(const char*)
{
  (*m_out) << "{\n";
  m_states.push_back(State());
  if (m_next_obj_rtti_code != -1)
  {
    _open_tag_name("_rtti_code_");
    _save_variant(utl::Variant(m_next_obj_rtti_code));
    _close_tag_name("_rtti_code_");
    m_next_obj_rtti_code = -1;
  }
}
void Archive_out_json::end_obj(const char*)
{
  (*m_out) << "\n}\n"; m_states.pop_back();
}

bool Archive_out_json::_open_tag_name(const char* name)
{
  if (m_states.empty())
  {
    I3S_ASSERT_EXT(false); // trying to serialize an object that does not define SERIALIZABLE() macro ?
    return false;
  }
  if (m_states.back().n_field > 0)
    (*m_out) << ",\n";
  (*m_out) << '\"' << (name ? name : "0") << '\"' << " : ";
  m_states.back().n_field++;
  return true;
}


bool Archive_out_json::_close_tag_name(const char* name) { return true; }
void Archive_out_json::_begin_seq(int) { (*m_out) << "["; }
void Archive_out_json::_write_seq_separator() { (*m_out) << ", "; }
void Archive_out_json::_close_seq() { (*m_out) << "]"; };
//bool Archive_out_json::_save_arraySize(int s) { return false; } // use utl::seq instead
//void Archive_out_json::_save_array(const char* ptr, int n_bytes) { I3S_ASSERT_EXT(false); } // use utl::seq instead
void Archive_out_json::_save_binary_blob(const Binary_blob_const& blob)
{
  std::string str = base64_encode(reinterpret_cast<const unsigned char*>(blob.data()), (int)blob.size());
  Variant vs(&str, Variant::Memory::Shared);
  _save_variant(vs);
}
void Archive_out_json::_save_variant(const Variant& v)
{
  if (Variant_trait::is_string(v.get_type()))
  {
    //special chars are already escaped
    (*m_out) << '\"' << v << '\"';
  }
  else
    (*m_out) << v;
}
void Archive_out_json::_save_unparsed_node(Unparsed_field& node)
{
  if (node.raw.empty())
    (*m_out) << "null";
  else
    (*m_out) << node.raw;
}

//#ifdef PCSL_WIDE_STRING_OS
//void Archive_out_json::_save_string(const utl::String_os& s) { std::string tmp = utl::os_to_utf8(s); (*m_out) << '\"' << tmp << '\"'; }
//#endif
//void Archive_out_json::_save_string(const std::string& s) { std::string tmp(utl::encode_double_quote(s)); (*m_out) << '\"' << tmp << '\"'; }

}

} // namespace i3slib
