/* $Id: idef.c,v 1.1.1.1 1997/10/30 16:17:31 maxp Exp $ */

/* This file manages the list of additional symbols to be defined in the
   dynamic code header file. */

#include "c.h"

typedef struct {		/* Hash table of idef'd symbols */
     struct hte {
	  char *name;		/* Hash key */
	  struct hte *link;	/* Link to more buckets */
     } *buckets[256];
} *pht;

#define HASHSIZE NELEMS(((pht)0)->buckets)

static pht itab;

/* itabinit: create the itable */
void itabinit (void) {
     NEW0(itab, PERM);
}

/* itabdump: unparse the itable */
void itabdump (void) {
     int i, l = HASHSIZE;
     struct hte * p;
     assert(itab);

     sw2dasm();
     print("\n/*\n * Used icode symbols\n */\n");
     print("#define _TCC_USED_ICODE __attribute__((used))\n");
#define unparse(name) \
     print("static unsigned int _tciu_%s _TCC_USED_ICODE;\n", name);
     for (i = 0; i < l; i++)
	  if ((p = itab->buckets[i]))
	       do {
		    assert(p->name);
		    unparse(p->name);
		    p = p->link;
	       } while (p);
     sw2sasm();
}

/* add2itab: add opcode name opname to the itab */
#define add2itab(opname) do {			\
     NEW0(p, PERM);				\
     p->link = itab->buckets[h];		\
     itab->buckets[h] = p;			\
     p->name = opname;				\
} while(0)

/* idef: defines opname as a symbol to be emitted in the dynamic code header
   file (by adding it to the itable), and returns its associated cost cost */
int idef (char *opname, int cost) {
     struct hte *p;
     unsigned long h = (unsigned long)opname&(HASHSIZE-1);

				/* Try to find opname in itable */
     for (p = itab->buckets[h]; p; p = p->link)
	  if (p->name == opname)
	       return cost;
				/* If not found, add it */
     add2itab(opname);
				/* Hack to add all types of i_lea */
     if (strcmp(opname, "i_leai") == 0) {
	  add2itab("i_leau");
	  add2itab("i_leap");
	  add2itab("i_leaf");
	  add2itab("i_lead");
     }
     return cost;
}

/* Register every integer-width spelling used by an LP64-sensitive template.
   The final `i' denotes an immediate operand, not the integer type. */
int idefScalarFamily(char *operation, int immediate, int cost) {
     static char *suffixes[] = { "i", "u", "l", "ul" };
     int i;

     for (i = 0; i < NELEMS(suffixes); i++)
	  idef(stringf("i_%s%s%s", operation, suffixes[i],
		       immediate ? "i" : ""), cost);
     return cost;
}

/* Register every width pairing that a conversion template can select via
   its source (%S) and destination (%T) suffixes. */
int idefScalarConversion(char *source_family, char *destination_family,
                         int immediate, int cost) {
     static char *signed_suffixes[] = { "i", "l" };
     static char *unsigned_suffixes[] = { "u", "ul" };
     static char *char_suffixes[] = { "c", "uc" };
     static char *short_suffixes[] = { "s", "us" };
     char **sources = source_family[0] == 'i' ? signed_suffixes :
		      source_family[0] == 'u' ? unsigned_suffixes :
		      source_family[0] == 'c' ? char_suffixes :
		      source_family[0] == 's' ? short_suffixes : NULL;
     char **destinations = destination_family[0] == 'i' ? signed_suffixes :
		           destination_family[0] == 'u' ? unsigned_suffixes :
		           destination_family[0] == 'c' ? char_suffixes :
		           destination_family[0] == 's' ? short_suffixes : NULL;
     int source_count = sources ? 2 : 1;
     int destination_count = destinations ? 2 : 1;
     int i, j;

     if (!sources) sources = &source_family;
     if (!destinations) destinations = &destination_family;
     for (i = 0; i < source_count; i++)
	  for (j = 0; j < destination_count; j++)
	       idef(stringf("i_cv%s2%s%s", sources[i], destinations[j],
		    immediate ? "i" : ""), cost);
     return cost;
}
