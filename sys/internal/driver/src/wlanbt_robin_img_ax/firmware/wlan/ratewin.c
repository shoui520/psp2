/*
 * The window of the transmissions the rate selection counts (0x4c0d8 - 0x4c408): what a
 * transmit status adds to WlanRateControl.window and the level, and the verdict on the window
 * (fmCoexReleaseTable()) that wlanPsParseFrame() takes. The names of the firmware's placeholders
 * are kept; see robin/ratesel.h.
 */
#include <stddef.h>
#include "robin/ratectl.h"
#include "robin/ratesel.h"
#include "robin/rom.h"
#include "robin/threadx.h"
#include "robin/txring.h"

extern void fmCoexManageTable(uint8_t *table);						/* 0x4c0cc */

struct WlanRateWindowState wlanRateWindowState __attribute__((section(".itcm.bss")));	/* 0x553f8 */

/* Starts a window for rate (bit 7: a probe). */
__attribute__((noinline)) void fmCoexAcquireTable(struct WlanRateWindow *window, unsigned int rate)
{
	window->rate = rate;
	window->exchanges = 0;
	window->sent = 0;
	window->delivered = 0;
	window->age = 0;
	window->validated = 0;
	window->prepared = 0;
	window->checked = 0;
}

/*
 * How the window fares: 0 when the rate holds, 1 when it is too poor and 2 when the window does
 * not tell yet. The thresholds (the transmissions it takes to decide, and the share that must be
 * acknowledged) are the ROM's for a connection without aggregation.
 */
int fmCoexReleaseTable(struct WlanRateWindow *window, int *percent, int ampdu)
{
	unsigned int rate = window->rate & 0x7f;
	unsigned int sent = 0;
	unsigned int delivered = 0;
	unsigned int limit;
	unsigned int few;
	UINT posture = tx_interrupt_control(TX_INT_DISABLE);

	wlanRateWindowState.count(window, &sent, &delivered);
	sent += window->sent;
	delivered += window->delivered;
	tx_interrupt_control(posture);
	if (wlanRateWindowState.reportEnabled != 0) {
		*wlanRateWindowState.reportDelivered = delivered;
		*wlanRateWindowState.reportSent = sent;
	}
	if (sent == 0)
		return 2;
	*percent = (100 * delivered) / sent;
	limit = 50;
	if (ampdu == 0)
		limit = romLibApplyEntry(rate, 255, 0);
	if (limit <= sent) {
		limit = 30;
		if (ampdu == 0)
			limit = romLibQueryEntry(rate, 255, 0);
		return limit > (unsigned int)*percent;
	}
	limit = 32;
	if (ampdu == 0)
		limit = romLibResolveEntry(rate, 255, 0);
	if (limit > sent) {
		few = 16;
		if (ampdu == 0)
			few = romLibScheduleEntry(rate, 255, 0);
		limit = 100;
		if (few > sent) {
			if (window->age >= wlanRateWindowState.ageLimit) {
				if (ampdu == 0)
					limit = romLibSyncEntry(rate, 255, 0);
				return limit > (unsigned int)*percent;
			}
			return 2;
		}
		if (ampdu == 0)
			limit = romLibSyncEntry(rate, 255, 0);
	} else {
		limit = 75;
		if (ampdu == 0)
			limit = romLibResetEntry(rate, 255, 0);
	}
	if (limit > (unsigned int)*percent)
		return 2;
	return 0;
}

/* The status is of a data frame the window counts: not of a block acknowledgement or a protected exchange, with a count. */
int fmCoexSetupTable(const struct WlanTxStatus *status)
{
	return (status->flags & 0x100) == 0 && (__aeabi_uread4(status->data + 8) & 0x20000000) == 0 &&
	       status->info != 0;
}

/* Counts what the status says of the level: acknowledged frames in a row at a rate, or frames dropped. */
__attribute__((noinline)) void fmCoexClearTable(struct WlanRateWindow *window, const struct WlanTxStatus *status)
{
	struct WlanRateLevel *level = (struct WlanRateLevel *)(window + 1);

	if (status->acknowledged != 0) {
		if (level->ackedRate == status->finalRate) {
			level->ackedRepeats++;
		} else {
			level->ackedRate = status->finalRate;
			level->ackedRepeats = 1;
		}
		level->dropped = 0;
	} else {
		if (status->retryBlocked != 0)
			return;
		level->dropped++;
	}
}

/*
 * Fills in what the status does not carry: from the transmissions the hardware counted (info),
 * the exchanges, the retries and the acknowledgement, and the transmissions at each rate of the
 * chain the frame was sent with (rateInfo).
 */
void fmCoexConvertTable(struct WlanTxStatus *status)
{
	unsigned int count = status->info;
	unsigned int flags = status->flags & 0xffff;
	unsigned int i;

	status->retryBlocked = status->type - count;
	if (flags & 0x20) {
		status->rtsTries = count;
		status->ctsToSelf = 0;
		if (flags & 0x100) {
			status->acknowledged = 0;
			status->dataRetries = 0;
			if (count == 0 || (status->result & 1)) {
				status->rtsAcknowledged = 0;
				status->rtsRetries = count;
				status->exchanges = 0;
			} else {
				status->rtsAcknowledged = 1;
				status->rtsRetries = count - 1;
				status->exchanges = 1;
			}
			status->retries = status->rtsRetries;
			status->rtsDataRetries = 0;
		} else {
			if (count == 0 || (status->result & 1)) {
				status->acknowledged = 0;
				status->retries = count;
			} else {
				status->acknowledged = 1;
				status->retries = count - 1;
			}
			status->rtsRetries = 0;
			status->rtsAcknowledged = count;
			status->rtsDataRetries = 0;
			status->exchanges = count;
			status->dataRetries = count - status->acknowledged;
		}
	} else {
		status->rtsAcknowledged = 0;
		status->rtsRetries = 0;
		status->rtsTries = 0;
		status->rtsDataRetries = 0;
		if (flags & 0x100) {
			status->dataRetries = 0;
			status->exchanges = 1;
		} else {
			if (count == 0 || (status->result & 1)) {
				status->acknowledged = 0;
				status->retries = count;
			} else {
				status->acknowledged = 1;
				status->retries = count - 1;
			}
			status->dataRetries = status->retries;
			status->exchanges = count;
		}
		if (flags & 0x80)
			status->ctsToSelf = status->exchanges;
		else
			status->ctsToSelf = 0;
	}
	__aeabi_memclr(status->attempts, sizeof(status->attempts));
	if (status->rateInfo != 0) {
		status->rate = wlanRateEntryIndex(*(const uint32_t *)(uintptr_t)status->rateInfo);
		for (i = 0; count != 0 && i < 8; i = (uint8_t)(i + 1)) {
			/* read again for each rate: the counts written below may alias it */
			uint32_t word = ((const uint32_t *)(uintptr_t)status->rateInfo)[i];
			unsigned int index = wlanRateEntryIndex(word);
			unsigned int tries = (uint8_t)wlanRateEntryRetryGet(word);

			if (i == 7)
				tries = 255;
			if (count <= tries) {
				status->attempts[index] += count;
				status->finalRate = index;
				count = 0;
				status->rateOption = (uint32_t)(word << 12) >> 28;
			} else {
				status->attempts[index] += tries;
				count = (uint8_t)(count - tries);
			}
		}
	} else {
		status->attempts[status->rate] = count;
	}
}

/* Adds the status to the window: the exchanges, and the transmissions and acknowledgements at its rate. */
unsigned int fmCoexVerifyTable(struct WlanRateWindow *window, const struct WlanTxStatus *status)
{
	unsigned int rate = window->rate & 0x7f;
	unsigned int probe = window->rate & 0x80;
	unsigned int sent;
	unsigned int delivered;

	window->exchanges += status->exchanges;
	sent = status->attempts[rate];
	delivered = sent;
	if (status->finalRate == rate && status->acknowledged != 0)
		delivered = (uint8_t)(sent - 1);
	if (probe != 0 && sent > 1) {
		sent = 1;
		delivered = 1;
	}
	window->delivered += delivered;
	window->sent += sent;
	return sent;
}

unsigned int fmCoexSelectTable(struct WlanRateWindow *window, const struct WlanTxStatus *status)
{
	unsigned int result = fmCoexSetupTable(status);

	if (result != 0) {
		fmCoexClearTable(window, status);
		return (uint8_t)fmCoexVerifyTable(window, status);
	}
	return result;
}

/* Starts the window and the level over for the rate the selection is at. */
void fmCoexAdjustTable(struct WlanRateControl *rc)
{
	unsigned int rate = wlanRateSelect(rc);
	unsigned int probe = rc->probing;

	if (probe != 0)
		probe = 0x80;
	fmCoexAcquireTable(&rc->window, probe | rate);
	fmCoexManageTable((uint8_t *)&rc->level);
}
