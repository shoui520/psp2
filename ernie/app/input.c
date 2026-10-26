/*
 * The inputs: three groups of port bits sampled by task 3 (event flag 2, bit 0x0008) at each
 * tick of the scan timer, debounced and dispatched to the handlers their bits list; seven
 * pins watched for a level held long enough (the pin monitors); the two buttons' presses
 * counted and decoded (with the keys held with them); and the external interrupts' edges.
 */
#pragma sfr
#pragma DI
#pragma EI

#include <stddef.h>

#include "ernie.h"
#include "adc.h"
#include "analog.h"
#include "board.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "input.h"
#include "inputbits.h"
#include "inputreq.h"
#include "kernel.h"
#include "motion.h"
#include "param.h"
#include "serial1.h"
#include "serial2.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "task5.h"
#include "timer.h"
#include "touch.h"

/* a port bit watched by the RTC's interrupt: state follows the level once two ticks agree */
struct LevelMonitor {
	unsigned char	last;
	unsigned char	now;
	unsigned char	state;
	unsigned char	enable;
};

/* the scan timer: mode 1 runs timer 3, mode 2 timer 0x16, every interval; hold ticks of
   timer 3 first (inputScanHold) */
struct InputScan {
	unsigned char	mode;
	unsigned int	interval;
	unsigned int	hold;
};

/* a button's presses (the pin monitor's handler counts them) */
struct ButtonPress {
	unsigned char	count;		/* presses */
	unsigned char	keys;		/* the keys held at the second press */
	unsigned char	result;		/* the keys of the press reported */
	unsigned char	reserved;
	unsigned char	time1;		/* presses for the first report (byte 0x2e of NVS area
					   1, at least 3) */
	unsigned char	time2;		/* presses for the second (byte 0x2f, at least 3) */
	unsigned int	state;		/* the system state the presses were counted in */
};

extern unsigned char getResetCause(void);

void intp1Enable(unsigned char enable);
void intp2Enable(unsigned char enable);
void intp13Enable(unsigned char enable);
void intp11Enable(unsigned char enable);
void intp7Enable(unsigned char enable);
void intp4Enable(unsigned char enable);
void intp3Enable(unsigned char enable);
void intp10Enable(unsigned char enable);
void intp8Enable(unsigned char enable);
void inputKeysOpen(void);
void inputKeysClose(void);
void sampleInputs1(void);
void sampleInputs2(void);
void sampleInputs3(void);
void inputGroupUpdate(unsigned char *raw, unsigned char *state, struct InputGroup *group);
void pinMonitorPoll(void);
unsigned char input1Bit5Active(void);
unsigned char input1Bit0Active(void);
unsigned char input1Bit3Active(void);
void pinMonitor1Set(struct PinConfig *config);
void pinMonitor1Enable(unsigned char enable);
void button2Held(void);
void pinMonitor0Set(struct PinConfig *config);
void pinMonitor0Enable(unsigned char enable);
int button1Reset(void);
int button1State(void);
int button1SetState(unsigned int state);
int button2Reset(void);
unsigned int button2State(void);
int button2SetState(unsigned int state);
unsigned char button2Keys(void);
unsigned char keysBothButtons(unsigned char *keys);
unsigned char keysButtonsOnly(unsigned char *keys);
unsigned char button1Keys(void);
void button1Held(void);
void pinMonitor2Set(struct PinConfig *config);
void pinMonitor2Enable(unsigned char enable);
void pinMonitor2Expired(void);
void pinMonitor3Set(struct PinConfig *config);
void pinMonitor3Enable(unsigned char enable);
void pinMonitor3Expired(void);
void pinMonitor4Set(struct PinConfig *config);
void pinMonitor4Enable(unsigned char enable);
void pinMonitor4Expired(void);
void pinMonitor5Set(struct PinConfig *config);
void pinMonitor5Enable(unsigned char enable);
void pinMonitor5Expired(void);
void pinMonitor6Set(struct PinConfig *config);
void pinMonitor6Enable(unsigned char enable);
void pinMonitor6Expired(void);
void input1Bit2Low(void);
void input1Bit2High(void);
void inputMultiCnChanged(void);
void input1Bit0Changed(void);
void inputScanHold(unsigned int ticks);
void inputScanTimer(void);
void levelMonitor2Poll(void);
void input1Bit3Changed(void);
void levelMonitor1Start(void);
void levelMonitor2Start(void);
void inputShutdownMark(void);
void inputScanMode1(unsigned int interval);
void inputScanMode2(unsigned int interval);
void input1Bit7Changed(void);
void inputNone(void);
unsigned char button2HoldDone(void);

unsigned int inputReserved;
unsigned char scanCount;			/* the scan's ticks */
unsigned char coldStart;			/* the reset cause's high nibble was 0 */
unsigned char inputShutdownMarked;			/* set by inputShutdownMark */
struct InputKeys inputKeys;
struct LevelMonitor levelMonitor1;		/* P11.4, reports 0x61 */
struct LevelMonitor levelMonitor2;		/* P12.0, reports 0x42 */
struct InputGroup inputGroup1;
struct InputGroup inputGroup2;
struct InputGroup inputGroup3;
struct InputHandler inputHandlerWake2;
struct InputHandler inputHandler1Bit2Low;
struct InputHandler inputHandler1Bit2High;
struct InputHandler inputHandlerSerial2Request;
struct InputHandler inputHandlerSerial2Link;
struct InputHandler inputHandlerSerial1;
struct InputHandler inputHandler3Bit2;
struct InputHandler inputHandlerMultiCn;
struct InputHandler inputHandlerWake1;
struct InputHandler inputHandlerCharge;
struct InputHandler inputHandler1Bit7;
struct InputHandler inputHandler1Bit3;
struct InputHandler inputHandler1Bit0;
struct InputDelays inputDelays;
struct InputScan inputScan;
struct PinMonitors pinMonitors;
unsigned char intp11Level;			/* P7.7 at its last edge (INTP11) */

__sreg unsigned char inputRaw1;
__sreg unsigned char inputRaw2;
__sreg unsigned char inputRaw3;
__sreg unsigned char inputState1;
__sreg unsigned char inputState2;
__sreg unsigned char inputState3;

__near struct ButtonPress buttonPress1;		/* pin monitor 0 (P11.5) */
__near struct ButtonPress buttonPress2;		/* pin monitor 1 (P4.6) */
__near unsigned char holdTicks;			/* RTC ticks P4.6 held with P11.5 */
__near unsigned char holdReached;		/* holdTicks reached buttonPress2.time2 + 1 */

void inputInit(void)
{
	struct TimerHandler timer;
	struct PinConfig config;

	inputReserved = 0;
	if ((getResetCause() & 0xf0) == 0)
		coldStart = 1;
	PM3.0 = 1;
	PM3.1 = 0;
	P3.1 = 0;
	PM6.7 = 1;
	intp1Enable(0);
	intp2Enable(0);
	intp13Enable(0);
	intp7Enable(0);
	intp4Enable(0);
	intp3Enable(0);
	intp10Enable(0);
	intp8Enable(0);
	intp11Enable(0);
	intp11Level = 1;
	inputRaw1 = 0xff;
	inputState1 = 0xff;
	inputGroup1.prev = 0xff;
	inputGroup1.changed = 0;
	inputGroup1.pending = 0xa1;
	inputGroup1.enable = 0xff;
	inputGroup1.bits = inputBits1;
	inputGroup1.count = inputBits1Count;
	inputRaw2 = 0;
	inputState2 = 0;
	inputGroup2.prev = 0;
	inputGroup2.changed = 0;
	inputGroup2.pending = 0;
	inputGroup2.enable = 0x0f;
	inputGroup2.bits = inputBits2;
	inputGroup2.count = inputBits2Count;
	inputRaw3 = 0x04;
	inputState3 = 0x04;
	inputGroup3.prev = 0x04;
	inputGroup3.changed = 0;
#if BOARD_INPUT3_PINS
	inputGroup3.pending = 0x24;
	inputGroup3.enable = 0x3d;
#else
	inputGroup3.pending = 0x26;
	inputGroup3.enable = 0x3f;
#endif
	inputGroup3.bits = inputBits3;
	inputGroup3.count = inputBits3Count;
	inputHandlerWake2.type = 1;
	inputHandlerWake2.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerWake2.handler = stateInputWake2;
	inputHandler1Bit2Low.type = 1;
	inputHandler1Bit2Low.edges = INPUT_EDGE_LOW;
	inputHandler1Bit2Low.handler = input1Bit2Low;
	inputHandler1Bit2High.type = 1;
	inputHandler1Bit2High.edges = INPUT_EDGE_HIGH;
	inputHandler1Bit2High.handler = input1Bit2High;
#if BOARD_SERIAL2
	inputHandlerSerial2Request.type = 1;
	inputHandlerSerial2Request.edges = INPUT_EDGE_HIGH;
	inputHandlerSerial2Request.handler = serial2InputRequest;
	inputHandlerSerial2Link.type = 1;
	inputHandlerSerial2Link.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerSerial2Link.handler = serial2InputLink;
#else
	inputHandlerSerial2Request.type = 0;
	inputHandlerSerial2Request.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerSerial2Request.handler = inputNone;
	inputHandlerSerial2Link.type = 0;
	inputHandlerSerial2Link.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerSerial2Link.handler = inputNone;
#endif
	inputHandlerSerial1.type = 1;
	inputHandlerSerial1.edges = INPUT_EDGE_HIGH;
	inputHandlerSerial1.handler = serial1InputHandler;
	inputHandlerMultiCn.type = 1;
	inputHandlerMultiCn.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerMultiCn.handler = inputMultiCnChanged;
	inputHandler3Bit2.type = 1;
	inputHandler3Bit2.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler3Bit2.handler = serialAuthInput;
	inputHandlerWake1.type = 1;
	inputHandlerWake1.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerWake1.handler = stateInputWake1;
	inputHandlerCharge.type = 1;
	inputHandlerCharge.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandlerCharge.handler = chargeUpdateSoon;
	inputHandler1Bit7.type = 1;
	inputHandler1Bit7.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler1Bit7.handler = input1Bit7Changed;
	inputHandler1Bit3.type = 1;
	inputHandler1Bit3.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler1Bit3.handler = input1Bit3Changed;
	inputHandler1Bit0.type = 1;
	inputHandler1Bit0.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler1Bit0.handler = input1Bit0Changed;
	config.reload = 0x40;
	config.handler = button1Held;
	pinMonitor0Set(&config);
	pinMonitor0Enable(0);
	button1Reset();
	config.reload = 0x40;
	config.handler = button2Held;
	pinMonitor1Set(&config);
	pinMonitor1Enable(0);
	button2Reset();
	config.reload = 3;
	config.handler = pinMonitor4Expired;
	pinMonitor4Set(&config);
	pinMonitor4Enable(0);
	config.reload = 3;
	config.handler = pinMonitor3Expired;
	pinMonitor3Set(&config);
	pinMonitor3Enable(0);
	config.reload = 3;
	config.handler = pinMonitor2Expired;
	pinMonitor2Set(&config);
	pinMonitor2Enable(0);
	config.reload = 3;
	config.handler = pinMonitor5Expired;
	pinMonitor5Set(&config);
	pinMonitor5Enable(0);
	config.reload = 0;
	config.handler = pinMonitor6Expired;
	pinMonitor6Set(&config);
	pinMonitor6Enable(0);
#if BOARD_TOUCH_RESET
	inputDelays.flag40 = 0;
	inputDelays.flag80 = 0;
#endif
	inputDelays.motionInfo = 0;
	inputDelays.reserved = 0;
	inputDelays.latchOn = 0;
	inputKeys.state = 0;
	inputKeys.events = 0;
	inputShutdownMarked = 0;
	levelMonitor1.enable = 0;
	levelMonitor2.enable = 0;
	inputScan.mode = 0;
	inputScan.interval = 0;
	inputScan.hold = 0;
	timer.handler = inputScanTimer;
	timerSetHandler(3, &timer);
	timerSetHandler(0x16, &timer);
}

void inputResumePrepare(void)
{
	intp1Enable(0);
	intp13Enable(0);
	intp2Enable(0);
	intp7Enable(0);
	intp4Enable(0);
	intp3Enable(0);
	intp10Enable(0);
	intp8Enable(0);
	inputScanMode1(1);
}

void inputResumeOpen(void)
{
	union PowerRequest mode;

	inputKeysOpen();
	touchOpen();
	mode = *stateGetWakeupFactor();
	if (!mode.bits.notify)
		analogCalibrationLoad();
	analogInit();
	adcOpenForBoard();
	inputGroup1.pending |= 0x08;
	if (elmoVersion0() == 0xff)
		motionOpen();
	motion.status = 0;
	motion.info[0].value = 0;
	motion.info[1].value = 0;
	inputRequestInit();
	return;
}

void inputPowerOff(void)
{
	unsigned long flags;

	flags = stateGetWakeMask();
	if ((flags & 0x00010000) == 0) {
		intp10Enable(1);
		if (!P7.6) {
			pinMonitor5Enable(1);
			pinMonitors.pin[5].last = 0;
			inputScanHold(0x1a);
		}
	}
	touchClose();
	motionClose();
	inputKeysClose();
	analogStop();
	adcCloseForBoard();
	analogLatchInit();
	return;
}

void inputStateOff(void)
{
	intp7Enable(0);
	intp4Enable(0);
	intp3Enable(0);
	intp13Enable(1);
	intp1Enable(1);
#if BOARD_INTP11
	intp11Enable(1);
#endif
	if (coldStart && !P4.6)
		inputScanHold(0x754);
	else
		inputScanMode2(1);
	coldStart = 0;
}

void inputShutdown(void)
{
	touchClose();
	motionClose();
	inputKeysClose();
	analogStop();
	adcCloseForBoard();
}

void inputStateShutdown(void)
{
	levelMonitor1Start();
	inputShutdownMark();
	timerStop(3);
	timerStop(0x16);
}

void inputCut(void)
{
	touchClose();
	motionClose();
	inputKeysClose();
	analogStop();
	adcCloseForBoard();
}

void inputStateCut(void)
{
	levelMonitor2Start();
	levelMonitor1Start();
	inputScanMode2(1);
}

void inputSuspend(void)
{
	unsigned long flags;

	flags = stateGetWakeMask();
	if ((flags & 0x00000004) == 0) {
		intp3Enable(1);
		if (!P3.0) {
			pinMonitor4Enable(1);
			pinMonitors.pin[4].last = 0;
			inputScanHold(0x1a);
		}
	}
	if ((flags & 0x00000008) == 0) {
	}
	if ((flags & 0x00010000) == 0) {
		intp10Enable(1);
		if (!P7.6) {
			pinMonitor5Enable(1);
			pinMonitors.pin[5].last = 0;
			inputScanHold(0x1a);
		}
	}
	touchClose();
	motionClose();
	inputKeysClose();
	analogStop();
	adcCloseForBoard();
	return;
}

void inputStateSuspend(void)
{
	intp1Enable(1);
	intp13Enable(1);
	inputScanMode2(1);
}

void inputReset(void)
{
	touchDisable0();
	touchDisable1();
	motionStop();
	analogStop();
	analogLatchOff();
}

/* P5.0-3, P8 and P11.2-3 inputs with their pull-ups (the keys button1Keys reads) */
void inputKeysOpen(void)
{
	PU5 = BOARD_KEYS_P5 | PU5;
	PM5 = 0x0f | PM5;
	PU8 = 0xff;
	PM8 = 0xff;
	PU11 = 0x0c | PU11;
	PM11 = 0x0c | PM11;
}

/* and back to outputs at 0 */
void inputKeysClose(void)
{
	P5 = (unsigned char)~BOARD_KEYS_P5 & P5;
	PM5 = (unsigned char)~BOARD_KEYS_P5 & PM5;
	P8 = 0;
	PM8 = 0;
	P11 = 0xf3 & P11;
	PM11 = 0xf3 & PM11;
}

/* task 3's mask 0x0008 (the scan's tick): the inputs sampled and dispatched */
void inputProcess(void)
{
	unsigned int keys;
	unsigned int events;

	events = inputKeys.events;
	switch (stateGet() & 0x7fff) {
	case 0x0020:
		if (events & 0x0400)
			motionInfoPoll();
		if (touch.rate == 0 && (events & 0x000c))
			touchStartFrame();
	case 0x0004:
	case 0x0008:
		sampleInputs1();
		sampleInputs2();
		sampleInputs3();
		pinMonitorPoll();
		keys = events & 0x00e3;
		if (keys != 0)
			analogStart(keys);
		break;
	case 0x0800:
		levelMonitor2Poll();
		break;
	case 0x0010:
		break;
	}
}

void sampleInputs1(void)
{
	inputRaw1.3 = P14.2;
	inputRaw1.4 = P4.6;
	inputRaw1.5 = P2.0;
	inputRaw1.6 = P11.5;
#if BOARD_INPUT1_BIT7_POLL
	inputRaw1.7 = P14.5;
#endif
	if (stateGet() == 0x0020)
		inputRaw1.0 = P11.0;
	inputGroupUpdate(&inputRaw1, &inputState1, &inputGroup1);
	if (inputGroup1.changed) {
	}
	sleepFactorSet4(inputState1.4 ? 0 : 1);
	sleepFactorSet7(inputState1.6 ? 0 : 1);
}

void sampleInputs2(void)
{
	inputRaw2.2 = P3.2;
	inputRaw2.1 = P3.4;
	inputRaw2.0 = P12.6;
	inputRaw2.3 = P3.6;
	inputGroupUpdate(&inputRaw2, &inputState2, &inputGroup2);
	if (inputGroup2.changed) {
	}
}

void sampleInputs3(void)
{
#if BOARD_INPUT3_PINS
	inputRaw3.0 = P7.3;
	inputRaw3.1 = P7.7;
#endif
	inputGroupUpdate(&inputRaw3, &inputState3, &inputGroup3);
	if (inputGroup3.changed) {
	}
}

/* a sample debounced (a mask changes when two samples agree) and the changed and pending bits
   dispatched, lowest first */
void inputGroupUpdate(unsigned char *raw, unsigned char *state, struct InputGroup *group)
{
	unsigned char stable;
	unsigned char handled;
	unsigned char mask;
	unsigned char i;
	unsigned char j;
	unsigned char bitCount;
	unsigned char handlerCount;
	struct InputHandler *handler;

	stable = ~(group->prev ^ *raw);
	group->changed = (*state ^ *raw) & stable;
	*state ^= group->changed;
	group->prev = *raw;
	group->changed |= group->pending & stable;
	group->changed &= group->enable;
	if (group->changed != 0) {
		mask = 0x01;
		handled = 0;
		bitCount = group->count();
		for (i = 0; i < bitCount; i++) {
			if (group->changed & mask) {
				group->pending &= ~mask;
				if (group->bits[i].handlers != NULL) {
					handler = group->bits[i].handlers[0];
					handlerCount = group->bits[i].count();
					for (j = 0; j < handlerCount; ) {
						if (handler->type == 1) {
							if (((handler->edges & INPUT_EDGE_LOW) && !(*state & mask)) ||
							    ((handler->edges & INPUT_EDGE_HIGH) && (*state & mask)))
								handler->handler();
						}
						j++;
						handler = group->bits[i].handlers[j];
					}
				}
				handled |= mask;
			}
			if (group->changed == handled)
				break;
			mask <<= 1;
		}
	}
}

/* each running pin monitor: its handler called once the level held its reload ticks, the
   monitor stopped when the level changes */
void pinMonitorPoll(void)
{
	unsigned char level;

	if (pinMonitors.mask != 0) {
		if (pinMonitors.mask & 0x01) {
			level = P11.5;
			if (pinMonitors.pin[0].last == level) {
				if (pinMonitors.pin[0].count-- == 0) {
					pinMonitor0Enable(0);
					pinMonitors.pin[0].config.handler();
				}
			} else {
				pinMonitor0Enable(0);
			}
			pinMonitors.pin[0].last = level;
		}
		if (pinMonitors.mask & 0x02) {
			level = P4.6;
			if (pinMonitors.pin[1].last == level) {
				if (pinMonitors.pin[1].count-- == 0) {
					pinMonitor1Enable(0);
					pinMonitors.pin[1].config.handler();
				}
			} else {
				pinMonitor1Enable(0);
			}
			pinMonitors.pin[1].last = level;
		}
		if (pinMonitors.mask & 0x04) {
		}
		if (pinMonitors.mask & 0x08) {
		}
		if (pinMonitors.mask & 0x10) {
			level = P3.0;
			if (pinMonitors.pin[4].last == level) {
				if (pinMonitors.pin[4].count-- == 0) {
					pinMonitor4Enable(0);
					pinMonitors.pin[4].config.handler();
				}
			} else {
				pinMonitor4Enable(0);
			}
			pinMonitors.pin[4].last = level;
		}
		if (pinMonitors.mask & 0x20) {
			level = P7.6;
			if (pinMonitors.pin[5].last == level) {
				if (pinMonitors.pin[5].count-- == 0) {
					pinMonitor5Enable(0);
					pinMonitors.pin[5].config.handler();
				}
			} else {
				pinMonitor5Enable(0);
			}
			pinMonitors.pin[5].last = level;
		}
		if (pinMonitors.mask & 0x40) {
			level = P7.4;
			if (pinMonitors.pin[6].last == level) {
				if (pinMonitors.pin[6].count-- == 0) {
					pinMonitor6Enable(0);
					pinMonitors.pin[6].config.handler();
				}
			} else {
				pinMonitor6Enable(0);
			}
			pinMonitors.pin[6].last = level;
		}
	}
}

unsigned char input1Bit4Active(void)
{
	return !inputState1.4;
}

unsigned char input1Bit6Active(void)
{
	return !inputState1.6;
}

unsigned char input1Bit1Active(void)
{
	return !inputState1.1;
}

unsigned char input1Bit2Active(void)
{
	return !inputState1.2;
}

unsigned char input1Bit5Active(void)
{
	return !inputState1.5;
}

unsigned char input1Bit0Active(void)
{
	return !inputState1.0;
}

unsigned char input1Bit3Active(void)
{
	return !inputState1.3;
}

unsigned char input3Bit0High(void)
{
	return inputState3.0 != 0;
}

unsigned char input3Bit2Active(void)
{
	return !inputState3.2;
}

unsigned char input3Bit5High(void)
{
	return inputState3.5 != 0;
}

unsigned char intp11Low(void)
{
#if BOARD_INTP11
	return intp11Level == 0;
#else
	return inputState3.1 == 0;
#endif
}

int input2Bit0Get(void)
{
	return (unsigned char)inputState2.0;
}

int input2Bit1Get(void)
{
	return (unsigned char)inputState2.1;
}

void inputMotionInfoLater(void)
{
	inputDelays.motionInfo = 0x10;
}

#if BOARD_TOUCH_RESET
void inputFlag40Later(void)
{
	inputDelays.flag40 = 9;
}

void inputFlag80Later(void)
{
	inputDelays.flag80 = 9;
}
#endif

/* the external interrupts: on (an edge chosen per pin) or off */
void intp2Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP0.2 = 0;
		EGN0.2 = 0;
		PMK2 = 1;
		PIF2 = 0;
	}
}

void intp1Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP0.1 = 0;
		EGN0.1 = 0;
		PMK1 = 1;
		PIF1 = 0;
	} else {
		EGP0.1 = 0;
		EGN0.1 = 1;
		PIF1 = 0;
		PMK1 = 0;
	}
}

void pinMonitor1Set(struct PinConfig *config)
{
	pinMonitors.pin[1].config = *config;
	pinMonitors.pin[1].count = 0;
}

void pinMonitor1Enable(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x02;
	} else {
		pinMonitors.pin[1].last = P4.6;
		pinMonitors.pin[1].count = pinMonitors.pin[1].config.reload;
		pinMonitors.mask |= 0x02;
	}
}

/* pin monitor 1's handler (P4.6 held): its presses counted with the key read at the second */
void button2Held(void)
{
	unsigned int state;
	unsigned char value;
	union PowerRequest mode;

	buttonPress2.count++;
	state = stateGet();
	if (buttonPress2.count < 2) {
		pinMonitor1Enable(1);
	} else if (buttonPress2.count == 2) {
		inputKeysOpen();
		dly_tsk(3);
		buttonPress2.keys = button2Keys();
		pinMonitor1Enable(1);
		paramRead(0x2e, 1, &value);
		buttonPress2.time1 = value >= 4 ? value : 3;
		paramRead(0x2f, 1, &value);
		buttonPress2.time2 = value >= 4 ? value : 3;
	} else if (buttonPress2.keys == button2Keys()) {
		switch (buttonPress2.keys) {
		case 1:
		case 2:
			mode = *stateGetWakeupFactor();
			if (buttonPress2.count == buttonPress2.time1) {
				if (state != button2State() && !mode.bits.notify &&
				    !configGet()->pressAnyState) {
					button2SetState(0);
					return;
				}
				buttonPress2.result = buttonPress2.keys;
				stateTransition(0x14);
				pinMonitor1Enable(1);
			} else if (buttonPress2.count == buttonPress2.time2 || button2HoldDone()) {
				if (state != button2State() && state != 0x0008 && !mode.bits.notify &&
				    !configGet()->pressAnyState) {
					button2SetState(0);
					return;
				}
				buttonPress2.result = 2;
				stateTransition(0x31);
			} else {
				pinMonitor1Enable(1);
			}
			break;
		}
	} else if (buttonPress2.count >= buttonPress2.time1) {
		if (state != button2State() && !configGet()->pressAnyState) {
			button2SetState(0);
			return;
		}
		buttonPress1.keys = 1;
		buttonPress2.result = buttonPress2.keys;
		stateTransition(0x14);
	}
}

void intp13Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP13 = 0;
		EGN13 = 0;
		MK0H.7 = 1;
		IF0H.7 = 0;
	} else {
		EGP13 = 0;
		EGN13 = 1;
		IF0H.7 = 0;
		MK0H.7 = 0;
	}
}

void intp11Enable(unsigned char enable)
{
#if BOARD_INTP11
	if (enable == 0) {
		EGP1.3 = 0;
		EGN1.3 = 0;
		PMK11 = 1;
		PIF11 = 0;
	} else {
		EGP1.3 = 1;
		EGN1.3 = 1;
		PIF11 = 0;
		PMK11 = 0;
	}
#endif
}

void pinMonitor0Set(struct PinConfig *config)
{
	pinMonitors.pin[0].config = *config;
	pinMonitors.pin[0].count = 0;
}

void pinMonitor0Enable(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x01;
	} else {
		pinMonitors.pin[0].last = P11.5;
		pinMonitors.pin[0].count = pinMonitors.pin[0].config.reload;
		pinMonitors.mask |= 0x01;
	}
}

int button1Reset(void)
{
	if (pinMonitors.mask & 0x01)
		return 1;
	buttonPress1.count = 0;
	buttonPress1.keys = 0;
	buttonPress1.result = 0;
	button1SetState(stateGet());
	return 0;
}

int button1Result(void)
{
	return buttonPress1.result;
}

int button1State(void)
{
	return buttonPress1.state;
}

int button1SetState(unsigned int state)
{
	buttonPress1.state = state;
	return 0;
}

int button2Reset(void)
{
	if (pinMonitors.mask & 0x02)
		return 1;
	buttonPress2.count = 0;
	buttonPress2.keys = 0;
	buttonPress2.result = 0;
	button2SetState(stateGet());
	return 0;
}

int button2Result(void)
{
	return buttonPress2.result;
}

unsigned int button2State(void)
{
	return buttonPress2.state;
}

int button2SetState(unsigned int state)
{
	buttonPress2.state = state;
	return 0;
}

unsigned char button2Keys(void)
{
	unsigned char keys;

	keys = 0;
	if (P11.5)
		keys++;
	return keys;
}

unsigned char keysComboA9(unsigned char *keys)
{
	return keys[0] == 0xff && keys[1] == 0xa9;
}

unsigned char keysBothButtons(unsigned char *keys)
{
	return (~keys[1] & 0x10) && (~keys[1] & 0x40);
}

unsigned char keysButtonsOnly(unsigned char *keys)
{
	return keys[0] == 0xff && keys[1] == 0xaf;
}

/* the keys held: P8, P5.0-3 and the two buttons */
unsigned char button1Keys(void)
{
	unsigned char result;
	unsigned char keys[2];

	keys[0] = P8;
	keys[1] = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
				 (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	result = 0;
	if (keysBothButtons(keys) && stateGet() == 0x0004)
		result = 3;
	else if (keysButtonsOnly(keys))
		result = 2;
	else
		result = 1;
	return result;
}

/* pin monitor 0's handler (P11.5 held) */
void button1Held(void)
{
	unsigned char value;

	buttonPress1.count++;
	if (stateGet() != button1State() && !configGet()->pressAnyState) {
		button1SetState(0);
	} else if (buttonPress1.count < 2) {
		pinMonitor0Enable(1);
	} else if (buttonPress1.count == 2) {
		inputKeysOpen();
		dly_tsk(3);
		buttonPress1.keys = button1Keys();
		pinMonitor0Enable(1);
		paramRead(0x2e, 1, &value);
		buttonPress1.time1 = value >= 4 ? value : 3;
		paramRead(0x2f, 1, &value);
		buttonPress1.time2 = value >= 4 ? value : 3;
	} else if (buttonPress1.keys == button1Keys()) {
		switch (buttonPress1.keys) {
		case 3:
			if (buttonPress1.count == buttonPress1.time1) {
				buttonPress1.result = buttonPress1.keys;
				stateTransition(0x0b);
			} else {
				pinMonitor0Enable(1);
			}
			break;
		}
	}
}

void intp7Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP0.7 = 0;
		EGN0.7 = 0;
		PMK7 = 1;
		PIF7 = 0;
	}
}

void pinMonitor2Set(struct PinConfig *config)
{
	pinMonitors.pin[2].config = *config;
	pinMonitors.pin[2].count = 0;
}

void pinMonitor2Enable(unsigned char enable)
{
	if (enable == 0)
		pinMonitors.mask &= ~0x04;
}

void pinMonitor2Expired(void)
{
}

void intp4Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP0.4 = 0;
		EGN0.4 = 0;
		PMK4 = 1;
		PIF4 = 0;
	}
}

void pinMonitor3Set(struct PinConfig *config)
{
	pinMonitors.pin[3].config = *config;
	pinMonitors.pin[3].count = 0;
}

void pinMonitor3Enable(unsigned char enable)
{
	if (enable == 0)
		pinMonitors.mask &= ~0x08;
}

void pinMonitor3Expired(void)
{
	stateTransition(5);
}

void intp3Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP0.3 = 0;
		EGN0.3 = 0;
		PMK3 = 1;
		PIF3 = 0;
	} else {
		EGP0.3 = 0;
		EGN0.3 = 1;
		PIF3 = 0;
		PMK3 = 0;
	}
}

void pinMonitor4Set(struct PinConfig *config)
{
	pinMonitors.pin[4].config = *config;
	pinMonitors.pin[4].count = 0;
}

void pinMonitor4Enable(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x10;
	} else {
		pinMonitors.pin[4].last = P3.0;
		pinMonitors.pin[4].count = pinMonitors.pin[4].config.reload;
		pinMonitors.mask |= 0x10;
	}
}

void pinMonitor4Expired(void)
{
	stateTransition(2);
}

void intp10Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP1.2 = 0;
		EGN1.2 = 0;
		PMK10 = 1;
		PIF10 = 0;
	} else {
		EGP1.2 = 0;
		EGN1.2 = 1;
		PIF10 = 0;
		PMK10 = 0;
	}
}

void pinMonitor5Set(struct PinConfig *config)
{
	pinMonitors.pin[5].config = *config;
	pinMonitors.pin[5].count = 0;
}

void pinMonitor5Enable(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x20;
	} else {
		pinMonitors.pin[5].last = P7.6;
		pinMonitors.pin[5].count = pinMonitors.pin[5].config.reload;
		pinMonitors.mask |= 0x20;
	}
}

void pinMonitor5Expired(void)
{
	stateTransition(0x10);
	return;
}

void intp8Enable(unsigned char enable)
{
	if (enable == 0) {
		EGP1.0 = 0;
		EGN1.0 = 0;
		PMK8 = 1;
		PIF8 = 0;
	} else {
		EGP1.0 = 1;
		EGN1.0 = 0;
		PIF8 = 0;
		PMK8 = 0;
	}
}

void pinMonitor6Set(struct PinConfig *config)
{
	pinMonitors.pin[6].config = *config;
	pinMonitors.pin[6].count = 0;
}

void pinMonitor6Enable(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x40;
	} else {
		pinMonitors.pin[6].last = P7.4;
		pinMonitors.pin[6].count = pinMonitors.pin[6].config.reload;
		pinMonitors.mask |= 0x40;
	}
}

void pinMonitor6Expired(void)
{
	task6SetFlag(6, 0x0200);
}

void input1Bit2Low(void)
{
}

void input1Bit2High(void)
{
}

void inputMultiCnChanged(void)
{
	if (input1Bit5Active() == 1) {
		analogMultiCnStart();
	} else {
		analogMultiCnStop();
		powerDeviceSetPending(0x20);
		sleepFactorSet5(0);
		analogMultiCnInit();
	}
	set_flg(2, 0x0100);
}

void input1Bit0Changed(void)
{
	if (input1Bit0Active() == 1) {
		sleepFactorSet15(1);
	} else {
		powerDeviceSetPending(2);
		sleepFactorSet15(0);
	}
	set_flg(2, 0x0100);
}

/* timer 3 run ticks times first (the inputs held at a power-on) */
void inputScanHold(unsigned int ticks)
{
	DI();
	if (inputScan.hold < ticks) {
		timerStop(0x16);
		inputScan.hold = ticks;
		timerStart(3, 1);
	}
	EI();
}

/* the scan timer's handler (timers 3 and 0x16): the key bits latched, task 3 woken and the
   timer restarted, the delays counted down */
void inputScanTimer(void)
{
	unsigned char toggled;

	inputKeys.events = 0;
	toggled = scanCount++ ^ scanCount;
	inputKeys.events |= inputKeys.state & 0x0402;
	if (toggled & 0x02)
		inputKeys.events |= inputKeys.state & 0x000c;
	if (toggled & 0x04)
		inputKeys.events |= inputKeys.state & 0x00a1;
	task3SetFlag(2, 0x0008);
	if (inputScan.hold != 0) {
		inputScan.hold--;
		timerStart(3, 1);
	} else if (inputScan.mode == 2) {
		timerStart(0x16, inputScan.interval);
	} else {
		timerStart(3, inputScan.interval);
	}
	if (inputDelays.motionInfo != 0) {
		if (--inputDelays.motionInfo == 0)
			motionInfoStart();
	}
#if BOARD_TOUCH_RESET
	if (inputDelays.flag40 != 0) {
		if (--inputDelays.flag40 == 0)
			task3SetFlag(2, 0x0040);
	}
	if (inputDelays.flag80 != 0) {
		if (--inputDelays.flag80 == 0)
			task3SetFlag(2, 0x0080);
	}
#endif
	if (inputDelays.latchOn != 0) {
		if (--inputDelays.latchOn == 0)
			analogLatchOn();
	}
}

/* the RTC's interrupt (state 0x0010 or 0x0800): P11.4 */
void levelMonitor1Poll(void)
{
	if (levelMonitor1.enable != 0) {
		levelMonitor1.now = P11.4;
		if (levelMonitor1.state != levelMonitor1.now && levelMonitor1.last == levelMonitor1.now) {
			levelMonitor1.state = levelMonitor1.now;
			if (levelMonitor1.state != 0)
				stateTransition(0x61);
		}
		levelMonitor1.last = levelMonitor1.now;
	}
}

/* task 3 in state 0x0800: P12.0 */
void levelMonitor2Poll(void)
{
	if (levelMonitor2.enable != 0) {
		levelMonitor2.now = P12.0;
		if (levelMonitor2.state != levelMonitor2.now && levelMonitor2.last == levelMonitor2.now) {
			levelMonitor2.state = levelMonitor2.now;
			if (levelMonitor2.state != 0)
				stateTransition(0x42);
		}
		levelMonitor2.last = levelMonitor2.now;
	}
}

/* the RTC's interrupt (state 0x0010), after a shutdown is marked: P14.5 as input 1's bit 7 */
void input1Bit7Poll(void)
{
#if BOARD_INPUT1_BIT7_POLL
	if (inputShutdownMarked != 0) {
		inputRaw1.7 = P14.5;
		if (inputState1.7 != inputRaw1.7 && (inputGroup1.prev & 0x80) == (inputRaw1 & 0x80)) {
			inputState1.7 = inputRaw1.7;
			if (!inputState1.7)
				stateTransition(0x62);
		}
		if (inputRaw1.7)
			inputGroup1.prev |= 0x80;
		else
			inputGroup1.prev &= ~0x80;
	}
#endif
}

unsigned char levelMonitor2Low(void)
{
	return levelMonitor2.state == 0;
}

unsigned char levelMonitor1High(void)
{
	return levelMonitor1.state == 1;
}

void input1Bit3Changed(void)
{
	if (input1Bit3Active() == 1) {
		if (stateGet() == 0x0020)
			inputDelays.latchOn = 0x40;
	} else {
		powerDeviceSetPending(8);
		analogLatchInit();
	}
	set_flg(2, 0x0100);
}

void inputNone(void)
{
}

unsigned char input1Bit7Active(void)
{
	return !inputState1.7;
}

void levelMonitor1Start(void)
{
	levelMonitor1.last = 0;
	levelMonitor1.now = 0;
	levelMonitor1.state = 0;
	levelMonitor1.enable = 1;
}

void levelMonitor2Start(void)
{
	levelMonitor2.last = 0;
	levelMonitor2.now = 0;
	levelMonitor2.state = 0;
	levelMonitor2.enable = 1;
}

void inputShutdownMark(void)
{
	inputShutdownMarked = 1;
}

/* the scan on timer 3 every interval */
void inputScanMode1(unsigned int interval)
{
	DI();
	if (inputScan.mode != 1) {
		timerStop(0x16);
		inputScan.mode = 1;
		inputScan.interval = interval;
		inputScan.hold = 0;
		timerStart(3, interval);
	}
	EI();
	return;
}

/* the scan on timer 0x16 every interval */
void inputScanMode2(unsigned int interval)
{
	DI();
	if (inputScan.mode != 2) {
		timerStop(3);
		inputScan.mode = 2;
		inputScan.interval = interval;
		timerStart(0x16, interval);
	}
	EI();
	return;
}

void input1Bit7Changed(void)
{
	set_flg(2, 0x0100);
	return;
}

void inputChanged(void)
{
	set_flg(2, 0x0100);
	return;
}

unsigned char button2HoldReached(unsigned char ticks)
{
	return ticks == buttonPress2.time2 + 1;
}

unsigned char button2HoldDone(void)
{
	return holdReached == 1;
}

void button2HoldReset(void)
{
	unsigned char value;

	holdTicks = 0;
	holdReached = 0;
	paramRead(0x2f, 1, &value);
	buttonPress2.time2 = value >= 4 ? value : 3;
}
