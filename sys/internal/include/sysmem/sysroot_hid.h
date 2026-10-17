#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

/* hid's dumps for the debugger (NID 34281746): its device table and its internal lists */
typedef struct SysrootHidOps {
	SceSize size;
	int (*dumpDevices)(void *buffer, SceSize size, SceUInt32 *count);
	int (*dumpLists)(void *buffer, SceSize size, SceUInt32 *count);
} SysrootHidOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootHidOps) == 0xc, "Sysroot HID service table size");
_Static_assert(offsetof(SysrootHidOps, dumpLists) == 8, "Sysroot HID second service offset");
#endif

int SceSysrootForDriver_34281746(const SysrootHidOps *ops);
