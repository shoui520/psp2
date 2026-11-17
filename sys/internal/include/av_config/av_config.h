/* SceAVConfig's driver interface. */
#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Display requests: a sound-exclusive application asks for the display at a
 * priority (index); its callers pass a fifth argument, which is not read.
 */
int SceAVConfigForDriver_2A2E9042(unsigned int index, unsigned int enabled,
		unsigned int mode, int blanked, int reserved);
int SceAVConfigForDriver_3817CDA1(unsigned int index, unsigned int enabled,
		unsigned int mode);
int SceAVConfigForDriver_91792D6F(unsigned int index);

/* the audio devices connected (a syscall SceAVConfig's user library passes on) */
int sceAVConfigGetConnectedAudioDevice(SceUInt32 *devices);

/* the auto suspend time while an application of the boot runs, and its reset (the shell's) */
int sceAVConfigSetAutoSuspend2(int seconds);
int sceAVConfigClearAutoSuspend2(void);

/* the display's brightness, its maximum, the display turned on, and the auto suspend */
int sceAVConfigGetDisplayMaxBrightness(unsigned int *brightness);
int sceAVConfigSetDisplayBrightness(unsigned int brightness);
int sceAVConfigDisplayOn(void);
int sceAVConfigSetAutoSuspend(unsigned int interval);

#ifdef __cplusplus
}
#endif
