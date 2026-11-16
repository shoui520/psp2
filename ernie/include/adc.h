/*
 * The A/D converter (app/adc.c): channels 11-15 (ANI11-ANI15), converted one after another
 * by INTAD into adcResults, which then sets event flag 2's bit 0x0004.
 */
#ifndef ERNIE_ADC_H
#define ERNIE_ADC_H

#include "ernie.h"

/* 0x00411000 starts from channel 8 */
#if HW_IS(0x00411000)
#define ADC_FIRST_CHANNEL	0x08
#else
#define ADC_FIRST_CHANNEL	0x0b
#endif
#define ADC_CHANNELS		(0x10 - ADC_FIRST_CHANNEL)
#define ADC_RESULT_MASK		0xffc0	/* ADCR: the result in its 10 high bits */

/* adcStart's mask: bit 0 for channel 8 up to bit 7 for channel 15 */
#define ADC_CHANNEL_BIT(channel)	(1 << ((channel) - 8))

extern __near unsigned int adcResults[ADC_CHANNELS];	/* channel 15 first */

extern void adcOpen(void);
extern void adcClose(void);
extern unsigned char adcStart(unsigned char mask, int skip);
extern void adcOpenForBoard(void);
extern void adcCloseForBoard(void);

#endif
