/*
 * shell_cron_impl.h: the shell's cron behind its interface (shell_cron.h): its entries and its
 * class, shared by shell_cron.cpp (the instance) and shell_cron_impl.cpp (the class's code). The
 * names are ours (PLACEHOLDER).
 */
#ifndef SHELL_SHELL_CRON_IMPL_H
#define SHELL_SHELL_CRON_IMPL_H

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <vshbridge/vshbridge.h>
#include <std/stdlib.h>
#include <thread/thread.h>

#include "shell_cron.h"

/* the cron's errors (PLACEHOLDER names) */
#define SHELL_CRON_ERROR_ALREADY_STARTED	0x80102700
#define SHELL_CRON_ERROR_NO_ENTRY		0x80102701
#define SHELL_CRON_ERROR_NOT_FOUND		0x80102702
#define SHELL_CRON_ERROR_IN_CALLBACK		0x80102703
#define SHELL_CRON_ERROR_INVALID_DAYS		0x80102705

/* the entries kept at most */
#define SHELL_CRON_ENTRY_MAX	1232

/* the dipsw that makes the wake-up window a minute, and AddDaily at the daily time a short
   interval */
#define SHELL_CRON_DIPSW_TEST	186

/* an entry's flags: the weekdays of a daily entry in the low bits */
#define SHELL_CRON_DAYS		0x0000FFFF
#define SHELL_CRON_RUNNING	0x00010000	/* its function is being called */
#define SHELL_CRON_FIRST	0x00020000	/* it has not run yet: the delay, not the interval */
#define SHELL_CRON_REMOVE	0x00040000	/* marked to be taken off */
#define SHELL_CRON_WAKEUP	0x00080000	/* the system wakes for it */
#define SHELL_CRON_PERSIST	0x00100000	/* it runs however late it is due */
#define SHELL_CRON_DAILY	0x20000000
#define SHELL_CRON_AT		0x40000000
#define SHELL_CRON_INTERVAL	0x80000000

/* a due entry more than this many seconds late is not called (unless SHELL_CRON_PERSIST) */
#define SHELL_CRON_LATE_MAX	60

/* the RTC alarm is set at most this many seconds ahead when a later one is refused */
#define SHELL_CRON_ALARM_MAX	(24 * 60 * 60)

typedef struct ShellCronEntry {
	union {
		SceUInt32 flags;
		struct {
			SceUInt32 days : 16;
			SceUInt32 running : 1;
			SceUInt32 first : 1;
			SceUInt32 remove : 1;
			SceUInt32 wakeUp : 1;
			SceUInt32 persist : 1;
			SceUInt32 reserved : 8;
			SceUInt32 daily : 1;
			SceUInt32 at : 1;
			SceUInt32 interval : 1;
		} bits;
	};
	ShellCronFunc func;
	void *arg;
	SceUID handle;		/* 0 when the entry is free */
	unsigned int delay;	/* seconds before the first run; a daily entry's second of the day */
	unsigned int interval;	/* seconds between runs; 0 to run once */
	SceUInt64 tick;		/* when it is due */
} ShellCronEntry;

class ShellCronImpl : public ShellCron {
public:
	ShellCronImpl();
	virtual ~ShellCronImpl();

	virtual int AddInterval(SceUID *handle, ShellCronFunc func, void *arg, unsigned int delay,
				unsigned int interval, const ShellCronOption *opt);
	virtual int AddAt(SceUID *handle, ShellCronFunc func, void *arg, SceUInt64 tick,
			  const ShellCronOption *opt);
	virtual int AddDaily(SceUID *handle, ShellCronFunc func, void *arg, int secondOfDay,
			     int dayMask, const ShellCronOption *opt);
	virtual int Remove(SceUID handle);
	virtual int GetRemaining(SceUID handle, int *seconds);
	virtual int GetDailyTime(void);
	virtual int MarkRemove(SceUID handle);
	virtual int IsMarkedRemove(SceUID handle);
	virtual int SetWakeUp(SceUID handle, bool wakeUp);

	int Start(void);
	int Stop(void);
	ShellCronEntry *AllocEntry(void);
	int UpdateDaily(ShellCronEntry *entry, SceUInt64 now, SceUInt64 today);
	void UpdateAlarm(void);
	void Run(void);
	int Setup(void);

	static int TimerCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *common);
	static int RtcCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			       void *common);
	static int ThreadEntry(SceSize args, void *argp);

	ShellCronEntry *FindEntry(SceUID handle)
	{
		for (unsigned int i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
			if (m_entries[i].handle == handle)
				return &m_entries[i];
		}
		return NULL;
	}

	void FreeEntry(ShellCronEntry *entry)
	{
		entry->handle = 0;
		entry->flags = 0;
		entry->delay = 0;
		entry->interval = 0;
		entry->arg = NULL;
		entry->func = NULL;
		m_count--;
		entry->tick = 0;
	}

	/* an interval entry that wakes the system is due in its window, at the cron's offset */
	void AlignTick(ShellCronEntry *entry)
	{
		if ((entry->flags & SHELL_CRON_WAKEUP) && (entry->flags & SHELL_CRON_INTERVAL)) {
			SceUInt64 tick = (entry->tick + m_window - 1) / m_window * m_window +
					 m_windowOffset;

			if (tick - entry->tick > m_window)
				tick -= m_window;
			entry->tick = tick;
		}
	}

	int SetAlarm(SceRtcTick *tick)
	{
		char date[128];
		int ret;

		if (tick->tick == m_alarm)
			return 0;
		ret = vshRtcSetAlarmTick(tick, 1);
		if (ret == 0) {
			m_alarm = tick->tick;
			sceRtcFormatRFC2822LocalTime(date, tick);
			sce_paf_printf_dummy();
		}
		return ret;
	}

	void ClearAlarm(void)
	{
		char date[128];

		vshRtcSetAlarmTick(NULL, 1);
		m_alarm = (SceUInt64)-1;
		sce_paf_printf_dummy();
	}

	SceUInt64 m_window;		/* the wake-up window, in ticks */
	SceUInt64 m_windowOffset;	/* the offset in it, picked at random */
	SceUID m_thread;
	int m_threadResult;		/* the thread's setup */
	SceUID m_timerCallback;
	SceUID m_rtcCallback;
	SceUID m_sema;			/* signalled to end the thread */
	SceUID m_timer;
	SceUInt64 m_tickResolution;
	SceUInt64 m_alarm;		/* the RTC alarm set; -1 for none */
	SceUInt64 m_lastRun;
	bool m_stop;
	int m_dailyTime;		/* GetDailyTime's; -1 before it is picked */
	int m_count;			/* the entries in use */
	SceUID m_nextHandle;
	paf::thread::Mutex m_lock;
	paf::thread::Cond m_cond;		/* a call ended */
	unsigned int m_maxIndex;	/* the highest index used */
	ShellCronEntry m_entries[SHELL_CRON_ENTRY_MAX];
};

#endif
