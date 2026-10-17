/*
 * __i64_shift_right_1bit of SNC's runtime library (libsnc i64utils.o), the
 * signed twin of __u64_shift_right_1bit (i64ops.c).  The SNC linker keeps
 * it only in modules that refer to it.
 */

#include "i64ops.h"

typedef union {
	unsigned long long v;
	struct {
		unsigned int lo;
		unsigned int hi;
	} w;
} u64parts;

long long __i64_shift_right_1bit(long long value)
{
	u64parts a;

	a.v = value;
	a.w.lo = a.w.lo >> 1 | a.w.hi << 31;
	a.w.hi = (int)a.w.hi >> 1;
	return a.v;
}
