/*
 * Task 5's part of the power sequences: a sequence posts its job (powerJobPostOn, ...,
 * task5.c), task 5 runs it here, and the job reports part 5 done (powerReady).
 */
#pragma sfr

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "charge.h"
#include "config.h"
#include "elmo.h"
#include "iica.h"
#include "kernel.h"
#include "power.h"
#include "powerjob.h"
#include "powerreq.h"
#include "powerseq.h"
#include "sysmode.h"
#include "timer.h"

#define POWER_JOB_PART	5	/* the power part the jobs report */


extern void sub_11D73(void);
extern void sub_11D7B(void);
extern unsigned char sub_2A0ED(unsigned char);
extern void sub_2A6A0(unsigned int);
extern void sub_2A816(void);
extern unsigned char sub_2AB49(int transition);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern void sub_2DF1F(void);
extern void sub_2DF86(unsigned int on);
extern void sub_2DFAA(unsigned int on);
extern void sub_2DFF2(unsigned int);
extern void sub_2E016(unsigned int);
extern void sub_2E05E(unsigned int);
extern unsigned char sub_2E0CF(void);
extern void sub_2E0D8(unsigned int);
extern void sub_2E165(unsigned int);
extern void sub_2E499(void);
extern void sub_2E4BB(void);
extern void sub_2E854(unsigned int);
extern unsigned char sub_2E8B4(void);

unsigned char powerResumed;		/* set by a resume, cleared by a power on */
static unsigned char padding;		/* 0xff2ed: nothing uses it */
unsigned char powerJobId;		/* the job task 5 runs next (POWER_JOB_*) */
unsigned char powerJobFlag;		/* only ever cleared */
unsigned int powerCutReg0;		/* registers 0xb0-0xb2 of elmoRead's device, read at a */
unsigned int powerCutReg1;		/* power cut and cleared at a power on */
unsigned int powerCutReg2;

static void powerJobOn(void);
static void powerJobResume(void);
static void powerJobOff(void);
static void powerJobShutdown(void);
static void powerJobSuspend(void);
static void powerJobCut(void);
static void powerJobOffEnd(void);
static void powerJobUpdate(void);
static void powerJobReset(void);

/* task 5, event 0x0002 of flag 4: runs the job posted */
void powerJob(void)
{
	switch (powerJobId) {
	case POWER_JOB_ON:
		powerJobOn();
		return;
	case POWER_JOB_RESUME:
		powerJobResume();
		return;
	case POWER_JOB_OFF:
		powerJobOff();
		return;
	case POWER_JOB_SUSPEND:
		powerJobSuspend();
		return;
	case POWER_JOB_SHUTDOWN:
		powerJobShutdown();
		return;
	case POWER_JOB_CUT:
		powerJobCut();
		return;
	case POWER_JOB_OFF_END:
		powerJobOffEnd();
		return;
	case POWER_JOB_UPDATE:
		powerJobUpdate();
		return;
	case POWER_JOB_RESET:
		powerJobReset();
		return;
	}
}

static void powerJobOn(void)
{
	if (!P12.0) {
		P6.2 = 0;
		sub_2AB49(0x41);
	}
	EGP0.0 = 0;
	EGN0.0 = 1;
	PIF0 = 0;
	PMK0 = 0;
	powerSeqOn();
	powerCutReg0 = powerCutReg1 = powerCutReg2 = 0;
	sub_2A816();
	powerSeqInit();
	sub_2E4BB();
	sub_2DF1F();
	timerStart(0x17, 1);
	powerResumed = 0;
	powerReady(POWER_JOB_PART);
}

static void powerJobResume(void)
{
	struct SystemMode *mode;

	batteryBLLineOff();
	powerResumed = 1;
	if (elmoRegA6 & 0x8000) {
		sub_2E854(0);
		sub_2A6A0(4);
		sub_2E854(1);
	}
	powerSeqResume();
	P0.6 = 1;
	P0.5 = 1;
	sub_11D73();
	sub_11D7B();
	powerRequestInit();
	chargeSetAcDelay();
	sub_2C315(4, 0x20);
	mode = sub_2AB39();
	if (mode->mode == 0x0f && !mode->flag)
		sub_2E05E(1);
	powerReady(POWER_JOB_PART);
}

/* the mode chargeUsbMode asks for, applied again */
#define POWER_JOB_MODE()						\
	switch (chargeUsbMode) {						\
	case 1:								\
	case 2:								\
	case 3:								\
		if (chargeBatteryLevel9())					\
			chargeUsbMode = 1;				\
		else							\
			chargeUsbMode = 4;				\
		chargeSetCurrent();						\
		break;							\
	case 0:								\
	case 4:								\
	case 5:								\
		break;							\
	}

static void powerJobOff(void)
{
	powerBbmcOff();
	powerSeqOff();
	powerBbmcOn();
	sub_2A0ED(1);
	if (powerResumed) {
		powerCookieHold();
		POWER_JOB_MODE();
	}
	chargeSetAcDelay();
	sub_2C315(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	sub_2E0D8(2);
	sub_2E165(0);
	if (sub_2C658()->indicatorOn0)
		sub_2DF86(1);
	else
		sub_2DF86(0);
	if (sub_2C658()->indicatorOn1)
		sub_2DFAA(1);
	else
		sub_2DFAA(0);
	sub_2DFF2(0);
	sub_2E016(0);
	sub_2E05E(0);
	powerReady(POWER_JOB_PART);
}

static void powerJobShutdown(void)
{
	powerSeqOff();
	iicaClose();
	chargeTimerStop();
	sub_2E0D8(2);
	sub_2E165(0);
	sub_2DFF2(0);
	sub_2E016(0);
	sub_2E05E(0);
	powerReady(POWER_JOB_PART);
}

static void powerJobSuspend(void)
{
	powerSeqSuspend();
	if (chargeBatteryLevel9()) {
		powerWirelessOff();
		if (!bbmcModeIsNone() || sub_2C658()->suspendCheck)
			powerBbmcCycle();
	}
	POWER_JOB_MODE();
	if (!sub_2E0CF())
		sub_2E0D8(2);
	chargeSetAcDelay();
	sub_2C315(4, 0x20);
	chargeVbus.0 = 1;
	chargeControl.1 = 1;
	powerCookieHold();
	sub_2E05E(0);
	sub_2E165(0);
	powerReady(POWER_JOB_PART);
}

static void powerJobCut(void)
{
	unsigned char reg;
	unsigned char result;

	P0.6 = 0;
	P0.5 = 0;
	P6.3 = 0;
	P13.1 = 0;
	reg = 0xb0;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg0);
	reg = 0xb1;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg1);
	reg = 0xb2;
	result = elmoRead(&reg, (unsigned char *)&powerCutReg2);
	sub_2DF1F();
	iicaClose();
	chargeTimerStop();
	powerReady(POWER_JOB_PART);
}

static void powerJobOffEnd(void)
{
	batteryHibernate();
	chargeTimerStop();
}

static void powerJobUpdate(void)
{
	sub_2DF1F();
	if (sub_2E8B4() & 0x1c)
		sub_2E0D8(1);
	sub_2E499();
	powerReady(POWER_JOB_PART);
}

static void powerJobReset(void)
{
	sub_2E05E(0);
	powerReady(POWER_JOB_PART);
}
