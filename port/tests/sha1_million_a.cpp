#include "sha.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(){
	static char a[1000000]; memset(a,'a',sizeof(a));
	SHAEngine e; srand(7); long pos=0;
	while(pos<1000000){ long n=1+rand()%997; if(pos+n>1000000) n=1000000-pos; e.Hash(a+pos,n); pos+=n; }
	unsigned char r[20]; e.Result(r); char h[41]; for(int i=0;i<20;i++) sprintf(h+2*i,"%02x",r[i]);
	printf("  1,000,000 x 'a' in irregular chunks -> %s %s\n",h,strcmp(h,"34aa973cd4c4daa4f61eeb2bdbad27316534016f")?"MISMATCH":"ok");
	return strcmp(h,"34aa973cd4c4daa4f61eeb2bdbad27316534016f")!=0;
}
