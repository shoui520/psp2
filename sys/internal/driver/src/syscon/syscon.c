/*
 * SceSyscon: the system controller link.
 *
 * Packet transport, notifications, startup and the command wrappers.
 */

#include <sysmem/sysmem.h>
#include <excpmgr/moncall.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/lowio/pervasive.h>
#include <sysmem/barrier.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysevent.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#include "syscon_private.h"

/* Sony's debug build is fortified */
#if FW_DEBUG
#include <sysmem/fortify.h>
#endif

struct SysconControl s_sysconControl;

static int sysconSetCallback(SceSysconCallback callback, void *argument,
			     unsigned int slot)
{
	uint32_t state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysconControl.callbackLock);
	s_sysconControl.callbacks[slot].callback = callback;
	s_sysconControl.callbacks[slot].argp = argument;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysconControl.callbackLock,
					       state);
	return 0;
}

int SceSysconForDriver_18A6F4D9(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 0);
}

int SceSysconForDriver_DE613081(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 1);
}

int SceSysconForDriver_229A07C2(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 2);
}

int SceSysconForDriver_8351526D(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 3);
}

int SceSysconForDriver_9F8340FF(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 4);
}

int SceSysconForDriver_35E1689F(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 5);
}

int SceSysconForDriver_474A9EA7(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 6);
}

int SceSysconForDriver_4E88B4D9(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 8);
}

int SceSysconForDriver_376CCCB8(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 9);
}

int SceSysconForDriver_3BAAC8A9(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 10);
}

int SceSysconForDriver_4A42712F(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 15);
}

int ksceSysconSetLowBatteryCallback(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 16);
}

int SceSysconForDriver_80D6E061(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 17);
}

int SceSysconForDriver_7682FE69(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 18);
}

int SceSysconForDriver_E0D52DF0(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 19);
}

int SceSysconForDriver_2D471528(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 20);
}

int SceSysconForDriver_129EA022(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 21);
}

int SceSysconForDriver_85E5DEBF(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 22);
}

int ksceSysconSetThermalAlertCallback(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 23);
}

int SceSysconForDriver_2E4BA4B8(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 25);
}

int SceSysconForDriver_C442D0BE(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 26);
}

int SceSysconForDriver_61AE3970(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 27);
}

int SceSysconForDriver_94678881(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 28);
}

int SceSysconForDriver_7AA00C01(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 29);
}

int ksceSysconSetStandbyButtonCallback(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 32);
}

int SceSysconForDriver_154676F1(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 33);
}

int SceSysconForDriver_63352A39(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 34);
}

int SceSysconForDriver_14730196(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 35);
}

int ksceSysconSetAlarmCallback(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 36);
}

int SceSysconForDriver_A26586B2(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 37);
}

int SceSysconForDriver_67A4CB9F(SceSysconCallback callback, void *argument)
{
	return sysconSetCallback(callback, argument, 38);
}


static int _sceSysconGpioIntr(void *subintrArg, void *registerArg, unsigned char intrPriority);

static void sysconStartPacket(SceSysconPacket *packet)
{
	volatile SceSpiRegisters *spi = s_sysconControl.spiRegisters;
	const SceSysconDebugHandlers *handlers;
	const uint8_t *tx;
	uint32_t offset, length;
	uint8_t checksum;

	packet->time = ksceKernelGetSystemTimeLow();
	handlers = s_sysconControl.debugHandlers;
	if (handlers)
		handlers->start(packet);
	tx = packet->status & SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX ? packet->externalTx : packet->tx;
	ksceGpioPortClear(0, 3);
	while (spi->receiveCount)
		(void)spi->receive;
	(void)spi->transmitCount;
	spi->intStatus = 0x600;
	if (!(packet->status & SYSCON_PACKET_COMPLETION)) {
		length = tx[2];
		for (offset = 0; offset <= length + 2; offset += 2)
			spi->transmit = tx[offset] | (uint32_t)tx[offset + 1] << 8;
	} else {
		spi->transmit = tx[0] | (uint32_t)tx[1] << 8;
		checksum = ~(tx[0] + tx[1] + 1);
		spi->transmit = 1 | (uint32_t)checksum << 8;
	}
	spi->control = 0;
	spi->enable = 1;
	(void)spi->enable;
	dsb();
	ksceGpioPortSet(0, 3);
	packet->status |= SYSCON_PACKET_STARTED | SYSCON_PACKET_TRANSFERRING;
	if (s_sysconControl.logger)
		s_sysconControl.logger(0, 0x22,
			packet->request.command[0] | (uint32_t)packet->request.command[1] << 8, packet->status,
			packet->request.command[0], packet->request.command[1], packet->request.length, packet->request.data[0],
			packet->request.data[1], packet->request.data[2], packet->request.data[3], packet->request.data[4],
			packet->request.data[5], packet->request.data[6], packet->request.data[7], packet->request.data[8],
			packet->request.data[9], packet->request.data[10], packet->request.data[11], packet->request.data[12],
			packet->request.data[13], packet->request.data[14], packet->request.data[15], packet->request.data[16],
			packet->request.data[17], packet->request.data[18], packet->request.data[19], packet->request.data[20],
			packet->request.data[21], packet->request.data[22], packet->request.data[23], packet->request.data[24],
			packet->request.data[25], packet->request.data[26], packet->request.data[27], packet->request.data[28]);
}

static void sysconWaitTransferDelay(uint32_t delay)
{
	while ((uint32_t)(ksceKernelGetSystemTimeLow() -
			  s_sysconControl.lastTransferTime) <= delay)
		;
}

static int sysconPreparePacket(SceSysconPacket *packet, uint32_t flags,
			      int (*callback)(SceSysconPacket *, void *),
			      void *argument)
{
	uint8_t *tx;
	uint32_t length, index;
	uint8_t checksum = 0;

	if (s_sysconControl.stopping)
		return SCE_SYSCON_ERROR_NOEXEC;
	if (!(flags & SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX)) {
		tx = packet->tx;
		packet->externalTx = NULL;
		if (tx[2] < 1 || tx[2] > 0x1d)
			return SCE_SYSCON_ERROR_TX_SIZE_TOO_LONG;
	} else {
		tx = packet->externalTx;
		if (!tx)
			return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
		packet->request.command[0] = tx[0];
		packet->request.command[1] = tx[1];
		packet->request.length = tx[2];
		if ((uint8_t)(tx[2] - 1) > 0xfc)
			return SCE_SYSCON_ERROR_TX_SIZE_TOO_LONG;
	}
	if (!(flags & SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX)) {
		packet->externalRx = NULL;
		packet->externalRxSize = 0;
	} else {
		if (!packet->externalRx)
			return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
		if (packet->externalRxSize < 0x21)
			return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	if (!(flags & SCE_SYSCON_PACKET_FLAG_CHECKSUMMED)) {
		length = tx[2];
		for (index = 0; index < length + 2; index++)
			checksum += tx[index];
		tx[length + 2] = ~checksum;
		if (length < 0x1d)
			memset(tx + length + 3, 0xff, 0x1d - length);
	}
	memset(packet->rx, 0xff, sizeof(packet->rx));
	packet->callback = callback;
	packet->argp = argument;
	packet->status = (flags & 0xffff) | SYSCON_PACKET_QUEUED;
	packet->received = 0;
	packet->next = NULL;
	packet->semaId = -1;
	if ((int8_t)tx[0] < 0) {
		packet->queue = tx[1] > 15 ? 16 : tx[1];
		packet->delay = s_sysconControl.commandPacketDelay;
	} else {
		packet->queue = 17;
		packet->delay = 0;
	}
	return 0;
}

static void sysconQueuePacket(SceSysconPacket *packet, uint32_t flags)
{
	SceSysconPacket *head, *selected;
	uint32_t slot = packet->queue;
	uint32_t delay;

	head = s_sysconControl.queueHeads[slot];
	if (!head) {
		s_sysconControl.queueHeads[slot] = packet;
		s_sysconControl.queueTails[slot] = packet;
	} else if (flags & SCE_SYSCON_PACKET_FLAG_PRIORITY) {
		if (!(head->status & SYSCON_PACKET_IN_FLIGHT)) {
			packet->next = head;
			s_sysconControl.queueHeads[slot] = packet;
		} else {
			packet->next = head->next;
			head->next = packet;
			if (!packet->next)
				s_sysconControl.queueTails[slot] = packet;
		}
	} else {
		s_sysconControl.queueTails[slot]->next = packet;
		s_sysconControl.queueTails[slot] = packet;
	}
	if (s_sysconControl.currentPacket || s_sysconControl.interruptActive)
		return;
	if (s_sysconControl.polling) {
		delay = slot == 17 ? 4 : s_sysconControl.commandPacketDelay;
		sysconWaitTransferDelay(delay);
		s_sysconControl.currentPacket = packet;
		sysconStartPacket(packet);
		return;
	}
	selected = s_sysconControl.queueHeads[17];
	if (!selected || (selected == packet && (flags & 2))) {
		if (slot == 17)
			return;
selectOwnQueue:
		selected = s_sysconControl.queueHeads[slot];
		if (selected == packet) {
			if (flags & 2)
				return;
		} else if (!selected) {
			return;
		}
	} else if (slot != 17 && (flags & 1)) {
		goto selectOwnQueue;
	}
	delay = s_sysconControl.affirmativeRertyMode ?
		s_sysconControl.startupPacketDelay : 4;
	sysconWaitTransferDelay(delay);
	s_sysconControl.currentPacket = selected;
	sysconStartPacket(selected);
}

int ksceSysconCmdExecAsync(SceSysconPacket *packet, uint32_t flags,
			 int (*callback)(SceSysconPacket *, void *),
			 void *argument)
{
	uint32_t state;
	int result;

	result = sysconPreparePacket(packet, flags, callback, argument);
	if (result < 0)
		return result;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysconControl.callbackLock);
	sysconQueuePacket(packet, flags);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysconControl.callbackLock, state);
	return 0;
}

int ksceSysconCmdSync(SceSysconPacket *packet, int noWait)
{
	uint32_t status, state;
	int result, semaphore;
	unsigned int slot;

	if (!noWait) {
		if (!s_sysconControl.polling) {
			result = sceKernelCheckWaitableStatus();
			if (result < 0) {
#if FW_DEBUG
				if (ksceKernelGetAssertLevel() > 0)
					ksceKernelAssertLevel(1, 0,
						SCE_DBGINFO_FUNC(2454, "sceSysconCmdSync"),
						__builtin_return_address(0));
#endif
				return result;
			}
			state = ksceKernelSpinlockLowLockCpuSuspendIntr(
						&s_sysconControl.callbackLock);
			if (!(packet->status & SYSCON_PACKET_DONE)) {
				semaphore = ksceKernelCreateSema("SceSysconSync", 0,
							       0, 1, NULL);
				if (semaphore < 0) {
					ksceKernelSpinlockLowUnlockCpuResumeIntr(
						&s_sysconControl.callbackLock, state);
					return semaphore;
				}
				packet->semaId = semaphore;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(
					&s_sysconControl.callbackLock, state);
				result = ksceKernelWaitSema(semaphore, 1, NULL);
#if FW_DEBUG
				if (ksceKernelGetAssertLevel() > 0)
					ksceKernelAssertLevel(1, result >= 0,
						SCE_DBGINFO_FUNC(2493, "sceSysconCmdSync"),
						__builtin_return_address(0));
#endif
				result = ksceKernelDeleteSema(semaphore);
#if FW_DEBUG
				if (ksceKernelGetAssertLevel() > 0)
					ksceKernelAssertLevel(1, result >= 0,
						SCE_DBGINFO_FUNC(2495, "sceSysconCmdSync"),
						__builtin_return_address(0));
#endif
			} else {
				ksceKernelSpinlockLowUnlockCpuResumeIntr(
					&s_sysconControl.callbackLock, state);
			}
		} else {
			while (!(packet->status & SYSCON_PACKET_DONE)) {
				sysconWaitTransferDelay(packet->queue == 17 ? 4 :
						       s_sysconControl.startupPacketDelay);
				while (!ksceGpioQueryIntr(0, 4))
					;
				_sceSysconGpioIntr((void *)4, NULL, 0);
			}
		}
	} else {
		if (noWait != 1)
			return SCE_KERNEL_ERROR_SYSCON_ERROR;
		if (!packet) {
			for (slot = 0; slot < 17; slot++) {
				if (s_sysconControl.queueHeads[slot])
					return 1;
			}
			return 0;
		}
		if (!(packet->status & SYSCON_PACKET_DONE))
			return 1;
	}
	status = packet->status;
	if (status & SYSCON_PACKET_TRANSFER_ERROR)
		return SCE_SYSCON_ERROR_ILLEGAL_STATUS;
	if (status & SYSCON_PACKET_RX_OVERFLOW)
		return SCE_SYSCON_ERROR_INVALID_FLAGS;
	if (!(packet->reply.result & 0x3f))
		return 0;
	return packet->reply.result | SCE_SYSCON_ERROR_REPLY;
}

static int sysconSubmitSynchronousPacket(SceSysconPacket *packet,
					 uint32_t flags, const void *caller)
{
	uint32_t state;
	int result;

	if (!s_sysconControl.polling) {
		result = sceKernelCheckWaitableStatus();
		if (result < 0) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelAssertLevel(1, 0,
					SCE_DBGINFO_FUNC(2153, "sceSysconCmdExec"),
					caller);
#endif
			return result;
		}
	} else if (ksceKernelIsIntrAllowedInCurrentContext()) {
		return SCE_SYSCON_ERROR_NOT_INTR_ALLOWED;
	}
	result = sysconPreparePacket(packet, flags, NULL, NULL);
	if (result < 0)
		return result;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysconControl.callbackLock);
	sysconQueuePacket(packet, flags);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysconControl.callbackLock, state);
	return 0;
}

int ksceSysconCmdExec(SceSysconPacket *packet, uint32_t flags)
{
	int result;

	result = sysconSubmitSynchronousPacket(packet, flags,
					       __builtin_return_address(0));
	if (result < 0)
		return result;
	return ksceSysconCmdSync(packet, 0);
}

int SceSysconForDriver_C9DB5BFF(SceSysconPacket *packet)
{
	SceSysconPacket *current, *previous = NULL;
	uint32_t state, slot = packet->queue;
	int result = 0;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysconControl.callbackLock);
	current = s_sysconControl.queueHeads[slot];
	while (current && current != packet) {
		previous = current;
		current = current->next;
	}
	if (!current) {
		result = SCE_SYSCON_ERROR_INVALID_POINTER;
	} else if (current->status & SYSCON_PACKET_IN_FLIGHT) {
		result = SCE_SYSCON_ERROR_INVALID_STATUS;
	} else {
		if (previous)
			previous->next = current->next;
		else
			s_sysconControl.queueHeads[slot] = current->next;
		if (!current->next)
			s_sysconControl.queueTails[slot] = previous;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysconControl.callbackLock, state);
	return result;
}

int ksceSysconSetDebugHandlers(const SceSysconDebugHandlers *handlers)
{
	s_sysconControl.debugHandlers = handlers;
	return 0;
}

int ksceSysconWaitInitialized(void)
{
	while (!s_sysconControl.initialized)
		sceKernelDelayThread(1000);
	return 0;
}

int ksceSysconSetAffirmativeRertyMode(uint32_t mode)
{
	s_sysconControl.affirmativeRertyMode = mode;
	return 0;
}

int SceSysconForDriver_0D58E297(uint32_t polling)
{
	ksceKernelSetIntrTarget(0xf8, polling ? 8 : 0xf);
	s_sysconControl.polling = polling;
	return 0;
}

uint64_t ksceSysconGetBaryonTimestamp(void)
{
	return s_sysconControl.baryonTimestamp;
}

struct SysconNotificationDescriptor {
	uint8_t byte;
	uint8_t mask;
	uint8_t invert;
	uint8_t slot;
	volatile int8_t *state;
};

static const struct SysconNotificationDescriptor s_sysconNotifications[25] = {
	{ 0, 0x01, 0, 0, &s_sysconControl.notificationState[SYSCON_STATE_WLAN_BT] },
	{ 0, 0x02, 0, 1, &s_sysconControl.notificationState[SYSCON_STATE_N0_BIT1] },
	{ 0, 0x04, 0, 2, &s_sysconControl.notificationState[SYSCON_STATE_N0_BIT2] },
	{ 0, 0x08, 0, 3, &s_sysconControl.notificationState[SYSCON_STATE_N0_BIT3] },
	{ 0, 0x10, 0, 4, &s_sysconControl.notificationState[SYSCON_STATE_N0_BIT4] },
	{ 0, 0x20, 0, 5, &s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN] },
	{ 0, 0x40, 0, 6, &s_sysconControl.notificationState[SYSCON_STATE_N0_BIT6] },
	{ 1, 0x01, 0, 8, &s_sysconControl.notificationState[SYSCON_STATE_N1_BIT0] },
	{ 1, 0x02, 0, 9, &s_sysconControl.notificationState[SYSCON_STATE_N1_BIT1] },
	{ 1, 0x04, 0, 10, &s_sysconControl.notificationState[SYSCON_STATE_N1_BIT2] },
	{ 1, 0x40, 0, 14, &s_sysconControl.notificationState[SYSCON_STATE_N1_BIT6] },
	{ 1, 0x80, 0, 15, &s_sysconControl.notificationState[SYSCON_STATE_N1_BIT7] },
	{ 2, 0x01, 0, 16, &s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY] },
	{ 2, 0x02, 0, 17, &s_sysconControl.notificationState[SYSCON_STATE_POWER_SOURCE] },
	{ 2, 0x04, 0, 18, &s_sysconControl.notificationState[SYSCON_STATE_N2_BIT2] },
	{ 2, 0x08, 0, 19, &s_sysconControl.notificationState[SYSCON_STATE_N2_BIT3] },
	{ 2, 0x10, 0, 20, &s_sysconControl.notificationState[SYSCON_STATE_N2_BIT4] },
	{ 2, 0x20, 0, 21, &s_sysconControl.notificationState[SYSCON_STATE_USB_POWER] },
	{ 2, 0x40, 0, 22, &s_sysconControl.notificationState[SYSCON_STATE_N2_BIT6] },
	{ 2, 0x80, 0, 23, &s_sysconControl.notificationState[SYSCON_STATE_THERMAL_ALERT] },
	{ 3, 0x02, 0, 25, &s_sysconControl.notificationState[SYSCON_STATE_N3_BIT1] },
	{ 3, 0x04, 0, 26, &s_sysconControl.notificationState[SYSCON_STATE_N3_BIT2] },
	{ 3, 0x08, 0, 27, &s_sysconControl.notificationState[SYSCON_STATE_N3_BIT3] },
	{ 3, 0x10, 0, 28, &s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_REBOOT] },
	{ 3, 0x20, 0, 29, &s_sysconControl.notificationState[SYSCON_STATE_LOW_BATTERY_INHIBIT_UPDATE_DOWNLOAD] },
};

static void sysconDecodeNotifications(uint32_t bits)
{
	const struct SysconNotificationDescriptor *descriptor;
	unsigned int index;
	uint8_t value;

	for (index = 0; index < 25; index++) {
		descriptor = &s_sysconNotifications[index];
		value = descriptor->invert;
		if (((bits >> (descriptor->byte * 8)) & descriptor->mask) != 0)
			value ^= 1;
		*descriptor->state = value;
	}
}

static int sysconReadWord(uint32_t *value, uint32_t command)
{
	SceSysconPacket packet;
	uint32_t resultValue = 0;
	int result;

	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	packet.request.command[0] = command;
	packet.request.command[1] = command >> 8;
	packet.request.length = 1;
	result = sysconSubmitSynchronousPacket(&packet, 0,
					       __builtin_return_address(0));
	if (result < 0)
		return result;
	result = ksceSysconCmdSync(&packet, 0);
	if (result < 0)
		return result;
	if (packet.reply.length < 3 || packet.reply.length > 6)
		return SCE_SYSCON_ERROR_ILLEGAL_SIZE;
	memcpy(&resultValue, packet.reply.data, packet.reply.length - 2);
	*value = resultValue;
	return 0;
}

static int sysconWriteWord(uint32_t value, uint32_t command, uint8_t length)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = command;
	packet.request.command[1] = command >> 8;
	packet.request.length = length;
	packet.request.data[0] = value;
	packet.request.data[1] = value >> 8;
	packet.request.data[2] = value >> 16;
	packet.request.data[3] = value >> 24;
	result = sysconSubmitSynchronousPacket(&packet, 0,
					       __builtin_return_address(0));
	if (result < 0)
		return result;
	return ksceSysconCmdSync(&packet, 0);
}

static int sysconReadTimestamp(uint8_t *text)
{
	SceSysconPacket packet;
	int result;

	if (!text)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_TIME_STAMP);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	text[12] = 0;
	memcpy(text, &packet.reply.data[0], 12);
	return 0;
}

static int sysconReadMultiCnInfo(uint32_t *value)
{
	uint32_t information;
	uint8_t revision;
	int result;

	if ((s_sysconControl.hardwareInfo & 0xff0000) >= 0x400000)
		return sysconReadWord(value, SYSCON_CMD_GET_MULTI_CN_INFO);
	result = sysconReadWord(&information, SYSCON_CMD_GET_MULTI_CN_INFO);
	if (result < 0)
		return result;
	switch ((information >> 8) & 0xff) {
	case 0x65:
		revision = 4;
		break;
	case 0x55:
		revision = 5;
		break;
	case 0x51:
		revision = 2;
		break;
	case 0x60:
		revision = 0;
		break;
	default:
		revision = 0xff;
		break;
	}
	information = (information & ~0xff00U) | (uint32_t)revision << 8;
	if (value)
		*value = information;
	return result;
}

static int sysconSysEvent(int resume, int eventid, void *args, void *opt)
{
	volatile SceSpiRegisters *spi = s_sysconControl.spiRegisters;
	SceSysconPacket *packet;
	const uint32_t *resumeInfo;
	unsigned int slot;

	if (!resume) {
		if (eventid == SYSEVENT_SUSPEND_DEVICE(6)) {
			ksceKernelDisableSubIntr(0xf8, 4);
		} else if (eventid == SYSEVENT_SUSPEND_DEVICE(15)) {
			ksceKernelSetIntrTarget(0xf8, 8);
			s_sysconControl.polling = 1;
		} else if (eventid == SYSEVENT_SUSPEND_IDLE_QUERY) {
			packet = s_sysconControl.queueHeads[0];
			if (!packet) {
				for (slot = 1; slot < 17; slot++) {
					if (s_sysconControl.queueHeads[slot])
						break;
				}
				if (slot == 17)
					return 0;
				for (slot = 1; slot < 17; slot++) {
					packet = s_sysconControl.queueHeads[slot];
					if (packet)
						break;
				}
				if (!packet)
					return -1;
			}
			return packet->request.command[0] | (uint32_t)packet->request.command[1] << 8 |
			       SCE_SYSCON_ERROR_PACKET_PENDING;
		}
	} else if (resume == 1) {
		if (eventid == SYSEVENT_RESUME_DEVICE(6)) {
			resumeInfo = ((const uint32_t *const *)args)[1];
			kscePervasiveSpiClockEnable(0);
			kscePervasiveSpiResetDisable(0);
			spi->mode = 0;
			dmb();
			ksceGpioSetPortMode(0, 3, 0);
			ksceGpioSetPortMode(0, 4, 1);
			ksceGpioSetIntrMode(0, 4, 3);
			ksceKernelEnableSubIntr(0xf8, 4);
			s_sysconControl.lastTransferTime = ksceKernelGetSystemTimeLow();
			s_sysconControl.notificationBits = resumeInfo[3];
			sysconDecodeNotifications(s_sysconControl.notificationBits);
			s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT14] =
				((resumeInfo[5] ^ 0x4000) >> 14) & 1;
			s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT12] =
				((resumeInfo[5] ^ 0x1000) >> 12) & 1;
			s_sysconControl.notificationState[SYSCON_STATE_HEADPHONE] = (resumeInfo[5] >> 27) & 1;
			if (s_sysconControl.hardwareInfo & 8)
				s_sysconControl.pendingCommand130 = 1;
			s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN] = -1;
			s_sysconControl.pendingMultiCnInfo = 1;
			s_sysconControl.notificationState[SYSCON_STATE_N0_BIT6] = -1;
			s_sysconControl.notificationState[SYSCON_STATE_MOTION_DEV] =
				((resumeInfo[5] ^ 0x2000000) >> 25) & 1;
		} else if (eventid == SYSEVENT_RESUME_DEVICE(15)) {
			ksceKernelSetIntrTarget(0xf8, 0xf);
			s_sysconControl.polling = 0;
		}
	}
	return 0;
}

static int sysconReadWord(uint32_t *value, uint32_t command);
static int sysconWriteWord(uint32_t value, uint32_t command, uint8_t length);
static int sysconReadTimestamp(uint8_t *text);
static int sysconReadMultiCnInfo(uint32_t *value);

static int sysconStartupWorker(void *argument)
{
	uint32_t value = 0;
	uint64_t timestamp = 0;
	unsigned int index;

	while (sysconReadWord(&s_sysconControl.baryonVersion, SYSCON_CMD_GET_ERNIE_VERSION) < 0)
		;
	while (sysconReadTimestamp(s_sysconControl.baryonTimestampText) < 0)
		;
	for (index = 0; index < 15 && s_sysconControl.baryonTimestampText[index];
	     index++)
		timestamp = timestamp * 10 +
			    (int64_t)((int)(int8_t)s_sysconControl.baryonTimestampText[index] - '0');
	s_sysconControl.baryonTimestamp = timestamp;
	if (s_sysconControl.baryonVersion >= 0x1000004) {
		while (sysconWriteWord(0x12, 0x80, 3) < 0)
			;
	} else if (s_sysconControl.baryonVersion > 0x70501) {
		while (sysconWriteWord(2, 0x80, 3) < 0)
			;
	}
	if (!(s_sysconControl.statusFlags & SYSCON_STATUS_DOWNLOADER_MODE)) {
		while (sysconReadMultiCnInfo(&value) < 0)
			;
		if (s_sysconControl.hardwareInfo & 8) {
			while (sysconReadWord(&value, 0x130) < 0)
				;
		}
	}
	s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN] = -1;
	s_sysconControl.notificationState[SYSCON_STATE_N0_BIT6] = -1;
	while (sysconReadWord(&value, SYSCON_CMD_GET_SLEEP_FACTOR) < 0)
		;
	s_sysconControl.notificationBits = value;
	sysconDecodeNotifications(value);
	s_sysconControl.pendingMultiCnInfo = 1;
	s_sysconControl.initialized = 1;
	s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN] = -1;
	s_sysconControl.notificationState[SYSCON_STATE_N0_BIT6] = -1;
	return 1;
}

static SceSysconPacket s_sysconCommand130;
static SceSysconPacket s_sysconCommandCompletion;
static SceSysconPacket s_sysconCommandStatus;
static SceSysconPacket s_sysconCommandMultiCnInfo;

static void sysconInitializeInternalPacket(SceSysconPacket *packet,
					 uint16_t command)
{
	__builtin_memset(packet, 0, sizeof(*packet));
	packet->queue = 17;
	packet->semaId = -1;
	packet->request.command[0] = command;
	packet->request.command[1] = command >> 8;
	packet->request.length = 1;
	packet->request.data[0] = ~(packet->request.command[0] + packet->request.command[1] + 1);
}

static __attribute__((noinline)) int sysconInitialize(void)
{
	SceKernelAllocMemBlockKernelOpt options;
	volatile SceSpiRegisters *spi;
	int result;

	memset(&s_sysconControl, 0, offsetof(struct SysconControl, logger));
	sysconInitializeInternalPacket(&s_sysconCommandStatus, 3);
	sysconInitializeInternalPacket(&s_sysconCommandCompletion, 4);
	sysconInitializeInternalPacket(&s_sysconCommandMultiCnInfo, SYSCON_CMD_GET_MULTI_CN_INFO);
	sysconInitializeInternalPacket(&s_sysconCommand130, 0x130);
	s_sysconControl.startupPacketDelay = 4000;
	s_sysconControl.commandPacketDelay = 20000;
	s_sysconControl.command885State = 7;
	s_sysconControl.affirmativeRertyMode = 1;
	s_sysconControl.command884State = 3;
	ksceGpioPortClear(0, 3);
	s_sysconControl.lastTransferTime = ksceKernelGetSystemTimeLow();
	ksceGpioSetPortMode(0, 3, 0);
	ksceGpioSetPortMode(0, 4, 1);
	ksceGpioSetIntrMode(0, 4, 3);
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.paddr = SPI_REG_BASE(0);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	result = ksceKernelAllocMemBlock("SceSpi0Reg",
					 SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					 0x1000, &options);
	if (result < 0)
		return result;
	s_sysconControl.spiMemblock = result;
	ksceKernelGetMemBlockBase(result, (void **)&s_sysconControl.spiRegisters);
	ksceKernelRegisterSubIntrHandler(0xf8, 4, "SceSysconCmdclr",
				       _sceSysconGpioIntr, NULL);
	ksceKernelEnableSubIntr(0xf8, 4);
	s_sysconControl.hardwareInfo = ksceSysrootGetHardwareInfo();
	ksceSysrootGetHardwareFlags(s_sysconControl.hardwareFlags);
	ksceKernelRegisterSysEventHandler("SceSyscon", sysconSysEvent, NULL);
	result = ksceKernelCreateMutex("SceSyscon", 0, 0, NULL);
	s_sysconControl.mutex = result;
	spi = s_sysconControl.spiRegisters;
	if (result < 0)
		return result;
	kscePervasiveSpiClockEnable(0);
	kscePervasiveSpiResetDisable(0);
	spi->mode = 0;
	dmb();
	ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceSysconInit", sysconStartupWorker, NULL);
	s_sysconControl.logger = (SysconLogger)SceSysrootForDriver_3AE319DA();
	return 0;
}

int module_start(unsigned int args, void *argp)
{
	return sysconInitialize() < 0 ? 2 : 0;
}

static uint16_t sysconPacketCommand(const SceSysconPacket *packet)
{
	return packet->request.command[0] | (uint16_t)packet->request.command[1] << 8;
}

static uint32_t sysconResponseWord(const uint8_t *response)
{
	return response[4] | (uint32_t)response[5] << 8 |
	       (uint32_t)response[6] << 16 | (uint32_t)response[7] << 24;
}

static void sysconDispatchNotification(unsigned int slot, int value)
{
	SceSysconCallback callback = s_sysconControl.callbacks[slot].callback;

	if (!callback)
		return;
	if (s_sysconControl.logger)
		s_sysconControl.logger(5, 4, slot, value, callback,
				      s_sysconControl.callbacks[slot].argp);
	callback(value, s_sysconControl.callbacks[slot].argp);
	if (s_sysconControl.logger)
		s_sysconControl.logger(6, 1, slot);
}

static void sysconDecodePacketNotifications(const uint8_t *response)
{
	const struct SysconNotificationDescriptor *descriptor;
	uint32_t changed = 0;
	unsigned int index;
	int previous, value;

	s_sysconControl.notificationBits = sysconResponseWord(response);
	for (index = 0; index < 25; index++) {
		descriptor = &s_sysconNotifications[index];
		previous = *descriptor->state;
		value = descriptor->invert;
		if (response[4 + descriptor->byte] & descriptor->mask)
			value ^= 1;
		*descriptor->state = value;
		if (previous != value || previous == -1)
			changed |= 1U << descriptor->slot;
	}
	for (index = 0; index < 25; index++) {
		descriptor = &s_sysconNotifications[index];
		if (changed & (1U << descriptor->slot))
			sysconDispatchNotification(descriptor->slot,
						   *descriptor->state);
	}
	if (changed & (1U << 5)) {
		if (!s_sysconControl.notificationState[SYSCON_STATE_MULTI_CN]) {
			if (s_sysconControl.pendingMultiCnInfo == 1)
				s_sysconControl.pendingMultiCnInfo = 0;
			s_sysconControl.multiCnInfo = 0x4ff00;
			sysconDispatchNotification(SYSCON_CALLBACK_MULTI_CN, 0x4ff00);
		} else if (!s_sysconControl.pendingMultiCnInfo) {
			s_sysconControl.pendingMultiCnInfo = 1;
		}
	}
	if (changed & (1U << 14)) {
		if (!s_sysconControl.notificationState[SYSCON_STATE_N1_BIT6]) {
			if (s_sysconControl.pendingCommand130 == 1)
				s_sysconControl.pendingCommand130 = 0;
			s_sysconControl.command130Info &= 0xff0000ff;
			sysconDispatchNotification(SYSCON_CALLBACK_COMMAND130,
					 s_sysconControl.command130Info);
		} else if (!s_sysconControl.pendingCommand130) {
			s_sysconControl.pendingCommand130 = 1;
		}
	}
}

static void sysconDecodePowerNotifications(const uint8_t *response)
{
	uint32_t changed = 0;
	int previous, value;
	unsigned int index;
	int values[4] = {
		((response[5] >> 6) & 1) ^ 1,
		((response[5] >> 4) & 1) ^ 1,
		(response[7] >> 3) & 1,
		((response[7] >> 1) & 1) ^ 1,
	};

	for (index = 0; index < 4; index++) {
		previous = s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT14 + index];
		value = values[index];
		s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT14 + index] = value;
		if (previous != value || previous == -1)
			changed |= 1U << index;
	}
	for (index = 0; index < 4; index++) {
		if (changed & (1U << index))
			sysconDispatchNotification(SYSCON_CALLBACK_POWER + index,
				 s_sysconControl.notificationState[SYSCON_STATE_POWER_BIT14 + index]);
	}
}

static void sysconDecodePacketResponse(SceSysconPacket *packet,
				      uint32_t *requestStatus,
				      uint32_t *requestCompletions,
				      uint8_t status)
{
	uint16_t command = sysconPacketCommand(packet);
	uint8_t *response = packet->rx;
	uint32_t size;

	s_sysconControl.pendingStatus = status;
	*requestStatus = !!(status & 4) && !s_sysconControl.polling &&
			 command != 4;
	*requestCompletions = !!(status & 0x20) && !s_sysconControl.polling &&
			     !s_sysconControl.pendingCompletions;
	if (((status ^ s_sysconControl.statusFlags) & SYSCON_STATUS_POWER_ONLINE) &&
	    s_sysconControl.callbacks[SYSCON_CALLBACK_STATUS].callback) {
		s_sysconControl.statusFlags = s_sysconControl.pendingStatus;
		sysconDispatchNotification(SYSCON_CALLBACK_STATUS, (status >> 3) & 1);
	}
	s_sysconControl.statusFlags = s_sysconControl.pendingStatus;
	if (command == 0x1b0) {
		if (!s_sysconControl.pendingCommand130)
			s_sysconControl.pendingCommand130 = 1;
		goto receive;
	}
	if (packet->status & SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX) {
		if (packet->received)
			goto receive;
		response = packet->externalRx;
	}
	switch (command) {
	case 3:
	case 4:
		sysconDecodePacketNotifications(response);
		*requestStatus = 0;
		break;
	case 0x100:
	case 0x101:
	case 0x104:
		sysconDecodePowerNotifications(response);
		break;
	case 0x103:
		s_sysconControl.pendingMultiCnInfo = 0;
		s_sysconControl.multiCnInfo = sysconResponseWord(response);
		sysconDispatchNotification(SYSCON_CALLBACK_MULTI_CN, s_sysconControl.multiCnInfo);
		break;
	case 0x130:
		s_sysconControl.pendingCommand130 = 0;
		s_sysconControl.command130Info = sysconResponseWord(response);
		sysconDispatchNotification(SYSCON_CALLBACK_COMMAND130, s_sysconControl.command130Info);
		break;
	case 0x1b0:
		if (!s_sysconControl.pendingCommand130)
			s_sysconControl.pendingCommand130 = 1;
		break;
	}
receive:
	if ((packet->reply.result & 0x3f) == 0 &&
	    (packet->status & (SYSCON_PACKET_RX_OVERFLOW | SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX)) ==
	    SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX && packet->reply.length > 1) {
		size = packet->reply.length - 2;
		if (packet->externalRxSize < packet->received + size) {
			packet->status |= SYSCON_PACKET_RX_OVERFLOW;
		} else {
			if (size)
				memmove(packet->externalRx + packet->received,
					packet->externalRx + packet->received + 4,
					size);
			packet->received += size;
		}
	}
}

static void sysconLogPacket(unsigned int event, SceSysconPacket *packet,
			    const uint8_t *bytes)
{
	int value[32];
	unsigned int index;

	if (!s_sysconControl.logger)
		return;
	for (index = 0; index < 32; index++)
		value[index] = event == 1 ? bytes[index] : (int8_t)bytes[index];
	if (event == 3) {
		s_sysconControl.logger(3, 0x21, (int16_t)sysconPacketCommand(packet),
			value[0], value[1], value[2], value[3], value[4], value[5],
			value[6], value[7], value[8], value[9], value[10], value[11],
			value[12], value[13], value[14], value[15], value[16], value[17],
			value[18], value[19], value[20], value[21], value[22], value[23],
			value[24], value[25], value[26], value[27], value[28], value[29],
			value[30], value[31]);
	} else {
		s_sysconControl.logger(event, 0x22,
			(int16_t)sysconPacketCommand(packet), packet->status,
			value[0], value[1], value[2], value[3], value[4], value[5],
			value[6], value[7], value[8], value[9], value[10], value[11],
			value[12], value[13], value[14], value[15], value[16], value[17],
			value[18], value[19], value[20], value[21], value[22], value[23],
			value[24], value[25], value[26], value[27], value[28], value[29],
			value[30], value[31]);
	}
}

static void sysconDecodeCommandCompletions(SceSysconPacket *packet,
					 const void *caller)
{
	SceSysconPacket *pending;
	uint16_t command, expected;
	uint32_t length = packet->reply.length;
	unsigned int size, offset, queue;

#if FW_DEBUG
#define SYSCON_EXECUTION_WARNING(line) do { \
	if (ksceKernelGetAssertLevel() > 0) \
		ksceKernelPrintfAssertLevel(1, 0, \
			SCE_DBGINFO_FUNC(line, "_sceSysconGpioIntr"), caller, \
			"[SceSyscon]: unexpected cmd execution info 0x%02X\n", \
			length); \
} while (0)
#endif
	if (length == 6) {
#if FW_DEBUG
		if (packet->reply.status[0] & 0x20)
			SYSCON_EXECUTION_WARNING(1771);
#endif
		return;
	}
	if (length < 8) {
#if FW_DEBUG
		SYSCON_EXECUTION_WARNING(1775);
#endif
		return;
	}
	if (length < 0x1d) {
#if FW_DEBUG
		if (length & 1)
			SYSCON_EXECUTION_WARNING(1781);
#endif
		size = length - 6;
	} else {
#if FW_DEBUG
		SYSCON_EXECUTION_WARNING(1778);
#endif
		size = 0x16;
	}
	for (offset = 0; offset + 1 < size; offset += 2) {
		command = packet->reply.data[4 + offset] |
			  (uint16_t)packet->reply.data[5 + offset] << 8;
		if (command & 0x8000)
			continue;
		queue = packet->reply.data[5 + offset];
		if (queue > 15)
			queue = 16;
		if (!(command & 0x80))
			queue = 17;
		pending = s_sysconControl.queueHeads[queue];
		if (!pending) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelPrintfAssertLevel(1, 0,
					SCE_DBGINFO_FUNC(1803, "_sceSysconGpioIntr"),
					caller,
					"[SceSyscon]: unexpected cmd completion 0x%04X\n",
					command);
#endif
			continue;
		}
		expected = sysconPacketCommand(pending);
		if (command == expected) {
			if (!(pending->status & SYSCON_PACKET_COMPLETION)) {
				pending->status |= SYSCON_PACKET_COMPLETION;
				s_sysconControl.pendingCompletions++;
			}
		} else {
			pending->status |= SYSCON_PACKET_WRONG_COMPLETION;
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelPrintfAssertLevel(1, 0,
					SCE_DBGINFO_FUNC(1817, "_sceSysconGpioIntr"),
					caller,
					"[SceSyscon]: invalid cmd completion 0x%04X (should be 0x%04X)\n",
					command, expected);
#endif
		}
	}
#if FW_DEBUG
#undef SYSCON_EXECUTION_WARNING
#endif
}

static void sysconPrependInternalPacket(SceSysconPacket *packet)
{
	if (!s_sysconControl.queueHeads[17])
		s_sysconControl.queueTails[17] = packet;
	packet->next = s_sysconControl.queueHeads[17];
	s_sysconControl.queueHeads[17] = packet;
}

static SceSysconPacket *sysconSelectNextPacket(void)
{
	SceSysconPacket *packet, *ordinary = NULL, *deferred = NULL;
	unsigned int index, queue = s_sysconControl.nextQueue;

	packet = s_sysconControl.queueHeads[17];
	if (packet)
		return packet;
	for (index = 0; index < 17; index++) {
		packet = s_sysconControl.queueHeads[queue];
		if (packet) {
			if (packet->status & SYSCON_PACKET_COMPLETION)
				goto selected;
			if (packet->status & SYSCON_PACKET_STARTED) {
				if (!deferred)
					deferred = packet;
			} else if (!ordinary) {
				ordinary = packet;
			}
		}
		if (++queue == 17)
			queue = 0;
	}
	packet = ordinary;
	if (!packet && deferred &&
	    (s_sysconControl.affirmativeRertyMode || s_sysconControl.polling))
		packet = deferred;
selected:
	if (s_sysconControl.nextQueue < 16)
		s_sysconControl.nextQueue++;
	else
		s_sysconControl.nextQueue = 0;
	return packet;
}

static int _sceSysconGpioIntr(void *subintrArg, void *registerArg, unsigned char intrPriority)
{
	SceSysconPacket *packet, *next;
	volatile SceSpiRegisters *spi;
	uint8_t *response;
	uint32_t state, capacity, first = 0, word, checksum = 0;
	uint32_t requestStatus = 0, requestCompletions = 0;
	unsigned int offset;
	int semaphore = -1, callbackResult = 2, result;
	const void *caller = __builtin_return_address(0);

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_sysconControl.callbackLock);
	packet = s_sysconControl.currentPacket;
	s_sysconControl.interruptActive = 1;
	s_sysconControl.currentPacket = NULL;
	if (!(packet->status & SCE_SYSCON_PACKET_FLAG_KEEP_SELECT))
		ksceGpioPortClear(0, 3);
	s_sysconControl.lastTransferTime = ksceKernelGetSystemTimeLow();
	ksceGpioAcquireIntr(0, 4);
	ksceKernelSpinlockLowUnlock(&s_sysconControl.callbackLock);
	spi = s_sysconControl.spiRegisters;
	if (packet->status & SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX) {
		response = packet->externalRx + packet->received;
		capacity = packet->externalRxSize - packet->received;
	} else {
		response = packet->rx;
		capacity = 32;
	}
	if (!spi->receiveCount) {
		response[2] = 0;
		response[0] = response[1] = 0xff;
		first = 0xffffffff;
		packet->status |= SYSCON_PACKET_NO_RESPONSE;
	}
	if (spi->transmitCount) {
		packet->status |= SYSCON_PACKET_TX_LEFT;
		first = 0xffffffff;
	}
	if (spi->intStatus & SPI_INT_RX_NOT_EMPTY) {
		packet->status |= SYSCON_PACKET_RX_LEFT;
		spi->intStatus = SPI_INT_RX_NOT_EMPTY;
	}
	for (offset = 0; offset + 1 < capacity && spi->receiveCount; offset += 2) {
		word = spi->receive;
		if (!offset)
			first = word & 0xff;
		response[offset] = word;
		response[offset + 1] = word >> 8;
	}
	if (packet->status & SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX)
		*(uint32_t *)packet->rx = *(uint32_t *)response;
	spi->enable = 0;
	dsb();
	ksceGpioPortClear(0, 3);
	sysconLogPacket(1, packet, response);
	if (s_sysconControl.debugHandlers)
		s_sysconControl.debugHandlers->end(packet);
	if (first == 0xffffffff || response[2] < 2 ||
	    capacity < (uint32_t)response[2] + 3)
		goto invalid;
	for (offset = 0; offset < (uint32_t)response[2] + 2; offset++)
		checksum += response[offset];
	if (response[offset] != ((~checksum) & 0xff)) {
invalid:
		packet->status = (packet->status & ~SYSCON_PACKET_TRANSFERRING) |
				 SYSCON_PACKET_BAD_CHECKSUM;
	} else {
		packet->status &= ~SYSCON_PACKET_TRANSFERRING;
		sysconDecodePacketResponse(packet, &requestStatus,
					   &requestCompletions, first);
	}
	ksceKernelSpinlockLowLock(&s_sysconControl.callbackLock);
	if (!(packet->status & (SYSCON_PACKET_BAD_CHECKSUM | SCE_SYSCON_PACKET_FLAG_NO_RETRY)) &&
	    ((packet->reply.result & 0x40) ||
	     (packet->queue != 17 && (packet->reply.result ^ 0x80) < 2)))
		goto queueInternal;
	if (packet->status & SYSCON_PACKET_COMPLETION) {
		packet->status &= ~SYSCON_PACKET_COMPLETION;
		s_sysconControl.pendingCompletions--;
	}
	if (sysconPacketCommand(packet) == 4)
		sysconDecodeCommandCompletions(packet, caller);
	sysconLogPacket(2, packet, packet->rx);
	s_sysconControl.queueHeads[packet->queue] = packet->next;
	if (!packet->next)
		s_sysconControl.queueTails[packet->queue] = NULL;
	packet->next = NULL;
	semaphore = packet->semaId;
	packet->status = (packet->status & ~(SYSCON_PACKET_STARTED | SYSCON_PACKET_DONE |
					     SYSCON_PACKET_QUEUED)) | SYSCON_PACKET_DONE;
	callbackResult = 0;
	if (!packet->callback)
		goto queueInternal;
	ksceKernelSpinlockLowUnlock(&s_sysconControl.callbackLock);
	sysconLogPacket(3, packet, packet->rx);
	callbackResult = packet->callback(packet, packet->argp);
	if (s_sysconControl.logger)
		s_sysconControl.logger(4, 2, (int16_t)sysconPacketCommand(packet),
				      callbackResult);
	ksceKernelSpinlockLowLock(&s_sysconControl.callbackLock);
	if (callbackResult == 1) {
		packet->next = s_sysconControl.queueHeads[packet->queue];
		s_sysconControl.queueHeads[packet->queue] = packet;
		if (!packet->next)
			s_sysconControl.queueTails[packet->queue] = packet;
	} else if (callbackResult == 2) {
		if (s_sysconControl.queueTails[packet->queue])
			s_sysconControl.queueTails[packet->queue]->next = packet;
		s_sysconControl.queueTails[packet->queue] = packet;
		packet->next = NULL;
	} else {
		goto queueInternal;
	}
	semaphore = -1;
	packet->status = (packet->status & ~(SYSCON_PACKET_DONE | SYSCON_PACKET_QUEUED)) |
			 SYSCON_PACKET_QUEUED;
	/* Native callback result 1 returns directly to queue selection. */
	if (callbackResult == 1)
		goto select;
queueInternal:
	if (s_sysconControl.pendingMultiCnInfo == 1) {
		sysconPrependInternalPacket(&s_sysconCommandMultiCnInfo);
		s_sysconControl.pendingMultiCnInfo = 2;
	}
	if (s_sysconControl.pendingCommand130 == 1) {
		sysconPrependInternalPacket(&s_sysconCommand130);
		s_sysconControl.pendingCommand130 = 2;
	}
	if (requestCompletions)
		sysconPrependInternalPacket(&s_sysconCommandCompletion);
	else if (requestStatus)
		sysconPrependInternalPacket(&s_sysconCommandStatus);
select:
	next = sysconSelectNextPacket();
	if (next) {
		sysconWaitTransferDelay(s_sysconControl.polling ?
					s_sysconControl.commandPacketDelay : 4);
		s_sysconControl.currentPacket = next;
		sysconStartPacket(next);
	}
	if (!callbackResult && semaphore >= 0) {
		result = ksceKernelSignalSema(semaphore, 1);
		(void)result;
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, result >= 0,
				SCE_DBGINFO_FUNC(2101, "_sceSysconGpioIntr"), caller);
#endif
	}
	s_sysconControl.interruptActive = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sysconControl.callbackLock, state);
	return -1;
}

int ksceSysconGetWakeupFactor(uint32_t *value)
{
	return sysconReadWord(value, SYSCON_CMD_GET_WAKEUP_FACTOR);
}

int SceSysconForDriver_93075DD1(uint32_t *value)
{
	return sysconReadWord(value, 0x14);
}

int ksceSysconGetManufacturesStatus(uint32_t *value)
{
	return sysconReadWord(value, SYSCON_CMD_GET_MANUFACTURES_STATUS);
}

int ksceSysconGetClock(uint32_t *value)
{
	return sysconReadWord(value, SYSCON_CMD_GET_CLOCK);
}

int SceSysconForDriver_373ECF8A(uint32_t value)
{
	return sysconWriteWord(value, 0x83, 3);
}

int SceSysconForDriver_A2FE9BF9(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x800);
}

int ksceSysconGetUsbDetStatus(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_USB_DET_STATUS);
}

int SceSysconForDriver_E7F5D3DC(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x806);
}

int ksceSysconGetManualChargeMode(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_MANUAL_CHARGE_MODE);
}

int SceSysconForDriver_3C3B949C(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x841);
}

int SceSysconForDriver_1C29C00E(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x842);
}

int SceSysconForDriver_730E4725(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x843);
}

int ksceSysconGetElmoVersion(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_ELMO_VERSION);
}

int ksceSysconGetCookieVersion(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_COOKIE_VERSION);
}

int ksceSysconCtrlHdmiCecPower(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_HDMI_CEC_POWER, 2);
}

int SceSysconForDriver_063425AE(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_MOTION_SENSOR_POWER, 2);
}

int ksceSysconCtrlAccPower(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_ACC_POWER, 2);
}

int ksceSysconCtrlWirelessPower2(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_WIRELESS_POWER, 2);
}

int SceSysconForDriver_5A614349(uint32_t value)
{
	return sysconWriteWord(value, 0x88b, 2);
}

int SceSysconForDriver_B872E904(uint32_t value)
{
	return sysconWriteWord(value, 0x88c, 2);
}

int SceSysconForDriver_DD16ABD9(uint32_t value)
{
	return sysconWriteWord(value, 0x88d, 2);
}

int SceSysconForDriver_285594F8(uint32_t value)
{
	return sysconWriteWord(value, 0x890, 2);
}

int ksceSysconCtrlChargeAC(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_CHARGE_AC, 2);
}

int ksceSysconCtrlUsbStatus(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_USB_STATUS, 3);
}

int ksceSysconCtrlDevKitUsbPower(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_DEV_KIT_USB_POWER, 2);
}

int ksceSysconEnableHibernateIO(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_ENABLE_HIBERNATE_IO, 1);
}

int ksceSysconCtrlManualChargeMode(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_MANUAL_CHARGE_MODE, 2);
}

int ksceSysconCtrlDolceUsbPower(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_DOLCE_USB_POWER, 2);
}

int SceSysconForDriver_DFB024C4(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_READ_COOKIE_STATUS);
}

int SceSysconForDriver_87FF8041(uint32_t value)
{
	return sysconWriteWord(value, 0x8c7, 2);
}

int ksceSysconCtrlUSBSupply(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_USB_SUPPLY, 2);
}

int SceSysconForDriver_451C1662(uint32_t value)
{
	return sysconWriteWord(value, 0x8c9, 2);
}

int ksceSysconClearTemperatureLog(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CLEAR_TEMPERATURE_LOG, 2);
}

int SceSysconForDriver_F87679EE(uint32_t value)
{
	return sysconWriteWord(value, 0x8d0, 2);
}

int ksceSysconGetBatteryLifePercent(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_BATTERY_LIFE_PERCENT);
}

int ksceSysconGetCurrentFullCap(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_CURRENT_FULL_CAP);
}

int SceSysconForDriver_E1885F68(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_ABBY_TEMP);
}

int ksceSysconGetBatteryCycleCount(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_GET_BATTERY_CYCLE_COUNT);
}

/* the secure monitor's power call (MONCALL_SYSCON_SET_POWER_MODE): mode, argument */
static __attribute__((noinline)) int sysconPowerMonitorCall(uint32_t mode, uint32_t argument)
{
	return moncall2(MONCALL_SYSCON_SET_POWER_MODE, mode, argument);
}

int ksceSysconSetPowerMode(uint32_t mode, uint32_t argument)
{
	return sysconPowerMonitorCall(mode, argument);
}

int ksceSysconErnieHibernate(void)
{
	return sysconPowerMonitorCall(5, 0);
}

int SceSysconForDriver_14B99945(void)
{
	return sysconPowerMonitorCall(2, 0);
}

int SceSysconForDriver_0D0B6D25(void)
{
	SceSysconPacket packet;

	sysconPacketSetCommand(&packet, SYSCON_CMD_NOP);
	packet.request.length = 1;
	return ksceSysconCmdExec(&packet, 0);
}

int ksceSysconReadCommand(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	if (size - 1 >= 24 || offset + size >= 0x101)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_READ_SCRATCH_PAD);
	packet.request.length = 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	memcpy(data, packet.reply.data, size);
	return 0;
}

int ksceSysconSendCommand(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	if (size - 1 >= 24 || offset + size >= 0x101)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_WRITE_SCRATCH_PAD);
	packet.request.length = size + 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_BF5CDB61(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	if (size - 1 >= 24 || offset + size >= 0x81)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.command[0] = 0x92;
	packet.request.command[1] = 0;
	packet.request.length = 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	memcpy(data, packet.reply.data, size);
	return 0;
}

int SceSysconForDriver_4E0EB57B(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	if (size - 1 >= 24 || offset + size >= 0x81)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.command[0] = 0x93;
	packet.request.command[1] = 0;
	packet.request.length = size + 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int ksceSysconGetMicroUsbInfo(uint32_t *information)
{
	if (information)
		*information = s_sysconControl.command130Info;
	return 0;
}

int ksceSysconCtrlDolceLED(uint32_t mode)
{
	if (mode >= 3)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	return sysconWriteWord(0, SYSCON_CMD_CTRL_DOLCE_LED + mode, 1);
}

int ksceSysconCtrlSdPower(uint32_t enabled)
{
	SceSysconForDriver_0D0B6D25();
	if (ksceSysconIsDownLoaderMode())
		return 0;
	return sysconWriteWord(enabled, SYSCON_CMD_CTRL_SD_POWER, 2);
}

int ksceSysconCtrlVoltage(uint8_t rail, uint32_t code)
{
	SceSysconPacket packet;
	uint32_t hardware;
	int result;

	hardware = ksceSysconGetHardwareInfo() & 0xff0000;
	if (code < 0x40) {
		if (hardware > 0x5fffff && hardware != 0x700000 &&
		    ((hardware < 0x800000 && hardware != 0x720000) || code < 0x3c))
			return SCE_SYSCON_ERROR_INVALID_SIZE;
	} else if (hardware < 0x600000 || hardware == 0x700000) {
		if (code - 0x4d > 0x3f)
			return SCE_SYSCON_ERROR_INVALID_SIZE;
		code = 0x8c - code;
	}
	sysconPacketSetCommand(&packet, SYSCON_CMD_CTRL_VOLTAGE);
	packet.request.length = 3;
	packet.request.data[0] = rail;
	packet.request.data[1] = code;
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int ksceSysconCtrlDeviceReset(uint32_t device, uint32_t mode)
{
	return sysconWriteWord(mode | device << 8, SYSCON_CMD_CTRL_DEVICE_RESET, 3);
}

int ksceSysconCtrlLED(uint32_t led, int enabled)
{
	return sysconWriteWord(led | (enabled ? 0x80 : 0), SYSCON_CMD_CTRL_LED, 2);
}

int ksceSysconCtrlChargeVBUS(uint32_t control, uint32_t enabled)
{
	return sysconWriteWord(enabled | control << 8, SYSCON_CMD_CTRL_CHARGE_VBUS, 3);
}

int ksceSysconCtrlRMRPower(uint32_t enabled)
{
	SceSysconForDriver_0D0B6D25();
	if (ksceSysconIsDownLoaderMode())
		return 0;
	return sysconWriteWord(enabled, SYSCON_CMD_CTRL_RMR_POWER, 2);
}

int ksceSysconCtrlLedPwmBlink(uint32_t led, uint32_t level)
{
	if (led != 0x40)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	return sysconWriteWord(level << 8 | 1, SYSCON_CMD_CTRL_LED_PWM_BLINK, 3);
}

int SceSysconForDriver_9CA6EB70(uint32_t enabled)
{
	if ((ksceSysconGetHardwareInfo() & 0xff0000) < 0x400000)
		return 0;
	return sysconWriteWord(enabled, SYSCON_CMD_SET_CHARGE_LED_CTRL, 2);
}

int ksceSysconGetBatteryRemainCap(uint32_t *capacity)
{
	uint32_t version = (uint32_t)ksceSysconGetBaryonVersion();

	*capacity = 0;
	return sysconReadWord(capacity,
			      version > 0x70503 ? 0x900 : 0x802);
}

int ksceSysconGetBatteryVolt(uint32_t *voltage)
{
	uint32_t version = (uint32_t)ksceSysconGetBaryonVersion();

	*voltage = 0;
	return sysconReadWord(voltage,
			      version > 0x70503 ? 0x901 : 0x803);
}

int ksceSysconGetBatteryRemainLevel(uint32_t *maximum, uint32_t *level)
{
	uint32_t value = 0;
	int result;

	result = sysconReadWord(&value, SYSCON_CMD_GET_BATTERY_REMAIN_LEVEL);
	if (result >= 0) {
		if (maximum)
			*maximum = value & 0xff;
		if (level)
			*level = (value >> 8) & 0xff;
	}
	return result;
}

int ksceSysconGetBatteryTemp(uint32_t *temperature)
{
	uint32_t version = (uint32_t)ksceSysconGetBaryonVersion();

	*temperature = 0;
	return sysconReadWord(temperature,
			      version > 0x70503 ? 0x981 : 0x8a0);
}

int ksceSysconGetBatteryFullCap(uint32_t *value)
{
	uint32_t version = (uint32_t)ksceSysconGetBaryonVersion();

	*value = 0;
	return sysconReadWord(value,
			      version > 0x70503 ? 0x982 : 0x8a1);
}

int ksceSysconGetBatteryElec(int32_t *current)
{
	uint32_t value;
	int result;

	result = sysconReadWord(&value,
			       (uint32_t)ksceSysconGetBaryonVersion() > 0x70503 ? 0x983 : 0x8a2);
	if (result >= 0) {
		*current = (int16_t)value;
		return 0;
	}
	return result;
}

int ksceSysconGetBatteryTTE(uint32_t *time)
{
	uint32_t version = (uint32_t)ksceSysconGetBaryonVersion();

	*time = 0;
	return sysconReadWord(time,
			      version > 0x70503 ? 0x984 : 0x8a3);
}

int ksceSysconGetBatterySOH(uint32_t *health, uint32_t *status)
{
	uint32_t value;
	int result;

	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		if (status)
			*status = 0;
		*health = 0;
		return sysconReadWord(health, SYSCON_CMD_GET_BATTERY_SOH_OLD);
	}
	result = sysconReadWord(&value, SYSCON_CMD_GET_BATTERY_SOH);
	if (result >= 0) {
		if (health)
			*health = value & 0xff;
		if (status)
			*status = (value >> 8) & 0xff;
	}
	return result;
}

int ksceSysconSetClock(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_SET_CLOCK, 5);
}

int ksceSysconOutputClock(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_OUTPUT_CLOCK, 2);
}

int ksceSysconCtrlHostOutputViaDongle(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_CTRL_HOST_OUTPUT_VIA_DONGLE, 2);
}

int SceSysconForDriver_33B5CDB3(uint32_t *value)
{
	return sysconReadWord(value, 0xb3);
}

int SceSysconForDriver_F6D4DDC4(uint32_t *value)
{
	return sysconReadWord(value, 0xb4);
}

int SceSysconForDriver_00AE3AEB(uint32_t value)
{
	return sysconWriteWord(value, 0xb5, 2);
}

int SceSysconForDriver_0D300158(uint32_t value)
{
	return sysconWriteWord(value, 0xb6, 2);
}

int SceSysconForDriver_91EF4EC3(uint32_t *value)
{
	return sysconReadWord(value, 0xb7);
}

int ksceSysconErnieShutdown(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_ERNIE_SHUTDOWN, 2);
}

int SceSysconForDriver_E7893732(uint32_t value)
{
	return sysconWriteWord(value, 0x1b0, 3);
}

int SceSysconForDriver_CFCEE733(uint32_t value)
{
	return sysconWriteWord(value, 0x181, 2);
}

int SceSysconForDriver_F99BC858(uint32_t *value)
{
	return sysconReadWord(value, 0x131);
}

int SceSysconForDriver_010F95D9(uint32_t value)
{
	return sysconWriteWord(value, 0x3a9, 1);
}

int SceSysconForDriver_42E599AC(uint32_t value)
{
	return sysconWriteWord(value, 0x3b9, 1);
}

int SceSysconForDriver_3664E2C0(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x3ac);
}

int SceSysconForDriver_2E6D97CD(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, 0x3bc);
}

int SceSysconForDriver_B8F4F4E3(uint32_t value)
{
	return sysconWriteWord(value, 0x3af, 2);
}

int SceSysconForDriver_240A604E(uint32_t value)
{
	return sysconWriteWord(value, 0x3bf, 2);
}

int SceSysconForDriver_2659535C(void)
{
	return sysconWriteWord(0, 0x84, 1);
}

int SceSysconForDriver_4295D497(void)
{
	return sysconWriteWord(0, 0x85, 1);
}

int ksceSysconLogStart(void)
{
	return sysconWriteWord(0, SYSCON_CMD_LOG_START, 1);
}

int ksceSysconLogStartWaiting(void)
{
	return sysconWriteWord(0, SYSCON_CMD_LOG_START_WAITING, 1);
}

int SceSysconForDriver_48ED8981(void)
{
	return sysconWriteWord(0, 0x392, 2);
}

int SceSysconForDriver_060E55C1(void)
{
	return sysconWriteWord(1, 0x392, 2);
}

int SceSysconForDriver_9A28BEEF(void)
{
	return sysconWriteWord(0, 0x393, 2);
}

int SceSysconForDriver_8874EF45(void)
{
	return sysconWriteWord(1, 0x393, 2);
}

int SceSysconForDriver_5946B29B(void)
{
	return sysconWriteWord(0, 0x3c2, 2);
}

int SceSysconForDriver_10327C64(void)
{
	return sysconWriteWord(1, 0x3c2, 2);
}

int ksceSysconSetMultiCnPort(uint32_t port)
{
	return sysconWriteWord(port, SYSCON_CMD_SET_MULTI_CN_PORT, 3);
}

int ksceSysconGetControlsInfo(uint32_t *information)
{
	uint32_t value;
	int result;

	result = sysconReadWord(&value, SYSCON_CMD_GET_CONTROLS_INFO);
	if (result >= 0)
		*information = value;
	return result;
}

int SceSysconForDriver_76272CB9(uint32_t *information)
{
	uint32_t value;
	int result;

	result = sysconReadWord(&value, 0x120);
	if (result >= 0)
		*information = value;
	return result;
}

int ksceSysconGetLogInfo(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_LOG_INFO);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	return 0;
}

int SceSysconForDriver_B7E294AF(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x82;
	packet.request.command[1] = 0x1;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	information[2] = packet.reply.word[2];
	information[3] = packet.reply.word[3];
	return 0;
}

int SceSysconForDriver_FDB3AE9D(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x83;
	packet.request.command[1] = 0x1;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	information[2] = packet.reply.word[2];
	information[3] = packet.reply.word[3];
	return 0;
}

int SceSysconForDriver_C314B64B(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x85;
	packet.request.command[1] = 0x1;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	information[2] = packet.reply.word[2];
	return 0;
}

int SceSysconForDriver_3B57B777(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa8;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	return 0;
}

int SceSysconForDriver_08A561C7(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb8;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	return 0;
}

int SceSysconForDriver_BC1D93D7(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa0;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_8B0B4A97(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb0;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_567865A3(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa1;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_AE127A96(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb1;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_41942B50(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa4;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_2A61ACE1(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb4;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_998ECD6D(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa5;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_CEE2B8F7(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb5;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_9B6B8BB9(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa7;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0] | (uint32_t)packet.reply.data[1] << 8;
	return result;
}

int SceSysconForDriver_C4A61241(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb7;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0 && value)
		*value = packet.reply.data[0] | (uint32_t)packet.reply.data[1] << 8;
	return result;
}

int SceSysconForDriver_0396BA09(void)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa6;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	return result;
}

int SceSysconForDriver_23558805(void)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb6;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	return result;
}

int SceSysconForDriver_357CC9D9(uint32_t first, uint32_t second)
{
	return sysconWriteWord((uint8_t)first | second << 8, 0x3ab, 3);
}

int SceSysconForDriver_CCA56A16(uint32_t first, uint32_t second)
{
	return sysconWriteWord((uint8_t)first | second << 8, 0x3bb, 3);
}

int SceSysconForDriver_B3960346(uint8_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xad;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_830A9347(uint8_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xbd;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_93FE51F5(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xae;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint32_t)packet.reply.data[1] << 8 |
		 (uint32_t)packet.reply.data[2] << 16;
	return 0;
}

int SceSysconForDriver_6A093056(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xbe;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint32_t)packet.reply.data[1] << 8 |
		 (uint32_t)packet.reply.data[2] << 16;
	return 0;
}

int SceSysconForDriver_9F9156BE(const void *request, void *reply)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xaa;
	packet.request.command[1] = 0x3;
	packet.request.length = 7;
	*(uint32_t *)&packet.request.data[0] = ((const uint32_t *)request)[0];
	*(uint16_t *)&packet.request.data[4] = ((const uint16_t *)request)[2];
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (reply) {
		((uint32_t *)reply)[0] = packet.reply.word[0];
		((uint16_t *)reply)[2] = *(uint16_t *)&packet.reply.data[4];
		((uint8_t *)reply)[6] = packet.reply.data[6];
	}
	return 0;
}

int SceSysconForDriver_F648BE61(const void *request, void *reply)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xba;
	packet.request.command[1] = 0x3;
	packet.request.length = 7;
	*(uint32_t *)&packet.request.data[0] = ((const uint32_t *)request)[0];
	*(uint16_t *)&packet.request.data[4] = ((const uint16_t *)request)[2];
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (reply) {
		((uint32_t *)reply)[0] = packet.reply.word[0];
		((uint16_t *)reply)[2] = *(uint16_t *)&packet.reply.data[4];
		((uint8_t *)reply)[6] = packet.reply.data[6];
	}
	return 0;
}

int SceSysconForDriver_B9835A63(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa2;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (size > 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 6;
	packet.request.data[0] = first;
	packet.request.data[1] = first >> 8;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	memcpy(&packet.request.data[5], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0)
		*replyLength = packet.reply.data[2];
	return result;
}

int SceSysconForDriver_CD6A6BC7(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb2;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	if (size > 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 6;
	packet.request.data[0] = first;
	packet.request.data[1] = first >> 8;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	memcpy(&packet.request.data[5], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0)
		*replyLength = packet.reply.data[2];
	return result;
}

int SceSysconForDriver_B63AF451(uint32_t offset, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xa3;
	packet.request.command[1] = 0x3;
	packet.request.length = 3;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0)
		*replyLength = packet.reply.data[2];
	return result;
}

int SceSysconForDriver_8EBA8A26(uint32_t offset, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xb3;
	packet.request.command[1] = 0x3;
	packet.request.length = 3;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0)
		*replyLength = packet.reply.data[2];
	return result;
}

int SceSysconForDriver_D9B11C0A(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x9d;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 0;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_FB37D7AD(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x9d;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_A1F1B973(void)
{
	uint32_t value = 0;
	int result;

	result = sysconReadWord(&value, 0x38a);
	return result < 0 ? result : (int)value;
}

int SceSysconForDriver_BA09F171(uint32_t *first, uint32_t *second)
{
	uint32_t value;
	int result;

	result = sysconReadWord(&value, 0x13);
	if (result < 0)
		return result;
	if (first)
		*first = (uint8_t)value;
	if (second)
		*second = (uint8_t)(value >> 8);
	return 0;
}

int SceSysconForDriver_51164951(uint32_t value, uint32_t extension)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x82;
	packet.request.command[1] = 0x0;
	packet.request.length = 6;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x90907)
		return sysconWriteWord(value, 0x82, 5);
	packet.request.data[0] = value;
	packet.request.data[1] = value >> 8;
	packet.request.data[2] = value >> 16;
	packet.request.data[3] = value >> 24;
	packet.request.data[4] = extension;
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_3168F3AF(uint32_t *value, uint32_t *extension)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x12;
	packet.request.command[1] = 0x0;
	packet.request.length = 1;
	if (!value)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.word[0];
	if (extension)
		*extension = packet.reply.length < 7 ? 0 : packet.reply.data[4];
	return 0;
}

int SceSysconForDriver_8A97E690(const uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x84;
	packet.request.command[1] = 0x1;
	packet.request.length = 17;
	__builtin_memcpy(packet.request.data, information, 16);
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int ksceSysconLogReadData(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_LOG_READ_DATA);
	packet.request.length = 3;
	if (size - 1 >= 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (size < packet.reply.data[2])
		return SCE_SYSCON_ERROR_ILLEGAL_SIZE;
	memcpy(data, &packet.reply.data[3], packet.reply.data[2]);
	return packet.reply.data[2];
}

int ksceSysconGetTouchpanelDeviceInfo(uint16_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_TOUCHPANEL_DEVICE_INFO);
	packet.request.length = 1;
	if (!information)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = (uint16_t)packet.reply.data[0] << 8 | packet.reply.data[1];
	information[1] = (uint16_t)packet.reply.data[2] << 8 | packet.reply.data[3];
	information[2] = (uint16_t)packet.reply.data[4] << 8 | packet.reply.data[5];
	information[3] = (uint16_t)packet.reply.data[6] << 8 | packet.reply.data[7];
	return 0;
}

int SceSysconForDriver_030D447F(SceKernelTouchpanelDeviceInfo2 *info)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_TOUCHPANEL_DEVICE_INFO2);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	info->front.vendorID = (uint16_t)packet.reply.data[0] << 8 | packet.reply.data[1];
	info->front.firmwareRev = (uint16_t)packet.reply.data[2] << 8 | packet.reply.data[3];
	info->front.configRev = (uint16_t)packet.reply.data[4] << 8 | packet.reply.data[5];
	info->front.hwVersion = packet.reply.data[6];
	info->front.vendorInfo = packet.reply.data[7];
	info->front.reserved[0] = packet.reply.data[8];
	info->front.reserved[1] = packet.reply.data[9];
	info->rear.vendorID = (uint16_t)packet.reply.data[10] << 8 | packet.reply.data[11];
	info->rear.firmwareRev = (uint16_t)packet.reply.data[12] << 8 | packet.reply.data[13];
	info->rear.configRev = (uint16_t)packet.reply.data[14] << 8 | packet.reply.data[15];
	info->rear.hwVersion = packet.reply.data[16];
	info->rear.vendorInfo = packet.reply.data[17];
	info->rear.reserved[0] = packet.reply.data[18];
	info->rear.reserved[1] = packet.reply.data[19];
	return 0;
}

int SceSysconForDriver_856DFC0E(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x83;
	packet.request.command[1] = 0x3;
	packet.request.length = 14;
	if (size - 1 >= 10)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	packet.request.data[3] = packet.request.data[4] = 0;
	packet.request.data[5] = packet.request.data[6] = 0;
	packet.request.data[7] = packet.request.data[8] = 0;
	packet.request.data[9] = packet.request.data[10] = 0;
	packet.request.data[11] = packet.request.data[12] = 0;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_E38045F5(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x85;
	packet.request.command[1] = 0x3;
	packet.request.length = 14;
	if (size - 1 >= 10)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	packet.request.data[3] = packet.request.data[4] = 0;
	packet.request.data[5] = packet.request.data[6] = 0;
	packet.request.data[7] = packet.request.data[8] = 0;
	packet.request.data[9] = packet.request.data[10] = 0;
	packet.request.data[11] = packet.request.data[12] = 0;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_1C5D466C(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x84;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	if (size - 1 >= 10)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (packet.reply.data[0])
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memcpy(data, &packet.reply.data[1], size);
	return 0;
}

int SceSysconForDriver_6D408E30(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x86;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	if (size - 1 >= 10)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (packet.reply.data[0])
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memcpy(data, &packet.reply.data[1], size);
	return 0;
}

int SceSysconForDriver_1546A141(int first, int second)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x87;
	packet.request.command[1] = 0x3;
	packet.request.length = 9;
	packet.request.data[0] = first >= 0;
	packet.request.data[1] = first < 0 ? 0 : first;
	packet.request.data[2] = 0;
	packet.request.data[3] = 0;
	packet.request.data[4] = second >= 0;
	packet.request.data[5] = second < 0 ? 0 : second;
	packet.request.data[6] = 0;
	packet.request.data[7] = 0;
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int ksceSysconGetHardwareInfo2(uint32_t *information)
{
	if (!(s_sysconControl.hardwareInfo & 0x20))
		return SCE_SYSCON_ERROR_INVALID_POINTER;
	information[0] = s_sysconControl.hardwareFlags[0];
	information[1] = s_sysconControl.hardwareFlags[1];
	information[2] = s_sysconControl.hardwareFlags[2];
	information[3] = s_sysconControl.hardwareFlags[3];
	return 0;
}

int SceSysconForDriver_769F9AC4(void)
{
	int state;

	if (ksceSblAimgrIsDolce())
		return 1;
	state = s_sysconControl.notificationState[SYSCON_STATE_N0_BIT6];
	return state < 0 ? 0 : state;
}

int ksceSysconGetMultiCnInfo(uint32_t *information)
{
	uint32_t value = s_sysconControl.multiCnInfo;
	uint32_t revision;

	if (((uint32_t)ksceSysconGetHardwareInfo() & 0xff0000) < 0x400000) {
		switch ((value >> 8) & 0xff) {
		case 0x65:
			revision = 4;
			break;
		case 0x55:
			revision = 5;
			break;
		case 0x51:
			revision = 2;
			break;
		case 0x60:
			revision = 0;
			break;
		default:
			revision = 0xff;
			break;
		}
		value = (value & ~0xff00U) | revision << 8;
	}
	if (information) {
		if (ksceSblAimgrIsGenuineDolce())
			value &= ~0xff00U;
		*information = value;
	}
	return 0;
}

int ksceSysconMotionGetDeviceInfo(uint32_t *value)
{
	*value = 0;
	return sysconReadWord(value, SYSCON_CMD_MOTION_GET_DEVICE_INFO);
}

int SceSysconForDriver_4DFA12FB(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc3;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	uint8_t transmit[128];

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(transmit, 0, sizeof(transmit));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX;
	packet.externalTx = transmit;
	packet.request.length = size + 7;
	packet.request.data[0] = 0;
	packet.request.data[1] = first;
	packet.request.data[2] = first >> 8;
	packet.request.data[3] = second;
	packet.request.data[4] = second >> 8;
	packet.request.data[5] = size;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[3] = packet.request.data[0];
	transmit[4] = packet.request.data[1];
	transmit[5] = packet.request.data[2];
	transmit[6] = packet.request.data[3];
	transmit[7] = packet.request.data[4];
	transmit[8] = packet.request.data[5];
	memcpy(transmit + 9, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	if (result >= 0 && replyLength)
		*replyLength = packet.reply.data[2];
	return result;
}

int SceSysconForDriver_3B06F619(uint32_t first, uint32_t second, const void *data,
			      uint32_t size, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc3;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	uint8_t transmit[128];

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(transmit, 0, sizeof(transmit));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX;
	packet.externalTx = transmit;
	packet.request.length = size + 7;
	packet.request.data[0] = 1;
	packet.request.data[1] = first;
	packet.request.data[2] = first >> 8;
	packet.request.data[3] = second;
	packet.request.data[4] = second >> 8;
	packet.request.data[5] = size;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[3] = packet.request.data[0];
	transmit[4] = packet.request.data[1];
	transmit[5] = packet.request.data[2];
	transmit[6] = packet.request.data[3];
	transmit[7] = packet.request.data[4];
	transmit[8] = packet.request.data[5];
	memcpy(transmit + 9, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	if (result >= 0 && replyLength)
		*replyLength = packet.reply.data[2];
	return result;
}

/* name unknown: command 0x3ca, read reply data (up to 32 bytes) */
int SceSysconForDriver_CA203A5F(uint32_t first, uint32_t second, uint32_t size,
			      void *reply, uint32_t *replyLength)
{
	SceSysconPacket packet;
	uint8_t receive[128];
	int result;

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(receive, 0, sizeof(receive));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX;
	packet.externalRx = receive;
	packet.externalRxSize = sizeof(receive);
	packet.request.command[0] = 0xca;
	packet.request.command[1] = 0x3;
	packet.request.length = 7;
	packet.request.data[0] = 0;
	packet.request.data[1] = first;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	packet.request.data[5] = 0;
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX);
	if (result < 0)
		return result;
	if (replyLength)
		*replyLength = packet.reply.data[0];
	if (reply)
		memcpy(reply, receive, packet.reply.data[0]);
	return result;
}

/* name unknown: as SceSysconForDriver_CA203A5F with tx[3] = 1 */
int SceSysconForDriver_55B6FEF5(uint32_t first, uint32_t second, uint32_t size,
			      void *reply, uint32_t *replyLength)
{
	SceSysconPacket packet;
	uint8_t receive[128];
	int result;

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(receive, 0, sizeof(receive));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX;
	packet.externalRx = receive;
	packet.externalRxSize = sizeof(receive);
	packet.request.command[0] = 0xca;
	packet.request.command[1] = 0x3;
	packet.request.length = 7;
	packet.request.data[0] = 1;
	packet.request.data[1] = first;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	packet.request.data[5] = 0;
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX);
	if (result < 0)
		return result;
	if (replyLength)
		*replyLength = packet.reply.data[0];
	if (reply)
		memcpy(reply, receive, packet.reply.data[0]);
	return result;
}

int SceSysconForDriver_94418CB4(uint32_t first, uint32_t second, uint32_t size,
			      const void *data, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xcb;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	uint8_t transmit[128];

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(transmit, 0, sizeof(transmit));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX;
	packet.externalTx = transmit;
	packet.request.length = size + 6;
	packet.request.data[0] = 0;
	packet.request.data[1] = first;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[3] = packet.request.data[0];
	transmit[4] = packet.request.data[1];
	transmit[5] = packet.request.data[2];
	transmit[6] = packet.request.data[3];
	transmit[7] = packet.request.data[4];
	memcpy(transmit + 8, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	if (result >= 0 && replyLength)
		*replyLength = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_7EC67149(uint32_t first, uint32_t second, uint32_t size,
			      const void *data, uint32_t *replyLength)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xcb;
	packet.request.command[1] = 0x3;
	packet.request.length = 1;
	uint8_t transmit[128];

	if (size > 32)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(transmit, 0, sizeof(transmit));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX;
	packet.externalTx = transmit;
	packet.request.length = size + 6;
	packet.request.data[0] = 1;
	packet.request.data[1] = first;
	packet.request.data[2] = second;
	packet.request.data[3] = second >> 8;
	packet.request.data[4] = size;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[3] = packet.request.data[0];
	transmit[4] = packet.request.data[1];
	transmit[5] = packet.request.data[2];
	transmit[6] = packet.request.data[3];
	transmit[7] = packet.request.data[4];
	memcpy(transmit + 8, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	if (result >= 0 && replyLength)
		*replyLength = packet.reply.data[0];
	return result;
}

int SceSysconForDriver_68366D29(uint32_t index, uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc4;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	packet.request.data[0] = 0;
	packet.request.data[1] = index;
	packet.request.data[2] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[2];
	return 0;
}

int SceSysconForDriver_2BCE7F5A(uint32_t index, uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc4;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	packet.request.data[0] = 1;
	packet.request.data[1] = index;
	packet.request.data[2] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[2];
	return 0;
}

int SceSysconForDriver_6450D6BE(uint32_t index, uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc8;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	packet.request.data[0] = 0;
	packet.request.data[1] = index;
	packet.request.data[2] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[2];
	return 0;
}

int SceSysconForDriver_16A466E7(uint32_t index, uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc8;
	packet.request.command[1] = 0x3;
	packet.request.length = 4;
	packet.request.data[0] = 1;
	packet.request.data[1] = index;
	packet.request.data[2] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[2];
	return 0;
}

int SceSysconForDriver_F162D3D6(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc6;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 0;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_25144105(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc6;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_9C840439(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc7;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 0;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_D632D197(uint32_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc7;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (value)
		*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_9A4304F2(uint8_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc5;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 0;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (information) {
		information[0] = packet.reply.data[0];
		information[1] = packet.reply.data[1];
		information[2] = packet.reply.data[3];
		information[3] = packet.reply.data[4];
		information[4] = packet.reply.data[5];
		information[5] = packet.reply.data[6];
		information[6] = packet.reply.data[7];
		information[7] = packet.reply.data[8];
	}
	return 0;
}

int SceSysconForDriver_C2955814(uint8_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc5;
	packet.request.command[1] = 0x3;
	packet.request.length = 2;
	packet.request.data[0] = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	if (information) {
		information[0] = packet.reply.data[0];
		information[1] = packet.reply.data[1];
		information[2] = packet.reply.data[3];
		information[3] = packet.reply.data[4];
		information[4] = packet.reply.data[5];
		information[5] = packet.reply.data[6];
		information[6] = packet.reply.data[7];
		information[7] = packet.reply.data[8];
	}
	return 0;
}

int SceSysconForDriver_9962CE6F(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc9;
	packet.request.command[1] = 0x3;
	packet.request.length = 3;
	packet.request.data[0] = 0;
	packet.request.data[1] = index;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0) {
		if (status)
			*status = packet.reply.data[0];
		if (first)
			*first = packet.reply.data[1] | (uint16_t)packet.reply.data[2] << 8;
		if (second)
			*second = packet.reply.data[3] | (uint16_t)packet.reply.data[4] << 8;
	}
	return result;
}

int SceSysconForDriver_31B8F7BE(uint32_t index, uint32_t *status, uint16_t *first, uint16_t *second)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xc9;
	packet.request.command[1] = 0x3;
	packet.request.length = 3;
	packet.request.data[0] = 1;
	packet.request.data[1] = index;
	result = ksceSysconCmdExec(&packet, 0);
	if (result >= 0) {
		if (status)
			*status = packet.reply.data[0];
		if (first)
			*first = packet.reply.data[1] | (uint16_t)packet.reply.data[2] << 8;
		if (second)
			*second = packet.reply.data[3] | (uint16_t)packet.reply.data[4] << 8;
	}
	return result;
}

int SceSysconForDriver_9A7858B6(uint32_t control, uint32_t value)
{
	return sysconWriteWord((control & 1) | value << 8, 0x481, 3);
}

int ksceSysconMotionGetMeasureData(void *reply, uint32_t capacity)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_MOTION_GET_MEASURE_DATA);
	packet.request.length = 1;
	packet.externalRx = reply;
	packet.externalRxSize = capacity;
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX);
	return result < 0 ? result : (int)packet.received;
}

int SceSysconForDriver_253CC522(uint8_t *value)
{
	uint32_t response = 0;
	int result;

	result = sysconReadWord(&response, 0x807);
	if (result < 0)
		return result;
	value[0] = response;
	value[1] = response >> 8;
	return 0;
}

int ksceSysconGetPowerStatus(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_POWER_STATUS);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	return 0;
}

int ksceSysconGetTemperatureLog(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_TEMPERATURE_LOG);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	information[2] = packet.reply.word[2];
	information[3] = packet.reply.word[3];
	return 0;
}

int SceSysconForDriver_3FDD29D6(uint32_t mask, int enabled)
{
	int result;

	result = sysconWriteWord(enabled ? mask | 0x80 : mask, 0x884, 2);
	if (result >= 0) {
		if (enabled)
			s_sysconControl.command884State |= mask;
		else
			s_sysconControl.command884State &= ~mask;
	}
	return result;
}

int SceSysconForDriver_79E6DD8B(uint32_t mask, int enabled)
{
	int result;

	result = sysconWriteWord(enabled ? mask | 0x80 : mask, 0x885, 2);
	if (result >= 0) {
		if (enabled)
			s_sysconControl.command885State |= mask;
		else
			s_sysconControl.command885State &= ~mask;
	}
	return result;
}

int ksceSysconReadElmoReg(uint32_t index, uint16_t *value)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_READ_ELMO_REG);
	packet.request.length = 2;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504)
		*value = packet.reply.data[1] | (uint16_t)packet.reply.data[0] << 8;
	return 0;
}

int ksceSysconWriteElmoReg(uint32_t index, uint32_t value)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_WRITE_ELMO_REG);
	packet.request.length = 4;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.data[1] = value >> 8;
		packet.request.data[2] = value;
	} else {
		packet.request.data[1] = value;
		packet.request.data[2] = value >> 8;
	}
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int ksceSysconReadCookieReg(uint32_t index, uint16_t *value)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_READ_COOKIE_REG);
	packet.request.length = 2;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504)
		*value = packet.reply.data[1] | (uint16_t)packet.reply.data[0] << 8;
	return 0;
}

int ksceSysconWriteCookieReg(uint32_t index, uint32_t value)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_WRITE_COOKIE_REG);
	packet.request.length = 4;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.data[1] = value >> 8;
		packet.request.data[2] = value;
	} else {
		packet.request.data[1] = value;
		packet.request.data[2] = value >> 8;
	}
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_5CDDA14D(uint32_t value)
{
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x80001)
		return 0;
	return sysconWriteWord(value, 0x899, 2);
}

int SceSysconForDriver_CB41B531(uint32_t first, uint32_t second, uint32_t third, uint32_t fourth)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_CTRL_LED_BLINK_TYPE2);
	packet.request.length = 7;
	packet.request.data[0] = first;
	packet.request.data[1] = second;
	packet.request.data[2] = third;
	packet.request.data[3] = third >> 8;
	packet.request.data[4] = fourth;
	packet.request.data[5] = fourth >> 8;
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_79074DE4(int32_t *value)
{
	uint32_t response = 0;
	int result;

	result = sysconReadWord(&response, 0x8ca);
	if (result < 0)
		return result;
	*value = (int8_t)response;
	return 0;
}

int SceSysconForDriver_7D25F6D2(uint32_t index, uint8_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xcb;
	packet.request.command[1] = 0x8;
	packet.request.length = 2;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	result = ksceSysconCmdExec(&packet, 0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0];
	return 0;
}

int SceSysconForDriver_D2ADABCA(uint32_t index, uint32_t value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xcc;
	packet.request.command[1] = 0x8;
	packet.request.length = 3;
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	packet.request.data[1] = value;
	result = ksceSysconCmdExec(&packet, 0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_7BAFE083(uint32_t value)
{
	return sysconWriteWord(value, 0xa82, 2);
}

int ksceSysconNvsSetRunMode(uint32_t value)
{
	return sysconWriteWord(value, SYSCON_CMD_NVS_SET_RUN_MODE, 2);
}

int SceSysconForDriver_2EC6D55D(uint32_t value)
{
	return sysconWriteWord(value, 0x1081, 2);
}

int SceSysconForDriver_351946B0(uint32_t *value)
{
	return sysconReadWord(value, 0x1310);
}

int ksceSysconGetBatteryCalibData(uint32_t *first, uint32_t *second,
			     uint32_t *third, uint32_t *fourth)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_BATTERY_CALIB_DATA);
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	if (first)
		*first = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	if (second)
		*second = packet.reply.data[2] | (uint16_t)packet.reply.data[3] << 8;
	if (third)
		*third = packet.reply.data[4] | (uint16_t)packet.reply.data[5] << 8;
	if (fourth)
		*fourth = packet.reply.data[6] | (uint16_t)packet.reply.data[7] << 8;
	return 0;
}

int ksceSysconGetBatteryVersion(uint32_t *first, uint32_t *second, uint32_t *third)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x80;
	packet.request.command[1] = 0x9;
	packet.request.length = 1;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[0] = 0x82;
		packet.request.command[1] = 8;
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	if (first)
		*first = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	if (second)
		*second = packet.reply.data[2] | (uint16_t)packet.reply.data[3] << 8;
	if (third)
		*third = packet.reply.data[4] | (uint16_t)packet.reply.data[5] << 8;
	return 0;
}

int ksceSysconReadBatteryReg(uint32_t index, uint16_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x86;
	packet.request.command[1] = 0x9;
	packet.request.length = 3;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[0] = 0xa8;
		packet.request.command[1] = 8;
	}
	if (index >= 0x10000)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	packet.request.data[1] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	return 0;
}

int ksceSysconWriteBatteryReg(uint32_t index, uint32_t value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x87;
	packet.request.command[1] = 0x9;
	packet.request.length = 5;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[0] = 0xa9;
		packet.request.command[1] = 8;
	}
	if (index >= 0x10000)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	packet.request.data[1] = index >> 8;
	packet.request.data[2] = value >> 8;
	packet.request.data[3] = value;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_A5AB19B1(uint32_t index, uint16_t *value)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x88;
	packet.request.command[1] = 0x9;
	packet.request.length = 3;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[0] = 0xaa;
		packet.request.command[1] = 8;
	}
	if (index >= 0x10000)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = index;
	packet.request.data[1] = index >> 8;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	*value = packet.reply.data[0] | (uint16_t)packet.reply.data[1] << 8;
	return 0;
}

int ksceSysconBatterySWReset(void)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_SW_RESET);
	packet.request.length = 1;
	packet.request.data[0] = 0;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		return (SCE_SYSCON_ERROR_REPLY | 0x3f);
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBatteryStartBLMode(void)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_START_BL_MODE);
	packet.request.length = 1;
	packet.request.data[0] = 0;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[1] = 8;
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBatteryStopBLMode(void)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_STOP_BL_MODE);
	packet.request.length = 1;
	packet.request.data[0] = 0;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504) {
		packet.request.command[1] = 8;
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBatterySetBLCommand(uint32_t offset, uint32_t mode, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_SET_BL_COMMAND);
	packet.request.length = 1;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504)
		packet.request.command[1] = 8;
	if (size - 1 >= 16)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 5;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	packet.request.data[3] = mode;
	memcpy(&packet.request.data[4], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBatteryExecBLCommand(uint32_t offset)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_EXEC_BL_COMMAND);
	packet.request.length = 3;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504)
		packet.request.command[1] = 8;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBatteryReadBLCommand(uint32_t offset, uint32_t first, uint32_t second,
			     void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_BATTERY_READ_BL_COMMAND);
	packet.request.length = 6;
	if ((uint32_t)ksceSysconGetBaryonVersion() < 0x70504)
		packet.request.command[1] = 8;
	if (size - 1 >= 16)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	packet.request.data[3] = first;
	packet.request.data[4] = second;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	memcpy(data, &packet.reply.data[2], size);
	return 0;
}

int SceSysconForDriver_C51B4E6F(uint32_t *information)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x00;
	packet.request.command[1] = 0xa;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	information[0] = packet.reply.word[0];
	information[1] = packet.reply.word[1];
	information[2] = packet.reply.word[2];
	information[3] = packet.reply.word[3];
	information[4] = packet.reply.word[4];
	information[5] = packet.reply.word[5];
	return 0;
}

int SceSysconForDriver_36AC812B(uint32_t offset, uint32_t size, void *data)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x80;
	packet.request.command[1] = 0xa;
	packet.request.length = 1;
	if (size - 1 >= 27 || offset + size > 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = 3;
	packet.request.data[0] = offset;
	packet.request.data[1] = size;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	memcpy(data, packet.reply.data, size);
	return 0;
}

int SceSysconForDriver_26E36625(uint32_t offset, uint32_t size, const void *data)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x81;
	packet.request.command[1] = 0xa;
	packet.request.length = 1;
	if (size - 1 >= 27 || offset + size > 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 3;
	packet.request.data[0] = offset;
	packet.request.data[1] = size;
	memcpy(&packet.request.data[2], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconNvsReadData(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_NVS_READ_DATA);
	packet.request.length = 1;
	if ((size != 1 && size != 2 && size != 4 &&
	     size != 8 && size != 16) ||
	    ((size - 1) & offset) || offset + size > 0xb60)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	memcpy(data, packet.reply.data, size);
	return 0;
}

int ksceSysconNvsWriteData(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_NVS_WRITE_DATA);
	packet.request.length = 1;
	if ((size != 1 && size != 2 && size != 4 &&
	     size != 8 && size != 16) ||
	    ((size - 1) & offset) || offset + size > 0xb60)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconJigOpenPort(void)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_JIG_OPEN_PORT);
	packet.request.length = 2;
	packet.request.data[0] = 0;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState >= 1) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result >= 0 && s_sysconControl.jigPortState < 0)
		s_sysconControl.jigPortState = 1;
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int ksceSysconJigClosePort(void)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_JIG_CLOSE_PORT);
	packet.request.length = 1;
	packet.request.data[0] = 0;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result >= 0)
		s_sysconControl.jigPortState = -1;
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int ksceSysconJigSetConfig(uint32_t first, uint32_t second)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_JIG_SET_CONFIG);
	packet.request.length = 5;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	packet.request.data[0] = first;
	packet.request.data[1] = second;
	packet.request.data[2] = 0;
	packet.request.data[3] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int SceSysconForDriver_3C80B529(uint32_t offset, uint32_t size, void *data)
{
	SceSysconPacket packet;
	int result;
	uint32_t chunk;

	packet.request.command[0] = 0x83;
	packet.request.command[1] = 0x20;
	packet.request.length = 3;
	if (!data)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	if (offset >= 64 || !size)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	do {
		chunk = size > 24 ? 24 : size;
		packet.request.data[0] = offset;
		packet.request.data[1] = chunk;
		result = ksceSysconCmdExec(&packet, 0);
		if (result < 0)
			break;
		offset += chunk;
		memcpy(data, packet.reply.data, chunk);
		size -= chunk;
		data = (uint8_t *)data + chunk;
	} while (size);
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int SceSysconForDriver_CE346793(uint32_t size, const void *data)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x84;
	packet.request.command[1] = 0x20;
	packet.request.length = 1;
	if (!data)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	if (size - 1 >= 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	packet.request.length = size + 3;
	packet.request.data[0] = 0;
	packet.request.data[1] = size;
	memcpy(&packet.request.data[2], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int SceSysconForDriver_7BFBA09E(uint32_t mode, uint32_t offset, uint32_t size, const void *data)
{
	SceSysconPacket packet;
	int result;
	uint32_t chunk;

	packet.request.command[0] = 0x85;
	packet.request.command[1] = 0x20;
	packet.request.length = 1;
	if (!data)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	if (offset >= 64 || !size || (int32_t)(offset + size) >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	do {
		chunk = size > 24 ? 24 : size;
		packet.request.command[0] = 0x85;
		packet.request.command[1] = 0x20;
		packet.request.length = chunk + 4;
		packet.request.data[0] = mode;
		packet.request.data[1] = offset;
		packet.request.data[2] = chunk;
		memcpy(&packet.request.data[3], data, chunk);
		result = ksceSysconCmdExec(&packet, 0);
		if (result < 0)
			break;
		size -= chunk;
		offset += chunk;
		data = (const uint8_t *)data + chunk;
	} while (size);
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int SceSysconForDriver_933D813F(uint32_t mode, uint32_t offset, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x85;
	packet.request.command[1] = 0x20;
	packet.request.length = 4;
	if (offset >= 64 || !size || (int32_t)(offset + size) >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	result = ksceKernelLockMutex(s_sysconControl.mutex, 1, NULL);
	if (result < 0)
		return result;
	if (s_sysconControl.jigPortState < 0) {
		ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	}
	packet.request.data[0] = mode;
	packet.request.data[1] = offset;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0x0);
	ksceKernelUnlockMutex(s_sysconControl.mutex, 1);
	return result;
}

int SceSysconForDriver_D2F456DC(uint8_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_ERNIE_DL_VERSION);
	packet.request.length = 1;
	if (!information)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result >= 0) {
		information[3] = packet.reply.data[0];
		information[2] = packet.reply.data[1];
		information[1] = packet.reply.data[2];
		information[0] = packet.reply.data[3];
	}
	return result;
}

int ksceSysconUpdaterSetSegment(uint32_t segment)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_UPDATER_SET_SEGMENT);
	packet.request.length = 2;
	packet.request.data[0] = segment;
	packet.request.data[1] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int SceSysconForDriver_356B9696(const void *data, uint32_t offset, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x81;
	packet.request.command[1] = 0x11;
	packet.request.length = 1;
	if (!data)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int SceSysconForDriver_734544E4(const void *data, uint32_t offset, uint32_t size)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];

	if (!data)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.command[0] = 0x81;
	packet.request.command[1] = 0x11;
	packet.request.length = 1;
	memset(transmit, 0, sizeof(transmit));
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[2] = size + 4;
	transmit[3] = offset;
	transmit[4] = offset >> 8;
	transmit[5] = size;
	memcpy(transmit + 6, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	return result;
}

int ksceSysconLoadConfigstorageScript(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];

	if (size - 1 >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_LOAD_CONFIGSTORAGE_SCRIPT);
	packet.request.length = 1;
	memset(transmit, 0, sizeof(transmit));
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[2] = size + 4;
	transmit[3] = offset;
	transmit[4] = offset >> 8;
	transmit[5] = size;
	memcpy(transmit + 6, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	return result < 0 ? result : 0;
}

int ksceSysconVerifyConfigstorageScript(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];

	if (size - 1 >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_VERIFY_CONFIGSTORAGE_SCRIPT);
	packet.request.length = 1;
	memset(transmit, 0, sizeof(transmit));
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[2] = size + 4;
	transmit[3] = offset;
	transmit[4] = offset >> 8;
	transmit[5] = size;
	memcpy(transmit + 6, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	return result < 0 ? result : 0;
}

int SceSysconForDriver_F9584DC9(uint32_t mode, uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];

	if (size - 1 >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.command[0] = 0x91;
	packet.request.command[1] = 0x13;
	packet.request.length = 1;
	memset(transmit, 0, sizeof(transmit));
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	transmit[2] = size + 5;
	transmit[3] = mode;
	transmit[4] = offset;
	transmit[5] = offset >> 8;
	transmit[6] = size;
	memcpy(transmit + 7, data, size);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	return result < 0 ? result : 0;
}

int ksceSysconUpdaterExecProgramming(uint32_t value)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_UPDATER_EXEC_PROGRAMMING);
	packet.request.length = 5;
	packet.request.data[0] = value >> 0;
	packet.request.data[1] = value >> 8;
	packet.request.data[2] = value >> 16;
	packet.request.data[3] = value >> 24;
	packet.request.data[4] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int ksceSysconUpdaterSetRunMode(uint32_t mode)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_UPDATER_SET_RUN_MODE);
	packet.request.length = 3;
	if (mode != 0x152e && mode != 0x3665 && mode != 0x72ba &&
	    mode != 0x9a54 && mode != 0xc5e7)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.data[0] = mode;
	packet.request.data[1] = mode >> 8;
	packet.request.data[2] = 0;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int ksceSysconUpdaterExecFinalize(const uint32_t *information, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_UPDATER_EXEC_FINALIZE);
	packet.request.length = 21;
	if (!information || size != 20)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	*(uint32_t *)&packet.request.data[0] = information[0];
	*(uint32_t *)&packet.request.data[4] = information[1];
	*(uint32_t *)&packet.request.data[8] = information[2];
	*(uint32_t *)&packet.request.data[12] = information[3];
	*(uint32_t *)&packet.request.data[16] = information[4];
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int ksceSysconUpdaterCheckSignature(const uint32_t *information, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_UPDATER_CHECK_SIGNATURE);
	packet.request.length = 25;
	if (!information || size != 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	*(uint32_t *)&packet.request.data[0] = information[0];
	*(uint32_t *)&packet.request.data[4] = information[1];
	*(uint32_t *)&packet.request.data[8] = information[2];
	*(uint32_t *)&packet.request.data[12] = information[3];
	*(uint32_t *)&packet.request.data[16] = information[4];
	*(uint32_t *)&packet.request.data[20] = information[5];
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}

int ksceSysconUpdaterCalcChecksum(const uint8_t *data, uint32_t size, uint32_t *checksum)
{
	uint32_t sum = 0;
	uint32_t index;

	if (!data || !size || !checksum)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	for (index = 0; index < size; index++)
		sum += data[index];
	*checksum = ~sum;
	return 0;
}

int ksceSysconGetConfigStorageInfo(uint16_t *information)
{
	SceSysconPacket packet;
	int result;

	sysconPacketSetCommand(&packet, SYSCON_CMD_GET_CONFIG_STORAGE_INFO);
	packet.request.length = 1;
	if (!information)
		return SCE_SYSCON_ERROR_INVALID_ARGUMENT;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	information[0] = (uint16_t)packet.reply.data[0] << 8 | packet.reply.data[1];
	information[1] = (uint16_t)packet.reply.data[2] << 8 | packet.reply.data[3];
	information[2] = (uint16_t)packet.reply.data[4] << 8 | packet.reply.data[5];
	information[3] = (uint16_t)packet.reply.data[6] << 8 | packet.reply.data[7];
	return 0;
}

int SceSysconForDriver_A6F05153(uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x80;
	packet.request.command[1] = 0x13;
	packet.request.length = 1;
	if (size - 1 >= 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	result = ksceSysconCmdExec(&packet, 0x0);
	if (result < 0)
		return result;
	memcpy(data, packet.reply.data, size);
	return 0;
}

int SceSysconForDriver_95C16911(uint32_t offset, const void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0x81;
	packet.request.command[1] = 0x13;
	packet.request.length = 1;
	if (size - 1 >= 24)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.length = size + 4;
	packet.request.data[0] = offset;
	packet.request.data[1] = offset >> 8;
	packet.request.data[2] = size;
	memcpy(&packet.request.data[3], data, size);
	result = ksceSysconCmdExec(&packet, 0x0);
	return result < 0 ? result : 0;
}

int ksceSysconBeginConfigstorageTransaction(void)
{
	return sysconWriteWord(0, SYSCON_CMD_BEGIN_CONFIGSTORAGE_TRANSACTION, 1);
}

int ksceSysconEndConfigstorageTransaction(void)
{
	return sysconWriteWord(0, SYSCON_CMD_END_CONFIGSTORAGE_TRANSACTION, 1);
}

int ksceSysconCommitConfigstorageTransaction(void)
{
	return sysconWriteWord(0, SYSCON_CMD_COMMIT_CONFIGSTORAGE_TRANSACTION, 1);
}

int SceSysconForDriver_76506A57(uint32_t mode, uint32_t offset, void *data, uint32_t size)
{
	SceSysconPacket packet;
	int result;
	uint8_t receive[128];

	packet.request.command[0] = 0x90;
	packet.request.command[1] = 0x13;
	packet.request.length = 5;
	if (offset >= 0x3f0 || size - 1 >= 64)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	memset(receive, 0, sizeof(receive));
	packet.status = SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX;
	packet.externalRx = receive;
	packet.externalRxSize = sizeof(receive);
	packet.request.data[0] = mode;
	packet.request.data[1] = offset;
	packet.request.data[2] = offset >> 8;
	packet.request.data[3] = size;
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_RX);
	if (result < 0)
		return result;
	memcpy(data, receive, size);
	return 0;
}

int SceSysconForDriver_FD65FFCB(uint32_t index, uint32_t mode)
{
	if (index >= 256 || mode >= 3)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	return sysconWriteWord(index | mode << 8, 0x1392, 3);
}

int SceSysconForDriver_02350352(uint32_t index)
{
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	return sysconWriteWord(index, 0x1393, 2);
}

int SceSysconForDriver_7DE84CE3(uint32_t index)
{
	if (index >= 256)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	return sysconWriteWord(index, 0x1394, 2);
}

static int sysconApplyConfigstorageMode(const uint8_t *script)
{
	int result;

	result = ksceSysconBeginConfigstorageTransaction();
	if (result < 0)
		return result;
	result = ksceSysconLoadConfigstorageScript(0, script, 32);
	if (result >= 0)
		result = ksceSysconLoadConfigstorageScript(32, script + 32, 32);
	if (result >= 0)
		result = ksceSysconCommitConfigstorageTransaction();
	if (result < 0) {
		ksceSysconEndConfigstorageTransaction();
		return result;
	}
	result = ksceSysconEndConfigstorageTransaction();
	return result < 0 ? result : 0;
}

static const uint8_t s_sysconIduModeSetScript[64] = {
	0x1f, 0x54, 0x35, 0x63, 0x38, 0x62, 0x31, 0x64,
	0x61, 0x63, 0x62, 0x38, 0x33, 0x36, 0x64, 0x64,
	0x36, 0x63, 0x39, 0x39, 0x36, 0x38, 0x65, 0x34,
	0x63, 0x30, 0x35, 0x30, 0x61, 0x36, 0x66, 0x35,
	0x07, 0x57, 0xa8, 0x01, 0x01, 0x00, 0x0d, 0x00,
	0x01, 0x4e, 0x15, 0x48, 0x51, 0xbd, 0x52, 0x2c,
	0x4c, 0x83, 0x70, 0xda, 0x4d, 0xf3, 0x8e, 0x82,
	0x6b, 0xd6, 0x34, 0x98, 0x3a, 0xfd, 0xa6, 0x94,
};

int ksceSysconIduModeSet(void)
{
	return sysconApplyConfigstorageMode(s_sysconIduModeSetScript);
}

static const uint8_t s_sysconIduModeClearScript[64] = {
	0x1f, 0x54, 0x65, 0x34, 0x61, 0x65, 0x38, 0x62,
	0x37, 0x61, 0x65, 0x38, 0x36, 0x33, 0x32, 0x65,
	0x64, 0x64, 0x39, 0x61, 0x61, 0x65, 0x38, 0x38,
	0x38, 0x39, 0x66, 0x30, 0x65, 0x65, 0x61, 0x38,
	0x07, 0x57, 0xa8, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x4e, 0x15, 0x48, 0x88, 0xea, 0xc7, 0xad,
	0x65, 0xb1, 0x73, 0xc1, 0xcf, 0x48, 0xc7, 0x29,
	0x1b, 0x2b, 0x8a, 0x32, 0x89, 0xcf, 0xb3, 0xc1,
};

int ksceSysconIduModeClear(void)
{
	return sysconApplyConfigstorageMode(s_sysconIduModeClearScript);
}

static const uint8_t s_sysconShowModeSetScript[64] = {
	0x1f, 0x54, 0x66, 0x63, 0x33, 0x39, 0x38, 0x39,
	0x30, 0x61, 0x33, 0x32, 0x64, 0x30, 0x66, 0x32,
	0x37, 0x36, 0x31, 0x35, 0x62, 0x65, 0x36, 0x65,
	0x63, 0x64, 0x63, 0x35, 0x65, 0x32, 0x63, 0x63,
	0x07, 0x57, 0xa8, 0x01, 0x03, 0x00, 0x10, 0x00,
	0x01, 0x4e, 0x15, 0x48, 0xb8, 0x1a, 0x54, 0xd1,
	0xf2, 0xa0, 0xd1, 0x05, 0x18, 0xea, 0x22, 0x09,
	0x0a, 0x02, 0x2e, 0xae, 0x00, 0x08, 0x25, 0xf1,
};

int ksceSysconShowModeSet(void)
{
	return sysconApplyConfigstorageMode(s_sysconShowModeSetScript);
}

static const uint8_t s_sysconShowModeClearScript[64] = {
	0x1f, 0x54, 0x65, 0x34, 0x61, 0x65, 0x38, 0x62,
	0x37, 0x61, 0x65, 0x38, 0x36, 0x33, 0x32, 0x65,
	0x64, 0x64, 0x39, 0x61, 0x61, 0x65, 0x38, 0x38,
	0x38, 0x39, 0x66, 0x30, 0x65, 0x65, 0x61, 0x38,
	0x07, 0x57, 0xa8, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x4e, 0x15, 0x48, 0x88, 0xea, 0xc7, 0xad,
	0x65, 0xb1, 0x73, 0xc1, 0xcf, 0x48, 0xc7, 0x29,
	0x1b, 0x2b, 0x8a, 0x32, 0x89, 0xcf, 0xb3, 0xc1,
};

int ksceSysconShowModeClear(void)
{
	return sysconApplyConfigstorageMode(s_sysconShowModeClearScript);
}

int SceSysconForDriver_4D03754A(const void *request, uint32_t requestSize,
			      void *reply, uint32_t replySize)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];
	uint8_t receive[128];

	if (!request || requestSize != 40 ||
	    !reply || replySize != 40)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	packet.request.command[0] = 0xd0;
	packet.request.command[1] = 0x0;
	packet.request.length = 41;
	memset(transmit, 0, sizeof(transmit));
	memset(receive, 0, sizeof(receive));
	packet.externalRx = receive;
	packet.externalRxSize = 46;
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	memcpy(transmit + 3, request, 40);
	result = ksceSysconCmdExec(&packet, 0xc00);
	if (result < 0)
		return result;
	memcpy(reply, receive, 40);
	return result;
}

int SceSysconForDriver_EBDF88B9(const void *request, uint32_t requestSize,
			      void *reply, uint32_t replySize)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];
	uint8_t receive[128];

	if (!request || requestSize != 16 ||
	    !reply || replySize != 48)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_SNVS_READ_DATA);
	packet.request.length = 17;
	memset(transmit, 0, sizeof(transmit));
	memset(receive, 0, sizeof(receive));
	packet.externalRx = receive;
	packet.externalRxSize = 54;
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	memcpy(transmit + 3, request, 16);
	result = ksceSysconCmdExec(&packet, 0xc00);
	if (result < 0)
		return result;
	memcpy(reply, receive, 48);
	return result;
}

int SceSysconForDriver_63683B9B(const void *request, uint32_t requestSize,
			      void *reply, uint32_t replySize)
{
	SceSysconPacket packet;
	int result;
	uint8_t transmit[128];

	if (!request || requestSize != 48 ||
	    !reply || replySize != 16)
		return SCE_SYSCON_ERROR_INVALID_SIZE;
	sysconPacketSetCommand(&packet, SYSCON_CMD_SNVS_WRITE_DATA);
	packet.request.length = 49;
	memset(transmit, 0, sizeof(transmit));
	packet.externalTx = transmit;
	transmit[0] = packet.request.command[0];
	transmit[1] = packet.request.command[1];
	transmit[2] = packet.request.length;
	memcpy(transmit + 3, request, 48);
	result = ksceSysconCmdExec(&packet, SCE_SYSCON_PACKET_FLAG_EXTERNAL_TX);
	if (result < 0)
		return result;
	memcpy(reply, packet.reply.data, 16);
	return result;
}

int SceSysconForDriver_C14BD637(void)
{
	SceSysconPacket packet;
	int result;

	packet.request.command[0] = 0xd1;
	packet.request.command[1] = 0x0;
	packet.request.length = 1;
	result = ksceSysconCmdExec(&packet, 0x0);
	return result;
}
