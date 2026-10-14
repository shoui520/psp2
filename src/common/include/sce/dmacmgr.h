/* DMA completion callbacks, including the completed byte count. */
#pragma once

/* The pinned SDK declares an older four-argument callback interface. */
#define ksceKernelDmaOpSetCallback ksceKernelDmaOpSetCallback_vitasdk
#include <psp2kern/kernel/dmac.h>
#undef ksceKernelDmaOpSetCallback

typedef int (*DmaOpCallback)(SceKernelDmaOpId opid, SceUInt32 stat,
	void *pUserData, const SceKernelDmaOpTag *pTag, SceSize completed);

int ksceKernelDmaOpSetCallback(SceKernelDmaOpId opid, DmaOpCallback callback,
	void *pUserData);
