/* SceKernelThreadMgr: complete kernel callback information. */
#pragma once

#include <stdint.h>
#include <psp2kern/types.h>

typedef struct ThreadmgrCallbackInfo {
	SceSize size;
	SceUID callbackId;
	char name[32];
	uint32_t attr;
	SceUID threadId;
	uintptr_t entry;
	uint32_t notifyId;
	uint32_t notifyCount;
	uint32_t notifyArg;
	void *common;
} ThreadmgrCallbackInfo;

_Static_assert(sizeof(ThreadmgrCallbackInfo) == 0x44, "callback info size");

int SceThreadmgrForDriver_FDC044AC(SceUID callbackId, ThreadmgrCallbackInfo *info);
