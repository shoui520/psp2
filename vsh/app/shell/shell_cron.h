/*
 * The shell's cron: entries that call a function at a set interval, at a set time, or daily at
 * a time of day on chosen weekdays, run by its thread (strings "SceShellCronCallback",
 * "SceShellCronCallbackComp"). One instance, shellCronGetInstance. The header's, the class's
 * and the methods' names are ours: no record names them.
 */
#ifndef SHELL_SHELL_CRON_H
#define SHELL_SHELL_CRON_H

#include <scetypes.h>

/* what an entry calls when it is due */
typedef int (*ShellCronFunc)(SceUID handle, void *arg);

/* the bit-mask of weekdays a daily entry runs on, Sunday first */
#define SHELL_CRON_EVERY_DAY	0x7f

/* an entry's options */
typedef struct ShellCronOption {
	bool wakeUp;		/* the system wakes for it: the earliest such time is set as the RTC alarm */
	bool persist;		/* AddAt only: sets the entry's 0x100000 bit, read by the cron thread */
} ShellCronOption;

class ShellCron {
public:
	virtual ~ShellCron();

	/* every interval seconds, offset by phase (0x810FBA10) */
	virtual int AddInterval(SceUID *handle, ShellCronFunc func, void *arg,
				unsigned int interval, int phase, const ShellCronOption *opt);
	/* once at tick (0x810FBC1C) */
	virtual int AddAt(SceUID *handle, ShellCronFunc func, void *arg, SceUInt64 tick,
			  const ShellCronOption *opt);
	/* daily at secondOfDay on the weekdays of dayMask (0x810FBE12) */
	virtual int AddDaily(SceUID *handle, ShellCronFunc func, void *arg, int secondOfDay,
			     int dayMask, const ShellCronOption *opt);
	/* the entry taken off, after a call in progress ends (0x810FC184) */
	virtual int Remove(SceUID handle);
	/* the seconds until the entry is due (0x810FC25C) */
	virtual int GetRemaining(SceUID handle, int *seconds);
	/* the shell's daily time: one picked at random between 4:00 and 6:00, once (0x810FC304) */
	virtual int GetDailyTime(void);
	/* the entry marked to be taken off (0x810FC39C) */
	virtual int MarkRemove(SceUID handle);
	/* whether the entry is marked to be taken off (0x810FC3F4) */
	virtual int IsMarkedRemove(SceUID handle);
	/* whether the system wakes for the entry (0x810FC446) */
	virtual int SetWakeUp(SceUID handle, bool wakeUp);
};

/* the shell's cron, NULL before it is made (0x810FB632) */
ShellCron *shellCronGetInstance(void);

#endif
