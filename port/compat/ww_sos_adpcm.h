/*
**	ww_sos_adpcm.h -- PORT-CREATED. The SOS IMA-ADPCM decoder, as Westwood's
**	assembly implemented it, for the C translations of:
**
**	  WIN32LIB/AUDIO/OLSOSDEC.ASM   General_sosCODECDecompressData
**	  WIN32LIB/AUDIO/SOSCODEC.ASM   sosCODECDecompressData (its 16-bit mono path)
**	  WINVQ/VQM32/SOSCODEC.ASM      VQA_sosCODECDecompressData
**
**	The three are one algorithm over two struct layouts (the library's and the
**	movie player's _SOS_COMPRESS_INFO order their fields differently), hence a
**	template. Each translation is verified against its own assembly under the
**	x86 emulator (port/asmref).
*/
#ifndef WW_SOS_ADPCM_H
#define WW_SOS_ADPCM_H

#include <stdint.h>

namespace WWPortSOS {

static const int16_t IndexTab[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static const uint16_t StepTab[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
	157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
	724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
	3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/*
**	One channel's state, by reference into the struct (channel 1 or 2).
*/
struct Channel {
	unsigned long & SampleIndex;
	long & Predicted;
	long & Difference;
	short & CodeBuf;
	short & Code;
	short & Step;
	short & Index;
};

/*
**	Decode `samples` samples of one channel. Source bytes are `sstep` apart
**	(2 in stereo: the channels' bytes interleave), output samples `dstep`
**	bytes apart; 8-bit output is the high byte with its sign flipped.
**	Each source byte holds two codes, low nibble first; the nibble phase
**	restarts at every call (SampleIndex is zeroed by the caller).
*/
inline void decode(Channel c, unsigned char const * s, unsigned char * d, uint32_t samples, int sstep, int dstep, bool sixteen)
{
	while (samples--) {
		unsigned code;
		if (c.SampleIndex & 1) {
			code = ((uint16_t)c.CodeBuf >> 4) & 15;
		} else {
			c.CodeBuf = *s;
			s += sstep;
			code = (uint16_t)c.CodeBuf & 15;
		}
		c.Code = (short)code;

		uint32_t step = (uint16_t)c.Step;
		int32_t diff = 0;
		if (code & 4) diff += step;
		if (code & 2) diff += step >> 1;
		if (code & 1) diff += step >> 2;
		diff += step >> 3;
		if (code & 8) diff = -diff;
		c.Difference = diff;

		int32_t p = (int32_t)((uint32_t)(int32_t)c.Predicted + (uint32_t)diff);
		if (p >= 0x7FFF) p = 0x7FFF;
		if (p <= -0x8000) p = -0x8000;
		c.Predicted = p;
		if (sixteen) {
			d[0] = (unsigned char)p;
			d[1] = (unsigned char)(p >> 8);
		} else {
			d[0] = (unsigned char)((p >> 8) ^ 0x80);
		}
		d += dstep;

		uint16_t index = (uint16_t)((uint16_t)c.Index + (uint16_t)IndexTab[code]);
		if (index >= 0x8000) index = 0;
		else if (index > 88) index = 88;
		c.Index = (short)index;
		c.Step = (short)StepTab[index];
		c.SampleIndex++;
	}
}

/*
**	The whole call: mono or stereo, 8- or 16-bit output. wBytes counts output
**	bytes; returns wBytes, as the assembly did.
**
**	Divergence: a sample count of 0 (or, in stereo, an odd count) made the
**	assembly's down-counter step past zero and run on for 2^32 samples; here
**	the remaining whole samples are decoded and it stops.
*/
template <class Info>
unsigned long decompress(Info * info, unsigned long wBytes)
{
	info->dwSampleIndex = 0;
	info->dwSampleIndex2 = 0;
	bool const sixteen = info->wBitSize == 16;
	uint32_t count = sixteen ? (uint32_t)wBytes >> 1 : (uint32_t)wBytes;
	unsigned char const * s = (unsigned char const *)info->lpSource;
	unsigned char * d = (unsigned char *)info->lpDest;
	Channel one = {info->dwSampleIndex, info->dwPredicted, info->dwDifference, info->wCodeBuf, info->wCode, info->wStep, info->wIndex};

	if (info->wChannels != 2) {
		decode(one, s, d, count, 1, sixteen ? 2 : 1, sixteen);
		return wBytes;
	}
	Channel two = {info->dwSampleIndex2, info->dwPredicted2, info->dwDifference2, info->wCodeBuf2, info->wCode2, info->wStep2, info->wIndex2};
	decode(one, s, d, count / 2, 2, sixteen ? 4 : 2, sixteen);
	decode(two, s + 1, d + (sixteen ? 2 : 1), count / 2, 2, sixteen ? 4 : 2, sixteen);
	return wBytes;
}

/*
**	Fresh stream: index 0, step 7, predictor 0, both channels.
*/
template <class Info>
void init(Info * info)
{
	info->wIndex = 0;
	info->wStep = 7;
	info->dwPredicted = 0;
	info->dwSampleIndex = 0;
	info->wIndex2 = 0;
	info->wStep2 = 7;
	info->dwPredicted2 = 0;
	info->dwSampleIndex2 = 0;
}

}

#endif
