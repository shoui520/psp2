/*
 * playlog.cpp: SceShell application playlog tracker and ShellSvc 0x120000 service
 * 3.65-CEX: 0x81233304 - 0x81234950
 */

#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <std/ctype.h>
#include <std/string.h>
#include <std/list>
#include <ipmi/session.h>
#include <thread/thread.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include "shell.h"

extern "C" {
extern int sceKernelGetProcessTime(SceKernelSysClock *sysClock);
extern SceUInt64 sceKernelGetProcessTimeWide(void);
extern int sceKernelGetRemoteProcessTime(SceUID pid, SceKernelSysClock *sysClock);
extern int sceAppMgrGetUserDirPath(int partitionId, char *path, SceSize size);
extern int sceAppMgrAppParamGetInt(SceUID pid, int paramId, int *value);
extern int sceAppMgrAppParamGetString(SceUID pid, int paramId, char *buf, SceSize size);
extern int sceRtcGetCurrentDebugNetworkTick(SceRtcTick *tick);
extern int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
}

extern int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
						  SceUInt32 methodId, const IPMI::DataInfo *inList,
						  SceUInt32 inCount);

struct PlaylogStats {
	SceUInt64 lastPlayedTick;
	SceUInt64 lastProcessTime;
	float launchesPerDay;
	SceUInt32 avgPlaySecPerDay;
	SceUInt32 totalPlaySec;
	float playShareRatio;
};

namespace {

static const SceUInt32 kPlaylogFileVersion = 0x00010000U;
static const SceUInt32 kPlaylogMaxEntries = 100U;
static const SceUInt32 kPlaylogMaxSecondsPerDay = 86400U;
static const int kPlaylogFlushIntervalMs = 3600000;
static const int kPlaylogWindowDays = 10;
static const float kPlaylogMinLaunchIntervalSec = 300.0f;

static const int kPlaylogErrorNoEntry = -2147418110;		/* 0x80010002 */
static const int kPlaylogErrorExists = -2147418095;		/* 0x80010011 */
static const int kPlaylogErrorInvalidMethod = -2147351103;	/* 0x800205C1 */
static const int kPlaylogErrorInvalidArg = -2146423295;		/* 0x80102E01 */
static const int kPlaylogErrorNotFound = -2146423294;		/* 0x80102E02 */
static const int kPlaylogErrorNoMemory = -2146423278;		/* 0x80102E12 */

enum {
	PLAYLOG_METHOD_GET_RECENT_APPS	= 0x00120000U,
	PLAYLOG_METHOD_GET_PLAY_STATS	= 0x00120001U,
};

enum {
	PLAYLOG_EVENT_STOP	= 1,
	PLAYLOG_EVENT_START	= 2,
};

struct PlaylogRecentAppsRequest {
	SceUInt32 maxCount;
	SceUInt32 withinDays;
};

struct PlaylogEventItem {
	int eventType;
	char titleId[36];
	SceKernelSysClock processTime;
};

struct PlaylogFileHeader {
	SceUInt32 version;
	SceUInt32 reserved;
	SceUInt64 lastUpdateTick;
	SceUInt64 lastProcessTime;
	SceUInt32 systemAvgSecPerDay;
	SceUInt32 reservedTail;
};

struct PlaylogDiskEntry {
	char titleId[24];
	SceUInt64 lastPlayedTick;
	SceUInt64 lastProcessTime;
	float launchesPerDay;
	SceUInt32 avgPlaySecPerDay;
	SceUInt32 totalPlaySec;
	float playShareRatio;
};

struct PlaylogEntry {
	PlaylogDiskEntry disk;
	PlaylogEntry *prev;
	PlaylogEntry *next;
};

static char s_playlogFilePath[64];
static char s_playlogTempPath[64];

static bool isValidTitleId(const char *titleId)
{
	if (titleId[0] == '\0')
		return false;

	for (int pos = 1; pos < 16; ++pos) {
		if ((sce_paf_look_ctype_table((unsigned char)titleId[pos]) & 7) == 0)
			return titleId[pos] == '\0';
	}
	return true;
}

class PlaylogFileAccessThread : public paf::thread::Thread {
public:
	PlaylogFileAccessThread();
	virtual ~PlaylogFileAccessThread();
	virtual void EntryFunction();

	void EnqueueEvent(const PlaylogEventItem &eventItem);

private:
	paf::list<PlaylogEventItem> m_eventQueue;
	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_cond;
};

class PlaylogManager {
public:
	static PlaylogManager *GetInstance();

	int Init();
	int SaveFile();
	int RecordAppStart(const char *titleId, const SceKernelSysClock *processTime);
	int RecordAppStop(const char *titleId, const SceKernelSysClock *processTime);
	int GetMostRecentPlayedApps(SceUInt32 maxCount, SceUInt32 withinDays,
				    SceUInt32 *outCount, char *outTitleIds);
	int GetPlayStats(const char *titleId, SceUInt32 queryType, PlaylogStats *outStats);

	virtual ~PlaylogManager();

private:
	PlaylogManager();

	void Reset();
	int GetCurrentTick(SceRtcTick *tick);
	int UpdateSystemActivity();
	int BuildFilePaths(char *datPath, SceUInt8 datSize, char *tmpPath, SceUInt8 tmpSize);
	int ShouldTrackApp(SceUID processId);

	static void OnAppEvent(int code, const void *eventData, SceSize eventSize, void *userArg);

	PlaylogFileHeader m_header;
	PlaylogEntry *m_head;
	PlaylogEntry *m_tail;
	SceUInt32 m_entryCount;
	PlaylogEntry m_entries[kPlaylogMaxEntries];
	PlaylogFileAccessThread *m_workerThread;
	paf::thread::RMutex m_mutex;
};

/* 0x81233404 */
PlaylogFileAccessThread::PlaylogFileAccessThread()
	: paf::thread::Thread(0x1000010A, 1024, "SceShellPlaylogFileAccessThread", NULL),
	  m_eventQueue(),
	  m_mutex("SceShellPlaylogFileAccessThreadMutex", 0),
	  m_cond(m_mutex, "SceShellPlaylogFileAccessThreadCond", 0)
{
}

/* 0x81233478 / 0x812334CC */
PlaylogFileAccessThread::~PlaylogFileAccessThread()
{
}

void PlaylogFileAccessThread::EnqueueEvent(const PlaylogEventItem &eventItem)
{
	m_eventQueue.push_back(eventItem);
	m_cond.Notify();
}

/* 0x81234056 */
void PlaylogFileAccessThread::EntryFunction()
{
	SceUInt64 lastFlushTime = sceKernelGetProcessTimeWide();
	int waitTimeoutMs = kPlaylogFlushIntervalMs;

	do {
		PlaylogManager *mgr = PlaylogManager::GetInstance();
		m_mutex.Lock();
		if (m_cond.Wait(waitTimeoutMs)) {
			mgr->SaveFile();
			waitTimeoutMs = kPlaylogFlushIntervalMs;
			lastFlushTime = sceKernelGetProcessTimeWide();
		} else {
			while (m_eventQueue.size() != 0) {
				PlaylogEventItem eventItem = m_eventQueue.front();
				m_eventQueue.pop_front();
				if (eventItem.eventType == PLAYLOG_EVENT_START)
					mgr->RecordAppStart(eventItem.titleId, &eventItem.processTime);
				else if (eventItem.eventType == PLAYLOG_EVENT_STOP)
					mgr->RecordAppStop(eventItem.titleId, &eventItem.processTime);
			}

			SceUInt64 nowTime = sceKernelGetProcessTimeWide();
			SceUInt32 elapsedMs = (SceUInt32)((nowTime - lastFlushTime) / 1000ULL);
			if (elapsedMs < (SceUInt32)waitTimeoutMs) {
				waitTimeoutMs -= (int)elapsedMs;
			} else {
				mgr->SaveFile();
				waitTimeoutMs = kPlaylogFlushIntervalMs;
				lastFlushTime = sceKernelGetProcessTimeWide();
			}
		}
		m_mutex.Unlock();
	} while (!IsCanceled());
}

/* 0x81233F34 */
void PlaylogManager::Reset()
{
	for (SceUInt32 index = 0; index < kPlaylogMaxEntries; ++index) {
		PlaylogEntry &cur = m_entries[index];
		cur.disk.titleId[0] = '\0';
		cur.disk.lastPlayedTick = 0;
		cur.disk.lastProcessTime = 0;
		cur.disk.launchesPerDay = 1.0f;
		cur.disk.avgPlaySecPerDay = 0;
		cur.disk.totalPlaySec = 0;
		cur.disk.playShareRatio = 1.0f;
		cur.prev = NULL;
		cur.next = NULL;
	}

	m_head = NULL;
	m_tail = NULL;
	m_entryCount = 0;
	m_header.version = kPlaylogFileVersion;
	m_header.reserved = 0;
	m_header.lastUpdateTick = 0;
	m_header.lastProcessTime = 0;
	m_header.systemAvgSecPerDay = 0;
	m_header.reservedTail = 0;
}

/* 0x81233F92 */
PlaylogManager::PlaylogManager()
	: m_workerThread(NULL),
	  m_mutex("SceShellPlaylogMutex", 0)
{
	Reset();
	if (SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, OnAppEvent, this) >= 0) {
		if (SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, OnAppEvent, this) < 0)
			SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, OnAppEvent, this);
	}
}

/* 0x812341AE / 0x812341E8 */
PlaylogManager::~PlaylogManager()
{
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, OnAppEvent, this);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, OnAppEvent, this);
}

/* 0x81233F92 */
PlaylogManager *PlaylogManager::GetInstance()
{
	static PlaylogManager instance;
	return &instance;
}

/* 0x81233524 */
int PlaylogManager::SaveFile()
{
	m_mutex.Lock();
	SceUID fd = sceIoOpen(s_playlogTempPath,
			      SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 06);
	if (fd < 0) {
		m_mutex.Unlock();
		return fd;
	}

	SceSSize written = sceIoWrite(fd, &m_header, sizeof(PlaylogFileHeader));
	if (written < 0) {
		sceIoClose(fd);
		m_mutex.Unlock();
		return (int)written;
	}

	PlaylogEntry *cur = m_head;
	for (SceUInt32 count = 0; count < m_entryCount; ++count) {
		if (cur == NULL) {
			sceIoClose(fd);
			m_mutex.Unlock();
			return -1;
		}
		if (sceIoWrite(fd, &cur->disk, sizeof(PlaylogDiskEntry)) <= 0)
			break;
		cur = cur->next;
	}

	sceIoClose(fd);
	int ret = sceIoRename(s_playlogTempPath, s_playlogFilePath);
	if (ret == kPlaylogErrorExists) {
		ret = sceIoRemove(s_playlogFilePath);
		if (ret >= 0)
			ret = sceIoRename(s_playlogTempPath, s_playlogFilePath);
	}

	m_mutex.Unlock();
	return ret;
}

/* 0x8123360E */
int PlaylogManager::GetCurrentTick(SceRtcTick *tick)
{
	int forceProcessClock = 0;
	sceRegMgrGetKeyInt("/CONFIG/SHELL", "playlog_clock", &forceProcessClock);
	if (forceProcessClock == 0) {
		int ret = sceRtcGetCurrentDebugNetworkTick(tick);
		if (ret == 0)
			return 0;
		ret = sceRtcGetCurrentRetainedNetworkTick(tick);
		if (ret == 0)
			return 0;
	}

	SceKernelSysClock procTime;
	procTime.quad = 0;
	if (sceKernelGetProcessTime(&procTime) < 0 || procTime.quad < m_header.lastProcessTime)
		return -1;

	SceUInt32 tickResolution = sceRtcGetTickResolution();
	float elapsedSec = (float)(procTime.quad - m_header.lastProcessTime) / 1000000.0f;
	SceUInt64 deltaTicks = (SceUInt64)(elapsedSec * (float)tickResolution);
	tick->tick = m_header.lastUpdateTick + deltaTicks;
	return 0;
}

/* 0x8123374A */
int PlaylogManager::UpdateSystemActivity()
{
	m_mutex.Lock();
	SceRtcTick nowTick;
	int ret = GetCurrentTick(&nowTick);
	if (ret < 0) {
		m_mutex.Unlock();
		return ret;
	}

	SceKernelSysClock procTime;
	procTime.quad = 0;
	if (m_header.lastUpdateTick > nowTick.tick ||
	    sceKernelGetProcessTime(&procTime) < 0 ||
	    procTime.quad < m_header.lastProcessTime) {
		m_mutex.Unlock();
		return -1;
	}

	SceUInt32 tickResolution = sceRtcGetTickResolution();
	SceRtcTick windowStartTick;
	if ((SceUInt64)kPlaylogWindowDays * kPlaylogMaxSecondsPerDay * (SceUInt64)tickResolution <= nowTick.tick)
		sceRtcTickAddDays(&windowStartTick, &nowTick, -kPlaylogWindowDays);
	else
		windowStartTick.tick = 0;

	float activeSec = (float)(procTime.quad - m_header.lastProcessTime) / 1000000.0f;
	if (m_header.lastUpdateTick <= windowStartTick.tick) {
		SceUInt32 avgSec = (SceUInt32)(activeSec / (float)kPlaylogWindowDays);
		if (avgSec > kPlaylogMaxSecondsPerDay)
			avgSec = kPlaylogMaxSecondsPerDay;
		m_header.systemAvgSecPerDay = avgSec;

		for (SceUInt32 index = 0; index < m_entryCount; ++index) {
			if (m_entries[index].disk.avgPlaySecPerDay > m_header.systemAvgSecPerDay)
				m_entries[index].disk.avgPlaySecPerDay = m_header.systemAvgSecPerDay;
		}
	} else {
		float overlapDays = ((float)(m_header.lastUpdateTick - windowStartTick.tick) /
				     (float)tickResolution) /
				    (float)kPlaylogMaxSecondsPerDay;
		m_header.systemAvgSecPerDay =
			(SceUInt32)(((float)m_header.systemAvgSecPerDay * overlapDays + activeSec) /
				    (float)kPlaylogWindowDays);
	}

	m_header.lastUpdateTick = nowTick.tick;
	m_header.lastProcessTime = procTime.quad;
	m_mutex.Unlock();
	return 0;
}

/* 0x81233996 */
int PlaylogManager::RecordAppStart(const char *titleId, const SceKernelSysClock *processTime)
{
	m_mutex.Lock();

	PlaylogEntry *target = NULL;
	for (PlaylogEntry *cur = m_head; cur != NULL; cur = cur->next) {
		if (sce_paf_strncmp(cur->disk.titleId, titleId, 16) == 0) {
			target = cur;
			break;
		}
	}

	if (target != NULL) {
		if (target != m_head) {
			if (target == m_tail)
				m_tail = target->prev;
			if (target->prev != NULL)
				target->prev->next = target->next;
			if (target->next != NULL)
				target->next->prev = target->prev;
			target->prev = NULL;
			target->next = m_head;
			m_head->prev = target;
			m_head = target;
		}
	} else {
		for (SceUInt32 index = 0; index < kPlaylogMaxEntries; ++index) {
			if (m_entries[index].disk.titleId[0] == '\0') {
				target = &m_entries[index];
				target->prev = NULL;
				target->next = m_head;
				if (m_head != NULL)
					m_head->prev = target;
				else
					m_tail = target;
				m_head = target;
				++m_entryCount;
				break;
			}
		}

		if (target == NULL) {
			target = m_tail;
			m_tail = target->prev;
			m_tail->next = NULL;
			target->prev = NULL;
			target->next = m_head;
			m_head->prev = target;
			m_head = target;
		}
	}

	if (target == NULL) {
		m_mutex.Unlock();
		return -1;
	}

	SceUInt32 tickResolution = sceRtcGetTickResolution();
	SceRtcTick nowTick;
	int ret = GetCurrentTick(&nowTick);
	if (ret < 0) {
		m_mutex.Unlock();
		return ret;
	}

	if (target->disk.lastPlayedTick > nowTick.tick) {
		m_mutex.Unlock();
		return -1;
	}

	SceRtcTick windowStartTick;
	if ((SceUInt64)kPlaylogWindowDays * kPlaylogMaxSecondsPerDay * (SceUInt64)tickResolution <= nowTick.tick)
		sceRtcTickAddDays(&windowStartTick, &nowTick, -kPlaylogWindowDays);
	else
		windowStartTick.tick = 0;

	sce_paf_strncpy(target->disk.titleId, titleId, 16);
	target->disk.titleId[16] = '\0';

	SceUInt64 prevTick = target->disk.lastPlayedTick;
	float gapSec = (float)(nowTick.tick - prevTick) / (float)tickResolution;
	if (prevTick <= windowStartTick.tick) {
		if (gapSec > kPlaylogMinLaunchIntervalSec)
			target->disk.launchesPerDay = 0.1f;
	} else {
		float overlapDays = ((float)(prevTick - windowStartTick.tick) /
				     (float)tickResolution) /
				    (float)kPlaylogMaxSecondsPerDay;
		if (gapSec > kPlaylogMinLaunchIntervalSec)
			target->disk.launchesPerDay =
				(target->disk.launchesPerDay * overlapDays + 1.0f) / (float)kPlaylogWindowDays;
	}

	target->disk.lastPlayedTick = nowTick.tick;
	target->disk.lastProcessTime = processTime->quad;
	UpdateSystemActivity();
	m_mutex.Unlock();
	return 0;
}

/* 0x81233C7C */
int PlaylogManager::RecordAppStop(const char *titleId, const SceKernelSysClock *processTime)
{
	m_mutex.Lock();

	PlaylogEntry *target = NULL;
	for (PlaylogEntry *cur = m_head; cur != NULL; cur = cur->next) {
		if (sce_paf_strncmp(cur->disk.titleId, titleId, 16) == 0) {
			target = cur;
			break;
		}
	}

	if (target == NULL) {
		m_mutex.Unlock();
		return -1;
	}

	SceRtcTick nowTick;
	if (GetCurrentTick(&nowTick) < 0 ||
	    target->disk.lastPlayedTick > nowTick.tick ||
	    target->disk.lastProcessTime > processTime->quad) {
		m_mutex.Unlock();
		return -1;
	}

	SceUInt32 tickResolution = sceRtcGetTickResolution();
	SceRtcTick windowStartTick;
	if ((SceUInt64)kPlaylogWindowDays * kPlaylogMaxSecondsPerDay * (SceUInt64)tickResolution <= nowTick.tick)
		sceRtcTickAddDays(&windowStartTick, &nowTick, -kPlaylogWindowDays);
	else
		windowStartTick.tick = 0;

	float sessionSec = (float)(processTime->quad - target->disk.lastProcessTime) / 1000000.0f;
	float weightedSec = sessionSec;
	if (target->disk.lastPlayedTick > windowStartTick.tick) {
		float overlapDays = ((float)(target->disk.lastPlayedTick - windowStartTick.tick) /
				     (float)tickResolution) /
				    (float)kPlaylogMaxSecondsPerDay;
		weightedSec = (float)target->disk.avgPlaySecPerDay * overlapDays + sessionSec;
	}

	bool wasFirstPlay = (target->disk.totalPlaySec == 0);
	target->disk.avgPlaySecPerDay = (SceUInt32)(weightedSec / (float)kPlaylogWindowDays);
	UpdateSystemActivity();

	target->disk.totalPlaySec += (SceUInt32)sessionSec;
	target->disk.playShareRatio =
		(float)target->disk.avgPlaySecPerDay / (float)m_header.systemAvgSecPerDay;
	target->disk.lastPlayedTick = nowTick.tick;
	target->disk.lastProcessTime = processTime->quad;

	if (wasFirstPlay)
		SaveFile();

	m_mutex.Unlock();
	return 0;
}

/* 0x81234228 */
int PlaylogManager::BuildFilePaths(char *datPath, SceUInt8 datSize, char *tmpPath, SceUInt8 tmpSize)
{
	char pathBuf[64];
	sce_paf_memset(pathBuf, 0, sizeof(pathBuf));

	int ret = sceAppMgrGetUserDirPath(1, pathBuf, sizeof(pathBuf));
	if (ret < 0)
		return ret;

	sce_paf_strncat(pathBuf, "/shell", sce_paf_strlen("/shell"));
	ret = sceIoMkdir(pathBuf, 06);
	if (ret != 0 && ret != kPlaylogErrorExists)
		return ret;

	sce_paf_strncat(pathBuf, "/playlog", sce_paf_strlen("/playlog"));
	ret = sceIoMkdir(pathBuf, 06);
	if (ret != 0 && ret != kPlaylogErrorExists)
		return ret;

	sce_paf_strncat(pathBuf, "/playlog.dat", sce_paf_strlen("/playlog.dat"));
	sce_paf_strncpy(datPath, pathBuf, datSize);
	sce_paf_strncpy(tmpPath, pathBuf, tmpSize);
	sce_paf_strncat(tmpPath, ".tmp", sce_paf_strlen(".tmp"));
	return ret;
}

/* 0x8123433A */
int PlaylogManager::Init()
{
	BuildFilePaths(s_playlogFilePath, sizeof(s_playlogFilePath),
		       s_playlogTempPath, sizeof(s_playlogTempPath));

	bool corrupted = false;
	SceUID fd = sceIoOpen(s_playlogFilePath, SCE_O_RDONLY, 06);
	int lastResult = fd;

	if (fd < 0 && fd == kPlaylogErrorNoEntry) {
		SceIoStat fileStat;
		lastResult = sceIoGetstat(s_playlogTempPath, &fileStat);
		if (lastResult >= 0) {
			lastResult = sceIoRename(s_playlogTempPath, s_playlogFilePath);
			if (lastResult >= 0) {
				fd = sceIoOpen(s_playlogFilePath, SCE_O_RDONLY, 06);
				lastResult = fd;
			}
		}
	}

	if (fd >= 0) {
		SceSSize readBytes = sceIoRead(fd, &m_header, sizeof(PlaylogFileHeader));
		lastResult = (int)readBytes;
		if (readBytes >= 0) {
			m_header.lastProcessTime = 0;
			if (m_header.systemAvgSecPerDay > kPlaylogMaxSecondsPerDay) {
				corrupted = true;
			} else {
				PlaylogEntry *prevEntry = NULL;
				SceUInt32 loadedCount = 0;
				while (loadedCount < kPlaylogMaxEntries) {
					PlaylogEntry *cur = &m_entries[loadedCount];
					lastResult = (int)sceIoRead(fd, &cur->disk, sizeof(PlaylogDiskEntry));
					if (lastResult <= 0)
						break;

					cur->disk.lastProcessTime = 0;
					if (!isValidTitleId(cur->disk.titleId) ||
					    cur->disk.avgPlaySecPerDay > m_header.systemAvgSecPerDay ||
					    cur->disk.avgPlaySecPerDay > kPlaylogMaxSecondsPerDay ||
					    cur->disk.playShareRatio > 1.0f ||
					    (prevEntry != NULL &&
					     cur->disk.lastPlayedTick > prevEntry->disk.lastPlayedTick)) {
						corrupted = true;
						break;
					}

					if (loadedCount == 0)
						m_head = cur;
					if (prevEntry != NULL) {
						prevEntry->next = cur;
						cur->prev = prevEntry;
					}
					prevEntry = cur;
					++loadedCount;
				}

				if (loadedCount != 0) {
					m_entryCount = loadedCount;
					m_tail = &m_entries[loadedCount - 1];
				}
			}
		}

		sceIoClose(fd);
		if (corrupted)
			Reset();
	}

	UpdateSystemActivity();
	m_workerThread = new PlaylogFileAccessThread();
	if (m_workerThread == NULL)
		return kPlaylogErrorNoMemory;

	m_workerThread->Start();
	return lastResult;
}

/* 0x81234566 */
int PlaylogManager::GetMostRecentPlayedApps(SceUInt32 maxCount, SceUInt32 withinDays,
					    SceUInt32 *outCount, char *outTitleIds)
{
	m_mutex.Lock();
	SceRtcTick nowTick;
	int ret = GetCurrentTick(&nowTick);
	if (ret < 0) {
		m_mutex.Unlock();
		return ret;
	}

	SceUInt32 tickResolution = sceRtcGetTickResolution();
	SceRtcTick cutoffTick;
	if (withinDays != 0) {
		SceUInt64 spanTicks = (SceUInt64)withinDays * kPlaylogMaxSecondsPerDay *
				      (SceUInt64)tickResolution;
		if (spanTicks <= nowTick.tick)
			sceRtcTickAddDays(&cutoffTick, &nowTick, -(int)withinDays);
		else
			cutoffTick.tick = 0;
	} else {
		cutoffTick.tick = 0;
	}

	SceUInt32 writtenCount = 0;
	for (PlaylogEntry *cur = m_head; cur != NULL && writtenCount < maxCount; cur = cur->next) {
		if (cur->disk.lastPlayedTick < cutoffTick.tick)
			break;
		sce_paf_strncpy(outTitleIds, cur->disk.titleId, 16);
		outTitleIds[16] = '\0';
		outTitleIds += 17;
		++writtenCount;
	}

	*outCount = writtenCount;
	m_mutex.Unlock();
	return 0;
}

/* 0x81234672 */
int PlaylogManager::GetPlayStats(const char *titleId, SceUInt32 queryType, PlaylogStats *outStats)
{
	m_mutex.Lock();
	if (titleId == NULL || queryType > 10U || outStats == NULL) {
		m_mutex.Unlock();
		return kPlaylogErrorInvalidArg;
	}

	PlaylogEntry *found = NULL;
	for (PlaylogEntry *cur = m_head; cur != NULL; cur = cur->next) {
		if (sce_paf_strncmp(cur->disk.titleId, titleId, 16) == 0) {
			found = cur;
			break;
		}
	}

	if (found == NULL) {
		m_mutex.Unlock();
		return kPlaylogErrorNotFound;
	}

	outStats->lastPlayedTick = found->disk.lastPlayedTick;
	outStats->lastProcessTime = found->disk.lastProcessTime;
	outStats->launchesPerDay = found->disk.launchesPerDay;
	outStats->avgPlaySecPerDay = found->disk.avgPlaySecPerDay;
	outStats->totalPlaySec = found->disk.totalPlaySec;
	outStats->playShareRatio = found->disk.playShareRatio;
	m_mutex.Unlock();
	return 0;
}

/* 0x81234722 */
int PlaylogManager::ShouldTrackApp(SceUID processId)
{
	int appAttr = 0;
	int ret = sceAppMgrAppParamGetInt(processId, 15, &appAttr);
	if (ret < 0)
		return ret;

	if ((appAttr & 0x00400000) != 0)
		return 1;
	if ((appAttr & 0x00001000) != 0)
		return 0;

	char category[8];
	sce_paf_memset(category, 0, sizeof(category));
	ret = sceAppMgrAppParamGetString(processId, 8, category, 4);
	if (ret < 0)
		return ret;

	if (sce_paf_strncmp(category, "gdb", 4) == 0 ||
	    sce_paf_strncmp(category, "gd", 4) == 0 ||
	    sce_paf_strncmp(category, "gp", 4) == 0 ||
	    sce_paf_strncmp(category, "gdc", 4) == 0 ||
	    sce_paf_strncmp(category, "gpc", 4) == 0) {
		return 1;
	}
	return 0;
}

/* 0x8123480E */
void PlaylogManager::OnAppEvent(int code, const void *eventData, SceSize eventSize, void *userArg)
{
	(void)eventSize;

	PlaylogManager *mgr = (PlaylogManager *)userArg;
	const ShellAppEvent *appEvent = (const ShellAppEvent *)eventData;
	SceAppMgrAppStatus appStatus;
	if (mgr == NULL || mgr->ShouldTrackApp(appEvent->processId) != 1 ||
	    sceAppMgrGetStatusByAppId(appEvent->appId, &appStatus) < 0) {
		return;
	}

	SceKernelSysClock remoteClock;
	remoteClock.quad = 0;
	if (sceKernelGetRemoteProcessTime(appStatus.processId, &remoteClock) < 0)
		return;

	PlaylogEventItem eventItem;
	if (code == SHELL_APP_EVENT_ACTIVATED)
		eventItem.eventType = PLAYLOG_EVENT_START;
	else if (code == SHELL_APP_EVENT_DEACTIVATED)
		eventItem.eventType = PLAYLOG_EVENT_STOP;
	else
		return;

	sce_paf_strncpy(eventItem.titleId, appStatus.appName, 32);
	eventItem.titleId[32] = '\0';
	eventItem.processTime = remoteClock;
	mgr->m_workerThread->EnqueueEvent(eventItem);
}

/* 0x81233374 */
static int playlogDispatchSync(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;

	if (methodId == PLAYLOG_METHOD_GET_RECENT_APPS) {
		const PlaylogRecentAppsRequest *req =
			(const PlaylogRecentAppsRequest *)inList[0].pData;
		int result = PlaylogManager::GetInstance()->GetMostRecentPlayedApps(
			req->maxCount, req->withinDays,
			(SceUInt32 *)outList[0].pBuffer, (char *)outList[1].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = 17U * req->maxCount;
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}

	if (methodId == PLAYLOG_METHOD_GET_PLAY_STATS) {
		const SceUInt32 *queryType = (const SceUInt32 *)inList[0].pData;
		const char *titleId = (const char *)inList[1].pData;
		int result = PlaylogManager::GetInstance()->GetPlayStats(
			titleId, *queryType, (PlaylogStats *)outList[0].pBuffer);
		outList[0].outSize = sizeof(PlaylogStats);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}

	return kPlaylogErrorInvalidMethod;
}

} /* namespace */

/* 0x81233304 */
int shellPlaylogGetMostRecentPlayedApps(SceUInt32 maxCount, SceUInt32 withinDays,
					SceUInt32 *outCount, char *outTitleIds)
{
	return PlaylogManager::GetInstance()->GetMostRecentPlayedApps(
		maxCount, withinDays, outCount, outTitleIds);
}

/* 0x81233328 */
int shellPlaylogGetPlayStats(const char *titleId, SceUInt32 queryType, PlaylogStats *outStats)
{
	return PlaylogManager::GetInstance()->GetPlayStats(titleId, queryType, outStats);
}

/* 0x81233340 */
int shellSvcDisp_12_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return playlogDispatchSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x81233354 */
int shellSvcDisp_12_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	return shellIncomingDialogServerAsyncDispatch(session, reqId, methodId, inList, inCount);
}

/* 0x81233368 */
int shellPlaylogInit(void)
{
	return PlaylogManager::GetInstance()->Init();
}

int shellNpNotifyStart(void)
{
	return shellPlaylogInit();
}
