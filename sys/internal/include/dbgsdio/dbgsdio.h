/* SceDbgSdio and dependency declarations absent from the SDK headers. */
#pragma once
#include <psp2kern/types.h>
#include <psp2kern/kernel/dmac.h>
#include <threadmgr/threadmgr.h>
#include <lowio/pervasive.h>

/* name from the NID database: the kind of memory an address maps (3: mapped at its physical address) */
int ksceKernelGetPhysicalMemoryType(const void *address);

/* Original names unknown: SDIO function clock/reset setup. */

int sceDbgSdioInit(void);
int _sceDbgSdioModuleStart(SceSize args, void *argp);
/* Original names unknown: attach/detach a channel's mailbox interrupt. */
int SceDbgSdioForDriver_3D3FAF3A(SceUInt32 index, SceUID notify);
int SceDbgSdioForDriver_361E1BB6(SceUInt32 index);
int receiveData(SceUInt32 index, void *buffer, SceSize size);
int sceDbgSdioWrite(SceUInt32 index, const void *buffer, SceSize size);
