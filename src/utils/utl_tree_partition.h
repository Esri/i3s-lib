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
#include "utils/utl_tiny_set.h"

#include <deque>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <limits>

#define I3S_UTL_TREE_PARTITION_WITH_STATS 0

namespace i3slib::utl::treepartition
{

#if I3S_UTL_TREE_PARTITION_WITH_STATS

struct Avg_pages_per_dive
{
  // assuming that at every level we only fetch the page of the node being traversed.
  double traversing_single_node_per_level{ 0. };

  // assuming that at every level we fetch pages of every siblings.
  double traversing_all_siblings_per_level{ 0. };
};

// Average number of pages fetched when traversing from root to each leaf
struct Stats
{
  Avg_pages_per_dive avg_pages_per_dive;

  double average_count_obb_tests_per_dive{ 0. };
  double average_leaf_depth{ 0. };

  uint64_t count_leafs{ 0 };
};

#endif  // I3S_UTL_TREE_PARTITION_WITH_STATS

/*
Holds the result of the Tree_partitioner algo.
*/
template<typename Node_index_t>
struct Pages
{
  void clear()
  {
    pages.clear();
  }

  // The order of pages is s.t the index is compatible with i3s page indexing, i.e:
  // the first page contains the root, the last page may not be full.
  // Also, the root will be at the front of the page.
  std::vector<std::vector<Node_index_t>> pages;

#if I3S_UTL_TREE_PARTITION_WITH_STATS
  Stats stats;
#endif  // I3S_UTL_TREE_PARTITION_WITH_STATS
};

// If nodes indexes start at 0 and are contiguous, you can use Node_indexes_packed::Yes
// as it will make the algorithm run faster.
enum class Node_indexes_packed { No, Yes };

struct packed_indexes_tag
{
  static constexpr auto val = Node_indexes_packed::Yes;
};
struct unpacked_indexes_tag
{
  static constexpr auto val = Node_indexes_packed::No;
};

namespace detail
{

template<typename Page_index_t>
struct Data
{
  explicit Data()
    : count_self_and_descendants_not_in_page(0)
  {}

  Data(size_t count_self_and_descendants_not_in_page)
    : count_self_and_descendants_not_in_page(count_self_and_descendants_not_in_page)
  {}

  size_t remaining_size() const
  {
    return count_self_and_descendants_not_in_page;
  }
  size_t& mutate_remaining_size() {
    return count_self_and_descendants_not_in_page;
  }

#if I3S_UTL_TREE_PARTITION_WITH_STATS
  void set_page(Page_index_t p) { page = p; }
  Page_index_t get_page() const { return page; }
#endif

private:
  // includes nodes in pending_page
  size_t count_self_and_descendants_not_in_page;

#if I3S_UTL_TREE_PARTITION_WITH_STATS
  Page_index_t page = std::numeric_limits<Page_index_t>::max(); // will be set when the page the created 
#endif
};

template<typename Node_index_t, typename Page_index_t, Node_indexes_packed >
struct Node_data_container;

template<typename Node_index_t, typename Page_index_t>
struct Node_data_container<Node_index_t, Page_index_t, Node_indexes_packed::Yes>
{
  using Data = Data<Page_index_t>;

  size_t size() const { return m_vec.size(); }

  void reserve(size_t n) { m_vec.reserve(n); }

  void clear() { m_vec.clear(); }

  Data * emplace_new(Node_index_t i, Data&& d)
  {
    if (m_vec.size() <= i)
    {
      m_vec.resize(i + 1);
    }
    m_vec[i] = std::move(d);
    return &m_vec[i];
  }

  Data& mutate(Node_index_t i)
  {
    I3S_ASSERT_EXT(i >= 0);
    I3S_ASSERT_EXT(i < m_vec.size());
    return m_vec[i];
  }
  const Data& get(Node_index_t i) const
  {
    I3S_ASSERT_EXT(i >= 0);
    I3S_ASSERT_EXT(i < m_vec.size());
    return m_vec[i];
  }

private:
  std::vector<Data> m_vec;
};

template<typename Node_index_t, typename Page_index_t>
struct Node_data_container<Node_index_t, Page_index_t, Node_indexes_packed::No>
{
  using Data = Data<Page_index_t>;

  size_t size() const { return m_map.size(); }

  void reserve(size_t n) { m_map.reserve(n); }

  void clear() { m_map.clear(); }

  Data* emplace_new(Node_index_t i, Data&& d)
  {
    auto res = m_map.try_emplace(i, std::move(d));
    I3S_ASSERT_EXT(res.second);
    return &res.first->second;
  }

  Data& mutate(Node_index_t i)
  {
    auto it = m_map.find(i);
    if (it != m_map.end())
    {
      return it->second;
    }
    I3S_ASSERT_EXT(false);
    return m_null;
  }
  const Data& get(Node_index_t i) const
  {
    auto it = m_map.find(i);
    if (it != m_map.end())
    {
      return it->second;
    }
    I3S_ASSERT_EXT(false);
    return m_null;
  }

private:
  std::unordered_map<Node_index_t, Data> m_map;
  Data m_null;
};

struct Element_id
{
  Element_id() = default;
  Element_id mut_increment() { ++m_id; return *this; }

  bool operator < (const Element_id& o) const { return m_id < o.m_id; }
private:
  size_t m_id = 0;

};

}  // NS detail


template<typename Node_index_t>
struct Children
{
  Children(const Node_index_t* b, const Node_index_t* e)
    : m_begin(b)
    , m_end(e)
  {}

  auto begin() const { return m_begin; }
  auto end() const { return m_end; }

  bool empty() const { return m_begin == m_end; }
  size_t size() const { return m_end - m_begin; }
private:
  const Node_index_t* m_begin;
  const Node_index_t* m_end;
};

/*
* Splits a tree structure into fixed size pages (except one page which may be smaller).
* 
* The pages are constructed s.t it is likely that the descendants of a node are in the same page as the node,
* and it is likely that nodes of the same page are geographically close.
*/
template<typename Node_index_t, Node_indexes_packed Packed, typename Page_index_t = uint32_t>
struct Tree_partitioner
{
  using Pages = Pages<Node_index_t>;
  using Data = detail::Data<Page_index_t>;
  using Node_data_container = detail::Node_data_container<Node_index_t, Page_index_t, Packed>;

  using Children = Children<Node_index_t>;
  using Children_it = const Node_index_t*;

  Tree_partitioner(size_t count_nodes, Node_index_t root_index, size_t page_size, int max_count_sibling_local_subtrees = 1)
    : m_count_nodes(count_nodes)
    , m_root_idx(root_index)
    , m_max_count_sibling_local_subtrees(max_count_sibling_local_subtrees)
  {
    page_size = std::min(page_size, count_nodes);

    m_page_size = page_size;
  }

  /*
  * Computes pages in |pages|.
  *
  * |get_children| returns the children of a node.
  *
  * |get_priority| is used to determine which sibling should be explored first.
  */
  template<typename F_get_children, typename F_get_priority>
  void operator()(Pages& pages, F_get_children && get_children, F_get_priority && get_priority)
  {
    pages.clear();

    // compute |count_self_and_descendants_not_in_page| for each node.

    initialize_data(get_children);

    const size_t size_before = m_node_data.size();

    // 1. Top-down phase: create full pages in the top part of the tree.

    std::unordered_set<Node_index_t> highest_unprocessed_nodes;
    std::vector<Node_index_t> pending_page;
    pending_page.reserve(m_page_size);

    {
      std::vector<Node_index_t> group;

      struct Page_roots {
        std::vector<Node_index_t> m_siblings_roots;
      };
      std::deque<Page_roots> pages_roots;

      using Priority_t = decltype(get_priority(Node_index_t{}));

      // represents the "priority" of a node, and is used to guide pages construction.
      struct Priority_elt
      {
        Priority_elt() = default;
        Priority_elt(Priority_t p, detail::Element_id i, std::vector<Node_index_t> && vec_nodes)
          : m_priority(p)
          , m_elt_id(i)
          , nodes(std::move(vec_nodes))
        {}

      private:
        Priority_t m_priority;
        detail::Element_id m_elt_id;
      public:
        mutable std::vector<Node_index_t> nodes;  // mutable because priority_queue::top() is const

        bool operator < (const Priority_elt& o) const
        {
          // induces a total order because it includes a unique value for each element.
          return
            std::tie(m_priority, m_elt_id) <
            std::tie(o.m_priority, o.m_elt_id);
        }
      };
      std::priority_queue<Priority_elt> priority;
      
      detail::Element_id elem_id;

      I3S_ASSERT_EXT(m_node_data.get(m_root_idx).remaining_size() >= m_page_size);

      pages_roots.push_back(Page_roots{ {m_root_idx} });

      while (!pages_roots.empty())
      {
        std::vector<Node_index_t> selected_roots{ std::move(pages_roots.front().m_siblings_roots) };
        pages_roots.pop_front();

        I3S_ASSERT_EXT(!selected_roots.empty());

        size_t total_remaining_sz = 0;
        for (Node_index_t root : selected_roots)
        {
          total_remaining_sz += m_node_data.get(root).remaining_size();
        }

        if (total_remaining_sz < m_page_size)
        {
          for (Node_index_t root : selected_roots)
          {
            highest_unprocessed_nodes.emplace(root);
          }
          continue;
        }

        // Create a full page.

        I3S_ASSERT_EXT(pending_page.empty());
        I3S_ASSERT_EXT(priority.empty());

        priority.emplace(
          std::numeric_limits<Priority_t>::max(),  // max priority to force roots to be in the page.
          elem_id.mut_increment(),
          std::move(selected_roots)
          );

        for (int i = 0; i < m_page_size; ++i)
        {
          while (!priority.empty() && priority.top().nodes.empty())
          {
            priority.pop();
          }
          if (priority.empty())
            break;
          
          const Node_index_t node = priority.top().nodes.back();

          priority.top().nodes.pop_back();

          pending_page.push_back(node);

          // enqueue children

          const Children children = get_children(node);
          if (!children.empty())
          {
            priority.emplace(
              get_priority(node),
              elem_id.mut_increment(),
              std::vector<Node_index_t>{children.begin(), children.end()}
              );
          }
        }

        while (!priority.empty())
        {
          std::vector<Node_index_t>& top_nodes = priority.top().nodes;
          if (top_nodes.empty())
          {
            priority.pop();
            continue;
          }

          // split groups according to m_max_count_sibling_local_subtrees

          group.clear();
          for (int i = 0; i < m_max_count_sibling_local_subtrees; ++i)
          {
            group.push_back(top_nodes.back());
            top_nodes.pop_back();
            if (top_nodes.empty())
            {
              break;
            }
          }
          pages_roots.push_back(Page_roots{ group });
        }

        create_page(pending_page, pages);
        pending_page.clear();
      }

      I3S_ASSERT_EXT(0 == highest_unprocessed_nodes.count(m_root_idx));
    }

    // 2. Create remaining pages using post order traversal, ignoring nodes that are already in pages.

    {
      I3S_ASSERT_EXT(pending_page.empty());

      struct Level
      {
        Node_index_t node_idx;

        mutable Children_it next_child;
        Children_it end_child;
      };

      // 'use_nodes' is false iff the next traversed node is already in a page.
      bool use_nodes = false;

      auto reduce_data = [&](const Level& level)
      {
        // if the node is not already in a page, add it to the pending page.

        if (use_nodes)
        {
          pending_page.push_back(level.node_idx);
          if (pending_page.size() == m_page_size)
          {
            create_page(pending_page, pages);
            pending_page.clear();
          }
        }

        // update 'use_nodes' if we crossed the "highest_unprocessed_nodes" border

        if (highest_unprocessed_nodes.count(level.node_idx))
        {
          I3S_ASSERT_EXT(use_nodes);
          use_nodes = false;
        }
      };

      std::vector<Level> stack;
      auto explore_down = [&](const Node_index_t n)
      {
        // update 'use_nodes' if we crossed the "highest_unprocessed_nodes" border

        if (highest_unprocessed_nodes.count(n))
        {
          I3S_ASSERT_EXT(!use_nodes);
          use_nodes = true;
        }
        const Children& cs = get_children(n);
        stack.push_back({ n , cs.begin(), cs.end() });
      };

      explore_down(m_root_idx);
      while (!stack.empty())
      {
        const Level& level = stack.back();

        if (level.next_child == level.end_child)
        {
          reduce_data(level);
          stack.pop_back();
        }
        else
        {
          explore_down(*(level.next_child++));
        }
      }
      if (!pending_page.empty())
      {
        create_page(pending_page, pages);
        pending_page.clear();
      }
    }

    if (!pages.pages.empty())
    {
      I3S_ASSERT_EXT(m_root_idx == pages.pages.front().front());
    }

#if I3S_UTL_TREE_PARTITION_WITH_STATS
    compute_stats(pages.stats, get_children);
#endif

    const size_t size_after = m_node_data.size();
    I3S_ASSERT_EXT(size_before == size_after);
  }

private:
  size_t m_page_size;
  const size_t m_count_nodes;
  const Node_index_t m_root_idx;

  Page_index_t m_next_page_idx = 0;
  Node_data_container m_node_data;
  int m_max_count_sibling_local_subtrees;

  template<typename F_get_children>
  void initialize_data(F_get_children & get_children)
  {
    m_node_data.clear();
    m_node_data.reserve(m_count_nodes);

    struct Level
    {
      Data * data_ptr = nullptr;

      Children_it begin_child;
      mutable Children_it next_child;
      Children_it end_child;
    };

    auto reduce_data = [&](const Level& level)
    {
      size_t& count = level.data_ptr->mutate_remaining_size();
      for (auto it = level.begin_child; it != level.end_child; ++it)
        count += m_node_data.get(*it).remaining_size();
    };

    std::vector<Level> stack;
    auto explore_down = [&](const Node_index_t n)
    {
      Data* data_ptr = m_node_data.emplace_new(n, Data{ 1 });
      const Children& cs = get_children(n);
      stack.push_back({data_ptr , cs.begin(), cs.begin(), cs.end() });
    };

    explore_down(m_root_idx);
    while (!stack.empty())
    {
      const Level& level = stack.back();

      if (level.next_child == level.end_child)
      {
        reduce_data(level);
        stack.pop_back();
      }
      else
      {
        explore_down(*(level.next_child++));
      }
    }
  }

  void create_page(const std::vector<Node_index_t> & nodes, Pages& pages)
  {
    I3S_ASSERT_EXT(nodes.size() <= m_page_size);
    pages.pages.push_back(nodes);
#if I3S_UTL_TREE_PARTITION_WITH_STATS
    for (const Node_index_t& n : pages.pages.back())
    {
      m_node_data.mutate(n).set_page(m_next_page_idx);
    }
#endif

    ++m_next_page_idx;
  }

#if I3S_UTL_TREE_PARTITION_WITH_STATS
  template<typename Get_children_f>
  void compute_stats(Stats& global_stats, Get_children_f && get_children) const
  {
    struct Stats
    {
      Stats()
      {
        clear();
      }

      void clear()
      {
        count_obb_tests = 0;
        count_pages_used_sofar_siblings = 0;
        count_pages_used_sofar_self = 0;
        pages_siblings.clear();
        page_self = std::numeric_limits<Page_index_t>::max();
      }

      std::size_t count_pages_used_sofar_siblings;
      Tiny_unordered_set<Page_index_t> pages_siblings;

      std::size_t count_pages_used_sofar_self;
      Page_index_t page_self;

      std::size_t count_obb_tests;
    };

    // will only grow, and max size = max tree depth
    std::vector<Stats> all_stats;

    struct Level
    {
      bool is_leaf() const { return begin_child == end_child; }

      Children_it begin_child;
      mutable Children_it next_child;
      Children_it end_child;
    };

    std::vector<Level> stack;
    auto explore_down = [&](const Node_index_t n)
    {
      const Page_index_t self_page = m_node_data.get(n).get_page();
      I3S_ASSERT_EXT(self_page != std::numeric_limits<Page_index_t>::max());

      if (all_stats.size() < (stack.size() + 1))
        all_stats.resize(stack.size() + 1);
      Stats& stats = all_stats[stack.size()];

      const Children cs = get_children(n);

      stats.clear();
      stats.page_self = self_page;
      if (stack.empty())
      {
        stats.count_pages_used_sofar_self = 1;
        stats.count_obb_tests = 1 + cs.size();
      }
      else
      {
        const Stats& prev_stats = all_stats[stack.size() - 1];
        stats.count_pages_used_sofar_self = prev_stats.count_pages_used_sofar_self;
        if (prev_stats.page_self != self_page)
        {
          ++stats.count_pages_used_sofar_self;
        }
        stats.count_obb_tests = prev_stats.count_obb_tests + cs.size();
      }

      if (stack.empty())
      {
        stats.pages_siblings.emplace(self_page);
        stats.count_pages_used_sofar_siblings = 1;
      }
      else
      {
        const Level& prev_level = stack.back();
        for (auto it = prev_level.begin_child; it != prev_level.end_child; ++it)
          stats.pages_siblings.emplace(m_node_data.get(*it).get_page());
        const Stats& prev_stats = all_stats[stack.size() - 1];
        stats.count_pages_used_sofar_siblings = prev_stats.count_pages_used_sofar_siblings;
        for (const Page_index_t p : stats.pages_siblings)
          if (0 == prev_stats.pages_siblings.count(p))
            ++stats.count_pages_used_sofar_siblings;
      }

      stack.push_back({ cs.begin(), cs.begin(), cs.end() });
    };

    uint64_t count_leafs = 0;
    uint64_t count_traversing_all_siblings_per_level = 0;
    uint64_t count_traversing_single_node_per_level = 0;
    uint64_t sum_leaf_depths = 0;
    uint64_t sum_obb_tests = 0;

    auto reduce_data = [&](const Level& level)
    {
      if (level.is_leaf())
      {
        ++count_leafs;
        sum_leaf_depths += stack.size() - 1;
        const Stats& stats = all_stats[stack.size() - 1];
        count_traversing_all_siblings_per_level += stats.count_pages_used_sofar_siblings;
        count_traversing_single_node_per_level += stats.count_pages_used_sofar_self;
        sum_obb_tests += stats.count_obb_tests;
      }
    };

    explore_down(m_root_idx);
    while (!stack.empty())
    {
      const Level& level = stack.back();

      if (level.next_child == level.end_child)
      {
        reduce_data(level);
        stack.pop_back();
      }
      else
      {
        explore_down(*(level.next_child++));
      }
    }

    global_stats.average_count_obb_tests_per_dive =
      static_cast<double>(sum_obb_tests) / count_leafs;
    global_stats.avg_pages_per_dive.traversing_all_siblings_per_level =
      static_cast<double>(count_traversing_all_siblings_per_level) / count_leafs;
    global_stats.avg_pages_per_dive.traversing_single_node_per_level =
      static_cast<double>(count_traversing_single_node_per_level) / count_leafs;
    global_stats.average_leaf_depth =
      static_cast<double>(sum_leaf_depths) / count_leafs;
    global_stats.count_leafs = count_leafs;
  }
#endif
};

}
