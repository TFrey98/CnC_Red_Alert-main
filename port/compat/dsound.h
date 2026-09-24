/*
**	dsound.h -- DirectSound compatibility shim for the native macOS port.
**
**	The engine's use of DirectSound is narrow: one primary buffer, a pool of
**	secondary buffers it locks and fills with PCM, plus play/stop/volume and a
**	"buffer lost" retry path that Core Audio has no equivalent for. The
**	implementation in port/compat/dsound_sdl.cpp is backed by SDL2's audio
**	callback and never reports a lost buffer.
*/
#ifndef WWPORT_COMPAT_DSOUND_H
#define WWPORT_COMPAT_DSOUND_H

#include "windows.h"
#include "mmsystem.h"

#define DS_OK                       ((HRESULT)0)
#define DSERR_ALLOCATED             ((HRESULT)0x8878000A)
#define DSERR_INVALIDPARAM          ((HRESULT)0x80070057)
#define DSERR_OUTOFMEMORY           ((HRESULT)0x8007000E)
#define DSERR_BUFFERLOST            ((HRESULT)0x88780096)
#define DSERR_PRIOLEVELNEEDED       ((HRESULT)0x88780046)
#define DSERR_NODRIVER              ((HRESULT)0x88780078)

/* Buffer capability flags. */
#define DSBCAPS_PRIMARYBUFFER       0x00000001
#define DSBCAPS_STATIC              0x00000002
#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_CTRLFREQUENCY       0x00000020
#define DSBCAPS_CTRLPAN             0x00000040
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_LOCSOFTWARE         0x00000008

/* Play flags. */
#define DSBPLAY_LOOPING             0x00000001

/* Status flags. */
#define DSBSTATUS_PLAYING           0x00000001
#define DSBSTATUS_BUFFERLOST        0x00000002
#define DSBSTATUS_LOOPING           0x00000004

/* Cooperative levels. */
#define DSSCL_NORMAL                0x00000001
#define DSSCL_PRIORITY              0x00000002
#define DSSCL_EXCLUSIVE             0x00000003
#define DSSCL_WRITEPRIMARY          0x00000004

/* Volume range, in hundredths of a decibel. */
#define DSBVOLUME_MIN               (-10000)
#define DSBVOLUME_MAX               0

struct IDirectSound;
struct IDirectSoundBuffer;

typedef struct IDirectSound *       LPDIRECTSOUND;
typedef struct IDirectSoundBuffer * LPDIRECTSOUNDBUFFER;

typedef struct _DSBUFFERDESC {
	DWORD          dwSize;
	DWORD          dwFlags;
	DWORD          dwBufferBytes;
	DWORD          dwReserved;
	LPWAVEFORMATEX lpwfxFormat;
} DSBUFFERDESC, *LPDSBUFFERDESC;

typedef struct _DSCAPS {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwMinSecondarySampleRate;
	DWORD dwMaxSecondarySampleRate;
	DWORD dwPrimaryBuffers;
} DSCAPS, *LPDSCAPS;

struct IDirectSoundBuffer {
	virtual HRESULT Lock(DWORD offset, DWORD bytes, LPVOID * ptr1, LPDWORD bytes1,
	                     LPVOID * ptr2, LPDWORD bytes2, DWORD flags) = 0;
	virtual HRESULT Unlock(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2) = 0;
	virtual HRESULT Play(DWORD reserved1, DWORD priority, DWORD flags) = 0;
	virtual HRESULT Stop() = 0;
	virtual HRESULT GetStatus(LPDWORD status) = 0;
	virtual HRESULT GetCurrentPosition(LPDWORD play, LPDWORD write) = 0;
	virtual HRESULT SetCurrentPosition(DWORD position) = 0;
	virtual HRESULT SetVolume(LONG volume) = 0;
	virtual HRESULT GetVolume(LPLONG volume) = 0;
	virtual HRESULT SetPan(LONG pan) = 0;
	virtual HRESULT SetFormat(LPWAVEFORMATEX format) = 0;
	virtual HRESULT Restore() = 0;
	virtual ULONG   Release() = 0;
protected:
	~IDirectSoundBuffer() {}
};

struct IDirectSound {
	virtual HRESULT CreateSoundBuffer(LPDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER * buffer, void * outer) = 0;
	virtual HRESULT SetCooperativeLevel(HWND window, DWORD level) = 0;
	virtual HRESULT GetCaps(LPDSCAPS caps) = 0;
	virtual ULONG   Release() = 0;
protected:
	~IDirectSound() {}
};

extern "C" HRESULT DirectSoundCreate(void * guid, LPDIRECTSOUND * ds, void * outer);

#endif /* WWPORT_COMPAT_DSOUND_H */
