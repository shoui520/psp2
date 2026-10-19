/* Audio process-local storage, initialized and released by audio.c. */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>
#include <sysmem/list.h>

typedef struct AudioVolumeRamp {
	int8_t direction;
	int16_t volume;
	int32_t target;
	int32_t current;
	int32_t step;
} AudioVolumeRamp;

typedef struct AudioMappedBuffer {
	SceUID mapping;
	int16_t *cursor;
	uint16_t remaining;
	uint8_t bytesPerFrame;
} AudioMappedBuffer;

typedef struct AudioBufferConfig {
	uint16_t len;
	uint8_t bytesPerFrame;
} AudioBufferConfig;

typedef struct AudioMainPort {
	AudioMappedBuffer current;
	AudioMappedBuffer queued;
	AudioBufferConfig config;
	uint16_t volume[2];
} AudioMainPort;

typedef struct AudioDirectPort {
	AudioVolumeRamp ramp;
	uint8_t adoptState;
	uint8_t bytesPerFrame;
	uint8_t alcMode;
	uint8_t eventMask;
	int8_t channel;
	int8_t compress;
	uint16_t len;
	uint16_t remaining;
	uint16_t volume[2];
	uint16_t freq;
	uint32_t equalizer;
	uint32_t deadline;
} AudioDirectPort;

typedef struct AudioProcessLocalStorage {
	ListNode link;
	AudioVolumeRamp ramp;
	uint8_t adopt;
	AudioMainPort main[9];
	SceUID eventFlag;
	AudioDirectPort direct[2];
} AudioProcessLocalStorage;

_Static_assert(sizeof(AudioVolumeRamp) == 16, "audio gain ramp");
_Static_assert(sizeof(AudioMappedBuffer) == 12, "audio mapped buffer");
_Static_assert(sizeof(AudioMainPort) == 32, "audio main port");
_Static_assert(sizeof(AudioDirectPort) == 40, "audio direct port");
_Static_assert(offsetof(AudioProcessLocalStorage, main) == 28, "audio main ports");
_Static_assert(offsetof(AudioProcessLocalStorage, eventFlag) == 316, "audio event");
_Static_assert(offsetof(AudioProcessLocalStorage, direct) == 320, "audio direct ports");
_Static_assert(sizeof(AudioProcessLocalStorage) == 400, "audio process storage");

/* i2s.c */
unsigned int audioGetDmaModeCommand(void);
unsigned int audioGetI2sDmaCommand(void);
int audioInitializeI2s(void);
int audioEnableI2sLocked(int port);
uintptr_t audioGetI2sDataAddress(int port);

/* mixer.c */
int _sceAudiodrvMixer(SceSize arglen, void *argp);
uint32_t audioWaitForPlaybackDeadline(uint32_t deadline, uint32_t freq, uint32_t samples);
int audioDispatchDmaBuffer(uintptr_t sourceAddress, int channel, unsigned int wordCount);
int audioTransferDmaBuffer(uintptr_t sourceAddress, unsigned int wordCount);
int audioInitializeHardware(void);
int audioPrepareSuspend(void);
int audioResumeHardware(void);
int audioSetSuspended(void);

/* spdif.c */
int audioInitializeSpdif(void);
int audioPrepareSuspendHook(void);
