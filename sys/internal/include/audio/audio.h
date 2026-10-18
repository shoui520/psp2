/* Audio driver interfaces absent from the pinned public SDK. */
#pragma once

#include <scetypes.h>

int SceAudioForDriver_134C96C1(int freq);
int SceAudioForDriver_FC375BAC(int mode);
int SceAudioForDriver_81EB0AE5(int channel, void *buffer, SceUInt32 wordCount);
int SceAudioForDriver_15FB30DF(unsigned int volume);
int SceAudioForDriver_AC9383D0(unsigned int channel, unsigned int mode);
