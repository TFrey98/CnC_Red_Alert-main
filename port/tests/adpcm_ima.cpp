/*
** adpcm_ima.cpp -- the game's ADPCM sound decoder, WIN32LIB/AUDIO/SOSCODEC.CPP
** (the C translation of SOSCODEC.ASM), against an IMA ADPCM decoder written
** independently from the parameters published at
** https://multimedia.cx/vqa_overview.htm: index adjust {-1,-1,-1,-1,2,4,6,8}
** and the standard 89-entry step table running from 7 to 32767.
**
** A second, independent check: port/tests/asm_misc.cpp holds the same code to
** the original assembly. (This test was first written for CODE/ADPCM.CPP,
** Westwood's own C version, which the port no longer builds -- see
** SOSCODEC.CPP.) Audio is decoded in chunks, so state must carry across
** calls -- that is tested too.
**
** The count argument is OUTPUT bytes. ADPCM.CPP took INPUT bytes and wrote four
** times what its callers asked for -- a heap overrun on every compressed 16-bit
** mono sound while it was in the build; the original assembly, run under the
** emulator, is what showed which convention is right.
*/
#include "soscomp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
extern "C" void sosCODECInitStream(_SOS_COMPRESS_INFO * info);
extern "C" unsigned long sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long numbytes);

static const int kIndexAdjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static const int kSteps[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
	253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
	3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
	12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };

struct Ref { int predicted = 0, index = 0; };
static short ref_nibble(Ref & r, int nib) {
	int step = kSteps[r.index];
	int diff = step >> 3;
	if (nib & 1) diff += step >> 2;
	if (nib & 2) diff += step >> 1;
	if (nib & 4) diff += step;
	if (nib & 8) diff = -diff;
	r.predicted += diff;
	if (r.predicted > 32767) r.predicted = 32767;
	if (r.predicted < -32768) r.predicted = -32768;
	r.index += kIndexAdjust[nib & 7];
	if (r.index < 0) r.index = 0;
	if (r.index > 88) r.index = 88;
	return (short)r.predicted;
}

int main() {
	srand(2);
	int streams = 0, exact = 0; long samples = 0, mismatched = 0;
	for (int trial = 0; trial < 400; trial++) {
		int len = 1 + rand() % 4000;
		std::vector<unsigned char> in(len);
		/* mix of random noise and long same-direction runs, which drive the step index to both clamps */
		for (int i = 0; i < len; i++) in[i] = (trial % 3 == 0) ? (unsigned char)(rand() & 0xFF) : (trial % 3 == 1 ? 0x77 : 0xFF);
		std::vector<short> want; Ref r;
		for (int i = 0; i < len; i++) { want.push_back(ref_nibble(r, in[i] & 0x0F)); want.push_back(ref_nibble(r, in[i] >> 4)); }

		std::vector<short> got(len * 2);
		_SOS_COMPRESS_INFO info; memset(&info, 0, sizeof info);
		info.wBitSize = 16; info.wChannels = 1;		/* as SOUNDIO.CPP sets them from the sound's header */
		sosCODECInitStream(&info);
		int pos = 0;
		while (pos < len) {						/* decode in random chunk sizes, as the streaming code does */
			int n = 1 + rand() % 257; if (pos + n > len) n = len - pos;
			info.lpSource = (char *)&in[pos];
			info.lpDest   = (char *)&got[pos * 2];
			/* the count is OUTPUT bytes, as every caller passes it (SOUNDINT.CPP's
			** dsize, LOADER.CPP's uncomp_size): n input bytes = 2n samples = 4n bytes */
			sosCODECDecompressData(&info, (unsigned long)n * 4);
			pos += n;
		}
		streams++; samples += len * 2;
		int bad = 0; for (int i = 0; i < len * 2; i++) if (got[i] != want[i]) bad++;
		mismatched += bad; if (!bad) exact++;
	}
	printf("  %d/%d streams decode identically (%ld samples, %ld mismatched), across chunked calls\n", exact, streams, samples, mismatched);
	printf("%s\n", exact == streams ? "adpcm_ima: all pass" : "FAILED");
	return exact != streams;
}
