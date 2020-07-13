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
#include "utils/utl_i3s_assert.h"
#include <stdint.h>
#include <array>
#include <string>

namespace i3slib
{

namespace utl
{
using Md5_digest = std::array<unsigned char, 16>;

/// class MD5
/// \brief Class to create MD5 Message digests.
/// \remarks MD5 digest is created by calling update zero or more times to add data followed by call to finalize.
class I3S_EXPORT Md5
{
public:

  using Digest = Md5_digest;

  Md5();
  ~Md5();

  Md5(const Md5&) = delete;
  Md5(Md5&&) = delete;
  Md5& operator=(const Md5&) = delete;
  Md5& operator=(Md5&&) = delete;

  /// \brief Add a block of data to the digest
  /// \param [in] unsigned char* buf - pointer to data buffer
  /// \param [in] length - number of bytes of data
  void update(const uint8_t* data, size_t length);

  /// \brief Finalize the message and return the digest
  void finalize(Digest& digest);

  /// \brief Compute MD5 digest in a single step.
  static void hash(const uint8_t* data, size_t length, Digest& digest);

  static std::string to_string(const Digest& digest);

private:

  void init_();
  void process_chunk_(const uint32_t* m);
  void process_chunk_(const uint8_t* data);

  uint32_t m_a;
  uint32_t m_b;
  uint32_t m_c;
  uint32_t m_d;
  uint64_t m_count; // total consumed byte count
  std::array<uint32_t, 16> m_chunk_buffer;
};

class I3S_EXPORT Md5_helper
{
public:

  const Md5::Digest& digest()
  {
    if (!m_is_final)
    {
      m_ctx.finalize(m_dig);
      m_is_final = true;
    }

    return m_dig;
  }

  // TODO: fix endianness issues
  template<typename T>
  Md5_helper& add(const T& v)
  {
    I3S_ASSERT(!m_is_final);
    m_ctx.update(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    return *this;
  }

  template<typename It>
  Md5_helper& add(It begin, It end)
  {
    for (auto it = begin; it != end; ++it)
      add(*it);

    return *this;
  }

  template<class Ar_t>
  Md5_helper& add_array(const Ar_t& v)
  {
    I3S_ASSERT(!m_is_final);
    if (v.size())
      m_ctx.update(reinterpret_cast<const uint8_t*>(v.data()), v.size() * sizeof(v[0]));

    return *this;
  }

private:

  utl::Md5 m_ctx;
  Md5::Digest m_dig;
  bool m_is_final = false;
};

} // namespace utl

} // namespace i3slib
