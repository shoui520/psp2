#ifndef LIBKERNEL_BACKTRACE_WALK_H
#define LIBKERNEL_BACKTRACE_WALK_H

#include <stdint.h>
#include <stddef.h>
#include "backtrace.h"

struct BacktraceRegisters {
	union {
		uint32_t r[16];
		struct {
			uint32_t r0, r1, r2, r3, r4, r5, r6, r7;
			uint32_t r8, r9, r10, r11, r12, sp, lr, pc;
		};
	};
};

_Static_assert(sizeof(struct BacktraceRegisters) == 64, "captured register extent");
_Static_assert(offsetof(struct BacktraceRegisters, sp) == 52, "captured SP offset");
_Static_assert(offsetof(struct BacktraceRegisters, pc) == 60, "captured PC offset");

int BacktraceWalk(SceUID processId, SceKernelCallFrame *frames, SceSize size,
		SceUInt32 *count, SceUInt32 flags, struct BacktraceRegisters *registers);

#endif
