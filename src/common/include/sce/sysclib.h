/* SceSysclibForDriver: the ABI helpers and the functions modules use without vitasdk's header. */
#pragma once

#include <psp2kern/types.h>

/* NID 0xAC86B4BA: quotient in r0, signed remainder in r1. */
SceUInt64 __aeabi_idivmod(SceInt32 numerator, SceInt32 denominator);

/* sysclib's memory compare, as vitasdk's psp2kern/kernel/sysclib.h declares it */
int memcmp(const void *s1, const void *s2, unsigned int len);
