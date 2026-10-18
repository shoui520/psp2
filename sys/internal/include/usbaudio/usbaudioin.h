#pragma once

#include <scetypes.h>
#include <stdint.h>

/* the error codes */
#define USBAUDIO_IN_ERROR_INVALID_ARGUMENT			0x803E0001
#define USBAUDIO_IN_ERROR_DUPLICATE_ID				0x803E0002
#define USBAUDIO_IN_ERROR_NO_MEMORY				0x803E0003
#define USBAUDIO_IN_ERROR_DEVICE_NOT_FOUND			0x803E0004
#define USBAUDIO_IN_ERROR_NOT_SUPPORTED				0x803E0005
#define USBAUDIO_IN_ERROR_CANNOT_GET_PORT_OWNERSHIP		0x803E0006
#define USBAUDIO_IN_ERROR_PORT_IS_ALREADY_OPENED		0x803E0007
#define USBAUDIO_IN_ERROR_PROCESS_HAS_NOT_A_DEVICE_OWNERSHIP	0x803E0008
#define USBAUDIO_IN_ERROR_FAILED_TO_READ_STREAM			0x803E0009
#define USBAUDIO_IN_ERROR_DEVICE_WAS_HALTED			0x803E000A
#define USBAUDIO_IN_ERROR_NO_DATA_TO_READ			0x803E000B
#define USBAUDIO_IN_ERROR_FAILED_TO_COPY_BUFFER			0x803E000C
#define USBAUDIO_IN_ERROR_FAILED_TO_REQUEST_ISOCHRONOUS		0x803E000D
#define USBAUDIO_IN_ERROR_TIMEOUT				0x803E000E
#define USBAUDIO_IN_ERROR_PROCESS_CANNOT_OPEN_MORE_DEVICE	0x803E000F

/* sceUsbAudioInGetDeviceInformation's record: the device descriptor's vendor and product */
typedef struct AudioInDeviceInfo {
	uint16_t vendor;
	uint16_t product;
	SceUInt32 reserved[5];
} AudioInDeviceInfo;

_Static_assert(sizeof(AudioInDeviceInfo) == 0x18, "device information size");

int SceUsbAudioIn_198A7C33(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInCloseDevice(SceUInt32 id);
SceInt32 sceUsbAudioInGetDeviceIdList(SceUInt32 *list, SceUInt32 *count, SceUInt32 size);
SceInt32 sceUsbAudioInGetDeviceInformation(SceUInt32 id, AudioInDeviceInfo *out);
SceInt32 sceUsbAudioInGetMaxValueOfVolume(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInGetMinValueOfVolume(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInInput(SceUInt32 id, void *buffer);
SceInt32 sceUsbAudioInOpenDevice(SceUInt32 id, int bits, int rate);
SceInt32 sceUsbAudioInSetCurrentValueOfVolume(SceUInt32 id, SceUInt32 value);
