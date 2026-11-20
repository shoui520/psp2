/*
 * The shell's cron: entries that call a function after a delay and then at an interval, at a set
 * time, or daily at a time of day on chosen weekdays, run by its thread (strings
 * "SceShellCronCallback", "SceShellCronCallbackComp"). One instance, shellCronGetInstance. The
 * header's, the class's and the methods' names are ours: no record names them (PLACEHOLDER).
 */
#ifndef SHELL_SHELL_CRON_H
#define SHELL_SHELL_CRON_H

#include <scetypes.h>

/* what an entry calls when it is due; an entry whose function fails is taken off */
typedef int (*ShellCronFunc)(SceUID handle, void *arg);

/* the bit-mask of weekdays a daily entry runs on, Sunday first */
#define SHELL_CRON_EVERY_DAY	0x7f

/* an entry's options */
typedef struct ShellCronOption {
	bool wakeUp;		/* the system wakes for it: the earliest such time is set as the RTC alarm */
	bool persist;		/* AddAt only: it runs however late it is due */
} ShellCronOption;

/* the cron's interface (0x810FB640: its constructor and destructor) */
class ShellCron {
public:
	ShellCron();
	virtual ~ShellCron();

	/* after delay seconds, then every interval seconds (once when 0) (0x810FBA10) */
	virtual int AddInterval(SceUID *handle, ShellCronFunc func, void *arg, unsigned int delay,
				unsigned int interval, const ShellCronOption *opt) = 0;
	/* once at tick (0x810FBC1C) */
	virtual int AddAt(SceUID *handle, ShellCronFunc func, void *arg, SceUInt64 tick,
			  const ShellCronOption *opt) = 0;
	/* daily at secondOfDay on the weekdays of dayMask (0x810FBE12) */
	virtual int AddDaily(SceUID *handle, ShellCronFunc func, void *arg, int secondOfDay,
			     int dayMask, const ShellCronOption *opt) = 0;
	/* the entry taken off, after a call in progress ends (0x810FC184) */
	virtual int Remove(SceUID handle) = 0;
	/* the seconds until the entry is due (0x810FC25C) */
	virtual int GetRemaining(SceUID handle, int *seconds) = 0;
	/* the shell's daily time: one picked at random between 4:00 and 6:00, once (0x810FC304) */
	virtual int GetDailyTime(void) = 0;
	/* the entry marked to be taken off (0x810FC39C) */
	virtual int MarkRemove(SceUID handle) = 0;
	/* whether the entry is marked to be taken off (0x810FC3F4) */
	virtual int IsMarkedRemove(SceUID handle) = 0;
	/* whether the system wakes for the entry (0x810FC446) */
	virtual int SetWakeUp(SceUID handle, bool wakeUp) = 0;
};

/* the shell's cron, NULL before it is started (0x810FB632) */
ShellCron *shellCronGetInstance(void);

#endif
