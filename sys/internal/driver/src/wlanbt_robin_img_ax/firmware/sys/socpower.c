/*
 * Power and clock set-up of the SoC and of the radio (0x262c0..0x27860, the
 * part that is not clock rates): the delay loop, the reset lines of the MAC,
 * the radio's power blocks, the clock configuration of SOC_REG(0x1a8) and the
 * temperature sensor. The filename and the names are inferred; no Marvell
 * source names this code.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/event.h"
#include "robin/hct.h"
#include "robin/rfbus.h"
#include "robin/rfreg.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

#define SOC_CHIP_REVISION_40	0x40	/* the revision with the second clock set */
#define MAC_RESET_LINES		0x0f
#define MAC_STEPS		5000

/* The radio register that holds the trim the oscillator is calibrated to. */
#define RF_REG_TRIM		7
#define RF_TRIM_MIN		43
#define RF_TRIM_COUNT		17
#define RF_TRIM_DEFAULT		51

/* The temperature sensor's offset read from the FM state: 0x40 until it was read. */
#define SENSOR_OFFSET_UNREAD	0x40

static int8_t socSensorOffset __attribute__((section(".sqram1.rw"))) = SENSOR_OFFSET_UNREAD;
/* What the entry the strap picked returned. */
static uint32_t uapActiveEntry __attribute__((section(".sqram1.rw"))) = 0x8000;
/* The delay routine the Mask ROM calls. */
int (*sysDelayHook)(uint32_t microseconds) __attribute__((section(".itcm.rw")));

/* The three words of the chip identification, kept for the host. */
uint16_t sysChipIdHigh __attribute__((section(".dtcm.bss")));
uint16_t sysChipIdLow __attribute__((section(".dtcm.bss")));
uint32_t sysMacVersion __attribute__((section(".dtcm.bss")));

/* Waits for a number of microseconds, in loops of the clock sysClockFrequency counts. */
int sysDelayUs(uint32_t microseconds)
{
	int loops = (int)(romMul32x32(microseconds, sysClockFrequency) >> 10);

	if (loops != 0)
		return robinDelayLoop(loops);
	return loops;
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

void periphControlInit(void)
{
	PERIPH_REG(0) = 1;
	PERIPH_REG(4) = 0;
}

void periphLatchSet(void)
{
	PERIPH_REG(0xc70) |= 0x80;
}

/* Holds the four reset lines of the MAC and waits two microseconds. */
int macResetAssert(void)
{
	MAC_REG(0x1a0) |= MAC_RESET_LINES;
	return sysDelayUs(2);
}

int macResetRelease(void)
{
	MAC_REG(0x1a0) = (MAC_REG(0x1a0) >> 4) << 4;
	return sysDelayUs(1);
}

/* Resets the MAC's engines and restarts the clock event they depend on. */
uint8_t *macRadioPowerCycle(void)
{
	int steps;
	unsigned int posture;

	MAC_REG(0x300) &= ~8u;
	MAC_REG(0x644) = (MAC_REG(0x644) >> 1) << 1;
	MAC_REG(0x650) |= 0xb;
	steps = MAC_STEPS;
	do {
		if (MAC_REG(0x658) & 0x10)
			break;
	} while (steps-- != 0);
	steps = MAC_STEPS;
	do {
		if (MAC_REG(0x658) & 0x20)
			break;
	} while (steps-- != 0);
	posture = sysIrqDisable();
	SOC_REG(0xc) &= ~8u;
	SOC_REG(0x5c) &= 0xefffefff;
	SOC_REG(0x5c) |= 0x10001000;
	SOC_REG(0xc) |= 8;
	sysIrqRestore(posture);
	sysDelayUs(1);
	macResetAssert();
	return sysResetEventPolicy();
}

void rfBusSettle(void)
{
	rfBusWaitIdle();
}

void socHookNop(void)
{
}

/* Restarts the radio: the peripherals, then the MAC. */
uint8_t *rfRestart(void)
{
	periphControlInit();
	periphLatchSet();
	return macRadioPowerCycle();
}

/* Writes the divider field (bits 3-5) of SOC_REG(0x1a8). */
unsigned int socClockSetDivider(char divider)
{
	unsigned int value = (divider & 0x38) | (SOC_REG(0x1a8) & 0xffffffc7);

	SOC_REG(0x1a8) = value;
	return value;
}

/* Selects the mode 0xa in bits 6-9 of SOC_REG(0x1a8). */
unsigned int socClockSetMode(void)
{
	unsigned int value = SOC_REG(0x1a8) & 0xfffffc3f;

	SOC_REG(0x1a8) = (SOC_REG(0x1a8) & 0xfffffc3f) | 0x280;
	return value;
}

/* The first argument is not used; the callers pass the mode of the revision. */
unsigned int socClockPrepare(int mode, char divider)
{
	socClockSetMode();
	return socClockSetDivider(divider);
}

/* Sets the clock configuration for the profile and the revision of the chip. */
int socClockConfigure(void)
{
	int mode = 0;
	int result;

	if ((sysRefClockProfile() == 5 || sysRefClockProfile() == 1) &&
	    (uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40)
		mode = 512;
	socClockPrepare(mode, 40);
	SOC_REG(0x1ac) &= 0xffff67ff;
	SOC_REG(0x1a8) &= 0xfc7fffff;
	SOC_REG(0x250) |= 8;
	SOC_REG(0x250) |= 4;
	SOC_REG(0x250) |= 0x1800;
	result = SOC_REG(0x68) | 0x20;
	SOC_REG(0x68) |= 0x20;
	return result;
}

/* Writes the low seven bits of radio register 0x26, keeping the rest. */
void rfRegWriteLow7(char value)
{
	unsigned int field;

	rfRegBegin();
	field = rfRegRead(0x26);
	rfRegWrite(0x26, (value & 0x7f) | ((uint16_t)(field >> 7) << 7));
	rfRegEnd();
}

/* Sets up the clocks, their requests and the radio's calibration state at start-up. */
void socClockInit(void)
{
	unsigned int field;
	char level;

	clockRegistersInit();
	clockSelectsClear();
	hctEnableCoexScheduling();
	clockRequestCore(80000, 3);
	clockRequestBus(80000, 3);
	clockRequestAux(80000, 3);
	clockRequestVote(80000, 3);
	clockRequestCore(16000, 0);
	clockRequestBus(16000, 0);
	clockRequestAux(16000, 0);
	clockRequestVote(16000, 0);
	clockRequestBus(40000, 2);
	clockRequestVote(40000, 2);
	SOC_EVENT_CONTROL &= 0x3bd430e7;
	SOC_EVENT_CONTROL |= 0x80000048;
	SOC_EVENT_CONTROL |= 0x218050;
	switch (SOC_REG(0) & 7) {
	case 5:
	case 7:
		SOC_EVENT_CONTROL |= 0x80000;
		break;
	case 4:
		SOC_EVENT_CONTROL |= 0x20000;
		break;
	case 6:
		SOC_EVENT_CONTROL |= 0x80000;
		SOC_EVENT_CONTROL |= 0x20000;
		break;
	default:
		break;
	}
	SOC_EVENT_CONTROL |= 0x80000;
	SOC_REG(0xa8) &= 0xff9f7f81;
	rfRegBegin();
	field = rfRegRead(18) & 4;
	rfRegEnd();
	if (field != 0) {
		SOC_EVENT_CONTEXT |= 1;
		level = 0;
	} else {
		SOC_EVENT_CONTEXT = (SOC_EVENT_CONTEXT >> 1) << 1;
		level = 64;
	}
	rfRegWriteLow7(level);
	SOC_REG(0xa8) &= 0xffbfff85;
}

/* Reads the chip identification into the shared state. */
void sysChipIdRead(void)
{
	sysChipIdLow = SOC_REG(0x18);
	sysChipIdHigh = SOC_REG(0x18) >> 16;
	sysMacVersion = MAC_REG(0x814);
}

/* The offset (sign and six bits) the FM state holds for the temperature sensor. */
int socSensorCalibration(void)
{
	uint32_t value;

	if (socSensorOffset == SENSOR_OFFSET_UNREAD) {
		socSensorOffset = 0;
		if (fmCoexSetupState(10, &value, 1) == 0 && (value & 0x80)) {
			socSensorOffset = value & 0x3f;
			if (value & 0x40)
				socSensorOffset = -(value & 0x3f);
		}
	}
	return socSensorOffset;
}

/* Starts the temperature sensor and returns what it measured less its offset. */
int socSensorRead(void)
{
	unsigned int tries = 0;
	unsigned int value = 0;

	SOC_EVENT_CONTEXT &= ~0x400u;
	SOC_EVENT_CONTEXT |= 0x1000;
	sysDelayUs(6);
	SOC_EVENT_CONTEXT |= 0x2000;
	sysDelayUs(6);
	for (;;) {
		tries++;
		if (SOC_REG(0xf8) & 0x20000) {
			value = (SOC_REG(0xf8) << 6) >> 24;
			break;
		}
		if (tries >= 1000)
			break;
	}
	SOC_EVENT_CONTEXT &= ~0x2000u;
	SOC_EVENT_CONTEXT |= 0x400;
	SOC_EVENT_CONTEXT &= ~0x1000u;
	return (uint8_t)(value - socSensorCalibration());
}

/* Two readings with six microseconds between them; the second counts. */
int socSensorSample(void)
{
	socSensorRead();
	sysDelayUs(6);
	return socSensorRead();
}

void socHookNop2(void)
{
}

/* Takes the two top bits of the third divider field out of SOC_REG(0x214). */
unsigned int socClockFilter(void)
{
	unsigned int value = SOC_REG(0x214) & 0xffff3fff;

	SOC_REG(0x214) &= 0xffff3fff;
	return value;
}

/* The clock and power preset of SOC_REG(0x208..0x228) of the revision. */
unsigned int socPowerPreset(void)
{
	unsigned int result;
	int loops;

	if ((uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40) {
		SOC_REG(0x208) = 0x0e001830;
		SOC_REG(0x210) = ((SOC_REG(0x210) >> 8) << 8) + 246;
		SOC_REG(0x20c) = 0x24960001;
		loops = (sysClockFrequency + sysClockFrequency) >> 10;
		if (loops != 0)
			robinDelayLoop(loops);
		result = (SOC_REG(0x20c) >> 1) << 1;
		SOC_REG(0x20c) = result;
	} else {
		SOC_REG(0x208) = 0x0e001870;
		SOC_REG(0x20c) = 0x249a0000;
		result = ((SOC_REG(0x210) >> 24) << 24) | 0xd2;
		SOC_REG(0x210) = result;
		SOC_REG(0x228) = 0;
	}
	return result;
}

void socEnableBlockA(void)
{
	SOC_REG(0x48) |= 0x80;
	SOC_REG(0x150) |= 0x200;
}

void socHookNop3(void)
{
}

/* Passes the entry to the SQRAM code that prepares it for the third mode. */
int uapPrepareEntry(uint8_t entry)
{
	return p2pSqramPrepareEntry(entry, 3);
}

/* Calibrates the oscillator trim of the radio against the one the FM state holds. */
void rfCalibrateTrim(void)
{
	uint32_t state;
	int trim;
	uint8_t status;

	if (fmCoexSetupState(0, &state, 1) != 0 || !(state & 0x40)) {
		rfRegBegin();
		rfRegClearBits(31, 32);
		rfRegClearBits(52, 26);
		rfRegSetBits(52, 6);
		do
			status = rfRegRead(17);
		while (!(status & 0x40));
		trim = status & 0x3f;
		rfRegEnd();
	} else {
		trim = state & 0x3f;
	}
	if ((unsigned int)(trim - RF_TRIM_MIN) >= RF_TRIM_COUNT)
		trim = RF_TRIM_DEFAULT;
	rfRegBegin();
	rfRegWrite(RF_REG_TRIM, trim);
	rfRegClearBits(52, 4);
	rfRegSetBits(52, 18);
	rfRegSetBits(31, 32);
	rfRegEnd();
}

/* The radio register sequences A to D: the receive and transmit paths on and off. */
void rfRegSequenceA(void)
{
	rfRegBegin();
	rfRegSetBits(83, 3);
	rfRegWrite(47, 0);
	rfRegWrite(48, 32);
	rfRegEnd();
}

void rfRegSequenceB(void)
{
	rfRegBegin();
	rfRegClearBits(83, 3);
	rfRegWrite(47, 64);
	rfRegWrite(48, 40);
	rfRegEnd();
}

void rfRegSequenceC(void)
{
	rfRegBegin();
	rfRegClearBits(57, 128);
	rfRegWrite(60, 2);
	rfRegSetBits(57, 128);
	while (!(rfRegRead(20) & 0x10))
		;
	rfRegWrite(58, 83);
	rfRegEnd();
}

/* Sets up the radio's registers at start-up. */
void rfRegSequenceInit(void)
{
	int profile;

	rfRegBegin();
	if ((rfRegRead(15) & 0x77) != 0x66) {
		SOC_REG(0xfc) |= 0x100000;
		rfRegWrite(65, 238);
		sysDelayUs(300);
		SOC_REG(0xfc) &= ~0x100000u;
	}
	rfRegWrite(32, (uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40 ? 254 : 238);
	rfRegWrite(61, 16);
	profile = sysRefClockProfile();
	if ((profile == 5 || sysRefClockProfile() == 1) && (uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40) {
		rfRegWrite(82, 3);
		rfRegWrite(49, 172);
	}
	rfRegWrite(51, 245);
	rfRegEnd();
	rfCalibrateTrim();
	rfRegSequenceC();
	macTxNotifyResetPacket();
}

void socHookNop4(void)
{
}

/* Whether the strap in SOC_REG(0) bit 4 is set. */
int socStrapIsSet(void)
{
	return (SOC_REG(0) & 0x10) != 0;
}

/* The entry the SQRAM code gives, by the strap. */
void uapSelectEntry(void)
{
	uint32_t entry;

	if (socStrapIsSet())
		entry = uapSqramUpdateEntry();
	else
		entry = uapSqramProcessEntry();
	uapActiveEntry = entry;
}

uint32_t uapActiveEntryGet(void)
{
	return uapActiveEntry;
}

void sysDelayHookInit(void)
{
	sysDelayHook = sysDelayUs;
}

unsigned int sysPendingEventPolicyIsEnabled(void)
{
	return SOC_REG(0x70) & 0x10000;
}

int wlanPsSetupEntry(void)
{
	return wlanPsSetupHeader();
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
			if ((SOC_REG(0x44) & 2) == 0) {
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
		} else if (rfPowerBlock[4] == 0 && on == 0) {
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
