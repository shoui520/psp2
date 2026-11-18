/*
 * The A/D converter (app/adc.c): channels 11-15 (ANI11-ANI15), converted one after another
 * by INTAD into adcResults, which then sets event flag 2's bit 0x0004.
 */
#ifndef ERNIE_ADC_H
#define ERNIE_ADC_H

#include "ernie.h"
#include "board.h"

/* 0x00411000 starts from channel 8, 0x00315200 (no channel 8) from channel 9; the slim
   converts ANI2-ANI6, channel 2 first */
#if BOARD_USS1001
#define ADC_FIRST_CHANNEL	0x02
#elif HW_IS(0x00315200)
#define ADC_FIRST_CHANNEL	0x09
#elif BOARD_IS(0x00411000)
#define ADC_FIRST_CHANNEL	0x08
#else
#define ADC_FIRST_CHANNEL	0x0b
#endif
#if BOARD_USS1001
#define ADC_CHANNELS		5
#define ADC_START_END		0x07	/* adcStart's search stops there, adcInterrupt's at 0x10 */
#else
#define ADC_CHANNELS		(0x10 - ADC_FIRST_CHANNEL)
#define ADC_START_END		0x10
#endif
#define ADC_RESULT_MASK		0xffc0	/* ADCR: the result in its 10 high bits */

/* adcStart's mask: bit 0 for channel 8 up to bit 7 for channel 15 (the slim's: bit 0 for
   channel 2) */
#if BOARD_USS1001
#define ADC_CHANNEL_BIT(channel)	(1 << ((channel) - 2))
#else
#define ADC_CHANNEL_BIT(channel)	(1 << ((channel) - 8))
#endif

extern __near unsigned int adcResults[ADC_CHANNELS];	/* channel 15 first (the slim's: 2) */

extern void adcOpen(void);
extern void adcClose(void);
extern unsigned char adcStart(unsigned char mask, int skip);
extern void adcOpenForBoard(void);
extern void adcCloseForBoard(void);

#endif
