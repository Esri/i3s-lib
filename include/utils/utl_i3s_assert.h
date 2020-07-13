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

#if defined(PROAPP_BUILD) || defined(RTC)
#define USE_EXTERNAL_I3SLIB_ASSERT
#endif

// This header file defines two macros: I3S_ASSERT and I3S_ASSERT_EXT.
// I3S_ASSERT is a usual assertion macro, it vanishes in Release build completely.
// I3S_ASSERT_EXT checks are supposed to be preserved in Release build,
// so it must be used wisely.
//
// There's also a way to inject definitions for these macros from outside
// i3slib: if USE_EXTERNAL_I3SLIB_ASSERT is defined, their definitions
// are expected to be provided through a utl_external_assert.h somewhere
// in include search path.
//
// This is handy if a project using i3slib has its own assertions mechanism
// and it's desirable to route i3slib assertions into the existing infrastructure.
// In this case one can create a utl_external_assert.h implementing I3S_ASSERT
// in terms of existing code, define USE_EXTERNAL_I3SLIB_ASSERT in the build
// settings, and add the path to the header to the i3s-lib build config.
// Note that there header files in i3slib with inline function definitions
// that use I3S_ASSERT() macros, so one should define USE_EXTERNAL_I3SLIB_ASSERT
// not only in i3slib build, but also in the client code build config.

#ifdef USE_EXTERNAL_I3SLIB_ASSERT

#include "utl_external_assert.h"
#ifndef I3S_ASSERT
  #error utl_external_assert.h must provide I3S_ASSERT definition!
#endif
#ifndef I3S_ASSERT_EXT
#error utl_external_assert.h must provide I3S_ASSERT_EXT definition!
#endif

#else

#ifdef _WIN32

// On Win32 we have _ASSERTE which provides a bit more output than a simple assert.
// In release mode we can use __debugbreak on Win32.
#include <crtdbg.h>
#include <intrin.h>
  
#define I3S_ASSERT(exp) _ASSERTE(exp)
#define I3S_ASSERT_RELEASE_IMPL __debugbreak()

#else

#include <cassert>
#include <signal.h>
  
#define I3S_ASSERT(exp) assert(exp)
#define I3S_ASSERT_RELEASE_IMPL ::raise(SIGTRAP)

#endif

#if defined(NDEBUG)
#define I3S_ASSERT_EXT(exp) (void)((!!(exp)) || (I3S_ASSERT_RELEASE_IMPL, 0))
#else
#define I3S_ASSERT_EXT(exp) I3S_ASSERT(exp)
#endif

#endif
