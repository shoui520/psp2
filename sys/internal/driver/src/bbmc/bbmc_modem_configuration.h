#ifndef BBMC_MODEM_CONFIGURATION_H
#define BBMC_MODEM_CONFIGURATION_H

#include <stddef.h>
#include <scetypes.h>
#include "qmi_information.h"

/*
 * The complete 512-byte IdStorage leaf 0x113. configurationMode is read back at
 * boot; SceBbmcForDriver_07ADDA90 (sceBbmcGetIdStorageFrom3GModule) fills the
 * other named fields from what the modem reported. The rest is opaque.
 */
typedef struct BbmcModemConfigurationLeaf {
	BbmcImeiString imei;
	SceUInt8 reserved0[104];
	SceUInt32 firmwareBuildSelection;
	SceUInt32 controlKeyReported;
	SceUInt8 reserved1[48];
	SceInt32 firmwareRevisionSelection;
	SceUInt32 configurationReplySeen;
	SceUInt32 replyOperationMode;
	SceUInt32 runningStorageIndex;
	SceUInt32 completionPending;
	SceUInt8 reserved2[4];
	SceUInt32 configurationMode;
	SceUInt32 imeiSoftwareVersion;
	SceUInt8 reserved3[288];
} BbmcModemConfigurationLeaf;

_Static_assert(sizeof(BbmcModemConfigurationLeaf) == 512,
	"BBMC modem configuration leaf size");
_Static_assert(__alignof__(BbmcModemConfigurationLeaf) == 4,
	"BBMC modem configuration leaf alignment");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, firmwareBuildSelection) == 136,
	"BBMC modem configuration build selection");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, firmwareRevisionSelection) == 192,
	"BBMC modem configuration revision selection");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, configurationMode) == 216,
	"BBMC modem configuration mode layout");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, imeiSoftwareVersion) == 220,
	"BBMC modem configuration IMEI software version");

#endif
