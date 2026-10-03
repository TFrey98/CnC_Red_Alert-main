/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WAVEFILE_H
#define WAVEFILE_H
/****************************************************************************
*
*        C O N F I D E N T I A L --- W E S T W O O D   S T U D I O S
*
*----------------------------------------------------------------------------
*
* FILE
*     wavefile.c
*
* DESCRIPTION
*     WAVE file format definitions.
*
* PROGRAMMER
*     Denzil E. Long, Jr.
*      
* DATE
*     February 21, 1995
*
****************************************************************************/

/* WAVHeader: WAVE file header.
 *
 * RIFF - 4 byte identifier (always "RIFF").
 * Size - Size in bytes of entire file.
 * WAVE - 4 byte identifier (always "WAVE").
 */
/*
**	Port: ON-DISK LAYOUT (RIFF/WAVE file header). `long` was 32 bits under Watcom and this
**	struct was packed by the `#ifdef __WATCOMC__ #pragma pack(1)` that clang never
**	saw. Fixed-width fields and a scoped pack restore the shipped layout.
*/
#pragma pack(push, 1)
typedef struct _WAVHeader {
	int32_t RIFF;
	int32_t Size;
	int32_t WAVE;
} WAVHeader;
#pragma pack(pop)
#ifdef __cplusplus
static_assert(sizeof(WAVHeader) == 12, "WAVHeader must match the original 12-byte on-disk layout");
#endif

/* WAVFormat: This structure describes the format of the audio data contained
 *            in the WAV file.
 *
 * FormatTag      - 
 * Channels       - Number of channels (1 = mono, 2 = stereo).
 * SamplesPerSec  - Sampling rate.
 * AvgBytesPerSec - Bytes in 1 second of audio (Rate * SampleSize * Channels)
 * BlockAlign     - padding.
 * BitsPerSample  - Number of bits per sample (8, 16).
 */
/*
**	Port: ON-DISK LAYOUT (WAVE fmt chunk). `long` was 32 bits under Watcom and this
**	struct was packed by the `#ifdef __WATCOMC__ #pragma pack(1)` that clang never
**	saw. Fixed-width fields and a scoped pack restore the shipped layout.
*/
#pragma pack(push, 1)
typedef struct _WAVFormat {
	short FormatTag;
	short Channels;
	int32_t  SamplesPerSec;
 	int32_t  AvgBytesPerSec;
	short BlockAlign;
	short BitsPerSample;
} WAVFormat;
#pragma pack(pop)
#ifdef __cplusplus
static_assert(sizeof(WAVFormat) == 16, "WAVFormat must match the original 16-byte on-disk layout");
#endif

#endif /* WAVEFILE_H */

