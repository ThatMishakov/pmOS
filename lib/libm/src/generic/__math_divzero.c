// Taken from musl library
// src/math/__math_divzero.c

#include "libm.h"

double __math_divzero(uint32_t sign)
{
	return fp_barrier(sign ? -1.0 : 1.0) / 0.0;
}
