/*
 * The results of a scan (0x4b538..0x4bb32): the BSS descriptors a scan keeps
 * for the host, one record each, ranked by RSSI or by SNR as the scan's
 * policy says; the SSID filter, with Marvell's wildcard SSIDs; and the
 * SSIDs the host's scan TLVs give.
 *
 * The results structure is packed: its words are read and written with
 * __aeabi_uread4() and __aeabi_uwrite4(). Marvell published no source; names
 * are inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/ie.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"

/* ROM-patch slots of this file (0x55c30..0x55c48). */
static ROBIN_PATCH_SLOT(fmCoexNotifyEntryOverride);	/* 0x55c30 */
static ROBIN_PATCH_SLOT(fmCoexInspectEntryOverride);	/* 0x55c34 */
static ROBIN_PATCH_SLOT(fmCoexSetupEntryOverride);	/* 0x55c38 */
static ROBIN_PATCH_SLOT(fmCoexFormatEntryOverride);	/* 0x55c3c */
static ROBIN_PATCH_SLOT(fmCoexAcquireEntryOverride);	/* 0x55c40 */
static __attribute__((used)) ROBIN_PATCH_SLOT(scanResultSpareOverride);	/* 0x55c44: nothing calls it */
static ROBIN_PATCH_SLOT(fmCoexConvertEntryOverride);	/* 0x55c48 */

const uint8_t wlanScanAnyBssid[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* The wildcards of an SSID with the maximum length 255. */
static const uint8_t wlanSsidStarChar[4] = "*";		/* 0x4b92c */
static const uint8_t wlanSsidAnyChar[4] = "?";		/* 0x4b930 */

#define SCAN_LE16(p)	((unsigned int)((p)[1] << 8 | (p)[0]))

static struct WlanScanDescriptor *scanRecordDescriptor(const struct WlanScanRecord *record)
{
	return (struct WlanScanDescriptor *)__aeabi_uread4(record->descriptor);
}

/* Removes record index and its descriptor, moving the ones after it down. */
void fmCoexNotifyEntry(struct WlanScanResults *results, unsigned int index)
{
	struct WlanScanRecord *record;
	uint8_t *descriptor;
	unsigned int size;
	unsigned int length;
	unsigned int i;

	if (fmCoexNotifyEntryOverride(results, index) != 0 || results == NULL)
		return;
	record = &results->records[index];
	descriptor = (uint8_t *)__aeabi_uread4(record->descriptor);
	size = SCAN_LE16(descriptor) + 2;
	if (results->count != index + 1) {
		length = __aeabi_uread4(&results->length) -
			 (descriptor - (uint8_t *)__aeabi_uread4(results->records[0].descriptor)) - size;
		__aeabi_memmove(descriptor, (const void *)__aeabi_uread4(record[1].descriptor), length);
		for (i = index; (int)i < results->count - 1; i++) {
			__aeabi_memcpy(&results->records[i], &results->records[i + 1], sizeof(struct WlanScanRecord));
			__aeabi_uwrite4(__aeabi_uread4(results->records[i].descriptor) - size,
					results->records[i].descriptor);
		}
	}
	__aeabi_memclr(&results->records[results->count - 1], sizeof(struct WlanScanRecord));
	results->count--;
	__aeabi_memclr((uint8_t *)__aeabi_uread4(&results->data) + __aeabi_uread4(&results->length) - size, size);
	__aeabi_uwrite4(__aeabi_uread4(&results->length) - size, &results->length);
}

/*
 * Compares string with pattern, where anyChar matches one character and
 * starChar (when given) any run of them: 0 when they match, -1 when not.
 */
int fmCoexFormatEntry(const uint8_t *pattern, const uint8_t *string, const uint8_t *anyChar,
		      const uint8_t *starChar, int patternLength, int stringLength)
{
	int result;
	int i;
	int j;
	unsigned int c;

	if (fmCoexFormatEntryOverride(pattern, string, anyChar, starChar, patternLength, stringLength,
				      &result) != 0)
		return result;
	i = 0;
	j = 0;
	while (i < patternLength || j < stringLength) {
		if (starChar != NULL && pattern[i] == *starChar) {
			while ((pattern[i] == *starChar || pattern[i] == *anyChar) && i < patternLength)
				i++;
			if (i == patternLength)
				return 0;
			c = pattern[i];
			while (string[j] != c && j < stringLength)
				j++;
			if (j == stringLength)
				return -1;
		}
		c = *anyChar;
		while (pattern[i] == c && i < patternLength) {
			j++;
			i++;
			if (j >= stringLength && i < patternLength)
				return -1;
		}
		if (j == stringLength && i == patternLength)
			return 0;
		if (string[j] != pattern[i])
			return -1;
		j++;
		i++;
	}
	if (i == patternLength && j == stringLength)
		return 0;
	return -1;
}

/* Sets *found when the frame's SSID is one the scan looks for (or the scan looks for none). */
void fmCoexInspectEntry(const WlanWirelessFrame *frame, const struct WlanScan *scan, int *found)
{
	const uint8_t *element;
	const uint8_t *ssid;
	const uint8_t *anyChar;
	const uint8_t *starChar;
	uint8_t maxLength;
	unsigned int length;
	unsigned int compared;
	int match = 0;
	uint8_t i;

	if (fmCoexInspectEntryOverride(frame, scan, found) != 0 || frame == NULL || scan == NULL ||
	    found == NULL)
		return;
	if (scan->ssidCount == 0) {
		match = 1;
	} else {
		/* the SSID element, after the timestamp, the beacon interval and the capabilities */
		element = frame->body + 12;
		for (i = 0; i < scan->ssidCount && match == 0; i++) {
			ssid = scan->ssids[i].ssid;
			maxLength = scan->ssids[i].maxLength;
			length = romStringLength(ssid, 32);
			if (length == 0) {
				if (maxLength != 0 && element[1] <= maxLength)
					match = 1;
				continue;
			}
			if (maxLength == 0) {
				if (element[1] != length || length > 32)
					continue;
				compared = length;
			} else if (maxLength <= 32 && length <= 32) {
				compared = maxLength < length ? maxLength : length;
				if (element[1] > maxLength)
					continue;
			} else {
				if (maxLength == 255) {
					anyChar = wlanSsidAnyChar;
					starChar = wlanSsidStarChar;
				} else if (element[1] == length) {
					anyChar = &maxLength;
					starChar = NULL;
				} else {
					continue;
				}
				if (fmCoexFormatEntry(ssid, element + 2, anyChar, starChar, length, element[1]) == 0)
					match = 1;
				continue;
			}
			if (memcmp(element + 2, ssid, compared) == 0)
				match = 1;
		}
	}
	*found = match;
}

/* The value a record is ranked by: its SNR (mode 1) or its RSSI (mode 0). */
__attribute__((noinline)) unsigned int fmCoexTrackEntry(const struct WlanScanRecord *record, uint32_t mode)
{
	if (mode != 0)
		return record->snr;
	return scanRecordDescriptor(record)->rssi;
}

/* Whether value is at least as good as threshold: a higher SNR, a lower RSSI magnitude. */
__attribute__((noinline)) int fmCoexManageEntry(unsigned int value, unsigned int threshold, uint32_t mode)
{
	if (mode != 0) {
		if (value >= threshold)
			return 1;
	} else if (value <= threshold) {
		return 1;
	}
	return 0;
}

/* The index of the best record (best 1) or of the worst (best 0). */
__attribute__((noinline)) unsigned int fmCoexAcquireEntry(const struct WlanScanResults *results, int best,
							  uint32_t mode)
{
	unsigned int found;
	uint8_t kept;
	uint8_t value;
	unsigned int left;
	unsigned int right;
	int i;

	if (fmCoexAcquireEntryOverride(results, best, mode, &found) != 0)
		return found;
	found = 0;
	kept = fmCoexTrackEntry(&results->records[0], mode);
	for (i = 1; i < results->count; i++) {
		value = fmCoexTrackEntry(&results->records[i], mode);
		if (best != 0) {
			left = value;
			right = kept;
		} else {
			left = kept;
			right = value;
		}
		if (fmCoexManageEntry(left, right, mode) != 0) {
			kept = value;
			found = i;
		}
	}
	return found;
}

__attribute__((noinline)) unsigned int fmCoexReleaseEntry(const struct WlanScanResults *results, uint32_t mode)
{
	return fmCoexAcquireEntry(results, 0, mode);
}

/*
 * Keeps what a beacon or probe response the scan received tells: updates the
 * record of its BSS, or adds a descriptor for it, dropping the worst records
 * while the results are full and worse than it.
 */
void fmCoexSetupEntry(WlanWirelessFrame *frame, uintptr_t argument, struct WlanScan *scan,
		      unsigned int channelIndex, unsigned int snr, unsigned int rssi)
{
	struct WlanScanResults *results;
	struct WlanScanChannel *channel;
	struct WlanScanRecord *record;
	uint8_t *descriptor;
	uint8_t *end;
	uint8_t *ds;
	uint64_t timestamp;
	unsigned int addedChannel;
	unsigned int band;
	unsigned int number;
	unsigned int value;
	unsigned int length;
	unsigned int size;
	unsigned int count;
	unsigned int worst;
	unsigned int i;

	if (fmCoexSetupEntryOverride(frame, argument, scan, channelIndex, snr, rssi) != 0)
		return;
	timestamp = romGetTimerTimestamp();
	addedChannel = 0;
	results = scan->results;
	channel = &scan->channels[channelIndex];
	band = channel->channelFlags;
	number = channel->channel;
	if (SCAN_LE16(frame->bodyLength) == 0)
		return;
	value = scan->policy != 0 ? snr : rssi;
	ds = wlanFindIe(frame, IE_DS_PARAMETER, WLAN_FRAME_BEACON, NULL, 0);
	for (i = 0; i < results->count; i++) {
		record = &results->records[i];
		if (memcmp((uint8_t *)__aeabi_uread4(record->descriptor) + 2, frame->address3, 6) == 0) {
			if ((ds != NULL && ds[2] == number) ||
			    fmCoexManageEntry(value, fmCoexTrackEntry(record, scan->policy), scan->policy) != 0) {
				scanRecordDescriptor(record)->rssi = rssi;
				record->snr = snr;
				__aeabi_memcpy(record->timestamp, &timestamp, 8);
			}
			return;
		}
	}
	length = SCAN_LE16(frame->bodyLength);
	size = (uint16_t)(length + 9);
	if (ds == NULL) {
		size = (uint16_t)(length + 12);
		addedChannel = number;
	} else if (ds[2] != number && (channel->flags & 2) == 0) {
		return;
	}
	for (;;) {
		count = results->count;
		if (count < results->capacity &&
		    __aeabi_uread4(&results->length) + size <= __aeabi_uread4(&results->size))
			break;
		if (count == 0)
			return;
		worst = (uint8_t)fmCoexReleaseEntry(results, scan->policy);
		if (fmCoexManageEntry(fmCoexTrackEntry(&results->records[worst], scan->policy), value,
				      scan->policy) != 0)
			return;
		fmCoexNotifyEntry(results, worst);
	}
	descriptor = (uint8_t *)__aeabi_uread4(&results->data) + __aeabi_uread4(&results->length);
	descriptor[0] = (uint8_t)(size - 2);		/* the records are packed: any alignment */
	descriptor[1] = (uint8_t)((size - 2) >> 8);
	__aeabi_memcpy(descriptor + 2, frame->address3, 6);
	descriptor[8] = rssi;
	__aeabi_memcpy(descriptor + 9, frame->body, length);
	end = descriptor + 9 + length;
	if (addedChannel != 0) {
		end[0] = IE_DS_PARAMETER;
		end[1] = 1;
		end[2] = addedChannel;
	}
	record = &results->records[results->count];
	__aeabi_uwrite4((int)(uintptr_t)descriptor, record->descriptor);
	__aeabi_memcpy(record->timestamp, &timestamp, 8);
	record->snr = snr;
	record->channelBand[0] = band;
	record->channelBand[1] = number;
	results->count++;
	__aeabi_uwrite4(__aeabi_uread4(&results->length) + size, &results->length);
}

/* Whether a frame from bssid is one the scan wants: a real BSSID, and the scan's or any. */
int fmCoexClearEntry(const uint8_t *bssid, const uint8_t *wanted)
{
	if (bssid == NULL || wanted == NULL || memcmp(wlanScanAnyBssid, bssid, 6) == 0)
		return 0;
	return memcmp(wanted, wlanScanAnyBssid, 6) == 0 || memcmp(wanted, bssid, 6) == 0;
}

/*
 * Adds the SSID of an SSID TLV (type 0) or a wildcard SSID TLV (type 274) to
 * the list at ssid while *count is below maximum; returns the TLV's size.
 */
unsigned int fmCoexConvertEntry(unsigned int type, const uint8_t *tlv, uint8_t *ssid, uint8_t *count,
				unsigned int maximum)
{
	uint8_t buffer[32];
	uint32_t size;
	uint8_t maxLength = 0;
	unsigned int length;
	unsigned int copied;

	if (fmCoexConvertEntryOverride(type, tlv, ssid, count, maximum, &size) != 0)
		return (uint16_t)size;
	__aeabi_memclr4(buffer, sizeof(buffer));
	if (type == 0) {
		size = wlanTlvCopyOfType(0, tlv, buffer, sizeof(buffer));
	} else if (type == 274) {
		size = SCAN_LE16(tlv + 2) + 4;
		length = SCAN_LE16(tlv + 2);
		copied = 32;
		if (length - 1 <= 32)
			copied = length - 1;
		strncpy((char *)buffer, (const char *)tlv + 5, copied);
		maxLength = tlv[4];
	} else {
		size = 0;
	}
	if (*count < maximum && (uint16_t)size > 4) {
		__aeabi_memcpy(ssid + 1, buffer, sizeof(buffer));
		ssid[0] = maxLength;
		(*count)++;
	}
	return (uint16_t)size;
}

unsigned int fmCoexVerifyEntry(const struct WlanScanResults *results, uint32_t mode)
{
	return fmCoexAcquireEntry(results, 1, mode);
}
