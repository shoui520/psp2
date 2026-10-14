#ifndef UPDATE_MGR_SPKG_PERIPHERALS_H
#define UPDATE_MGR_SPKG_PERIPHERALS_H

#include "update_internal.h"

int SysconUtilCheckFirmwareType(uint64_t hardwareInfo, int kind);
int CpUtilCheckFirmwareType(uint64_t hardwareInfo);
int MotionUtilCheckFirmwareType(uint64_t hardwareInfo);
int TouchUtilCheckFirmwareType(uint64_t hardwareInfo, unsigned int kind);
int BicUtilCheckFirmwareType(uint64_t hardwareInfo);
int BicUtilCheckFirmwareTypeCommon(uint64_t hardwareInfo, unsigned int kind);
int BicUtilCheckDataFlashType(uint64_t hardwareInfo);
int BbmcUtilCheckFirmwareType(uint64_t hardwareInfo);
int BbmcUtilGetFirmwareVersion(uint32_t *version);
uint32_t TouchUtilGetConfigVersionByHardware(uint64_t hardwareInfo);
uint32_t TouchUtilGetFirmwareVersionByHardware(uint64_t hardwareInfo);
int TouchUtilRequiresNewerFirmware(uint64_t hardwareInfo);
int SysconUtilGetConfigVersion(uint32_t *version);
int SysconUtilGetDownloaderVersion(uint32_t *version);
int SysconUtilWriteFirmware(int kind, const void *buffer, uint64_t size,
		unsigned int nodeType, UpdateUtilNodeStatus *status);
int CpUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
		unsigned int nodeType, UpdateUtilNodeStatus *status);
int BicUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
		unsigned int nodeType, UpdateUtilNodeStatus *status);
int MotionUtilWriteFirmware(uint64_t hardwareInfo, const void *buffer,
		uint64_t size, uint32_t version, unsigned int nodeType,
		UpdateUtilNodeStatus *status);
int TouchUtilWriteFirmware(uint64_t hardwareInfo, const void *buffer,
		uint64_t size, uint32_t version, unsigned int nodeType,
		UpdateUtilNodeStatus *status);
int TouchUtilWriteConfig(uint64_t hardwareInfo, const void *buffer,
		uint64_t size, uint32_t version, unsigned int nodeType,
		UpdateUtilNodeStatus *status);
int BbmcUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
		uint64_t chunkIndex, uint64_t chunkCount, unsigned int nodeType,
		UpdateUtilNodeStatus *status);

#endif
