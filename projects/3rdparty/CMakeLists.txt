# Copyright 2020-2023 Esri
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

include(ExternalProject)

#set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Verbose build output" FORCE)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING "Choose the type of build." FORCE)
endif()

option(NO_ETC2_SUPPORT "Disable ETC2 support.")

option(GIT_PROGRESS OFF)
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/../../3rdparty) 
set(INTERMEDIATE_DIR ${CMAKE_BINARY_DIR}/install) 

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

function(copy_to_3rdparty LIB_NAME FIX_HEADERS_DIR HAS_BIN_DIR)
  set(SRC_BASE_DIR ${INTERMEDIATE_DIR}/${LIB_NAME})
  set(DST_BASE_DIR ${THIRD_PARTY_DIR}/${LIB_NAME})
  set(DST_LIB_DIR ${DST_BASE_DIR}/lib/x64/${CMAKE_BUILD_TYPE})

  if(${FIX_HEADERS_DIR})
    set(HEADERS_DIR ${DST_BASE_DIR}/include/${LIB_NAME})
  else()
    set(HEADERS_DIR ${DST_BASE_DIR}/include)
  endif()

  set(SRC_LIB_DIRS ${SRC_BASE_DIR}/lib)
  if(${HAS_BIN_DIR})
    set(SRC_LIB_DIRS ${SRC_LIB_DIRS} ${SRC_BASE_DIR}/bin)
  endif()

  ExternalProject_Add_Step(${LIB_NAME} copy_to_3rdparty
    DEPENDEES install
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${DST_BASE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${DST_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HEADERS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SRC_BASE_DIR}/include ${HEADERS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SRC_LIB_DIRS} ${DST_LIB_DIR}
  )
endfunction()

# We need to have project() before using CMAKE_CXX_COMPILER_ID.
project(i3s_3rdparty)

# Clang on Windows relies on the value of CFLAGS/CXXFLAGS environment variables to determine
# the desired target archictecture. If CXXFLAGS specifies "-m64", it builds x64, otherwise it
# builds x86.
# When running Clang from Visual Studio, the IDE sets those environment vars depending on the
# active build configuration. When running cmake from command line, it's up to you to setup
# the environment before running CMake.
# However this environment variables are not passed down by CMake to the CMake runs of
# external projects, and this becomes a problem when you run CMake + Clang from Visual Studio
# IDE. The simplest workaround is to pass them by hand, which is a bit tricky.

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(EXT_PROJ_CMAKE_COMMAND ${CMAKE_COMMAND} -E env CFLAGS=$ENV{CFLAGS} CXXFLAGS=$ENV{CXXFLAGS} ${CMAKE_COMMAND})
else()
  set(EXT_PROJ_CMAKE_COMMAND ${CMAKE_COMMAND})
endif()

# We need to pass down some arguments to each of the external projects.
set(EXT_PROJ_COMMON_ARGS
  -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}) 

# zlib stuff
set(ZLIB_DIR ${THIRD_PARTY_DIR}/zlib)
set(ZLIB_LIB_DIR ${ZLIB_DIR}/lib/x64/${CMAKE_BUILD_TYPE})

ExternalProject_Add(zlib
  PREFIX zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${ZLIB_DIR}
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${EXT_PROJ_COMMON_ARGS} -DINSTALL_INC_DIR=${ZLIB_DIR}/include -DINSTALL_BIN_DIR=${ZLIB_LIB_DIR} -DINSTALL_LIB_DIR=${ZLIB_LIB_DIR} 
)

# libjpeg
set(LIBJPEG_DIR ${THIRD_PARTY_DIR}/libjpeg)
set(LIBJPEG_LIB_DIR ${LIBJPEG_DIR}/lib/x64/${CMAKE_BUILD_TYPE})

ExternalProject_Add(libjpeg
  PREFIX libjpeg
  GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
  GIT_TAG main
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${LIBJPEG_DIR}
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${EXT_PROJ_COMMON_ARGS} -DWITH_TURBOJPEG=OFF -DWITH_CRT_DLL=ON -DCMAKE_INSTALL_BINDIR=${LIBJPEG_LIB_DIR} -DCMAKE_INSTALL_LIBDIR=${LIBJPEG_LIB_DIR} -DCMAKE_INSTALL_INCLUDEDIR=${LIBJPEG_DIR}/include
)

# Using ZLIB_ROOT casuses CMP0074 policy warning, suppress it with -Wno-dev.
ExternalProject_Add(libpng
  PREFIX libpng
  DEPENDS zlib
  GIT_REPOSITORY https://github.com/glennrp/libpng.git
  GIT_TAG v1.6.40
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${INTERMEDIATE_DIR}/libpng
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${EXT_PROJ_COMMON_ARGS} -DZLIB_ROOT=${ZLIB_LIB_DIR} -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF -Wno-dev -DZLIB_INCLUDE_DIR=${ZLIB_DIR}/include
)

copy_to_3rdparty(libpng TRUE TRUE)

# rapidjson
set(RJSON_DIR ${THIRD_PARTY_DIR}/rapidjson)
set(RJSON_LIB_DIR ${RJSON_DIR}/lib/x64/${CMAKE_BUILD_TYPE})
set(RJSON_FLAGS -DRAPIDJSON_BUILD_TESTS=OFF -DRAPIDJSON_BUILD_DOC=OFF -DRAPIDJSON_BUILD_EXAMPLES=OFF)

ExternalProject_Add(rapidjson
  PREFIX rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${INTERMEDIATE_DIR}/rapidjson
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${EXT_PROJ_COMMON_ARGS} ${RJSON_FLAGS} -DLIB_INSTALL_DIR=${RJSON_LIB_DIR} -DINCLUDE_INSTALL_DIR=${RJSON_DIR}/include
)

# draco
ExternalProject_Add(draco
  PREFIX draco
  GIT_REPOSITORY https://github.com/google/draco.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${INTERMEDIATE_DIR}/draco
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${EXT_PROJ_COMMON_ARGS} -DBUILD_SHARED_LIBS=ON
)

copy_to_3rdparty(draco FALSE TRUE)

# LEPCC
set(LEPCC_CXX_FLAGS ${CMAKE_CXX_FLAGS} $<$<NOT:$<BOOL:${WIN32}>>:-fPIC>)
  
ExternalProject_Add(lepcc
  PREFIX lepcc
  GIT_REPOSITORY https://github.com/Esri/lepcc.git
  GIT_PROGRESS ${GIT_PROGRESS}
  CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
  CMAKE_ARGS ${EXT_PROJ_COMMON_ARGS} -DCMAKE_CXX_FLAGS=${LEPCC_CXX_FLAGS}
  INSTALL_COMMAND ""
)

ExternalProject_Get_property(lepcc SOURCE_DIR BINARY_DIR)

if(WIN32)
  set(LEPCC_ARTIFACTS ${BINARY_DIR}/lepcc.lib)
else()
  set(LEPCC_ARTIFACTS ${BINARY_DIR}/liblepcc.a)
endif()

set(LEPCC_INSTALL_INC_DIR ${THIRD_PARTY_DIR}/lepcc/include)
set(LEPCC_INSTALL_LIB_DIR ${THIRD_PARTY_DIR}/lepcc/lib/x64/${CMAKE_BUILD_TYPE})

ExternalProject_Add_Step(lepcc do_install
  DEPENDEES build
  COMMAND ${CMAKE_COMMAND} -E make_directory ${LEPCC_INSTALL_INC_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_DIR}/src/include ${LEPCC_INSTALL_INC_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${LEPCC_INSTALL_LIB_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${LEPCC_ARTIFACTS} ${LEPCC_INSTALL_LIB_DIR}
)

if(NOT NO_ETC2_SUPPORT)
  # etc2comp is a bit problematic.
  # First, EtcLib is designed to be used as a static library: it does not export any symbols,
  # and its CMake script does not have an option to build the library as SHARED.
  # We deal with this by injecting source files with some exported wrapper API,
  # and forcing CMake to build a shared lib with BUILD_SHARED_LIBS=ON.
  # Second, etc2comp does not perform install() in its CMakeLists.txt,
  # so we copy out the artifacts with a custom post-build step.
  # Third, its CMakeLists.txt does not have cmake_minimum_required(), which causes a CMP0000
  # policy warning from CMake. Suppressed it with -Wno-dev.
  
  set(ETC2COMP_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DETC2COMP_LIB")
  
  if(NOT WIN32)
    set(ETC2COMP_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fvisibility=hidden")
    set(ETC2COMP_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pthread")
  endif()
  
  ExternalProject_Add(etc2comp
    PREFIX etc2comp
    GIT_REPOSITORY https://github.com/google/etc2comp.git
    GIT_PROGRESS ${GIT_PROGRESS}
    SOURCE_SUBDIR EtcLib
    CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
    CMAKE_ARGS ${EXT_PROJ_COMMON_ARGS} -Wno-dev -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_FLAGS=${ETC2COMP_CXX_FLAGS} -DCMAKE_SHARED_LINKER_FLAGS=${ETC2COMP_LINKER_FLAGS}
    INSTALL_COMMAND ""
  )
  
  ExternalProject_Get_property(etc2comp SOURCE_DIR BINARY_DIR)
  
  ExternalProject_Add_Step(etc2comp inject_c_api
    DEPENDEES update
    DEPENDERS configure
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/etc2comp_c_api.h ${CMAKE_SOURCE_DIR}/etc2comp_c_api.cpp ${SOURCE_DIR}/EtcLib/Etc
  )

  if(WIN32)
    set(ETC2COMP_ARTIFACTS ${BINARY_DIR}/EtcLib.dll ${BINARY_DIR}/EtcLib.lib)
  else()
    set(ETC2COMP_ARTIFACTS ${BINARY_DIR}/libEtcLib.so)
  endif()
 
  set(ETC2COMP_INSTALL_DIR ${THIRD_PARTY_DIR}/etc2comp/lib/x64/${CMAKE_BUILD_TYPE})
 
  ExternalProject_Add_Step(etc2comp do_install
 	DEPENDEES build
 	COMMAND ${CMAKE_COMMAND} -E make_directory ${ETC2COMP_INSTALL_DIR}
 	COMMAND ${CMAKE_COMMAND} -E copy ${ETC2COMP_ARTIFACTS} ${ETC2COMP_INSTALL_DIR}
  )
endif()

if(NOT NO_BASIS_ENCODER_SUPPORT OR NOT NO_BASIS_TRANSCODER_SUPPORT)
  # Basis Universal.
  # The build script in the basis_universal codebase is very minimalistic: it just
  # builds the converter executable and copies it to ${CMAKE_INSTALL_PREFIX}/bin.
  # It does not provide means to build the code as a shared or static library.
  # Neither it's possible to install headers or binaries to a specific location.
  # On top of that, it uses some global variables in its public API, and they are
  # not explicitly marked for export, which makes impossible to use the code as
  # a shared lib.
  # To work around the above, we hack the original basis_universal build scripts,
  # injecting some extra build config (see basisu_CMakeLists.txt) that builds
  # basis_univeral code as a reusable static library and places the lib and
  # the headers under 3rdparty/basisu.
  
  set(BASISU_DIR ${THIRD_PARTY_DIR}/basisu)
  set(BASISU_LIB_DIR ${BASISU_DIR}/lib/x64/${CMAKE_BUILD_TYPE})
  
  set(BASISU_CMAKE_ARGS
    ${EXT_PROJ_COMMON_ARGS} 
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_INSTALL_BINDIR=${BASISU_LIB_DIR}
    -DCMAKE_INSTALL_LIBDIR=${BASISU_LIB_DIR}
    -DCMAKE_INSTALL_INCLUDEDIR=${BASISU_DIR}/include)
  
  ExternalProject_Add(basisu
    PREFIX basisu
    GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
    GIT_TAG 1.16.4
    GIT_PROGRESS ${GIT_PROGRESS}
    INSTALL_DIR ${INTERMEDIATE_DIR}/basisu
    CMAKE_COMMAND ${EXT_PROJ_CMAKE_COMMAND}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> ${BASISU_CMAKE_ARGS}
  )
  
  ExternalProject_Get_property(basisu SOURCE_DIR)

  set(BASISU_BACKUP_DIR ${SOURCE_DIR}/backup)

  # This custom pre-build step essentially performs the following: it copies
  # the original CMakeLists.txt to the backup subfolder and replaces it with
  # our own CMakeLists.txt that includes the original one and applies some
  # additional build commmands on top of it (see basisu_CMakeLists.txt).
  # However, we need this to be idempotent: when applied mutiple times,
  # the result should be same even if the source tree has not been reset
  # to its pristine state beforehand (i.e. the second run must not overwrite
  # previously stashed original CMakeLists.txt with the injected one).
  ExternalProject_Add_Step(basisu inject_cmake_lists_txt
    DEPENDEES update
    DEPENDERS configure
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BASISU_BACKUP_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BASISU_BACKUP_DIR} ${SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_DIR}/CMakeLists.txt ${BASISU_BACKUP_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/basisu_CMakeLists.txt ${SOURCE_DIR}/CMakeLists.txt
  )

endif()

#
add_custom_target(3rdparty)
add_dependencies(3rdparty zlib libpng libjpeg draco lepcc rapidjson)

if(NOT NO_ETC2_SUPPORT)
  add_dependencies(3rdparty etc2comp)
endif()

if(NOT NO_BASIS_ENCODER_SUPPORT OR NOT NO_BASIS_TRANSCODER_SUPPORT)
  add_dependencies(3rdparty basisu)
endif()
