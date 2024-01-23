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

#include "i3s_index_dom.h"

#include <functional>
#include <cstdint>
#include <vector>

namespace i3slib::i3s
{

class Page_builder
{
public:

  using F_on_page_created =
    std::function<status_t(const Node_page_desc_v17& page, const size_t page_index)>;

  /*
  * Groups nodes by pages and reindexes nodes.
  *
  * |on_page| will be called at each created page. If it returns an error,
  * the function stops processing and returns the same error.
  *
  * Each page will have a size of |page_size| except the last page which may be smaller but not empty.
  *
  * |root_id| is the index of the root of the nodes tree.
  *
  * In the input |nodes|, indexes correspond to the implicit indexing of the |nodes| vector.
  * The function modifies these indexes s.t they correspond to the indexes of nodes in pages.
  */
  virtual status_t build_pages(
    const size_t node_count,
    const uint32_t root_id,
    const uint32_t page_size,
    const F_on_page_created& on_page,
    std::vector<Node_desc_v17>& nodes) = 0;
};

}