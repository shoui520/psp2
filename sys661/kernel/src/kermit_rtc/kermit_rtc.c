#include <kermit.h>

#define SCE_ERROR_NOT_FOUND		0x80000001
#define SCE_ERROR_ALREADY		0x80000020
#define SCE_ERROR_PRIV_REQUIRED		0x80000023
#define SCE_ERROR_INVALID_ID		0x80000100
#define SCE_ERROR_INVALID_POINTER	0x80000103
#define SCE_ERROR_INVALID_VALUE		0x800001FE
#define SCE_ERROR_INVALID_YEAR		0x800001FF

typedef struct {
	u16 year;
	u16 month;
	u16 day;
	u16 hour;
	u16 minutes;
	u16 seconds;
	u32 microseconds;
} pspTime;

typedef struct {
	u8 pad[0x16];
	u16 attr;
} SceUidControlBlock;

static inline int pspShiftK1(void)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);
	return (int)oldK1;
}

static inline int pspK1PtrOk(const void *ptr)
{
	return ((s32)(pspGetK1() & (u32)ptr)) >= 0;
}

void *memset(void *dst, int c, SceSize len);
s64 sceKernelGetSystemTimeWide(void);

#define RTC_TICKS_PER_SECOND		1000000ULL
#define RTC_DEFAULT_TICK		0x00dddef8b3a14000ULL
#define RTC_SECURE_MIN_TICK		0x00e121cf1db13fffULL
#define RTC_UNIX_EPOCH_TICK		0x00dcbffeff2bc000ULL
#define RTC_UNIX_MAX_32_TICK		0x00e4611eff2bbfffULL
#define RTC_WIN32_EPOCH_TICK		0x00b36168b6a58000ULL

typedef struct {
	u32 aUnk;			/* 0x00 */
	u32 bUnk;			/* 0x04 */
	s32 cUnk;			/* 0x08 */
	s32 dUnk;			/* 0x0c */
	u32 currPresent;		/* 0x10 */
	u32 securePresent;		/* 0x14 */
	u32 tickCleared;		/* 0x18 */
	u32 callbackPending;		/* 0x1c */
	u64 accumBaseTick;		/* 0x20 */
	u64 lastSysTime;		/* 0x28 */
	u64 baseCurrentTick;		/* 0x30 */
	u64 baseSecureTick;		/* 0x38 */
	u64 currentTickOffset;		/* 0x40 */
	u64 secureTickOffset;		/* 0x48 */
	u64 internalTickOffset;		/* 0x50 */
	u64 alarmTick;			/* 0x58 */
	u64 savedAlarmTick;		/* 0x60 */
	u32 accumHighSec;		/* 0x68 */
	SceUID rtcCbId;			/* 0x6c */
} SceRtcContext;

typedef struct {
	u8 offset;
	u8 len;
	s16 minutes;
} SceRtcTzEntry;

extern int sceKernelApplicationType(void);
extern int sceKernelRegisterRtcFunc(void *clockHandler, void *timeHandler,
				    void *getTimeOfDayHandler, void *getSecureTickHandler);
extern s32 sceKermitPeripheral_driver_1617E966(u64 *tick);
extern u64 sceKermitPeripheral_driver_20638B97(void);
extern s32 sceKermitPeripheral_driver_55D690DD(u64 *tick);

s32 sceRtc_driver_852255B8(void);
s32 sceRtcSetCurrentTick(const u64 *tick);
s32 sceRtc_driver_66054C2A(const u64 *tick);
s32 sceRtc_driver_508BA64B(const u64 *tick);
s32 sceRtc_driver_E09880CF(const u64 *tick);
s32 sceRtc_driver_366669D6(u64 *tick);
s32 sceRtc_driver_CF76CFE5(void);
s32 sceRtc_driver_530A903E(SceUID cbId);
s32 sceRtcGetCurrentTick(u64 *tick);
s32 sceRtc_driver_CEEF238F(u64 *tick);
s32 sceRtc_driver_ED15334F(u64 *tick);
u64 sceRtc_driver_C0F36B91(void);
s32 sceRtc_driver_7C6E9610(u64 *tick);
s32 sceRtc_driver_E98FEC46(u64 *tick);
s32 sceRtc_driver_7D8E37E1(SceUID cbId);
s32 sceRtc_driver_00F66D06(int year);
s32 sceRtc_driver_1DAB3CF3(int year, int month);
s32 sceRtc_driver_E86D8FC0(const pspTime *date, u32 *timeOut);
s32 sceRtcSetTick(pspTime *date, const u64 *tick);
s32 sceRtcGetTick(const pspTime *date, u64 *tick);
s32 sceRtcTickAddTicks(u64 *destTick, const u64 *srcTick, u64 numTicks);
s32 sceRtc_driver_B84AC7D7(u64 *destTick, const u64 *srcTick, u64 numUS);
s32 sceRtc_driver_77138347(u64 *destTick, const u64 *srcTick, u64 numMins);
s32 sceRtc_driver_321A839A(int year, int month, int day);
s32 sceRtc_driver_BDA60897(u64 *destTick, const char *dateString);
s32 sceRtc_driver_1A86F5FD(char *pszDateTime, const u64 *pUtc, int iTimeZoneMinutes);
s32 sceRtc_driver_1FCE9E23(char *pszDateTime, const u64 *pUtc, int iTimeZoneMinutes);
static void registerRtcFunctions(void);

static int rtcSysEventHandler(int ev_id, char *ev_name, void *param, int *result);

static const s8 g_days_in_month[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const char g_tz_names[] =
	"GMTESTEDTCSTCDTMSTMDTPSTPDTNZDTNZSTIDLENZTAESSTACSSTCADTSADTAEST"
	"EASTGSTLIGTACSTSASTCASTAWSSTJSTKSTWDTMTAWSTCCTWADTWSTJTWASTITBTE"
	"ETDSTEETCETDSTFWTISTMESTMETDSTSSTBSTCETDNTFSTMETMEWTMEZNORSETSWT"
	"WETDSTWETWATNDTADTNFTNSTASTYDTHDTYSTAHSTCATNTIDLW";

static const SceRtcTzEntry g_tz_table[70] = {
	{ 0x00, 3,    0 }, { 0x03, 3, -300 }, { 0x06, 3, -240 }, { 0x09, 3, -360 },
	{ 0x0c, 3, -300 }, { 0x0f, 3, -420 }, { 0x12, 3, -360 }, { 0x15, 3, -480 },
	{ 0x18, 3, -420 }, { 0x1b, 4,  780 }, { 0x1f, 4,  720 }, { 0x23, 4,  720 },
	{ 0x27, 3,  720 }, { 0x2a, 5,  660 }, { 0x2f, 5,  630 }, { 0x34, 4,  630 },
	{ 0x38, 4,  630 }, { 0x3c, 4,  600 }, { 0x40, 4,  600 }, { 0x44, 3,  600 },
	{ 0x47, 4,  600 }, { 0x4b, 4,  570 }, { 0x4f, 4,  570 }, { 0x53, 4,  570 },
	{ 0x57, 5,  540 }, { 0x5c, 3,  540 }, { 0x5f, 3,  540 }, { 0x62, 3,  540 },
	{ 0x65, 2,  510 }, { 0x67, 4,  480 }, { 0x6b, 3,  480 }, { 0x6e, 4,  480 },
	{ 0x72, 3,  480 }, { 0x75, 2,  450 }, { 0x77, 4,  420 }, { 0x7b, 2,  210 },
	{ 0x7d, 2,  180 }, { 0x7f, 6,  180 }, { 0x85, 3,  120 }, { 0x88, 6,  120 },
	{ 0x8e, 3,  120 }, { 0x91, 3,  120 }, { 0x94, 4,  120 }, { 0x98, 6,  120 },
	{ 0x9e, 3,  120 }, { 0xa1, 3,   60 }, { 0xa4, 3,   60 }, { 0xa7, 3,   60 },
	{ 0xaa, 3,   60 }, { 0xad, 3,   60 }, { 0xb0, 4,   60 }, { 0xb4, 3,   60 },
	{ 0xb7, 3,   60 }, { 0xba, 3,   60 }, { 0xbd, 3,   60 }, { 0xc0, 6,   60 },
	{ 0xc6, 3,    0 }, { 0xc9, 3,  -60 }, { 0xcc, 3,  -90 }, { 0xcf, 3, -180 },
	{ 0xd2, 3, -150 }, { 0xd5, 3, -150 }, { 0xd8, 3, -240 }, { 0xdb, 3, -480 },
	{ 0xde, 3, -540 }, { 0xe1, 3, -540 }, { 0xe4, 4, -600 }, { 0xe8, 3, -600 },
	{ 0xeb, 2, -660 }, { 0xed, 4, -720 }
};

static const char g_weekday_names[] = "SundayMondayTuesdayWednesdayThursdayFridaySaturday";
static const u8 g_weekday_lens[8] = { 6, 6, 7, 9, 8, 6, 8, 0 };

static const char g_month_names[] =
	"JanuaryFebruaryMarchAprilMayJuneJulyAugustSeptemberOctoberNovemberDecember";
static const u8 g_month_lens[12] = { 7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8 };

static const char g_weekday_abbr[] = "SunMonTueWedThuFriSat";
static const char g_month_abbr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

static SceSysEventHandler g_rtc_sys_event = {
	sizeof(SceSysEventHandler),
	"SceRtc",
	0x00ffff00,
	rtcSysEventHandler,
	0,
	0,
	NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

u32 sceRtc_driver_var_5A64FA55;
static u64 g_last_accum_time;
static SceRtcContext g_rtc_ctx;

static int rtcSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_name;
	(void)result;

	if (ev_id == 0x1000) {
		u8 *p = *(u8 **)((u8 *)param + 0x14);
		sceRtcGetCurrentTick((u64 *)(p + 0x10));
		sceRtc_driver_366669D6((u64 *)(p + 0x18));
		if (((u32 *)param)[1] != 0) {
			if (sceKernelApplicationType() == 0x200) {
				sceRtc_driver_366669D6(&g_rtc_ctx.savedAlarmTick);
				sceRtc_driver_E09880CF(NULL);
			}
		}
	} else if (ev_id < 0x1001) {
		if (ev_id == 0x200)
			g_rtc_ctx.callbackPending = 1;
	} else if (ev_id == 0x100000) {
		u32 *sub;

		sceRtc_driver_852255B8();
		sub = ((u32 **)param)[1];
		if (sub[1] == 0x80000000U) {
			g_rtc_ctx.tickCleared = 0;
			sceRtc_driver_E09880CF(&g_rtc_ctx.savedAlarmTick);
		}
		if (g_rtc_ctx.callbackPending != 1) {
			s32 st = sceRtc_driver_CF76CFE5();
			sceKernelNotifyCallback(g_rtc_ctx.rtcCbId, st);
		}
		g_rtc_ctx.callbackPending = 0;
	}

	return 0;
}

s32 sceRtc_driver_852255B8(void)
{
	u64 curTick = 0;
	u64 curOffset = 0;
	u64 secTickRaw = 0;
	u64 secOffset = 0;
	u64 shifted = 0;
	u64 intOffset;
	u64 accum;
	s64 sysTime;
	s32 res;
	int intr;

	accum = sceKermitPeripheral_driver_20638B97();
	g_rtc_ctx.accumHighSec = (u32)(accum / 500000ULL);

	res = sceKermitPeripheral_driver_1617E966(&curTick);
	if (res < 0) {
		Kprintf("Failed to sceKermitRtcGetCurrentTick 0x%08x \n", res);
		return 0;
	}

	sceRtcSetCurrentTick(&curTick);
	sysTime = sceKernelGetSystemTimeWide();
	sceRtc_driver_B84AC7D7(&curOffset, &curTick, 0);
	sceRtc_driver_B84AC7D7(&curOffset, &curOffset, (u64)(-sysTime));

	if (sceKermitPeripheral_driver_55D690DD(&secTickRaw) >= 0)
		shifted = (secTickRaw >> 19) & 0x000000ffffffffffULL;

	if (shifted != 0) {
		g_rtc_ctx.securePresent = 1;
		secTickRaw = shifted << 19;
		sceRtc_driver_B84AC7D7(&secOffset, &secTickRaw, 0);
		sceRtc_driver_B84AC7D7(&secOffset, &secOffset, (u64)(-sysTime));
		if (secOffset <= RTC_SECURE_MIN_TICK)
			shifted = 0;
	}

	if (shifted == 0) {
		g_rtc_ctx.securePresent = 0;
		secTickRaw = 0;
		secOffset = 0;
	}

	intOffset = g_rtc_ctx.internalTickOffset;
	if (g_rtc_ctx.tickCleared != 0)
		intOffset += curOffset - g_rtc_ctx.currentTickOffset;

	intr = sceKernelCpuSuspendIntr();
	g_rtc_ctx.alarmTick = 0;
	g_rtc_ctx.accumBaseTick = 0;
	g_rtc_ctx.lastSysTime = (u64)sysTime;
	g_rtc_ctx.currPresent = 1;
	g_rtc_ctx.baseCurrentTick = curTick;
	g_rtc_ctx.baseSecureTick = secTickRaw;
	g_rtc_ctx.currentTickOffset = curOffset;
	g_rtc_ctx.internalTickOffset = intOffset;
	g_rtc_ctx.secureTickOffset = secOffset;
	sceKernelCpuResumeIntr(intr);

	return 0;
}

s32 sceRtcSetCurrentTick(const u64 *tick)
{
	int oldK1;
	int intr;
	u64 sysTime;
	u64 delta;
	s32 res;

	if (tick == NULL)
		return SCE_ERROR_INVALID_POINTER;

	oldK1 = pspShiftK1();
	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceKernelPowerLockForUser(0);
	g_rtc_ctx.callbackPending = 1;
	intr = sceKernelCpuSuspendIntr();
	sysTime = (u64)sceKernelGetSystemTimeWide();
	delta = (sysTime - g_rtc_ctx.lastSysTime) + g_rtc_ctx.accumBaseTick;
	g_rtc_ctx.lastSysTime = sysTime;
	g_rtc_ctx.currentTickOffset = *tick - sysTime;
	if (g_rtc_ctx.securePresent != 0)
		g_rtc_ctx.baseSecureTick += delta;
	g_rtc_ctx.accumBaseTick = 0;
	g_rtc_ctx.currPresent = 1;
	g_rtc_ctx.baseCurrentTick = *tick;
	sceKernelCpuResumeIntr(intr);

	res = sceRtc_driver_E09880CF(&g_rtc_ctx.alarmTick);
	if (res < 0) {
		g_rtc_ctx.callbackPending = 0;
		sceKernelPowerUnlockForUser(0);
		pspSetK1(oldK1);
		return res;
	}

	intr = sceKernelCpuSuspendIntr();
	if (g_rtc_ctx.callbackPending != 1) {
		s32 st = sceRtc_driver_CF76CFE5();
		sceKernelNotifyCallback(g_rtc_ctx.rtcCbId, st);
	}
	g_rtc_ctx.callbackPending = 0;
	sceKernelCpuResumeIntr(intr);

	sceKernelPowerUnlockForUser(0);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_66054C2A(const u64 *tick)
{
	int oldK1;
	int intr;
	u64 sysTime;
	u64 delta;

	oldK1 = pspShiftK1();
	if (tick == NULL) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_POINTER;
	}
	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (*tick <= RTC_SECURE_MIN_TICK) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}

	sceKernelPowerLockForUser(0);
	sysTime = (u64)sceKernelGetSystemTimeWide();
	delta = (sysTime - g_rtc_ctx.lastSysTime) + g_rtc_ctx.accumBaseTick;
	intr = sceKernelCpuSuspendIntr();
	g_rtc_ctx.baseSecureTick = *tick - delta;
	g_rtc_ctx.securePresent = 1;
	g_rtc_ctx.secureTickOffset = *tick - sysTime;
	sceKernelCpuResumeIntr(intr);
	sceKernelPowerUnlockForUser(0);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_508BA64B(const u64 *tick)
{
	int oldK1;
	int intr;

	oldK1 = pspShiftK1();
	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	intr = sceKernelCpuSuspendIntr();
	if (tick == NULL || *tick == 0) {
		g_rtc_ctx.tickCleared = 0;
	} else {
		u64 sysTime = (u64)sceKernelGetSystemTimeWide();
		g_rtc_ctx.tickCleared = 1;
		g_rtc_ctx.internalTickOffset = *tick - sysTime;
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_E09880CF(const u64 *tick)
{
	int oldK1;
	int intr;
	u64 curTick;

	oldK1 = pspShiftK1();
	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (tick != NULL && *tick != 0 && g_rtc_ctx.baseCurrentTick != 0) {
		sceRtcGetCurrentTick(&curTick);
		if (*tick < curTick) {
			if (oldK1 < 0) {
				pspSetK1(oldK1);
				return SCE_ERROR_INVALID_VALUE;
			}
		} else {
			u64 target = *tick;
			if (oldK1 < 0) {
				u64 diff = target - curTick;
				if (diff > 0x000000141dd75fffULL) {
					pspSetK1(oldK1);
					return SCE_ERROR_INVALID_VALUE;
				}
			}
			curTick = (target - g_rtc_ctx.baseCurrentTick) / 500000ULL;
		}
	}

	g_rtc_ctx.callbackPending = 1;
	intr = sceKernelCpuSuspendIntr();
	g_rtc_ctx.alarmTick = (tick != NULL) ? *tick : 0;
	if (g_rtc_ctx.callbackPending != 1) {
		s32 st = sceRtc_driver_CF76CFE5();
		sceKernelNotifyCallback(g_rtc_ctx.rtcCbId, st);
	}
	g_rtc_ctx.callbackPending = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_366669D6(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (tick == NULL) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_POINTER;
	}
	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	*tick = g_rtc_ctx.alarmTick;
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_CF76CFE5(void)
{
	return 0;
}

s32 sceRtc_driver_530A903E(SceUID cbId)
{
	int oldK1;
	SceUidControlBlock *cb;
	s32 res;
	int intr;

	oldK1 = pspShiftK1();
	if (sceKernelGetThreadmanIdType(cbId) != 8) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_ID;
	}
	if (sceKernelGetUIDcontrolBlock(cbId, (void **)&cb) != 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (((u32)pspGetK1() >> 31) != 0 && (cb->attr & 2) == 0) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	res = SCE_ERROR_ALREADY;
	intr = sceKernelCpuSuspendIntr();
	if (g_rtc_ctx.rtcCbId < 0) {
		s32 st;
		g_rtc_ctx.rtcCbId = cbId;
		res = 0;
		st = sceRtc_driver_CF76CFE5();
		sceKernelNotifyCallback(cbId, st);
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return res;
}

s32 sceRtcGetCurrentTick(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_rtc_ctx.currPresent == 0) {
		*tick = RTC_DEFAULT_TICK;
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_FOUND;
	}

	*tick = g_rtc_ctx.currentTickOffset + (u64)sceKernelGetSystemTimeWide();
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_CEEF238F(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_rtc_ctx.securePresent == 0) {
		*tick = RTC_DEFAULT_TICK;
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_FOUND;
	}

	*tick = g_rtc_ctx.secureTickOffset + (u64)sceKernelGetSystemTimeWide();
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_ED15334F(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_rtc_ctx.tickCleared == 0) {
		*tick = 0;
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_FOUND;
	}

	*tick = g_rtc_ctx.internalTickOffset + (u64)sceKernelGetSystemTimeWide();
	pspSetK1(oldK1);
	return 0;
}

u64 sceRtc_driver_C0F36B91(void)
{
	u64 val = 0;
	u64 sysTime;

	if (sceRtc_driver_var_5A64FA55 == 0)
		val = (u64)g_rtc_ctx.accumHighSec * 500000ULL;

	val += g_rtc_ctx.accumBaseTick;
	sysTime = (u64)sceKernelGetSystemTimeWide();
	val += sysTime - g_rtc_ctx.lastSysTime;

	if (sceRtc_driver_var_5A64FA55 != 0) {
		if (val > g_last_accum_time) {
			g_last_accum_time = val;
		} else {
			g_rtc_ctx.accumBaseTick = g_last_accum_time;
			val += g_last_accum_time;
		}
	}

	return val;
}

s32 sceRtc_driver_7C6E9610(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_rtc_ctx.currPresent == 0) {
		*tick = RTC_DEFAULT_TICK;
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_FOUND;
	}

	*tick = ((g_rtc_ctx.currentTickOffset + g_rtc_ctx.lastSysTime) - g_rtc_ctx.accumBaseTick) -
		((u64)g_rtc_ctx.accumHighSec * 500000ULL);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_E98FEC46(u64 *tick)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}
	if (g_rtc_ctx.currPresent == 0) {
		*tick = RTC_DEFAULT_TICK;
		pspSetK1(oldK1);
		return SCE_ERROR_NOT_FOUND;
	}

	*tick = (g_rtc_ctx.currentTickOffset + g_rtc_ctx.lastSysTime) - g_rtc_ctx.accumBaseTick;
	pspSetK1(oldK1);
	return 0;
}

int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	memset(&g_rtc_ctx, 0, sizeof(g_rtc_ctx));
	g_rtc_ctx.rtcCbId = -1;
	g_rtc_ctx.aUnk = 0;
	g_rtc_ctx.cUnk = 0;
	g_rtc_ctx.bUnk = 0;
	g_rtc_ctx.dUnk = 0;
	sceRtc_driver_852255B8();
	registerRtcFunctions();
	sceKernelRegisterSysEventHandler(&g_rtc_sys_event);
	sceKernelApplicationType();
	return 0;
}

int module_reboot_before(void)
{
	sceRtc_driver_E09880CF(NULL);
	sceKernelUnregisterSysEventHandler(&g_rtc_sys_event);
	return 0;
}

s32 sceRtcInit(void)
{
	memset(&g_rtc_ctx, 0, sizeof(g_rtc_ctx));
	g_rtc_ctx.rtcCbId = -1;
	g_rtc_ctx.aUnk = 0;
	g_rtc_ctx.cUnk = 0;
	g_rtc_ctx.bUnk = 0;
	g_rtc_ctx.dUnk = 0;
	sceRtc_driver_852255B8();
	registerRtcFunctions();
	sceKernelRegisterSysEventHandler(&g_rtc_sys_event);
	return 0;
}

s32 sceRtcEnd(void)
{
	sceKernelUnregisterSysEventHandler(&g_rtc_sys_event);
	return 0;
}

s32 sceRtcSuspend(void)
{
	return 0;
}

s32 sceRtcResume(void)
{
	sceRtc_driver_852255B8();
	return 0;
}

s32 sceRtc_driver_1C1859DF(void)
{
	g_rtc_ctx.tickCleared = 0;
	g_rtc_ctx.currPresent = 0;
	g_rtc_ctx.securePresent = 0;
	return 0;
}

s32 sceRtc_driver_DFF30673(u32 a0, u32 a1, s32 a2, s32 a3)
{
	int intr = sceKernelCpuSuspendIntr();
	g_rtc_ctx.cUnk = a3;
	g_rtc_ctx.bUnk = a1;
	g_rtc_ctx.dUnk = a2;
	g_rtc_ctx.aUnk = a0;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceRtc_driver_7D8E37E1(SceUID cbId)
{
	if (g_rtc_ctx.rtcCbId == cbId) {
		g_rtc_ctx.rtcCbId = -1;
		return 0;
	}
	return SCE_ERROR_NOT_FOUND;
}

s32 sceRtcGetCurrentClock(pspTime *time, int tz)
{
	u64 tick;
	int oldK1 = pspShiftK1();
	s32 res = SCE_ERROR_PRIV_REQUIRED;

	if (pspK1PtrOk(time)) {
		sceRtcGetCurrentTick(&tick);
		res = sceRtc_driver_77138347(&tick, &tick, (s64)tz);
		sceRtcSetTick(time, &tick);
	}

	pspSetK1(oldK1);
	return res;
}

s32 sceRtc_driver_9012B140(pspTime *time)
{
	int oldK1 = pspShiftK1();
	s32 res = SCE_ERROR_PRIV_REQUIRED;

	if (pspK1PtrOk(time))
		res = sceRtcGetCurrentClock(time, g_rtc_ctx.dUnk + g_rtc_ctx.cUnk);

	pspSetK1(oldK1);
	return res;
}

s32 sceRtc_driver_4E267E02(const u64 *tickUTC, u64 *tickLocal)
{
	return sceRtc_driver_77138347(tickLocal, tickUTC, (s64)(g_rtc_ctx.dUnk + g_rtc_ctx.cUnk));
}

s32 sceRtc_driver_3E66CB7E(const u64 *tickLocal, u64 *tickUTC)
{
	return sceRtc_driver_77138347(tickUTC, tickLocal, (s64)(-(g_rtc_ctx.dUnk + g_rtc_ctx.cUnk)));
}

s32 sceRtc_driver_00F66D06(int year)
{
	if (year <= 0)
		return SCE_ERROR_INVALID_YEAR;
	if ((year % 400) == 0)
		return 1;
	if ((year % 100) == 0)
		return 0;
	return (year & 3) == 0;
}

s32 sceRtc_driver_1DAB3CF3(int year, int month)
{
	if (year < 1 || month < 1 || month >= 13)
		return SCE_ERROR_INVALID_YEAR;

	if (sceRtc_driver_00F66D06(year) != 0)
		return g_days_in_month[1][month - 1];
	return g_days_in_month[0][month - 1];
}

s32 sceRtcCheckValid(const pspTime *date)
{
	s32 days;

	if (!pspK1PtrOk(date))
		return SCE_ERROR_PRIV_REQUIRED;
	if (date->year == 0 || date->year >= 10000)
		return -1;
	if (date->month == 0 || date->month >= 13)
		return -2;

	days = (sceRtc_driver_00F66D06(date->year) != 0)
		? g_days_in_month[1][date->month - 1]
		: g_days_in_month[0][date->month - 1];
	if (date->day == 0 || date->day > days)
		return -3;
	if (date->hour >= 24)
		return -4;
	if (date->minutes >= 60)
		return -5;
	if (date->seconds >= 60)
		return -6;
	if (date->microseconds >= 1000000)
		return -7;
	return 0;
}

s32 sceRtc_driver_A4A5BF1B(const pspTime *date, u32 *dosTime)
{
	int oldK1 = pspShiftK1();
	u16 year;

	if (!pspK1PtrOk(date) || !pspK1PtrOk(dosTime)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	year = date->year;
	if (year < 1980) {
		if (dosTime != NULL)
			*dosTime = 0;
		pspSetK1(oldK1);
		return -1;
	}
	if (year >= 2108) {
		if (dosTime != NULL)
			*dosTime = 0xff9fbf7dU;
		pspSetK1(oldK1);
		return -1;
	}

	if (dosTime != NULL) {
		u32 hi = (((year - 1980) & 0x7f) << 9) | ((date->month & 0xf) << 5) | (date->day & 0x1f);
		u32 lo = ((date->hour & 0x1f) << 11) | ((date->minutes & 0x3f) << 5) | ((date->seconds >> 1) & 0x1f);
		*dosTime = (hi << 16) | lo;
	}
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_B2B6578C(const pspTime *date, u64 *win32Time)
{
	u64 tick;
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(date) || !pspK1PtrOk(win32Time)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceRtcGetTick(date, &tick);
	if (tick < RTC_WIN32_EPOCH_TICK) {
		if (win32Time != NULL)
			*win32Time = 0;
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (win32Time != NULL)
		*win32Time = (tick - RTC_WIN32_EPOCH_TICK) * 10ULL;

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_94225550(const pspTime *date, u64 *time64)
{
	u64 tick;
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(date) || !pspK1PtrOk(time64)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceRtcGetTick(date, &tick);
	if (tick < RTC_UNIX_EPOCH_TICK) {
		if (time64 != NULL)
			*time64 = 0;
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (time64 != NULL)
		*time64 = (tick - RTC_UNIX_EPOCH_TICK) / RTC_TICKS_PER_SECOND;

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_E86D8FC0(const pspTime *date, u32 *timeOut)
{
	u64 tick;
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(date) || !pspK1PtrOk(timeOut)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceRtcGetTick(date, &tick);
	if (tick < RTC_UNIX_EPOCH_TICK) {
		if (timeOut != NULL)
			*timeOut = 0;
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}
	if (tick > RTC_UNIX_MAX_32_TICK) {
		if (timeOut != NULL)
			*timeOut = 0x7fffffffU;
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}

	if (timeOut != NULL)
		*timeOut = (u32)((tick - RTC_UNIX_EPOCH_TICK) / RTC_TICKS_PER_SECOND);

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtcSetTick(pspTime *date, const u64 *tick)
{
	int oldK1 = pspShiftK1();
	u64 t;
	s32 days;
	s32 remSec;
	s32 minTotal;
	s32 hourTotal;
	s32 year = 1;
	s32 leap;
	const s8 *monTbl;
	s32 month = 0;

	if (!pspK1PtrOk(date) || !pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	t = *tick;
	days = (s32)(t / 86400000000ULL);
	t -= (u64)days * 86400000000ULL;
	remSec = (s32)(t / 1000000ULL);
	minTotal = remSec / 60;
	hourTotal = minTotal / 60;

	date->microseconds = (u32)(t - (u64)remSec * 1000000ULL);
	date->seconds = (u16)(remSec - minTotal * 60);
	date->minutes = (u16)(minTotal - hourTotal * 60);
	date->hour = (u16)(hourTotal % 24);

	if (days > 146096) {
		s32 c400 = days / 146097;
		year = c400 * 400 + 1;
		days -= c400 * 146097;
	}
	if (days > 36523) {
		s32 c100 = days / 36524;
		days -= c100 * 36524;
		year += c100 * 100;
		if (days == 0) {
			days = 365;
			year--;
		}
	}
	if (days >= 1461) {
		s32 c4 = days / 1461;
		year += c4 * 4;
		days -= c4 * 1461;
	}

	for (;;) {
		leap = sceRtc_driver_00F66D06(year);
		if (leap != 0) {
			if (days < 366) {
				date->year = (u16)year;
				break;
			}
			days -= 366;
		} else {
			if (days < 365) {
				date->year = (u16)year;
				break;
			}
			days -= 365;
		}
		year++;
	}

	leap = sceRtc_driver_00F66D06(year);
	monTbl = g_days_in_month[leap];
	while (days >= monTbl[month]) {
		days -= monTbl[month];
		month++;
	}
	date->month = (u16)(month + 1);
	date->day = (u16)(days + 1);

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtcGetTick(const pspTime *date, u64 *tick)
{
	int oldK1 = pspShiftK1();
	u16 year;
	s32 yDays;
	s64 totalDays;
	s32 leap;
	u16 month;
	u16 i;
	u64 totalSec;
	u64 outTick;

	if (!pspK1PtrOk(date) || !pspK1PtrOk(tick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	year = date->year;
	if ((u32)(year - 1) >= 9999U) {
		pspSetK1(oldK1);
		return SCE_ERROR_INVALID_VALUE;
	}

	yDays = year * 365 + ((year + 3) >> 2) - ((year + 99) / 100) + ((year + 399) / 400) - 366;
	totalDays = yDays;
	leap = sceRtc_driver_00F66D06(year);
	month = date->month;
	if (month == 0)
		month = 1;
	else if (month >= 13)
		month = 12;

	for (i = 0; i < (u16)(month - 1); i++)
		totalDays += g_days_in_month[leap][i];

	totalDays += (s32)date->day - 1;
	totalSec = ((u64)totalDays * 24ULL + date->hour) * 60ULL + date->minutes;
	totalSec = totalSec * 60ULL + date->seconds;
	outTick = totalSec * 1000000ULL + date->microseconds;
	if (tick != NULL)
		*tick = outTick;

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_E45726F6(u64 *destTick, const u64 *srcTick, int numMonths)
{
	pspTime date;
	int oldK1 = pspShiftK1();
	s32 totalMonths;
	s32 newYear;
	s32 newMonth;
	s32 maxDays;

	if (!pspK1PtrOk(destTick) || !pspK1PtrOk(srcTick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceRtcSetTick(&date, srcTick);
	totalMonths = (s32)date.year * 12 + (s32)date.month + numMonths - 1;
	newYear = totalMonths / 12;
	newMonth = (totalMonths - newYear * 12) + 1;
	date.year = (u16)newYear;
	date.month = (u16)newMonth;
	maxDays = sceRtc_driver_1DAB3CF3(date.year, date.month);
	if (maxDays < (s32)date.day)
		date.day = (u16)maxDays;

	sceRtcGetTick(&date, destTick);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_AAAE90FF(u64 *destTick, const u64 *srcTick, int numYears)
{
	pspTime date;
	int oldK1 = pspShiftK1();
	s32 maxDays;

	if (!pspK1PtrOk(destTick) || !pspK1PtrOk(srcTick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	sceRtcSetTick(&date, srcTick);
	date.year = (u16)(date.year + numYears);
	maxDays = sceRtc_driver_1DAB3CF3(date.year, date.month);
	if (maxDays < (s32)date.day)
		date.day = (u16)maxDays;

	sceRtcGetTick(&date, destTick);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_281144FE(const u64 *tick1, const u64 *tick2)
{
	int oldK1 = pspShiftK1();
	u64 t1, t2;

	if (!pspK1PtrOk(tick1) || !pspK1PtrOk(tick2)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	t1 = *tick1;
	t2 = *tick2;
	pspSetK1(oldK1);
	if (t1 < t2)
		return -1;
	if (t1 > t2)
		return 1;
	return 0;
}

u32 sceRtc_driver_C66D9686(void)
{
	return 1000000U;
}

s32 sceRtc_driver_321A839A(int year, int month, int day)
{
	int y = year;
	int m = month;
	int total;

	if ((u32)(month - 1) < 2U) {
		y = year - 1;
		m = month + 12;
	}

	total = y + (y / 4) - (y / 100) + (y / 400) + ((13 * m + 8) / 5) + day;
	return total % 7;
}

s32 sceRtc_driver_74772CCC(pspTime *date, u32 dosTime)
{
	int oldK1 = pspShiftK1();
	u32 lo = dosTime & 0xffffU;
	u32 hi = dosTime >> 16;

	if (!pspK1PtrOk(date)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	date->year = (u16)((hi >> 9) + 1980);
	date->seconds = (u16)((lo << 1) & 0x3e);
	date->minutes = (u16)((lo >> 5) & 0x3f);
	date->hour = (u16)(lo >> 11);
	date->day = (u16)(hi & 0x1f);
	date->month = (u16)((hi >> 5) & 0xf);
	date->microseconds = 0;
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_CEF8FE8E(pspTime *date, u64 win32Time)
{
	u64 tick;
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(date)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	tick = (win32Time / 10ULL) + RTC_WIN32_EPOCH_TICK;
	sceRtcSetTick(date, &tick);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_CF4E0EE0(pspTime *date, u64 time64)
{
	u64 tick;
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(date)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	tick = time64 * 1000000ULL + RTC_UNIX_EPOCH_TICK;
	sceRtcSetTick(date, &tick);
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_40B07E72(pspTime *date, u32 time32)
{
	return sceRtc_driver_CF4E0EE0(date, (u64)time32);
}

s32 sceRtcTickAddTicks(u64 *destTick, const u64 *srcTick, u64 numTicks)
{
	int oldK1 = pspShiftK1();

	if (!pspK1PtrOk(destTick) || !pspK1PtrOk(srcTick)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	*destTick = *srcTick + numTicks;
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_B84AC7D7(u64 *destTick, const u64 *srcTick, u64 numUS)
{
	return sceRtcTickAddTicks(destTick, srcTick, numUS);
}

s32 sceRtcTickAddSeconds(u64 *destTick, const u64 *srcTick, u64 numSecs)
{
	return sceRtcTickAddTicks(destTick, srcTick, numSecs * 1000000ULL);
}

s32 sceRtc_driver_77138347(u64 *destTick, const u64 *srcTick, u64 numMins)
{
	return sceRtcTickAddTicks(destTick, srcTick, numMins * 60000000ULL);
}

s32 sceRtc_driver_8413CADC(u64 *destTick, const u64 *srcTick, int numHours)
{
	return sceRtcTickAddTicks(destTick, srcTick, (s64)numHours * 3600000000LL);
}

s32 sceRtc_driver_CB0538FD(u64 *destTick, const u64 *srcTick, int numDays)
{
	return sceRtcTickAddTicks(destTick, srcTick, (s64)numDays * 86400000000LL);
}

s32 sceRtc_driver_80F21937(u64 *destTick, const u64 *srcTick, int numWeeks)
{
	return sceRtcTickAddTicks(destTick, srcTick, (s64)numWeeks * 604800000000LL);
}

static const char *parseTzName(const char *str, s32 *outMinutes)
{
	int i, j;

	for (i = 0; i < 70; i++) {
		u8 off = g_tz_table[i].offset;
		u8 len = g_tz_table[i].len;
		const char *tz = &g_tz_names[off];
		for (j = 0; j < len; j++) {
			if (((tz[j] ^ str[j]) & 0xdf) != 0)
				break;
		}
		if (j == len) {
			*outMinutes = g_tz_table[i].minutes;
			return str + len;
		}
	}
	return NULL;
}

static const char *parseWeekday(const char *str, s32 *outWday)
{
	const char *name = g_weekday_names;
	int i, j;

	for (i = 0; i < 7; i++) {
		u8 len = g_weekday_lens[i];
		for (j = 0; j < len; j++) {
			if (((name[j] ^ str[j]) & 0xdf) != 0)
				break;
		}
		if (j == len) {
			*outWday = i;
			return str + len;
		}
		for (j = 0; j < 3; j++) {
			if (((name[j] ^ str[j]) & 0xdf) != 0)
				break;
		}
		if (j == 3) {
			*outWday = i;
			return str + 3;
		}
		name += len;
	}
	return NULL;
}

static const char *parseMonth(const char *str, s32 *outMonth)
{
	const char *name = g_month_names;
	int i, j;

	for (i = 0; i < 12; i++) {
		u8 len = g_month_lens[i];
		for (j = 0; j < len; j++) {
			if (((name[j] ^ str[j]) & 0xdf) != 0)
				break;
		}
		if (j == len) {
			*outMonth = i + 1;
			return str + len;
		}
		for (j = 0; j < 3; j++) {
			if (((name[j] ^ str[j]) & 0xdf) != 0)
				break;
		}
		if (j == 3) {
			*outMonth = i + 1;
			return str + 3;
		}
		name += len;
	}
	return NULL;
}

static s32 parse2Digits(const char *s)
{
	if ((u32)(s[0] - '0') >= 10U || s[1] < '0' || s[1] > '9')
		return -1;
	return (s[0] - '0') * 10 + (s[1] - '0');
}

static s32 parse4Digits(const char *s)
{
	if ((u32)(s[0] - '0') >= 10U || s[1] < '0' || s[1] > '9' ||
	    s[2] < '0' || s[2] > '9' || s[3] < '0' || s[3] > '9')
		return -1;
	return (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
}

static const char *parse1Or2Digits(const char *s, s32 *outVal)
{
	s32 d0 = s[0] - '0';
	s32 d1;

	if ((u32)(d0 & 0xff) >= 10U) {
		*outVal = -1;
		return NULL;
	}
	d1 = s[1] - '0';
	if ((u32)d1 < 10U) {
		*outVal = d0 * 10 + d1;
		return s + 2;
	}
	*outVal = d0;
	return s + 1;
}

s32 sceRtc_driver_C3A806EE(u64 *destTick, const char *dateString)
{
	int oldK1 = pspShiftK1();
	pspTime date;
	s32 wday, val;
	s32 tzMinutes = 0;
	const char *p = dateString;
	const char *next;

	if (!pspK1PtrOk(destTick) || !pspK1PtrOk(dateString)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	while (*p == ' ' || *p == '\t')
		p++;

	if (parse4Digits(p) != -1) {
		s32 res = sceRtc_driver_BDA60897(destTick, p);
		pspSetK1(oldK1);
		return res;
	}

	p = parseWeekday(p, &wday);
	if (p == NULL) {
		pspSetK1(oldK1);
		return -1;
	}
	if (*p == ',')
		p++;
	while (*p == ' ' || *p == '\t')
		p++;

	next = parseMonth(p, &val);
	if (next != NULL) {
		date.month = (u16)val;
		if (*next != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		p = next + 1;
		if (*p == ' ') {
			s32 d = p[1] - '0';
			p += 2;
			if (d >= 10) {
				pspSetK1(oldK1);
				return -1;
			}
			val = d;
		} else {
			p = parse1Or2Digits(p, &val);
			if (p == NULL) {
				pspSetK1(oldK1);
				return -1;
			}
		}
		date.day = (u16)val;
		if (*p != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		p++;
		p = parse1Or2Digits(p, &val);
		if (p == NULL || *p != ':') {
			pspSetK1(oldK1);
			return -1;
		}
		date.hour = (u16)val;
		p++;
		p = parse1Or2Digits(p, &val);
		if (p == NULL || *p != ':') {
			pspSetK1(oldK1);
			return -1;
		}
		date.minutes = (u16)val;
		p++;
		p = parse1Or2Digits(p, &val);
		if (p == NULL || *p != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		date.seconds = (u16)val;
		p++;
		val = parse4Digits(p);
		if (val < 0) {
			pspSetK1(oldK1);
			return -1;
		}
		date.year = (u16)val;
		date.microseconds = 0;
	} else {
		p = parse1Or2Digits(p, &val);
		if (p == NULL) {
			pspSetK1(oldK1);
			return -1;
		}
		date.day = (u16)val;
		if (*p != '-' && *p != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		p++;
		p = parseMonth(p, &val);
		if (p == NULL) {
			pspSetK1(oldK1);
			return -1;
		}
		date.month = (u16)val;
		if (*p != '-' && *p != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		p++;
		val = parse4Digits(p);
		if (val >= 0) {
			p += 4;
		} else {
			val = parse2Digits(p);
			if (val < 0) {
				pspSetK1(oldK1);
				return -1;
			}
			val += (val < 50) ? 2000 : 1900;
			p += 2;
		}
		date.year = (u16)val;
		if (*p != ' ') {
			pspSetK1(oldK1);
			return -1;
		}
		p++;
		p = parse1Or2Digits(p, &val);
		if (p == NULL || val >= 26 || *p != ':') {
			pspSetK1(oldK1);
			return -1;
		}
		date.hour = (u16)val;
		p++;
		p = parse1Or2Digits(p, &val);
		if (p == NULL) {
			pspSetK1(oldK1);
			return -1;
		}
		date.minutes = (u16)val;
		if (*p == ' ') {
			p++;
			val = 0;
		} else if (*p == ':') {
			p++;
			p = parse1Or2Digits(p, &val);
			if (p == NULL) {
				pspSetK1(oldK1);
				return -1;
			}
		} else {
			pspSetK1(oldK1);
			return -1;
		}
		date.seconds = (u16)val;
		date.microseconds = 0;
		if (*p == ' ') {
			char sign;
			p++;
			sign = *p;
			if (sign == '+' || sign == '-') {
				s32 hhmm = parse4Digits(p + 1);
				tzMinutes = (hhmm / 100) * 60 + (hhmm - (hhmm / 60) * 60);
				if (sign == '-')
					tzMinutes = -tzMinutes;
			} else if (sign == 'U' && p[1] == 'T') {
				tzMinutes = 0;
			} else if (parseTzName(p, &tzMinutes) == NULL) {
				char c = p[0] & ~0x20;
				if ((u32)((c - 'A') & 0xff) < 9U || (u32)((c - 'K') & 0xff) < 3U) {
					tzMinutes = c * 60 - 3900;
				} else if ((u32)((c - 'N') & 0xff) < 12U) {
					tzMinutes = ('N' - c) * 60;
				} else if (c == 'Z') {
					tzMinutes = 0;
				} else {
					pspSetK1(oldK1);
					return -1;
				}
			}
		}
	}

	sceRtcGetTick(&date, destTick);
	sceRtc_driver_77138347(destTick, destTick, (s64)(-tzMinutes));
	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_BDA60897(u64 *destTick, const char *dateString)
{
	int oldK1 = pspShiftK1();
	pspTime date;
	const char *p = dateString;
	s32 tzMinutes = 0;
	char c;

	if (!pspK1PtrOk(destTick) || !pspK1PtrOk(dateString)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	date.year = (u16)parse4Digits(p);
	if (p[4] != '-') {
		pspSetK1(oldK1);
		return -1;
	}
	date.month = (u16)parse2Digits(p + 5);
	if (p[7] != '-') {
		pspSetK1(oldK1);
		return -1;
	}
	date.day = (u16)parse2Digits(p + 8);
	if (p[10] != 'T' && p[10] != 't') {
		pspSetK1(oldK1);
		return -1;
	}
	date.hour = (u16)parse2Digits(p + 11);
	if (p[13] != ':') {
		pspSetK1(oldK1);
		return -1;
	}
	date.minutes = (u16)parse2Digits(p + 14);
	if (p[16] != ':') {
		pspSetK1(oldK1);
		return -1;
	}
	date.seconds = (u16)parse2Digits(p + 17);
	p += 19;
	if (*p == '.') {
		u32 scale = 100000;
		u32 usec = 0;
		p++;
		while ((u32)((*p - '0') & 0xff) < 10U) {
			usec += (u32)(*p - '0') * scale;
			if (scale == 100000)
				scale = 10000;
			else if (scale == 10000)
				scale = 1000;
			else if (scale == 1000)
				scale = 100;
			else if (scale == 100)
				scale = 10;
			else
				scale = (scale == 10) ? 1 : 0;
			p++;
		}
		date.microseconds = usec;
	} else {
		date.microseconds = 0;
	}

	c = *p++;
	if (c != 'Z' && c != 'z') {
		s32 tzH, tzM;
		if (c != '+' && c != '-') {
			pspSetK1(oldK1);
			return -1;
		}
		tzH = parse2Digits(p);
		tzM = parse2Digits(p + 3);
		if (tzH < 0 || tzM < 0 || p[2] != ':') {
			pspSetK1(oldK1);
			return -1;
		}
		tzMinutes = tzH * 60 + tzM;
		if (c == '-')
			tzMinutes = -tzMinutes;
	}

	sceRtcGetTick(&date, destTick);
	sceRtc_driver_77138347(destTick, destTick, (s64)(-tzMinutes));
	pspSetK1(oldK1);
	return 0;
}

static char *format2Digits(char *dst, int val)
{
	int q = val / 10;
	dst[0] = (char)((q % 10) + '0');
	dst[1] = (char)((val - q * 10) + '0');
	return dst + 2;
}

static char *format4Digits(char *dst, int val)
{
	int q1 = val / 10;
	int q2 = q1 / 10;
	int q3 = q2 / 10;
	dst[3] = (char)((val - q1 * 10) + '0');
	dst[2] = (char)((q1 - q2 * 10) + '0');
	dst[1] = (char)((q2 - q3 * 10) + '0');
	dst[0] = (char)((q3 % 10) + '0');
	return dst + 4;
}

s32 sceRtc_driver_1A86F5FD(char *pszDateTime, const u64 *pUtc, int iTimeZoneMinutes)
{
	int oldK1 = pspShiftK1();
	pspTime date;
	u64 tick;
	s32 wday;
	const char *wstr;
	char *p;
	int tzHour;

	if (!pspK1PtrOk(pszDateTime) || !pspK1PtrOk(pUtc)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (pUtc != NULL)
		tick = *pUtc;
	else
		sceRtcGetCurrentTick(&tick);

	sceRtc_driver_77138347(&tick, &tick, (s64)iTimeZoneMinutes);
	sceRtcSetTick(&date, &tick);
	wday = sceRtc_driver_321A839A(date.year, date.month, date.day);
	wstr = &g_weekday_abbr[wday * 3];

	pszDateTime[0] = wstr[0];
	pszDateTime[1] = wstr[1];
	pszDateTime[2] = wstr[2];
	pszDateTime[3] = ',';
	pszDateTime[4] = ' ';
	if (date.day < 10) {
		pszDateTime[5] = '0';
		pszDateTime[6] = (char)(date.day + '0');
	} else {
		pszDateTime[5] = (char)((date.day / 10) + '0');
		pszDateTime[6] = (char)((date.day % 10) + '0');
	}
	p = pszDateTime + 7;
	*p++ = ' ';
	*p++ = g_month_abbr[(date.month - 1) * 3 + 0];
	*p++ = g_month_abbr[(date.month - 1) * 3 + 1];
	*p++ = g_month_abbr[(date.month - 1) * 3 + 2];
	*p++ = ' ';
	p = format4Digits(p, date.year);
	*p++ = ' ';
	p = format2Digits(p, date.hour);
	*p++ = ':';
	p = format2Digits(p, date.minutes);
	*p++ = ':';
	p = format2Digits(p, date.seconds);
	*p++ = ' ';
	if (iTimeZoneMinutes < 0) {
		iTimeZoneMinutes = -iTimeZoneMinutes;
		*p++ = '-';
	} else {
		*p++ = '+';
	}
	tzHour = iTimeZoneMinutes / 60;
	p = format2Digits(p, tzHour);
	p = format2Digits(p, iTimeZoneMinutes - tzHour * 60);
	*p = '\0';

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_1FCE9E23(char *pszDateTime, const u64 *pUtc, int iTimeZoneMinutes)
{
	int oldK1 = pspShiftK1();
	pspTime date;
	u64 tick;
	char *p;

	if (!pspK1PtrOk(pszDateTime) || !pspK1PtrOk(pUtc)) {
		pspSetK1(oldK1);
		return SCE_ERROR_PRIV_REQUIRED;
	}

	if (pUtc != NULL)
		tick = *pUtc;
	else
		sceRtcGetCurrentTick(&tick);

	sceRtc_driver_77138347(&tick, &tick, (s64)iTimeZoneMinutes);
	sceRtcSetTick(&date, &tick);

	p = format4Digits(pszDateTime, date.year);
	*p++ = '-';
	p = format2Digits(p, date.month);
	*p++ = '-';
	p = format2Digits(p, date.day);
	*p++ = 'T';
	p = format2Digits(p, date.hour);
	*p++ = ':';
	p = format2Digits(p, date.minutes);
	*p++ = ':';
	p = format2Digits(p, date.seconds);
	*p++ = '.';
	p = format2Digits(p, (int)(date.microseconds / 10000U));
	if (iTimeZoneMinutes == 0) {
		*p++ = 'Z';
	} else {
		int tzHour;
		if (iTimeZoneMinutes < 0) {
			iTimeZoneMinutes = -iTimeZoneMinutes;
			*p++ = '-';
		} else {
			*p++ = '+';
		}
		tzHour = iTimeZoneMinutes / 60;
		p = format2Digits(p, tzHour);
		*p++ = ':';
		p = format2Digits(p, iTimeZoneMinutes - tzHour * 60);
	}
	*p = '\0';

	pspSetK1(oldK1);
	return 0;
}

s32 sceRtc_driver_27FAEC90(char *pszDateTime, const u64 *pUtc)
{
	int oldK1 = pspShiftK1();
	s32 res = SCE_ERROR_PRIV_REQUIRED;

	if (pspK1PtrOk(pszDateTime) && pspK1PtrOk(pUtc))
		res = sceRtc_driver_1A86F5FD(pszDateTime, pUtc, g_rtc_ctx.dUnk + g_rtc_ctx.cUnk);

	pspSetK1(oldK1);
	return res;
}

s32 sceRtc_driver_8DED141A(char *pszDateTime, const u64 *pUtc)
{
	int oldK1 = pspShiftK1();
	s32 res = SCE_ERROR_PRIV_REQUIRED;

	if (pspK1PtrOk(pszDateTime) && pspK1PtrOk(pUtc))
		res = sceRtc_driver_1FCE9E23(pszDateTime, pUtc, g_rtc_ctx.dUnk + g_rtc_ctx.cUnk);

	pspSetK1(oldK1);
	return res;
}

static s32 getTimeOfDayHandler(u32 *tv, u32 *tz)
{
	u64 tick = 0;
	pspTime date;

	sceRtcGetCurrentTick(&tick);
	sceRtcSetTick(&date, &tick);
	if (tv != NULL) {
		tv[1] = date.microseconds;
		tv[0] = (u32)date.hour * 3600U + (u32)date.minutes * 60U + (u32)date.seconds;
	}
	if (tz != NULL) {
		tz[1] = 0;
		tz[0] = 0;
	}
	return 0;
}

static u32 clockHandler(void)
{
	u64 tick = 0;

	sceRtcGetCurrentTick(&tick);
	return (u32)tick;
}

static void timeHandler(u32 *timeOut)
{
	u64 tick = 0;
	pspTime date;
	u32 t = 0;

	sceRtcGetCurrentTick(&tick);
	sceRtcSetTick(&date, &tick);
	sceRtc_driver_E86D8FC0(&date, &t);
	if (timeOut != NULL)
		*timeOut = t;
}

static void registerRtcFunctions(void)
{
	sceKernelRegisterRtcFunc(clockHandler, timeHandler,
				 getTimeOfDayHandler, sceRtc_driver_CEEF238F);
}
