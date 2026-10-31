/* The modem updater (updater/updater_QMDM6200.c, updater/updater_seq_QMDM6200.c). */
#pragma once
#include <scetypes.h>
#include <kernel/types.h>
#include <kernel/iofilemgr.h>

/* the flags the download sequences open an update file with */
#define UD_IO_OPEN_FLAGS	(SCE_O_RDONLY | 0x04000000)

/* name unknown: open, close and size an update file in RAM or through iofilemgr */
SceUID udIoOpen(const char *path, int flags, SceMode mode);
int udIoClose(SceUID fd);
SceOff udIoGetSize(SceUID fd);
