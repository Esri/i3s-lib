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
#include "utils/utl_md5.h"
#include "utils/utl_i3s_assert.h"
#include "utils/utl_endian.h"
#include <algorithm>
#include <cstring>
#include <stdint.h>

namespace
{

template<typename T>
bool is_aligned_for(const void* ptr) noexcept
{
  return (reinterpret_cast<uintptr_t>(ptr) % alignof(T)) == 0;
}

uint32_t left_rotate(uint32_t x, int n)
{
  return (x << n) | (x >> (32 - n));
}

const uint32_t s[] =
{
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,   // 0..15
  5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,   // 16..31
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,   // 32..47
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21    // 48..63
};

const uint32_t k[] =
{
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, // 0..3
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, // 4..7
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, // 8..11
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, // 12..15
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, // 16..19
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, // 20..23
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, // 24..27
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, // 28..31
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, // 32..35
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, // 36..39
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, // 40..43
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, // 44..47
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, // 48..51
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, // 52..55
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, // 56..59
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391  // 60..63
}; 

}

namespace i3slib
{

namespace utl
{

// Implementation based on the algorithm and pseudo code described in:
// http://en.wikipedia.org/wiki/Md5
// http://tools.ietf.org/html/rfc1321

// TODO: fix endianness issues!

Md5::Md5()
{
  init_();
}

Md5::~Md5() = default;

void Md5::update(const uint8_t* data, size_t length)
{
  // Determine current position (i.e. start of remainder unprocessed by previous chunks)
  // Effectively byte count modulo 64
  uint32_t pos = m_count & 0x3f;
  const uint32_t left = 64 - pos;

  // Running total of number of bytes for finalization
  m_count += static_cast<uint64_t>(length);

  // Process remainder of previous chunk (if we now have enough data).
  if (pos > 0 && left <= length)
  {
    std::memcpy(reinterpret_cast<uint8_t*>(m_chunk_buffer.data()) + pos, data, left);
    process_chunk_(m_chunk_buffer.data());
    data += left;
    length -= left;
    pos = 0;
  }

  // Process full chunks directly from input data.
  if (is_aligned_for<uint32_t>(data))
  {
    for (; length >= 64; data += 64, length -= 64)
      process_chunk_(reinterpret_cast<const uint32_t*>(data));
  }
  else
  {
    for (; length >= 64; data += 64, length -= 64)
      process_chunk_(data);
  }

  // Buffer the remainder for the next chunk.
  if (length > 0)
    std::memcpy(reinterpret_cast<uint8_t*>(m_chunk_buffer.data()) + pos, data, length);
}

void Md5::finalize(Md5::Digest& digest)
{
  constexpr uint8_t c_closing_bit = 0x80;

  // At this point the remainder buffer is guaranteed to have space for at least one more byte
  uint32_t pos = m_count & 0x3f;
  uint32_t left = 64 - pos;
  reinterpret_cast<uint8_t*>(m_chunk_buffer.data())[pos++] = c_closing_bit;
  left--;

  // Pad the current block with zeros up to 56 bytes (448 bits), leaving 8 bytes to store the bit count.
  // If there's less than 8 bytes left in the current block, process it and pad another one.
  // Bit cound is stored as 64-bit little-endian.
  if (left < 8)
  {
    std::memset(reinterpret_cast<uint8_t*>(m_chunk_buffer.data()) + pos, 0, left);
    process_chunk_(m_chunk_buffer.data());
    pos = 0;
    left = 64;
  }

  // Append count to the last 8 bytes of the final block
  // Keep track of the length of the message
  uint64_t message_bits = m_count << 3;
  std::memset(reinterpret_cast<uint8_t*>(m_chunk_buffer.data()) + pos, 0, left - 8);
  boost::endian::native_to_little_inplace(message_bits);
  std::memcpy(m_chunk_buffer.data() + 14, &message_bits, 8);

  // Process the final block.
  process_chunk_(m_chunk_buffer.data());

  // Construct the digest.
  // TODO: possible unaligned access here, would be nice to fix that.
  // TODO: fix endianness.
  uint32_t *dig = reinterpret_cast<uint32_t*>(digest.data());
  dig[0] = m_a;
  dig[1] = m_b;
  dig[2] = m_c;
  dig[3] = m_d;

  // Done, reset state.
  init_();
}

//static
void Md5::hash(const uint8_t* data, size_t length, Digest& digest)
{
  Md5 hasher;
  hasher.update(data, length);
  hasher.finalize(digest);
}

//static
std::string Md5::to_string(const Digest& digest)
{
  static const char hex_digits[] = "0123456789abcdef";

  std::string hex;
  hex.reserve(32);
  for(const auto c : digest)
  {
    hex.push_back(hex_digits[c >> 4]);
    hex.push_back(hex_digits[c & 0x0f]);
  }

  I3S_ASSERT(hex.size() == 32);
  return hex;
}

bool from_hex(char h, unsigned char& out)
{
  char base = -1;
  if (h >= '0' && h <= '9')
    base = '0';
  else if (h >= 'a' && h <= 'f')
    base = 'a' - 10;
  else if (h >= 'A' && h <= 'F')
    base = 'A' - 10; 
  out = h - base;
  return base != -1;
}

//static
bool Md5::from_string(const std::string& in, Md5::Digest& out)
{
  if (in.size() != 32)
    return false;
  out.fill(0);
  unsigned char v;
  for (uint32_t i = 0; i < 32; ++i)
  {
    if (!from_hex(in[i], v))
      return false;
    out[i>> 1u] |= v << ( (1-(i&1u)) << 2u);
  }
  return true;
}

// Initialize internal state
void Md5::init_()
{
  m_a = 0x67452301; //A
  m_b = 0xefcdab89; //B
  m_c = 0x98badcfe; //C
  m_d = 0x10325476; //D
  m_count = 0;
  std::fill(std::begin(m_chunk_buffer), std::end(m_chunk_buffer), 0);
}

// Process a single 512-bit message chunk represented as 16 * 32-bit integers.
void Md5::process_chunk_(const uint32_t* m)
{
  uint32_t a = m_a;
  uint32_t b = m_b;
  uint32_t c = m_c;
  uint32_t d = m_d;
  int i = 0;

  const auto rotate = [m, &i, &a, &b, &c, &d](uint32_t f, uint32_t g)
  {
    const auto t = d;
    d = c;
    c = b;
    b += left_rotate(a + f + k[i] + m[g], s[i]);
    a = t;
  };

  for (; i < 16; ++i)
    rotate((b & c) | ((~b) & d), i);

  for (; i < 32; ++i)
    rotate((d & b) | ((~d) & c), (i * 5 + 1) & 0x0f);

  for (; i < 48; ++i)
    rotate(b ^ c ^ d, (i * 3 + 5) & 0x0f);

  for (; i < 64; ++i)
    rotate(c ^ (b | (~d)), (i * 7) & 0x0f);

  m_a += a;
  m_b += b;
  m_c += c;
  m_d += d;
}

// Process a single 512-bit message chunk represented as 128 bytes,
// which is most likely not 32-bit aligned.
void Md5::process_chunk_(const uint8_t* data)
{
  // TODO: add endianness support.
  std::memcpy(m_chunk_buffer.data(), data, 4);
  process_chunk_(m_chunk_buffer.data());
}

} // namespace utl

} // namespace i3slib
