/* Venezia mailbox interrupt and completion handling. */
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <intrmgr/intrmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <lowio/mailbox_regs.h>
#include "venezia_internal.h"
#include <lowio/pervasive.h>
#include <sysmem/cpu.h>

/* name unknown: host-side Venezia mailbox interrupt state. */
typedef struct VeneziaMailboxState {
	ScePervasiveMailboxRegisters *registers;
	SceUID blockingEvent;
	SceKernelSpinlock completionLock;
	unsigned int completedChannels;
} VeneziaMailboxState;

VeneziaMailboxState g_veneziaMailboxState;

/* name unknown: acknowledge incoming mailbox notifications. */
int veneziaMailboxIntrHandler(int intrCode, void *userCtx)
{
	unsigned int channel = (uintptr_t)userCtx & 15;
	unsigned int mask = 1U << channel;
	ScePervasiveMailboxRegisters *registers = g_veneziaMailboxState.registers;
	volatile uint32_t *receive;
	unsigned int pending;
	int intr;

	receive = registers->receive;
	intr = SceCpuForDriver_821FC0EE();
	pending = receive[channel];
	receive[channel] = pending;
	__asm__ volatile("dmb sy" ::: "memory");
	(void)receive[channel];
	__asm__ volatile("dsb sy" ::: "memory");
	sceKernelCpuResumeIntr(intr);
	if (pending << 16)
		sceKernelSetEventFlag(g_veneziaMailboxState.blockingEvent, mask);
	if (pending >> 16) {
		intr = SceCpuForDriver_D32ACE9E(
			&g_veneziaMailboxState.completionLock);
		g_veneziaMailboxState.completedChannels |= mask;
		sceKernelSpinlockLowUnlockCpuResumeIntr(
			&g_veneziaMailboxState.completionLock, intr);
	}
	return -1;
}

/* name unknown: attach the sixteen Venezia mailbox interrupt channels. */
int initVeneziaMailbox(void)
{
	char name[32];
	unsigned int channel;
	int ret;

	g_veneziaMailboxState.registers = NULL;
	g_veneziaMailboxState.completedChannels = 0;
	g_veneziaMailboxState.blockingEvent = -1;
	g_veneziaMailboxState.completionLock = 0;
	g_veneziaMailboxState.registers = (ScePervasiveMailboxRegisters *)(uintptr_t)
		ScePervasiveForDriver_04D145C7();
	ret = sceKernelCreateEventFlag("SceVeneziaRpcBlocking", 0x1000, 0, NULL);
	if (ret < 0)
		return ret;
	g_veneziaMailboxState.blockingEvent = ret;
	for (channel = 0; channel < 16; channel++) {
		snprintf(name, sizeof(name), "SceVeneziaMailbox%02u", channel);
		ret = ksceKernelRegisterIntrHandler(channel + 80, name, 0,
			veneziaMailboxIntrHandler, (void *)(uintptr_t)channel, 208, 15, NULL);
		if (ret < 0)
			return ret;
	}
	for (channel = 80; channel < 96; channel++) {
		ret = ksceKernelEnableIntr(channel);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* name unknown: enable all mailbox channels after resume. */
int enableVeneziaMailboxInterrupts(void)
{
	unsigned int channel;
	int ret;

	for (channel = 80; channel < 96; channel++) {
		ret = ksceKernelEnableIntr(channel);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* name unknown: flush shared RPC data and notify the DSP mailbox. */
int submitVeneziaRpc(unsigned int channel)
{
	ScePervasiveMailboxRegisters *registers = g_veneziaMailboxState.registers;
	volatile uint32_t *send;

	channel &= 15;
	SceCpuForDriver_E813EBB2();
	send = registers->send;
	send[channel] = 0x12345678;
	return 0;
}

/* name unknown: wait and consume the selected blocking completion. */
int waitVeneziaRpc(unsigned int channel)
{
	return sceKernelWaitEventFlag(g_veneziaMailboxState.blockingEvent,
		1U << (channel & 15), SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
}

/* name unknown: consume all asynchronous mailbox completions. */
unsigned int getCompletedVeneziaRpc(void)
{
	unsigned int completed;
	int intr;

	intr = SceCpuForDriver_D32ACE9E(
		&g_veneziaMailboxState.completionLock);
	completed = g_veneziaMailboxState.completedChannels;
	g_veneziaMailboxState.completedChannels = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(
		&g_veneziaMailboxState.completionLock, intr);
	return completed;
}

extern int veneziaBlockingRpc(unsigned int command, ...)
	__asm__("SceCodecEngineWrapperForDriver_0D3BB164");
extern unsigned int SceCodecEngineWrapperForDriver_F9495589(unsigned int physical);
extern unsigned int SceCodecEngineWrapperForDriver_C7C150AE(unsigned int address);

int SceCodecEngineWrapperForDriver_2DD83BB0(unsigned int argument0, unsigned int argument1)
{
	int physical;

	physical = veneziaBlockingRpc(496, argument0, argument1);
	return SceCodecEngineWrapperForDriver_F9495589(physical);
}

int SceCodecEngineWrapperForDriver_3C5E9CC3(unsigned int argument0, unsigned int address,
					unsigned int argument2)
{
	unsigned int physical;

	physical = SceCodecEngineWrapperForDriver_C7C150AE(address);
	return veneziaBlockingRpc(497, argument0, physical, argument2);
}

int SceCodecEngineWrapperForDriver_91535820(unsigned int argument0, unsigned int argument1)
{
	return veneziaBlockingRpc(498, argument0, argument1);
}

int SceCodecEngineWrapperForDriver_3A79A935(unsigned int argument0, unsigned int argument1,
					unsigned int argument2)
{
	return veneziaBlockingRpc(499, argument0, argument1, argument2);
}

/* name unknown: internal wrapper for DSP RPC command 500. */
int callVeneziaRpcCommand500(unsigned int argument0, unsigned int argument1)
{
	return veneziaBlockingRpc(500, argument0, argument1);
}

int SceCodecEngineWrapperForDriver_0F235F44(unsigned int argument0, unsigned int argument1)
{
	return veneziaBlockingRpc(501, argument0, argument1);
}

int SceCodecEngineWrapperForDriver_26FAF6EC(unsigned int argument0, unsigned int argument1)
{
	return veneziaBlockingRpc(502, argument0, argument1);
}
