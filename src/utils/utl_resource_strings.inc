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

{ IDS_I3S_OK, u8"Success" },
{ IDS_I3S_IO_OPEN_FAILED, u8"Failed to open File %1" },
{ IDS_I3S_IO_WRITE_FAILED, u8"Failed to write to File %1" },
{ IDS_I3S_INTERNAL_ERROR, u8"Unexpected Error  %1" },
{ IDS_I3S_COMPRESSION_ERROR, u8"Failed to compress data %1 using method [%2]" },
{ IDS_I3S_DEGENERATED_MESH, u8"Data %1 contains only degenerated faces" },
{ IDS_I3S_VISITING_NODE, u8"Node: %1" },
{ IDS_I3S_OUT_OF_RANGE_ID, u8"\"%1\" = %2 out-of- range  [0, %3]" },
{ IDS_I3S_TYPE_MISMATCH, u8"Unexpected %1 type (%2). Expected %3 " },
{ IDS_I3S_MISSING_JPG_OR_PNG, u8"Jpeg (or png) encoded version of the texture for node %1 must be provided when a GPU compressed version of the texture is provided" },
{ IDS_I3S_INVALID_TREE_TOPOLOGY, u8"Invalid parent-child relationship for node %1. Node must have single parent (except for the root node which has no parent)" },
{ IDS_I3S_EMPTY_LEAF_NODE, u8"Leaf nodes must have geometry (node: %1 )" },
{ IDS_I3S_UNCONNECTED_NODE, u8"%1 node(s) have been created without being connected to the tree" },
{ IDS_I3S_MISSING_ATTRIBUTE_STATS, u8"Missing statistics for field \"%1\"" },
{ IDS_I3S_GEOMETRY_COMPRESSION_RATIO, u8"Draco compression ratio: %1" },
{ IDS_I3S_JSON_PARSING_ERROR, u8" JSON parsing error in \"%1\":  %2" },
{ IDS_I3S_INVALID_LOD_METRIC, u8"Invalid or missing LOD metric in node %1" },
{ IDS_I3S_EXPECTS, u8"Expected value \"%2\" for %1. Got \"%3\"" },
{ IDS_I3S_UNSUPPORTED_VERSION, u8"I3S version \"%1\" is not supported (expected %2 ≤ version < %3)" },
{ IDS_I3S_PATH_COMPATIBILITY_WARNING, u8"URL '%2' for resource %1 may not be supported by Esri products. (Compatible alternative would be: '%3' )" },
{ IDS_I3S_INVALID_EXTENT, u8"Extent %1 is invalid" },
{ IDS_I3S_DUPLICATE_ATTRIBUTE_KEY, u8"Attribute key '%1' must be unique per layer" },
{ IDS_I3S_DUPLICATE_ATTRIBUTE_NAME, u8"Attribute name '%1' must be unique per layer" },
{ IDS_I3S_MISSING_ATTRIBUTE_STATS_DECL, u8"Attribute '%1' is missing statistics resource declaration (i.e. layer.statisticsInfo)." },
{ IDS_I3S_STATS_DECL_UNKNOWN_ATTRIBUTE, u8"layer.statisticsInfo references an unknown attribute key and/or attribute name. Key:'%1', name:'%2'" },
{ IDS_I3S_MISSING_ATTRIBUTE_STORAGE_DECL, u8"Field '%1' is missing corresponding layer.attributeStorageInfo declaration" },
{ IDS_I3S_INVALID_COMPRESSED_GEOMETRY_INDEX, u8"Compressed geometry buffer must be at index %1 (declared: %2)" },
{ IDS_I3S_INVALID_UNCOMPRESSED_GEOMETRY_INDEX, u8"Uncompressed geometry buffer must be at index %1 (declared: %2)" },
{ IDS_I3S_INVALID_VERTEX_COUNT_IN_BUFFER, u8"Geometry buffer %1 has an invalid number of vertices (%2). Vertex count must be a a multiple of 3." },
{ IDS_I3S_MISSING_ATTRIBUTE_SET_DECL, u8"AttributeSetDefinition is missing for attribute with key = %1" },
{ IDS_I3S_UNEXPECTED_ATTRIBUTE_IN_COMPRESSED_GEOMETRY, u8"Attribute(s) %2 in compressed geometry buffer %1  not listed in the corresponding compressedAttributes.attributes declaration" },
{ IDS_I3S_MISSING_ATTRIBUTE_IN_COMPRESSED_GEOMETRY, u8"Missing declared attribute(s) %2 in compressed geometry buffer %1  " },
{ IDS_I3S_INVALID_FEATURE_COUNT_IN_BUFFER, u8"Failed to read feature information from geometry buffer %1. Geometry buffer appears invalid or corrupted.  (feature count: %2)" },
{ IDS_I3S_MISSING_TEXEL_COUNT_ESTIMATE, u8"Node index=%1 has a textured material but texelCountHint is missing." },
{ IDS_I3S_IMAGE_ENCODING_ERROR, u8"Failed to encode image %1 using method %2" },
{ IDS_I3S_IMAGE_DECODING_ERROR, u8"Failed to decode image %1 using method %2" },
{ IDS_I3S_UNEXPECTED_ALPHA_CHANNEL, u8"Texture %1 has alpha-channel, but associated material is opaque. Alpha will be dropped. (performance warning)" },
{ IDS_I3S_MISSING_ALPHA_CHANNEL, u8"Opaque texture %1 is referenced by a transparent material. (performance warning)" },
{ IDS_I3S_MISSING_RESOURCE, u8"Referenced resource %1 is missing" },
{ IDS_I3S_INVALID_BYTE_ALIGNMENT, u8"Binary array %1 (value-type=%2) is not properly aligned." },
{ IDS_I3S_INVALID_BINARY_BUFFER_SIZE, u8"Binary buffer %1 has an invalid size ( expects %3 bytes, got %2 )" },
{ IDS_I3S_CONNECTION_ERROR, u8"Loading resource %1 failed with error:%2" },
{ IDS_I3S_MBS_OBB_CENTER_MISMATCH, u8"Oriented-Bounding Box (OBB) and Minimum Bounding Sphere (MBS) centers mismatch for node %1. MBS center will be used as mesh origin" },
{ IDS_I3S_INVALID_RADIUS_MBS, u8"Mimimum bounding sphere radius for node %1 must be greater than 0.0 (got: %2) " },
{ IDS_I3S_INVALID_STRING_LENGTH_IN_ATTRIBUTE_BUFFER, u8"Length %3 for string index=%2 in attribute buffer %1 in out-of-bound" },
{ IDS_I3S_INVALID_FEATURE_FACE_RANGE, u8"%2 out of %3 feature face-range indices are out-of-range in geometry buffer %1" },
{ IDS_I3S_BAD_UV, u8"Geometry in node %1 has %2 invalid texture coordinate(s). " },
{ IDS_I3S_MISSING_PROPERTY_COMPATIBILITY_WARNING, u8"%1 may be required by Esri products" },
{ IDS_I3S_COORDINATE_SYSTEM_COMPATIBILITY_WARNING, u8"layer.store.normalReferenceFrame '%1' with a %2 coordinate system may not be compatible with Esri Products. (Compatible alternative would be '%3')" },
{ IDS_I3S_IMAGE_TOO_LARGE, u8"Texture in node %1 is too large. Max dimension allowed is %2. Found %3" },
{ IDS_I3S_LEPCC_DECODE_ERROR, u8"Failed to decode LEPCC buffer %1 ( Error code: %2 )" },
{ IDS_I3S_DRACO_DECODE_ERROR, u8"Failed to decode DRACO buffer %1 ( Error code: %2 )" },
{ IDS_I3S_ATTRIBUTE_VALUE_COUNT_MISMATCH, u8"%2 value(s) found in attribute buffer %1 (expected : %3 )" },
{ IDS_I3S_TEX_ATLAS_VERTEX_REGION_MISMATCH, u8"(%1): sharedResources property texture.atlas (=%2) does not match property material.use_vertex_region (=%3)" },
{ IDS_I3S_LEGACY_TEX_ATLAS_FLAG_MISMATCH, u8"(%1): Legacy sharedResources textureDefinitionInfo.atlas: %2 mismatches with textureSetDefinition.atlas: %3" },
{ IDS_I3S_MISSING_VERSION, u8"Missing version" },
