#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

/* bt's services for the debugger (NID 5A4A7F7E) */
typedef struct SysrootBtOps {
	SceSize size;
	int (*getRegisteredInfo)(uint64_t deviceAddress, void *buffer, int maxCount);	/* SceBtForDriver_F86D25E2 */
	int (*getConnectingInfo)(uint64_t deviceAddress);	/* SceBtForDriver_E9936446 */
} SysrootBtOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootBtOps) == 0xc, "Sysroot Bluetooth service table size");
_Static_assert(offsetof(SysrootBtOps, getConnectingInfo) == 8,
	       "Sysroot Bluetooth second service offset");
#endif
