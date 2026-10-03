#include "sha.h"
#include <stdio.h>
#include <string.h>
static const char* hex(const unsigned char*d,int n){static char b[300];for(int i=0;i<n;i++)sprintf(b+2*i,"%02x",d[i]);return b;}
int main(){
	struct { const char* in; const char* want; } v[]={
		{"abc","a9993e364706816aba3e25717850c26c9cd0d89d"},
		{"","da39a3ee5e6b4b0d3255bfef95601890afd80709"},
		{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","84983e441c3bd26ebaae4aa1f95129e5e54670f1"}};
	int bad=0;
	for(auto&t:v){ SHAEngine e; e.Hash(t.in,(long)strlen(t.in)); unsigned char r[128]={0}; int n=e.Result(r);
		printf("  len %-3d -> %s (%d bytes) %s\n",(int)strlen(t.in),hex(r,n),n,strcmp(hex(r,n),t.want)?"MISMATCH":"ok"); bad+=strcmp(hex(r,n),t.want)!=0; }
	return bad;
}
