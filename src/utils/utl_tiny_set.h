/*
Copyright 2021 Esri

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
#include "utils/utl_static_vector.h"

#include <vector>

namespace i3slib::utl
{

namespace detail
{

template<typename Vec>
struct Tiny_unordered_set_base
{
  using T = typename Vec::value_type;

  void clear() { vec.clear(); }

  bool empty() const { return vec.empty(); }

  void reserve(size_t n) { vec.reserve(n); }

  size_t size() const { return vec.size(); }

  bool emplace(T t)
  {
    for (const T& val : vec)
    {
      if (val == t)
        return false;
    }
    vec.push_back(t);
    return true;
  }

  size_t count(const T& t) const
  {
    /* Binary search over a sorted vector of very few elements is slower than
    * linear search so we do linear search.
    */
    for (const T& val : vec)
    {
      if (val == t)
        return 1;
    }
    return 0;
  }

  auto begin() const { return vec.begin(); }
  auto end() const { return vec.end(); }

private:
  Vec vec;
};

}  // NS detail

/*
* |Tiny_unordered_set| uses a std::vector underneath and implements a subset of std::unordered_set's api.
*
* With std::unordered_set, even if memory was reserved upfront, allocation occurs on insertion.
* |Tiny_unordered_set| fixes this behaviour : no memory is allocated on insertion, if memory was reserved.
*
* |Tiny_unordered_set| can be used as a replacement for std::unordered_set<T> when "very few" elements
* live in the container (rule of thumb : sizeof(T) * count_elements <= cacheline_size).
*
* Complexities: Lookups and insertions have a complexity of O(number of elements).
* 
* See also: |Static_tiny_unordered_set|
*/
template<typename T>
using Tiny_unordered_set = detail::Tiny_unordered_set_base<std::vector<T>>;

/*
* |Static_tiny_unordered_set| has the same interface as |Tiny_unordered_set| but uses a |static_vector| underneath.
*
* Prefer |Static_tiny_unordered_set| over |Tiny_unordered_set| if you know the max capacity in advance:
* it uses memory from the stack so it will be faster.
*/
template<typename T, std::uint32_t Capacity>
using Static_tiny_unordered_set = detail::Tiny_unordered_set_base<utl::static_vector<T, Capacity>>;

}
