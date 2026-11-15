/*
 * The MAC's start-up and mode switches (0x3110c..0x3158e, 0x31596): the
 * hooks the crypto engine calls, the interrupt line of the host wake pin,
 * the contention window and the filter masks of the connections that
 * exist, the MAC's indexed entry tables, and the bring-up of the radio and
 * the DMA rings. Marvell published no source for it; the names say what the
 * code does and the file name is inferred.
 */
#include <stddef.h>
#include "robin/baslot.h"
#include "robin/calibration.h"
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/dmaring.h"
#include "robin/gpio.h"
#include "robin/mac.h"
#include "robin/macrate.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/socpower.h"
#include "robin/swcrypto.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wep.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"

/* Not converted yet; provisional names. */
void *p2pDirectProcessEntry(unsigned int role);			/* 0x41250 */
void fmCoexParseBuffer(void);					/* 0x4c85c */
void fmCoexDispatchBuffer(void);				/* 0x4c850 */
int fmCoexCheckBuffer(int value);				/* 0x4c80c */
void fmCoexApplyBuffer(int first, int second);			/* 0x4c942 */
void fmCoexBuildBuffer(volatile uint32_t *registers);		/* 0x4c8bc */

extern struct OsaSemaphore *macTxNotifySemaphore;		/* 0x54bd8 */

/* The MAC's indexed entries kept in SQRAM, written by wlanMacEntryTablesWrite() (0xc0011c20..0xc0011cc0). */
extern uint32_t wlanMacEntries000[32];				/* SQRAM 0xc0011c30: entries 0..15 */
extern uint32_t wlanMacEntries040[4];				/* SQRAM 0xc0011c20: entry 64 */
extern uint32_t wlanMacEntries100[32];				/* SQRAM 0xc0011cc0: entries 256..271 */
extern uint32_t wlanMacEntries140[4];				/* SQRAM 0xc0011cb0: entry 320 */
extern uint32_t wlanMacEntrySelect;				/* SQRAM 0xc0011be8 */
extern uint16_t wlanContentionWindowSet;			/* SQRAM 0xc0011bd8 */
extern uint32_t wlanMacProvider;				/* SQRAM 0xc0011be0 */
extern uint32_t wlanMacPowered;					/* SQRAM 0xc0011be4: the radio was powered once */

/* Waits until the crypto engine is idle, then takes the semaphore the host commands hold. */
uint32_t wlanCryptoEnter(void)
{
	do {
		while (wlanCryptoCount(0) != 0)
			;
	} while (wlanCryptoCount(0) != 0);
	osaSemaphoreWait(macTxNotifySemaphore, 0xffffffff);
	socEventsHold();
	return 0;
}

void wlanCryptoLeave(uint32_t token)
{
	socEventsRelease();
	osaSemaphoreRelease(macTxNotifySemaphore);
}

void wlanCryptoAfterReset(void)
{
	sysDelayUs(20);
}

void wlanCryptoStart(void)
{
	sysDelayUs(20);
}

/* Installs the crypto engine's hooks. */
void wlanCryptoOpsInit(void)
{
	wlanCryptoOps.enter = wlanCryptoEnter;
	wlanCryptoOps.leave = wlanCryptoLeave;
	wlanCryptoOps.afterReset = wlanCryptoAfterReset;
	wlanCryptoOps.start = wlanCryptoStart;
	wlanCryptoOps.pollDelay = 30;
}

/* Sets how the interrupt line of a pin triggers (the registers' roles are inferred from use). */
void gpioPinInterruptSet(unsigned int pin, int first, int second)
{
	uint32_t bit = 1u << pin;

	if (first == 1)
		PERIPH_REG(0x254) |= bit;
	else
		PERIPH_REG(0x254) &= ~bit;
	if (second == 1)
		PERIPH_REG(0x214) |= bit;
	else
		PERIPH_REG(0x214) &= ~bit;
	PERIPH_REG(0x218) |= bit;
}

/* The host wake pin (HostCmd 0x74): released and set up as an interrupt, only while sleeping on the secondary source. */
void hostCmd0074Reset(void)
{
	if (sleepStats.mode == 2) {
		gpioPinSelect(4, 0);
		gpioPinInterruptSet(4, 1, 1);
	}
}

void hostCmd0074Arm(void)
{
	if (sleepStats.mode == 2) {
		PERIPH_REG(0x220) &= ~0x10u;
		SOC_IRQ_ENABLE_SET |= 0x1000;
	}
}

/* The PMIC is not present: its register commands do nothing. */
void pmicRegWrite(uint8_t reg, uint8_t value)
{
}

uint8_t pmicRegRead(uint8_t reg)
{
	return 0;
}

void hostCmd0096Track(void)
{
}

int hostCmd008fApply(void)
{
	return 0;
}

/* The time an acknowledgement takes in the connection's rate set. */
uint16_t wlanConnectionAckTime(struct WlanConnection *connection, unsigned int rate, uint32_t flags)
{
	return wlanAckTime(connection->rateSet, rate, flags);
}

uint16_t wlanConnectionFrameDuration(struct WlanConnection *connection, unsigned int rate, uint32_t flags,
				     unsigned int length, unsigned int wmm)
{
	return wlanFrameDuration(connection->rateSet, rate, flags, length, wmm);
}

uint16_t wlanFrameDurationMacCall(int a, int b, int c, int d, int e)
{
	return wlanFrameDurationMac(a, b, c, d, e);
}

/* Sets the contention windows of the four queues when the BSS has no WMM (31 slots for a station, else 15). */
void wlanContentionWindowUpdate(unsigned int interfaceType, unsigned int lowRate)
{
	unsigned int queue;
	unsigned int minimum;

	if (wlanContentionWindowSet == lowRate)
		return;
	wlanContentionWindowSet = lowRate;
	if (wlanWmmEnabled() != 0)
		return;
	if (lowRate == 0) {
		wlanMacTimersProgram(15, 1023, 63);
		return;
	}
	minimum = interfaceType == 2 ? 31 : 15;
	for (queue = 0; queue <= 3; queue++) {
		romEdcaSetCwMin(queue, minimum);
		romEdcaSetCwMax(queue, 1023);
	}
}

/* Sets the two filter masks of the MAC from the connections that exist. */
void wlanMacFilterMasksUpdate(void)
{
	uint32_t first = 0xffffffff;
	uint32_t second = 0xffffffff;
	int ibss = wlanConnectionExists(0) != 0 || wlanConnectionExists(1) != 0;
	int station = wlanConnectionExists(2);
	int ap = wlanConnectionExists(3);
	int amp = wlanConnectionExists(4);

	if (ibss) {
		first = 0xffebefeb;
		second = 0xffeaeee8;
	}
	if (ap | station) {
		if (p2pDirectProcessEntry(1) != NULL) {
			first &= 0xffeaefeb;
			second &= 0xffeaeee8;
		} else if (p2pDirectProcessEntry(2) != NULL) {
			first &= 0xfffafefa;
			second &= 0xffeaecf8;
		} else if (p2pDirectProcessEntry(3) != NULL) {
			first &= 0xffebefeb;
			second &= 0xffeaeee8;
		}
		if (station != 0) {
			first &= 0xfffafefa;
			second &= 0xffeaecf8;
		}
	}
	if (amp != 0) {
		first &= 0xffeaffea;
		second &= 0xfffaeefa;
	}
	MAC_46C_FILTER = first;
	MAC_REG(0x470) = second;
}

/* Switches the MAC between its two operating modes. */
void wlanMacOperationModeSet(int on)
{
	MAC_REG(0x47c) = (MAC_REG(0x47c) >> 1) << 1;
	if (on != 0) {
		MAC_REG(0x0e0) = 0;
		wlanMacFilterMasksUpdate();
		MAC_REG(0x47c) |= 1;
	}
	wlanMacModeSwitch(on);
}

/* Checks the saved entries against the MAC's (the first argument is not used). */
int wlanMacEntryTablesCheck(unsigned int unused, int upper)
{
	return wlanMacEntryCheckTable(256, 8, wlanMacEntries100, upper) != 0 &&
	       wlanMacEntryCheckTable(320, 0, wlanMacEntries140, upper) != 0;
}

/* Writes the saved entries to the MAC. */
int wlanMacEntryTablesWrite(void)
{
	return wlanMacEntryWriteTable(0, 8, wlanMacEntries000) != 0 &&
	       wlanMacEntryWriteTable(64, 0, wlanMacEntries040) != 0;
}

int wlanMacEntryTablesApply(void)
{
	wlanMacEntryTablesWrite();
	return wlanMacEntryTablesCheck((uint8_t)wlanMacEntrySelect, 0);
}

/* Puts the MAC's rate and entry state back after it was reset. */
int wlanMacRestoreState(void)
{
	fmCoexParseBuffer();
	fmCoexDispatchBuffer();
	wlanRateParametersClear();
	wlanMacEntryTablesApply();
	return fmCoexCheckBuffer(0);
}

/* Brings the radio and the MAC up (the region provider names where the DMA regions are). */
int wlanMacBringUp(uint32_t descriptor, const struct MacRegionProvider *provider, uint32_t dmaContext)
{
	uint8_t first[4];
	uint8_t second[2];
	unsigned int ring;
	const uint8_t *config;

	first[0] = 0x22;
	first[1] = 0x32;
	first[2] = 0xff;
	wlanMacProvider = (uint32_t)provider;
	if (wlanMacPowered == 0) {
		wlanMacPowered = 1;
		macRadioPowerCycle();
	} else {
		wlanMacResume(1);
	}
	if (wlanMacLoadRegions(provider) == 0)
		return 0;
	for (ring = 0; ring < WLAN_TX_RING_COUNT; ring++) {
		*wlanTxRingReadIndex[ring] = 0;
		*wlanTxRingWriteIndex[ring] = 0;
	}
	config = (const uint8_t *)wlanRadioConfig;
	MAC_REG(0x2a0) = config[148];
	MAC_REG(0x2a4) = config[149];
	MAC_REG(0x3e0) = *(const uint32_t *)(config + 156);
	MAC_REG(0x628) = config[0];
	MAC_REG(0x62c) = *(const uint16_t *)(config + 2);
	wlanMacSlotConfigure(1);
	romLibDispatchState();
	romLibParseState(first);
	second[0] = 0x10;
	second[1] = 0xff;
	romLibParseState(second);
	MAC_REG(0x928) &= ~0x05000000u;
	MAC_REG(0x500) = (MAC_REG(0x500) >> 2) << 2;
	MAC_REG(0x468) = 30;
	wlanDmaRingsInit();
	wlanMacMode300Reset();
	wlanMacTimersProgram(15, 1023, 63);
	wlanBaEntriesInit();
	wlanBaWindowsStart();
	wlanMacRestoreState();
	romLibScheduleState();
	wlanMacClear868Enable();
	wlanPsSetPollEnabled(1);
	if (wlanPsPollUsesMacTimer() != 0)
		wlanPsPollEnableMac();
	wlanPsPollReset();
	wlanCoexInit();
	fmCoexApplyBuffer(1, 0);
	MAC_REG(0xe00) = (uint16_t)MAC_REG(0xe00) + 0x160000;
	SOC_SLEEP_MEASURE_CONTROL &= ~0x8000u;
	sysSetEventCountdownMode();
	MAC_REG(0x8c0) |= 0x08000000u;
	MAC_2D0_FLAGS |= 0x800;
	fmCoexBuildBuffer(&MAC_REG(0));
	return 1;
}

/* Restarts the DMA rings after the MAC was halted. */
void wlanMacDmaRestart(void)
{
	MAC_REG(0x800) = 4;
	sysDelayUs(6);
	wlanDmaRingsInit();
}
