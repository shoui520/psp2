/*
 * The device on the SPI bus, as the updater reads it (serial1.c's 0x0110 exchange): one
 * register's 4 bytes, the device selected by P9.0 for a polled transfer of 7 bytes each way.
 */
#pragma sfr

#include <string.h>
#include "chip.h"
#include "spidev.h"
#include "updater.h"

/* reads a register's 4 bytes; the transfer's result */
unsigned char spiDevRead(unsigned char command, unsigned int reg, unsigned char *data)
{
	unsigned char result;
	unsigned char send[3 + 4];
	unsigned char receive[3 + 4];

	result = 0;
	send[0] = command;
	send[1] = reg >> 8;
	send[2] = reg & 0xff;
	P90 = 0;
	result = csiTransferPoll(receive, send, sizeof(receive));
	P90 = 1;
	memcpy(data, &receive[3], 4);
	return result;
}
