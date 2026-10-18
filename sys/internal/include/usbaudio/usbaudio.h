#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

/* vitasdk: ksceUsbAudioUnregisterCallback */
int SceUsbAudioForDriver_23A07D29(int callback);
/* vitasdk: ksceUsbAudioInput */
int SceUsbAudioForDriver_863B0617(unsigned id, void *buffer, unsigned size);
/* vitasdk: ksceUsbAudioGetDeviceIdList */
int SceUsbAudioForDriver_96EC3E18(unsigned *list, int size);
int SceUsbAudioForDriver_974DBE03(int callback);	/* vitasdk: ksceUsbAudioRegisterCallback */
/* vitasdk: ksceUsbAudioInput2 */
int SceUsbAudioForDriver_EFB4E3E4(unsigned id, void *buffer, unsigned size);
