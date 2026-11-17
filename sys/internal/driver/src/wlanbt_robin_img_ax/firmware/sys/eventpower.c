/*
 * Event paths and sleep votes (0x28cc..0x3540, 0x238d4..0x24334).
 *
 * The chip shares its radio between WLAN, Bluetooth, an AMP link and the FM
 * receiver. Four routes (sys/eventroute.c) bind a device event to an event
 * source; three event paths (primary, secondary, tertiary) hand the engine
 * over between them. Every step that must keep the chip awake clears a bit
 * of sysSleepVotes, every step that finishes sets it again, and the idle
 * thread sleeps only when the votes allow it (sysIdleSleep()).
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

/* The calibration timings, 60 for every reference clock. */
const uint8_t rfCalibTimingA[RF_CALIB_CLOCK_PROFILES] = { [0 ... RF_CALIB_CLOCK_PROFILES - 1] = 60 };	/* 0x53d7e */
const uint8_t rfCalibTimingC[RF_CALIB_CLOCK_PROFILES] = { [0 ... RF_CALIB_CLOCK_PROFILES - 1] = 60 };	/* 0x53d88 */
const uint8_t rfCalibTimingB[RF_CALIB_CLOCK_PROFILES] = { [0 ... RF_CALIB_CLOCK_PROFILES - 1] = 60 };	/* 0x53d92 */

/* Registers of the event paths; the names say what the code does with them. */
#define EVENT_PATH_PENDING_TIMER	SOC_REG(0x040)
#define EVENT_PATH_PENDING_CHANNEL	SOC_REG(0x044)	/* bit 1: a path was entered */
#define EVENT_PATH_PENDING_PARAM	SOC_REG(0x048)
#define EVENT_PATH_PENDING_CONFIG	SOC_REG(0x04c)	/* bit 16: the engine runs */
#define EVENT_PATH_PENDING_CONTROL	SOC_REG(0x060)
#define EVENT_PATH_EVENT		SOC_REG(0x09c)
#define EVENT_PATH_CHANNEL		SOC_REG(0x130)
#define EVENT_PATH_STATUS		SOC_REG(0x140)
#define EVENT_PATH_BUS_CLOCK		SOC_REG(0x144)	/* low nibble: bus clock divider */
#define EVENT_PATH_COUNTER		SOC_REG(0x180)
#define EVENT_PATH_RATE			SOC_REG(0x188)
#define EVENT_PATH_KEY			SOC_REG(0x18c)
#define EVENT_PATH_COMMAND		SOC_REG(0x1a8)
#define EVENT_PATH_REGISTER		SOC_REG(0x200)
#define EVENT_PATH_RECORD		SOC_REG(0x204)
#define EVENT_PATH_FRAME		SOC_REG(0x250)
#define EVENT_PATH_CONTROL		SOC_CONTROL		/* bit 3 */
#define EVENT_PATH_MEASURE		SOC_SLEEP_MEASURE_CONTROL	/* bit 7 */

#define SOC_IRQ_PRIMARY_EVENT		0x200000
#define SOC_IRQ_SECONDARY_EVENT		0x8000000
#define SOC_IRQ_MIDDLE_EVENT		0x100

/* The BT link controller's wake-up statistics, in the engine's microseconds. */
#define SYS_PRIMARY_WINDOW_DIVISOR	1250
#define SYS_PRIMARY_POLL_US		2000000

uint8_t sysEventEngineFlags __attribute__((section(".itcm.rw"))) = SYS_ENGINE_EVENT_PATHS;
uint8_t sysSleepShortWake __attribute__((section(".itcm.rw")));
uint32_t sysSleepVotes __attribute__((section(".itcm.rw")));
/* Time the idle thread spent waiting, once the host asked for the load (nonzero). */
uint64_t sysIdleTime __attribute__((section(".itcm.rw")));
uint64_t sysIdleStart __attribute__((section(".itcm.rw")));

struct SysEventSourceHandlerEntry sysEventSourceHandlers[3] __attribute__((section(".itcm.rw"))) = {
	{ 12, NULL },
	{ 11, NULL },
	{ 10, NULL },
};

/* The primary path: the slots until the Bluetooth link controller needs the radio. */
static uint8_t sysPrimaryMinWindow __attribute__((section(".itcm.rw"))) = 0x15;
static uint32_t sysPrimaryNextBtEvent __attribute__((section(".itcm.rw")));
static uint32_t sysPrimaryBtEventClock __attribute__((section(".itcm.rw")));
static uint32_t sysPrimaryPollClock __attribute__((section(".itcm.rw")));
static uint32_t sysFmCoexPathState __attribute__((section(".itcm.rw")));
/* Bit 2 of radio register 18, read once; 0xff until then. */
static uint8_t sysTertiaryRadioFlag __attribute__((section(".itcm.rw"))) = 0xff;

/* 0x28cc */
void sysProcessEventQueues(uint32_t events)
{
	sysSleepForbid(SLEEP_VOTE_WLAN | SLEEP_VOTE_BT);
	sysClearEventEngineTrigger();
	if (events & 0x10)
		wlanMacRestart();
	else
		wlanMacSleepExit();
}

/* Routes 0 to 3 hold the chip awake while their device is busy and let go again. */
void sysEventRoute2Hold(void)
{
	sysProcessEventQueues(0);
}

void sysEventRoute2Release(void)
{
}

/* 0x28f0: a path was entered for a device event routed from a source. */
void sysEventPathEnter(int route, uint32_t device, uint32_t source)
{
	if (device == 2 || device == 3 || device == 4 || device == 5)
		SOC_IRQ_ENABLE_SET = SOC_IRQ_MIDDLE_EVENT;
	if ((source == 4 || source == 5 || source == 6 || source == 7) && route == 0)
		btLmpLcFormatContext(0, 0, 10, 0);
}

void sysEventPathLeft(void)
{
}

/* 0x293e: every route bound to the device event holds the chip awake. */
void sysEventRoutesHold(uint32_t device)
{
	if (sysEventRouteDevice(0) == device)
		sysEventRoute0Hold();
	if (sysEventRouteDevice(1) == device)
		sysEventRoute1Hold();
	if (sysEventRouteDevice(2) == device)
		sysEventRoute2Hold();
	if (sysEventRouteDevice(3) == device)
		sysEventRoute3Hold();
}

/* 0x297c */
void sysEventRoutesRelease(uint32_t device)
{
	if (sysEventRouteDevice(0) == device)
		sysEventRoute0Release();
	if (sysEventRouteDevice(1) == device)
		sysEventRoute1Release();
	if (sysEventRouteDevice(2) == device)
		sysEventRoute2Release();
	if (sysEventRouteDevice(3) == device)
		sysEventRoute3Release();
}

/* 0x29ba: whether the route may stop (a device it serves is idle). */
int sysEventRouteReady(unsigned int route, uint32_t device)
{
	if (device == 4 || device == 5 || device == 2 || device == 3) {
		if (sysEventRouteIdle(route) == 0)
			return 0;
		return 1;
	}
	return 1;
}

/* 0x29dc: stops a route that serves one of the device events 2 to 5. */
void sysEventRouteStop(unsigned int route, uint32_t device, uint32_t source)
{
	if (device == 2 || device == 3 || device == 4 || device == 5)
		sysEventRouteConfigure(route, device, source);
}

/* 0x29f4 */
void sysEventRouteStart(int route, uint32_t device)
{
	if (device == 2 || device == 3 || device == 4 || device == 5)
		sysEventDeviceActivate(route, device);
}

/*
 * 0x2aa4: binds a device event to a channel of its route; the value goes
 * into the route's flags above the bound and masked bits.
 */
int sysEventRouteBind(unsigned int device, unsigned int channel, unsigned int flags)
{
	unsigned int index;
	struct SysEventRoute *route;

	if (channel >= 12 || device >= 9)
		return 0;
	for (index = 0; index < SYS_EVENT_ROUTE_COUNT; ++index) {
		route = &sysEventRoutes[index];
		if (route->device != device)
			continue;
		if ((route->flags & SYS_EVENT_ROUTE_BOUND) && route->channel != channel)
			return 0;
		if (route->channel == channel ||
		    !sysEventRouteReleaseChannel(index, device, route->channel))
			continue;
		route->channel = channel;
		route->flags = (route->flags & 7) | (flags << 3);
		if (!sysEventRouteAcquireChannel(index, device, channel)) {
			route->channel = 0xff;
			route->flags &= 7;
			return 0;
		}
		route->flags |= SYS_EVENT_ROUTE_BOUND;
	}
	return 1;
}

/* 0x2b82: an event source fired; its device event is dispatched. */
void sysDispatchEventSource(uint32_t index, uint32_t sources, uint32_t flags)
{
	uint32_t mapping;

	sysSetEventFlags(flags);
	sysDisableEventSources(sources);
	mapping = sysGetEventSourceMapping(index);
	if (mapping == 2 || mapping == 3 || mapping == 4 || mapping == 5)
		sysDispatchMiddleEventBits(sources);
	else
		sysEventSourceCallHandler(sources);
}

/*
 * 0x2c1c: a device event routed to source 4 to 7 acknowledges the event of
 * its source; unless it is dispatched from the interrupt, the routes that
 * serve it hold the chip awake.
 */
void sysHandleMappedEventSource(uint32_t device, uint32_t dispatch)
{
	unsigned int route = sysEventRouteFind(device);
	unsigned int source = sysEventRouteSourceOf(device);
	unsigned int channel = sysEventRouteChannelOf(device);

	if (route < 2 || route == 3) {
		switch (source) {
		case 4:
			sysSetEventFlags(0x10);
			sysAcknowledgeEvents(0x100000);
			break;
		case 5:
			sysSetEventFlags(0x20);
			sysAcknowledgeEvents(0x200000);
			break;
		case 6:
			sysSetEventFlags(0x40);
			sysAcknowledgeEvents(0x400000);
			break;
		case 7:
			sysSetEventFlags(0x80);
			sysAcknowledgeEvents(0x800000);
			break;
		default:
			break;
		}
	}
	if (dispatch == 0 && sysEventRouteState(device) != 0) {
		if (channel == 8)
			gpioPinWrite(sysEventRoutes[route].flags >> 3, 0);
		else if (channel == 9)
			gpioPinWrite(16, 0);
	}
	sysEventRoutesHold(device);
}

/* 0x2cc2: called, never inlined (sysEventRouteStart) */
__attribute__((noinline)) void sysEventDeviceActivate(int route, uint32_t device)
{
	(void)route;
	sysHandleMappedEventSource(device, 0);
}

/* 0x2d48: the counterpart of sysHandleMappedEventSource() for a stopped route. */
void sysEventRouteConfigure(unsigned int route, int device, int source)
{
	unsigned int channel = sysEventRouteChannelOf(device);

	if (route < 2 || route == 3) {
		switch (source) {
		case 4:
			sysClearEventFlags(0x10);
			sysSetSecondaryEventFlags(0x10);
			sysEnableEventSources(0x100000);
			break;
		case 5:
			sysClearEventFlags(0x20);
			sysSetSecondaryEventFlags(0x20);
			sysEnableEventSources(0x200000);
			break;
		case 6:
			sysClearEventFlags(0x40);
			sysSetSecondaryEventFlags(0x40);
			sysEnableEventSources(0x400000);
			break;
		case 7:
			sysClearEventFlags(0x80);
			sysSetSecondaryEventFlags(0x80);
			sysEnableEventSources(0x800000);
			break;
		default:
			break;
		}
	}
	if (channel == 8)
		gpioPinWrite(sysEventRoutes[route].flags >> 3, 1);
	else if (channel == 9)
		gpioPinWrite(16, 1);
	sysEventRoutesRelease(device);
}

/* 0x2dea: the SDIO devices of a route have nothing pending. */
int sysEventRouteIdle(unsigned int route)
{
	int result = 0;

	if (route != 2) {
		if (route <= 1) {
			if (mdevIoctl(btSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0 ||
			    mdevIoctl(btSdioDev, 8, 0) != 0 || btHccRomHandleState() == 0)
				return 0;
			result = 1;
		}
		if (route != 3)
			return result;
		if (mdevIoctl(ampSdioDev, SDIO_IOCTL_HOST_PENDING, 0) != 0 ||
		    mdevIoctl(ampSdioDev, 8, 0) != 0 || !ampHciIsIdle())
			return 0;
	}
	return 1;
}

/* 0x2e64: route 0's device is busy. */
void sysEventRoute0Hold(void)
{
	sysSleepForbid(SLEEP_VOTE_PRIMARY_DEVICE);
	if (sysEventAlternateModeIsSet() != 0) {
		sysSecondaryGateOpen();
		sysSecondaryWaitAltMode();
	}
	while (rfBlock4Get() == 0)
		clockSourceMode();
	btLmpLcFormatContext(0, 0, 8, 0);
}

void sysEventRoute0Release(void)
{
	sysSleepAllow(SLEEP_VOTE_PRIMARY_DEVICE);
}

/* 0x2ea2: the primary path is entered. */
void sysHandlePrimaryEvent(void)
{
	int device = sysEventRouteDevice(0);
	int source = sysEventRouteSource(0);

	sysSleepForbid(SLEEP_VOTE_PRIMARY_PATH);
	EVENT_PATH_PENDING_CHANNEL |= 2;
	SOC_IRQ_ENABLE_SET = SOC_IRQ_PRIMARY_EVENT;
	sysEnableSecondaryEventMode();
	sysDisableEventSources(8);
	sysEventPathEnter(0, device, source);
}

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
		return sysPrimaryRouteReady();
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
		return sysPrimaryRouteReady();
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
		EVENT_PATH_STATUS = (EVENT_PATH_STATUS & 0xedffffff) | 0x10000000;
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
		EVENT_PATH_STATUS = (EVENT_PATH_STATUS & 0xedffffff) | 0x10000000;
		clock = sysReadEventClock();
		while (sysReadEventClock() == clock)
			;
		sysDelayUs(10);
		EVENT_PATH_CONTROL |= 8;
		EVENT_PATH_STATUS &= 0xedffffff;
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
void sysEventRoute1Hold(void)
{
	sysSleepForbid(SLEEP_VOTE_SECONDARY_DEVICE);
}

void sysEventRoute1Release(void)
{
	sysSleepAllow(SLEEP_VOTE_SECONDARY_DEVICE);
}

/* 0x3270: the secondary path is entered. */
void sysHandleSecondaryEvent(void)
{
	uint32_t state = 0;
	int device = sysEventRouteDevice(1);
	int source = sysEventRouteSource(1);

	sysSleepForbid(SLEEP_VOTE_SECONDARY_PATH);
	sysFmCoexPathRestore();
	sysEnableTertiaryEventMode();
	sysDisableEventSources(0x20);
	sysConfigureTertiaryEventPath();
	fmCoexPrepareState(&state);
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
		sysTertiaryPathConfigure();
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

/* 0x3460 */
void sysTertiaryPathConfigure(void)
{
	sysConfigureTertiaryEventPath();
	EVENT_PATH_COMMAND = (EVENT_PATH_COMMAND & 0xcffffffd) | 0x3800000;
	EVENT_PATH_FRAME &= 0xffffc7fe;
	if (!socStrapIsSet())
		EVENT_PATH_PENDING_CONTROL |= 0x10001;
	if (sysTertiaryRadioFlag == 0xff) {
		rfRegBegin();
		sysTertiaryRadioFlag = rfRegRead(18) & 4;
		rfRegEnd();
	}
	if (sysTertiaryRadioFlag == 0)
		EVENT_PATH_PENDING_CONTROL |= 0x10001;
	EVENT_PATH_STATUS = (EVENT_PATH_STATUS & 0x9ffffff0) + 0x4000000e;
}

/* 0x351c */
__attribute__((noinline)) int sysEnableEventSources(uint32_t sources)
{
	SOC_EVENT_SOURCE_ENABLE |= sources;
	SOC_EVENT_SECONDARY_ENABLE |= sources;
	return 1;
}

/* 0x2431c: route 3's device is busy. */
void sysEventRoute3Hold(void)
{
	sysSleepForbid(SLEEP_VOTE_TERTIARY_DEVICE);
	btLmpLcFormatContext(1, 0, 8, 0);
}

void sysEventRoute3Release(void)
{
	sysSleepAllow(SLEEP_VOTE_TERTIARY_DEVICE);
}

/* 0x2433c */
void sysTertiaryRouteStart(void)
{
	int device = sysEventRouteDevice(3);

	sysEventRouteSource(3);
	sysEventRouteStart(3, device);
}

/* 0x24358: the tertiary route is started when its vote allows it. */
void sysEnterTertiaryEvent(void)
{
	UINT posture = sysIrqDisable();

	if (sysSleepVotes & SLEEP_VOTE_TERTIARY_DEVICE)
		sysTertiaryRouteStart();
	sysIrqRestore(posture);
}

/* 0x24374 */
int sysTertiaryRouteReady(void)
{
	int device = sysEventRouteDevice(3);

	sysEventRouteSource(3);
	return sysEventRouteReady(3, device) != 0;
}

/* 0x24396 */
void sysTertiaryRouteStop(void)
{
	int device = sysEventRouteDevice(3);
	uint32_t source = sysEventRouteSource(3);

	sysEventRouteStop(3, device, source);
}

/* 0x243b2 */
int sysTertiaryRouteArmed(void)
{
	return sysTertiaryRouteReady() && btLmpLcManageContext(1) != 0;
}

/* 0x243ce */
void sysTertiaryRouteTryStop(void)
{
	if ((sysSleepVotes & SLEEP_VOTE_TERTIARY_DEVICE) == 0 && sysTertiaryRouteArmed())
		sysTertiaryRouteStop();
}

/* 0x238d4: one wait for an interrupt of the idle thread, timed once the host asked for the load. */
void sysIdleWait(void)
{
	uint64_t start = 0;

	if (sysIdleTime != 0)
		start = osaGetHardwareTime();
	serialBusIdleEnter();
	robinCpuWaitForInterrupt();
	serialBusIdleLeave();
	if (sysIdleTime != 0)
		sysIdleTime += osaGetHardwareTime() - start;
}

/* 0x2391e: waits for an interrupt with the event engine's secondary and tertiary engines running. */
static __attribute__((noinline)) void sysIdleDeepSleep(uint32_t macState)
{
	const uint32_t sources = 0x10000;	/* the engine-running bit of the pending configuration */

	sysSetEventSleepRequest();
	SOC_IRQ_ENABLE_SET = 0x400000;
	sysEnableEventSources(0x8000);
	sysStartSecondaryEventEngine();
	sysStartTertiaryEventEngine();
	sysWaitForEventSleepHandshake();
	EVENT_PATH_PENDING_CONFIG &= ~sources;
	if (sysEventEngineFlags & SYS_ENGINE_EVENT_PATHS) {
		if (macState != 14) {
			SOC_EVENT_SECONDARY_CONFIGURATION |= 1;
			sysClearEventFlags(0x80);
			sysEnableEventSources(sources << 7);
			SOC_EVENT_LATCH = 3;
			sysDelayUs(100);
		}
		if ((SOC_EVENT_STATE & 0xe) == macState)
			robinCpuWaitForInterrupt();
		SOC_EVENT_LATCH = 0;
	} else {
		robinCpuWaitForInterrupt();
	}
	EVENT_PATH_PENDING_CONFIG |= sources;
	sysStopSecondaryEventEngine();
	sysStopTertiaryEventEngine();
	sysClearEventSleepRequest();
}

/*
 * 0x239a8: the idle thread's decision. The chip sleeps when the primary,
 * secondary and tertiary paths have all given up the radio; otherwise it
 * only waits for an interrupt.
 */
uint32_t sysIdleSleep(void)
{
	int primary = 0, secondary = 0, wlan = 0, tertiary = 0;
	uint32_t actual, enabled, macState, interval;
	UINT posture;

	posture = sysIrqDisable();
	sysPrimaryRouteTryStop();
	sysSecondaryRouteTryStop();
	sysTertiaryRouteTryStop();
	if (sysSleepVotes & SLEEP_VOTE_PRIMARY_DEVICE)
		primary = (sysSleepVotes & SLEEP_VOTE_PRIMARY_PATH) != 0;
	if ((sysSleepVotes & SLEEP_VOTE_SECONDARY_DEVICE) && (sysSleepVotes & SLEEP_VOTE_SECONDARY_PATH))
		secondary = 1;
	if ((sysSleepVotes & SLEEP_VOTE_BT) && (sysSleepVotes & SLEEP_VOTE_WLAN))
		wlan = 1;
	if ((sysSleepVotes & SLEEP_VOTE_TERTIARY_DEVICE) && (sysSleepVotes & SLEEP_VOTE_WLAN_ENGINE))
		tertiary = 1;
	macState = SOC_EVENT_STATE & 0xe;
	if ((sysEventEngineFlags & SYS_ENGINE_EVENT_PATHS) == 0) {
		if (!wlan)
			goto wait;
	} else {
		if ((macState & 4) == 0)
			primary = 1;
		if ((macState & 8) == 0)
			tertiary = 1;
		if ((macState & 4) == 0)
			secondary = 1;
		if (macState & 2) {
			if (sysForceEventDispatch != 0)
				goto wait;
			if (!wlan)
				goto wait;
		} else if (!wlan) {
			if (wlanConnectionExists(4) != 0)
				goto wait;
			osaEventFlagsWait(wlanHostSleepFlags, 1, 8, &actual, 0);
			if (sleepEnter(NULL, 0, 0, 0, 1) != 1)
				goto wait;
		}
	}
	if (!primary || !secondary || !tertiary)
		goto wait;
	enabled = SOC_IRQ_ENABLE_SET;
	SOC_IRQ_ENABLE_CLEAR = 0xffffffff;
	interval = 16000;
	if (clockSlowestRate() <= 0x3e80)
		interval = clockSlowestRate();
	clockRequestCore(interval, 4);
	sysIdleDeepSleep(macState);
	SOC_IRQ_ENABLE_SET = enabled;
	goto done;
wait:
	sysIdleWait();
done:
	sysIrqRestore(posture);
	return clockRequestCore(0, 4);
}

/* 0x23ab0: the idle thread's pass. */
unsigned int powerIdleStep(void)
{
	sysPrimaryEventPoll();
	sysSecondaryEventPoll();
	return sysIdleSleep();
}

/* 0x23ac0 */
int powerIdlePrepare(void)
{
	UINT posture = sysIrqDisable();

	sysPathPresetsInit();
	sysEventRoutesAdjust();
	return sysIrqRestore(posture);
}

/* 0x23ad8: the routes of the four device events. */
void sysEventRoutesInit(void)
{
	sysEventRoutes[0].device = 3;
	sysEventRoutes[0].source = 5;
	sysEventRoutes[1].device = 3;
	sysEventRoutes[1].source = 5;
	sysEventRoutes[2].device = 2;
	sysEventRoutes[2].source = 4;
	sysEventRoutes[3].device = 4;
	sysEventRoutes[3].source = 6;
}

/* 0x23af8 */
uint32_t sysEventEngineSleepPeriod(void)
{
	if (sleepWakeTimingKept() != 0)
		return 1999999;
	return 15679;
}

/* 0x23b0a: the bus clock in Hz. */
uint32_t sysGetBusClock(void)
{
	uint32_t clock = rfCalibrationTable[sysRefClockProfile()] * 100000;

	switch (EVENT_PATH_BUS_CLOCK & 0xf) {
	case 0:
		return clock >> 1;
	case 1:
		return clock >> 2;
	case 4:
	case 5:
		return clock >> 3;
	case 6:
	case 7:
		return clock >> 4;
	default:
		return clock;
	}
}

/* 0x23b40: the event engine's primary timing for the bus clock. */
uint32_t sysEventEnginePollTicks(void)
{
	uint32_t clock = sysGetBusClock();

	switch (clock) {
	case 12000000:
		return 5460;
	case 13000000:
		return 5040;
	case 19200000:
		return 3412;
	case 24000000:
		return 2730;
	case 26000000:
		return 2520;
	case 38400000:
		return 1706;
	case 52000000:
		return 1259;
	default:
		break;
	}
	if (clock != 9600000)
		return 3412;
	return 6826;
}

/* 0x23b9a: programs the event engine's timing from the bus clock. */
void sysEventEngineInit(void)
{
	uint32_t upper, lower, secondaryUpper, period;

	if ((uint8_t)SOC_CHIP_REVISION == 64)
		sysEventEngineFlags &= ~SYS_ENGINE_EVENT_PATHS;
	sysSleepVotes = 0;
	sysInitializeEventRegisters();
	upper = (uint16_t)(romMul32x32(sysGetBusClock(), rfCalibTimingA[sysRefClockProfile()]) /
			   1000000 + 1);
	lower = (uint16_t)(romMul32x32(sleepClockRateGet(), rfCalibrationTable[10 + sysRefClockProfile()]) /
			   1000000 + 1);
	sysSetEventPrimaryTiming(lower, upper);
	secondaryUpper = (uint16_t)(romMul32x32(sysGetBusClock(), rfCalibTimingB[sysRefClockProfile()]) /
				    1000000 + 1);
	lower = (uint16_t)(romMul32x32(sysGetBusClock(), rfCalibTimingC[sysRefClockProfile()]) /
			   1000000 + 1);
	sysSetEventSecondaryTiming(lower, secondaryUpper);
	sysSetEventTimingControl(8);
	sysSetEventClockPeriod((uint16_t)(sysGetBusClock() / 1000000 + 1));
	sysSetEventControlByte((uint8_t)(romMul32x32(sysGetBusClock(), 5) / 3000000 + 25));
	sysSetEventFlags(0xffffffff);
	sysResetSecondaryEventConfiguration();
	sysEnableSecondaryEventMode();
	sysConfigureTertiaryEventPath();
	sysEnableTertiaryEventMode();
	sysResetEventEngineConfiguration();
	sysEnableEventEngineMode();
	period = sysEventEngineSleepPeriod();
	sysConfigureEventEngineTiming(sysEventEnginePollTicks(), period);
	sysConfigureSecondaryEventEngine();
	sysEventRoutesInit();
	sysEventEngineInitHook();
}

/* 0x23d54: all the votes are cast, that is the chip may sleep as far as they go. */
__attribute__((noinline)) int sysSleepVotesAllowed(uint32_t votes)
{
	return (votes & ~sysSleepVotes) == 0;
}

/* 0x23d64: the votes allow the chip to sleep. */
void sysSleepAllow(uint32_t votes)
{
	UINT posture = sysIrqDisable();
	uint32_t held;

	sysSleepVotes |= votes;
	held = sysSleepVotes;
	if ((votes & SLEEP_VOTE_WLAN) || (votes & SLEEP_VOTE_BT)) {
		if (held & SLEEP_VOTE_WLAN) {
			if (held & SLEEP_VOTE_BT) {
				sysStartEventEngine();
				SOC_EVENT_CONTROL &= ~0x80000048u;
				SOC_EVENT_PATH_CONTROL &= 0xffdfff7f;
				if (sysSleepShortWake == 0 || (SOC_EVENT_STATE & 4) != 0 ||
				    !sysSleepVotesAllowed(1) || !sysSleepVotesAllowed(4))
					clockHostSetMin();
				sysSleepShortWake = 0;
			} else {
				sysStartEventEngine();
			}
		}
		if (votes & SLEEP_VOTE_WLAN)
			sysSleepVotes |= SLEEP_VOTE_WLAN_ENGINE;
	}
	sysIrqRestore(posture);
}

/* 0x23dea: the votes keep the chip awake. */
void sysSleepForbid(uint32_t votes)
{
	UINT posture = sysIrqDisable();
	uint32_t held;

	sysSleepVotes &= ~votes;
	held = sysSleepVotes;
	if ((votes & SLEEP_VOTE_WLAN) || (votes & SLEEP_VOTE_BT)) {
		if ((held & SLEEP_VOTE_WLAN) == 0 || (held & SLEEP_VOTE_BT) == 0) {
			SOC_EVENT_CONTROL |= 0x80000048;
			sysStopEventEngineAndWait();
			SOC_EVENT_PATH_CONTROL |= 0x200080;
			clockHostApplyTarget();
		}
		if (votes & SLEEP_VOTE_WLAN)
			sysSleepVotes &= ~SLEEP_VOTE_WLAN_ENGINE;
	}
	sysIrqRestore(posture);
}

/* 0x23e3e: the share of the time since the host asked for it that the idle thread waited, in percent. */
uint32_t sysIdlePercent(void)
{
	uint64_t elapsed = osaGetHardwareTime() - sysIdleStart;
	uint64_t idle = sysIdleTime;
	uint32_t percent = romMul64x32(idle, 100) / elapsed;

	if (idle == 0) {
		sysIdleTime = 1;
		sysIdleStart = osaGetHardwareTime() - 1;
	}
	return percent;
}

/* 0x23e90 */
void sysIdleStatsReset(void)
{
	sysIdleTime = 0;
	sysIdleStart = 0;
}

/* 0x23ea0 */
uint32_t sysEventClockNow(void)
{
	return sysReadEventClock();
}

/* 0x23ea8: microseconds since the event clock read the value. */
uint32_t sysEventClockElapsedUs(uint32_t since)
{
	uint32_t elapsed = sysReadEventClock() - since;
	uint32_t frequency = sleepClockRateGet();

	return romMul32x32(elapsed, 1000000) / frequency;
}

/* 0x23ecc: sets the handler of an event source that has none bound to a device event. */
int sysSetEventSourceHandler(uint32_t source, SysEventSourceHandler handler)
{
	unsigned int index;

	if (sysGetEventSourceMapping(source) != 255)
		return 0;
	if (source != 10 && source != 11) {
		if (source != 12)
			return 0;
		gpioPinSelect(4, 0);
		PERIPH_REG(0x214) &= ~0x10u;
		PERIPH_REG(0x218) &= ~0x10u;
	}
	for (index = 0; sysEventSourceHandlers[index].source != source; ) {
		if (++index >= 3)
			return 1;
	}
	sysEventSourceHandlers[index].handler = handler;
	return 1;
}

/* 0x23f24 */
int sysEventSourceReset(uint32_t source)
{
	if (source == 12) {
		SOC_EVENT_STATUS &= ~0x10000000u;
		sysClearEventFlags(0x1000);
		sysClearSecondaryEventFlags(0x1000);
		return sysEnableEventSources(0x10000000);
	}
	return source;
}

/* 0x23f54: source 12's handler is called when the event of its source arrives. */
void sysEventSourceCallHandler(uint32_t sources)
{
	if (sources & 0x10000000) {
		if (sysEventSourceHandlers[0].handler != NULL)
			sysEventSourceHandlers[0].handler();
	}
}

/*
 * The radio's frequency plan presets (0x54d30): the registers the plan
 * touches are saved before the first preset is applied and restored with the
 * last one. The words are indexed by the radio configuration.
 */
static struct {
	uint32_t saved;			/* SOC_EVENT_PATH_CONTROL-side register before sysPathPresetSave() */
	uint32_t savedValid;
	uint32_t ready;			/* sysPathPresetsInit() ran */
	uint32_t base[10];		/* applied when PERIPH_REG(0xc018) bit 5 is clear */
	uint32_t alternate[10];		/* applied when it is set; zero where there is none */
} sysPathPresets __attribute__((section(".itcm.rw"))) = {
	0, 0, 0,
	{
	0, 0,
	0x6978d52c, 0x6f7e3d2c,
	0x786c2224, 0x6f28bd24,
	0x4b439624, 0x4840e124,
	0x41af5924, 0x37945f24,
	},
	{
	0, 0,
	0x5c28f62c, 0x62b6ae2c,
	0x7ced912c, 0x78fd5224,
	0x51ebe524, 0x4ea4a924,
	0x477e6b24, 0x3c7ea924,
	},
};

#define EVENT_PATH_PRESET_SOURCE	PERIPH_REG(0xc018)

/* 0x23f84 */
void sysPathPresetsInit(void)
{
	sysPathRegistersInit();
	sysPathPresets.ready = 1;
}

/*
 * 0x23fea: quiesces the registers of an event channel. Returns whether the
 * channel is one the engine runs itself (1) or not (0).
 */
int sysChannelQuiesce(int channel)
{
	if ((unsigned int)(channel - 11) <= 4) {
		EVENT_PATH_PENDING_PARAM &= ~2u;
		SOC_EVENT_ENGINE_GATE &= ~0x1000u;
		EVENT_PATH_RATE &= ~0x100000u;
		EVENT_PATH_KEY &= ~1u;
		EVENT_PATH_RECORD &= ~0x2000u;
		EVENT_PATH_REGISTER |= 0xf800;
	} else if ((unsigned int)(channel - 20) <= 3) {
		SOC_EVENT_ENGINE_GATE &= ~0x1000u;
		EVENT_PATH_RATE &= ~0x100000u;
		EVENT_PATH_KEY &= ~1u;
		EVENT_PATH_RECORD &= ~0x1000u;
		EVENT_PATH_REGISTER |= 0xf00000;
	} else if (channel != 4 && channel != 18) {
		return 0;
	}
	return 1;
}

/* Clears the channel's bit in the interrupt sources of the peripheral block. */
static inline __attribute__((always_inline)) void sysChannelClearSources(unsigned int channel)
{
	if (channel < 16) {
		PERIPH_REG(0x214) &= ~(1u << channel);
		PERIPH_REG(0x218) &= ~(1u << channel);
	} else {
		PERIPH_REG(0x270) &= ~(1u << (channel & 15));
		PERIPH_REG(0x274) &= ~(1u << (channel & 15));
	}
}

/* 0x2406e: releases the source's channel, and takes its calibration table again. */
int sysEventRouteChannelUpdate(int route, int device, int source, int hold)
{
	unsigned int channel;

	if (source == 8) {
		if (hold != 0)
			return 1;
		channel = sysEventRoutes[route].flags >> 3;
		if (!sysChannelQuiesce(channel))
			return 0;
		gpioPinWrite(channel, 1);
		gpioPinSelect(channel, 1);
		sysChannelClearSources(channel);
	} else if (source == 9 && hold == 0) {
		gpioPinWrite(16, 1);
		gpioPinSelect(16, 1);
		PERIPH_REG(0x270) &= ~1u;
		PERIPH_REG(0x274) &= ~1u;
	}
	return 1;
}

/* 0x240fc */
int sysEventRouteAcquireChannel(int route, int device, int source)
{
	return sysEventRouteChannelUpdate(route, device, source, 1);
}

/* 0x24100 */
int sysEventRouteReleaseChannel(int route, int device, int source)
{
	return sysEventRouteChannelUpdate(route, device, source, 0);
}

/* 0x24104 */
int sysChannelReset(unsigned int channel)
{
	if (!sysChannelQuiesce(channel))
		return 0;
	gpioPinWrite(channel, 1);
	gpioPinSelect(channel, 1);
	sysChannelClearSources(channel);
	return 1;
}

/* 0x24152: nothing to do after the engine's routes are set up. */
void sysEventEngineInitHook(void)
{
}

/* 0x24296: a frequency plan preset exists for the radio configuration. */
__attribute__((noinline)) int sysPathPresetSelect(void)
{
	if (sysPathPresets.ready == 0)
		return 0;
	return sysPathPresets.alternate[sysRefClockProfile()] != 0;
}

/* 0x24154: applies the preset, when there is one. */
int sysPathPresetApply(void)
{
	const uint32_t *table;

	if (!sysPathPresetSelect())
		return 0;
	if (EVENT_PATH_PRESET_SOURCE & 0x20)
		table = sysPathPresets.alternate;
	else
		table = sysPathPresets.base;
	EVENT_PATH_KEY = table[sysRefClockProfile()] | (EVENT_PATH_KEY & 0xc3);
	return 1;
}

/* 0x2418a */
int sysPathPresetClear(void)
{
	EVENT_PATH_KEY &= ~4u;
	return sysPathPresetSelect() != 0;
}

/* 0x241a2: takes the preset, remembering the register it changes. */
int sysPathPresetSave(void)
{
	UINT posture = sysIrqDisable();

	if (!sysPathPresetApply()) {
		EVENT_PATH_KEY &= ~0x20u;
		if (sysPathPresets.savedValid == 0)
			sysPathPresets.saved = EVENT_PATH_EVENT;
		EVENT_PATH_EVENT |= 0xc06c0005;
		sysPathPresets.savedValid = 1;
	}
	return sysIrqRestore(posture);
}

/* 0x241e0: gives the register back. */
int sysPathPresetRestore(void)
{
	UINT posture = sysIrqDisable();

	if (!sysPathPresetClear()) {
		uint32_t value = (EVENT_PATH_EVENT | 0x40240000) & 0x7fb7ffff;

		if (sysPathPresets.savedValid != 0)
			value = (value & ~5u) | (sysPathPresets.saved & 5);
		EVENT_PATH_EVENT = value;
		sysPathPresets.savedValid = 0;
	}
	return sysIrqRestore(posture);
}

/* 0x24220: the path registers' state when the engine starts. */
void sysPathRegistersInit(void)
{
	uint32_t value;

	EVENT_PATH_PENDING_CHANNEL |= 2;
	EVENT_PATH_PENDING_CONTROL = (EVENT_PATH_PENDING_CONTROL & 0xcfffce00) | 0x200;
	value = SOC_EVENT_PATH_CONTROL & 0xaaba7aff;
	if (SOC_CHIP_REVISION & 0xff)
		value &= 0xffefffef;
	SOC_EVENT_PATH_CONTROL = value;
	if ((SOC_CHIP_REVISION & 0xff) && sysReadEventClock() < 0x200)
		EVENT_PATH_EVENT = (EVENT_PATH_EVENT | 1) & ~4u;
	else
		EVENT_PATH_EVENT &= ~1u;
	EVENT_PATH_STATUS &= 0xbfaaaab5;
	SOC_EVENT_PATH_CONTROL &= ~5u;
	EVENT_PATH_CHANNEL &= ~0x3ffu;
}

/* 0x242ec: the source's bits of an event route are reset (only the event source 12's are). */
__attribute__((noinline)) void sysEventRouteAdjust(int route, int device, int source, int flag)
{
	if (source != 12 || flag != 0)
		return;
	gpioPinSelect(4, 0);
	PERIPH_REG(0x214) &= ~0x10u;
	PERIPH_REG(0x218) &= ~0x10u;
}

/* 0x23f92: the three routes of the paths are reset to the sources the device events map to. */
void sysEventRoutesAdjust(void)
{
	unsigned int device0 = sysEventRouteDevice(0);
	unsigned int device1 = sysEventRouteDevice(1);
	unsigned int device2 = sysEventRouteDevice(2);
	unsigned int source0 = sysEventRouteSource(0);
	unsigned int source1 = sysEventRouteSource(1);
	unsigned int source2 = sysEventRouteSource(2);

	sysEventRouteAdjust(0, device0, source0, 0);
	sysEventRouteAdjust(1, device1, source1, 0);
	sysEventRouteAdjust(2, device2, source2, 0);
}
