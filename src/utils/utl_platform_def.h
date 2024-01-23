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

#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define PLATFORM_EXPORT __declspec(dllexport)
#define PLATFORM_IMPORT __declspec(dllimport)
#define SELECTANY __declspec(selectany)
#else
#define PLATFORM_EXPORT __attribute__ ((visibility("default")))
#define PLATFORM_IMPORT __attribute__ ((visibility("default")))
#define SELECTANY __attribute__((weak))
#endif

#ifndef _MSC_VER

#define __forceinline __attribute__((always_inline)) inline

// localtime_s with reversed parameter order is MSVC-specific.
// Emulate it with localtime_r on other compilers.
// TODO: replace its usages with std::chrono ? 
inline tm* localtime_s(tm* tm, const time_t* time)
{
  return ::localtime_r(time, tm);
}

#endif

#ifndef _WIN32

// C11/C++17 have aligned_alloc, but it's not available on Windows.  
// https://docs.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance?view=vs-2019
// Instead they force us to use their own _aligned_malloc / _aligned_free.
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc?view=vs-2019
// (Note the reversed order of parameters compared to the standard!)
// Let's accept this API and emulate it on other platforms, as a temp workaround.

#ifdef __APPLE__
#include "Availability.h"
#ifdef __MAC_OS_X_VERSION_MIN_REQUIRED
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_15
#define _USE_POSIX_MEMALIGN_
#endif
#elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
#define _USE_POSIX_MEMALIGN_
#endif
#endif
#elif __ANDROID__
#define _USE_POSIX_MEMALIGN_
#endif

inline void* _aligned_malloc(size_t size, size_t alignment)
{
#ifdef _USE_POSIX_MEMALIGN_
  void* mem_ptr = nullptr;
  if (posix_memalign(&mem_ptr, alignment, size) == 0)
    return mem_ptr;
  return nullptr;
#else
  return ::aligned_alloc(alignment, size);
#endif
}

inline void _aligned_free(void* memblock)
{
  ::free(memblock);
}

#endif
