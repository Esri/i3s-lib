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
#include "utils/utl_i3s_export.h"
#include "utils/utl_declptr.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_variant.h"
#include "utils/utl_geom.h" 
#include "utils/utl_enum_flag_op.h" 
#include <stdint.h>
#include <vector>
#include <sstream>
#include <array>
#include <cstring>

#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)

// -------------------------------------------------------------------------------
// Define macro to make class/struct  "serializable" using this framework
// -------------------------------------------------------------------------------

#define SERIALIZABLE( name ) \
  friend i3slib::utl::Archive_out& operator&( i3slib::utl::Archive_out& ar,  const name& me )   { ar.begin_obj( #name ); const_cast< name& >( me ).serialize( ar ); ar.end_obj(#name); return ar; } \
  friend i3slib::utl::Archive_in& operator&( i3slib::utl::Archive_in& ar, name& me ) \
  {                                   \
    if (ar.has_parse_error())         \
      return ar;                      \
    ar.begin_obj( #name);             \
    if (ar.has_parse_error())         \
      return ar;                      \
    me.serialize( ar );               \
    if (ar.has_parse_error())         \
      return ar;                      \
    ar.end_obj(#name);                \
    return ar;                        \
  }                                   \
  static constexpr bool has_serializable_macro() { return true; }

#define SERIALIZABLE_SPLIT( name ) \
  friend i3slib::utl::Archive_out& operator&( i3slib::utl::Archive_out& ar,  const name& me )   { ar.begin_obj( #name );  const_cast< name& >(me).serialize( ar ); ar.end_obj(#name); return ar; } \
  friend i3slib::utl::Archive_in& operator&( i3slib::utl::Archive_in& ar, name& me ) \
  {                                   \
    if (ar.has_parse_error())         \
      return ar;                      \
    ar.begin_obj( #name);             \
    if (ar.has_parse_error())         \
      return ar;                      \
    me.deserialize( ar );             \
    ar.end_obj(#name);                \
    return ar;                        \
  }                                   \
  static constexpr bool has_serializable_macro() { return true; }

namespace i3slib
{

namespace utl
{

//when things go wrong...
class I3S_EXPORT Json_parse_error
{
public:
  typedef uint32_t Error_flags_t;
  enum class Error : Error_flags_t {
    Not_set = 0,
    Not_found = 1,
    Scope_error = 2,
    Unnamed_field_not_object = 4,
    Unknown_enum = 8,
    Unexpected_array = 16,
    Unexpected_object = 32,
    Array_expected = 64,
    Fixed_array_out_of_bound = 128,
    Object_expected = 256,
    Variant_conversion = 512
  };
public:
  Json_parse_error(Error what, const std::string& where, const std::string& add = std::string());
  Json_parse_error(const std::string& w) : m_generic_what(w) {  }
  Json_parse_error(const std::exception& e) { m_generic_what = e.what(); }
  const char* what() const noexcept { return m_generic_what.c_str(); }
  Error         m_type = Error::Not_set;
  std::string   m_where;
  std::string   m_generic_what;
};

static const char* c_txt_inf_plus = "+inf";
static const char* c_txt_inf_minus = "-inf";
static const char* c_txt_nan = "nan";


// -------------------------------------------------------------------------------
//  API to serialize data members 
// -------------------------------------------------------------------------------

enum class Serialize_field_mode : int {
  Required = 0,
  Optional_always_write,
  Optional_skip_if_default,
};

// forward declaration:
template<class T>           struct Nvp;
template<class T, class Y>  struct Nvp_opt;
template< class V > struct Sequence;
template< class T > struct Enum_str;
template< class T > struct Infinite_number;

//! To serialize a **required** field:
template<class T> Nvp<T> nvp(const char* n, T&& v);
//! to serialize a **optional** field:
template<class T, class Y>  Nvp_opt<T, Y>  opt(const char* n, T&& v, const Y& def, const Serialize_field_mode m = Serialize_field_mode::Optional_skip_if_default);
template<class V>  Nvp_opt<Sequence<V>, V> opt(const char* n, Sequence<V>&& v, const V& def = V());

//! To wrap a **array-like**  container:
template<class V>  Sequence<V> seq(V& v);
//! To wrap a string-based enum:
template< class T > Enum_str<T> enum_str(T& v);
//! To wrap a possibly non-finite number:
template< class T > Infinite_number<T> infinite_number(T& v);

//! for direct copy of "unparsed" fields without having to parse to serializable objects.
//! Only supported for JSON archive
//class Unparsed_node
//{
//public:
//  virtual ~Unparsed_node() = default;
//  typedef std::shared_ptr< Unparsed_node > Ptr;
//  virtual uint32_t            rtti() const = 0;
//  virtual std::string     as_json_string() const=0;
//};

struct Unparsed_field
{
  friend bool operator==(const Unparsed_field& a, const Unparsed_field& b) { return a.raw == b.raw; }
  std::string  raw;
};

class Binary_blob_const
{
public:
  virtual ~Binary_blob_const() = default;
  virtual size_t size() const = 0;
  virtual const char* data() const = 0;
};

class Binary_blob : public Binary_blob_const
{
public:
  using Binary_blob_const::size;
  using Binary_blob_const::data;
   
  virtual ~Binary_blob() = default;
  virtual void resize(size_t n) = 0;
  virtual char* data() = 0;
};

// shallow-copy wrapper for vector/array
template< class T > class Readable_blob_wrap : public Binary_blob_const
{
public:
  explicit Readable_blob_wrap(const T& p) : _m(&p) {}
  virtual size_t size() const override { return _m->size() * sizeof(T); }
  virtual const char* data() const override { return reinterpret_cast<const char*>(_m); }
private:
  const T* _m;
};

template< class T > class Writable_blob_wrap : public Binary_blob
{
public:
  explicit Writable_blob_wrap(T* p) : _m(p) {}
  virtual void resize(size_t n) override { _m->resize(n); }
  virtual char* data() override { return reinterpret_cast<char*>(_m); }
  virtual size_t size() const override { return _m->size() * sizeof(T); }
  virtual const char* data() const override { return reinterpret_cast<const char*>(_m); }
private:
  T* _m;
};


// -------------------------------------------------------------------------------
//            class Archive_in  
// -------------------------------------------------------------------------------
enum class Archive_io_enum { In, Out };

//! Abtract base class for an input archive. 
class Archive_in
{
public:
  DECL_PTR(Archive_in);
  virtual ~Archive_in() {}
  virtual Archive_io_enum   direction() const { return Archive_io_enum::In; }
  virtual void              begin_obj(const char*) {}
  virtual void              end_obj(const char*) {}
  virtual int               version() const = 0;// { return 0; } 
  void                      report_parsing_error(Json_parse_error::Error what, std::string arg);
  void                      report_parsing_error(Json_parse_error::Error what, const char* arg);
  virtual void              push_suppressed_error_mask(Json_parse_error::Error mask) { m_suppressed_masks.push_back({ (Json_parse_error::Error_flags_t)mask, (int)m_suppressed_log.size() }); }
  virtual void              push_suppressed_error_mask(Json_parse_error::Error_flags_t mask) { m_suppressed_masks.push_back({ mask, (int)m_suppressed_log.size() }); }
  virtual bool              pop_suppressed_error_mask(Json_parse_error::Error_flags_t* errors_out = nullptr);
  virtual void              pop_suppressed_log(std::vector< Json_parse_error >* errors);
  bool                      has_parse_error() const;
  std::string               get_parse_error_string() const;
  void                      clear_unsuppressed_error();
  void                      set_basic_parse_error_string(const std::string& s);
  friend Archive_in& operator&(Archive_in& in, bool& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, char& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, int8_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, uint8_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, int16_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, uint16_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, int32_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, uint32_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, int64_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, uint64_t& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, float& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, double& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  friend Archive_in& operator&(Archive_in& in, std::string& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
  
#ifdef PCSL_WIDE_STRING_OS
  friend Archive_in& operator&(Archive_in& in, std::wstring& v) { if (in.has_parse_error()) return in; Variant wrap(&v, Variant::Memory::Shared);  in._load_variant(wrap); return in; }
#endif

  friend Archive_in& operator&(Archive_in& in, Variant& v) { if (in.has_parse_error()) return in; in._load_variant(v); return in; }
  friend Archive_in& operator&(Archive_in& in, Unparsed_field& v) { if (in.has_parse_error()) return in; in._load_unparsed_node(v); return in; }

  template< class T > friend Archive_in& operator&(Archive_in& in, Enum_str<T>& v);
  template< class T > friend Archive_in& operator&(Archive_in& in, Infinite_number<T>& v);
  template< class T > friend Archive_in& operator&(Archive_in& in, Sequence<T>& sq);
  template< class T > friend Archive_in& operator&(Archive_in& in, std::shared_ptr< T >& v);
  template< class T > friend Archive_in& operator&(Archive_in& in, std::vector< T >& v);
  template< class T > friend Archive_in& operator&(Archive_in& in, Nvp< T >&& v);
  template< class T, class Y > friend Archive_in& operator&(Archive_in& in, Nvp_opt< T, Y >&& v);

protected:
  virtual bool    _open_tag_name(const char*) = 0;
  virtual bool    _try_open_tag_name(const char*) = 0; //no throw version...
  virtual bool    _close_tag_name(const char*) = 0;
  virtual int     _open_sequence() = 0;  // returns the sequence size and if non empty sets the first sequence element as current
  virtual int     _read_seq_separator() = 0; // advances the current element to a next in an array/dictionary
                                            // and returns the number of elements remaining in the array
                                            // including the current one
  virtual void    _load_variant(Variant& v) = 0;
  virtual void    _load_binary_blob(Binary_blob& blob) = 0;
  //virtual int     _load_array_size() = 0;
  //virtual void    _load_array(char* ptr, int nBytes) = 0;
  virtual void    _load_unparsed_node(Unparsed_field& node) = 0;
  virtual std::string       _get_locator() const { return std::string(); } //experimental for better error reporting.
  virtual int     _get_rtti_code() = 0;
  struct Parse_error
  {
    Json_parse_error::Error what;
    std::string location;
    std::string arg;
  };
  struct Mask_item
  {
    Json_parse_error::Error_flags_t what = 0;
    int stack_0 = 0;
  };
  std::vector<Mask_item> m_suppressed_masks = { {0,0} };
  std::vector<Parse_error>  m_suppressed_log;
  Parse_error m_unsuppressed_error = {Json_parse_error::Error::Not_set, "", ""};
  std::string m_basic_parse_error_string = "";
private:
  bool has_unsuppressed_error() const;

};


// -------------------------------------------------------------------------------
//            class Archive_out 
// -------------------------------------------------------------------------------

enum class Archive_out_flags : int { None = 0, Force_write_default_value = 1 };
SLL_DEFINE_ENUM_FLAG_OPERATORS(Archive_out_flags);


// Abract base class for output Archive.
class I3S_EXPORT Archive_out
{
public:
  DECL_PTR(Archive_out);
  virtual ~Archive_out() {}
  virtual Archive_io_enum     direction() const { return Archive_io_enum::Out; }
  virtual void    begin_obj(const char*) {}
  virtual void    end_obj(const char*) {}
  virtual int     version() const = 0;// { return 0; }
  friend Archive_out& operator&(Archive_out& out, bool v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, char v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, int8_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, uint8_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, int16_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, uint16_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, int32_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, uint32_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, int64_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, uint64_t v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, float v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, double v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, std::wstring& v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, std::string& v) { out._save_variant(Variant(v)); return out; }
  friend Archive_out& operator&(Archive_out& out, Variant& v) { out._save_variant(v); return out; }
  friend Archive_out& operator&(Archive_out& out, Unparsed_field& v) { out._save_unparsed_node(v); return out; }
  template< class T > friend Archive_out& operator&(Archive_out& out, const Enum_str<T>& v);
  template< class T > friend Archive_out& operator&(Archive_out& out, const Infinite_number<T>& v);
  template< class T > friend Archive_out& operator&(Archive_out& out, const Sequence<T>& v);
  template< class T > friend Archive_out& operator&(Archive_out& out, const std::shared_ptr< T >& v);
  template< class T > friend Archive_out& operator&(Archive_out& out, const std::vector< T >& v);
  template< class T > friend Archive_out& operator&(Archive_out& out, const Nvp< T >& v);
  template< class T, class Y > friend  Archive_out& operator&(Archive_out& out, const Nvp_opt< T, Y >& v);
  inline constexpr void          set_flags(Archive_out_flags flags) { m_flags = flags; }
  //for symmetry with Archive_in:
  inline constexpr void          report_parsing_error(Json_parse_error::Error, const std::string&) noexcept {}
  inline constexpr void          report_parsing_error(Json_parse_error::Error, const char*) noexcept {}
  inline constexpr void          push_suppressed_error_mask(Json_parse_error::Error)noexcept {}
  inline constexpr void          push_suppressed_error_mask(Json_parse_error::Error_flags_t) noexcept {}
  inline constexpr bool          pop_suppressed_error_mask(Json_parse_error::Error_flags_t* = nullptr) { return false; }
  inline constexpr bool          pop_suppressed_error_if_single(Json_parse_error::Error) noexcept { return false; }
  inline constexpr void          pop_suppressed_log(std::vector< Json_parse_error >*) {}
protected:
  virtual bool    _open_tag_name(const char*) = 0;
  virtual bool    _close_tag_name(const char*) = 0;
  virtual void    _begin_seq(int size) = 0;
  virtual void    _write_seq_separator() = 0;
  virtual void    _close_seq() {};
  virtual void    _save_variant(const Variant& v) = 0;
  virtual void    _save_binary_blob(const Binary_blob_const& blob) = 0;
  virtual void    _save_unparsed_node(utl::Unparsed_field& node) = 0;
  virtual void    _set_rtti_code(int code) = 0;
  Archive_out_flags m_flags { 0 };
};

//void  Archive_out::force_optional_alway_write(bool force) { if (force) m_flags |= (uint32_t)Out_flags::Force_write_default_value; else m_flags &= (~(uint32_t)Out_flags::Force_write_default_value); }


// -------------------------------------------------------------------------------
//            Inline implementation (template)
// -------------------------------------------------------------------------------

namespace detail
{
// "Trait" function for sequence-like containers:
template < class T, class Alloc = std::allocator<T> >  void clear_vec(std::vector<T, Alloc>& vec) { vec.clear(); }
template < class T, class Alloc = std::allocator<T> >  bool resize_vec(std::vector<T, Alloc>& vec, size_t n) { vec.resize(n); return true; }

template < class T, size_t N >                    void clear_vec(std::array<T, N>& vec) {  } //do nothing
template < class T, size_t N >                    bool resize_vec(std::array<T, N>& vec, size_t n) { return n <= N; } //do nothing
}


//! Named-value-pair template class (i.e. a named field)
template<class T>
struct Nvp
{
  Nvp(const char* n, T&& v) : name(n), val(v) {}
  ~Nvp()
  {
    //int dummy = 24;
  }
  const char* name;
  T& val;
};


//! Named-value-pair template class (i.e. a named field)
template<class T, class Y>
struct Nvp_opt
{
  Nvp_opt(const char* n, T&& v, const Y& def_val, Serialize_field_mode m) : name(n), val(v), default_value(def_val), mode(m) {}
  ~Nvp_opt()
  {
    //int dummy = 24;
  }
  const char* name;
  T& val;
  const Y& default_value;
  Serialize_field_mode   mode;
};

//! String representing an enum value
template< class T>
struct Enum_str
{
  explicit Enum_str(T& v) : val_ref(v) {}
  ~Enum_str()
  {
    //int dummy = 24;
  }
  Enum_str<T>& operator=(const Enum_str<T>& src) { val_ref = src.val_ref; return *this; }
  Enum_str<T>& operator=(const T& src) { val_ref = src; return *this; }

  friend bool operator==(const Enum_str<T>& a, const Enum_str<T>& b) { return a.val_ref == b.val_ref; }
  friend bool operator==(const Enum_str<T>& a, const T& b) { return a.val_ref == b; }
  friend bool operator==(const T& a, const Enum_str<T>& b) { return a == b.val_ref; }
  friend bool operator==(const T& a, Enum_str<T&> b) { return a == b.val_ref; }
  T& val_ref;
};
template< class T > inline Enum_str<T> enum_str(T& v) { return Enum_str<T>(v); }


//! String representing an enum value
template< class T>
struct Infinite_number
{
  explicit Infinite_number(T& v) : val_ref(v) {}
  ~Infinite_number()
  {
    //int dummy = 24;
  }
  Infinite_number<T>& operator=(const Infinite_number<T>& src) { val_ref = src.val_ref; return *this; }
  Infinite_number<T>& operator=(const T& src) { val_ref = src; return *this; }

  friend bool operator==(const Infinite_number<T>& a, const Infinite_number<T>& b) { return a.val_ref == b.val_ref; }
  friend bool operator==(const Infinite_number<T>& a, const T& b) { return a.val_ref == b; }
  friend bool operator==(const T& a, const Infinite_number<T>& b) { return a == b.val_ref; }
  friend bool operator==(const T& a, Infinite_number<T&> b) { return a == b.val_ref; }
  T& val_ref;
};
template< class T > inline Infinite_number<T> infinite_number(T& v) { return Infinite_number<T>(v); }

template< class V >
struct Sequence
{
  explicit Sequence(V& v) : vec(v) {}
  ~Sequence()
  {
    //int dummy = 24;
  }
  Sequence<V>& operator=(const Sequence<V>& src) { vec = src.vec; return *this; }
  Sequence<V>& operator=(const V& src) { vec = src; return *this; }
  friend bool operator==(const Sequence<V>& a, const Sequence<V>& b) { return a.vec == b.vec; }
  friend bool operator==(const Sequence<V>& a, const V& b) { return a.vec == b; }
  friend bool operator==(const V& a, const Sequence<V>& b) { return a == b.vec; }

  V& vec;
};
template< class T > inline Sequence<T> seq(T& v)
{
  return Sequence<T>(v);
}

//template<class T> Nvp<T> nvp(const char* n, T& v);
//template<class T> Nvp<const T> nvp(const char* n, const T& v);


template<class T> inline Nvp<const T> nvp(const char* n, const T& v) {
  return Nvp<const T>(n, v);
}

template<class T> inline Nvp<T> nvp(const char* n, T&& v) {
  return Nvp<T>(n, std::forward<T>(v));
}
template<class T, class Y> inline Nvp_opt<T, Y> opt(const char* n, T&& v, const Y& def, const Serialize_field_mode m)
{
  return Nvp_opt<T, Y>(n, std::forward<T>(v), def, m);
}

template<class V>  Nvp_opt<Sequence<V>, V> opt(const char* n, Sequence<V>&& v, const V& def)
{
  return Nvp_opt<Sequence<V>, V>(n, std::forward<Sequence<V>>(v), def, Serialize_field_mode::Optional_skip_if_default);
}

//SFINAE test to prevent usage of non SERIALIZABLE() objects
template <class T>
struct has_serialize {
  // Types "yes" and "no" are guaranteed to have different sizes,
  // specifically sizeof(yes) == 1 and sizeof(no) == 2.
  typedef char yes[1];
  typedef char no[2];

  template <typename C>
  static yes& test(decltype(C::has_serializable_macro));

  template <typename>
  static no& test(...);

  // If the "sizeof" of the result of calling test<T>(nullptr) is equal to sizeof(yes),
  // the first overload worked and T has a nested type named foobar.
  static const bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
};


// ------------------------------------------------------------------------------------
//        class Archive_in  **** inline implementation: **** 
// ------------------------------------------------------------------------------------

inline bool  Archive_in::has_parse_error() const
{
  return !m_basic_parse_error_string.empty() || has_unsuppressed_error();
}

inline bool  Archive_in::has_unsuppressed_error() const
{
  return m_unsuppressed_error.what != Json_parse_error::Error::Not_set;
}

inline void  Archive_in::clear_unsuppressed_error()
{
  m_unsuppressed_error = { Json_parse_error::Error::Not_set, "", "" };
}

inline void Archive_in::set_basic_parse_error_string(const std::string& s)
{
  m_basic_parse_error_string = s;
}

inline std::string Archive_in::get_parse_error_string() const
{
  utl::Json_parse_error::Error what = m_unsuppressed_error.what;
  std::string where = m_unsuppressed_error.location;
  std::string add = m_unsuppressed_error.arg;

  std::string ret("");

  switch (what)
  {
  case utl::Json_parse_error::Error::Not_found:
    ret = "JSON node not found:\"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Scope_error:
    ret = "JSON Object scope error: \"" + where + "\"";
    break;
    break;
  case utl::Json_parse_error::Error::Unnamed_field_not_object:
    ret = "JSON Object is expected for anonynous field:\"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Unknown_enum:
    ret = "JSON string \"" + where + "\" is not a known value for this enumeration (" + add + ")";
    break;
  case utl::Json_parse_error::Error::Unexpected_array:
    ret = "\"Unexpected JSON array: \"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Unexpected_object:
    ret = "\"Unexpected JSON object: \"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Array_expected:
    ret = "\"JSON array expected: \"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Fixed_array_out_of_bound:
    ret = "\"JSON array \"" + where + "\" must be of size \"" + add + "\"";
    break;
  case utl::Json_parse_error::Error::Object_expected:
    ret = "\"Object expected: \"" + where + "\"";
    break;
  case utl::Json_parse_error::Error::Variant_conversion:
    ret = add;// +" (" + where + ")";
    break;
  case utl::Json_parse_error::Error::Not_set:
    {
      // There is no unsuppressed error, which means the parse failure
      // was a basic failure to adhere to the JSON spec.
      I3S_ASSERT_EXT(!m_basic_parse_error_string.empty());
      ret = m_basic_parse_error_string;
      break;
    }
  }
  return ret;
}

inline void  Archive_in::pop_suppressed_log(std::vector< Json_parse_error >* errors)
{
  errors->clear();
  errors->reserve(m_suppressed_log.size());
  for (auto& log : m_suppressed_log)
  {
    errors->emplace_back(log.what, std::move(log.location), std::move(log.arg));
  }
  m_suppressed_log.clear();
}
inline bool Archive_in::pop_suppressed_error_mask(Json_parse_error::Error_flags_t* errors_out)
{
  I3S_ASSERT_EXT(m_suppressed_masks.size());
  Json_parse_error::Error_flags_t out = 0;
  int count = 0;
  for (auto iter = m_suppressed_log.begin() + m_suppressed_masks.back().stack_0; iter < m_suppressed_log.end(); ++iter)
  {
    out |= static_cast<Json_parse_error::Error_flags_t>(iter->what);
    ++count;
  }
  //auto count = (int)m_suppressed_log.size() - m_suppressed_masks.back().stack_0 ;
  if (errors_out)
    *errors_out = out;
  m_suppressed_masks.pop_back();
  return count > 0;
}

inline void Archive_in::report_parsing_error(Json_parse_error::Error what, const char* arg)
{
  report_parsing_error(what, arg ? std::string(arg) : std::string());
}

inline void Archive_in::report_parsing_error(Json_parse_error::Error what, std::string arg)
{
  if (m_suppressed_masks.back().what & ((Json_parse_error::Error_flags_t)what))
  {
    //suppressed error are still recorded:
    m_suppressed_log.push_back({ what, _get_locator(), std::move(arg) });
  }
  else
  {
    // The framework should prevent adding an unsupressed error when there already is one.
    // If there is already an unsupressed error deserialization should be a series of 
    // no-ops until the root read call is reached.
    I3S_ASSERT(!has_unsuppressed_error());
    m_unsuppressed_error = { what, _get_locator(), std::move(arg)};
  }
}

template< class T > inline Archive_in& operator&(Archive_in& in, Enum_str<T>& v)
{
  if (in.has_parse_error()) return in;

  std::string tmp;
  Variant wrap(&tmp, Variant::Memory::Shared);
  in._load_variant(wrap);
  if (in.has_parse_error()) return in;
  if (!from_string(tmp, &v.val_ref))
  {
    in.report_parsing_error(Json_parse_error::Error::Unknown_enum, tmp);
  }
  return in;
}

template< class T > inline Archive_in& operator&(Archive_in& in, Infinite_number<T>& v)
{
  if (in.has_parse_error()) return in;

  Variant wrap;
  in._load_variant(wrap);
  if (in.has_parse_error()) return in;
  switch (wrap.get_type())
  {
  case Variant_trait::Type::String:
  {
    //must be a non-finite:
    const std::string& str = wrap.get<std::string>();
    if (str == c_txt_inf_plus)
      v.val_ref = std::numeric_limits<T>::infinity();
    else if (str == c_txt_inf_minus)
      v.val_ref = -std::numeric_limits<T>::infinity();
    else  //assume not a number then
      v.val_ref = std::numeric_limits<T>::quiet_NaN();
    break;
  }
  default:
    v.val_ref = (T)wrap.to_double();
    break;
  }
  return in;
}


template< class T > inline Archive_in& operator&(Archive_in& in, Sequence<T>& sq)
{
  if (in.has_parse_error()) return in;

  detail::clear_vec(sq.vec);
  if (auto sq_size = in._open_sequence())
  {
    if (!detail::resize_vec(sq.vec, sq_size))
    {
      // skip the entire sequence
      while(in._read_seq_separator())
      {}
      // detect the minimal unexpected size
      int unexpected_size = 2;
      while (detail::resize_vec(sq.vec, unexpected_size)) ++unexpected_size;
      // report the expected size as unexpected minus 1
      in.report_parsing_error(Json_parse_error::Error::Fixed_array_out_of_bound, std::to_string(unexpected_size - 1));
      return in;
    }
    for (int i = 0;; ++i)
    {
      I3S_ASSERT(i < sq_size);
      in & sq.vec[i];
      if (in.has_parse_error()) 
        return in;
      if (!in._read_seq_separator())
        break;
    }    
  }
  return in;
}

template< typename T > inline Archive_in& operator&(Archive_in& in, std::shared_ptr<T>& v)
{
  if (in.has_parse_error()) return in;

  int code = in._get_rtti_code();
  if (in.has_parse_error()) return in;

  std::shared_ptr<T> obj = T::create_from_rtti_code(code);
  if (obj)
  {
    in& (*obj);
    v.swap(obj);
  }
  return in;
}

template< class T > inline Archive_in& operator&(Archive_in& in, std::vector< T >& v)
{
  if (in.has_parse_error()) return in;

  Writable_blob_wrap<std::vector< T > > wrap(&v);
  in._load_binary_blob(wrap);
  return in;
  //int n = in._load_array_size();
  //v.resize(n);
  //if (n)
  //  in._loadArray(reinterpret_cast<char*>(v.data()), n * sizeof(T));
  //return in;
}

template< class T > inline Archive_in& operator&(Archive_in& in, Nvp< T >&& v) 
{
  if (in.has_parse_error()) return in;

  //will throw() if required field is missing:
  if (in._open_tag_name(v.name))
  {
    in& v.val;
    bool is_ok = in._close_tag_name(v.name);
    I3S_ASSERT_EXT(is_ok);
  }
  return in;
}


template< class T, class Y > inline Archive_in& operator&(Archive_in& in, Nvp_opt< T, Y >&& v) 
{
  if (in.has_parse_error()) return in;

  //will throw() if required, false if optional and missing:
  if (in._try_open_tag_name(v.name))
  {
    in& v.val;
    bool is_ok = in._close_tag_name(v.name);
    I3S_ASSERT_EXT(is_ok);
  }
  else
  {
    v.val = v.default_value;
  }
  return in;
}

// ------------------------------------------------------------------------------------
//        class Archive_out  **** inline implementation: **** 
// ------------------------------------------------------------------------------------

template< class T > inline Archive_out& operator&(Archive_out& out, const Enum_str<T>& v)
{
  out._save_variant(Variant(to_string(v.val_ref)));
  return out;
}

template< class T > inline Archive_out& operator&(Archive_out& out, const Infinite_number<T>& v)
{
  if (std::isfinite(v.val_ref))
    out._save_variant(Variant(v.val_ref));
  else if (std::isinf(v.val_ref))
    out._save_variant(Variant(std::string(v.val_ref > 0 ? c_txt_inf_plus : c_txt_inf_minus)));
  else
    out._save_variant(Variant(std::string(c_txt_nan)));
  return out;
}

template< class T > inline Archive_out& operator&(Archive_out& out, const Sequence<T>& v)
{
  out._begin_seq((int)v.vec.size());
  for (size_t i = 0; i < v.vec.size(); ++i)
  {
    if (i > 0)
      out._write_seq_separator();
    out& v.vec[i];
    //sq.vec.push_back(elem);
  }
  out._close_seq();
  return out;
}

template< class T > inline  Archive_out& operator&(Archive_out& out, const std::shared_ptr< T >& v)
{
  out._set_rtti_code(v->get_rtti_code());
  //! deserialize/serialize() must be a virtual functions in T.

  out&* v;
  return out;
}


template< class T > inline  Archive_out& operator&(Archive_out& out, const std::vector< T >& v)
{
  Readable_blob_wrap< std::vector<T> > wrap(v);
  out._save_binary_blob(wrap);
  return out;

  //auto n = (int)v.size(); 
  //out._save_array_size(n); 
  //out._save_array(reinterpret_cast<const char*>( n ? v.data() : nullptr ), n * sizeof(T)); 
  //return out; 
}



template< class T > inline  Archive_out& operator&(Archive_out& out, const Nvp< T >& v)
{
  if (out._open_tag_name(v.name))
  {
    out& v.val;
    bool is_ok = out._close_tag_name(v.name);
    I3S_ASSERT_EXT(is_ok);
  }
  return out;
}

template< class T, class Y > inline  Archive_out& operator&(Archive_out& out, const Nvp_opt< T, Y >& v)
{
  if (v.mode == Serialize_field_mode::Optional_skip_if_default 
    && v.default_value == v.val 
    && !is_set( out.m_flags,  Archive_out_flags::Force_write_default_value) )
    return out; //Skip if value is default.

  if (out._open_tag_name(v.name))
  {
    out& v.val;
    bool is_ok = out._close_tag_name(v.name);
    I3S_ASSERT_EXT(is_ok);
  }
  return out;
}

};//endof ::utl

} // namespace i3slib

#pragma warning(pop)
