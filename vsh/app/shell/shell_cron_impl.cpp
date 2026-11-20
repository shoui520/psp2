/*
 * shell_cron_impl.cpp: the shell's cron (0x810FB68C-0x810FC9E8). Its entries are kept in a table
 * of SHELL_CRON_ENTRY_MAX; a thread waits on its callbacks: a timer's every half second runs the
 * entries that are due, and the RTC's, after a wake-up, sets the RTC alarm again. The alarm is
 * the earliest time an entry that wakes the system is due; such entries run in a window
 * (15 minutes, 1 with dipsw 186) at an offset picked at random once. No record or string names
 * the file, its functions or its types beyond the thread's and the condition's names: the names
 * are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <kernel/dipsw.h>
#include <libkernel/libkernel.h>
#include <vshbridge/vshbridge.h>
#include <std/stdlib.h>
#include <thread/thread.h>

#include "shell.h"
#include "shell_cron_impl.h"

using namespace paf;

/* (0x810FB68C) */
ShellCronImpl::ShellCronImpl()
	: m_lock("SceShellCronCallback", 0),
	  m_cond(m_lock, "SceShellCronCallbackComp", 0)
{
}

/* (0x810FB6CC, 0x810FB6F8) */
ShellCronImpl::~ShellCronImpl()
{
}

/* (0x810FB72A) */
int ShellCronImpl::Start(void)
{
	SceUInt64 random[8];
	SceKernelSysClock period;
	ShellCronImpl *self;
	int stat;
	int ret;

	if (sceKernelCheckDipsw(SHELL_CRON_DIPSW_TEST))
		m_window = 60ULL * 1000 * 1000;
	else
		m_window = 15ULL * 60 * 1000 * 1000;
	sceKernelGetRandomNumber(random, sizeof(random));
	m_windowOffset = random[0] % m_window;
	sce_paf_printf_dummy();

	m_stop = false;
	m_nextHandle = 1;
	m_count = 0;
	m_alarm = (SceUInt64)-1;
	m_dailyTime = -1;
	m_lastRun = 0;
	ClearAlarm();
	m_tickResolution = sceRtcGetTickResolution();

	m_sema = sceKernelCreateSema("SceShellCronCallback", SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0,
				     0xFFFFFF, NULL);
	if (m_sema < 0)
		return m_sema;
	m_timer = sceKernelCreateTimer("SceShellCronCallback", SCE_KERNEL_EVENT_ATTR_AUTO_RESET, NULL);
	if (m_timer < 0) {
		sceKernelDeleteSema(m_sema);
		return m_timer;
	}
	period.quad = 500 * 1000;
	sceKernelSetTimerEvent(m_timer, SCE_KERNEL_TIMER_TYPE_PULSE_EVENT, &period, 1);

	m_maxIndex = 0;
	for (unsigned int i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
		m_entries[i].flags = 0;
		m_entries[i].func = NULL;
		m_entries[i].arg = NULL;
		m_entries[i].handle = 0;
		m_entries[i].delay = 0;
		m_entries[i].interval = 0;
		m_entries[i].tick = 0;
	}

	m_thread = sceKernelCreateThread("SceShellCronCallback", ThreadEntry, 0x1000010A, 0x4000,
					 0, 0, NULL);
	if (m_thread < 0) {
		sceKernelDeleteSema(m_sema);
		sceKernelDeleteTimer(m_timer);
		return m_thread;
	}

	m_lock.Lock();
	m_threadResult = 0;
	self = this;
	sceKernelStartThread(m_thread, sizeof(self), &self);
	m_cond.Wait(0);
	ret = m_threadResult;
	m_lock.Unlock();
	if (ret < 0) {
		sceKernelWaitThreadEnd(m_thread, &stat, NULL);
		sceKernelDeleteThread(m_thread);
		sceKernelDeleteTimer(m_timer);
	}
	return 0;
}

/* (0x810FB8EC) the thread ended, after the calls in progress */
int ShellCronImpl::Stop(void)
{
	int stat;

	if (m_stop)
		return SHELL_CRON_ERROR_ALREADY_STARTED;
	m_stop = true;
	sceKernelSignalSema(m_sema, 1);
	for (unsigned int i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
		ShellCronEntry *entry = &m_entries[i];

		m_lock.Lock();
		while (entry->handle != 0 && (m_entries[i].flags & SHELL_CRON_RUNNING)) {
			m_entries[i].flags |= SHELL_CRON_REMOVE;
			m_cond.Wait(0);
		}
		m_lock.Unlock();
	}
	sceKernelWaitThreadEnd(m_thread, &stat, NULL);
	return 0;
}

/* (0x810FB992) a free entry with a new handle, NULL when the table is full */
ShellCronEntry *ShellCronImpl::AllocEntry(void)
{
	int count = m_count;
	SceUID handle;
	bool used;
	unsigned int i;

	if (count == SHELL_CRON_ENTRY_MAX)
		return NULL;
	m_count = count + 1;
	do {
		do {
			handle = m_nextHandle++;
			used = false;
		} while (handle == 0);
		for (i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
			ShellCronEntry *entry = &m_entries[i];

			if (entry->handle == handle)
				used = true;
		}
	} while (used);

	for (i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
		ShellCronEntry *entry = &m_entries[i];

		if (entry->handle == 0) {
			entry->handle = handle;
			if (m_maxIndex > i)
				i = m_maxIndex;
			m_maxIndex = i;
			return entry;
		}
	}
	m_count = count;
	return NULL;
}

/* (0x810FBA10) */
int ShellCronImpl::AddInterval(SceUID *handle, ShellCronFunc func, void *arg, unsigned int delay,
			       unsigned int interval, const ShellCronOption *opt)
{
	ShellCronEntry *entry;
	SceRtcTick now;

	m_lock.Lock();
	entry = AllocEntry();
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NO_ENTRY;
	}
	entry->flags = SHELL_CRON_INTERVAL | SHELL_CRON_FIRST;
	entry->func = func;
	entry->arg = arg;
	entry->delay = delay;
	entry->interval = interval;
	if (opt != NULL && opt->wakeUp) {
		entry->flags = SHELL_CRON_INTERVAL | SHELL_CRON_WAKEUP | SHELL_CRON_FIRST;
		static unsigned int s_windowSeconds = m_window / 1000 / 1000;
		entry->interval = (entry->interval + s_windowSeconds - 1) / s_windowSeconds *
				  s_windowSeconds;
	}
	sceRtcGetCurrentTick(&now);
	entry->tick = now.tick + delay * m_tickResolution;
	if (opt != NULL && opt->wakeUp) {
		AlignTick(entry);
		if (entry->tick < m_alarm) {
			SceRtcTick tick;

			tick.tick = entry->tick;
			SetAlarm(&tick);
		}
	}
	if (handle != NULL)
		*handle = entry->handle;
	m_lock.Unlock();
	return 0;
}

/* (0x810FBC1C) */
int ShellCronImpl::AddAt(SceUID *handle, ShellCronFunc func, void *arg, SceUInt64 tick,
			 const ShellCronOption *opt)
{
	ShellCronEntry *entry;

	m_lock.Lock();
	entry = AllocEntry();
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NO_ENTRY;
	}
	entry->flags = SHELL_CRON_AT | SHELL_CRON_FIRST;
	entry->func = func;
	entry->arg = arg;
	entry->delay = 0;
	entry->interval = 0;
	if (opt != NULL) {
		if (opt->wakeUp)
			entry->flags = SHELL_CRON_AT | SHELL_CRON_WAKEUP | SHELL_CRON_FIRST;
		if (opt->persist)
			entry->flags |= SHELL_CRON_PERSIST;
	}
	entry->tick = tick;
	if (opt != NULL && opt->wakeUp) {
		if (entry->tick < m_alarm) {
			SceRtcTick alarm;

			alarm.tick = entry->tick;
			SetAlarm(&alarm);
		}
	}
	if (handle != NULL)
		*handle = entry->handle;
	m_lock.Unlock();
	return 0;
}

/* (0x810FBD0E) a daily entry due on the next of its weekdays from today at its time, after now;
   whether that changed it */
int ShellCronImpl::UpdateDaily(ShellCronEntry *entry, SceUInt64 now, SceUInt64 today)
{
	SceRtcTick day;
	SceRtcTick local;
	SceRtcTick next;
	SceDateTime time;
	SceUInt64 tick;
	unsigned int i;

	if (!(entry->flags & SHELL_CRON_DAILY))
		return 0;
	day.tick = today;
	for (i = 0; ; ) {
		sceRtcConvertUtcToLocalTime(&day, &local);
		sceRtcSetTick(&time, &local);
		if ((1 << sceRtcGetDayOfWeek(time.year, time.month, time.day)) & entry->flags) {
			tick = day.tick + entry->delay * m_tickResolution;
			if (tick > now)
				break;
		}
		sceRtcTickAddDays(&next, &day, 1);
		i++;
		day.tick = next.tick;
		if (i >= 8) {
			sce_paf_printf_dummy();
			entry->tick = (SceUInt64)-1;
			return 0;
		}
	}
	if (entry->tick == tick)
		return 0;
	entry->tick = tick;
	return 1;
}

/* (0x810FBE12) */
int ShellCronImpl::AddDaily(SceUID *handle, ShellCronFunc func, void *arg, int secondOfDay,
			    int dayMask, const ShellCronOption *opt)
{
	ShellCronEntry *entry;
	SceRtcTick now;
	SceRtcTick local;
	SceRtcTick today;
	SceDateTime time;

	if (secondOfDay == m_dailyTime && sceKernelCheckDipsw(SHELL_CRON_DIPSW_TEST))
		return AddInterval(handle, func, arg, 30, 180, opt);
	if (dayMask == 0)
		return SHELL_CRON_ERROR_INVALID_DAYS;

	m_lock.Lock();
	entry = AllocEntry();
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NO_ENTRY;
	}
	entry->flags = dayMask | SHELL_CRON_DAILY | SHELL_CRON_FIRST;
	entry->func = func;
	entry->arg = arg;
	entry->delay = secondOfDay;
	entry->interval = 0;
	if (opt != NULL && opt->wakeUp)
		entry->flags = dayMask | SHELL_CRON_DAILY | SHELL_CRON_WAKEUP | SHELL_CRON_FIRST;
	sceRtcGetCurrentTick(&now);
	sceRtcGetCurrentClockLocalTime(&time);
	time.hour = 0;
	time.minute = 0;
	time.second = 0;
	time.microsecond = 0;
	sceRtcGetTick(&time, &local);
	sceRtcConvertLocalTimeToUtc(&local, &today);
	UpdateDaily(entry, now.tick, today.tick);
	if (opt != NULL && opt->wakeUp) {
		if (entry->tick < m_alarm) {
			SceRtcTick alarm;

			alarm.tick = entry->tick;
			SetAlarm(&alarm);
		}
	}
	if (handle != NULL)
		*handle = entry->handle;
	m_lock.Unlock();
	return 0;
}

/* (0x810FBF84) the RTC alarm set to the earliest entry that wakes the system; a day ahead when
   the RTC refuses one further, and cleared when it fails */
void ShellCronImpl::UpdateAlarm(void)
{
	SceRtcTick now;
	SceRtcTick next;
	bool again;
	int ret;

	do {
		again = false;
		sceRtcGetCurrentTick(&now);
		next.tick = (SceUInt64)-1;
		for (unsigned int i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
			ShellCronEntry *entry = &m_entries[i];

			if (entry->handle != 0 && (entry->flags & SHELL_CRON_WAKEUP)) {
				if (entry->tick < next.tick && entry->tick > now.tick)
					next.tick = entry->tick;
			}
		}
		if (next.tick != (SceUInt64)-1) {
			ret = SetAlarm(&next);
			if (ret != 0) {
				if (ret == SCE_RTC_ERROR_INVALID_VALUE) {
					SceUInt64 max = SHELL_CRON_ALARM_MAX * m_tickResolution;

					if (now.tick < next.tick && next.tick - now.tick >= max) {
						now.tick += max;
						ret = SetAlarm(&now);
						if (ret == SCE_RTC_ERROR_INVALID_VALUE)
							again = true;
					} else {
						again = true;
					}
				}
				if (ret != 0 && ret != SCE_RTC_ERROR_INVALID_VALUE) {
					sce_paf_printf_dummy();
					ClearAlarm();
				}
			}
		} else {
			ClearAlarm();
		}
	} while (again);
}

/* (0x810FC184) */
int ShellCronImpl::Remove(SceUID handle)
{
	ShellCronEntry *entry;
	bool wakeUp;

	m_lock.Lock();
	if (sceKernelGetThreadId() == m_thread) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_IN_CALLBACK;
	}
	entry = FindEntry(handle);
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NOT_FOUND;
	}
	entry->flags |= SHELL_CRON_REMOVE;
	wakeUp = (entry->flags & SHELL_CRON_WAKEUP) != 0;
	while (entry->handle == handle && (entry->flags & SHELL_CRON_RUNNING))
		m_cond.Wait(0);
	if (entry->handle != 0)
		FreeEntry(entry);
	if (wakeUp)
		UpdateAlarm();
	m_lock.Unlock();
	return 0;
}

/* (0x810FC25C) */
int ShellCronImpl::GetRemaining(SceUID handle, int *seconds)
{
	ShellCronEntry *entry;
	SceRtcTick now;
	int ret = 0;
	int remaining;

	m_lock.Lock();
	entry = FindEntry(handle);
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NOT_FOUND;
	}
	sceRtcGetCurrentTick(&now);
	remaining = 0;
	if (now.tick <= entry->tick)
		remaining = (entry->tick - now.tick) / m_tickResolution;
	*seconds = remaining;
	m_lock.Unlock();
	return ret;
}

/* (0x810FC304) */
int ShellCronImpl::GetDailyTime(void)
{
	SceDateTime time;
	int dailyTime;

	m_lock.Lock();
	dailyTime = m_dailyTime;
	if (dailyTime == -1) {
		sceRtcGetCurrentClockLocalTime(&time);
		dailyTime = time.hour;
		if (dailyTime >= 4 && dailyTime < 6) {
			dailyTime = (dailyTime * 60 + time.minute) * 60 + time.second + 30;
		} else {
			sce_paf_srand(time.microsecond);
			dailyTime = sce_paf_rand() % (2 * 60 * 60) + 4 * 60 * 60;
		}
		m_dailyTime = dailyTime;
	}
	m_lock.Unlock();
	return dailyTime;
}

/* (0x810FC39C) */
int ShellCronImpl::MarkRemove(SceUID handle)
{
	ShellCronEntry *entry;

	m_lock.Lock();
	entry = FindEntry(handle);
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NOT_FOUND;
	}
	entry->flags |= SHELL_CRON_REMOVE;
	m_lock.Unlock();
	return 0;
}

/* (0x810FC3F4) */
int ShellCronImpl::IsMarkedRemove(SceUID handle)
{
	ShellCronEntry *entry;
	int marked = 0;

	m_lock.Lock();
	entry = FindEntry(handle);
	if (entry == NULL) {
		m_lock.Unlock();
		return 0;
	}
	if (entry->flags & SHELL_CRON_REMOVE)
		marked = 1;
	m_lock.Unlock();
	return marked;
}

/* (0x810FC446) */
int ShellCronImpl::SetWakeUp(SceUID handle, bool wakeUp)
{
	ShellCronEntry *entry;

	m_lock.Lock();
	entry = FindEntry(handle);
	if (entry == NULL) {
		m_lock.Unlock();
		return SHELL_CRON_ERROR_NOT_FOUND;
	}
	if (wakeUp) {
		entry->flags |= SHELL_CRON_WAKEUP;
		AlignTick(entry);
	} else {
		entry->flags &= ~SHELL_CRON_WAKEUP;
	}
	UpdateAlarm();
	m_lock.Unlock();
	return 0;
}

/* (0x810FC538) the entries that are due called, each with the lock released */
void ShellCronImpl::Run(void)
{
	SceRtcTick now;
	SceRtcTick local;
	SceRtcTick today;
	SceDateTime time;
	bool updateAlarm;

	sceRtcGetCurrentTick(&now);
	m_lastRun = now.tick;
	sceRtcGetCurrentClockLocalTime(&time);
	time.hour = 0;
	time.minute = 0;
	time.second = 0;
	time.microsecond = 0;
	sceRtcGetTick(&time, &local);
	sceRtcConvertLocalTimeToUtc(&local, &today);

	m_lock.Lock();
	updateAlarm = false;
	for (unsigned int i = 0; i < SHELL_CRON_ENTRY_MAX; i++) {
		ShellCronEntry *entry = &m_entries[i];
		SceUInt64 tick;

		if (entry->handle == 0)
			continue;
		tick = entry->tick;
		/* the clock went back: not later than its delay or interval from now */
		if (tick > now.tick) {
			bool wakeUp = false;

			if (entry->flags & SHELL_CRON_WAKEUP)
				wakeUp = true;
			if (entry->flags & SHELL_CRON_INTERVAL) {
				unsigned int seconds = (entry->flags & SHELL_CRON_FIRST) ?
						       entry->delay : entry->interval;
				SceUInt64 next = now.tick + seconds * m_tickResolution;

				if (wakeUp)
					next = (next + m_window - 1) / m_window * m_window +
					       m_windowOffset;
				if (tick > next) {
					entry->tick = next;
					tick = next;
					if (wakeUp)
						updateAlarm = true;
				}
			}
		}
		if (tick <= now.tick) {
			SceUInt32 flags;
			int ret = 0;

			if (entry->flags & SHELL_CRON_WAKEUP)
				updateAlarm = true;
			flags = entry->flags;
			if ((flags & SHELL_CRON_PERSIST) ||
			    now.tick - tick < SHELL_CRON_LATE_MAX * m_tickResolution) {
				entry->flags |= SHELL_CRON_RUNNING;
				if (entry->flags & SHELL_CRON_WAKEUP)
					sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
				m_lock.Unlock();
				ret = entry->func(entry->handle, entry->arg);
				m_lock.Lock();
				entry->bits.running = 0;
				flags = entry->flags;
			}
			entry->bits.first = 0;
			if (ret != 0 || (!(flags & SHELL_CRON_DAILY) && entry->interval == 0)) {
				FreeEntry(entry);
			} else if ((flags & SHELL_CRON_INTERVAL) && entry->interval != 0) {
				do
					entry->tick += entry->interval * m_tickResolution;
				while (entry->tick < now.tick);
			}
			m_cond.NotifyAll();
		}
		if (UpdateDaily(entry, now.tick, today.tick) && (entry->flags & SHELL_CRON_WAKEUP))
			updateAlarm = true;
	}
	if (updateAlarm)
		UpdateAlarm();
	m_lock.Unlock();
}

/* (0x810FC864) the timer's, every half second */
int ShellCronImpl::TimerCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *common)
{
	((ShellCronImpl *)common)->Run();
	return 0;
}

/* (0x810FC870) the RTC's: after a wake-up, the alarm set again */
int ShellCronImpl::RtcCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			       void *common)
{
	ShellCronImpl *cron = (ShellCronImpl *)common;

	if (notifyArg != 0) {
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
		cron->m_lock.Lock();
		cron->UpdateAlarm();
		cron->m_lock.Unlock();
	}
	return 0;
}

/* (0x810FC896) the callbacks made and the timer started, in the cron's thread */
int ShellCronImpl::Setup(void)
{
	SceRtcTick now;
	int ret;

	m_lock.Lock();
	m_timerCallback = sceKernelCreateCallback("SceShellCronCallback", 0, TimerCallback, this);
	if (m_timerCallback < 0) {
		m_cond.Notify();
		ret = m_timerCallback;
		m_lock.Unlock();
		return ret;
	}
	m_rtcCallback = sceKernelCreateCallback("SceShellCronCallback", 0, RtcCallback, this);
	if (m_rtcCallback < 0) {
		sceKernelDeleteCallback(m_timerCallback);
		m_cond.Notify();
		ret = m_rtcCallback;
		m_lock.Unlock();
		return ret;
	}
	ret = vshRtcRegisterCallback(m_rtcCallback);
	if (ret != 0) {
		sceKernelDeleteCallback(m_timerCallback);
		sceKernelDeleteCallback(m_rtcCallback);
		m_cond.Notify();
		m_lock.Unlock();
		return ret;
	}
	ret = sceKernelRegisterCallbackToEvent(m_timer, m_timerCallback);
	if (ret != 0) {
		vshRtcUnregisterCallback(m_rtcCallback);
		sceKernelDeleteCallback(m_timerCallback);
		sceKernelDeleteCallback(m_rtcCallback);
		m_cond.Notify();
		m_lock.Unlock();
		return ret;
	}
	sceRtcGetCurrentTick(&now);
	m_lastRun = now.tick;
	sceKernelStartTimer(m_timer);
	m_cond.Notify();
	m_lock.Unlock();
	return 0;
}

/* (0x810FC9AE) the cron's thread: its callbacks run while it waits */
int ShellCronImpl::ThreadEntry(SceSize args, void *argp)
{
	ShellCronImpl *cron = *(ShellCronImpl **)argp;
	int ret;

	ret = cron->Setup();
	if (ret != 0) {
		cron->m_threadResult = ret;
		return 0;
	}
	do
		sceKernelWaitSemaCB(cron->m_sema, 1, NULL);
	while (!cron->m_stop);
	sceKernelDeleteCallback(cron->m_timerCallback);
	vshRtcUnregisterCallback(cron->m_rtcCallback);
	sceKernelDeleteCallback(cron->m_rtcCallback);
	return 0;
}
