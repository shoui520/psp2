/*
 * The division-by-zero handlers of SNC's runtime library (libsnc aeabi_div0.o, weak
 * there: a module may define its own): they pass the division to the system's
 * __sce_aeabi_idiv0 / __sce_aeabi_ldiv0.
 */

#include "i32ops.h"

int __sce_aeabi_idiv0(int return_value);
long long __sce_aeabi_ldiv0(long long return_value);

int __aeabi_idiv0(int return_value)
{
	return __sce_aeabi_idiv0(return_value);
}

#ifndef LIBSNC_OMIT_LDIV0
long long __aeabi_ldiv0(long long return_value)
{
	return __sce_aeabi_ldiv0(return_value);
}
#endif
