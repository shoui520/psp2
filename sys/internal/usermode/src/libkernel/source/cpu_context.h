#ifndef LIBKERNEL_CPU_CONTEXT_H
#define LIBKERNEL_CPU_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

/* Four-byte alignment is part of this ABI, including the VFP register words. */
struct CpuContext {
	uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
	uint32_t stackPointer;
	uint32_t returnAddress;
	struct {
		uint32_t low, high;
	} d8, d9, d10, d11, d12, d13, d14, d15;
	uint32_t fpscr;
};

_Static_assert(offsetof(struct CpuContext, d8) == 40, "VFP context offset");
_Static_assert(offsetof(struct CpuContext, fpscr) == 104, "FPSCR offset");
_Static_assert(sizeof(struct CpuContext) == 108, "CPU context extent");

int SceLibKernel_37F4ED04(struct CpuContext *context)
	__attribute__((returns_twice));
void SceLibKernel_DC277B4D(const struct CpuContext *context, int result)
	__attribute__((noreturn));

#endif
