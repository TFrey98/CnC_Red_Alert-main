/*
** pragma_aux.cpp -- CODE/PRAGMAUX.CPP against a register-level emulation of
** each original `#pragma aux` instruction sequence. The C versions are algebraic
** simplifications ("bits 7..22 of the product"); this model executes the x86
** instructions literally on 16/32-bit registers, so a wrong simplification shows
** up as a mismatch.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* PRAGMAUX.CPP hands each finished DAC colour to the display (win32_ddraw.cpp); record them here. */
static int DacEntries = 0;
extern "C" void WWPort_DAC_Entry(int, int, int, int) {DacEntries++;}
int  calcx(signed short, short);
int  calcy(signed short, short);
unsigned Fixed_To_Cardinal(unsigned base, unsigned fixed);
unsigned Cardinal_To_Fixed(unsigned base, unsigned cardinal);
int  Get_Bit(void const * array, int bit);
void Set_Bit(void * array, int bit, int value);
int  First_True_Bit(void const * array);
int  First_False_Bit(void const * array);

/* --- literal x86 models --- */
static uint16_t model_calc_ax(int16_t ax_in, int16_t bx) {
	int32_t prod = (int32_t)ax_in * (int32_t)bx;			/* imul bx: DX:AX = AX*BX */
	uint16_t ax = (uint16_t)prod, dx = (uint16_t)((uint32_t)prod >> 16);
	unsigned cf = ax >> 15; ax = (uint16_t)(ax << 1);		/* shl ax,1 */
	dx = (uint16_t)((dx << 1) | cf);						/* rcl dx,1 */
	ax = (uint16_t)((ax & 0xFF00) | (ax >> 8));				/* mov al,ah */
	ax = (uint16_t)((ax & 0x00FF) | ((dx & 0xFF) << 8));	/* mov ah,dl */
	return ax;
}
static uint32_t model_f2c(uint32_t eax, uint32_t edx) {
	uint64_t p = (uint64_t)eax * edx; eax = (uint32_t)p;		/* mul edx (EDX discarded after) */
	eax += 0x80;												/* add eax,080h */
	if (eax & 0xFF000000u) eax = 0x00FFFFFF;					/* test / jz / mov */
	return eax >> 8;											/* shr eax,8 */
}
static uint32_t model_c2f(uint32_t ebx, uint32_t eax) {
	if (ebx == 0) return eax;									/* or ebx,ebx / jz fini */
	eax <<= 8; return eax / ebx;								/* shl eax,8 / xor edx,edx / div ebx */
}
static int model_get_bit(const uint32_t * a, int bit) { return (a[(uint32_t)bit >> 5] >> (bit & 31)) & 1; }

int main() {
	int fails = 0; long n = 0;
	for (int v = -32768; v <= 32767; v += 7)
		for (int d = -32768; d <= 32767; d += 251) {
			int16_t ax = model_calc_ax((int16_t)v, (int16_t)d); n++;
			if (calcx((short)v, (short)d) != (int)(int16_t)ax) fails++;
			if (calcy((short)v, (short)d) != (int)(int16_t)(uint16_t)(-(int)ax)) fails++;
		}
	printf("  calcx/calcy        vs register model: %ld input pairs, %d mismatches\n", n, fails);
	int f2 = 0; srand(5);
	for (int i = 0; i < 2000000; i++) {
		uint32_t a = (uint32_t)rand() << 1 ^ rand(), b = (i % 3) ? (uint32_t)(rand() & 0x1FF) : ((uint32_t)rand() << 1 ^ rand());
		if (Fixed_To_Cardinal(a & 0xFFFFFF, b) != model_f2c(a & 0xFFFFFF, b)) f2++;
		if (Cardinal_To_Fixed(b, a & 0xFFFFFF) != model_c2f(b, a & 0xFFFFFF)) f2++;
	}
	if (Cardinal_To_Fixed(0, 1234) != 1234) f2++;
	printf("  Fixed/Cardinal     vs register model: 2,000,000 inputs each, %d mismatches\n", f2);
	int fb = 0;
	uint32_t arr[8]; unsigned char shadow[32];
	for (int t = 0; t < 20000; t++) {
		for (int k = 0; k < 8; k++) arr[k] = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
		memcpy(shadow, arr, 32);
		int bit = rand() % 256, val = rand() & 1;
		if (Get_Bit(shadow, bit) != model_get_bit(arr, bit)) fb++;
		Set_Bit(shadow, bit, val);
		if (val) arr[bit >> 5] |= 1u << (bit & 31); else arr[bit >> 5] &= ~(1u << (bit & 31));
		if (memcmp(shadow, arr, 32)) fb++;
	}
	uint32_t z[4] = {0, 0, 0x00100000, 0}; uint32_t o[4] = {~0u, ~0u, ~0x00000400u, ~0u};
	if (First_True_Bit(z) != 64 + 20) fb++;
	if (First_False_Bit(o) != 64 + 10) fb++;
	printf("  bit arrays         vs dword model:    20,000 ops, %d mismatches\n", fb);
	bool ok = !fails && !f2 && !fb;
	printf("%s\n", ok ? "pragma_aux: all pass" : "FAILED");
	return !ok;
}
