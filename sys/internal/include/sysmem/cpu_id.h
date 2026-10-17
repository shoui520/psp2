/*
 * This core's number, for code that cannot include vitasdk's
 * psp2kern/kernel/cpu.h (SceSysmem defines those functions itself).
 */

#pragma once

#include <sysmem/cp15.h>

/* this core's number (MPIDR.Aff0) */
static inline int SceCpuId(void)
{
	return readMpidr() & 3;
}
