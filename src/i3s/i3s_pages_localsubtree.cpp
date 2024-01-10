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

#include "i3s/i3s_pages_localsubtree.h"

#include "utils/utl_tree_partition.h"

#define I3S_PAGES_LOCAL_SUBTREE_STATS_ALL 0  // depends on I3S_UTL_TREE_PARTITION_WITH_STATS

#if I3S_PAGES_LOCAL_SUBTREE_STATS_ALL
#include <iostream>
#endif

namespace i3slib::i3s
{

Page_builder_localsubtree::Page_builder_localsubtree(Max_count_sibling_local_subtrees n)
  : m_max_count_sibling_local_subtrees(n)
{}

status_t Page_builder_localsubtree::build_pages(
  const size_t node_count,
  const uint32_t root_id,
  const uint32_t page_size,
  const F_on_page_created& on_page,
  std::vector<Node_desc_v17>& nodes)
{
  using Node_index_t = decltype(std::declval<Node_desc_v17>().index);

  // Detect whether node indexes are packed.
  Node_index_t min_index = std::numeric_limits<Node_index_t>::max();
  Node_index_t max_index = std::numeric_limits<Node_index_t>::lowest();
  for (const Node_desc_v17& node : nodes)
  {
    min_index = std::min(min_index, node.index);
    max_index = std::max(max_index, node.index);
  }

  const bool packed = (min_index == 0) && (max_index == static_cast<Node_index_t>(nodes.size() - 1));

  using Pages = utl::treepartition::Pages<Node_index_t>;
  using Children = utl::treepartition::Children<Node_index_t>;

  using utl::treepartition::packed_indexes_tag;
  using utl::treepartition::unpacked_indexes_tag;

  Pages pages;

  auto get_children = [&](Node_index_t i) -> Children {
    std::vector<Node_index_t>& children = nodes[i].children;
    return { children.data(), children.data() + children.size() };
  };

  auto get_priority = [&](const Node_index_t parent) -> float {
    // Clients will load the page of a node when its _parent_ node is split.
    // So we use the parent obb here.
    return nodes[parent].obb.radius();
  };

  std::vector<int> page_sizes;

#if I3S_PAGES_LOCAL_SUBTREE_STATS_ALL
  for (int i = 1; i < 1024; i *= 2)
  {
    page_sizes.push_back(i);
  }
#endif

  page_sizes.push_back(page_size);

  auto run = [&](auto packed_tag) {
    I3S_ASSERT_EXT(page_sizes.back() == page_size);  // verify that the page size specified as argument will be the one used for the results.

    for (auto sz : page_sizes)
    {
      pages.clear();
      utl::treepartition::Tree_partitioner<Node_index_t, decltype(packed_tag)::val>
        p(nodes.size(),
          root_id,
          sz,
          m_max_count_sibling_local_subtrees.unsafe_get());
      p(pages, get_children, get_priority);

#if I3S_PAGES_LOCAL_SUBTREE_STATS_ALL
      // todo these stats could be returned via the context instead?
      std::cout << "--- " << std::endl;
      if (!pages.pages.empty())
        std::cout << "max page size: " << pages.pages[0].size() << std::endl;
      std::cout << "count pages: " << pages.pages.size() << std::endl;
      std::cout << "avg obb tests " << pages.stats.average_count_obb_tests_per_dive << std::endl;
      std::cout << "avg pages all siblings " << pages.stats.avg_pages_per_dive.traversing_all_siblings_per_level << std::endl;
      std::cout << "avg pages single node " << pages.stats.avg_pages_per_dive.traversing_single_node_per_level << std::endl;
      std::cout << "avg leaf depth " << pages.stats.average_leaf_depth << std::endl;
      std::cout << "count leaves " << pages.stats.count_leafs << std::endl;
#endif
    }
  };

  if (packed)
  {
    run(packed_indexes_tag{});
  }
  else
  {
    run(unpacked_indexes_tag{});
  }

  // reindex nodes

  {
    Node_index_t new_index = 0;
    for (size_t i = 0, sz = pages.pages.size(); i < sz; ++i)
    {
      const std::vector<Node_index_t>& page_nodes = pages.pages[i];

      I3S_ASSERT_EXT(!page_nodes.empty());
      if (i < sz - 1)
      {
        I3S_ASSERT_EXT(page_nodes.size() == page_size);
      }
      else
      {
        I3S_ASSERT_EXT(page_nodes.size() <= page_size);
      }

      // sanity : verify that the first page contains the root in first position
      if (i == 0)
      {
        I3S_ASSERT_EXT(page_nodes[0] == root_id);
      }

      for (const Node_index_t old_node_index : page_nodes)
      {
        nodes[old_node_index].index = new_index++;
      }
    }
    I3S_ASSERT_EXT(node_count == new_index);
  }

  // reindex parents and children

  for (Node_desc_v17& node : nodes)
  {
    if (node.parent_index != std::numeric_limits<decltype(node.parent_index)>::max())
      node.parent_index = nodes[node.parent_index].index;
    for (Node_index_t& ch : node.children)
    {
      ch = nodes[ch].index;
    }
  }

  // write pages

  Node_page_desc_v17 current_page;

  for (size_t i = 0, sz = pages.pages.size(); i < sz; ++i)
  {
    const std::vector<Node_index_t>& page_nodes = pages.pages[i];

    current_page.nodes.resize(page_nodes.size());
    int j = 0;
    for (const Node_index_t old_node_index : page_nodes)
    {
      current_page.nodes[j++] = nodes[old_node_index];
    }

    auto status = on_page(current_page, i);
    if (status != IDS_I3S_OK)
      return status;
  }

  return IDS_I3S_OK;
}

}
