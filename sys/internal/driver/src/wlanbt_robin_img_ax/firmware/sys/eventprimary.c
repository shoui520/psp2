/*
 * The primary and secondary event paths (0x2ee0..0x345f): entered, polled
 * and left as the sleep votes and the Bluetooth link controller allow, and
 * the FM coexistence path.
 *
 * Marvell published no source; the filename and all names are inferred from
 * what the code does, and the register offsets are the hardware's.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/amp.h"
#include "robin/eventpower.h"
#include "robin/eventroute.h"
#include "robin/hostcmd.h"
#include "robin/conn.h"
#include "robin/keyMgmtAp.h"
#include "robin/mdev.h"
#include "robin/nvram.h"
#include "robin/osa.h"
#include "robin/phy.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlantask.h"
#include "eventpath_regs.h"

/* The BT link controller's wake-up statistics, in the engine's microseconds. */
#define SYS_PRIMARY_WINDOW_DIVISOR	1250
#define SYS_PRIMARY_POLL_US		2000000

/* The primary path: the slots until the Bluetooth link controller needs the radio. */
static uint8_t sysPrimaryMinWindow __attribute__((section(".itcm.rw"))) = 0x15;
static uint32_t sysPrimaryNextBtEvent __attribute__((section(".itcm.rw")));
static uint32_t sysPrimaryBtEventClock __attribute__((section(".itcm.rw")));
static uint32_t sysPrimaryPollClock __attribute__((section(".itcm.rw")));
static uint32_t sysFmCoexPathState __attribute__((section(".itcm.rw")));

/* 0x2ee0: the primary path is entered when its vote allows it. */
void sysEnterPrimaryEvent(void)
{
	UINT posture = sysIrqDisable();

	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH) {
		sysHandlePrimaryEvent();
		sysDelayUs(1000);
		btLtsRomParseState(0);
	}
	sysIrqRestore(posture);
}

/* 0x2f0c */
void sysPrimaryRouteStart(void)
{
	int device = sysEventRouteDevice(0);

	sysEventRouteSource(0);
	sysEventRouteStart(0, device);
}

/* 0x2f28 */
void sysPrimaryRouteResume(void)
{
	UINT posture = sysIrqDisable();

	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_DEVICE)
		sysPrimaryRouteStart();
	sysIrqRestore(posture);
}

/* 0x2f44 */
int sysPrimaryRouteReady(void)
{
	int device = sysEventRouteDevice(0);

	sysEventRouteSource(0);
	return sysEventRouteReady(0, device) != 0;
}

/*
 * 0x2f66: whether the Bluetooth link controller leaves the radio alone long
 * enough; its next activity is measured once and then counted down.
 */
int sysPrimaryWindowOpen(void)
{
	uint32_t remaining;

	if (sysEventAlternateModeIsSet() != 0)
		return sysPrimaryRouteReady() != 0;
	remaining = 0xffffffff;
	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH) {
		if (sysPrimaryNextBtEvent != 0xffffffff) {
			remaining = sysEventClockElapsedUs(sysPrimaryBtEventClock) /
				    SYS_PRIMARY_WINDOW_DIVISOR;
			if (remaining < sysPrimaryNextBtEvent)
				remaining = sysPrimaryNextBtEvent - remaining;
			else
				remaining = 0;
		}
	} else {
		if (btLmpLcBuildContext(&remaining) == 0)
			return 0;
		sysPrimaryNextBtEvent = remaining;
		sysPrimaryBtEventClock = sysEventClockNow();
	}
	if (remaining >= sysPrimaryMinWindow)
		return sysPrimaryRouteReady() != 0;
	return 0;
}

/* 0x2fce */
int sysPrimaryWindowCheck(void)
{
	return (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH) != 0 && sysPrimaryWindowOpen();
}

/* 0x2fe8 */
void sysPrimaryRouteStop(void)
{
	int device = sysEventRouteDevice(0);
	uint32_t source = sysEventRouteSource(0);

	sysEventRouteStop(0, device, source);
}

/* 0x3004 */
int sysPrimaryRouteArmed(void)
{
	return sysPrimaryRouteReady() && btLmpLcManageContext(0) != 0;
}

/* 0x3020 */
void sysPrimaryRouteTryStop(void)
{
	if ((sysSleepVotes & SLEEP_VOTE_PRIMARY_DEVICE) == 0 && btLmpLcActiveCount == 2 &&
	    sysPrimaryWindowCheck() && sysPrimaryRouteArmed())
		sysPrimaryRouteStop();
}

/* 0x3048: the primary path is left. */
void sysLeavePrimaryEvent(void)
{
	sysEventRouteDevice(0);
	sysEventRouteSource(0);
	*(uint16_t *)(btLinkContext + 26) = 23;
	sysEnableEventSources(8);
	sysDisableSecondaryEventMode();
	sysResetSecondaryEventConfiguration();
	SOC_IRQ_ENABLE_CLEAR = SOC_IRQ_PRIMARY_EVENT;
	sysEventPathLeft();
	EVENT_PATH_PENDING_CHANNEL &= ~2u;
	sysSleepAllow(SLEEP_VOTE_PRIMARY_PATH);
}

/* 0x3094: polled by the idle thread: the primary path is left once the window allows it. */
void sysPrimaryEventPoll(void)
{
	UINT posture;

	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH)
		return;
	if (!sysPrimaryWindowOpen()) {
		if (sysEventClockElapsedUs(sysPrimaryPollClock) > SYS_PRIMARY_POLL_US) {
			sysPrimaryPollClock = sysEventClockNow();
			btLmpLcClearState();
		}
	}
	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH)
		return;
	if (sysPrimaryWindowOpen()) {
		posture = sysIrqDisable();
		if (sysPrimaryWindowOpen())
			sysLeavePrimaryEvent();
		sysIrqRestore(posture);
	}
}

/* 0x3108: puts the FM receiver's radio path in the state the secondary path needs. */
void sysFmCoexPathSet(void)
{
	UINT posture = sysIrqDisable();
	uint32_t clock;

	if (sysFmCoexPathState == 0) {
		rfRegBegin();
		rfRegSetBits(31, 134);
		rfRegEnd();
		EVENT_PATH_STATUS = (EVENT_PATH_STATUS | 0x10000000) & ~0x02000000u;
		clock = sysReadEventClock();
		while (sysReadEventClock() == clock)
			;
		EVENT_PATH_STATUS |= 0x12000000;
		EVENT_PATH_CONTROL &= ~8u;
		EVENT_PATH_MEASURE &= ~0x80u;
		rfRegBegin();
		rfRegClearBits(32, 8);
		rfRegEnd();
		sysFmCoexPathState = 1;
	}
	sysIrqRestore(posture);
}

/* 0x3182: the opposite. */
void sysFmCoexPathRestore(void)
{
	UINT posture = sysIrqDisable();
	uint32_t clock;

	if (sysFmCoexPathState == 1) {
		if (!socStrapIsSet()) {
			rfRegBegin();
			rfRegSetBits(32, 8);
			rfRegEnd();
		}
		EVENT_PATH_MEASURE |= 0x80;
		EVENT_PATH_CONTROL &= ~8u;
		EVENT_PATH_STATUS = (EVENT_PATH_STATUS | 0x10000000) & ~0x02000000u;
		clock = sysReadEventClock();
		while (sysReadEventClock() == clock)
			;
		sysDelayUs(10);
		EVENT_PATH_CONTROL |= 8;
		EVENT_PATH_STATUS &= ~0x12000000u;
		rfRegBegin();
		rfRegClearBits(31, 4);
		rfRegEnd();
		sysFmCoexPathState = 0;
	}
	sysIrqRestore(posture);
}

/* 0x3210 */
void sysSecondaryGateOpen(void)
{
	unsigned int device = sysEventRouteDevice(1);

	if ((sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH) && (sysSleepVotes & SLEEP_VOTE_WLAN) && device != 6) {
		sysClearEventFlags(0x8000);
		sysEnableEventSources(0x80000000);
		SOC_EVENT_ENGINE_GATE |= 0x200;
	}
}

/* 0x3246 */
void sysSecondaryWaitAltMode(void)
{
	if (sysEventRouteDevice(1) != 6) {
		while (sysEventAlternateModeIsSet() != 0)
			;
	}
}

/* 0x325c: route 1's device is busy. */
__attribute__((noinline)) void sysEventRoute1Hold(void)
{
	sysSleepForbid(SLEEP_VOTE_SECONDARY_DEVICE);
}

__attribute__((noinline)) void sysEventRoute1Release(void)
{
	sysSleepAllow(SLEEP_VOTE_SECONDARY_DEVICE);
}

/* 0x3270: the secondary path is entered. */
void sysHandleSecondaryEvent(void)
{
	uint32_t state = 0;
	int fmActive;
	int device = sysEventRouteDevice(1);
	int source = sysEventRouteSource(1);

	sysSleepForbid(SLEEP_VOTE_SECONDARY_PATH);
	sysFmCoexPathRestore();
	sysEnableTertiaryEventMode();
	sysDisableEventSources(0x20);
	sysConfigureTertiaryEventPath();
	fmActive = fmCoexPrepareState(&state) ? state != 0 : 0;
	SOC_IRQ_ENABLE_SET = SOC_IRQ_SECONDARY_EVENT;
	sysEventPathEnter(1, device, source);
}

/* 0x32be */
void sysEnterSecondaryEvent(void)
{
	UINT posture = sysIrqDisable();

	if (sysSleepVotes & SLEEP_VOTE_SECONDARY_PATH)
		sysHandleSecondaryEvent();
	sysIrqRestore(posture);
}

/* 0x32dc */
int sysSecondaryRouteReady(void)
{
	int device = sysEventRouteDevice(1);

	sysEventRouteSource(1);
	return sysEventRouteReady(1, device) != 0;
}

/* 0x32fe */
int sysSecondaryReady(void)
{
	uint32_t state;

	return fmCoexValidateState(&state) != 0 && sysSecondaryRouteReady();
}

/* 0x331a */
int sysSecondaryCheck(void)
{
	return (sysSleepVotes & SLEEP_VOTE_SECONDARY_PATH) != 0 && sysSecondaryReady();
}

/* 0x3334 */
void sysSecondaryRouteStop(void)
{
	unsigned int device = sysEventRouteDevice(1);
	uint32_t source = sysEventRouteSource(1);

	if (sysEventRouteDevice(0) != device)
		sysEventRouteStop(1, device, source);
}

/* 0x335c */
void sysSecondaryRouteTryStop(void)
{
	if ((sysSleepVotes & SLEEP_VOTE_SECONDARY_DEVICE) == 0 && sysSecondaryCheck() &&
	    sysSecondaryRouteReady())
		sysSecondaryRouteStop();
}

/* 0x337c */
void sysTertiaryPathRestore(void)
{
	sysRestoreTertiaryEventPath();
	EVENT_PATH_COMMAND &= 0xcc7ffffd;
	EVENT_PATH_FRAME |= 0x1800;
	EVENT_PATH_PENDING_CONTROL = 2 * (EVENT_PATH_PENDING_CONTROL >> 1);
	EVENT_PATH_STATUS &= 0xbffffff5;
}

/* 0x33b0: the secondary path is left. */
void sysLeaveSecondaryEvent(void)
{
	uint32_t state = 0;

	sysEventRouteDevice(1);
	sysEventRouteSource(1);
	sysEnableEventSources(0x20);
	sysDisableTertiaryEventMode();
	SOC_IRQ_ENABLE_CLEAR = SOC_IRQ_SECONDARY_EVENT;
	if (fmCoexPrepareState(&state) != 0) {
		sysTertiaryPathConfigure(state != 0);
	} else {
		sysTertiaryPathRestore();
		if (fmCoexDispatchState() != 0)
			sysFmCoexPathSet();
	}
	sysEventPathLeft();
	sysSleepAllow(SLEEP_VOTE_SECONDARY_PATH);
}

/* 0x3412: polled by the idle thread: the secondary path is left once the FM receiver allows it. */
void sysSecondaryEventPoll(void)
{
	UINT posture;

	if ((sysSleepVotes & SLEEP_VOTE_SECONDARY_PATH) == 0 && sysSecondaryReady()) {
		posture = sysIrqDisable();
		if (sysSecondaryReady())
			sysLeaveSecondaryEvent();
		sysIrqRestore(posture);
	}
}
