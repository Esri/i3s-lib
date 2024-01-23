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
#include <stdint.h>
#include <vector> //testing only
#include "utils/utl_i3s_assert.h"

namespace i3slib
{

namespace utl
{

//! Lamba signature: []( int bit_number )->bool{}
template< class T, class Pred >
inline bool for_each_bit_set_conditional(T val, Pred pred)
{
  int loop = 0;
  do
  {
    if (val & 0x1)
      //if (!pred(1 << loop))
      if (!pred(loop))
        return false;//bailout
    ++loop;
  } while (val >>= 1);
  return true;
}

template< class T, class Pred >
inline void for_each_bit_set(T val, Pred pred)
{
  int loop = 0;
  do
  {
    if (val & 0x1)
      pred(loop);
    ++loop;
  } while (val >>= 1);
}

//! return 0 if input =0, return the bitnumber +1 otherwise.
template< class T >
inline constexpr int first_bit_set(T num) noexcept
{
  uint32_t i = 0;
  while (num)
  {
    num >>= 1;
    i++;
  }
  return i;
}

//! return the bit number of the n-th bit set to 1 (or -1 otherwise)
template< class T >
inline int n_th_bit_set(T num, int bit_set_count) noexcept
{
  I3S_ASSERT(bit_set_count); //can't be zero.
  uint32_t i = 0;
  while (num && bit_set_count)
  {
    if (num & (T)0x1)
      --bit_set_count;
    num >>= (T)1;
    i++;
  }
  return bit_set_count == 0 ? i - 1 : -1;
}

//! 
inline int count_bit_set(uint64_t bits, int bit_number)
{
  //count the number of bit sets before ours:
  int count = 0;
  while (bits && bit_number)
  {
    count += bits & 0x1ull;
    bits >>= 1ull;
    --bit_number;
  }
  return count;
}

//! returns true if n == 1u << k for some k
inline constexpr bool is_power_of_two(uint32_t n)
{
  return  (n & (n - 1)) == 0; // checks that there is no bit set in n except the most significant one
}

inline constexpr uint32_t round_up_power_of_two(uint32_t v) noexcept
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

inline uint32_t round_down_power_of_two(uint32_t v) noexcept
{
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v - (v >> 1);
}

inline uint32_t nearest_power_of_two(uint32_t v) noexcept
{
  int hi = round_up_power_of_two(v);
  int lo = (uint32_t)hi >> 1u;
  return (int)v - lo < hi - (int)v ? lo : hi;
}

}//endof ::utl

} // namespace i3slib
