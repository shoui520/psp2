/*
 * The device on the SPI bus (app/spidev.c): CSI00 opened most significant bit first
 * (csiOpenMsb) while the host link is paused, the device selected by P9.0.
 */
#ifndef ERNIE_SPIDEV_H
#define ERNIE_SPIDEV_H

#define SPIDEV_READ		0x84	/* the command that reads a register */
#define SPIDEV_WRITE		0x85	/* the command that writes one */

extern void spiDevInit(void);
extern unsigned char spiDevAuthenticate(void);
extern unsigned char spiDevRead(unsigned char command, unsigned int reg, unsigned char *data);
extern unsigned char spiDevReadBlock(unsigned char command, unsigned int reg, unsigned char *data,
				     unsigned char length);
extern unsigned char spiDevWrite(unsigned char command, unsigned int reg, unsigned char *data);
extern unsigned char spiDevPowerOn(void);

#endif
