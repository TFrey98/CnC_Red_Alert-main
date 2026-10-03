#include "crc.h"
#include <stdio.h>
#include <stdlib.h>
int main(){
	unsigned char buf[1000]; for(int i=0;i<1000;i++) buf[i]=(unsigned char)(i*131+7);
	int bad=0,total=0; srand(12345);
	for(int trial=0;trial<2000;trial++){
		int len=rand()%1000; long one=CRCEngine()(buf,len);
		CRCEngine e; int pos=0;
		while(pos<len){ int n=1+rand()%11; if(pos+n>len) n=len-pos; e(buf+pos,n); pos+=n; }
		total++; if((long)e()!=one) bad++;
	}
	printf("%d/%d chunked runs match one-shot\n",total-bad,total); return bad!=0;
}
