/*
 * The host's ad-hoc start and join commands and their TLVs: the responses
 * the firmware builds, the parameters taken from the command, the elements
 * the TLVs become and the WEP default keys. The filename is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/buffer.h"
#include "robin/ie.h"
#include "robin/join.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/wep.h"

#define HostCmd_RET_802_11_AD_HOC_START	0x802b
#define HostCmd_RET_8040		0x8040

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))

/* The header of a response the firmware builds in a pool buffer (8 bytes, the host's HostCmd_DS_GEN). */
struct WlanHostResponse {
	uint8_t command[2];
	uint8_t size[2];
	uint8_t sequence;		/* the high byte is left as it was */
	uint8_t reserved5;
	uint8_t result[2];
	uint8_t status;			/* the first byte after the header */
};

/* The MAC's address register (the station's address, 48 bits); the ad-hoc start response reports it as the BSSID. */
#define MAC_ADDRESS_LOW		(*(volatile uint32_t *)0x8000a530)
#define MAC_ADDRESS_HIGH	(*(volatile uint32_t *)0x8000a534)


/* The power limits of a Power Capability element, set by the host (0x54e70). */
struct WlanPowerCapability {
	uint32_t present;
	uint8_t minimum;
	uint8_t maximum;
};

extern struct WlanPowerCapability *wlanPowerCapability;				/* 0x54e70 */
unsigned int wlanTlvPowerCapabilityParse(const uint8_t *tlv, struct WlanPowerCapability *capability); /* 0x2c5b4 */

/*
 * Override slots of the TLV parsers (0x55bf0..0x55c00): each gets the arguments of its
 * parser and a pointer to the length it returns, and returns nonzero when it handled the TLV.
 */
static ROBIN_PATCH_SLOT(wlanTlvChannelListOverride);		/* 0x55bf0 */
static ROBIN_PATCH_SLOT(wlanTlvThresholdOverride);		/* 0x55bf4 */
ROBIN_PATCH_SLOT(wlanTlvOverride55bf8);				/* 0x55bf8, used by no function of this file */
static ROBIN_PATCH_SLOT(wlanTlvNumProbesOverride);		/* 0x55bfc */
static ROBIN_PATCH_SLOT(wlanTlvPowerCapabilityOverride);	/* 0x55c00 */

/* The ad-hoc start response carries the station's own address as the BSSID. */
void wlanAdHocResponseBssid(uint8_t *response, const uint8_t *command)
{
	uint32_t address[2];

	if (LE16(command) == HostCmd_RET_802_11_AD_HOC_START) {
		address[0] = MAC_ADDRESS_LOW;
		address[1] = MAC_ADDRESS_HIGH;
		__aeabi_memcpy(response + 11, address, 6);
	}
}

/* Starts a response to a command in a buffer: the code, its size, the sequence and the status. */
void wlanHostResponseBuild(OsaBuffer *buffer, unsigned int command, unsigned int status,
			   unsigned int sequence)
{
	struct WlanHostResponse *response =
		(struct WlanHostResponse *)((uint8_t *)buffer + buffer->dataOffset);

	response->command[0] = command;
	response->command[1] = command >> 8;
	response->result[0] = 0;
	response->result[1] = 0;
	response->sequence = sequence;
	response->size[0] = 8;
	response->size[1] = 0;
	if (command == HostCmd_RET_802_11_AD_HOC_START) {
		response->size[0] = 20;
		response->size[1] = 0;
		((uint8_t *)response)[19] = status;
	} else {
		response->size[0] = 9;
		response->size[1] = 0;
		response->status = status;
	}
}

/*
 * Takes the parameters of an AD_HOC_START command: the fixed part (69 bytes
 * after the header), the default rates when it has none and the length of
 * the TLVs that follow.
 */
unsigned int wlanAdHocStartParse(WlanAdHocStart *start, uint8_t *command, const uint8_t *defaultRates,
				 uint16_t *tlvLength, uint16_t defaultBeaconPeriod)
{
	unsigned int i;
	unsigned int length;

	__aeabi_memclr(start, sizeof(*start));
	command[61] |= 0x20;			/* capability: short preamble */
	__aeabi_memcpy(start, command + 8, 69);
	if (start->beaconPeriod == 0)
		start->beaconPeriod = defaultBeaconPeriod;
	if (start->dataRate[0] == 0) {
		for (i = 0; i < 14; i++)
			start->dataRate[i] = defaultRates[i];
	}
	length = LE16(command + 2) - 69;
	*tlvLength = length;
	return length;
}

/* The same for an AD_HOC_JOIN command (93 bytes after the header). */
unsigned int wlanAdHocJoinParse(WlanAdHocBssDesc *join, uint8_t *command, uint16_t *tlvLength)
{
	unsigned int length;

	__aeabi_memclr(join, sizeof(*join));
	command[81] |= 0x20;			/* capability: short preamble */
	__aeabi_memcpy(join, command + 8, 93);
	length = LE16(command + 2) - 93;
	*tlvLength = length;
	return length;
}

/* The response of command 0x8040 carries a result of its own. */
void wlanHostResponse8040Build(OsaBuffer *buffer, unsigned int result)
{
	struct WlanHostResponse *response =
		(struct WlanHostResponse *)((uint8_t *)buffer + buffer->dataOffset);

	response->command[0] = HostCmd_RET_8040 & 0xff;
	response->command[1] = HostCmd_RET_8040 >> 8;
	response->result[0] = result;
	response->result[1] = result >> 8;
	response->size[0] = 8;
	response->size[1] = 0;
}

/*
 * A TLV of the HT elements becomes the 802.11 element it names: the TLV's
 * type is replaced by the element's ID, the value is kept in elements and the
 * TLV goes on into the buffer of extra elements. Returns 0 for another type.
 */
int wlanHtTlvApply(struct WlanHtTlvElements *elements, struct WlanIeBuffer *ies, uint8_t *tlv)
{
	unsigned int type = LE16(tlv);
	uint8_t *element;
	unsigned int length;

	switch (type) {
	case TLV_TYPE_HT_CAPABILITY:
		tlv[2] = 26;
		tlv[3] = 0;
		tlv[0] = IE_HT_CAPABILITY;
		tlv[1] = 0;
		/* fall through */
	case TLV_TYPE_HT_CAPABILITY_IE:
		element = elements->capability;
		element[0] = IE_HT_CAPABILITY;
		element[1] = 26;
		length = 26;
		break;
	case TLV_TYPE_HT_INFORMATION:
		tlv[0] = IE_HT_INFORMATION;
		tlv[1] = 0;
		/* fall through */
	case TLV_TYPE_HT_INFORMATION_IE:
		element = elements->information;
		element[0] = IE_HT_INFORMATION;
		element[1] = 22;
		length = 22;
		break;
	case TLV_TYPE_2040BSS_COEX:
		tlv[0] = IE_20_40_BSS_COEXISTENCE;
		tlv[1] = 0;
		/* fall through */
	case TLV_TYPE_2040BSS_COEX_IE:
		element = elements->coexistence;
		element[0] = IE_20_40_BSS_COEXISTENCE;
		element[1] = 1;
		length = 1;
		break;
	case TLV_TYPE_OBSS_SCAN_PARAM:
		tlv[0] = IE_OBSS_SCAN_PARAMETERS;
		tlv[1] = 0;
		/* fall through */
	case TLV_TYPE_OBSS_SCAN_PARAM_IE:
		element = elements->obssScan;
		element[0] = IE_OBSS_SCAN_PARAMETERS;
		element[1] = 14;
		length = 14;
		break;
	default:
		return 0;
	}
	__aeabi_memcpy(element + 2, tlv + 4, length);
	wlanIeBufferAppendTlv(ies, tlv);
	return 1;
}

/*
 * The remaining TLVs of the commands: the power capability becomes an
 * element, the channel list is read and the pass-through elements are kept.
 */
int wlanHostTlvResolve(struct WlanIeBuffer *ies, uint8_t *tlv, uint8_t *channels)
{
	unsigned int type = LE16(tlv);

	if (type == TLV_TYPE_POWER_CAPABILITY_IE) {
		wlanTlvPowerCapabilityParse(tlv, wlanPowerCapability);
		wlanIeBufferAppendTlv(ies, tlv);
		return 1;
	}
	if (type == TLV_TYPE_CHANLIST) {
		wlanTlvChannelListParse(tlv, channels, NULL);
		return 1;
	}
	if (type == TLV_TYPE_PASSTHROUGH) {
		wlanIeBufferAppend(ies, tlv + 4, LE16(tlv + 2));
		return 1;
	}
	return 0;
}

/*
 * Stores a default key of the host's key command (the TLV's value: the key
 * index, whether it is the one to send with and the key itself). The
 * original tests the table for NULL after using it.
 */
void wlanWepHostKeySet(WlanWepHostConfig *config, WlanWepHostKeys *keys, const uint8_t *tlv,
		       int keyType)
{
	unsigned int index = tlv[0];
	WlanWepHostKey *entry = &keys->key[index];

	config->flags |= 1;
	entry->index = index;
	if (keyType == 2) {
		entry->isWep104 = 1;
		config->keyLength = 13;
		if (config != NULL)
			config->keyFlags |= 8;
	} else {
		entry->isWep104 = 0;
		config->keyLength = 5;
		if (config != NULL)
			config->keyFlags &= ~8;
	}
	__aeabi_memcpy(entry->key, tlv + 2, config->keyLength);
	if (tlv[1] != 0)
		keys->defaultKey = index;
}

/* Which of the host's event masks (by bit) an event code belongs to; 0 for none. */
int wlanEventMaskIndex(int event)
{
	switch (event) {
	case 2:
	case 3:
		return 1;
	case 6:
		return 4;
	case 15:
		return 8;
	case 16:
		return 9;
	default:
		return 0;
	}
}

/* The TLV behind tlv, if it has the type (the value of tlv has a length of its own); NULL otherwise. */
const uint8_t *wlanTlvNextOfType(unsigned int type, const uint8_t *tlv)
{
	unsigned int length = LE16(tlv + 2);
	const uint8_t *next;

	if (length == 0)
		return NULL;
	next = tlv + (uint16_t)(length + 4);
	if (LE16(next) != type)
		return NULL;
	return next;
}

/* Copies the value of a TLV of the type, at most maximum bytes of it; returns the TLV's size, 0 for another type or a value too long. */
unsigned int wlanTlvCopyOfType(unsigned int type, const uint8_t *tlv, void *destination,
			       unsigned int maximum)
{
	unsigned int length;

	if (LE16(tlv) != type)
		return 0;
	length = LE16(tlv + 2);
	if (length > maximum)
		return 0;
	if (length == 0)
		return 4;
	__aeabi_memcpy(destination, tlv + 4, (uint8_t)tlv[2]);
	return (uint16_t)(LE16(tlv + 2) + 4);
}

/* TLV_TYPE_CHANLIST: the channel entries (7 bytes each, Marvell's ChanScanParamSet_t); a single one sets *channel too. */
unsigned int wlanTlvChannelListParse(const uint8_t *tlv, uint8_t *channels, uint32_t *channel)
{
	unsigned int count;

	if (wlanTlvChannelListOverride(tlv, channels, channel, &count) != 0)
		return (uint16_t)count;
	if (LE16(tlv) != TLV_TYPE_CHANLIST)
		return 0;
	if (LE16(tlv + 2) == 0)
		return 4;
	count = LE16(tlv + 2) / 7;
	__aeabi_memcpy(channels, tlv + 4, 7 * count);
	if ((uint16_t)count == 1 && channel != NULL)
		*channel = channels[1];
	return (uint16_t)(LE16(tlv + 2) + 4);
}

/* TLV_TYPE_NUMPROBES: the number of probes of a scan (16 bits) into two bytes, 1 for zero. */
unsigned int wlanTlvNumProbesParse(const uint8_t *tlv, uint8_t *numProbes)
{
	unsigned int count = 0;
	unsigned int probes;

	if (wlanTlvNumProbesOverride(tlv, numProbes, &count) != 0)
		return (uint16_t)count;
	if (LE16(tlv) != TLV_TYPE_NUMPROBES)
		return 0;
	if (LE16(tlv + 2) == 2) {
		count = tlv[4];
		probes = count | (tlv[5] << 8);
		count = probes;
		if (probes != 0) {
			numProbes[0] = probes;
			numProbes[1] = probes >> 8;
		} else {
			numProbes[0] = 1;
			numProbes[1] = 0;
		}
	}
	return (uint16_t)(LE16(tlv + 2) + 4);
}

/* An empty TLV of type 0xffff into out; returns its size. */
unsigned int wlanTlvTerminate(uint8_t *out)
{
	out[0] = 0xff;
	out[1] = 0xff;
	out[2] = 0;
	out[3] = 0;
	return 4;
}

/* TLV_TYPE_POWER_CAPABILITY_IE: the minimum and maximum power a Power Capability element gives; an empty one clears them. */
unsigned int wlanTlvPowerCapabilityParse(const uint8_t *tlv, struct WlanPowerCapability *capability)
{
	unsigned int length;

	if (wlanTlvPowerCapabilityOverride(tlv, capability, &length) != 0)
		return (uint16_t)length;
	if (LE16(tlv) != TLV_TYPE_POWER_CAPABILITY_IE)
		return 0;
	if (LE16(tlv + 2) == 0) {
		capability->present = 0;
		capability->minimum = 0;
		capability->maximum = 0;
		capability->present = 0;
		return 4;
	}
	capability->minimum = tlv[4];
	capability->maximum = tlv[5];
	capability->present = 1;
	return (uint16_t)(LE16(tlv + 2) + 4);
}

/*
 * The threshold and frequency of an RSSI or SNR TLV (Marvell's MrvlIEtypes_RssiParamSet_t, a
 * value byte and a frequency byte) into out; *frequency gets the frequency, 1 for zero.
 */
unsigned int wlanTlvParseThreshold(const uint8_t *tlv, uint8_t *out, uint8_t *frequency)
{
	uint16_t length;
	uint8_t dummy;
	uint8_t value;

	if (wlanTlvThresholdOverride(tlv, out, frequency, &length) != 0)
		return length;
	if (frequency == NULL)
		frequency = &dummy;
	if (LE16(tlv + 2) == 2) {
		out[0] = tlv[4];
		value = tlv[5];
		if (value == 0)
			value = 1;
		*frequency = value;
		out[1] = tlv[5];
	}
	return (uint16_t)(LE16(tlv + 2) + 4);
}

/* Copies the value of a TLV to destination; returns the TLV's size. */
unsigned int wlanTlvCopyValue(const uint8_t *tlv, void *destination)
{
	__aeabi_memcpy(destination, tlv + 4, LE16(tlv + 2));
	return (uint16_t)(LE16(tlv + 2) + 4);
}
