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
#include "utils/utl_gzip_context.h"
#include "utils/utl_gzip.h"

namespace i3slib
{

bool Gzip_context::compress_inplace(std::string* in_out) const
{
  using utl::compress_gzip;
  using utl::compress_gzip;

  Borrowed b = m_gzip_buffers->borrow();

  auto& scratch = b.get().m_scratch;
  I3S_ASSERT(in_out->data() != scratch.data());

  bool res;
  if (m_option == Gzip_with_monotonic_allocator::Yes)
  {
    res = compress_gzip(*in_out, &scratch, b.get().m_scratch_for_monotonic_allocator);
  }
  else
  {
    res = compress_gzip(*in_out, &scratch);
  }
  if (res)
    in_out->swap(scratch);
  return res;
}

}
