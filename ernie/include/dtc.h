/*
 * USS-1001's (BOARD_USS1001) data transfer controller (app/dtc.c): a channel's control data
 * copied into the controller's area and its interrupt source made to start a transfer.
 */
#ifndef ERNIE_DTC_H
#define ERNIE_DTC_H

/* a transfer's control data, as the controller reads it */
struct DtcControl {
	unsigned char	mode;			/* DTCCR */
	unsigned char	blockSize;		/* DTBLS */
	unsigned char	count;			/* DTCCT */
	unsigned char	reload;			/* DTRLD */
	unsigned int	source;			/* DTSAR */
	unsigned int	destination;		/* DTDAR */
};

/* a channel: its control data (DTC_CONTROLS blocks from the first) and the source starting it */
struct DtcChannel {
	unsigned char	control;
	unsigned char	blocks;
	unsigned char	vector;			/* the source's place in the vector table */
	unsigned char	reserved;
};

#define DTC_CONTROLS		3
#define DTC_VECTORS		0x40

extern void dtcOpen(void);
extern void dtcClose(void);
extern unsigned char dtcStart(unsigned char channel, struct DtcControl *control);
extern unsigned char dtcStop(unsigned char channel);

#endif
