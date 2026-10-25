/*
 * The power sequences: task 1 runs the one powerState asks for (its bit 15 set) once the
 * phase is 1. A sequence brings six parts down or up in turn; each part's bit in
 * powerPending is cleared as the sequence passes it, and a part that reports itself
 * (powerReady) sets its bit in event flag 5, which powerWait waits for. A sequence fails
 * when a part is still pending at its end.
 */
#pragma sfr

#include "ernie.h"
#include "chip.h"
#include "clockout.h"
#include "config.h"
#include "csi.h"
#include "host.h"
#include "kernel.h"
#include "power.h"
#include "sleepfactor.h"
#include "spidev.h"
#include "state.h"
#include "task5.h"
#include "updatereq.h"
#include "version.h"

#define POWER_PARTS	6
#define POWER_ALL	0x3f		/* every part's bit */

/* a part of a sequence and its bit in powerPending and in event flag 5 */
struct PowerPart {
	unsigned char	id;
	unsigned int	flag;
};

extern void inputInit(void);
extern void sub_2C2ED(int, int);
extern void sub_2C394(int, int);
extern void sub_2C572(void);
extern unsigned char sub_2CD3F(void);
extern void sub_2C0C0(void);
extern void sub_11572(void);
extern void sub_1159F(void);
extern void sub_115F0(void);
extern void sub_116D8(void);
extern void sub_1168D(void);
extern void sub_116B7(void);
extern void sub_1178A(void);
extern unsigned char sub_11D57(void);

static unsigned char powerError;

static __near unsigned char powerPending;

static const struct PowerPart powerParts[POWER_PARTS] = {
	{ 1, 0x01 },
	{ 2, 0x02 },
	{ 3, 0x04 },
	{ 4, 0x08 },
	{ 5, 0x10 },
	{ 6, 0x20 },
};

unsigned char powerOn(void);
unsigned char powerResume(union PowerRequest *request);
unsigned char powerOff(void);
unsigned char powerSuspend(void);
unsigned char powerShutdown(void);
unsigned char powerCut(void);
unsigned char powerReset(void);
unsigned char powerUpdateAnswer(void);
unsigned char powerUpdate(void);
void sub_24076(void);

/* runs the sequence powerState asks for, and ends its phase */
void powerSequence(void)
{
	unsigned char result;

	result = 0;
	if (statePhase != 1)
		return;
	switch (powerState) {
	case 0x8020:
		result = powerResume(&stateRequest);
		break;
	case 0x8008:
		result = powerSuspend();
		break;
	case 0x8004:
		result = powerOff();
		break;
	case 0x8002:
		result = powerOn();
		break;
	case 0x8010:
		result = powerShutdown();
		break;
	case 0x8800:
		result = powerCut();
		break;
	case 0x8040:
		result = powerReset();
		break;
	case 0x8200:
		result = powerUpdateAnswer();
		break;
	case 0x8400:
		result = powerUpdate();
		break;
	}
	if (result == 0) {
		powerState &= 0x7fff;
		statePhase = 2;
		stateEnter();
		sub_2C2ED(3, 0x1000);
		sub_2C394(6, 0x80);
	} else {
		powerState &= 0x7fff;
		statePhase = 3;
	}
	sub_2C572();
}

void sub_23A30(void)
{
	powerError = 0;
}

unsigned char sub_23A37(void)
{
	return powerError;
}

unsigned char sub_23A40(void)
{
	return 0;
}

/* a part reports itself: its bit in event flag 5 */
unsigned char powerReady(unsigned char id)
{
	unsigned char result;
	unsigned char i;

	result = 0;
	i = POWER_PARTS;
	while (i != 0) {
		i--;
		if (powerParts[i].id == id) {
			set_flg(5, powerParts[i].flag);
			break;
		}
	}
	return result;
}

/* the sequence has passed a part */
unsigned char powerPendingClear(unsigned char id)
{
	unsigned char result;
	unsigned char i;

	result = 0;
	i = POWER_PARTS;
	while (i != 0) {
		i--;
		if (powerParts[i].id == id) {
			powerPending &= ~powerParts[i].flag;
			break;
		}
	}
	return result;
}

void powerPendingReset(void)
{
	powerPending = POWER_ALL;
}

unsigned char powerPendingGet(void)
{
	return powerPending;
}

/* waits for any part to report itself; 1 when none did in time */
unsigned char powerWait(TMO timeout)
{
	unsigned char result;
	ER ercd;
	FLGPTN ptn;

	result = 0;
	ptn = 0;
	ercd = twai_flg(5, POWER_ALL, TWF_ORW, &ptn, timeout);
	if (ercd == E_OK)
		clr_flg(5, ~ptn);
	else
		result = 1;
	return result;
}

/* 0x8002: from off */
unsigned char powerOn(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	sleepFactorInit();
	sub_23A30();
	spiDevInit();
	powerPendingClear(1);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	inputInit();
	powerPendingClear(3);
	powerJobPostOn();
	powerWait(0x19);
	powerPendingClear(5);
	powerPendingClear(4);
	powerPendingClear(6);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8020: brings the host up; the request's mode (bits 0-6) picks the last step */
unsigned char powerResume(union PowerRequest *request)
{
	unsigned char result;

	result = 0;
	if (stateCause == 0x72) {
		updaterRequestAnswerOk();
		powerPendingClear(5);
		return result;
	}
	powerPendingReset();
	if (sub_2CD3F() != 0)
		sub_24076();
	sleepFactorSet20(0);
	stateWakeupFactor.word = request->word;
	sub_11572();
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	sub_2C394(6, 0x80);
	powerPendingClear(6);
	if (request->bits.mode != 0x54) {
		powerJobPostResume();
		powerWait(0x19);
	}
	powerPendingClear(5);
	sub_1159F();
	powerPendingClear(3);
	csiOpenLsb();
	hostInit();
	hostIntInit();
	hostIntEnable();
	set_flg(1, 0x40);
	dly_tsk(3);
	powerPendingClear(4);
	switch (request->bits.mode) {
	case 0x0c:
		spiDevInit();
		result = spiDevAuthenticate();
		break;
	case 0x0d:
		sub_23A30();
		result = sub_23A37();
		break;
	case 0x11:
		result = spiDevPowerOn();
		break;
	default:
		P91 = 0;
		P92 = 0;
		P1.5 = 0;
		dly_tsk(1);
		P1.7 = 1;
		if (request->bits.notify)
			set_flg(4, 0x08);
		dly_tsk(1);
		result = 0;
		break;
	}
	powerPendingClear(1);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8004 */
unsigned char powerOff(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	switch (sub_2C658()->hardwareInfoSelect) {
	case 1:
		setHardwareInfoFlags(0x80);
		break;
	case 3:
		setHardwareInfoFlags(0x40);
		break;
	case 0:
	case 2:
	default:
		clearHardwareInfoFlags(0xc0);
		break;
	}
	if (sub_2CD3F() != 0)
		sub_24076();
	sub_115F0();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	P5.7 = 0;
	P5.6 = 0;
	powerPendingClear(4);
	P91 = 0;
	P92 = 0;
	P1.5 = 0;
	dly_tsk(1);
	P1.7 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	powerJobPostOff();
	powerWait(0x8ca);
	powerPendingClear(5);
	sub_2C394(6, 0x80);
	powerWait(5);
	powerPendingClear(6);
	PER0.2 = 0;
	if (stateCause == 0x56)
		powerJobPostOffEnd();
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8008: as powerOff, with P9.1, P9.2 and P1.5 left high */
unsigned char powerSuspend(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	sleepFactorSet20(0);
	sub_116D8();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	P5.7 = 0;
	P5.6 = 0;
	powerPendingClear(4);
	P91 = 1;
	P92 = 1;
	P1.5 = 1;
	dly_tsk(1);
	P1.7 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	powerJobPostSuspend();
	powerWait(0xfa);
	powerPendingClear(5);
	sub_2C394(6, 0x80);
	powerWait(5);
	powerPendingClear(6);
	PER0.2 = 0;
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8010 */
unsigned char powerShutdown(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	sub_1168D();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	P5.7 = 0;
	P5.6 = 0;
	powerPendingClear(4);
	P91 = 0;
	P92 = 0;
	P1.5 = 0;
	dly_tsk(1);
	P1.7 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	powerJobPostShutdown();
	powerWait(0x8ca);
	powerPendingClear(5);
	powerPendingClear(6);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8800: stops the clock output first, pulses P6.2 at the end */
unsigned char powerCut(void)
{
	unsigned char result;

	result = 0;
	clockOutputStop();
	powerJobPostCut();
	powerWait(0x19);
	powerPendingClear(5);
	if (sub_11D57() == 1)
		P33 = 1;
	P6.2 = 0;
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	P5.7 = 0;
	P5.6 = 0;
	powerPendingClear(4);
	P91 = 0;
	P92 = 0;
	P1.5 = 0;
	dly_tsk(1);
	P1.7 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	sub_116B7();
	powerPendingClear(3);
	powerPendingClear(6);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	if (powerPendingGet() != 0)
		result = 1;
	dly_tsk(3);
	P6.2 = 1;
	if (P33) {
		dly_tsk(0x39d);
		P33 = 0;
	}
	return result;
}

/* 0x8040: pulses P1.7 low */
unsigned char powerReset(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	sub_1178A();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	P5.7 = 0;
	powerPendingClear(4);
	powerPendingClear(1);
	powerJobPostReset();
	powerWait(0x19);
	powerPendingClear(5);
	powerPendingClear(6);
	sub_2C2ED(3, 0x1000);
	powerWait(0x19);
	powerPendingClear(2);
	P1.7 = 0;
	dly_tsk(3);
	P1.7 = 1;
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8200 */
unsigned char powerUpdateAnswer(void)
{
	unsigned char result;

	result = 0;
	updaterRequestAnswerOk();
	return result;
}

/* 0x8400 */
unsigned char powerUpdate(void)
{
	unsigned char result;

	result = 0;
	powerPendingReset();
	powerPendingClear(3);
	powerPendingClear(1);
	powerJobPostUpdate();
	powerWait(0xfa);
	powerPendingClear(5);
	powerPendingClear(6);
	powerPendingClear(2);
	updaterRequestAnswerOk();
	powerWait(0x32);
	powerPendingClear(4);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

void sub_24076(void)
{
	sub_2C0C0();
}
