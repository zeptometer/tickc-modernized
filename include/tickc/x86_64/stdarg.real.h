#ifndef __STDARG
#define __STDARG

/* Variadic Tick-C functions are Phase 6 work. This parsing facade keeps the
   historical front end independent of the host GCC va_list representation. */
typedef char *va_list;

#define va_start(list, start) ((void)((list) = (char *)(&(start) + 1)))
#define va_end(list) ((void)0)
#define va_arg(list, mode) (*(mode *)((list) += sizeof(mode), (list) - sizeof(mode)))

#endif
