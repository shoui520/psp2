#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int SceUsbAudioForDriver_23A07D29(int callback);
int SceUsbAudioForDriver_863B0617(unsigned id, void *buffer, unsigned size);
int SceUsbAudioForDriver_96EC3E18(unsigned *list, int size);
int SceUsbAudioForDriver_974DBE03(int callback);
int SceUsbAudioForDriver_EFB4E3E4(unsigned id, void *buffer, unsigned size);
