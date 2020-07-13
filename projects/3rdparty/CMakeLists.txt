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

include(ExternalProject)

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING "Choose the type of build." FORCE)
endif()

option(NO_ETC2_SUPPORT "Disable ETC2 support.")

set(GIT_PROGRESS False)
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

# zlib stuff
set(ZLIB_DIR ${THIRD_PARTY_DIR}/zlib)
set(ZLIB_LIB_DIR ${ZLIB_DIR}/lib/x64/${CMAKE_BUILD_TYPE})

ExternalProject_Add(zlib
  PREFIX zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${ZLIB_DIR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DINSTALL_INC_DIR=${ZLIB_DIR}/include -DINSTALL_BIN_DIR=${ZLIB_LIB_DIR} -DINSTALL_LIB_DIR=${ZLIB_LIB_DIR} -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
)

set(LIBJPEG_DIR ${THIRD_PARTY_DIR}/libjpeg)
set(LIBJPEG_LIB_DIR ${LIBJPEG_DIR}/lib/x64/${CMAKE_BUILD_TYPE})

ExternalProject_Add(libjpeg
  PREFIX libjpeg
  GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${LIBJPEG_DIR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DWITH_TURBOJPEG=OFF -DCMAKE_INSTALL_BINDIR=${LIBJPEG_LIB_DIR} -DCMAKE_INSTALL_LIBDIR=${LIBJPEG_LIB_DIR} -DCMAKE_INSTALL_INCLUDEDIR=${LIBJPEG_DIR}/include -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
)

# Using ZLIB_ROOT casuses CMP0074 policy warning, suppress it with -Wno-dev.
ExternalProject_Add(libpng
  PREFIX libpng
  DEPENDS zlib
  GIT_REPOSITORY https://github.com/glennrp/libpng.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${INTERMEDIATE_DIR}/libpng
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DZLIB_ROOT=${ZLIB_LIB_DIR} -DPNG_TESTS=OFF -Wno-dev -DZLIB_INCLUDE_DIR=${ZLIB_DIR}/include -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}}
)

copy_to_3rdparty(libpng TRUE TRUE)

ExternalProject_Add(draco
  PREFIX draco
  GIT_REPOSITORY https://github.com/google/draco.git
  GIT_PROGRESS ${GIT_PROGRESS}
  INSTALL_DIR ${INTERMEDIATE_DIR}/draco
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_SHARED_LIBS=ON -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
)

copy_to_3rdparty(draco FALSE FALSE)

# LEPCC
set(LEPCC_CXX_FLAGS ${CMAKE_CXX_FLAGS} $<$<NOT:$<BOOL:${WIN32}>>:-fPIC>)
  
ExternalProject_Add(lepcc
  PREFIX lepcc
  GIT_REPOSITORY https://github.com/Esri/lepcc.git
  GIT_PROGRESS ${GIT_PROGRESS}
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_CXX_FLAGS=${LEPCC_CXX_FLAGS} -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
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
    CMAKE_ARGS -Wno-dev -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_SHARED_LIBS=ON -DCMAKE_CXX_FLAGS=${ETC2COMP_CXX_FLAGS} -DCMAKE_SHARED_LINKER_FLAGS=${ETC2COMP_LINKER_FLAGS} -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
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

#
project(i3s_3rdparty)
add_custom_target(3rdparty)
add_dependencies(3rdparty zlib libpng libjpeg draco lepcc)

if(NOT NO_ETC2_SUPPORT)
  add_dependencies(3rdparty etc2comp)
endif()
