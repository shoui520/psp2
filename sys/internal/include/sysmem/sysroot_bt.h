#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>

/* bt's services for the debugger (NID 5A4A7F7E) */
typedef struct SysrootBtOps {
	SceSize size;
	int (*getRegisteredInfo)(uint64_t deviceAddress, void *buffer, int maxCount);	/* ksceBtGetRegisteredInfo */
	int (*getConnectingInfo)(uint64_t deviceAddress);	/* ksceBtGetConnectingInfo */
} SysrootBtOps;

_Static_assert(sizeof(SysrootBtOps) == 0xc, "Sysroot Bluetooth service table size");
_Static_assert(offsetof(SysrootBtOps, getConnectingInfo) == 8,
	       "Sysroot Bluetooth second service offset");
