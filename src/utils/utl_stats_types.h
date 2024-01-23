/*
Copyright 2020-2023 Esri

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

#ifndef NO_UTL_SERIALIZABLE 
#include "utils/utl_serialize.h"
#endif

namespace i3slib
{

namespace utl
{

typedef  int8_t         int8;
typedef  uint8_t        uint8;
typedef  int16_t        int16;
typedef  uint16_t       uint16;
typedef  int32_t        int32;
typedef  uint32_t       uint32;
typedef  uint32_t       uint;
typedef  int64_t        int64;
typedef  uint64_t       uint64;
//typedef  uint8_t        BYTE;

// ---- Serialization structs used to write the stats JSON docs:

template< class T >
struct Value_count_tpl
{
  typedef T     first_type;
  typedef int64 second_type;
  Value_count_tpl() {}
  Value_count_tpl(const T& v, int64 c) : value(v), count(c) {}
  first_type     value{};
  second_type    count{};
  friend bool operator==(const Value_count_tpl& a, const Value_count_tpl& b) { return a.value == b.value && a.count == b.count; }
  friend bool operator>(const Value_count_tpl& a, const Value_count_tpl& b) { return a.count >b.count; } //for descending sorting in utl_stats

#ifndef NO_UTL_SERIALIZABLE 
  SERIALIZABLE(Value_count_tpl);
  template< class Ar > void  serialize(Ar& ar)
  {
    ar & nvp("value", value);
    ar & nvp("count", count);
  }
#endif
};
typedef Value_count_tpl< double > ValueCount;


struct Histo_stats
{
  Histo_stats() : minH(0.0), maxH(0.0) {}

  template< class T, class Y > void    set_stats(const T& a, const T& b, const std::vector<Y>& bins) 
  { 
    minH = (double)a; 
    maxH = (double)b; 
    counts.resize(bins.size());
    for (int i = 0; i < bins.size(); ++i)
      counts[i] = (int64)bins[i];
  }


  double minH, maxH;
  std::vector< uint64> counts;
  friend bool operator==(const Histo_stats& a, const Histo_stats& b) { return a.minH == b.minH && a.maxH == b.maxH && a.counts == b.counts; }
#ifndef NO_UTL_SERIALIZABLE 
  SERIALIZABLE(Histo_stats);
  template< class Ar > void  serialize(Ar& ar)
  {
    ar & nvp("minimum", minH);
    ar & nvp("maximum", maxH);
    ar & nvp("counts", seq(counts));
  }
#endif

};

struct Atrb_stats
{
  Atrb_stats() : minH(0.0), maxH(0.0), avg(0.0), stddev(0.0), count(0.0), sum(0.0), var(0.0) {}

  template< class T > void    set_min_max(const T& a, const T& b) { minH = (double)a; maxH = (double)b; }
  void    set_stddev(double sum_of_square, double sum1, double ct) 
  { 
    sum = sum1;
    count = ct;
    avg = count > 0.0 ? sum / count : 0.0;
    stddev = ct >1 ? sqrt((count* sum_of_square - sum*sum) / (ct*(ct - 1))) : 0.0;
    var = stddev*stddev;
  }

  double minH, maxH, avg, stddev, count, sum, var;
  Histo_stats histo;
  std::vector< ValueCount > mostFrequent;

  friend bool operator==(const Atrb_stats& a, const Atrb_stats& b) 
  {
    return a.minH == b.minH && a.maxH == b.maxH && a.count == b.count && a.sum == b.sum && a.stddev == b.stddev && a.histo == b.histo;
  }

#ifndef NO_UTL_SERIALIZABLE 
  SERIALIZABLE(Atrb_stats);
  template< class Ar > void  serialize(Ar& ar)
  {
    //auto kSkip = OptionalFieldMode::SkipOnWriteIfDefault;
    ar & nvp("min", minH);
    ar & nvp("max", maxH);
    ar & opt("avg", avg, 0.0);
    ar & opt("stddev", stddev, 0.0);
    ar & nvp("count", count);
    ar & opt("sum", sum, 0.0);
    ar & opt("variance", var, 0.0);
    ar & opt("histogram", histo, Histo_stats());
    ar & opt("mostFrequentValues", seq(mostFrequent));
  }
#endif
};

template< class String_t >
struct Atrb_stats_string
{
  Atrb_stats_string() {}

  uint64 totalValuesCount{ 0 };
  std::vector< Value_count_tpl< String_t > > mostFrequent;

  friend bool operator==(const Atrb_stats_string& a, const Atrb_stats_string& b)
  {
    return a.totalValuesCount == b.totalValuesCount;
      //&& a.mostFrequent == b.mostFrequent;
  }

#ifndef NO_UTL_SERIALIZABLE 
  SERIALIZABLE(Atrb_stats_string);
  template< class Ar > void  serialize(Ar& ar)
  {
    //auto kSkip = OptionalFieldMode::SkipOnWriteIfDefault;
    ar & nvp("totalValuesCount", totalValuesCount);
    ar & opt("mostFrequentValues", seq(mostFrequent) );
  }
#endif
};


template< class String_t >
struct Atrb_stats_datetime
{
  Atrb_stats_datetime() = default;

  String_t min_time;
  String_t max_time;
  uint64 totalValuesCount{ 0 };
  std::vector< Value_count_tpl< String_t > > mostFrequent;

  friend bool operator==(const Atrb_stats_datetime& a, const Atrb_stats_datetime& b)
  {
    return a.min_time == b.min_time && a.max_time == b.max_time
      && a.totalValuesCount == b.totalValuesCount && a.mostFrequent == b.mostFrequent;
  }

  SERIALIZABLE(Atrb_stats_datetime);
  template< class Ar > void  serialize(Ar& ar)
  {
    ar& opt("minTimeStr", min_time, String_t());
    ar& opt("maxTimeStr", max_time, String_t());
    ar& nvp("totalValuesCount", totalValuesCount);
    ar& opt("mostFrequentValues", seq(mostFrequent));
  }
};

template <class T>
struct Attribute_stats_desc
{
  // --- fields:
  T stats;
  // --- 
  SERIALIZABLE(Attribute_stats_desc);
  template< class Ar >  void serialize(Ar& ar)
  {
    ar & nvp("stats", stats);
  }

};

} //endof ::utl
} // namespace i3slib

