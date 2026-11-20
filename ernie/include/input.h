/*
 * The inputs (app/input.c): the keys the scan latches for task 3, the power key's hold
 * that the RTC's interrupt counts.
 */
#ifndef ERNIE_INPUT_H
#define ERNIE_INPUT_H

#include "ernie.h"
#include "board.h"

struct InputKeys {
	unsigned int	state;		/* the keys held */
	unsigned int	events;		/* the keys the last scan tick latched for task 3 */
};

extern struct InputKeys inputKeys;

/* the slim's 0x0105 (USS-1002): analog input 0x0080, runs of two scan ticks with P7.7 high,
   ANI7 and ANI8 converted eight times on the first, ANI9 on the second; the sums of the runs
   the scan asks for, averaged when the last is in */
/* the input of 0x0104 (DOL-1002's, also the slim's 0x0105: USS-1002) */
#define INPUT_0104		(SOURCE_0104 && (BOARD_DOL1001 || BOARD_USS1001))
#define INPUT_MEASURE		(BOARD_USS1001 && SOURCE_0104)
#if INPUT_MEASURE
struct InputMeasure {
	unsigned int	average[3];	/* ANI7's, ANI8's, ANI9's */
	unsigned int	ticks;		/* the RTC's ticks at the averages */
	unsigned long	sum[3];
	unsigned char	count[3];	/* the runs summed */
	unsigned char	ask5:1;		/* the RTC's: five runs */
	unsigned char	only5:1;	/* only ask5 is heard */
	unsigned char	ask1:1;		/* one run */
	unsigned char	ask3:1;		/* three runs */
	unsigned char	done:1;		/* the last run is in: average */
	unsigned char	runs;		/* left to start */
	unsigned char	step;		/* the run's ticks left: 1 ANI7 and ANI8, 0 ANI9 */
	unsigned char	wait;		/* scan ticks to the next run */
	unsigned char	holdOff;	/* RTC ticks before the RTC asks for runs again */
};

extern struct InputMeasure inputMeasure;
/* the battery's calibration scans on timer 3 (batteryCalibrate) */
extern void inputScanMode1(unsigned int interval);
extern void inputScanMode2(unsigned int interval);
#endif

/* ticks of the scan left before each request */
struct InputDelays {
	unsigned char	flag40;		/* sets bit 0x0040 of event flag 2 */
	unsigned char	flag80;		/* sets bit 0x0080 of event flag 2 */
#if BOARD_MOTION
	unsigned char	motionInfo;	/* starts reading the motion sensor's information */
#endif
	unsigned char	analog80;	/* calls analogStart80 (BOARD_ANALOG_CHANNEL8; otherwise
					   only cleared) */
	unsigned char	latchOn;	/* calls analogLatchOn */
};

extern struct InputDelays inputDelays;

/* a handler of an input bit: called when the bit changes to the levels in edges */
struct InputHandler {
	unsigned char	type;		/* 1: called on the edges below */
	unsigned char	edges;		/* INPUT_EDGE_* */
	void		(*handler)(void);
};

#define INPUT_EDGE_LOW		0x01	/* the bit became 0 */
#define INPUT_EDGE_HIGH		0x02	/* the bit became 1 */

/* the handlers of one bit of a group: count() of them, in a constant list (app/inputbits.c) */
struct InputBit {
	struct InputHandler * const	*handlers;
	unsigned char			(*count)(void);
};

extern struct InputHandler inputHandlerWake2;
#if !FW_SINCE(0x0100060B)
extern struct InputHandler inputHandler1Bit2Low;
extern struct InputHandler inputHandler1Bit2High;
#endif
extern struct InputHandler inputHandlerSerial2Request;
extern struct InputHandler inputHandlerSerial2Link;
extern struct InputHandler inputHandlerSerial1;
extern struct InputHandler inputHandler3Bit2;
extern struct InputHandler inputHandlerMultiCn;
extern struct InputHandler inputHandlerWake1;
extern struct InputHandler inputHandlerCharge;
#if FW_SINCE(0x00090804)
extern struct InputHandler inputHandler1Bit7;
#endif
extern struct InputHandler inputHandler1Bit3;
#if FW_SINCE(0x00090009)
extern struct InputHandler inputHandler1Bit0;
#endif
/* a group of input bits, debounced over two samples */
struct InputGroup {
	unsigned char	enable;		/* the bits dispatched */
	unsigned char	pending;	/* bits dispatched at the next sample even unchanged */
	unsigned char	prev;		/* the previous sample */
	unsigned char	changed;	/* the bits the last sample changed */
	const struct InputBit *bits;
	unsigned char	(*count)(void);	/* how many bits */
};

/* a pin watched for a level held reload ticks of the scan, then handler called */
struct PinConfig {
	void		(*handler)(void);
	unsigned char	reload;
};

struct PinMonitor {
	struct PinConfig config;
	unsigned char	count;		/* the ticks left */
	unsigned char	last;		/* the level at the last tick */
};

struct PinMonitors {
	unsigned char	mask;		/* bit n: pin monitor n runs */
	struct PinMonitor pin[7];	/* P11.5, P4.6, -, -, P3.0, P7.6, P7.4 */
};

extern struct InputGroup inputGroup1;
extern struct PinMonitors pinMonitors;
#if FW_SINCE(0x00090206)
extern unsigned char intp11Level;
#endif
extern __sreg unsigned char inputState1;
#if FW_SINCE(0x01000306)
extern __near unsigned char holdTicks;
extern __near unsigned char holdReached;
#endif

extern void inputProcess(void);
#if SOURCE_0103
extern void inputStateRequest(unsigned char state);
/* inputStateRequest's states; the slim's 0x0105 adds state 1 (inputStateOn), the others one up */
#if INPUT_MEASURE
/* the analog sticks' calibration asked on the serial link (inputCalibrationRequest) */
#define INPUT_CALIBRATION_SET	1	/* data[0]: 0 reset, 1 reset and save, 2 default and
					   save, 3 default */
#define INPUT_CALIBRATION_READ	2	/* answers the 16 bytes stored */
#define INPUT_CALIBRATION_WRITE	3	/* stores 16 bytes */
extern unsigned char inputCalibrationCommand;	/* INPUT_CALIBRATION_*, 0 when none runs */
extern unsigned char inputCalibrationData[0x21];
extern unsigned int inputCalibrationLength;
extern unsigned char inputCalibrationRequest(unsigned char command, unsigned char *data,
					     unsigned int length);
extern void inputCalibrationRun(void);
extern void inputCalibrationAnswer(unsigned char status, unsigned char *data,
				   unsigned char length);
extern void inputCalibrationLoaded(unsigned char command, unsigned char result);
extern void inputCalibrationStored(unsigned char command, unsigned char result);
#define INPUT_STATE(n)		((n) + ((n) != 0))
#define INPUT_STATE_ON		1
extern void inputStateOn(void);
#else
#define INPUT_STATE(n)		(n)
#endif
extern void inputStateRun(void);
extern void inputLogTransition(unsigned char transition);
#endif
#if INPUT_0104
extern void inputTouchRun(void);
#endif
extern unsigned char button2Keys(void);
extern void levelMonitor1Poll(void);
extern void input1Bit7Poll(void);
#if FW_SINCE(0x01000306)
extern unsigned char button2HoldReached(unsigned char ticks);
extern void button2HoldReset(void);
#endif
extern void inputScanHold(unsigned int ticks);
extern void intp11Enable(unsigned char enable);
/* the scan timer: mode 1 runs timer 3, mode 2 timer 0x16, every interval; hold ticks of
   timer 3 first (inputScanHold) */
struct InputScan {
	unsigned char	mode;
	unsigned int	interval;
	unsigned int	hold;
};

#if !FW_SINCE(0x00090804)
extern struct InputScan inputScan;
extern void inputScanMode1(unsigned int interval);
extern void intp1Enable(unsigned char enable);
extern void intp13Enable(unsigned char enable);
extern void intp4Enable(unsigned char enable);
extern void intp3Enable(unsigned char enable);
extern void intp10Enable(unsigned char enable);
extern void intp2Enable(unsigned char enable);
extern void intp7Enable(unsigned char enable);
#endif
extern unsigned char input3Bit0High(void);
#if !FW_SINCE(0x01000104) || BOARD_INPUTS_102603
extern void pinMonitor3Enable(unsigned char enable);
#endif
#if BOARD_INPUTS_102603
extern void pinMonitor2Enable(unsigned char enable);
#endif
extern void pinMonitor0Enable(unsigned char enable);
extern void pinMonitor1Enable(unsigned char enable);
extern void pinMonitor4Enable(unsigned char enable);
extern void pinMonitor5Enable(unsigned char enable);
extern void pinMonitor6Enable(unsigned char enable);
#if INPUT_0104
extern unsigned char inputTouchRequest(unsigned char mask, unsigned char timeout);
#endif

#endif
