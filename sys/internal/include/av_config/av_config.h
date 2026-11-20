/* SceAVConfig's driver interface. */
#pragma once

#include <scetypes.h>
#include <hdmi/hdmi.h>

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
int sceAVConfigSetAutoSuspend2(unsigned int interval);
int sceAVConfigClearAutoSuspend2(void);

/* the display's brightness, its maximum, the display turned on, and the auto suspend */
int sceAVConfigGetDisplayMaxBrightness(unsigned int *brightness);

/* the connected HDMI monitor: its EDID's product bytes and name, and the video formats it takes */
typedef struct AvConfigMonitorInfo {
	SceSize size;
	SceUInt8 productId[10];
	SceUInt8 blockCount;
	char monitorName[13];
	SceUInt8 reserved[3];
	SceUInt8 padding;
	struct {
		SceUInt32 supported;
		SceUInt32 reserved;
	} videoFormats[4];
} AvConfigMonitorInfo;

#ifndef __cplusplus
_Static_assert(sizeof(AvConfigMonitorInfo) == 64, "monitor information size");
_Static_assert(__builtin_offsetof(AvConfigMonitorInfo, videoFormats) == 32,
	       "monitor video formats");
#endif

int sceAVConfigHdmiGetMonitorInfo(AvConfigMonitorInfo *info);
int sceAVConfigHdmiGetCecInfo(CecStatistics *statistics);
int sceAVConfigHdmiClearCecInfo(void);
int sceAVConfigSetDisplayBrightness(unsigned int brightness);
int sceAVConfigDisplayOn(void);
int sceAVConfigSetAutoSuspend(unsigned int interval);

#ifdef __cplusplus
}
#endif
