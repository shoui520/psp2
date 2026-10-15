/*
 * SceKernelIntrMgr's interrupt handler records and per-core nesting state,
 * one 0x4200-byte block in both builds: os0:kd/intrmgr.skprx and the secure
 * world's slb2:kbl/SceKernelIntrMgr (at the start of its data segment, where
 * its FIQ entry reads them).
 */
#pragma once

#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>

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
