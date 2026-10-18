#include "audio_work.h"
/* I2S lifecycle and DMA command selection. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include "audio_regs.h"
#include "srcmix.h"
#include <lowio/pervasive.h>

int audioEnableI2s(int port);

const uint8_t audioI2sClockIndex[8] = { 0, 1, 2, 3, 5, 4, 255, 7 };
const uint8_t audioI2sInterrupt[8] = { 193, 194, 196, 0, 0, 0, 0, 0 };
const uint8_t audioI2sClockPort[8] = { 3, 3, 5, 3, 5, 5, 0, 3 };

const uintptr_t audioI2sPhysicalAddress[8] = {
	I2S0_REG_BASE, I2S1_REG_BASE, I2S2_REG_BASE, I2S3_REG_BASE,
	I2S4_REG_BASE, I2S5_REG_BASE, 0, I2S7_REG_BASE
};

int audioI2sInterruptHandler(int intrCode, void *userCtx)
{
	int port = (uintptr_t)userCtx;
	SceI2sRegisters *registers = audioI2sRegisters[port];
	SceKernelIntrStatus intr;

	(void)intrCode;
	intr = SceCpuForDriver_D32ACE9E(&audioI2sLock);
	if (registers->interruptStatus & 2) {
		if (audioDmaStates[2].transmitSourceMode == 0) {
			struct AudioDmaTransfer *transfer =
				&audioDmaStates[2].transfers[port == 0 ? 1 : 0];

			if (sceKernelDmaOpSync(transfer->opId, 1, 0, 0) > 0)
				sceKernelDmaOpQuit(transfer->opId);
		}
		audioI2sEnabledMask &= ~(1U << port);
		ScePervasiveForDriver_146108D4(audioI2sClockIndex[port]);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&audioI2sLock, intr);
	return -1;
}

int audioEnableI2s(int port)
{
	int clockPort = audioI2sClockPort[port];
	unsigned int bit = 1U << port;
	SceI2sRegisters *registers;
	int clock;

	if ((bit | audioI2sEnabledMask) == audioI2sEnabledMask)
		return 0;
	clock = audioI2sClockIndex[port];
	audioI2sEnabledMask |= bit;
	registers = audioI2sRegisters[port];
	ScePervasiveForDriver_146108D4(clock);
	ScePervasiveForDriver_A3CF9844(clock);
	ScePervasiveForDriver_4235EB7D(clock);
	ScePervasiveForDriver_EEE340ED(clock);
	registers->formatControl = 0x2810;
	registers->interruptControl = 0;
	registers->control = 0x100;
	if (clockPort != port) {
		audioEnableI2s(clockPort);
		registers->clockControl = 1;
		registers->transferEnable = 1;
		registers->transferMode = 2;
		registers->control = 1;
	}
	(void)registers->transferEnable;
	__asm__ volatile("dsb sy" ::: "memory");
	return 1;
}

int SceAudioForDriver_A8C38998(int port)
{
	SceKernelIntrStatus intr;

	if ((unsigned int)port - 3 > 4)
		return -1;
	audioSelectedI2sPort = port;
	intr = SceCpuForDriver_D32ACE9E(&audioI2sLock);
	audioEnableI2s(port);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&audioI2sLock, intr);
	return 0;
}

int SceAudioForDriver_7E0E1331(unsigned int mode)
{
	if (mode > 2)
		return -1;
	audioDmaCommandMode = mode;
	return 0;
}

unsigned int audioGetDmaModeCommand(void)
{
	return (audioDmaCommandMode + 13U) << 14;
}

unsigned int audioGetI2sDmaCommand(void)
{
	return audioSelectedI2sPort == 4 ? 0x900000 : 0xa00000;
}

int audioInitializeI2s(void)
{
	SceKernelAllocMemBlockOptKernel option;
	char name[32];
	SceKernelIntrStatus intr;
	int port, result;

	audioI2sLock = 0;
	for (port = 0; port != 8; port++) {
		if (audioI2sRegisters[port] == NULL && audioI2sPhysicalAddress[port]) {
			memset(&option, 0, sizeof(option));
			option.size = sizeof(option);
			option.attr = 2;
			option.paddr = audioI2sPhysicalAddress[port];
			snprintf(name, sizeof(name), "SceI2s%dReg", port);
			result = SceSysmemForDriver_C94850C9(name, 0x20100806, 4096, &option);
			if (result < 0)
				return result;
			SceSysmemForDriver_A841EDDA(result, (void **)&audioI2sRegisters[port]);
			if (audioI2sInterrupt[port]) {
				snprintf(name, sizeof(name), "SceI2s%d", port);
				sceKernelRegisterIntrHandler(audioI2sInterrupt[port], name, 0,
					audioI2sInterruptHandler, (void *)(uintptr_t)port,
					160, 0x80000, NULL);
			}
		}
		if (audioI2sInterrupt[port])
			sceKernelEnableIntr(audioI2sInterrupt[port]);
	}
	audioI2sEnabledMask = 0;
	ScePervasiveForDriver_A1D2E440(1);
	audioDmaCommandMode = 1;
	audioSelectedI2sPort = 7;
	intr = SceCpuForDriver_D32ACE9E(&audioI2sLock);
	audioEnableI2s(7);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&audioI2sLock, intr);
	return 0;
}

int audioEnableI2sLocked(int port)
{
	SceKernelIntrStatus intr;
	int result;

	intr = SceCpuForDriver_D32ACE9E(&audioI2sLock);
	result = audioEnableI2s(port);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&audioI2sLock, intr);
	return result;
}

uintptr_t audioGetI2sDataAddress(int port)
{
	return audioI2sPhysicalAddress[port] + offsetof(SceI2sRegisters, data);
}

