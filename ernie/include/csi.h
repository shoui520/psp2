/*
 * The SPI link to the host (app/csi.c): serial channel 0 of SAU0 as CSI00, whose transfers
 * receive and send at once, by interrupt, by polling or by DMA.
 */
#ifndef ERNIE_CSI_H
#define ERNIE_CSI_H

/* csiGetState() */
#define CSI_BUSY		0x01	/* a transfer is running */
#define CSI_OPEN		0x02	/* the channel is set up */

extern int csiOpenLsb(void);
extern int csiTransfer(unsigned char *receive, unsigned int receiveLength,
		       unsigned char *send, unsigned int sendLength);
extern int csiTransferWait(unsigned char *receive, unsigned int receiveLength,
			   unsigned char *send, unsigned int sendLength);
extern int csiTransferDma(unsigned char *receive, unsigned char *send, unsigned int length);
extern int csiTransferDmaWait(unsigned char *receive, unsigned char *send, unsigned int length);
extern int csiCancel(void);
extern void csiStopChannel(void);
extern void csiClose(void);
extern void csiOpenMsb(void);
extern unsigned int csiGetState(void);
extern void csiRelease(void);

#endif
