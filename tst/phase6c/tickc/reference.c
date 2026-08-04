#include <stdarg.h>

long reference_integer_variadic(int count, ...)
{
	va_list arguments;
	long total = 0;
	int i;
	va_start(arguments, count);
	for (i = 0; i < count; i++) total += va_arg(arguments, long);
	va_end(arguments);
	return total;
}

double reference_floating_variadic(int count, ...)
{
	va_list arguments;
	double total = 0.0;
	int i;
	va_start(arguments, count);
	for (i = 0; i < count; i++) total += va_arg(arguments, double);
	va_end(arguments);
	return total;
}
