/*
 * Power and clock set-up of the SoC and of the radio (0x26550..0x270c0, one
 * object in Sony's code): the delay loop, the reset lines of the MAC, register
 * access to the radio through the serial bus, the clock configuration of
 * SOC_REG(0x1a8), the host clock presets, the temperature sensor and the SoC
 * register window of host command 0x8f. The filename and the names are
 * inferred; no Marvell source names this code.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/event.h"
#include "robin/fm.h"
#include "robin/hct.h"
#include "robin/hostcmd.h"
#include "robin/mactx.h"
#include "robin/nvram.h"
#include "robin/rfbus.h"
#include "robin/rfreg.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
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

#define RFBUS_CONFIGURATION_KEEP	0x400	/* the bit of the configuration every mode keeps */
#define RFREG_LOCK_FREE		0xff

/* The event paths the crypto engine and the host commands hold. */
#define SOC_EVENT_CONTROL_CRYPTO_MASK	0x04004300u

/* A register of the SoC and the bits of it the host may change. */
struct SocRegisterMask {
	volatile uint32_t *reg;
	uint32_t mask;
};

/*
 * The data, in Sony's order (0xc0011b1c..0xc0011b94): the bytes, the halfwords,
 * the words, then the tables.
 */
static uint8_t rfRegLockState __attribute__((section(".sqram1.rw"))) = RFREG_LOCK_FREE;
/* The reference clock profile: valid once read, then the index. */
static uint8_t sysRefClockProfileValid __attribute__((section(".sqram1.rw"))) = 0;
static uint8_t sysRefClockProfileIndex __attribute__((section(".sqram1.rw"))) = 0;
static int8_t socSensorOffset __attribute__((section(".sqram1.rw"))) = SENSOR_OFFSET_UNREAD;
/* The control and configuration words a mode that saves them left. */
static uint16_t rfRegSavedControl __attribute__((section(".sqram1.rw")));
static uint16_t rfRegSavedConfig __attribute__((section(".sqram1.rw")));
uint32_t sysClockFrequency __attribute__((section(".sqram1.rw"))) = 0x2000;
/* What the entry the strap picked returned. */
static uint32_t sleepClockRate __attribute__((section(".sqram1.rw"))) = SLEEP_CLOCK_RATE;
/* The interrupt state the mode in use has to restore; non-zero while the bus is taken. */
static uint32_t rfRegPosture __attribute__((section(".sqram1.rw")));
static uint8_t sysRefClockProfileMap[8] __attribute__((section(".sqram1.rw"))) = {
	0, 1, 2, 4, 5, 9, 6, 6,
};
/* The registers hostCmd008f reads and writes. */
static struct SocRegisterMask socHostRegisters[9] __attribute__((section(".sqram1.rw"))) = {
	{ (volatile uint32_t *)0x80002064, 0x18000000 },
	{ (volatile uint32_t *)0x80002214, 0xffffffff },
	{ (volatile uint32_t *)0x80002218, 0x00000003 },
	{ (volatile uint32_t *)0x80002248, 0x000000ff },
	{ (volatile uint32_t *)0x8000224c, 0xff000000 },
	{ (volatile uint32_t *)0x80002208, 0x7fffff80 },
	{ (volatile uint32_t *)0x8000220c, 0x36dbfffe },
	{ (volatile uint32_t *)0x80002210, 0x001ffedb },
	{ (volatile uint32_t *)0x80002228, 0x00007fff },
};
/* The control word each mode of the bus uses, by mode index. */
uint16_t rfBusControlModes[10] __attribute__((section(".sqram1.rw"))) = {
	0x000f, 0x007f, 0x0001, 0x0000, 0x0000, 0x0004, 0x0008, 0x0010, 0x0014, 0x0020,
};

/* The delay routine the Mask ROM calls. */
int (*sysDelayHook)(uint32_t microseconds) __attribute__((section(".itcm.rw")));

/* The three words of the chip identification, kept for the host. */
uint16_t sysChipIdHigh __attribute__((section(".dtcm.bss")));
uint8_t sysChipIdLow __attribute__((section(".dtcm.bss")));
uint32_t sysMacVersion __attribute__((section(".dtcm.bss")));

static int rfBusSetControl(unsigned int mode);

void periphControlInit(void)
{
	PERIPH_REG(0) = 1;
	PERIPH_REG(4) = 0;
}

void periphLatchSet(void)
{
	PERIPH_REG(0xc70) |= 0x80;
}

/* Waits for a number of microseconds, in loops of the clock sysClockFrequency counts. */
int sysDelayUs(uint32_t microseconds)
{
	int loops = (int)(romMul32x32(microseconds, sysClockFrequency) >> 10);

	if (loops != 0)
		return robinDelayLoop(loops);
	return loops;
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

/* Takes the bus; a mode taken twice stops the firmware. */
static inline void rfRegTake(void)
{
	if (rfRegPosture != 0) {
		sysIrqDisable();
		for (;;)
			;
	}
	rfRegPosture = sysIrqDisable();
}

/* Gives the bus back to whoever held interrupts masked before. */
static inline void rfRegGive(void)
{
	unsigned int posture = rfRegPosture;

	rfRegPosture = 0;
	rfRegLockState = RFREG_LOCK_FREE;
	sysIrqRestore(posture);
}

/* The mode with the configuration 0. */
void rfBusEnter0000(void)
{
	unsigned int kept;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if ((RFBUS_CONFIGURATION & ~kept) != 0) {
		if (rfBusSetControl(1) != 0)
			RFBUS_CONFIGURATION = kept;
	}
}

void rfBusLeave0000(void)
{
	rfRegGive();
}

/* The mode with the configuration 0x82; keeps the previous control and configuration. */
int rfBusEnter0082(void)
{
	unsigned int kept;
	int result;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	result = (uint16_t)RFBUS_CONFIGURATION & ~kept;
	if (result != 0x82) {
		rfRegSavedControl = RFBUS_CONTROL;
		result = rfBusSetControl(0);
		if (result != 0) {
			rfRegSavedConfig = RFBUS_CONFIGURATION;
			RFBUS_CONFIGURATION = kept | 0x82;
			return 0x82;
		}
	}
	return result;
}

void rfBusLeave0082(void)
{
	RFBUS_CONFIGURATION = rfRegSavedConfig;
	RFBUS_CONTROL = rfRegSavedControl;
	rfRegGive();
}

/* The mode with the configuration 2 and the control word 0x1a. */
unsigned int rfBusEnter0002(void)
{
	unsigned int kept;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if (((uint16_t)RFBUS_CONFIGURATION & ~kept) != 2) {
		rfRegSavedControl = RFBUS_CONTROL;
		RFBUS_CONTROL = 0x1a;
		rfRegSavedConfig = RFBUS_CONFIGURATION;
		kept |= 2;
		RFBUS_CONFIGURATION = kept;
	}
	return kept;
}

void rfBusLeave0002(void)
{
	RFBUS_CONFIGURATION = rfRegSavedConfig;
	RFBUS_CONTROL = rfRegSavedControl;
	rfRegGive();
}

/* The mode with the configuration 0x5350. */
void rfBusEnter5350(void)
{
	unsigned int kept;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if (((uint16_t)RFBUS_CONFIGURATION & ~kept) != 0x5350) {
		if (rfBusSetControl(4) != 0)
			RFBUS_CONFIGURATION = kept | 0x5350;
	}
}

void rfBusLeave5350(void)
{
	rfRegGive();
}

/* The mode with the configuration 0x1350. */
void rfBusEnter1350(void)
{
	unsigned int kept;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if (((uint16_t)RFBUS_CONFIGURATION & ~kept) != 0x1350) {
		if (rfBusSetControl(4) != 0)
			RFBUS_CONFIGURATION = kept | 0x1350;
	}
}

void rfBusLeave1350(void)
{
	rfRegGive();
}

/* Writes a radio register in the bus's 16-bit data access. */
void rfBusWriteReg16(uint16_t reg, uint16_t value)
{
	rfBusWaitIdle();
	RFBUS_ADDRESS = reg;
	rfBusWaitIdle();
	REG16(0x90000824) = value;
	rfBusWaitIdle();
}

void rfRegWrite(uint16_t reg, uint16_t value)
{
	rfBusWaitIdle();
	RFBUS_ADDRESS = reg;
	rfBusWaitIdle();
	REG16(0x90000824) = value;
	rfBusWaitIdle();
}

uint8_t rfRegRead(uint16_t reg)
{
	rfBusWaitIdle();
	RFBUS_ADDRESS = reg;
	rfBusWaitIdle();
	return RFBUS_DATA;
}

void rfRegSetBits(uint16_t reg, uint16_t mask)
{
	uint16_t value = rfRegRead(reg);

	rfRegWrite(reg, value | mask);
}

void rfRegClearBits(uint16_t reg, uint16_t mask)
{
	uint16_t value = rfRegRead(reg);

	rfRegWrite(reg, value & ~mask);
}

void socHookNop(int argument)
{
}

/* Restarts the radio: the peripherals, then the MAC. */
uint8_t *rfRestart(void)
{
	periphControlInit();
	periphLatchSet();
	return macRadioPowerCycle();
}

/* The profile of the reference clock: a strap in bits 9-11 of SOC_REG(0), mapped by a table. */
int __attribute__((noinline)) sysRefClockProfile(void)
{
	if (sysRefClockProfileValid == 0) {
		sysRefClockProfileValid = 1;
		sysRefClockProfileIndex = sysRefClockProfileMap[(SOC_REG(0) << 20) >> 29];
	}
	return sysRefClockProfileIndex;
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

	SOC_REG(0x1a8) = value | 0x280;
	return value;
}

/* The first argument is not used; the callers pass the mode of the revision. */
unsigned int __attribute__((noinline)) socClockPrepare(int mode, char divider)
{
	socClockSetMode();
	return socClockSetDivider(divider);
}

/* Sets the clock configuration for the profile and the revision of the chip. */
void socClockConfigure(void)
{
	int mode = 0;

	if ((sysRefClockProfile() == 5 || sysRefClockProfile() == 1) &&
	    (uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40)
		mode = 512;
	socClockPrepare(mode, 40);
	SOC_REG(0x1ac) &= 0xffff67ff;
	SOC_REG(0x1a8) &= 0xfc7fffff;
	SOC_REG(0x250) |= 8;
	SOC_REG(0x250) |= 4;
	SOC_REG(0x250) |= 0x1800;
	SOC_REG(0x68) |= 0x20;
}

void rfRegEnd(void)
{
	rfRegGive();
}

/* Opens register access: the configuration 0xb50, held until rfRegEnd(). */
void rfRegBegin(void)
{
	unsigned int kept;

	rfRegTake();
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if (((uint16_t)RFBUS_CONFIGURATION & ~kept) != 0xb50) {
		if (rfBusSetControl(2) != 0)
			RFBUS_CONFIGURATION = kept | 0xb50;
	}
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

/* The silicon revision: the low byte of SOC_CHIP_REVISION, read as a half word. */
uint32_t socGetChipRevision(void)
{
	return (uint8_t)REG16(0x80002018);
}

/* Reads the chip identification into the shared state. */
void sysChipIdRead(void)
{
	uint32_t id = SOC_REG(0x18);

	sysChipIdLow = id;
	sysChipIdHigh = id >> 16;
	sysMacVersion = MAC_REG(0x814);
}

/* Sets the core clock unless the sleep clock is being calibrated. */
void clockApplyCore(unsigned int khz)
{
	if (sleepCalibrationActive() == 0)
		clockSetCore(khz);
}

/*
 * The rate (client 3) of the host commands that set the clock, when it changed. Sony's is a
 * macro: khz is read again for each request (0x26b48, 0x26b78).
 */
#define CLOCK_HOST_REQUEST(khz)						\
	do {								\
		if (wlanMacDescriptor.hostClockApplied != (khz)) {	\
			wlanMacDescriptor.hostClockApplied = (khz);	\
			clockRequestCore((khz), 3);			\
			clockRequestAux((khz), 3);			\
			clockRequestBus((khz), 3);			\
			clockRequestVote((khz), 3);			\
		}							\
	} while (0)

void clockHostSetMax(void)
{
	CLOCK_HOST_REQUEST(128000);
}

void clockHostSetMin(void)
{
	CLOCK_HOST_REQUEST(4000);
}

void clockHostApplyTarget(void)
{
	CLOCK_HOST_REQUEST(wlanMacDescriptor.hostClockTarget);
}

void clockHostSelectPreset0(void)
{
	wlanMacDescriptor.hostClockTarget = wlanMacDescriptor.hostClockPreset[0];
	CLOCK_HOST_REQUEST(wlanMacDescriptor.hostClockPreset[0]);
}

void clockHostSelectPreset1(void)
{
	wlanMacDescriptor.hostClockTarget = wlanMacDescriptor.hostClockPreset[1];
	CLOCK_HOST_REQUEST(wlanMacDescriptor.hostClockPreset[1]);
}

void clockHostSelectPreset2(void)
{
	wlanMacDescriptor.hostClockTarget = wlanMacDescriptor.hostClockPreset[2];
	CLOCK_HOST_REQUEST(wlanMacDescriptor.hostClockPreset[2]);
}

void clockHostSelectPreset3(void)
{
	wlanMacDescriptor.hostClockTarget = wlanMacDescriptor.hostClockPreset[3];
	CLOCK_HOST_REQUEST(wlanMacDescriptor.hostClockPreset[3]);
}

/* Holds the event paths while the engine or a host command works. */
void socEventsHold(void)
{
	SOC_EVENT_CONTROL |= SOC_EVENT_CONTROL_CRYPTO_MASK;
}

unsigned int socEventsRelease(void)
{
	unsigned int control = SOC_EVENT_CONTROL & ~SOC_EVENT_CONTROL_CRYPTO_MASK;

	SOC_EVENT_CONTROL = control;
	return control;
}

void hostCmd0096Enable(void)
{
}

void hostCmd0096Disable(void)
{
}

unsigned int hostCmd0096Get(void)
{
	return 0;
}

/* The offset (sign and six bits) the FM state holds for the temperature sensor. */
int socSensorCalibration(void)
{
	uint8_t value;

	if (socSensorOffset == SENSOR_OFFSET_UNREAD) {
		socSensorOffset = 0;
		if (otpRead(10, &value, 1) == 0 && (value & 0x80)) {
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
	do {
		uint32_t sample = SOC_REG(0xf8);

		tries++;
		if (sample & 0x20000) {
			value = (sample << 6) >> 24;
			break;
		}
	} while (tries < 1000);
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

	SOC_REG(0x214) = value;
	return value;
}

/* The clock and power preset of SOC_REG(0x208..0x228) of the revision. */
void socPowerPreset(void)
{
	uint32_t loops;

	if ((uint8_t)SOC_REG(0x18) == SOC_CHIP_REVISION_40) {
		SOC_REG(0x208) = 0x0e001830;
		SOC_REG(0x20c) = 0x24960000;
		SOC_REG(0x210) = ((SOC_REG(0x210) >> 8) << 8) + 246;
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
		SOC_REG(0x20c) |= 1;
		loops = (uint32_t)(((uint64_t)sysClockFrequency + sysClockFrequency) >> 10);
		if (loops != 0)
			robinDelayLoop(loops);
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
	} else {
		SOC_REG(0x208) = 0x0e001870;
		SOC_REG(0x20c) = 0x249a0000;
		SOC_REG(0x210) = ((SOC_REG(0x210) >> 24) << 24) | 0xd2;
		SOC_REG(0x228) = 0;
	}
}

void socEnableBlockA(void)
{
	SOC_REG(0x48) |= 0x80;
	SOC_REG(0x150) |= 0x200;
}

void socHookNop3(void)
{
}

/* Switches the audio clock on for mode 3 when the argument is nonzero. */
void uapPrepareEntry(uint8_t entry)
{
	btAudioPathClockHook(entry, 3);
}

/* Calibrates the oscillator trim of the radio against the one the FM state holds. */
void rfCalibrateTrim(void)
{
	uint8_t state;
	int trim;
	uint8_t status;

	if (otpRead(0, &state, 1) != 0 || !(state & 0x40)) {
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

void socHookNop4(int argument)
{
}

/* Whether the strap in SOC_REG(0) bit 4 is set. */
int socStrapIsSet(void)
{
	return (SOC_REG(0) & 0x10) != 0;
}

/* The sleep clock: the external one when the strap is set, else the internal oscillator. */
void sleepClockSelect(void)
{
	uint32_t rate;

	if (socStrapIsSet())
		rate = sleepClockUseExternal();
	else
		rate = sleepClockUseInternal();
	sleepClockRate = rate;
}

uint32_t sleepClockRateGet(void)
{
	return sleepClockRate;
}

void sysDelayHookInit(void)
{
	sysDelayHook = sysDelayUs;
}

unsigned int sysPendingEventPolicyIsEnabled(void)
{
	return SOC_REG(0x70) & 0x10000;
}

/* The host command that reads and, with an action of 1, writes the registers of socHostRegisters. */
unsigned int hostCmd008fResolve(uint8_t *body)
{
	unsigned int length = 36;
	uint8_t *data;
	int i;
	uint32_t value;

	if ((body[0] | (body[1] << 8)) == 1) {
		if ((body[4] | (body[5] << 8)) != 36)
			return 0;
		data = body + 6;
		for (i = 0; i < 9; i++) {
			value = data[0] | (data[1] << 8) | (data[2] << 16) | ((uint32_t)data[3] << 24);
			*socHostRegisters[i].reg = (value & socHostRegisters[i].mask) |
						   (*socHostRegisters[i].reg & ~socHostRegisters[i].mask);
			data += 4;
		}
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
		SOC_REG(0x20c) |= 1;
		sysDelayUs(1);
		SOC_REG(0x20c) = (SOC_REG(0x20c) >> 1) << 1;
	}
	data = body + 6;
	for (i = 0; i < 9; i++) {
		value = *socHostRegisters[i].reg & socHostRegisters[i].mask;
		data[0] = value;
		data[1] = value >> 8;
		data[2] = value >> 16;
		data[3] = value >> 24;
		data += 4;
	}
	body[4] = length;	/* the body may be unaligned: written byte by byte */
	body[5] = length >> 8;
	return length;
}

int wlanPsSetupEntry(void)
{
	return wlanPsSetupHeader();
}

/* Loads the control word of a mode; the first four modes also exist as constants of 0x7f. */
static int __attribute__((noinline)) rfBusSetControl(unsigned int mode)
{
	uint16_t control = 0x10;

	if (mode < 10)
		control = rfBusControlModes[mode];
	RFBUS_CONTROL = control;
	return 1;
}
