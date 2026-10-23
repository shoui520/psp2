/* Audio driver interfaces absent from the pinned public SDK. */
#pragma once

#include <stdint.h>
#include <scetypes.h>

int SceAudioForDriver_134C96C1(int freq);
int SceAudioForDriver_FC375BAC(int mode);
int SceAudioForDriver_81EB0AE5(int channel, void *buffer, SceUInt32 wordCount);
int SceAudioForDriver_15FB30DF(unsigned int volume);
int SceAudioForDriver_AC9383D0(unsigned int channel, unsigned int mode);
int SceAudioForDriver_3440C1CA(SceUID pid, int type, int adopt, int duration, int wait);
int SceAudioForDriver_629261CD(SceUID pid, int mask, unsigned int volume, int duration);
int sceAudioOutSetAdoptMode(int mode);
int SceAudioForDriver_75915003(uintptr_t sourceAddress, unsigned int wordCount);
int SceAudioForDriver_F07D34C2(void (*callback)(SceUID pid));
int SceAudioForDriver_E87902DF(void (*callback)(SceUID pid));
