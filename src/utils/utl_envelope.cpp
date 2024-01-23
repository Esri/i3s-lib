/*
Copyright 2022 Esri

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

#include "pch.h"
#include "utils/utl_envelope.h"
#include "utils/utl_box.h"
#include "utils/utl_i3s_assert.h"
#include <algorithm>
#include <utility>

namespace i3slib
{

namespace utl 
{

namespace
{

// TODO: this function can be implemented using "pairing" technique that reduces
// the number of comparisons from 2 * N to 3/2 * N per dimension. 
// (This approach is employed by std::minmax_element, see its specification and
// implementation).
Boxd compute_simple_envelope(const Vec3d* points, size_t point_count)
{
  I3S_ASSERT(point_count > 0);

  Boxd aabb;
  for (size_t i = 0; i < point_count; i++)
    aabb.expand(points[i]);

  return aabb;
}

Boxd compute_simple_geo_envelope(const Vec3d* points, size_t point_count)
{
  I3S_ASSERT(point_count > 0);

  Boxd aabb = compute_simple_envelope(points, point_count);

  I3S_ASSERT(aabb.left() >= -180.0);
  I3S_ASSERT(aabb.right() <= 180.0);
  I3S_ASSERT(aabb.left() <= aabb.right());

  I3S_ASSERT(aabb.bottom() >= -90.0);
  I3S_ASSERT(aabb.top() <= 90.0);
  I3S_ASSERT(aabb.bottom() <= aabb.top());

  I3S_ASSERT(aabb.front() <= aabb.back());
  return aabb;
}

Boxd& set_lon_range(Boxd& aabb, double x_min, double x_max)
{
  I3S_ASSERT(x_min >= -180.0);
  I3S_ASSERT(x_min <= 180.0);
  I3S_ASSERT(x_max >= -180.0);
  I3S_ASSERT(x_max <= 180.0);
  aabb.left() = x_min;
  aabb.right() = x_max;
  return aabb;
}

template<typename T>
void set_min(T& cur_min, T value)
{
  if (value < cur_min)
    cur_min = value;
}

template<typename T>
void set_max(T& cur_max, T value)
{
  if (value > cur_max)
    cur_max = value;
}

bool points_envelope_optimistic(const Vec3d* points, size_t point_count, Boxd& aabb)
{
  I3S_ASSERT(point_count > 0);

  // Compute "naive" coordinate ranges.
  // For latitude and height this gives the correct values right away.
  // For longitude things may get more complicated. 
  aabb = compute_simple_envelope(points, point_count);

  // The further computations here only involve longitudes.
  // All comments mentioning degrees, ranges etc. refer to longitude.

  // If all the input points are within a 180 degree range that does not
  // wrap over the 180/-180 break, we know that the "naive" range is the true one.
  // This covers the most typical case: a "local" set of points not in the vicinity
  // of the 180th meridian.
  if (aabb.right() <= aabb.left() + 180.0)
  {
    if (aabb.left() == -180.0 && aabb.right() == 0.0)
    {
      const auto is_internal = [](const Vec3d& p) { return p.x != 0.0 && p.x != -180.0; };
      if (!std::any_of(points, points + point_count, is_internal))
        set_lon_range(aabb, 0.0, 180.0);
    }
    else if (aabb.right() == -180.0)
    {
      // If all input points have longitude value of -180, we get [-180, -180] for the
      // longitude range. In this case we normalize the range to [180, 180].
      I3S_ASSERT(aabb.left() == -180.0);
      set_lon_range(aabb, 180.0, 180.0);
    }

    return true;
  }

  // If all the points were within [-180, 0] range, the above check would cover this.
  // If all the points were within [0, 180] range, the above check would also cover this.
  // So if we got here it means that at least one of the input points must be in [-180, 0)
  // and at least one of the input points must be in (0, 180].
  I3S_ASSERT(point_count >= 2);
  I3S_ASSERT(aabb.left() < 0.0);
  I3S_ASSERT(aabb.right() > 0.0);

  // Compute longitude range using the [0, 360] coordinate range.
  double min_pos = 180.0, max_neg = -180.0;
  for (size_t i = 0; i < point_count; i++)
  {
    if (const auto x = points[i].x; x >= 0.0)
      set_min(min_pos, x);
    else
      set_max(max_neg, x);
  }

  I3S_ASSERT(min_pos >= 0.0);
  I3S_ASSERT(min_pos <= 180.0);
  I3S_ASSERT(max_neg >= -180.0);
  I3S_ASSERT(max_neg < 0.0);

  // If all the input points are within a 180 degree range that does not
  // wrap over the 360/0 break, we know that this range is the true one.
  // This covers the case when we have a "local" set of points somewhere near
  // the 180th meridian.
  if (min_pos >= max_neg + 180.0)
  {
    if (max_neg == -180.0)
    {
      I3S_ASSERT(aabb.left() == -180.0);

      // The range touches the 180th meridian from east but does not go beyond it.
      // In this case it's natural to represent it ending at 180 degrees.
      max_neg = 180.0;
    }
    else if (min_pos == 180.0)
    {
      I3S_ASSERT(aabb.right() == 180.0);

      // The range touches the 180th meridian from west but does not go beyond it.
      // In this case it's natural to represent it starting at -180 degrees.
      min_pos = -180.0;
    }

    set_lon_range(aabb, min_pos, max_neg);
    return true;
  }

  return false;
}

} // namespace

Boxd compute_points_geo_envelope(const Vec3d* points, size_t point_count)
{
  Boxd aabb;
  if (points_envelope_optimistic(points, point_count, aabb))
    return aabb;

  // The optimistic tests did not succeed for the input we've got.
  // We have to resort to the generic approach that finds the largest gap
  // in longitudes between the input points around the globe.
  I3S_ASSERT(point_count >= 2);

  // Extract longitude values and sort them.
  // TODO: would be nice to use a container like small_vector to avoid heap allocation
  // when point_count is small. This is not critical however because inputs consisting
  // of small number of points scattered around the globe are very rare in practice.
  std::vector<double> lons(point_count);
  std::transform(points, points + point_count, std::begin(lons), [](const Vec3d& p) { return p.x; });
  std::sort(std::begin(lons), std::end(lons));

  //
  if (lons.front() == -180.0 && lons.back() != 180.0)
    lons.push_back(180.0);

  // Go through the sorted longitudes and find the largest gap between adjacent values.
  // We also need to consider the gap between the last and the first value (wrapping over
  // the 180/-180 break).
  size_t ind_max = 0;
  auto d_max = lons[0] - lons.back() + 360.0; // last-to-first wrapping gap
  for (size_t i = 1, size = lons.size(); i < size; i++)
  {
    if (const auto d = lons[i] - lons[i - 1]; d > d_max)
    {
      ind_max = i;
      d_max = d;
    }
  }

  // Gap between lons[ind_max - 1] and lons[ind_max] is the maximum one.
  // Swapping its endpoints we get the mimimum range covering all the points.
  const auto w = lons[ind_max];
  const auto e = ind_max == 0 ? lons.back() : lons[ind_max - 1];

  aabb.left() = w == 180.0 ? -180.0 : w;
  aabb.right() = e == -180.0 ? 180.0 : e;
  return aabb;
}

namespace
{

std::pair<double, double> merge_longitude_ranges_impl(
  size_t input_count,
  std::vector<std::pair<double, double>>& ranges,
  double wrap_max,
  double wrap_min)
{
  // Check if the input AABBs together cover the whole longitude circumference.
  if (wrap_min <= wrap_max)
    return { -180.0, 180.0 };

  if (ranges.empty())
  {
    // There are no input AABBs within the (wrap_max, wrap_min) longitude range,
    // so the result is the range [wrap_min, wrap_max] (wrapping over 180/-180).
    // NB: if the resulting range degenerates to the 180/-180 point, we always
    // output it as [180, 180].
    if (wrap_max == -180.0)
      return { wrap_min, 180.0 };
    else if (wrap_min == 180.0)
      return { -180.0, wrap_max };
    else
      return { wrap_min, wrap_max };
  }

  // Sort ranges by start ascending, breaking ties by end descending.
  std::sort(std::begin(ranges), std::end(ranges), [](const auto& r1, const auto& r2)
    { return r1.first < r2.first || r1.first == r2.first && r1.second > r2.second; });

  if (ranges.size() == input_count)
  {
    // None of the input ranges wraps over the 180/-180 break.
    auto cur = ranges.front().second;
    double d_max = 0.0, max_l = 0.0, max_r = 0.0;
    for (const auto& range : ranges)
    {
      I3S_ASSERT(range.first <= range.second);
      I3S_ASSERT(range.first > -180.0);
      I3S_ASSERT(range.second < 180.0);

      if (range.first > cur)
      {
        // There's a gap between cur and range.first.
        // Compare it to the current max gap and update if needed.
        if (const auto d = range.first - cur; d > d_max)
        {
          d_max = d;
          max_l = cur;
          max_r = range.first;
        }
      }

      if (range.second > cur)
        cur = range.second;
    }

    // Now cur is at the eastmost longitude of input segments. 
    // The westmost longitude of input segments is the beginning of the first segment
    // in the sorted list.
    // Compute the size of the gap between these two points (which wraps over 180/-180).
    // Compare the wrapping gap to the maximum non-wrapping gap computed above,
    // choose the largest one.
    // The complement of the largest gap is the resulting range.

    return ranges.front().first + 360.0 - cur >= d_max ?
      std::make_pair(ranges.front().first, cur) :  // non-wrapping range
      std::make_pair(max_r, max_l);                // a wrapping range
  }

  // The most general case: we have a non-empty wrapping range along with some "normal" segments.
  auto cur = wrap_max;
  double d_max = 0.0, max_l = 0.0, max_r = 0.0;
  for (const auto& range : ranges)
  {
    I3S_ASSERT(range.first <= range.second);
    I3S_ASSERT(range.first > -180.0);
    I3S_ASSERT(range.second < 180.0);

    if (range.first >= wrap_min)
      break;

    if (range.first > cur)
    {
      // There's a gap between cur and range.first.
      // Compare it to the current max gap and update if needed.
      if (const auto d = range.first - cur; d > d_max)
      {
        d_max = d;
        max_l = cur;
        max_r = range.first;
      }
    }

    if (range.second > cur)
    {
      cur = range.second;
      if (cur >= wrap_min)
        break;
    }
  }

  if (cur + d_max < wrap_min)
    return { wrap_min == 180.0 ? -180.0 : wrap_min, cur };
  else if (d_max > 0.0)
    return { max_r, max_l };
  else
    return { -180.0, 180.0 };
}

template<typename F>
std::pair<double, double> merge_longitude_ranges(size_t input_count, F range_func)
{
  std::vector<std::pair<double, double>> ranges;
  ranges.reserve(input_count); // ?

  double wrap_min = 180.0, wrap_max = -180.0;
  for (size_t i = 0; i < input_count; i++)
  {
    // Get next input range.
    const auto [w, e] = range_func(i);

    if (w > e)
    {
      // This input range crosses the 180th meridian, update wrapping range.
      set_min(wrap_min, w);
      set_max(wrap_max, e);
    }
    else
    {
      // This input segment does not cross the 180th meridian.
      // If the AABB intersects the wrapping range, just extend the wrapping range.
      // Otherwise add the AABB longitude range to the list. 
      if (w <= wrap_max)
        set_max(wrap_max, e);
      else if (e >= wrap_min)
        set_min(wrap_min, w);
      else
        ranges.emplace_back(w, e);
    }
  }

  return merge_longitude_ranges_impl(input_count, ranges, wrap_max, wrap_min);
}

std::pair<double, double> get_triangle_longitude_range(const Vec3d* points)
{
  double left, right;
    
  if (points[0].x <= points[1].x)
  {
    left = std::min(points[0].x, points[2].x);
    right = std::max(points[1].x, points[2].x);
  }
  else
  {
    left = std::min(points[1].x, points[2].x);
    right = std::max(points[0].x, points[2].x);
  }

  I3S_ASSERT(left <= right);
  I3S_ASSERT(left <= points[0].x);
  I3S_ASSERT(left <= points[1].x);
  I3S_ASSERT(left <= points[2].x);
  I3S_ASSERT(points[0].x <= right);
  I3S_ASSERT(points[1].x <= right);
  I3S_ASSERT(points[2].x <= right);

  return right < 180 + left ? std::make_pair(left, right) : std::make_pair(right, left);
}

}

Boxd compute_mesh_geo_envelope(const Vec3d* triangles, size_t triangle_count)
{
  I3S_ASSERT(triangle_count > 0);

  // Consider all triangle vertices as just a set of points and try the same
  // optimistic tests we use for points. This is valid since we can't have
  // a triangle that spans more than 180 degrees in longitude.
  Boxd aabb;
  if (points_envelope_optimistic(triangles, triangle_count * 3, aabb))
    return aabb;

  // The optimistic attempts did not succeed.
  // We have to resort to the general approach that finds the largest gap
  // in longitude between the input triangles around the globe.
  const auto range_func =
    [triangles](size_t i) { return get_triangle_longitude_range(&triangles[3 * i]); };
  
  const auto lon_range = merge_longitude_ranges(triangle_count, range_func);
  return set_lon_range(aabb, lon_range.first, lon_range.second);
}

namespace
{

Boxd compute_lat_height_ranges(const Boxd* aabbs, size_t count)
{
  I3S_ASSERT(count > 0);

  // Compute latitude and height ranges.
  Boxd aabb;
  for (size_t i = 0; i < count; i++)
  {
    set_min(aabb.bottom(), aabbs[i].bottom());
    set_max(aabb.top(), aabbs[i].top());
    set_min(aabb.front(), aabbs[i].front());
    set_max(aabb.back(), aabbs[i].back());
  }

  I3S_ASSERT(aabb.bottom() >= -90.0);
  I3S_ASSERT(aabb.top() <= 90.0);
  I3S_ASSERT(aabb.bottom() <= aabb.top());

  I3S_ASSERT(aabb.front() <= aabb.back());
  return aabb;
}

} // namespace

Boxd merge_geo_envelopes(const Boxd* aabbs, size_t count)
{
  I3S_ASSERT(count > 0);

  Boxd aabb = compute_lat_height_ranges(aabbs, count);

  // All the computations below only involve longitudes.
  // All comments mentioning degrees, ranges etc. refer to longitude.
  {
    // Try to compute simple longitude range.
    // If we discover in the process that one of the input boxes wraps over
    // the 180/-180 break we know right away that the naive approach is not gonna work,
    // and break out.
    size_t i = 0;
    for (; i < count; i++)
    {
      const auto w = aabbs[i].left();
      const auto e = aabbs[i].right();
      if (e < w)
        break;

      set_min(aabb.left(), w);
      set_max(aabb.right(), e);
    }

    // If no input AABB wraps over the 180/-180 break AND they all are within
    // a 180 degree range that does not wrap over the 180/-180 break, we know that
    // the "naive" range is the true one.
    // This covers the most typical case: a "local" set of AABBs not in the vicinity
    // of the 180th meridian.
    if (i == count && aabb.right() <= aabb.left() + 180.0)
    {
      I3S_ASSERT(aabb.left() >= -180.0);
      I3S_ASSERT(aabb.right() <= 180.0);
      I3S_ASSERT(aabb.left() <= aabb.right());

      // If the resulting range is degenerate [-180, -180], normalize to [180, 180].
      if (aabb.right() == -180.0)
      {
        I3S_ASSERT(aabb.left() == -180.0);
        set_lon_range(aabb, 180.0, 180.0);
      }

      return aabb;
    }
  }

  {
    // Try to compute "naive" range in the [0, 360] worldview.
    double min_pos = 180.0, max_neg = -180.0;
    size_t i = 0;
    for (; i < count; i++)
    {
      const auto w = aabbs[i].left();
      const auto e = aabbs[i].right();

      // If one of the input boxes wraps over the 360/0 break, the naive approach
      // is not gonna work.
      if (w * e * (e - w) < 0.0)
        break;

      if (w >= 0.0 && w < min_pos)
        min_pos = w;

      if (e <= 0.0 && e > max_neg)
        max_neg = e;
    }

    I3S_ASSERT(min_pos >= 0.0);
    I3S_ASSERT(min_pos <= 180.0);
    I3S_ASSERT(max_neg >= -180.0);
    I3S_ASSERT(max_neg <= 0.0);

    // If no input AABB wraps over the 360/0 break AND they all are within
    // a 180 degree range that does not wrap over the 360/0 break, we know that
    // this range is the true one. For the output we need to represent it as
    // a wrapping range in the [-180, 180] universe.
    // This covers the case when we have a "local" set of AABBs near the 180th
    // meridian.
    if (i == count && min_pos >= max_neg + 180.0)
    {
      if (max_neg == -180.0)
      {
        // The range touches the 180th meridian from east but does not go beyond it.
        // In this case it's natural to represent it ending at 180 degrees.
        max_neg = 180.0;
      }
      else if(min_pos == 180.0)
      {
        // The range touches the 180th meridian from west but does not go beyond it.
        // In this case it's natural to represent it starting at -180 degrees.
        min_pos = -180.0;
      }

      return set_lon_range(aabb, min_pos, max_neg);
    }
  }

  // Optimistic attempts did not work out for this input.
  // We need to resort to the general approach that finds the largest gap in longitudes
  // between the input AABBs around the globe.
  const auto range_func =
    [aabbs](size_t i) { return std::make_pair(aabbs[i].left(), aabbs[i].right()); };

  const auto lon_range = merge_longitude_ranges(count, range_func);
  return set_lon_range(aabb, lon_range.first, lon_range.second);
}

// Considers X and Y components of the box as a geographic extent and inflates it.
void inflate_geo_envelope(utl::Boxd& box, double margin_factor)
{
  I3S_ASSERT_EXT(box.left() >= -180.0 && box.left() <= 180.0);
  I3S_ASSERT_EXT(box.right() >= -180.0 && box.right() <= 180.0);
  I3S_ASSERT_EXT(box.bottom() >= -90.0);
  I3S_ASSERT_EXT(box.bottom() <= box.top());
  I3S_ASSERT_EXT(box.top() <= 90.0);

  I3S_ASSERT_EXT(margin_factor >= 0.0);

  // Inflate longitude span.
  const auto d_lon = utl::get_longitude_range_size(box.left(), box.right()) * margin_factor;
  const auto lon_range = utl::expand_longitude_range(box.left(), box.right(), d_lon, d_lon);
  box.left() = lon_range.first;
  box.right() = lon_range.second;

  // Inflate latitude span.
  const auto d_lat = box.height() * margin_factor;
  box.bottom() = std::max(box.bottom() - d_lat, -90.0);
  box.top() = std::min(box.top() + d_lat, 90.0);
}

double get_longitude_range_size(double west, double east)
{
  I3S_ASSERT_EXT(west >= -180.0 && west <= 180.0);
  I3S_ASSERT_EXT(east >= -180.0 && east <= 180.0);
  const auto d = east - west;
  return d >= 0.0 ? d : d + 360.0;
}

std::pair<double, double> expand_longitude_range(double west, double east, double dw, double de)
{
  I3S_ASSERT_EXT(west >= -180.0 && west <= 180.0);
  I3S_ASSERT_EXT(east >= -180.0 && east <= 180.0);
  I3S_ASSERT_EXT(dw >= 0.0);
  I3S_ASSERT_EXT(de >= 0.0);

  constexpr auto c_full_circle = std::make_pair(-180.0, 180.0);

  auto w = west - dw;
  auto e = east + de;

  if (west <= east)
  {
    if (w <= -180.0)
    {
      const auto w_wrapped = w + 360.0;

      if (e >= w_wrapped)
        return c_full_circle;
      else
      {
        I3S_ASSERT(e < 180.0);
        if (e == -180.0)
          e = 180.0;

        return { w_wrapped, e };
      }
    }
    else if (e > 180.0)
    {
      const auto e_wrapped = e - 360.0;

      if (e_wrapped >= w)
        return c_full_circle;
      else
        return { w, e_wrapped };
    }
    else
      return { w, e };
  }
  else
  {
    // Wrapping range should remain wrapping after expansion
    // (unless it turns into a full circle).
    return w <= e ? c_full_circle : std::make_pair(w, e);
  }
}

bool longitude_ranges_overlap(double w1, double e1, double w2, double e2)
{
  I3S_ASSERT_EXT(-180.0 <= w1 && w1 <= 180.0);
  I3S_ASSERT_EXT(-180.0 <= e1 && e1 <= 180.0);
  I3S_ASSERT_EXT(-180.0 <= w2 && w2 <= 180.0);
  I3S_ASSERT_EXT(-180.0 <= e2 && e2 <= 180.0);

  if (w1 <= e1)
  {
    if (e2 < w2)
      return w2 <= e1 || w1 <= e2; // regular vs wrapping 
    else
    {
      // Regular vs regular.
      if (w2 > e1)
        return w1 == -180.0 && e2 == 180.0;
      else if (w1 > e2)
        return w2 == -180.0 && e1 == 180.0;
      else
        return true;
    }
  }
  else
  {
    if (w2 <= e2)
      return w2 <= e1 || w1 <= e2; // wrapping vs regular
    else
      return true; // wrapping vs wrapping 
  }
}

} // namespace utl

} // namespace i3slib
