/*
 * trigger_mgr_internal.h: internal database layout, IPC wire formats, locks,
 * and shared helpers for trigger_mgr.cpp, periodic_timer.cpp, and time_of_day_timer.cpp.
 */
#ifndef SHELL_TRIGGER_MGR_INTERNAL_H
#define SHELL_TRIGGER_MGR_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <ipmi/server.h>
#include "trigger_mgr.h"

/* Error codes returned by SceTriggerMgr (0x801036xx and 0x801026xx) */
#define TRIGGER_MGR_ERROR_INTERNAL		(-2146421248)	/* 0x80103600 */
#define TRIGGER_MGR_ERROR_BUSY			(-2146421247)	/* 0x80103601 */
#define TRIGGER_MGR_ERROR_NO_MEMORY		(-2146421245)	/* 0x80103603 */
#define TRIGGER_MGR_ERROR_APP_NOT_FOUND		(-2146421232)	/* 0x80103610 */
#define TRIGGER_MGR_ERROR_APP_TABLE_FULL	(-2146421231)	/* 0x80103611 */
#define TRIGGER_MGR_ERROR_SERVICE_NOT_FOUND	(-2146421230)	/* 0x80103612 */
#define TRIGGER_MGR_ERROR_TIMER_NOT_FOUND	(-2146421216)	/* 0x80103620 */
#define TRIGGER_MGR_ERROR_TIMER_TYPE_MISMATCH	(-2146421213)	/* 0x80103623 */

#define TRIGGER_MGR_ERROR_DIALOG_XML_NOT_FOUND	(-2146425296)	/* 0x80102630 */
#define TRIGGER_MGR_ERROR_DIALOG_XML_TOO_LARGE	(-2146425295)	/* 0x80102631 */
#define TRIGGER_MGR_ERROR_DIALOG_XML_INVALID	(-2146425294)	/* 0x80102632 */
#define TRIGGER_MGR_ERROR_DIALOG_CANCELED	(-2146425288)	/* 0x80102638 */
#define TRIGGER_MGR_ERROR_DIALOG_TIMEOUT	(-2146425287)	/* 0x80102639 */
#define TRIGGER_MGR_ERROR_DIALOG_REJECTED	(-2146425286)	/* 0x8010263A */
#define TRIGGER_MGR_ERROR_DB_CORRUPT		(-2146425280)	/* 0x80102640 */
#define TRIGGER_MGR_ERROR_MISSED		(-2146425279)	/* 0x80102641 */
#define TRIGGER_MGR_ERROR_DISABLED		(-2146425278)	/* 0x80102642 */

#define TRIGGER_MGR_ERROR_UNSUPPORTED_METHOD	(-2147351103)	/* 0x800201C1 */

/* Capacity constants matching ur0:shell/trigger_table.bin (85856 = 0x14F60 bytes) */
#define TRIGGER_MGR_MAX_SERVICES		32
#define TRIGGER_MGR_MAX_APPS			100
#define TRIGGER_MGR_MAX_PERIODIC_SLOTS		4
#define TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS		8
#define TRIGGER_MGR_MAX_IPC_JOBS		16
#define TRIGGER_MGR_TITLE_ID_LEN		10
#define TRIGGER_MGR_NOTIFY_EVENT_FLAG		49

/* App/service record flag bits */
#define TRIGGER_APP_FLAG_ENABLED		0x80000000u

/* Timer entry flag bits */
#define TRIGGER_TIMER_FLAG_FIRED		0x00000001u
#define TRIGGER_TIMER_FLAG_PERSIST_ON_SLEEP	0x00000001u
#define TRIGGER_TIMER_FLAG_WAKE_UP		0x00000002u
#define TRIGGER_TIMER_FLAG_WAIT_LAUNCH		0x00000004u
#define TRIGGER_TIMER_FLAG_IS_DAILY		0x00000008u
#define TRIGGER_TIMER_FLAG_RESUME_ONLY		0x00000010u
#define TRIGGER_TIMER_FLAG_PERSIST_IF_LATE	0x00000020u

/* Service state values */
enum TriggerMgrServiceState {
	TRIGGER_SVC_STATE_UNSTARTED = 0,
	TRIGGER_SVC_STATE_SUSPENDED = 1,
	TRIGGER_SVC_STATE_RUNNING   = 2
};

/* IPMI method IDs on SceShellSvc channel 0x00100000 */
enum TriggerMgrIpmiMethod {
	TRIGGER_METHOD_REGISTER_PERIODIC_TIMER			= 0x00100000,
	TRIGGER_METHOD_UNREGISTER_PERIODIC_TIMER		= 0x00100001,
	TRIGGER_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME	= 0x00100002,
	TRIGGER_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME_RESULT	= 0x00100003,
	TRIGGER_METHOD_GET_PERIODIC_TIMER_LAST_ERROR		= 0x00100004,
	TRIGGER_METHOD_REGISTER_TIME_OF_DAY_TIMER		= 0x00100005,
	TRIGGER_METHOD_UNREGISTER_TIME_OF_DAY_TIMER		= 0x00100006,
	TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_TIME		= 0x00100007,
	TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_TIME_RESULT	= 0x00100008,
	TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_LAST_ERROR		= 0x00100009,
	TRIGGER_METHOD_REGISTER_DAILY_TIMER			= 0x0010000A,
	TRIGGER_METHOD_UNREGISTER_DAILY_TIMER			= 0x0010000B,
	TRIGGER_METHOD_GET_DAILY_TIMER_INFO			= 0x0010000C,
	TRIGGER_METHOD_GET_DAILY_TIMER_INFO_RESULT		= 0x0010000D,
	TRIGGER_METHOD_GET_DAILY_TIMER_LAST_ERROR		= 0x0010000E,
	TRIGGER_METHOD_GET_POLICY				= 0x0010000F,
	TRIGGER_METHOD_GET_POLICY_RESULT			= 0x00100010,
	TRIGGER_METHOD_GET_RESULT				= 0x00100011,
	TRIGGER_METHOD_SET_TITLE_FLAG				= 0x00100012,
	TRIGGER_METHOD_GET_APP_TITLE_ID_LIST			= 0x00100013,
	TRIGGER_METHOD_GET_APP_INFO				= 0x00100014,
	TRIGGER_METHOD_GET_APP_INFO_RESULT			= 0x00100015,
	TRIGGER_METHOD_GET_SYSTEM_TITLE_ID_LIST			= 0x00100016,
	TRIGGER_METHOD_GET_SYSTEM_SERVICE_INFO			= 0x00100017,
	TRIGGER_METHOD_GET_SYSTEM_SERVICE_INFO_RESULT		= 0x00100018
};

/* Built-in service database record (32 bytes) */
struct TriggerServiceRecord {
	char serviceId[16];
	SceUInt32 flags;
	SceUInt32 reserved[3];
};

/* Registered application database record (48 bytes) */
struct TriggerAppRecord {
	char titleId[16];
	SceUInt32 flags;
	SceUInt32 registrationOrder;
	SceUInt32 reserved[6];
};

/* Periodic timer database record (72 bytes) */
struct TriggerPeriodicTimerRecord {
	char targetTitleId[16];
	SceUInt32 flags;
	SceInt32 lastError;
	SceUInt16 initialDelayMinutes;
	SceUInt16 intervalMinutes;
	SceUInt32 alignmentPad;
	SceUInt64 nextFireTick;
	SceUInt8 reserved[32];
};

/* Time-of-day / daily timer database record (64 bytes) */
struct TriggerTimeOfDayTimerRecord {
	char targetTitleId[16];
	SceUInt32 flags;
	SceInt32 lastError;
	SceUInt64 scheduledTime;
	SceUInt8 reserved[32];
};

/* Full persistent database (ur0:shell/trigger_table.bin, 85856 = 0x14F60 bytes) */
struct TriggerDatabase {
	SceUInt32 checksum;
	SceUInt32 version;
	SceUInt32 sequenceCounter;
	SceUInt32 headerReserved[5];
	TriggerServiceRecord services[TRIGGER_MGR_MAX_SERVICES];
	TriggerAppRecord apps[TRIGGER_MGR_MAX_APPS];
	TriggerPeriodicTimerRecord periodicTimers[TRIGGER_MGR_MAX_APPS][TRIGGER_MGR_MAX_PERIODIC_SLOTS];
	TriggerTimeOfDayTimerRecord timeOfDayTimers[TRIGGER_MGR_MAX_APPS][TRIGGER_MGR_MAX_TIMEOFDAY_SLOTS];
};

/* IPC wire structs exchanged with SceTriggerUtil */
struct TriggerTimerRequestHeader {
	SceUInt32 timerSlot;
	bool useCallback;
};

struct TriggerInfoRequestHeader {
	SceUInt32 language;
	bool useCallback;
};

struct TriggerPeriodicTimerParam {
	SceUInt32 version;
	SceUInt32 initialDelayMinutes;
	SceUInt32 intervalMinutes;
	bool persistOnSleep;
	bool wakeUp;
	bool waitLaunchResult;
	bool resumeOnly;
	SceUInt8 reserved[64];
};

struct TriggerTimeOfDayTimerParam {
	SceUInt64 headerReserved;
	SceRtcTick targetTick;
	bool wakeUp;
	bool waitLaunchResult;
	bool resumeOnly;
	bool persistIfLate;
	SceUInt8 reserved[68];
};

struct TriggerDailyTimerParam {
	SceUInt32 version;
	SceUInt32 secondOfDay;
	bool wakeUp;
	bool waitLaunchResult;
	bool resumeOnly;
	SceUInt8 alignmentPad;
	SceUInt16 dayOfWeekMask;
	SceUInt8 reserved[66];
};

struct TriggerAppInfoResponse {
	wchar_t title[52];
	char iconPath[1024];
	bool enabled;
	SceUInt8 alignmentPad;
};

struct TriggerServiceInfoResponse {
	wchar_t name[256];
	char iconPath[1024];
	bool enabled;
	SceUInt8 alignmentPad;
};

/* Runtime state for an active async IPC job slot (16 bytes) */
struct TriggerIpcJobSlot {
	IPMI::Session *session;
	SceUInt32 requestId;
	paf::common::SharedPtr<paf::job::JobItem> job;
};

/* Base class for IPC jobs carrying session + requestId */
class TriggerIpcJobBase : public paf::job::JobItem {
public:
	TriggerIpcJobBase(const char *name, IPMI::Session *session,
			  SceUInt32 requestId, bool useCallback)
		: paf::job::JobItem(name, NULL),
		  m_session(session),
		  m_requestId(requestId),
		  m_useCallback(useCallback)
	{
	}

	virtual ~TriggerIpcJobBase() {}
	virtual void Finish(int result) { (void)result; }

	IPMI::Session *m_session;
	SceUInt32 m_requestId;
	bool m_useCallback;
};

/* Base class for timer fire jobs carrying launch completion state */
class TriggerFireJobBase : public paf::job::JobItem {
public:
	TriggerFireJobBase(const char *appTitleId, SceUInt32 timerSlot)
		: paf::job::JobItem("SceTriggerMgrJob", NULL),
		  m_launchCallbackDone(false),
		  m_launchResult(TRIGGER_MGR_ERROR_INTERNAL),
		  m_timerSlot(timerSlot)
	{
		sce_paf_strncpy(m_appTitleId, appTitleId, TRIGGER_MGR_TITLE_ID_LEN);
		m_appTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
	}

	virtual ~TriggerFireJobBase() {}
	virtual void Finish(int result) { (void)result; }

	bool m_launchCallbackDone;
	SceInt32 m_launchResult;
	char m_appTitleId[12];
	SceUInt32 m_timerSlot;
};

/* Shared globals owned by trigger_mgr.cpp */
extern paf::memory::HeapAllocator *g_triggerMgrHeap;
extern paf::job::JobQueue *g_triggerMgrJobQueue;
extern paf::thread::RWLock g_triggerMgrTableLock;
extern paf::thread::Mutex g_triggerMgrIpcLock;
extern TriggerDatabase g_triggerDb;

/* Shared utility functions in trigger_mgr.cpp */
int triggerMgrAssertFail(const char *tag, int errorCode);
void triggerMgrAssertFailNoCode(void);
void *triggerMgrHeapAlloc(SceSize size);
void triggerMgrHeapFree(void *ptr);

int triggerMgrSaveDatabase(void);
TriggerAppRecord *triggerMgrGetAppRecord(int appIndex);
int triggerMgrFindAppIndex(const char *titleId);
int triggerMgrFindFreeAppIndex(void);
SceUInt32 triggerMgrGetSequenceCounter(void);
int triggerMgrResolveProcessTitleId(SceUID processId, char *outTitleId);
bool triggerMgrIsTitleInstalled(const char *titleId);
int triggerMgrLaunchOrResumeApp(TriggerFireJobBase *fireJob, const char *titleId,
				const char *launchArg, bool waitLaunchResult,
				bool resumeOnly);

TriggerIpcJobSlot *triggerMgrFindIpcSlot(IPMI::Session *session, SceUInt32 requestId);
TriggerIpcJobSlot *triggerMgrAllocIpcSlot(IPMI::Session *session, SceUInt32 requestId,
					  paf::common::SharedPtr<paf::job::JobItem> job);
int triggerMgrClearIpcSlot(TriggerIpcJobSlot *slot);
void triggerMgrCompleteSimpleAsyncJob(TriggerIpcJobBase *job, int result);

int triggerMgrRespondRemainingTime(IPMI::Session *session, SceUInt32 requestId,
				   int result, const SceUInt32 *remainingMinutes);
int triggerMgrRespondTimeOfDayTime(IPMI::Session *session, SceUInt32 requestId,
				   int result, const SceRtcTick *targetTick,
				   const bool *expired);
int triggerMgrRespondDailyTimerInfo(IPMI::Session *session, SceUInt32 requestId,
				    int result, const char *targetTitleId,
				    const TriggerDailyTimerParam *timerParam,
				    const SceInt32 *remainingSeconds);
int triggerMgrRespondPolicy(IPMI::Session *session, SceUInt32 requestId,
			    int result, const SceUInt8 *policy);
int triggerMgrRespondResultOnly(IPMI::Session *session, SceUInt32 requestId, int result);
int triggerMgrRespondAppInfo(IPMI::Session *session, SceUInt32 requestId,
			     int result, const TriggerAppInfoResponse *appInfo);
int triggerMgrRespondServiceInfo(IPMI::Session *session, SceUInt32 requestId,
				 int result, const TriggerServiceInfoResponse *serviceInfo);

/* External SceShellSvc event notifier (0x811B7560) */
extern int shellSvcNotifyEvent(IPMI::Session *session, int eventId);

/* periodic_timer.cpp entry points */
int triggerPeriodicTimerRegisterAll(void);
void triggerPeriodicTimerCancelAllForApp(int appIndex);
int triggerPeriodicTimerSetWakeUpForApp(int appIndex, bool wakeUp);
int triggerPeriodicTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
					const char *targetTitleId,
					const TriggerPeriodicTimerParam *timerParam,
					SceUInt32 timerSlot, bool useCallback);
int triggerPeriodicTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
					  SceUInt32 timerSlot, bool useCallback);
int triggerPeriodicTimerEnqueueGetRemainingTime(IPMI::Session *session, SceUInt32 requestId,
						SceUInt32 timerSlot, bool useCallback);
int triggerPeriodicTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					    SceUInt32 timerSlot, bool useCallback);

/* time_of_day_timer.cpp entry points */
int triggerTimeOfDayTimerRegisterAll(void);
void triggerTimeOfDayTimerCancelAllForApp(int appIndex);
int triggerTimeOfDayTimerSetWakeUpForApp(int appIndex, bool wakeUp);
int triggerTimeOfDayTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
					 const char *targetTitleId,
					 const TriggerTimeOfDayTimerParam *timerParam,
					 SceUInt32 timerSlot, bool useCallback);
int triggerTimeOfDayTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 timerSlot, bool useCallback);
int triggerTimeOfDayTimerEnqueueGetTime(IPMI::Session *session, SceUInt32 requestId,
					SceUInt32 timerSlot, bool useCallback);
int triggerTimeOfDayTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					     SceUInt32 timerSlot, bool useCallback);
int triggerDailyTimerEnqueueRegister(IPMI::Session *session, SceUInt32 requestId,
				     const char *targetTitleId,
				     const TriggerDailyTimerParam *timerParam,
				     SceUInt32 timerSlot, bool useCallback);
int triggerDailyTimerEnqueueUnregister(IPMI::Session *session, SceUInt32 requestId,
				       SceUInt32 timerSlot, bool useCallback);
int triggerDailyTimerEnqueueGetInfo(IPMI::Session *session, SceUInt32 requestId,
				    SceUInt32 timerSlot, bool useCallback);
int triggerDailyTimerEnqueueGetLastError(IPMI::Session *session, SceUInt32 requestId,
					 SceUInt32 timerSlot, bool useCallback);

#endif
