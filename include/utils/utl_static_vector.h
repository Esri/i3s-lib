/*
Copyright 2020-2023 Esri

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

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <new>
#include <iterator>
#include <initializer_list>
#include <cstring>
#include "utils/utl_i3s_assert.h"

namespace i3slib
{

namespace utl
{

template<typename Input_iterator>
using Require_input_iterator = std::enable_if_t<
  std::is_convertible_v<typename std::iterator_traits<Input_iterator>::iterator_category,
  std::input_iterator_tag> >;

// the class provides fixed capacity vector functionality
// 
template<class T, std::uint32_t Capacity>
class static_vector
{
private:
  static_assert(Capacity > 0);
  static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");
  using self = static_vector<T, Capacity>;

public:

  using value_type = typename std::remove_cv<T>::type;
  using size_type = decltype(Capacity);
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = T const*;
  using reference = T&;
  using const_reference =  T const&;
  using iterator = T*;
  using const_iterator = T const*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  ~static_vector()
  {
    unsafe_destroy_all();
  }

  static_vector() noexcept = default;

  explicit static_vector(size_type n) noexcept(noexcept(T()))
  {
    I3S_ASSERT_EXT(n <= Capacity);
    if constexpr (std::is_nothrow_default_constructible_v<value_type>)
    {
      for (size_type i = 0; i < n; ++i)
        new(&m_data[i]) T();
      m_size = n;
    }
    else
    {
      for (size_type i = 0; i < n; ++i, ++m_size)
        new(&m_data[i]) T();
    }
  }

  static_vector(size_type n, T const& value) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  {
    I3S_ASSERT_EXT(n <= Capacity);
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>
      || std::is_trivially_destructible_v<value_type>)
    {
      for (size_type i = 0; i < n; ++i)
        new(&m_data[i]) T(value);
      m_size = n;
    }
    else
    {
      try {
        for (size_type i = 0; i < n; ++i, ++m_size)
          new(&m_data[i]) T(value);
      }
      catch (...)
      {
        unsafe_destroy_all();
        throw;
      }
    }
  }

  template <class Input_iterator, typename = Require_input_iterator<Input_iterator>>
  static_vector(Input_iterator first, Input_iterator last) noexcept(noexcept(T(*first++)))
  {
    if constexpr (std::is_nothrow_copy_constructible_v<value_type> 
      || std::is_trivially_destructible_v<value_type>)
    {
      if constexpr (std::is_convertible_v<typename std::iterator_traits<Input_iterator>::iterator_category,
        std::random_access_iterator_tag>
      )
      {
        const auto n = static_cast<size_type>(std::distance(first, last));
        I3S_ASSERT_EXT(n <= Capacity);
        for (size_type i = 0; i < n; ++i)
          new(&m_data[i]) T(*first++);
        m_size = n;
      }
      else
      {
        size_type n = 0;
        for (auto data_it = &m_data[0]; first != last; ++first, ++n)
        {
          I3S_ASSERT_EXT(n < Capacity);
          new(data_it++) T(*first);
        }
        m_size = n;
      }
    }
    else
    {
      try {
        for (auto data_it = &m_data[0]; first != last; ++first, ++m_size)
        {
          I3S_ASSERT_EXT(m_size < Capacity);
          new(data_it++) T(*first);
        }
      }
      catch (...)
      {
        unsafe_destroy_all();
        throw;
      }
    }
  }

  static_vector(static_vector const& src)noexcept(
    noexcept(std::is_nothrow_copy_constructible_v<value_type>))
  {
    const auto n = src.size();
    I3S_ASSERT_EXT(n <= Capacity);
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>
      || std::is_trivially_destructible_v<value_type>)
    {
      if constexpr (std::is_trivially_copyable_v<value_type>)
      {
        std::memcpy(&m_data[0], &src.m_data[0], sizeof(value_type) * n);
      }
      else
      {
        auto data_it = &m_data[0];
        for (const auto& v : src)
          new(data_it++) T(v);
      }
      m_size = n;
    }
    else
    {
      try {
        auto data_it = &m_data[0];
        for (const auto& v : src)
        {
          new(data_it++) T(v);
          ++m_size;
        }
      }
      catch (...)
      {
        unsafe_destroy_all();
        throw;
      }
    }
  }

  static_vector(std::initializer_list<T> const& il)
    noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  {
    I3S_ASSERT(il.size() <= Capacity);
    if constexpr (std::is_nothrow_copy_constructible_v<value_type>
      || std::is_trivially_destructible_v<value_type>)
    {
      auto data_it = &m_data[0];
      for (const auto& v : il)
        new(data_it++) T(v);
      m_size = static_cast<size_type>(il.size());
    }
    else
    {
      try {
        auto data_it = &m_data[0];
        for (const auto& v : il)
        {
          new(data_it++) T(v);
          ++m_size;
        }
      }
      catch (...)
      {
        unsafe_destroy_all();
        throw;
      }
    }
  }

  template< typename = std::enable_if_t<std::is_nothrow_move_constructible_v<value_type> > >
  static_vector(static_vector&& src)noexcept
  {
    const auto n = src.size();
    I3S_ASSERT_EXT(n <= Capacity);
    if constexpr (std::is_trivially_move_constructible_v<value_type>)
    {
      std::memcpy(&m_data[0], &src.m_data[0], sizeof(value_type) * n);
    }
    else
    {
      auto data_it = &m_data[0];
      for (auto& v : src)
        new(data_it++) T(std::move(v));
    }
    src.clear();
    m_size = n;
  }

  template <typename = std::enable_if_t<std::is_nothrow_move_assignable_v<value_type> > >
  static_vector& operator=(static_vector&& src) noexcept
  {
    if (this != std::addressof(src)) 
    {
      const auto n = src.size();
      I3S_ASSERT_EXT(n <= Capacity);
      if constexpr (std::is_trivially_move_assignable_v<value_type>)
      {
        std::memcpy(&m_data[0], &src.m_data[0], sizeof(value_type) * n);
        static_assert(std::is_trivially_destructible_v<value_type>);
        src.m_size = 0;;
      }
      else
      {
        unsafe_destroy_all();
        auto data_it = &m_data[0];
        for (auto& v : src)
          new(data_it++) T(std::move(v));
      }
      src.clear();
      m_size = n;
    }
    return *this;
  }

  template<typename ...Args> 
  reference emplace_back(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
  {
    I3S_ASSERT_EXT(m_size < Capacity);
    auto* p = new(&m_data[m_size]) T(std::forward<Args>(args)...);
    ++m_size;
    return *p;
  }

  template <typename S = T> 
  std::enable_if_t<std::is_copy_constructible_v<S>>
      push_back(const T& val) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  {
    emplace_back(val);
  }
  
  template <typename S = T>
  std::enable_if_t<std::is_copy_constructible_v<S>>
    push_back(T&& val) noexcept(std::is_nothrow_move_constructible_v<value_type>)
  {
    emplace_back(std::move(val));
  }
  
  void assign(size_type n, T const& value) noexcept(
    noexcept(std::is_nothrow_copy_assignable_v<value_type>) && 
    noexcept(std::is_nothrow_copy_constructible_v<value_type>))
  {
    I3S_ASSERT_EXT(n <= Capacity);
    if (n < m_size)
    {
      unsafe_destroy(data() + n, data() + m_size);
      m_size = n;
    }
    std::fill_n(data(), m_size, value);
    if(n > m_size)
    {
      for (size_type i = m_size; i < n; ++i, ++m_size)
        new(&m_data[i]) T(value);
    }
  }

  template <class Input_iterator,typename = Require_input_iterator<Input_iterator>>
  void assign(Input_iterator first, Input_iterator last) noexcept(noexcept(T(*first++)))
  {
    clear();
    for (auto data_it = &m_data[0]; first != last; ++first, ++m_size)
    {
      I3S_ASSERT_EXT(m_size <= Capacity);
      new(data_it++) T(*first);
    }
  }

  static_vector& operator=(const static_vector& src) 
  {
    if (this != std::addressof(src)) 
    {
      const auto n = src.m_size;
      I3S_ASSERT(n <= Capacity);
      clear();
      if (std::is_trivially_copyable_v<value_type>)
      {
        std::memcpy(&m_data[0], &src.m_data[0], sizeof(value_type) * n);
        m_size = n;
      }
      else
      {
        auto data_it = &m_data[0];
        for (const auto& v: src)
        {
          new(data_it++) T(v);
          ++m_size;
        }
      }
    }
    return *this;
  }

  static_vector& operator=(std::initializer_list<value_type>& il) 
  {
    I3S_ASSERT(il.size() <= Capacity);
    clear();
    auto data_it = &m_data[0];
    for (const auto& v : il)
    {
      new(data_it++) T(v);
      ++m_size;
    }
    return *this;
  }
  
  void resize(const size_type new_size) noexcept(noexcept(T()))
  {
    if (new_size < m_size)
    {
      unsafe_destroy(data() + new_size, data() + m_size);
      m_size = new_size;
    }
    else if(new_size > m_size)
    {
      I3S_ASSERT_EXT(new_size <= Capacity);
      if constexpr (std::is_nothrow_default_constructible_v<value_type>)
      {
        for (size_type i = m_size; i < new_size; ++i)
          new(&m_data[i]) T();
        m_size = new_size;
      }
      else
      {
        for (size_type i = m_size; i < new_size; ++i, ++m_size)
          new(&m_data[i]) T();
      }
    }
  }

  void resize(const size_type new_size, const T& value) 
    noexcept(std::is_nothrow_copy_constructible_v<value_type>)
  {
    if (new_size < m_size)
    {
      unsafe_destroy(data() + new_size, data() + m_size);
      m_size = new_size;
    }
    else if (new_size > m_size)
    {
      I3S_ASSERT_EXT(new_size <= Capacity);
      if constexpr (std::is_nothrow_copy_constructible_v<value_type>)
      {
        for (size_type i = m_size; i < new_size; ++i)
          new(&m_data[i]) T(value);
        m_size = new_size;
      }
      else
      {
        for (size_type i = m_size; i < new_size; ++i, ++m_size)
          new(&m_data[i]) T(value);
      }
    }
  }

  void reserve([[maybe_unused]] size_type new_capacity) noexcept
  {
    I3S_ASSERT(new_capacity <= Capacity);
  }
  void shrink_to_fit() noexcept
  {
  }

  void pop_back() noexcept
  {
    I3S_ASSERT(m_size);
    --m_size;
  }
  void clear() noexcept
  {
    unsafe_destroy_all();
    m_size = 0;
  }

  void swap(static_vector& rhs) noexcept(std::is_nothrow_swappable_v<value_type>)
  {
    static_assert(std::is_nothrow_move_constructible_v<value_type>,
      "please, avoid throwing move constructors");
    if (this == std::addressof(rhs))
      return;
    auto lhs_data = data();
    auto rhs_data = rhs.data();
    auto* lhs_size = &m_size;
    auto* rhs_size = &rhs.m_size;

    if (*rhs_size < *lhs_size)
    {
      std::swap(rhs_size, lhs_size);
      std::swap(lhs_data, rhs_data);
    }

    if (*lhs_size)
      unsafe_swap(lhs_data, rhs_data, *lhs_size);
    auto to_move_count = *rhs_size - *lhs_size;
    if (to_move_count)
      unsafe_move(rhs_data + *lhs_size, lhs_data + *lhs_size,
        to_move_count, *rhs_size, *lhs_size);
  }

  [[nodiscard]]
  const_reference operator[](difference_type pos) const
  {
    I3S_ASSERT(static_cast<size_t>(pos) < static_cast<size_t>(m_size));
    return *std::launder(reinterpret_cast<T const*>(&m_data[pos]));
  }

  [[nodiscard]]
  reference operator[](difference_type pos)
  {
    I3S_ASSERT(static_cast<size_t>(pos) < static_cast<size_t>(m_size));
    return *std::launder(reinterpret_cast<T*>(&m_data[pos]));
  }

  [[nodiscard]]
  constexpr pointer data() noexcept {
    return std::launder(reinterpret_cast<T*>(&m_data[0]));
  }

  [[nodiscard]] 
  constexpr const_pointer data() const noexcept {
    return std::launder(reinterpret_cast<const T*>(&m_data[0]));
  }

  [[nodiscard]]
  constexpr iterator begin() noexcept {
    return data();
  }

  [[nodiscard]]
  constexpr const_iterator begin() const noexcept {
    return data();
  }

  [[nodiscard]]
  constexpr const_iterator cbegin() const noexcept {
    return begin();
  }

  [[nodiscard]]
  constexpr iterator end() noexcept {
    return data() + m_size;
  }

  [[nodiscard]]
  constexpr const_iterator end() const noexcept {
    return data() + m_size;
  }

  [[nodiscard]]
  constexpr const_iterator cend() const noexcept {
    return end();
  }

  [[nodiscard]]
  constexpr reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }

  [[nodiscard]]
  constexpr const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  [[nodiscard]]
  constexpr const_reverse_iterator crbegin() const noexcept {
    return rbegin();
  }

  [[nodiscard]]
  constexpr reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }

  [[nodiscard]]
  constexpr const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  [[nodiscard]]
  constexpr const_reverse_iterator crend() const noexcept {
    return rend();
  }

  [[nodiscard]]
  constexpr size_type size() const noexcept {
    return m_size;
  }

  [[nodiscard]]
  static constexpr size_type capacity() noexcept {
    return Capacity;
  }

  [[nodiscard]]
  constexpr size_type max_size() const noexcept {
    return Capacity;
  }

  [[nodiscard]]
  constexpr bool empty() const noexcept {
    return m_size == 0;
  }

  [[nodiscard]]
  reference at(size_type pos) noexcept {
    I3S_ASSERT(pos < m_size);
    return (*this)[pos];
  }

  [[nodiscard]]
  const_reference at(size_type pos) const noexcept {
    I3S_ASSERT(pos < m_size);
    return (*this)[pos];
  }

  [[nodiscard]]
  reference front() noexcept {
    I3S_ASSERT(!empty());
    return *data();
  }

  [[nodiscard]]
  const_reference front() const noexcept {
    I3S_ASSERT(!empty());
    return *data();
  }

  [[nodiscard]]
  reference back() noexcept {
    I3S_ASSERT(!empty());
    return data()[m_size - 1];
  }

  [[nodiscard]]
  const_reference back() const noexcept {
    I3S_ASSERT(!empty());
    return data()[m_size - 1];
  }

private:
  void unsafe_move(T* src, T* dest, size_type item_count, size_type& src_count, size_type& dest_count) 
    noexcept(std::is_nothrow_move_constructible_v<value_type>)
  {
    static_assert(std::is_nothrow_move_constructible_v<value_type>,
      "please, avoid throwing move constructors");
    if constexpr (std::is_trivially_move_constructible_v<value_type>)
    {
      std::memcpy(dest, src, sizeof(value_type) * item_count);
      src_count -= item_count;
      dest_count += item_count;
    }
    else 
    {
      for (size_type i = 0; i < item_count; ++i)
      {
        new(&dest[i]) T(std::move(src[i]));
      }
      src_count -= item_count;
      dest_count += item_count;
    }
  }

  void unsafe_swap(T* lhs, T* rhs, size_type item_count) noexcept(std::is_nothrow_swappable_v<value_type>)
  {
    for (size_type i = 0; i < item_count; ++i, ++lhs, ++rhs)
      lhs->swap(*rhs);
  }
  void unsafe_destroy(iterator first, iterator last) noexcept// doesn't change m_size
  {
    if (!std::is_trivially_destructible_v<value_type>)
    {
      for (; first != last; ++first)
        first->~T();
    }
  }

  void unsafe_destroy_all() noexcept// doesn't change m_size
  {
    if (!std::is_trivially_destructible_v<value_type>)
      unsafe_destroy(data(), data() + m_size);
  }
  // properly aligned uninitialized storage for N T's
  typename std::aligned_storage<sizeof(T), alignof(T)>::type m_data[Capacity];
  size_type m_size = 0;
};
}
} // namespace i3slib
