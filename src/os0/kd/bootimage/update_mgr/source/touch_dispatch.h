#pragma once

#include <stdint.h>

int SceSysconForDriver_48ED8981(void);
int SceSysconForDriver_060E55C1(void);
int SceSysconForDriver_9A28BEEF(void);
int SceSysconForDriver_8874EF45(void);
int SceSysconForDriver_5946B29B(void);
int SceSysconForDriver_10327C64(void);
int SceSysconForDriver_BC1D93D7(uint32_t *value);
int SceSysconForDriver_8B0B4A97(uint32_t *value);
int SceSysconForDriver_567865A3(uint32_t *value);
int SceSysconForDriver_AE127A96(uint32_t *value);
int SceSysconForDriver_41942B50(uint32_t *value);
int SceSysconForDriver_2A61ACE1(uint32_t *value);
int SceSysconForDriver_998ECD6D(uint32_t *value);
int SceSysconForDriver_CEE2B8F7(uint32_t *value);
int SceSysconForDriver_9B6B8BB9(SceUInt32 *value);
int SceSysconForDriver_C4A61241(SceUInt32 *value);
int SceSysconForDriver_0396BA09(void);
int SceSysconForDriver_23558805(void);
int SceSysconForDriver_B3960346(uint8_t *value);
int SceSysconForDriver_830A9347(uint8_t *value);
int SceSysconForDriver_B9835A63(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_CD6A6BC7(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_B63AF451(uint32_t offset, uint32_t *replyLength);
int SceSysconForDriver_8EBA8A26(uint32_t offset, uint32_t *replyLength);
int SceSysconForDriver_D9B11C0A(uint32_t *value);
int SceSysconForDriver_FB37D7AD(uint32_t *value);
int SceSysconForDriver_856DFC0E(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_E38045F5(uint32_t offset, const void *data, uint32_t size);
int SceSysconForDriver_1C5D466C(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_6D408E30(uint32_t offset, void *data, uint32_t size);
int SceSysconForDriver_4DFA12FB(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_3B06F619(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength);
int SceSysconForDriver_CA203A5F(uint32_t first, uint32_t second, uint32_t size,
			      void *reply, uint32_t *replyLength);
int SceSysconForDriver_55B6FEF5(uint32_t first, uint32_t second, uint32_t size,
			      void *reply, uint32_t *replyLength);
int SceSysconForDriver_94418CB4(uint32_t first, uint32_t second, uint32_t size,
			      const void *data, uint32_t *replyLength);
int SceSysconForDriver_7EC67149(uint32_t first, uint32_t second, uint32_t size,
			      const void *data, uint32_t *replyLength);
int SceSysconForDriver_68366D29(uint32_t index, uint32_t *value);
int SceSysconForDriver_2BCE7F5A(uint32_t index, uint32_t *value);
int SceSysconForDriver_6450D6BE(uint32_t index, uint32_t *value);
int SceSysconForDriver_16A466E7(uint32_t index, uint32_t *value);
int SceSysconForDriver_F162D3D6(uint32_t *value);
int SceSysconForDriver_25144105(uint32_t *value);
int SceSysconForDriver_9C840439(uint32_t *value);
int SceSysconForDriver_D632D197(uint32_t *value);
int SceSysconForDriver_9A4304F2(uint8_t *information);
int SceSysconForDriver_C2955814(uint8_t *information);
int SceSysconForDriver_9962CE6F(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second);
int SceSysconForDriver_31B8F7BE(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second);

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
