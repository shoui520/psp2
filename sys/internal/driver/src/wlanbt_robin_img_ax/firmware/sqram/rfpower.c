/*
 * The radio's power-up and the sleep clock's source (SQRAM 0xc0008b30..
 * 0xc0008ea4). The power-up is the first thing the init stage
 * rfCalibInitStage() does: every clock on, each 64 KiB bank of SQRAM and
 * ITCM read once, the PLL programmed, then the radio's power sequence
 * through its registers 199 and 200, its start-up calibration (registers
 * 0x46, 0x16f and 0x170) and the bus registers written directly. No Marvell
 * source names any of it; the filename and the names are inferred.
 */
#include <stdint.h>
#include "robin/clock.h"
#include "robin/gpio.h"
#include "robin/rfbus.h"
#include "robin/rfreg.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

#define MEMORY_BANK_SIZE	0x10000u
#define SQRAM_START		0xc0000000u
#define SQRAM_END		0xc0040000u
#define ITCM_END		0x00060000u
#define DTCM_START		0x04000000u
#define SERIAL_WINDOW		0x90014000u

/* The radio's power control and status registers and their bits. */
#define RF_REG_POWER_CONTROL	199
#define RF_REG_POWER_STATUS	200
#define RF_POWER_ENABLE		0x01
#define RF_POWER_STAGE1		0x02
#define RF_POWER_STAGE2		0x04
#define RF_POWER_STAGE1_DONE	0x01
#define RF_POWER_STAGE2_DONE	0x02

#define RF_REG_SERIAL_RESET	18
#define RF_SERIAL_RESET		0x80

#define RF_REG_CALIBRATION	0x46
#define RF_REG_CALIBRATION_START	0x16f
#define RF_REG_CALIBRATION_DONE	0x170
#define RF_CALIBRATION_MODE_MASK	0x0c
#define RF_CALIBRATION_MODE_RUN	0x08
#define RF_CALIBRATION_STEPS	0x0f

#define RF_BUS_DELAY		50

/* Baseband registers read and written a byte at a time. */
#define BBP_REGISTER_BYTE(n)	REG8(0x90002000u + 4u * (n))
#define BBP_REG_RESET		0x2bb
#define BBP_REG_CLOCK		0x10d
#define BBP_CLOCK_REQUEST	0x80
#define BBP_CLOCK_READY		0x40

/* Waits for the bus to finish, then sets its mode. */
static void __attribute__((noinline, noclone)) rfBusSetMode(uint16_t mode)
{
	while (RFBUS_BUSY & 1)
		;
	RFBUS_MODE = mode;
}

/* Writes a radio register straight through the bus registers, with a delay after each step. */
static inline __attribute__((always_inline)) void rfBusWriteDelayed(uint16_t reg, uint16_t value)
{
	RFBUS_ADDRESS = reg;
	robinDelayLoop(RF_BUS_DELAY);
	RFBUS_DATA16 = value;
	robinDelayLoop(RF_BUS_DELAY);
	(void)RFBUS_DATA16;
}

int rfPowerOn(void)
{
	uintptr_t address;
	unsigned int revision;

	SOC_REG(0x0c) = 0xffffffff;
	SOC_REG(0xa8) = 0xffffffff;
	SOC_REG(0xb4) &= 0xfffffccc;
	for (address = SQRAM_START; address < SQRAM_END; address += MEMORY_BANK_SIZE)
		(void)*(volatile uint32_t *)address;
	for (address = 0; address < ITCM_END; address += MEMORY_BANK_SIZE)
		(void)*(volatile uint32_t *)address;
	(void)REG32(DTCM_START);
	clockPllProgram();
	(void)REG32(SERIAL_WINDOW);
	rfBusLock();
	rfBusSetMode(0);

	rfBusWriteReg16(RF_REG_POWER_CONTROL, rfRegRead(RF_REG_POWER_CONTROL) | RF_POWER_ENABLE);
	rfBusWriteReg16(RF_REG_POWER_CONTROL, rfRegRead(RF_REG_POWER_CONTROL) | RF_POWER_STAGE1);
	while (!(rfRegRead(RF_REG_POWER_STATUS) & RF_POWER_STAGE1_DONE))
		;
	rfBusWriteReg16(RF_REG_POWER_CONTROL, rfRegRead(RF_REG_POWER_CONTROL) & ~RF_POWER_STAGE1);
	rfBusWriteReg16(RF_REG_POWER_CONTROL, rfRegRead(RF_REG_POWER_CONTROL) | RF_POWER_STAGE2);
	while (!(rfRegRead(RF_REG_POWER_STATUS) & RF_POWER_STAGE2_DONE))
		;
	rfBusWriteReg16(RF_REG_POWER_CONTROL,
			rfRegRead(RF_REG_POWER_CONTROL) & ~RF_POWER_STAGE2 & ~RF_POWER_ENABLE);
	rfBusSetMode(0);

	rfRegWrite(RF_REG_SERIAL_RESET, (uint8_t)(rfRegRead(RF_REG_SERIAL_RESET) | RF_SERIAL_RESET));
	rfRegRead(RF_REG_SERIAL_RESET);
	rfBusSetMode(0);
	rfRegWrite(RF_REG_SERIAL_RESET, (uint8_t)(rfRegRead(RF_REG_SERIAL_RESET) & ~RF_SERIAL_RESET));
	rfRegRead(RF_REG_SERIAL_RESET);
	rfBusUnlock();

	SOC_REG(0x7c) |= 1;
	BBP_REGISTER_BYTE(BBP_REG_RESET) = 0;
	(void)BBP_REGISTER_BYTE(0);
	(void)BBP_REGISTER_BYTE(BBP_REG_RESET);
	BBP_REGISTER(BBP_REG_CLOCK) = BBP_REGISTER_BYTE(BBP_REG_CLOCK) | BBP_CLOCK_REQUEST;
	while (!(BBP_REGISTER_BYTE(BBP_REG_CLOCK) & BBP_CLOCK_READY))
		;
	BBP_REGISTER(BBP_REG_CLOCK) = BBP_REGISTER_BYTE(BBP_REG_CLOCK) & ~BBP_CLOCK_REQUEST;

	rfBusEnter1350();
	if (rfRegRead(RF_REG_CALIBRATION_DONE) & RF_CALIBRATION_STEPS)
		for (;;)
			;
	rfBusWriteReg16(RF_REG_CALIBRATION,
			(rfRegRead(RF_REG_CALIBRATION) & ~RF_CALIBRATION_MODE_MASK) + RF_CALIBRATION_MODE_RUN);
	rfBusWriteReg16(RF_REG_CALIBRATION_START,
			(rfRegRead(RF_REG_CALIBRATION_START) & ~0xff) + RF_CALIBRATION_STEPS);
	while ((rfRegRead(RF_REG_CALIBRATION_DONE) & RF_CALIBRATION_STEPS) != RF_CALIBRATION_STEPS)
		;
	rfBusWriteReg16(RF_REG_CALIBRATION_START, 0);
	rfBusWriteReg16(RF_REG_CALIBRATION, rfRegRead(RF_REG_CALIBRATION) & ~RF_CALIBRATION_MODE_MASK);

	rfBusWriteDelayed(0x46, 0x11);
	rfBusWriteDelayed(0x14, 0xb1);
	rfBusWriteDelayed(0x15, 0xc1);
	rfBusWriteDelayed(0x16, 0x80);
	rfBusWriteDelayed(0x15, 0xc0);
	rfBusLeave1350();

	rfRegBegin();
	rfRegSetBits(27, 1);
	revision = SOC_CHIP_REVISION & 0xff;
	if (revision != 0 && (SOC_CHIP_REVISION & 0xff) != 0x10)
		rfRegSetBits(39, 0x80);
	rfRegEnd();
	SOC_REG(0x208) |= 0x10;
	SOC_REG(0x18c) &= ~0x10u;
	return 0;
}

/* The internal oscillator's divider by reference clock profile (sysRefClockProfile()). */
static const uint32_t sleepClockDividers[10] = {
	0x00165ea0, 0x0014a61d, 0x000dfb24, 0x000d6bf9, 0x000b2f50,
	0x000a530f, 0x0006fd92, 0x0006b5fd, 0x000619ce, 0x00052987,
};

#define SLEEP_CLOCK_CONTROL		SOC_REG(0x138)
#define SLEEP_CLOCK_DIVIDER_MASK	0x00ffffffu
#define SLEEP_CLOCK_INTERNAL		0x01000000u
#define SLEEP_CLOCK_EXTERNAL		0x02000000u
#define SLEEP_CLOCK_EXTERNAL_READY	0x04000000u
#define SLEEP_CLOCK_GATE		SOC_REG(0x48)
#define SLEEP_CLOCK_GATE_INTERNAL	0x02000000u

/* Runs the sleep clock from the internal oscillator, its divider set for the reference clock. */
uint32_t sleepClockUseInternal(void)
{
	rfRegBegin();
	rfRegClearBits(0x53, 0x10);
	rfRegClearBits(14, 0x40);
	rfRegClearBits(0x21, 0x0c);
	rfRegSetBits(0x22, 8);
	robinDelayLoop(1000);
	rfRegSetBits(0x53, 0x10);
	rfRegSetBits(0x4a, 2);
	rfRegEnd();
	SLEEP_CLOCK_CONTROL = sleepClockDividers[sysRefClockProfile()] |
			      (SLEEP_CLOCK_CONTROL & ~SLEEP_CLOCK_DIVIDER_MASK);
	SLEEP_CLOCK_CONTROL |= SLEEP_CLOCK_INTERNAL;
	SLEEP_CLOCK_CONTROL &= ~SLEEP_CLOCK_EXTERNAL_READY;
	SLEEP_CLOCK_CONTROL &= ~SLEEP_CLOCK_EXTERNAL;
	SLEEP_CLOCK_GATE |= SLEEP_CLOCK_GATE_INTERNAL;
	gpioPinWrite(0, 1);
	gpioPinSelect(0, 1);
	REG32(0x90000214) &= ~1u;
	REG32(0x90000218) &= ~1u;
	SOC_REG(0x200) |= 1;
	return SLEEP_CLOCK_RATE;
}

/* Runs the sleep clock from the external 32 kHz clock. */
uint32_t sleepClockUseExternal(void)
{
	SLEEP_CLOCK_CONTROL |= SLEEP_CLOCK_EXTERNAL;
	SLEEP_CLOCK_CONTROL &= ~SLEEP_CLOCK_EXTERNAL_READY;
	SLEEP_CLOCK_GATE &= ~SLEEP_CLOCK_GATE_INTERNAL;
	SLEEP_CLOCK_CONTROL &= ~SLEEP_CLOCK_INTERNAL;
	rfRegBegin();
	rfRegClearBits(14, 0x40);
	rfRegSetBits(0x4a, 2);
	rfRegEnd();
	return SLEEP_CLOCK_RATE;
}
