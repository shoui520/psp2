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
#include "csi.h"
#include "kernel.h"
#include "spidev.h"

void spiDevInit(void)
{
}

/*
 * PLACEHOLDER: cryptographic routine, deferred (the device's authentication: with the device
 * powered and ready, the two words of registers 0x88 and 0x8c are encrypted with AES-256
 * under the key schedule at 0xfaa4 and written to registers 0x80 and 0x84; the result is
 * reported with logEvent and the host link resumed). Returns the last transfer's result.
 */
unsigned char spiDevAuthenticate(void)
{
	return 1;
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
