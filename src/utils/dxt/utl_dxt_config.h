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
#include <stdint.h>

#ifdef _WIN32
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
#else
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
#endif

// We don't need any kind of export or import for the DXT stuff here.
// DXT compressor code is included directly in the i3slib project and it's only used by i3slib
// implementation privately, nothing of it is visible in i3slib APIs.
#define EXTERN(type) type
