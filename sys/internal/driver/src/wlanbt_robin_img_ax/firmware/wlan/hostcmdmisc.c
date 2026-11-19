/*
 * Host commands the dispatcher of wlan/hostcmd.c hands to small handlers of
 * their own: the multicast list, the antenna, the EEPROM, memory and TSF
 * accesses, the supplicant's PMK and the hardware description, and the
 * commands that are not answered at all. The command numbers and field
 * names are Marvell's (mlan_fw.h) where it names them. The filename is
 * inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/mac.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/session.h"
#include "robin/soc.h"

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))
#define PUT16(bytes, value) \
	do { unsigned int put16_ = (value); (bytes)[0] = (uint8_t)put16_; (bytes)[1] = (uint8_t)(put16_ >> 8); } while (0)

#define HostCmd_CMD_GET_HW_SPEC			0x0003
#define HostCmd_CMD_802_11_RF_CHANNEL		0x001d
#define HostCmd_CMD_802_11_EEPROM_ACCESS	0x0059
#define HostCmd_CMD_GET_TSF			0x0080
#define HostCmd_CMD_MEM_ACCESS			0x0086
#define HostCmd_CMD_SUPPLICANT_PMK		0x00c4
#define HostCmd_CMD_802_11_PS_MODE_ENH		0x00e4
#define HostCmd_CMD_802_11_HS_CFG_ENH		0x00e5
#define HostCmd_CMD_802_11_KEY_ENCRYPT		0x0054

#define HostCmd_ACT_GEN_GET			0
#define HostCmd_ACT_GEN_SET			1
#define HostCmd_ACT_GEN_REMOVE			4
#define HostCmd_ACT_GET_RX			0x0004	/* HostCmd_CMD_802_11_RF_ANTENNA */
#define HostCmd_ACT_GET_TX			0x0008
#define HostCmd_ACT_GET_BOTH			0x000c
#define SLEEP_CONFIRM				5	/* Marvell's PS mode action */
#define HS_CONFIGURE				1	/* Marvell's host sleep actions */
#define HS_ACTIVATE				2
#define HS_CONDITION_CANCEL			0xffffffff

#define EEPROM_MAGIC				0x38333058	/* the first word of the EEPROM the host may read */
#define EEPROM_WORDS_MAX			20		/* bytes of one read */

/* The 802.11 antenna mode the host reads when the radio diversifies. */
#define ANTENNA_DIVERSITY			0xffff

/* The commands the control task does not act on; the dispatcher goes on with the first four of them. */
extern void hostCmd00c4(uint8_t *body);						/* 0x2cc18 */

/* The command code without its response bit (bit 15 is the response flag, bits 11-14 are none of the command). */
#define COMMAND_CODE(header)	((((header)[1] << 8) & 0x7ff) | (header)[0])

/* HostCmd_CMD_802_11_PS_MODE_ENH with the action SLEEP_CONFIRM that has no result yet. */
int hostCmdIsSleepConfirm(const uint8_t *command)
{
	return ((((command[1] << 8) & 0x7fff) | command[0]) == HostCmd_CMD_802_11_PS_MODE_ENH) &&
	       LE16(command + 6) == 0 && LE16(command + 8) == SLEEP_CONFIRM;
}

/* HostCmd_CMD_802_11_HS_CFG_ENH that configures the conditions (and does not cancel them). */
int hostCmdIsHostSleepConfigure(const uint8_t *command)
{
	const uint8_t *body = command + 8;

	return ((((command[1] << 8) & 0x7fff) | command[0]) == HostCmd_CMD_802_11_HS_CFG_ENH) &&
	       LE16(body) == HS_CONFIGURE && __aeabi_uread4(body + 2) != HS_CONDITION_CANCEL;
}

/* HostCmd_CMD_802_11_HS_CFG_ENH that activates the host sleep. */
int hostCmdIsHostSleepActivate(const uint8_t *command)
{
	return ((((command[1] << 8) & 0x7fff) | command[0]) == HostCmd_CMD_802_11_HS_CFG_ENH) &&
	       LE16(command + 8) == HS_ACTIVATE;
}

/*
 * HostCmd_CMD_802_11_EEPROM_ACCESS (a read only): the words of the EEPROM from the offset on,
 * when its first word is the magic of the firmware's configuration; 1 when it is not.
 */
int hostCmd0059(uint8_t *command, uint32_t magic)
{
	uint8_t *body = command + 8;
	unsigned int count = LE16(body + 4);
	int result = 0;
	uint32_t value;
	uint8_t *out;
	unsigned int done;
	unsigned int offset;
	int shift;

	if (LE16(body) != 0 || count < 4 || count > EEPROM_WORDS_MAX || (count & 3) != 0)
		return 1;
	wlanSessionOps.eepromOpen();
	wlanSessionOps.eepromRead(0, &value);
	if (value == magic) {
		out = body + 6;
		offset = LE16(body + 2);
		for (done = 0; done < count; done += 4) {
			wlanSessionOps.eepromRead(offset, &value);
			for (shift = 24; shift >= 0; shift -= 8)
				*out++ = value >> shift;
			offset = (uint16_t)(offset + 4);
		}
		PUT16(command + 2, LE16(command + 2) + LE16(body + 4) - 1);
	} else {
		result = 1;
	}
	wlanSessionOps.eepromClose();
	return result;
}

/* The multicast address table of the MAC: 64 entries of 6 bytes from entry 192. */
void hostCmd0010(uint8_t *body, uint16_t *count)
{
	unsigned int index = MAC_TABLE_FIRST;
	uint16_t entry[4];
	const uint8_t *address;
	uint32_t low;
	uint32_t high;
	unsigned int written;
	unsigned int length;
	int i;

	if (LE16(body) == HostCmd_ACT_GEN_SET) {
		address = body + 4;
		i = 0;
		while (LE16(body + 2) > i) {
			__aeabi_memcpy(&low, address, 4);
			high = (uint16_t)(address[4] | (address[5] << 8));
			address += 6;
			wlanMacTableWrite(low, high, index);
			index = (uint16_t)(index + 1);
			if (index == MAC_TABLE_FIRST + MAC_TABLE_COUNT)
				break;
			i++;
		}
		written = LE16(body + 2);
		if (*count > written) {
			while (*count > written) {
				wlanMacTableWrite(0, 0, index);
				index = (uint16_t)(index + 1);
				written++;
			}
		}
		length = LE16(body + 2);
		if (length > 64)
			length = 64;
		*count = length;
	} else if (LE16(body) == HostCmd_ACT_GEN_GET) {
		PUT16(body + 2, *count);
		address = body + 4;
		for (i = 0; *count > i; i++) {
			wlanMacTableRead(entry, index);
			__aeabi_memcpy((uint8_t *)address, entry, 6);
			address += 6;
			index = (uint16_t)(index + 1);
		}
	}
}

/* The station's MAC address and the state of the radio and the power save (what the host reads as the "state"). */
void hostCmd000a(uint8_t *body, unsigned int channel, int psState, unsigned int radioOn)
{
	*(uint32_t *)(body + 8) = MAC_REG(0x530);
	*(uint32_t *)(body + 12) = MAC_REG(0x534);
	body[0] = 2;
	body[1] = 0;
	PUT16(body + 6, channel);
	body[2] = 3;
	body[3] = 0;
	__aeabi_uwrite4(psState, body + 16);
	PUT16(body + 4, radioOn);
}

/*
 * HostCmd_CMD_802_11_RF_CHANNEL: a read answers the channel and the band; a set is not
 * supported (result 2).
 */
void hostCmd001d(uint8_t *body, uint16_t *result)
{
	uint8_t band;
	uint32_t channel;
	unsigned int action = LE16(body);

	if (action == HostCmd_ACT_GEN_GET) {
		wlanSessionOps.mgmtPrepare(&band, &channel);
		PUT16(body + 2, channel);
		body[5] = (body[5] & ~3) | (band & 3);
	} else if (action == HostCmd_ACT_GEN_SET) {
		*result = 2;
	}
}

/* HostCmd_CMD_SUPPLICANT_PMK: sets, reads or removes the passphrases and PMKs of the TLVs after the action. */
void hostCmd00c4(uint8_t *body)
{
	uint8_t *command = body - 8;
	uint16_t length = LE16(command + 2) - 12;
	unsigned int action = LE16(body);
	unsigned int result;

	if (action == HostCmd_ACT_GEN_SET) {
		result = SupplicantSetPassphrase(body + 4, length);
		PUT16(body + 2, result);
	} else if (action == HostCmd_ACT_GEN_GET) {
		result = SupplicantQueryPassphrase(body + 4, &length);
		PUT16(body + 2, result);
		result = (uint16_t)(length + 12);
		PUT16(command + 2, result);
	} else {
		body[2] = 0;
		body[3] = 0;
		wlanSessionOps.supplicantClearPMK(body + 4, length);
	}
}

/* A value of the connection's SNMP block: 32 bits then 16 (the host's action in the first halfword). */
void hostCmd0036(uint8_t *body, int authenticator, uint8_t *state)
{
	(void)authenticator;
	if (LE16(body) == HostCmd_ACT_GEN_SET) {
		*(uint32_t *)(state + 52) = __aeabi_uread4(body + 2);
		*(uint16_t *)(state + 56) = LE16(body + 6);
	} else {
		__aeabi_uwrite4(*(uint32_t *)(state + 52), body + 2);
		PUT16(body + 6, *(uint16_t *)(state + 56));
	}
}

/*
 * The commands that are not part of the dispatcher's own cases: most are accepted and ignored;
 * returns 1 when it handled the command, 0 for an unknown one.
 */
int hostCmdMisc(uint16_t *result, uint8_t *body, struct WlanConnection *connection)
{
	uint8_t *command = body - 8;
	uint64_t timestamp;

	(void)connection;
	switch (COMMAND_CODE(command)) {
	case HostCmd_CMD_802_11_KEY_ENCRYPT:
		if (LE16(body) == HostCmd_ACT_GEN_SET)
			RC4_Encrypt(body + 20, body + 4, 16, body + 38, LE16(body + 36), 0);
		return 1;
	case HostCmd_CMD_MEM_ACCESS:
		if (LE16(body) == HostCmd_ACT_GEN_SET)
			**(uint32_t **)(body + 4) = *(uint32_t *)(body + 8);
		else
			*(uint32_t *)(body + 8) = **(uint32_t **)(body + 4);
		return 1;
	case 0x9c:
	case 0xa8:
	case 0x2f:
	case 0x38:
	case 0x39:
	case 0x13:
		return 1;
	case HostCmd_CMD_SUPPLICANT_PMK:
		hostCmd00c4(body);
		return 1;
	case HostCmd_CMD_802_11_EEPROM_ACCESS:
		*result = hostCmd0059(command, EEPROM_MAGIC);
		return 1;
	case 0x76:
		*result = LE16(body) != 0;
		return 1;
	case HostCmd_CMD_GET_TSF:
		timestamp = romGetTimerTimestamp();
		__aeabi_memcpy(body, &timestamp, 8);
		command[2] = 16;
		command[3] = 0;
		return 1;
	case 0x49:
		*(volatile uint8_t *)&MAC_REG(0x644) = 2 * (*(volatile uint8_t *)&MAC_REG(0x644) >> 1);
		return 1;
	case 5:
		SOC_REG(0x1c) |= 2;
		MAC_REG(0x514) = 0;
		REG32(0x80003034) = 1;
		return 1;
	case HostCmd_CMD_802_11_RF_CHANNEL:
		hostCmd001d(body, result);
		return 1;
	default:
		return 0;
	}
}

/* HostCmd_CMD_INACTIVITY_TIMEOUT_EXT: the power save intervals (the host's zero timeout is 1000). */
void hostCmd008a(uint8_t *body, uint16_t *beaconInterval, uint16_t *wakeBeacons,
		 uint16_t *wakeDtims, uint16_t *listenBeacons)
{
	if (LE16(body) == HostCmd_ACT_GEN_SET) {
		unsigned int interval = LE16(body + 2);

		*beaconInterval = interval;
		if (interval == 0)
			*beaconInterval = 1000;
		*wakeBeacons = LE16(body + 4);
		*wakeDtims = LE16(body + 6);
		*listenBeacons = LE16(body + 8);
	}
	PUT16(body + 2, *beaconInterval);
	PUT16(body + 4, *wakeBeacons);
	PUT16(body + 6, *wakeDtims);
	PUT16(body + 8, *listenBeacons);
}

/*
 * HostCmd_CMD_802_11_RF_ANTENNA, the reads: the antenna (1-based) of the receive side, the
 * transmit side or both; ANTENNA_DIVERSITY when the side diversifies. Returns 1 when the answer is
 * written (or *result set), 0 for the sets.
 */
int hostCmd0020(uint8_t *body, unsigned int action, uint16_t *result, uint8_t *antenna)
{
	unsigned int selected;

	switch (action) {
	case HostCmd_ACT_GET_RX:
		if (antenna[2] == 0) {
			selected = antenna[0];
			goto answer;
		}
		break;
	case HostCmd_ACT_GET_TX:
		if (antenna[3] == 0) {
			selected = antenna[1];
			goto answer;
		}
		break;
	case HostCmd_ACT_GET_BOTH:
		if (antenna[3] != 0) {
			if (antenna[2] != 0)
				break;
			*result = 1;
			return 1;
		}
		if (antenna[2] != 0) {
			*result = 1;
			return 1;
		}
		selected = antenna[1];
		goto answer;
	default:
		return 0;
	}
	PUT16(body + 2, ANTENNA_DIVERSITY);
	return 1;
answer:
	selected = selected + 1;
	PUT16(body + 2, selected);
	return 1;
}

/* The fixed words of the hardware description (HostCmd_CMD_GET_HW_SPEC) after the addresses. */
void hostCmd0003Fill(uint8_t *body, int caps, unsigned int primary, unsigned int secondary,
		     int extra)
{
	uint8_t *capabilities = body + 34;
	uint32_t value;

	__aeabi_uwrite4(0, capabilities);
	if (caps == 0)
		__aeabi_uwrite4(8, capabilities);
	value = __aeabi_uread4(capabilities) | (16 * primary) | (secondary << 6);
	__aeabi_uwrite4(value, capabilities);
	if (extra != 0)
		__aeabi_uwrite4(value | 0x400, capabilities);
	value = __aeabi_uread4(capabilities) | 0xb03;
	__aeabi_uwrite4(value, capabilities);
	__aeabi_uwrite4(0x25830011, body + 38);
	body[42] = 17;
}
