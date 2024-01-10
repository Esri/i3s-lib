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

#include "pch.h"
#include "utils/utl_image_resize.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <memory>
#include <cstring>

namespace i3slib
{

namespace utl
{

namespace
{

void average_samples_uint8(double frac_part, const uint8_t* first_sample, const uint8_t* second_sample
                           , uint8_t* dst, uint32_t channels, Alpha_mode mode)
{
  if (mode == Alpha_mode::Pre_mult)
  {
    for (uint32_t channel = 0; channel < channels; channel++)
    {
      double avg = (first_sample[channel] * (1.0 - frac_part)) + (second_sample[channel] * frac_part);
      if (avg <= 0.0)
        dst[channel] = 0;
      else if (avg >= 255.0)
        dst[channel] = 255;
      else
        dst[channel] = (uint8_t)avg;
    }
  }
  else
  {
    // non pre-multiplied 
    double alpha1 = first_sample[3] / 255.0;
    double alpha2 = second_sample[3] / 255.0;
    for (uint32_t channel = 0; channel < 3; channel++)
    {
      double x1 = (first_sample[channel] * alpha1) + (second_sample[channel] * (1.0 - alpha1));
      double x2 = (first_sample[channel] * (1.0 - alpha2)) + (second_sample[channel] * alpha2);
      double avg = (x1 * (1.0 - frac_part)) + (x2 * frac_part);
      if (avg <= 0.0)
        dst[channel] = 0;
      else if (avg >= 255.0)
        dst[channel] = 255;
      else
        dst[channel] = (uint8_t)avg;
    }
    double avg = (first_sample[3] * (1.0 - frac_part)) + (second_sample[3] * frac_part);
    if (avg <= 0.0)
      dst[3] = 0;
    else if (avg >= 255.0)
      dst[3] = 255;
    else
      dst[3] = (uint8_t)avg;
  }
}

void average_samples_uint8(double frac_part, int int_part, const std::uint8_t* src, std::uint8_t* dst
                           , uint32_t channels,  Alpha_mode mode = Alpha_mode::Default)
{
  const std::uint8_t* first_sample = src + (channels * int_part);
  const std::uint8_t* second_sample = src + (channels * (int_part + 1));
  average_samples_uint8(frac_part, first_sample, second_sample, dst, channels, mode);
}

void get_real_sample_coords(uint32_t dimension, double u, double* frac_part, int* int_part)
{
  u *= dimension;
  u -= 0.5;
  *frac_part = modf(u, &u);
  *int_part = (int)u;

  // clamp to edge
  if (*int_part < 0)
  {
    *int_part = 0;
    *frac_part = 0.0;
  }
  else if (*int_part >= ((int)dimension - 1))
  {
    *int_part = dimension - 2;
    if (*int_part < 0)
      *int_part = 0;
    *frac_part = 1.0;
  }
}

void sample_linear_1d_uint8(uint32_t dimension, double u, const uint8_t* src, uint8_t* dst, uint32_t channels)
{
  if (dimension == 1)
    std::memcpy(dst, src, channels);
  else
  {
    double frac_part;
    int int_part;
    get_real_sample_coords(dimension, u, &frac_part, &int_part);
    average_samples_uint8(frac_part, int_part, src, dst, channels);
  }
}

void sample_linear_2d_uint8(uint32_t width, uint32_t height, double u, double v
                            , const uint8_t* src, uint8_t* dst, uint32_t channels, Alpha_mode alpha_mode)
{
  if (width == 1)
    sample_linear_1d_uint8(height, v, src, dst, channels);
  else if (height == 1)
    sample_linear_1d_uint8(width, u, src, dst, channels);
  else
  {
    double frac_part;
    int int_part;
    get_real_sample_coords(height, v, &frac_part, &int_part);

    uint32_t pitch = channels * width;
    const uint8_t* first_row = src + (pitch * int_part);
    const uint8_t* second_row = src + (pitch * (int_part + 1));

    uint8_t first_sample[4];
    uint8_t second_sample[4];

    {
      double frac_part_u;
      int int_part_u;
      get_real_sample_coords(width, u, &frac_part_u, &int_part_u);
      average_samples_uint8(frac_part_u, int_part_u, first_row, first_sample, channels);
      average_samples_uint8(frac_part_u, int_part_u, second_row, second_sample, channels);
    }

    average_samples_uint8(frac_part, first_sample, second_sample, dst, channels, alpha_mode);
  }
}

} // namespace

void resample_2d_uint8(uint32_t src_width, uint32_t src_height, uint32_t dst_width
                       , uint32_t dst_height, const void* src, void* dst, uint32_t channels, Alpha_mode alpha_mode)
{
  uint8_t* bdst = (uint8_t*)dst;

  double increment_w = 0.5 / (double)dst_width;
  double increment_h = 0.5 / (double)dst_height;

  for (uint32_t y = 0; y < dst_height; y++)
  {
    double v = ((double)y / (double)dst_height) + increment_h;
    for (uint32_t x = 0; x < dst_width; x++)
    {
      double u = ((double)x / (double)dst_width) + increment_w;
      sample_linear_2d_uint8(src_width, src_height, u, v, (const uint8_t*)src, bdst, channels, alpha_mode);
      bdst += channels;
    }
  }
}

} // namespace utl 

} // namespace i3slib
