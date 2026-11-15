/*
 * The non-volatile stores (0x4a348..0x4a818): the one-time programmable
 * bytes, an external I2C EEPROM and an external SPI flash on the serial bus,
 * and the clock drop around the idle wait.
 *
 * Marvell published no source; names are inferred from use.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/nvram.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

/* The clock client whose bus request the idle wait drops to. */
#define CLOCK_CLIENT_IDLE	6

/* Polls of the serial bus before a transfer is given up. */
#define SERIAL_TRANSFER_POLLS	1000
/* The wait between two flash status polls, in microseconds. */
#define FLASH_POLL_US		1000

uint8_t eepromAddressMode __attribute__((section(".itcm.rw")));	/* 0x553cc */
uint8_t eepromPresent __attribute__((section(".itcm.rw")));		/* 0x553cd */
struct FlashConfig flashConfig __attribute__((section(".itcm.rw")));	/* 0x553d0 */
uint8_t flashMode __attribute__((section(".itcm.rw")));		/* 0x553e0 */
uint8_t flashId __attribute__((section(".itcm.rw")));			/* 0x553e1 */

/* Reads count of the one-time programmable bytes from offset on; 1 for a bad request. */
int otpRead(unsigned int offset, uint8_t *buffer, unsigned int count)
{
	UINT posture;
	unsigned int bank;
	unsigned int strobe;
	uint16_t control;
	uint16_t word;

	if (count == 0 || buffer == NULL || offset >= OTP_BYTES)
		return 1;
	posture = sysIrqDisable();
	otpClockOpen();
	OTP_COMMAND = 0;
	for (bank = 0; bank < 1; bank++) {
		control = 0xfc;
		OTP_CONTROL = control;
		strobe = 1 << bank;
		control &= ((~strobe & 3) << 6) | ~0xc0;
		OTP_CONTROL = control;
		sysDelayUs(1);
		control = (control & ~0xc0) | ((((control >> 6) & 3) | strobe) & 3) << 6;
		OTP_CONTROL = control;
		sysDelayUs(1);
	}
	if (offset & 1) {
		*buffer++ = OTP_DATA(offset >> 1) >> 8;
		offset++;
		count--;
	}
	if (count > OTP_BYTES - offset)
		count = OTP_BYTES - offset;
	while (count >= 2) {
		word = OTP_DATA(offset >> 1);
		*buffer++ = word;
		*buffer++ = word >> 8;
		offset += 2;
		count -= 2;
	}
	if (count == 1)
		*buffer = OTP_DATA(offset >> 1);
	otpClockClose();
	sysIrqRestore(posture);
	return 0;
}

/* Opens the one-time programmable store's clock. */
__attribute__((noinline)) void otpClockOpen(void)
{
	SOC_EVENT_CONTROL = SOC_EVENT_CONTROL;
	SOC_SLEEP_MEASURE_CONTROL |= 4;
}

__attribute__((noinline)) void otpClockClose(void)
{
	SOC_EVENT_CONTROL = SOC_EVENT_CONTROL;
	SOC_SLEEP_MEASURE_CONTROL &= ~4;
}

/* The EEPROM's device address and the word address after a start. */
static inline void eepromSendAddress(unsigned int mode, uint32_t address)
{
	if (mode == 1) {
		RFBUS_DATA = ((address >> 7) & 0xe) + EEPROM_WRITE;
		RFBUS_DATA = address;
	} else if (mode == 2) {
		RFBUS_DATA = ((address >> 16) & 7) + EEPROM_WRITE;
		RFBUS_DATA = address >> 8;
		RFBUS_DATA = address;
	}
}

/* Whether bytes 1..3 of the EEPROM hold the ID word's. */
int eepromProbe(void)
{
	int match = 1;
	int stopped;
	unsigned int mode;
	int i;

	if (eepromPresent != 1)
		return 0;
	mode = eepromAddressMode;
	SERIAL_SELECT = SERIAL_SELECT_START;
	RFBUS_DATA = EEPROM_WRITE;
	if (mode == 1) {
		RFBUS_DATA = 1;
	} else if (mode == 2) {
		RFBUS_DATA = 0;
		RFBUS_DATA = 1;
	}
	SERIAL_SELECT = SERIAL_SELECT_START;
	RFBUS_DATA = EEPROM_READ;
	stopped = 0;
	for (i = 1; i <= 3; i++) {
		if (i == 3) {
			stopped = 1;
			SERIAL_SELECT = SERIAL_SELECT_STOP;
		}
		if ((uint8_t)SERIAL_DATA_WORD != (uint8_t)(NVRAM_MAGIC >> (24 - 8 * i))) {
			match = 0;
			break;
		}
	}
	if (!stopped) {
		SERIAL_SELECT = SERIAL_SELECT_STOP;
		(void)SERIAL_DATA_WORD;
	}
	return match;
}

/* Whether a flash with the ID word is fitted; in mode 2 it is woken and unprotected. */
int flashProbe(void)
{
	int found = 0;
	uint8_t id;

	if (flashMode != 1 && flashMode != 2)
		return 0;
	if (flashReadWord(0) == NVRAM_MAGIC)
		found = 1;
	if (flashMode == 2 && found == 1) {
		SERIAL_TRANSFER_WORD = 0x4c2;
		SERIAL_DATA_WORD = FLASH_RELEASE_POWER_DOWN;
		SERIAL_DATA_WORD = 0;
		SERIAL_DATA_WORD = 0;
		SERIAL_DATA_WORD = 1;
		id = SERIAL_DATA_WORD;
		serialTransferWait();
		if (id == 0xff) {
			found = 0;
		} else {
			flashId = id;
			flashWriteEnable();
			flashWriteStatus(0);
			flashWriteDisable();
		}
	}
	return found;
}

/* Before the idle wait: the bus clock drops to the idle client's vote. */
void serialBusIdleEnter(void)
{
	uint32_t bus = clockRequestedBus(CLOCK_CLIENT_IDLE);
	uint32_t vote = clockRequestedVote(CLOCK_CLIENT_IDLE);

	if (vote != bus)
		clockSetBus(vote);
}

/* After the idle wait: the bus clock comes back. */
void serialBusIdleLeave(void)
{
	uint32_t bus = clockRequestedBus(CLOCK_CLIENT_IDLE);

	if (clockRequestedVote(CLOCK_CLIENT_IDLE) != bus)
		clockSetBus(bus);
}

/* Writes a word to the EEPROM, waits for it to finish and returns what a dummy read gives. */
uint32_t eepromWriteWord(uint32_t address, uint32_t value)
{
	unsigned int mode;

	SERIAL_SELECT = SERIAL_SELECT_START;
	mode = eepromAddressMode;
	eepromSendAddress(mode, address);
	RFBUS_DATA = value >> 24;
	RFBUS_DATA = value >> 16;
	RFBUS_DATA = value >> 8;
	SERIAL_SELECT = SERIAL_SELECT_STOP;
	RFBUS_DATA = value;
	do {
		SERIAL_SELECT = SERIAL_SELECT_START;
		RFBUS_DATA = EEPROM_WRITE;
	} while (SERIAL_STATUS & SERIAL_STATUS_NACK);
	if (mode == 1) {
		RFBUS_DATA = 0;
	} else if (mode == 2) {
		RFBUS_DATA = 0;
		RFBUS_DATA = 0;
	}
	SERIAL_SELECT = SERIAL_SELECT_START;
	RFBUS_DATA = EEPROM_READ;
	SERIAL_SELECT = SERIAL_SELECT_STOP;
	return SERIAL_DATA_WORD;
}

/* Reads a big-endian word of the EEPROM. */
uint32_t eepromReadWord(uint32_t address)
{
	uint32_t value = 0;
	short i;

	SERIAL_SELECT = SERIAL_SELECT_START;
	eepromSendAddress(eepromAddressMode, address);
	SERIAL_SELECT = SERIAL_SELECT_START;
	RFBUS_DATA = EEPROM_READ;
	for (i = 0; i < 4;) {
		value = value << 8 | (uint8_t)SERIAL_DATA_WORD;
		i++;
		if (i == 3)
			SERIAL_SELECT = SERIAL_SELECT_STOP;
	}
	return value;
}

/* Waits for the serial transfer; returns the polls left. */
__attribute__((noinline)) unsigned int serialTransferWait(void)
{
	unsigned int polls = SERIAL_TRANSFER_POLLS;

	while (SERIAL_TRANSFER_WORD & SERIAL_TRANSFER_BUSY)
		if (polls-- == 0)
			break;
	return polls;
}

__attribute__((noinline)) unsigned int flashWriteEnable(void)
{
	SERIAL_TRANSFER = 0x420;
	RFBUS_DATA = FLASH_WRITE_ENABLE;
	return serialTransferWait();
}

/* The flash's status register. */
__attribute__((noinline)) uint8_t flashReadStatus(void)
{
	uint8_t status;

	SERIAL_TRANSFER = 0x408;
	RFBUS_DATA = FLASH_READ_STATUS;
	status = RFBUS_DATA;
	serialTransferWait();
	return status;
}

/* Writes the flash's status register; 0xff when it stays busy. */
int flashWriteStatus(uint8_t status)
{
	uint32_t timeout = flashConfig.statusTimeout;

	SERIAL_TRANSFER = 0x404;
	RFBUS_DATA = FLASH_WRITE_STATUS;
	RFBUS_DATA = status;
	serialTransferWait();
	while (flashReadStatus() & FLASH_STATUS_BUSY) {
		sysDelayHook(FLASH_POLL_US);
		if (timeout-- == 0)
			return 0xff;
	}
	return 0;
}

__attribute__((noinline)) unsigned int flashWriteDisable(void)
{
	SERIAL_TRANSFER = 0x410;
	RFBUS_DATA = FLASH_WRITE_DISABLE;
	return serialTransferWait();
}

/* The flash address, in as many bytes as the flash takes. */
static inline void flashSendAddress(unsigned int bytes, uint32_t address)
{
	if (bytes == 2 || bytes == 3) {
		if (bytes == 3)
			RFBUS_DATA = address >> 16;
		RFBUS_DATA = address >> 8;
		RFBUS_DATA = address;
	}
}

/* Reads a big-endian word of the flash. */
uint32_t flashReadWord(uint32_t address)
{
	unsigned int bytes = flashConfig.addressBytes;
	uint32_t value = 0;
	uint16_t i;

	SERIAL_TRANSFER = ((bytes + 3) << 6) | 0x402;
	RFBUS_DATA = FLASH_READ;
	flashSendAddress(bytes, address);
	serialTransferWait();
	for (i = 0; i < 4; i++)
		value = value << 8 | RFBUS_DATA;
	return value;
}

/* Programs a big-endian word of the flash and waits for it. */
void flashWriteWord(uint32_t address, uint32_t value)
{
	unsigned int bytes = flashConfig.addressBytes;
	uint32_t timeout = flashConfig.programTimeout;

	SERIAL_TRANSFER = ((bytes + 3) << 6) | 0x401;
	RFBUS_DATA = FLASH_PAGE_PROGRAM;
	flashSendAddress(bytes, address);
	RFBUS_DATA = value >> 24;
	RFBUS_DATA = value >> 16;
	RFBUS_DATA = value >> 8;
	RFBUS_DATA = value;
	serialTransferWait();
	while (flashReadStatus() & FLASH_STATUS_BUSY) {
		sysDelayHook(FLASH_POLL_US);
		if (timeout-- == 0)
			return;
	}
}

/* Erases a 64 KiB sector of the flash and waits for it. */
void flashEraseSector(uint16_t sector)
{
	uint32_t address = (uint32_t)sector << 16;
	unsigned int bytes;
	uint32_t timeout = flashConfig.eraseTimeout;

	SERIAL_TRANSFER = 0x420;
	bytes = flashConfig.addressBytes;
	SERIAL_TRANSFER = ((bytes - 1) << 6) | 0x401;
	RFBUS_DATA = FLASH_SECTOR_ERASE;
	if (bytes == 3)
		RFBUS_DATA = address >> 16;
	RFBUS_DATA = address >> 8;
	RFBUS_DATA = address;
	serialTransferWait();
	while (flashReadStatus() & FLASH_STATUS_BUSY) {
		sysDelayHook(FLASH_POLL_US);
		if (timeout-- == 0)
			return;
	}
}
