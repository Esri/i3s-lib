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
#include <memory>

#ifndef DECL_PTR
#define DECL_PTR( name )\
  typedef std::shared_ptr< name > Ptr;\
  typedef std::shared_ptr< const name > ConstPtr;\
  typedef std::unique_ptr< name > Var;\
  typedef std::unique_ptr< name > uptr;\
  typedef std::weak_ptr< name > Weak; \
  typedef std::weak_ptr< const name > ConstWeak;\
  typedef std::shared_ptr< name > ptr; \
  typedef std::shared_ptr< const name > cptr; 
// Ignore unused params/vars
template <typename T>
void _unused(T&&)
{
}
#endif

#ifndef DISALLOW_COPY
#define DISALLOW_COPY( name )\
  name( const name& ) = delete;\
  name& operator=( const name& ) = delete;
#endif

