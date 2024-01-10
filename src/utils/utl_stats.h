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

#include <vector>
#include <unordered_map>
#include <limits>
#include <functional> 
#include "utils/utl_stats_types.h"
#include "utils/utl_i3s_assert.h"
#include <numeric>
#include <type_traits>
#include <cmath>
#ifdef max
#undef max
#endif

namespace i3slib
{

namespace utl
{
  // helper function:
template< class Count_t  >
double calc_percentile(const std::vector< Count_t >& histo, double percentile);

//--------------------------------------------------------------------------------
//      struct Range<>
//--------------------------------------------------------------------------------

#if 0 //obsolete. use Range_std_dev instead
//! Compute the min/max/count
template< class T >
struct Range
{
  SERIALIZABLE(Range);
  Range() :count(0), minV(std::numeric_limits< T >::max()), maxV(std::numeric_limits< T >::lowest()), sum(0.0){}
  inline void      Expand(T v)                noexcept { if (v < minV) minV = v; if (v > maxV) maxV = v; count++; sum += (double)v; };
  inline void      Expand(const Range<T>& r)  noexcept{ if (r.minV < minV) minV = r.minV; if (r.maxV > maxV) maxV = r.maxV; count += r.count; };
  inline bool      IsEmpty() const            noexcept{ return !count || (minV==maxV && minV ==(T)0 ); }
  void             get_stats(Atrb_stats* stats) const { stats->set_min_max(minV, maxV); stats->set_stddev(m_sum2, m_sum, count); }

  template< class Ar> void serialize(Ar& ar)
  {
    ar & nvp("min", minV);
    ar & nvp("max", maxV);
  }

  T minV, maxV;
  uint64  count;
  double  sum;
};
#endif


//--------------------------------------------------------------------------------
//      struct Range_std_dev<>
//--------------------------------------------------------------------------------
//! Compute min/max/count/sum/sumOfSquared  ( to compute std deviation.)
template< class T >
struct Range_std_dev
{
  SERIALIZABLE(Range_std_dev);
  Range_std_dev() :count(0), m_sum(0.0), m_sum2(0.0), minV(std::numeric_limits< T >::max()), maxV(std::numeric_limits< T >::lowest()){}
  inline void      add_value(T v)                noexcept{ if (v < minV) minV = v; if (v> maxV) maxV = v; count++; m_sum += (double)v; m_sum2 += (double)v * (double)v; };
  inline void      merge(const Range_std_dev<T>& r)  noexcept { if (r.minV < minV) minV = r.minV; if (r.maxV > maxV) maxV = r.maxV; count += r.count; m_sum2 += r.m_sum2; m_sum += r.m_sum; };
  inline bool      is_empty() const            noexcept { return !count; }
  inline bool      is_all_zeros() const         noexcept { return count == 0 || (minV ==(T)0 && maxV == minV); }
  //! scale the stats by a constant. (usefull for unit conversion of underlying attribute: i.e. "elevation" )
  inline void      scale_by(double s)                   { minV = T((double)minV*s); maxV = T((double)maxV*s); m_sum *= s; m_sum2 *= s*s; };
  inline void      get_min_max(double* lo, double* hi) const{ *lo = (double)minV; *hi = (double)maxV; }
  inline void      set_minmax(double lo, double hi) { minV = lo; maxV = hi; }
  inline void      calc_mean(double* mean, double* sigma) const
  {
    if (count == 0)
    {
      *mean = 0;
      *sigma = 0;
    }
    else
    {
      const auto n = static_cast<double>(count);
      *mean = m_sum / n;
      const double tmp = (count > 1) ? (n * m_sum2 - m_sum * m_sum) / (n * (n - 1)) : 0.0;
      // due to numerical errors, tmp could be negative.
      *sigma = (tmp >= 0.) ? std::sqrt(tmp) : 0.0;
    }
  }
  void    get_stats(Atrb_stats* stats) const { stats->set_min_max<T>(minV, maxV); stats->set_stddev(m_sum2, m_sum, (double)count); }
  template< class Ar> void serialize(Ar& ar)
  {
    ar & nvp("min", minV);
    ar & nvp("max", maxV);
    ar & nvp("sum2", m_sum2);
    ar & nvp("sum", m_sum);
    ar & nvp("count", count);
  }
  //public:
  T minV, maxV;
  uint64  count;
  double  m_sum, m_sum2; //sum and sum of sqr for standard deviation estimation.
};


//--------------------------------------------------------------------------------
//      struct Histo_unsigned<>
//--------------------------------------------------------------------------------
//! Histogram optimized for **unsigned integral** types: 
//! "BitOut" defines the actual dynamic range of the histogam: i.e Histo_uint< ushort, 10 > has 1024 bins (bin_width = 64 )
//! "Count_t" internal type used to store each bin count. 
template< class T, uint BitOut, class Count_t=uint64 >
class Histo_unsigned
{
public:
  using value_type = T;

  explicit Histo_unsigned() : m_histo(1 << BitOut), m_bitShift(sizeof(T) * 8 - BitOut)
  { 
    memset(&m_histo[0], 0, sizeof(Count_t) * m_histo.size()); 
    static_assert(std::is_unsigned<T>::value, "Unsigned type only");
  }
  void    add_value(T val)   noexcept { auto k = val >> c_shift; ++m_histo[k]; }
  //void    Merge(const Histo& h);
  void    resample( std::vector< Count_t >* histo, double* minV, double* maxV, int number_of_bin_hint=-1) const;
  T       get_min_value() const            { return 0; }
  double  get_bin_size() const             { return (double)( (std::numeric_limits< T >::max() >> m_bitShift)) / (double)(m_histo.size() - 1); }
  bool    is_empty() const             { for (auto x : m_histo) if (x) return false; return true; }
  bool    is_all_zeros() const          { for (int i = 1; i < m_histo.size(); i++) if (m_histo[i]) return false; return true; }
  size_t  get_size() const             { return m_histo.size(); }
  Count_t get_bin_count(size_t i) const          { return m_histo[i]; }
  T       get_bin_center_value(size_t i) const { I3S_ASSERT(i >= 0 && i < m_histo.size()); double v = ((double)i + 0.5) * get_bin_size(); return (T)v; }
  uint64  get_total_count() const             { uint64 ret = 0; for (auto x : m_histo) ret += x; return ret; }
  template< class Pair_t> void    get_most_frequent_values(std::vector< Pair_t >* valCount, int maxOut = 256)const;

  const std::vector< Count_t >& get_histo() const { return m_histo; }
  void    get_stats(Atrb_stats* stats) const;

private:
  static const uint c_shift = ((sizeof(T) * 8 - BitOut));
  std::vector< Count_t >  m_histo;
  T                       m_bitShift; //const
};


template< class T, uint BitOut, class Count_t >
void Histo_unsigned<T, BitOut, Count_t>::resample( std::vector< Count_t >* dest, double* minOut, double* maxOut, int number_of_bin_hint) const
{
  I3S_ASSERT_EXT(m_histo.size());
  //figure out the useful range:
  size_t iN = m_histo.size();
  while (--iN >= 0 && m_histo[iN] == 0){};
  size_t i0 = -1;
  while (++i0 < m_histo.size() && m_histo[i0] == 0){};
  if (number_of_bin_hint <= 0)
  {
    //figure out a "good" size:
    number_of_bin_hint = (int)std::min((size_t)256, iN - i0 + 1);
  }

  double minV        = 0;
  double maxV        = 1 + (std::numeric_limits< T >::max() );
  double binSize= (maxV - minV) / m_histo.size();
  int step      = int( ceil(double(iN - i0 + 1) / (double)number_of_bin_hint) + 0.5);
  int outSize = int( ceil(double(iN - i0 + 1) / (double)step) +0.5);
  *minOut = binSize * i0;
  *maxOut = *minOut + outSize* step *binSize;
  dest->resize( outSize ); 
  for (auto i = i0; i <= iN; i++)
  {
    (*dest)[(i - i0) / step] += m_histo[i];
  }
}

template< class T, uint BitOut, class Count_t >
template< class Pair_t>
void Histo_unsigned<T, BitOut, Count_t>::get_most_frequent_values(std::vector< Pair_t >* valCount, int maxOut)const
{
  if (c_shift)
  {
    valCount->clear(); // histo is quantized, so we can't know the most frequent values.
  }
  valCount->clear();
  for (int i = 0; i <(int)m_histo.size(); ++i)
  {
    if( m_histo[i])
      valCount->push_back(Pair_t((typename Pair_t::first_type)i, (typename Pair_t::second_type)m_histo[i]));
  }
  //sort descending:
  std::sort(valCount->begin(), valCount->end(), [](const Pair_t& a, const Pair_t& b) {return a.count > b.count; });
  //truncate:
  if(valCount->size() > maxOut )
    valCount->resize(maxOut);
}


//! Since stddev doesn't make much sense for this type of data, we ball-park estimate it from the histogram 
template< class T, uint BitOut, class Count_t >
void Histo_unsigned<T, BitOut, Count_t>::get_stats(Atrb_stats* stats) const
{
  std::vector< Count_t> resampled;
  double minS, maxS;
  resample(&resampled, &minS, &maxS);
  stats->histo.set_stats(minS, maxS, resampled);

  double sum2 = 0; //sum of square:
  double s1 = 0;
  double ct = 0;
  int i1 = -1, i2 = -1;
  for (int i = 0; i < m_histo.size(); ++i)
  {
    if (!m_histo[i])
      continue;
    if (m_histo[i] && i1 == -1)
      i1 = i;
    if (m_histo[i])
      i2 = i;

    double val = get_bin_center_value(i);
    sum2 += val*val * (double)m_histo[i];
    s1 += m_histo[i] * val;
    ct += m_histo[i];
  }
  stats->set_stddev(sum2, s1, ct);
  stats->set_min_max(minS, maxS); //not exact, but conservative estimates.
  get_most_frequent_values(&stats->mostFrequent);
}



//--------------------------------------------------------------------------------
//      struct Histo_quantized<>
//--------------------------------------------------------------------------------

//! Histogram with "quantized" bins that may be used for float-types.
template< class T, class Count_t = uint64 >
class Histo_quantized
{
public:
  using value_type = T;

  SERIALIZABLE(Histo_quantized);
  Histo_quantized() : m_histo(1), m_minH(0), m_maxH(1), m_inv_bin_size(1.0), m_bias(0.0) {}

  Histo_quantized(const Range_std_dev<T>& range, int number_of_bins, double sigma=4.0 ) : m_histo(number_of_bins) {
    static_assert(std::is_signed<T>::value, "Only signed types are supported.");
    Atrb_stats stats;
    range.get_stats(&stats);
    // Find the "significant" range for the histogram as 4 sigma stddev + clamped to actual min-max:
    m_minH = (std::max)( (T)range.minV, (T)(stats.avg - stats.stddev * sigma) );
    m_maxH = (std::min)( (T)range.maxV, (T)(stats.avg + stats.stddev * sigma) );
    m_inv_bin_size = (double)(number_of_bins) / double(m_maxH - m_minH);
    m_bias = (double)m_minH;
    //clear memory:
    memset(&m_histo[0], 0, sizeof(Count_t) * m_histo.size());
  }

  Histo_quantized(T minH, T maxH, int number_of_bins) : m_histo(number_of_bins), m_minH(minH), m_maxH(maxH)
  {
    m_inv_bin_size = (double)(number_of_bins) / double(m_maxH - m_minH);
    m_bias = (double)m_minH;
    //clear memory:
    memset(&m_histo[0], 0, sizeof(Count_t) * m_histo.size());
  }
  //! scale the stats by a constant. (usefull for unit conversion of underlying attribute: i.e. "elevation" )
  void    scale_by(double z)           { m_minH = T( (double)m_minH*z); m_maxH = T((double)m_maxH*z);  };
  void    add_value(T val)   noexcept;
  T       get_max_value() const            { return m_maxH; }
  T       get_min_value() const            { return m_minH; }
  T       get_bin_size() const             { return 1.0 / m_inv_bin_size; }
  //bool    IsEmpty() const { for (auto x : m_histo) if (x) return false; return true; }
  bool    is_all_zeros() const          { int loop = 0; for (auto x : m_histo) if (x && loop++) return false; return true; }
  void    resample( std::vector< Count_t >* histo, double* minV, double* maxV, int number_of_bin_hint=-1) const;
  uint64  get_total_count() const       { uint64 ret = 0; for (auto x : m_histo) ret += x; return ret; }

  void    get_stats(Atrb_stats* stats) const;
  void    get_percentile(double percentile, double* val) const;
  double  get_quality( T preferred_median) const;
  const std::vector< Count_t >& get_histo() const { return m_histo; }

  template< class Ar> void serialize(Ar& ar) 
  {
    ar & nvp("min", m_minH);
    ar & nvp("max", m_maxH);
    ar & nvp("counts", seq(m_histo));
  }
private:
  std::vector< Count_t > m_histo;
  T                      m_minH, m_maxH;
  double                 m_inv_bin_size, m_bias;
};

template< class T, class Count_t  >
inline void Histo_quantized<T, Count_t>::add_value(T val) noexcept
{ 
  int k = (int)( ((double)val - m_bias) * m_inv_bin_size +0.5 ); 
  if (k < 0) 
    k = 0; 
  if((size_t)k >= m_histo.size()) 
    k = (int)m_histo.size() - 1;  
  m_histo[k]++; 
}

template< class T, class Count_t  >
inline void Histo_quantized<T, Count_t>::get_stats(Atrb_stats* stats) const
{
  std::vector< Count_t> resampled;
  double minS, maxS;
  resample(&resampled, &minS, &maxS);
  stats->histo.set_stats(minS, maxS, resampled);
  // won't set the stddv stats nor min/max. ( use Range_std_dev<T> for that )
  // won't set the most-frequent value (histo is quantized, we don't have this info)
}

//return the position of the percentile ( in histogram bin units ). Percentile is [0,100] 
template< class Count_t  >
inline double calc_percentile(const std::vector< Count_t >& histo, double percentile )
{
  if (histo.size() < 2)
    return 0.0;
  I3S_ASSERT(percentile > 0.0 && percentile < 100.0);
  double total = std::accumulate(histo.begin(), histo.end(), 0.0, [](double a, Count_t b) { return a + (double)b; });
  double limit = (0.01 * percentile * total);
  int iter = 0;
  double sum = 0;
  while (sum < limit && iter < histo.size())
  {
    sum += (double)histo[iter];
    ++iter;
  }
  if (iter >= histo.size())
    iter = (int)histo.size() - 1;
  double x1 = iter > 0 ? (double)iter - 1.0 : 0.0;
  double x2 = (double)iter;
  double y1 = sum - (double)histo[(size_t)(x2 + 0.5)];
  double y2 = sum;
  double alpha = y1 < y2 ? (limit - y1) / (y2 - y1) : 1.0;
  if (alpha < 0.0)
    alpha = 0.0;
  if (alpha > 1.0)
    alpha = 1.0;
  return (1.0 - alpha) * x1 + alpha * x2;
}

template< class T, class Count_t  >
inline void Histo_quantized<T,Count_t>::get_percentile(double percentile, double* val) const
{
  double pos = calc_percentile(m_histo, percentile);
  //map bin to value:
  *val = (double)pos / m_inv_bin_size + m_bias;
}


//! quality in [0, 1]
template< class T, class Count_t  >
inline double Histo_quantized<T, Count_t>::get_quality( T preferred_median ) const
{
  const double ideal = (double)preferred_median;
  double error_sum = 0.0;
  double sample_count = 0.0;
  for (int i = 0; i < m_histo.size(); ++i)
  {
    double x = (i+0.5) / m_inv_bin_size + m_bias;
    error_sum = abs( x- ideal) * m_histo[0];
    sample_count += (double)m_histo[0];
  }
  return sample_count / (error_sum+ 1.0);

  //double ten, ninety;
  //get_percentile(90.0, &ninety);
  //get_percentile(10.0, &ten);
  //int k1 = (int)(((double)ten - m_bias) * m_inv_bin_size + 0.5);
  //int k2 = (int)(((double)ninety - m_bias) * m_inv_bin_size + 0.5);
  //double quality = (double)(k2-k1) / (double)(m_histo.size() - 1);
  //return quality;
  //double fifty;
  //get_percentile(50.0, &fifty);
  ////how far from target  ?
  //double dist = std::abs(fifty - preferred_median);
  ////normalize:
  //dist = std::min( 1.0, 2.0 *dist / (double)(m_maxH - m_minH) );
  //return (1.0-dist);
}


//! begin/end of the histogram will be dropped. If number_of_bin_hint =1 => resampled to 256 (or less if the histogram is smaller than 256 )
template< class T,  class Count_t >
void Histo_quantized<T, Count_t>::resample( std::vector< Count_t >* dest, double* minOut, double* maxOut, int number_of_bin_hint) const
{
  I3S_ASSERT_EXT(m_histo.size());
  //figure out the useful range:
  int iN = (int)m_histo.size();
  while (--iN >= 0 && m_histo[iN] == 0){}; // iN must a signed integer ( otherwise will wrap around)
  if (iN < 0)
  {
    //this histo is empty.
    dest->resize(0);
    *minOut = 0.0;
    *maxOut = 0.0;
    return;
  }
  int i0 = -1;
  while (++i0 < m_histo.size() && m_histo[i0] == 0){};
  if (number_of_bin_hint <=0)
  {
    //figure out a "good" size:
    number_of_bin_hint = std::min( 256, (int)(iN - i0 + 1) );
  }
  I3S_ASSERT(i0 < m_histo.size());
  double binSize = double(m_maxH - m_minH) / (double)m_histo.size();
  int step = (int)(ceil(double(iN - i0 + 1) / (double)number_of_bin_hint) + 0.5);
  int outSize = (int)(ceil(double(iN - i0 + 1) / (double)step) + 0.5);
  *minOut = binSize * i0 + m_minH;
  *maxOut = *minOut + outSize* step *binSize;
  dest->resize(outSize);
  for (auto i = i0; i <= iN; i++)
  {
    (*dest)[(i-i0) / step] += m_histo[i];
  }
}


//--------------------------------------------------------------------------------
//      class  Histo_hash<>
//--------------------------------------------------------------------------------

//! Histogram for integer types where quantization is *NOT* desired AND number of non-empty bins will be reasonable.
//!
template< class T, class Count_t = uint64 >
class Histo_hash
{
public:
  using value_type = T;

  explicit Histo_hash()
  {
    static_assert(sizeof(T) <= 4 || (T)(0.5 ) != (T)(0.0), "This class in not designed for large integer types, nor floating point types");
  }
   
  void    add_value(T val)   noexcept { m_histo[val]++; }
  bool    is_all_zeros() const { return m_histo.size() ==0 || (m_histo.size()==1 && m_histo.begin()->first==0 ); } //a single value is not very interesting...
  void    resample( std::vector< Count_t>* histo, double* minHisto, double* maxHisto, int maxNbBins=256) const;
  template< class Pair_t> void    get_most_frequent_values(std::vector< Pair_t >* valCount, int maxOut=256 )const ;
  uint64  get_total_count() const { uint64 ret = 0; for (auto x : m_histo) ret += x.second; return ret; }
  void    get_stats(Atrb_stats* stats) const;

  int bucket_count() const { return (int)m_histo.size(); }

private:
  std::unordered_map< T, Count_t > m_histo;
};


template< class T, class Count_t>
void  Histo_hash<T, Count_t>::get_stats(Atrb_stats* stats) const
{

  std::vector< Count_t> resampled;
  double minS, maxS;
  resample(&resampled, &minS, &maxS);
  stats->histo.set_stats(minS, maxS, resampled);

  double s1 = 0.0, s2 = 0.0, ct = 0.0;
  T lo = std::numeric_limits<T>::max(), hi = std::numeric_limits<T>::lowest();
  for (auto iter : m_histo)
  {
    if (iter.first < lo)
      lo = iter.first;
    if (iter.first > hi)
      hi = iter.first;

    ct += (double)iter.second;
    double val = (double)iter.first;
    s1 += val;
    s2 += val*val * (double)iter.second;
  }
  stats->set_stddev(s2, s1, ct);
  if (ct)
  {
    stats->set_min_max(lo, hi);
    get_most_frequent_values(&stats->mostFrequent);
  }
  else
    stats->set_min_max(0.0, 0.0);

}


template< class T, class Count_t>
template< class Pair_t> 
void Histo_hash<T, Count_t>::get_most_frequent_values(std::vector< Pair_t >* valCount, int maxCount) const
{
  valCount->clear();
  for (auto& x : m_histo)
    valCount->push_back( Pair_t( static_cast<typename Pair_t::first_type>(x.first), x.second ) );

  //sort descending:
  std::sort(valCount->begin(), valCount->end(), std::greater<Pair_t >() );
  if(valCount->size() > maxCount )
    valCount->resize(maxCount);
}

//! convert to fixed-size bin histogram. 
template< class T, class Count_t>
void  Histo_hash<T, Count_t>::resample( std::vector< Count_t>* out, double* minHisto, double* maxHisto, int maxBinCount ) const
{
  //if (maxBinCount <= 0)
  //  maxBinCount = 256;
  //find range:
  T minV = std::numeric_limits<T>::max();
  T maxV = std::numeric_limits<T>::lowest();
  for (auto& x : m_histo)
  {
    if (x.first < minV)
      minV = x.first;
    if (x.first > maxV)
      maxV = x.first;
  }
  if ((int)(maxV - minV) < maxBinCount)
  {
    //good news, this conversion will be lossless:
    out->clear();
    out->resize((size_t)(maxV - minV + 1), (Count_t)0); // zeros.
    for (auto& iter : m_histo)
    {
      auto k = (size_t)(iter.first - minV);
      I3S_ASSERT(k >= 0 && k < out->size());
      (*out)[k] = iter.second;
    }
    *minHisto = static_cast<double>(minV); 
    *maxHisto = static_cast<double>(maxV) + 1.0; //so that binSize = (maxHisto-minHisto) / out->size() == 1.0 
    return;
  }
  else
  {
    //we need to resample in this case:
    out->clear();
    out->resize(maxBinCount, (Count_t)0); // zeros.
    if (maxV - minV < 1.0)
      return; //empty
    double step = double(maxBinCount)  / double( maxV-minV+1 );
    for (auto& iter : m_histo)
    {
      double x = step * (double)(iter.first - minV);
      double alpha = x - floor(x);
      double w1 = 1.0 - alpha;
      double w2 = alpha;
      int  k1 = (int)floor(x);
      int  k2 = std::min( k1 + 1, (int)out->size()-1);
      (*out)[k1] += Count_t((double)iter.second * w1 + 0.5 );
      (*out)[k2] += Count_t((double)iter.second * w2 + 0.5 );
    }
    *minHisto = static_cast<double>(minV);
    *maxHisto = static_cast<double>(maxV) + 1.0; //so that binSize = (maxHisto-minHisto) / out->size() == binSize 
    return;
  }
}

//--------------------------------------------------------------------------------
//      class      Histo_generic
//--------------------------------------------------------------------------------

//! a "generic" implementation. Potentially slower, but easy to use (if no assumption can be made about input)
//! must call range_estimate_add_sample() to add all samples, then add_sample() on all samples ( 2-pass )
template< class T >
class Histo_generic
{
public:
  using value_type = T;

  explicit Histo_generic(int max_unique_value = 64 * 1024);
  void    range_estimate_add_sample(T val) { m_range.add_value(val); }
  void    add_sample(T val);
  void    get_stats( Atrb_stats* stats ) const;

  uint64     get_count() const { return m_range.count; }
private:
  void     _finalize_estimation_pass();
private:
  static const uint64                   m_number_of_bins= 8192;
  int                                   m_max_unique_value; // before we give up on the hash...
  std::unique_ptr< Histo_hash<T> >      m_hash;
  std::unique_ptr< Histo_quantized<T> > m_quantized;
  Range_std_dev<T>                      m_range;
  uint64_t                              m_pass2_count;
};

template< class T >
inline Histo_generic<T>::Histo_generic(int max_unique_value)
  : m_max_unique_value(max_unique_value), m_pass2_count(0)
  //, m_hash( new Histo_hash<T>())
{
  
}

template< class T >
inline void Histo_generic<T>::_finalize_estimation_pass()
{
  //collect info about the "estimated" range:
  double mean, sigma;
  m_range.calc_mean(&mean, &sigma);
  m_quantized.reset(new Histo_quantized< T >(T(mean - sigma* 4.0), T(mean + sigma* 4.0), m_number_of_bins));
  
  if constexpr (std::is_integral<T>::value && sizeof(T) <= 4) //Only for integral types.
    m_hash.reset(new  Histo_hash<T>());

  m_pass2_count = 0;
}

template< class T >
inline void Histo_generic<T>::add_sample(T val)
{
  if (!m_quantized)
    _finalize_estimation_pass();

  if (m_hash)
    m_hash->add_value( val );
  m_quantized->add_value(val);
  ++m_pass2_count;
  if (m_hash && (m_hash->bucket_count() > m_max_unique_value))
  {
    //forget it,too many unique values:
    m_hash.reset();
  }
}

template< class T >
inline void  Histo_generic<T>::get_stats(Atrb_stats* stats) const
{
  if (m_range.is_empty())
  {
    *stats = Atrb_stats();
    return;
  }
  //collect the stats:
  stats->count = (double)m_range.count;
  stats->minH = (double)m_range.minV;
  stats->maxH = (double)m_range.maxV;
  m_range.calc_mean(&stats->avg, &stats->stddev);
  stats->sum = (double) m_range.m_sum;
  stats->var = stats->stddev*stats->stddev;
  //collect histo:
  if(m_hash)
  {
    m_hash->get_most_frequent_values( &stats->mostFrequent);
    m_hash->resample(&stats->histo.counts, &stats->histo.minH, &stats->histo.maxH);
  }
  else
  {
    m_quantized->resample(&stats->histo.counts, &stats->histo.minH, &stats->histo.maxH);
  }
}

//--------------------------------------------------------------------------------
//      class      Histo_string
//--------------------------------------------------------------------------------

template< class String_t >
class Histo_string
{
public:
  using value_type = String_t;

  typedef Value_count_tpl< String_t > StringCount;
  explicit Histo_string(size_t mem_quota = 10 * 1024* 1024) : m_mem_quota(mem_quota), m_mem_usage(0){}
  void      add_value(const String_t& str);
  void      get_most_frequent_values(std::vector< StringCount >* freq, uint64* total_count, int max_output_size=256) const;
private:
  size_t                                  m_mem_quota;
  size_t                                  m_mem_usage;
  std::unordered_map< String_t, uint64 >  m_counts;
};

template< class String_t >
void Histo_string<String_t>::add_value(const String_t& str)
{
  if (m_mem_usage < m_mem_quota)
  {
    auto before = m_counts.size();
    ++m_counts[str];
    if( before < m_counts.size() )
      m_mem_usage += str.size() * sizeof(str[0]); // estimated memory used by string keys in the hash table. 
    if (m_mem_usage >= m_mem_quota)
    {
      //m_counts.swap(std::unordered_map< String_t, uint64 >()); //too many unique strings. no stats.
      m_counts.clear();
    }
  }
}

template< class String_t >
void Histo_string<String_t>::get_most_frequent_values(std::vector< StringCount >* freq, uint64* total_count, int max_output_size) const
{
  *total_count = 0;
  freq->clear();
  if (m_counts.size() == 0)
    return; 

  freq->resize(m_counts.size());
  //copy:
  size_t loop = 0;
  for (auto s : m_counts)
  {
    (*freq)[loop++] = StringCount(s.first, s.second);
    *total_count += s.second;
  }
  //sort by descending order:
  std::sort(freq->begin(), freq->end(), std::greater<StringCount>());
  //trim:
  if (freq->size() > max_output_size)
    freq->resize(max_output_size);
  return;
}


//--------------------------------------------------------------------------------
//      class      Histo_datetime
//--------------------------------------------------------------------------------
template< class String_t >
class Histo_datetime
{
public:
  using value_type = String_t;

  explicit  Histo_datetime(size_t mem_quota = 10 * 1024 * 1024) : m_histo(mem_quota) {}
  void      add_value(const String_t& str);
  void      get_stats(utl::Atrb_stats_datetime<String_t>* stats) const;
private:
  Histo_string<String_t>  m_histo;
  String_t min_time;
  String_t max_time;
};

template< class String_t >
void Histo_datetime<String_t>::add_value(const String_t& str)
{
  m_histo.add_value(str);
  if (str.size())
  {
    if (str < min_time || min_time.empty())
      min_time = str;
    if (str > max_time || max_time.empty())
      max_time = str;
  }
}

template<class String_t>
void Histo_datetime<String_t>::get_stats(utl::Atrb_stats_datetime<String_t>* stats) const
{
  m_histo.get_most_frequent_values(&stats->mostFrequent, &stats->totalValuesCount);
  stats->max_time = max_time;
  stats->min_time = min_time;
}


//--------------------------------------------------------------------------------
//      class      Std_dev
//--------------------------------------------------------------------------------
//! Simple std deviation calculation
class Std_dev
{
public:
  Std_dev() : m_sum(0.0), m_sum2(0.0), m_count(0) {}
  bool      is_valid() const { return m_count > 0; }
  void      add_value(double v) noexcept { m_sum += v; m_sum2 += v*v; m_count++; }
  void      add_value(const Std_dev& d) { m_sum += d.m_sum; m_sum2 += d.m_sum2; m_count += d.m_count; };
  void      calc_mean(double* mean, double* sigma) const
  {
    if (m_count == 0)
    {
      *mean = 0.0;
      *sigma = 0.0;
    }
    else
    {
      const auto n = static_cast<double>(m_count);
      *mean = m_sum / n;
      *sigma = m_count > 1 ? std::sqrt((n * m_sum2 - m_sum * m_sum) / (n * (n - 1))) : 0.0;
    }
  }
  template< class Ar > void serialize(Ar& ar) { ar & nvp("sum", m_sum) & nvp("sumSqr", m_sum2) & nvp("count", m_count); }
private:
  double m_sum, m_sum2;
  uint64 m_count;
};



}
} // namespace i3slib
