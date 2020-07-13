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
#include "utl_i3s_export.h"
#include <string>

namespace i3slib
{
namespace utl
{

I3S_EXPORT std::string get_message_string(int string_id);
I3S_EXPORT bool get_message_string(int string_id, std::string& string);

}
}

#define IDS_I3S_VISITING_NODE           7000
#define IDS_I3S_OUT_OF_RANGE_ID         7001
#define IDS_I3S_TYPE_MISMATCH           7002
#define IDS_I3S_MISSING_JPG_OR_PNG      7003
#define IDS_I3S_INVALID_TREE_TOPOLOGY   7004
#define IDS_I3S_EMPTY_LEAF_NODE         7005
#define IDS_I3S_UNCONNECTED_NODE        7006
#define IDS_I3S_MISSING_ATTRIBUTE_STATS 7007
#define IDS_I3S_GEOMETRY_COMPRESSION_RATIO 7008
#define IDS_I3S_JSON_PARSING_ERROR      7009
#define IDS_I3S_INVALID_LOD_METRIC      7010
#define IDS_I3S_EXPECTS                 7011
#define IDS_I3S_UNSUPPORTED_VERSION     7012
#define IDS_I3S_PATH_COMPATIBILITY_WARNING 7013
#define IDS_I3S_INVALID_EXTENT          7014
#define IDS_I3S_DUPLICATE_ATTRIBUTE_KEY 7015
#define IDS_I3S_DUPLICATE_ATTRIBUTE_NAME 7016
#define IDS_I3S_MISSING_ATTRIBUTE_STATS_DECL 7017
#define IDS_I3S_STATS_DECL_UNKNOWN_ATTRIBUTE 7018
#define IDS_I3S_MISSING_ATTRIBUTE_STORAGE_DECL 7019
#define IDS_I3S_INVALID_COMPRESSED_GEOMETRY_INDEX 7020
#define IDS_I3S_INVALID_UNCOMPRESSED_GEOMETRY_INDEX 7021
#define IDS_I3S_INVALID_VERTEX_COUNT_IN_BUFFER 7022
#define IDS_I3S_MISSING_ATTRIBUTE_SET_DECL 7023
#define IDS_I3S_UNEXPECTED_ATTRIBUTE_IN_COMPRESSED_GEOMETRY 7024
#define IDS_I3S_MISSING_ATTRIBUTE_IN_COMPRESSED_GEOMETRY 7025
#define IDS_I3S_INVALID_FEATURE_COUNT_IN_BUFFER 7026
#define IDS_I3S_MISSING_TEXEL_COUNT_ESTIMATE 7027
#define IDS_I3S_IMAGE_ENCODING_ERROR    7028
#define IDS_I3S_IMAGE_DECODING_ERROR    7029
#define IDS_I3S_UNEXPECTED_ALPHA_CHANNEL 7030
#define IDS_I3S_MISSING_ALPHA_CHANNEL   7031
#define IDS_I3S_MISSING_RESOURCE        7032
#define IDS_I3S_INVALID_BYTE_ALIGNMENT  7033
#define IDS_I3S_INVALID_BINARY_BUFFER_SIZE 7034
#define IDS_I3S_CONNECTION_ERROR        7035
#define IDS_I3S_MBS_OBB_CENTER_MISMATCH 7036
#define IDS_I3S_INVALID_RADIUS_MBS      7037
#define IDS_I3S_INVALID_STRING_LENGTH_IN_ATTRIBUTE_BUFFER 7038
#define IDS_I3S_INVALID_FEATURE_FACE_RANGE                7039
#define IDS_I3S_BAD_UV                                    7040
#define IDS_I3S_MISSING_PROPERTY_COMPATIBILITY_WARNING    7041
#define IDS_I3S_COORDINATE_SYSTEM_COMPATIBILITY_WARNING   7042
#define IDS_I3S_IMAGE_TOO_LARGE                           7043
#define IDS_I3S_LEPCC_DECODE_ERROR                        7044
#define IDS_I3S_DRACO_DECODE_ERROR                        7045
#define IDS_I3S_ATTRIBUTE_VALUE_COUNT_MISMATCH            7046
#define IDS_I3S_WRONG_CHILD_LINK                          7047
#define IDS_I3S_TEX_ATLAS_VERTEX_REGION_MISMATCH          7048
#define IDS_I3S_LEGACY_TEX_ATLAS_FLAG_MISMATCH            7049
#define IDS_I3S_MISSING_VERSION                           7050

#define IDS_I3S_OK                      8000
#define IDS_I3S_IO_OPEN_FAILED          8004
#define IDS_I3S_IO_WRITE_FAILED         8007
#define IDS_I3S_INTERNAL_ERROR          8009
#define IDS_I3S_COMPRESSION_ERROR       8032
#define IDS_I3S_DEGENERATED_MESH        8090
