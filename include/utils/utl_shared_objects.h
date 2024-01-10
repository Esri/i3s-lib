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

#include <memory>
#include <mutex>
#include <vector>

namespace i3slib::utl::detail
{

/*
* Shared_objects is a thread-safe pool of objects.
* 
* You can borrow one object, use it, and then (using RAII) the object returns to the pool.
* 
* The pool grows if no object is available when you request one. New objects are default-constructed.
* 
* We use a std::mutex for synchronization, so if you think contention will be an issue for your use case,
* you should not use it.
* 
* If you want to make sure that no copies of T will be done you should disable copy construtors of T.
* (see the comment below about is_nothrow_move_constructible_v)
*/
template<typename T>
class Shared_objects : public std::enable_shared_from_this<Shared_objects<T>>
{
  static_assert(std::is_default_constructible_v<T>);
  // Note: static_assert(std::is_nothrow_move_constructible_v<T>); is not enough to make sure that a no except move constructor exists,
  // it could be true if a noexcept copy constructor exists.
  static_assert(std::is_nothrow_move_constructible_v<T>);
  static_assert(std::is_nothrow_move_assignable_v<T>);

public:
  // Instances of this class must be wrapped in a std::shared_ptr
  // because we use 'shared_from_this' inside.
  // Hence we declare the constructors private and use this static function
  // to create instances.
  template<typename ...Args>
  static std::shared_ptr<Shared_objects> Mk_shared(Args&&...  args)
  {
    // we do this trick because the construtor of Shared_objects is private
    struct Concrete : public Shared_objects
    {
      Concrete(Args&&... args) : Shared_objects(std::forward<Args>(args)...) {}
    };
    return std::make_shared<Concrete>(std::forward<Args>(args)...);
  }

  struct Borrowed
  {
    friend class Shared_objects;

    // no copy
    Borrowed(Borrowed const&) = delete;
    void operator=(Borrowed const&) = delete;

    ~Borrowed()
    {
      m_pool->return_one(std::move(m_object));
    }
    T& get()
    {
      return m_object;
    }
  private:
    T m_object;
    std::shared_ptr<Shared_objects<T>> m_pool;

    Borrowed(std::shared_ptr<Shared_objects<T>> const& pool)
      : m_object(pool->get_one())
      , m_pool(pool)
    {
      I3S_ASSERT_EXT(m_pool);
    }
  };

  Borrowed borrow()
  {
    return Borrowed{ this->shared_from_this() };
  }

private:
  Shared_objects(int initial_size = 0) :
    m_objects(initial_size)
  {}

  T get_one()
  {
    {
      std::unique_lock l(m_mut);
      if (!m_objects.empty())
      {
        T res = std::move(m_objects.back());
        m_objects.pop_back();

        l.unlock();
        return res;
      }
    }
    return T();
  }

  void return_one(T && obj)
  {
    std::unique_lock l(m_mut);
    m_objects.push_back(std::move(obj));
  }

private:
  // protects accesses to 'm_objects'
  std::mutex m_mut;

  std::vector<T> m_objects;
};
} // namespace i3slib::utl::detail
