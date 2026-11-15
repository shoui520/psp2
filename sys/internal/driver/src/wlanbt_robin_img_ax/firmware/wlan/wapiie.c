/*
 * The WAPI information element of an access point's BSS (0x4ccb0..0x4ced8).
 * Marvell published no source; names are inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/wapi.h"
#include "robin/wapiie.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))

/* The WAPI information element of the BSS (ID 68): its version, the one suite of each kind and no capabilities. Returns its size, 0 when the BSS is not a WAPI one. */
unsigned int fmCoexSetupBuffer(const struct WlanConnectionInfo *info, uint8_t *ie, unsigned int size)
{
	uint32_t oui = 0x721400;
	uint8_t *cursor = ie;

	if (ie != NULL && info->wapi.configured != 0 && size >= 24) {
		__aeabi_memclr(ie, size);
		ie[0] = IE_WAPI_ID;
		ie[1] = 0;
		ie[2] = info->wapi.version;
		ie[4] = 1;
		cursor = ie + 6;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapi.akm;
		cursor++;
		*cursor = 1;
		cursor += 2;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapi.unicastCipher;
		cursor++;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapi.multicastCipher;
		cursor += 3;
		ie[1] = (uint8_t)(cursor - ie - 2);
	}
	return (uint8_t)(cursor - ie);
}

/* Forgets the BSS's WAPI element. */
__attribute__((noinline)) void fmCoexClearBuffer(struct WlanConnectionInfo *info)
{
	__aeabi_memclr(&info->wapi, 6);	/* the settings and the byte after them */
}

/*
 * Reads a WAPI element into settings, field by field: the version, the AKM
 * suite count and suite, the unicast suite count and suite and the multicast
 * suite. Returns 1 when the element is not a WAPI one, a count or version is
 * above 1, a suite type is not 1 or 2, or it ends early; 0 when the multicast
 * suite is reached (its type is kept only when it is 1 or 2).
 */
__attribute__((noinline)) int fmCoexConvertBuffer(struct WlanWapiSettings *settings, const uint8_t *element)
{
	const uint8_t *cursor;
	int16_t remaining;
	unsigned int used = 0;
	int invalid;
	uint8_t field;

	if (element[0] != IE_WAPI_ID)
		return 1;
	invalid = 0;
	remaining = element[1];
	cursor = element + 2;
	for (field = 0;; field++) {
		switch (field) {
		case 0:
			used = 2;
			break;
		case 1:
			settings->version = cursor[0];
			if (settings->version > 1)
				invalid = 1;
			cursor += 2;
			used = 2;
			break;
		case 2:
		case 4:
			if (cursor[0] > 1)
				invalid = 1;
			cursor += 2;
			used = 4;
			break;
		case 3:
			cursor += 3;
			if (cursor[0] == 1 || cursor[0] == 2)
				settings->akm = cursor[0];
			else
				invalid = 1;
			cursor++;
			used = 2;
			break;
		case 5:
			cursor += 3;
			if (cursor[0] == 1 || cursor[0] == 2)
				settings->unicastCipher = cursor[0];
			else
				invalid = 1;
			cursor++;
			used = 4;
			break;
		case 6:
			if (cursor[3] == 1 || cursor[3] == 2)
				settings->multicastCipher = cursor[3];
			return 0;
		default:
			break;
		}
		remaining = (int16_t)(remaining - used);
		if (invalid != 0 || remaining <= 0)
			return 1;
	}
}

/* Takes the WAPI element the host gives for the BSS; returns 1 when it is not valid. */
int fmCoexVerifyBuffer(struct WlanConnectionInfo *info, const uint8_t *element)
{
	int result = fmCoexConvertBuffer(&info->wapi, element);

	if (result == 1)
		fmCoexClearBuffer(info);
	else
		info->wapi.configured = 1;
	return result;
}

unsigned int fmCoexSelectBuffer(const struct WlanConnectionInfo *info, uint8_t *ie)
{
	if (info->securityMode.wapi)
		return fmCoexSetupBuffer(info, ie, 0xff);
	return 0;
}

/* The WAPI context goes to the host as TLV 103 (byte-reversed); a BSS without WAPI adds nothing. Returns the TLV's size. */
unsigned int fmCoexAdjustBuffer(const struct WlanConnectionInfo *info, uint8_t *tlv)
{
	uint8_t data[16];

	if (!info->securityMode.wapi)
		return 0;
	tlv[0] = 103;
	tlv[1] = 1;
	tlv[2] = 16;
	tlv[3] = 0;
	if (info->wapiContext != NULL) {
		wlanReverseBytes(data, info->wapiContext->data, 16);
		__aeabi_memcpy(tlv + 4, data, 16);
	} else {
		__aeabi_memclr(tlv + 4, 16);
	}
	return U16(tlv + 2) + 4;
}


/*
 * Checks a station's WAPI element against the BSS's: 1 and status 0 when it
 * fits, else 0 with the association status to refuse it with.
 */
int fmCoexRecordBuffer(const struct WlanConnectionInfo *info, const uint8_t *element, uint16_t *status)
{
	struct WlanWapiSettings station;

	if (element == NULL || fmCoexConvertBuffer(&station, element) == 1) {
		*status = WAPI_STATUS_INVALID_ELEMENT;
		return 0;
	}
	if (station.version > info->wapi.version) {
		*status = WAPI_STATUS_VERSION;
		return 0;
	}
	if (info->wapi.akm != station.akm) {
		*status = WAPI_STATUS_INVALID_AKMP;
		return 0;
	}
	if (station.unicastCipher != info->wapi.unicastCipher) {
		*status = WAPI_STATUS_UNICAST_CIPHER;
		return 0;
	}
	if (station.multicastCipher != info->wapi.multicastCipher) {
		*status = WAPI_STATUS_MULTICAST_CIPHER;
		return 0;
	}
	*status = 0;
	return 1;
}
