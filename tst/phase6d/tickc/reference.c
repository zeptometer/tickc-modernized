struct integer_pair { long first, second; };
struct float_pair { double first, second; };
struct mixed_pair { long integer; double floating; };
struct reverse_mixed_pair { double floating; long integer; };
struct large_value { long first, second, third; };
struct nested_value { struct { int first, second; } inner; double value; };
union integer_union { long integer; double floating; };
struct union_value { union integer_union value; int tag; };
struct byte_triple { unsigned char first, second, third; };

struct integer_pair reference_integer_pair(struct integer_pair value)
{ value.first += 10; value.second += 20; return value; }
struct float_pair reference_float_pair(struct float_pair value)
{ value.first += 1.25; value.second += 2.5; return value; }
struct mixed_pair reference_mixed_pair(struct mixed_pair value)
{ value.integer += 7; value.floating += 0.5; return value; }
struct reverse_mixed_pair reference_reverse_mixed_pair(
    struct reverse_mixed_pair value)
{ value.floating += 0.75; value.integer += 8; return value; }
struct large_value reference_large_value(struct large_value value)
{ value.first++; value.second += 2; value.third += 3; return value; }
struct nested_value reference_nested_value(struct nested_value value)
{ value.inner.first++; value.inner.second += 2; value.value += 3.0; return value; }
struct union_value reference_union_value(struct union_value value)
{ value.value.integer += 4; value.tag += 5; return value; }
struct byte_triple reference_byte_triple(struct byte_triple value)
{ value.first++; value.second += 2; value.third += 3; return value; }

long reference_integer_rollback(long a, long b, long c, long d, long e,
    struct integer_pair pair, long tail)
{
    return a + b + c + d + e + pair.first * 10 +
           pair.second * 100 + tail * 1000;
}

double reference_sse_rollback(double a, double b, double c, double d,
    double e, double f, double g, struct float_pair pair, double tail)
{
    return a + b + c + d + e + f + g + pair.first * 10 +
           pair.second * 100 + tail * 1000;
}

int check_integer_identity(struct integer_pair (*function)(struct integer_pair))
{
    struct integer_pair input = { 1, 2 }, result = function(input);
    return result.first == 1 && result.second == 2;
}
int check_integer_caller(struct integer_pair (*function)(struct integer_pair))
{
    struct integer_pair input = { 1, 2 }, result = function(input);
    return result.first == 11 && result.second == 22;
}
int check_integer_constant(struct integer_pair (*function)(void))
{
    struct integer_pair result = function();
    return result.first == 21 && result.second == 22;
}
int check_float_identity(struct float_pair (*function)(struct float_pair))
{
    struct float_pair input = { 1.0, 2.0 }, result = function(input);
    return result.first == 1.0 && result.second == 2.0;
}
int check_float_caller(struct float_pair (*function)(struct float_pair))
{
    struct float_pair input = { 1.0, 2.0 }, result = function(input);
    return result.first == 2.25 && result.second == 4.5;
}
int check_mixed_identity(struct mixed_pair (*function)(struct mixed_pair))
{
    struct mixed_pair input = { 3, 4.0 }, result = function(input);
    return result.integer == 3 && result.floating == 4.0;
}
int check_mixed_caller(struct mixed_pair (*function)(struct mixed_pair))
{
    struct mixed_pair input = { 3, 4.0 }, result = function(input);
    return result.integer == 10 && result.floating == 4.5;
}
int check_reverse_identity(
    struct reverse_mixed_pair (*function)(struct reverse_mixed_pair))
{
    struct reverse_mixed_pair input = { 4.25, 5 }, result = function(input);
    return result.floating == 4.25 && result.integer == 5;
}
int check_reverse_caller(
    struct reverse_mixed_pair (*function)(struct reverse_mixed_pair))
{
    struct reverse_mixed_pair input = { 4.25, 5 }, result = function(input);
    return result.floating == 5.0 && result.integer == 13;
}
int check_large_identity(struct large_value (*function)(struct large_value))
{
    struct large_value input = { 5, 6, 7 }, result = function(input);
    return result.first == 5 && result.second == 6 && result.third == 7;
}
int check_large_caller(struct large_value (*function)(struct large_value))
{
    struct large_value input = { 5, 6, 7 }, result = function(input);
    return result.first == 6 && result.second == 8 && result.third == 10;
}

int check_nested_identity(struct nested_value (*function)(struct nested_value))
{
    struct nested_value input = { { 10, 11 }, 12.0 }, result = function(input);
    return result.inner.first == 10 && result.inner.second == 11 &&
           result.value == 12.0;
}
int check_nested_caller(struct nested_value (*function)(struct nested_value))
{
    struct nested_value input = { { 10, 11 }, 12.0 }, result = function(input);
    return result.inner.first == 11 && result.inner.second == 13 &&
           result.value == 15.0;
}
int check_union_identity(struct union_value (*function)(struct union_value))
{
    struct union_value input = { { 13 }, 14 }, result = function(input);
    return result.value.integer == 13 && result.tag == 14;
}
int check_union_caller(struct union_value (*function)(struct union_value))
{
    struct union_value input = { { 13 }, 14 }, result = function(input);
    return result.value.integer == 17 && result.tag == 19;
}
int check_byte_identity(struct byte_triple (*function)(struct byte_triple))
{
    struct byte_triple input = { 15, 16, 17 }, result = function(input);
    return result.first == 15 && result.second == 16 && result.third == 17;
}
int check_byte_caller(struct byte_triple (*function)(struct byte_triple))
{
    struct byte_triple input = { 15, 16, 17 }, result = function(input);
    return result.first == 16 && result.second == 18 && result.third == 20;
}

int check_integer_rollback(long (*function)(long, long, long, long, long,
    struct integer_pair, long))
{
    struct integer_pair pair = { 6, 7 };
    return function(1, 2, 3, 4, 5, pair, 8) == 8775;
}

int check_sse_rollback(double (*function)(double, double, double, double,
    double, double, double, struct float_pair, double))
{
    struct float_pair pair = { 8.0, 9.0 };
    return function(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0,
                    pair, 10.0) == 11008.0;
}
