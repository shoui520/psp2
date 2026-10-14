#ifndef BBMC_MODEM_CONFIGURATION_H
#define BBMC_MODEM_CONFIGURATION_H

#include <stddef.h>
#include <psp2kern/types.h>

/*
 * The complete 512-byte IdStorage leaf 0x113. Only configurationMode is
 * interpreted by this module; the other serialized modem metadata is opaque.
 */
typedef struct BbmcModemConfigurationLeaf {
	SceUInt8 encodedPrefix[216];
	SceUInt32 configurationMode;
	SceUInt8 encodedSuffix[292];
} BbmcModemConfigurationLeaf;

_Static_assert(sizeof(BbmcModemConfigurationLeaf) == 512,
	"BBMC modem configuration leaf size");
_Static_assert(__alignof__(BbmcModemConfigurationLeaf) == 4,
	"BBMC modem configuration leaf alignment");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, encodedPrefix) == 0,
	"BBMC modem configuration prefix layout");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, configurationMode) == 216,
	"BBMC modem configuration mode layout");
_Static_assert(offsetof(BbmcModemConfigurationLeaf, encodedSuffix) == 220,
	"BBMC modem configuration suffix layout");

#endif
