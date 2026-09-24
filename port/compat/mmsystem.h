/*
**	mmsystem.h -- the slice of the Windows multimedia API the audio path needs.
**	WAVEFORMATEX is serialized into AUD/WAV headers, so its layout is fixed.
*/
#ifndef WWPORT_COMPAT_MMSYSTEM_H
#define WWPORT_COMPAT_MMSYSTEM_H
#include "windows.h"

#define WAVE_FORMAT_PCM 1

#pragma pack(push, 1)
typedef struct tWAVEFORMATEX {
	WORD  wFormatTag;
	WORD  nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD  nBlockAlign;
	WORD  wBitsPerSample;
	WORD  cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;
#pragma pack(pop)

#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(WAVEFORMATEX) == 18, "WAVEFORMATEX must stay 18 bytes");
#endif
#endif
