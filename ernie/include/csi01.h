/*
 * The motion sensor's SPI link (app/csi01.c): serial channel 1 of SAU0 as CSI01, the sensor
 * selected by P4.2 (low: selected).
 */
#ifndef ERNIE_CSI01_H
#define ERNIE_CSI01_H

/* a transfer: the bytes to receive and to send, and the phase it is (csi01GetState) */
struct Csi01Transfer {
	unsigned char	*receive;
	unsigned char	*send;
	unsigned char	sendCount;
	unsigned char	receiveCount;
	unsigned char	state;
};

/* csi01GetState(): the low bits */
#define CSI01_BUSY		0x01	/* a transfer is running */
#define CSI01_OPEN		0x02	/* the channel is set up, no transfer runs */

/* the high bits: the phase of the running transfer */
#define CSI01_PHASE		0xf0
#define CSI01_TRANSFER		0x10	/* a transfer alone */
#define CSI01_FRAME		0x20	/* a frame of the sensor's, for stream 0x0400 */

extern int csi01Open(void);
extern int csi01Transfer(struct Csi01Transfer *transfer);
extern int csi01Cancel(void);
extern void csi01StopChannel(void);
extern void csi01Close(void);
extern unsigned char csi01GetState(void);

#endif
