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

#include "utils/utl_serialize.h"

namespace i3slib
{

namespace geo
{

struct SR_def
{
  SERIALIZABLE(SR_def);
  static const int c_wkid_not_set = -1;
  int             wkid = c_wkid_not_set;
  int             latest_wkid = c_wkid_not_set;
  int             vcs_wkid = c_wkid_not_set;
  int             vcs_latest_wkid = c_wkid_not_set;
  std::string     wkt;
  std::string     to_string() const { return wkid > 0 ? std::to_string(wkid) : wkt; }
  bool            is_null() const { return wkid < 0 && latest_wkid < 0 && wkt.empty(); }
  friend bool operator==(const SR_def& a, const SR_def& b) 
  { 
    //could be smarter, but good enough for "serialize" purpose:
    return (a.is_null() && b.is_null()  )
    || (a.wkid == b.wkid && a.latest_wkid == b.latest_wkid && a.vcs_wkid == b.vcs_wkid && a.vcs_latest_wkid == b.vcs_latest_wkid );  
  }
  template< class Ar > void serialize(Ar& ar)
  {
    ar & utl::opt("wkid", wkid, -1);
    ar & utl::opt("latestWkid", latest_wkid, -1);
    ar & utl::opt("vcsWkid", vcs_wkid, -1);
    ar & utl::opt("latestVcsWkid", vcs_latest_wkid, -1);
    ar & utl::opt("wkt", wkt, std::string());
  }
};

}//endof geo

} // namespace i3slib
