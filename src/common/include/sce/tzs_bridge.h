/*
 * The TrustZone bridge (Sony's tzs_bridge.c): the areas the Non-secure
 * kernel (smsc_proxy) and the secure world (SceSblSmsched) pass monitor
 * call arguments and interrupt messages in, at the start of the sysroot
 * shared memory. The Non-secure kernel allocates argument areas, the
 * secure world interrupt areas; each side keeps its own free lists.
 */
#pragma once

#include <psp2kern/types.h>

#define TZS_BRIDGE_AREA_SIZE		0x80
#define TZS_BRIDGE_AREAS		8

typedef struct SceSblTzsBridgeMemory {
	SceUInt8 argArea[TZS_BRIDGE_AREAS][TZS_BRIDGE_AREA_SIZE];
	SceUInt8 intrArea[TZS_BRIDGE_AREAS][TZS_BRIDGE_AREA_SIZE];	/* 0x400 */
	SceUInt32 intrIndex;		/* 0x800: the interrupt area pushed last */
} SceSblTzsBridgeMemory;

int sceSblTzsBridgeInit(void);
int sceSblTzsBridgeAlloc(void *areas, SceSize size, void **area, SceSize *areaSize);
int sceSblTzsBridgePush(void *areas, int index);
int sceSblTzsBridgePushArgArea(int index);
int sceSblTzsBridgePop(void *areas, int index, void **area, SceSize *areaSize);
int sceSblTzsBridgePopArgArea(int index, void **area, SceSize *areaSize);
int sceSblTzsBridgeFree(void *areas, int index);

/* the secure world's */
int sceSblTzsBridgeAllocIntrArea(SceSize size, void **area, SceSize *areaSize);
int sceSblTzsBridgePushIntrArea(int index);
int sceSblTzsBridgeFreeIntrArea(int index);

/* the Non-secure kernel's */
int sceSblTzsBridgeStart(void);
int sceSblTzsBridgeAllocArgArea(SceSize size, void **area, SceSize *areaSize);
int sceSblTzsBridgePopIntrArea(void **area, SceSize *areaSize);
int sceSblTzsBridgeFreeArgArea(int index);
