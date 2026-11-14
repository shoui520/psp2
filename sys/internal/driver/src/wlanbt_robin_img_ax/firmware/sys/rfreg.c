/*
 * Register access to the radio through the serial bus in the modes the RAM
 * code uses it (0x26550..0x26842): a mode sets the bus configuration, takes
 * the bus's control word from a table, keeps interrupts masked and puts
 * everything back on leaving. The filename and the names are inferred.
 */
#include <stddef.h>
#include "robin/rfbus.h"
#include "robin/rfreg.h"
#include "robin/soc.h"
#include "robin/sys.h"

#define RFBUS_CONFIGURATION_KEEP	0x400	/* the bit of the configuration every mode keeps */
#define RFREG_LOCK_FREE		0xff

/* The control word each mode of the bus uses, by mode index. */
uint16_t rfBusControlModes[10] __attribute__((section(".sqram1.rw"))) = {
	0x000f, 0x007f, 0x0001, 0x0000, 0x0000, 0x0004, 0x0008, 0x0010, 0x0014, 0x0020,
};
/* The control and configuration words a mode that saves them left. */
static uint16_t rfRegSavedControl __attribute__((section(".sqram1.rw")));
static uint16_t rfRegSavedConfig __attribute__((section(".sqram1.rw")));
static uint8_t rfRegLockState __attribute__((section(".sqram1.rw"))) = RFREG_LOCK_FREE;
/* The interrupt state the mode in use has to restore; non-zero while the bus is taken. */
static uint32_t rfRegPosture __attribute__((section(".sqram1.rw")));

/* Loads the control word of a mode; the first four modes also exist as constants of 0x7f. */
static int __attribute__((noinline)) rfBusSetControl(unsigned int mode)
{
	uint16_t control = 0x10;

	if (mode < 10)
		control = rfBusControlModes[mode];
	RFBUS_CONTROL = control;
	return 1;
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

void rfRegEnd(void)
{
	rfRegGive();
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
