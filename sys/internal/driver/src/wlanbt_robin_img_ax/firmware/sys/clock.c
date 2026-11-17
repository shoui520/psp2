/*
 * Clock requests, the reference clock and the radio's power blocks
 * (0x26398..0x26550, 0x26ab4..0x26c40, 0x270c0..0x27558: the power blocks use
 * this file's data in Sony's code; the rates are sys/clockrate.c). A client asks for the rate it needs, in kHz, in each of
 * the four clock domains; the fastest request of all clients is applied to the
 * hardware. Rates come from tables in units of 100 kHz, selected by the
 * reference clock profile (the crystal strapped on SOC_REG(0)). The
 * filename and the names are inferred; no Marvell source names this code.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/event.h"
#include "robin/hct.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

/* SOC_REG(0x108): bits 16-19 select the core clock, bits 24-27 the auxiliary one. */
#define CLOCK_SELECT_REG	SOC_REG(0x108)


/*
 * The rates, in units of 100 kHz, of the reference clock of each profile
 * (index 0..9) and a second per-profile table (10..19); the table the
 * firmware shares with its sleep code (rfCalibrationTable, 0x53d56).
 */
const uint16_t rfCalibrationTable[20] = {
	0x0078, 0x0082, 0x00c0, 0x00c8, 0x00f0, 0x0104, 0x0180, 0x0190, 0x01b8, 0x0208,
	0x0fa0, 0x0fa0, 0x0dac, 0x0e10, 0x0dac, 0x0dac, 0x0bb8, 0x0bb8, 0x0bb8, 0x0bb8,
};

/* Per profile: the first word of the pair has bit 31 set when the reference is halved. */
static const uint32_t clockPllWords[10][2] = {
	{ 0x05555556, 0x04aec33e }, { 0x053b13b0, 0x04a151c4 }, { 0x04d55555, 0x046d3a06 },
	{ 0x04cccccc, 0x0468db8c }, { 0x04aaaaaa, 0x0457619e }, { 0x049d89d8, 0x0450a8e2 },
	{ 0x84d55555, 0x04369d04 }, { 0x84cccccc, 0x04346dc6 }, { 0x00000000, 0x00000000 },
	{ 0x849d89d8, 0x04285472 },
};
/* Per profile, the value of the select register's low half. */
static const uint32_t clockSelectWords[10] = {
	0x5555c000, 0x6276c000, 0x6aab8000, 0x66668000, 0x55558000,
	0x4ec58000, 0x35558000, 0x33338000, 0x2e8c8000, 0x27628000,
};

/* The request of each client for each domain, and the rate applied. */
struct ClockRequests {
	uint32_t khz[CLOCK_DOMAINS];
};
static struct ClockRequests clockRequests[CLOCK_CLIENTS] __attribute__((section(".itcm.bss")));
static uint32_t clockApplied[CLOCK_DOMAINS] __attribute__((section(".itcm.bss")));

static uint32_t clockReferenceCache __attribute__((section(".itcm.rw")));
static uint32_t clockBaseCache __attribute__((section(".itcm.rw")));
int rfPowerBlock[5] __attribute__((section(".itcm.rw")));



/*
 * Records the request of a client and applies the fastest request of all
 * clients when it changed; the two flags select the domain.
 */
uint32_t clockRequest(uint32_t khz, unsigned int client, int high, int low)
{
	unsigned int domain = low ? (high ? CLOCK_DOMAIN_CORE : CLOCK_DOMAIN_AUX) :
			     (high ? CLOCK_DOMAIN_BUS : CLOCK_DOMAIN_VOTE);
	unsigned int posture = sysIrqDisable();
	uint32_t applied = clockApplied[domain];
	uint32_t previous = clockRequests[client].khz[domain];
	uint32_t fastest = applied;
	unsigned int i;

	clockRequests[client].khz[domain] = khz;
	if (khz > applied) {
		fastest = khz;
	} else {
		if (previous != applied || previous <= khz)
			goto done;
		fastest = 0;
		for (i = 0; i < CLOCK_CLIENTS; i++) {
			if (clockRequests[i].khz[domain] > fastest)
				fastest = clockRequests[i].khz[domain];
		}
		if (fastest == applied)
			goto done;
	}
	clockApplied[domain] = fastest;
	if (high) {
		if (low)
			clockApplyCore(fastest);
		else
			clockSetBus(fastest);
	} else if (low) {
		CLOCK_SELECT_REG = ((clockIndexFor(fastest) << 24) & 0x0fffffff) |
				   (CLOCK_SELECT_REG & 0xf0ffffff);
	}
done:
	sysIrqRestore(posture);
	return fastest;
}

/* Clears the clock selections the clock requests of the profile program. */
unsigned int clockSelectsClear(void)
{
	unsigned int value;

	SOC_REG(0x30) &= ~0x2000u;
	value = SOC_REG(0x108) & ~0x800000u;
	SOC_REG(0x108) = value;
	return value;
}

uint32_t __attribute__((noinline)) clockRequestCore(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 1, 1);
}

uint32_t __attribute__((noinline)) clockRequestBus(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 1, 0);
}

uint32_t __attribute__((noinline)) clockRequestAux(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 0, 1);
}

uint32_t __attribute__((noinline)) clockRequestVote(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 0, 0);
}

/* What a client asked for in a domain. */
uint32_t __attribute__((noinline)) clockRequestRead(unsigned int client, int high, int low)
{
	unsigned int posture = sysIrqDisable();
	unsigned int domain = low ? (high ? CLOCK_DOMAIN_CORE : CLOCK_DOMAIN_AUX) :
				    (high ? CLOCK_DOMAIN_BUS : CLOCK_DOMAIN_VOTE);
	uint32_t khz = clockRequests[client].khz[domain];

	sysIrqRestore(posture);
	return khz;
}

uint32_t clockRequestedCore(unsigned int client)
{
	return clockRequestRead(client, 1, 1);
}

uint32_t clockRequestedBus(unsigned int client)
{
	return clockRequestRead(client, 1, 0);
}

uint32_t clockRequestedVote(unsigned int client)
{
	return clockRequestRead(client, 0, 0);
}

/* The base rate of the reference clock, in Hz (4 MHz), once looked up. */
uint32_t clockBaseRate(void)
{
	if (clockBaseCache == 0)
		clockBaseCache = 4000000;
	return clockBaseCache;
}

/* The reference clock, in kHz, divided by two when its table says so. */
uint32_t clockReferenceRate(void)
{
	int profile;
	int index;

	if (clockReferenceCache == 0) {
		profile = sysRefClockProfile();
		index = sysRefClockProfile();
		clockReferenceCache = (100 * rfCalibrationTable[index]) /
				      ((clockPllWords[profile][0] >> 31) + 1);
	}
	return clockReferenceCache;
}

/* The slowest rate: the reference divided by the last divider of the row. */
uint32_t clockSlowestRateGet(void)
{
	return clockSlowestRate();
}

/* Whether SOC_REG(0x30) bit 26 is set; refused while the first radio block is on. */
int clockSelectSource(int enable)
{
	unsigned int posture = sysIrqDisable();

	if (rfPowerBlock[0] == 0) {
		if (enable)
			SOC_REG(0x30) |= 0x4000000;
		else
			SOC_REG(0x30) &= ~0x4000000u;
	}
	sysIrqRestore(posture);
	return 0;
}

/* Programs the clock multiplier of the profile into SOC_REG(0x118) and SOC_REG(0x11c). */
void clockPllProgram(void)
{
	int profile = sysRefClockProfile();
	int first = clockPllWords[profile][0];

	if (first >= 0)
		SOC_REG(0x94) = (uint16_t)SOC_REG(0x94) | 0x100;
	SOC_REG(0x118) = (SOC_REG(0x118) & 0x7c000000) | (first & 0x83ffffff);
	SOC_REG(0x11c) = ((SOC_REG(0x11c) >> 26) << 26) | (clockPllWords[profile][1] & 0x3ffffff);
	SOC_REG(0x118) |= 0x4000000;
	SOC_REG(0x11c) |= 0x4000000;
	SOC_REG(0x16c) &= ~0x2000u;
	SOC_REG(0x16c) &= ~0x1000u;
}

/* Programs the clock selection of the profile into SOC_REG(0xac). */
unsigned int clockSelectProgram(void)
{
	unsigned int select = clockSelectWords[sysRefClockProfile()] & 0xffffc008;

	SOC_REG(0xac) = (SOC_REG(0xac) & 0x3ff7) | select;
	return select;
}

/*
 * The radio's power blocks, one flag each in rfPowerBlock[]. The setters
 * return 1 when the block cannot change now: the blocks 0 to 3 are switched
 * on before and off after block 4, which the others depend on.
 */
#define RF_BLOCK_COUNT		5

/* Whether the clock event is in its transition mode. */
int rfEventTransitionActive(void)
{
	return sysEventTransitionModeIsSet() != 0;
}

/* Waits until the event engine has finished the handshake of the clock change. */
static inline void rfEventHandshakeWait(unsigned int primary)
{
	if (primary) {
		while (sysEventPrimaryHandshakeComplete() == 0)
			;
	} else {
		while (sysEventSecondaryHandshakeComplete() == 0)
			;
	}
}

int rfBlock0Set(int on)
{
	int refused = 0;
	unsigned int posture = sysIrqDisable();
	uint32_t pattern;
	uint32_t control;
	unsigned int held;

	if (on != rfPowerBlock[0]) {
		if (rfPowerBlock[4] == 0 || on != 0) {
			held = SOC_REG(0x44) & 2;
			pattern = SOC_REG(0x9c);
			control = SOC_REG(0xc);
			if (held == 0) {
				SOC_REG(0x44) |= 2;
				sysDelayUs(1000);
			}
			clockPllProgram();
			if (!rfEventTransitionActive())
				rfEventHandshakeWait((pattern & 0x20000) != 0);
			if (on != 0) {
				SOC_REG(0x11c) |= 0x20000000;
				rfPowerBlock[0] = 1;
			} else {
				SOC_REG(0x11c) &= ~0x20000000u;
				SOC_REG(0x118) &= ~0x4000000u;
				SOC_REG(0x94) &= ~0x100u;
				rfPowerBlock[0] = 0;
			}
			SOC_REG(0x9c) = pattern;
			SOC_REG(0xc) = control;
			if (held == 0)
				SOC_REG(0x44) &= ~2u;
		} else {
			refused = 1;
		}
	}
	sysIrqRestore(posture);
	return refused;
}

int rfBlock1Set(int on)
{
	int refused = 0;
	unsigned int posture = sysIrqDisable();

	if (on != rfPowerBlock[1]) {
		if (rfPowerBlock[4] != 0 && on == 0) {
			refused = 1;
		} else if (on == 0) {
			SOC_REG(0x11c) &= ~0x40000000u;
			rfPowerBlock[1] = 0;
		} else {
			SOC_REG(0x11c) |= 0x40000000;
			rfPowerBlock[1] = 1;
		}
	}
	sysIrqRestore(posture);
	return refused;
}

int rfBlock2Set(int on)
{
	int refused = 0;
	unsigned int posture = sysIrqDisable();

	if (on != rfPowerBlock[2]) {
		if (rfPowerBlock[4] != 0 && on == 0) {
			refused = 1;
		} else if (rfPowerBlock[3] != 0) {
			if (on != 0) {
				SOC_REG(0xac) |= 2;
				SOC_REG(0x48) |= 0x40;
				if ((unsigned int)sysRefClockProfile() > 6)
					SOC_REG(0x118) = (SOC_REG(0x118) & 0x87ffffff) | 0x58000000;
				rfPowerBlock[2] = 1;
			} else {
				SOC_REG(0x48) &= ~0x40u;
				SOC_REG(0x118) &= ~0x40000000u;
				SOC_REG(0xac) &= ~2u;
				rfPowerBlock[2] = 0;
			}
		} else if (on != 0) {
			refused = 1;
		} else {
			SOC_REG(0x48) &= ~0x40u;
			SOC_REG(0x118) &= ~0x40000000u;
			SOC_REG(0xac) &= ~2u;
			rfPowerBlock[2] = 0;
		}
	}
	sysIrqRestore(posture);
	return refused;
}

int rfBlock3Set(int on)
{
	int refused = 0;
	unsigned int posture = sysIrqDisable();

	if (on != rfPowerBlock[3]) {
		if ((rfPowerBlock[4] == 0 || on != 0) && (rfPowerBlock[2] == 0 || on != 0)) {
			clockSelectProgram();
			if (on != 0) {
				SOC_REG(0xac) |= 4;
				rfPowerBlock[3] = 1;
			} else {
				SOC_REG(0xac) &= ~4u;
				rfPowerBlock[3] = 0;
				SOC_REG(0xac) &= ~0x8000u;
			}
		} else {
			refused = 1;
		}
	}
	sysIrqRestore(posture);
	return refused;
}

int rfBlock4Set(int on)
{
	int refused = 0;
	unsigned int posture = sysIrqDisable();

	if (on != rfPowerBlock[4]) {
		if (((rfPowerBlock[0] != 0 && rfPowerBlock[1] != 0 && rfPowerBlock[2] != 0 &&
		      rfPowerBlock[3] != 0) || on == 0) &&
		    (rfEventTransitionActive() || (SOC_REG(0x9c) & 0x10000) == 0)) {
			if (on != 0) {
				SOC_REG(0x9c) = (SOC_REG(0x9c) & 0xe07fffff) + 0x16800000;
				SOC_REG(0x9c) &= ~0x20000000u;
				sysSetEventModeFlag();
				rfPowerBlock[4] = 1;
			} else {
				sysClearEventModeFlag();
				if (!rfEventTransitionActive())
					rfEventHandshakeWait((SOC_REG(0x9c) & 0x10000) != 0);
				SOC_REG(0x9c) = (SOC_REG(0x9c) & 0xffff000f) + 0x7770;
				SOC_REG(0x9c) |= 0x20000000;
				rfPowerBlock[4] = 0;
			}
			clockSetCore(clockRequestedCore(6));
			clockSetBus(clockRequestedBus(6));
		} else {
			refused = 1;
		}
	}
	sysIrqRestore(posture);
	return refused;
}

int rfBlock4Get(void)
{
	return rfPowerBlock[4];
}

/* Switches the clock event's transition mode and what it depends on. */
int rfEventTransitionSet(int on)
{
	unsigned int posture = sysIrqDisable();

	if ((sysEventTransitionModeIsSet() == 0 || on == 0) && (sysEventTransitionModeIsSet() | on) != 0) {
		if (on != 0) {
			SOC_REG(0x60) &= 0xfffffff9;
			SOC_REG(0x9c) = (SOC_REG(0x9c) >> 1) << 1;
			if (rfPowerBlock[4] == 0) {
				while (sysEventPrimaryHandshakeComplete() == 0)
					;
				while (sysEventSecondaryHandshakeComplete() == 0)
					;
			}
			SOC_REG(0x138) = (SOC_REG(0x138) >> 4) << 4;
			sysSetEventTransitionMode();
		} else {
			sysClearEventTransitionMode();
			SOC_REG(0x138) |= 0xf;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

int rfBlock0Get(void)
{
	return rfPowerBlock[0];
}

int rfBlock1Get(void)
{
	return rfPowerBlock[1];
}

int rfBlock2Get(void)
{
	return rfPowerBlock[2];
}

int rfBlock3Get(void)
{
	return rfPowerBlock[3];
}
