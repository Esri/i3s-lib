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

#include "pch.h"

#include "utils/utl_slpk_writer_factory.h"
#include "utils/utl_slpk_writer_api.h"
#include <string>
#include <unordered_map>

namespace i3slib
{

namespace utl
{

namespace
{

using Factory_map = std::unordered_map<std::string, std::shared_ptr<Slpk_writer_factory>>;

Factory_map& get_factory_map()
{
  static Factory_map factory_map;
  return factory_map;
}

// i3slib only supports outputting SLPKs to files, so we register file SLPK
// writer for file:// scheme, and also for empty scheme, which we use to
// designate filesystem paths.
// Other libs using/linking i3slib may register extra writer factories.

struct File_slpk_writer_factory : public Slpk_writer_factory
{
  Slpk_writer* create_writer(const std::string& path_utf8) override
  {
    return create_file_slpk_writer(path_utf8);
  }
};

const auto file_slpk_writer_factory = std::make_shared<File_slpk_writer_factory>();
const auto reg_empty = Slpk_writer_factory::register_factory({},     file_slpk_writer_factory);
const auto reg_file  = Slpk_writer_factory::register_factory("file", file_slpk_writer_factory);

}

/*static*/
bool Slpk_writer_factory::register_factory(
  const std::string& url_scheme,
  std::shared_ptr<Slpk_writer_factory> factory)
{
  return get_factory_map().try_emplace(url_scheme, std::move(factory)).second;
}

/*static*/
std::shared_ptr<Slpk_writer_factory> Slpk_writer_factory::get(const std::string& url_scheme)
{
  const auto& factories = get_factory_map();
  const auto it = factories.find(url_scheme);
  return it == factories.end() ? nullptr : it->second;
}

} // namespace utl

} // namespace i3slib
