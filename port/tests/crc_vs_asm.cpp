#include "crc.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* Line-by-line model of WIN32LIB/MISC/CRC.ASM: rol-1-and-add over little-endian
** dwords, then the 1-3 byte tail gathered with `ror eax,8` per byte and
** realigned with `ror eax,(4-n)*8` -- i.e. zero-padded, little-endian. */
static uint32_t rol1(uint32_t v){ return (v<<1)|(v>>31); }
static uint32_t asm_crc(const unsigned char*p, unsigned len){
	uint32_t ebx=0; unsigned n=len>>2, rem=len&3;
	for(unsigned i=0;i<n;i++){ uint32_t d; memcpy(&d,p+4*i,4); ebx=rol1(ebx)+d; }
	if(rem){ uint32_t eax=0; const unsigned char*q=p+4*n;
		for(unsigned i=0;i<rem;i++){ eax=(eax&0xFFFFFF00u)|q[i]; eax=(eax>>8)|(eax<<24); }	/* lodsb, then ror eax,8 */
		unsigned sh=(4-rem)*8; eax=(eax>>sh)|(eax<<(32-sh));
		ebx=rol1(ebx)+eax; }
	return ebx;
}
int main(){
	const char* names[]={"CONQUER.MIX","LOCAL.MIX","RULES.INI","A","AB","ABC","ABCD","ABCDE","MOUSE.SHP","SCG01EA.INI","ALLIES.MIX"};
	int bad=0,total=0;
	for(auto n:names){ uint32_t a=asm_crc((const unsigned char*)n,strlen(n)); uint32_t c=(uint32_t)CRCEngine()((void*)n,strlen(n));
		printf("  %-12s asm=%08X  CRCEngine=%08X %s\n",n,a,c,a==c?"":"  MISMATCH"); bad+=a!=c; total++; }
	unsigned char buf[300]; for(int i=0;i<300;i++) buf[i]=(unsigned char)(i*37+11);
	for(int len=0;len<300;len++){ total++; if(asm_crc(buf,len)!=(uint32_t)CRCEngine()(buf,len)) bad++; }
	printf("%d/%d inputs agree\n",total-bad,total); return bad!=0;
}
