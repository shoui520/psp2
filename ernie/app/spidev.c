/*
 * The device on the SPI bus: CSI00 opened most significant bit first (csiOpenMsb) while
 * the host link is paused (event flag 1, bit 0x0080; bit 0x0040 resumes it). The device is
 * powered by P5.7 and P1.7, tells it is ready on P1.6, and is selected by P9.0 for a
 * transfer: a command byte and a register's number, most significant byte first, then the
 * register's data either way.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "chip.h"
#include "aes.h"
#include "authkeys.h"
#include "board.h"
#include "csi.h"
#include "kernel.h"
#include "log.h"
#include "spidev.h"

void spiDevInit(void)
{
}

/* Authenticate the device, then release the host link. */
unsigned char spiDevAuthenticate(void)
{
	union {
		unsigned long words[4];
		unsigned char bytes[16];
	} block;
	struct AesKey key;
	unsigned char result = 0;
	unsigned char timeout = 0;
	unsigned char retries;
	unsigned long word;

	set_flg(1, 0x0080);
	dly_tsk(3);
	P5.7 = 1;
	P1.7 = 1;
	retries = 60;
	while (!P1.6) {
#if BOARD_WDT
		WDTE = 0xac;
#endif
		dly_tsk(50);
		if (--retries == 0) {
			timeout = 1;
			break;
		}
	}
	result = spiDevRead(SPIDEV_READ, 0x88, block.bytes);
	word = block.words[0];
	block.words[0] = (word << 24) | ((word << 8) & 0x00ff0000UL) |
			 ((word >> 8) & 0x0000ff00UL) | (word >> 24);
	result = spiDevRead(SPIDEV_READ, 0x8c, block.bytes + 4);
	word = block.words[1];
	block.words[1] = (word << 24) | ((word << 8) & 0x00ff0000UL) |
			 ((word >> 8) & 0x0000ff00UL) | (word >> 24);
	memcpy(block.bytes + 8, block.bytes, 8);
	key.w = (unsigned long *)factoryRoundKeys;
	key.nk = 8;
	key.nb = 4;
	aesEncrypt((unsigned char __near *)block.bytes, (unsigned char __near *)block.bytes,
		   (struct AesKey __near *)&key);
	word = block.words[0];
	block.words[0] = (word << 24) | ((word << 8) & 0x00ff0000UL) |
			 ((word >> 8) & 0x0000ff00UL) | (word >> 24);
	result = spiDevWrite(SPIDEV_WRITE, 0x80, block.bytes);
	word = block.words[1];
	block.words[1] = (word << 24) | ((word << 8) & 0x00ff0000UL) |
			 ((word >> 8) & 0x0000ff00UL) | (word >> 24);
	result = spiDevWrite(SPIDEV_WRITE, 0x84, block.bytes + 4);
	memset(block.bytes, 0, sizeof(block.bytes));
	P5.7 = 0;
	retries = 60;
	while (P1.6) {
#if BOARD_WDT
		WDTE = 0xac;
#endif
		dly_tsk(50);
		if (--retries == 0) {
			timeout = 1;
			break;
		}
	}
	logEvent(timeout ? 0x14 : 0x15);
	set_flg(1, 0x0040);
	return result;
}

/* reads a register's 4 bytes; the transfer's result */
unsigned char spiDevRead(unsigned char command, unsigned int reg, unsigned char *data)
{
	unsigned char result;
	unsigned char send[3];
	unsigned char receive[3 + 4];

	result = 0;
	send[0] = command;
	send[1] = reg >> 8;
	send[2] = reg & 0xff;
	P90 = 0;
	result = csiTransfer(receive, sizeof(receive), send, sizeof(send));
	dly_tsk(3);
	P90 = 1;
	memcpy(data, &receive[3], 4);
	return result;
}

/* reads length bytes (up to 32) from a register */
unsigned char spiDevReadBlock(unsigned char command, unsigned int reg, unsigned char *data,
			      unsigned char length)
{
	unsigned char result;
	unsigned char send[3];
	unsigned char receive[3 + 32];

	result = 0;
	send[0] = command;
	send[1] = reg >> 8;
	send[2] = reg & 0xff;
	P90 = 0;
	result = csiTransfer(receive, length + 3, send, sizeof(send));
	dly_tsk(3);
	P90 = 1;
	memcpy(data, &receive[3], length);
	return result;
}

/* writes a register's 4 bytes */
unsigned char spiDevWrite(unsigned char command, unsigned int reg, unsigned char *data)
{
	unsigned char result;
	unsigned char send[3 + 4];

	result = 0;
	send[0] = command;
	send[1] = reg >> 8;
	send[2] = reg & 0xff;
	memcpy(&send[3], data, 4);
	P90 = 0;
	result = csiTransfer(NULL, 0, send, sizeof(send));
	dly_tsk(3);
	P90 = 1;
	return result;
}

/* pauses the host link and powers the device */
unsigned char spiDevPowerOn(void)
{
	unsigned char result;

	result = 0;
	set_flg(1, 0x0080);
	dly_tsk(3);
	P5.7 = 1;
	P1.7 = 1;
	return result;
}
