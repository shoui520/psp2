/*
 * shutter_sound_server.cpp: SceShell SceShutterSound IPMI service (method 0x140000) and
 * shutter sound playback job queue.
 * 3.65-CEX: 0x812443B4 - 0x81244AE8
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/math.h>
#include <av_config/av_config.h>
#include <kernel/processmgr.h>
#include <common/job_slots.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <src/avout/sound.h>
#include <system/system.h>
#include <thread/thread.h>

#include "shell.h"

extern "C" {
extern SceUInt64 sceKernelGetProcessTimeWide(void);
extern int sceAVConfigGetShutterVol(void);
}

extern int musicCoreEnableAvrcp(void);				/* 0x811CCB70 */
extern int musicCoreDisableAvrcp(void);				/* 0x811CCBD8 */
extern int shellSoundOutExclusiveBegin(int displayRequest);	/* 0x813C9714 */
extern int shellSoundOutExclusiveEnd(void);			/* 0x813C9758 */
extern int shellSvcLiveAreaSyncDefault(IPMI::Session *, SceUInt32, const IPMI::DataInfo *,
				       SceUInt32, IPMI::BufferInfo *, SceUInt32); /* 0x8115B554 */

namespace {

static const SceUInt32 kShutterMethodPlay = 0x140000U;
static const SceUInt32 kShutterSoundTypeCount = 3;
static const int kShutterErrAlreadyInit = -2146413951;		/* 0x80105281 */
static const int kShutterErrInvalidParam = -2146414079;		/* 0x80105201 */
static const int kShutterErrNotReady = -2146414077;		/* 0x80105203 */
static const int kIpmiErrUnsupported = -2147351103;		/* 0x800205C1 */

static const SceUInt32 kAudioStatePafMuted = 0x1U;
static const SceUInt32 kAudioStateAvrcpPaused = 0x2U;
static const SceUInt32 kAudioStateExclusiveCamera = 0x4U;
static const SceUInt32 kAudioStateExclusiveScreen = 0x8U;
static const SceUInt32 kAudioStateExclusiveMask = 0xCU;

struct ShutterSoundDesc {
	int commonSeId;
	int durationMs;
};

static const ShutterSoundDesc kShutterSoundTable[kShutterSoundTypeCount] = {
	{ 47, 1000 },
	{ 48, 1000 },
	{ 49, 1000 }
};

struct ShutterSoundInitParam {
	paf::memory::HeapAllocator *allocator;
	paf::job::JobQueue *jobQueue;
};

static paf::job::JobQueue *s_shutterJobQueue = NULL;
static paf::memory::HeapAllocator *s_shutterHeap = NULL;
static paf::memory::HeapAllocator *s_shutterAlloc = NULL;

static void *shutterAlloc(SceSize size)
{
	if (s_shutterAlloc != NULL)
		return s_shutterAlloc->sce_paf_malloc(size);
	return NULL;
}

static void shutterFree(void *ptr)
{
	if (s_shutterAlloc != NULL)
		s_shutterAlloc->sce_paf_free(ptr);
}

class ShutterSoundJob : public paf::common::SlotJobItem {
public:
	static void *operator new(size_t size)
	{
		return shutterAlloc(size);
	}

	static void operator delete(void *ptr)
	{
		shutterFree(ptr);
	}

	ShutterSoundJob(SceUInt32 *audioState, SceUInt32 soundType, int useCameraVol)
		: paf::common::SlotJobItem("SceShutterSoundJob", NULL),
		  m_audioState(audioState),
		  m_soundType(soundType),
		  m_useCameraVol(useCameraVol),
		  m_mutex("SceShutterSoundJob", 0),
		  m_cond(m_mutex, "SceShutterSoundJob", 0)
	{
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	}

	virtual ~ShutterSoundJob()
	{
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	}

	virtual void Cancel()
	{
		m_mutex.Lock();
		paf::job::JobItem::Cancel();
		m_cond.NotifyAll();
		m_mutex.Unlock();
	}

	virtual int Run()
	{
		if (m_audioState == NULL || m_soundType >= kShutterSoundTypeCount)
			return -1;

		paf::Framework *framework = paf::Framework::Instance();
		if (framework == NULL)
			return -1;

		paf::sound::Output *soundOut = framework->GetSoundInstance();
		SceUInt32 exclusiveBit = (m_useCameraVol != 0)
			? kAudioStateExclusiveCamera
			: kAudioStateExclusiveScreen;
		SceUInt32 sleepMs = 0;
		bool newlyMuted = false;

		if ((*m_audioState & kAudioStatePafMuted) == 0) {
			if (soundOut != NULL) {
				*m_audioState |= kAudioStatePafMuted;
				soundOut->EnableSound(false);
				newlyMuted = true;
			}
		}

		if (soundOut != NULL) {
			if (((*m_audioState & exclusiveBit) == 0 || newlyMuted)) {
				soundOut->StopSoundAll();
				sleepMs = 100;
			}

			bool acquireExclusive = true;
			if ((*m_audioState & kAudioStateAvrcpPaused) == 0) {
				*m_audioState |= kAudioStateAvrcpPaused;
				if (musicCoreEnableAvrcp() == 0) {
					if (sleepMs < 16)
						sleepMs = 16;
				} else {
					acquireExclusive = false;
				}
			}

			if (acquireExclusive) {
				if (sleepMs != 0)
					paf::thread::Sleep((int)sleepMs);
				if ((*m_audioState & exclusiveBit) == 0) {
					*m_audioState = (*m_audioState & ~kAudioStateExclusiveMask) |
							exclusiveBit;
					shellSoundOutExclusiveBegin(m_useCameraVol != 0 ? 1 : 0);
				}
			}
		}

		float volume = 1.0f;
		if (m_useCameraVol == 1) {
			SceUInt32 shutterVol = (SceUInt32)sceAVConfigGetShutterVol();
			if (shutterVol < 30U) {
				float attenuation = sce_paf_powf(
					10.0f, (float)(31 - (int)shutterVol) / 20.0f);
				volume = 1.0f / attenuation;
			}
		}

		int playResult = framework->PlayCommonSE(
			kShutterSoundTable[m_soundType].commonSeId, volume, true);
		SceUInt64 startTick = sceKernelGetProcessTimeWide();

		m_mutex.Lock();
		while (!IsCanceled()) {
			SceUInt64 elapsedUsec = sceKernelGetProcessTimeWide() - startTick;
			int elapsedMs = (int)(elapsedUsec / 1000ULL);
			int remainMs = kShutterSoundTable[m_soundType].durationMs - elapsedMs;
			if (remainMs <= 0)
				break;
			m_cond.Wait(remainMs);
		}
		bool canceled = IsCanceled();
		m_mutex.Unlock();

		if (canceled)
			return 0;

		if ((*m_audioState & kAudioStateExclusiveMask) != 0) {
			*m_audioState &= ~kAudioStateExclusiveMask;
			int releaseRet = shellSoundOutExclusiveEnd();
			if (releaseRet != 0 && playResult == 0)
				playResult = releaseRet;
		}
		if ((*m_audioState & kAudioStateAvrcpPaused) != 0) {
			*m_audioState &= ~kAudioStateAvrcpPaused;
			int resumeRet = musicCoreDisableAvrcp();
			if (resumeRet != 0 && playResult == 0)
				playResult = resumeRet;
		}
		if ((*m_audioState & kAudioStatePafMuted) != 0) {
			if (soundOut != NULL) {
				*m_audioState &= ~kAudioStatePafMuted;
				soundOut->EnableSound(true);
			} else if (playResult == 0) {
				playResult = -1;
			}
		}

		return playResult;
	}

	SceUInt32 *m_audioState;
	SceUInt32 m_soundType;
	int m_useCameraVol;
	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_cond;
};

class ShutterSoundManager {
public:
	static void *operator new(size_t size)
	{
		return shutterAlloc(size);
	}

	static void operator delete(void *ptr)
	{
		shutterFree(ptr);
	}

	static void OnJobRemoved(uint32_t index,
				 paf::common::SharedPtr<paf::job::JobItem> item,
				 void *userData)
	{
		(void)index;
		(void)item;
		(void)userData;
	}

	ShutterSoundManager(paf::memory::HeapAllocator *allocator, paf::job::JobQueue *jobQueue)
		: m_allocator(allocator), m_jobQueue(jobQueue), m_audioState(0)
	{
		m_slots.Grow(1);
		m_slots.SetRemovedFunction(OnJobRemoved, this);
	}

	int Play(SceUInt32 soundType, int useCameraVol)
	{
		if (soundType >= kShutterSoundTypeCount)
			return kShutterErrInvalidParam;
		if (m_jobQueue == NULL)
			return kShutterErrNotReady;

		int activeCameraVol = 0;
		paf::common::SharedPtr<paf::job::JobItem> current = m_slots.Get(0);
		if (current.get() != NULL)
			activeCameraVol = ((ShutterSoundJob *)current.get())->m_useCameraVol;

		if (activeCameraVol != 0 && useCameraVol == 0)
			return 0;
		if ((SceUInt32)m_jobQueue->NumItems() >= 5U)
			return 0;

		paf::common::SharedPtr<paf::job::JobItem> job(
			new ShutterSoundJob(&m_audioState, soundType, useCameraVol));
		if (job.get() == NULL)
			return 0;

		m_slots.Cancel(0);
		return m_slots.Enqueue(0, m_jobQueue, job);
	}

	paf::memory::HeapAllocator *m_allocator;
	paf::job::JobQueue *m_jobQueue;
	SceUInt32 m_audioState;
	paf::common::JobSlots m_slots;
};

static ShutterSoundManager *s_shutterManager = NULL;

static int shutterManagerInit(const ShutterSoundInitParam *param)
{
	if (s_shutterManager != NULL)
		return kShutterErrAlreadyInit;
	if (param->allocator == NULL || param->jobQueue == NULL)
		return kShutterErrInvalidParam;

	s_shutterAlloc = param->allocator;
	s_shutterManager = new ShutterSoundManager(param->allocator, param->jobQueue);
	return (s_shutterManager != NULL) ? 0 : kShutterErrNotReady;
}

} /* anonymous namespace */

/* 0x812443E8 */
int shellShutterSoundStart(void)
{
	paf::job::JobQueue::Option option;
	option.priority = 0x10000105;
	option.stackSize = 4096;
	option.numThreads = 1;
	s_shutterJobQueue = new paf::job::JobQueue("SceShutterSoundJobQueue", &option);
	s_shutterHeap = new paf::memory::HeapAllocator(NULL, 0x400U,
						       "SceShutterSoundHeapAllocator", NULL);

	ShutterSoundInitParam initParam;
	initParam.allocator = s_shutterHeap;
	initParam.jobQueue = s_shutterJobQueue;
	return shutterManagerInit(&initParam);
}

/* 0x8124448E */
int shellShutterSoundPlay(SceUInt32 soundType, int useCameraVol)
{
	if (!paf::system::Capability::HasCamera())
		useCameraVol = 0;
	if (s_shutterManager == NULL)
		return kShutterErrNotReady;
	return s_shutterManager->Play(soundType, useCameraVol);
}

/* SceShellSvcServer dispatch table entrypoints for 0x00140000-0x0014FFFF */

/* 0x812443C0 -> 0x812444BC */
int shellSvcDisp_14_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	if (methodId != kShutterMethodPlay)
		return kIpmiErrUnsupported;

	SceUInt32 soundType = *(const SceUInt32 *)inList[0].pData;
	int result = shellShutterSoundPlay(soundType, 1);
	return session->respondToSyncMethodRequest(result, outList, outCount);
}

/* 0x812443D4 */
int shellSvcDisp_14_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)reqId;
	return shellSvcLiveAreaSyncDefault(session, methodId, inList, inCount, NULL, 0);
}
