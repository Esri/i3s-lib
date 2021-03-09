/*
Copyright 2021 Tisham Dhar

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of
the License at http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

For additional information, contact:
Tisham <whatnick> Dhar
email: tisham@whanick.com
*/

#include "i3s/i3s_writer.h"
#include "fastobj/fast_obj.h"
#include <iostream>

#include <filesystem>
namespace stdfs = std::filesystem;

int main(int argc, char* argv[])
{
  if (argc != 5)
  {
    std::cout << "Usage:" << std::endl
      << "obj2slpk <full_res_obj> <lod1_obj> <lod2_obj> <output_slpk_file>" << std::endl;

    return 1;
  }

  const stdfs::path full_res_obj_path(argv[1]);
  const stdfs::path lod1_obj_path(argv[2]);
  const stdfs::path lod2_obj_path(argv[3]);
  const stdfs::path slpk_file_path(argv[4]);

  // Read the source obj files into structs, this is memory intensive
  fastObjMesh* mesh_full = fast_obj_read(full_res_obj_path.c_str());
  fastObjMesh* mesh_lod_1 = fast_obj_read(lod1_obj_path.c_str());
  fastObjMesh* mesh_lod_2 = fast_obj_read(lod2_obj_path.c_str());

}