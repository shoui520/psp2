/*
 * USS-1001's (BOARD_USS1001) second IICA (I2C) master, the rear touch panel's (app/iica1.c).
 */
#ifndef ERNIE_IICA1_H
#define ERNIE_IICA1_H

/* a transfer: txCount bytes written, then rxCount read (app/iica.c's IicaRequest, with counts
   of a word) */
struct Iica1Request {
	unsigned char	*rxData;
	unsigned char	*txData;
	unsigned int	txCount;
	unsigned int	rxCount;
	unsigned char	address;		/* the device's, shifted: bit 0 clear */
	unsigned char	state;
	unsigned char	*status;		/* 0x80 while it runs, then 0, or 1 and 2: no
						   acknowledge */
};

/* the rear panel's controller, as iica1Transfer addresses it */
#define IICA1_TOUCH		0x96
#define IICA1_TOUCH_BOOTLOADER	0x48	/* and its bootloader */

extern struct Iica1Request iica1Request;

/* iica1Close's: P6.2 and P6.3 high, the IICA off (touchClose has it in its own code) */
#define IICA1_CLOSE()							\
	do {								\
		P6.2 = 1;						\
		PM6.2 = 0;						\
		P6.3 = 1;						\
		PM6.3 = 0;						\
		IICE1 = 0;						\
		IICAMK1 = 1;						\
		IICAIF1 = 0;						\
		iica1Request.state = 0;	/* idle */			\
		IICA1EN = 0;						\
	} while (0)

extern unsigned char iica1Init(void);
extern int iica1Start(struct Iica1Request *request);
extern unsigned char iica1Transfer(struct Iica1Request *request);
extern void iica1Close(void);

#endif
