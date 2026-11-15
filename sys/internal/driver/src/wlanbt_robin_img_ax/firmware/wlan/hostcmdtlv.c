/*
 * The host's ad-hoc start and join commands and their TLVs: the responses
 * the firmware builds, the parameters taken from the command, the elements
 * the TLVs become and the WEP default keys. The filename is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/buffer.h"
#include "robin/ie.h"
#include "robin/join.h"
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

/* The elements the host's TLVs became (HT Capabilities, HT Information, 20/40 BSS Coexistence, OBSS Scan Parameters). */
struct WlanHtElements {
	uint8_t *capability;
	uint8_t *information;
	uint8_t *coexistence;
	uint8_t *obssScan;
};

/* The power limits of a Power Capability element, set by the host (0x54e70). */
struct WlanPowerCapability {
	uint32_t present;
	uint8_t minimum;
	uint8_t maximum;
};

extern struct WlanPowerCapability *wlanPowerCapability;				/* 0x54e70 */
unsigned int wlanTlvPowerCapabilityParse(const uint8_t *tlv, struct WlanPowerCapability *capability); /* 0x2c5b4 */

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
int wlanHtTlvApply(struct WlanHtElements *elements, struct WlanIeBuffer *ies, uint8_t *tlv)
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
		wpaSuppConfigureQueue(tlv, channels, NULL);
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
void wlanWepHostKeySet(WlanWepHostConfig *config, const uint8_t *tlv, int keyType)
{
	unsigned int index = tlv[0];
	WlanWepHostKey *entry = &config->keys[index];

	config->flags |= 1;
	entry->index = index;
	if (keyType == 2) {
		entry->isWep104 = 1;
		config->keyLength = 13;
		config->keyFlags |= 8;
	} else {
		entry->isWep104 = 0;
		config->keyLength = 5;
		config->keyFlags &= ~8;
	}
	__aeabi_memcpy(entry->key, tlv + 2, config->keyLength);
	if (tlv[1] != 0)
		config->defaultKey = index;
}

/* Which of the host's event masks (by bit) an event code belongs to; 0 for none. */
unsigned int wlanEventMaskIndex(int event)
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
