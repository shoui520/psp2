/*
 * The IICA (I2C) master (app/iica.c).
 */
#ifndef ERNIE_IICA_H
#define ERNIE_IICA_H

/* a transfer: txCount bytes written, then rxCount read */
struct IicaRequest {
	unsigned char	*rxData;
	unsigned char	*txData;
	unsigned char	txCount;
	unsigned char	rxCount;
	unsigned char	address;		/* the device's, shifted: bit 0 clear */
	unsigned char	state;
	unsigned char	*status;		/* 0, or 1 and 2: no acknowledge */
};

extern int iicaInit(void);
extern int iicaStart(struct IicaRequest *request);
extern int iicaTransfer(struct IicaRequest *request);
extern void iicaClose(void);

#endif
