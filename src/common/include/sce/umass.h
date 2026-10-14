/* SceUsbMass driver entry points identified by the original diagnostic strings. */
#pragma once
#include <psp2kern/types.h>
#include <stddef.h>

typedef struct SceUsbMassDevInfo {
	SceUInt64 sectorCount;
	SceUInt32 blockSize;
	SceUInt32 writeProtected;
} SceUsbMassDevInfo;

_Static_assert(sizeof(SceUsbMassDevInfo) == 16, "SceUsbMassDevInfo");
_Static_assert(offsetof(SceUsbMassDevInfo, sectorCount) == 0,
	       "SceUsbMassDevInfo.sectorCount");
_Static_assert(offsetof(SceUsbMassDevInfo, blockSize) == 8,
	       "SceUsbMassDevInfo.blockSize");
_Static_assert(offsetof(SceUsbMassDevInfo, writeProtected) == 12,
	       "SceUsbMassDevInfo.writeProtected");

int _sceUsbMassGetDevInfo(SceUsbMassDevInfo *info);
int _sceUsbMassWriteSector(SceUInt32 sector, void *buffer, SceUInt32 count);
int _sceUsbMassReadSector(SceUInt32 sector, void *buffer, SceUInt32 count);
int sceKernelDelayThread(SceUInt32 usec);
