/*
** dsound_mixer.cpp -- the DirectSound emulation (port/compat/win32_dsound.cpp)
** driven directly, without an audio device (fake_platform.cpp's RA_Audio_Start
** reports a rate and never calls back; the test calls the mixer itself).
**
** Checked against values computed here independently: format conversion,
** resampling, volume, Lock's wrap-around, looping and stopping -- and the
** game's own streaming pattern (SOUNDINT.CPP's maintenance_callback: refill a
** quarter-buffer when the play cursor is within a quarter of the write point),
** which must come out as the source stream with no gap or repeat.
*/
#include "windows.h"
#include "dsound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

void WWPort_DSound_Mix(float * out, int frames);

static int failures = 0;
static void check(bool ok, char const * what)
{
	if (!ok) {printf("  FAIL: %s\n", what); failures++;}
}

static LPDIRECTSOUNDBUFFER make(LPDIRECTSOUND ds, int rate, int bits, int channels, DWORD bytes)
{
	WAVEFORMATEX f;
	memset(&f, 0, sizeof(f));
	f.wFormatTag = WAVE_FORMAT_PCM;
	f.nChannels = (WORD)channels;
	f.nSamplesPerSec = (DWORD)rate;
	f.wBitsPerSample = (WORD)bits;
	f.nBlockAlign = (WORD)(channels * bits / 8);
	f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
	DSBUFFERDESC d;
	memset(&d, 0, sizeof(d));
	d.dwSize = sizeof(d);
	d.dwFlags = DSBCAPS_CTRLVOLUME;
	d.dwBufferBytes = bytes;
	d.lpwfxFormat = &f;
	LPDIRECTSOUNDBUFFER b = NULL;
	check(ds->CreateSoundBuffer(&d, &b, NULL) == DS_OK && b, "CreateSoundBuffer");
	return b;
}

static void fill16(LPDIRECTSOUNDBUFFER b, std::vector<int16_t> const & v)
{
	void * p1; void * p2; DWORD n1, n2;
	check(b->Lock(0, (DWORD)(v.size() * 2), &p1, &n1, &p2, &n2, 0) == DS_OK, "Lock");
	memcpy(p1, v.data(), n1);
	b->Unlock(p1, n1, p2, n2);
}

int main()
{
	setenv("RA_TEST_AUDIO_RATE", "22050", 1);
	LPDIRECTSOUND ds = NULL;
	check(DirectSoundCreate(NULL, &ds, NULL) == DS_OK && ds, "DirectSoundCreate");

	/* 1. 16-bit mono at the device rate: samples come out exactly, on both channels. */
	{
		std::vector<int16_t> v(1000);
		for (size_t i = 0; i < v.size(); i++) v[i] = (int16_t)((i * 37) % 20000 - 10000);
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 16, 1, 2000);
		fill16(b, v);
		b->Play(0, 0, DSBPLAY_LOOPING);
		std::vector<float> out(300 * 2);
		WWPort_DSound_Mix(out.data(), 300);
		bool same = true;
		for (int i = 0; i < 300; i++) {
			float want = v[i] / 32768.0f;
			if (fabsf(out[i * 2] - want) > 1e-6f || fabsf(out[i * 2 + 1] - want) > 1e-6f) same = false;
		}
		check(same, "16-bit mono samples, unresampled");
		DWORD play, write;
		b->GetCurrentPosition(&play, &write);
		check(play == 600, "play cursor advances by what was consumed (300 frames = 600 bytes)");
		check(write == (600 + 220 * 2) % 2000, "write cursor 10 ms ahead");
		b->Release();
	}

	/* 2. 11025 Hz into 22050: every other output is the midpoint (linear interpolation). */
	{
		std::vector<int16_t> v(100);
		for (size_t i = 0; i < v.size(); i++) v[i] = (int16_t)(i * 300);
		LPDIRECTSOUNDBUFFER b = make(ds, 11025, 16, 1, 200);
		fill16(b, v);
		b->Play(0, 0, 0);
		std::vector<float> out(100 * 2);
		WWPort_DSound_Mix(out.data(), 100);
		bool same = true;
		for (int i = 0; i < 98; i++) {
			float want = (i % 2) ? (v[i / 2] + v[i / 2 + 1]) / 2.0f / 32768.0f : v[i / 2] / 32768.0f;
			if (fabsf(out[i * 2] - want) > 1e-5f) same = false;
		}
		check(same, "resampling 11025 -> 22050 interpolates linearly");
		b->Release();
	}

	/* 3. 8-bit unsigned stereo: 128 is silence; left and right stay apart. */
	{
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 8, 2, 64);
		void * p1; void * p2; DWORD n1, n2;
		b->Lock(0, 64, &p1, &n1, &p2, &n2, 0);
		unsigned char * u = (unsigned char *)p1;
		for (int i = 0; i < 32; i++) {u[i * 2] = 192; u[i * 2 + 1] = 64;}
		b->Unlock(p1, n1, p2, n2);
		b->Play(0, 0, DSBPLAY_LOOPING);
		float out[8];
		WWPort_DSound_Mix(out, 4);
		check(fabsf(out[0] - 0.5f) < 1e-6f && fabsf(out[1] + 0.5f) < 1e-6f, "8-bit unsigned stereo");
		b->Release();
	}

	/* 4. Volume in hundredths of a dB: -2000 is a tenth; -10000 is silence. */
	{
		std::vector<int16_t> v(64, 16384);
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 16, 1, 128);
		fill16(b, v);
		b->SetVolume(-2000);
		b->Play(0, 0, DSBPLAY_LOOPING);
		float out[8];
		WWPort_DSound_Mix(out, 4);
		check(fabsf(out[0] - 0.05f) < 1e-5f, "SetVolume(-2000) = x0.1");
		b->SetVolume(DSBVOLUME_MIN);
		WWPort_DSound_Mix(out, 4);
		check(out[0] == 0.0f, "SetVolume(-10000) = silence");
		b->Release();
	}

	/* 5. Lock across the end splits into two pieces, as DirectSound did. */
	{
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 16, 1, 1000);
		void * p1; void * p2; DWORD n1, n2;
		check(b->Lock(800, 500, &p1, &n1, &p2, &n2, 0) == DS_OK && n1 == 200 && n2 == 300 && p2 != NULL,
			"Lock(800, 500) on 1000 bytes -> 200 + 300");
		b->Release();
	}

	/* 6. A non-looping buffer stops at its end. */
	{
		std::vector<int16_t> v(50, 1000);
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 16, 1, 100);
		fill16(b, v);
		b->Play(0, 0, 0);
		float out[200];
		WWPort_DSound_Mix(out, 100);
		DWORD status = 99;
		b->GetStatus(&status);
		check(!(status & DSBSTATUS_PLAYING), "one-shot buffer stops at its end");
		check(out[2 * 60] == 0.0f, "and is silent after it");
		b->Release();
	}

	/*
	** 7. The game's streaming pattern. A 32 KB looping buffer is primed with half
	**    the sound; then, between mixer runs, SOUNDINT.CPP's rule refills a
	**    quarter-buffer whenever the play cursor is within a quarter of the
	**    write point. The output must be exactly the source.
	*/
	{
		int const SIZE = 32768;
		std::vector<int16_t> src(200000);
		for (size_t i = 0; i < src.size(); i++) src[i] = (int16_t)(sin(i * 0.013) * 20000 + (i % 7));
		LPDIRECTSOUNDBUFFER b = make(ds, 22050, 16, 1, SIZE);
		size_t fed = 0;
		DWORD dest = 0;
		auto feed = [&](void) {
			void * p1; void * p2; DWORD n1, n2;
			b->Lock(dest, SIZE / 2, &p1, &n1, &p2, &n2, 0);
			size_t samples = SIZE / 4 / 2;
			std::vector<int16_t> chunk(samples, 0);
			for (size_t i = 0; i < samples && fed + i < src.size(); i++) chunk[i] = src[fed + i];
			memcpy(p1, chunk.data(), SIZE / 4);
			fed += samples;
			dest = (dest + SIZE / 4) % SIZE;
			b->Unlock(p1, n1, p2, n2);
		};
		feed(); feed();
		b->Play(0, 0, DSBPLAY_LOOPING);
		std::vector<float> got;
		float out[2 * 300];
		while (got.size() < 150000) {
			WWPort_DSound_Mix(out, 300);
			for (int i = 0; i < 300; i++) got.push_back(out[i * 2]);
			DWORD play;
			b->GetCurrentPosition(&play, NULL);
			bool more = (play < dest) ? (dest - play <= SIZE / 4) : ((int)play > SIZE * 3 / 4 && dest == 0);
			if (more) feed();
		}
		bool same = true;
		size_t bad = 0;
		for (size_t i = 0; i < 150000; i++) {
			if (fabsf(got[i] - src[i] / 32768.0f) > 1e-6f) {same = false; bad = i; break;}
		}
		if (!same) printf("  stream differs first at sample %zu\n", bad);
		check(same, "streamed sound comes out exactly, no gap or repeat (150000 samples)");
		b->Release();
	}

	ds->Release();
	printf(failures ? "dsound_mixer: FAIL\n" : "dsound_mixer: all pass\n");
	return failures ? 1 : 0;
}
