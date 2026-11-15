/*
 * i64mod.c: the signed 64-bit "%" of SNC's runtime library (libsnc i64mod.o), its own
 * object, so a module that never takes a signed 64-bit remainder does not link it
 */

#ifdef __SNC__
#pragma control autoinlinesize=0
#endif

#include "i64ops.h"

long long __moddi3(long long n, long long d)
{
	return n - __divdi3(n, d) * d;
}
