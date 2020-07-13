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
#include <stdint.h>
#include <vector>
#include <memory>
#include <filesystem>

#pragma warning(push)
#pragma warning(disable : 4251)

namespace i3slib
{

namespace utl
{
  
  struct Point3d
  {
    double x, y, z;
  };
  struct Point3f
  {
    float x, y, z;
  };

  struct Pro_set;

  struct I3S_EXPORT Bvh_node
  {
    int64_t     feature_id;   // id of the original geometry feature in node

    Point3d   mbs_center; // xyz + radius of bounding sphere
    double    mbs_radius;

    int      parent;      // index of parent node
    std::vector<int>  child;  // array of indexes of childs        
  };

  class Hierarchy;

  class I3S_EXPORT Bvh_builder 
  {
  public:
    Bvh_builder();
    ~Bvh_builder();

    void add_feature(int64_t id, const Point3d& origin, const Point3f* vertices, int vertices_count);
    void build_tree(std::vector<Bvh_node>& tree, double cluster_scale);

// ---- debug functions 
    void debug_read(const std::filesystem::path& path);   
    void debug_write(const std::filesystem::path& path);

  private:
    std::vector<Bvh_node> Bvh_tree;        // graph representation using vector of bvh_nodes 
    std::unique_ptr< Hierarchy > m_impl;    // internal implementation 

    double cluster_scale=0.0;                       // ratio of desired cluster sizes between levels 
    void filter_kids(int parent_id, int child_id, int target_id);  // remove unnecessary kids nodes based on cluster_scale ratio
  };

}

} // namespace i3slib

#pragma warning(pop)
