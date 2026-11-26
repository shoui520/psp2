/*
 * remote_download_lib.cpp: SceRemoteDownloadLibJob scheduler, status.dat
 * persistence, and network-tick fallback via SceNpManager clock sync
 * (0x81372BF0..0x81373367, job Run at 0x81449B40).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <job/job.h>
#include <rtc/rtc.h>
#include <thread/thread.h>
#include <kernel/iofilemgr.h>
#include <vshbridge/vshbridge.h>

#define REMOTEDOWNLOAD_ERR_NO_MEMORY		(-2146405631)	/* 0x80107301 */
#define REMOTEDOWNLOAD_ERR_NO_QUEUE		(-2146405552)	/* 0x80107350 */
#define REMOTEDOWNLOAD_RTC_ERR_NOT_INIT		(-2145054718)	/* 0x80251002 */

#define REMOTEDOWNLOAD_DEFAULT_INTERVAL_SEC	86400u
#define REMOTEDOWNLOAD_FLOCK_SHARED		1
#define REMOTEDOWNLOAD_FLOCK_EXCLUSIVE		2
#define REMOTEDOWNLOAD_FLOCK_UNLOCK		16

#define REMOTEDOWNLOAD_ACTION_RESET		0
#define REMOTEDOWNLOAD_ACTION_CHECK		2

#define REMOTEDOWNLOAD_OPEN_LOCK_FLAGS		0x202		/* SCE_O_RDWR | SCE_O_CREAT */
#define REMOTEDOWNLOAD_WRITE_FLAGS		0x602u		/* SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC */
#define REMOTEDOWNLOAD_FILE_MODE		6u

typedef void (*RemoteDownloadClockCallback)(void *userData);

struct RemoteDownloadDiskStatus {
	SceUInt64 lastCheckTick;
	SceUInt32 statusWord;
	SceUInt32 intervalSeconds;
};

struct RemoteDownloadState {
	SceUInt8 disabled;
	RemoteDownloadDiskStatus disk;
};

/* Helpers in download_plugin/src/podl_cache.cpp (0x8113D5CC, 0x8113D6F8, 0x8113D8C2) */
int Bgdl_8113D5CC(paf::string *path);
int Bgdl_8113D6F8(const char *path, void *buf, SceSize size);
int Bgdl_8113D8C2(const char *path, const void *buf, SceSize size, SceUInt32 flags, SceUInt32 mode);

/* Shell event callback (0x81004634 -> 0x81372EE0) */
extern int shellRemoteDownloadOnEvent(void);

extern "C" {
int sceAppMgrLaunchAppByName(SceUInt32 flags, const char *name, const char *param);
int sceIoCloseForUser(SceUID fd);
int SceNpManager_27112D01(void);
int SceNpManager_E41932D4(int *outReqId, int threadPriority);
int SceNpManager_D1D191FB(int reqId);
int SceNpManager_96534006(int reqId, int *outResult);
int SceNpManager_FAEA7A73(void);
}

/* (0x8154899C) Active remote-download status loaded for the current job */
static RemoteDownloadState *s_remoteDownloadState = NULL;

/* (0x815489A0, 0x815489A4) Pending network-clock callback and user pointer */
static RemoteDownloadClockCallback s_clockCallback = NULL;
static void *s_clockCallbackUserData = NULL;

/* (0x813732FA) MainThreadCallList poller for SceNpManager network clock sync */
static void remoteDownloadPollNetworkClock(void *reqHandle)
{
	int reqId = static_cast<int>(reinterpret_cast<intptr_t>(reqHandle));
	int state = SceNpManager_D1D191FB(reqId);
	int result = state;

	if (state < 0 || state == 2) {
		paf::common::MainThreadCallList::Unregister(
			remoteDownloadPollNetworkClock, reqHandle);
		SceNpManager_96534006(reqId, &result);
		SceNpManager_FAEA7A73();
		if (result >= 0 && s_clockCallback != NULL)
			s_clockCallback(s_clockCallbackUserData);
	}
}

/* (0x81373270) Reads the network tick or initiates an asynchronous NP clock update */
int remoteDownloadGetNetworkTick(paf::rtc::Tick *outTick,
				 RemoteDownloadClockCallback callback,
				 void *userData)
{
	int ret = paf::rtc::GetCurrentNetworkTick(outTick);
	if (ret != REMOTEDOWNLOAD_RTC_ERR_NOT_INIT)
		return ret;

	int initRet = SceNpManager_27112D01();
	if (initRet < 0)
		return initRet;

	int reqId = 0;
	int startRet = SceNpManager_E41932D4(&reqId, paf::thread::GetPriority()->normal);
	if (startRet < 0) {
		SceNpManager_FAEA7A73();
		return startRet;
	}

	s_clockCallback = callback;
	s_clockCallbackUserData = userData;
	paf::common::MainThreadCallList::Register(
		remoteDownloadPollNetworkClock,
		reinterpret_cast<void *>(static_cast<intptr_t>(reqId)));
	return ret;
}

/* (0x81372BF0) Loads or resets <userDir>/remotedownload/status.dat under flock */
static void remoteDownloadLoadOrResetStatus(bool resetFile)
{
	RemoteDownloadState *state = new RemoteDownloadState();
	if (state == NULL) {
		s_remoteDownloadState = NULL;
		return;
	}

	state->disabled = 0;
	sce_paf_memset(&state->disk, 0, sizeof(state->disk));

	paf::string statusPath("/remotedownload");
	SceUID lockFd = -1;

	if (Bgdl_8113D5CC(&statusPath) >= 0) {
		statusPath += "/status.dat";
		lockFd = sceIoOpen(statusPath.c_str(), REMOTEDOWNLOAD_OPEN_LOCK_FLAGS,
				   REMOTEDOWNLOAD_FILE_MODE);
		if (lockFd >= 0) {
			vshIoFlock(lockFd, resetFile ? REMOTEDOWNLOAD_FLOCK_EXCLUSIVE
						     : REMOTEDOWNLOAD_FLOCK_SHARED);
		}
	}

	const char *lockedPath = (lockFd >= 0) ? statusPath.c_str() : NULL;
	if (resetFile) {
		Bgdl_8113D8C2(lockedPath, &state->disk, sizeof(state->disk),
			      REMOTEDOWNLOAD_WRITE_FLAGS, REMOTEDOWNLOAD_FILE_MODE);
	} else {
		Bgdl_8113D6F8(lockedPath, &state->disk, sizeof(state->disk));
	}

	if (lockFd >= 0) {
		vshIoFlock(lockFd, REMOTEDOWNLOAD_FLOCK_UNLOCK);
		sceIoCloseForUser(lockFd);
	}

	if (state->disk.intervalSeconds == 0)
		state->disk.intervalSeconds = REMOTEDOWNLOAD_DEFAULT_INTERVAL_SEC;

	s_remoteDownloadState = state;
}

static void remoteDownloadClockRetryAdapter(void *userData)
{
	(void)userData;
	shellRemoteDownloadOnEvent();
}

/* (vtable 0x8153DBF8, Run 0x81449B40) Background job for remote-download polling */
class RemoteDownloadLibJob : public paf::job::JobItem {
public:
	explicit RemoteDownloadLibJob(SceInt32 action)
		: paf::job::JobItem("SceRemoteDownloadLibJob", NULL),
		  m_action(action)
	{
	}

	virtual ~RemoteDownloadLibJob()
	{
	}

	virtual int32_t Run()
	{
		if (m_action == REMOTEDOWNLOAD_ACTION_RESET) {
			remoteDownloadLoadOrResetStatus(true);
			delete s_remoteDownloadState;
			s_remoteDownloadState = NULL;
			return 0;
		}

		if (m_action != REMOTEDOWNLOAD_ACTION_CHECK)
			return 0;

		remoteDownloadLoadOrResetStatus(false);
		if (s_remoteDownloadState == NULL || s_remoteDownloadState->disabled != 0) {
			delete s_remoteDownloadState;
			s_remoteDownloadState = NULL;
			return 0;
		}

		paf::rtc::Tick currentTick = 0;
		if (remoteDownloadGetNetworkTick(&currentTick, remoteDownloadClockRetryAdapter, NULL) < 0) {
			delete s_remoteDownloadState;
			s_remoteDownloadState = NULL;
			return 0;
		}

		paf::rtc::Tick nextCheckTick = 0;
		paf::rtc::TickAddSeconds(
			&nextCheckTick,
			&s_remoteDownloadState->disk.lastCheckTick,
			static_cast<SceInt64>(s_remoteDownloadState->disk.intervalSeconds));

		if (currentTick <= nextCheckTick) {
			delete s_remoteDownloadState;
			s_remoteDownloadState = NULL;
			return 0;
		}

		int launchRet = sceAppMgrLaunchAppByName(0x20000u, "NPXS10079", "RemoteDownload");
		delete s_remoteDownloadState;
		s_remoteDownloadState = NULL;
		return (launchRet >= 0) ? 0 : launchRet;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

private:
	SceInt32 m_action;
};

/* (0x81372EE0) Enqueues a remote-download check job onto the default job queue */
int remoteDownloadEnqueueCheck(void)
{
	paf::job::JobQueue *queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return REMOTEDOWNLOAD_ERR_NO_QUEUE;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new RemoteDownloadLibJob(REMOTEDOWNLOAD_ACTION_CHECK));
	if (job.get() == NULL)
		return REMOTEDOWNLOAD_ERR_NO_MEMORY;

	queue->Enqueue(job);
	return 0;
}

/* (0x81372FE2) Enqueues status reset followed by a remote-download check job */
int remoteDownloadEnqueueInitAndCheck(void)
{
	paf::job::JobQueue *queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return REMOTEDOWNLOAD_ERR_NO_QUEUE;

	{
		paf::common::SharedPtr<paf::job::JobItem> resetJob(
			new RemoteDownloadLibJob(REMOTEDOWNLOAD_ACTION_RESET));
		if (resetJob.get() == NULL)
			return REMOTEDOWNLOAD_ERR_NO_MEMORY;
		queue->Enqueue(resetJob);
	}

	queue = paf::job::JobQueue::default_queue;
	if (queue == NULL)
		return REMOTEDOWNLOAD_ERR_NO_QUEUE;

	{
		paf::common::SharedPtr<paf::job::JobItem> checkJob(
			new RemoteDownloadLibJob(REMOTEDOWNLOAD_ACTION_CHECK));
		if (checkJob.get() == NULL)
			return REMOTEDOWNLOAD_ERR_NO_MEMORY;
		queue->Enqueue(checkJob);
	}

	return 0;
}
