/*
 * Host commands that carry configuration as TLVs (0x3eb24 - 0x3f400): the keys (0x5e), the
 * security profile (0xc5), the LED TLVs (0x4e, whose handlers the SQRAM code calls, led.h),
 * the vendor element table (0xf5), the time sharing of the radio (0xe0), the power-save
 * timings (0xe4) and the rate lists of 0xd6. The filename is inferred.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/esa.h"
#include "robin/hostps.h"
#include "robin/ie.h"
#include "robin/led.h"
#include "robin/protection.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/soc.h"
#include "robin/sleep.h"
#include "robin/stamachine.h"
#include "robin/stapssm.h"
#include "robin/vendorie.h"

#define TLV_TYPE(tlv)		((uint16_t)((tlv)[0] | ((tlv)[1] << 8)))
#define TLV_LENGTH(tlv)		((uint16_t)((tlv)[2] | ((tlv)[3] << 8)))

/* The TLV types of these commands. */
enum {
	TLV_KEY_MATERIAL = 0x100,
	TLV_KEY_PEER = 0x120,
	TLV_SECURITY_MODE = 0x140,
	TLV_SECURITY_CIPHERS = 0x142,
	TLV_RATE_LIST_GENERAL = 0x151,
	TLV_RATE_LIST_LEGACY = 0x153,
	TLV_RATE_LIST_HT = 0x162,
	TLV_VENDOR_ELEMENT = 0x173,
	TLV_VENDOR_FILTER = 0x174,
	TLV_COEX_POLL = 0x160,
	TLV_COEX_MODE = 0x161,
	TLV_SLEEP_CONTROL = 0x16a,
	TLV_SLEEP_INACTIVITY = 0x16b,
	TLV_SESSION_TIMEOUT = 0x171,
	TLV_PS_HOST_CONFIG = 0x172,
};

extern uint16_t wlanPsSessionTimeout;							/* 0x568cc */
extern struct WlanPsHostConfig wlanPsHostConfig;					/* 0x568be */
extern const struct { uint8_t address[6]; } *wlanDeviceAddress;				/* 0x54a7c */
extern WlanWirelessFrame *wlanIbssProbeResponseFrame;					/* 0x54c28 */
extern WlanWirelessFrame *wlanIbssBeaconFrame;						/* 0x54c2c */
extern struct WlanMacTxRequest *wlanIbssProbeResponseRequest;				/* 0x54bc8 */
extern struct WlanMacTxRequest *wlanIbssBeaconRequest;					/* 0x54bc4 */
void *mdevRomComputeState(void *table, int next);					/* ROM 0x03f065a2 */

/* The ROM expires the software timers through this one (0x54e98). */
extern void (*wlanSoftTimerExpireHook)(struct WlanSoftTimer *timer);			/* 0x54e98 */

/* TLV_LED_GPIO: each LED's pin, for every state; release() frees a pin the LED gives up. */
unsigned int ledGpioTlvParse(const uint8_t *tlv, uint16_t length, struct LedConfig *config,
			     void (*release)(unsigned int pin))
{
	unsigned int size = TLV_LENGTH(tlv);
	unsigned int remaining;
	const uint8_t *item;

	if (size == 0)
		return 4;
	remaining = (uint16_t)(length - 4);
	item = tlv + 4;
	while (remaining >= 2 && size >= 2) {
		unsigned int led = item[0];
		const uint8_t *gpio = item + 1;

		if (led < LED_COUNT + 1 && led != 0) {
			struct LedConfig *states = &config[led - 1];
			unsigned int current = states->behavior[0].gpio;
			unsigned int i;

			if (current != LED_GPIO_NONE) {
				if (*gpio == LED_GPIO_NONE || current != *gpio)
					release(current);
			}
			for (i = 0; i < LED_FIRMWARE_STATES; i = (uint8_t)(i + 1))
				states->behavior[i].gpio = *gpio;
		}
		item = gpio + 1;
		remaining = (uint16_t)(remaining - 2);
		size = (uint16_t)(size - 2);
	}
	return (uint16_t)(TLV_LENGTH(tlv) + 4);
}

/* TLV_LED_BEHAVIOR: what one LED does in one firmware state. */
unsigned int ledBehaviorTlvParse(const uint8_t *tlv, int length, struct LedConfig *config)
{
	if (TLV_LENGTH(tlv) == 4) {
		unsigned int state = tlv[4];

		if (state < LED_FIRMWARE_STATES) {
			unsigned int led = tlv[5];

			if (led - 1 < LED_COUNT) {
				config[led - 1].behavior[state].state = tlv[6];
				config[led - 1].behavior[state].argument = tlv[7];
			}
		}
	}
	return (uint16_t)(TLV_LENGTH(tlv) + 4);
}

/* The answer to 0x4e: each LED's pin, then its behaviour in every state. */
unsigned int ledControlTlvsPut(uint8_t *out, const struct LedConfig *config)
{
	unsigned int led;
	unsigned int state;
	uint16_t size = 4;

	out[0] = TLV_LED_GPIO & 0xff;
	out[1] = TLV_LED_GPIO >> 8;
	out[2] = 6;
	out[3] = 0;
	out += 4;
	led = 0;
	do {
		const struct LedConfig *states = &config[led];

		led = (uint8_t)(led + 1);
		*out++ = led;
		*out++ = states->behavior[0].gpio;
		size += 2;
	} while (led < LED_COUNT);
	led = 0;
	do {
		const struct LedConfig *states = &config[led];

		led = (uint8_t)(led + 1);
		state = 0;
		do {
			out[0] = TLV_LED_BEHAVIOR & 0xff;
			out[1] = TLV_LED_BEHAVIOR >> 8;
			out[2] = 4;
			out[3] = 0;
			out[4] = state;
			out[5] = led;
			out[6] = states->behavior[state].state;
			out[7] = states->behavior[state].argument;
			out += 8;
			size += 8;
			state = (uint8_t)(state + 1);
		} while (state < LED_FIRMWARE_STATES);
	} while (led < LED_COUNT);
	return (uint16_t)(wlanTlvTerminate(out) + size);
}

/* The ROM's key handling of the supplicant: when the PMK is cleared the keys of the contexts go with it. */
void wlanPsConvertDescriptor(const uint8_t *buffer, int length)
{
	if (SupplicantClearPMK_internal(buffer, length) == 0) {
		supplicantDisable();
		pmkCacheFlush();
	}
}

/* The ROM calls these for the supplicant instead of its own. */
void p2pDirectCheckState(void)
{
	ramHook_supplicantEnable = supplicantEnable;
	ramHook_pmkCacheFindPSK = pmkCacheFindPSK;
}

/* The ROM expires the software timers with the management state check from now on. */
int wlanPsInspectDescriptor(struct WlanSoftTimer *timer)
{
	return macMgmtCheckState(timer);
}

void wlanPsTrackDescriptor(void)
{
	wlanSoftTimerExpireHook = (void (*)(struct WlanSoftTimer *))wlanPsInspectDescriptor;
}

/* The key material TLVs of 0x5e: each goes to the connection, or the peer a following TLV names; returns how many. */
unsigned int hostCmd005e(struct WlanConnection *connection, uint8_t *tlv, uint8_t *end)
{
	unsigned int count = 0;
	int last = 0;

	do {
		unsigned int type = TLV_TYPE(tlv);

		if (type == TLV_KEY_MATERIAL) {
			struct WlanConnection *target;

			if (connection->mode == 2) {
				uint8_t *next = tlv + TLV_LENGTH(tlv) + 4;
				uint32_t any[2] = { 0xffffffff, 0xffff };

				target = NULL;
				if (next < end && TLV_TYPE(next) == TLV_KEY_PEER) {
					if (TLV_LENGTH(next) == 6) {
						target = connection;
						if (memcmp(next + 4, any, 6) != 0)
							target = wlanConnectionFindByPeer(next + 4);
					}
				} else {
					target = connection;
				}
				if (target == NULL)
					goto skipped;
			} else {
				target = connection;
			}
			keyApi_UpdateKeyMaterial(target, (key_MgtMaterial_t *)(tlv + 4));
skipped:
			count = (uint16_t)(count + 1);
		} else if (type == 0xffff) {
			last = 1;
		}
		tlv += TLV_LENGTH(tlv) + 4;
	} while (tlv < end && last == 0);
	return count;
}

/* The answer to 0x5e: the group key's and the pairwise key's TLVs. */
unsigned int hostCmd005eGet(struct WlanConnection *connection, uint8_t *out)
{
	struct MrvlIEtypesHeader *header = (struct MrvlIEtypesHeader *)out;
	unsigned int first, second;

	header->type = TLV_KEY_MATERIAL;
	first = keyApi_GetKeyMaterial(connection, (key_MgtMaterial_t *)(out + 4), 1);
	header->length = first;
	header = (struct MrvlIEtypesHeader *)(out + 4 + first);
	header->type = TLV_KEY_MATERIAL;
	second = keyApi_GetKeyMaterial(connection, (key_MgtMaterial_t *)((uint8_t *)header + 4), 0);
	header->length = second;
	return (uint16_t)(wlanTlvTerminate((uint8_t *)header + 4 + second) + (first + 4) + 4 + second);
}

/* 0xc5: the security profile: the mode and the ciphers, set from the TLVs or written into them. */
void hostCmd00c5(unsigned int action, uint8_t *data, uint16_t *length)
{
	const uint8_t *mode = NULL;
	const uint8_t *unicast = NULL;
	const uint8_t *multicast = NULL;
	int remaining = (int16_t)*length;

	while (remaining > 0) {
		const uint8_t *tlv = data + *length - remaining;

		if (TLV_TYPE(tlv) == TLV_SECURITY_MODE) {
			mode = tlv + 4;
		} else if (TLV_TYPE(tlv) == TLV_SECURITY_CIPHERS) {
			unicast = tlv + 4;
			multicast = tlv + 5;
		}
		remaining = (int16_t)(remaining - TLV_LENGTH(tlv) - 4);
	}
	if (action == 1) {
		if (mode != NULL && multicast != NULL && unicast != NULL)
			supplicantSetProfile(*(const SecurityMode_t *)mode, *(const Cipher_t *)multicast,
					     *(const Cipher_t *)unicast);
	} else {
		uint8_t *tlv = data + *length;
		uint8_t *payload = tlv + 4;

		tlv[2] = 2;
		tlv[3] = 0;
		tlv[0] = TLV_SECURITY_MODE & 0xff;
		tlv[1] = TLV_SECURITY_MODE >> 8;
		tlv += 6;
		tlv[2] = 2;
		tlv[3] = 0;
		tlv[0] = TLV_SECURITY_CIPHERS & 0xff;
		tlv[1] = TLV_SECURITY_CIPHERS >> 8;
		*length += 12;
		if (action != 0)
			supplicantGetProfileCurrent((SecurityMode_t *)payload, (Cipher_t *)(payload + 7),
						    (Cipher_t *)(payload + 6));
		else
			supplicantGetProfile((SecurityMode_t *)payload, (Cipher_t *)(payload + 7),
					     (Cipher_t *)(payload + 6));
	}
}

/*
 * The vendor element table of 0xf5: gets or sets the elements of the peers (0x173) or the vendor
 * filter (0x174). The TLV has the peer's address and, to set, the element.
 */
int wlanPsSelectDescriptor(struct WlanConnection *connection, unsigned int action, uint8_t *tlv, uint8_t *data,
			   uint16_t *length)
{
	int result = 1;
	unsigned int size = *length;

	switch (action) {
	case 0:
		if (TLV_LENGTH(tlv) != 0) {
			struct WlanVendorIeEntry *entry = macMgmtVerifyTable(tlv + 4);

			if (entry == NULL) {
				result = 1;
				break;
			}
			__aeabi_memcpy(data + 10, entry->element, entry->element[1] + 2);
			size = entry->element[1] + 12;
			((struct MrvlIEtypesHeader *)data)->length = entry->element[1] + 8;	/* a byte at a time */
			result = 0;
			break;
		}
		size = 4;
		mdevRomComputeState(&wlanVendorIeTable, 0);
		for (;;) {
			uint8_t *item = mdevRomComputeState(&wlanVendorIeTable, 1);

			if (item == NULL)
				break;
			tlv = data + size;
			tlv[0] = (uint8_t)TLV_VENDOR_ELEMENT;	/* a byte at a time: the reply may be unaligned */
			tlv[1] = TLV_VENDOR_ELEMENT >> 8;
			tlv[2] = 6;
			tlv[3] = 0;
			__aeabi_memcpy(data + size + 4, item + 28, 6);
			size = (uint16_t)(size + 10);
		}
		result = 0;
		break;
	case 1:
		result = (uint8_t)macMgmtSelectTable(tlv + 4, tlv + 10);
		if (result == 0 && connection->interfaceType == 1) {
			if (memcmp(wlanDeviceAddress->address, tlv + 4, 6) == 0) {
				if (wlanBeaconTemplateEnd != 0)
					wlanTemplateSetIe(8, wlanIbssBeaconFrame, wlanIbssBeaconRequest, tlv + 10);
				if (wlanProbeTemplateEnd != 0)
					wlanTemplateSetIe(5, wlanIbssProbeResponseFrame, wlanIbssProbeResponseRequest,
							  tlv + 10);
			}
		}
		break;
	case 4:
		if (TLV_LENGTH(tlv) == 0) {
			macMgmtHandleBuffer();
			result = 0;
		} else {
			struct WlanVendorIeEntry *entry = macMgmtVerifyTable(tlv + 4);

			if (entry != NULL) {
				macMgmtClearTable(entry);
				result = 0;
			} else {
				result = 1;
			}
		}
		break;
	default:
		break;
	}
	if (size != 0)
		*length = size;
	return result;
}

/* The vendor filter's bytes of 0xf5 (6 at most): set or get. */
int wlanPsAdjustDescriptor(unsigned int action, uint8_t *tlv, uint8_t *data, uint16_t *length)
{
	unsigned int size;
	void *destination;
	const void *source;

	if (action == 1) {
		*length = 0;
		size = 6;
		if ((unsigned int)(tlv[4] | (tlv[5] << 8)) <= 6)
			size = tlv[4];
		wlanVendorIeFilter.matchLength = size;
		destination = wlanVendorIeFilter.match;
		source = tlv + 6;
	} else {
		if (action != 0)
			return 1;
		*length = 0;
		size = wlanVendorIeFilter.matchLength;
		data[4] = size;
		data[5] = 0;
		source = wlanVendorIeFilter.match;
		destination = data + 6;
	}
	__aeabi_memcpy(destination, source, size);
	*length += 12;
	return 0;
}

int hostCmd00f5(struct WlanConnection *connection, unsigned int action, uint8_t *data, uint16_t *length)
{
	uint8_t *element = NULL;
	uint8_t *filter = NULL;
	int remaining = (int16_t)*length;

	while (remaining > 0) {
		uint8_t *tlv = &data[(uint16_t)*length - remaining];
		unsigned int type = TLV_TYPE(tlv);

		if (type == TLV_VENDOR_ELEMENT)
			element = tlv;
		else if (type == TLV_VENDOR_FILTER)
			filter = tlv;
		remaining = (int16_t)(remaining - TLV_LENGTH(tlv) - 4);
	}
	if (element != NULL)
		return wlanPsSelectDescriptor(connection, action, element, data, length);
	if (filter != NULL)
		return wlanPsAdjustDescriptor(action, filter, data, length);
	return 1;
}

/*
 * 0xe0: the time sharing of the radio with Bluetooth: the poll flag (0x160) and up to two mode
 * entries (0x161): the mode, the two times of a period and the period. Returns 1 for an entry
 * whose times do not fit its period.
 */
int hostCmd00e0(unsigned int action, uint8_t *data, unsigned int length)
{
	uint8_t *poll = NULL;
	uint8_t *entries[2] = { NULL, NULL };
	uint16_t modes = 0;
	int remaining = (int16_t)length;
	uint8_t *end = data + length;
	int i;

	while (remaining > 0) {
		uint8_t *tlv = end - remaining;

		if (TLV_TYPE(tlv) == TLV_COEX_POLL) {
			poll = tlv;
		} else if (TLV_TYPE(tlv) == TLV_COEX_MODE) {
			if (entries[0] != NULL) {
				if (entries[1] == NULL)
					entries[1] = tlv;
			} else {
				entries[0] = tlv;
			}
		}
		remaining = (int16_t)(remaining - TLV_LENGTH(tlv) - 4);
	}
	if (action == 1) {
		for (i = 0; i < 2 && entries[i] != NULL;) {
			uint8_t *entry = entries[i];
			unsigned int times = __aeabi_uread4(entry + 8);
			unsigned int low = (uint16_t)times;
			unsigned int high = times >> 16;
			unsigned int period;
			unsigned int mode;
			unsigned int index;

			if (low == 0)
				return 1;
			period = __aeabi_uread4(entry + 12);
			if (period <= low || period <= high)
				return 1;
			mode = TLV_TYPE(entry + 4);
			index = mode >> 1;
			modes |= 1 << (mode == 0 ? 1 : mode);
			wlanCoexConfig.period[index] = period;
			wlanCoexConfig.reserve[index].word = __aeabi_uread4(entry + 8);
			wlanCoexConfig.flags[index] = TLV_TYPE(entry + 6);
			i = (int16_t)(i + 1);
		}
		wlanCoexConfig.btConfig = modes;
		if (poll != NULL)
			wlanPsSetPollEnabled(poll[4]);
	} else {
		unsigned int bit = 0;
		unsigned int configured;
		int used = 0;

		if (poll != NULL)
			poll[4] = wlanPsPollEnabled();
		configured = wlanCoexConfig.btConfig;
		while (configured != 0) {
			uint8_t *entry;
			unsigned int mask;

			if (used >= 2)
				break;
			entry = entries[used];
			if (entry == NULL)
				break;
			mask = 1 << bit;
			if ((mask & configured) != 0) {
				unsigned int index = (uint16_t)bit >> 1;
				unsigned int flags;

				entry[4] = bit;
				entry[5] = (uint16_t)bit >> 8;
				__aeabi_uwrite4(wlanCoexConfig.period[index], entry + 12);
				__aeabi_uwrite4(wlanCoexConfig.reserve[index].word, entry + 8);
				flags = wlanCoexConfig.flags[index];
				entry[6] = flags;
				entry[7] = flags >> 8;
				used = (int16_t)(used + 1);
				configured = (uint16_t)(mask ^ configured);
			}
			bit = (uint16_t)(bit + 1);
		}
	}
	return 0;
}

/* The power-save timings of 0xe4: the sleep parameters (0x16a, 0x16b), the session timeout and the host's configuration; 1 when a TLV was refused. */
int hostCmd00e4Check(struct WlanConnection *connection, uint8_t *tlv, int length)
{
	unsigned int updated = 0;
	int refused = 0;
	struct WlanPsSleep *sleep = &wlanConnectionInfo(connection)->sleep;
	void *destination;

	while (length != 0) {
		switch (TLV_TYPE(tlv)) {
		case TLV_SLEEP_CONTROL:
			if (*(uint32_t *)(tlv + 8) >= sleepWakeLatency + 2945) {
				destination = &sleep->control;
				goto copy;
			}
			refused = 1;
			break;
		case TLV_SLEEP_INACTIVITY:
			destination = &sleep->inactivityTimeout;
copy:
			updated = wlanTlvCopyOfType(TLV_TYPE(tlv), tlv, destination, 12);
			break;
		case TLV_SESSION_TIMEOUT:
			updated = wlanTlvCopyOfType(TLV_SESSION_TIMEOUT, tlv, &wlanPsSessionTimeout, 2);
			wlanPsPhaseDelay = wlanPsSessionTimeout >= 10 ? wlanPsSessionTimeout : 10;
			break;
		case TLV_PS_HOST_CONFIG:
			updated = wlanTlvCopyOfType(TLV_PS_HOST_CONFIG, tlv, &wlanPsHostConfig, 14);
			wlanStaPsLoadHostConfig();
			break;
		default:
			updated = (uint16_t)(TLV_LENGTH(tlv) + 4);
			break;
		}
		if (updated == 0)
			return refused;
		length = (uint16_t)(length - updated);
		tlv += updated;
	}
	return refused;
}

/* The answer to 0xe4: four TLVs, 56 bytes. */
unsigned int hostCmd00e4(struct WlanConnection *connection, uint8_t *out)
{
	struct WlanPsSleep *sleep = &wlanConnectionInfo(connection)->sleep;

	out[0] = TLV_SESSION_TIMEOUT & 0xff;
	out[1] = TLV_SESSION_TIMEOUT >> 8;
	out[2] = 2;
	out[3] = 0;
	memcpy(out + 4, &wlanPsSessionTimeout, 2);	/* a byte at a time */
	out += 6;
	out[0] = TLV_PS_HOST_CONFIG & 0xff;
	out[1] = TLV_PS_HOST_CONFIG >> 8;
	out[2] = 14;
	out[3] = 0;
	__aeabi_memcpy(out + 4, &wlanPsHostConfig, 14);
	out += 18;
	out[0] = TLV_SLEEP_CONTROL & 0xff;
	out[1] = TLV_SLEEP_CONTROL >> 8;
	out[2] = 12;
	out[3] = 0;
	__aeabi_memcpy(out + 4, &sleep->control, 12);
	out += 16;
	out[0] = TLV_SLEEP_INACTIVITY & 0xff;
	out[1] = TLV_SLEEP_INACTIVITY >> 8;
	((struct MrvlIEtypesHeader *)out)->length = 12;	/* unaligned: a byte at a time */
	__aeabi_memcpy(out + 4, &sleep->inactivityTimeout, 12);
	return 56;
}

/* 0xd6: finds the rate lists of the command: the legacy one (0x153), the general one (0x151) and the HT one (0x162). */
int hostCmd00d6Parse(uint8_t *tlv, int length, uint8_t **legacy, uint8_t **general, uint8_t **ht)
{
	*legacy = NULL;
	*general = NULL;
	length = (int16_t)length;
	*ht = NULL;
	while (length > 0) {
		unsigned int size;

		switch (TLV_TYPE(tlv)) {
		case TLV_RATE_LIST_GENERAL:
			*general = tlv;
			break;
		case TLV_RATE_LIST_LEGACY:
			*legacy = tlv;
			break;
		case TLV_RATE_LIST_HT:
			*ht = tlv;
			break;
		default:
			break;
		}
		size = (uint16_t)(TLV_LENGTH(tlv) + 4);
		if ((int)size > length)
			break;
		length = (int16_t)(length - size);
		tlv += size;
	}
	return length != 0;
}

/* The management state of an unset address: all zeros. */
int p2pDirectPrepareState(const uint8_t *address)
{
	uint32_t zero[2] = { 0, 0 };

	return memcmp(address, zero, 6) == 0;
}

/* The command 0x1ff answers with nothing. */
unsigned int hostCmd01ff(void)
{
	return 0;
}

/* The ROM does nothing here (a hook of the ROM's, kept as the firmware has it). */
void wlanPsApplyDescriptor(void)
{
}

/* The MAC timer register a set of the templates parks (the value it had before). */
uint32_t wlanTemplateTimerSaved __attribute__((section(".itcm.rw")));				/* 0x55264 */

/*
 * HostCmd 0x84: the vendor elements (TLV 0x11c for the IBSS beacon, 0x11d for its probe response) that
 * go into the templates. The TLV is turned into the element, ID 0xdd, in place. The register is parked
 * at 1 for a get (action 0) and given back for a set.
 */
int hostCmd0084(unsigned int action, uint8_t *data, unsigned int size)
{
	unsigned int remaining = (uint16_t)(size - 10);

	while (remaining != 0) {
		unsigned int type = (data[1] << 8) | data[0];
		unsigned int length = (uint16_t)(((data[3] << 8) | data[2]) + 4);

		if (type == 0x11c) {
			data[3] = data[2];
			data[2] = 0xdd;
			wlanTemplateSetIe(8, wlanIbssBeaconFrame, wlanIbssBeaconRequest, data + 2);
		} else if (type == 0x11d) {
			data[3] = data[2];
			data[2] = 0xdd;
			wlanTemplateSetIe(5, wlanIbssProbeResponseFrame, wlanIbssProbeResponseRequest, data + 2);
		}
		remaining = (uint16_t)(remaining - length);
		data += length;
	}
	if (action != 0) {
		MAC_REG(0x0e0) = wlanTemplateTimerSaved;
	} else {
		wlanTemplateTimerSaved = MAC_REG(0x0e0);
		MAC_REG(0x0e0) = 1;
	}
	return 1;
}
