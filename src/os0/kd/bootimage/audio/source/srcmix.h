#ifndef SCE_AUDIO_SRCMIX_H
#define SCE_AUDIO_SRCMIX_H

#include <stddef.h>
#include <stdint.h>
#include <sce/dmacmgr.h>
#include <psp2kern/kernel/cpu/spinlock.h>

/* The three sample-rate converter/mixers' 4 KiB windows (SceSrcMix<n>Reg). */
#define SRCMIX_REG_STRIDE	0x10000
#define SRCMIX_REG_BASE(n)	(0xe04a0000 + (n) * SRCMIX_REG_STRIDE)
#define SRCMIX_REG_SIZE		0x1000
/* SrcMix0's mixed output, which the capture DMA reads */
#define SRCMIX0_OUTPUT_DATA	(SRCMIX_REG_BASE(0) + 0x240)

/* Observed registers in each mapped 4 KiB SrcMix window. */
struct AudioSrcMixRegisters {
	volatile uint32_t command;
	uint32_t reserved[31];
	volatile uint32_t control;
	volatile uint32_t status;
	volatile uint32_t volumeLeft;
	volatile uint32_t volumeRight;
	volatile uint32_t transmitControl;
	volatile uint32_t secondTransmitControl;
	volatile uint32_t captureControl;
	volatile uint32_t auxiliaryTransmitControl;
};

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
	struct AudioSrcMixRegisters *registers;
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
