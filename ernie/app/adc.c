/*
 * The A/D converter: adcStart converts the channels of a mask, lowest first, each in turn
 * when INTAD comes (after skip conversions thrown away), into adcResults; at the last it
 * stops and sets event flag 2's bit 0x0004.
 */
#pragma rtos_interrupt INTAD adcInterrupt
#pragma sfr

#include "adc.h"
#include "input.h"
#include "kernel.h"
#include "state.h"

extern void sub_2C342(ID flgid, FLGPTN setptn);

static __near unsigned char adcMask;		/* the channels to convert */
static __near unsigned char adcBit;		/* the channel being converted, as its mask bit */
static __near unsigned char adcDone;		/* the channels converted */
static __near unsigned char adcSkip;		/* conversions still to throw away */
static __near unsigned char adcChannel;	/* the channel being converted */
__near unsigned int adcResults[ADC_CHANNELS];

/* powers the converter on (if it is off): ANI11-ANI15 analog, INTAD enabled */
void adcOpen(void)
{
	if (!ADCEN) {
		adcMask = 0;
		adcBit = 0;
		adcDone = 0;
		adcSkip = 0;
		adcChannel = ADC_FIRST_CHANNEL;
		ADCEN = 1;
		ADCE = 1;
		ADPC = 0x0b;
		ADM = 0x03;
		ADIF = 0;
		ADMK = 0;
	}
}

void adcClose(void)
{
	ADMK = 1;
	ADCS = 0;
	ADCE = 0;
	ADIF = 0;
	ADCEN = 0;
}

/* converts the channels of mask; 1 if a conversion runs or mask is empty */
unsigned char adcStart(unsigned char mask, int skip)
{
	if (ADCS || mask == 0)
		return 1;
	adcMask = mask;
	adcSkip = (unsigned char)skip;
	adcDone = 0;
	adcBit = ADC_CHANNEL_BIT(ADC_FIRST_CHANNEL);
	adcChannel = ADC_FIRST_CHANNEL;
	while (adcChannel < 0x10 && !(adcBit & adcMask)) {
		adcBit <<= 1;
		adcChannel++;
	}
	ADS = adcChannel;
	ADCS = 1;
	return 0;
}

/* opens the converter on the boards that have the channels */
void adcOpenForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0004:
	case 0x0008:
	case 0x0020:
		adcOpen();
		inputScanHold(0x1a);
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

/* closes the converter, unless a key of 0x00e3 is held on the boards that have it */
void adcCloseForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0004:
	case 0x0008:
		if (inputKeys.state & 0x00e3)
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
	inputKeys.state &= ~0x00e3;
	adcClose();
}

/* a conversion ended: its result kept, the next channel of the mask started, or at the last
   the converter stopped and event flag 2's bit 0x0004 set */
void adcInterrupt(void)
{
	if (adcSkip != 0) {
		adcSkip--;
	} else {
		adcResults[0x0f - adcChannel] = ADCR;
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
			sub_2C342(2, 0x0004);
		}
	}
}
