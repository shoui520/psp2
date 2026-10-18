#pragma once

#include <scetypes.h>
#include <stdint.h>

int SceUsbAudioIn_198A7C33(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInCloseDevice(SceUInt32 id);
SceInt32 sceUsbAudioInGetMaxValueOfVolume(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInGetMinValueOfVolume(SceUInt32 id, SceUInt32 *volume);
SceInt32 sceUsbAudioInInput(SceUInt32 id, void *buffer);
SceInt32 sceUsbAudioInOpenDevice(SceUInt32 id, int bits, int rate);
SceInt32 sceUsbAudioInSetCurrentValueOfVolume(SceUInt32 id, SceUInt32 value);
