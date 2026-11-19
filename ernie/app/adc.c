/*
 * The A/D converter: adcStart converts the channels of a mask, lowest first, each in turn
 * when INTAD comes (after skip conversions thrown away), into adcResults; at the last it
 * stops and sets event flag 2's bit 0x0004.
 */
#pragma rtos_interrupt INTAD adcInterrupt
#pragma sfr
#pragma DI
#pragma EI

#include "ernie.h"
#include "adc.h"
#include "analog.h"
#include "input.h"
#include "kernel.h"
#include "state.h"
#include "task.h"


static __near unsigned char adcMask;		/* the channels to convert */
static __near unsigned char adcBit;		/* the channel being converted, as its mask bit */
static __near unsigned char adcDone;		/* the channels converted */
static __near unsigned char adcSkip;		/* conversions still to throw away */
static __near unsigned char adcChannel;	/* the channel being converted */
__near unsigned int adcResults[ADC_CHANNELS];

/* powers the converter on (if it is off; before 0x0100010B, always): ANI11-ANI15 analog, INTAD
   enabled */
void adcOpen(void)
{
#if FW_SINCE(0x0100010B)
	if (!ADCEN) {
#endif
		adcMask = 0;
		adcBit = 0;
		adcDone = 0;
		adcSkip = 0;
		adcChannel = ADC_FIRST_CHANNEL;
		ADCEN = 1;
#if BOARD_USS1001	/* the RL78's converter: ANI0-ANI7 analog, 8-bit limits wide open */
		ADPC = 0x08;
		ADM0 = 0x16;
		ADM1 = 0x00;
		ADM2 = 0x60;
		ADUL = 0xff;
		ADLL = 0x00;
		ADCE = 1;
#else
		ADCE = 1;
#if BOARD_IS(0x00411000) && !FW_SINCE(0x0100010B)
		ADPC = ADC_FIRST_CHANNEL;	/* the analog pins from the first channel */
#elif BOARD_DOL1002
		ADPC = 0x0a;
#else
		ADPC = 0x0b;
#endif
		ADM = 0x03;
#endif
		ADIF = 0;
		ADMK = 0;
#if FW_SINCE(0x0100010B)
	}
#endif
}

void adcClose(void)
{
	ADMK = 1;
#if FW_SINCE(0x01000104)
	ADCS = 0;
	ADCE = 0;
	ADIF = 0;
#else
	ADIF = 0;
	ADCS = 0;
	ADCE = 0;
#endif
	ADCEN = 0;
}

/* converts the channels of mask; 1 if a conversion runs or mask is empty */
unsigned char adcStart(unsigned char mask, int skip)
{
#if FW_SINCE(0x0100060B)
	if (ADCS)
		return 1;
	if (mask == 0)
		return 2;
#else
	if (ADCS || mask == 0)
		return 1;
#endif
	adcMask = mask;
	adcSkip = (unsigned char)skip;
	adcDone = 0;
	adcBit = ADC_CHANNEL_BIT(ADC_FIRST_CHANNEL);
	adcChannel = ADC_FIRST_CHANNEL;
	while (adcChannel < ADC_START_END && !(adcBit & adcMask)) {
		adcBit <<= 1;
		adcChannel++;
	}
	ADS = adcChannel;
	ADCS = 1;
	return 0;
}

#if !FW_SINCE(0x0100010B)
/* before 0x0100010B: the sticks' state reset and the converter opened */
void adcOpenForBoard(void)
{
	analogInit();
	adcOpen();
}

/* and the sticks stopped and the converter closed */
void adcCloseForBoard(void)
{
	analogSetMode(0);
	adcClose();
}
#else
/* opens the converter on the boards that have the channels */
void adcOpenForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0004:
	case 0x0008:
	case 0x0020:
		adcOpen();
#if !BOARD_USS1001
		inputScanHold(0x1a);
#endif
		break;
	case 0x0000:
	case 0x0001:
	case 0x0002:
	case 0x0010:
	case 0x0040:
	case 0x0200:
	case 0x0400:
	case 0x0800:
		break;
	}
}

/* the keys that keep the converter open (the slim's 0x0032) */
#if BOARD_DOL1002
#define ADC_KEYS	0x0072
#elif SOURCE_0103
#define ADC_KEYS	0x0032
#else
#define ADC_KEYS	0x00e3
#endif

/* closes the converter, unless a key of ADC_KEYS is held on the boards that have it */
void adcCloseForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0004:
	case 0x0008:
		if (inputKeys.state & ADC_KEYS)
			return;
		adcClose();
		return;
	case 0x0020:
		return;
	case 0x0000:
	case 0x0001:
	case 0x0002:
	case 0x0010:
	case 0x0040:
	case 0x0200:
	case 0x0400:
	case 0x0800:
		break;
	}
#if SOURCE_0103
	DI();
	inputKeys.state &= ~ADC_KEYS;
	EI();
#else
	inputKeys.state &= ~ADC_KEYS;
#endif
	adcClose();
}
#endif

/* a conversion ended: its result kept, the next channel of the mask started, or at the last
   the converter stopped and event flag 2's bit 0x0004 set */
void adcInterrupt(void)
{
	if (adcSkip != 0) {
		adcSkip--;
	} else {
#if BOARD_USS1001
		adcResults[adcChannel - ADC_FIRST_CHANNEL] = ADCR;
#else
		adcResults[0x0f - adcChannel] = ADCR;
#endif
		if ((adcDone |= adcBit) != adcMask) {
			adcBit <<= 1;
			adcChannel++;
			while (adcChannel < 0x10 && !(adcBit & adcMask)) {
				adcBit <<= 1;
				adcChannel++;
			}
			ADS = adcChannel;
			ADIF = 0;
		} else {
			ADCS = 0;
			task3SetFlag(2, 0x0004);
		}
	}
}
