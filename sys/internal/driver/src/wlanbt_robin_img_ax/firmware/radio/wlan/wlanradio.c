/*
 * The WLAN radio controller's program (8051, built with SDCC): the 0x1384 bytes
 * macTxNotifyManagePacket() loads from wlanPacketBufferPoolMemory.
 *
 * After its start-up it runs one state machine, its state in SFR 0xF1 where
 * the host sees it. The states calibrate the radio: each search steps one
 * radio register up or down until a comparator in a status register flips, or
 * halves its step at each probe (a binary search), with Timer 1 as the timeout;
 * one state finds the peak of the ADC reading. External interrupt 0 restarts
 * the machine at the state its cause selects.
 *
 * Nothing names Marvell's functions or variables here; the names are ours.
 */
#include "radio8051.h"

#define RADIO_STATE	SFR_F1

/* Flags (the bit-addressable RAM). */
__bit searchExit;		/* ends linearSearch() */
__bit searchFlag;		/* cleared after each search; nothing sets it */
__bit timedOut;			/* set by the timeout and by external interrupt 0 */
__bit wakePending;		/* an interrupt came in since the state cleared it */
__bit trackContinue;		/* dacTrack(): keep the last directions; nothing sets it */
__bit unusedFlag;
__bit stepActive;		/* the DAC step search (state 6) has started */
__bit restarted;		/* state 0 ran before */
__bit peakActive;		/* the peak search (state 9) has started */
__bit peakRising;		/* the peak search goes up */
__bit isrNoWait;

/* Parameters of the searches, set by their callers. */
uint8_t searchMask;		/* the status bit that says the register is above the target */
uint8_t searchUpper;		/* linearSearch() stops at these */
uint8_t searchLower;
uint8_t searchStep;
uint8_t searchCount;		/* steps of binarySearch(), also the DAC step count */

uint8_t dacLimits[4];		/* the DAC code's limits, 16 bits each: [0..1] upper, [2..3] lower */
uint16_t stepDelay;		/* timer 0 count before each DAC step */
uint16_t trackDelay;		/* timer 0 count before each tracking step */
uint8_t dacStepSize;
uint8_t trackHistory;		/* dacTrack(): the last two directions */
uint8_t unusedByte;

/* Waits for timer 0 to overflow from count. */
void delay(uint16_t count)
{
	TMOD |= 0x01;
	TL0 = count;
	TH0 = count >> 8;
	TR0 = 1;
	while (!TF0)
		;
	TR0 = 0;
	TF0 = 0;
}

/* Starts timer 1 from count; its interrupt sets timedOut. */
void timeoutStart(uint16_t count)
{
	TMOD |= 0x10;
	TL1 = count;
	TH1 = count >> 8;
	EA = 1;
	ET1 = 1;
	TR1 = 1;
}

/* Reads the ADC: latch, settle, start the conversion and wait for it. */
uint8_t adcRead(void)
{
	uint8_t value;

	P3_2 = 1;
	P3_2 = 0;
	delay(0xfffb);
	P3_1 = 1;
	while (!P1_5)
		;
	value = SFR_F3;
	P3_1 = 0;
	return value;
}

/*
 * Steps *reg by searchStep toward the target the status bit shows until the bit
 * has alternated over the last four probes (or the timeout); 1 if the register
 * reached searchUpper or searchLower first.
 */
__bit linearSearch(__xdata uint8_t *reg, __xdata uint8_t *status)
{
	uint8_t history = 0;
	uint8_t value = *reg;
	volatile uint8_t i;		/* the original runs this loop once */
	__bit above;
	__bit converged;

	for (i = 0; i < 1; i++)
		;
	while (!searchExit) {
		P3_2 = 1;
		P3_2 = 0;
		above = (*status & searchMask) == searchMask;
		if (above) {
			if (value == searchUpper)
				return 1;
			value += searchStep;
		} else {
			if (value == searchLower)
				return 1;
			value -= searchStep;
		}
		history = ((history << 1) + above) & 0x0f;
		*reg = value;
		/*
		 * The original also tests (history < 1) == 0xf5 here, which is
		 * never true: only the alternating pattern 1010 ends the search.
		 */
		converged = history == 0x0a;
		searchExit = converged | timedOut;
	}
	return 0;
}

/* Binary search of *reg: searchCount probes, each halving searchStep, then a last one-step correction. */
void binarySearch(__xdata uint8_t *reg, __xdata uint8_t *status)
{
	while (searchCount != 0) {
		P3_2 = 1;
		P3_2 = 0;
		if ((*status & searchMask) == searchMask)
			*reg = *reg + searchStep;
		else
			*reg = *reg - searchStep;
		searchStep >>= 1;
		searchCount--;
	}
	P3_2 = 1;
	P3_2 = 0;
	if ((*status & searchMask) == searchMask)
		*reg = *reg + 1;
}

/* One step of the DAC code search; 1 when the comparator is balanced (neither input high). */
__bit dacStep(void)
{
	uint8_t low;

	delay(stepDelay);
	P3_2 = 1;
	P3_2 = 0;
	P3_2 = 0;
	/* The 16-bit code in E9 (low) and EA (high), each byte read and written in turn. */
	if (P1_1) {
		low = SFR_E9;
		SFR_E9 = low - dacStepSize;
		SFR_EA = SFR_EA - (low < dacStepSize);
	} else if (P1_0) {
		low = SFR_E9;
		SFR_E9 = low + dacStepSize;
		SFR_EA = SFR_EA + ((uint8_t)(low + dacStepSize) < low);
	} else {
		return 1;
	}
	SFR_ED = SFR_EB;
	SFR_EE = SFR_EC;
	dacStepSize >>= 1;
	return 0;
}

/*
 * Follows the comparator with the DAC code one searchStep at a time; 1 at a
 * limit, when the direction reversed or on the timeout. Nothing calls it.
 */
__bit dacTrack(void)
{
	uint8_t low = SFR_E9;
	uint8_t high = SFR_EA;
	uint16_t code = ((high << 8) | low) >> 4;
	__bit reversed;

	delay(stepDelay);
	P3_2 = 1;
	P3_2 = 0;
	P3_2 = 0;
	if (P1_0) {
		if ((code & 0xff) == dacLimits[0] && (code >> 8) == dacLimits[1]) {
			P1_7 = 1;
			return 1;
		}
		code += searchStep;
	} else {
		/* The lower limit as the original reads it: the bytes at [1..2]. */
		if ((code & 0xff) == dacLimits[1] && (code >> 8) == dacLimits[2]) {
			P1_7 = 1;
			return 1;
		}
		code -= searchStep;
	}
	if (!trackContinue)
		trackHistory = 0;
	trackHistory = ((trackHistory << 1) + P1_0) & 0x03;
	code <<= 4;
	SFR_E9 = code;
	SFR_EA = code >> 8;
	SFR_ED = SFR_EB;
	SFR_EE = SFR_EC;
	reversed = trackHistory == 2;
	return reversed | timedOut;
}

/* The timeout. */
void timer1Isr(void) __interrupt(3) __using(1)
{
	EA = 0;
	TR1 = 0;
	TF1 = 0;
	timedOut = 1;
	EA = 1;
}

/* Resets the calibrated registers to their mid values. */
#define RESET_IQ_REGS()			\
	do {				\
		RADIO_REG(0xf18e) = 0x0f;	\
		RADIO_REG(0xf18f) = 0x3f;	\
		RADIO_REG(0xf190) = 0x3f;	\
		RADIO_REG(0xf191) = 0x3f;	\
		RADIO_REG(0xf192) = 0x0f;	\
		RADIO_REG(0xf193) = 0x3f;	\
		RADIO_REG(0xf194) = 0x3f;	\
		RADIO_REG(0xf195) = 0x3f;	\
	} while (0)

#define RESET_CALIBRATION()		\
	do {				\
		RADIO_REG(0xf053) = 0x05;	\
		RADIO_REG(0xf054) = 0x0f;	\
		RADIO_REG(0xf056) = 0x0f;	\
		RADIO_REG(0xf1c9) = 0x3f;	\
		RADIO_REG(0xf1ca) = 0x3f;	\
		RADIO_REG(0xf1cb) = 0x3f;	\
		RADIO_REG(0xf1cc) = 0x3f;	\
		RESET_IQ_REGS();		\
	} while (0)

/* The host's request: the cause bits in P0, SFR 0xC8 and the radio registers select the next state. */
void ie0Isr(void) __interrupt(0) __using(1)
{
	__bit wlanBusy;

	isrNoWait = 0;
	EA = 0;
	TR1 = 0;
	TF1 = 0;
	timedOut = 1;
	wakePending = 1;
	SFR_DE = 0;
	SFR_E4 = 0;
	SFR_E3 = 0;
	SFR_E6 = 0;
	SFR_DC = 0;
	SFR_DB = 0;
	if (P0_7) {
		RADIO_STATE = 0x00;
		P0 &= 0x48;
		isrNoWait = 1;
	} else if (P0_0 | P0_3) {
		RADIO_STATE = 0x20;
		RESET_CALIBRATION();
		P0 &= 0xc0;
	} else if (P0_2 | P0_1) {
		wlanBusy = (RADIO_REG(0xf04b) & 0x20) == 0x20;
		if (wlanBusy | P0_1)
			RADIO_REG(0xf0b2) |= 0x10;
		RADIO_STATE = 0x20;
		RESET_CALIBRATION();
		P0 &= 0xc0;
	} else if (SFR_C8_0) {
		RADIO_STATE = 0x0f;
		SFR_C8 &= 0x02;
	} else if (SFR_C8_1) {
		RADIO_STATE = 0x11;
		SFR_C8 &= 0x01;
	} else if (P0_6) {
		RADIO_STATE = 0x13;
		P0 &= 0xbf;
	} else if (P0_4) {
		RADIO_REG(0xf0b2) |= 0x10;
		RADIO_STATE = 0x21;
		RESET_IQ_REGS();
		P0 &= 0xcf;
	} else {
		RADIO_STATE = 0x21;
		RESET_IQ_REGS();
		P0 &= 0xcf;
	}
	if (!((RADIO_REG(0xf007) & 0x10) | (RADIO_REG(0xf008) & 0x80) | isrNoWait))
		while ((RADIO_REG(0xf007) & 0x02) != 0)
			;
	EX0 = 0;
	EA = 1;
}

/* The receive path's bandwidth bit: band (F007 bit 4) or the second mode (F008 bit 7). */
#define WIDE_MODE()	((RADIO_REG(0xf007) & 0x10) | (RADIO_REG(0xf008) & 0x80))
#define F003_ACTIVE()	((RADIO_REG(0xf003) & 0x10) | (RADIO_REG(0xf003) & 0x01))

/* Stops the timeout after a search. */
#define TIMEOUT_STOP()		\
	do {			\
		TR1 = 0;	\
		TF1 = 0;	\
		searchFlag = 0;	\
		searchExit = 0;	\
		timedOut = 0;	\
	} while (0)

/* One timed linear search of reg against status bit mask, from 0 to upper. */
#define LINEAR_SEARCH(timeout, strobe, mask, upper, reg, status)		\
	do {								\
		timeoutStart(timeout);					\
		strobe = 1;						\
		searchMask = (mask);					\
		searchUpper = (upper);					\
		searchLower = 0;					\
		P3_7 = linearSearch((__xdata uint8_t *)(reg), (__xdata uint8_t *)(status)); \
		strobe = 0;						\
		TIMEOUT_STOP();						\
	} while (0)

/* One binary search: count probes from step. */
#define BINARY_SEARCH(count, step, mask, reg, status)		\
	do {							\
		searchCount = (count);				\
		searchStep = (step);				\
		searchMask = (mask);				\
		binarySearch((__xdata uint8_t *)(reg), (__xdata uint8_t *)(status)); \
	} while (0)

/* The receive I/Q calibration: two gain and six phase registers, one path selected at a time in SFR 0xE8. */
#define IQ_CALIBRATION()					\
	do {							\
		SFR_E4 = 0x04;					\
		SFR_E8 = 0x01;					\
		searchUpper = 0x1f;				\
		searchLower = 0;				\
		BINARY_SEARCH(4, 0x08, 0x04, 0xf18e, 0xf141);	\
		SFR_E8 = 0x02;					\
		BINARY_SEARCH(4, 0x08, 0x08, 0xf192, 0xf141);	\
		SFR_E8 = 0x04;					\
		searchUpper = 0x7f;				\
		searchLower = 0;				\
		BINARY_SEARCH(6, 0x20, 0x04, 0xf18f, 0xf141);	\
		SFR_E8 = 0x08;					\
		BINARY_SEARCH(6, 0x20, 0x08, 0xf193, 0xf141);	\
		SFR_E8 = 0x00;					\
		SFR_E8 = 0x10;					\
		searchUpper = 0x7f;				\
		searchLower = 0;				\
		BINARY_SEARCH(6, 0x20, 0x04, 0xf190, 0xf141);	\
		SFR_E8 = 0x20;					\
		BINARY_SEARCH(6, 0x20, 0x08, 0xf194, 0xf141);	\
		SFR_E8 = 0x40;					\
		searchUpper = 0x7f;				\
		searchLower = 0;				\
		BINARY_SEARCH(6, 0x20, 0x04, 0xf191, 0xf141);	\
		SFR_E8 = 0x80;					\
		BINARY_SEARCH(6, 0x20, 0x08, 0xf195, 0xf141);	\
		SFR_E8 = 0x00;					\
	} while (0)

/* Runs the I/Q calibration when F041 bit 7 asks for it and F003 bit 0 allows it. */
#define IQ_CALIBRATION_STATE()						\
	do {								\
		if ((RADIO_REG(0xf041) & 0x80) == 0x80) {		\
			if (!WIDE_MODE())				\
				while ((RADIO_REG(0xf007) & 0x02) != 0)	\
					;				\
			if (RADIO_REG(0xf003) & 0x01) {			\
				SFR_DC |= 0x7e;				\
				if (WIDE_MODE())			\
					SFR_E6 |= 0x06;			\
				else					\
					SFR_E6 |= 0x0a;			\
				IQ_CALIBRATION();			\
			}						\
		}							\
	} while (0)

/*
 * One tracking step of reg: up while the status bits read up (to max), down
 * while they read down (to 0); P3.7 reports a limit.
 */
#define TRACK(reg, status, mask, up, down, max)			\
	do {							\
		uint8_t bits = RADIO_REG(status) & (mask);	\
		uint8_t value = RADIO_REG(reg);			\
		if (bits == (up)) {				\
			if (value == (max))			\
				P3_7 = 1;			\
			else					\
				value++;			\
		} else if (bits == (down)) {			\
			if (value == 0)				\
				P3_7 = 1;			\
			else					\
				value--;			\
		}						\
		RADIO_REG(reg) = value;				\
	} while (0)

/* The radio's settings for the host's operating mode (state 0x17: idle until an interrupt). */
static void setOperatingMode(void)
{
	if ((RADIO_REG(0xf005) & 0x08) == 0x08)
		SFR_D9 = 0x17;
	else
		SFR_D9 = 0x07;
	if ((RADIO_REG(0xf005) & 0x80) == 0x80)
		SFR_D9 |= 0x20;
	if (F003_ACTIVE())
		SFR_DA = 0xb7;
	else
		SFR_DA = 0xb3;
	SFR_DD = 0x07;
}

void main(void)
{
	uint8_t peak = 0;

	unusedByte = 0;
	restarted = 0;
	peakActive = 0;
	peakRising = 0;
	dacLimits[2] = 0xff;
	dacLimits[3] = 0xff;
	trackDelay = 0xffc9;
	stepDelay = 0xff59;
	dacLimits[0] = 0x1f;
	dacLimits[1] = 0x00;
	dacStepSize = 0x80;
	unusedFlag = 0;
	stepActive = 0;
	searchFlag = 0;
	searchExit = 0;
	timedOut = 0;
	searchStep = 1;
	RADIO_REG(0xf0b2) |= 0x20;
	SFR_8E = 0;
	SFR_86 = 0;
	TCON = 0;
	TMOD = 0;
	RADIO_REG(0xf002) = 0x05;
	RADIO_REG(0xf0b2) |= 0x40;
	while (!(P3 & 0x30))
		RADIO_REG(0xf0b1) = 0;

	for (;;) {
		switch (RADIO_STATE) {
		case 0x00:
			SFR_DE = 0;
			SFR_E4 = 0;
			SFR_DB = 0;
			SFR_E3 = 0;
			SFR_E6 = 0;
			SFR_DC = 0;
			SFR_DA = 0;
			SFR_DD = 0;
			SFR_D9 = 0x03;
			if ((RADIO_REG(0xf005) & 0x20) != 0x20)
				SFR_D9 = 0x00;
			if (restarted) {
				if ((RADIO_REG(0xf07b) & 0x01) == 0x01)
					RADIO_REG(0xf0b2) &= 0xdf;
				RADIO_REG(0xf0b1) &= 0xfe;
				RADIO_REG(0xf0b2) |= 0x40;
				while (!(P3 & 0x30))
					RADIO_REG(0xf0b1) = 0;
			}
			restarted = 1;
			SFR_F2 = 0xff;
			SFR_F4 = 0x03;
			EA = 1;
			if ((RADIO_REG(0xf007) & 0xc0) != 0x40) {
				uint8_t value = RADIO_REG(0xf055);

				RADIO_STATE = 0x03;
				RADIO_REG(0xf055) = value;
			}
			RADIO_STATE = 0x01;
			/* fall through */
		case 0x01:
			SFR_D9 = 0x07;
			if ((RADIO_REG(0xf005) & 0x20) != 0x20)
				delay(0xfce0);
			RADIO_STATE = 0x02;
			/* fall through */
		case 0x02:
			setOperatingMode();
			delay(0xffd8);
			if ((RADIO_REG(0xf007) & 0xc0) == 0x40)
				RADIO_STATE = 0x1f;
			else
				RADIO_STATE = 0x22;
			break;
		case 0x22:
			delay(0xff0e);
			if ((RADIO_REG(0xf007) & 0xc0) == 0x80)
				RADIO_STATE = 0x04;
			else
				RADIO_STATE = 0x03;
			break;
		case 0x03:
			wakePending = 0;
			timedOut = 0;
			searchStep = 1;
			if ((RADIO_REG(0xf040) & 0x04) == 0x04)
				LINEAR_SEARCH(0xfc18, P2_0, 0x01, 0x08, 0xf055, 0xf0b7);
			RADIO_STATE = 0x04;
			/* fall through */
		case 0x04:
			wakePending = 0;
			timedOut = 0;
			searchStep = 1;
			if ((RADIO_REG(0xf040) & 0x08) == 0x08)
				LINEAR_SEARCH(0xfa24, P2_1, 0x02, 0x1f, 0xf053, 0xf0b7);
			RADIO_STATE = 0x05;
			/* fall through */
		case 0x05:
			if ((RADIO_REG(0xf040) & 0x02) == 0x02)
				LINEAR_SEARCH(0xf830, P2_2, 0x08, 0x3f, 0xf056, 0xf0b7);
			RADIO_STATE = 0x06;
			break;
		case 0x06:
			if ((RADIO_REG(0xf040) & 0x01) != 0x01) {
				RADIO_STATE = 0x08;
				break;
			}
			if (!stepActive) {
				searchCount = 0;
				SFR_E9 = 0x00;
				SFR_EA = 0x01;
				SFR_ED = SFR_EB;
				SFR_EE = SFR_EC;
				dacStepSize = 0x80;
				RADIO_STATE = 0x07;
				stepActive = 1;
				break;
			}
			if (searchCount < 9) {
				__bit balanced;

				P2_3 = 1;
				balanced = dacStep();
				P2_3 = 0;
				if (!balanced) {
					searchCount++;
					RADIO_STATE = 0x07;
					break;
				}
			} else {
				P2_3 = 0;
			}
			stepActive = 0;
			if ((RADIO_REG(0xf04b) & 0x10) == 0x10) {
				delay(0xfeb3);
				P0_2 = 0;
				delay(0xfeb3);
				if (P1_6 | P0_2)
					RADIO_STATE = 0x04;
				else
					RADIO_STATE = 0x08;
				break;
			}
			RADIO_STATE = 0x08;
			break;
		case 0x07:
			if ((RADIO_REG(0xf040) & 0x01) != 0x01)
				break;
			P2_2 = 1;
			searchMask = 0x08;
			searchUpper = 0x3f;
			searchLower = 0;
			P3_7 = linearSearch((__xdata uint8_t *)0xf056, (__xdata uint8_t *)0xf0b7);
			P2_2 = 0;
			TR1 = 0;
			TF1 = 0;
			searchFlag = 0;
			searchExit = 0;
			RADIO_STATE = 0x06;
			break;
		case 0x08:
			if ((RADIO_REG(0xf040) & 0x10) == 0x10)
				LINEAR_SEARCH(0xf830, P2_4, 0x20, 0x1f, 0xf054, 0xf0b7);
			RADIO_STATE = 0x09;
			/* fall through */
		case 0x09:
			if ((RADIO_REG(0xf041) & 0x10) != 0x10) {
				RADIO_STATE = 0x0a;
				break;
			}
			if (!peakActive) {
				uint8_t value;

				P2_5 = 1;
				P3_0 = 1;
				RADIO_REG(0xf072) = SFR_F5;
				delay(0xfff9);
				adcRead();
				peak = adcRead();
				if (RADIO_REG(0xf072) < 0x0f) {
					RADIO_REG(0xf072) = RADIO_REG(0xf072) + 1;
					value = adcRead();
					if (peak < value) {
						peak = value;
						peakRising = 1;
					} else {
						RADIO_REG(0xf072) = RADIO_REG(0xf072) - 1;
						peakRising = 0;
					}
				} else {
					peakRising = 0;
				}
				peakActive = 1;
				break;
			}
			if (peakRising) {
				if (RADIO_REG(0xf072) < 0x0f) {
					uint8_t value;

					RADIO_REG(0xf072) = RADIO_REG(0xf072) + 1;
					value = adcRead();
					if (peak < value) {
						peak = value;
						RADIO_STATE = 0x09;
						break;
					}
					RADIO_REG(0xf072) = RADIO_REG(0xf072) - 1;
				}
			} else if (RADIO_REG(0xf072) != 0) {
				uint8_t value;

				RADIO_REG(0xf072) = RADIO_REG(0xf072) - 1;
				value = adcRead();
				if (peak < value) {
					peak = value;
					RADIO_STATE = 0x09;
					break;
				}
				RADIO_REG(0xf072) = RADIO_REG(0xf072) + 1;
			}
			RADIO_STATE = 0x0a;
			P3_0 = 0;
			peakActive = 0;
			P2_5 = 0;
			break;
		case 0x0a:
			if ((RADIO_REG(0xf041) & 0x08) == 0x08)
				LINEAR_SEARCH(0xf830, P3_3, 0x80, 0x0f, 0xf071, 0xf0b7);
			RADIO_STATE = 0x0c;
			/* fall through */
		case 0x0c:
			if ((RADIO_REG(0xf041) & 0x20) == 0x20 && F003_ACTIVE()) {
				timeoutStart(0xf060);
				P2_6 = 1;
				searchUpper = 0x0f;
				searchLower = 0;
				if ((RADIO_REG(0xf008) & 0x80) == 0x80) {
					searchMask = 0x04;
					P3_7 = linearSearch((__xdata uint8_t *)0xf06d, (__xdata uint8_t *)0xf0b8);
				} else {
					searchMask = 0x02;
					P3_7 = linearSearch((__xdata uint8_t *)0xf06d, (__xdata uint8_t *)0xf0b8);
				}
				P2_6 = 0;
				TIMEOUT_STOP();
			}
			RADIO_STATE = 0x0d;
			/* fall through */
		case 0x0d:
			P0_3 = 0;
			P0_1 = 0;
			P0_2 = 0;
			if (RADIO_REG(0xf178) & 0x01) {
				SFR_DC = 0x01;
				SFR_E6 = 0x01;
			}
			timeoutStart(0x3cb0);
			if ((RADIO_REG(0xf041) & 0x40) == 0x40 && (RADIO_REG(0xf003) & 0x10)) {
				/* The transmit I/Q calibration, one path selected at a time in SFR 0xC0. */
				SFR_DE = 0x08;
				SFR_C0_0 = 1;
				searchUpper = 0x7f;
				searchLower = 0;
				BINARY_SEARCH(6, 0x20, 0x01, 0xf1c9, 0xf141);
				SFR_C0 = 0x02;
				BINARY_SEARCH(6, 0x20, 0x02, 0xf1cb, 0xf141);
				SFR_E4 = 0x0c;
				SFR_C0 = 0x10;
				searchUpper = 0x7f;
				searchLower = 0;
				BINARY_SEARCH(6, 0x20, 0x01, 0xf1ca, 0xf141);
				SFR_C0 = 0x20;
				BINARY_SEARCH(6, 0x20, 0x02, 0xf1cc, 0xf141);
				SFR_C0 = 0x00;
				SFR_DE = 0x00;
				SFR_E4 = 0x04;
			}
			TR1 = 0;
			TF1 = 0;
			timedOut = 0;
			IQ_CALIBRATION_STATE();
			if ((RADIO_REG(0xf007) & 0xc0) == 0xc0)
				RADIO_STATE = 0x0e;
			else
				RADIO_STATE = 0x16;
			break;
		case 0x0e:
			SFR_E6 = 0;
			SFR_DE = 0;
			SFR_DC = 0x30;
			SFR_E4 = 0x0c;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x10) != 0x10)
				;
			RADIO_STATE = 0x0f;
			/* fall through */
		case 0x0f:
			SFR_E6 = 0;
			SFR_DE = 0;
			SFR_DC = 0x30;
			SFR_E4 = 0x0c;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x10) != 0)
				;
			RADIO_STATE = 0x10;
			/* fall through */
		case 0x10:
			SFR_DC = 0x6e;
			if (WIDE_MODE())
				SFR_E6 = 0x06;
			else
				SFR_E6 = 0x0a;
			SFR_DE = 0x0e;
			SFR_E4 = 0x0e;
			SFR_DB = 0x01;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x08) != 0x08)
				;
			RADIO_STATE = 0x11;
			/* fall through */
		case 0x11:
			SFR_DC = 0x6e;
			if (WIDE_MODE())
				SFR_E6 = 0x06;
			else
				SFR_E6 = 0x0a;
			SFR_DE = 0x0e;
			SFR_E4 = 0x0e;
			SFR_DB = 0x01;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x08) != 0)
				;
			if ((RADIO_REG(0xf005) & 0x40) == 0x40)
				RADIO_STATE = 0x12;
			else
				RADIO_STATE = 0x16;
			break;
		case 0x12:
			if (WIDE_MODE())
				SFR_E6 = 0x06;
			else
				SFR_E6 = 0x0a;
			SFR_DC = 0x6e;
			SFR_DE = 0x0e;
			SFR_E4 = 0x0e;
			SFR_DB = 0x21;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x08) != 0x08)
				;
			RADIO_STATE = 0x13;
			break;
		case 0x13:
			if (WIDE_MODE())
				SFR_E6 = 0x06;
			else
				SFR_E6 = 0x0a;
			SFR_DC = 0x6e;
			SFR_DE = 0x0e;
			SFR_E4 = 0x0e;
			SFR_DB = 0x21;
			SFR_E3 = 0x03;
			while ((RADIO_REG(0xf047) & 0x08) != 0)
				;
			if ((RADIO_REG(0xf047) & 0x04) == 0x04)
				RADIO_STATE = 0x14;
			else
				RADIO_STATE = 0x16;
			break;
		case 0x14:
			SFR_DB = 0x01;
			SFR_E4 = 0x0a;
			while ((RADIO_REG(0xf047) & 0x04) != 0)
				;
			RADIO_STATE = 0x15;
			/* fall through */
		case 0x15:
			if (RADIO_REG(0xf047) & 0x01)
				SFR_DB = 0x02;
			else
				SFR_DB = 0x01;
			SFR_DC = 0x7e;
			SFR_E4 = 0x0e;
			while ((RADIO_REG(0xf047) & 0x02) != 0)
				;
			RADIO_STATE = 0x16;
			break;
		case 0x21:
			wakePending = 0;
			SFR_DE = 0;
			if (RADIO_REG(0xf178) & 0x01) {
				SFR_DC |= 0x01;
				SFR_E6 |= 0x01;
			}
			IQ_CALIBRATION_STATE();
			RADIO_STATE = 0x16;
			break;
		case 0x1f:
			P2_1 = 1;
			P2_1 = 0;
			delay(0xff4c);
			if (!(RADIO_REG(0xf005) & 0x10)) {
				wakePending = 0;
				SFR_DC = 0x01;
				SFR_E4 = 0x04;
				if ((RADIO_REG(0xf043) & 0x80) == 0x80) {
					if (RADIO_REG(0xf103) & 0x04)
						SFR_DC = 0x35;
					else
						SFR_DC = 0x31;
					if ((RADIO_REG(0xf003) & 0x01) == 0x01) {
						SFR_E8 = 0x0c;
						delay(0xfff6);
						P3_2 = 1;
						P3_2 = 0;
						TRACK(0xf18f, 0xf141, 0x04, 0x04, 0, 0x7f);
						SFR_E8 = 0x08;
						P3_2 = 1;
						P3_2 = 0;
						TRACK(0xf193, 0xf141, 0x08, 0x08, 0, 0x7f);
						SFR_E8 = 0x00;
					}
				}
				SFR_DC = 0x01;
				if ((RADIO_REG(0xf043) & 0x40) == 0x40) {
					SFR_DE = 0x08;
					if (RADIO_REG(0xf103) & 0x08)
						SFR_E4 = 0x0e;
					else
						SFR_E4 = 0x0c;
					if ((RADIO_REG(0xf003) & 0x10) == 0x10) {
						SFR_C0 = 0x30;
						delay(0xfff6);
						P3_2 = 1;
						P3_2 = 0;
						TRACK(0xf1ca, 0xf141, 0x01, 0x01, 0, 0x7f);
						SFR_C0 = 0x20;
						P3_2 = 1;
						P3_2 = 0;
						TRACK(0xf1cc, 0xf141, 0x02, 0x02, 0, 0x7f);
						SFR_C0 = 0x00;
					}
				}
				SFR_DC = 0;
				SFR_E4 = 0;
				if ((RADIO_REG(0xf042) & 0x01) == 0x01) {
					uint8_t direction;

					P2_3 = 1;
					delay(trackDelay);
					P3_2 = 1;
					P3_2 = 0;
					direction = P1 & 0x07;
					if (direction == 6) {
						if (SFR_ED != 0) {
							SFR_ED--;
						} else if (SFR_EE != 0) {
							SFR_EE--;
							SFR_ED = 0x07;
						}
					} else if (direction == 5) {
						if (SFR_ED < 0x1f) {
							SFR_ED++;
						} else if (SFR_EE < 0x1f) {
							SFR_EE++;
							SFR_ED = 0x10;
						}
					}
					P2_3 = 0;
				}
				if ((RADIO_REG(0xf042) & 0x02) == 0x02) {
					P2_2 = 1;
					P2_2 = 1;
					P3_2 = 1;
					P3_2 = 0;
					TRACK(0xf056, 0xf0b7, 0x18, 0x18, 0x10, 0x3f);
					P2_2 = 0;
				}
			}
			SFR_E4 &= 0x06;
			P0_3 = 0;
			P0_1 = 0;
			if (!P1_6)
				P0_2 = 0;
			RADIO_STATE = 0x16;
			/* fall through */
		case 0x16: {
			uint8_t request;

			wakePending = 0;
			request = RADIO_REG(0xf101) & 0x7c;
			while (request != 0) {
				if ((request & 0x08) == 0x08) {
					SFR_DB = 0x02;
					SFR_DC = 0x7f;
					if (WIDE_MODE())
						SFR_E6 = 0x06;
					else
						SFR_E6 = 0x0a;
					SFR_DE = 0x0e;
					SFR_E4 = 0x0e;
					SFR_E3 = 0x03;
				} else if ((request & 0x04) == 0x04) {
					SFR_DB = 0x00;
					SFR_DC = 0x7f;
					if (WIDE_MODE())
						SFR_E6 = 0x07;
					else
						SFR_E6 = 0x0b;
					SFR_DE = 0x0f;
					SFR_E4 = 0x0e;
					SFR_E3 = 0x03;
				} else if ((request & 0x70) != 0) {
					SFR_D9 = 0x07;
					SFR_DA = 0x00;
					SFR_DD = 0x00;
					SFR_E6 = 0x00;
					switch (RADIO_REG(0xf101) & 0x70) {
					case 0x10:
						SFR_DB = 0x00;
						SFR_DC = 0x00;
						SFR_DE = 0x00;
						SFR_E4 = 0x00;
						SFR_E3 = 0x08;
						break;
					case 0x20:
						SFR_DB = 0x04;
						SFR_DC = 0x00;
						SFR_DE = 0x00;
						SFR_E4 = 0x08;
						SFR_E3 = 0x00;
						break;
					case 0x30:
						SFR_DB = 0x08;
						SFR_DC = 0x00;
						SFR_DE = 0x00;
						SFR_E4 = 0x0c;
						SFR_E3 = 0x00;
						break;
					case 0x40:
						SFR_DB = 0x10;
						SFR_DC = 0x00;
						SFR_DE = 0x08;
						SFR_E4 = 0x0c;
						SFR_E3 = 0x00;
						break;
					case 0x50:
						SFR_DB = 0x80;
						SFR_DC = 0x20;
						SFR_DE = 0x00;
						SFR_E4 = 0x00;
						SFR_E3 = 0x00;
						break;
					case 0x60:
						SFR_DB = 0x40;
						SFR_DC = 0x30;
						SFR_DE = 0x00;
						SFR_E4 = 0x04;
						SFR_E3 = 0x00;
						break;
					}
				}
				delay(0xfd44);
				request = RADIO_REG(0xf101) & 0x7c;
			}
			if ((RADIO_REG(0xf005) & 0x08) == 0x08)
				SFR_D9 |= 0x07;
			if ((RADIO_REG(0xf005) & 0x80) == 0x80)
				SFR_D9 |= 0x20;
			if (F003_ACTIVE())
				SFR_DA = 0xb7;
			else
				SFR_DA = 0xb3;
			SFR_DD = 0x07;
			SFR_DE = 0x00;
			SFR_DB = 0x00;
			SFR_E3 = 0x00;
			SFR_DC = 0x7f;
			if (WIDE_MODE())
				SFR_E6 = 0x07;
			else
				SFR_E6 = 0x0b;
			if (RADIO_REG(0xf103) & 0x08)
				SFR_E4 = 0x06;
			else
				SFR_E4 = 0x04;
			EX0 = 1;
			RADIO_STATE = 0x17;
			if (!wakePending)
				PCON = 0x01;
			EX0 = 0;
			break;
		}
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x20:
			SFR_DA = 0x00;
			SFR_DD = 0x00;
			SFR_D9 = 0x03;
			wakePending = 0;
			if (!(RADIO_REG(0xf04c) & 0x01))
				delay(0xfe9c);
			RADIO_STATE = 0x04;
			setOperatingMode();
			break;
		case 0x0b:
			break;
		}
	}
}

/* The C start-up's hook: 1 skips SDCC's clearing and initialisation of the data memory, as the original does. */
unsigned char __sdcc_external_startup(void)
{
	return 1;
}
