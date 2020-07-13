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

#ifdef ETC2COMP_STATIC
  #define ETC2COMP_EXPORT
#else
  #ifdef ETC2COMP_LIB
    #if defined(_WIN32)
      #define ETC2COMP_EXPORT __declspec(dllexport)
    #else
      #define ETC2COMP_EXPORT __attribute__ ((visibility("default")))
    #endif
  #else
    #if defined(_WIN32)
      #define ETC2COMP_EXPORT __declspec(dllimport)
    #else
      #define ETC2COMP_EXPORT __attribute__ ((visibility("default")))
    #endif
  #endif
#endif

//! create a C api for interop ( runtime loading of the DLL)
extern "C"
{
  //! must match etc2comp lib
  enum etc_error_metric
  {
    RGBA,    RGBX,    REC709,    NUMERIC,    NORMALXYZ,    ERROR_METRICS,  BT709 = etc_error_metric::REC709  };

  //! must match etc2comp lib
  enum etc_img_format
  {
    UNKNOWN=0,    ETC1,    RGB8,    SRGB8,    RGBA8,    SRGBA8,    R11,    SIGNED_R11,    RG11,    SIGNED_RG11,    RGB8A1,    SRGB8A1,    FORMATS,    DEFAULT = SRGB8
  };
  
  //! opaque pointer. caller must call etc_free_mip_image() when done
  typedef void* mip_image_hdl_t;


  //! Do not forget to call etc_free_mip_image() if the return hdl is != 0 
ETC2COMP_EXPORT mip_image_hdl_t   etc_encode_mipmaps(float *a_pafSourceRGBA,
                                     unsigned int a_uiSourceWidth,
                                     unsigned int a_uiSourceHeight,
                                     etc_img_format a_format,
                                     etc_error_metric a_eErrMetric,
                                     float a_fEffort,
                                     unsigned int a_uiMaxJobs,
                                     unsigned int a_uiMipFilterFlags,
                                     int *a_piEncodingTime_ms);

//! useful to allocate to ouput buffer size (KTX)
ETC2COMP_EXPORT int   etc_get_ktx_size(mip_image_hdl_t hdl);
//! Write the KTX out:
ETC2COMP_EXPORT bool  etc_write_ktx(mip_image_hdl_t hdl, char* out_ptr, int max_byte_out);
//! Free resources:
ETC2COMP_EXPORT void  etc_free_mip_image(mip_image_hdl_t hdl);

}
