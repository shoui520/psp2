/* SceDbgSdio and dependency declarations absent from the SDK headers. */
#pragma once
#include <scetypes.h>
#include <dmacmgr/dmacmgr.h>
#include <threadmgr/threadmgr.h>
#include <lowio/pervasive.h>
#include <sysmem/sysmem.h>

/* Original names unknown: SDIO function clock/reset setup. */

int sceDbgSdioInit(void);
int _sceDbgSdioModuleStart(SceSize args, void *argp);
/* Original names unknown: attach/detach a channel's mailbox interrupt. */
int SceDbgSdioForDriver_3D3FAF3A(SceUInt32 index, SceUID notify);
int SceDbgSdioForDriver_361E1BB6(SceUInt32 index);
int receiveData(SceUInt32 index, void *buffer, SceSize size);
int sceDbgSdioWrite(SceUInt32 index, const void *buffer, SceSize size);
