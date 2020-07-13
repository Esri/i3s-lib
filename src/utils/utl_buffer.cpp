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
#include "utils/utl_buffer.h"
#include "utils/utl_platform_def.h"

namespace i3slib
{

namespace utl
{

#pragma warning(push)
#pragma warning(disable: 4724) //potential mod by 0

Buffer::Buffer(const char* ptr, int size, Memory mem, int align)
  : m_read_ptr(ptr) //union.
  , m_size(size)
  , m_mode(mem)
{
  if (mem == Memory::Deep)
  {
    m_rw_ptr = new char[size];
    if (ptr)
      memcpy(m_rw_ptr, ptr, size);
  }
  else if (mem == Memory::Deep_aligned)
  {
    if (align == 0)
      align = 16; //default
    int rem = (size % align);
    if (rem)
      size += align - rem; // size must be multiple of align
    I3S_ASSERT_EXT(align > 0 && size % align == 0);
    //TODO: c++17 has std::aligned_malloc()
    m_rw_ptr = (char*)_aligned_malloc( size, align );

    if (ptr)
      memcpy(m_rw_ptr, ptr, size);
  }
}
#pragma warning(pop)

Buffer::~Buffer()
{
  if (m_mode == Memory::Deep)
    delete[] m_rw_ptr;
  else if (m_mode == Memory::Deep_aligned)
    _aligned_free(m_rw_ptr);
}

//!warning: un-tested.
Buffer::Ptr  Buffer::deep_copy() const
{
  I3S_ASSERT_EXT(m_mode != Memory::Deep_aligned); //not supported. 
  return std::make_shared< Buffer >(m_read_ptr, m_size, m_mode);
}


Raw_buffer_view Buffer::create_shallow_view(const char* data, int bytes)
{
  return std::make_shared< Buffer >(data, bytes, Buffer::Memory::Shallow)->create_view();
}


Buffer_view<char> Buffer::create_writable_view(const char* data, int bytes)
{
  auto buff = std::make_shared< Buffer >(data, bytes, Memory::Deep);
  return Buffer_view<char>(buff, buff->m_rw_ptr, bytes);
}


Raw_buffer_view Buffer::create_view(int count, int offset) const
{
  if (count <= 0)
    count = m_size;
  if (offset + count > m_size)
    return Raw_buffer_view(shared_from_this(), nullptr, 0);

  return Raw_buffer_view(shared_from_this(), m_rw_ptr + offset, count);
}

Buffer_view<char> Buffer::create_writable_view(int count, int offset)
{
  if (count <= 0)
    count = m_size;
  if (offset + count > m_size)
    return Buffer_view<char>(shared_from_this(), nullptr, 0);

  return Buffer_view<char>(shared_from_this(), m_rw_ptr + offset, count);
}

}

} // namespace i3slib
