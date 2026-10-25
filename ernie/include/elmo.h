/*
 * The Elmo (app/elmo.c), the power management IC on the I2C bus at 0xd0: its registers are
 * written as a register number and a 16-bit value, high byte first, and read two bytes at a
 * time. The firmware keeps a copy of the registers it writes.
 */
#ifndef ERNIE_ELMO_H
#define ERNIE_ELMO_H

#define ELMO_ADDRESS		0xd0	/* on the IICA bus, shifted */
#define ELMO_VERSION		0xd0	/* the register holding its version */

/* registers 0xa0-0xa4: the status masks, read by the host byte by byte */
union ElmoStatusMask {
	unsigned int	reg[5];
	unsigned char	byte[10];
};

/* the bits elmoSetMasks and elmoClearMasks change: 0 leaves a register alone */
struct ElmoMasks {
	unsigned int	status0;		/* registers 0xa0-0xa4 */
	unsigned int	status1;
	unsigned int	status2;
	unsigned int	status3;
	unsigned int	status4;
	unsigned int	regA7;			/* 0xa7, changed the other way */
};

extern union ElmoStatusMask elmoStatusMask;
extern unsigned int elmoLed;			/* 0xa5: the LEDs' state */
extern unsigned int elmoRegA6;			/* 0xa6 */
extern unsigned int elmoRegA7;			/* 0xa7: a mask the clears set */
extern unsigned int elmoRegAA;			/* 0xaa: the RMR's power (bit 15) */
extern unsigned int elmoRegAB;			/* 0xab: the multi connector's power (bit 15) */

extern void elmoInitRegs(void);
extern unsigned char elmoWrite(unsigned char *data, unsigned char length);
extern unsigned char elmoRead(unsigned char *reg, unsigned char *value);
extern unsigned char elmoReadVersion(unsigned char *version);
extern unsigned char elmoSetMasks(struct ElmoMasks *masks, unsigned char *command);
extern unsigned char elmoClearMasks(struct ElmoMasks *masks, unsigned char *command);
extern unsigned char elmoGetMasks(unsigned char select, unsigned char *command);

#endif
