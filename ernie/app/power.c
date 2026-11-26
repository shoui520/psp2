/*
 * The power sequences: task 1 runs the one powerState asks for (its bit 15 set) once the
 * phase is 1. A sequence brings six parts down or up in turn; each part's bit in
 * powerPending is cleared as the sequence passes it, and a part that reports itself
 * (powerReady) sets its bit in event flag 5, which powerWait waits for. A sequence fails
 * when a part is still pending at its end.
 */
#pragma sfr

#include "ernie.h"
#include "board.h"
#include "battery.h"
#include "chip.h"
#include "clockout.h"
#include "config.h"
#include "csi.h"
#include "debugpin.h"
#if SOURCE_0103
#include "dtc.h"
#endif
#include "host.h"
#include "keepram.h"
#include "input.h"
#include "kernel.h"
#include "param.h"
#include "power.h"
#include "sleepfactor.h"
#include "spidev.h"
#include "state.h"
#include "task.h"
#include "task5.h"
#include "updatereq.h"
#include "version.h"

#define POWER_PARTS	6
#define POWER_ALL	0x3f		/* every part's bit */
#define POWER_PART(id)	(1 << ((id) - 1))	/* part id's bit (powerParts) */

/* a sequence's wait for a part: since 0x0100060B for that part's bit alone, cleared before the
   part is started, and the result kept by some sequences */
#if FW_SINCE(0x0100060B)
#define POWER_WAIT_CLEAR(id)		clr_flg(5, ~POWER_PART(id))
#define POWER_WAIT(id, timeout)		powerWait(POWER_PART(id), timeout)
#define POWER_WAIT_KEEP(id, timeout)	(waitResult = powerWait(POWER_PART(id), timeout))
#else
#define POWER_WAIT_CLEAR(id)
#define POWER_WAIT(id, timeout)		powerWait(timeout)
#define POWER_WAIT_KEEP(id, timeout)	powerWait(timeout)
#endif

/* a part of a sequence and its bit in powerPending and in event flag 5 */
struct PowerPart {
	unsigned char	id;
	unsigned int	flag;
};

extern void inputInit(void);
#if SOURCE_0103
#endif
extern void inputResumePrepare(void);
extern void inputResumeOpen(void);
extern void inputPowerOff(void);
extern void inputSuspend(void);
extern void inputShutdown(void);
extern void inputCut(void);
extern void inputReset(void);
extern unsigned char input2Bit0Get(void);
extern unsigned char input3Bit5High(void);

#if !SOURCE_0103
static unsigned char powerError;
#endif

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
#if FW_SINCE(0x00090903)
unsigned char powerUpdateAnswer(void);
unsigned char powerUpdate(void);
#endif
#if FW_SINCE(0x01000104)
void powerParamLoad(void);
#endif

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
#if FW_SINCE(0x00090903)
	case 0x8200:
		result = powerUpdateAnswer();
		break;
	case 0x8400:
		result = powerUpdate();
		break;
#endif
	}
	if (result == 0) {
		powerState &= 0x7fff;
		statePhase = 2;
#if BOARD_DEBUG_PINS_POWER
		if (powerState & STATE_OFF)
			debugPinReserved(1);
		else if (powerState & STATE_SUSPEND)
			debugPinReserved(2);
		else if (powerState & STATE_RESUME)
			debugPinReserved(3);
		else
			debugPinReserved(0);
#elif FW_SINCE(0x0100060B)
		/* Sony's tests off, suspend and resume here and does nothing (0x00601000 shows the
		   state on its probe pins there) */
		if ((powerState & STATE_OFF) || (powerState & STATE_SUSPEND) ||
		    (powerState & STATE_RESUME))
			;
#if BOARD_DOL1002 || BATTERY_GAUGE
		if (powerState & STATE_OFF)	/* DOL-1002: and the off once more, also for nothing */
			;
#endif
#endif
		stateEnter();
		task2SetFlag(3, 0x1000);
#if BATTERY_GAUGE
		switch (powerState) {
		case STATE_RESUME:
			break;
		default:
			batteryStateDone();
			break;
		}
#endif
#if SOURCE_0103 && !BOARD_DOL1001
		/* USS-1001: the power LED told of an off or a suspend */
		switch (powerState) {
		case STATE_OFF:
		case STATE_SUSPEND:
			task5SetFlag(4, 0x0800);
			break;
		}
#endif
#if FW_SINCE(0x01000104)
		task6SetFlag(6, 0x80);
#endif
	} else {
		powerState &= 0x7fff;
		statePhase = 3;
#if SOURCE_0103
		stateEnter();
#endif
	}
	taskHookState();
}

#if !SOURCE_0103	/* the slim has no power-on test */
void powerErrorClear(void)
{
	powerError = 0;
#if BOARD_POWER_TEST_PINS
	P2.6 = 0;
	P1.7 = 0;
	P2.4 = 0;
#endif
}

#if BOARD_POWER_TEST_PINS
/* the steps of the test, one a tick: P2.7 low, P2.6 up, P1.7 and P2.4 up, P2.5 high, P2.6
   down; 0x80 for a step it does not know, where it stays */
unsigned char powerErrorGet(void)
{
	if (powerError == 0)
		powerError = 1;
	else
		powerError = 0x80;
	while (powerError != 0) {
		switch (powerError) {
		case 1:
			if (!P2.7)
				powerError++;
			break;
		case 2:
			P2.6 = 1;
			powerError++;
			break;
		case 3:
			P1.7 = 1;
			P2.4 = 1;
			powerError++;
			break;
		case 4:
			if (P2.5)
				powerError++;
			break;
		case 5:
			P2.6 = 0;
			powerError = 0;
			break;
		default:
			powerError = 0x80;
			break;
		}
		dly_tsk(1);
	}
	return powerError;
}
#else
unsigned char powerErrorGet(void)
{
	return powerError;
}
#endif

#if HW_IRT001
/* no fault line */
int powerFaulty(void)
{
	return 0;
}
#elif BOARD_102603
/* its fault line: input 3's bit 5 */
int powerFaulty(void)
{
	return input3Bit5High();
}
#else
unsigned char powerFaulty(void)
{
	return 0;
}
#endif
#endif	/* !SOURCE_0103 */

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
#if FW_SINCE(0x0100060B)
unsigned char powerWait(FLGPTN pattern, TMO timeout)
#else
unsigned char powerWait(TMO timeout)
#endif
{
	unsigned char result;
	ER ercd;
	FLGPTN ptn;

	result = 0;
	ptn = 0;
#if FW_SINCE(0x0100060B)
	ercd = twai_flg(5, pattern, TWF_ORW, &ptn, timeout);
#else
	ercd = twai_flg(5, POWER_ALL, TWF_ORW, &ptn, timeout);
#endif
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
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
	sleepFactorInit();
#if SOURCE_0103
	/* USS-1001: longer waits, and the input started by its state request */
	spiDevInit();
	powerPendingClear(1);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x177);
	powerPendingClear(2);
	POWER_WAIT_CLEAR(3);
	inputStateRequest(0);
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	POWER_WAIT_CLEAR(5);
	powerJobPostOn();
	POWER_WAIT(5, 0x4e2);
#else
	powerErrorClear();
	spiDevInit();
	powerPendingClear(1);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
	inputInit();
	powerPendingClear(3);
	POWER_WAIT_CLEAR(5);
	powerJobPostOn();
	POWER_WAIT(5, 0x19);
#endif
	powerPendingClear(5);
	powerPendingClear(4);
#if FW_SINCE(0x0100060B)
	POWER_WAIT_CLEAR(6);
	task6SetFlag(6, 0x2080);
	POWER_WAIT(6, 3);
#endif
	powerPendingClear(6);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8020: brings the host up; the request's mode (bits 0-6) picks the last step */
unsigned char powerResume(union PowerRequest *request)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
#if FW_SINCE(0x00090903)
	if (stateCause == 0x72) {
		updaterRequestAnswerOk();
		powerPendingClear(5);
		return result;
	}
#endif
	powerPendingReset();
#if FW_SINCE(0x01000104)
	if (configTakeChanged() != 0)
		powerParamLoad();
	sleepFactorSet20(0);
#endif
	stateWakeupFactor.word = request->word;
#if BATTERY_GAUGE
	batteryResume(stateModeAtLevel9(&stateWakeupFactor));
#endif
#if SOURCE_0103
	/* USS-1001: the input's two state requests around the parts, the DTC and the host's
	   requests opened with the link */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(5));
	POWER_WAIT_KEEP(3, 3);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT_KEEP(2, 0x19);
	powerPendingClear(2);
	task6SetFlag(6, 0x80);
	POWER_WAIT_KEEP(6, 5);
	powerPendingClear(6);
#if BOARD_USS1001 && SOURCE_0104
	/* USS-1002: on every mode, a longer wait */
	POWER_WAIT_CLEAR(5);
	powerJobPostResume();
	POWER_WAIT_KEEP(5, 0x4b);
#elif BOARD_DOL1002
	/* DOL-1002: not on mode 4, and a longer wait */
	if (request->bits.mode != 0x04) {
		POWER_WAIT_CLEAR(5);
		powerJobPostResume();
		POWER_WAIT_KEEP(5, 0xaf);
	}
#else
	if (request->bits.mode != 0x54) {
		POWER_WAIT_CLEAR(5);
		powerJobPostResume();
		POWER_WAIT_KEEP(5, 0x19);
	}
#endif
	powerPendingClear(5);
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(6));
	POWER_WAIT_KEEP(3, 3);
	powerPendingClear(3);
#if BOARD_USS1001
	dtcOpen();
#endif
	csiOpenLsb();
	hostInit();
	hostOpenRequests();
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
	case 0x11:
		result = spiDevPowerOn();
		break;
	default:
		P91 = 0;
		dly_tsk(1);
		SPIDEV_POWER2 = 1;
#if !BOARD_DOL1001	/* DOL-1001 tells nobody */
		if (request->bits.notify)
			set_flg(4, 0x08);
#endif
		dly_tsk(1);
		result = 0;
		break;
	}
#else
#if BOARD_DEBUG_PINS_POWER
	debugPinCode(stateWakeupFactor.bits.mode);
#endif
	inputResumePrepare();
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT_KEEP(2, 0x19);
	powerPendingClear(2);
#if FW_SINCE(0x00090502)
	task6SetFlag(6, 0x80);
#endif
#if FW_SINCE(0x0100060B)
	POWER_WAIT_KEEP(6, 5);
#endif
	powerPendingClear(6);
	if (request->bits.mode != 0x54) {
		POWER_WAIT_CLEAR(5);
		powerJobPostResume();
		POWER_WAIT_KEEP(5, 0x19);
	}
	powerPendingClear(5);
	inputResumeOpen();
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
		powerErrorClear();
		result = powerErrorGet();
		break;
	case 0x11:
		result = spiDevPowerOn();
		break;
	default:
		P91 = 0;
#if BOARD_POWER_P92_P15
		P92 = 0;
		P1.5 = 0;
#endif
		dly_tsk(1);
		P1.7 = 1;
#if FW_SINCE(0x01000306)
		if (request->bits.notify)
			set_flg(4, 0x08);
#endif
		dly_tsk(1);
		result = 0;
		break;
	}
#endif
	powerPendingClear(1);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8004 */
unsigned char powerOff(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif
#if !FW_SINCE(0x01000104)
	unsigned char cause;
	STATE_TRANSITION_EVENT
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
#if FW_SINCE(0x01000104)
	switch (configGet()->hardwareInfoSelect) {
	case 1:
		setHardwareInfoFlags(0x80);
		break;
#if FW_SINCE(0x01000306)
	case 3:
		setHardwareInfoFlags(0x40);
		break;
	case 0:
	case 2:
	default:
		clearHardwareInfoFlags(0xc0);
		break;
#else
	case 0:
	case 2:
	default:
		clearHardwareInfoFlags(0x80);
		break;
#endif
	}
	if (configTakeChanged() != 0)
		powerParamLoad();
#endif
#if SOURCE_0103
	/* USS-1001: the input stopped by its state request, the DTC closed with the CSI */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(1));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
#if BOARD_USS1001
	dtcClose();
#endif
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x177);
	powerPendingClear(2);
#else
	inputPowerOff();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
	P92 = 0;
	P1.5 = 0;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
#if FW_SINCE(0x01000104)
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
#endif
	POWER_WAIT_CLEAR(5);
	powerJobPostOff();
	POWER_WAIT(5, 0x8ca);
	powerPendingClear(5);
	POWER_WAIT_CLEAR(6);
	task6SetFlag(6, 0x80);
#if FW_SINCE(0x00090502)
	POWER_WAIT(6, 5);
#endif
	powerPendingClear(6);
#if !FW_SINCE(0x01000104)
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
#if FW_SINCE(0x00080502)	/* 0x00080307 leaves PER0 bit 2 set in an off or a suspend */
	PER0.2 = 0;
#endif
#if FW_SINCE(0x0100060B)
	if (stateCause == 0x56 || stateCause == 0x5c)
#elif FW_SINCE(0x00090006)	/* 0x00080504 has no off end job */
	if (stateCause == 0x56)
#endif
#if FW_SINCE(0x00090006)
		powerJobPostOffEnd();
#endif
	if (powerPendingGet() != 0)
		result = 1;
#if !FW_SINCE(0x01000104)
	/* before 0x01000104 a warm reset's cause is taken here: transition 0x14 after one
	   (0x00090804's 0x00) */
	if (result == 0) {
		cause = getResetCause();
		switch (cause) {
		case 0xf1:
		case 0x11:
#if FW_SINCE(0x00090903)
			STATE_TRANSITION_NOMASK(0x14);
#else
			STATE_TRANSITION_NOMASK(0x00);
#endif
			break;
		}
		setResetCause(0);
	}
#endif
	return result;
}

/* 0x8008: as powerOff, with P9.1, P9.2 and P1.5 left high */
unsigned char powerSuspend(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
#if FW_SINCE(0x01000104)
	sleepFactorSet20(0);
#endif
#if SOURCE_0103
	/* USS-1001: as its off, with P9.1 high */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(3));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
#if BOARD_USS1001
	dtcClose();
#endif
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 1;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x177);
	powerPendingClear(2);
#else
	inputSuspend();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 1;
	P92 = 1;
	P1.5 = 1;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
#if FW_SINCE(0x01000104)
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
#endif
	POWER_WAIT_CLEAR(5);
	powerJobPostSuspend();
	POWER_WAIT(5, 0xfa);
	powerPendingClear(5);
	POWER_WAIT_CLEAR(6);
#if FW_SINCE(0x00090502)
	task6SetFlag(6, 0x80);
	POWER_WAIT(6, 5);
#endif
	powerPendingClear(6);
#if !FW_SINCE(0x01000104)
	/* before 0x01000104 the host link stops last */
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
#if FW_SINCE(0x00080502)	/* 0x00080307 leaves PER0 bit 2 set in an off or a suspend */
	PER0.2 = 0;
#endif
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8010 */
unsigned char powerShutdown(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
#if SOURCE_0103
	/* USS-1001: as its off, the timers' clock stopped at the end */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(9));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
#if BOARD_USS1001
	dtcClose();
#endif
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
	POWER_WAIT_CLEAR(5);
	powerJobPostShutdown();
	POWER_WAIT(5, 0x8ca);
	powerPendingClear(5);
	powerPendingClear(6);
	PER0.2 = 0;
#else
	inputShutdown();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
#if BOARD_POWER_P92_P15
	P92 = 0;
	P1.5 = 0;
#endif
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
#if FW_SINCE(0x01000104)
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
	POWER_WAIT_CLEAR(5);
	powerJobPostShutdown();
	POWER_WAIT_KEEP(5, 0x8ca);
	powerPendingClear(5);
	powerPendingClear(6);
#endif
#if !FW_SINCE(0x01000104)
	/* before 0x01000104 the host link stops last */
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#endif
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

/* 0x8800: stops the clock output first, pulses P6.2 (USS-1001: P3.4) at the end */
unsigned char powerCut(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
#if SOURCE_0103
	/* USS-1001: P3.4 in place of P6.2, the input stopped by its state request */
	clockOutputStop();
	POWER_WAIT_CLEAR(5);
	powerJobPostCut();
	POWER_WAIT(5, 0x19);
	powerPendingClear(5);
	BOARD_POWER_CUT_PIN = 0;
	hostIntDisable();
	dly_tsk(1);
	csiClose();
#if BOARD_USS1001
	dtcClose();
#endif
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(7));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	powerPendingClear(6);
	PER0.2 = 0;
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x32);
	powerPendingClear(2);
	if (powerPendingGet() != 0)
		result = 1;
	dly_tsk(3);
	BOARD_POWER_CUT_PIN = 1;
	return result;
#else
#if FW_SINCE(0x00090203)
	clockOutputStop();
#endif
	POWER_WAIT_CLEAR(5);
	powerJobPostCut();
	POWER_WAIT(5, 0x19);
	powerPendingClear(5);
#if BOARD_BBMC
	if (input2Bit0Get() == 1)
		P33 = 1;
#endif
	P6.2 = 0;
#if !FW_SINCE(0x00090203)
	clockOutputStop();	/* 0x00090009: after the line */
#endif
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	HOST_FRAME = 0;
	HOST_READY = 0;
	powerPendingClear(4);
	P91 = 0;
#if BOARD_POWER_P92_P15
	P92 = 0;
	P1.5 = 0;
#endif
	dly_tsk(1);
	SPIDEV_POWER2 = 0;
	dly_tsk(1);
	powerPendingClear(1);
	inputCut();
	powerPendingClear(3);
	powerPendingClear(6);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT_KEEP(2, 0x19);
	powerPendingClear(2);
	if (powerPendingGet() != 0)
		result = 1;
	dly_tsk(3);
	P6.2 = 1;
#if BOARD_BBMC
	if (P33) {
		dly_tsk(0x39d);
		P33 = 0;
	}
#endif
	return result;
#endif
}

/* 0x8040: pulses SPIDEV_POWER2 (P1.7, USS-1001 P9.7) low */
unsigned char powerReset(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
#if SOURCE_0103
	/* USS-1001: the input reset by its state request, P9.7 pulsed */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(0x0b));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
#if BOARD_USS1001
	dtcClose();
#endif
	HOST_FRAME = 0;
	powerPendingClear(4);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(5);
	powerJobPostReset();
	POWER_WAIT(5, 0x19);
	powerPendingClear(5);
	powerPendingClear(6);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x19);
	powerPendingClear(2);
#else
	inputReset();
	powerPendingClear(3);
	hostIntDisable();
	dly_tsk(1);
	csiClose();
	HOST_FRAME = 0;
	powerPendingClear(4);
	powerPendingClear(1);
#if FW_SINCE(0x01000107)
	POWER_WAIT_CLEAR(5);
	powerJobPostReset();
#endif
#if FW_SINCE(0x01000306)
	POWER_WAIT(5, 0x19);
#endif
	powerPendingClear(5);
	powerPendingClear(6);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT_KEEP(2, 0x19);
	powerPendingClear(2);
#endif
	SPIDEV_POWER2 = 0;
	dly_tsk(3);
#if !BOARD_DOL1002 && !BATTERY_GAUGE	/* DOL-1002 and USS-1002 leave it low */
	SPIDEV_POWER2 = 1;
#endif
	if (powerPendingGet() != 0)
		result = 1;
	return result;
}

#if FW_SINCE(0x00090903)
/* 0x8200 */
unsigned char powerUpdateAnswer(void)
{
	unsigned char result;
#if SOURCE_0103
	unsigned char waitResult;

	/* USS-1001: the parts ended, the host link given its time first */
	result = 0;
	waitResult = 0;
	powerPendingReset();
	powerPendingClear(3);
	powerPendingClear(1);
	powerPendingClear(5);
	powerPendingClear(6);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT_KEEP(2, 0x1f4);
	powerPendingClear(2);
	updaterRequestAnswerOk();
	powerPendingClear(4);
	if (powerPendingGet() != 0)
		;
	return result;
#else
	result = 0;
	updaterRequestAnswerOk();
	return result;
#endif
}

/* 0x8400 */
unsigned char powerUpdate(void)
{
	unsigned char result;
#if FW_SINCE(0x0100060B)
	unsigned char waitResult;
#endif

	result = 0;
#if FW_SINCE(0x0100060B)
	waitResult = 0;
#endif
	powerPendingReset();
#if SOURCE_0103
	/* USS-1001: the input and the host's requests closed, the host link given its time */
	POWER_WAIT_CLEAR(3);
	inputStateRequest(INPUT_STATE(0x0c));
	POWER_WAIT(3, 3);
	powerPendingClear(3);
	powerPendingClear(1);
	POWER_WAIT_CLEAR(5);
	powerJobPostUpdate();
	POWER_WAIT(5, 0xfa);
	powerPendingClear(5);
	powerPendingClear(6);
	POWER_WAIT_CLEAR(2);
	task2SetFlag(3, 0x1000);
	POWER_WAIT(2, 0x1f4);
	powerPendingClear(2);
	POWER_WAIT_CLEAR(4);
#if BOARD_DOL1002 || BATTERY_GAUGE
	hostCloseRequests(5);
#else
	if (hostCloseRequests(5) != 0)
		;
#endif
	updaterRequestAnswerOk();
	POWER_WAIT(4, 0x32);
	powerPendingClear(4);
	if (powerPendingGet() != 0)
		;
#if BOARD_DOL1002 || BATTERY_GAUGE
	return result;
#else
	return waitResult;
#endif
#else
	powerPendingClear(3);
	powerPendingClear(1);
#if FW_SINCE(0x01000104)
	POWER_WAIT_CLEAR(5);
	powerJobPostUpdate();
	POWER_WAIT(5, 0xfa);
#endif
	powerPendingClear(5);
	powerPendingClear(6);
	powerPendingClear(2);
	POWER_WAIT_CLEAR(4);
	updaterRequestAnswerOk();
	POWER_WAIT_KEEP(4, 0x32);
	powerPendingClear(4);
	if (powerPendingGet() != 0)
		result = 1;
	return result;
#endif
}
#endif

#if FW_SINCE(0x01000104)
void powerParamLoad(void)
{
	paramLoad();
}
#endif
