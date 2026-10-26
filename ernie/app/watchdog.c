/*
 * The host's watchdog: the host arms it (command 0x0083) with a timeout and a mode, and
 * kicks it (0x0085) before the timeout runs out; when it does run out, transition 0x35 is
 * run (in mode 0). At a power-on it takes the setting of the settings block, or keeps the
 * one it had (watchdogKeep).
 */
#include <string.h>

#include "config.h"
#include "state.h"
#include "timer.h"
#include "watchdog.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#endif

static void watchdogExpire(void);

union Watchdog watchdog;

/* a power-on's: the watchdog's setting, and the watchdog started or stopped by it */
int watchdogSetup(void)
{
	struct TimerHandler handler;
	union Watchdog *setting;
	union ConfigWatchdogKeep *keep;
	union PowerRequest *factor;

	factor = stateGetWakeupFactor();
	handler.handler = watchdogExpire;
	timerSetHandler(WATCHDOG_TIMER, &handler);
	keep = &configGet()->watchdogKeep;
	if (!factor->bits.notify && keep->bits.keepOff)
		setting = &watchdog;
	else if (factor->bits.notify && keep->bits.keepNotify)
		setting = &watchdog;
	else
		setting = &configGet()->watchdog;
	watchdogSet(setting);
	if (setting->bytes.timeout != 0)
		return watchdogStart();
	return watchdogStop();
}

/* 0x0083's */
unsigned char watchdogSet(union Watchdog *setting)
{
	memcpy(&watchdog, setting, sizeof(watchdog));
	return 0;
}

/* 0x0013's */
unsigned char watchdogGet(union Watchdog *setting)
{
	*setting = watchdog;
	return 0;
}

unsigned char watchdogStart(void)
{
	if (watchdog.bytes.timeout != 0)
		timerStart(WATCHDOG_TIMER, watchdog.bytes.timeout * 2);
	return 0;
}

/* 0x0084's */
unsigned char watchdogStop(void)
{
	timerStop(WATCHDOG_TIMER);
	return 0;
}

/* 0x0085's, and the host link's at a frame: the timeout starts again */
int watchdogKick(void)
{
	return watchdogStart();
}

#ifdef ERNIE_UPDATER
/* the updater's: transition 0x32 */
static void watchdogExpire(void)
{
	struct StateEvent event;

	switch (watchdog.bytes.mode) {
	case 0:
		event.transition = 0x32;
		updStateEvent(&event);
		break;
	}
}
#else
static void watchdogExpire(void)
{
	switch (watchdog.bytes.mode) {
	case 0:
		stateTransition(0x35);
		break;
	}
}
#endif
