long reference_mix(long value, int count)
{
	long result = value;
	int i;

	for (i = 0; i < count; i++) {
		if (result & 1)
			result = result * 3 + 1;
		else
			result = result / 2;
	}
	return result;
}

long reference_conversion(long value, int kind)
{
	signed char c = value;
	unsigned char uc = value;
	short s = value;
	unsigned short us = value;
	int i = value;
	unsigned int u = value;

	if (kind == 0) return (long)c;
	if (kind == 1) return (long)uc;
	if (kind == 2) return (long)s;
	if (kind == 3) return (long)us;
	if (kind == 4) return (long)i;
	return (long)u;
}

long reference_deep(long a, long b, long c, long d,
		    long e, long f, long g, long h)
{
	return (((a + b) * (c - d)) ^ ((e | f) & (g + h))) +
	       1099511627776L;
}
