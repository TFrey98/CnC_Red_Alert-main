/*
** vqa_format.cpp -- the VQA player's on-disk structures checked against an
** INDEPENDENT description of the format: Gordan Ugarkovic's VQA overview
** (https://multimedia.cx/vqa_overview.htm), written from outside Westwood.
**
** The synthetic file below is laid out from that document's offsets, not the
** engine's. It is then parsed through the engine's own structs and macros
** (VQAPLAYP.H, VQAFILE.H, IFF.H). Before the port's fixes, ChunkHeader was 16
** bytes and FormHeader 24 -- this test would have failed at the first chunk.
*/
#include "vqaplayp.h"
#include <vqm32/iff.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-66s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }
static void be32(unsigned char * p, unsigned v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void le16(unsigned char * p, unsigned v) { p[0]=v; p[1]=v>>8; }

extern "C" void __cdecl UnVQ_4x2(unsigned char *, unsigned char *, unsigned char *, unsigned long, unsigned long, unsigned long);
extern "C" void __cdecl UnVQ_4x4(unsigned char *, unsigned char *, unsigned char *, unsigned long, unsigned long, unsigned long);

/*
**	The document's block decode, as it words it: "The index table is an array
**	of bytes and is split into 2 parts - the top half and the bottom half.
**	TopVal = Table[by*(Width/Wx)+bx]; LowVal = Table[(Width/Wx)*(Height/Wy)+by*(Width/Wx)+bx].
**	If LowVal=0x0f (0x0ff for the start movie of Red Alert 95) you should simply
**	fill the block with color TopVal", otherwise the block is codebook entry
**	LowVal*256+TopVal, Wx*Wy bytes, row by row.
*/
static void reference_decode(unsigned char const * cb, unsigned char const * table, unsigned char * out,
	int bw, int bh, int wx, int wy, int stride, int solid)
{
	for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
		int top = table[by * bw + bx], low = table[bw * bh + by * bw + bx];
		for (int y = 0; y < wy; y++) for (int x = 0; x < wx; x++) {
			out[(by * wy + y) * stride + bx * wx + x] = (low == solid) ? (unsigned char)top
				: cb[(low * 256 + top) * wx * wy + y * wx + x];
		}
	}
}

static uint32_t rng = 0x12345678u;
static unsigned rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* Random frames through the engine's decoder and the reference; returns cases that matched. */
static int blocks_match(int wy, int solid, int cases)
{
	int ok = 0;
	for (int n = 0; n < cases; n++) {
		int bw = 1 + rnd() % 60, bh = 1 + rnd() % 40, stride = 4 * bw + (rnd() % 3 ? 0 : (int)(rnd() % 16));
		int entries = 1 + rnd() % (solid == 0xFF ? 0xFF00 : 0x0F00);
		std::vector<unsigned char> cb((size_t)entries * 4 * wy), table(2 * bw * bh);
		for (auto & c : cb) c = (unsigned char)rnd();
		for (int i = 0; i < bw * bh; i++) {
			if (rnd() % 4 == 0) { table[i] = (unsigned char)rnd(); table[bw * bh + i] = (unsigned char)solid; continue; }
			int e = rnd() % entries;
			table[i] = (unsigned char)e; table[bw * bh + i] = (unsigned char)(e >> 8);
		}
		size_t size = (size_t)stride * bh * wy;
		std::vector<unsigned char> a(size, 0xAA), b(size, 0xAA);
		reference_decode(cb.data(), table.data(), a.data(), bw, bh, 4, wy, stride, solid);
		(wy == 4 ? UnVQ_4x4 : UnVQ_4x2)(cb.data(), table.data(), b.data(), bw, bh, stride);
		ok += a == b;
	}
	return ok;
}

int main() {
	/* ---- per the document: FORM <size BE> WVQA, then VQHD <size BE> + 42-byte payload ---- */
	unsigned char file[12 + 8 + 42];
	memset(file, 0, sizeof file);
	memcpy(file, "FORM", 4); be32(file + 4, sizeof file - 8); memcpy(file + 8, "WVQA", 4);
	unsigned char * vqhd = file + 12;
	memcpy(vqhd, "VQHD", 4); be32(vqhd + 4, 42);
	unsigned char * h = vqhd + 8;			/* document offsets minus the 12-byte "WVQAVQHD"+RStartPos prefix */
	le16(h + 0, 2);							/* Version */
	le16(h + 4, 345);						/* NumFrames  (doc offset 16) */
	le16(h + 6, 320);						/* Width      (doc offset 18) */
	le16(h + 8, 156);						/* Height     (doc offset 20) */
	h[10] = 4; h[11] = 2;					/* Wx, Wy     (doc offsets 22, 23) */
	le16(h + 24, 22050);					/* Freq       (doc offset 36) */
	h[26] = 1;								/* Channels   (doc offset 38) */

	/* ---- parsed through the engine's structures ---- */
	check(sizeof(FormHeader) == 12 && sizeof(ChunkHeader) == 8 && sizeof(VQAHeader) == 42, "sizes: FormHeader 12, ChunkHeader 8, VQAHeader 42 (document: 54 incl. 12-byte prefix)");
	FormHeader form; memcpy(&form, file, sizeof form);
	check(form.id == MAKE_ID('F','O','R','M') && REVERSE_LONG(form.size) == sizeof file - 8 && form.type == MAKE_ID('W','V','Q','A'), "FormHeader: FORM, big-endian size, WVQA");
	ChunkHeader chunk; memcpy(&chunk, vqhd, sizeof chunk);
	check(chunk.id == MAKE_ID('V','Q','H','D') && REVERSE_LONG(chunk.size) == 42, "ChunkHeader: VQHD id, big-endian size 42");
	VQAHeader hd; memcpy(&hd, h, sizeof hd);
	check(hd.Version == 2 && hd.Frames == 345, "VQAHeader.Version / Frames at the document's offsets");
	check(hd.ImageWidth == 320 && hd.ImageHeight == 156, "VQAHeader.ImageWidth / ImageHeight");
	check(hd.BlockWidth == 4 && hd.BlockHeight == 2, "VQAHeader.BlockWidth / BlockHeight (document: Wx, Wy)");
	check(hd.SampleRate == 22050 && hd.Channels == 1, "VQAHeader.SampleRate / Channels (document: Freq, Channels)");
	check(offsetof(VQAHeader, SampleRate) == 24 && offsetof(VQAHeader, Channels) == 26, "offsets match the document exactly (Freq 36-12, Channels 38-12)");

	/* ---- FINF: Intel-order 32-bit entries, x2, with flag bits ---- */
	int32_t finf = (int32_t)(0x40000000u | 0x1234u);	/* the document's "0x40000000 too large" entry */
	check(VQAFRAME_OFFSET(finf) == 0x1234 * 2, "FINF: VQAFRAME_OFFSET strips flags and multiplies by 2");
	check((finf & VQAFINF_PAL) != 0 && (finf & VQAFINF_KEY) == 0, "FINF: 0x40000000 is the palette flag (VQAFINB_PAL = 30)");
	int32_t key = (int32_t)0x80000010u;
	check((key & VQAFINF_KEY) != 0 && VQAFRAME_OFFSET(key) == 0x20, "FINF: key-frame flag in bit 31 survives the signed 32-bit entry");
	check(PADSIZE(7) == 8 && PADSIZE(8) == 8, "odd-sized chunks are padded to even");

	/* ---- VPT block decode against the document's own description ---- */
	check(blocks_match(2, 0x0F, 500) == 500, "4x2 blocks: UnVQ_4x2 == the document's decode (solid 0x0f), 500 frames");
	check(blocks_match(4, 0xFF, 500) == 500, "4x4 blocks: UnVQ_4x4 == the document's decode (solid 0xff), 500 frames");

	printf("%s\n", fails ? "FAILED" : "vqa_format: all pass");
	return fails != 0;
}
