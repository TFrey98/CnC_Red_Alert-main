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

/****************************************************************************
*
*  File              : soscomp.h
*  Date Created      : 6/1/94
*  Description       :
*
*  Programmer(s)     : Nick Skrepetos
*  Last Modification : 10/1/94 - 11:37:9 AM
*  Additional Notes  : Modified by Denzil E. Long, Jr.
*
*****************************************************************************
*            Copyright (c) 1994,  HMI, Inc.  All Rights Reserved            *
****************************************************************************/

#ifndef  _SOS_COMPRESS
#define  _SOS_COMPRESS

/* compression types */
enum {
	_ADPCM_TYPE_1,
	};

/* define compression structure */
typedef struct _tagCOMPRESS_INFO {
	char       *lpSource;
	char       *lpDest;
	unsigned long dwCompSize;
	unsigned long dwUnCompSize;
	short         wBitSize;
	short			    wChannels;
	unsigned long dwSampleIndex;
	long          dwPredicted;
	long          dwDifference;
	short         wCodeBuf;
	short         wCode;
	short         wStep;
	short         wIndex;

	unsigned long dwSampleIndex2; //added BP for channel 2
	long          dwPredicted2;   //added BP for channel 2
	long          dwDifference2; 	//added BP for channel 2
	short         wCodeBuf2; 		//added BP for channel 2
	short         wCode2; 			//added BP for channel 2
	short         wStep2; 			//added BP for channel 2
	short         wIndex2; 			//added BP for channel 2
	} _SOS_COMPRESS_INFO;

/* compressed file type header */
/*
**	Port: ON-DISK LAYOUT (SOS compressed-audio header). `long` was 32 bits under Watcom and this
**	struct was packed by the `#ifdef __WATCOMC__ #pragma pack(1)` that clang never
**	saw. Fixed-width fields and a scoped pack restore the shipped layout.
*/
#pragma pack(push, 1)
typedef struct _tagCOMPRESS_HEADER {
	uint32_t dwType;              // type of compression
	uint32_t dwCompressedSize;    // compressed file size
	uint32_t dwUnCompressedSize;  // uncompressed file size
	uint32_t dwSourceBitSize;     // original bit size
	char          szName[16];          // file type, for error checking
	} _SOS_COMPRESS_HEADER;
#pragma pack(pop)
#ifdef __cplusplus
static_assert(sizeof(_SOS_COMPRESS_HEADER) == 32, "_SOS_COMPRESS_HEADER must match the original 32-byte on-disk layout");
#endif

/* Prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void __cdecl VQA_sosCODECInitStream(_SOS_COMPRESS_INFO *);
unsigned long __cdecl VQA_sosCODECCompressData(_SOS_COMPRESS_INFO *,unsigned long);
unsigned long __cdecl VQA_sosCODECDecompressData(_SOS_COMPRESS_INFO *,unsigned long);

#ifdef __cplusplus
}
#endif

#endif

