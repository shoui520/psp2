/*
 * The Bluetooth link controller's contexts (0x42b66 - 0x43030): the clock requests, the SoC registers its
 * start-up sets, the small coexistence state, and the hooks the ROM's controller calls. The file name and
 * the roles are inferred; the function names are the firmware's placeholders.
 */
#include <stddef.h>
#include "robin/btcontext.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/eventpower.h"
#include "robin/socpower.h"
#include "robin/soc.h"

struct BtLcContext btLmpLcContext __attribute__((section(".itcm.rw"))) = {
	.node = 5,
	.slot = 4,
	.activeCount = 3,
};
uint16_t *btLinkRate __attribute__((section(".dtcm.bss")));			/* 0x400f934 */
uint8_t btLinkMaskContext __attribute__((section(".dtcm.bss")));			/* 0x400f7ff */
int32_t btLinkMaskHeader __attribute__((section(".dtcm.bss")));			/* 0x400f8c8 */
uint32_t btLmpLcEntryCount[8] __attribute__((section(".itcm.bss")));		/* 0x59ea8 */

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

unsigned int btLmpLcPrepareContext(void)
{
	return btLmpLcTrackFrame();
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
