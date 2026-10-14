/* The work area cleared and constructed by audioModuleStart. */
#pragma once

#include "audio_private.h"
#include "alc.h"
#include "equalizer.h"
#include "srcmix.h"
#include "i2s.h"
#include "spdif.h"
#include "aclkgen.h"

struct AudioWork {
	int16_t mixerBuffers[2][1024][2];
	int16_t outputBuffers[2][2][1024][2];
	union AudioCaptureSampleBank captureSampleBanks[2];
	uintptr_t outputPhysicalAddress[2][2];
	AudioAlc alc;
	AudioEqualizer equalizer;
	uint32_t equalizerSetting;
	struct AudioDmaState dmaStates[3];
	uint8_t outputBufferIndex[2];
	signed char mixerIdle;
	signed char suspended;
	uint32_t mixerDeadline;
	uint16_t playbackRate;
	uint16_t captureRate;
	AudioProcessLocalStorage *processList;
	uint32_t reserved[2];
	AudioDirectPort *adoptedPorts[2];
	AudioDirectPort *adoptingPorts[2];
	uint16_t hardwareVolume;
	signed char adoptMode;
	signed char legacyTitleCompatibility;
	signed char invalidPortDelayCompatibility;
	unsigned char eightKhzMode;
	unsigned char modeOneEnabled;
	int8_t captureSampleBankIndex;
	SceUID eventFlag;
	void (*portOpenCallback)(SceUID pid);
	void (*portCloseCallback)(SceUID pid);
	int storageKey;
	struct SceAclkgenRegisters *clockRegisters;
	struct AudioSpdifRegisters *spdifRegisters;
	struct AudioI2sRegisters *i2sRegisters[8];
	SceKernelSpinlock i2sLock;
	uint8_t reservedTail;
	uint8_t i2sEnabledMask;
	uint8_t selectedI2sPort;
	uint8_t dmaCommandMode;
};

_Static_assert(sizeof(struct AudioWork) == 0xbd24, "audio work area");
_Static_assert(offsetof(struct AudioWork, outputBuffers) == 0x2000,
	"audio output buffers");
_Static_assert(offsetof(struct AudioWork, alc) == 0x76d0, "audio ALC");
_Static_assert(offsetof(struct AudioWork, dmaStates) == 0xb870, "audio DMA states");
_Static_assert(offsetof(struct AudioWork, eventFlag) == 0xbce4, "audio event flag");
_Static_assert(offsetof(struct AudioWork, i2sLock) == 0xbd1c, "audio I2S lock");

extern struct AudioWork audioWork;

#define audioMixerBuffers (audioWork.mixerBuffers)
#define audioOutputBuffers (audioWork.outputBuffers)
#define audioCaptureSampleBanks (audioWork.captureSampleBanks)
#define audioOutputPhysicalAddress (audioWork.outputPhysicalAddress)
#define audioAlc (audioWork.alc)
#define audioEqualizer (audioWork.equalizer)
#define audioEqualizerSetting (audioWork.equalizerSetting)
#define audioDmaStates (audioWork.dmaStates)
#define audioOutputBufferIndex (audioWork.outputBufferIndex)
#define audioMixerIdle (audioWork.mixerIdle)
#define audioSuspended (audioWork.suspended)
#define audioMixerDeadline (audioWork.mixerDeadline)
#define audioPlaybackRate (audioWork.playbackRate)
#define audioCaptureRate (audioWork.captureRate)
#define audioProcessList (audioWork.processList)
#define audioAdoptedPorts (audioWork.adoptedPorts)
#define audioAdoptingPorts (audioWork.adoptingPorts)
#define audioHardwareVolume (audioWork.hardwareVolume)
#define audioAdoptMode (audioWork.adoptMode)
#define audioLegacyTitleCompatibility (audioWork.legacyTitleCompatibility)
#define audioInvalidPortDelayCompatibility (audioWork.invalidPortDelayCompatibility)
#define audioEightKhzMode (audioWork.eightKhzMode)
#define audioModeOneEnabled (audioWork.modeOneEnabled)
#define audioCaptureSampleBankIndex (audioWork.captureSampleBankIndex)
#define audioEventFlag (audioWork.eventFlag)
#define audioStorageKey (audioWork.storageKey)
#define audioClockRegisters (audioWork.clockRegisters)
#define audioSpdifRegisters (audioWork.spdifRegisters)
#define audioI2sRegisters (audioWork.i2sRegisters)
#define audioI2sLock (audioWork.i2sLock)
#define audioI2sEnabledMask (audioWork.i2sEnabledMask)
#define audioSelectedI2sPort (audioWork.selectedI2sPort)
#define audioDmaCommandMode (audioWork.dmaCommandMode)
#define audioPortOpenCallback (audioWork.portOpenCallback)
#define audioPortCloseCallback (audioWork.portCloseCallback)
