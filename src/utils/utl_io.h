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
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdint.h>

#ifdef min
#undef min
#endif

namespace i3slib
{

namespace utl
{

template< class Y, class T > inline bool read_it( Y* in, T* val  )        
{ 
  static_assert( !std::is_pointer< T >::value, "Cannot read  pointer type"); 
  in->read( reinterpret_cast< char* >( val ), sizeof( T )  ); 
  return in->gcount() == sizeof(T);
}

template< class Y, class T > inline void write_it( Y* out, const T& val  ) 
{ 
  static_assert( !std::is_pointer< T >::value, "Cannot write pointer type"); 
  out->write( reinterpret_cast< const char* >(&val), sizeof( T )  ); 
}

template< class Y, class T > inline void write_array(Y* out, const T& val)
{
  static_assert(!std::is_pointer< T >::value, "Cannot write pointer type");
  out->write(reinterpret_cast<const char*>(&val[0]), sizeof(val[0])* val.size());
}

template <typename T>
struct is_container {
  static const bool value = false;
};
template <typename T, typename Alloc>
struct is_container<std::vector<T, Alloc> > {
  static const bool value = true;
};


template< class T > void zeros(std::vector<T>* p)
{
  static_assert(!std::is_pointer< T >::value, "Zero-fill pointer type is disallowed. use nullptr assign instead");
  memset(p->data(), 0x00, sizeof(T)*p->size());
}

template< class T > void zeros( T* p )
{
  static_assert(!std::is_pointer< T >::value, "Zero-fill pointer type is disallowed. use nullptr assign instead");
  static_assert(!is_container< T >::value, "Do not use with stl container");
  memset(p, 0x00, sizeof(T));
}



template< class Y  > inline void write_str( Y* out, const std::string& val  ) 
{ 
  write_it( out, (uint32_t)val.size() ); 
  if( val.size() )
    out->write( val.data(), val.size() ); 
}

template< class Y > inline void read_str( Y* in, std::string* val  )        
{ 
  uint32_t size=0;
  read_it( in, &size );
  val->resize( size );
  if( size )
    in->read( val->data(), val->size() );
}


//! stream-to-stream copy using a temp buffer
inline bool  copy_stream(std::istream* src, std::ostream* dest, uint64_t n_bytes)
{
  std::vector< char > buff(1024 * 1024);
  while (n_bytes > 0 && src->good() && dest->good())
  {
    auto n = std::min((uint64_t)buff.size(), n_bytes);
    src->read(buff.data(), n);
    dest->write(buff.data(), n);
    n_bytes -= n;
  }
  return !src->fail() && dest->good() && !dest->fail();
}

} //endof ::utl

} // namespace i3slib
