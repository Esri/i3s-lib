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
#include "utils/utl_serialize_json_dom.h"

namespace i3slib
{

namespace utl
{

template< class T > inline bool from_json_safe(
  const std::string& str, T* obj, utl::Basic_tracker* trk, 
  const std::string& ref_document_for_error_reporting, int version = 0)
{
  auto in = Json_input::create(str, trk, ref_document_for_error_reporting, version);
  return in && in->read(*obj, trk, ref_document_for_error_reporting);
}

} // namespace i3s

} // namespace i3slib
