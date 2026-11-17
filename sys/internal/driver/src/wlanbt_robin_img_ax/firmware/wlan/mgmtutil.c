/*
 * Small management helpers: the station's MAC address in the ROM, the timing
 * record an IBSS start or join hands the callback processor, the management
 * frame header and the messages of the connection state machine. The
 * filename is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/mac.h"
#include "robin/mgmt.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/threadx.h"

/* Override slots (0x55bcc..): the IBSS join record, the generated address and the others. */
static ROBIN_PATCH_SLOT(wlanIbssRecordJoinOverride);
static ROBIN_PATCH_SLOT(wlanIbssAddressGenerateOverride);
static ROBIN_PATCH_SLOT(wlanIbssRecordStartOverride);
static ROBIN_PATCH_SLOT(wlanBuildMgmtHeaderOverride);
static ROBIN_PATCH_SLOT(wlanEventMessageBuildOverride);

extern int (*wlanFrameRecordHook)(struct WlanBssTiming *timing);	/* 0x54e5c */

/* What the timing record of an IBSS holds (romLibUpdateContext() fills it; 48 bytes). */
/* 0x0002a20c: the association changed (set or cleared); tells the ROM. */
int wlanAssociationChanged(void)
{
	return romLibComputeState();
}

/* Sets the address the MAC uses (six bytes), or clears it. */
void wlanMacAddressSet(const uint8_t *address)
{
	uint32_t word[2];
	uint8_t *buffer = (uint8_t *)word;
	uint32_t zero[2] = { 0, 0 };

	if (address == NULL) {
		romLibQueryState(zero);
		return;
	}
	buffer[0] = address[0];
	buffer[1] = address[1];
	buffer[2] = address[2];
	buffer[3] = address[3];
	buffer[4] = address[4];
	buffer[5] = address[5];
	buffer[6] = 0;
	buffer[7] = 0;
	romLibResetState(word[1], word[0]);
}

/* 0x0002a254: the MAC takes the SSID (the ROM's routine). */
void wlanMacSsidApply(const uint8_t *ssid, unsigned int length)
{
	romLibBuildState(ssid, length);
}

/*
 * Makes an address for an IBSS that has none: locally administered, from the
 * time, the TSF and a value of the interface (a halfword at 24 of source),
 * and gives it to the ROM.
 */
void wlanIbssAddressGenerate(uint8_t *address, const uint8_t *source)
{
	uint32_t zero[2] = { 0, 0 };
	uint32_t word[2];
	uint32_t tsfLow;

	if (wlanIbssAddressGenerateOverride(address, source) != 0)
		return;
	if (memcmp(address, zero, 6) != 0)
		return;
	msi_wl_GetMCUCoreTimerTxTSF(&word[0], &word[1]);
	tsfLow = word[1];
	word[1] = tx_time_get();
	srand((*(const uint16_t *)(source + 24) + word[1]) ^ tsfLow);
	word[0] = (uint16_t)rand();
	word[1] += word[0];
	address[0] = 2;
	address[1] = (word[1] & 0xf) + 0x20;
	__aeabi_memcpy(address + 2, &word[1], 4);
	__aeabi_memcpy(&word[1], address, 4);
	address[4] = ((uint8_t *)&word[0])[0];
	address[5] = ((uint8_t *)&word[0])[1];
	romLibResetState(word[0], word[1]);
}

/* The timing record of an IBSS the host starts. */
int wlanIbssRecordStart(const WlanAdHocStart *start)
{
	struct WlanBssTiming record;
	int result;

	result = wlanIbssRecordStartOverride(start);
	if (result == 0 && start != NULL) {
		romLibUpdateContext(&record);
		record.beaconPeriod = start->beaconPeriod;
		record.dtimPeriod = start->dtimPeriod;
		if (start->ssParamSet[0] == 6) {
			record.atimWindow = (start->ssParamSet[3] << 8) | start->ssParamSet[2];
		} else if (start->ssParamSet[0] == 4) {
			record.cfpCount = start->ssParamSet[2];
			record.cfpPeriod = start->ssParamSet[3];
			record.cfpMaxDuration = (start->ssParamSet[5] << 8) | start->ssParamSet[4];
		}
		record.next[1] = 0;
		record.next[0] = start->beaconPeriod << 11;
		record.timestamp[0] = 0;
		record.timestamp[1] = 0;
		return wlanFrameRecordHook(&record);
	}
	return result;
}

/* The timing record of the IBSS the host joins. */
int wlanIbssRecordJoin(const WlanAdHocBssDesc *join)
{
	struct WlanBssTiming record;
	int result;

	result = wlanIbssRecordJoinOverride(join);
	if (result == 0 && join != NULL) {
		romLibUpdateContext(&record);
		record.beaconPeriod = join->beaconPeriod;
		record.dtimPeriod = join->dtimPeriod;
		if (join->ssParamSet[0] == 4) {
			record.cfpCount = join->ssParamSet[2];
			record.cfpPeriod = join->ssParamSet[3];
			record.cfpMaxDuration = (join->ssParamSet[5] << 8) | join->ssParamSet[4];
		} else if (join->ssParamSet[0] == 6) {
			record.atimWindow = (join->ssParamSet[3] << 8) | join->ssParamSet[2];
		}
		record.next[1] = 0;
		record.next[0] = join->beaconPeriod << 11;
		return wlanFrameRecordHook(&record);
	}
	return result;
}

/* Starts (zero: stops) the IBSS's timers in the ROM; the mask is bit 25. */
void wlanIbssTimersControl(int start)
{
	romLibComputeContext(0x2000000);
	if (start != 0)
		romLibParseContext(0x2000000);
	else
		romLibBuildContext(0x2000000);
}

/*
 * The channel width an HT operation element's flags select: 0 and 3 go as
 * they are (a 20 MHz secondary offset 0 or 1), 1 as 2; 2 is left alone.
 */
int wlanChannelWidthSelect(const uint8_t *config, uint8_t *width)
{
	int result = (config[0] >> 4) & 3;

	if (result != 0) {
		if (result == 1)
			result = 2;
		else if (result != 3)
			return result;
	} else {
		result = ((config[0] >> 2) & 3) != 1;
	}
	*width = result;
	return result;
}

/* The 802.11 header of a management frame: the subtype, power management and the three addresses. */
void wlanBuildMgmtHeader(WlanWirelessFrame *frame, const uint8_t *source, const uint8_t *destination,
			 const uint8_t *bssid, unsigned int subtype, unsigned int powerManagement)
{
	uint8_t *header = (uint8_t *)frame;

	if (wlanBuildMgmtHeaderOverride(frame, source, destination, bssid, subtype, powerManagement) != 0)
		return;
	if (frame == NULL || source == NULL || destination == NULL || bssid == NULL)
		return;
	__aeabi_memclr(frame, 32);
	/* frame control: protocol version 0, type 0 (management), the subtype */
	header[2] &= ~0x03;
	header[2] &= ~0x0c;
	header[2] = (header[2] & 0x0f) | (subtype << 4);
	header[3] = (header[3] & ~0x10) | ((powerManagement << 4) & 0x10);	/* the power management bit */
	__aeabi_memcpy(header + 6, destination, 6);
	__aeabi_memcpy(header + 12, source, 6);
	__aeabi_memcpy(header + 18, bssid, 6);
}

/*
 * Builds the message of an event for the connection's state machine; 0 for an
 * event that needs none (it is handled without one).
 */
int wlanEventMessageBuild(struct WlanSmMessage *message, int code, const void *data)
{
	int result = 1;

	if (wlanEventMessageBuildOverride(message, code, data, &result) != 0)
		return result;
	if (message == NULL)
		return 0;
	__aeabi_memclr4(message, sizeof(*message));
	message->event = code;
	switch (code) {
	case 7:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
		break;
	case 8:
	case 9:
		__aeabi_memcpy(message->value, data, 8);
		break;
	case 10:
	case 12:
		message->value[0] = *(const uint8_t *)data;
		break;
	case 11:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 27:
	default:
		result = 0;
		break;
	}
	return result;
}

/* The timer's current time (64 bits). */
uint64_t wlanSignalTimeNow(void)
{
	return romGetTimerTimestamp();
}

/* Enables the MAC entry of a table (the value 3). */
int wlanMacEntryEnable(unsigned int index)
{
	return romMacConfigureEntry(index, 3);
}
