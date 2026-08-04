/* $Id: tpot.c,v 1.2 1997/12/11 01:26:18 maxp Exp $ */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tpo.h"

#define BSZ 131072

static char ibuf[BSZ], obuf[BSZ];

int main (int argc, char **argv) {
     int i, dp = 0, rp = 0;
     FILE *fp;
     char *q;
     for (i = 1; i < argc; i++)
	  if (!strcmp(argv[i], "-d"))
	       ++dp;
	  else if (!strcmp(argv[i], "-r"))
	       ++rp;
	  else {
	       if ((fp = fopen(argv[i], "r")) == NULL) {
		    printf("Can't read file '%s'\n", argv[i]);
		    return -1;
	       }
	       q = ibuf; 
	       while (fgets(q, BSZ-(q-ibuf), fp)) { while (*q++); q--; }
	       assert(q < ibuf+BSZ); *q = '\0';
	       tpo(ibuf, obuf, obuf+BSZ, rp, dp);
	       fprintf(stdout, "%s", obuf);
	       fclose(fp);
	  }
     return 0;
}
