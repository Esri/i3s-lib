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
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>

#include <fstream>
#include <stdint.h>
#include "utils/utl_geom.h"
#include "utils/utl_prohull.h"
#include "utils/utl_bvh.h"
#include "utils/utl_quaternion.h"
#include "utils/utl_i3s_assert.h"

// -----------------------------------

namespace i3slib
{

namespace utl
{

  //----------------------------------------------------------------------------------------------------------
  //   Clusters
  //----------------------------------------------------------------------------------------------------------

  struct Cluster
  {
    explicit Cluster(const Pro_set* proset) : hull(proset) {}
    //friend class Pro_hull;
    Pro_hull hull;      // projection hull of the geometry in this cluster
    int64_t id=0;        // feature id
  
    int match=0;        // id of best match cluster 
    double size=0.0;      // match distance
  };

  //----------------------------------------------------------------------------------------------------------
  //   Hierarchy Tree
  //----------------------------------------------------------------------------------------------------------

  struct Hierarchy_node      // hierarchy tree node
  {
    int cluster;      // id of original cluster
    int child;              // id of first child
    int next;               // id of next sibling 
  };

  //------------------------------------------------------------------------------------------------------------------------------

  class  Hierarchy {

    const int c_downsize = 64;     // number of leaf nodes in cluster when processing down
    const int c_upsize = 32;    // target number of clusters in node when processing up

  public:
    explicit Hierarchy() { Pro_set m_base = Pro_set(); }     // init hierarchy using base set of directions

    void join_up(int& list_id, int target_size);   // joins clasters reducing total number to target_size
    int split_down(int begin, int end);            // splitting a set of original clusters into 2 smaller sets
    void list2vector(int list_id, std::vector<int>& child);    // convert hierarchy list to int array

  public: //to become private
    std::vector<Cluster>      clusters;        // the set of geometry clusters of original objects
    std::vector<Hierarchy_node>    hierarchy;        // hierarchy tree on array 

    const Pro_set m_base;
  private:

    void list_list_match(int list1, int list2);        // match list1 toward list2 
    void list_node_match(int list_id, int node1);      // find the best match and distance for a node <node1> in a list <list_id>. Store info in the cluster.
    int  list_best_match(int list_id);                 // get the best match for all nodes in a list
    void list_remove(int list_id, int node);           // remove node from list
    void list_join_match(int& list_id, int node1, int node2);  // join matching nodes into list and remove them from list, replace them by a new joint node
  
    int find_partition(int begin, int end);          // find best list partition and sort set along it
    void common_hull(Pro_hull& hull, int begin, int end);    // common hull of all clusters in a list
  };

  //--------------------------------------------------------------------------------------------------------------------
  // Projection Set methods
  //--------------------------------------------------------------------------------------------------------------------

  Pro_set::Pro_set() {

    base_vector_size = c_base_vector_size;           
    base_obb_size = c_base_obb_size;              

    dir.resize(base_vector_size);

    // main coordinate axes 
    dir[0] = Vec3d(1., 0., 0.);
    dir[1] = Vec3d(0., 1., 0.);
    dir[2] = Vec3d(0., 0., 1.);

    // 2-axis diagonals
    dir[3] = Vec3d(0., 1., 1.);    // (dir[2] + dir[1]);
    dir[4] = Vec3d(0., -1., 1.);   // (dir[2] - dir[1]);
    dir[5] = Vec3d(1., 0., 1.);   // (dir[2] + dir[0]);
    dir[6] = Vec3d(-1., 0., 1.);   // (dir[2] - dir[0]);
    dir[7] = Vec3d(1., 1., 0.);   // (dir[1] + dir[0]);
    dir[8] = Vec3d(-1., 1., 0.);   // (dir[1] - dir[0]);

    // 3-axis diagonals 
    dir[9] = Vec3d(1., 1., 1.);   //(dir[0] + dir[1] + dir[2]);
    dir[10] = Vec3d(-1., -1., 1.);  //(-dir[0] - dir[1] + dir[2]);
    dir[11] = Vec3d(1., -1., 1.);  //(dir[0] - dir[1] + dir[2]);
    dir[12] = Vec3d(-1., 1., 1.);   //(-dir[0] + dir[1] + dir[2]);

    // mix of main axis and 3-axis diagonals
    dir[13] = Vec3d(2., -1., 1.);   //(dir[0] + dir[11]);
    dir[14] = Vec3d(2., 1., -1.);   //(dir[0] - dir[10]);
    dir[15] = Vec3d(2., 1., 1.);    //(dir[0] + dir[9]);
    dir[16] = Vec3d(2., -1., -1.);  //(dir[0] - dir[12]);

    dir[17] = Vec3d(-1., 2., 1.);   //(dir[1] + dir[12]);
    dir[18] = Vec3d(1., 2., -1.);   //(dir[1] - dir[10]);
    dir[19] = Vec3d(-1., 2., -1.);  //(dir[1] - dir[11]);
    dir[20] = Vec3d(1., 2., 1.);    //(dir[1] + dir[9]);

    dir[21] = Vec3d(-1., -1., 2.);  //(dir[2] + dir[10]);
    dir[22] = Vec3d(1., 1., 2.);    //(dir[2] + dir[9]);
    dir[23] = Vec3d(1., -1., 2.);   //(dir[2] + dir[11]);
    dir[24] = Vec3d(-1., 1., 2.);   //(dir[2] + dir[12]);

    //mix of main axis and 2-axis diagonals
    dir[25] = Vec3d(2., -1., 0.);   //(dir[0] - dir[8]);
    dir[26] = Vec3d(1., 2., 0.);   //(dir[1] + dir[7]);
    dir[27] = Vec3d(2., 1., 0.);    //(dir[0] + dir[7]);
    dir[28] = Vec3d(-1., 2., 0.);   //(dir[1] + dir[8]);

    dir[29] = Vec3d(-1., 0., 2.);   //(dir[2] + dir[6]);
    dir[30] = Vec3d(2., 0., 1.);   //(dir[0] + dir[5]);
    dir[31] = Vec3d(1., 0., 2.);    //(dir[2] + dir[5]);
    dir[32] = Vec3d(2., 0., -1.);   //(dir[0] - dir[6]);

    dir[33] = Vec3d(0., -1., 2.);   //(dir[2] - dir[4]);
    dir[34] = Vec3d(0., 2., 1.);   //(dir[1] + dir[3]);
    dir[35] = Vec3d(0., 1., 2.);    //(dir[2] + dir[3]);
    dir[36] = Vec3d(0., 2., -1.);   //(dir[1] - dir[4]);


    //normalize all base vectors
    for (int i = 0; i < base_vector_size; i++)  dir[i] = dir[i].normalized();

    // set corresponding OBBs.  Intersection of these OBBs gives a projection hull

    obb.resize(base_obb_size);

    obb[0] = Vec3i(0, 1, 2);
    obb[1] = Vec3i(0, 3, 4);
    obb[2] = Vec3i(1, 5, 6);
    obb[3] = Vec3i(2, 7, 8);
    obb[4] = Vec3i(3, 10, 13);
    obb[5] = Vec3i(3, 11, 14);
    obb[6] = Vec3i(4, 9, 16);
    obb[7] = Vec3i(4, 12, 15);
    obb[8] = Vec3i(5, 10, 17);
    obb[9] = Vec3i(5, 12, 18);
    obb[10] = Vec3i(6, 9, 19);
    obb[11] = Vec3i(6, 11, 20);
    obb[12] = Vec3i(7, 11, 24);
    obb[13] = Vec3i(7, 12, 23);
    obb[14] = Vec3i(8, 9, 21);
    obb[15] = Vec3i(8, 10, 22);
    obb[16] = Vec3i(2, 25, 26);
    obb[17] = Vec3i(2, 27, 28);
    obb[18] = Vec3i(1, 29, 30);
    obb[19] = Vec3i(1, 31, 32);
    obb[20] = Vec3i(0, 33, 34);
    obb[21] = Vec3i(0, 35, 36);
  }

  Pro_set::Pro_set( Polyhedron base) {

    double Fi1 = 0.5*(1. + sqrt(5.));  // golden ratio
    double Fi2 = 1. / Fi1;             // reverse golden ratio

    switch (base)
    {
    case Polyhedron::Cube:
      base_vector_size = 4;
      base_obb_size = 0;

      dir.resize(base_vector_size);
      // 3-axis diagonals 
      dir[0] = Vec3d(1., 1., 1.);   //(dir[0] + dir[1] + dir[2]);
      dir[1] = Vec3d(-1., -1., 1.);  //(-dir[0] - dir[1] + dir[2]);
      dir[2] = Vec3d(1., -1., 1.);  //(dir[0] - dir[1] + dir[2]);
      dir[3] = Vec3d(-1., 1., 1.);   //(-dir[0] + dir[1] + dir[2]);

      // fixed OBBs
      obb.resize(base_obb_size);
      break;

    case Polyhedron::Octahedron:
      base_vector_size = 3;
      base_obb_size = 1;

      dir.resize(base_vector_size);
      // main coordinate axes 
      dir[0] = Vec3d(1., 0., 0.);
      dir[1] = Vec3d(0., 1., 0.);
      dir[2] = Vec3d(0., 0., 1.);

      // fixed OBBs
      obb.resize(base_obb_size);
      obb[0] = Vec3i(0, 1, 2);
      break;


    case Polyhedron::Dodecahedron:
      base_vector_size = 10;
      base_obb_size = 0;

      dir.resize(base_vector_size);
/*
(±1, ±1, ±1)
(0, ±1/f, ±f)
(±1/f, ±f, 0)
(±f, 0, ±1/f)
where f = (1 + √5) / 2 is the golden ratio
*/
      // 3-axis diagonals 
      dir[0] = Vec3d(1., 1., 1.);   
      dir[1] = Vec3d(-1., -1., 1.);  
      dir[2] = Vec3d(1., -1., 1.);  
      dir[3] = Vec3d(-1., 1., 1.);   

      // 2-axis directions 
      dir[4] = Vec3d(  0.,  Fi2,  Fi1);    
      dir[5] = Vec3d(  0., -Fi2,  Fi1);   
      dir[6] = Vec3d( Fi2,  Fi1,   0.);   
      dir[7] = Vec3d(-Fi2,  Fi1,   0.);   
      dir[8] = Vec3d( Fi1,   0.,  Fi2);  
      dir[9] = Vec3d( Fi1,   0., -Fi2);  

      // fixed OBBs
      obb.resize(base_obb_size);
      break;

    case Polyhedron::Icosohedron:    // alligned with dodecahedron to merge bases
      base_vector_size = 6;
      base_obb_size = 0;

      dir.resize(base_vector_size);

      // 2-axis directions 
      dir[0] = Vec3d(0., 1.,  Fi1);
      dir[1] = Vec3d(0., 1., -Fi1);
      dir[2] = Vec3d(1., Fi1, 0.);
      dir[3] = Vec3d(1, -Fi1, 0.);
      dir[4] = Vec3d( Fi1, 0., 1.);
      dir[5] = Vec3d(-Fi1, 0., 1.);

      // fixed OBBs
      obb.resize(base_obb_size);
      break;

     case Polyhedron::Rhombic_triacontahedron :  // mix of icosohedron and dodecahedron

      base_vector_size = 16;
      base_obb_size = 0;

      dir.resize(base_vector_size);

      // 3-axis diagonals 
      dir[0] = Vec3d(1., 1., 1.);   
      dir[1] = Vec3d(-1., -1., 1.);  
      dir[2] = Vec3d(1., -1., 1.);  
      dir[3] = Vec3d(-1., 1., 1.);   

      // 2-axis directions 
      dir[4] = Vec3d(  0.,  Fi2,  Fi1);    
      dir[5] = Vec3d(  0., -Fi2,  Fi1);   
      dir[6] = Vec3d( Fi2,  Fi1,   0.);   
      dir[7] = Vec3d(-Fi2,  Fi1,   0.);   
      dir[8] = Vec3d( Fi1,   0.,  Fi2);  
      dir[9] = Vec3d( Fi1,   0., -Fi2);  

      dir[10] = Vec3d(0., 1., Fi1);
      dir[11] = Vec3d(0., 1., -Fi1);
      dir[12] = Vec3d(1., Fi1, 0.);
      dir[13] = Vec3d(1, -Fi1, 0.);
      dir[14] = Vec3d(Fi1, 0., 1.);
      dir[15] = Vec3d(-Fi1, 0., 1.);

      // fixed OBBs
      obb.resize(base_obb_size);
      break;

    case Polyhedron::Disdyakis_dodecahedron:
      base_vector_size = 13;
      base_obb_size = 4;

      dir.resize(base_vector_size);

      // main coordinate axes 
      dir[0] = Vec3d(1., 0., 0.);
      dir[1] = Vec3d(0., 1., 0.);
      dir[2] = Vec3d(0., 0., 1.);

      // 2-axis diagonals
      dir[3] = Vec3d(0., 1., 1.);    // (dir[2] + dir[1]);
      dir[4] = Vec3d(0., -1., 1.);   // (dir[2] - dir[1]);
      dir[5] = Vec3d(1., 0., 1.);   // (dir[2] + dir[0]);
      dir[6] = Vec3d(-1., 0., 1.);   // (dir[2] - dir[0]);
      dir[7] = Vec3d(1., 1., 0.);   // (dir[1] + dir[0]);
      dir[8] = Vec3d(-1., 1., 0.);   // (dir[1] - dir[0]);

      // 3-axis diagonals 
      dir[9] = Vec3d(1., 1., 1.);   //(dir[0] + dir[1] + dir[2]);
      dir[10] = Vec3d(-1., -1., 1.);  //(-dir[0] - dir[1] + dir[2]);
      dir[11] = Vec3d(1., -1., 1.);  //(dir[0] - dir[1] + dir[2]);
      dir[12] = Vec3d(-1., 1., 1.);   //(-dir[0] + dir[1] + dir[2]);

      // fixed OBBs
      obb.resize(base_obb_size);
      obb[0] = Vec3i(0, 1, 2);
      obb[1] = Vec3i(0, 3, 4);
      obb[2] = Vec3i(1, 5, 6);
      obb[3] = Vec3i(2, 7, 8);
      break;

    case Polyhedron::Rhombohedron:  // Rhombic_triacontahedron + Disdyakis_dodecahedron

      base_vector_size = 37;
      base_obb_size = 0;

      dir.resize(base_vector_size);

      // 3-axis diagonals 
      dir[0] = Vec3d(1., 1., 1.);
      dir[1] = Vec3d(-1., -1., 1.);
      dir[2] = Vec3d(1., -1., 1.);
      dir[3] = Vec3d(-1., 1., 1.);

      // 2-axis directions 
      dir[4] = Vec3d(0., Fi2, Fi1);
      dir[5] = Vec3d(0., -Fi2, Fi1);
      dir[6] = Vec3d(Fi2, Fi1, 0.);
      dir[7] = Vec3d(-Fi2, Fi1, 0.);
      dir[8] = Vec3d(Fi1, 0., Fi2);
      dir[9] = Vec3d(Fi1, 0., -Fi2);

      dir[10] = Vec3d(0., 1., Fi1);
      dir[11] = Vec3d(0., 1., -Fi1);
      dir[12] = Vec3d(1., Fi1, 0.);
      dir[13] = Vec3d(1, -Fi1, 0.);
      dir[14] = Vec3d(Fi1, 0., 1.);
      dir[15] = Vec3d(-Fi1, 0., 1.);

      // mix of main axis and 3-axis diagonals
      dir[16] = Vec3d(2., -1., 1.);   
      dir[17] = Vec3d(2., 1., -1.);   
      dir[18] = Vec3d(2., 1., 1.);    
      dir[19] = Vec3d(2., -1., -1.);  

      dir[20] = Vec3d(-1., 2., 1.);   
      dir[21] = Vec3d(1., 2., -1.);   
      dir[22] = Vec3d(-1., 2., -1.);  
      dir[23] = Vec3d(1., 2., 1.);    

      dir[24] = Vec3d(-1., -1., 2.);  
      dir[25] = Vec3d(1., 1., 2.);    
      dir[26] = Vec3d(1., -1., 2.);   
      dir[27] = Vec3d(-1., 1., 2.);   

      // main coordinate axes 
      dir[28] = Vec3d(1., 0., 0.);
      dir[29] = Vec3d(0., 1., 0.);
      dir[30] = Vec3d(0., 0., 1.);

      // 2-axis diagonals
      dir[31] = Vec3d(0., 1., 1.);    
      dir[32] = Vec3d(0., -1., 1.);   
      dir[33] = Vec3d(1., 0., 1.);   
      dir[34] = Vec3d(-1., 0., 1.);   
      dir[35] = Vec3d(1., 1., 0.);   
      dir[36] = Vec3d(-1., 1., 0.);   

      // fixed OBBs
      obb.resize(base_obb_size);
      break;

    default:
      I3S_ASSERT(false); //something new
    }

    //normalize all base direction vectors 
    for (int i = 0; i < base_vector_size; i++)  dir[i] = dir[i].normalized();
  }

  const Pro_set* Pro_set::get_default()
  {
    static const Pro_set base(Polyhedron::Rhombic_triacontahedron);
    return &base;
  }

  //--------------------------------------------------------------------------------------------------------------------
  // Projection Hull methods
  //--------------------------------------------------------------------------------------------------------------------

  Pro_hull::Pro_hull(const Pro_set* proset)
    : m_base(proset)
    , m_promin(proset->base_vector_size, std::numeric_limits<double>::max())
    , m_promax(proset->base_vector_size, std::numeric_limits<double>::lowest())
    , m_provertex(2 * proset->base_vector_size)
  {
  }

  Pro_hull::Pro_hull(const Pro_set* proset, const Vec3d& vector)
    : Pro_hull(proset)
  {
    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto proj = vector.dot(m_base->dir[i]);    //projection to i-th base direction
      m_promin[i] = proj;
      m_promax[i] = proj;
      m_provertex[2 * i] = vector;
      m_provertex[2 * i + 1] = vector;
    }
  }

  void Pro_hull::clear()
  {
    std::fill(m_promin.begin(), m_promin.end(), std::numeric_limits< double >::max());
    std::fill(m_promax.begin(), m_promax.end(), std::numeric_limits< double >::lowest());
    std::fill(m_provertex.begin(), m_provertex.end(), Vec3d{});
  }

  double Pro_hull::center(int dir) const {                  // middle value of projection for a direction
    return 0.5 * (m_promax[dir] + m_promin[dir]);
  }

  double Pro_hull::extent(int dir) const {                  // projection extent for a direction
    return (m_promax[dir] - m_promin[dir]);
  }

  void Pro_hull::add(const Pro_hull& h) {   // add a hull

    for (int i = 0; i < m_base->base_vector_size; i++) {
      if (h.m_promax[i] > m_promax[i]) {
        m_promax[i] = h.m_promax[i];
        m_provertex[2 * i] = h.m_provertex[2 * i];
      }
      if (h.m_promin[i] < m_promin[i]) {
        m_promin[i] = h.m_promin[i];
        m_provertex[2 * i + 1] = h.m_provertex[2 * i + 1];
      }
    }
  }

  void Pro_hull::add(const Vec3d& vector) {   // add a point

    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto proj = vector.dot(m_base->dir[i]);    //projection to i-th base direction
      if (proj > m_promax[i]) {
        m_promax[i] = proj;
        m_provertex[2 * i] = vector;
      }
      if (proj < m_promin[i]) {
        m_promin[i] = proj;
        m_provertex[2 * i + 1] = vector;
      }
    }
  }

  //--------------------------------------------------------------------------------------------------------------------

  // different methods to calc a principal dimension of a hull

  int Pro_hull::principal_dimension_max() const {   // calculate principal dimension using maximal projection direction
    double max = extent(0);    // maximal projection length
    int max_p = 0;

    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto len = extent(i);
      if (len > max) { max = len; max_p = i; }
    }
    return max_p;
  }

  // calculate principal dimension using some variation of PCA ( Principal Componet Analysis )
  // find a dimension having least square diameter of hull projection along this axis
  int Pro_hull::principal_dimension_pca() const {

    double mindiam = std::numeric_limits<double>::max();    // minimal projection diameter
    int principal = 0;

    for (int i = 0; i < m_base->base_vector_size; i++) {    // 
      double sum = 0.;

      // calculate projection diameter
      for (int j = 0; j < m_base->base_vector_size; j++) {    // 
        if (i != j) {
          auto len = extent(j);          // projection length
          double proj = m_base->dir[i].dot(m_base->dir[j]);    // axis projection
          double diam = 1.0 - proj*proj;           // projection axis deviation
          diam = len*len*diam;
          sum += diam;
        }
      }
      if (sum < mindiam) { mindiam = sum; principal = i; }
    }
    return principal;
  }

  // calculate principal dimension using OBB set
  // find a dimension having least projection of OBB along this axis
  int Pro_hull::principal_dimension_obb() const {

    double min_face = std::numeric_limits<double>::max();    // minimal projection perimeter
    int principal = 0;

    for (int i = 0; i < m_base->base_obb_size; i++) {    // check all OBB

      int dir_x = m_base->obb[i].x;
      int dir_y = m_base->obb[i].y;
      int dir_z = m_base->obb[i].z;

      double size_x = extent(dir_x);
      double size_y = extent(dir_y);
      double size_z = extent(dir_z);

      double face;
      face = size_x*size_y;
      if (face < min_face) { min_face = face; principal = dir_z; }
      face = size_x*size_z;
      if (face < min_face) { min_face = face; principal = dir_y; }
      face = size_z*size_y;
      if (face < min_face) { min_face = face; principal = dir_x; }
    }
    return principal;
  }

  //--------------------------------------------------------------------------------------------------------------------
  // different cluster distance metrics 

  double Pro_hull::common_mean_proj(const Pro_hull& h) const {   // square mean of projections by dimensions for common hull with <h>

    double distance = 0.;
    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto len = std::max(m_promax[i], h.m_promax[i]) - std::min(m_promin[i], h.m_promin[i]);
      distance += len*len;
    }
    return distance;     // should be: sqrt( dimension * sum( proj*proj) / num ), but it doesn't impact comparisons 
  }

  double Pro_hull::common_mean_proj_cmp(const Pro_hull& h, double current_distance) const {  // distance calc with inline comparison

    double distance = 0.;
    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto len = std::max(m_promax[i], h.m_promax[i]) - std::min(m_promin[i], h.m_promin[i]);
      distance += len*len;
      if ( distance > current_distance ) return distance;    // don't process dimensions further if distance is already greater than current one
    }
    return distance;     // should be: sqrt( dimension * sum( proj*proj) / num ), but it doesn't impact comparisons 
  }

  double Pro_hull::common_max_proj(const Pro_hull& h) const {   // max of projections by dimensions for common hull with <h>

    double distance = 0.;
    for (int i = 0; i < m_base->base_vector_size; i++) {
      auto len = std::max(m_promax[i], h.m_promax[i]) - std::min(m_promin[i], h.m_promin[i]);
      distance = std::max(len, distance);
    }
    return distance;
  }

  double Pro_hull::common_min_area_obb(const Pro_hull& h) const {   // a surface area of smallest OBB of the common hull with <h>

    int dir_x, dir_y, dir_z;
    double size_x, size_y, size_z, area;
    double min_area = std::numeric_limits<double>::max();

    for (int i = 0; i < m_base->base_obb_size; i++) {    // check all OBB of a hull

      dir_x = m_base->obb[i].x;
      dir_y = m_base->obb[i].y;
      dir_z = m_base->obb[i].z;

      // common oob extents 
      size_x = std::max(m_promax[dir_x], h.m_promax[dir_x]) - std::min(m_promin[dir_x], h.m_promin[dir_x]);
      size_y = std::max(m_promax[dir_y], h.m_promax[dir_y]) - std::min(m_promin[dir_y], h.m_promin[dir_y]);
      size_z = std::max(m_promax[dir_z], h.m_promax[dir_z]) - std::min(m_promin[dir_z], h.m_promin[dir_z]);

      area = size_x*size_y + size_y*size_z + size_z*size_x;

      min_area = std::min(min_area, area);
    }
    return min_area;
  }

  double Pro_hull::common_mean_diag_obb(const Pro_hull& h) const {   // average diagonal of OBB of common hull with <h>

    int dir_x, dir_y, dir_z;
    double size_x, size_y, size_z, diag = 0.0;

    for (int i = 0; i < m_base->base_obb_size; i++) {    // check all OBB of a hull

      dir_x = m_base->obb[i].x;
      dir_y = m_base->obb[i].y;
      dir_z = m_base->obb[i].z;

      // common oob extents 
      size_x = std::max(m_promax[dir_x], h.m_promax[dir_x]) - std::min(m_promin[dir_x], h.m_promin[dir_x]);
      size_y = std::max(m_promax[dir_y], h.m_promax[dir_y]) - std::min(m_promin[dir_y], h.m_promin[dir_y]);
      size_z = std::max(m_promax[dir_z], h.m_promax[dir_z]) - std::min(m_promin[dir_z], h.m_promin[dir_z]);

      diag += size_x*size_x + size_y*size_y + size_z*size_z;
    }
    return sqrt(diag / m_base->base_obb_size);
  }

  //--------------------------------------------------------------------------------------------------------------------
  // calculate a bounding sphere... use the best OBB as a base 

  void Pro_hull::get_bounding_sphere(Vec3d& origin, double& radius ) const {   // calculate a bounding sphere for a hull

    int best = 0;    // best box

    int dir_x, dir_y, dir_z;   // axis directions of OBB
    double size_x, size_y, size_z, area;
    double min_area = std::numeric_limits<double>::max();    // minimal projection area

    for (int i = 0; i < m_base->base_obb_size; i++) {    // check all OBB of a hull to find the best fit 
      
      size_x = extent(m_base->obb[i].x);
      size_y = extent(m_base->obb[i].y);
      size_z = extent(m_base->obb[i].z);

      area = size_x*size_y + size_y*size_z + size_z*size_x;

      if (area < min_area) {
        min_area = area;
        best = i;
      }
    }

    dir_x = m_base->obb[best].x;
    dir_y = m_base->obb[best].y;
    dir_z = m_base->obb[best].z;

    origin = center(dir_x) * m_base->dir[dir_x] + center(dir_y) * m_base->dir[dir_y] + center(dir_z) * m_base->dir[dir_z];

    size_x = extent(dir_x);
    size_y = extent(dir_y);
    size_z = extent(dir_z);

    radius = 0.25 * sqrt(size_x*size_x + size_y*size_y + size_z*size_z);
  }

  //--------------------------------------------------------------------------------------------------------------------
  // calculate a best OBB for a hull
  // methods:  0 - minimal diameter, 1 - minimal surface area

  void Pro_hull::get_bounding_box(Obb_abs& obb, Method method ) const {

    int best = 0;    // best box
    auto min = std::numeric_limits<double>::max();   

    for (int i = 0; i < m_base->base_obb_size; i++)
    {    // check all OBB of a hull to find the best fit 
      const auto size_x = extent(m_base->obb[i].x);
      const auto size_y = extent(m_base->obb[i].y);
      const auto size_z = extent(m_base->obb[i].z);
      double value;

      switch (method)
      {
        case Method::Minimal_diameter:
          value = size_x*size_x + size_y*size_y + size_z*size_z;
          break;
        case Method::Minimal_surface_area:
          value = size_x*size_y + size_y*size_z + size_z*size_x;
          break;
        case Method::Minimal_volume:
          value = size_x*size_y*size_z;
          break;
        default:
          I3S_ASSERT(false); //something new
          value = 0;
          break;
      }

      if (value < min) {
        min = value;
        best = i;
      }
    }

    // axis directions of OBB
    const int dir_x = m_base->obb[best].x;
    const int dir_y = m_base->obb[best].y;
    const int dir_z = m_base->obb[best].z;

    obb.center = center(dir_x) * m_base->dir[dir_x] + center(dir_y) * m_base->dir[dir_y] + center(dir_z) * m_base->dir[dir_z];
    obb.extents = 0.5f * Vec3f((float)extent(dir_x), (float)extent(dir_y), (float)extent(dir_z));

    //create the rotation matrix:
    Mat4d R;
    R._11 = m_base->dir[dir_x].x;
    R._12 = m_base->dir[dir_x].y;
    R._13 = m_base->dir[dir_x].z;
    R._14 = 0.0;

    R._21 = m_base->dir[dir_y].x;
    R._22 = m_base->dir[dir_y].y;
    R._23 = m_base->dir[dir_y].z;
    R._24 = 0.0;

    // get a sign of orientation of obb axises 
    double sign = Vec3d::dot(Vec3d::cross(m_base->dir[dir_x], m_base->dir[dir_y]), m_base->dir[dir_z]);

    R._31 = (sign > 0.) ? m_base->dir[dir_z].x : -m_base->dir[dir_z].x;
    R._32 = (sign > 0.) ? m_base->dir[dir_z].y : -m_base->dir[dir_z].y;
    R._33 = (sign > 0.) ? m_base->dir[dir_z].z : -m_base->dir[dir_z].z;
    R._34 = 0.0;

    R._41 = 0.0;
    R._42 = 0.0;
    R._43 = 0.0;
    R._44 = 1.0;

    obb.orientation = rotation_matrix_to_quaternion(R);
  }

  //--------------------------------------------------------------------------------------------------------------------
 
  Vec3d Pro_hull::base_dir(int vindex) {   // get base direction by vertex index
    return ((vindex & 1) == 0 ? m_base->dir[vindex / 2] : -m_base->dir[vindex / 2]);
  }

  // roll a plane around vertex with index vindex in x direction with normal y... return index of vertex with a minimal slope
  int Pro_hull::convex_roll(int vindex, Vec3d& x_axis, Vec3d& y_axis) {  // x - roll direction, y - normal  

    double max_proj = std::numeric_limits<double>::lowest();
    double c_eps = 0.000000001;   // tolerance to avoid point clusters 
    int max_vertex = vindex;
    Vec3d base = m_provertex[vindex];      // a convexoid vertex
    Vec3d local;

    for (auto i = 0; i < 2 * m_base->base_vector_size; i++) {
      Vec3d dir = m_provertex[i] - base;
      local = Vec3d(x_axis.dot(dir), y_axis.dot(dir), 0. );   // project to local coordinates
      if (std::abs(local.x) > c_eps || std::abs(local.y) > c_eps ) {   // skip vertexes projected near the base vertex
        local = local.normalized();
        if (local.x > max_proj) {
          max_proj = local.x;
          max_vertex = i;
        }
      }
    }
    return max_vertex;
  }

  int Pro_hull::convex_vertex_roll(int vindex) {    // find adjacent hull vertex for vertex

    double max_proj = std::numeric_limits<double>::lowest();
    double c_eps = 0.000000001;   // tolerance to avoid point clusters 
    int max_vertex = vindex;
    Vec3d base = m_provertex[vindex];      // a convexoid vertex
    Vec3d norm = base_dir(vindex);         // convexoid normal
    double proj;

    for (auto i = 0; i < 2 * m_base->base_vector_size; i++) {
      Vec3d dir = m_provertex[i] - base;
      if (std::abs(dir.x) > c_eps || std::abs(dir.y) > c_eps || std::abs(dir.z) > c_eps) {   // skip vertexes near the base vertex
        dir = dir.normalized();
        proj = norm.dot(dir);   // project to normal
        if (proj > max_proj) {
          max_proj = proj;
          max_vertex = i;
        }
      }
    }
    return max_vertex;
  }

  int Pro_hull::convex_edge_roll(int vindex0, int vindex1) {    // find adjacent hull vertex for edge

    Vec3d dir = m_provertex[vindex1] - m_provertex[vindex0];    // edge
    Vec3d norm = base_dir(vindex0);                             // out vector
    Vec3d dir1 = Vec3d::cross(norm, dir);                       // roll direction
    norm = Vec3d::cross(dir, dir1);
    dir1 = dir1.normalized();
    norm = norm.normalized();
    return convex_roll(vindex0, dir1, norm);
  }

  int Pro_hull::convex_face_roll(int vindex0, int vindex1, int vindex2) {  // find adjacent hull vertex for face
    Vec3d dir1 = m_provertex[vindex1] - m_provertex[vindex0];    // edge
    Vec3d dir2 = m_provertex[vindex2] - m_provertex[vindex0];    // rool back
    Vec3d norm = Vec3d::cross(dir2, dir1);   // face normal
    dir1 = Vec3d::cross(norm, dir1);          // roll direction
    dir1 = dir1.normalized();
    norm = norm.normalized();
    return convex_roll(vindex0, dir1, norm);
  }

  void Pro_hull::get_convexoid_faces(std::vector<Vec3i>& faces ) {  // get convexoid faces... some may be duplicated 

    int vindex0, vindex1;
    faces.clear();

    std::vector<bool> used_vertexes(2 * m_base->base_vector_size);    // usage vector

    for (auto i = 0; i < 2 * m_base->base_vector_size; i++) {   // roll around all vertexes

      for (vindex0 = 0; vindex0 < i; vindex0++) if ( m_provertex[i] == m_provertex[vindex0] ) break;   // check duplicated vertexes 
      if (vindex0 < i) continue;   // skip duplicates

      used_vertexes.assign(used_vertexes.size(), false); // clean used 
      vindex0 = convex_vertex_roll(i);   
      vindex1 = convex_edge_roll(i, vindex0);  
      used_vertexes[i] = true; used_vertexes[vindex0] = true; used_vertexes[vindex1] = true;  // mark used

      while (1) {  // roll around vertex until return to start
        if (vindex0 > i && vindex1 > i) faces.emplace_back(Vec3i(i, vindex0, vindex1));   // add a new face
        vindex0 = convex_face_roll(i, vindex1, vindex0);
        std::swap(vindex0, vindex1);
        if (used_vertexes[vindex1]) break;  // came back to used vertex => vertex roll is completed
        used_vertexes[vindex1] = true;      // mark used
      }
    }
  }

  // get convexoid slab for a direction
  void Pro_hull::get_extrema( const Vec3d&  dir, double &min_proj, double &max_proj, Vec3d& min_vertex, Vec3d& max_vertex) {

    min_proj = std::numeric_limits<double>::max(), 
    max_proj = std::numeric_limits<double>::lowest();

    for (auto& vertex : m_provertex) {
      double proj = dir.dot(vertex);
      if (proj < min_proj) {
        min_proj = proj;
        min_vertex = vertex;
      }
      if (proj > max_proj) {
        max_proj = proj;
        max_vertex = vertex;
      }
    }
  }

  void get_dir_extrema(std::vector<Vec3d>& vertexes, const Vec3d&  dir, double &min_proj, double &max_proj ) {
    min_proj = std::numeric_limits<double>::max(),
    max_proj = std::numeric_limits<double>::lowest();

    for (auto& vertex : vertexes) {
      double proj = dir.dot(vertex);
      if (proj < min_proj) {
        min_proj = proj;
      }
      if (proj > max_proj) {
        max_proj = proj;
      }
    }
  }

  // calculate a 2D convexoid projection fold of 3D convexoid for a direction
  void Pro_hull::get_projection_fold(const Vec3d& normal, const Vec3d& dir1, std::vector<Vec3d>& fold) {
    Vec3d dir2 = Vec3d::cross(normal, dir1);
    Vec3d min_point, max_point;
    double min, max;

    // 2D convexoid base... use 8 uniform directions
    constexpr double step = 0.125 * c_pi;
    double sins = sin(step), coss = cos(step);
    std::vector<Vec3d> base_8 = { {0., 1., 0.}, { sins, coss, 0. }, { 1., 1., 0. }, { coss, sins, 0. },
                                  {1., 0., 0.}, { coss, -sins, 0. }, {1., -1., 0.}, { sins, coss, 0. }, };

    for (int j = 0; j < 8; j++) {
      // get directions for 2D convexoid 
      Vec3d dir = dir1*base_8[j].x + dir2*base_8[j].y;
      dir = dir.normalized();
      get_extrema(dir, min, max, min_point, max_point);
      fold[j] = max_point;
      fold[j + 8] = min_point;
    }
  }

  double Pro_hull::get_metrics(Method method, Vec3d&  extent) {

    switch (method)
    {
    case Method::Minimal_diameter:
      return extent.length_sqr();
      break;
    case Method::Minimal_surface_area:
     return extent.x*extent.y + extent.y*extent.z + extent.z*extent.x;
      break;
    case Method::Minimal_volume:
      return extent.x*extent.y*extent.z;
      break;
    default:
      I3S_ASSERT(false); //something new
    }
    return 0.;
  }

  void extend_obb(const Vec3d& origin, const Vec3d* points, int count , Obb_abs& obb, Vec3d* obb_axis ) {

    Vec3d min_proj = Vec3d(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    Vec3d max_proj = Vec3d(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest());
    for( int i=0; i < count; ++i)
    {
      for (auto axis = 0; axis < 3; axis++) {
        double proj = obb_axis[axis].dot(points[i] - origin);
        if (proj < min_proj[axis]) {
          min_proj[axis] = proj;
        }
        if (proj > max_proj[axis]) {
          max_proj[axis] = proj;
        }
      }
    }
    // calculate OBB center and extents
    obb.center = origin + 0.5 *((min_proj.x + max_proj.x) * obb_axis[0] + (min_proj.y + max_proj.y) * obb_axis[1] + (min_proj.z + max_proj.z) * obb_axis[2]);
    obb.extents = 0.5f * Vec3f((float)(max_proj.x - min_proj.x), (float)(max_proj.y - min_proj.y), (float)(max_proj.z - min_proj.z));
  }

  static bool _is_equal(const Vec3d& a, const Vec3d& b)
  {
    const double c_epsi = 1e-6;
    return  std::abs(a.x - b.x) < c_epsi 
      &&    std::abs(a.y - b.y) < c_epsi 
      &&    std::abs(a.z - b.z) < c_epsi;
  }

  static void check_and_swap(Vec3f& extents, Vec3d obb_axis[3], int k)
  {
    Vec3d v[2] = { {0.,0.,0.},{ 0.,0.,0. } };
    v[0][k] = 1.; v[1][k] = -1.0;

    for (int i = 0; i < 3; ++i)
    {
      if (i == k)
        continue;
      for( int sign =0; sign <2; ++sign )
        if (_is_equal(obb_axis[i], v[sign]) )
        {
          obb_axis[i] = v[0]; //snap to it and take the "positive" ref vector. ( extent is absolute, so AABB axis sign is irrelevant)
          std::swap(obb_axis[i], obb_axis[k]);
          std::swap(extents[i], extents[k]);
          return;
        }
    }
  }

  static void snap_to_aabb( Obb_abs* obb, Vec3d obb_axis[3])
  {
    for (int i=0; i < 3; ++i)
      check_and_swap(obb->extents, obb_axis,i);
  }



  //---------------------------------------------------------------------------------------------------------------
  // calculate a ballbox for a convexoid -  a combination of OBB and BS

  void Pro_hull::get_ball_box(const Vec3d* points, int count
    , Obb_abs& obb, double& radius, Method method) {
    I3S_ASSERT(count > 0);
    if (count == 0)
    {
      obb.center = Vec3d(0.0);
      obb.orientation = utl::identity_quaternion<double>();
      obb.extents = Vec3f(std::numeric_limits<float>::max());
      return;
    }

    auto origin = std::accumulate(points + 1, points + count, points[0]) * (1.0 / count) ;
    // reset the Pro_hull
    std::fill(m_promin.begin(), m_promin.end(), std::numeric_limits<double>::max());
    std::fill(m_promax.begin(), m_promax.end(), std::numeric_limits<double>::lowest());

    // accumulate relative positions of all of the remaining points
    for (int i = 0; i < count; i++) 
      add(points[i] - origin);    // insert points into a convexoid

    Vec3d min_p, max_p; 
//    double size_x, size_y, size_z;
    double value, min2d = std::numeric_limits<double>::max(), min3d = std::numeric_limits<double>::max();
    Vec3d min_point, max_point, extent; 
    Vec3d min_point2, max_point2;
    Vec3d axis2_x, axis2_y;         // 2D OBB axes
    Vec3d obb_axis[3];              // OBB axes
    std::vector<Vec3d> fold(16);    // fixed 16 vertex convexoid projection fold

    // init as AABB..  it will be used if convexoid degraded into a point
    obb_axis[0] = Vec3d(1., 0., 0.);
    obb_axis[1] = Vec3d(0., 1., 0.);
    obb_axis[2] = Vec3d(0., 0., 1.);

    std::vector<Vec3i> faces;
    get_convexoid_faces( faces );  // construct connexoid faces

    if (faces.size() == 0) {   // no one face was detacted => degraded convexoid... linear or point
      int roll = convex_vertex_roll(0);  // try to roll over a vertex .. 
      if (roll) { // if the same vertex => linear degradation => use this line as one of OBB axes 
        obb_axis[0] = m_provertex[roll] - m_provertex[0];
        obb_axis[1] = (obb_axis[0].y != 0 || obb_axis[0].z != 0) ? Vec3d( 0., -obb_axis[0].z, obb_axis[0].y) : Vec3d(0., 1., 0.);
        obb_axis[2] = Vec3d::cross(obb_axis[0], obb_axis[1]);
        for (auto& axis : obb_axis) axis = axis.normalized();  // normalize axes 
      }
      // else there is a point degradation => use default AABB
    }

    //  if there are faces => find face aligned OBBs
    for (int i = 0; i < faces.size(); i++)  { // for each face of connexoid find the best fit OBB and select the best one for all faces
      
      if (m_provertex[faces[i][0]] == m_provertex[faces[i][1]] || 
          m_provertex[faces[i][1]] == m_provertex[faces[i][2]] ||
          m_provertex[faces[i][2]] == m_provertex[faces[i][0]] ) continue;   // skip degenerated faces;

      //  calculate face normal
      Vec3d edge1 = m_provertex[faces[i][1]] - m_provertex[faces[i][0]];
      Vec3d edge2 = m_provertex[faces[i][2]] - m_provertex[faces[i][0]];
      Vec3d face_normal = Vec3d::cross( edge1, edge2);
      face_normal = face_normal.normalized();

      get_extrema(face_normal, min_point.z, max_point.z, min_p, max_p);
      min_point2.z = min_point.z;  max_point2.z = max_point.z;
      extent.z = max_point2.z - min_point2.z;

      // now calculate 2D OBB for projection to this face
      get_projection_fold(face_normal, edge1, fold);  // projection fold for face normal 

      min2d = std::numeric_limits<double>::max();
      for (int j = 0; j < 16; j++) {    // loop aroind 2D convexoid to find 2D OBB
        //  calculate fold edge normal, check that the fold edge is not degenerative  
        Vec3d dir_x = fold[(j+1)%8] - fold[j];
        if (dir_x == Vec3d({ 0., 0., 0. }) )  continue;   // skip degenerated edges;

        // set potential x,y box directions 
        Vec3d dir_y = Vec3d::cross(face_normal, dir_x);

        if (dir_y == Vec3d(0, 0, 0)) 
          continue;

        dir_x = Vec3d::cross(dir_y, face_normal);
        dir_x = dir_x.normalized();
        dir_y = dir_y.normalized();

        get_dir_extrema(fold, dir_x, min_point2.x, max_point2.x);
        get_dir_extrema(fold, dir_y, min_point2.y, max_point2.y);

        extent.x = max_point2.x - min_point2.x;    // x dimension
        extent.y = max_point2.y - min_point2.y;    // y dimension
        
        value = extent.x * extent.y;     // use area for 2D 
        if ( value < min2d) {
            min2d = value;
            min_point = min_point2;
            max_point = max_point2;
            axis2_x = dir_x;
            axis2_y = dir_y;
        }
      }

      extent = max_point - min_point;
      value = get_metrics(method, extent);
      if (value < min3d) {    // potential minimal 3D box
          min3d = value;
          obb_axis[0] = axis2_x;
          obb_axis[1] = axis2_y;
          obb_axis[2] = face_normal;
      }
    }

    // now OBB axes are determined, calculate OBB with these axes for the whole point set
    extend_obb(origin, points, count, obb, obb_axis);

    // calculate a radius of a bounding sphere
    radius = 0.0;
    for(int i=0; i < count; ++i)
    {
      double dist = obb.center.distance(points[i]); 
      if (dist > radius) radius = dist;
    }

    //snap to AABBB
    snap_to_aabb( &obb, obb_axis);

    //create the rotation matrix
    Mat4d R;
    R._11 = obb_axis[0].x;
    R._12 = obb_axis[0].y;
    R._13 = obb_axis[0].z;
    R._14 = 0.0;

    R._21 = obb_axis[1].x;
    R._22 = obb_axis[1].y;
    R._23 = obb_axis[1].z;
    R._24 = 0.0;

    // get a sign of orientation of obb axises 
    double sign = Vec3d::dot(Vec3d::cross(obb_axis[0], obb_axis[1]), obb_axis[2]);

    R._31 = (sign > 0.) ? obb_axis[2].x : -obb_axis[2].x;
    R._32 = (sign > 0.) ? obb_axis[2].y : -obb_axis[2].y;
    R._33 = (sign > 0.) ? obb_axis[2].z : -obb_axis[2].z;
    R._34 = 0.0;

    R._41 = 0.0;
    R._42 = 0.0;
    R._43 = 0.0;
    R._44 = 1.0;

    obb.orientation = rotation_matrix_to_quaternion(R);
  }

  //--------------------------------------------------------------------------------------------------------------------
  //  Hierarchy methods
  //--------------------------------------------------------------------------------------------------------------------

  // match all elements of a list1 toward a list2 
  void Hierarchy::list_list_match(int list1, int list2) {
    // find best match for each node in a list... greedy algorithm
    for (auto node = list1; node; node = hierarchy[node].next) {
      list_node_match(list2, node);
    }
  }

  // find the best match and distance for a node <node1> in a list <list_id>. Store info in the cluster.
  void Hierarchy::list_node_match(int list_id, int node1) {

    auto cluster = &clusters[hierarchy[node1].cluster];

    for (int node2 = list_id; node2; node2 = hierarchy[node2].next) { // run through the list 
      if (node2 == node1) continue; // skip node itself
      
      auto size = cluster->hull.common_mean_proj_cmp(clusters[hierarchy[node2].cluster].hull, cluster->size);   // calc distance between nodes as a size of common hull
      if (size < cluster->size) {   // update match info if distance between clusters is better 
        cluster->match = node2;
        cluster->size = size;
      }
    }
  }

  // get the best match for all nodes in a list 
  int Hierarchy::list_best_match(int list_id) {

    int best = 0;
    double maxsize = std::numeric_limits<double>::max();

    for (int node = list_id; node; node = hierarchy[node].next) {
      auto size = clusters[hierarchy[node].cluster].size;
      if (size < maxsize) {
        maxsize = size;
        best = node;
      }
    }
    return best;
  }

  // remove node from list
  void Hierarchy::list_remove(int list_id, int node) {

    int i;
    for (i = list_id; i; i = hierarchy[i].next) {
      if (hierarchy[i].next == node) break;
    }
    hierarchy[i].next = hierarchy[node].next;
  }

  void Hierarchy::list2vector(int list_id, std::vector<int>& child) {
    for (int node = hierarchy[list_id].child; node; node = hierarchy[node].next) {
      child.push_back(node);
    }
  }

  // join matching nodes into a new list and remove them from the old list, replace them by a new joint node
  void Hierarchy::list_join_match(int& list_id, int node1, int node2) {

    int new_index = (int)hierarchy.size();

    hierarchy.resize(new_index + 1);  // add a node
    clusters.resize(new_index + 1, Cluster( &m_base ));   // add a cluster

    // create a new cluster joining clusters' hulls
    clusters[new_index].id = 0;      // non-leaf nodes have no feature id
    clusters[new_index].hull = Pro_hull(&m_base);
    clusters[new_index].hull.add(clusters[hierarchy[node1].cluster].hull);
    clusters[new_index].hull.add(clusters[hierarchy[node2].cluster].hull);
    clusters[new_index].match = 0;                  // no match
    clusters[new_index].size = std::numeric_limits<double>::max();    // init distance value

    hierarchy[new_index].cluster = new_index;
    hierarchy[new_index].child = node1;
    hierarchy[new_index].next = list_id;

    list_id = new_index;            // insert new node into the list

    // remove match nodes from the list and create a new siblings list
    list_remove(list_id, node1);
    list_remove(list_id, node2);
    hierarchy[node1].next = node2;
    hierarchy[node2].next = 0;
  }

  // calculate common hull for the whole subset
  void Hierarchy::common_hull(Pro_hull& hull, int beg, int end) {

    for (auto i = beg; i < end; i++) {
      hull.add(clusters[hierarchy[i].cluster].hull);
    }
  }

  //-----------------------------------------------------------------------------------------------------------------------------------
  // run through the set and calculate the longest projection direction, then split along this direction using some mediane value 
  // all primitives in left part are in one halfspace, and in right part are in another
  int Hierarchy::find_partition(int beg, int end) {

    Pro_hull  hull = Pro_hull(&m_base);

    common_hull(hull, beg, end);          // calculate a common hull for all clusters in set 
    int max_p = hull.principal_dimension_obb();    // calculate principal dimension

    // Option 1. sort the set by max direction  using lambda function
    for (auto i = beg; i < end; i++) {   // precalc sort values for max_p dimension and store it in size 
      clusters[hierarchy[i].cluster].size = clusters[hierarchy[i].cluster].hull.center(max_p);
    }
    std::sort(hierarchy.begin() + beg, hierarchy.begin() + end,
      [&](const Hierarchy_node& l, const Hierarchy_node& r) {
      return clusters[l.cluster].size < clusters[r.cluster].size;
    });
    int mid = (beg + end) / 2; // true mediane 

    /*
    // Option 2. do not sort, but just split by mid value and order 2 parts .... should be more eficient, but less balanced

    value= hull.mid(max_p)
    while ( beg < end ) {
      while ( clusters[hierarchy[beg].cluster].hull.mid(max_p) < value ) beg++;
      while ( clusters[hierarchy[end].cluster].hull.mid(max_p) >= value ) end--;
      if ( beg < end ) swap(hierarchy[beg],hierarchy[end]);
    }
    int mid = beg
    */

    return mid;
  }

  //----------------------------------------------------------------------------------------------------------
  // this is bottom to top joining stage
  // reduce the number of nodess in a list to a given value

  void Hierarchy::join_up(int& list_id, int target_size) {

    int list_size = 0;
    for (auto node = list_id; node; node = hierarchy[node].next) list_size++;

    while (list_size > target_size) {     

      auto node1 = list_best_match(list_id);   // find the best matching pair in the list
      auto node2 = clusters[hierarchy[node1].cluster].match;

      list_join_match(list_id, node1, node2);    // list_id is set into a new joint node
      list_size--;

      // update matches for all related nodes
      list_node_match(list_id, list_id);   // match a new node toward list 

      // rematch nodes linked to removed matches node1 and node2
      for (auto node = list_id; node; node = hierarchy[node].next) {  
        auto match = clusters[hierarchy[node].cluster].match;
        if (match == node1 || match == node2) {  
          clusters[hierarchy[node].cluster].match = 0;                  // no match
          clusters[hierarchy[node].cluster].size = std::numeric_limits<double>::max();  // init distance value
          list_node_match(list_id, node);
        }
      }
    }
  }

  //----------------------------------------------------------------------------------------------------------
  //  this is top to bottom stage
  //  spliting a set of original clusters into 2 smaller sets

  int Hierarchy::split_down(int beg, int end) {

    int list_id;

    if (end - beg > c_downsize) {     // a set is too big => split it... here could be any other check for complexity of the scene in the set 

      int mid = find_partition(beg, end);   // find a partinion element. set may be resorted.
      int list_l = split_down(beg, mid);    // recursive split by the median into 2 sets
      int list_r = split_down(mid, end);

      list_list_match(list_l, list_r);    // crossmatch left and right parts 
      list_list_match(list_r, list_l);

      // join list_l and list_r lists into one 
      int leftend=list_l;
      for (auto node = list_l; node; node = hierarchy[node].next) leftend = node;
      hierarchy[leftend].next = list_r;
      list_id = list_l;
    }
    else {    // create a list from a set

      for (auto i = beg; i < end; i++) hierarchy[i].next = i + 1;   // link elements
      hierarchy[end - 1].next = 0;
      list_id = beg;

      list_list_match(list_id, list_id);       // find matches in the new list
    }

    join_up(list_id, c_upsize);    // now join some of elements in the list to have upsize elements in total

    return list_id;
  }

  //----------------------------------------------------------------------------------------------------------------------------------
  // BVH builder methods 
  //----------------------------------------------------------------------------------------------------------------------------------


  Bvh_builder::Bvh_builder()
    : m_impl(new Hierarchy()) 
  {
    m_impl->hierarchy.resize(1);
  }

  Bvh_builder::~Bvh_builder() {}

  void Bvh_builder::add_feature(int64_t id, const Point3d& origin, const Point3f* vertices, int vertices_count) {

    auto feature_count = m_impl->hierarchy.size();

    m_impl->hierarchy.resize(feature_count + 1);
    m_impl->clusters.resize(feature_count + 1, Cluster( &m_impl->m_base));

    m_impl->hierarchy[feature_count].cluster = (int)feature_count;        // set cluster link
    m_impl->clusters[feature_count].hull = Pro_hull(&m_impl->m_base);    // init a prohull 
    m_impl->clusters[feature_count].id = id;                    // save feature id, but not its' geometry
    m_impl->clusters[feature_count].match = 0;                  // no match
    m_impl->clusters[feature_count].size = std::numeric_limits<double>::max();    // init distance value

    for (int i = 0; i < vertices_count; i++) {                  // feed feature vertexes 
      Vec3d point = reinterpret_cast<const Vec3d&>(origin) + Vec3d( reinterpret_cast<const Vec3f&>(vertices[i]) );
      m_impl->clusters[feature_count].hull.add(point);
    }
  }

  void  Bvh_builder::build_tree(std::vector<Bvh_node>& tree, double scale) {

    auto feature_count = m_impl->hierarchy.size();
    int list_id = m_impl->split_down(1, (int)feature_count);
    m_impl->join_up(list_id, 1);        // combine all clusters into a list containig just one cluster
                      // convert hierarchy to BVH tree
    Bvh_tree = tree;
    cluster_scale = scale;

    // make node 0 a root node of the tree
    Bvh_tree.resize(1);
    Bvh_tree[0].feature_id = 0;   // no feature id in the root node
    m_impl->clusters[list_id].hull.get_bounding_sphere(reinterpret_cast<Vec3d&>(Bvh_tree[0].mbs_center), Bvh_tree[0].mbs_radius);  // bounding sphere

    int child_id = m_impl->hierarchy[list_id].child;

    if (child_id == 0) {  // special case of just one cluster in a scene, so there are no childs...
      Bvh_tree.resize(2);    

      Bvh_tree[1].feature_id = m_impl->clusters[m_impl->hierarchy[1].cluster].id;  // copy id info
      Bvh_tree[1].mbs_center = Bvh_tree[0].mbs_center;   // same sphere
      Bvh_tree[1].mbs_radius = Bvh_tree[0].mbs_radius;

      Bvh_tree[1].parent = 0;
      Bvh_tree[0].child.push_back(1);
    }
    else {   // general case 
      filter_kids(list_id, child_id, 0);
    }
  }

  // traverse hierarcy recursively, filter unnecessary kids, and create N-ary tree
  void Bvh_builder::filter_kids(int parent_id, int child_id, int target_id) {

    if (child_id == 0) return;  // stop recursion if there is no child

    Vec3d origin;
    double radius;

    m_impl->clusters[m_impl->hierarchy[child_id].cluster].hull.get_bounding_sphere(origin, radius);

    if ( radius < Bvh_tree[target_id].mbs_radius/cluster_scale  || m_impl->hierarchy[child_id].child == 0 ) {   // cluster diameter is acceptable 
      auto size = Bvh_tree.size();  // new node index
      Bvh_tree.resize(size + 1);    // child is good add a node to BVH tree 

      Bvh_tree[size].feature_id = m_impl->clusters[m_impl->hierarchy[size].cluster].id;  // copy id info
      Bvh_tree[size].mbs_center = { origin.x, origin.y, origin.z };
      Bvh_tree[size].mbs_radius = radius;

      Bvh_tree[size].parent = target_id;
      Bvh_tree[target_id].child.push_back((int)size);

      filter_kids(child_id, m_impl->hierarchy[child_id].child, (int)size); // continue with grandkids 
    }
    else { // traverse grandkids as kids 
      filter_kids(parent_id, m_impl->hierarchy[child_id].child, target_id);
    }
    filter_kids(parent_id, m_impl->hierarchy[child_id].next, target_id);// traverse next sibling
  }
}

//-------------------------------------------------------------------------------------------------------------------------
//  debug functions 


namespace utl
{

  struct Feature
  {
    int id;
    Vec3d origin;
    std::vector< Vec3f > xyz;       // triples of points. Each triple is a triangle.

    void scale_feature(const Vec3f& scale) {    // coordinate systems scaling
      origin = origin * static_cast<Vec3d>(scale);

      for (auto& point : xyz) {
        point = point * scale;
      }
    }
  };

  class Feature_reader
  {
  public:
    Feature_reader() {}

    bool     open_bin(const std::filesystem::path& path);
    bool     close_bin();
    bool     next_feature(Feature* out);
  private:
    std::ifstream m_ifs;
  };

  inline bool Feature_reader::open_bin(const std::filesystem::path& path)
  {
    m_ifs.open(path, std::ios::binary);
    return m_ifs.good();
  }

  inline bool Feature_reader::close_bin()
  {
    m_ifs.close();
    return m_ifs.good();
  }

  inline bool Feature_reader::next_feature(Feature* out)
  {
    static const int c_magic = 80888;
    struct Hdr
    {
      int magic, id, vtx_count;
      Vec3d origin;
    };
    //read the header:
    Hdr hdr;
    m_ifs.read(reinterpret_cast< char*>(&hdr), sizeof(hdr));
    if (m_ifs.fail() || hdr.magic != c_magic || hdr.vtx_count == 0)
      return false;
    out->id = hdr.id;
    out->origin = hdr.origin;
    out->xyz.resize(hdr.vtx_count);
    m_ifs.read(reinterpret_cast< char*>(out->xyz.data()), out->xyz.size() * sizeof(out->xyz[0]));

   //Vec3f scale(111000., 111000., 1.);
 //  out->scale_feature(scale);

    return !m_ifs.fail();

  }

  class Feature_writer
  {
  public:
    Feature_writer() {}
    bool     open_bin(const std::filesystem::path& path);
    bool     close_bin();
    bool     next_feature(Feature* out);
  private:
    std::ofstream m_ofs;
  };

  inline bool Feature_writer::open_bin(const std::filesystem::path& path)
  {
    m_ofs.open(path, std::ios::binary);
    return m_ofs.good();
  }

  inline bool Feature_writer::close_bin()
  {
    m_ofs.close();
    return m_ofs.good();
  }

  inline bool Feature_writer::next_feature(Feature* out)
  {
    static const int c_magic = 77777;
    struct Hdr
    {
      int magic, id, vtx_count;
      Vec3d origin;
    };
    //write the header:
    Hdr hdr;

    //Vec3f scale(1.f / 111000.f, 1.f / 111000.f, 1.f);
    // out->scale_feature(scale);

    hdr.magic = c_magic;
    hdr.id = out->id;
    hdr.vtx_count = (int)out->xyz.size();
    hdr.origin = out->origin;


    m_ofs.write(reinterpret_cast< char*>(&hdr), sizeof(hdr));
    if (m_ofs.fail() || hdr.vtx_count == 0)
      return false;

    m_ofs.write(reinterpret_cast< char*>(out->xyz.data()), out->xyz.size() * sizeof(out->xyz[0]));
    return !m_ofs.fail();

  }

  //void make_feature(const Pro_hull& h, Feature* feature) {   // make ouitput elements from hull

  //  I3S_ASSERT(feature);

  //  int best = 0;    // best box

  //  int dir_x, dir_y, dir_z;   // axis directions of OBB
  //  double size_x, size_y, size_z, area;
  //  double min_area = std::numeric_limits<double>::max();    // minimal projection area

  //  for (int i = 0; i < c_base_obb_size; i++) {    // check all OBB of a hull

  //    dir_x = h.get_base().obb[i].x;
  //    dir_y = h.get_base().obb[i].y;
  //    dir_z = h.get_base().obb[i].z;

  //    // common oob extents 
  //    size_x = h.get_promax()[dir_x] - h.get_promin()[dir_x];
  //    size_y = h.get_promax()[dir_y] - h.get_promin()[dir_y];
  //    size_z = h.get_promax()[dir_z] - h.get_promin()[dir_z];

  //    area = size_x*size_y + size_y*size_z + size_z*size_x;

  //    if (area < min_area) {
  //      min_area = area;
  //      best = i;
  //    }
  //  }

  //  dir_x = h.get_base().obb[best].x;
  //  dir_y = h.get_base().obb[best].y;
  //  dir_z = h.get_base().obb[best].z;

  //  feature->origin = 0.5 * (h.get_promax()[dir_x] + h.get_promin()[dir_x]) * h.get_base().dir[dir_x] +
  //    0.5 * (h.get_promax()[dir_y] + h.get_promin()[dir_y]) * h.get_base().dir[dir_y] +
  //    0.5 * (h.get_promax()[dir_z] + h.get_promin()[dir_z]) * h.get_base().dir[dir_z];

  //  Vec3f dx, dy, dz;   // extent vectors of OBB
  //  dx = static_cast<Vec3f>(0.5 * (h.get_promax()[dir_x] - h.get_promin()[dir_x]) * (h.get_base().dir[dir_x]));
  //  dy = static_cast<Vec3f>(0.5 * (h.get_promax()[dir_y] - h.get_promin()[dir_y]) * (h.get_base().dir[dir_y]));
  //  dz = static_cast<Vec3f>(0.5 * (h.get_promax()[dir_z] - h.get_promin()[dir_z]) * (h.get_base().dir[dir_z]));

  //  Vec3f c[8];        // 8 corner vertexes of OBB

  //  c[0] = dx + dy + dz;
  //  c[1] = dx + dy - dz;
  //  c[2] = dx - dy + dz;
  //  c[3] = dx - dy - dz;
  //  c[4] = -dx + dy + dz;
  //  c[5] = -dx + dy - dz;
  //  c[6] = -dx - dy + dz;
  //  c[7] = -dx - dy - dz;

  //  /*
  //  // write down triples of corner vertexes creating 12 triangles representing faces of OBB
  //  feature->xyz[ 0] = c[3]; feature->xyz[ 1] = c[1]; feature->xyz[ 2] = c[0];    // 0, 1, 3
  //  feature->xyz[ 3] = c[0]; feature->xyz[ 4] = c[2]; feature->xyz[ 5] = c[3];    // 3, 2, 0
  //  feature->xyz[ 6] = c[6]; feature->xyz[ 7] = c[7]; feature->xyz[ 8] = c[3];    // 3, 7, 6
  //  feature->xyz[ 9] = c[3]; feature->xyz[10] = c[2]; feature->xyz[11] = c[6];    // 6, 2, 3
  //  feature->xyz[12] = c[0]; feature->xyz[13] = c[4]; feature->xyz[14] = c[6];    // 6, 4, 0
  //  feature->xyz[15] = c[6]; feature->xyz[16] = c[2]; feature->xyz[17] = c[0];    // 0, 2, 6

  //  feature->xyz[18] = c[7]; feature->xyz[19] = c[6]; feature->xyz[20] = c[4];    // 4, 6, 7
  //  feature->xyz[21] = c[4]; feature->xyz[22] = c[5]; feature->xyz[23] = c[7];    // 7, 5, 4
  //  feature->xyz[24] = c[1]; feature->xyz[25] = c[3]; feature->xyz[26] = c[7];    // 7, 3, 1
  //  feature->xyz[27] = c[7]; feature->xyz[28] = c[5]; feature->xyz[29] = c[1];    // 1, 5, 7
  //  feature->xyz[30] = c[4]; feature->xyz[31] = c[0]; feature->xyz[32] = c[1];    // 1, 0, 4
  //  feature->xyz[33] = c[1]; feature->xyz[34] = c[5]; feature->xyz[35] = c[4];    // 4, 5, 1
  //  */

  //  // 24 edge segments of OBB
  //  feature->xyz[0] = c[0]; feature->xyz[1] = c[1];
  //  feature->xyz[2] = c[1]; feature->xyz[3] = c[3];
  //  feature->xyz[4] = c[3]; feature->xyz[5] = c[2];
  //  feature->xyz[6] = c[2]; feature->xyz[7] = c[0];
  //  feature->xyz[8] = c[0]; feature->xyz[9] = c[4];
  //  feature->xyz[10] = c[1]; feature->xyz[11] = c[5];
  //  feature->xyz[12] = c[3]; feature->xyz[13] = c[7];
  //  feature->xyz[14] = c[2]; feature->xyz[15] = c[6];
  //  feature->xyz[16] = c[6]; feature->xyz[17] = c[4];
  //  feature->xyz[18] = c[4]; feature->xyz[19] = c[5];
  //  feature->xyz[20] = c[5]; feature->xyz[21] = c[7];
  //  feature->xyz[22] = c[7]; feature->xyz[23] = c[6];

  //}

  // traverse hierarcy recursively and create output features as OBBs of clusters
  /*static void traverse_obb(Hierarchy* hr, int list_id, int level, Feature_writer& fwriter, Feature* feature) {

    while (list_id) {
      if (level < 10) {
        auto hull = hr->clusters[hr->hierarchy[list_id].cluster].hull;
        make_feature(hull, feature);
        feature->id = level;
        fwriter.next_feature(feature);
      }

      auto child = hr->hierarchy[list_id].child;
      if (child) traverse_obb( hr, child, level + 1, fwriter, feature);

      list_id = hr->hierarchy[list_id].next;
    }
  }*/

  // traverse hierarcy recursively and create a segment tree representing BVH
  static void traverse_bvh_tree(std::vector<Bvh_node>& tree, int node, int level, Feature_writer& fwriter) {

    Feature feature;
    feature.id = level;
    
    const auto &center = tree[node].mbs_center;
    feature.origin = { center.x, center.y, center.z };

    for (auto kid : tree[node].child) {
      feature.xyz.push_back(Vec3f(0.,0.,0.));
      
      const auto& kc = tree[kid].mbs_center;
      feature.xyz.push_back(Vec3f(Vec3d{kc.x, kc.y, kc.z} - feature.origin));  // add centroid 
      
      if ( tree[kid].child.size() && level < 16 )
        traverse_bvh_tree(tree, kid, level + 1, fwriter);    // for non-leaf nodes traverse further
    }
    fwriter.next_feature(&feature);
  }


  void Bvh_builder::debug_write( const std::filesystem::path& path )
  {
    Feature_writer fwriter;
    
    Feature feature;
    feature.xyz.resize(24);    // 12 segments 2 vertexes  // all box features will have 12 triangles or 36 vertexes

    //int list_id = (int)m_impl.get()->hierarchy.size() - 1;    // list head for inner tree structure for debugging

                   // convert cluster hierarchy volumes into boxes represented by triangle soups and write them into a file
    if (fwriter.open_bin(path)) {
//      traverseOBB(m_impl.get(), list_id, 0, fwriter, &feature);
      traverse_bvh_tree( Bvh_tree, 0, 0, fwriter);    // traverse hierarchy tree statrting from head 0 node
    }
    fwriter.close_bin();

  }

  void Bvh_builder::debug_read(const std::filesystem::path& path) {
    Feature_reader freader;

    Feature feature;
    int feature_count = 0;
    long vertex_count = 0;

    if (freader.open_bin(path)) {
      while (freader.next_feature(&feature)) {
        
          feature_count++;
          vertex_count += (int)feature.xyz.size();

          add_feature(feature.id, reinterpret_cast<const Point3d&>(feature.origin),
            reinterpret_cast<Point3f *>(feature.xyz.data()), (int)feature.xyz.size());
      
      }
    }
    freader.close_bin();
  }

}

} // namespace i3slib
