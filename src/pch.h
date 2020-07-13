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

#ifdef I3S_PCH

#include <algorithm>
#include <exception>
#include <string>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <numeric>
#include <functional>
#include <iostream>
#include <locale>
#include <codecvt>
#include <sstream>
#include <future>
#include <mutex>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>

// 3rd party libs
// Not adding jpeglib.h here because utl_jpeg.cpp needs to define a symbol _before_
// including jpeglib.h, and I don't like putting that definition here. 
#include "utils/json/json.h"
#include "draco/draco_features.h"
#include "libpng/png.h"
#include "zlib.h"

#endif
