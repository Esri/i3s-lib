/*
Copyright 2022 Esri

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
#include <string>
#include <memory>

namespace i3slib
{

namespace utl
{

class Slpk_writer;

struct Slpk_writer_factory
{
  virtual Slpk_writer* create_writer(const std::string& url_or_path_utf8) = 0;
  
  virtual ~Slpk_writer_factory() = default;

  I3S_EXPORT static bool register_factory(
    const std::string& url_scheme, 
    std::shared_ptr<Slpk_writer_factory> factory);

  I3S_EXPORT static std::shared_ptr<Slpk_writer_factory> get(const std::string& url_scheme);
};

} // namespace utl

} // namespace i3slib
