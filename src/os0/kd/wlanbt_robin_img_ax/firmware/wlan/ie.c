/*
 * 802.11 information element (TLV) helpers for management frame bodies.
 * The filename is inferred; no source filename survives.
 */
#include "robin/ie.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/threadx.h"

/* Set by the management code to the DMA copy routines. */
WlanMoveFn wlanIeMoveForward __attribute__((section(".itcm.rw"))) = NULL;
WlanMoveFn wlanIeMoveBackward __attribute__((section(".itcm.rw"))) = NULL;
uint16_t wlanIeLimitProbe __attribute__((section(".itcm.rw"))) = 0;
uint16_t wlanIeLimitBeacon __attribute__((section(".itcm.rw"))) = 0;

static ROBIN_PATCH_SLOT(wlanFindIeOverride);
static ROBIN_PATCH_SLOT(wlanSetIeOverride);
static ROBIN_PATCH_SLOT(wlanInsertIeOverride);
static ROBIN_PATCH_SLOT(wlanCopyIesOverride);
static ROBIN_PATCH_SLOT(wlanAppendIeOverride);
static ROBIN_PATCH_SLOT(wlanAppendIeListOverride);
static ROBIN_PATCH_SLOT(wlanRemoveIeOverride);
static ROBIN_PATCH_SLOT(wlanRemoveIeListOverride);
static ROBIN_PATCH_SLOT(wlanClearBodyOverride);
static ROBIN_PATCH_SLOT(wlanRateBitmapBasicOverride);
static ROBIN_PATCH_SLOT(wlanRateBitmapOverride);
static ROBIN_PATCH_SLOT(wlanTimeDeltaOverride);

static unsigned int bodyLengthOf(const WlanWirelessFrame *frame)
{
	return frame->bodyLength[0] | (frame->bodyLength[1] << 8);
}

static void addBodyLength(WlanWirelessFrame *frame, int delta)
{
	unsigned int length = bodyLengthOf(frame) + delta;

	frame->bodyLength[0] = length;
	frame->bodyLength[1] = length >> 8;
}

/*
 * Finds the element with the given id, and with the given leading payload bytes
 * when match is set, in a beacon, probe request or probe response body.
 */
uint8_t *wlanFindIe(WlanWirelessFrame *frame, int ieId, int frameKind,
		    const void *match, unsigned int matchLength)
{
	uint8_t *ie;
	unsigned int length;
	unsigned int offset;
	int result = 0;

	if (wlanFindIeOverride((uintptr_t)frame, ieId, (uintptr_t)&result, 0))
		return (uint8_t *)(uintptr_t)result;
	if (!frame)
		return NULL;
	length = bodyLengthOf(frame);
	if (frameKind == WLAN_FRAME_BEACON || frameKind == WLAN_FRAME_PROBE_RESPONSE) {
		/* Timestamp, beacon interval and capability come first. */
		ie = frame->body + 12;
		offset = 12;
	} else if (frameKind == WLAN_FRAME_PROBE_REQUEST) {
		ie = frame->body;
		offset = 0;
	} else {
		return NULL;
	}
	while (offset < length) {
		if (ie[0] == ieId &&
		    (!match || memcmp(ie + 2, match, matchLength) == 0))
			return ie;
		offset += ie[1] + 2;
		ie += ie[1] + 2;
	}
	return NULL;
}

/*
 * Replaces, appends or (for a zero-length element) removes the element of the
 * same id in a body ending at end, moving the following elements with the DMA
 * copy routines. Reports the old and new element sizes.
 */
void wlanSetIe(WlanWirelessFrame *frame, const uint8_t *ie, uint8_t *end,
	       uint8_t *removed, uint8_t *added)
{
	const void *match = NULL;
	unsigned int matchLength = 0;
	unsigned int id = ie[0];
	unsigned int length = ie[1];
	uint8_t tail;
	uint8_t *position;
	UINT posture;

	if (wlanSetIeOverride((uintptr_t)frame, (uintptr_t)ie, (uintptr_t)end, 0))
		return;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	if (id == IE_VENDOR) {
		match = ie + 2;
		matchLength = 6;
	}
	position = wlanFindIe(frame, id, WLAN_FRAME_BEACON, match, matchLength);
	if (position) {
		*removed = position[1] + 2;
	} else {
		position = end;
		*removed = 0;
		if (length == 0) {
			tx_interrupt_control(posture);
			return;
		}
	}
	tail = end - position - *removed;
	if (length == 0) {
		*added = 0;
		wlanIeMoveForward(position, position + *removed, tail);
	} else {
		*added = length + 2;
		if (*added > *removed)
			wlanIeMoveBackward(position + *added, position + *removed, tail);
		else if (*added != *removed)
			wlanIeMoveForward(position + *added, position + *removed, tail);
		__aeabi_memcpy(position, ie, *added);
	}
	addBodyLength(frame, *added - *removed);
	tx_interrupt_control(posture);
}

/* ERP Information element: returns its size, or 0 without a buffer. */
int wlanPutErpIe(uint8_t *buffer, int nonErpPresent, int useProtection, int barker)
{
	if (!buffer)
		return 0;
	buffer[0] = IE_ERP_INFORMATION;
	buffer[1] = 1;
	buffer[2] = (useProtection << 1) | nonErpPresent | (barker << 2);
	return 3;
}

int wlanPutDsParameterIe(uint8_t *buffer, int channel)
{
	buffer[0] = IE_DS_PARAMETER;
	buffer[1] = 1;
	buffer[2] = channel;
	return 3;
}

unsigned int wlanPutSsidIe(const void *ssid, uint8_t *buffer)
{
	unsigned int length;

	buffer[0] = IE_SSID;
	length = (uint8_t)romStringCopy(buffer + 2, ssid, 32);
	buffer[1] = length;
	return (uint8_t)(length + 2);
}

/*
 * Supported Rates element (first eight rates) or, when extended is set,
 * Extended Supported Rates element (the rest). Returns 0 when there is none.
 */
unsigned int wlanPutRatesIe(const uint8_t *rates, uint8_t *buffer, int extended)
{
	unsigned int count = 0;
	unsigned int rest;
	unsigned int length;

	count = romStringLength(rates, 14);
	rest = 0;
	if (count > 8) {
		rest = (uint8_t)(count - 8);
		count = 8;
	}
	if (!extended) {
		buffer[0] = IE_SUPPORTED_RATES;
		length = (uint8_t)romStringCopy(buffer + 2, rates, count);
	} else {
		if (!rest)
			return 0;
		buffer[0] = IE_EXTENDED_RATES;
		length = (uint8_t)romStringCopy(buffer + 2, rates + count, rest);
	}
	buffer[1] = length;
	return (uint8_t)(length + 2);
}

/*
 * Inserts the element into the list start..end keeping ascending id order
 * (vendor elements by their leading bytes); returns the element size.
 */
unsigned int wlanInsertIeSorted(const uint8_t *ie, uint8_t *start, uint8_t *end)
{
	unsigned int id = ie[0];
	unsigned int length = ie[1];
	unsigned int compare = length < 4 ? length : 4;
	uint8_t *position = start;
	uint8_t *last;
	uint8_t *target;
	int result = 0;

	if (wlanInsertIeOverride((uintptr_t)ie, (uintptr_t)start, (uintptr_t)&result, 0))
		return result;
	while (position < end) {
		if (id == IE_VENDOR && position[0] == IE_VENDOR) {
			unsigned int n = position[1] < compare ? position[1] : compare;

			if (memcmp(ie + 2, position + 2, n) < 0)
				break;
		} else if (position[0] >= id) {
			break;
		}
		position += position[1] + 2;
	}
	if (position < end) {
		target = &end[length + 2];
		for (last = end; last >= position; last--)
			*target-- = *last;
	}
	__aeabi_memcpy(position, ie, length + 2);
	return length + 2;
}

/*
 * Copies the elements of one id from a list into a frame being built: the
 * RSN element once, vendor elements sorted in as long as they fit.
 * Returns the number of bytes written to destination.
 */
unsigned int wlanCopyIes(int ieId, uint8_t *destination, int frameKind,
			 uint8_t *base, int ieOffset, const uint8_t *source,
			 unsigned int sourceLength)
{
	unsigned int written = 0;
	unsigned int limit;
	int room;
	unsigned int length;
	unsigned int n;
	int result = 0;

	if (wlanCopyIesOverride(ieId, (uintptr_t)destination, (uintptr_t)source, (uintptr_t)&result))
		return result;
	if (sourceLength == 0)
		return 0;
	if (frameKind == WLAN_FRAME_PROBE_RESPONSE)
		limit = wlanIeLimitProbe;
	else if (frameKind == WLAN_FRAME_BEACON)
		limit = wlanIeLimitBeacon;
	else
		return 0;
	room = limit - (destination - base);
	if (room <= 0)
		return 0;
	while (sourceLength >= 2) {
		length = source[1];
		if ((int)length > (int)sourceLength - 2)
			break;
		if (source[0] == ieId) {
			if (ieId == IE_RSN) {
				written = length + 2;
				if ((int)written > room)
					return 0;
				__aeabi_memcpy(destination, source, written);
				return written;
			}
			if (ieId == IE_VENDOR) {
				if ((int)(length + 2) > room)
					return written;
				n = (uint16_t)wlanInsertIeSorted(source, base + ieOffset,
								 destination + written);
				written = (uint16_t)(written + n);
				room -= n;
			}
		}
		sourceLength -= length + 2;
		source += length + 2;
	}
	return written;
}

/* Appends one element to the body; the data may be omitted. */
void wlanAppendIe(WlanWirelessFrame *frame, uint8_t id, const void *data, unsigned int length)
{
	uint8_t *position;
	unsigned int size = 2;
	int result = 0;

	if (wlanAppendIeOverride((uintptr_t)frame, id, (uintptr_t)data, (uintptr_t)&result))
		return;
	if (!frame)
		return;
	position = frame->body + bodyLengthOf(frame);
	position[0] = id;
	position[1] = length;
	if (data) {
		__aeabi_memcpy(position + 2, data, length);
		size = (uint8_t)(length + 2);
	}
	addBodyLength(frame, size);
}

/* Inserts every element of a list into the body; returns the bytes added. */
unsigned int wlanAppendIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length)
{
	const uint8_t *end = ies + length;
	uint8_t *start = frame->body + offset;
	unsigned int total = (uintptr_t)ies;
	unsigned int n;

	if (wlanAppendIeListOverride((uintptr_t)frame, offset, (uintptr_t)ies, 0))
		return total;
	total = 0;
	while (ies < end) {
		n = wlanInsertIeSorted(ies, start, frame->body + bodyLengthOf(frame));
		addBodyLength(frame, n);
		ies += n;
		total += n;
	}
	return total;
}

/* Deletes the element equal to ie from start..end; returns the bytes removed. */
unsigned int wlanRemoveIe(const uint8_t *ie, uint8_t *start, uint8_t *end)
{
	unsigned int id = ie[0];
	unsigned int length = ie[1];
	uint8_t *position = start;
	uint8_t *tail;
	unsigned int size;
	int result = 0;

	if (wlanRemoveIeOverride((uintptr_t)ie, (uintptr_t)start, (uintptr_t)end, (uintptr_t)&result))
		return result;
	while (position < end) {
		if (position[0] == id && position[1] == length &&
		    (id != IE_VENDOR || memcmp(position + 2, ie + 2, length) == 0))
			break;
		position += position[1] + 2;
	}
	if (position >= end)
		return 0;
	size = length + 2;
	tail = end - size;
	__aeabi_memcpy(position, position + size, tail - position);
	__aeabi_memclr(tail, size);
	return size;
}

/* Deletes every element of a list from the body; returns the last result. */
unsigned int wlanRemoveIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length)
{
	const uint8_t *end = ies + length;
	uint8_t *start = frame->body + offset;
	unsigned int total = 0;
	unsigned int n;

	if (wlanRemoveIeListOverride((uintptr_t)frame, offset, (uintptr_t)ies, 0))
		return total;
	while (ies < end) {
		n = wlanRemoveIe(ies, start, frame->body + bodyLengthOf(frame));
		addBodyLength(frame, -(int)n);
		total += n;
		ies += ies[1] + 2;
	}
	return total;
}

int wlanClearBodyLength(uint16_t *length)
{
	int result = wlanClearBodyOverride(0, 0, 0, 0);

	if (result == 0 && length)
		*length = 0;
	return result;
}

/* Bit mask of the rate indices listed with the basic-rate flag (0x80) set. */
unsigned int wlanRateBitmapBasic(const uint8_t *rates, unsigned int count)
{
	unsigned int i;
	int bitmap = 0;

	if (wlanRateBitmapBasicOverride((uintptr_t)rates, count, (uintptr_t)&bitmap, 0))
		return bitmap;
	for (i = 0; i < count; i = (uint8_t)(i + 1)) {
		if ((rates[i] & 0x80) && (rates[i] & 0x7f))
			bitmap |= 1 << romRateIndex(rates[i] & 0x7f);
	}
	return bitmap;
}

/* Bit mask of the rate indices of all listed rates. */
unsigned int wlanRateBitmap(const uint8_t *rates, unsigned int count)
{
	unsigned int i = 0;
	int bitmap = 0;

	if (wlanRateBitmapOverride((uintptr_t)rates, count, (uintptr_t)&bitmap, 0))
		return bitmap;
	while (i < count) {
		if (rates[i] & 0x7f)
			bitmap |= 1 << romRateIndex(rates[i] & 0x7f);
		i = (uint8_t)(i + 1);
	}
	return bitmap;
}

/* Difference of two wrapping counters; the counter wraps at 0xffffffff. */
int wlanTimeDelta(unsigned int now, unsigned int last)
{
	int result = 0;
	int delta;

	if (wlanTimeDeltaOverride(now, last, (uintptr_t)&result, 0))
		return result;
	delta = now - last;
	if (now < last)
		--delta;
	return delta;
}
