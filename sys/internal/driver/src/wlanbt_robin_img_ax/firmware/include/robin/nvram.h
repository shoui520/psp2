/*
 * The chip's non-volatile stores (sys/nvram.c, 0x4a348..0x4a818): its
 * one-time programmable bytes, and an external EEPROM (I2C, 24Cxx) or serial
 * flash (SPI), both behind the serial bus of 0x90000800 (RFBUS_*), and the
 * SQRAM layer above them (sqram/nvram.c, 0xc00088dc..0xc0008b30) that finds
 * the store fitted and reads and writes it (wlanSessionOps.eeprom*).
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
#define OTP_DATA(offset)	REG16(0x90000400 + 2 * (offset))	/* the register of the even byte offset's halfword */

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

int otpRead(unsigned int offset, uint8_t *buffer, unsigned int count);	/* 0x4a348 */
void otpClockOpen(void);						/* 0x4a436 */
void otpClockClose(void);						/* 0x4a44a */
int eepromProbe(void);						/* 0x4a46c */
int flashProbe(void);						/* 0x4a4f4 */
void serialBusIdleEnter(void);						/* 0x4a570 */
void serialBusIdleLeave(void);						/* 0x4a58a */
uint32_t eepromWriteWord(uint32_t address, uint32_t value);		/* 0x4a5a8 */
uint32_t eepromReadWord(uint32_t address);				/* 0x4a614 */
unsigned int serialTransferWait(void);				/* 0x4a67c */
unsigned int flashWriteEnable(void);					/* 0x4a690 */
uint8_t flashReadStatus(void);					/* 0x4a6a2 */
int flashWriteStatus(uint8_t status);					/* 0x4a6be */
unsigned int flashWriteDisable(void);				/* 0x4a6fc */
uint32_t flashReadWord(uint32_t address);			/* 0x4a70e */
void flashWriteWord(uint32_t address, uint32_t value);		/* 0x4a75e */
void flashEraseSector(uint16_t sector);				/* 0x4a7c2 */

/* The store the layer looks for: its type and the drivers' modes (nvramProbe() takes it by value). */
struct NvramConfig {
	uint8_t type;			/* NVRAM_TYPE_* */
	uint8_t addressMode;		/* the EEPROM's eepromAddressMode, or the flash's address bytes */
	uint8_t mode;			/* the EEPROM's eepromPresent, or flashMode */
	uint8_t reserved;
};

_Static_assert(sizeof(struct NvramConfig) == 4, "store configuration");

#define NVRAM_TYPE_FLASH	1
#define NVRAM_TYPE_EEPROM	2

/* The store found and its drivers. */
struct NvramDevice {
	uint8_t ready;			/* the store was looked for */
	uint8_t writeEnabled;		/* NVRAM_CONTROL_WRITE_ENABLE */
	uint8_t reserved[2];
	uint32_t eraseSize;		/* NVRAM_CONTROL_SET_ERASE: the size erase works on (64 KiB), 0: none */
	union {
		struct NvramConfig config;
		uint32_t configWord;	/* NVRAM_CONTROL_GET_CONFIG, NVRAM_CONTROL_SET_CONFIG */
	};
	int (*open)(void);
	int (*close)(void);
	uint32_t (*read)(uint32_t address);
	void (*write)(uint32_t address, uint32_t value);
	void (*erase)(uint16_t sector);	/* a flash's, once NVRAM_CONTROL_SET_ERASE allowed it */
};

_Static_assert(sizeof(struct NvramDevice) == 32, "store");

#define NVRAM_PROBE_CONFIGS	5

extern struct NvramDevice *nvramDevice;					/* 0x54da8 */
extern struct NvramConfig nvramProbeConfigs[NVRAM_PROBE_CONFIGS];	/* 0x54dac */

/* nvramControl() commands. */
#define NVRAM_CONTROL_WRITE_DISABLE	0
#define NVRAM_CONTROL_WRITE_ENABLE	1
#define NVRAM_CONTROL_WRITE_STATUS	2	/* a flash's status register */
#define NVRAM_CONTROL_READ_STATUS	3
#define NVRAM_CONTROL_GET_CONFIG	0x100
#define NVRAM_CONTROL_SET_CONFIG	0x101
#define NVRAM_CONTROL_DETECT		0x102
#define NVRAM_CONTROL_GET_ERASE		0x103
#define NVRAM_CONTROL_SET_ERASE		0x104

int nvramProbe(struct NvramConfig config);					/* SQRAM 0xc00088dc */
int nvramDetect(void);								/* SQRAM 0xc0008922 */
int nvramConfigure(void);							/* SQRAM 0xc0008962 */
int nvramOpen(void);								/* SQRAM 0xc00089b6 */
int nvramClose(void);								/* SQRAM 0xc00089e6 */
int nvramRead(unsigned int offset, uint32_t *value);				/* SQRAM 0xc00089ee */
int nvramWrite(unsigned int offset, uint32_t value);				/* SQRAM 0xc0008a00 */
int nvramControl(unsigned int command, uint32_t argument, uint32_t *result);	/* SQRAM 0xc0008a1c */

#endif /* ROBIN_NVRAM_H */
