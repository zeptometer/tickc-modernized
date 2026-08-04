#include "c.h"
extern Interface nullIR,   symbolicIR;
extern Interface x86_64IIR, x86_64VIR;
extern Interface cIR;
Binding * binding;
Binding bindings[] = {
     { "symbolic",      &symbolicIR, NULL, NULL },
     { "x86_64-linux",	&cIR,	     &x86_64IIR, &x86_64VIR },
     { "c-x86_64",	&cIR,	     &x86_64IIR, &x86_64VIR },
     { "null",          &nullIR,     &nullIR, NULL },
     { NULL,            NULL,        NULL, NULL },
};
