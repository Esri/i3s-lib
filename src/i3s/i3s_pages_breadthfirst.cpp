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

#include <deque>
#include "i3s/i3s_pages_breadthfirst.h"

namespace i3slib::i3s
{

status_t Page_builder_breadthfirst::build_pages(
  const size_t node_count,
  const uint32_t root_id,
  const uint32_t page_size,
  const F_on_page_created& on_page,
  std::vector<Node_desc_v17>& nodes)
{

  // we need to re-order the tree breadth first:
  struct Item
  {
    Item(Node_desc_v17* n) : node(n) {}
    Node_desc_v17* node;
  };

  std::deque<Item > queue;
  nodes[root_id].index = 0; //update the root_id : clients expect the root index to be 0
  queue.push_back({ &nodes[root_id] }); //root is node 0
  uint32_t visit_count = 0;
  Node_page_desc_v17 current_page;
  uint32_t page_id = 0;
  I3S_ASSERT_EXT(page_size > 0);
  current_page.nodes.resize(page_size);
  while (!queue.empty())
  {
    Item item = queue.front();
    queue.pop_front();
    ++visit_count;
    uint32_t ch0 = visit_count + (uint32_t)queue.size();
    uint32_t node_id = (visit_count - 1) % page_size;
    //get the children:
    for (uint32_t& ch_id : item.node->children)
    {
      uint32_t updated_id = ch0++;
      //rewrite childen index:
      nodes[ch_id].index = updated_id;
      nodes[ch_id].parent_index = page_id * page_size + node_id;
      queue.push_back({ &nodes[ch_id] });
      //update childen in parent:
      ch_id = updated_id;
    }
    current_page.nodes[node_id] = *item.node;
    //write the page out ?
    if (node_id == page_size - 1)
    {
      auto status = on_page(current_page, page_id++);
      if (status != IDS_I3S_OK)
        return status;
    }
  }
  uint32_t left_over = visit_count % page_size;
  //write the last (incomplete?) page :
  if (left_over)
  {
    current_page.nodes.resize(left_over);
    auto status = on_page(current_page, page_id++);
    if (status != IDS_I3S_OK)
      return status;
  }
  return IDS_I3S_OK;
}

}
