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

#include "utils/utl_geom.h"
#include "utils/utl_obb.h"

#pragma warning(push)
#pragma warning(disable : 4251)

// -----------------------------------

namespace i3slib
{

namespace utl
{

  static const int c_base_vector_size = 37;    // size of projection set
  static const int c_base_obb_size = 22;       // size of OBB set
  static const int c_base_face_size = 0;       // size of face set

  // disdyakis dodecahedron base 
  static const int c_base_vector_size_1 = 13;  
  static const int c_base_obb_size_1 = 4;      
  static const int c_base_face_size_1 = 48;     
  //----------------------------------------------------------------------------------------------------------
  //   Projection Base Set
  //----------------------------------------------------------------------------------------------------------

  struct  I3S_EXPORT Pro_set
  {
    // polyhedron bases for convexoids 
    enum class Polyhedron :int { 
      Octahedron = 1,     // 3
      Cube = 2,           // 4
      Icosohedron = 3,    // 6
      Dodecahedron = 4,   // 10
      Rhombic_triacontahedron = 5,  // 16
      Disdyakis_dodecahedron = 6,    // 13 
      Rhombohedron = 7 //  37 
    };

    std::vector<Vec3d> dir;    // projection directions set.. vertexes on the unit sphere
    std::vector<Vec3i> obb;    // set of fixed OBBs 
    int base_vector_size;           // number of directions
    int base_obb_size;              // number of base OBBs

    Pro_set();                       // constructor
    Pro_set( Polyhedron base);       // constructor

    // get_default returns pointer to statically cached instance of Pro_set(Polyhedron::Rhombic_triacontahedron)
    static const Pro_set* get_default();
  };

  //----------------------------------------------------------------------------------------------------------
  //   Projection Hulls and Convexoids 
  //----------------------------------------------------------------------------------------------------------

  /**
  // typical usage to compute oriented bounding box and concentric ball of an object:

    Pro_hull hull(Pro_set::get_default());
    std::vector<Vec3d> points;  // array of all points of the object
	  Obb_abs box;                  // oriented box
    double radius;                    // radius of concentric bounding ball
    hull.get_ball_box(points, box, radius, Pro_hull::Method::Minimal_surface_area);
  */
  class  I3S_EXPORT Pro_hull {

  public:
    enum class Method :int  { Minimal_diameter=0, Minimal_surface_area=1, Minimal_volume=2 };
    explicit Pro_hull(const Pro_set* proset = Pro_set::get_default());
    // calculate convexoid OBB for a set of points. 
    void get_ball_box(const std::vector<Vec3d>& points
      , Obb_abs& obb, double& radius, Method method = Pro_hull::Method::Minimal_surface_area)
    { 
      return get_ball_box(points.data(), (int)points.size(), obb, radius, method); 
    }    
    void get_ball_box(const Vec3d* points, int count
      , Obb_abs& obb, double& radius, Method method = Pro_hull::Method::Minimal_surface_area);

  private:
    friend class Bvh_builder;
    friend class Hierarchy;

    Pro_hull(const Pro_set* proset, const Vec3d& vector);

    //----------------------------------------------------------------------------------------------------------
    void clear();

    double center(int dir) const;      // middle value of projection for a direction
    double extent(int dir) const;      // size of projection for a direction

    void add(const Pro_hull& h);        // add a hull
    void add(const Vec3d& vector);    // add a point

    int principal_dimension_max() const;      // calculate principal dimension using maximal projection direction
    int principal_dimension_pca() const;      // calculate principal dimension using some variation of PCA ( Principal Componet Analysis )
    int principal_dimension_obb() const;      // calculate principal dimension using OBB set

    double common_mean_proj(const Pro_hull& h) const;      // mean projection of a hull
    double common_mean_proj_cmp(const Pro_hull& h, double current_distance) const; // distance with inline comparison 
    double common_max_proj(const Pro_hull& h) const;        // max projection of a hull
    double common_min_area_obb(const Pro_hull& h) const;      // minimal area OOB of a hull
    double common_mean_diag_obb(const Pro_hull& h) const;      // mean diagonal of OOBs of a hull

    void get_bounding_sphere(Vec3d& center, double& radius) const;   // calculate a bounding sphere 
    void get_bounding_box(Obb_abs& obb, Method method= Method::Minimal_diameter) const;      // calc a best circumscribed obb

    //accessor for debug:
    const Pro_set&          get_base() const { return *m_base; }
    const std::vector<double>&    get_promin() const { return m_promin; }      // minimal projection values
    const std::vector<double>&    get_promax() const { return m_promax; }       // maximal projection values

  private:
    Vec3d base_dir(int vindex);
    int convex_roll(int vindex, Vec3d& x_axis, Vec3d& y_axis);
    int convex_vertex_roll(int vindex);
    int convex_edge_roll(int vindex0, int vindex1);
    int convex_face_roll(int vindex0, int vindex1, int vindex2);

    double get_metrics(Method method, Vec3d&  extent);
    void get_extrema(const Vec3d& dir, double &min_proj, double &max_proj, Vec3d& min_vertex, Vec3d& max_vertex); // extermal slab of convexoid for direction
    void get_projection_fold( const Vec3d& normal, const Vec3d& dir, std::vector<Vec3d>& fold); // calculate a projection fold of convexoid for a direction
    void get_convexoid_faces(std::vector<Vec3i>& faces);    // construct faces of inner convexoid

    // base direction set of a hull
    const Pro_set* m_base;
    std::vector<double> m_promin;       // minimal projection values
    std::vector<double> m_promax;       // maximal projection values

    // convexoids vertexes 
    std::vector<Vec3d> m_provertex;   // convexoid vertexes .. twice a number of base directions 

  };

}

} // namespace i3slib

#pragma warning(pop)
