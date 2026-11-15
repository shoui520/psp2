/*
 * The supervision of a peer's rate (0x332ec - 0x3361a), and a store of calibration values
 * (0x3361c - 0x336de). Every 10 ms a connection's timer (macTxNotifyBuildFrame()) adds the
 * transmit statistics of the peer's block acknowledgement agreements to the rate control's
 * window and lets the rate selection decide whether to move to another rate; the functions
 * keep the firmware's placeholder names, "macTxNotify*Frame", which say nothing of what they do.
 * The calibration store has the same: it holds the per-band index values wlanCalibrationLookupQuad()
 * and wlanCalibrationLookupInterpolated() read, filled from SQRAM code. The names of the fields
 * and of the file are inferred from use.
 */
#include <stddef.h>
#include "robin/antdiv.h"
#include "robin/calibration.h"
#include "robin/txpower.h"
#include "robin/conn.h"
#include "robin/htba.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/peerrate.h"
#include "robin/ratectl.h"
#include "robin/rom.h"
#include "robin/softtimer.h"
#include "robin/sys.h"

int wlanRateProbingAt(const struct WlanRateControl *rc, int index);			/* ratectl.h */

/* The statistics macTxStatsAdd*() last read of each hardware block acknowledgement slot: prepared, checked, validated. */
static int baLast[2][3] __attribute__((section(".itcm.bss")));				/* 0x57588 */

/* The rates of the peer, for the frames the MAC sends it: the anchor, the first and the one in use. */
__attribute__((noinline)) int macTxNotifyFilterPacket(struct WlanConnection *connection)
{
	uint8_t current = connection->peerLink.rateControl.rates[connection->peerLink.rateControl.current];

	connection->probeRate = current;
	connection->currentRate = current;
	connection->anchorRate = connection->peerLink.rateControl.rates[connection->peerLink.rateControl.last];
	connection->firstRate = connection->peerLink.rateControl.rates[0];
	return 1;
}

__attribute__((noinline)) int macTxNotifyHandleFrame(struct WlanConnection *connection)
{
	connection->peerLink.rateControl.peerStatus = 0;
	wlanPsAcquireBuffer(connection);
	return macTxNotifyFilterPacket(connection);
}

/* 1: nothing was counted for 10 s, or the level says to start over. */
int macTxNotifyProcessFrame(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;

	rc->idleTime += 10;
	if (rc->idleTime < 10000 && wlanPsConfigureFrame(&rc->level) < 2)
		return 0;
	wlanPsManageBuffer(connection);
	antDivStart();
	rc->idleTime = 0;
	rc->peerStatus = 0;
	return 1;
}

int macTxNotifyUpdateFrame(struct WlanConnection *connection)
{
	struct WlanRateControl *rc = &connection->peerLink.rateControl;
	unsigned int status = rc->peerStatus;
	int result;

	wlanPsValidateFrame(&rc->window);
	result = macTxNotifyProcessFrame(connection);
	if (result != 0)
		return result;
	if (status == 0 || status == 2) {
		status = antDivRecord(connection);
		if (status == 0) {
			if (!(wlanTxRateFlags & 0x40))
				status = wlanPsHandlePacket(connection);
		} else if (status == 1) {
			status = wlanPsHandlePacket(connection);
		}
	} else if (status == 1) {
		status = wlanPsHandlePacket(connection);
	}
	rc->peerStatus = status;
	return result;
}

/* An IBSS peer or a station starts without block acknowledgement agreements and without a rate table slot. */
void macTxNotifyConfigureFrame(struct WlanConnection *connection)
{
	struct WlanPeerLink *link = &connection->peerLink;

	if (connection->interfaceType == 1 || connection->mode == 2) {
		link->baActive[0] = 0;
		link->baActive[1] = 0;
		link->reserved0 = 0;
		if (link->rateControl.slot != NULL) {
			link->rateControl.slot->used = 0;
			link->rateControl.slot = NULL;
		}
	}
}

void wlanBaNotifyAdded(struct WlanConnection *connection, int hardwareSlot)
{
	unsigned int type = connection->interfaceType;

	if (type != 0) {
		if (type == 3) {
			if (connection->mode == 0)
				return;
		} else if (type != 2) {
			return;
		}
	}
	if (hardwareSlot != 0)
		connection->peerLink.baActive[1] = 1;
	else
		connection->peerLink.baActive[0] = 1;
}

void wlanBaNotifyDeleted(struct WlanConnection *connection, int hardwareSlot)
{
	unsigned int type = connection->interfaceType;
	int *last;

	if (type == 0 || (type == 3 && connection->mode != 0) || type == 2) {
		if (hardwareSlot != 0) {
			if (hardwareSlot == 1 && connection->peerLink.baActive[1] != 0)
				connection->peerLink.baActive[1] = 0;
		} else if (connection->peerLink.baActive[0] != 0) {
			connection->peerLink.baActive[0] = 0;
		}
	}
	last = baLast[hardwareSlot];
	last[0] = 0;
	last[1] = 0;
	last[2] = 0;
}

/* Adds what the transmit statistics of the agreements of the peer count to the window. */
__attribute__((noinline)) void macTxNotifyPrepareFrame(struct WlanPeerLink *link)
{
	struct WlanRateControl *rc = &link->rateControl;
	int validated = 0;
	int prepared = 0;
	unsigned int checked = 0;
	int counted = 0;
	UINT posture = sysIrqDisable();

	if (link->baActive[0] != 0) {
		prepared = macTxStatsAddPrepared(0, &baLast[0][0]);
		checked = macTxStatsAddChecked(0, &baLast[0][1]);
		validated = macTxStatsAddValidated(0, &baLast[0][2]);
		counted = 1;
	}
	if (link->baActive[1] != 0) {
		prepared += macTxStatsAddPrepared(1, &baLast[1][0]);
		checked += macTxStatsAddChecked(1, &baLast[1][1]);
		validated += macTxStatsAddValidated(1, &baLast[1][2]);
	} else if (!counted) {
		sysIrqRestore(posture);
		return;
	}
	rc->window.validated += validated;
	rc->window.prepared += prepared;
	if (checked > 16)
		checked = 16;
	rc->window.checked = checked;
	rc->frames += prepared;
	if (validated != 0)
		rc->idleTime = 0;
	sysIrqRestore(posture);
}

void macTxNotifyDispatchFrame(struct WlanConnection *connection)
{
	connection->rateHold = 0;
}

void macTxNotifyParseFrame(struct WlanConnection *connection)
{
	connection->rateHold = 1;
}

/* The connection's 10 ms timer. */
void macTxNotifyBuildFrame(uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)(uintptr_t)argument;

	if (connection->rateHold != 1) {
		macTxNotifyPrepareFrame(&connection->peerLink);
		macTxNotifyUpdateFrame(connection);
		macTxNotifyFilterPacket(connection);
	}
}

unsigned int macTxNotifyComputeFrame(void)
{
	antDivStart();
	wlanPsComputePacket();
	macMgmtComputeHeader();
	return wlanPsReleaseBuffer();
}

void macTxNotifyQueryFrame(struct WlanConnection *connection)
{
	wlanSoftTimerStop(&connection->softTimer);
	wlanSoftTimerStartPeriodic(&connection->softTimer, 1, macTxNotifyBuildFrame, (uintptr_t)connection, 1);
}

void macTxNotifyResetFrame(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	if (connection->rateHold != 1)
		wlanPsProcessPacket(&connection->peerLink.rateControl, status);
}

/*
 * HostCmd_CMD_TX_RATE_QUERY: the rate index the peer is sent at, or the one in use while the host
 * lets the firmware choose (dataRate 254), as the rate index and as flags: bit 0 set when the
 * rate is an HT rate, bit 1 for 40 MHz and bit 2 for the short guard interval.
 */
void hostCmd007f(struct WlanConnection *connection, uint8_t *rate, uint8_t *flags)
{
	unsigned int index = connection->dataRate;
	uint8_t wide;
	uint8_t value;
	int shortGuard;

	if (index == 254) {
		index = connection->currentRate;
		shortGuard = wlanRateProbingAt(&connection->peerLink.rateControl, index) != 0;
	} else {
		shortGuard = wlanHtShortGiWanted(connection, index);
	}
	if (wlanPsUpdateFrame(index, &wide, &value) != 0) {
		*rate = value;
		*flags = (2 * wide + 1) | (4 * shortGuard);
	} else {
		*rate = index;
		*flags = 0;
	}
}

int macTxNotifyApplyFrame(struct WlanConnection *connection)
{
	return macTxNotifyHandleFrame(connection);
}

/*
 * The calibration values of the band indexes: a quad per index, whose validity is two bits of
 * the slot (wlanParam32Set()), and a record per index, with a bit each in `present`.
 */
struct WlanChannelCalibration {
	uint8_t quad[10];
	struct WlanCalibrationChannelValues record[2];
	uint8_t present;
	uint8_t reserved;
};

_Static_assert(sizeof(struct WlanChannelCalibration) == 48, "channel calibration");

static struct WlanChannelCalibration channelCalibration __attribute__((section(".itcm.bss")));	/* 0x575a0 */

int macTxNotifyResolveFrame(unsigned int index, const uint32_t *value)
{
	return wlanParam32Set(channelCalibration.quad, index, value);
}

int macTxNotifyScheduleFrame(unsigned int index, uint32_t *value)
{
	return wlanParam32Get(channelCalibration.quad, index, value);
}

int macTxNotifyNotifyFrame(unsigned int index, const struct WlanCalibrationChannelValues *values)
{
	if (index >= 2)
		return 0;
	channelCalibration.present |= 1 << index;
	__aeabi_memcpy(&channelCalibration.record[index], values, sizeof(channelCalibration.record[0]));
	return 1;
}

int macTxNotifyFormatFrame(unsigned int index, struct WlanCalibrationChannelValues *values)
{
	if (index >= 2 || !(channelCalibration.present & (1 << index)))
		return 0;
	__aeabi_memcpy(values, &channelCalibration.record[index], sizeof(channelCalibration.record[0]));
	return 1;
}

__attribute__((noinline)) void macTxNotifyInspectFrame(void)
{
	__aeabi_memclr(channelCalibration.record,
		       sizeof(channelCalibration) - offsetof(struct WlanChannelCalibration, record));
}

/* Drops every calibration value that was loaded. */
void macTxNotifyTrackFrame(void)
{
	wlanCalibFlagsClear();
	wlanCalibContextClear();
	wlanCalibValue1Clear();
	wlanCalibValue0Clear();
	wlanCalibValue2Clear();
	wlanCalibSignalClear();
	wlanCalibBlockClear();
	wlanParamSlotClear(channelCalibration.quad);
	wlanCalibPowerClear();
	wlanCalibSecondaryClear();
	wlanCalibLevelsClear();
	wlanCalibRangeAdjustClear();
	macTxNotifyInspectFrame();
}
