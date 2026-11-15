/*
 * The Bluetooth radio controller's program (8051, built with SDCC): the 0x2b54
 * bytes btLmpLcSyncEntry() loads from btPoolHeaderMemory.
 *
 * Like the WLAN radio's, it is a state machine (the state in SFR 0xC1, 27
 * states) that calibrates the radio: it steps radio registers in XDATA
 * 0x3C5E..0x3C96 against a comparator or a timer capture until the result
 * settles, tabulates the readings (XDATA 0x2D00..0x2F55) and reports what it
 * did in the status register 0x3C57. The host starts it through the external
 * interrupt 0 and port 0; timer 1 is the timeout.
 *
 * Nothing names Marvell's functions, variables or registers here; the names
 * are ours.
 */
#include "btradio_hw.h"

#define XB(addr)	RADIO_REG(addr)
#define STATUS		XB(0x3c57)

/* The calibration tables in XDATA. */
#define READINGS	0x2d1e	/* 16 bits per code: the readings of the search just made */
#define TABULATED	0x2dc0	/* 16 bits each, the readings at the codes found */
#define BASE_COARSE	0x2de8	/* 16 bits: the coarse reading's base */
#define BASE_FINE	0x2e38	/* 16 bits: the fine reading's base */
#define CODES		0x2e62	/* 8 bits each: the codes found */
#define LEVELS		0x2eb3	/* 8 bits each: the levels at the codes */
#define LEVELS_FOUND	0x2f04	/* 8 bits each */
#define TARGETS		0x2f55	/* 16 bits each: the target readings, 0x1b9f + 50 per code */

/* States. */
enum {
	ST_WAIT_HOST = 0,
	ST_INIT_TABLES = 1,
	ST_INIT_REGISTERS = 2,
	ST_COARSE_DAC = 3,
	ST_STEP_A = 4,
	ST_COARSE_DAC_2 = 5,
	ST_STEP_B = 6,
	ST_STEP_C = 7,
	ST_TABULATE = 8,
	ST_RESPONSE = 9,
	ST_GAIN_A = 0x0a,
	ST_GAIN_B = 0x0b,
	ST_PHASE = 0x0c,
	ST_OFFSET = 0x0d,
	ST_ABORT = 0x0e,
	ST_IDLE = 0x0f,
	ST_TRACK_SELECT = 0x10,
	ST_TRACK_A = 0x11,
	ST_TRACK_B = 0x12,
	ST_SLEEP_A = 0x13,
	ST_SLEEP_B = 0x14,
	ST_SLEEP_C = 0x15,
	ST_SLEEP_D = 0x16,
	ST_SLEEP_E = 0x17,
	ST_NOTHING = 0x18,
	ST_TRACK_C = 0x19,
	ST_COPY = 0x1a,
	ST_COUNT = 0x1b
};

/* Flags (the bit-addressable RAM). */
__bit timedOut;			/* timer 1 ran out, or a division by zero (RAM 20.0) */
__bit hostEvent;		/* the host interrupted (RAM 20.1) */
__bit stepActive;		/* state 4's search started (RAM 20.2) */
__bit stepRestart;		/* state 4 asks for a new start code (RAM 20.3) */
__bit captureToggle;		/* alternates the capture of the tracking states (RAM 20.4) */
__bit modeDirect;		/* X[3C58] bit 1: measure the capture directly (RAM 20.5) */
__bit optionPairB;		/* X[3C79] bit 7 (RAM 20.6) */
__bit optionPairA;		/* X[3C79] bit 6 (RAM 20.7) */
__bit invertSense;		/* X[3C5B] == 6 (RAM 21.0) */
__bit calibrate;		/* X[3C58] bit 7 clear (RAM 21.1) */
__bit option58_0;		/* X[3C58] bit 0 (RAM 21.2) */
__bit option59_7;		/* X[3C59] bit 7 (RAM 21.3) */
__bit option65_7;		/* X[3C65] bit 7 (RAM 21.4) */
__bit readingLow;		/* state F: X[3C6D] < the capture (RAM 21.5) */
__bit readingHigh;		/* state F: the capture < X[3C61] (RAM 21.6) */
__bit option69_3;		/* X[3C69] bit 3 (RAM 21.7) */
__bit option7a_6;		/* X[3C7A] bit 6 (RAM 22.0) */
__bit dacOffsetMode;		/* X[3C69] bit 2: the DAC code is offset by dacOffset (RAM 22.1) */
__bit option66;			/* X[3C66] bit 5 clear (RAM 22.2) */
__bit option7d_6;		/* X[3C7D] bit 6 (RAM 22.3) */
__bit option7a_7;		/* X[3C7A] bit 7 (RAM 22.4) */
__bit option65_3;		/* X[3C65] bit 3 (RAM 22.5) */
__bit dacAtZero;		/* the last DAC code was 0 (RAM 22.6) */
__bit trackFlagA;		/* X[3C0C] bit 1 (RAM 22.7) */
__bit trackFlagB;		/* X[3C0C] bit 2 (RAM 23.0) */
__bit option5c_7;		/* X[3C5C] bit 7 (RAM 23.1) */
__bit resetPending;		/* RAM 23.2 */
__bit searchResult;		/* RAM 23.3 */
__bit flag23_4;
__bit flag23_5;
__bit flag23_6;
__bit flag23_7;
__bit flag24_0;
__bit flag24_1;
__bit flag24_2;
__bit flag24_3;
__bit flag24_4;
__bit flag24_5;
__bit flag24_6;
__bit flag24_7;

__data uint8_t portMode;	/* RAM 25: the host's mode, P1 bits 0..2 */
__data uint8_t stepSize;	/* RAM 26: a search's step, halved at each probe */
__data uint8_t offsetA;		/* RAM 27: a signed offset */
__data uint8_t offsetB;		/* RAM 28: a signed offset */
__data uint8_t probeCount;	/* RAM 29 */
__data uint8_t savedCode;	/* RAM 2A */
__data uint16_t capture;	/* RAM 2B (CMH6), 2C (CML7) */
__data uint8_t work;		/* RAM 2D */
__data uint8_t work2;		/* RAM 2E */
__data uint16_t operandA;	/* RAM 33/34 */
__data uint16_t operandB;	/* RAM 35/36 */
__data uint8_t levelMid;	/* RAM 37 */
__data uint8_t levelLow;	/* RAM 38 */
__data uint8_t thresholdHigh;	/* RAM 39 */
__data uint8_t thresholdLow;	/* RAM 3A */
__data uint8_t deltaA, deltaB, deltaC;	/* RAM 3B..3D */
__data uint8_t windowHigh;	/* RAM 3E */
__data uint8_t windowLow;	/* RAM 3F */
__data uint8_t drift0, drift1, drift2;	/* RAM 40..42 */
__data uint8_t driftApplied;	/* RAM 43 */
__data uint8_t trackPasses;	/* RAM 44 */
__data uint8_t tableIndex;	/* RAM 45 */
__data uint8_t stepRate;	/* RAM 46 */
__data uint8_t stepCount;	/* RAM 47 */
__data uint16_t limitLevel;	/* RAM 48/49 */
__data uint8_t limitLow;	/* RAM 4A */
__data uint8_t limitHigh;	/* RAM 4B */
__data uint8_t lastCapture;	/* RAM 4C */
__data uint8_t saveA[3];	/* RAM 4D..4F: X[3C77], X[3C78], X[3C7B] of the first pass */
__data uint8_t saveB[3];	/* RAM 50..52: the same of the second */
__data uint8_t saveC[2];	/* RAM 53, 54: X[3C7E], X[3C80] of the first pass */
__data uint8_t saveD[2];	/* RAM 55, 56: of the second */
__data uint8_t baseA, baseB;	/* RAM 57, 58 */
__data uint8_t dacValue;	/* RAM 59 */
__data uint8_t dacMax;		/* RAM 5A */
__data uint8_t dacOffset;	/* RAM 5B */
__data uint8_t gateHigh;	/* RAM 5C */
__data uint8_t gateOn;		/* RAM 5D */
__data uint8_t gateLow;		/* RAM 5E */
__data uint8_t gateOff;		/* RAM 5F */
__data uint8_t gateInput;	/* RAM 60 */
__data uint8_t gateState;	/* RAM 61 */
__data uint8_t runA, runB;	/* RAM 62, 63 */
__data uint8_t channelIndex[6];	/* RAM 64..69: the channel indices the host may select */
__data uint8_t channelStep[6];	/* RAM 6A..6F: the code of each channel in channelIndex */

#define channelCode	stepSize	/* the tracking states reuse the search registers */
#define channelLevel	probeCount

/* Waits for timer 0 to overflow from count. */
static void delay(uint16_t count)
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
static void timeoutStart(uint16_t count)
{
	TMOD |= 0x10;
	TL1 = count;
	TH1 = count >> 8;
	EA = 1;
	ET1 = 1;
	TR1 = 1;
}

/* A strobe of the capture timer. */
#define STROBE()	do { IRCON_6 = 1; IRCON_6 = 0; } while (0)

/* Reads the timer's capture into capture; nothing after a timeout. */
static void captureTimer(void)
{
	if (timedOut)
		return;
	P2_0 = 1;
	delay(0xfffb);
	while (!(T2CON_7 | timedOut))
		;
	capture = ((uint16_t)CML7 << 8) | CMH6;
	P2_0 = 0;
}

/* operandA times operandB. */
static uint32_t multiplyOperands(void)
{
	return (uint32_t)operandA * operandB;
}

/*
 * The reciprocal 2^30 / x of a 16-bit value, cut to 16 bits: x is shifted up
 * to its top bit and a restoring division makes 16 quotient bits, shifted back.
 * For x = 0 it sets timedOut (the original's RAM 20.0) and returns 0.
 */
static uint16_t reciprocal(uint16_t x)
{
	uint32_t divisor = x;
	uint32_t remainder = 0x8000;
	uint16_t quotient = 0;
	uint8_t shift = 0;
	uint8_t i;

	while (shift < 16 && (divisor & 0x8000) == 0) {
		divisor <<= 1;
		shift++;
	}
	if (shift == 16) {
		timedOut = 1;
		return 0;
	}
	for (i = 0; i < 16; i++) {
		if (remainder >= divisor) {
			quotient = (quotient << 1) | 1;
			remainder = (remainder - divisor) << 1;
		} else {
			quotient <<= 1;
			remainder <<= 1;
		}
	}
	return quotient << shift;
}

/* Whether the history of a search is one of the alternating patterns that end it. */
static __bit searchSettled(uint8_t history)
{
	return history == 5 || history == 6 || history == 9 || history == 10;
}

/*
 * Moves the DAC code in dacValue by stepSize toward the window
 * [thresholdLow, thresholdHigh] (probeCount probes, the step halved at each),
 * measuring the comparator's time at each: from the capture, or against the
 * previous capture when modeDirect is clear. The code goes to *reg, minus
 * dacOffset (and P4.2 for the high half) when dacOffsetMode.
 */
static void dacSearch(__xdata uint8_t *reg)
{
	uint8_t reading;

	while (probeCount != 0) {
		if (modeDirect) {
			STROBE();
			delay(0xffff);
			while (!(T2CON_6 | timedOut))
				;
			reading = CML6;
		} else {
			IRCON_4 = 1;
			delay(0xffff);
			STROBE();
			delay(0xffff);
			while (!(T2CON_6 | timedOut))
				;
			reading = CML6;
			IRCON_4 = 0;
			delay(0xffff);
			STROBE();
			delay(0xffff);
			while (!(T2CON_6 | timedOut))
				;
			reading = CML6 >= reading ? CML6 - reading : 0;
		}
		if (timedOut)
			return;
		if (reading < thresholdLow) {
			dacValue += stepSize;
		} else if (thresholdHigh < reading) {
			dacValue -= stepSize;
		} else {
			if (dacMax < dacValue)
				dacValue = dacMax;
			return;
		}
		if (dacOffsetMode) {
			if (dacValue < dacOffset) {
				*reg = dacValue;
				P4_2 = 0;
			} else if (dacMax < dacValue) {
				*reg = 0x7f;
				P4_2 = 1;
			} else {
				P4_2 = 1;
				*reg = dacValue - dacOffset;
			}
		} else {
			*reg = dacValue;
		}
		probeCount--;
		stepSize >>= 1;
	}
	if (dacMax < dacValue)
		dacValue = dacMax;
}

/* Reads the capture (the timer's capture, or the difference of two) of one probe; 0 on timeout. */
static __bit probeCapture(uint8_t *reading)
{
	if (modeDirect) {
		STROBE();
		delay(0xffff);
		while (!(T2CON_6 | timedOut))
			;
		*reading = CML6;
	} else {
		uint8_t first;

		IRCON_4 = 1;
		delay(0xffff);
		STROBE();
		delay(0xffff);
		while (!(T2CON_6 | timedOut))
			;
		first = CML6;
		IRCON_4 = 0;
		delay(0xffff);
		STROBE();
		delay(0xffff);
		while (!(T2CON_6 | timedOut))
			;
		*reading = CML6 >= first ? CML6 - first : 0;
	}
	return !timedOut;
}

/*
 * Searches *reg (0..0x7f, offset by dacOffset in dacOffsetMode) one step at a
 * time, up while the reading is under thresholdLow and down while it is over
 * thresholdHigh, until it has alternated; 1 at the limit of the register.
 */
static __bit dacStepSearch(__xdata uint8_t *reg)
{
	uint8_t history = 0;
	uint8_t reading;
	__bit done = 0;

	while (!done) {
		if (!probeCapture(&reading))
			return 0;
		if (reading < thresholdLow) {
			if (dacOffsetMode) {
				if (*reg == dacOffset - 1 && !P4_2) {
					P4_2 = 1;
					*reg = 0;
					dacValue++;
				} else {
					if (*reg == 0x7f && P4_2)
						return 1;
					*reg = *reg + 1;
					dacValue++;
				}
			} else {
				if (*reg == 0x7f)
					return 1;
				*reg = *reg + 1;
				dacValue++;
			}
			history <<= 1;
		} else if (thresholdHigh < reading) {
			if (dacOffsetMode) {
				if (*reg == 0 && P4_2) {
					*reg = dacOffset - 1;
					P4_2 = 0;
					dacValue--;
				} else {
					if (*reg == 0 && !P4_2)
						return 1;
					*reg = *reg - 1;
					dacValue--;
				}
			} else {
				if (*reg == 0)
					return 1;
				*reg = *reg - 1;
				dacValue--;
			}
			history = (history << 1) + 1;
		} else {
			return 0;
		}
		done = searchSettled(history);
		history &= 7;
	}
	return 0;
}

/* Reads the DAC's capture, once (timer 2 capture 6). */
static void captureWait6(void)
{
	STROBE();
	delay(0xffff);
	while (!(T2CON_6 | timedOut))
		;
}

/*
 * Steps *reg (0..0x3f) by stepSize toward the sign of the capture (the
 * comparator's output in bit 7), the direction reversed by IRCON.1; halves the
 * step each time and ends with a step of 1 when probeCount reaches 1 and the
 * history is alternating. 1 at the limits.
 */
static __bit trackSearch(__xdata uint8_t *reg)
{
	uint8_t history = 0;
	uint8_t value;

	flag23_6 = 0;
	while (!(flag23_6 | hostEvent)) {
		captureWait6();
		if (timedOut)
			return 0;
		if (!(CML6 & 0x80)) {
			value = *reg;
			if (value == 0x3f)
				return 1;
			*reg = IRCON_1 ? value + stepSize : value - stepSize;
			history <<= 1;
		} else {
			value = *reg;
			if (value == 0)
				return 1;
			*reg = IRCON_1 ? value - stepSize : value + stepSize;
			history = (history << 1) + 1;
		}
		if (probeCount == 1) {
			stepSize = 1;
			flag23_7 = searchSettled(history);
			flag23_6 = flag23_7;
		} else {
			probeCount--;
			stepSize >>= 1;
			delay(0xffff);
		}
		history &= 7;
	}
	return 0;
}

/* Steps the low nibble (0..15) of *reg, up while capture bit 7 is set, until the history alternates. */
static __bit nibbleSearch(__xdata uint8_t *reg)
{
	uint8_t history = 0;
	uint8_t nibble = 7;

	flag24_0 = 0;
	while (!(flag24_0 | hostEvent)) {
		captureWait6();
		if (timedOut)
			return 0;
		if ((CML6 & 0x80) == 0x80) {
			if (nibble == 0x0f)
				return 1;
			nibble++;
			history = (history << 1) + 1;
		} else {
			if (nibble == 0)
				return 1;
			nibble--;
			history <<= 1;
		}
		flag24_1 = searchSettled(history);
		flag24_0 = flag24_1;
		*reg = (*reg & 0xf0) | nibble;
		history &= 7;
		delay(0xffff);
	}
	return 0;
}

/*
 * Binary search of *reg against the target operandB (a timer-1 capture, CMH7
 * and CMEN, taken 0xffd3 and 0xfffc counts after the pulse): adds stepSize
 * when over, subtracts when under, halving, probeCount times.
 */
static void binarySearchTarget(__xdata uint8_t *reg)
{
	uint16_t measured;

	while (probeCount != 0) {
		delay(0xffd3);
		P3_1 = 1;
		delay(0xfffc);
		while (!(T2CON_5 | timedOut))
			;
		P3_1 = 0;
		if (timedOut)
			return;
		measured = ((uint16_t)CMEN << 8) | CMH7;
		if (operandB < measured)
			*reg = *reg + stepSize;
		else if (measured < operandB)
			*reg = *reg - stepSize;
		else
			return;
		probeCount--;
		stepSize >>= 1;
	}
}

/*
 * Searches *reg (0..0x3f) one step at a time for the target operandB by the
 * same measurement, until the history alternates; then picks the nearer of the
 * last two codes. 1 at the limits.
 */
static __bit stepSearchTarget(__xdata uint8_t *reg)
{
	uint8_t history = 0;
	uint8_t code = *reg;
	uint16_t measured, before, after;

	flag24_2 = 0;
	while (!flag24_2) {
		delay(0xffd3);
		P3_1 = 1;
		delay(0xfffc);
		while (!(T2CON_5 | timedOut))
			;
		P3_1 = 0;
		if (timedOut)
			return 0;
		measured = ((uint16_t)CMEN << 8) | CMH7;
		if (operandB < measured) {
			if (code == 0x3f)
				return 1;
			code++;
			history = (history << 1) + 1;
		} else {
			if (code == 0)
				return 1;
			code--;
			history <<= 1;
		}
		flag24_3 = searchSettled(history);
		flag24_2 = flag24_3;
		if (!flag24_2) {
			operandA = measured;
			*reg = code;
			history &= 7;
			continue;
		}
		if ((history & 1) == 1) {
			after = measured - operandB;
			before = operandB - operandA;
			*reg = before < after ? code : code - 1;
		} else {
			after = operandB - measured;
			before = operandA - operandB;
			*reg = before < after ? code : code + 1;
		}
	}
	return 0;
}

/*
 * Searches a nibble of *reg (selected by work: bits 4..7 pick the status bit of
 * T2CON that reports above/below, bit 0..3 the low (1) or high nibble) until
 * the history alternates. 1 at the limits.
 */
static __bit nibbleSearchStatus(__xdata uint8_t *reg)
{
	uint8_t history = 0;
	uint8_t nibble = 7;
	uint8_t selector = work & 0xf0;
	__bit status;

	flag24_5 = 0;
	while (!(flag24_5 | timedOut)) {
		if (selector == 0x10)
			status = T2CON_0;
		else if (selector == 0x20)
			status = T2CON_1;
		else if (selector == 0x40)
			status = T2CON_2;
		else
			status = T2CON_3;
		if (!status) {
			if (nibble == 0x0f)
				return 1;
			nibble++;
			history = (history << 1) + 1;
		} else {
			if (nibble == 0)
				return 1;
			nibble--;
			history <<= 1;
		}
		flag24_6 = searchSettled(history);
		flag24_5 = flag24_6;
		history &= 7;
		if ((work & 0x0f) == 1)
			*reg = (*reg & 0xf0) | nibble;
		else
			*reg = (*reg & 0x0f) | (nibble << 4);
	}
	return 0;
}

/* Runs a search over reg and keeps its failure in P3.6. */
#define SEARCH_STICKY(call)	do { if (call) P3_6 = 1; } while (0)

/* The DAC's start for a search: the code in dacValue, its offset form and the probes. */
static void dacStart(void)
{
	if (dacOffsetMode) {
		dacValue = 0x7f;
		P4_2 = 1;
		XB(0x3c5e) = 0x7f - dacOffset;
		probeCount = 7;
		stepSize = 0x40;
	} else {
		dacValue = 0x3f;
		XB(0x3c5e) = 0x3f;
		probeCount = 6;
		stepSize = 0x20;
	}
}

/* A 16-bit value in XDATA, low byte first. */
static uint16_t xdataRead16(uint16_t addr)
{
	return ((uint16_t)XB(addr + 1) << 8) | XB(addr);
}

static void xdataWrite16(uint16_t addr, uint16_t value)
{
	XB(addr) = value;
	XB(addr + 1) = value >> 8;
}

/* base plus the signed delta, held at 0 and 0xff; P3.6 reports the clamp. */
static uint8_t addClamped(uint8_t base, uint8_t delta)
{
	uint8_t sum = base + delta;

	if (delta & 0x80) {
		if (base < sum) {
			sum = 0;
			P3_6 = 1;
		}
	} else if (sum < base) {
		sum = 0xff;
		P3_6 = 1;
	}
	return sum;
}

/*
 * Writes a DAC code to the register at 0x3C5E: whole when dacOffsetMode is
 * clear, otherwise split in P4.2 (the upper half) and the register, the code
 * taken from dacOffset. markZero keeps dacAtZero current.
 */
static void dacOutput(uint8_t code, __bit markZero)
{
	if (dacOffsetMode) {
		if (code < dacOffset) {
			XB(0x3c5e) = code;
			P4_2 = 0;
			if (markZero)
				dacAtZero = code == 0;
		} else if (dacMax < code) {
			XB(0x3c5e) = 0x7f;
			P4_2 = 1;
		} else {
			P4_2 = 1;
			XB(0x3c5e) = code - dacOffset;
		}
	} else if (markZero) {
		if (code & 0x80) {
			XB(0x3c5e) = 0x7f;
		} else {
			XB(0x3c5e) = code;
			dacAtZero = code == 0;
		}
	} else {
		XB(0x3c5e) = code;
	}
}

/* Moves offsetA by one step from the capture, against limitLow and limitHigh; 1 at the limit. */
static __bit offsetStep(void)
{
	STROBE();
	delay(0xffff);
	while (!(T2CON_6 | timedOut))
		;
	if (timedOut)
		return 0;
	if (CML6 < limitHigh) {
		if (offsetA == 0x7f)
			return 1;
		offsetA++;
	} else if (limitLow < CML6) {
		if (offsetA == 0x80)
			return 1;
		offsetA--;
	}
	MD4 = (MD4 & 0xcf) | 0x30;
	return 0;
}

/*
 * Moves offsetB toward the window [windowLow, windowHigh] by the capture (the
 * difference of two when the comparator is switched in between); 1 at the limit.
 */
static __bit offsetMeasure(void)
{
	uint8_t reading = 0;
	uint8_t first;

	if (modeDirect) {
		STROBE();
		delay(0xffff);
		while (!T2CON_6)
			;
		reading = CML6;
	} else if (!option69_3) {
		IRCON_6 = 0;
		IRCON_6 = 0;
		STROBE();
		delay(0xffff);
		while (!T2CON_6)
			;
		if (captureToggle)
			reading = CML6 < lastCapture ? 0 : CML6 - lastCapture;
		else
			lastCapture = CML6;
	} else {
		IRCON_6 = 0;
		IRCON_6 = 0;
		STROBE();
		delay(0xffff);
		while (!T2CON_6)
			;
		first = CML6;
		IRCON_4 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		IRCON_6 = 0;
		STROBE();
		delay(0xffff);
		while (!T2CON_6)
			;
		reading = CML6 < first ? 0 : CML6 - first;
	}
	MD4 = (MD4 & 0xcf) | 0x30;
	flag24_7 = modeDirect | captureToggle;
	if (flag24_7 | option69_3) {
		if (reading < windowLow) {
			if (offsetB == 0x7f)
				return 1;
			offsetB++;
		} else if (windowHigh < reading) {
			if ((offsetB == 0x80) | dacAtZero)
				return 1;
			offsetB--;
		}
	}
	return 0;
}

/*
 * The response of table entry work: how far the reading at the code is from
 * the base, scaled and tripled (the response in 16-bit fixed point), then
 * divided into the capture by the reciprocal, with the host's correction
 * (X[3C76]) added or subtracted. The coarse entries (fine clear) take the
 * coarse base and scale 0x4554, the fine ones 0x4433. P3.6 collects the
 * overflows.
 */
static void responseEntry(__bit fine)
{
	uint16_t reading = xdataRead16(TABULATED + 2 * work);
	uint16_t base = xdataRead16(fine ? BASE_FINE : BASE_COARSE);
	uint32_t p;
	uint16_t d, value;

	operandB = fine ? 0x4433 : 0x4554;
	if (reading >= base) {
		operandA = reading - base;
		p = multiplyOperands();
		d = -(uint16_t)(p >> 16);
	} else {
		operandA = base - reading;
		p = multiplyOperands();
		d = p >> 16;
	}
	operandA = 3 * d + 0x8000;
	operandB = capture;
	p = multiplyOperands();
	P3_6 = (p & 0x80000000UL) == 0x80000000UL;
	value = reciprocal((p & 0x7fffffffUL) >> 15) >> 4;
	if (XB(0x3c76) & 0x80)
		value -= XB(0x3c76) & 0x7f;
	else
		value += XB(0x3c76) & 0x7f;
	xdataWrite16(READINGS + 2 * work, value);
	MD3 = XB(READINGS + 2 * work);
	MD4 = XB(READINGS + 2 * work + 1);
}

/* Two captures of the timer with X[3C6E] at 0x3f and 0, and the product of their difference. */
static void responseCapture(void)
{
	uint16_t first;
	uint32_t p;

	XB(0x3c6e) = 0x3f;
	timeoutStart(0xf060);
	delay(0xffff);
	captureTimer();
	first = capture;
	TR1 = 0;
	TF1 = 0;
	XB(0x3c6e) = 0;
	timeoutStart(0xf060);
	delay(0xffff);
	captureTimer();
	TR1 = 0;
	TF1 = 0;
	if (timedOut)
		return;
	operandA = 0x1156;
	operandB = capture - first;
	p = multiplyOperands();
	P3_6 = p > 0xffffffUL;
	capture = p >> 8;
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

/* The host's request. */
void ie0Isr(void) __interrupt(0) __using(1)
{
	EA = 0;
	TR1 = 0;
	TF1 = 0;
	hostEvent = 1;
	if (P0_1) {
		XB(0x3c4a) = (XB(0x3c4a) & 0xf0) | ((XB(0x3c4a) + 1) & 0x0f);
		if (!(XB(0x3c58) & 0x40)) {
			XB(0x3c51) |= 0x40;
			while (!P1_5)
				;
		}
	}
}

/* The channel indices (and codes) the host may select, by the channel family X[3C02] & 0xF0. */
static void channelTablesSet(uint8_t family)
{
	uint8_t i;

	for (i = 0; i < 6; i++) {
		channelStep[i] = 0;
		channelIndex[i] = 0x7f;
	}
	switch (family) {
	case 0x10:
	case 0x40:
		channelIndex[0] = 0x0a;
		channelIndex[1] = 0x16;
		channelIndex[2] = 0x22;
		channelIndex[3] = 0x2e;
		channelIndex[4] = 0x3a;
		channelIndex[5] = 0x46;
		break;
	case 0x70:
	case 0xb0:
		channelIndex[0] = 0x03;
		channelIndex[1] = 0x10;
		channelIndex[2] = 0x1d;
		channelIndex[3] = 0x2a;
		channelIndex[4] = 0x37;
		channelIndex[5] = 0x44;
		break;
	case 0xd0:
	case 0x90:
		channelIndex[0] = 0x11;
		channelStep[0] = 3;
		channelIndex[1] = 0x24;
		channelStep[1] = 4;
		channelIndex[2] = 0x38;
		channelStep[2] = 1;
		channelIndex[3] = 0x4b;
		channelStep[3] = 2;
		break;
	case 0x20:
		channelIndex[0] = 0x12;
		channelIndex[1] = 0x26;
		channelIndex[2] = 0x3a;
		channelIndex[3] = 0x4e;
		break;
	case 0x60:
		channelIndex[0] = 0x16;
		channelIndex[1] = 0x2e;
		channelIndex[2] = 0x46;
		break;
	case 0xe0:
	case 0xf0:
		channelIndex[0] = 0x10;
		channelIndex[1] = 0x2a;
		channelIndex[2] = 0x44;
		break;
	case 0x00:
	case 0xc0:
		channelIndex[0] = 0x11;
		channelStep[0] = 3;
		channelIndex[1] = 0x38;
		channelStep[1] = 1;
		break;
	case 0x80:
		channelIndex[0] = 0x26;
		channelIndex[1] = 0x4e;
		break;
	case 0xa0:
		channelIndex[0] = 0x12;
		channelIndex[1] = 0x3e;
		break;
	case 0x30:
		channelIndex[0] = 0x2a;
		break;
	}
}

/* A pulse on the strobe line IRCON.3. */
#define PULSE_IRCON3()	do { IRCON_3 = 1; IRCON_3 = 0; } while (0)

/* A pulse on the host line P3.5. */
#define PULSE_P35()	do { P3_5 = 1; P3_5 = 0; } while (0)

/* MD2 for the host's mode: 1 with X[3C58] bit 5, otherwise 3. */
#define MD2_PULSE_MODE()	(MD2 = (XB(0x3c58) & 0x20) ? 1 : 3)

/* P3.6 keeps the failure of a search. */
#define STICKY(call)	(P3_6 = P3_6 | (call))

/* The search window the tracking states use, from the level's drift and delta. */
static void windowFor(uint8_t drift, uint8_t delta)
{
	driftApplied = drift;
	windowHigh = levelMid - delta;
	windowLow = levelLow - delta;
}

static void windowZero(void)
{
	driftApplied = 0;
	windowHigh = levelMid;
	windowLow = levelLow;
}

/* The calibrated pair X[3C7E], X[3C80] of the first or second pass, and its FB level. */
static void restorePair(__bit second)
{
	if (second) {
		XB(0x3c7e) = saveD[0];
		XB(0x3c80) = saveD[1];
		SFR_FB = 0x90;
	} else {
		XB(0x3c7e) = saveC[0];
		XB(0x3c80) = saveC[1];
		SFR_FB = 0xe0;
	}
}

/* The calibrated triple X[3C77], X[3C78], X[3C7B] of the first or second pass. */
static void restoreTriple(__bit second)
{
	if (second) {
		XB(0x3c77) = saveB[0];
		XB(0x3c78) = saveB[1];
		XB(0x3c7b) = saveB[2];
	} else {
		XB(0x3c77) = saveA[0];
		XB(0x3c78) = saveA[1];
		XB(0x3c7b) = saveA[2];
	}
}

/* The words the radio's tables start with, 0x2D00..0x2D1D. */
static void radioTableInit(void)
{
	xdataWrite16(0x2d00, 0x9596);
	xdataWrite16(0x2d02, 0x8bc9);
	xdataWrite16(0x2d04, 0x9059);
	xdataWrite16(0x2d06, 0x9b81);
	xdataWrite16(0x2d08, 0xa206);
	xdataWrite16(0x2d0a, 0x3ffc);
	xdataWrite16(0x2d0c, 0x3ff8);
	xdataWrite16(0x2d0e, 0x3ff8);
	xdataWrite16(0x2d10, 0x3ff8);
	xdataWrite16(0x2d12, 0x3ffa);
	xdataWrite16(0x2d14, 0x3ffe);
	xdataWrite16(0x2d16, 0x3ffc);
	xdataWrite16(0x2d18, 0x3ffc);
	xdataWrite16(0x2d1a, 0x3ffc);
	xdataWrite16(0x2d1c, 0x3ffd);
}

/* The tabulation: the codes, readings and levels found for each target. */
static void stateTabulate(void)
{
	uint16_t t, t2;

	if ((XB(0x3c59) & 0x20) == 0x20) {
		work = 0;
		work2 = 0;
		while (work2 < 0x52) {
			if (xdataRead16(READINGS) < xdataRead16(TARGETS + 2 * work2))
				break;
			XB(CODES + 0x50 - work2) = savedCode;
			XB(LEVELS_FOUND + 0x50 - work2) = XB(LEVELS);
			work2++;
		}
		work = 1;
		while (work2 < 0x51) {
			t = xdataRead16(READINGS + 2 * work);
			t2 = xdataRead16(TARGETS + 2 * work2);
			if (!(t2 < t)) {
				work++;
				continue;
			}
			if ((uint16_t)(t2 - xdataRead16(READINGS + 2 * (work - 1))) < (uint16_t)(t - t2)) {
				XB(CODES + 0x50 - work2) = (work - 1) + savedCode;
				xdataWrite16(TABULATED + 2 * (0x50 - work2), xdataRead16(READINGS + 2 * (work - 1)));
				XB(LEVELS_FOUND + 0x50 - work2) = XB(LEVELS + work - 1);
			} else {
				XB(CODES + 0x50 - work2) = work + savedCode;
				xdataWrite16(TABULATED + 2 * (0x50 - work2), t);
				XB(LEVELS_FOUND + 0x50 - work2) = XB(LEVELS + work);
			}
			work2++;
		}
		XB(0x3c70) = XB(BASE_COARSE);
		XB(0x3c71) = XB(BASE_COARSE + 1);
		if (work2 < 0x52)
			XB(0x3c57) |= 0x80;
}
STATE = work2 >= 0x52 ? ST_ABORT : ST_COPY;
}

/* The host's tracking request: the channel's code and level, the window, the radio's lines. */
static void stateTrackSelect(void)
{
	uint8_t i, d, idx;

	if (portMode == 5 && (XB(0x3c74) & 0x80) == 0x80)
		P2_6 = 1;
	if (portMode == 2) {
		work = PORT_INDEX + 2;
		channelCode = XB(CODES + work);
		channelLevel = XB(LEVELS_FOUND + work);
		MD3 = XB(READINGS + 2 * work);
		MD4 = XB(READINGS + 2 * work + 1);
		work = XB(0x3c0c) & 7;
		trackFlagA = (work & 2) == 2;
		trackFlagB = (work & 4) == 4;
		if (trackFlagA)
			MD1 = option5c_7 ? runA : baseA;
		else
			MD1 = option5c_7 ? runB : baseB;
		if (calibrate) {
			if (trackFlagA) {
				if (trackFlagB | readingLow)
					windowZero();
				else
					windowFor(drift1, deltaB);
			} else if (readingLow) {
				windowZero();
			} else if (readingHigh) {
				windowFor(drift1, deltaB);
			} else if (trackFlagB) {
				windowFor(drift1, deltaB);
			} else {
				windowFor(drift2, deltaC);
			}
		}
} else if (portMode == 3) {
	channelCode = XB(CODES + PORT_INDEX);
	channelLevel = XB(LEVELS_FOUND + PORT_INDEX);
	if (calibrate) {
		if ((XB(0x3c0e) & 3) == 1) {
			if (readingLow)
				windowZero();
			else
				windowFor(drift1, deltaB);
		} else if (readingLow) {
			windowZero();
		} else {
			windowFor(drift2, deltaC);
		}
	}
} else {
	idx = portMode == 5 ? XB(0x3c98) : PORT_INDEX;
	channelCode = XB(CODES + idx);
	channelLevel = XB(LEVELS_FOUND + idx);
	if (calibrate) {
		if (readingLow)
			windowFor(drift0, deltaA);
		else
			windowFor(drift2, deltaC);
	}
}
work = addClamped(channelCode, offsetA);
if (portMode == 5) {
	SFR_FE = work;
	SFR_FF = work;
} else {
	CCL1 = work;
}
if (calibrate)
	channelLevel = channelLevel >= driftApplied ? channelLevel - driftApplied : 0;
work2 = addClamped(channelLevel, offsetB);
dacOutput(work2, 1);
P2_7 = 1;
switch (P1 & 7) {
case 2:
	P6 = 0xfe;
	SFR_FB = 0x84;
	P4_3 = 1;
	P4_3 = 0;
	delay(0xffff);
	P4_3 = 1;
	delay(0xffff);
	PULSE_IRCON3();
	break;
case 3:
	SFR_FB = 0x80;
	SFR_FC = 0x80;
	P6 = 0xf8;
	if (optionPairA && (XB(0x3c09) & 0x10) == 0x10) {
		d = PORT_INDEX;
		if (d == channelIndex[0]) {
			channelCode = channelStep[0];
			P5_3 = 1;
		} else if (d == channelIndex[1]) {
			channelCode = channelStep[1];
			P5_3 = 1;
		} else if (d == channelIndex[2]) {
			channelCode = channelStep[2];
			P5_3 = 1;
		} else if (d == channelIndex[3]) {
			channelCode = channelStep[3];
			P5_3 = 1;
		} else if (d == channelIndex[4] || d == channelIndex[5]) {
			channelCode = 0;
			P5_3 = 1;
		} else if (d == 0x4e) {
			if (option7d_6 || (P1_3 & option7a_6)) {
				channelCode = 0;
				P5_3 = 1;
			}
		} else if (d == 0x1e) {
			if (option7a_7) {
				channelCode = 0;
				P5_3 = 1;
			}
		}
		idx = 2 * channelCode;
		CTRELL = XB(0x2d00 + idx);
		CTRELH = XB(0x2d00 + idx + 1);
		CML3 = XB(0x2d0a + idx);
		CMH3 = XB(0x2d0a + idx + 1);
		CML4 = XB(0x2d14 + idx);
		CMH4 = XB(0x2d14 + idx + 1);
		CML5 = CTRELL;
		CMH5 = CTRELH;
		MD5 = CML4;
		ARCON = CMH4;
	} else {
		delay(0xfffc);
	}
	PULSE_IRCON3();
	break;
case 5:
	channelCode = XB(CODES + XB(0x3c99));
	work = addClamped(channelCode, offsetA);
	CCL1 = XB(0x2e89);
	SFR_FB = 0x80;
	MD4 = (MD4 & 0x7f) | 0x80;
	operandA = xdataRead16(TARGETS + 2 * (0x50 - XB(0x3c98)));
	if ((XB(0x3c74) & 0x60) == 0x20)
		delay(0xfe84);
	else if ((XB(0x3c74) & 0x60) == 0x60)
		delay(0xfe25);
	SFR_FC = 0x80;
	P6 = option65_7 ? 0xf9 : 0xd9;
	delay(0xfffb);
	PULSE_IRCON3();
	trackPasses = 0;
	delay(0xffba);
	break;
case 6:
case 7:
	SFR_FB = 0x80;
	MD4 = (MD4 & 0x7f) | 0x80;
	SFR_FC = 0x80;
	P6 = 0xf8;
	delay(0xfffb);
	PULSE_IRCON3();
	trackPasses = 0;
	break;
default:
	P2_7 = 0;
	break;
}
if (XB(0x3c48) & 1) {
	delay(0xfff0);
	delay(0xfff0);
}
if (XB(0x3c48) & 2) {
	delay(0xfff0);
	delay(0xfff0);
	delay(0xfff0);
	delay(0xfff0);
}
if (XB(0x3c48) & 4) {
	for (i = 0; i < 8; i++)
		delay(0xfff0);
}
if (!P2_7) {
	STATE = ST_IDLE;
} else if (!option69_3 || trackPasses < 2) {
	STATE = ST_TRACK_B;
	if (option58_0) {
		P2_4 = 1;
		IRCON_7 = 1;
	}
} else {
	delay(0xffd8);
	STATE = ST_TRACK_A;
	if (option59_7) {
		P2_2 = 1;
		IRCON_7 = 1;
	}
}
}

void main(void)
{
	resetPending = 0;
	radioTableInit();
	timedOut = 0;
	SFR_8E = 0;
	SFR_WDTREL = 0;
	TCON = 0;
	TMOD = 0;
	XB(0x3c51) |= 1;
	P3_5 = 0;
	P3_5 = 0;
	P3_5 = 0;
	P3_5 = 1;
	P3_5 = 0;
	for (;;) {
		if (STATE > ST_COUNT - 1)
			continue;
		switch (STATE) {
		case ST_WAIT_HOST:
			P6 = 0;
			SFR_FB = 0;
			SFR_FC = 0;
			SFR_FD = 0x80;
			P2 = 0;
			P3 = 0;
			IRCON = 0;
			CTRELL = 0;
			CTRELH = 0;
			CML3 = 0;
			CMH3 = 0;
			CML4 = 0;
			CMH4 = 0;
			CML5 = 0;
			CMH5 = 0;
			MD5 = 0;
			ARCON = 0;
			delay(0xffff);
			XB(0x3c4a) = 0;
			if (resetPending) {
				P3_5 = 1;
				P3_5 = 0;
				resetPending = 0;
			}
			STATE = (!P1_1 && !P1_0) ? ST_WAIT_HOST : ST_INIT_TABLES;
			break;

		case ST_INIT_TABLES:
			invertSense = XB(0x3c5b) == 6;
			work = 0;
			xdataWrite16(TARGETS, 0x1b9f);
			while (work < 0x50) {
				xdataWrite16(TARGETS + 2 * (work + 1), xdataRead16(TARGETS + 2 * work) + 0x32);
				work++;
			}
			channelTablesSet(XB(0x3c02) & 0xf0);
			STATE = ST_INIT_REGISTERS;
			/* fall through */

		case ST_INIT_REGISTERS:
			MD2 = 0;
			P0 = 0;
			EA = 1;
			EX0 = 1;
			SFR_FB = 0;
			SFR_FC = 0;
			SFR_FD = 0;
			P6 = 0;
			P2 = 0;
			P3 = 0;
			IRCON = 0;
			XB(0x3c57) = 0;
			P2_6 = 1;
			MD4 = (MD4 & 0xbf) | 0x40;
			MD4 &= 0xbf;
			XB(0x3c5e) = 0x3f;
			CCL1 = 0x7f;
			MD3 = 0;
			MD4 = 8;
			XB(0x3c6e) = 0x20;
			P4 = 0x88;
			XB(0x3c77) = 0x1f;
			XB(0x3c78) = 0x1f;
			XB(0x3c7b) = 0x27;
			XB(0x3c7e) = 0x1f;
			XB(0x3c80) = 7;
			XB(0x3c82) = 0x1f;
			XB(0x3c92) = 0x70;
			XB(0x3c94) = 0x77;
			XB(0x3c96) = 0x77;
			SFR_FE = 0x84;
			SFR_FF = 0x70;
			P5_3 = 0;
			P5_7 = 0;
			MD1 = XB(0x3c5c) & 0x3f;
			captureToggle = 0;
			timedOut = 0;
			offsetA = 0;
			offsetB = 0;
			trackFlagA = 0;
			trackPasses = 0;
			tableIndex = 0;
			lastCapture = 0;
			work = 0;
			work2 = 0;
			stepCount = 0;
			stepActive = 0;
			stepRestart = 0;
			readingLow = 0;
			readingHigh = 0;
			dacAtZero = 0;
			gateState = 0;
			option66 = (XB(0x3c66) & 0x20) == 0;
			optionPairA = (XB(0x3c79) & 0x40) == 0x40;
			option7d_6 = (XB(0x3c7d) & 0x40) == 0x40;
			option7a_7 = (XB(0x3c7a) & 0x80) == 0x80;
			option7a_6 = (XB(0x3c7a) & 0x40) == 0x40;
			optionPairB = (XB(0x3c79) & 0x80) == 0x80;
			option58_0 = (XB(0x3c58) & 1) == 1;
			option59_7 = (XB(0x3c59) & 0x80) == 0x80;
			option65_7 = (XB(0x3c65) & 0x80) == 0x80;
			dacOffsetMode = (XB(0x3c69) & 4) == 4;
			calibrate = (XB(0x3c58) & 0x80) == 0;
			modeDirect = (XB(0x3c58) & 2) == 2;
			option69_3 = (XB(0x3c69) & 8) == 8;
			option65_3 = (XB(0x3c65) & 8) == 8;
			option5c_7 = (XB(0x3c5c) & 0x80) == 0x80;
			if (modeDirect) {
				if (invertSense) {
					P4_2 = 0;
					P4_5 = 0;
				} else {
					MD4 &= 0xcf;
				}
			} else if (invertSense) {
				P4_2 = 1;
				P4_5 = 0;
			} else {
				MD4 = (MD4 & 0xcf) | 0x20;
			}
			levelLow = XB(0x3c60) & 0x3f;
			levelMid = levelLow + 2 * (1 + (XB(0x3c60) >> 6));
			thresholdHigh = levelMid;
			thresholdLow = levelLow;
			windowHigh = levelMid;
			windowLow = levelLow;
			deltaA = (XB(0x3c9d) & 0x0f) * 2;
			deltaB = ((XB(0x3c9d) >> 4) & 0x0f) * 2;
			deltaC = (XB(0x3c9e) & 0x0f) * 2;
			dacOffset = XB(0x3c86);
			dacMax = dacOffset + 0x7f;
			baseA = XB(0x3c5c) & 0x3f;
			baseB = XB(0x3c81) & 0x3f;
			runA = baseA;
			runB = baseB;
			gateOn = XB(0x3d87);
			gateHigh = ((XB(0x3cab) >> 4) & 0x0f) + gateOn;
			gateOff = XB(0x3d88);
			gateLow = (XB(0x3cab) & 0x0f) + gateOff;
			switch (XB(0x3c65) & 0x70) {
			case 0x00:
				limitLevel = 0x2dfb;
				break;
			case 0x10:
				limitLevel = 0x2d33;
				break;
			case 0x20:
				limitLevel = 0x2c6b;
				break;
			default:
				limitLevel = 0x2ba3;
				break;
			}
			switch (XB(0x3c69) & 0x30) {
			case 0x00:
				limitLow = 0xa3;
				break;
			case 0x10:
				limitLow = 0xad;
				break;
			case 0x20:
				limitLow = 0xb6;
				break;
			default:
				limitLow = 0xbf;
				break;
			}
			switch (XB(0x3c69) & 3) {
			case 0:
				limitHigh = 0x6d;
				break;
			case 1:
				limitHigh = 0x64;
				break;
			case 2:
				limitHigh = 0x5b;
				break;
			default:
				limitHigh = 0x51;
				break;
			}
			delay(0xfed4);
			delay(0xfe0c);
			delay(0xfe0c);
			delay(0xfe0c);
			delay(0xfe0c);
			STATE = ST_COARSE_DAC;
			/* fall through */

		case ST_COARSE_DAC:
			if ((XB(0x3c59) & 0x40) != 0x40) {
				STATE = ST_STEP_A;
				break;
			}
			dacStart();
			P6 = 0xf9;
			SFR_FB = 0x80;
			SFR_FC = 0x80;
			delay(0xffff);
			PULSE_IRCON3();
			delay(0xfe0c);
			timeoutStart(0xf448);
			P2_5 = 1;
			IRCON_7 = 1;
			delay(0xfffe);
			dacSearch(&XB(0x3c5e));
			delay(0xffff);
			IRCON_7 = 0;
			P2_5 = 0;
			TR1 = 0;
			TF1 = 0;
			if (timedOut) {
				STATE = ST_ABORT;
			} else {
				XB(0x3c57) |= 0x40;
				STATE = ST_STEP_A;
			}
			break;

		case ST_STEP_A:
			if ((XB(0x3c59) & 0x20) != 0x20) {
				STATE = ST_RESPONSE;
				break;
			}
			P2_3 = 1;
			if (!stepActive) {
				P6 = 0xf9;
				SFR_FB = 0x80;
				SFR_FC = 0x80;
				SFR_FD = 0;
				stepCount = 7;
				stepRate = 0x40;
				stepActive = 1;
				operandA = ((uint16_t)XB(0x3c62) << 8) | XB(0x3c63);
				delay(0xffff);
				PULSE_IRCON3();
				delay(0xffff);
			} else if (stepRestart) {
				CCL1 = 0x3f;
				stepCount = 6;
				stepRate = 0x20;
				stepRestart = 0;
			} else {
				stepCount--;
				stepRate >>= 1;
			}
			timeoutStart(0xf060);
			captureTimer();
			if (stepCount == 0) {
				xdataWrite16(READINGS, capture);
				savedCode = CCL1;
				MD0 = CCL1;
				XB(LEVELS) = dacValue;
			} else if (operandA - 0xc8 < capture) {
				if (operandA + 0xc8 < capture)
					CCL1 -= stepRate;
			} else {
				CCL1 += stepRate;
			}
			P2_3 = 0;
			TR1 = 0;
			TF1 = 0;
			if (timedOut) {
				STATE = ST_ABORT;
			} else if (stepCount == 0) {
				if (CCL1 == 0xfe) {
					stepRestart = 1;
					STATE = ST_STEP_A;
				} else {
					STATE = ST_STEP_C;
					CCL1 = savedCode + 1;
					tableIndex = 1;
				}
			} else {
				STATE = ST_COARSE_DAC_2;
			}
			break;

		case ST_COARSE_DAC_2:
			if ((XB(0x3c59) & 0x40) != 0x40) {
				STATE = ST_STEP_A;
				break;
			}
			timeoutStart(0xf060);
			dacStart();
			P2_5 = 1;
			IRCON_7 = 1;
			delay(0xfffe);
			dacSearch(&XB(0x3c5e));
			IRCON_7 = 0;
			P2_5 = 0;
			TR1 = 0;
			TF1 = 0;
			STATE = timedOut ? ST_ABORT : ST_STEP_A;
			break;

		case ST_STEP_B:
			if ((XB(0x3c59) & 0x20) != 0x20)
				break;
			P2_3 = 1;
			delay(0xffff);
			timeoutStart(0xf060);
			captureTimer();
			xdataWrite16(READINGS + 2 * tableIndex, capture);
			P2_3 = 0;
			TR1 = 0;
			TF1 = 0;
			if (timedOut) {
				STATE = ST_ABORT;
			} else if (limitLevel < xdataRead16(READINGS + 2 * tableIndex)) {
				XB(0x3c57) |= 0x20;
				STATE = ST_TABULATE;
			} else if (CCL1 == 0xff) {
				STATE = ST_ABORT;
			} else if (tableIndex == 0x50) {
				STATE = ST_ABORT;
			} else {
				CCL1++;
				tableIndex++;
				STATE = ST_STEP_C;
			}
			break;

		case ST_STEP_C:
			if ((XB(0x3c59) & 0x40) == 0x40) {
				P2_5 = 1;
				IRCON_7 = 1;
				delay(0xfffe);
				timeoutStart(0xf060);
				STICKY(dacStepSearch(&XB(0x3c5e)));
				XB(LEVELS + tableIndex) = dacValue;
				IRCON_7 = 0;
				P2_5 = 0;
				TR1 = 0;
				TF1 = 0;
			}
			STATE = timedOut ? ST_ABORT : ST_STEP_B;
			break;

		case ST_TABULATE:
			stateTabulate();
			break;

		case ST_COPY:
			if (calibrate) {
				CCL1 = XB(LEVELS - 1);
				dacStart();
				P2_5 = 1;
				IRCON_7 = 1;
				delay(0xffff);
				thresholdHigh = levelMid - deltaA;
				thresholdLow = levelLow - deltaA;
				dacSearch(&XB(0x3c5e));
				drift0 = XB(0x2f54) > dacValue ? XB(0x2f54) - dacValue : 0;
				dacStart();
				thresholdHigh = levelMid - deltaB;
				thresholdLow = levelLow - deltaB;
				dacSearch(&XB(0x3c5e));
				drift1 = XB(0x2f54) > dacValue ? XB(0x2f54) - dacValue : 0;
				dacStart();
				thresholdHigh = levelMid - deltaC;
				thresholdLow = levelLow - deltaC;
				dacSearch(&XB(0x3c5e));
				drift2 = XB(0x2f54) > dacValue ? XB(0x2f54) - dacValue : 0;
				IRCON_7 = 0;
				P2_5 = 0;
			}
			dacOutput(XB(LEVELS_FOUND), 0);
			STATE = ST_RESPONSE;
			break;

		case ST_RESPONSE:
			if ((XB(0x3c59) & 0x10) != 0x10) {
				STATE = ST_GAIN_A;
				break;
			}
			P6 = 0xf9;
			SFR_FB = 0x80;
			SFR_FC = 0x80;
			P2_1 = 1;
			CCL1 = XB(0x2e76);
			responseCapture();
			if (!timedOut) {
				for (work = 0; work < 0x28; work++)
					responseEntry(0);
			}
			CCL1 = XB(0x2e9e);
			responseCapture();
			P4_7 = 0;
			if (!timedOut) {
				for (work = 0x28; work < 0x51; work++)
					responseEntry(1);
			}
			P2_1 = 0;
			P4_6 = 1;
			if (timedOut) {
				STATE = ST_ABORT;
			} else {
				XB(0x3c57) |= 0x10;
				STATE = ST_GAIN_A;
			}
			break;

		case ST_GAIN_A:
			if ((XB(0x3c59) & 8) != 8) {
				STATE = ST_GAIN_B;
				break;
			}
			hostEvent = 0;
			IRCON_2 = 1;
			CCL1 = XB(CODES);
			P6 = 0xf8;
			SFR_FB = (P1_3 & optionPairB) ? 0x90 : 0xa0;
			SFR_FC = 0xff;
			SFR_FD = 0x60;
			XB(0x3c77) = 0x1f;
			XB(0x3c78) = 0x1f;
			XB(0x3c7b) = (XB(0x3c7b) & 0xf0) | 7;
			delay(0xff9c);
			PULSE_IRCON3();
			delay(0xff06);
			if (invertSense) {
				P4_2 = 0;
				P4_5 = 0;
			} else {
				MD4 &= 0xcf;
			}
			IRCON_5 = 1;
			IRCON_1 = 1;
			IRCON_7 = 1;
			if (!(XB(0x3c58) & 0x20))
				MD2 = 2;
			delay(0xffff);
			timeoutStart(0xf448);
			XB(0x3c7b) = (XB(0x3c7b) & 0xcf) | 0x20;
			probeCount = 5;
			stepSize = 0x10;
			STICKY(trackSearch(&XB(0x3c78)));
			XB(0x3c7b) = (XB(0x3c7b) & 0xcf) | 0x10;
			probeCount = 5;
			stepSize = 0x10;
			STICKY(trackSearch(&XB(0x3c77)));
			XB(0x3c7b) &= 0xcf;
			STICKY(nibbleSearch(&XB(0x3c7b)));
			MD2 = 0;
			IRCON_1 = 0;
			TR1 = 0;
			TF1 = 0;
			if (hostEvent) {
				STATE = ST_INIT_REGISTERS;
			} else if (timedOut) {
				P6 = 0;
				IRCON_2 = 0;
				STATE = ST_ABORT;
			} else {
				if (P1_3 & optionPairB) {
					saveB[0] = XB(0x3c77);
					saveB[1] = XB(0x3c78);
					saveB[2] = XB(0x3c7b);
					P5_4 = 1;
				} else {
					saveA[0] = XB(0x3c77);
					saveA[1] = XB(0x3c78);
					saveA[2] = XB(0x3c7b);
					XB(0x3c57) |= 8;
				}
				STATE = ST_GAIN_B;
			}
			break;

		case ST_GAIN_B:
			if ((XB(0x3c59) & 4) != 4) {
				STATE = ST_PHASE;
				break;
			}
			SFR_FC = 0xd4;
			SFR_FD = 0x20;
			XB(0x3c7b) = (XB(0x3c7b) & 0xcf) | 0x20;
			XB(0x3c7e) = 0x1f;
			XB(0x3c80) = 7;
			IRCON_0 = 1;
			P3_4 = 1;
			if (!(XB(0x3c58) & 0x20))
				MD2 = 2;
			timeoutStart(0xf448);
			probeCount = 5;
			stepSize = 0x10;
			STICKY(trackSearch(&XB(0x3c7e)));
			STICKY(nibbleSearch(&XB(0x3c80)));
			MD2 = 0;
			IRCON_7 = 0;
			IRCON_0 = 0;
			IRCON_5 = 0;
			TR1 = 0;
			TF1 = 0;
			if (hostEvent) {
				STATE = ST_INIT_REGISTERS;
			} else if (timedOut) {
				P6 = 0;
				IRCON_2 = 0;
				STATE = ST_ABORT;
			} else {
				if (P1_3 & optionPairB) {
					saveD[0] = XB(0x3c7e);
					saveD[1] = XB(0x3c80);
					P5_5 = 1;
				} else {
					saveC[0] = XB(0x3c7e);
					saveC[1] = XB(0x3c80);
					XB(0x3c57) |= 4;
				}
				if ((XB(0x3c58) & 0x10) == 0x10) {
					P6 = 0;
					IRCON_2 = 0;
					STATE = ST_ABORT;
				} else {
					STATE = ST_PHASE;
				}
			}
			break;

		case ST_PHASE:
			if ((XB(0x3c59) & 2) != 2) {
				STATE = ST_OFFSET;
				break;
			}
			P6 = 0;
			IRCON_2 = 0;
			SFR_FB = 0x80;
			SFR_FC = 0x1f;
			SFR_FD = 0x41;
			XB(0x3c82) = ((XB(0x3c84) & 0xc0) >> 4) + ((XB(0x3c85) & 0xc0) >> 6);
			P3_0 = 1;
			P3_2 = 1;
			P3_4 = 1;
			T2CON_4 = 1;
			operandA = ((uint16_t)(XB(0x3c8b) & 0x70) << 4) | XB(0x3c8d);
			P3_3 = 1;
			delay(0xffc9);
			timeoutStart(0xf448);
			P3_1 = 1;
			delay(0xfffc);
			while (!(T2CON_5 | timedOut))
				;
			operandB = ((uint16_t)CMEN << 8) | CMH7;
			P3_1 = 0;
			operandB = multiplyOperands() >> 11;
			delay(0xffff);
			P3_3 = 0;
			XB(0x3c82) = 0x1f;
			probeCount = 5;
			stepSize = 0x10;
			binarySearchTarget(&XB(0x3c82));
			P3_2 = 1;
			P3_3 = 0;
			STICKY(stepSearchTarget(&XB(0x3c82)));
			P3_2 = 0;
			P3_4 = 0;
			P3_0 = 0;
			T2CON_4 = 0;
			TR1 = 0;
			TF1 = 0;
			if (timedOut) {
				STATE = ST_ABORT;
			} else {
				XB(0x3c57) |= 2;
				STATE = ST_OFFSET;
			}
			break;

		case ST_OFFSET:
			if ((XB(0x3c59) & 1) == 1) {
				timeoutStart(0xf448);
				P6 = 0;
				SFR_FB = 0x80;
				SFR_FC = 0x18;
				SFR_FD = 0x1e;
				P3_7 = 1;
				work = 0x10;
				STICKY(nibbleSearchStatus(&XB(0x3c94)));
				work = 0x21;
				STICKY(nibbleSearchStatus(&XB(0x3c94)));
				work = 0x40;
				STICKY(nibbleSearchStatus(&XB(0x3c96)));
				work = 0x81;
				STICKY(nibbleSearchStatus(&XB(0x3c96)));
				MD4 = (MD4 & 0x7f) | 0x80;
				work = 0x40;
				STICKY(nibbleSearchStatus(&XB(0x3c92)));
				MD4 &= 0x7f;
				P3_7 = 0;
				TR1 = 0;
				TF1 = 0;
				if (!timedOut)
					XB(0x3c57) |= 1;
			}
			STATE = ST_ABORT;
			break;

		case ST_ABORT:
			SFR_FB = 0;
			SFR_FC = 0;
			SFR_FD = 0;
			P6 = 0;
			P4_6 = 1;
			P4_3 = 1;
			P3_4 = 0;
			P3_3 = 0;
			if (timedOut) {
				timedOut = 0;
				XB(0x3c51) |= 0x20;
			} else {
				XB(0x3c51) |= 8;
			}
			STATE = ST_IDLE;
			break;

		case ST_IDLE:
			if (hostEvent) {
				if ((XB(0x3c58) & 0x10) == 0x10) {
					P0 = 0;
					MD2 = 0;
					P2_6 = 1;
					delay(0xfff6);
					STATE = ST_GAIN_A;
				} else {
					STATE = ST_INIT_REGISTERS;
				}
				break;
			}
			if (option65_3) {
				SFR_F9 = 0xe0;
				IRCON_7 = 1;
				MD4 = (MD4 & 0xcf) | 0x30;
			}
			SFR_FB = 0x80;
			SFR_FC = 0;
			SFR_FD = 0;
			P6 = 0;
			P2 = 0;
			P3_4 = 0;
			P3_3 = 0;
			P5_3 = 0;
			MD4 = (MD4 & 0xbf) | 0x40;
			MD4 &= 0x3f;
			if (option65_3) {
				delay(0xffff);
				STROBE();
				delay(0xffff);
				while (!(T2CON_6 | timedOut))
					;
				MD0 = CML6;
				readingLow = XB(0x3c6d) < CML6;
				readingHigh = CML6 < XB(0x3c61);
				IRCON_7 = 0;
			}
			SFR_F9 = 0;
			SFR_FB = 0;
			P5_7 = 0;
			if (P1 != 4 && option5c_7 && P1_6) {
				gateInput = XB(0x3d79);
				if (gateInput != 0) {
					if (gateState == 0x10) {
						if (gateInput < gateOn) {
							runB = baseB;
							runA = baseA;
							gateState = 0;
						}
					} else if (gateState == 0) {
						if (gateHigh < gateInput) {
							runB = baseB - ((XB(0x3cb3) & 0xf0) >> 3);
							runA = baseA - ((XB(0x3cb2) & 0xf0) >> 3);
							gateState = 0x10;
						} else if (gateInput < gateOff) {
							runB = (XB(0x3cb3) & 0x0f) + baseB;
							runA = (XB(0x3cb2) & 0x0f) + baseA;
							gateState = 0x20;
						}
					} else if (gateState == 0x20) {
						if (gateLow < gateInput) {
							runB = baseB;
							runA = baseA;
							gateState = 0;
						}
					}
					XB(0x3c4a) = (XB(0x3c4a) & 0xcf) | gateState;
				}
			}
			MD2 = 1;
			PULSE_P35();
			portMode = P1 & 7;
			if (portMode == 0) {
				MD2 = 0;
				resetPending = 1;
				STATE = ST_WAIT_HOST;
			} else if (portMode != 4 && portMode != 1) {
				MD2 = 0;
				STATE = ST_TRACK_SELECT;
			} else {
				STATE = ST_IDLE;
			}
			break;

		case ST_TRACK_SELECT:
			stateTrackSelect();
			break;

		case ST_TRACK_A:
			if (invertSense) {
				P4_2 = 0;
				P4_5 = 0;
			} else {
				MD4 &= 0xcf;
			}
			delay(0xffff);
			if (option59_7)
				STICKY(offsetStep());
			else
				delay(0xffd8);
			IRCON_7 = 0;
			P2_2 = 0;
			P2_7 = 0;
			trackPasses = 0;
			switch (P1 & 7) {
			case 2:
				if (P1_4) {
					STATE = ST_SLEEP_C;
				} else {
					STATE = ST_SLEEP_A;
					SFR_FB = 0x87;
				}
				break;
			case 3:
				STATE = ST_SLEEP_B;
				break;
			case 6:
			case 7:
				STATE = ST_SLEEP_E;
				break;
			default:
				STATE = ST_IDLE;
				break;
			}
			SFR_F9 = 0xe0;
			break;

		case ST_TRACK_B:
			if (modeDirect) {
				if (invertSense) {
					P4_2 = 0;
					P4_5 = 0;
				} else {
					MD4 &= 0xcf;
				}
			} else {
				if (invertSense) {
					P4_2 = 1;
					P4_5 = 0;
				} else {
					MD4 = (MD4 & 0xcf) | 0x20;
				}
				IRCON_4 = !captureToggle;
			}
			if (option58_0)
				STICKY(offsetMeasure());
			else
				delay(0xffbf);
			P2_4 = 0;
			if (!option69_3) {
				captureToggle = !captureToggle;
				switch (P1 & 7) {
				case 2:
				case 3:
				case 6:
				case 7:
					STATE = ST_TRACK_A;
					if (option59_7) {
						P2_2 = 1;
						IRCON_7 = 1;
					}
					break;
				case 5:
					STATE = ST_TRACK_C;
					IRCON_7 = 0;
					break;
				default:
					STATE = ST_IDLE;
					IRCON_7 = 0;
					P2_7 = 0;
					break;
				}
			} else {
				trackPasses++;
				switch (P1 & 7) {
				case 2:
					if (P1_4) {
						STATE = ST_SLEEP_C;
					} else {
						STATE = ST_SLEEP_A;
						SFR_FB = 0x87;
					}
					IRCON_7 = 0;
					P2_7 = 0;
					break;
				case 3:
					STATE = ST_SLEEP_B;
					IRCON_7 = 0;
					P2_7 = 0;
					break;
				case 5:
					STATE = ST_TRACK_C;
					IRCON_7 = 0;
					trackPasses = 2;
					break;
				case 6:
				case 7:
					STATE = ST_TRACK_A;
					if (option59_7) {
						P2_2 = 1;
						IRCON_7 = 1;
					}
					break;
				default:
					STATE = ST_IDLE;
					IRCON_7 = 0;
					P2_7 = 0;
					break;
				}
			}
			SFR_F9 = 0xe0;
			break;

		case ST_SLEEP_A:
			if (hostEvent) {
				if ((P1 & 7) == 2) {
					STATE = ST_INIT_REGISTERS;
				} else {
					MD2 = 0;
					P0 = 0;
					hostEvent = 0;
					EA = 1;
					STATE = ST_IDLE;
				}
				break;
			}
			SFR_FB = 0x8f;
			P5_7 = 1;
			MD2_PULSE_MODE();
			if ((P1 & 7) == 2)
				P3_5 = !hostEvent;
			P3_5 = 0;
			if ((P1 & 7) == 2) {
				STATE = ST_SLEEP_A;
				break;
			}
			EA = 0;
			MD2 = 0;
			if (option66) {
				SFR_FB = 0x80;
				SFR_FC = 0;
				SFR_FD = 0;
				P6 = 0;
				MD4 = (MD4 & 0xbf) | 0x40;
				MD4 &= 0xbf;
			}
			P0 = 0;
			hostEvent = 0;
			EA = 1;
			STATE = ST_IDLE;
			break;

		case ST_SLEEP_B:
			if (hostEvent) {
				STATE = ST_INIT_REGISTERS;
				break;
			}
			if (optionPairB)
				restoreTriple(P1_3);
			SFR_FB = P1_3 ? 0x90 : 0xe0;
			SFR_FD = 0x7f;
			SFR_FC = 0xff;
			P5_7 = 1;
			MD2_PULSE_MODE();
			if ((P1 & 7) == 3)
				P3_5 = 1;
			P3_5 = 0;
			if ((P1 & 7) == 3) {
				STATE = ST_SLEEP_B;
			} else {
				MD2 = 0;
				STATE = ST_IDLE;
			}
			break;

		case ST_SLEEP_C:
			if (hostEvent) {
				STATE = ST_INIT_REGISTERS;
				break;
			}
			SFR_FB = 0xef;
			SFR_FD = 0x7f;
			SFR_FC = 0x7f;
			MD2_PULSE_MODE();
			PULSE_P35();
			if ((P1 & 7) == 2) {
				STATE = ST_SLEEP_C;
			} else {
				MD2 = 0;
				STATE = ST_IDLE;
			}
			break;

		case ST_SLEEP_D:
			if ((P1 & 7) == 5)
				P3_5 = 1;
			P3_5 = 0;
			if (hostEvent)
				STATE = ST_INIT_REGISTERS;
			else if ((P1 & 7) == 5)
				STATE = ST_SLEEP_D;
			else
				STATE = ST_IDLE;
			break;

		case ST_SLEEP_E:
			if (hostEvent) {
				STATE = ST_INIT_REGISTERS;
				break;
			}
			if (optionPairB)
				restorePair(P1_3);
			SFR_FB = P1_3 ? 0x90 : 0xe0;
			SFR_FD = 0x24;
			SFR_FC = 0xd4;
			P3_4 = 1;
			P3_3 = 1;
			MD2_PULSE_MODE();
			PULSE_P35();
			if ((P1 & 7) == 6) {
				STATE = ST_SLEEP_E;
			} else {
				MD2 = 0;
				STATE = ST_IDLE;
			}
			break;

		case ST_NOTHING:
			break;

		case ST_TRACK_C:
			if (option65_7) {
				XB(0x3c6e) = 0x20;
				P2_3 = 1;
				delay(0xffff);
				captureTimer();
				if (operandA + 0x4b < capture) {
					if (offsetA != 0x80)
						offsetA--;
				} else if (capture < operandA - 0x4b) {
					if (offsetA != 0x7f)
						offsetA++;
				}
				P2_3 = 0;
				P6 = 0xd9;
			}
			P2_7 = 0;
			if ((P1 & 7) == 5) {
				STATE = ST_SLEEP_D;
				SFR_FF = work;
				if (optionPairB)
					restorePair(P1_3);
				SFR_FB = P1_3 ? 0x90 : 0xe0;
				SFR_FD = 0x24;
				SFR_FC = 0xd4;
				P3_4 = 1;
				P3_3 = 1;
				P2_6 = 0;
			} else {
				STATE = ST_IDLE;
				P2_6 = 0;
			}
			SFR_F9 = 0xe0;
			break;

		default:
			break;
		}
	}
}

/* The original's sdcc start-up hook: 1 skips the clearing of the data memory. */
unsigned char __sdcc_external_startup(void)
{
	return 1;
}
