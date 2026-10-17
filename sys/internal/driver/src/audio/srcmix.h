#ifndef SCE_AUDIO_SRCMIX_H
#define SCE_AUDIO_SRCMIX_H

#include <stddef.h>
#include <stdint.h>
#include "audio_regs.h"
#include <dmacmgr/dmacmgr.h>
#include <psp2kern/kernel/cpu/spinlock.h>

struct AudioCaptureSampleBlock {
	uint32_t samples[4];
};

union AudioCaptureSampleBank {
	int16_t samples[1456];
	uint32_t words[728];
	struct AudioCaptureSampleBlock blocks[182];
};

struct AudioDmaTransfer {
	SceKernelDmaOpTag tags[3];
	int state;
	SceKernelDmaOpId opId;
	uint32_t command;
};

struct AudioDmaState {
	struct AudioDmaTransfer transfers[2];
	SceKernelDmaOpTag captureTags[3];
	int captureState;
	SceKernelDmaOpId captureOpId;
	uint32_t captureCommand;
	SceUID eventFlag;
	SceSrcMixRegisters *registers;
	int8_t sourceMode;
	int8_t previousSourceMode;
	int8_t transmitSourceMode;
	uint16_t transmitFrames;
	uint16_t captureFrames;
	uint16_t previousCaptureFrames;
	uint16_t currentCaptureFrames;
	int16_t *previousCaptureBuffer;
	int16_t *currentCaptureBuffer;
	void *pendingTransmitBuffer;
	void *pendingCaptureBuffer;
	SceKernelSpinlock lock;
};

_Static_assert(sizeof(struct AudioDmaTransfer) == 108, "DMA transfer layout");
_Static_assert(sizeof(struct AudioDmaState) == 364, "DMA state layout");
_Static_assert(offsetof(struct AudioDmaState, transmitFrames) == 336,
	"DMA frame counts");

extern const SceKernelDmaOpChainParam audioDmaChainParam;

#endif
