/* Complete records returned by Bluetooth's registered-device query. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "sce/bt_pairing.h"

typedef struct SceBtRegisteredInfo {
	uint8_t mac[6];
	uint16_t macUpper;
	uint32_t bt_class;
	uint32_t profiles;
	uint32_t flags;
	uint16_t vid;
	uint16_t pid;
	uint16_t version;
	uint8_t primary;
	uint8_t reservedIdentity[5];
	char name[128];
	uint8_t reserved[96];
} SceBtRegisteredInfo;

_Static_assert(sizeof(SceBtRegisteredInfo) == 256, "SceBtRegisteredInfo");
_Static_assert(_Alignof(SceBtRegisteredInfo) == 4,
	       "SceBtRegisteredInfo alignment");
_Static_assert(offsetof(SceBtRegisteredInfo, mac) == 0, "registered MAC");
_Static_assert(offsetof(SceBtRegisteredInfo, macUpper) == 6,
	       "registered upper MAC bits");
_Static_assert(offsetof(SceBtRegisteredInfo, bt_class) == 8,
	       "registered device class");
_Static_assert(offsetof(SceBtRegisteredInfo, profiles) == 12,
	       "registered profiles");
_Static_assert(offsetof(SceBtRegisteredInfo, flags) == 16, "registered flags");
_Static_assert(offsetof(SceBtRegisteredInfo, vid) == 20, "registered VID");
_Static_assert(offsetof(SceBtRegisteredInfo, pid) == 22, "registered PID");
_Static_assert(offsetof(SceBtRegisteredInfo, version) == 24,
	       "registered version");
_Static_assert(offsetof(SceBtRegisteredInfo, primary) == 26,
	       "registered primary record");
_Static_assert(offsetof(SceBtRegisteredInfo, reservedIdentity) == 27,
	       "registered unused identity bytes");
_Static_assert(offsetof(SceBtRegisteredInfo, name) == 32, "registered name");
_Static_assert(offsetof(SceBtRegisteredInfo, reserved) == 160,
	       "registered unused tail");

/* The same batch-output ABI is used by SysrootBtOps. */
int ksceBtGetRegisteredInfo(uint64_t deviceAddress, void *buffer, int maxCount);

