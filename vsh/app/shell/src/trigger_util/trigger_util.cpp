/*
 * SceTriggerUtil (vs0:sys/external/trigger_util.suprx): an application's timers and the shell's
 * lists of the applications that use them, through the shell's IPMI service (SceShellSvc).
 * One call runs at a time, in the async call slot: with a callback the call returns at once and
 * the shell's callback event collects the result for the callback; without one the call waits
 * for the shell. Sony's file: vsh/app/shell/src/trigger_util/trigger_util.cpp (the build path
 * in 1.06's asserts, which name _sceTriggerUtilInit, _sceTriggerUtilTerm, lock, unlock and
 * release_async_call_slot_without_lock); the classes and the other names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "../../../../lowlayer/src/shell_svc/shell_svc_client.h"
#include "../../../../lowlayer/src/tinylibc/tinylibc.h"
#include "trigger_util.h"
#include "trigger_util_ipc_client.h"

/* the ShellSvc events: the one a synchronous call waits on, the one that runs the callback */
#define TRIGGER_UTIL_WAIT_EVENT		0
#define TRIGGER_UTIL_CALLBACK_EVENT	49

/* a call that must not fail stops at a breakpoint */
#define TRIGGER_UTIL_ASSERT(expr)						\
	do {									\
		int _ret = (expr);						\
		if (_ret != 0) {						\
			sceClibPrintf("[SceTriggerUtil] ASSERION FAILURE (0x%x)\n", _ret); \
			__breakpoint(0);					\
		}								\
	} while (0)

/* An asynchronous call in the slot: its request, and where its result goes. */
class AsyncCall {
public:
	int m_requestId;

	virtual ~AsyncCall() {}
	/* the shell's result, collected */
	virtual int getResult() = 0;
};

/* a call that answers only its result */
class ResultCall : public AsyncCall {
public:
	virtual int getResult()
	{
		return triggerUtilGetResult(SceShellSvc_B31E7F1C(), m_requestId);
	}
};

class AppInfoCall : public AsyncCall {
public:
	SceTriggerUtilAppInfo *m_pInfo;

	virtual int getResult()
	{
		return triggerUtilGetAppInfoResult(SceShellSvc_B31E7F1C(), m_requestId, m_pInfo);
	}
};

class SystemServiceInfoCall : public AsyncCall {
public:
	SceTriggerUtilSystemServiceInfo *m_pInfo;

	virtual int getResult()
	{
		return triggerUtilGetSystemServiceInfoResult(SceShellSvc_B31E7F1C(), m_requestId,
							     m_pInfo);
	}
};

class PeriodicTimerRemainingTimeCall : public AsyncCall {
public:
	SceUInt32 *m_pRemainingTime;

	virtual int getResult()
	{
		return triggerUtilGetPeriodicTimerRemainingTimeResult(SceShellSvc_B31E7F1C(),
								      m_requestId,
								      m_pRemainingTime);
	}
};

class TimeOfDayTimerTimeCall : public AsyncCall {
public:
	SceRtcTick *m_pTime;
	bool *m_pFlag;

	virtual int getResult()
	{
		return triggerUtilGetTimeOfDayTimerTimeResult(SceShellSvc_B31E7F1C(), m_requestId,
							      m_pTime, m_pFlag);
	}
};

class DailyTimerInfoCall : public AsyncCall {
public:
	SceTriggerUtilDailyTimerState *m_pState;
	SceTriggerUtilDailyTimerParam *m_pParam;
	SceUInt32 *m_pStatus;

	virtual int getResult()
	{
		return triggerUtilGetDailyTimerInfoResult(SceShellSvc_B31E7F1C(), m_requestId,
							  m_pState, m_pParam, m_pStatus);
	}
};

class PolicyCall : public AsyncCall {
public:
	SceUInt8 *m_pPolicy;

	virtual int getResult()
	{
		return triggerUtilGetPolicyResult(SceShellSvc_B31E7F1C(), m_requestId, m_pPolicy);
	}
};

static ResultCall s_resultCall;
static AppInfoCall s_appInfoCall;
static SystemServiceInfoCall s_systemServiceInfoCall;
static PeriodicTimerRemainingTimeCall s_periodicTimerRemainingTimeCall;
static TimeOfDayTimerTimeCall s_timeOfDayTimerTimeCall;
static DailyTimerInfoCall s_dailyTimerInfoCall;
static PolicyCall s_policyCall;

static SceKernelLwMutexWork s_lock;
/* the async call slot: in use (1), the callback and its argument, the call */
static int s_asyncCallSlotUsed;
static SceTriggerUtilCallback s_callback;
static void *s_callbackArg;
static AsyncCall *s_asyncCall;

/* module_start and module_stop call them whole in Sony's build */
static __attribute__((noinline)) int _sceTriggerUtilInit(void);
static __attribute__((noinline)) int _sceTriggerUtilTerm(void);

extern "C" int module_start(SceSize args, const void *argp)
{
	tinyLibcInit();
	return _sceTriggerUtilInit();
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	int ret;

	ret = _sceTriggerUtilTerm();
	if (ret == 0)
		tinyLibcTerm();
	return ret;
}

extern "C" int module_exit(void)
{
	return 0;
}

static inline void lock(void)
{
	TRIGGER_UTIL_ASSERT(sceKernelLockLwMutex(&s_lock, 1, NULL));
}

static inline void unlock(void)
{
	TRIGGER_UTIL_ASSERT(sceKernelUnlockLwMutex(&s_lock, 1));
}

static int eventCallback(int event, void *pArg);

static int _sceTriggerUtilInit(void)
{
	int ret;

	ret = sceKernelCreateLwMutex(&s_lock, "SceTriggerUtil", SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0,
				     NULL);
	if (ret != 0)
		return ret;
	TRIGGER_UTIL_ASSERT(SceShellSvc_1FAB9A1E(TRIGGER_UTIL_CALLBACK_EVENT));
	TRIGGER_UTIL_ASSERT(SceShellSvc_D943CE15(TRIGGER_UTIL_CALLBACK_EVENT, eventCallback, NULL));
	TRIGGER_UTIL_ASSERT(SceShellSvc_B452A177(TRIGGER_UTIL_WAIT_EVENT));
	s_callback = NULL;
	s_callbackArg = NULL;
	s_asyncCall = NULL;
	return 0;
}

static int _sceTriggerUtilTerm(void)
{
	int ret;

	TRIGGER_UTIL_ASSERT(SceShellSvc_7CBF442B(TRIGGER_UTIL_CALLBACK_EVENT));
	ret = sceKernelDeleteLwMutex(&s_lock);
	if (ret != 0)
		return ret;
	return 0;
}

int SceTriggerUtil_845BF533(void)
{
	SceShellSvc_FB03FBC9(TRIGGER_UTIL_CALLBACK_EVENT);
	return 0;
}

/* empties the slot and drops the events its call may have left */
static inline int release_async_call_slot_without_lock(void)
{
	if (s_asyncCallSlotUsed != 1)
		return SCE_TRIGGER_UTIL_ERROR_NOT_BUSY;
	TRIGGER_UTIL_ASSERT(SceShellSvc_1FAB9A1E(TRIGGER_UTIL_CALLBACK_EVENT));
	TRIGGER_UTIL_ASSERT(SceShellSvc_B452A177(TRIGGER_UTIL_WAIT_EVENT));
	s_asyncCallSlotUsed = 0;
	s_callback = NULL;
	s_callbackArg = NULL;
	s_asyncCall = NULL;
	return 0;
}

static int release_async_call_slot(void)
{
	int ret;

	lock();
	ret = release_async_call_slot_without_lock();
	unlock();
	return ret;
}

/* takes the slot for call, or fails while another call has it */
static inline int acquire_async_call_slot(AsyncCall *call, SceTriggerUtilCallback callback,
					  void *pArg)
{
	lock();
	if (s_asyncCallSlotUsed != 0) {
		unlock();
		return SCE_TRIGGER_UTIL_ERROR_BUSY;
	}
	s_callback = callback;
	s_callbackArg = pArg;
	s_asyncCall = call;
	s_asyncCallSlotUsed = 1;
	unlock();
	return 0;
}

int sceTriggerUtilRegisterPeriodicTimer(const char *titleId,
					const SceTriggerUtilPeriodicTimerParam *param,
					SceUInt32 timerId, SceTriggerUtilCallback callback,
					void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (titleId == NULL || param == NULL)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER;
	if (timerId >= SCE_TRIGGER_UTIL_PERIODIC_TIMER_COUNT || param->interval == 0)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilRegisterPeriodicTimer(SceShellSvc_B31E7F1C(), &requestId,
					       useCallback ? NULL : &eventFlag, titleId, param,
					       timerId, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilUnregisterPeriodicTimer(SceUInt32 timerId, SceTriggerUtilCallback callback,
					  void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_PERIODIC_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilUnregisterPeriodicTimer(SceShellSvc_B31E7F1C(), &requestId,
						 useCallback ? NULL : &eventFlag, timerId,
						 useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilGetPeriodicTimerRemainingTime(SceUInt32 timerId, SceUInt32 *pRemainingTime,
						SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_PERIODIC_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	if (pRemainingTime == NULL)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER;
	ret = acquire_async_call_slot(&s_periodicTimerRemainingTimeCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetPeriodicTimerRemainingTime(SceShellSvc_B31E7F1C(), &requestId,
						       useCallback ? NULL : &eventFlag, timerId,
						       useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_periodicTimerRemainingTimeCall.m_requestId = requestId;
	s_periodicTimerRemainingTimeCall.m_pRemainingTime = pRemainingTime;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_periodicTimerRemainingTimeCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilGetPeriodicTimerLastError(SceUInt32 timerId, SceTriggerUtilCallback callback,
					    void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_PERIODIC_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetPeriodicTimerLastError(SceShellSvc_B31E7F1C(), &requestId,
						   useCallback ? NULL : &eventFlag, timerId,
						   useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilRegisterTimeOfDayTimer(const char *titleId,
					 const SceTriggerUtilTimeOfDayTimerParam *param,
					 SceUInt32 timerId, SceTriggerUtilCallback callback,
					 void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (titleId == NULL || param == NULL)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER;
	if (timerId >= SCE_TRIGGER_UTIL_TIME_OF_DAY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilRegisterTimeOfDayTimer(SceShellSvc_B31E7F1C(), &requestId,
						useCallback ? NULL : &eventFlag, titleId, param,
						timerId, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilUnregisterTimeOfDayTimer(SceUInt32 timerId, SceTriggerUtilCallback callback,
					   void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_TIME_OF_DAY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilUnregisterTimeOfDayTimer(SceShellSvc_B31E7F1C(), &requestId,
						  useCallback ? NULL : &eventFlag, timerId,
						  useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilGetTimeOfDayTimerTime(SceUInt32 timerId, SceRtcTick *pTime, bool *pFlag,
					SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_TIME_OF_DAY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	if (pTime == NULL || pFlag == NULL)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER;
	ret = acquire_async_call_slot(&s_timeOfDayTimerTimeCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetTimeOfDayTimerTime(SceShellSvc_B31E7F1C(), &requestId,
					       useCallback ? NULL : &eventFlag, timerId,
					       useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_timeOfDayTimerTimeCall.m_requestId = requestId;
	s_timeOfDayTimerTimeCall.m_pTime = pTime;
	s_timeOfDayTimerTimeCall.m_pFlag = pFlag;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_timeOfDayTimerTimeCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int sceTriggerUtilGetTimeOfDayTimerLastError(SceUInt32 timerId,
					     SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_TIME_OF_DAY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetTimeOfDayTimerLastError(SceShellSvc_B31E7F1C(), &requestId,
						    useCallback ? NULL : &eventFlag, timerId,
						    useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_E2B2D2D0(char *titleIdList, SceUInt32 *pCount)
{
	return triggerUtilGetAppTitleIdList(SceShellSvc_B31E7F1C(), titleIdList, pCount);
}

int sceTriggerUtilGetAppInfo(const char *titleId, SceUInt32 option,
			     SceTriggerUtilAppInfo *pInfo, SceTriggerUtilCallback callback,
			     void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	ret = acquire_async_call_slot(&s_appInfoCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetAppInfo(SceShellSvc_B31E7F1C(), &requestId,
				    useCallback ? NULL : &eventFlag, titleId, option, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_appInfoCall.m_requestId = requestId;
	s_appInfoCall.m_pInfo = pInfo;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_appInfoCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_2A67EF8C(char *titleIdList, SceUInt32 *pCount)
{
	return triggerUtilGetSystemTitleIdList(SceShellSvc_B31E7F1C(), titleIdList, pCount);
}

int sceTriggerUtilGetSystemServiceInfo(const char *titleId, SceUInt32 option,
				       SceTriggerUtilSystemServiceInfo *pInfo,
				       SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	ret = acquire_async_call_slot(&s_systemServiceInfoCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetSystemServiceInfo(SceShellSvc_B31E7F1C(), &requestId,
					      useCallback ? NULL : &eventFlag, titleId, option,
					      useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_systemServiceInfoCall.m_requestId = requestId;
	s_systemServiceInfoCall.m_pInfo = pInfo;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_systemServiceInfoCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_257BA954(const char *titleId, bool flag)
{
	return triggerUtilSetTitleFlag(SceShellSvc_B31E7F1C(), titleId, flag);
}

int sceTriggerUtilGetPolicy(SceUInt8 *pPolicy, SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	ret = acquire_async_call_slot(&s_policyCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetPolicy(SceShellSvc_B31E7F1C(), &requestId,
				   useCallback ? NULL : &eventFlag, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_policyCall.m_requestId = requestId;
	s_policyCall.m_pPolicy = pPolicy;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_policyCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_A69102DE(const char *titleId, const SceTriggerUtilDailyTimerParam *param,
			    SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (titleId == NULL || param == NULL)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_POINTER;
	if (timerId >= SCE_TRIGGER_UTIL_DAILY_TIMER_COUNT || param->days == 0 ||
	    (param->days & ~SCE_TRIGGER_UTIL_DAILY_TIMER_DAYS_MASK) != 0 ||
	    param->time > SCE_TRIGGER_UTIL_DAILY_TIMER_TIME_MAX)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilRegisterDailyTimer(SceShellSvc_B31E7F1C(), &requestId,
					    useCallback ? NULL : &eventFlag, titleId, param,
					    timerId, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_58BF700F(SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_DAILY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilUnregisterDailyTimer(SceShellSvc_B31E7F1C(), &requestId,
					      useCallback ? NULL : &eventFlag, timerId,
					      useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_DF3ED08E(SceUInt32 timerId, SceTriggerUtilDailyTimerState *pState,
			    SceTriggerUtilDailyTimerParam *param, SceUInt32 *pStatus,
			    SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	SceTriggerUtilDailyTimerState state;
	SceTriggerUtilDailyTimerParam timerParam;
	SceUInt32 status;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_DAILY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	/* the outputs the caller does not want go to the stack */
	if (pState == NULL)
		pState = &state;
	if (param == NULL)
		param = &timerParam;
	if (pStatus == NULL)
		pStatus = &status;
	ret = acquire_async_call_slot(&s_dailyTimerInfoCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetDailyTimerInfo(SceShellSvc_B31E7F1C(), &requestId,
					   useCallback ? NULL : &eventFlag, timerId, useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_dailyTimerInfoCall.m_requestId = requestId;
	s_dailyTimerInfoCall.m_pState = pState;
	s_dailyTimerInfoCall.m_pParam = param;
	s_dailyTimerInfoCall.m_pStatus = pStatus;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_dailyTimerInfoCall.getResult();
		release_async_call_slot();
	}
	return result;
}

int SceTriggerUtil_2AD6A378(SceUInt32 timerId, SceTriggerUtilCallback callback, void *pArg)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	bool useCallback;
	int result;
	int ret;

	if (timerId >= SCE_TRIGGER_UTIL_DAILY_TIMER_COUNT)
		return SCE_TRIGGER_UTIL_ERROR_INVALID_ARGUMENT;
	ret = acquire_async_call_slot(&s_resultCall, callback, pArg);
	if (ret != 0)
		return ret;
	result = 0;
	useCallback = callback != NULL;
	TRIGGER_UTIL_ASSERT(SceShellSvc_A0B067AC(&eventFlag, TRIGGER_UTIL_WAIT_EVENT));
	ret = triggerUtilGetDailyTimerLastError(SceShellSvc_B31E7F1C(), &requestId,
						useCallback ? NULL : &eventFlag, timerId,
						useCallback);
	if (ret != 0) {
		release_async_call_slot();
		return ret;
	}
	s_resultCall.m_requestId = requestId;
	if (!useCallback) {
		TRIGGER_UTIL_ASSERT(SceShellSvc_8D4F9F2E(TRIGGER_UTIL_WAIT_EVENT, NULL));
		result = s_resultCall.getResult();
		release_async_call_slot();
	}
	return result;
}

/* SceShellSvc's callback event: the slot's call is done; its result goes to the callback */
static int eventCallback(int event, void *pArg)
{
	SceTriggerUtilCallback callback;
	void *callbackArg;
	int result;

	lock();
	result = SCE_TRIGGER_UTIL_ERROR_NO_ASYNC_CALL;
	callback = s_callback;
	callbackArg = s_callbackArg;
	if (s_asyncCall != NULL) {
		result = s_asyncCall->getResult();
		release_async_call_slot_without_lock();
	}
	unlock();
	if (callback != NULL)
		callback(result, callbackArg);
	return 0;
}
