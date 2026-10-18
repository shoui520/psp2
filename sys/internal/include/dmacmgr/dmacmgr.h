/* DMA completion callbacks, including the completed byte count. */
#pragma once

#include <psp2kern/kernel/dmac.h>

typedef int (*DmaOpCallback)(SceKernelDmaOpId opid, SceUInt32 stat,
	void *pUserData, const SceKernelDmaOpTag *pTag, SceSize completed);

int sceKernelDmaOpSetCallback(SceKernelDmaOpId opid, DmaOpCallback callback,
	void *pUserData);

int SceDmacmgrForDriver_893FCB80(void);
int SceDmacmgrForDriver_BCBB5E85(int dmac, SceUInt32 channel);
int _DmaOpSetupDirect(SceKernelDmaOpId opid, SceKernelDmaOpDirectParam *pParam, SceKernelDmaOpFlag flag);
int sceDmacMemcpy(void *dst, const void *src, SceSize size);
int sceDmacMemset(void *dst, int c, SceSize size);
SceKernelDmaOpId sceKernelDmaOpAlloc(const char *name);
int sceKernelDmaOpAssign(SceKernelDmaOpId opid, SceKernelDmacId dmacId, SceUInt32 channel);
int sceKernelDmaOpConcatenate(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag, SceKernelDmaOpFlag flag);
int sceKernelDmaOpDeQueue(SceKernelDmaOpId opid);
int sceKernelDmaOpEnQueue(SceKernelDmaOpId opid);
int sceKernelDmaOpFree(SceKernelDmaOpId opid);
int sceKernelDmaOpQuit(SceKernelDmaOpId opid);
int sceKernelDmaOpSetupChain(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag, SceKernelDmaOpChainParam *pParam, SceKernelDmaOpFlag flag);
int sceKernelDmaOpSync(SceKernelDmaOpId opid, SceKernelDmaOpSyncMode syncMode, SceUInt32 timeout, const SceKernelDmaOpTag **ppErrorTag);
int sceKernelDmacMemcpy(void *dst, const void *src, SceSize size);
int sceKernelDmacMemset(void *dst, int c, SceSize size);
