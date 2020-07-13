# Copyright 2020 Esri
# 
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use
# this file except in compliance with the License. You may obtain a copy of
# the License at http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
# 
# For additional information, contact:
# Environmental Systems Research Institute, Inc.
# Attn: Contracts Dept
# 380 New York Street
# Redlands, California, USA 92373
# email: contracts@esri.com

cmake_minimum_required(VERSION 3.10)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Please give us -std=c++17, not -std=gnu++17 or something

option(NO_ETC2_SUPPORT "Disable ETC2 support.")

project(i3s)

# Uncomment to see compilation commands with Linux builds
#set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Verbose build output" FORCE)

set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/3rdparty) 

set(I3SLIB_SOURCES
  src/i3s/i3s_context_impl.cpp
  src/i3s/i3s_enums_generated.cpp
  src/i3s/i3s_layer_dom.cpp
  src/i3s/i3s_legacy_mesh.cpp
  src/i3s/i3s_writer_impl.cpp
  src/utils/dxt/IntelDXTCompressor.cpp
  src/utils/json/jsoncpp.cpp
  src/utils/utl_base64.cpp
  src/utils/utl_buffer.cpp
  src/utils/utl_bvh.cpp
  src/utils/utl_colors.cpp
  src/utils/dxt/utl_dxt_mipmap_dds.cpp
  src/utils/utl_fs.cpp
  src/utils/utl_gzip.cpp
  src/utils/utl_image_2d.cpp
  src/utils/utl_image_resize.cpp
  src/utils/utl_jpeg.cpp
  src/utils/utl_libdraco_api.cpp
  src/utils/utl_md5.cpp
  src/utils/utl_mime.cpp
  src/utils/utl_obb.cpp
  src/utils/utl_png.cpp
  src/utils/utl_quaternion.cpp
  src/utils/utl_resource_strings.cpp
  src/utils/utl_serialize_json.cpp
  src/utils/utl_serialize_json_dom.cpp
  src/utils/utl_slpk_writer_impl.cpp
  src/utils/utl_string.cpp
  src/utils/utl_variant.cpp
  src/utils/utl_zip_archive_impl.cpp)

add_library(i3s SHARED ${I3SLIB_SOURCES})

# If we don't do this, CMake would define i3s_EXPORTS symbol by default. 
set_target_properties(i3s PROPERTIES DEFINE_SYMBOL "I3S_LIB")

if(WIN32)
  target_compile_definitions(i3s PRIVATE -D_UNICODE -DUNICODE)
else()
  target_compile_options(i3s PRIVATE -fvisibility=hidden -Wall)

  # Suppress complaints on MSVS-specific pragmas.
  target_compile_options(i3s PRIVATE -Wno-unknown-pragmas)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Disable this really useless but annoying Clang-specific parentheses warning.
  target_compile_options(i3s PRIVATE -Wno-logical-op-parentheses)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # GCC can warn about usage of undefined macros.
  target_compile_options(i3s PRIVATE -Wundef)
  # GCC also has this weird parentheses warning.
  target_compile_options(i3s PRIVATE -Wno-parentheses)
  # FIXME: we have too much of those, fix'em all and remove this
  target_compile_options(i3s PRIVATE -Wno-sign-compare)
endif()

#target_compile_definitions(i3s PRIVATE -DI3S_EXPORT_API $<$<CONFIG:Release>:-DNDEBUG>)

target_include_directories(i3s PUBLIC include) 

target_include_directories(i3s PRIVATE
  src
  ${THIRD_PARTY_DIR}/zlib/include
  ${THIRD_PARTY_DIR}/libpng/include
  ${THIRD_PARTY_DIR}/libjpeg/include
  ${THIRD_PARTY_DIR}/draco/include
  ${THIRD_PARTY_DIR}/lepcc/include)

if(NOT WIN32)
  target_link_libraries(i3s stdc++fs)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

if(WIN32)
  add_library(zlib UNKNOWN IMPORTED)
  set_target_properties(zlib PROPERTIES IMPORTED_LOCATION_DEBUG ${THIRD_PARTY_DIR}/zlib/lib/x64/${CMAKE_BUILD_TYPE}/zlibd.lib)
  set_target_properties(zlib PROPERTIES IMPORTED_LOCATION_RELEASE ${THIRD_PARTY_DIR}/zlib/lib/x64/${CMAKE_BUILD_TYPE}/zlib.lib)
  
  add_library(libpng UNKNOWN IMPORTED)
  set_target_properties(libpng PROPERTIES IMPORTED_LOCATION_DEBUG ${THIRD_PARTY_DIR}/libpng/lib/x64/${CMAKE_BUILD_TYPE}/libpng16d.lib)
  set_target_properties(libpng PROPERTIES IMPORTED_LOCATION_RELEASE ${THIRD_PARTY_DIR}/libpng/lib/x64/${CMAKE_BUILD_TYPE}/libpng16.lib)
  
  add_library(libjpeg UNKNOWN IMPORTED)
  set_target_properties(libjpeg PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/libjpeg/lib/x64/${CMAKE_BUILD_TYPE}/jpeg.lib)
  
  add_library(draco UNKNOWN IMPORTED)
  set_target_properties(draco PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/draco/lib/x64/${CMAKE_BUILD_TYPE}/draco.lib)

  add_library(lepcc UNKNOWN IMPORTED)
  set_target_properties(lepcc PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/lepcc/lib/x64/${CMAKE_BUILD_TYPE}/lepcc.lib)
  
  if(NOT NO_ETC2_SUPPORT)
    add_library(EtcLib UNKNOWN IMPORTED)
    set_target_properties(EtcLib PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/etc2comp/lib/x64/${CMAKE_BUILD_TYPE}/EtcLib.lib)
  endif()
else()
  add_library(zlib SHARED IMPORTED)
  set_target_properties(zlib PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/zlib/lib/x64/${CMAKE_BUILD_TYPE}/libz.so)
  
  add_library(libpng SHARED IMPORTED)
  set_target_properties(libpng PROPERTIES IMPORTED_LOCATION_DEBUG ${THIRD_PARTY_DIR}/libpng/lib/x64/${CMAKE_BUILD_TYPE}/libpng16d.so)
  set_target_properties(libpng PROPERTIES IMPORTED_LOCATION_RELEASE ${THIRD_PARTY_DIR}/libpng/lib/x64/${CMAKE_BUILD_TYPE}/libpng16.so)
  
  add_library(libjpeg SHARED IMPORTED)
  set_target_properties(libjpeg PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/libjpeg/lib/x64/${CMAKE_BUILD_TYPE}/libjpeg.so)
  
  add_library(draco SHARED IMPORTED)
  set_target_properties(draco PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/draco/lib/x64/${CMAKE_BUILD_TYPE}/libdraco.so)

  add_library(lepcc STATIC IMPORTED)
  set_target_properties(lepcc PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/lepcc/lib/x64/${CMAKE_BUILD_TYPE}/liblepcc.a)
  
  if(NOT NO_ETC2_SUPPORT)
	  add_library(EtcLib SHARED IMPORTED)
	  set_target_properties(EtcLib PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/etc2comp/lib/x64/${CMAKE_BUILD_TYPE}/libEtcLib.so)
  endif()
endif()

target_link_libraries(i3s zlib libpng libjpeg draco lepcc)
if(NO_ETC2_SUPPORT)
  target_compile_definitions(i3s PRIVATE -DNO_ETC2_SUPPORT)
else()
  target_link_libraries(i3s EtcLib)
endif()

# raster2slpk example app target
set(RASTER2SLPK_SOURCES "examples/raster2slpk/main.cpp")
add_executable(raster2slpk ${RASTER2SLPK_SOURCES})

if(WIN32)
  target_compile_definitions(raster2slpk PRIVATE -D_UNICODE -DUNICODE)
endif()

target_include_directories(raster2slpk PRIVATE include) 

target_link_libraries(raster2slpk i3s)
