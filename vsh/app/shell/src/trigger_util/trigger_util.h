/*
 * SceTriggerUtil (vs0:sys/external/trigger_util.suprx): an application's timers and the shell's
 * lists of the applications that use them, through the shell's IPMI service (SceShellSvc). The
 * export names are Sony's where 1.06's asserts give them; the types, constants and the
 * other names are ours.
 */
#ifndef TRIGGER_UTIL_TRIGGER_UTIL_H
#define TRIGGER_UTIL_TRIGGER_UTIL_H

#include <scetypes.h>
#include <rtc.h>

/* the module's errors (0x801036xx) */
#define SCE_TRIGGER_UTIL_ERROR_NO_ASYNC_CALL	-2146421248	/* 0x80103600 */
#define SCE_TRIGGER_UTIL_ERROR_BUSY		-2146421247	/* 0x80103601 */
#define SCE_TRIGGER_UTIL_ERROR_NOT_BUSY		-2146421246	/* 0x80103602 */
#define SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER	-2146421244	/* 0x80103604 */
#define SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT	-2146421243	/* 0x80103605 */

/* timers per application */
#define SCE_TRIGGER_UTIL_PERIODIC_TIMER_COUNT		4
#define SCE_TRIGGER_UTIL_TIME_OF_DAY_TIMER_COUNT	8
#define SCE_TRIGGER_UTIL_DAILY_TIMER_COUNT		8

/* a daily timer's days: one bit a weekday, Sunday first */
#define SCE_TRIGGER_UTIL_DAILY_TIMER_DAYS_MASK		0x7F
/* the last second of a day */
#define SCE_TRIGGER_UTIL_DAILY_TIMER_TIME_MAX		86399

/* where an asynchronous call's result goes; pArg is the caller's */
typedef void (*SceTriggerUtilCallback)(int result, void *pArg);

typedef struct SceTriggerUtilPeriodicTimerParam {
	SceUInt8 reserved0[8];
	SceUInt32 interval;		/* never 0 */
	SceUInt8 reserved[68];
} SceTriggerUtilPeriodicTimerParam;

typedef struct SceTriggerUtilTimeOfDayTimerParam {
	SceUInt8 reserved[88];
} SceTriggerUtilTimeOfDayTimerParam;

typedef struct SceTriggerUtilDailyTimerParam {
	SceUInt32 reserved0;
	SceUInt32 time;			/* seconds after midnight */
	SceUInt32 reserved1;
	SceUInt16 days;			/* SCE_TRIGGER_UTIL_DAILY_TIMER_DAYS_MASK bits, not none */
	SceUInt8 reserved[66];
} SceTriggerUtilDailyTimerParam;

/* what sceTriggerUtilGetAppInfo and sceTriggerUtilGetSystemServiceInfo fill */
typedef struct SceTriggerUtilAppInfo {
	SceUInt8 data[1130];
} SceTriggerUtilAppInfo;

typedef struct SceTriggerUtilSystemServiceInfo {
	SceUInt8 data[1538];
} SceTriggerUtilSystemServiceInfo;

/* the shell's title ID lists: ten bytes a title ID */
#define SCE_TRIGGER_UTIL_APP_TITLE_ID_LIST_SIZE		1000
#define SCE_TRIGGER_UTIL_SYSTEM_TITLE_ID_LIST_SIZE	320

/* what SceTriggerUtil_DF3ED08E fills before the timer's parameters */
typedef struct SceTriggerUtilDailyTimerState {
	SceUInt8 data[16];
} SceTriggerUtilDailyTimerState;

#ifdef __cplusplus
extern "C" {
#endif

/* runs the module's callback when its asynchronous call is done */
int SceTriggerUtil_845BF533(void);

/*
 * Each timer call is asynchronous with a callback (the result goes to it) and synchronous
 * without one; one call runs at a time.
 */
int sceTriggerUtilRegisterPeriodicTimer(const char *titleId,
					const SceTriggerUtilPeriodicTimerParam *param,
					SceUInt32 timerId, SceTriggerUtilCallback callback,
					void *pArg);
int sceTriggerUtilUnregisterPeriodicTimer(SceUInt32 timerId, SceTriggerUtilCallback callback,
					  void *pArg);
int sceTriggerUtilGetPeriodicTimerRemainingTime(SceUInt32 timerId, SceUInt32 *pRemainingTime,
						SceTriggerUtilCallback callback, void *pArg);
int sceTriggerUtilGetPeriodicTimerLastError(SceUInt32 timerId, SceTriggerUtilCallback callback,
					    void *pArg);
int sceTriggerUtilRegisterTimeOfDayTimer(const char *titleId,
					 const SceTriggerUtilTimeOfDayTimerParam *param,
					 SceUInt32 timerId, SceTriggerUtilCallback callback,
					 void *pArg);
int sceTriggerUtilUnregisterTimeOfDayTimer(SceUInt32 timerId, SceTriggerUtilCallback callback,
					   void *pArg);
int sceTriggerUtilGetTimeOfDayTimerTime(SceUInt32 timerId, SceRtcTick *pTime, bool *pFlag,
					SceTriggerUtilCallback callback, void *pArg);
int sceTriggerUtilGetTimeOfDayTimerLastError(SceUInt32 timerId,
					     SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_E2B2D2D0(char *titleIdList, SceUInt32 *pCount);
int sceTriggerUtilGetAppInfo(const char *titleId, SceUInt32 option,
			     SceTriggerUtilAppInfo *pInfo, SceTriggerUtilCallback callback,
			     void *pArg);
int SceTriggerUtil_2A67EF8C(char *titleIdList, SceUInt32 *pCount);
int sceTriggerUtilGetSystemServiceInfo(const char *titleId, SceUInt32 option,
				       SceTriggerUtilSystemServiceInfo *pInfo,
				       SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_257BA954(const char *titleId, bool flag);
int sceTriggerUtilGetPolicy(SceUInt8 *pPolicy, SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_A69102DE(const char *titleId, const SceTriggerUtilDailyTimerParam *param,
			    SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_58BF700F(SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_DF3ED08E(SceUInt32 timerId, SceTriggerUtilDailyTimerState *pState,
			    SceTriggerUtilDailyTimerParam *param, SceUInt32 *pStatus,
			    SceTriggerUtilCallback callback, void *pArg);
int SceTriggerUtil_2AD6A378(SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg);

#ifdef __cplusplus
}
#endif

#endif
