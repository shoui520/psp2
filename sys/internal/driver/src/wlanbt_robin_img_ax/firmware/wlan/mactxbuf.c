/*
 * What the MAC is told about the radio and the rates, the "macTxNotify*Buffer" functions (0x3218c
 * - 0x32538): response rates, the address table, the beacon timer, a few mode bits. Marvell
 * published no source for it; the names are the firmware's own placeholder names, kept, and the
 * file name is inferred. Registers are offsets in the MAC's block (MAC_REG()).
 */
#include <stddef.h>
#include "robin/ie.h"
#include "robin/mac.h"
#include "robin/macentry.h"
#include "robin/macrate.h"
#include "robin/mgmt.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/wlan.h"

#define MAC_2E0_STATE		MAC_REG(0x2e0)
#define MAC_2E4_CONTEXT		MAC_REG(0x2e4)
#define MAC_2C0_PAYLOAD		(SOC_MAC_REGISTER_BASE + 0x2c0)
#define MAC_340_QUEUE		(SOC_MAC_REGISTER_BASE + 0x340)
#define MAC_35C_ENTRY_WORD0	MAC_REG(0x35c)
#define MAC_360_ENTRY_WORD1	MAC_REG(0x360)
#define MAC_364_ENTRY_WORD2	MAC_REG(0x364)
#define MAC_368_ENTRY_WORD3	MAC_REG(0x368)
#define MAC_36C_ENTRY_COMMAND	MAC_REG(0x36c)
#define MAC_380_SESSION		MAC_REG(0x380)
#define MAC_528_ADDRESS_LOW	MAC_REG(0x528)
#define MAC_52C_ADDRESS_HIGH	MAC_REG(0x52c)
#define MAC_610_DEFAULT_CIPHER	MAC_REG(0x610)
#define MAC_620_DEFAULT_EVENT	MAC_REG(0x620)
#define MAC_640_DEFAULT_RECORD	MAC_REG(0x640)
#define MAC_648_PRIMARY		MAC_REG(0x648)
#define MAC_824_CONTROL_BITS	MAC_REG(0x824)
#define MAC_ENTRY_COMMAND_WRITE	8
#define MAC_ENTRY_TARGET	0x200

/* Not converted yet; provisional names. */
uint64_t romLibHandleContext(uint32_t low, uint32_t high);				/* ROM veneer 0x53224 */
void romLibSyncState(unsigned int a, uint32_t value, uint32_t mask);			/* ROM veneer */
extern uint32_t wlanMacEntrySelect;							/* SQRAM 0xc0011be8 */
extern uint32_t wlanMacEntries100[32];						/* SQRAM 0xc0011cc0 */
extern uint32_t wlanResponseRates[];							/* SQRAM 0xc0011d40: the rates, a byte each (0..12), then words */
extern uint8_t wlanMacAddressTable[6][16];						/* SQRAM 0xc00321e0 */
extern uint32_t wlanMacPendingState;							/* SQRAM 0xc0011bf4 */
extern uint32_t wlanMacPendingFilter;							/* SQRAM 0xc0011bf8 */
extern uint32_t wlanMacPendingEntry;							/* SQRAM 0xc0011bfc */
extern uint32_t wlanMacPendingTable;							/* SQRAM 0xc0011c00 */

/* 0x3218c: the response rate for the rates from start up, programmed into the MAC. */
void macTxNotifyResetBuffer(struct WlanConnection *connection, unsigned int start)
{
	wlanMacConfigApply(wlanTxLowRate(connection, start));
	wlanMacConfigBit15Update(!(wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE));
}

/* 0x321aa: the rate a frame is answered at: the register's mask of rates, as a count of bits. */
unsigned int macTxNotifySyncBuffer(unsigned int index, uint32_t rates)
{
	unsigned int result = 9;
	unsigned int mask = 0;

	switch (index) {
	case 15: case 23:
		mask = 224;
		result = 7;
		break;
	case 16:
		mask = 480;
		result = 7;
		break;
	case 17: case 24:
		mask = 992;
		break;
	case 18: case 25:
		mask = 2016;
		break;
	case 19:
		mask = 4064;
		break;
	case 20: case 21: case 26: case 27: case 28: case 29: case 30:
		mask = 8160;
		break;
	case 22: case 31:
		mask = 32;
		result = 5;
		break;
	case 32:
		for (;;)
			;
	default:
		break;
	}
	if (mask != 0) {
		rates = (mask & rates) >> 5;
		if (rates == 0)
			return result;
	}
	result = 4;
	do {
		result = (uint8_t)(result + 1);
		rates >>= 1;
	} while (rates != 0);
	return result;
}

/* 0x32204: the response rate of each rate is recorded, and the entries of the MAC take the rate of their class. */
void macTxNotifyApplyBuffer(struct WlanConnection *connection)
{
	unsigned int index;

	for (index = 0; index <= 12; index = (uint8_t)(index + 1)) {
		unsigned int rate = wlanConnectionResponseRate(connection, index);

		((uint8_t *)wlanResponseRates)[index] = rate;
		romLibConvertContext(index, rate);
	}
	for (index = 14; index <= 21; index = (uint8_t)(index + 1)) {
		uint32_t value = wlanResponseRates[macTxNotifySyncBuffer(index, connection->rateSet) - 1];

		wlanMacEntries100[4 * (index - 14) + 0] = value;
		wlanMacEntries100[4 * (index - 14) + 1] = value;
	}
	for (index = 23; index <= 30; index = (uint8_t)(index + 1)) {
		uint32_t value = wlanResponseRates[macTxNotifySyncBuffer(index, connection->rateSet) - 1];

		wlanMacEntries100[4 * (index - 23) + 2] = value;
		wlanMacEntries100[4 * (index - 23) + 3] = value;
	}
	wlanMacEntryTablesCheck(wlanMacEntrySelect, 0);
}

/* 0x32294 */
void macTxNotifyResolveBuffer(unsigned int rate)
{
	wlanMacConfigApply(rate);
	wlanMacConfigBit15Update(!(wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE));
}

/* 0x322c0 */
void macTxNotifyFormatBuffer(void)
{
	MAC_300_MODE &= ~4u;
}

/* 0x322cc */
void macTxNotifyInspectBuffer(void)
{
	MAC_2D4_FLAGS |= 4;
}

/* 0x322da */
void macTxNotifyTrackBuffer(void)
{
	MAC_868_ENABLE |= 0x20000;
}

/* 0x322e8: the beacon period, TU. */
unsigned int macTxNotifyManageBuffer(void)
{
	return (uint16_t)MAC_618_BEACON_PERIOD;
}

/*
 * 0x322f2: the MAC's table of the 16 local addresses the BSSs answer to. The table is kept
 * transposed (row i holds byte i of every address) and each row is written to the MAC in turn;
 * the address goes in the slot (or is cleared, enable 0); slot 0 is also the default address.
 */
int macTxNotifyAcquireBuffer(int enable, const uint8_t *address, unsigned int slot, int valid)
{
	unsigned int posture;
	uint32_t mode;
	unsigned int row;

	if (slot >= 0x10)
		return 1;
	posture = sysIrqDisable();
	mode = MAC_300_MODE;
	romLibResolveState();
	for (row = 0; row < 6; row++) {
		const uint32_t *words;

		wlanMacAddressTable[row][slot] = enable != 0 ? address[row] : 0;
		words = (const uint32_t *)wlanMacAddressTable[row];
		MAC_35C_ENTRY_WORD0 = words[0];
		MAC_360_ENTRY_WORD1 = words[1];
		MAC_364_ENTRY_WORD2 = words[2];
		MAC_368_ENTRY_WORD3 = words[3];
		MAC_36C_ENTRY_COMMAND = row | MAC_ENTRY_COMMAND_WRITE | MAC_ENTRY_TARGET;
	}
	if (enable != 0) {
		if (slot == 0) {
			MAC_528_ADDRESS_LOW = *(const uint32_t *)address;
			MAC_52C_ADDRESS_HIGH = *(const uint16_t *)(address + 4);
		}
		if (valid == 1)
			MAC_380_SESSION |= 1u << slot;
		MAC_370_QUEUE_MODE = 5;
	} else {
		MAC_380_SESSION &= ~(1u << slot);
	}
	MAC_300_MODE = mode;
	sysIrqRestore(posture);
	return 0;
}

/* 0x3239c */
void macTxNotifyReleaseBuffer(void)
{
	MAC_2E4_CONTEXT = 48;
	MAC_2D4_FLAGS |= 0x2000000;
	romLibSyncState(1, MAC_2D4_FLAGS, 0x2000000);
	MAC_824_CONTROL_BITS |= 4;
	MAC_300_MODE |= 0x2000;
}

/* 0x323ce */
void macTxNotifySetupBuffer(void)
{
	MAC_2E4_CONTEXT = 0;
	MAC_2D4_FLAGS &= ~0x2000000u;
	MAC_370_QUEUE_MODE = 1;
	macTxNotifyFormatBuffer();
	MAC_2F4_RINGS |= 0xff;
}

/* 0x323f8: a ring and a session bit are set up (or released) with the default frame settings. */
void macTxNotifyClearBuffer(unsigned int ring, unsigned int session, int enable, uint32_t event,
			    uint32_t cipher)
{
	if (enable != 0) {
		MAC_380_SESSION |= 1u << session;
		MAC_2F4_RINGS |= 1u << ring;
		MAC_648_PRIMARY = ring;
		MAC_620_DEFAULT_EVENT = event;
		MAC_610_DEFAULT_CIPHER = cipher;
		MAC_640_DEFAULT_RECORD = 68;
	} else {
		MAC_380_SESSION &= ~(1u << session);
		MAC_2F4_RINGS &= ~(1u << ring);
	}
}

/* 0x3243c */
void macTxNotifyConvertBuffer(unsigned int state)
{
	MAC_2E0_STATE = state & 0x3f;
}

/* 0x32448 */
unsigned int wlanTxRingLastQueue(void)
{
	return MAC_2E0_STATE & 0x3f;
}

/* 0x32454: the next beacon time: the timer rounded down to a beacon period, plus one. */
void macTxNotifySelectBuffer(void)
{
	uint64_t now = romGetTimerTimestamp();
	uint64_t period = (uint64_t)(macTxNotifyManageBuffer() << 10);

	romLibHandleContext((uint32_t)(now - now % period + period),
			    (uint32_t)((now - now % period + period) >> 32));
	MAC_370_QUEUE_MODE |= 4;
	MAC_2F4_RINGS |= 0x200;
}

/* 0x324a8 */
void macTxNotifyAdjustBuffer(void)
{
	MAC_370_QUEUE_MODE = 1;
	MAC_2F4_RINGS = 0x100;
}

/* 0x324f4 */
void macTxNotifyRecordBuffer(int shared)
{
	unsigned int value = MAC_300_MODE;

	if (shared)
		MAC_300_MODE = (value & ~1u) | 14;
	else
		MAC_300_MODE = (value & ~3u) | 12;
}

/* 0x32510 */
void macTxNotifyFilterBuffer(int shared)
{
	unsigned int value = MAC_300_MODE;

	if (shared)
		MAC_300_MODE = (value & ~5u) | 10;
	else
		MAC_300_MODE = (value & ~7u) | 8;
}

/* 0x3252c */
void macTxNotifyHandlePacket(void)
{
	MAC_300_MODE |= 0x2000;
}

/* 0x32538: the filter the host set is put aside and the MAC accepts everything, or put back. */
void hostCmd0102(unsigned int on)
{
	if (on) {
		if (wlanMacPendingState == 0) {
			wlanMacPendingEntry = MAC_46C_FILTER;
			wlanMacPendingTable = MAC_474_VALUE;
			wlanMacPendingFilter = MAC_300_MODE;
			wlanMacPendingState = 1;
		}
		MAC_46C_FILTER = 0;
		MAC_474_VALUE = 0;
		MAC_300_MODE |= 0x800222eb;
	} else if (wlanMacPendingState != 0) {
		MAC_46C_FILTER = wlanMacPendingEntry;
		MAC_474_VALUE = wlanMacPendingTable;
		MAC_300_MODE = wlanMacPendingFilter;
		wlanMacPendingState = 0;
		wlanMacPendingFilter = 0;
		wlanMacPendingEntry = 0;
		wlanMacPendingTable = 0;
	}
}
