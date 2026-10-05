/*
**	win32_dsound.cpp -- DirectSound, as the engine uses it: a software mixer
**	over the backend's CoreAudio output (ra_platform.h, RA_Audio_Start).
**
**	Engine side: built with the engine's flags. No CoreAudio here -- the
**	backend just asks for stereo frames, and this file makes them.
**
**	How the game uses DirectSound (WIN32LIB/AUDIO/SOUNDIO.CPP, SOUNDINT.CPP;
**	WINVQ/VQA32/AUDIO.CPP for movies): each sound gets a secondary buffer,
**	played LOOPING, which a timer callback keeps filled a quarter-buffer ahead
**	of the play cursor (GetCurrentPosition, Lock, write, Unlock) and stops when
**	the sound ends. The primary buffer only carries the output format. So:
**
**	  * A buffer is a circular byte array in its own format (8-bit unsigned or
**	    16-bit signed, mono or stereo, any rate). Lock returns the one or two
**	    pieces of the requested range, wrapping, as DirectSound did; the game
**	    writes into them directly.
**	  * The mixer runs on CoreAudio's thread: every playing buffer is resampled
**	    (linear interpolation) to the device rate, scaled by its volume, and
**	    summed. Its play position advances by what was consumed, which is what
**	    GetCurrentPosition reports; the write cursor is 10 ms ahead of it.
**	  * Looping buffers wrap; others stop at their end.
**	  * Buffers are never "lost" (that was a Windows focus artefact).
*/

#include "windows.h"
#include "dsound.h"
#include "ra_platform.h"

#include <math.h>
#include <mutex>
#include <string.h>
#include <vector>

void WWPort_DSound_Mix(float * out, int frames);

namespace {

struct Sound;
extern Sound * TheSound;

std::mutex Mix;					// the buffer list and every buffer's play state

struct Buffer;
std::vector<Buffer *> Buffers;
int DeviceRate = 0;
float MasterGain = 1.0f;

inline float gain_of(LONG centibels)
{
	if (centibels <= DSBVOLUME_MIN) return 0.0f;
	if (centibels >= 0) return 1.0f;
	return (float)pow(10.0, centibels / 2000.0);
}

struct Buffer : IDirectSoundBuffer {
	std::vector<unsigned char>	Data;
	WAVEFORMATEX					Format;
	bool								Primary;
	bool								Playing;
	bool								Looping;
	double							Position;		// in frames
	LONG								Volume;
	float								Gain;
	ULONG								Refs;

	Buffer(DWORD bytes, LPWAVEFORMATEX fmt, bool primary) : Data(bytes, 0), Primary(primary), Playing(false),
		Looping(false), Position(0), Volume(0), Gain(1.0f), Refs(1)
	{
		memset(&Format, 0, sizeof(Format));
		if (fmt) Format = *fmt;
		if (Format.nChannels == 0) Format.nChannels = 1;
		if (Format.wBitsPerSample == 0) Format.wBitsPerSample = 8;
		if (Format.nSamplesPerSec == 0) Format.nSamplesPerSec = 22050;
		Format.nBlockAlign = (WORD)(Format.nChannels * Format.wBitsPerSample / 8);
	}

	DWORD frames(void) const {return Format.nBlockAlign ? (DWORD)(Data.size() / Format.nBlockAlign) : 0;}

	/* One channel of one frame, -1..1. */
	float sample(DWORD frame, int channel) const
	{
		size_t at = (size_t)frame * Format.nBlockAlign + (size_t)channel * (Format.wBitsPerSample / 8);
		if (Format.wBitsPerSample == 16) {
			int16_t v = (int16_t)(Data[at] | (Data[at + 1] << 8));
			return v / 32768.0f;
		}
		return ((int)Data[at] - 128) / 128.0f;
	}

	/* Add this buffer's next `n` output frames into `out`, at device rate `rate`. */
	void mix_into(float * out, int n, int rate)
	{
		DWORD const total = frames();
		if (!Playing || total == 0 || Gain == 0.0f) {
			if (Playing && total) advance(n, rate);
			return;
		}
		double const step = (double)Format.nSamplesPerSec / rate;
		bool const stereo = Format.nChannels >= 2;
		for (int i = 0; i < n; i++) {
			DWORD f0 = (DWORD)Position;
			double frac = Position - f0;
			DWORD f1 = f0 + 1;
			if (f1 >= total) f1 = Looping ? 0 : f0;
			float l0 = sample(f0, 0), l1 = sample(f1, 0);
			float r0 = stereo ? sample(f0, 1) : l0, r1 = stereo ? sample(f1, 1) : l1;
			out[i * 2 + 0] += (float)(l0 + (l1 - l0) * frac) * Gain;
			out[i * 2 + 1] += (float)(r0 + (r1 - r0) * frac) * Gain;
			Position += step;
			if (Position >= total) {
				if (Looping) {
					Position -= total;
				} else {
					Position = total;
					Playing = false;
					return;
				}
			}
		}
	}

	/* Silent buffers still move on, so the game's cursor arithmetic holds. */
	void advance(int n, int rate)
	{
		DWORD const total = frames();
		Position += (double)Format.nSamplesPerSec / rate * n;
		if (Position >= total) {
			if (Looping) Position = fmod(Position, (double)total);
			else {Position = total; Playing = false;}
		}
	}

	HRESULT Lock(DWORD offset, DWORD bytes, LPVOID * p1, LPDWORD b1, LPVOID * p2, LPDWORD b2, DWORD flags)
	{
		(void)flags;
		DWORD const size = (DWORD)Data.size();
		if (p1 == NULL || b1 == NULL || size == 0 || offset >= size || bytes > size) return DSERR_INVALIDPARAM;
		*p1 = &Data[offset];
		*b1 = (offset + bytes <= size) ? bytes : size - offset;
		DWORD rest = bytes - *b1;
		if (p2) *p2 = rest ? &Data[0] : NULL;
		if (b2) *b2 = rest;
		return DS_OK;
	}
	HRESULT Unlock(LPVOID p1, DWORD b1, LPVOID p2, DWORD b2) {(void)p1; (void)b1; (void)p2; (void)b2; return DS_OK;}

	HRESULT Play(DWORD reserved, DWORD priority, DWORD flags)
	{
		(void)reserved; (void)priority;
		std::lock_guard<std::mutex> g(Mix);
		if (Primary) return DS_OK;
		if (Position >= frames()) Position = 0;
		Looping = (flags & DSBPLAY_LOOPING) != 0;
		Playing = true;
		return DS_OK;
	}
	HRESULT Stop()
	{
		std::lock_guard<std::mutex> g(Mix);
		Playing = false;
		return DS_OK;
	}
	HRESULT GetStatus(LPDWORD status)
	{
		if (status == NULL) return DSERR_INVALIDPARAM;
		std::lock_guard<std::mutex> g(Mix);
		*status = (Playing || Primary) ? (DSBSTATUS_PLAYING | (Looping ? DSBSTATUS_LOOPING : 0)) : 0;
		return DS_OK;
	}
	HRESULT GetCurrentPosition(LPDWORD play, LPDWORD write)
	{
		std::lock_guard<std::mutex> g(Mix);
		DWORD const size = (DWORD)Data.size();
		DWORD p = (DWORD)Position * Format.nBlockAlign;
		if (size) p %= size;
		if (play) *play = p;
		if (write) {
			DWORD lead = (Format.nSamplesPerSec / 100) * Format.nBlockAlign;
			*write = size ? (p + lead) % size : 0;
		}
		return DS_OK;
	}
	HRESULT SetCurrentPosition(DWORD position)
	{
		std::lock_guard<std::mutex> g(Mix);
		if (Format.nBlockAlign == 0 || position >= Data.size()) return DSERR_INVALIDPARAM;
		Position = position / Format.nBlockAlign;
		return DS_OK;
	}
	HRESULT SetVolume(LONG volume)
	{
		if (volume < DSBVOLUME_MIN || volume > DSBVOLUME_MAX) return DSERR_INVALIDPARAM;
		std::lock_guard<std::mutex> g(Mix);
		Volume = volume;
		if (Primary) MasterGain = gain_of(volume);
		else Gain = gain_of(volume);
		return DS_OK;
	}
	HRESULT GetVolume(LPLONG volume)
	{
		if (volume == NULL) return DSERR_INVALIDPARAM;
		*volume = Volume;
		return DS_OK;
	}
	HRESULT SetPan(LONG pan) {(void)pan; return DS_OK;}

	/* The primary's format is the output format; the device rate is the mixer's. */
	HRESULT SetFormat(LPWAVEFORMATEX format)
	{
		if (format == NULL) return DSERR_INVALIDPARAM;
		std::lock_guard<std::mutex> g(Mix);
		Format = *format;
		return DS_OK;
	}
	HRESULT Restore() {return DS_OK;}

	ULONG Release()
	{
		std::lock_guard<std::mutex> g(Mix);
		ULONG r = --Refs;
		if (r == 0) {
			for (size_t i = 0; i < Buffers.size(); i++) {
				if (Buffers[i] == this) {Buffers.erase(Buffers.begin() + i); break;}
			}
			delete this;
		}
		return r;
	}
};

struct Sound : IDirectSound {
	ULONG Refs;
	Sound() : Refs(1) {}

	HRESULT CreateSoundBuffer(LPDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER * out, void * outer)
	{
		(void)outer;
		if (desc == NULL || out == NULL) return DSERR_INVALIDPARAM;
		bool primary = (desc->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
		if (!primary && desc->dwBufferBytes == 0) return DSERR_INVALIDPARAM;
		Buffer * b = new Buffer(primary ? 0 : desc->dwBufferBytes, desc->lpwfxFormat, primary);
		std::lock_guard<std::mutex> g(Mix);
		Buffers.push_back(b);
		*out = b;
		return DS_OK;
	}
	HRESULT SetCooperativeLevel(HWND window, DWORD level) {(void)window; (void)level; return DS_OK;}
	HRESULT GetCaps(LPDSCAPS caps)
	{
		if (caps == NULL) return DSERR_INVALIDPARAM;
		DWORD size = caps->dwSize;
		memset(caps, 0, sizeof(*caps));
		caps->dwSize = size;
		caps->dwMinSecondarySampleRate = 4000;
		caps->dwMaxSecondarySampleRate = 96000;
		caps->dwPrimaryBuffers = 1;
		return DS_OK;
	}
	ULONG Release()
	{
		ULONG r = --Refs;
		if (r == 0) {
			RA_Audio_Stop();
			TheSound = NULL;
			delete this;
		}
		return r;
	}
};

Sound * TheSound = NULL;

void render(float * out, int frames, void * user)
{
	(void)user;
	WWPort_DSound_Mix(out, frames);
}

}

/*
**	Fill `frames` frames of interleaved stereo at the device rate. CoreAudio's
**	thread calls this through the backend; the tests call it directly.
*/
void WWPort_DSound_Mix(float * out, int frames)
{
	memset(out, 0, (size_t)frames * 2 * sizeof(float));
	std::lock_guard<std::mutex> g(Mix);
	if (DeviceRate <= 0) return;
	for (Buffer * b : Buffers) {
		if (!b->Primary) b->mix_into(out, frames, DeviceRate);
	}
	for (int i = 0; i < frames * 2; i++) {
		float v = out[i] * MasterGain;
		out[i] = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
	}
}

/*
**	No output device (none present, or it would not start) is reported the
**	way Windows reported a machine without a sound card; the game then runs
**	silently.
*/
extern "C" HRESULT DirectSoundCreate(void * guid, LPDIRECTSOUND * ds, void * outer)
{
	(void)guid; (void)outer;
	if (ds == NULL) return DSERR_INVALIDPARAM;
	if (TheSound != NULL) {
		TheSound->Refs++;
		*ds = TheSound;
		return DS_OK;
	}
	int rate = 0;
	if (!RA_Audio_Start(render, NULL, &rate) || rate <= 0) {
		*ds = NULL;
		return DSERR_NODRIVER;
	}
	DeviceRate = rate;
	TheSound = new Sound;
	*ds = TheSound;
	return DS_OK;
}
