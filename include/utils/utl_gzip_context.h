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

#include "utl_shared_objects.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>

namespace i3slib
{

// Whether to use a monotonic allocator when compressing and decompressing with Gzip_context.
enum class Gzip_with_monotonic_allocator { No, Yes };

// Gzip_context optimizes compression and decompression times by reusing buffers.
// All methods are thread-safe.
struct Gzip_context
{
  struct Buffers
  {
    // no copy
    Buffers(Buffers const&) = delete;
    Buffers& operator=(Buffers const&) = delete;

    // move
    Buffers(Buffers&& b) noexcept = default;
    Buffers& operator=(Buffers&& b) noexcept = default;

    Buffers() = default;

    std::vector<uint8_t> m_scratch_for_monotonic_allocator;
    std::string m_scratch;
  };
  using Shared_buffers = utl::detail::Shared_objects<Buffers>;
  using Borrowed = Shared_buffers::Borrowed;

  Gzip_context(Gzip_with_monotonic_allocator g)
    : m_option(g)
  {
    m_gzip_buffers = Shared_buffers::Mk_shared();
  }

  bool compress_inplace(std::string* in_out) const;

private:
  std::shared_ptr<Shared_buffers> m_gzip_buffers;
  Gzip_with_monotonic_allocator m_option;
};

}
