/*
 * The touch panels' SPI link (app/csi10.c): serial channel 2 of SAU0 as CSI10, to the two
 * touch panel controllers, panel 0 selected by P0.1 and panel 1 by P0.0 (low: selected).
 */
#ifndef ERNIE_CSI10_H
#define ERNIE_CSI10_H

/* a transfer: the bytes to receive and to send, and the phase it is (csi10GetState) */
struct Csi10Transfer {
	unsigned char	*receive;
	unsigned char	*send;
	unsigned int	sendCount;
	unsigned int	receiveCount;
	unsigned char	state;
};

/* csi10GetState(): the low bits */
#define CSI10_BUSY		0x01	/* a transfer is running */
#define CSI10_OPEN		0x02	/* the channel is set up, no transfer runs */
#define CSI10_DONE		0x04	/* a CSI10_TRANSFER ended */

/* the high bits: the phase of the running transfer; from a panel's command on, the
   interrupt starts each next phase itself, up to CSI10_FRAME */
#define CSI10_PHASE		0xf0
#define CSI10_TRANSFER		0x10	/* a transfer alone */
#define CSI10_COMMAND0		0x20	/* panel 0's report command */
#define CSI10_REPORT0		0x30	/* panel 0's report */
#define CSI10_COMMAND1		0x40	/* panel 1's report command */
#define CSI10_REPORT1		0x50	/* panel 1's report */
#define CSI10_STATUS1		0x60	/* panel 1's status */
#define CSI10_STATUS0		0x70	/* panel 0's status */
#define CSI10_FRAME		0x80	/* the frame is complete */

extern int csi10Open(void);
extern int csi10Transfer(struct Csi10Transfer *transfer);
extern int csi10TransferNoLock(struct Csi10Transfer *transfer);
extern int csi10Cancel(void);
extern void csi10StopChannel(void);
extern void csi10Close(void);
extern unsigned char csi10GetState(void);
extern void csi10Release(void);

#endif
