#include "audio_work.h"
/* Audio clock-divider programming. */
#include "aclkgen.h"
#include <audioout.h>


const struct AudioClockDivider audioClockDividers[10] = {
	{ 48000, 512, 1, 0 },
	{ 44100, 558, 41, 106 },
	{ 44184, 557, 70, 249 },
	{ 32000, 768, 1, 0 },
	{ 24000, 1024, 1, 0 },
	{ 22050, 1115, 82, 65 },
	{ 16000, 1536, 1, 0 },
	{ 12000, 2048, 1, 0 },
	{ 11025, 2230, 17, 130 },
	{ 8000, 3072, 1, 0 }
};

int audioSetClockRates(unsigned int channel, int inputRate, int outputRate,
	int clockRate)
{
	SceAclkgenRegisters *registers;
	SceAclkgenChannel *clock;
	int clockIndex = -1, inputIndex = -1, outputIndex = -1;
	unsigned int setMask = 0, clearMask = 0;
	uint32_t retained, requested, restart, control;

	if (channel > 2)
		return -1;
	if (clockRate > 0) {
		if (clockRate == 48000)
			clockIndex = 0;
		else if (clockRate == 44100)
			clockIndex = 1;
		else if (clockRate == 44184)
			clockIndex = 2;
		else if (clockRate == 32000)
			clockIndex = 3;
		else if (clockRate == 24000)
			clockIndex = 4;
		else if (clockRate == 22050)
			clockIndex = 5;
		else if (clockRate == 16000)
			clockIndex = 6;
		else if (clockRate == 12000)
			clockIndex = 7;
		else if (clockRate == 11025)
			clockIndex = 8;
		else if (clockRate == 8000)
			clockIndex = 9;
		else
			return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
		setMask = 4;
	} else if (clockRate == 0) {
		clearMask = 4;
	}
	if (inputRate > 0) {
		if (inputRate == 48000)
			inputIndex = 0;
		else if (inputRate == 44100)
			inputIndex = 1;
		else if (inputRate == 44184)
			inputIndex = 2;
		else if (inputRate == 32000)
			inputIndex = 3;
		else if (inputRate == 24000)
			inputIndex = 4;
		else if (inputRate == 22050)
			inputIndex = 5;
		else if (inputRate == 16000)
			inputIndex = 6;
		else if (inputRate == 12000)
			inputIndex = 7;
		else if (inputRate == 11025)
			inputIndex = 8;
		else if (inputRate == 8000)
			inputIndex = 9;
		else
			return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
		setMask |= 1;
	} else if (inputRate == 0) {
		clearMask |= 1;
	}
	if (outputRate > 0) {
		if (outputRate == 48000)
			outputIndex = 0;
		else if (outputRate == 44100)
			outputIndex = 1;
		else if (outputRate == 44184)
			outputIndex = 2;
		else if (outputRate == 32000)
			outputIndex = 3;
		else if (outputRate == 24000)
			outputIndex = 4;
		else if (outputRate == 22050)
			outputIndex = 5;
		else if (outputRate == 16000)
			outputIndex = 6;
		else if (outputRate == 12000)
			outputIndex = 7;
		else if (outputRate == 11025)
			outputIndex = 8;
		else if (outputRate == 8000)
			outputIndex = 9;
		else
			return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
		setMask |= 2;
	} else if (outputRate == 0) {
		clearMask |= 2;
	}
	registers = audioClockRegisters;
	clock = &registers->channels[channel];
	if (inputIndex != -1) {
		const struct AudioClockDivider *divider = &audioClockDividers[inputIndex];
		uint32_t base = divider->divider;
		uint32_t primary = divider->primaryControl;
		uint32_t alternate = divider->alternateControl;
		uint32_t word = base + (primary << 16);

		clock->inputDivider[0] = word;
		if (alternate)
			word = base - 1 + (alternate << 16);
		clock->inputDivider[1] = word;
	}
	if (outputIndex != -1) {
		const struct AudioClockDivider *divider = &audioClockDividers[outputIndex];
		uint32_t base = divider->divider;
		uint32_t primary = divider->primaryControl;
		uint32_t alternate = divider->alternateControl;
		uint32_t word = base + (primary << 16);

		clock->outputDivider[0] = word;
		if (alternate)
			word = base - 1 + (alternate << 16);
		clock->outputDivider[1] = word;
	}
	if (clockIndex != -1) {
		const struct AudioClockDivider *divider = &audioClockDividers[clockIndex];
		uint32_t base = divider->divider;
		uint32_t primary = divider->primaryControl;
		uint32_t alternate = divider->alternateControl;
		uint32_t word = base + (primary << 16);

		clock->clockDivider[0] = word;
		if (alternate)
			word = base - 1 + (alternate << 16);
		clock->clockDivider[1] = word;
	}
	requested = setMask << (3 * channel);
	retained = registers->control & ~(clearMask << (3 * channel));
	restart = retained & requested;
	control = retained | requested;
	if (restart)
		registers->restartControl = restart;
	if (control != retained)
		registers->control = control;
	(void)registers->control;
	__asm__ volatile("dsb sy" ::: "memory");
	return 0;
}
