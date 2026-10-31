#ifndef IPMI_ASSERT_H
#define IPMI_ASSERT_H

#include <kernel.h>
#include <libkernel/libkernel.h>

/*
 * libipmi's internal assertion: a call that must not fail prints Sony's message, waits a
 * second for the output to drain and stops at a breakpoint.
 */
#define IPMI_ASSERT_SUCCESS(ret)						\
	do {									\
		if ((ret) != 0) {						\
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE (0x%x)\n", (ret)); \
			sceKernelDelayThread(1000000);				\
			__builtin_trap();					\
		}								\
	} while (0)

#endif
