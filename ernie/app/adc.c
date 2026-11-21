/*
 * The A/D converter: adcStart converts the channels of a mask, lowest first, each in turn
 * when INTAD comes (after skip conversions thrown away), into adcResults; at the last it
 * stops and sets event flag 2's bit 0x0004.
 */
#ifndef ERNIE_UPDATER
#pragma rtos_interrupt INTAD adcInterrupt
#endif
#pragma sfr
#pragma DI
#pragma EI

#include <string.h>

#include "ernie.h"
#include "adc.h"
#include "analog.h"
#include "input.h"
#include "kernel.h"
#include "state.h"
#include "task.h"


#ifdef ERNIE_UPDATER
/* the PS TV's updater reads the converter polled (adcRead): no conversion state */
#elif ADC_AVERAGE
static __near unsigned int adcMask;		/* the channels to convert */
static __near unsigned int adcBit;		/* the channel being converted, as its mask bit */
static __near unsigned int adcDone;		/* the channels converted */
static __near unsigned int adcFirstBit;	/* the first channel's bit */
static __near unsigned char adcConversions[ADC_CHANNELS];	/* each one's still to make */
static __near unsigned char adcSkip;		/* conversions still to throw away */
static __near unsigned char adcChannel;	/* the channel being converted */
static __near unsigned char adcFirstChannel;	/* the mask's first */
__near unsigned long adcResults[ADC_CHANNELS];
__near unsigned char adcCounts[ADC_CHANNELS];
static __near unsigned char adcIndex;		/* adcInterrupt's channel less the first */
#else
static __near unsigned char adcMask;		/* the channels to convert */
static __near unsigned char adcBit;		/* the channel being converted, as its mask bit */
static __near unsigned char adcDone;		/* the channels converted */
static __near unsigned char adcSkip;		/* conversions still to throw away */
static __near unsigned char adcChannel;	/* the channel being converted */
__near unsigned int adcResults[ADC_CHANNELS];
#endif

/* powers the converter on (if it is off; before 0x0100010B, always): ANI11-ANI15 analog, INTAD
   enabled */
void adcOpen(void)
{
#if FW_SINCE(0x0100010B)
	if (!ADCEN) {
#endif
#ifndef ERNIE_UPDATER
		adcMask = 0;
		adcBit = 0;
		adcDone = 0;
		adcSkip = 0;
		adcChannel = ADC_FIRST_CHANNEL;
#endif
		ADCEN = 1;
#if ADC_AVERAGE		/* ANI0-ANI10 analog */
		ADPC = 0x0b;
		ADM0 = 0x16;
		ADM1 = 0x00;
		ADM2 = 0x60;
		ADUL = 0xff;
		ADLL = 0x00;
		ADCE = 1;
#elif BOARD_USS1001	/* the RL78's converter: ANI0-ANI7 analog, 8-bit limits wide open */
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
#ifdef ERNIE_UPDATER
		ADMK = 1;	/* polled */
#else
		ADMK = 0;
#endif
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

#if (defined(ERNIE_UPDATER)) && ADC_AVERAGE
/* the slim's 0x0105 updater: one conversion thrown away, then the average of count more (0:
   none, 0) */
unsigned int adcRead(unsigned int channel, unsigned char count)
{
	unsigned long sum;
	unsigned char left;

	sum = 0;
	left = count;
	if (count == 0)
		return 0;
	ADIF = 0;
	ADS = channel;
	ADCS = 1;
	while (!ADIF)
		;
	while (left-- != 0) {
		ADCS = 0;
		ADIF = 0;
		ADCS = 1;
		while (!ADIF)
			;
		sum += ADCR;
	}
	ADCS = 0;
	ADIF = 0;
	return sum / count;
}
#elif defined(ERNIE_UPDATER)
/* converts a channel twice and gives the second conversion (ADCR) */
unsigned int adcRead(unsigned int channel)
{
	ADIF = 0;
	ADS = channel;
	ADCS = 1;
	while (!ADIF)
		;
	ADCS = 0;
	ADIF = 0;
	ADCS = 1;
	while (!ADIF)
		;
	return ADCR;
}
#else

#if ADC_AVERAGE
/* the slim's 0x0105: converts the channels of the request's mask in turn, each the request's
   number of times, and keeps each one's average; 1 if a conversion runs, 2 if the mask is
   empty */
unsigned char adcStart(const struct AdcRequest *request)
{
	if (ADCS)
		return 1;
	if (request->mask == 0)
		return 2;
	adcMask = request->mask;
	memcpy(adcConversions, &request->conversions[0], sizeof(adcConversions));
	adcSkip = request->skip;
	adcDone = 0;
	adcBit = 1;
	adcFirstBit = 0;
	for (adcChannel = ADC_FIRST_CHANNEL; adcChannel < ADC_FIRST_CHANNEL + ADC_CHANNELS;
	     adcChannel++) {
		if (adcBit & adcMask) {
			if (adcFirstBit == 0) {
				adcFirstChannel = adcChannel;
				adcFirstBit = adcBit;
			}
			adcResults[adcChannel - ADC_FIRST_CHANNEL] = 0;
			adcCounts[adcChannel - ADC_FIRST_CHANNEL] = 0;
		}
		adcBit += adcBit;
	}
	adcChannel = adcFirstChannel;
	adcBit = adcFirstBit;
	ADS = adcChannel;
	ADCS = 1;
	return 0;
}
#else
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
#endif

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
#elif ADC_AVERAGE
/* the slim's 0x0105: opens the converter in states 0x0002, 0x0004, 0x0008 and 0x0020 */
void adcOpenForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0002:
	case 0x0004:
	case 0x0008:
	case 0x0020:
		adcOpen();
		break;
	case 0x0000:
	case 0x0001:
	case 0x0010:
	case 0x0040:
	case 0x0200:
	case 0x0400:
	case 0x0800:
		break;
	}
}

/* and closes it there, unless a key of 0x00f2 is held, keeps it open in 0x0020, 0x0040, 0x0200
   and 0x0400, and in the others closes it with the keys let go */
void adcCloseForBoard(void)
{
	switch (stateGet() & 0x7fff) {
	case 0x0002:
	case 0x0004:
	case 0x0008:
		if (inputKeys.state & 0x00f2)
			break;
		adcClose();
		return;
	case 0x0020:
	case 0x0040:
	case 0x0200:
	case 0x0400:
		break;
	case 0x0000:
	case 0x0001:
	case 0x0010:
	case 0x0800:
	default:
		DI();
		inputKeys.state &= ~0x00f2;
		EI();
		adcClose();
		break;
	}
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

#if ADC_AVERAGE
/* the slim's 0x0105: a conversion ended: added to its channel's sum; when the channel has had
   its number, the sum made the average; then the next channel of the mask not yet done, round
   from the first, or at the last the converter stopped and event flag 2's bit 0x0004 set */
void adcInterrupt(void)
{
	if (adcSkip != 0) {
		adcSkip--;
	} else {
		adcIndex = adcChannel - ADC_FIRST_CHANNEL;
		adcResults[adcIndex] += ADCR;
		adcCounts[adcIndex]++;
		if (adcConversions[adcIndex] != 0)
			adcConversions[adcIndex]--;
		if (adcConversions[adcIndex] == 0) {
			adcDone |= adcBit;
			if (adcCounts[adcIndex] >= 2)
				adcResults[adcIndex] /= adcCounts[adcIndex];
		}
		if (adcDone != adcMask) {
			do {
				adcBit += adcBit;
				adcChannel++;
				if (adcChannel >= ADC_FIRST_CHANNEL + ADC_CHANNELS) {
					adcChannel = adcFirstChannel;
					adcBit = adcFirstBit;
				}
			} while (!(adcBit & adcMask) || (adcBit & adcDone));
			ADS = adcChannel;
			ADIF = 0;
		} else {
			ADCS = 0;
			ADIF = 0;
			task3ISetFlag(0x0004);
		}
	}
}
#else
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
			task3ISetFlag(0x0004);
		}
	}
}
#endif
#endif	/* ERNIE_UPDATER */
