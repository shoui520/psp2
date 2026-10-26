/*
 * The host's watchdog (app/watchdog.c): armed by the host's command 0x0083 and kicked by
 * 0x0085, it runs transition 0x35 when the host stops kicking it.
 */
#ifndef ERNIE_WATCHDOG_H
#define ERNIE_WATCHDOG_H

/* an RTC timer: its ticks are 0.5 s */
#ifdef ERNIE_UPDATER
#define WATCHDOG_TIMER		0x1c
#else
#define WATCHDOG_TIMER		0x1b
#endif

/* the watchdog's setting, as the host sends it */
union Watchdog {
	unsigned int	value;		/* 0: off */
	struct {
		unsigned char	timeout;	/* in seconds; 0: off */
		unsigned char	mode;		/* 0: transition 0x35 at the timeout; else nothing */
	} bytes;
};

extern int watchdogSetup(void);
extern unsigned char watchdogSet(union Watchdog *setting);
extern unsigned char watchdogGet(union Watchdog *setting);
extern unsigned char watchdogStart(void);
extern unsigned char watchdogStop(void);
extern int watchdogKick(void);

#endif
