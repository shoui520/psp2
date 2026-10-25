/*
 * The A/D converter (app/adc.c): channels 11-15 (ANI11-ANI15), converted one after another
 * by INTAD into adcResults, which then sets event flag 2's bit 0x0004.
 */
#ifndef ERNIE_ADC_H
#define ERNIE_ADC_H

#define ADC_FIRST_CHANNEL	0x0b
#define ADC_CHANNELS		5

/* adcStart's mask: bit 3 for channel 11 up to bit 7 for channel 15 */
#define ADC_CHANNEL_BIT(channel)	(1 << ((channel) - ADC_FIRST_CHANNEL + 3))

extern __near unsigned int adcResults[ADC_CHANNELS];	/* channel 15 first */

extern void adcOpen(void);
extern void adcClose(void);
extern unsigned char adcStart(unsigned char mask, int skip);
extern void adcOpenForBoard(void);
extern void adcCloseForBoard(void);

#endif
