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
#include "i3s_pages.h"

namespace i3slib::i3s
{

/*
The max number of local subtrees that are in a page.
If several local subtrees are in the same page, their roots must be siblings.
*/
struct Max_count_sibling_local_subtrees
{
  explicit Max_count_sibling_local_subtrees(int n)
    : count(n)
  {}

  int unsafe_get() const { return count; }

private:
  int count;
};


class Page_builder_localsubtree final : public Page_builder
{
public:
  I3S_EXPORT explicit Page_builder_localsubtree(Max_count_sibling_local_subtrees);
  /*

  The local subtree algo has 2 passes:

  - In the top down pass, we create pages by grouping descendants of a single node until the page is full.
     - Clients will need to load the page of a node when that node is displayed, hence when the parent is split.
       So we use the obb size of the _parent_ node as a criteria to prioritize descendants.
     - If there are not enough descendants to create a full page, we don't create the page.
  - Then we traverse remaining nodes in post order and create pages greedily.


  Note : to achieve good geographical locality for pages created in the second pass of the algorithm,
  it is recommended to create the tree s.t consecutive nodes in a post order traversal are geographically close.
  One way to achieve this is to use a quadtree with square tiles that are recursively subdivided in 4 square tiles,
  and to create the tree s.t each node corresponds to one tile, and to order the children of the nodes according to
  their index on the Hilbert curve whose extent corresponds to the root tile.

  */
  I3S_EXPORT status_t build_pages(
    const size_t node_count,
    const uint32_t root_id,
    const uint32_t page_size,
    const F_on_page_created& on_page,
    std::vector<Node_desc_v17>& nodes) override;

private:
  Max_count_sibling_local_subtrees m_max_count_sibling_local_subtrees;
};

}