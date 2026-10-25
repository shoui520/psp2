/*
 * The DMA controller's two channels (the driver at 0x16fff): a transfer between RAM and an
 * SFR, one byte each time the trigger's interrupt comes. Channel 0 moves from the SFR to
 * RAM, channel 1 from RAM to the SFR.
 */
#ifndef ERNIE_DMA_H
#define ERNIE_DMA_H

struct DmaRequest {
	unsigned int	count;		/* the bytes to move (DBCn: 10 bits) */
	unsigned char	*address;	/* the RAM's side (DRAn takes its low word) */
	unsigned char	sfr;		/* the SFR's side: its address's low byte (DSAn) */
	unsigned char	trigger;	/* the interrupt that moves each byte (DMCn's IFCn) */
};

extern unsigned char sub_16FFF(struct DmaRequest *request);
extern void sub_1704B(void);
extern void sub_1704F(void);
extern void sub_17056(void);
extern unsigned char sub_17076(struct DmaRequest *request);
extern void sub_170CA(void);

#endif
