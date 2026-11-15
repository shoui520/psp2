/*
 * The chip's non-volatile stores (sys/nvram.c, 0x4a348..0x4a818): its
 * one-time programmable bytes, and an external EEPROM (I2C, 24Cxx) or serial
 * flash (SPI), both behind the serial bus of 0x90000800 (RFBUS_*). The SQRAM
 * code that reads the host's EEPROM (wlanSessionOps.eeprom*) sets the modes.
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_NVRAM_H
#define ROBIN_NVRAM_H

#include <stdint.h>

/* The ID word at the start of the external store, big-endian. */
#define NVRAM_MAGIC		0x38333058

/* One-time programmable bytes: two in the low half of each word register. */
#define OTP_BYTES		32
#define OTP_CONTROL		REG16(0x90000604)	/* bits 7..6: the bank strobes */
#define OTP_COMMAND		REG16(0x90000608)
#define OTP_DATA(n)		REG16(0x90000400 + 4 * (n))

/* The serial bus as a controller: chip select, transfer control and data. */
#define SERIAL_SELECT		REG8(0x90000810)	/* 2: start a transfer, 1: end it */
#define SERIAL_TRANSFER		REG16(0x90000808)
#define SERIAL_TRANSFER_WORD	REG32(0x90000808)
#define SERIAL_TRANSFER_BUSY	0x400
#define SERIAL_DATA_WORD	REG32(0x90000824)
#define SERIAL_STATUS		REG8(0x9000081c)
#define SERIAL_STATUS_NACK	0x08

#define SERIAL_SELECT_START	2
#define SERIAL_SELECT_STOP	1

/* I2C EEPROM device addresses (24Cxx). */
#define EEPROM_WRITE		0xa0
#define EEPROM_READ		0xa1

/* SPI flash commands. */
#define FLASH_WRITE_STATUS	0x01
#define FLASH_PAGE_PROGRAM	0x02
#define FLASH_READ		0x03
#define FLASH_WRITE_DISABLE	0x04
#define FLASH_READ_STATUS	0x05
#define FLASH_WRITE_ENABLE	0x06
#define FLASH_RELEASE_POWER_DOWN 0xab
#define FLASH_SECTOR_ERASE	0xd8

#define FLASH_STATUS_BUSY	0x01

/* How the external SPI flash is driven. */
struct FlashConfig {
	uint8_t addressBytes;		/* 2 or 3 */
	uint8_t reserved[3];
	uint32_t statusTimeout;		/* polls of 1 ms after a status write */
	uint32_t programTimeout;	/* after a page program */
	uint32_t eraseTimeout;		/* after a sector erase */
};

_Static_assert(sizeof(struct FlashConfig) == 16, "SPI flash configuration");

extern uint8_t eepromAddressMode;	/* 0x553cc: 1: one address byte, 2: two, else none */
extern uint8_t eepromPresent;		/* 0x553cd */
extern struct FlashConfig flashConfig;	/* 0x553d0 */
extern uint8_t flashMode;		/* 0x553e0: 1 or 2: a flash is fitted, 2: wake it */
extern uint8_t flashId;			/* 0x553e1: what the release from power-down answered */

int fmCoexSetupState(unsigned int offset, uint8_t *buffer, unsigned int count);	/* 0x4a348 */
void fmCoexClearState(void);						/* 0x4a436 */
void fmCoexConvertState(void);						/* 0x4a44a */
int fmCoexVerifyState(void);						/* 0x4a46c */
int fmCoexSelectState(void);						/* 0x4a4f4 */
void fmCoexAdjustState(void);						/* 0x4a570 */
void fmCoexRecordState(void);						/* 0x4a58a */
uint32_t fmCoexFilterState(uint32_t address, uint32_t value);		/* 0x4a5a8 */
uint32_t fmCoexHandleContext(uint32_t address);				/* 0x4a614 */
unsigned int fmCoexProcessContext(void);				/* 0x4a67c */
unsigned int fmCoexUpdateContext(void);					/* 0x4a690 */
uint8_t fmCoexConfigureContext(void);					/* 0x4a6a2 */
int fmCoexCheckContext(uint8_t status);					/* 0x4a6be */
unsigned int fmCoexValidateContext(void);				/* 0x4a6fc */
uint32_t fmCoexPrepareContext(uint32_t address);			/* 0x4a70e */
void fmCoexDispatchContext(uint32_t address, uint32_t value);		/* 0x4a75e */
void fmCoexParseContext(uint16_t sector);				/* 0x4a7c2 */

#endif /* ROBIN_NVRAM_H */
