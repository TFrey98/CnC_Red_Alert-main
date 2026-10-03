#include "mp.h"
#include "int.h"
#include <stdio.h>
#include <string.h>
static int hexbytes(const char*h, unsigned char*out){ int n=strlen(h); int bytes=(n+1)/2; memset(out,0,bytes);
	for(int i=0;i<n;i++){ int v=h[n-1-i]; v=(v<='9')?v-'0':(v|32)-'a'+10; out[bytes-1-i/2]|=(unsigned char)(v<<((i&1)*4)); } return bytes; }
static bignum dec(const char*h){ unsigned char b[300]; int n=hexbytes(h,b); bignum x; x.Unsigned_Decode(b,n); return x; }
int main(){
	FILE*f=fopen(getenv("KAT"),"r"); char a[200],e[200],m[200],r[200]; int ok=0,total=0;
	while(fscanf(f,"%199s %199s %199s %199s",a,e,m,r)==4){
		bignum A=dec(a),E=dec(e),M=dec(m),R=dec(r);
		bignum got=A.exp_b_mod_c(E,M); total++;
		if(got==R) ok++; else if(total-ok<=3) printf("  MISMATCH: %d-bit modulus\n",(int)strlen(m)*4);
	}
	printf("%d/%d modular exponentiations match Python pow()\n",ok,total); return ok!=total;
}
