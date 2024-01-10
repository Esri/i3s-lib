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
#include <string>

namespace i3slib
{

namespace utl
{

typedef uint32_t Mime_type_set  ;
typedef uint32_t Mime_encoding_set ;

enum class Mime_type : Mime_type_set
{
  Not_set = 1,
  Json = 2,
  Binary= 4,
  Jpeg= 8,
  Png =16,
  Dds_proper=32,
  Dds_legacy = 64,
  Ktx = 128,
  Basis = 256,
  Ktx2 = 512,

  _next_ = 1024, //to make sure we update the static array related to this enum 
  Default = Json | Binary | Not_set,
  Dds_any = Dds_proper | Dds_legacy,
  Any_texture= Jpeg | Png | Dds_any | Ktx | Ktx2 | Binary
};

enum class Mime_encoding : Mime_encoding_set
{
  Not_set = 1,
  Gzip = 2,
  //LEPCC compression should me moved to type. (since web browsers don't understand this encoding)
  Lepcc_xyz = 4,
  Lepcc_rgb = 8,
  Lepcc_int = 16,

  _next_ = 32, //to make sure we update the static array related to this enum  

  Default = Not_set | Gzip,
  Any_pcsl_attributes = Gzip | Lepcc_rgb | Lepcc_int | Not_set,
  Any_geometry = Not_set | Gzip | Lepcc_xyz
};

I3S_EXPORT void add_slpk_extension_to_path(std::string* path, const Mime_type& type, const Mime_encoding& pack);

I3S_EXPORT Mime_type to_known_mime_type(const char* str, int len);

}//endof ::utl

} // namespace i3slib
