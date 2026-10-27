/* Sample-rate conversion and mixing helpers. */
#include <stdint.h>
#include <arm_neon.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include "srcmix.h"
#include "aclkgen.h"
#include "audio_work.h"
#include <lowio/pervasive.h>
#include <audioout.h>

const uint32_t audioDmaCommands[3][3] = {
	{ 0x1a80000, 0x1b00000, 0xc54000 },
	{ 0x1b80000, 0x58000, 0x5c000 },
	{ 0xc80000, 0xd00000, 0x2064000 }
};

const SceKernelDmaOpChainParam audioDmaChainParam = {
	.size = sizeof(SceKernelDmaOpChainParam),
	.coherencyMask = 0x3ffff,
	.setValue = 0
};

int audioStartCaptureDma(struct AudioDmaState *state);
int audioStartTransmitDma(struct AudioDmaState *state,
	SceSrcMixRegisters *registers);
int audioSetHardwareVolume(int channel, int left, int right);

/* Programs must be rebuilt for the SrcMix processor before this module links. */
extern const uint32_t audioSrcMixProgram[256];
extern const uint32_t audioSrcMixSecondaryProgram[1414];
extern const uint32_t audioSrcMixCoefficients[1024];
extern const uint32_t audioSrcMixSecondCoefficients[1814];

int SceAudioForDriver_37517CC9(uintptr_t destination, unsigned int wordCount)
{
	struct AudioDmaState *state = &audioDmaStates[2];
	struct AudioDmaTransfer *transfer = &state->transfers[1];
	SceKernelIntrStatus intr;
	int threshold = 0;
	int running, result;
	uintptr_t source;
	unsigned int length;

	if (destination & 127)
		return (int)0x80260105;
	if (wordCount <= 63 || (wordCount & 31))
		return (int)0x80260102;
	if (!destination)
		goto wait;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	running = transfer->state;
	if (running > 1) {
		result = (int)0x8026010a;
		goto unlock;
	}
	source = audioGetI2sDataAddress(0);
	length = 4 * wordCount - 128;
	transfer->tags[1].dst = (void *)(destination + length);
	transfer->tags[0].len = length + 0x5000000;
	transfer->tags[1].src = (const void *)source;
	transfer->tags[0].src = (const void *)source;
	transfer->tags[0].dst = (void *)destination;
	transfer->tags[1].len = 0x5000080;
	transfer->tags[0].cmd = 0x2034000;
	transfer->tags[1].cmd = 0x2036000;
	transfer->tags[1].blockSize = 128;
	transfer->tags[0].blockSize = 128;
	audioEnableI2sLocked(0);
	if (running == 1 &&
	    sceKernelDmaOpSync(transfer->opId, 1, 0, 0) > 0 &&
	    sceKernelDmaOpConcatenate(transfer->opId, transfer->tags, 16) >= 0) {
		transfer->state = 2;
	} else {
		result = sceKernelDmaOpSetupChain(transfer->opId, transfer->tags,
			(SceKernelDmaOpChainParam *)&audioDmaChainParam, 16);
		if (result < 0)
			goto unlock;
		result = sceKernelDmaOpEnQueue(transfer->opId);
		if (result < 0)
			goto unlock;
		transfer->state = 1;
	}
	threshold = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
wait:
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		running = transfer->state;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
		if (running <= threshold)
			return 0;
		sceKernelWaitEventFlag(state->eventFlag, 2, 5, NULL, NULL);
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return result;
}

void audioDownsampleCapture(int bank, int16_t *output, int count)
{
	const int16_t *input = audioCaptureSampleBanks[bank].samples;
	int16_t *end = output + count;
	const struct AudioCaptureSampleBlock *history;

	do {
		int64_t sum = 11041LL * ((input[2] + input[3]) +
					(input[14] + input[15]));

		sum += 7598LL * ((input[0] + input[1]) + (input[16] + input[17]));
		sum += 14034LL * ((input[4] + input[5]) + (input[12] + input[13]));
		sum += 16071LL * ((input[6] + input[7]) + (input[10] + input[11]));
		sum += 16793LL * (input[8] + input[9]);
		*output++ = sum >> 18;
		input += 12;
	} while (output != end);

	history = (const struct AudioCaptureSampleBlock *)
		&audioCaptureSampleBanks[bank].words[6 * count];
	audioCaptureSampleBanks[bank ^ 1].blocks[0] = history[0];
	audioCaptureSampleBanks[bank ^ 1].blocks[1] = history[1];
}

int audioBuildCaptureDmaTags(struct AudioDmaState *state);

int audioStartCaptureDma(struct AudioDmaState *state)
{
	SceSrcMixRegisters *registers;
	SceKernelDmaOpTag *tags;
	unsigned int flags;
	int result;

	if (audioSuspended || sceKernelDmaOpSync(state->captureOpId, 1, 0, 0) > 0)
		return 0;
	registers = state->registers;
	registers->captureControl = 0;
	(void)registers->status;
	__asm__ volatile("dsb sy" ::: "memory");
	tags = state->captureTags;
	flags = audioBuildCaptureDmaTags(state);
	result = sceKernelDmaOpSetupChain(state->captureOpId,
		tags + ((flags & 0x10000) ? 2 : 0),
		(SceKernelDmaOpChainParam *)&audioDmaChainParam, flags & ~0x10000U);
	if (result < 0)
		return result;
	flags = audioBuildCaptureDmaTags(state);
	result = sceKernelDmaOpConcatenate(state->captureOpId,
		tags + ((flags & 0x10000) ? 2 : 0), flags & ~0x10000U);
	if (result < 0)
		return result;
	result = sceKernelDmaOpEnQueue(state->captureOpId);
	if (result >= 0) {
		registers->captureControl = 0x10001;
		state->captureState = 1;
		return 0;
	}
	return result;
}

int audioBuildCaptureDmaTags(struct AudioDmaState *state)
{
	int mode = state->sourceMode;
	uint32_t command = state->captureCommand;
	uint32_t firstLength, lastLength, changeLength, blockSize, changeBlockSize;
	uint32_t firstCommand;
	int result, unchangedResult;
	uintptr_t destination;

	if (state == &audioDmaStates[0]) {
		SceKernelIntrStatus intr;

		mode = -2;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&audioDmaStates[1].lock);
		audioStartCaptureDma(&audioDmaStates[1]);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&audioDmaStates[1].lock, intr);
		goto mixed;
	}
	if (mode == 0) {
		destination = audioGetI2sDataAddress(audioSelectedI2sPort);
		state->captureTags[1].dst = (void *)destination;
		state->captureTags[0].dst = (void *)destination;
		command |= audioGetI2sDmaCommand();
		changeLength = 0x55000100;
		changeBlockSize = 0x800000;
		blockSize = 0x800010;
	} else if (mode == 1) {
		state->captureTags[1].dst = (void *)SPDIF_DATA;
		state->captureTags[0].dst = (void *)SPDIF_DATA;
		command |= 0xd80000;
		changeLength = 0x55000200;
		changeBlockSize = 0x1000000;
		blockSize = 0x1000010;
	} else if (mode == -2) {
mixed:
		changeLength = 0x55000020;
		changeBlockSize = 0x100000;
		blockSize = 0x100010;
	} else {
		uint32_t bytes = 4 * state->captureFrames;

		state->previousSourceMode = mode;
		destination = (uintptr_t)state->pendingCaptureBuffer;
		if (mode != 2 || !destination) {
			state->captureTags[1].dst = (void *)0x1d000000;
			state->captureTags[0].dst = (void *)0x1d000000;
			firstLength = bytes + 0x55000000 - 0x100;
			lastLength = 0x55000100;
			firstCommand = command;
			blockSize = 16;
			result = 0;
			goto writeTags;
		}
		if (audioEightKhzMode | audioModeOneEnabled) {
			int bank = audioCaptureSampleBankIndex;

			if (audioEightKhzMode)
				bytes = 24 * state->captureFrames;
			audioCaptureSampleBankIndex ^= 1;
			destination = (uintptr_t)&audioCaptureSampleBanks[bank].samples[16];
		}
		state->captureTags[0].dst = (void *)destination;
		state->captureTags[1].dst = (void *)(destination + bytes - 256);
		state->pendingCaptureBuffer = 0;
		if ((uint32_t)(destination - 0x18000000) < 0x8000000) {
			if (!sceKernelIsUncacheAddressInTmpFsGame((void *)destination))
				command |= 0x2000000;
		} else if ((uint32_t)(destination - 0x20000000) >= 0x20000000 &&
			   (uint32_t)(destination - 0x60000000) >= 0x21000000) {
			command |= 0x2000000;
		}
		firstLength = bytes + 0x5000000 - 0x100;
		lastLength = 0x5000100;
		firstCommand = command;
		command |= 0x2000;
		blockSize = 16;
		changeLength = 0x5000000;
		changeBlockSize = 0;
		result = 0x10010;
		unchangedResult = 16;
		goto updateSource;
	}
	firstLength = 0x55000f00;
	lastLength = 0x55000100;
	firstCommand = command;
	result = 0x10000;
	unchangedResult = 0;
updateSource:
	if (mode == state->previousSourceMode) {
		result = unchangedResult;
	} else {
		state->previousSourceMode = mode;
		state->captureTags[2].blockSize = changeBlockSize;
		state->captureTags[2].len = changeLength;
		state->captureTags[2].cmd = firstCommand & 0xf8003f;
		state->captureTags[2].dst = state->captureTags[0].dst;
	}
writeTags:
	state->captureTags[0].len = firstLength;
	state->captureTags[1].len = lastLength;
	state->captureTags[1].blockSize = blockSize;
	state->captureTags[0].blockSize = blockSize;
	state->captureTags[0].cmd = firstCommand;
	state->captureTags[1].cmd = command;
	return result;
}

int audioCaptureDmaCallback(SceKernelDmaOpId opId, unsigned int status,
	void *userData, const SceKernelDmaOpTag *tag, SceSize completed)
{
	struct AudioDmaState *state = userData;
	SceKernelIntrStatus intr;
	SceKernelDmaOpId otherOp;
	unsigned int flags;
	SceKernelDmaOpTag *tags;

	(void)tag;
	(void)completed;
	sceKernelSpinlockLowLock(&state->lock);
	if (state->previousCaptureBuffer && state->previousCaptureFrames)
		audioDownsampleCapture(audioCaptureSampleBankIndex,
			state->previousCaptureBuffer, state->previousCaptureFrames);
	state->previousCaptureFrames = state->currentCaptureFrames;
	state->previousCaptureBuffer = state->currentCaptureBuffer;
	state->currentCaptureBuffer = state->pendingCaptureBuffer;
	state->currentCaptureFrames = (audioEightKhzMode | audioModeOneEnabled) ?
		state->captureFrames : 0;
	if (!(status & 1)) {
		if (state->currentCaptureBuffer && state->currentCaptureFrames) {
			audioDownsampleCapture(audioCaptureSampleBankIndex ^ 1,
				state->currentCaptureBuffer, state->currentCaptureFrames);
			state->currentCaptureFrames = 0;
			state->previousCaptureFrames = 0;
		}
		intr = sceKernelCpuSuspendIntr();
		if (sceKernelDmaOpSync(opId, 1, 0, 0) <= 0) {
			state->captureState = 0;
			if (state == &audioDmaStates[2]) {
				SceSrcMixRegisters *registers = state->registers;

				registers->captureControl = 0;
				registers->secondTransmitControl = 0;
				registers->transmitControl = 0;
				(void)registers->status;
				__asm__ volatile("dsb sy" ::: "memory");
				ScePervasiveForDriver_F861FEE4(2);
			}
			goto notify;
		}
	} else {
		intr = sceKernelCpuSuspendIntr();
	}
	if (state == &audioDmaStates[2]) {
		state->captureState = 1;
		goto notify;
	}
	if (state == &audioDmaStates[1]) {
		otherOp = audioDmaStates[0].captureOpId;
		if (audioDmaStates[1].sourceMode == 1)
			goto concatenate;
	} else {
		otherOp = audioDmaStates[0].sourceMode == audioDmaStates[1].sourceMode ?
			state->transfers[1].opId : 0;
	}
	if (state->sourceMode != 2 &&
	    sceKernelDmaOpSync(state->transfers[0].opId, 1, 0, 0) <= 0 &&
	    (!otherOp || sceKernelDmaOpSync(otherOp, 1, 0, 0) <= 0)) {
		state->captureState = 0;
		goto notify;
	}
concatenate:
	flags = audioBuildCaptureDmaTags(state);
	tags = state->captureTags;
	if (sceKernelDmaOpConcatenate(state->captureOpId,
	    tags + ((flags & 0x10000) ? 2 : 0), flags & ~0x10000U) < 0)
		state->captureState = 0;
notify:
	if (state != &audioDmaStates[0])
		sceKernelSetEventFlag(state->eventFlag, 4);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

int audioDmaTransferCallback(SceKernelDmaOpId opId, unsigned int status,
	void *userData, const SceKernelDmaOpTag *tag, SceSize completed)
{
	uintptr_t taggedState = (uintptr_t)userData;
	struct AudioDmaState *state = (void *)(taggedState & ~1U);
	unsigned int stream = taggedState & 1;
	SceKernelIntrStatus intr;
	int running;

	(void)tag;
	(void)completed;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	running = (status & 1) ||
		sceKernelDmaOpSync(opId, 1, 0, 0) > 0;
	state->transfers[stream].state = running;
	sceKernelSetEventFlag(state->eventFlag, stream + 1);
	if (state == &audioDmaStates[2] && (running & (stream ^ 1)) &&
	    state->captureState > 0) {
		struct AudioDmaTransfer *transfer = &state->transfers[0];
		uint32_t command, firstLength, lastLength, blockSize;
		int concatenateMode;

		audioEnableI2sLocked(audioDmaCommandMode);
		concatenateMode = state->transmitSourceMode;
		command = transfer->command;
		if (state->transmitSourceMode) {
			uintptr_t source = (uintptr_t)state->pendingTransmitBuffer;
			unsigned int frames = state->transmitFrames;

			if (source) {
				transfer->tags[0].src = (const void *)source;
				transfer->tags[1].src = (const void *)(source + 4 * frames - 128);
				state->transmitFrames = 1024;
				command |= 0x1000000;
				concatenateMode = 1;
				state->pendingTransmitBuffer = 0;
				firstLength = 0x50000f80;
				lastLength = 0x50000080;
			} else {
				transfer->tags[1].src = (const void *)0x1d000000;
				transfer->tags[0].src = (const void *)0x1d000000;
				firstLength = 4 * frames + 0x55000000 - 128;
				lastLength = 0x55000080;
				concatenateMode = 0;
			}
			blockSize = 0x100000;
		} else {
			uintptr_t source;

			command |= audioGetDmaModeCommand();
			source = audioGetI2sDataAddress(audioDmaCommandMode);
			transfer->tags[1].src = (const void *)source;
			transfer->tags[0].src = (const void *)source;
			firstLength = 4 * state->transmitFrames + 0x55000000 - 128;
			lastLength = 0x55000080;
			blockSize = 0x100080;
		}
		transfer->tags[0].len = firstLength;
		transfer->tags[1].blockSize = blockSize;
		transfer->tags[0].blockSize = blockSize;
		transfer->tags[1].len = lastLength;
		transfer->tags[0].cmd = command;
		transfer->tags[1].cmd = command;
		if (sceKernelDmaOpConcatenate(transfer->opId,
		    audioDmaStates[2].transfers[0].tags, concatenateMode) < 0)
			audioDmaStates[2].transfers[0].state = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

int audioAuxiliaryDmaCallback(SceKernelDmaOpId opId, unsigned int status,
	void *userData, const SceKernelDmaOpTag *tag, SceSize completed)
{
	struct AudioDmaState *state = userData;
	struct AudioDmaTransfer *transfer = &(state + 1)->transfers[1];
	SceKernelIntrStatus intr;
	uint32_t bytes, command, lastCommand, firstLength, lastLength;
	uintptr_t source;
	int concatenateMode;

	(void)tag;
	(void)completed;
	sceKernelSpinlockLowLock(&state->lock);
	if (state->previousCaptureBuffer && state->previousCaptureFrames)
		audioDownsampleCapture(audioCaptureSampleBankIndex,
			state->previousCaptureBuffer, state->previousCaptureFrames);
	state->previousCaptureFrames = state->currentCaptureFrames;
	state->previousCaptureBuffer = state->currentCaptureBuffer;
	state->currentCaptureBuffer = state->pendingCaptureBuffer;
	state->currentCaptureFrames = (audioEightKhzMode | audioModeOneEnabled) ?
		state->captureFrames : 0;
	if (!(status & 1)) {
		if (state->currentCaptureBuffer && state->currentCaptureFrames) {
			audioDownsampleCapture(audioCaptureSampleBankIndex ^ 1,
				state->currentCaptureBuffer, state->currentCaptureFrames);
			state->currentCaptureFrames = 0;
			state->previousCaptureFrames = 0;
		}
		intr = sceKernelCpuSuspendIntr();
		if (sceKernelDmaOpSync(opId, 1, 0, 0) <= 0)
			goto stop;
	} else {
		intr = sceKernelCpuSuspendIntr();
	}
	if (audioDmaStates[1].sourceMode == audioDmaStates[0].sourceMode ||
	    (audioDmaStates[0].sourceMode != 2 &&
	     sceKernelDmaOpSync(transfer->opId, 1, 0, 0) <= 0))
		goto stop;
	bytes = 4 * state->captureFrames;
	command = transfer->command;
	source = (uintptr_t)state->pendingCaptureBuffer;
	if (source) {
		if (audioEightKhzMode | audioModeOneEnabled) {
			int bank = audioCaptureSampleBankIndex;

			if (audioEightKhzMode)
				bytes = 24 * state->captureFrames;
			audioCaptureSampleBankIndex ^= 1;
			source = (uintptr_t)&audioCaptureSampleBanks[bank].samples[16];
		}
		transfer->tags[0].dst = (void *)source;
		transfer->tags[1].dst = (void *)(source + bytes - 256);
		state->pendingCaptureBuffer = 0;
		if ((uint32_t)(source - 0x18000000) < 0x8000000) {
			if (!sceKernelIsUncacheAddressInTmpFsGame((void *)source))
				command |= 0x2000000;
		} else if ((uint32_t)(source - 0x20000000) >= 0x20000000 &&
			   (uint32_t)(source - 0x60000000) >= 0x21000000) {
			command |= 0x2000000;
		}
		firstLength = bytes + 0x5000000 - 256;
		lastLength = 0x5000100;
		lastCommand = command | 0x2000;
		concatenateMode = 16;
	} else {
		transfer->tags[1].dst = (void *)0x1d000000;
		transfer->tags[0].dst = (void *)0x1d000000;
		firstLength = bytes + 0x55000000 - 256;
		lastLength = 0x55000100;
		lastCommand = command;
		concatenateMode = 0;
	}
	transfer->tags[1].len = lastLength;
	transfer->tags[0].len = firstLength;
	transfer->tags[0].cmd = command;
	transfer->tags[1].cmd = lastCommand;
	transfer->tags[1].blockSize = 16;
	transfer->tags[0].blockSize = 16;
	if (sceKernelDmaOpConcatenate(transfer->opId, transfer->tags,
	    concatenateMode) >= 0)
		goto notify;
stop:
	transfer->state = 0;
notify:
	if (transfer->state == 0) {
		state->registers->auxiliaryTransmitControl = 0;
		(void)state->registers->status;
	}
	sceKernelSetEventFlag(state->eventFlag, 4);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

int audioStartTransmitDma(struct AudioDmaState *state,
	SceSrcMixRegisters *registers)
{
	struct AudioDmaTransfer *transfer = &state->transfers[0];
	uint32_t command, firstLength, lastLength, blockSize;
	uintptr_t source;
	unsigned int frames;
	int mode, result;

	if (ScePervasiveForDriver_47F2C35C(2) == 0)
		registers->captureControl = 0x10001;
	if (sceKernelDmaOpSync(transfer->opId, 1, 0, 0) > 0)
		return 0;
	registers->transmitControl = 0;
	(void)registers->status;
	__asm__ volatile("dsb sy" ::: "memory");
	mode = state->transmitSourceMode;
	command = transfer->command;
	if (state->transmitSourceMode) {
		source = (uintptr_t)state->pendingTransmitBuffer;
		frames = state->transmitFrames;
		if (source) {
			transfer->tags[0].src = (const void *)source;
			transfer->tags[1].src = (const void *)(source + 4 * frames - 128);
			state->transmitFrames = 1024;
			command |= 0x1000000;
			state->pendingTransmitBuffer = 0;
			firstLength = 0x50000f80;
			lastLength = 0x50000080;
			mode = 1;
		} else {
			transfer->tags[1].src = (const void *)0x1d000000;
			transfer->tags[0].src = (const void *)0x1d000000;
			firstLength = 4 * frames + 0x55000000 - 128;
			lastLength = 0x55000080;
			mode = 0;
		}
		blockSize = 0x100000;
	} else {
		command |= audioGetDmaModeCommand();
		source = audioGetI2sDataAddress(audioDmaCommandMode);
		transfer->tags[1].src = (const void *)source;
		transfer->tags[0].src = (const void *)source;
		firstLength = 4 * state->transmitFrames + 0x55000000 - 128;
		lastLength = 0x55000080;
		blockSize = 0x100080;
	}
	transfer->tags[0].len = firstLength;
	transfer->tags[1].blockSize = blockSize;
	transfer->tags[0].blockSize = blockSize;
	transfer->tags[1].len = lastLength;
	transfer->tags[0].cmd = command;
	transfer->tags[1].cmd = command;
	result = sceKernelDmaOpSetupChain(transfer->opId, transfer->tags,
		(SceKernelDmaOpChainParam *)&audioDmaChainParam, mode);
	if (result < 0)
		return result;
	mode = state->transmitSourceMode;
	command = transfer->command;
	if (state->transmitSourceMode) {
		source = (uintptr_t)state->pendingTransmitBuffer;
		frames = state->transmitFrames;
		if (source) {
			transfer->tags[0].src = (const void *)source;
			transfer->tags[1].src = (const void *)(source + 4 * frames - 128);
			state->transmitFrames = 1024;
			command |= 0x1000000;
			state->pendingTransmitBuffer = 0;
			firstLength = 0x50000f80;
			lastLength = 0x50000080;
			mode = 1;
		} else {
			transfer->tags[1].src = (const void *)0x1d000000;
			transfer->tags[0].src = (const void *)0x1d000000;
			firstLength = 4 * frames + 0x55000000 - 128;
			lastLength = 0x55000080;
			mode = 0;
		}
		blockSize = 0x100000;
	} else {
		command |= audioGetDmaModeCommand();
		source = audioGetI2sDataAddress(audioDmaCommandMode);
		transfer->tags[1].src = (const void *)source;
		transfer->tags[0].src = (const void *)source;
		firstLength = 4 * state->transmitFrames + 0x55000000 - 128;
		lastLength = 0x55000080;
		blockSize = 0x100080;
	}
	transfer->tags[0].len = firstLength;
	transfer->tags[1].len = lastLength;
	transfer->tags[1].blockSize = blockSize;
	transfer->tags[0].blockSize = blockSize;
	transfer->tags[0].cmd = command;
	transfer->tags[1].cmd = command;
	result = sceKernelDmaOpConcatenate(transfer->opId, transfer->tags, mode);
	if (result < 0)
		return result;
	result = sceKernelDmaOpEnQueue(transfer->opId);
	if (result >= 0) {
		registers->transmitControl = 0x10001;
		transfer->state = 1;
		return 0;
	}
	return result;
}

int audioSetHardwareTransferMode(unsigned int channel, unsigned int mode)
{
	struct AudioDmaState *state;
	SceKernelIntrStatus intr;

	if (channel > 2)
		return -1;
	state = &audioDmaStates[channel ^ 1];
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	if (state->sourceMode != mode) {
		state->sourceMode = mode;
		if (mode <= 1)
			state->captureFrames = 1024;
		state->pendingCaptureBuffer = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	if (mode == 1)
		audioStartCaptureDma(state);
	return 0;
}

int audioQueueHardwareBuffer(int channel, void *buffer, unsigned int wordCount)
{
	struct AudioDmaState *state;
	SceKernelIntrStatus intr;

	if (wordCount <= 4 || (wordCount & 3) ||
	    ((audioEightKhzMode | audioModeOneEnabled) && wordCount != 120))
		return SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
	if (channel == 0) {
		state = &audioDmaStates[1];
		if (state->sourceMode != 2)
			goto notReady;
		audioStartCaptureDma(state);
		goto queue;
	}
	if (channel != 1)
		return -1;
	state = &audioDmaStates[0];
	if (state->sourceMode != 2)
		goto notReady;
	if (audioDmaStates[1].sourceMode != 2 && !audioSuspended &&
	    sceKernelDmaOpSync(audioDmaStates[1].transfers[1].opId, 1, 0, 0) <= 0) {
		struct AudioDmaTransfer *transfer = &audioDmaStates[1].transfers[1];
		SceSrcMixRegisters *registers = state->registers;

		registers->auxiliaryTransmitControl = 0;
		(void)registers->status;
		__asm__ volatile("dsb sy" ::: "memory");
		{
			uintptr_t destination = (uintptr_t)state->pendingCaptureBuffer;
			unsigned int bytes = 4 * state->captureFrames;
			uint32_t command = transfer->command;
			uint32_t firstLength, lastLength, lastCommand;
			int flags;

			if (destination) {
				if (audioEightKhzMode | audioModeOneEnabled) {
					int bank = audioCaptureSampleBankIndex;

					if (audioEightKhzMode)
						bytes = 24 * state->captureFrames;
					audioCaptureSampleBankIndex ^= 1;
					destination = (uintptr_t)&audioCaptureSampleBanks[bank].samples[16];
				}
				transfer->tags[0].dst = (void *)destination;
				transfer->tags[1].dst = (void *)(destination + bytes - 256);
				state->pendingCaptureBuffer = 0;
				if ((uint32_t)(destination - 0x18000000) < 0x8000000) {
					if (!sceKernelIsUncacheAddressInTmpFsGame((void *)destination))
						command |= 0x2000000;
				} else if ((uint32_t)(destination - 0x20000000) >= 0x20000000 &&
					   (uint32_t)(destination - 0x60000000) >= 0x21000000) {
					command |= 0x2000000;
				}
				firstLength = bytes + 0x5000000 - 256;
				lastLength = 0x5000100;
				lastCommand = command | 0x2000;
				flags = 16;
			} else {
				transfer->tags[1].dst = (void *)0x1d000000;
				transfer->tags[0].dst = (void *)0x1d000000;
				firstLength = bytes + 0x55000000 - 256;
				lastLength = 0x55000100;
				lastCommand = command;
				flags = 0;
			}
			transfer->tags[1].len = lastLength;
			transfer->tags[0].len = firstLength;
			transfer->tags[0].cmd = command;
			transfer->tags[1].cmd = lastCommand;
			transfer->tags[1].blockSize = 16;
			transfer->tags[0].blockSize = 16;
			if (sceKernelDmaOpSetupChain(transfer->opId, transfer->tags,
			    (SceKernelDmaOpChainParam *)&audioDmaChainParam, flags) < 0)
				goto queue;
		}
		{
			uintptr_t destination = (uintptr_t)state->pendingCaptureBuffer;
			unsigned int bytes = 4 * state->captureFrames;
			uint32_t command = transfer->command;
			uint32_t firstLength, lastLength, lastCommand;
			int flags;

			if (destination) {
				if (audioEightKhzMode | audioModeOneEnabled) {
					int bank = audioCaptureSampleBankIndex;

					if (audioEightKhzMode)
						bytes = 24 * state->captureFrames;
					audioCaptureSampleBankIndex ^= 1;
					destination = (uintptr_t)&audioCaptureSampleBanks[bank].samples[16];
				}
				transfer->tags[0].dst = (void *)destination;
				transfer->tags[1].dst = (void *)(destination + bytes - 256);
				state->pendingCaptureBuffer = 0;
				if ((uint32_t)(destination - 0x18000000) < 0x8000000) {
					if (!sceKernelIsUncacheAddressInTmpFsGame((void *)destination))
						command |= 0x2000000;
				} else if ((uint32_t)(destination - 0x20000000) >= 0x20000000 &&
					   (uint32_t)(destination - 0x60000000) >= 0x21000000) {
					command |= 0x2000000;
				}
				firstLength = bytes + 0x5000000 - 256;
				lastLength = 0x5000100;
				lastCommand = command | 0x2000;
				flags = 16;
			} else {
				transfer->tags[1].dst = (void *)0x1d000000;
				transfer->tags[0].dst = (void *)0x1d000000;
				firstLength = bytes + 0x55000000 - 256;
				lastLength = 0x55000100;
				lastCommand = command;
				flags = 0;
			}
			transfer->tags[0].len = firstLength;
			transfer->tags[1].len = lastLength;
			transfer->tags[0].cmd = command;
			transfer->tags[1].cmd = lastCommand;
			transfer->tags[1].blockSize = 16;
			transfer->tags[0].blockSize = 16;
			if (sceKernelDmaOpConcatenate(transfer->opId, transfer->tags, flags) < 0)
				goto queue;
		}
		if (sceKernelDmaOpEnQueue(transfer->opId) < 0)
			goto queue;
		registers->auxiliaryTransmitControl = 0x10001;
		transfer->state = 1;
	}
queue:
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	while (state->pendingCaptureBuffer) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
		sceKernelWaitEventFlag(state->eventFlag, 4, 5, NULL, NULL);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	}
	state->pendingCaptureBuffer = buffer;
	state->captureFrames = wordCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
notReady:
	sceKernelDelayThread(1000);
	return -2;
}

int audioSetSingleTransferMode(int mode)
{
	struct AudioDmaState *state = &audioDmaStates[2];
	SceKernelIntrStatus intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	if (state->transmitSourceMode != mode) {
		state->transmitSourceMode = mode;
		if (mode) {
			state->transfers[0].tags[1].src = (const void *)0x1d000000;
			state->transfers[0].tags[0].src = (const void *)0x1d000000;
		} else {
			state->transmitFrames = 1024;
		}
		state->pendingTransmitBuffer = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

int audioQueueSingleHardwareBuffer(void *buffer, unsigned int wordCount)
{
	struct AudioDmaState *state = &audioDmaStates[2];
	SceKernelIntrStatus intr;

	if (wordCount <= 4 || (wordCount & 3))
		return SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
	if (state->transmitSourceMode != 2)
		return -2;
	if (sceKernelDmaOpSync(state->transfers[0].opId, 1, 0, 0) <= 0)
		audioStartTransmitDma(state, state->registers);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	while (state->pendingTransmitBuffer) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
		sceKernelWaitEventFlag(state->eventFlag, 1, 5, NULL, NULL);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	}
	state->pendingTransmitBuffer = buffer;
	state->transmitFrames = wordCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return 0;
}

int audioSubmitDmaBuffer(uintptr_t source, unsigned int wordCount,
	unsigned int bank, unsigned int stream)
{
	struct AudioDmaState *state;
	struct AudioDmaTransfer *transfer;
	SceKernelIntrStatus intr;
	int threshold = 0;
	int running, result;
	int submittedState;
	unsigned int length;
	uint32_t command;

	if (bank > 1)
		return -1;
	if (stream > 1)
		return -2;
	state = &audioDmaStates[bank];
	transfer = &state->transfers[stream];
	if (!source)
		goto wait;
	if (source & 15)
		return SCE_AUDIO_OUT_ERROR_INVALID_POINTER;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	running = transfer->state;
	if (running > 1) {
		result = SCE_AUDIO_OUT_ERROR_BUSY;
		goto unlock;
	}
	if (wordCount <= 4 || (wordCount & 3)) {
		result = SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
		goto unlock;
	}
	transfer->tags[0].src = (const void *)source;
	command = transfer->command;
	length = 4 * wordCount - 16;
	transfer->tags[1].src = (const void *)(source + length);
	transfer->tags[0].len = length + 0x50000000;
	transfer->tags[0].cmd = command;
	transfer->tags[1].len = 0x50000010;
	transfer->tags[1].cmd = command;
	if (running == 1 &&
	    sceKernelDmaOpSync(transfer->opId, 1, 0, 0) > 0 &&
	    sceKernelDmaOpConcatenate(transfer->opId, transfer->tags, 0) >= 0) {
		audioStartCaptureDma(state);
		submittedState = 2;
	} else {
		audioStartCaptureDma(state);
		result = sceKernelDmaOpSetupChain(transfer->opId, transfer->tags,
			(SceKernelDmaOpChainParam *)&audioDmaChainParam, 0);
		if (result < 0)
			goto unlock;
		result = sceKernelDmaOpEnQueue(transfer->opId);
		if (result < 0)
			goto unlock;
		submittedState = 1;
	}
	state->transfers[stream].state = submittedState;
	threshold = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
wait:
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		running = transfer->state;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
		if (running <= threshold)
			return 0;
		sceKernelWaitEventFlag(state->eventFlag, stream + 1, 5, NULL, NULL);
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return result;
}

int audioSubmitSingleDmaBuffer(uintptr_t destination, unsigned int wordCount)
{
	struct AudioDmaState *state = &audioDmaStates[2];
	SceKernelIntrStatus intr;
	int threshold = 0;
	int running, result;
	unsigned int length;

	if (destination & 15)
		return (int)0x80260105;
	if (!destination)
		goto wait;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	running = state->captureState;
	if (running > 1) {
		result = (int)0x8026010a;
		goto unlock;
	}
	if (wordCount <= 4 || (wordCount & 3)) {
		result = (int)0x80260102;
		goto unlock;
	}
	length = 4 * wordCount - 16;
	state->captureTags[0].dst = (void *)destination;
	state->captureTags[1].dst = (void *)(destination + length);
	state->captureTags[0].len = length + 0x5000000;
	state->captureTags[0].cmd = state->captureCommand;
	state->captureTags[1].len = 0x5000010;
	state->captureTags[1].cmd = state->captureCommand | 0x2000;
	if (audioEnableI2sLocked(audioDmaCommandMode) == 0 && running != 0) {
		if (running == 1 &&
		    sceKernelDmaOpSync(state->captureOpId, 1, 0, 0) > 0 &&
		    sceKernelDmaOpConcatenate(state->captureOpId,
			state->captureTags, 16) >= 0) {
			state->captureState = 2;
			goto submitted;
		}
	} else {
		result = audioStartTransmitDma(state, state->registers);
		if (result < 0)
			goto unlock;
	}
	result = sceKernelDmaOpSetupChain(state->captureOpId, state->captureTags,
		(SceKernelDmaOpChainParam *)&audioDmaChainParam, 16);
	if (result < 0)
		goto unlock;
	result = sceKernelDmaOpEnQueue(state->captureOpId);
	if (result < 0)
		goto unlock;
	state->captureState = 1;
submitted:
	threshold = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
wait:
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
		running = state->captureState;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
		if (running <= threshold)
			return 0;
		sceKernelWaitEventFlag(state->eventFlag, 4, 5, NULL, NULL);
	}
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return result;
}

int SceAudioForDriver_28B3AF10(int channel, int inputRate, int outputRate,
	int clockRate)
{
	struct AudioDmaState *state = &audioDmaStates[channel];
	SceSrcMixRegisters *registers = state->registers;
	uint32_t packet[8];
	unsigned int tail, withoutOutput;
	int selector;
	int send;
	SceKernelIntrStatus intr;
	uint32_t gate, status;
	int result;

	switch (clockRate) {
	case 48000: selector = 1; break;
	case 44100: selector = 0; break;
	case 32000: selector = 2; break;
	case 24000: selector = 17; break;
	case 22050: selector = 16; break;
	case 16000: selector = 18; break;
	case 12000: selector = 33; break;
	case 11050: selector = 32; break;
	case 8000: selector = 34; break;
	default:
		if (clockRate > 0)
			return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
		selector = -1;
		break;
	}
	if (selector > 0) {
		packet[0] = selector;
		tail = 1;
		withoutOutput = 0;
	} else {
		tail = 0;
		withoutOutput = ~0U;
	}
	if (outputRate > 0) {
		if ((unsigned int)(clockRate - 1) <= 0x7cfe)
			return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
		packet[tail] = 0;
		send = 1;
	} else if (outputRate == 0) {
		packet[tail] = 1;
		send = 1;
	} else {
		send = tail != 0;
		tail = withoutOutput;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&state->lock);
	gate = ScePervasiveForDriver_47F2C35C(channel);
	if (send) {
		const uint32_t *cursor = packet;
		uint32_t command = selector == -1 ? 0x201100 : 0x201000;

		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto hardwareError;
		}
		registers->control = 0;
		registers->command = command | tail;
		if (tail > 7) {
			do {
				__builtin_prefetch(cursor + 34);
				registers->command = cursor[0];
				registers->command = cursor[1];
				registers->command = cursor[2];
				registers->command = cursor[3];
				registers->command = cursor[4];
				registers->command = cursor[5];
				registers->command = cursor[6];
				registers->command = cursor[7];
				cursor += 8;
				tail -= 8;
			} while (tail > 7);
		}
		do {
			uint32_t value = *cursor++;

			tail--;
			registers->command = value;
		} while (tail != ~0U);
		registers->control = 1;
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto hardwareError;
		}
	}
	result = audioSetClockRates(channel, inputRate, outputRate, clockRate);
	if (gate == 0)
		ScePervasiveForDriver_F861FEE4(channel);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return result;
hardwareError:
	registers->control = 0x100;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, intr);
	return -1;
}

int audioSetHardwareVolume(int channel, int left, int right)
{
	SceSrcMixRegisters *registers = audioDmaStates[channel].registers;
	union {
		uint32_t words[8];
		uint64_t pairs[4];
	} parameters;
	uint32_t status;

	parameters.words[0] = (uint32_t)left << 7;
	parameters.words[1] = (uint32_t)right << 7;
	parameters.words[2] = 19021;
	parameters.words[3] = 9510;
	parameters.words[4] = 0x80000;
	parameters.words[5] = 9510;
	parameters.words[6] = 4755;
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	registers->control = 0;
	registers->command = 0x201206;
	registers->command = parameters.words[0];
	registers->command = parameters.words[1];
	registers->command = parameters.words[2];
	registers->command = parameters.words[3];
	registers->command = parameters.words[4];
	registers->command = parameters.words[5];
	registers->command = parameters.words[6];
	registers->control = 1;
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	vst1q_u64(parameters.pairs, vreinterpretq_u64_u32(
		(uint32x4_t){ 0x400000, 0x400000, 0x400000, 0x7fffff }));
	vst1_u64(parameters.pairs + 2, vreinterpret_u64_u32(
		(uint32x2_t){ 4096, 1 }));
	vst1_u64(parameters.pairs + 3, vreinterpret_u64_u32(
		(uint32x2_t){ 8384512, 0x7fffff }));
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	registers->control = 0;
	registers->command = 0x201a07;
	registers->command = parameters.words[0];
	registers->command = parameters.words[1];
	registers->command = parameters.words[2];
	registers->command = parameters.words[3];
	registers->command = parameters.words[4];
	registers->command = parameters.words[5];
	registers->command = parameters.words[6];
	registers->command = parameters.words[7];
	registers->control = 1;
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	parameters.words[0] = 128;
	parameters.words[1] = 4473924;
	parameters.words[2] = 0x7fffff;
	parameters.words[3] = 7829367;
	parameters.words[4] = 512;
	parameters.words[5] = 16776832;
	parameters.words[6] = 0x7fffff;
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	registers->control = 0;
	registers->command = 0x202206;
	registers->command = parameters.words[0];
	registers->command = parameters.words[1];
	registers->command = parameters.words[2];
	registers->command = parameters.words[3];
	registers->command = parameters.words[4];
	registers->command = parameters.words[5];
	registers->command = parameters.words[6];
	registers->control = 1;
	while ((status = registers->status) != 0) {
		if (status & 0x100)
			goto hardwareError;
	}
	return status;
hardwareError:
	registers->control = 0x100;
	return -1;
}

/* Each upload keeps the command FIFO's original load/store ordering. */
#define AUDIO_SRCMIX_UPLOAD(table, wordCount, uploadCommand, extra) do { \
	const uint32_t *cursor = (table); \
	unsigned int remaining = (wordCount); \
	while ((status = registers->status) != 0) { \
		if (status & 0x100) \
			goto hardwareError; \
	} \
	registers->control = 0; \
	registers->command = (uploadCommand); \
	while (remaining > 8) { \
		uint32_t first = cursor[0]; \
		__builtin_prefetch(cursor + 34); \
		registers->command = first; \
		registers->command = cursor[1]; \
		registers->command = cursor[2]; \
		registers->command = cursor[3]; \
		registers->command = cursor[4]; \
		registers->command = cursor[5]; \
		registers->command = cursor[6]; \
		registers->command = cursor[7]; \
		cursor += 8; \
		remaining -= 8; \
	} \
	do { \
		registers->command = *cursor++; \
	} while (--remaining != 0); \
	extra; \
	registers->control = 1; \
	while ((status = registers->status) != 0) { \
		if (status & 0x100) \
			goto hardwareError; \
	} \
} while (0)

int audioInitializeSrcMix(void)
{
	SceKernelAllocMemBlockOptKernel option;
	struct AudioDmaState *state;
	SceSrcMixRegisters *registers;
	uintptr_t physicalAddress = SRCMIX_REG_BASE(0);
	unsigned int channel = 0;
	unsigned int operation;
	uint32_t status, started;
	int result = 0;
	char name[32];

	ScePervasiveForDriver_925D9D24(0);
	if (audioClockRegisters == NULL) {
		memset(&option, 0, sizeof(option));
		option.size = sizeof(option);
		option.paddr = ACLKGEN_REG_BASE;
		option.attr = 2;
		result = sceKernelAllocMemBlockForKernel("SceAclkgenReg", 0x20100806,
			ACLKGEN_REG_SIZE, &option);
		if (result >= 0)
			sceKernelGetMemBlockBase(result, (void **)&audioClockRegisters);
	}
	state = audioDmaStates;
	ScePervasiveForDriver_2FB5F88F(1);
	do {
		ScePervasiveForDriver_47F2C35C(channel);
		ScePervasiveForDriver_5BAFA0DC(channel);
		audioSetClockRates(channel, 48000, 48000, 48000);
		registers = state->registers;
		if (registers == NULL) {
			state->lock = 0;
			memset(&option, 0, sizeof(option));
			option.size = sizeof(option);
			option.attr = 2;
			option.paddr = physicalAddress;
			snprintf(name, sizeof(name), "SceSrcMix%dReg", channel);
			result = sceKernelAllocMemBlockForKernel(name, 0x20100806, SRCMIX_REG_SIZE, &option);
			if (result < 0)
				return result;
			sceKernelGetMemBlockBase(result, (void **)&state->registers);
			for (operation = 0; operation < 3; ++operation) {
				SceKernelDmaOpTag *tags;
				SceKernelDmaOpId opId;
				DmaOpCallback callback;
				void *userData;

				if (operation == 2) {
					tags = state->captureTags;
					state->captureCommand = audioDmaCommands[channel][operation];
				} else {
					tags = state->transfers[operation].tags;
					state->transfers[operation].command =
						audioDmaCommands[channel][operation];
				}
				snprintf(name, sizeof(name), "SceSrcMix%d%d", channel, operation);
				opId = sceKernelDmaOpAlloc(name);
				if (opId < 0)
					return opId;
				if (operation == 2)
					state->captureOpId = opId;
				else
					state->transfers[operation].opId = opId;
				if (operation == 1 && channel == 1) {
					callback = audioAuxiliaryDmaCallback;
					userData = &audioDmaStates[0];
				} else if (operation == 2) {
					callback = audioCaptureDmaCallback;
					userData = state;
				} else {
					callback = audioDmaTransferCallback;
					userData = (void *)((uintptr_t)state + operation);
				}
				result = sceKernelDmaOpSetCallback(opId, callback, userData);
				if (result < 0)
					return result;
				result = sceKernelDmaOpAssign(opId, 18, 3 * channel + operation);
				if (result < 0)
					return result;
				tags[0].pNext = &tags[1];
				if (operation == 2) {
					tags[0].src = (const void *)(physicalAddress + 512);
					tags[1].src = (const void *)(physicalAddress + 512);
					tags[0].blockSize = 16;
					tags[1].blockSize = 16;
					tags[1].pNext = SCE_KERNEL_DMAC_CHAIN_END;
					state->captureState = 0;
					tags[2].pNext = tags;
					tags[2].src = (const void *)0x1d000000;
					state->captureTags[1].pNext = SCE_KERNEL_DMAC_CHAIN_END;
					state->captureState = 0;
					state->previousSourceMode = -1;
					state->eventFlag = sceKernelCreateEventFlag(name, 0x1000, 0, NULL);
				} else {
					tags[0].blockSize = 0x100000;
					tags[1].blockSize = 0x100000;
					tags[1].pNext = &tags[2];
					tags[0].dst = (void *)(physicalAddress + (operation ? 320 : 256));
					tags[1].dst = tags[0].dst;
					state->transfers[operation].state = 0;
					tags[1].pNext = SCE_KERNEL_DMAC_CHAIN_END;
					state->transfers[operation].state = 0;
					state->previousSourceMode = -1;
				}
			}
			registers = state->registers;
		}
		(void)registers->status;
		__asm__ volatile("dsb sy" ::: "memory");
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto hardwareError;
		}
		registers->control = 0;
		registers->command = 32;
		registers->control = 1;
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto hardwareError;
		}
		AUDIO_SRCMIX_UPLOAD(audioSrcMixProgram, 256, 0x60, (void)0);
		AUDIO_SRCMIX_UPLOAD(audioSrcMixSecondaryProgram, 1414, 0x120060, (void)0);
		AUDIO_SRCMIX_UPLOAD(audioSrcMixCoefficients, 1024, 0x280060, (void)0);
		AUDIO_SRCMIX_UPLOAD(audioSrcMixSecondCoefficients, 1814, 0x300060,
			for (operation = 0; operation < 4; ++operation) {
				const struct AudioClockDivider *divider = &audioClockDividers[operation];

				registers->command = divider->sampleRate | ((uint32_t)divider->divider << 16);
				registers->command = divider->primaryControl | ((uint32_t)divider->alternateControl << 16);
			});
		result = 0;
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto finalUploadError;
		}
		registers->control = 0;
		registers->command = 0;
		registers->control = 1;
		while ((result = registers->status) != 0) {
			if (result & 0x100) {
finalUploadError:
				registers->control = 0x100;
				result = -1;
				break;
			}
		}
		started = SceThreadmgrForDriver_47F6DE49();
		while ((uint32_t)(SceThreadmgrForDriver_47F6DE49() - started) <= 67)
			;
		{
			int volume = channel == 1 ? audioHardwareVolume : 0x8000;

			audioSetHardwareVolume(channel, volume, volume);
		}
		SceAudioForDriver_28B3AF10(channel, 48000, 48000, 48000);
		registers = state->registers;
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto enableError;
		}
		registers->control = 0;
		registers->command = 0x201900;
		registers->command = 1;
		registers->control = 1;
		while ((status = registers->status) != 0) {
			if (status & 0x100)
				goto enableError;
		}
		if (channel != 2) {
			registers->transmitControl = 0x10001;
			registers->secondTransmitControl = 0x10001;
			registers->captureControl = 0x10001;
		}
nextChannel:
		if (result != 0)
			return -1;
		++channel;
		++state;
		physicalAddress += SRCMIX_REG_STRIDE;
	} while (channel != 3);
	state = &audioDmaStates[0];
	state->sourceMode = -1;
	audioDmaStates[1].transfers[1].tags[0].src = (const void *)SRCMIX0_OUTPUT_DATA;
	state->captureTags[0].dst = audioDmaStates[1].transfers[1].tags[0].dst;
	state->captureTags[1].dst = audioDmaStates[1].transfers[1].tags[0].dst;
	audioDmaStates[1].transfers[1].tags[1].src = (const void *)SRCMIX0_OUTPUT_DATA;
	audioSetHardwareTransferMode(0, 0);
	audioDmaStates[1].sourceMode = -1;
	audioSetHardwareTransferMode(1, 0);
	audioDmaStates[2].transmitFrames = 1024;
	audioDmaStates[2].transmitSourceMode = 0;
	audioSetHardwareTransferMode(2, 2);
	ScePervasiveForDriver_F861FEE4(2);
	return result;
hardwareError:
	registers->control = 0x100;
	return -1;
enableError:
	registers->control = 0x100;
	goto nextChannel;
}

#undef AUDIO_SRCMIX_UPLOAD

int audioShutdownHardware(void)
{
	struct AudioDmaState *state = audioDmaStates;

	do {
		SceUID eventFlag;

		state->transfers[0].state = 0;
		state->transfers[1].state = 0;
		state++;
		eventFlag = state[-1].eventFlag;
		state[-1].previousSourceMode = -1;
		state[-1].captureState = 0;
		sceKernelSetEventFlag(eventFlag, 3);
		sceKernelClearEventFlag(state[-1].eventFlag, ~3U);
	} while (state != audioDmaStates + 3);
	return 0;
}
