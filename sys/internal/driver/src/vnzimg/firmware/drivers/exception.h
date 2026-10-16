/*
 * Exception hooks (drivers/exception.c): a function run, on the core that
 * takes it, for an exception the kernel would otherwise handle.
 */
#pragma once

#include "api.h"

/* the exceptions (their vector slots: kind * 4, the debug exception's 0x18) */
#define VK_EXCEPTION_NMI		1
#define VK_EXCEPTION_RESERVED		2	/* a reserved instruction */
#define VK_EXCEPTION_ZERO_DIVIDE	3
#define VK_EXCEPTION_BREAK		4
#define VK_EXCEPTION_DEBUG		6
#define VK_EXCEPTIONS			9

/* the state saved when a hooked exception is taken (0xc0 bytes) */
typedef struct VkExceptionState {
	uint32_t registers[16];		/* $0 to $15 ($sp as it was) */
	uint32_t control[32];		/* the control registers, by number */
} VkExceptionState;

typedef void (*VkExceptionHandler)(uint32_t kind);

int32_t vkExceptionHook(uint32_t kind, VkExceptionHandler handler, VkExceptionHandler *previous);
void vkExceptionChain(void (*function)(void));
VkExceptionState *vkExceptionState(void);
uint32_t vkExceptionKind(void);
