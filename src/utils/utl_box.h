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
#include "utils/utl_serialize.h"
#include "utils/utl_geom.h"

namespace i3slib
{

namespace utl 
{
  
//! IF we need to support box of integer with proper rounding to nearest, we'll template-specialized the divXXX fctions to do this
template< class T > T div_by2( T v ) { return v / T(2); }
template< class T > T div_it( T a, T b ) { return a / b; }

//! templated AABB 
template< class T >
class Box 
{
public:
  SERIALIZABLE( Box<T> );
  typedef Vec3<T> Point_t;
  enum class Dim { Left=0, Right, Bottom, Top, Front, Back, Count };
  Box() { set_empty(); }
  Box( const Vec3<T>& a, const Vec3<T>& b ) { set_empty(); expand( a ); expand( b ); } // sub-obptimal, but good enough for now.
  Box(T left, T bottom, T right, T top, T front, T back) : m_({ left, right, bottom, top, front, back }) {}
  template< class Y > Box( const Box<Y>& b ) : m_({ (T)b.m_[0], (T)b.m_[1],(T)b.m_[2],(T)b.m_[3],(T)b.m_[4],(T)b.m_[5] }) {}
  bool      is_empty() const noexcept { return m_[0] == std::numeric_limits<T>::max(); }
  void      set_empty()      noexcept { m_[0] = std::numeric_limits<T>::max(); }
  T         width() const    noexcept { return right() - left(); }
  T         height() const   noexcept { return top() - bottom(); }
  T         depth() const    noexcept { return back() - front(); }
  void      expand( const Vec3<T>& v ) noexcept;
  void      expand( const Box<T>& b );
  void      scale_by( T factor );
  T         operator[]( Dim d ) const noexcept { return m_[d]; }
  T&        operator[]( Dim d )      noexcept { return m_[d]; }
  Box<T>&   operator*(T s) noexcept { for (auto& v : m_) v *= s; return *this; }
  bool      collides3d(const Box<T>& b) const;
  bool      collides2d(const Box<T>& b) const;
  bool      collides2d( const Vec2<T>& pt ) const;
  bool      is_inside2d( const Box<T>& b, T epsi=T(0) ) const;
  bool      is_inside( const Box<T>& b, T epsi=T(0) ) const;
  bool      is_inside( const Vec3<T>& b, T epsi=T(0) ) const;
  void      clamp_to(const Box<T>& b);
  void      move_by(const Vec3<T>& d) noexcept { left() += d.x; right() += d.x; bottom() += d.y; top() += d.y; front() += d.z; back() += d.z; }
  Vec2<T>   bottom_left() const       noexcept { return Vec2<T>(left(), bottom()); }
  Vec3<T>   bottom_left_front() const noexcept { return Vec3<T>(left(), bottom(), front()); }
  Vec3<T>   top_right_back() const    noexcept { return Vec3<T>(right(), top(), back()); }
  Vec2<T>   bottom_right() const      noexcept { return Vec2<T>(right(), bottom()); }
  Vec2<T>   top_left() const          noexcept { return Vec2<T>(left(), top()); }
  Vec2<T>   top_right() const         noexcept { return Vec2<T>(right(), top()); }
  double    area2d() const            noexcept { return is_empty() ? 0.0 : (double)width() * (double)height(); }
  Vec3<T>   center() const            noexcept { return Vec3<T>(div_by2(left() + right()), div_by2(bottom() + top()), div_by2(front() + back())); }
  
  template< class Ar > void serialize(Ar& ar)
  {
    static const char* names[6] = { "left", "right", "bottom", "top", "front", "back" };
    for (auto i = 0; i < (int)Dim::Count; i++)
      ar & nvp(names[i], m_[i]);
  }

  template< class Y > void get_corners( Y* pts, int n ) const;
  
  Box<T>&   operator*=(T s)                        noexcept { if (!is_empty()) { for (auto& x : m_) x *= s; } return *this; }

  Box<T>    slice2d( int x, int y, int w, int h ) const;

  bool      _is_degenerated() const                 noexcept { if (is_empty()) return false; return left() > right() || bottom() > top() || front() > back(); }

  const T&  left() const   noexcept { return m_[ (int)Dim::Left ]; }
  const T&  right() const  noexcept { return m_[ (int)Dim::Right ]; }
  const T&  bottom() const noexcept { return m_[ (int)Dim::Bottom ]; }
  const T&  top() const    noexcept { return m_[ (int)Dim::Top ]; }
  const T&  front() const  noexcept { return m_[ (int)Dim::Front ]; }
  const T&  back() const   noexcept { return m_[ (int)Dim::Back ]; }
  T&        left()         noexcept { return m_[ (int)Dim::Left ]; }
  T&        right()        noexcept { return m_[ (int)Dim::Right ]; }
  T&        bottom()       noexcept { return m_[ (int)Dim::Bottom ]; }
  T&        top()          noexcept { return m_[ (int)Dim::Top ]; }
  T&        front()        noexcept { return m_[ (int)Dim::Front ]; }
  T&        back()         noexcept { return m_[ (int)Dim::Back ]; }
//private:
  std::array<T, (size_t)Dim::Count > m_;
  friend std::ostream& operator<<( std::ostream& out, const Box<T>& b ) { out <<"[" << b.left() << ", " << b.right()<< ", " << b.bottom() << ", " << b.top() << " :" << b.width() <<"x" << b.height() << "]"; return out; }
  friend bool operator==(const Box<T>& a, const Box<T>& b) { return (a.is_empty() && b.is_empty() ) || (memcmp(&a, &b, sizeof(a)) == 0 ); }
};
typedef Box< double > Boxd;
typedef Box< float >  Boxf;
typedef Box< int >    Boxi;





namespace detail {
template< class T > inline void set_min(T& v, T a) { if (a < v) v = a; }
template< class T > inline void set_max(T& v, T a) { if (a > v) v = a; }
}



template< class T >
inline void Box<T>::expand( const Vec3<T>& v ) noexcept
{
  if( is_empty() )
  {
    left()= right() = v.x;
    top()= bottom() = v.y;
    back()= front() = v.z;
  }
  else
  {
    detail::set_min( left(), v.x );
    detail::set_max( right(), v.x );
    detail::set_min( bottom(), v.y );
    detail::set_max( top(), v.y );
    detail::set_min( front(), v.z );
    detail::set_max( back(), v.z );
  }
}




template< class T >
inline void  Box<T>::expand( const Box<T>& b )
{
  if( b.is_empty() )
    return;
  if( is_empty() )
  {
    *this = b;
    return;
  }
  for (auto i = 0; i < (int)Dim::Count; i += 2)
  {
    if (b.m_[i] < m_[i]) m_[i] = b.m_[i];
    if (b.m_[i + 1] > m_[i + 1]) m_[i + 1] = b.m_[i + 1];
  }
}

template< class T >
void  Box<T>::scale_by( T v )
{
  if( !is_empty() )
  {
    for( auto i = 0; i< 3; i++ )
    {
      T l      = div_by2( ( m_[i*2+1] - m_[i*2] ) );
      T c      = div_by2( ( m_[i*2] + m_[i*2+1] ) );
      m_[i*2]   = c - l * v;
      m_[i*2+1] = c + l * v;
    }
  }
}

template< class T >
Box<T>  Box<T>::slice2d( int x, int y, int w, int h ) const
{
  Boxd box;
  if( is_empty() || _is_degenerated() )
    return box;
  T dx = div_it( width() , T(w) );
  T dy = div_it( height(), T(h) );
  
  box.left()   = T( x )   * dx;
  box.right()  = T( x+1 ) * dx; 
  box.bottom() = T( y )   * dy;
  box.top()    = T( y+1 ) * dy; 
  box.front()  = front();
  box.back()   = back();
  box.move_by( bottom_left() );
  return box;
}


template< class T >
bool  Box<T>::collides2d( const Vec2<T>& pt ) const
{
  return !( pt.x < left() || pt.x > right() || pt.y < bottom() || pt.y > top() );
}


template< class T > inline bool _greater_or_equal( T a, T b, T epsi ) noexcept { return a-b +epsi>= T(0) ; } 
template< class T > inline bool _less_or_equal( T a, T b, T epsi ) noexcept    { return b-a +epsi>= T(0) ; } 

//! return true if provided box fits inside this box ( within epsi margin of error)
template< class T >
bool  Box<T>::is_inside2d( const Box<T>& b, T epsi ) const
{
  return _greater_or_equal( b.left(), left(), epsi)       && _less_or_equal( b.right(), right(), epsi ) 
      && _greater_or_equal( b.bottom(), bottom(), epsi )  && _less_or_equal( b.top(), top(), epsi );
}

//! return true if provided box fits inside this box ( within epsi margin of error)
template< class T >
bool  Box<T>::is_inside( const Box<T>& b, T epsi ) const
{
  return _greater_or_equal( b.left(), left(), epsi)       && _less_or_equal( b.right(), right(), epsi ) 
      && _greater_or_equal( b.bottom(), bottom(), epsi )  && _less_or_equal( b.top(), top(), epsi )
      && _greater_or_equal( b.front(), front(), epsi )  && _less_or_equal( b.back(), back(), epsi );
}

//! return true if provided point fits inside this box ( within epsi margin of error)
template< class T >
bool  Box<T>::is_inside( const Vec3<T>& p, T epsi ) const
{
  return _greater_or_equal( p.x, left(), epsi)     && _less_or_equal( p.x, right(), epsi ) 
      && _greater_or_equal( p.y, bottom(), epsi )  && _less_or_equal( p.y, top(), epsi )
      && _greater_or_equal( p.z, front(), epsi )   && _less_or_equal( p.z, back(), epsi );
}


template< class T >
inline std::istream&  operator>>( std::istream& in,  Box<T>& b ) 
{ 
  char dummy; 
  in >> dummy >> b.left() >> dummy;
  in >> b.right() >>dummy;
  in >> b.bottom() >> dummy;
  in >> b.top() >> dummy;
  double w,h;
  in >> w;
  in >> dummy;
  in >> h; 
  in >> dummy;
  return in; 
}



//! empty boxes never collides.
template< class T >
bool Box<T>::collides2d( const Box<T>& b ) const
{
  if( is_empty() || b.is_empty() )
    return false;
  I3S_ASSERT( !_is_degenerated() && !b._is_degenerated() );
  return !(
       b.right() < left() || b.left() > right()   //left-right test
    || b.top() < bottom() || b.bottom() > top()   //up-down test
    //||( b.m_[Back] < m_[Front] || b.m_[Front] > m_[Back]  ) //z test
    );    
}

//! empty boxes never collides.
template< class T >
bool  Box<T>::collides3d(const Box<T>& b) const
{
  if (is_empty() || b.is_empty())
    return false;
  I3S_ASSERT(!_is_degenerated() && !b._is_degenerated());
  return !(b.right() < left() || b.left() > right() || 
           b.top() < bottom() || b.bottom() > top() || 
           b.back() < front() || b.front() > back());
}



template< class Y, class T >  inline void _load_pts( Y& pt, T x, T y, T z )
{
  pt.x = (decltype(pt.x))x;
  pt.y = (decltype(pt.y))y;
  pt.z = (decltype(pt.z))z;
}

template< class T >
template< class Y > void Box<T>::get_corners( Y* pts, int n ) const
{
  I3S_ASSERT( n ==8 );
  _load_pts( pts[0], left(),  bottom(),  front() );
  _load_pts( pts[1], right(), bottom(),  front() );
  _load_pts( pts[2], right(), top(),     front() );
  _load_pts( pts[3], left(),  top(),     front() );
  _load_pts( pts[4], left(),  bottom(),  back() );
  _load_pts( pts[5], right(), bottom(),  back() );
  _load_pts( pts[6], right(), top(),     back() );
  _load_pts( pts[7], left(),  top(),     back() );
}

template< class T >
void  Box<T>::clamp_to(const Box<T>& b)
{
  for (int i = 0; i < 3; ++i)
  {
    m_[i*2]   = std::max(m_[i*2], b.m_[i*2]);
    m_[i*2+1] = std::min(m_[i*2+1], b.m_[i*2+1]);
  }
  if (_is_degenerated())
    set_empty();
}

}

} // namespace i3slib
