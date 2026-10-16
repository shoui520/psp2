/*
 * Serial register bus to the radio. Access is bracketed by rfBusLock() and
 * rfBusUnlock(), which keep interrupts masked for the whole sequence and
 * nest. The filename is inferred.
 */
#include <stddef.h>
#include "robin/rfbus.h"
#include "robin/soc.h"
#include "robin/threadx.h"

#define RFBUS_LOCK_FREE		0xff
#define RFBUS_LOCK_HELD		2
#define RFBUS_CONFIGURATION_MODE 0x358
#define RFBUS_CONFIGURATION_KEEP 0x400

/* Non-zero once the bus has been brought up; the accessors do nothing before. */
uint32_t rfBusEnabled __attribute__((section(".itcm.bss")));
static uint8_t rfBusLockState __attribute__((section(".itcm.rw"))) = RFBUS_LOCK_FREE;
static uint32_t rfBusOwnerPosture __attribute__((section(".itcm.bss")));
static uint32_t rfBusLockDepth __attribute__((section(".itcm.bss")));

/* Constant words in read-only data; the third one goes to the control register. */
static const uint16_t rfBusSettings[3] = { 0x000f, 0x007f, 0x0001 };

/* Lets the accessors touch the bus. */
int rfBusEnable(void)
{
	rfBusEnabled = 1;
	return 1;
}

void rfBusWaitIdle(void)
{
	while (RFBUS_STATUS & RFBUS_STATUS_BUSY)
		;
}

/*
 * Takes the bus. The first holder keeps interrupts masked and programs the
 * configuration; a nested caller must already have them masked.
 */
void rfBusLock(void)
{
	unsigned int posture = tx_interrupt_control(TX_INT_DISABLE);
	unsigned int kept;

	if (rfBusLockState == RFBUS_LOCK_HELD) {
		++rfBusLockDepth;
		if (!(posture & TX_INT_DISABLE))
			for (;;)
				;
		tx_interrupt_control(posture);
		return;
	}
	if (rfBusOwnerPosture != 0)
		for (;;)
			;
	rfBusOwnerPosture = posture;
	rfBusLockState = RFBUS_LOCK_HELD;
	rfBusLockDepth = 1;
	kept = RFBUS_CONFIGURATION & RFBUS_CONFIGURATION_KEEP;
	if ((RFBUS_CONFIGURATION & ~kept) != RFBUS_CONFIGURATION_MODE) {
		RFBUS_CONTROL = rfBusSettings[2];
		RFBUS_CONFIGURATION = kept | RFBUS_CONFIGURATION_MODE;
	}
}

void rfBusUnlock(void)
{
	unsigned int posture;

	--rfBusLockDepth;
	if (rfBusLockDepth != 0) {
		if ((int)rfBusLockDepth < 0) {
			tx_interrupt_control(TX_INT_DISABLE);
			for (;;)
				;
		}
		return;
	}
	posture = rfBusOwnerPosture;
	rfBusOwnerPosture = 0;
	rfBusLockState = RFBUS_LOCK_FREE;
	tx_interrupt_control(posture);
}

void rfBusAcquire(void)
{
	rfBusLock();
}

void rfBusRelease(void)
{
	rfBusUnlock();
}

uint8_t rfBusRead(uint16_t reg)
{
	if (!rfBusEnabled)
		return 0;
	rfBusWaitIdle();
	RFBUS_ADDRESS = reg;
	return RFBUS_DATA;
}

void rfBusWrite(uint16_t reg, uint8_t value)
{
	if (!rfBusEnabled)
		return;
	rfBusWaitIdle();
	RFBUS_ADDRESS = reg;
	RFBUS_DATA = value;
}

void rfBusWriteTable(const struct PhyRegisterEntry *table)
{
	if (table == NULL)
		return;
	while (!(table->reg == 0xffff && table->value == 0xff)) {
		rfBusWrite(table->reg, table->value);
		table++;
	}
}

void rfBusSetBits(uint16_t reg, uint8_t mask)
{
	rfBusWrite(reg, rfBusRead(reg) | mask);
}

void rfBusClearBits(uint16_t reg, uint8_t mask)
{
	rfBusWrite(reg, rfBusRead(reg) & ~mask);
}

/* Writes register 0x1bb + 0x100 * index under the bus lock. */
void rfBusWriteIndexed(int index, uint8_t value)
{
	uint16_t reg = (index << 8) + 443;

	rfBusAcquire();
	rfBusWrite(reg, value);
	rfBusRelease();
}
