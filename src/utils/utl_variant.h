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
#include "utils/utl_i3s_assert.h"
#include "utils/utl_string.h"
#include <stdint.h>
#include <array>
#include <iomanip>

namespace i3slib
{

namespace utl
{
//! Trait of the type supported by the variant. 
namespace Variant_trait
{
enum class Type : char { Int8 = 0, Uint8, Int16, Uint16, Int32, Uint32, Int64, Uint64, Float, Double, Bool, String, WString, Not_set };
template< class T > inline Type get_type(); //must specialize
template<>          inline Type get_type<char>() { return Type::Int8; }
template<>          inline Type get_type<int8_t>() { return Type::Int8; }
template<>          inline Type get_type<uint8_t>() { return Type::Uint8; }
template<>          inline Type get_type<int16_t>() { return Type::Int16; }
template<>          inline Type get_type<uint16_t>() { return Type::Uint16; }
template<>          inline Type get_type<int32_t>() { return Type::Int32; }
template<>          inline Type get_type<uint32_t>() { return Type::Uint32; }
template<>          inline Type get_type<int64_t>() { return Type::Int64; }
template<>          inline Type get_type<uint64_t>() { return Type::Uint64; }
template<>          inline Type get_type<float>() { return Type::Float; }
template<>          inline Type get_type<double>() { return Type::Double; }
template<>          inline Type get_type<bool>() { return Type::Bool; }
template<>          inline Type get_type<std::string>() { return Type::String; }
template<>          inline Type get_type<std::wstring>() { return Type::WString; }
inline              bool is_string(Type t) { return  t == Type::String || t == Type::WString; }
inline size_t       size_in_bytes(Type t)
{
  static_assert((size_t)Type::Not_set == 13, "Update sizes[]?");
  static const size_t sizes[(size_t)Variant_trait::Type::Not_set] = { 1,1,2,2,4,4,8,8,4,8,1,0,0 };
  return sizes[(size_t)t];
}

}

//! Variant class that can own or "warp" a type values. 
//! "Wrapping" is useful for the utl::serialize framework to add type info to a writable value. 
class I3S_EXPORT Variant
{
public:
  enum class Memory : char { Copy, Shared };

  Variant() = default;
  //constexpr Variant() : m_type(Variant_trait::Type::Not_set), m_memory( Memory::Copy){}
  template< class T > Variant(T* v, Memory ownership) noexcept;
  template< class T > explicit Variant(const T& v) noexcept;
  Variant(const Variant&);
  Variant& operator=(const Variant&);
  ~Variant();

  template< class T > void      set(const T& v) noexcept;
  template< class T > const T&   get() const noexcept;
  
  double                         to_double() const noexcept;
  std::string                    to_string() const;

  template< class Y, class Y_Trait > Y         to_variant() const noexcept;
  Variant_trait::Type           get_type() const { return m_type; }
  bool                          is_valid() const { return m_type != Variant_trait::Type::Not_set; }
  I3S_EXPORT friend std::ostream&          operator<<(std::ostream& out, const Variant& v);
  I3S_EXPORT friend bool                   operator==(const Variant& a, const Variant& b);
  I3S_EXPORT friend bool                   operator<(const Variant& a, const Variant& b);
  // TDB: the following functions are used for binary serialization. 
  void*                         _get_scalar_ptr() noexcept 
  { 
    I3S_ASSERT(m_type != Variant_trait::Type::String && m_type != Variant_trait::Type::WString); 
    return m_memory == Memory::Shared ? m_shared_ptr : &m_scalar_copy; 
  }
  const void*               _get_scalar_ptr() const noexcept { return m_memory == Memory::Shared ? m_shared_ptr : &m_scalar_copy; }
private:
  template< class T > T     _get_scalar_value() const noexcept { I3S_ASSERT(Variant_trait::get_type<T>() == m_type); return *reinterpret_cast<const T*>(_get_scalar_ptr()); };
  const std::string&        _get_string() const noexcept { I3S_ASSERT(m_type == Variant_trait::Type::String);  return *m_string_ptr; } // owned or shared.
  const std::wstring&       _get_wstring() const noexcept { I3S_ASSERT(m_type == Variant_trait::Type::WString);  return *m_wstring_ptr; } // owned or shared.
  void                      _copy_from(const Variant& src) noexcept;

  typedef std::string(*format_str_fct)(const std::string& src);
  static std::ostream&      _to_string_internal(std::ostream& out, const Variant& v, format_str_fct);
private:
  union
  {
    int64_t             m_scalar_copy=0;
    std::string*      m_string_ptr;
    std::wstring*     m_wstring_ptr;
    void*             m_shared_ptr;
  };
  Variant_trait::Type m_type = Variant_trait::Type::Not_set; // 1 byte
  Memory              m_memory= Memory::Copy; //1 byte
};


template< class T >
inline Variant::Variant(const T& v) noexcept
  : m_type(Variant_trait::get_type<T>())
  , m_memory(Memory::Copy)
{
  static_assert(sizeof(v) <= sizeof(m_scalar_copy), "Only <= 64 bit scalar may be used as template parameter.");
  new(&m_scalar_copy) T(v);
}


template<>
inline Variant::Variant(const std::string& v) noexcept
  : m_type(Variant_trait::Type::String)
  , m_memory(Memory::Copy)
{
  m_string_ptr = new std::string(v);
}

template<>
inline Variant::Variant(const std::wstring& v) noexcept
  : m_type(Variant_trait::Type::WString)
  , m_memory(Memory::Copy)
{
  m_wstring_ptr = new std::wstring(v);
}

template< class T >
inline Variant::Variant(T* v, Memory ownership) noexcept
  : m_type(Variant_trait::get_type<T>())
  , m_memory(ownership)
{
  static_assert(std::is_pointer<T>::value == false, "Template type cannot be a pointer");
  static_assert(sizeof(v) <= sizeof(m_scalar_copy), "Only <= 64 bit scalar may be used as template parameter.");
  if (m_memory == Memory::Shared)
    m_shared_ptr = v; //copy the pointer.
  else
    new(&m_scalar_copy) T(*v); //copy the data
}

//!specialized constructor for strings:
template<>
inline Variant::Variant(std::string* v, Memory ownership) noexcept
  : m_type(Variant_trait::Type::String)
  , m_memory(ownership)
{
  if (m_memory == Memory::Shared)
    m_shared_ptr = v;
  else
    m_string_ptr = new std::string(*v);
}

template<>
inline Variant::Variant(std::wstring* v, Memory ownership) noexcept
  : m_type(Variant_trait::Type::WString)
  , m_memory(ownership)
{
  if (m_memory == Memory::Shared)
    m_shared_ptr = v;
  else
    m_wstring_ptr = new std::wstring(*v);
}

inline Variant::~Variant()
{
  if (m_memory == Memory::Copy)
  {
    if (m_type == Variant_trait::Type::String)
      delete m_string_ptr;
    if (m_type == Variant_trait::Type::WString)
      delete m_wstring_ptr;
  } 
}

template< class T > 
inline void Variant::set(const T& v) noexcept
{ 
  static_assert( sizeof(T) <= sizeof( m_scalar_copy), "Expected <= 64 bit scalar" );
  m_type = Variant_trait::get_type<T>(); 
  *reinterpret_cast<T*>(_get_scalar_ptr()) = v;
}

template<>
inline void Variant::set(const std::string& v) noexcept
{
  m_type = Variant_trait::Type::String;
  *m_string_ptr= v; //shared or copy.
}

template<>
inline void Variant::set(const std::wstring& v) noexcept
{
  m_type = Variant_trait::Type::WString;
  *m_wstring_ptr = v; //shared or copy.
}

template< class T > inline const T&  Variant::get() const noexcept
{
  I3S_ASSERT(Variant_trait::get_type<T>() == m_type);
  static_assert(sizeof(T) <= sizeof(m_scalar_copy), "Expected <= 64 bit scalar");
  return *reinterpret_cast<const T*>(_get_scalar_ptr());
}

template<> inline const std::string& Variant::get() const noexcept 
{ 
  return _get_string(); 
}

template<> inline const std::wstring& Variant::get() const noexcept 
{ 
  return _get_wstring(); 
}

#if 0
//! may be used in conjonction with Com_type_adapter to convert to a COM variant. 
template< class Y, class Y_Trait > inline Y Variant::to_variant() const noexcept
{
  switch (m_type)
  {
    case Variant_trait::Type::Bool:  return Y(Y_Trait::adapt(_get_scalar_value< bool >()));
    case Variant_trait::Type::Int8:  return Y(Y_Trait::adapt(_get_scalar_value< int8_t >()));
    case Variant_trait::Type::Uint8: return Y(Y_Trait::adapt(_get_scalar_value< uint8_t >()));
    case Variant_trait::Type::Int16: return Y(Y_Trait::adapt(_get_scalar_value< int16_t >()));
    case Variant_trait::Type::Uint16:return Y(Y_Trait::adapt(_get_scalar_value< uint16_t >()));
    case Variant_trait::Type::Int32: return Y(Y_Trait::adapt(_get_scalar_value< int32_t >()));
    case Variant_trait::Type::Uint32:return Y(Y_Trait::adapt(_get_scalar_value< uint32_t >()));
    case Variant_trait::Type::Int64: return Y(Y_Trait::adapt(_get_scalar_value< int64_t >()));
    case Variant_trait::Type::Uint64:return Y(Y_Trait::adapt(_get_scalar_value< uint64_t >()));
    case Variant_trait::Type::Float: return Y(Y_Trait::adapt(_get_scalar_value< float >()));
    case Variant_trait::Type::Double:return Y(Y_Trait::adapt(_get_scalar_value< double >()));
    case Variant_trait::Type::String:return Y(Y_Trait::adapt(_get_string()));
    case Variant_trait::Type::WString:return Y(Y_Trait::adapt(_get_wstring()));
    default:
      I3S_ASSERT_EXT(false);
      return Y();
  }
}
#endif

}//endof ::utl

} // namespace i3slib
