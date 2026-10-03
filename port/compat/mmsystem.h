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

/*
**	Multimedia timers -- implemented for real in wwcompat.cpp on one serial GCD
**	queue. This is the game clock (WIN32LIB/TIMER/TIMERINI.CPP), the mouse
**	(KEYBOARD/MOUSE.CPP), sound maintenance (AUDIO/SOUNDIO.CPP) and the VQA
**	player's timers. As on Win32, all callbacks run on one thread: they never
**	overlap each other, though they do run concurrently with the main thread.
**	The callback signature is the Win32 one the engine declares; `dwUser` is a
**	DWORD (Win32), and the engine always passes 0.
*/
typedef void (CALLBACK * LPTIMECALLBACK)(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
typedef UINT MMRESULT;
#define TIME_ONESHOT     0x0000
#define TIME_PERIODIC    0x0001
#define TIMERR_NOERROR   0
#define TIMERR_NOCANDO   97
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);
MMRESULT timeSetEvent(UINT delay_ms, UINT resolution_ms, LPTIMECALLBACK callback, DWORD user, UINT flags);
MMRESULT timeKillEvent(UINT id);
DWORD    timeGetTime(void);

#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(WAVEFORMATEX) == 18, "WAVEFORMATEX must stay 18 bytes");
#endif
#endif
