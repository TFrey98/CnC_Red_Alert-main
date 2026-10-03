#include "function.h"
#include <stdio.h>
/* Dirty the stack, then build the same coordinate twice the engine's way. */
__attribute__((noinline)) static void dirty(void){ volatile unsigned char junk[256]; for (int i=0;i<256;i++) junk[i]=(unsigned char)(0xA5^i); }
__attribute__((noinline)) static COORDINATE build(void){ return XY_Coord(0x1234, 0x5678); }
int main(void){
	dirty(); COORDINATE a = build();
	COORDINATE b = build();
	printf("sizeof(COORDINATE)=%zu sizeof(COORD_COMPOSITE)=%zu sizeof(TARGET)=%zu\n", sizeof(COORDINATE), sizeof(COORD_COMPOSITE), sizeof(TARGET));
	printf("XY_Coord(0x1234,0x5678) = %016llx and %016llx -> %s\n",(unsigned long long)a,(unsigned long long)b,((unsigned long long)a==0x56781234ULL && (unsigned long long)b==0x56781234ULL)?"exact":"WRONG");
	return 0;
}
