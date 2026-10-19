/* Audio input interfaces absent from the pinned public SDK. */
#pragma once

#include <scetypes.h>

int sceAudioInSelectInput(int input);
int SceAudioInForDriver_3D164387(const SceInt16 *samples, int count);
int SceAudioInForDriver_005A2A31(void);
int SceAudioInForDriver_E4042F6F(int pid, int unusedArgument1, int unusedArgument2, int delay);
