#pragma once

#include <psp2kern/types.h>

/*
 * sceKernelGetProcessInfo's output (os0:kd/processmgr.skprx). vitasdk's
 * <psp2kern/kernel/processmgr.h> declares the same name with unknown fields; include
 * one or the other: this header is separate from processmgr/processmgr.h for that reason.
 */
typedef struct SceKernelProcessInfo {
	SceSize size;				/* 0xe8 */
	SceUID pid;
	SceUID addressSpace;
	SceUInt32 attr;
	SceUInt32 status;
	SceUID parentPid;
	SceUID budgetId;
	SceUInt32 cpuAffinity;
	SceUInt32 param;			/* SceProcessObject.infoParam */
	SceUID mainModule;
	void *entryPoint;
	SceUInt32 type;
	char name[0x20];
	void *objectHeap[2];
	SceUID entryHeap;
	SceUID abortThread;
	SceUInt32 abortCode;
	SceUInt32 libraryDBFlags;
	void *localStorage[32];		/* PROCESS_LOCAL_STORAGE_MAX (processmgr/process.h) */
} SceKernelProcessInfo;

_Static_assert(sizeof(SceKernelProcessInfo) == 0xe8, "SceKernelProcessInfo size");
