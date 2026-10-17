/*
 * SceKernelIntrMgr's interrupt handler records and per-core nesting state,
 * one 0x4200-byte block in both builds: os0:kd/intrmgr.skprx and the secure
 * world's slb2:kbl/SceKernelIntrMgr (at the start of its data segment, where
 * its FIQ entry reads them).
 */
#pragma once

#include <psp2kern/types.h>
#include <sysmem/cpu.h>
#include <kernel/error.h>
#include <intrmgr/gic_regs.h>

#define INTR_COUNT 256

typedef int (*SubIntrCallback)(unsigned int, unsigned int, ...);

typedef struct IntrHandler {
	void *handler;
	SceUInt16 intrCode;
	SceUInt16 targetCpu;
	void *arg;
	SceUInt16 priority;
	SceUInt8 type;
	SceUInt8 configuration;
	char *name;
	void *oldHandler;
	SceUInt32 reserved[2];
	SceUInt32 count[4];
	SceKernelSpinlock lock;
	struct IntrHandler *sub;
	SubIntrCallback *callbacks;
	SceUInt32 subCount;
} IntrHandler;

typedef struct IntrCpuState {
	SceUInt32 nesting;
	SceUInt32 padding[31];		/* one 0x80 line per CPU */
} IntrCpuState;

/* Handler records and their per-CPU nesting state share the native base. */
typedef struct IntrState {
	IntrHandler handlers[INTR_COUNT];
	IntrCpuState cpu[4];
} IntrState;

_Static_assert(sizeof(IntrHandler) == 64, "interrupt record ABI");
_Static_assert(sizeof(IntrState) == 0x4200, "handler and CPU state ABI");

/* the distributor as each build maps it */
extern GicDistributor *volatile g_intrDistributor;

/* name unknown: update the interrupt configuration field. */
static inline void SetConfiguration(unsigned int intrCode, unsigned int config)
{
	unsigned int index = intrCode >> 4;
	unsigned int shift = (intrCode & 15) * 2;
	unsigned int value = g_intrDistributor->config[index];

	g_intrDistributor->config[index] = value ^ (((value >> shift) ^ config) & 3) << shift;
}

/* name unknown: convert the two accepted CPU mask encodings. */
static inline __attribute__((always_inline)) int TargetMask(unsigned int target, unsigned int *mask)
{
	if (!target)
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	if (target & 0xf0000) {
		if (target & 15)
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		*mask = target >> 16;
	} else if (target & 15) {
		if ((int)target < 0)
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		*mask = target;
	} else {
		*mask = target >> 16;
	}
	return 0;
}
