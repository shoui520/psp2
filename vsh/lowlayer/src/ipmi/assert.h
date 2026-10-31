#ifndef IPMI_ASSERT_H
#define IPMI_ASSERT_H

#include <kernel.h>
#include <libkernel/libkernel.h>

/*
 * libipmi's internal assertion: a call that must not fail prints Sony's message, waits a
 * second for the output to drain and stops at a breakpoint.
 */
#define IPMI_ASSERT_SUCCESS(expression)					\
	do {									\
		int _ipmiRet = (expression);					\
		if (_ipmiRet != 0) {						\
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE (0x%x)\n", _ipmiRet); \
			sceKernelDelayThread(1000000);				\
			__builtin_trap();					\
		}								\
	} while (0)

/* The same stop for a condition, without a value to print. */
#define IPMI_ASSERT(cond)							\
	do {									\
		if (!(cond)) {							\
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE\n");	\
			sceKernelDelayThread(1000000);				\
			__builtin_trap();					\
		}								\
	} while (0)

/* A result the code has no case for: wait a second, report it and stop. */
#define IPMI_ASSERT_UNEXPECTED(value)						\
	do {									\
		sceKernelDelayThread(1000000);					\
		IPMI_ASSERT_SUCCESS(value);					\
		__builtin_trap();						\
	} while (0)

#endif
