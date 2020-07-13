/*
	This code is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This code is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.
*/

#pragma once
#include "utils/dxt/utl_dxt_config.h"

namespace DXTC
{
	// DXT compressor (scalar version).
	EXTERN(void) CompressImageDXT1(const BYTE* inBuf, BYTE* outBuf, int width, int height);
	EXTERN(void) CompressImageDXT5(const BYTE* inBuf, BYTE* outBuf, int width, int height, unsigned int rowPitch = 0);
	EXTERN(WORD) ColorTo565(const BYTE* color);
	EXTERN(void) EmitByte(BYTE*& dest, BYTE b);
	EXTERN(void) EmitWord(BYTE*& dest, WORD s);
	EXTERN(void) EmitDoubleWord(BYTE*& dest, DWORD i);
	EXTERN(void) ExtractBlock(const BYTE* inPtr, int width, BYTE* colorBlock);
	EXTERN(void) GetMinMaxColors(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	EXTERN(void) GetMinMaxColorsWithAlpha(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	EXTERN(void) EmitColorIndices(const BYTE* colorBlock, BYTE*& outBuf, const BYTE* minColor, const BYTE* maxColor);
	EXTERN(void) EmitAlphaIndices(const BYTE* colorBlock,  BYTE*& outBuf, const BYTE minAlpha, const BYTE maxAlpha);

	// DXT compressor (SSE2 version).
	EXTERN(void) CompressImageDXT1SSE2(const BYTE* inBuf, BYTE* outBuf, int width, int height);
	EXTERN(void) CompressImageDXT5SSE2(const BYTE* inBuf, BYTE* outBuf, int width, int height, unsigned int rowPitch = 0);
	EXTERN(void) ExtractBlock_SSE2(const BYTE* inPtr, int width, BYTE* colorBlock);
	EXTERN(void) GetMinMaxColors_SSE2(const BYTE* colorBlock, BYTE* minColor, BYTE* maxColor);
	EXTERN(void) EmitColorIndices_SSE2(const BYTE* colorBlock, BYTE*& outBuf, const BYTE* minColor, const BYTE* maxColor);
	EXTERN(void) EmitAlphaIndices_SSE2(const BYTE* colorBlock, BYTE*& outBuf, const BYTE minAlpha, const BYTE maxAlpha);
}
