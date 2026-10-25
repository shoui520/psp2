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
#include "chip.h"
#include "config.h"
#include "input.h"
#include "inputbits.h"
#include "inputreq.h"
#include "kernel.h"
#include "motion.h"
#include "sleepfactor.h"
#include "timer.h"
#include "touch.h"

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

/* PLACEHOLDER: the word sub_2AB39 returns (its owner not written yet): a 7-bit mode
   (sub_2457F compares it with 0x0f) and a flag that keeps a button's presses counted across
   a state change */
struct SystemMode {
	unsigned int	mode:7;
	unsigned int	flag:1;
	unsigned int	reserved:8;
};

extern unsigned char getResetCause(void);
extern unsigned int sub_2AB46(void);
extern unsigned char sub_2AB49(int);
extern struct SystemMode *sub_2AB39(void);
extern unsigned long sub_2AA79(void);
extern unsigned char sub_2C15B(unsigned int offset, unsigned int size, unsigned char *buf);
extern void sub_2C342(ID flgid, FLGPTN setptn);
extern void sub_2C394(int, int);
extern void sub_17159(void);
extern void sub_1C265(void);
extern void sub_1C042(void);
extern void sub_1BFAB(void);
extern unsigned char sub_21F2A(void);
extern void sub_17204(void);
extern void sub_1CFC6(void);
extern void sub_1BFE9(void);
extern void sub_1D0A8(void);
extern void sub_17544(void);
extern void sub_17580(void);
extern void sub_1D0CC(void);
extern void sub_172BC(void);
extern void sub_1C3B9(unsigned int keys);
extern void sub_1CFFC(void);
extern void sub_1D012(void);
extern void sub_21F33(int);
extern void sub_1C3A1(void);
extern void sub_1D0CB(void);
extern void sub_2B475(void);
extern void sub_25263(void);
extern void sub_2526B(void);
extern void sub_1D8ED(void);
extern void sub_224FD(void);
extern void sub_2B4BC(void);
extern void sub_29B68(void);

void sub_11D97(unsigned char enable);
void sub_11D83(unsigned char enable);
void sub_11F84(unsigned char enable);
void sub_11FA6(unsigned char enable);
void sub_1224B(unsigned char enable);
void sub_12296(unsigned char enable);
void sub_122E7(unsigned char enable);
void sub_12360(unsigned char enable);
void sub_123DB(unsigned char enable);
void sub_1179E(void);
void sub_117C4(void);
void sampleInputs1(void);
void sampleInputs2(void);
void sampleInputs3(void);
void inputGroupUpdate(unsigned char *raw, unsigned char *state, struct InputGroup *group);
void pinMonitorPoll(void);
unsigned char sub_11D1B(void);
unsigned char sub_11D23(void);
unsigned char sub_11D2B(void);
void sub_11DB9(struct PinConfig *config);
void sub_11DE0(unsigned char enable);
void sub_11E0A(void);
void sub_11FC8(struct PinConfig *config);
void sub_11FEF(unsigned char enable);
int sub_12019(void);
int sub_12040(void);
int sub_12044(unsigned int state);
int sub_1204D(void);
unsigned int sub_12074(void);
int sub_12078(unsigned int state);
unsigned char sub_12081(void);
unsigned char sub_120B8(unsigned char *keys);
unsigned char sub_120E7(unsigned char *keys);
unsigned char sub_12111(void);
void sub_1217E(void);
void sub_1225F(struct PinConfig *config);
void sub_12286(unsigned char enable);
void sub_12295(void);
void sub_122AA(struct PinConfig *config);
void sub_122D1(unsigned char enable);
void sub_122E0(void);
void sub_12309(struct PinConfig *config);
void sub_12330(unsigned char enable);
void sub_1235A(void);
void sub_12382(struct PinConfig *config);
void sub_123A9(unsigned char enable);
void sub_123D3(void);
void sub_123FD(struct PinConfig *config);
void sub_12424(unsigned char enable);
void sub_1244E(void);
void sub_1245B(void);
void sub_1245C(void);
void sub_1245D(void);
void sub_12489(void);
void inputScanHold(unsigned int ticks);
void inputScanTimer(void);
void sub_12638(void);
void sub_12698(void);
void sub_126D1(void);
void sub_126E4(void);
void sub_126F7(void);
void inputScanMode1(unsigned int interval);
void inputScanMode2(unsigned int interval);
void sub_1275F(void);
unsigned char sub_12784(void);

unsigned int inputReserved;
unsigned char scanCount;			/* the scan's ticks */
unsigned char coldStart;			/* the reset cause's high nibble was 0 */
unsigned char inputFlag126F7;			/* set by sub_126F7 */
struct InputKeys inputKeys;
struct LevelMonitor levelMonitor1;		/* P11.4, reports 0x61 */
struct LevelMonitor levelMonitor2;		/* P12.0, reports 0x42 */
struct InputGroup inputGroup1;
struct InputGroup inputGroup2;
struct InputGroup inputGroup3;
struct InputHandler inputHandler2B475;
struct InputHandler inputHandler1245B;
struct InputHandler inputHandler1245C;
struct InputHandler inputHandler25263;
struct InputHandler inputHandler2526B;
struct InputHandler inputHandler1D8ED;
struct InputHandler inputHandler224FD;
struct InputHandler inputHandler1245D;
struct InputHandler inputHandler2B4BC;
struct InputHandler inputHandler29B68;
struct InputHandler inputHandler1275F;
struct InputHandler inputHandler12698;
struct InputHandler inputHandler12489;
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
	sub_11D97(0);
	sub_11D83(0);
	sub_11F84(0);
	sub_1224B(0);
	sub_12296(0);
	sub_122E7(0);
	sub_12360(0);
	sub_123DB(0);
	sub_11FA6(0);
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
	inputGroup3.pending = 0x24;
	inputGroup3.enable = 0x3d;
	inputGroup3.bits = inputBits3;
	inputGroup3.count = inputBits3Count;
	inputHandler2B475.type = 1;
	inputHandler2B475.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler2B475.handler = sub_2B475;
	inputHandler1245B.type = 1;
	inputHandler1245B.edges = INPUT_EDGE_LOW;
	inputHandler1245B.handler = sub_1245B;
	inputHandler1245C.type = 1;
	inputHandler1245C.edges = INPUT_EDGE_HIGH;
	inputHandler1245C.handler = sub_1245C;
	inputHandler25263.type = 1;
	inputHandler25263.edges = INPUT_EDGE_HIGH;
	inputHandler25263.handler = sub_25263;
	inputHandler2526B.type = 1;
	inputHandler2526B.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler2526B.handler = sub_2526B;
	inputHandler1D8ED.type = 1;
	inputHandler1D8ED.edges = INPUT_EDGE_HIGH;
	inputHandler1D8ED.handler = sub_1D8ED;
	inputHandler1245D.type = 1;
	inputHandler1245D.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler1245D.handler = sub_1245D;
	inputHandler224FD.type = 1;
	inputHandler224FD.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler224FD.handler = sub_224FD;
	inputHandler2B4BC.type = 1;
	inputHandler2B4BC.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler2B4BC.handler = sub_2B4BC;
	inputHandler29B68.type = 1;
	inputHandler29B68.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler29B68.handler = sub_29B68;
	inputHandler1275F.type = 1;
	inputHandler1275F.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler1275F.handler = sub_1275F;
	inputHandler12698.type = 1;
	inputHandler12698.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler12698.handler = sub_12698;
	inputHandler12489.type = 1;
	inputHandler12489.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	inputHandler12489.handler = sub_12489;
	config.reload = 0x40;
	config.handler = sub_1217E;
	sub_11FC8(&config);
	sub_11FEF(0);
	sub_12019();
	config.reload = 0x40;
	config.handler = sub_11E0A;
	sub_11DB9(&config);
	sub_11DE0(0);
	sub_1204D();
	config.reload = 3;
	config.handler = sub_1235A;
	sub_12309(&config);
	sub_12330(0);
	config.reload = 3;
	config.handler = sub_122E0;
	sub_122AA(&config);
	sub_122D1(0);
	config.reload = 3;
	config.handler = sub_12295;
	sub_1225F(&config);
	sub_12286(0);
	config.reload = 3;
	config.handler = sub_123D3;
	sub_12382(&config);
	sub_123A9(0);
	config.reload = 0;
	config.handler = sub_1244E;
	sub_123FD(&config);
	sub_12424(0);
	inputDelays.flag40 = 0;
	inputDelays.flag80 = 0;
	inputDelays.motionInfo = 0;
	inputDelays.reserved = 0;
	inputDelays.call1D0CB = 0;
	inputKeys.state = 0;
	inputKeys.events = 0;
	inputFlag126F7 = 0;
	levelMonitor1.enable = 0;
	levelMonitor2.enable = 0;
	inputScan.mode = 0;
	inputScan.interval = 0;
	inputScan.hold = 0;
	timer.handler = inputScanTimer;
	sub_240CF(3, &timer);
	sub_240CF(0x16, &timer);
}

void sub_11572(void)
{
	sub_11D97(0);
	sub_11F84(0);
	sub_11D83(0);
	sub_1224B(0);
	sub_12296(0);
	sub_122E7(0);
	sub_12360(0);
	sub_123DB(0);
	inputScanMode1(1);
}

void sub_1159F(void)
{
	struct SystemMode mode;

	sub_1179E();
	sub_17159();
	mode = *sub_2AB39();
	if (!mode.flag)
		sub_1C265();
	sub_1C042();
	sub_1BFAB();
	inputGroup1.pending |= 0x08;
	if (sub_21F2A() == 0xff)
		motionOpen();
	motion.status = 0;
	motion.info[0].value = 0;
	motion.info[1].value = 0;
	inputRequestInit();
	return;
}

void sub_115F0(void)
{
	unsigned long flags;

	flags = sub_2AA79();
	if ((flags & 0x00010000) == 0) {
		sub_12360(1);
		if (!P7.6) {
			sub_123A9(1);
			pinMonitors.pin[5].last = 0;
			inputScanHold(0x1a);
		}
	}
	sub_17204();
	motionClose();
	sub_117C4();
	sub_1CFC6();
	sub_1BFE9();
	sub_1D0A8();
	return;
}

void sub_1164E(void)
{
	sub_1224B(0);
	sub_12296(0);
	sub_122E7(0);
	sub_11F84(1);
	sub_11D97(1);
	sub_11FA6(1);
	if (coldStart && !P4.6)
		inputScanHold(0x754);
	else
		inputScanMode2(1);
	coldStart = 0;
}

void sub_1168D(void)
{
	sub_17204();
	motionClose();
	sub_117C4();
	sub_1CFC6();
	sub_1BFE9();
}

void sub_116A1(void)
{
	sub_126D1();
	sub_126F7();
	sub_24163(3);
	sub_24163(0x16);
}

void sub_116B7(void)
{
	sub_17204();
	motionClose();
	sub_117C4();
	sub_1CFC6();
	sub_1BFE9();
}

void sub_116CB(void)
{
	sub_126E4();
	sub_126D1();
	inputScanMode2(1);
}

void sub_116D8(void)
{
	unsigned long flags;

	flags = sub_2AA79();
	if ((flags & 0x00000004) == 0) {
		sub_122E7(1);
		if (!P3.0) {
			sub_12330(1);
			pinMonitors.pin[4].last = 0;
			inputScanHold(0x1a);
		}
	}
	if ((flags & 0x00000008) == 0) {
	}
	if ((flags & 0x00010000) == 0) {
		sub_12360(1);
		if (!P7.6) {
			sub_123A9(1);
			pinMonitors.pin[5].last = 0;
			inputScanHold(0x1a);
		}
	}
	sub_17204();
	motionClose();
	sub_117C4();
	sub_1CFC6();
	sub_1BFE9();
	return;
}

void sub_1177B(void)
{
	sub_11D97(1);
	sub_11F84(1);
	inputScanMode2(1);
}

void sub_1178A(void)
{
	sub_17544();
	sub_17580();
	motionStop();
	sub_1CFC6();
	sub_1D0CC();
}

/* P5.0-3, P8 and P11.2-3 inputs with their pull-ups (the keys sub_12111 reads) */
void sub_1179E(void)
{
	PU5 = 0x0f | PU5;
	PM5 = 0x0f | PM5;
	PU8 = 0xff;
	PM8 = 0xff;
	PU11 = 0x0c | PU11;
	PM11 = 0x0c | PM11;
}

/* and back to outputs at 0 */
void sub_117C4(void)
{
	P5 = 0xf0 & P5;
	PM5 = 0xf0 & PM5;
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
	switch (sub_2AB46() & 0x7fff) {
	case 0x0020:
		if (events & 0x0400)
			motionInfoPoll();
		if (touch.rate == 0 && (events & 0x000c))
			sub_172BC();
	case 0x0004:
	case 0x0008:
		sampleInputs1();
		sampleInputs2();
		sampleInputs3();
		pinMonitorPoll();
		keys = events & 0x00e3;
		if (keys != 0)
			sub_1C3B9(keys);
		break;
	case 0x0800:
		sub_12638();
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
	if (sub_2AB46() == 0x0020)
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
	inputRaw3.0 = P7.3;
	inputRaw3.1 = P7.7;
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
					sub_11FEF(0);
					pinMonitors.pin[0].config.handler();
				}
			} else {
				sub_11FEF(0);
			}
			pinMonitors.pin[0].last = level;
		}
		if (pinMonitors.mask & 0x02) {
			level = P4.6;
			if (pinMonitors.pin[1].last == level) {
				if (pinMonitors.pin[1].count-- == 0) {
					sub_11DE0(0);
					pinMonitors.pin[1].config.handler();
				}
			} else {
				sub_11DE0(0);
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
					sub_12330(0);
					pinMonitors.pin[4].config.handler();
				}
			} else {
				sub_12330(0);
			}
			pinMonitors.pin[4].last = level;
		}
		if (pinMonitors.mask & 0x20) {
			level = P7.6;
			if (pinMonitors.pin[5].last == level) {
				if (pinMonitors.pin[5].count-- == 0) {
					sub_123A9(0);
					pinMonitors.pin[5].config.handler();
				}
			} else {
				sub_123A9(0);
			}
			pinMonitors.pin[5].last = level;
		}
		if (pinMonitors.mask & 0x40) {
			level = P7.4;
			if (pinMonitors.pin[6].last == level) {
				if (pinMonitors.pin[6].count-- == 0) {
					sub_12424(0);
					pinMonitors.pin[6].config.handler();
				}
			} else {
				sub_12424(0);
			}
			pinMonitors.pin[6].last = level;
		}
	}
}

unsigned char sub_11CFB(void)
{
	return !inputState1.4;
}

unsigned char sub_11D03(void)
{
	return !inputState1.6;
}

unsigned char sub_11D0B(void)
{
	return !inputState1.1;
}

unsigned char sub_11D13(void)
{
	return !inputState1.2;
}

unsigned char sub_11D1B(void)
{
	return !inputState1.5;
}

unsigned char sub_11D23(void)
{
	return !inputState1.0;
}

unsigned char sub_11D2B(void)
{
	return !inputState1.3;
}

unsigned char sub_11D33(void)
{
	return inputState3.0 != 0;
}

unsigned char sub_11D3B(void)
{
	return !inputState3.2;
}

unsigned char sub_11D43(void)
{
	return inputState3.5 != 0;
}

unsigned char sub_11D4B(void)
{
	return intp11Level == 0;
}

int sub_11D57(void)
{
	return (unsigned char)inputState2.0;
}

int sub_11D61(void)
{
	return (unsigned char)inputState2.1;
}

void sub_11D6B(void)
{
	inputDelays.motionInfo = 0x10;
}

void sub_11D73(void)
{
	inputDelays.flag40 = 9;
}

void sub_11D7B(void)
{
	inputDelays.flag80 = 9;
}

/* the external interrupts: on (an edge chosen per pin) or off */
void sub_11D83(unsigned char enable)
{
	if (enable == 0) {
		EGP0.2 = 0;
		EGN0.2 = 0;
		PMK2 = 1;
		PIF2 = 0;
	}
}

void sub_11D97(unsigned char enable)
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

void sub_11DB9(struct PinConfig *config)
{
	pinMonitors.pin[1].config = *config;
	pinMonitors.pin[1].count = 0;
}

void sub_11DE0(unsigned char enable)
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
void sub_11E0A(void)
{
	unsigned int state;
	unsigned char value;
	struct SystemMode mode;

	buttonPress2.count++;
	state = sub_2AB46();
	if (buttonPress2.count < 2) {
		sub_11DE0(1);
	} else if (buttonPress2.count == 2) {
		sub_1179E();
		dly_tsk(3);
		buttonPress2.keys = sub_12081();
		sub_11DE0(1);
		sub_2C15B(0x2e, 1, &value);
		buttonPress2.time1 = value >= 4 ? value : 3;
		sub_2C15B(0x2f, 1, &value);
		buttonPress2.time2 = value >= 4 ? value : 3;
	} else if (buttonPress2.keys == sub_12081()) {
		switch (buttonPress2.keys) {
		case 1:
		case 2:
			mode = *sub_2AB39();
			if (buttonPress2.count == buttonPress2.time1) {
				if (state != sub_12074() && !mode.flag &&
				    !sub_2C658()->pressAnyState) {
					sub_12078(0);
					return;
				}
				buttonPress2.result = buttonPress2.keys;
				sub_2AB49(0x14);
				sub_11DE0(1);
			} else if (buttonPress2.count == buttonPress2.time2 || sub_12784()) {
				if (state != sub_12074() && state != 0x0008 && !mode.flag &&
				    !sub_2C658()->pressAnyState) {
					sub_12078(0);
					return;
				}
				buttonPress2.result = 2;
				sub_2AB49(0x31);
			} else {
				sub_11DE0(1);
			}
			break;
		}
	} else if (buttonPress2.count >= buttonPress2.time1) {
		if (state != sub_12074() && !sub_2C658()->pressAnyState) {
			sub_12078(0);
			return;
		}
		buttonPress1.keys = 1;
		buttonPress2.result = buttonPress2.keys;
		sub_2AB49(0x14);
	}
}

void sub_11F84(unsigned char enable)
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

void sub_11FA6(unsigned char enable)
{
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
}

void sub_11FC8(struct PinConfig *config)
{
	pinMonitors.pin[0].config = *config;
	pinMonitors.pin[0].count = 0;
}

void sub_11FEF(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x01;
	} else {
		pinMonitors.pin[0].last = P11.5;
		pinMonitors.pin[0].count = pinMonitors.pin[0].config.reload;
		pinMonitors.mask |= 0x01;
	}
}

int sub_12019(void)
{
	if (pinMonitors.mask & 0x01)
		return 1;
	buttonPress1.count = 0;
	buttonPress1.keys = 0;
	buttonPress1.result = 0;
	sub_12044(sub_2AB46());
	return 0;
}

int sub_1203A(void)
{
	return buttonPress1.result;
}

int sub_12040(void)
{
	return buttonPress1.state;
}

int sub_12044(unsigned int state)
{
	buttonPress1.state = state;
	return 0;
}

int sub_1204D(void)
{
	if (pinMonitors.mask & 0x02)
		return 1;
	buttonPress2.count = 0;
	buttonPress2.keys = 0;
	buttonPress2.result = 0;
	sub_12078(sub_2AB46());
	return 0;
}

int sub_1206E(void)
{
	return buttonPress2.result;
}

unsigned int sub_12074(void)
{
	return buttonPress2.state;
}

int sub_12078(unsigned int state)
{
	buttonPress2.state = state;
	return 0;
}

unsigned char sub_12081(void)
{
	unsigned char keys;

	keys = 0;
	if (P11.5)
		keys++;
	return keys;
}

unsigned char sub_1208E(unsigned char *keys)
{
	return keys[0] == 0xff && keys[1] == 0xa9;
}

unsigned char sub_120B8(unsigned char *keys)
{
	return (~keys[1] & 0x10) && (~keys[1] & 0x40);
}

unsigned char sub_120E7(unsigned char *keys)
{
	return keys[0] == 0xff && keys[1] == 0xaf;
}

/* the keys held: P8, P5.0-3 and the two buttons */
unsigned char sub_12111(void)
{
	unsigned char result;
	unsigned char keys[2];

	keys[0] = P8;
	keys[1] = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
				 (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	result = 0;
	if (sub_120B8(keys) && sub_2AB46() == 0x0004)
		result = 3;
	else if (sub_120E7(keys))
		result = 2;
	else
		result = 1;
	return result;
}

/* pin monitor 0's handler (P11.5 held) */
void sub_1217E(void)
{
	unsigned char value;

	buttonPress1.count++;
	if (sub_2AB46() != sub_12040() && !sub_2C658()->pressAnyState) {
		sub_12044(0);
	} else if (buttonPress1.count < 2) {
		sub_11FEF(1);
	} else if (buttonPress1.count == 2) {
		sub_1179E();
		dly_tsk(3);
		buttonPress1.keys = sub_12111();
		sub_11FEF(1);
		sub_2C15B(0x2e, 1, &value);
		buttonPress1.time1 = value >= 4 ? value : 3;
		sub_2C15B(0x2f, 1, &value);
		buttonPress1.time2 = value >= 4 ? value : 3;
	} else if (buttonPress1.keys == sub_12111()) {
		switch (buttonPress1.keys) {
		case 3:
			if (buttonPress1.count == buttonPress1.time1) {
				buttonPress1.result = buttonPress1.keys;
				sub_2AB49(0x0b);
			} else {
				sub_11FEF(1);
			}
			break;
		}
	}
}

void sub_1224B(unsigned char enable)
{
	if (enable == 0) {
		EGP0.7 = 0;
		EGN0.7 = 0;
		PMK7 = 1;
		PIF7 = 0;
	}
}

void sub_1225F(struct PinConfig *config)
{
	pinMonitors.pin[2].config = *config;
	pinMonitors.pin[2].count = 0;
}

void sub_12286(unsigned char enable)
{
	if (enable == 0)
		pinMonitors.mask &= ~0x04;
}

void sub_12295(void)
{
}

void sub_12296(unsigned char enable)
{
	if (enable == 0) {
		EGP0.4 = 0;
		EGN0.4 = 0;
		PMK4 = 1;
		PIF4 = 0;
	}
}

void sub_122AA(struct PinConfig *config)
{
	pinMonitors.pin[3].config = *config;
	pinMonitors.pin[3].count = 0;
}

void sub_122D1(unsigned char enable)
{
	if (enable == 0)
		pinMonitors.mask &= ~0x08;
}

void sub_122E0(void)
{
	sub_2AB49(5);
}

void sub_122E7(unsigned char enable)
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

void sub_12309(struct PinConfig *config)
{
	pinMonitors.pin[4].config = *config;
	pinMonitors.pin[4].count = 0;
}

void sub_12330(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x10;
	} else {
		pinMonitors.pin[4].last = P3.0;
		pinMonitors.pin[4].count = pinMonitors.pin[4].config.reload;
		pinMonitors.mask |= 0x10;
	}
}

void sub_1235A(void)
{
	sub_2AB49(2);
}

void sub_12360(unsigned char enable)
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

void sub_12382(struct PinConfig *config)
{
	pinMonitors.pin[5].config = *config;
	pinMonitors.pin[5].count = 0;
}

void sub_123A9(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x20;
	} else {
		pinMonitors.pin[5].last = P7.6;
		pinMonitors.pin[5].count = pinMonitors.pin[5].config.reload;
		pinMonitors.mask |= 0x20;
	}
}

void sub_123D3(void)
{
	sub_2AB49(0x10);
	return;
}

void sub_123DB(unsigned char enable)
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

void sub_123FD(struct PinConfig *config)
{
	pinMonitors.pin[6].config = *config;
	pinMonitors.pin[6].count = 0;
}

void sub_12424(unsigned char enable)
{
	if (enable == 0) {
		pinMonitors.mask &= ~0x40;
	} else {
		pinMonitors.pin[6].last = P7.4;
		pinMonitors.pin[6].count = pinMonitors.pin[6].config.reload;
		pinMonitors.mask |= 0x40;
	}
}

void sub_1244E(void)
{
	sub_2C394(6, 0x0200);
}

void sub_1245B(void)
{
}

void sub_1245C(void)
{
}

void sub_1245D(void)
{
	if (sub_11D1B() == 1) {
		sub_1CFFC();
	} else {
		sub_1D012();
		sub_21F33(0x20);
		sleepFactorSet5(0);
		sub_1C3A1();
	}
	set_flg(2, 0x0100);
}

void sub_12489(void)
{
	if (sub_11D23() == 1) {
		sleepFactorSet15(1);
	} else {
		sub_21F33(2);
		sleepFactorSet15(0);
	}
	set_flg(2, 0x0100);
}

/* timer 3 run ticks times first (the inputs held at a power-on) */
void inputScanHold(unsigned int ticks)
{
	DI();
	if (inputScan.hold < ticks) {
		sub_24163(0x16);
		inputScan.hold = ticks;
		sub_24110(3, 1);
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
	sub_2C342(2, 0x0008);
	if (inputScan.hold != 0) {
		inputScan.hold--;
		sub_24110(3, 1);
	} else if (inputScan.mode == 2) {
		sub_24110(0x16, inputScan.interval);
	} else {
		sub_24110(3, inputScan.interval);
	}
	if (inputDelays.motionInfo != 0) {
		if (--inputDelays.motionInfo == 0)
			motionInfoStart();
	}
	if (inputDelays.flag40 != 0) {
		if (--inputDelays.flag40 == 0)
			sub_2C342(2, 0x0040);
	}
	if (inputDelays.flag80 != 0) {
		if (--inputDelays.flag80 == 0)
			sub_2C342(2, 0x0080);
	}
	if (inputDelays.call1D0CB != 0) {
		if (--inputDelays.call1D0CB == 0)
			sub_1D0CB();
	}
}

/* the RTC's interrupt (state 0x0010 or 0x0800): P11.4 */
void sub_125F2(void)
{
	if (levelMonitor1.enable != 0) {
		levelMonitor1.now = P11.4;
		if (levelMonitor1.state != levelMonitor1.now && levelMonitor1.last == levelMonitor1.now) {
			levelMonitor1.state = levelMonitor1.now;
			if (levelMonitor1.state != 0)
				sub_2AB49(0x61);
		}
		levelMonitor1.last = levelMonitor1.now;
	}
}

/* task 3 in state 0x0800: P12.0 */
void sub_12638(void)
{
	if (levelMonitor2.enable != 0) {
		levelMonitor2.now = P12.0;
		if (levelMonitor2.state != levelMonitor2.now && levelMonitor2.last == levelMonitor2.now) {
			levelMonitor2.state = levelMonitor2.now;
			if (levelMonitor2.state != 0)
				sub_2AB49(0x42);
		}
		levelMonitor2.last = levelMonitor2.now;
	}
}

void sub_1267E(void)
{
}

unsigned char sub_1267F(void)
{
	return levelMonitor2.state == 0;
}

unsigned char sub_1268B(void)
{
	return levelMonitor1.state == 1;
}

void sub_12698(void)
{
	if (sub_11D2B() == 1) {
		if (sub_2AB46() == 0x0020)
			inputDelays.call1D0CB = 0x40;
	} else {
		sub_21F33(8);
		sub_1D0A8();
	}
	set_flg(2, 0x0100);
}

void sub_126C8(void)
{
}

unsigned char sub_126C9(void)
{
	return !inputState1.7;
}

void sub_126D1(void)
{
	levelMonitor1.last = 0;
	levelMonitor1.now = 0;
	levelMonitor1.state = 0;
	levelMonitor1.enable = 1;
}

void sub_126E4(void)
{
	levelMonitor2.last = 0;
	levelMonitor2.now = 0;
	levelMonitor2.state = 0;
	levelMonitor2.enable = 1;
}

void sub_126F7(void)
{
	inputFlag126F7 = 1;
}

/* the scan on timer 3 every interval */
void inputScanMode1(unsigned int interval)
{
	DI();
	if (inputScan.mode != 1) {
		sub_24163(0x16);
		inputScan.mode = 1;
		inputScan.interval = interval;
		inputScan.hold = 0;
		sub_24110(3, interval);
	}
	EI();
	return;
}

/* the scan on timer 0x16 every interval */
void inputScanMode2(unsigned int interval)
{
	DI();
	if (inputScan.mode != 2) {
		sub_24163(3);
		inputScan.mode = 2;
		inputScan.interval = interval;
		sub_24110(0x16, interval);
	}
	EI();
	return;
}

void sub_1275F(void)
{
	set_flg(2, 0x0100);
	return;
}

void sub_1276A(void)
{
	set_flg(2, 0x0100);
	return;
}

unsigned char sub_12775(unsigned char ticks)
{
	return ticks == buttonPress2.time2 + 1;
}

unsigned char sub_12784(void)
{
	return holdReached == 1;
}

void sub_1278E(void)
{
	unsigned char value;

	holdTicks = 0;
	holdReached = 0;
	sub_2C15B(0x2f, 1, &value);
	buttonPress2.time2 = value >= 4 ? value : 3;
}
