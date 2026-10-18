/*
 * SceRtc: the real time clock.
 *
 * The syscon keeps a counter of half seconds and the tick it started from
 * (RTC base, stored as tick >> 19). The current tick is the system time
 * plus an offset fixed at synchronisation. Eight auxiliary clocks (secure,
 * network, GPS, ...) are kept the same way, each as an offset to the RTC
 * base (saved in the syscon) and to the system time.
 */

#include <sysmem/cpu.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sysmem/dipsw.h>
#include <syscon/syscon.h>
#include <acmgr/acmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include <sysmem/suspend.h>
#include <sysmem/sysevent.h>
#include <touch_dummy/touch_dummy.h>
#include "rtc_internal.h"
#include <kernel/error.h>
#include <threadmgr/threadmgr.h>

static int rtcSysEventHandler(int resume, int eventid, void *args, void *opt);
static int rtcInitWork(void *argp);

RtcState g_rtc;

static inline void rtcPackTick(SceUInt8 *packed, SceUInt64 tick)
{
	packed[0] = tick >> 19;
	packed[1] = tick >> 27;
	packed[2] = tick >> 35;
	packed[3] = tick >> 43;
	packed[4] = tick >> 51;
}

/* name unknown: alarm of an aux clock reached */
static SceUInt32 rtcAuxAlarmHandler(void *argp)
{
	RtcAuxClock *aux = argp;

	if (aux->callback > 0) {
		if (sceKernelNotifyCallback(aux->callback, 1) < 0)
			aux->callback = -1;
	}
	return 0;
}

/* name unknown: syscon alarm event */
static void rtcSysconAlarmHandler(int arg)
{
	if (g_rtc.alarmCallback < 0)
		return;
	if (g_rtc.alarmState == 0) {
		if (sceKernelNotifyCallback(g_rtc.alarmCallback, arg) < 0)
			g_rtc.alarmCallback = -1;
	} else if (g_rtc.alarmState == 1) {
		g_rtc.alarmState = 2;
	}
}

/* name unknown: (re)arms the alarm of an aux clock */
static int rtcSetAuxAlarm(unsigned int index, const SceRtcTick *tick)
{
	RtcAuxClock *aux;
	SceUInt64 now, delay;
	unsigned int context;
	SceUID alarm;

	if (index >= RTC_AUX_CLOCKS)
		return SCE_RTC_ERROR_INVALID_VALUE;
	context = rtcEnter();
	aux = &g_rtc.aux[index];
	if (!(aux->flags & RTC_AUX_VALID)) {
		rtcLeave(context);
		return SCE_RTC_ERROR_NOT_INITIALIZED;
	}
	now = aux->sysOffset.tick + sceKernelGetSystemTimeWide();
	rtcLeave(context);
	if (aux->alarm > 0) {
		alarm = aux->alarm;
		aux->alarm = -1;
		sceKernelCancelAlarm(alarm);
	}
	if (tick == NULL) {
		aux->alarmTick.tick = 0;
	} else {
		aux->alarmTick.tick = tick->tick;
		if (aux->alarmTick.tick != 0) {
			if (now >= aux->alarmTick.tick) {
				if (aux->callback > 0) {
					if (sceKernelNotifyCallback(aux->callback, 1) < 0)
						aux->callback = -1;
				}
				return 0;
			}
			if (aux->callback > 0) {
				if (sceKernelNotifyCallback(aux->callback, 0) < 0)
					aux->callback = -1;
			}
			delay = aux->alarmTick.tick - now;
			alarm = SceThreadmgrForDriver_10517330("SceRtcAuxAlarm", &delay,
							       rtcAuxAlarmHandler, aux);
			if (alarm < 0)
				return alarm;
			aux->alarm = alarm;
			return 0;
		}
	}
	if (aux->callback > 0) {
		if (sceKernelNotifyCallback(aux->callback, 0) < 0)
			aux->callback = -1;
	}
	return 0;
}

int module_start(SceSize args, void *argp)
{
	int i;

	memset(&g_rtc, 0, sizeof(g_rtc));
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		g_rtc.aux[i].callback = -1;
		g_rtc.aux[i].alarm = -1;
	}
	g_rtc.alarmCallback = -1;
	rtcRegisterLibcTime();
	SceSysconForDriver_8351526D(rtcSysconAlarmHandler, NULL);
	sceKernelRegisterSysEventHandler("SceRtc", rtcSysEventHandler, NULL);
	sceKernelScheduleWorkTask(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceRtcInit", rtcInitWork, NULL);
	return 0;
}

/* name unknown */
int SceRtcForDriver_ABA035B7(void)
{
	g_rtc.initialized = 0;
	g_rtc.aux[0].flags = 0;
	g_rtc.aux[1].flags = 0;
	g_rtc.aux[2].flags = 0;
	g_rtc.aux[3].flags = 0;
	g_rtc.aux[4].flags = 0;
	g_rtc.aux[5].flags = 0;
	g_rtc.aux[6].flags = 0;
	g_rtc.aux[7].flags = 0;
	return 0;
}

int sceRtcSynchronize(void)
{
	SceRtcTick last, tick, aux[RTC_AUX_CLOCKS];
	SceUInt64 rtcBase, now;
	SceKernelIntrStatus intr;
	SceUInt32 count;
	int i, ret;

	if (!g_rtc.initialized)
		return 0;
	last = g_rtc.lastSetTick;
	ret = SceSysconForDriver_D7BEFF8B(&count);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 698);
		return ret;
	}
	rtcBase = (SceUInt64)count * RTC_COUNT_UNIT;
	now = sceKernelGetSystemTimeWide();
	SceRtcForDriver_CE51C2A1(&tick, &last, rtcBase);
	SceRtcForDriver_CE51C2A1(&tick, &tick, -now);
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		aux[i] = g_rtc.aux[i].sysOffset;
		if (g_rtc.aux[i].flags) {
			SceRtcForDriver_CE51C2A1(&aux[i], &g_rtc.aux[i].rtcOffset, rtcBase);
			SceRtcForDriver_CE51C2A1(&aux[i], &aux[i], -now);
		}
	}
	intr = SceCpuForDriver_821FC0EE();
	g_rtc.rtcBase = rtcBase;
	g_rtc.systemTimeBase = now;
	g_rtc.tickOffset = tick;
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		if (g_rtc.aux[i].flags) {
			g_rtc.aux[i].sysOffset = aux[i];
			rtcSetAuxAlarm(i, &g_rtc.aux[i].alarmTick);
		}
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* where the suspend handler leaves the clock for the resume code */
typedef struct RtcSuspendSave {
	SceUInt8 reserved[0x50];		/* other modules' parts */
	SceRtcTick tick;
	SceRtcTick alarmTick;
} RtcSuspendSave;

typedef struct RtcSuspendArgs {
	SceUInt32 reserved[4];
	RtcSuspendSave *save;
} RtcSuspendArgs;

/* name unknown */
static int rtcSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	RtcSuspendSave *save;

	if (resume == 0) {
		if (eventid == SYSEVENT_SUSPEND_PREPARE(0)) {
			g_rtc.alarmState = 1;
		} else if (eventid == SYSEVENT_SUSPEND_COMMIT) {
			save = ((RtcSuspendArgs *)args)->save;
			ksceRtcGetCurrentTick(&save->tick);
			ksceRtcGetAlarmTick(&save->alarmTick, NULL);
		}
	} else if (resume == 1) {
		if (eventid == SYSEVENT_RESUME_DEVICE(10)) {
			sceRtcSynchronize();
		} else if (eventid == SYSEVENT_RESUME_START) {
			if (g_rtc.alarmState != 1) {
				if (sceKernelNotifyCallback(g_rtc.alarmCallback, ksceRtcIsAlarmed()) < 0)
					g_rtc.alarmCallback = -1;
			}
			g_rtc.alarmState = 0;
		}
	}
	return 0;
}

int ksceRtcSetConf(unsigned int dateFormat, unsigned int timeFormat, int timezone,
		   unsigned int daylightSaving)
{
	SceKernelIntrStatus intr;

	if (dateFormat > 2)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (timeFormat > 1)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if ((unsigned int)(timezone + 1439) > 2878)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (daylightSaving > 1440)
		return SCE_RTC_ERROR_INVALID_VALUE;
	intr = SceCpuForDriver_821FC0EE();
	g_rtc.dateFormat = dateFormat;
	g_rtc.timeFormat = timeFormat;
	g_rtc.daylightSaving = daylightSaving;
	g_rtc.timezone = timezone;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

static inline __attribute__((always_inline))
int _sceRtcSetCurrentAuxTick(int index, const SceRtcTick *tick, int cmd)
{
	RtcAuxClock *aux = &g_rtc.aux[index];
	SceUInt64 now, rtcBase, systemTimeBase, offset;
	SceKernelIntrStatus intr;
	unsigned int context;
	int ret;

	context = rtcEnter();
	SceKernelSuspendForDriver_4DF40893(0);
	now = sceKernelGetSystemTimeWide();
	rtcBase = g_rtc.rtcBase;
	systemTimeBase = g_rtc.systemTimeBase;
	intr = SceCpuForDriver_821FC0EE();
	if (tick == NULL) {
		aux->flags = 0;
		aux->rtcOffset.tick = 0;
		aux->sysOffset.tick = 0;
	} else {
		aux->rtcOffset.tick = tick->tick + systemTimeBase - rtcBase - now;
		aux->sysOffset.tick = tick->tick - now;
		__sync_synchronize();
		aux->flags = RTC_AUX_VALID | RTC_AUX_SET;
	}
	offset = aux->rtcOffset.tick;
	sceKernelCpuResumeIntr(intr);
	rtcPackTick(g_rtc.auxPacked[index], offset);
	ret = ksceSysconSendCommand(cmd, g_rtc.auxPacked[index], 5);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 1026);
		SceKernelSuspendForDriver_2BB92967(0);
		rtcLeave(context);
		return ret;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	rtcLeave(context);
	return 0;
}

int ksceRtcSetCurrentSecureTick(const SceRtcTick *tick)
{
	if (tick != NULL && tick->tick <= RTC_TICK_2014_END)
		return SCE_RTC_ERROR_INVALID_VALUE;
	return _sceRtcSetCurrentAuxTick(RTC_AUX_SECURE, tick,
					SYSCON_SCRATCH_PAD(auxTick[RTC_AUX_SECURE]));
}

int ksceRtcSetCurrentNetworkTick(const SceRtcTick *tick)
{
	if (tick != NULL && tick->tick <= RTC_TICK_2014_END)
		return SCE_RTC_ERROR_INVALID_VALUE;
	return _sceRtcSetCurrentAuxTick(RTC_AUX_NETWORK, tick,
					SYSCON_SCRATCH_PAD(auxTick[RTC_AUX_NETWORK]));
}

int ksceRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick)
{
	return _sceRtcSetCurrentAuxTick(RTC_AUX_DEBUG_NETWORK, tick,
					SYSCON_SCRATCH_PAD(auxTick[RTC_AUX_DEBUG_NETWORK]));
}

int sceRtcSetAlarmTick(const SceRtcTick *tick, int arg)
{
	SceKernelIntrStatus intr;
	unsigned int context;
	SceRtcTick now;
	SceUInt64 count;
	int alarm, ret;

	context = rtcEnter();
	if (tick == NULL || tick->tick == 0 || g_rtc.lastSetTick.tick == 0) {
		alarm = -1;
	} else {
		ksceRtcGetCurrentTick(&now);
		if (tick->tick < now.tick) {
			if (rtcContext() & 0x10000) {
				rtcLeave(context);
				return SCE_RTC_ERROR_INVALID_VALUE;
			}
			alarm = 0;
		} else {
			if ((rtcContext() & 0x10000) && tick->tick - now.tick > 86399999999ULL) {
				rtcLeave(context);
				return SCE_RTC_ERROR_INVALID_VALUE;
			}
			count = __aeabi_uldivmod(tick->tick - g_rtc.lastSetTick.tick, RTC_COUNT_UNIT);
			if (count <= 0xfffffeff)
				alarm = count + 1;
			else
				alarm = -1;
		}
	}
	g_rtc.alarmState = 1;
	ret = SceSysconForDriver_51164951(alarm, arg);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 1124);
		g_rtc.alarmState = 0;
		rtcLeave(context);
		return ret;
	}
	intr = SceCpuForDriver_821FC0EE();
	g_rtc.alarmArg = arg;
	if (tick == NULL)
		g_rtc.alarmTick.tick = 0;
	else
		g_rtc.alarmTick = *tick;
	if (g_rtc.alarmState != 1) {
		if (sceKernelNotifyCallback(g_rtc.alarmCallback, ksceRtcIsAlarmed()) < 0)
			g_rtc.alarmCallback = -1;
	}
	g_rtc.alarmState = 0;
	sceKernelCpuResumeIntr(intr);
	rtcLeave(context);
	return 0;
}

int sceRtcSetCurrentTick(const SceRtcTick *tick)
{
	SceKernelIntrStatus intr;
	SceUInt64 now, delta;
	unsigned int context;
	SceUInt8 base[8];
	SceUInt32 count;
	int i, ret;

	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	context = rtcEnter();
	SceKernelSuspendForDriver_4DF40893(0);
	g_rtc.alarmState = 1;
	intr = SceCpuForDriver_821FC0EE();
	now = sceKernelGetSystemTimeWide();
	g_rtc.tickOffset.tick = tick->tick - now;
	delta = now + (g_rtc.rtcBase - g_rtc.systemTimeBase);
	g_rtc.systemTimeBase = now;
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		if (g_rtc.aux[i].flags)
			g_rtc.aux[i].rtcOffset.tick += delta;
	}
	g_rtc.initialized = 1;
	g_rtc.lastSetTick = *tick;
	g_rtc.rtcBase = 0;
	sceKernelCpuResumeIntr(intr);
	ret = SceSysconForDriver_D7BEFF8B(&count);
	if (ret < 0)
		SCE_DBG_ASSERT_CHECK(0, 832);
	ret = SceSysconForDriver_51164951(-1, 0);
	if (ret < 0)
		SCE_DBG_ASSERT_CHECK(0, 840);
	ret = SceSysconForDriver_9B6A6F64(0);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 847);
		goto fail;
	}
	g_rtc.rtcCarry += count;
	ret = ksceSysconSendCommand(SYSCON_SCRATCH_PAD(powerOnTime), &g_rtc.rtcCarry, 4);
	if (ret < 0)
		SCE_DBG_ASSERT_CHECK(0, 859);
	ret = ksceSysconReadCommand(SYSCON_SCRATCH_PAD(currentTick), base, sizeof(base));
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 868);
		goto fail;
	}
	rtcPackTick(base, tick->tick);
	ret = ksceSysconSendCommand(SYSCON_SCRATCH_PAD(currentTick), base, sizeof(base));
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 888);
		goto fail;
	}
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		if (g_rtc.aux[i].flags)
			rtcPackTick(g_rtc.auxPacked[i], g_rtc.aux[i].rtcOffset.tick);
	}
	ret = ksceSysconSendCommand(SYSCON_SCRATCH_PAD(auxTick[0]), g_rtc.auxPacked[0], 20);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 912);
		goto fail;
	}
	ret = ksceSysconSendCommand(SYSCON_SCRATCH_PAD(auxTick[4]), g_rtc.auxPacked[4], 20);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 921);
		goto fail;
	}
	ret = sceRtcSetAlarmTick(&g_rtc.alarmTick, g_rtc.alarmArg);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 932);
		goto fail;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (g_rtc.alarmState != 1) {
		if (sceKernelNotifyCallback(g_rtc.alarmCallback, ksceRtcIsAlarmed()) < 0)
			g_rtc.alarmCallback = -1;
	}
	g_rtc.alarmState = 0;
	sceKernelCpuResumeIntr(intr);
	SceKernelSuspendForDriver_2BB92967(0);
	rtcLeave(context);
	return 0;

fail:
	g_rtc.alarmState = 0;
	SceKernelSuspendForDriver_2BB92967(0);
	rtcLeave(context);
	return ret;
}

int sceRtcSynchronizeCold(void)
{
	RtcAuxClock aux[RTC_AUX_CLOCKS];
	SceRtcTick tick, offset, alarm;
	SceUInt64 raw, rtcBase, now;
	SceKernelIntrStatus intr;
	SceUInt32 count, alarmRaw, alarmArg, dipsw;
	SceDateTime time;
	SceUInt8 *packed;
	int i, ret;

	memset(aux, 0, sizeof(aux));
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		aux[i].alarmTick = g_rtc.aux[i].alarmTick;
		aux[i].callback = g_rtc.aux[i].callback;
		aux[i].alarm = g_rtc.aux[i].alarm;
	}
	ret = ksceSysconReadCommand(SYSCON_SCRATCH_PAD(powerOnTime), &g_rtc.rtcCarry, 4);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 476);
		return ret;
	}
	ret = ksceSysconReadCommand(SYSCON_SCRATCH_PAD(currentTick), &raw, 8);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 484);
		return ret;
	}
	raw &= 0xffffffffffULL;
	dipsw = 0;
	if (raw == 0) {
		for (i = 0; i < 32; i++) {
			if (SceDipswForDriver_A98FC2FD(i))
				dipsw |= 1 << i;
		}
		g_rtc.initialized = 0;
		for (i = 0; i < RTC_AUX_CLOCKS; i++)
			g_rtc.aux[i].flags = 0;
		if (dipsw == 0)
			return 0;
		SceRtcForDriver_DE9F4515(&time, dipsw);
		SceRtcForDriver_F139E359(&time, &tick);
		if (SceSysconForDriver_D7BEFF8B(&count) < 0)
			SCE_DBG_ASSERT_CHECK(0, 521);
		else
			tick.tick += (SceUInt64)count * RTC_COUNT_UNIT;
		sceRtcSetCurrentTick(&tick);
	} else {
		tick.tick = raw << 19;
	}
	ret = SceSysconForDriver_D7BEFF8B(&count);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 537);
		return ret;
	}
	rtcBase = (SceUInt64)count * RTC_COUNT_UNIT;
	now = sceKernelGetSystemTimeWide();
	SceRtcForDriver_CE51C2A1(&offset, &tick, rtcBase);
	SceRtcForDriver_CE51C2A1(&offset, &offset, -now);
	ret = ksceSysconReadCommand(SYSCON_SCRATCH_PAD(auxTick[0]), g_rtc.auxPacked[0], 20);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 557);
		return ret;
	}
	ret = ksceSysconReadCommand(SYSCON_SCRATCH_PAD(auxTick[4]), g_rtc.auxPacked[4], 20);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 563);
		return ret;
	}
	for (i = 0; i < RTC_AUX_CLOCKS; i++) {
		packed = g_rtc.auxPacked[i];
		raw = (SceUInt32)(packed[0] | packed[1] << 8 | packed[2] << 16 | packed[3] << 24) |
		      (SceUInt64)packed[4] << 32;
		aux[i].flags = g_rtc.aux[i].flags;
		if (raw != 0) {
			aux[i].rtcOffset.tick = raw << 19;
			SceRtcForDriver_CE51C2A1(&aux[i].sysOffset, &aux[i].rtcOffset, rtcBase);
			SceRtcForDriver_CE51C2A1(&aux[i].sysOffset, &aux[i].sysOffset, -now);
			__sync_synchronize();
			aux[i].flags |= RTC_AUX_VALID;
			if (i < 4 && aux[i].sysOffset.tick <= RTC_TICK_2014_END)
				raw = 0;
			if (raw != 0)
				continue;
		}
		aux[i].rtcOffset.tick = 0;
		aux[i].flags = 0;
		aux[i].sysOffset.tick = 0;
		if (dipsw != 0 && i == RTC_AUX_2) {
			aux[i].rtcOffset = tick;
			aux[i].sysOffset = offset;
			__sync_synchronize();
			aux[i].flags = RTC_AUX_VALID;
			rtcPackTick(g_rtc.auxPacked[i], tick.tick);
			ret = ksceSysconSendCommand(SYSCON_SCRATCH_PAD(auxTick[RTC_AUX_2]),
						    g_rtc.auxPacked[i], 5);
			if (ret < 0) {
				SCE_DBG_ASSERT_CHECK(0, 623);
				return ret;
			}
		}
	}
	ret = SceSysconForDriver_3168F3AF(&alarmRaw, &alarmArg);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 636);
		return ret;
	}
	if (alarmRaw == 0xffffffff) {
		alarm.tick = 0;
	} else {
		SceRtcForDriver_9C8AF264(&alarm, &tick, alarmRaw >> 1);
		if (alarmRaw & 1)
			SceRtcForDriver_CE51C2A1(&alarm, &alarm, RTC_COUNT_UNIT);
	}
	intr = SceCpuForDriver_821FC0EE();
	g_rtc.rtcBase = rtcBase;
	g_rtc.tickOffset = offset;
	g_rtc.systemTimeBase = now;
	g_rtc.lastSetTick = tick;
	memcpy(g_rtc.aux, aux, sizeof(aux));
	g_rtc.alarmArg = alarmArg;
	g_rtc.alarmTick = alarm;
	__sync_synchronize();
	g_rtc.initialized = 1;
	for (i = 0; i < RTC_AUX_CLOCKS; i++)
		rtcSetAuxAlarm(i, &aux[i].alarmTick);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int ksceRtcGetAlarmTick(SceRtcTick *tick, int *arg)
{
	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	*tick = g_rtc.alarmTick;
	if (arg != NULL)
		*arg = g_rtc.alarmArg;
	return 0;
}

int ksceRtcIsAlarmed(void)
{
	unsigned int context;
	SceRtcTick now;
	int ret;

	if (g_rtc.alarmTick.tick == 0)
		return 0;
	context = rtcEnter();
	ret = SceSysconForDriver_86BAAF7D();
	if (ret == 1) {
		ksceRtcGetCurrentTick(&now);
		ret = now.tick >= g_rtc.alarmTick.tick;
	}
	rtcLeave(context);
	return ret;
}

int ksceRtcRegisterCallback(SceUID cb)
{
	SceKernelIntrStatus intr;
	unsigned int context;
	int ret;

	context = rtcEnter();
	intr = SceCpuForDriver_821FC0EE();
	if (g_rtc.alarmCallback >= 0) {
		ret = SCE_RTC_ERROR_ALREADY_REGISTERD;
	} else {
		ret = sceKernelNotifyCallback(cb, ksceRtcIsAlarmed());
		if (ret >= 0) {
			g_rtc.alarmCallback = cb;
			ret = 0;
		}
	}
	sceKernelCpuResumeIntr(intr);
	rtcLeave(context);
	return ret;
}

int ksceRtcUnregisterCallback(SceUID cb)
{
	unsigned int context;
	int ret;

	context = rtcEnter();
	if (g_rtc.alarmCallback == cb) {
		g_rtc.alarmCallback = -1;
		ret = 0;
	} else {
		ret = SCE_RTC_ERROR_NOT_FOUND;
	}
	rtcLeave(context);
	return ret;
}

int ksceRtcGetCurrentTick(SceRtcTick *tick)
{
	unsigned int context;
	int ret;

	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	context = rtcEnter();
	if (g_rtc.initialized) {
		tick->tick = g_rtc.tickOffset.tick + sceKernelGetSystemTimeWide();
		ret = 0;
	} else {
		tick->tick = RTC_TICK_1980;
		ret = SCE_RTC_ERROR_NOT_INITIALIZED;
	}
	rtcLeave(context);
	return ret;
}

/* name unknown */
static int rtcInitWork(void *argp)
{
	ksceSysconWaitInitialized();
	sceRtcSynchronizeCold();
	sceRtcSetAlarmTick(NULL, 0);
	return 1;
}

static inline int rtcGetAuxTick(int index, SceRtcTick *tick)
{
	unsigned int context;
	int ret;

	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	context = rtcEnter();
	if (g_rtc.aux[index].flags & RTC_AUX_VALID) {
		tick->tick = g_rtc.aux[index].sysOffset.tick + sceKernelGetSystemTimeWide();
		ret = 0;
	} else {
		tick->tick = RTC_TICK_1980;
		ret = SCE_RTC_ERROR_NOT_INITIALIZED;
	}
	rtcLeave(context);
	return ret;
}

int ksceRtcGetCurrentSecureTick(SceRtcTick *tick)
{
	return rtcGetAuxTick(RTC_AUX_SECURE, tick);
}

int ksceRtcGetCurrentNetworkTick(SceRtcTick *tick)
{
	if (SceSblACMgrForDriver_426A4E8C(0) && ksceRtcGetCurrentDebugNetworkTick(tick) == 0)
		return 0;
	return ksceRtcGetCurrentRetainedNetworkTick(tick);
}

int ksceRtcGetCurrentDebugNetworkTick(SceRtcTick *tick)
{
	return rtcGetAuxTick(RTC_AUX_DEBUG_NETWORK, tick);
}

int ksceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick)
{
	return rtcGetAuxTick(RTC_AUX_NETWORK, tick);
}

int ksceRtcGetCurrentAdNetworkTick(SceRtcTick *tick)
{
	if ((SceQafMgrForDriver_4BC1883F() || ksceSblUtMgrHasStoreFlag()) &&
	    ksceRtcGetCurrentDebugNetworkTick(tick) == 0)
		return 0;
	return ksceRtcGetCurrentRetainedNetworkTick(tick);
}

/* name unknown */
int SceRtcForDriver_A0D7899A(SceRtcTick *tick)
{
	return rtcGetAuxTick(RTC_AUX_2, tick);
}

/* name unknown: sets the GPS tick */
int SceRtcForDriver_A7236656(const SceRtcTick *tick)
{
	if (tick != NULL && tick->tick <= RTC_TICK_2014_END)
		return SCE_RTC_ERROR_INVALID_VALUE;
	return _sceRtcSetCurrentAuxTick(RTC_AUX_GPS, tick,
					SYSCON_SCRATCH_PAD(auxTick[RTC_AUX_GPS]));
}

int ksceRtcGetCurrentGpsTick(SceRtcTick *tick)
{
	return rtcGetAuxTick(RTC_AUX_GPS, tick);
}

int ksceRtcSetSecureAlarmTick(const SceRtcTick *tick)
{
	return rtcSetAuxAlarm(RTC_AUX_SECURE, tick);
}

int ksceRtcGetSecureAlarmTick(SceRtcTick *tick)
{
	SceRtcTick now;
	int ret;

	ret = rtcGetAuxTick(RTC_AUX_SECURE, &now);
	if (ret < 0)
		return ret;
	*tick = g_rtc.aux[RTC_AUX_SECURE].alarmTick;
	return 0;
}

int ksceRtcIsSecureAlarmed(void)
{
	SceRtcTick now;
	int ret;

	ret = rtcGetAuxTick(RTC_AUX_SECURE, &now);
	if (ret < 0)
		return ret;
	if (g_rtc.aux[RTC_AUX_SECURE].alarmTick.tick == 0)
		return 0;
	return g_rtc.aux[RTC_AUX_SECURE].alarmTick.tick < now.tick;
}

int ksceRtcRegisterSecureAlarmCallback(SceUID cb)
{
	SceKernelIntrStatus intr;
	unsigned int context;
	int ret;

	context = rtcEnter();
	if (sceKernelGetThreadmgrUIDClass(cb) != SCE_KERNEL_THREADMGR_UID_CLASS_CALLBACK) {
		rtcLeave(context);
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID;	/* SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID */
	}
	intr = SceCpuForDriver_821FC0EE();
	if (g_rtc.aux[RTC_AUX_SECURE].callback >= 0) {
		ret = SCE_RTC_ERROR_ALREADY_REGISTERD;
	} else {
		ret = sceKernelNotifyCallback(cb, ksceRtcIsSecureAlarmed() == 1);
		if (ret >= 0) {
			g_rtc.aux[RTC_AUX_SECURE].callback = cb;
			ret = 0;
		}
	}
	sceKernelCpuResumeIntr(intr);
	rtcLeave(context);
	return ret;
}

int ksceRtcUnregisterSecureAlarmCallback(SceUID cb)
{
	if (g_rtc.aux[RTC_AUX_SECURE].callback == cb) {
		g_rtc.aux[RTC_AUX_SECURE].callback = -1;
		return 0;
	}
	return SCE_RTC_ERROR_NOT_FOUND;
}

int ksceRtcGetCurrentClock(SceDateTime *time, int timezone)
{
	unsigned int context;
	SceRtcTick tick;
	int ret;

	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	context = rtcEnter();
	ret = ksceRtcGetCurrentTick(&tick);
	SceRtcForDriver_0F9E02EF(&tick, &tick, timezone);
	ksceRtcConvertTickToDateTime(time, &tick);
	rtcLeave(context);
	return ret;
}

int ksceRtcGetCurrentClockLocalTime(SceDateTime *time)
{
	unsigned int context;
	int ret;

	context = rtcEnter();
	ret = ksceRtcGetCurrentClock(time, g_rtc.daylightSaving + g_rtc.timezone);
	rtcLeave(context);
	return ret;
}

SceUInt64 ksceRtcGetAccumulativeTime(void)
{
	SceUInt64 rtcBase = g_rtc.rtcBase;
	SceUInt32 carry = g_rtc.rtcCarry;

	return sceKernelGetSystemTimeWide() + (rtcBase - g_rtc.systemTimeBase) +
	       (SceUInt64)carry * RTC_COUNT_UNIT;
}

int ksceRtcGetLastReincarnatedTick(SceRtcTick *tick)
{
	unsigned int context;
	int ret;

	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	context = rtcEnter();
	if (g_rtc.initialized) {
		tick->tick = g_rtc.systemTimeBase + g_rtc.tickOffset.tick - g_rtc.rtcBase -
			     (SceUInt64)g_rtc.rtcCarry * RTC_COUNT_UNIT;
		ret = 0;
	} else {
		tick->tick = RTC_TICK_1980;
		ret = SCE_RTC_ERROR_NOT_INITIALIZED;
	}
	rtcLeave(context);
	return ret;
}

int ksceRtcGetLastAdjustedTick(SceRtcTick *tick)
{
	if (tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if (!g_rtc.initialized) {
		tick->tick = RTC_TICK_1980;
		return SCE_RTC_ERROR_NOT_INITIALIZED;
	}
	tick->tick = g_rtc.systemTimeBase + g_rtc.tickOffset.tick - g_rtc.rtcBase;
	return 0;
}

int ksceRtcConvertUtcToLocal(const SceRtcTick *utc, SceRtcTick *local)
{
	unsigned int context;
	int ret;

	context = rtcEnter();
	ret = SceRtcForDriver_0F9E02EF(local, utc, g_rtc.timezone + g_rtc.daylightSaving);
	rtcLeave(context);
	return ret;
}

int ksceRtcConvertLocalTimeToUtc(const SceRtcTick *local, SceRtcTick *utc)
{
	unsigned int context;
	int ret;

	context = rtcEnter();
	ret = SceRtcForDriver_0F9E02EF(utc, local, -(g_rtc.timezone + g_rtc.daylightSaving));
	rtcLeave(context);
	return ret;
}
