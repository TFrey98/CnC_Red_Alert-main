/*
**	asm_winvq.cpp -- C translations of the movie player's assembly (WINVQ) vs
**	vectors recorded from the original assembly (port/asmref).
*/
#include "asm_replay.h"
#include <vqm32/compress.h>
#include <vqm32/soscomp.h>
#include <vqm32/palette.h>
#include "unvq.h"

/*
**	Stand-in for CODE/PRAGMAUX.CPP's emulated VGA ports: records the writes.
*/
static std::vector<std::pair<int, int> > PortLog;
void outportb(int port, unsigned char data) {PortLog.push_back(std::make_pair(port, (int)data));}

int main()
{
	bool ok = true;
	{
		Tally t("AudioUnzap");
		for (Case const & c : load("audio_unzap")) {
			t.check(c, snd1_case(c, [](void * s, void * d, long n) {return AudioUnzap(s, d, n);}));
		}
		ok &= t.report();
	}
	{
		Tally t("VQA_sosCODECDecompressData");
		for (Case const & c : load("vqa_sos")) {
			t.check(c, sos_case<_SOS_COMPRESS_INFO>(c, VQA_sosCODECInitStream, VQA_sosCODECDecompressData, true));
		}
		ok &= t.report();
	}
	{
		Tally t("UnVQ_4x2");
		for (Case const & c : load("unvq_4x2")) {
			long long const * p = &c.in[0];		// bpr rows bufwidth cbentries cseed pseed dseed
			long long bpr = p[0], rows = p[1], bufwidth = p[2], cbentries = p[3], entries = bpr * rows;
			std::vector<unsigned char> cb(8 * cbentries), ptr(2 * entries), buf(GUARD + bufwidth * 2 * rows + GUARD);
			fill(cb.data(), cb.size(), (uint32_t)p[4]);
			fill(ptr.data(), ptr.size(), (uint32_t)p[5]);
			for (long long i = 0; i < entries; i++) {
				if ((ptr[entries + i] & 3) == 0) {
					ptr[entries + i] = 0x0F;
				} else {
					long long idx = ((long long)ptr[entries + i] << 8 | ptr[i]) % cbentries;
					if ((idx >> 8) == 0x0F) idx = 0;
					ptr[i] = (unsigned char)idx;
					ptr[entries + i] = (unsigned char)(idx >> 8);
				}
			}
			fill(buf.data(), buf.size(), (uint32_t)p[6]);
			UnVQ_4x2(cb.data(), ptr.data(), buf.data() + GUARD, (unsigned long)bpr, (unsigned long)rows, (unsigned long)bufwidth);
			t.check(c, fnv(buf.data(), buf.size()) == c.out[0]);
		}
		ok &= t.report();
	}
	{
		/*
		**	SetPalette (native): the bytes the assembly sent to the DAC ports --
		**	index 0 to 3C8h, then the palette to 3C9h; whole colours when slow.
		*/
		Tally t("SetPalette (native, DAC port writes)");
		unsigned char pal[768];
		fill(pal, sizeof(pal), 1234);
		long const sizes[] = {768, 48, 3, 4, 0};
		for (long n : sizes) {
			for (unsigned long slow = 0; slow < 2; slow++) {
				PortLog.clear();
				SetPalette(pal, n, slow);
				long want = slow ? ((n > 0 ? n : 1) + 2) / 3 * 3 : n;
				bool same = PortLog.size() == (size_t)want + 1 && PortLog[0] == std::make_pair(0x3C8, 0);
				for (long i = 0; same && i < want; i++) same = PortLog[1 + i] == std::make_pair(0x3C9, (int)pal[i]);
				Case c; c.line = (int)n;
				t.check(c, same);
			}
		}
		ok &= t.report();
	}
	printf(ok ? "asm_winvq: all pass\n" : "asm_winvq: FAIL\n");
	return ok ? 0 : 1;
}
