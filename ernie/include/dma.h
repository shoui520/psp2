/*
 * The DMA controller's two channels (app/dma.c): a transfer between RAM and an SFR, one byte
 * each time the trigger's interrupt comes. Channel 0 moves from the SFR to RAM, channel 1
 * from RAM to the SFR.
 */
#ifndef ERNIE_DMA_H
#define ERNIE_DMA_H

struct DmaRequest {
	unsigned int	count;		/* the bytes to move (DBCn: 10 bits) */
	unsigned char	*address;	/* the RAM's side (DRAn takes its low word) */
	unsigned char	sfr;		/* the SFR's side: its address's low byte (DSAn) */
	unsigned char	trigger;	/* the interrupt that moves each byte (DMCn's IFCn) */
};

/* DmaRequest.trigger: the interrupts whose transfers INTDMA0 ends */
#define DMA_TRIGGER_CSI00	6	/* INTST0, INTCSI00 */
#define DMA_TRIGGER_SR0		7	/* INTSR0 (UART0 receives) */
#define DMA_TRIGGER_CSI10	8	/* INTST1, INTCSI10 */

extern unsigned char dma0Setup(struct DmaRequest *request);
extern void dma0Start(void);
extern void dma0EnableInterrupt(void);
extern void dma0DisableInterrupt(void);
extern void dma0Stop(void);
extern void dma0End(void);
extern unsigned char dma1Setup(struct DmaRequest *request);
extern void dma1Start(void);
extern void dma1Stop(void);
extern void dma1End(void);

#endif
