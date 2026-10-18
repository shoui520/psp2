/*
 * SDK include compatibility: SceThreadmgr's interface is threadmgr/threadmgr.h (and the headers
 * of its parts). A vitasdk header that includes this one, or one of vitasdk's
 * psp2kern/kernel/threadmgr/ headers, gets it. Below, the vitasdk types kernel code still names
 * until each moves onto the thread manager's own.
 */
#pragma once

#include <threadmgr/threadmgr.h>
#include <threadmgr/mutex.h>
#include <threadmgr/lw_mutex.h>
#include <threadmgr/threadmgr_callback.h>
#include <threadmgr/threadmgr_event.h>
#include <threadmgr/threadmgr_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* vitasdk's fast_mutex.h: the fast mutex's work area, opaque (threadmgr's ThreadmgrFastMutex) */
typedef struct SceKernelFastMutex {
	SceUInt8 data[0x40];
} SceKernelFastMutex;

/* vitasdk's debugger.h: a thread's registers, as SceThreadmgrForDriver_5022689D gives them */
typedef struct SceArmCpuRegisters {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	uint32_t r12;
	uint32_t sp;
	uint32_t lr;
	uint32_t pc;
	uint32_t cpsr;
	uint32_t fpscr;
} SceArmCpuRegisters;

typedef struct SceThreadCpuRegisters {
	union {
		struct {
			SceArmCpuRegisters user;
			SceArmCpuRegisters kernel;
		};
		SceArmCpuRegisters entry[2];
	};
} SceThreadCpuRegisters;

/* vitasdk's debugger.h: the process and thread scheduled on this CPU */
typedef struct SceKernelThreadContextInfo {
	ScePID process_id;
	SceUID thread_id;
} SceKernelThreadContextInfo;

/* vitasdk's workqueues.h */
typedef int (*SceKernelWorkQueueWorkFunction)(void *args);

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelFastMutex) == 0x40, "SceKernelFastMutex size");
_Static_assert(sizeof(SceArmCpuRegisters) == 0x48, "SceArmCpuRegisters size");
_Static_assert(sizeof(SceThreadCpuRegisters) == 0x90, "SceThreadCpuRegisters size");
_Static_assert(sizeof(SceKernelThreadContextInfo) == 8, "SceKernelThreadContextInfo size");
#endif

#ifdef __cplusplus
}
#endif
