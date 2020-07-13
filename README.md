# i3slib - Indexed 3D SceneLayers library

A C++ library for producing [Scene Layer Package](https://github.com/Esri/i3s-spec) (SLPK) files.
Currently it does not provide reading / validating capabilities, this is planned for the future.
For a test program please see [here](https://devtopia.esri.com/3rdparty/scene-layer-lib/tree/vla96772/i3s_sdk_14/sdk/test/demo).

## Features
-Create SLPK from elevation image(PNG) and color image(PNG)

# Installing dependencies

There is a number of third-party libraries needed for _i3slib_ to build and run:
* [zlib](https://www.zlib.net)
* [libjpeg](http://libjpeg.sourceforge.net) There's no public git repository for the project. Today it seems like instead of the original libjpeg everyone is using [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) - SIMD-enabled fork of libjpeg, API & ABI compatible. This is the version our 3rdparty project (see below) installs.
* [libpng](http://www.libpng.org/pub/png/libpng.html) The project home page has no references to a public git repo, but [this one](https://github.com/glennrp/libpng) seems to be the "official" one.
* [draco](https://github.com/google/draco)
* [etc2comp](https://github.com/google/etc2comp)
* [lepcc](https://github.com/Esri/lepcc)
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)

Due to some historical reasons _jsoncpp_ sources are included directly in the source tree, so you don't need to perform a separate install. (This may change in the future).

The rest of the dependencies need to be present in ````3rdparty```` directory. You can download, build and install them manually, but it is recommended to do this by performing a CMake build on ````projects/3rdparty```` (see instructions below).

NB: the only library from the above list that has an external dependency is _libpng_, and its only dependency is _zlib_.

### On Windows with Visual Studio + CMake

Start Visual Studio, choose _"Open folder"_ command, point to ````projects/3rdparty````. A CMake project will open. Select a configuration to build. Run "Project -> Generate Cache", then "Build -> Build All".

You can build everything with Clang instead of the MSVC compiler, select "x64-Clang-" configurations for that. NB: you'll need to have the __C++ Clang tools for Windows__ component installed.

### On Linux with CMake

```sh
$ mkdir -p build/3rdparty
$ cd build/3rdparty
$ cmake ../../projects/3rdparty
$ cmake --build .
```
The CMake script downloads the source code for all third-party dependencies, builds them and installs into the ````3rdparty```` directory. After the build process completes````3rdparty```` should contain subdirectories ````draco, etc2comp, lepcc, libjpeg, libpng, zlib````.

NB: the current version of the 3rdparty build script relies on ExternalProject_Add() feature, which has some peculiarities. An alternative to consider is using git submodules to checkout the third-party repositories explicitly, in a more controllable manner.

NB: In a typical Linux distribution some of the libraries listed above (zlib, libjpeg, libpng) and their development headers can be easily installed from packages available in the distribution. On most systems zlib, libjpeg and libpng are installed system-wide with the base system setup. Future implementation build options would allow use of existing instances of basic libraries, which would avoid downloading and building them.

# Building the i3s library

### On Windows with Visual Studio

Open the ````sdk/build/i3s.sln```` solution. It contains three projects:
* ````i3s_static```` - builds i3s library as a static library
* ````i3s```` - builds i3s library as a DLL
* ````demo```` - a sample application demonstrating a basic workflow of SLPK generation using the library

Select configuration and build the solution.

### On Windows with Visual Studio + CMake

Start Visual Stusio, choose _"Open folder"_ command, point to the root of the _i3slib_ source tree. A CMake project will open. Select a configuration to build. Run "Project -> Generate Cache", then "Build -> Build All". The build will produce i3s DLL and the demo application executable.

You can build everything with Clang instead of the MSVC compiler, select "x64-Clang-" configurations for that. NB: you'll need to have the __C++ Clang tools for Windows__ component installed.

### On Linux with CMake

```sh
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .
```

By default, the Release configuration is built. You can specify the build configuration (Debug or Release) with CMAKE_BUILD_TYPE option:

```sh
$ cmake . -DCMAKE_BUILD_TYPE="Debug"
$ cmake --build .
```

The library should build correctly with GCC&nbsp;8+ or Clang&nbsp;6+. The code uses C++17 features, so you need an appropriate release of libstdc++ (the one coming with GCC&nbsp;8 is good). To build with particular compiler, do something like this before the build:
```sh
export CXX=/usr/bin/clang++-9
```
or use -DCMAKE_CXX_COMPILER option at configuration step:
```sh
$ cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/g++-8
$ cmake --build .
```

# Known issues / limitations
* Have never built for / tested on 32-bit platforms yet. Please report if you have any issues. We are going to add 32-bit build configuration.

Find a bug or want to request a new feature? Please let us know by submitting an issue.

## Licensing
Copyright 2020 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

A copy of the license is available in the repository's [license.txt]( https://raw.github.com/Esri/quickstart-map-js/master/license.txt) file.
