#pragma once

#include <stdint.h>
#include <psp2kern/types.h>
#include <syscon/syscon.h>

/* The updater's complete front/back panel operation table. */
typedef struct TouchUtilPanelOperations {
	int (*readLegacyFirmwareStatus)(uint32_t *status);
	int (*beginLegacyFirmwareUpdate)(uint32_t *status);
	int (*writeLegacyFirmwareChunk)(uint32_t block, uint32_t offset,
		const void *data, uint32_t size, uint32_t *status);
	int (*commitLegacyFirmwareBlock)(uint32_t block, uint32_t *status);
	int (*readLegacyAuxiliaryStatus)(uint32_t *status);
	int (*readLegacyConfigStatus)(uint32_t *status);
	int (*finishLegacyConfigUpdate)(void);
	int (*writeConfigData)(uint32_t offset, const void *data,
		uint32_t size);
	int (*readConfigData)(uint32_t offset, void *data, uint32_t size);
	int (*getFirmwareVersion)(uint32_t *version);
	int (*getConfigUpdateFlags)(uint8_t *flags);
	int (*beginConfigUpdate)(void);
	int (*finishConfigUpdate)(void);
	int (*getFirmwareUpdateState)(uint32_t *status);
	int (*beginFirmwareUpdate)(void);
	int (*writeFirmwareChunk)(uint32_t block, uint32_t offset,
		const void *data, uint32_t size, uint32_t *status);
	int (*commitFirmwareBlock)(uint32_t block, uint32_t *status);
	int (*getFirmwareUpdateInfo)(uint8_t *information);
	int (*readFirmwareAuxiliaryStatus)(uint32_t *status);
	int (*finishFirmwareUpdate)(uint32_t *status);
	int (*checkFirmwareBlock)(uint32_t block, uint32_t *status);
	int (*getConfigBlockInfo)(uint32_t index, uint32_t *status,
		uint16_t *first, uint16_t *second);
	int (*readConfigBlock)(uint32_t index, uint32_t offset, uint32_t size,
		void *data, uint32_t *status);
	int (*writeConfigBlock)(uint32_t index, uint32_t offset, uint32_t size,
		const void *data, uint32_t *status);
} TouchUtilPanelOperations;

_Static_assert(sizeof(TouchUtilPanelOperations) == 96,
	"touch panel operation table size");
_Static_assert(__builtin_offsetof(TouchUtilPanelOperations, getConfigUpdateFlags) == 40,
	"touch configuration flags query position");

extern const TouchUtilPanelOperations touchPanelOperations[2];
int TouchUtilGetConfigUpdateFlags(int port, unsigned int hardwareInfo, uint8_t *flags);
