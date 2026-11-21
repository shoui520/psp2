/*
 * The SPI link to the host (app/csi.c): serial channel 0 of SAU0 as CSI00, whose transfers
 * receive and send at once, by interrupt, by polling or by DMA.
 */
#ifndef ERNIE_CSI_H
#define ERNIE_CSI_H

/* a transfer's state: what INTCSI00 (or the DMA) has left to do */
struct Csi {
	unsigned char	*receive;	/* where the next byte received goes */
	unsigned char	*send;		/* the next byte to send */
	unsigned char	sendCount;	/* the bytes left to send */
	unsigned char	receiveCount;	/* the bytes left to receive */
	unsigned char	state;		/* CSI_* */
};

/* csiGetState() */
#define CSI_BUSY		0x01	/* a transfer is running */
#define CSI_OPEN		0x02	/* the channel is set up */

extern struct Csi csi;

extern int csiOpenLsb(void);
extern int csiTransfer(unsigned char *receive, unsigned int receiveLength,
		       unsigned char *send, unsigned int sendLength);
extern int csiTransferWait(unsigned char *receive, unsigned int receiveLength,
			   unsigned char *send, unsigned int sendLength);
extern int csiTransferDma(unsigned char *receive, unsigned char *send, unsigned int length);
extern int csiTransferDmaWait(unsigned char *receive, unsigned char *send, unsigned int length);
#ifdef ERNIE_UPDATER
/* the updater's (updater/main.c): count bytes each way, polled */
extern int csiTransferPoll(unsigned char *rx, unsigned char *tx, unsigned char count);
#endif
extern int csiCancel(void);
extern void csiStopChannel(void);
extern void csiClose(void);
extern void csiOpenMsb(void);
extern unsigned int csiGetState(void);
extern void csiRelease(void);

#endif
