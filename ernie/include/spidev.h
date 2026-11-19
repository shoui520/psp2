/*
 * The device on the SPI bus (app/spidev.c): CSI00 opened most significant bit first
 * (csiOpenMsb) while the host link is paused, the device selected by P9.0.
 */
#ifndef ERNIE_SPIDEV_H
#define ERNIE_SPIDEV_H

#include "board.h"

#define SPIDEV_READ		0x84	/* the command that reads a register */
#define SPIDEV_WRITE		0x85	/* the command that writes one */

/* the device's power lines (in a file with #pragma sfr) */
#if BOARD_USS1001
#define SPIDEV_POWER		P1.5
#define SPIDEV_POWER2		P9.7
#else
#define SPIDEV_POWER		P5.7
#define SPIDEV_POWER2		P1.7
#endif

extern void spiDevInit(void);
extern unsigned char spiDevAuthenticate(void);
extern unsigned char spiDevRead(unsigned char command, unsigned int reg, unsigned char *data);
extern unsigned char spiDevReadBlock(unsigned char command, unsigned int reg, unsigned char *data,
				     unsigned char length);
extern unsigned char spiDevWrite(unsigned char command, unsigned int reg, unsigned char *data);
extern unsigned char spiDevPowerOn(void);

#endif
