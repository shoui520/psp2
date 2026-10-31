#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

typedef struct HidCollectionInfo {
	SceSize size;
	int id;
	int type;
} HidCollectionInfo;

typedef struct HidDeviceInfo {
	SceSize size;
	unsigned int transportType;
	char name[256];
	uint8_t untouchedAfterName[4];
	uint32_t mac0;
	uint32_t mac1;
	uint16_t vendorId;
	uint16_t productId;
	uint16_t version;
	uint8_t primary;
	uint8_t untouchedAfterPrimary;
	HidCollectionInfo collections[24];
} HidDeviceInfo;

typedef struct HidHintCallbackInfo {
	SceSize size;
	unsigned int deviceType;
	unsigned int hintType;
	SceUID callbackId;
	SceUID pid;
} HidHintCallbackInfo;

#ifndef __cplusplus
_Static_assert(sizeof(HidCollectionInfo) == 12, "HID collection diagnostic record");
_Static_assert(sizeof(HidDeviceInfo) == 572, "HID device diagnostic stride");
_Static_assert(offsetof(HidDeviceInfo, mac0) == 268, "HID diagnostic address");
_Static_assert(offsetof(HidDeviceInfo, collections) == 284, "HID diagnostic collections");
_Static_assert(sizeof(HidHintCallbackInfo) == 20, "HID callback diagnostic record");
#endif

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
