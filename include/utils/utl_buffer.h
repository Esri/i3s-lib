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
#include "utils/utl_declptr.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_i3s_export.h"
#include <cstring>

#pragma warning(push)
#pragma warning(disable:4251)

namespace i3slib
{

namespace utl
{
  
enum class Buffer_memory_ownership :int { Shallow = 0, Deep, Deep_aligned };
class Buffer;

//! Typed view of a buffer. Underlying buffer is ref_counted so the Buffer_view control the lifetime of the underlying buffer
//! T may be const or non const type.
template< class T >
class Buffer_view
{
public:
  using value_type = T;
  template< class Y>  friend class  Buffer_view;

  typedef std::remove_const_t< T > Mutable_T;
  Buffer_view() = default;
  Buffer_view(std::shared_ptr< const Buffer> buff, T* ptr, int count);
  Buffer_view(T* ptr, int count, Buffer_memory_ownership memory);
  // SK: can't make the constructor below explicit since for non const T it is just the default copy constructor
  Buffer_view(const Buffer_view<Mutable_T>& src);
  Buffer_view<T>& operator=(const Buffer_view<Mutable_T>& src);

  int           size() const noexcept { return m_count; }

  bool          is_valid() const noexcept { return m_data != nullptr; }
  T&            operator[](int i) noexcept { I3S_ASSERT(i >= 0 && i < m_count); return m_data[i]; }
  const T&      operator[](int i) const noexcept { I3S_ASSERT(i >= 0 && i < m_count); return m_data[i]; }
  T*            data() noexcept { return m_data; }
  const T*      data() const noexcept { return m_data; }
  void          deep_copy();

  template< class Y> Buffer_view<Y>   cast_as();


  T* begin() noexcept { return m_data; }
  const T* begin() const noexcept { return m_data; }
  const T* cbegin() const noexcept { return m_data; }
  T* end() noexcept { return m_data + m_count; }
  const T* end() const noexcept { return m_data + m_count; }
  const T* cend() const noexcept { return m_data + m_count; }
  void shrink(int new_item_count) noexcept { 
    I3S_ASSERT(new_item_count <= m_count && new_item_count >= 0);
    m_count = new_item_count;
  }
  //WARING: no item initialization!, must stay below capacity
  void resize(int new_item_count) noexcept {
    I3S_ASSERT_EXT(new_item_count <= m_original_capacity && new_item_count >= 0);
    m_count = new_item_count;
  }
  const std::shared_ptr<const Buffer >& get_buffer() const { return m_buff; }

private:
  std::shared_ptr<const Buffer > m_buff; //only for lifetime, m_data points the the memory of this underlying buffer (to control constness)
  T*           m_data = nullptr;
  int          m_count = 0;
  int          m_original_capacity = 0;
};

template< class T >
template< class Y> inline Buffer_view<Y>  Buffer_view<T>::cast_as()
{
  I3S_ASSERT((size_t)m_data % sizeof(Y) == 0);// Mis-alignment TBD ?
  return Buffer_view<Y>(m_buff, reinterpret_cast<Y*>(m_data), m_count * sizeof(T) / sizeof(Y));
}

typedef Buffer_view< const char > Raw_buffer_view; //immutable view of byte-buffer

//! Buffer of bytes supporting aligned, shallow (wrap) and deep (copy) block of memory
//! Buffer are always accessed using Buffer_views<> . 
//! Note: not a polymorphic class.
class I3S_EXPORT Buffer : public std::enable_shared_from_this< Buffer >
{
public:
  DECL_PTR(Buffer);
  using Memory = Buffer_memory_ownership;
  Buffer() : m_mode(Memory::Deep) {}
  explicit Buffer(int size_in_bytes) : m_rw_ptr( new char[size_in_bytes]), m_size(size_in_bytes), m_mode(Memory::Deep) {}
  Buffer(const char* ptr, int size, Memory mem, int align=0);
  ~Buffer(); 

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  Raw_buffer_view           create_view(int count = -1, int offset = 0) const;
  Buffer_view<char>         create_writable_view(int count = -1, int offset = 0) ;
  Buffer::Ptr               deep_copy() const;

  template<class T > Buffer_view<const T>   create_typed_view(int item_count, int byte_offset = 0) const;
  
  template<class T > static Buffer_view<T>   create_writable_typed_view(int size);
  template<class T > static Buffer_view<T>   create_deep_copy(const T* src, int size);

  template< class T > static Buffer_view<T>  create_and_own(T* data, int count);
  template< class T > static Buffer_view<T>  create_and_wrap(T* data, int count);
  template< class T > static Buffer_view<T>  move_exising(T** data, int count);
  //static Buffer_view<char>                  create_from_existing(char* data, int bytes);
  static Raw_buffer_view                    create_shallow_view(const char* data, int bytes);

  static Buffer_view<char>                  create_writable_view(const char* data, int bytes);

  const char*         data() const { return m_rw_ptr; }

private:
  union
  {
    //TBD: To work-around const-correctness for shared raw pointer from outside.
    char*         m_rw_ptr = nullptr;
    const char*   m_read_ptr;
  };
  int             m_size = 0;
  Memory          m_mode = Memory::Shallow;
};



// ----------- inline implementation: ----------------
template<class T>
inline Buffer_view<T>::Buffer_view(std::shared_ptr< const Buffer >buff, T* ptr, int count)
  : m_buff(std::move(buff))
  , m_data(ptr)
  , m_count(count)
  , m_original_capacity(count)
{
}

template<class T>
inline Buffer_view<T>::Buffer_view(T* ptr, int count, Buffer_memory_ownership memory)
  : m_buff(std::make_shared<Buffer>(
      reinterpret_cast<const char*>(ptr), static_cast<int>(count * sizeof(T)), memory))
  , m_data(reinterpret_cast<T*>(const_cast<char*>(m_buff->data())))
  , m_count(count)
  , m_original_capacity(count)
{  
}

template<class T>
inline Buffer_view<T>::Buffer_view(const Buffer_view<Mutable_T>& src) :
  m_buff(src.m_buff), m_data(src.m_data), m_count(src.m_count), m_original_capacity(src.m_original_capacity)
{
}

//! shallow assignment:
template<class T>
inline Buffer_view<T>& Buffer_view<T>::operator=(const Buffer_view<Mutable_T>& src)
{
  m_buff = src.m_buff;
  m_count = src.m_count;
  m_data = src.m_data;
  m_original_capacity = src.m_original_capacity;
  return *this;
}


template<class T > inline Buffer_view<T>  Buffer::create_writable_typed_view(int size)
{
  auto buff = std::make_shared< Buffer >((int)(sizeof(T)*size));
  return Buffer_view<T>(buff, reinterpret_cast<T*>(buff->m_rw_ptr), size);
}

template<class T > inline Buffer_view<T> Buffer::create_deep_copy(const T* src, int size)
{
  auto buff = std::make_shared< Buffer >((int)(sizeof(T)*size));
  auto ret = Buffer_view<T>(buff, reinterpret_cast<T*>(buff->m_rw_ptr), size);
  std::memcpy(ret.data(), src, size * sizeof(T));
  return ret;
}



template<class T >
Buffer_view<const T> inline Buffer::create_typed_view(int item_count, int byte_offset) const
{
  if (item_count * sizeof(T) + byte_offset > m_size)
  {
    I3S_ASSERT_EXT(false);
    return  Buffer_view<T>(shared_from_this(), nullptr, 0);
  }

  return Buffer_view<T>(shared_from_this(), reinterpret_cast<T*>(m_rw_ptr + byte_offset), item_count);
}


//deep copy
template< class T >
inline Buffer_view<T>  Buffer::create_and_own(T* data, int count)
{
  auto buffer = std::make_shared< Buffer>(reinterpret_cast<char*>(data), (int)(count * sizeof(T)), utl::Buffer_memory_ownership::Deep);
  return Buffer_view<T>(buffer, data, count);
}
//shallow copy
template< class T >
inline Buffer_view<T>  Buffer::create_and_wrap(T* data, int count)
{
  auto buffer = std::make_shared< Buffer>(reinterpret_cast<char*>(data), (int)(count* sizeof(T)), utl::Buffer_memory_ownership::Shallow );
  return Buffer_view<T>(buffer, data, count);
}
//move
template< class T >
inline Buffer_view<T>  Buffer::move_exising(T** data, int count)
{
  if (data)
  {
    auto buff = std::make_shared< Buffer >();
    buff->m_rw_ptr = reinterpret_cast<char*>(*data);
    buff->m_size = count;
    auto ret = Buffer_view<T>(buff, *data, count);
    *data = nullptr;
    return ret;
  }
  return Buffer_view<T>();
}


// !warning: un - tested.
template<class T > inline void Buffer_view<T>::deep_copy()
{
  if (m_buff)
  {
    auto buff = std::make_shared< Buffer >(reinterpret_cast<const char*>(m_data), m_count * (int)sizeof(T), Buffer::Memory::Deep);
    *this = buff->template create_typed_view<T>(m_count);
  }
}

}

/*
Copies elements of the _same_ type from one buffer to another.
This is a type-safe alternative to memcpy when we know that the elements should have the same type.
*/
template<typename T>
void copy_elements(T* dest, const T* src, size_t n_elements)
{
  memcpy(dest, src, n_elements * sizeof(T));
}

} // namespace i3slib

#pragma warning(pop)
