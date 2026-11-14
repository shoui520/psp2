/*
 * Exception hooks (drivers/exception.c): a function run, on the core that
 * takes it, for an exception the kernel would otherwise handle.
 */
#pragma once

#include "api.h"

/* the exceptions (their vector slots: kind * 4, the debug exception's 0x18) */
#define VNZ_KERNEL_EXCEPTION_NMI		1
#define VNZ_KERNEL_EXCEPTION_RESERVED		2	/* a reserved instruction */
#define VNZ_KERNEL_EXCEPTION_ZERO_DIVIDE	3
#define VNZ_KERNEL_EXCEPTION_BREAK		4
#define VNZ_KERNEL_EXCEPTION_DEBUG		6
#define VNZ_KERNEL_EXCEPTIONS			9

/* the state saved when a hooked exception is taken (0xc0 bytes) */
typedef struct VnzKernelExceptionState {
	uint32_t registers[16];		/* $0 to $15 ($sp as it was) */
	uint32_t control[32];		/* the control registers, by number */
} VnzKernelExceptionState;

typedef void (*VnzKernelExceptionHandler)(uint32_t kind);

int32_t vnzKernelExceptionHook(uint32_t kind, VnzKernelExceptionHandler handler,
	VnzKernelExceptionHandler *previous);
void vnzKernelExceptionChain(void (*function)(void));
VnzKernelExceptionState *vnzKernelExceptionState(void);
uint32_t vnzKernelExceptionKind(void);
