/*
 * The Bluetooth link controller's contexts (0x42b66 - 0x43030): the clock requests, the SoC registers its
 * start-up sets, the small coexistence state, and the hooks the ROM's controller calls. The file name and
 * the roles are inferred; the function names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btbuffer.h"
#include "robin/btcontext.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/amp.h"
#include "robin/eventpower.h"
#include "robin/eventroute.h"
#include "robin/hct.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/socpower.h"
#include "robin/soc.h"

struct BtLcContext btLmpLcContext __attribute__((section(".itcm.rw"))) = {
	.node = 5,
	.slot = 4,
	.activeCount = 3,
};
uint32_t btLmpLcEntryCount[8] __attribute__((section(".itcm.bss")));		/* 0x59ea8 */
struct BtLcTarget btLmpLcTarget[2] __attribute__((section(".itcm.bss")));		/* 0x59ec8 */

/* The controller is not running: every clock request is withdrawn (0 kHz). */
unsigned int btLmpLcHandleContext(void)
{
	clockRequestCore(0, 1);
	clockRequestBus(0, 1);
	clockRequestAux(0, 1);
	return clockRequestVote(0, 1);
}

/* The ROM's descriptor of the host transport goes into the record. */
uint8_t *btLmpLcProcessContext(uint8_t *record)
{
	record[9] = romHctDescriptor;
	*(uint16_t *)(record + 10) = romHctHeaderSize;
	return record;
}

int btLmpLcUpdateContext(void)
{
	return btHccRomPrepareState();
}

/*
 * Brings the Bluetooth block up: its SoC registers, the clocks (16000 kHz), the PLL, and the clock
 * divider by the reference clock's rate (3200, 32000 or 320000 kHz).
 */
int btLmpLcConfigureContext(void)
{
	unsigned int rate;

	SOC_REG(0x30) &= 0xf3ffffff;
	SOC_REG(0x0c) |= 0x218010;
	SOC_REG(0x0c) |= 0x10083020;
	clockRequestCore(0x3e80, 0);
	clockRequestAux(0x3e80, 0);
	clockRequestBus(0x3e80, 0);
	clockRequestVote(0x3e80, 0);
	SOC_REG(0x44) |= 2;
	SOC_REG(0x9c) |= 5;
	SOC_REG(0x9c) &= ~0x100000u;
	SOC_REG(0x9c) |= 0x40200000;
	SOC_REG(0x9c) |= 0x80400000;
	SOC_REG(0x9c) |= 0x40000;
	SOC_REG(0x9c) |= 0x80000;
	SOC_REG(0x9c) |= 0x10000;
	SOC_REG(0x9c) |= 0x20000;
	SOC_REG(0x9c) |= 2;
	clockPllProgram();
	SOC_REG(0x11c) &= ~0x20000000u;
	SOC_REG(0x11c) |= 0x08000000;
	SOC_REG(0xfc) &= ~0x100000u;
	sysDelayUs(1000);
	SOC_REG(0x9c) |= 0x40200000;
	SOC_REG(0x9c) &= 0x7fbfffff;
	SOC_REG(0x9c) |= 0x40000;
	SOC_REG(0x9c) &= ~0x80000u;
	if ((SOC_REG(0x18) & 0xff) != 0)
		SOC_REG(0x9c) &= ~4u;
	rate = uapActiveEntryGet();
	if (rate == 3200) {
		SOC_REG(0x44) |= 0x3000000;
	} else if (rate == 32000) {
		SOC_REG(0x44) &= 0xfcffffff;
	} else {
		SOC_REG(0x44) &= 0xfcffffff;
		SOC_REG(0x44) |= rate == 320000 ? 0x2000000 : 0x1000000;
	}
	SOC_REG(0x5c) &= 0xfeffffFD;
	SOC_REG(0x5c) |= 0x1000002;
	return sysDelayUs(1);
}

/* The version of the host interface, as the controller reports it: its major number plus 0x4300. */
unsigned int btLmpLcCheckContext(void)
{
	return (hostCmd0003Version() >> 24) + 17152;
}

void btLmpLcPrepareContext(void)
{
	btLmpLcTrackFrame();
}

int btLmpLcDispatchContext(void)
{
	btLmpLcContext.window = 0;
	return (int)(uintptr_t)btLtsRomPrepareContext(&btLmpLcContext.node, 0);
}

uint8_t *btLmpLcParseContext(void)
{
	btLmpLcContext.window = 0;
	return &btLmpLcContext.node;
}

/* The shortest time the ROM's scheduler allows (or -1 when no link) for the targets in use. */
int btLmpLcBuildContext(uint32_t *remaining)
{
	*remaining = -1;
	if (romTargetTable != 4) {
		if (romTargetTable != 3)
			return 0;
		*remaining = btLdrRomUpdateState(0);
	}
	if (btLinkMaskHeader != -1) {
		uint32_t updated;

		if (btLdrRomUpdateState(1) >= *remaining)
			updated = *remaining;
		else
			updated = btLdrRomUpdateState(1);
		*remaining = updated;
	}
	return 1;
}

void btLmpLcRecordState(void)
{
}

void btLmpLcComputeContext(void)
{
}

/* The coexistence events: 1 and 2 start a format of the link's context, 3 ends it. */
int btLmpLcQueryContext(int link, const uint8_t *event)
{
	unsigned int type = event[0];
	uint32_t mode;
	uint32_t argument;

	if (type == 0)
		return 1;
	if (type == 1 || type == 2) {
		if (link == 1) {
			mode = 1;
			argument = 0;
		} else if (link == 3) {
			argument = event[1] | (event[2] << 8);
			mode = 1;
		} else {
			return 0;
		}
	} else if (type == 3 && link == 1) {
		if (btLmpLcActiveCount == 1) {
			btLmpLcActiveCount = event[0];
			return 1;
		}
		mode = 2;
		argument = 0;
	} else {
		return 0;
	}
	btLmpLcFormatContext(0, 0, mode, argument);
	if (event[0] == 0)
		return 1;
	btLmpLcActiveCount = event[0];
	return 1;
}

/* The ROM's events 0xe1-0xe4 (result 2), 0xf1 and 0xf2 (the bitmap flag; 0xf2 also ends the format). */
int btLmpLcResetContext(int link, const uint8_t *event)
{
	uint32_t ended = 3;
	unsigned int type = event[0];

	if (type == 228)
		return 2;
	if (type > 0xe4) {
		if (type == 241) {
			btLmpLcContext.bitmap = 1;
		} else {
			if (type != 242)
				return 0;
			btLmpLcQueryContext(1, (const uint8_t *)&ended);
			btLmpLcContext.bitmap = 0;
		}
		return 0;
	}
	if (type == 225 || type == 226 || type == 227)
		return 2;
	return 0;
}

/* Tells the coexistence code whether the controller is scheduled, and counts the entry's use. */
void btLmpLcSyncContext(void)
{
	uint8_t *link = btLcoRomResetContext(0);
	unsigned int scheduled = 1;
	unsigned int entry;
	unsigned int rate;
	unsigned int index;

	if (btLdaRomPrepareState(5) == 0 && btLdaRomPrepareState(7) == 0)
		return;
	for (index = 1; index < 8; index = (uint16_t)(index + 1)) {
		if ((link[52 * index] & 2) != 0 && link[52 * index + 7] == 0) {
			scheduled = 0;
			break;
		}
	}
	btCoexSetScheduled(scheduled);
	rate = *btLinkRate >> 11;
	entry = btLmpLcResetEntry();
	if (rate & 1) {
		if (entry > btLmpLcContext.node && btLinkMaskContext == 0) {
			btCoexActivityStopped();
			*btLinkRate &= 0xf7ff;
		}
	} else if (!(entry > btLmpLcContext.slot && btLinkMaskContext == 0)) {
		btCoexActivityStarted();
		*btLinkRate |= 0x800;
	}
	if (entry < 8)
		btLmpLcEntryCount[entry]++;
}

/*
 * Tells the coexistence code the profile of the links in use: bit 0 a link of the Bluetooth device, bit 1
 * a link of the AMP, bits 4-6 the packet type class, bits 7-13 the smallest interval, bit 14 all links
 * of the device are in sniff or hold, and the bits 2 and 3 the device's flags. It is told only when the
 * profile changed.
 */
void btLmpLcApplyContext(void)
{
	uint16_t profile = 0x3f80;
	unsigned int links = 0;
	unsigned int idle = 0;
	unsigned int slot;

	if ((btLinkMaskState | btLinkIndexRecord) != 0)
		profile |= 8;
	if (btDeviceContext.entry != 0)
		profile |= 4;
	for (slot = 1; slot < 11; slot = (uint16_t)(slot + 1)) {
		unsigned int state = romHctHostSlot[slot][0];

		if (state != 5 && state != 7)
			continue;
		if (slot < 8) {
			unsigned int mode = btLdrRomReleaseState()[116 * slot + 68];

			links = (uint8_t)(links + 1);
			if (mode == 3 || mode == 1)
				idle = (uint8_t)(idle + 1);
			profile |= 1;
		} else {
			const uint8_t *amp = btLcoRomProcessState(slot - 8) + 30 * (uint16_t)(slot - 8);
			unsigned int type;

			if (amp[5] < ((profile >> 7) & 0x7f))
				profile = (profile & 0xc07f) | ((amp[5] << 7) & 0x3fff);
			type = amp[9];
			if (type == 44 || type == 61 || type == 13 || type == 12)
				profile = (profile & 0xff8f) + 0x60;
			else if (((profile >> 4) & 7) == 0)
				profile = (profile & 0xff8f) + 0x20;
			profile |= 2;
		}
	}
	if (links != 0 && links == idle)
		profile |= 0x4000;
	if (profile != btLmpLcContext.rate) {
		btLmpLcContext.rate = profile;
		btCoexSetProfile(profile);
	}
}

/* The idle hook: the coexistence profile is refreshed while the chip may not sleep. */
int btLmpLcResolveContext(void)
{
	if (!sysSleepVotesAllowed(1)) {
		btLmpLcSyncContext();
		btLmpLcApplyContext();
	}
	hctCoexTick();
	return 0;
}

int btLmpLcScheduleContext(void)
{
	btLmpLcContext.idleHook.function = btLmpLcResolveContext;
	return osaAddIdleHook(&btLmpLcContext.idleHook);
}

/*
 * Posts an event of the link (message 5100, or 5099 without type) with the data to the host transport's
 * queue; the AMP's untyped event goes to the host as a vendor event instead. 1 when no message was free.
 */
int btLmpLcNotifyContext(int link, int type, const void *data, unsigned int length)
{
	uint8_t *message;

	message = osa_MsgAlloc(length + 13);
	if (message == NULL)
		return 1;
	if (type != 0) {
		*(uint32_t *)message = 5100;
		message[8] = type;
		*(uint32_t *)(message + 4) = link;
	} else {
		*(uint32_t *)message = 5099;
		message[8] = 0xff;
	}
	message[9] = length;
	__aeabi_memcpy(message + 10, data, length);
	if (link == 1 && type == 0) {
		ampHciSendEvent(0xff, 0, "Vm", data, length);
		osa_MsgFree((void **)&message);
	} else {
		osa_MsgSend((void **)&message, romQueueHct);
	}
	return 0;
}

/* A two byte event: the bytes kind 1 gives, or the halfword value under the type. */
int btLmpLcFormatContext(int link, int kind, int type, int value)
{
	uint8_t data[4];

	if (kind == 1) {
		data[0] = type;
		data[1] = value;
		type = 0;
	} else {
		data[0] = value;
		data[1] = value >> 8;
	}
	return btLmpLcNotifyContext(link, type, data, 2);
}

/* A target that was released is taken off the event routing, and the controller is told if asked. */
void btLmpLcInspectContext(int link, int notify)
{
	unsigned int released = 0;
	UINT posture = sysIrqDisable();
	struct BtLcTarget *target = &btLmpLcTarget[link];

	if (target->phase == 3) {
		target->phase = 0;
		target->flag = 0;
		sysEventRouteSetMasked(sysEventRouteDevice(link == 1 ? 3 : 0), 0);
		released = 1;
	}
	sysIrqRestore(posture);
	if (released != 0 && notify != 0)
		btLmpLcFormatContext(link, 1, 32, 2);
}

/*
 * Releases a target: waits out the delay it asked for (mode 0xff: after the event clock's elapsed
 * time), then hands the device over through mdevIoctl() 0xb, or 0x61 with the Bluetooth device.
 */
int btLmpLcTrackContext(int link)
{
	struct BtLcTarget *target = &btLmpLcTarget[link];

	if (target->mode == 0xff) {
		if (target->delay != 0 && target->delay != 0xff) {
			unsigned int elapsed = sysEventClockElapsedUs(target->since);
			unsigned int limit = 1000 * target->delay;

			if (elapsed < limit && limit - elapsed > 1)
				sysDelayUs(limit - elapsed);
		}
		mdevIoctl(link == 1 ? ampSdioDev : btSdioActiveHandle, 0xb, 0);
		btLmpLcInspectContext(link, 1);
		target->flag = 0;
		return 0;
	}
	mdevIoctl(btSdioActiveHandle, 0x61, 1);
	if (target->delay == 0xff)
		return mdevIoctl(link == 1 ? ampSdioDev : btSdioActiveHandle, 0xb, 0);
	if (target->delay != 0)
		sysDelayUs(1000 * target->delay);
	mdevIoctl(btSdioActiveHandle, 0x61, 0);
	btLmpLcInspectContext(link, 1);
	return 0;
}
