/*
 * np_friend_importer_internal.h: Internal types for SceNpFriendImporter and
 * SceNpFriendProfileImporter.
 * 3.65-CEX: 0x813C541C..0x813C85CF
 */
#ifndef SHELL_NP_FRIEND_IMPORTER_INTERNAL_H
#define SHELL_NP_FRIEND_IMPORTER_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/new>
#include <std/list>
#include <std/string>
#include <std/string.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <job/job.h>
#include <rtc/rtc.h>
#include <np/np_common.h>
#include "np_friend_storage.h"

#define NP_FRIEND_IMPORTER_ERR_INVALID_STATE		(-2141877503)	/* 0x80558B01 */
#define NP_FRIEND_IMPORTER_ERR_INVALID_ARGUMENT		(-2141877502)	/* 0x80558B02 */
#define NP_FRIEND_IMPORTER_ERR_ALREADY_INITIALIZED	(-2141877501)	/* 0x80558B03 */
#define NP_FRIEND_IMPORTER_ERR_NOT_INITIALIZED		(-2141877500)	/* 0x80558B04 */
#define NP_FRIEND_IMPORTER_ERR_OUT_OF_MEMORY		(-2141877499)	/* 0x80558B05 */
#define NP_FRIEND_IMPORTER_ERR_QUEUE_FULL		(-2141877498)	/* 0x80558B06 */

/* Importer private heap helpers (0x813C7378..0x813C73DF) */
void npFriendImporterSetHeap(paf::memory::HeapAllocator *heap);
void npFriendImporterClearHeap(void);
void *npFriendImporterMalloc(SceSize size);
void npFriendImporterFree(void *pointer);
SceSize npFriendImporterGetFreeSize(void);

template <class ElemType>
class NpFriendImporterAllocator {
public:
	ElemType *allocate(size_t count)
	{
		return static_cast<ElemType *>(npFriendImporterMalloc(count * sizeof(ElemType)));
	}

	void deallocate(ElemType *pointer, size_t)
	{
		npFriendImporterFree(pointer);
	}

	void report_failure()
	{
	}
};

/* Concrete player entry stored in friend/blocked lists (0x813C84FC..0x813C85CF) */
class NpFriendPlayerImpl : public NpFriendPlayer {
public:
	NpFriendPlayerImpl(const SceNpId &npId, int playerType, const paf::rtc::Tick &requestDate,
			   int requestDirection);
	virtual ~NpFriendPlayerImpl();

	virtual const SceNpId *GetNpId();
	virtual const char *GetOnlineId();
	virtual int GetPlayerType();
	virtual const paf::rtc::Tick *GetFriendRequestDate();
	virtual int GetRequestDirection();

	static int Create(paf::intrusive_ptr<NpFriendPlayerImpl> *outPlayer, const SceNpId *npId,
			  int playerType, const paf::rtc::Tick *requestDate, int requestDirection);

private:
	SceNpId m_npId;
	int m_playerType;
	paf::rtc::Tick m_requestDate;
	int m_requestDirection;
};

/* Queued event base class processed by SceNpFriendImporter thread */
class NpFriendImporterEvent : public paf::common::RefObj {
public:
	NpFriendImporterEvent() {}
	virtual ~NpFriendImporterEvent() {}
	virtual void Execute() = 0;
};

struct NpFriendImporterJobQueueOption {
	unsigned int numThreads;
	int priority;
	size_t stackSize;
};

/* SceNpFriendImporter singleton (0x813C5464..0x813C5BE0) */
class NpFriendImporter : public paf::thread::Thread {
public:
	explicit NpFriendImporter(paf::memory::HeapAllocator *heap);
	virtual ~NpFriendImporter();

	static int Initialize(paf::memory::HeapAllocator *heap,
			      const NpFriendImporterJobQueueOption *jobOpt);
	static int Terminate(void);
	static int GetInstance(NpFriendImporter **outInstance);
	static paf::job::JobQueue *GetJobQueue(void);

	int EnqueueUpdateSelfProfile(void);
	int EnqueueProfileJob(paf::common::SharedPtr<paf::job::JobItem> &job);
	void CancelAllProfileJobs(void);
	void WakeWorker(void);
	int PushEvent(const paf::intrusive_ptr<NpFriendImporterEvent> &event);

	virtual void EntryFunction();

private:
	void DispatchEvents(void);
	void ClearEvents(void);

	int m_netCtlCallbackId;
	SceUInt32 m_impsListenerId;
	paf::thread::Mutex m_workerMutex;
	paf::thread::Cond m_workerCond;
	paf::memory::HeapAllocator *m_heap;
	SceUInt32 m_maxEvents;
	paf::thread::Mutex m_queueMutex;
	paf::list<paf::intrusive_ptr<NpFriendImporterEvent>,
		  NpFriendImporterAllocator<paf::intrusive_ptr<NpFriendImporterEvent> > > m_eventList;
	paf::job::JobQueue *m_jobQueue;
};

/* Background profile import job (0x813C71C8..0x813C7377) */
int npFriendImportProfileJobCreate(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				   const SceNpId *npId, int requestDirection);

/* SceNpFriendProfileImporter singleton (0x813C7938..0x813C84FB) */
class NpFriendProfileImporter {
public:
	explicit NpFriendProfileImporter(NpFriendStorage *storage);
	virtual ~NpFriendProfileImporter();

	static int Initialize(NpFriendStorage *storage);
	static int Terminate(void);
	static NpFriendProfileImporter *GetInstance(void);

	int RefreshSelfNpId(void);
	void OnSignOut(void);
	void AddPendingRetry(const SceNpId *npId);
	int ImportProfile(const SceNpId *npId, int requestDirection);
	int ImportProfiles(unsigned int count, NpFriendPlayerImpl *const *players);
	void RetryPending(void);
	const SceNpId *GetSelfNpId(void) const;
	int RefreshSelfProfile(void);
	void MarkSelfRetryPending(void);
	int RetrySelfProfile(void);

	static int FetchProfileFromServer(const SceNpId *selfNpId, const SceNpId *targetNpId,
					  struct NpLookupProfile *outProfile,
					  SceUInt32 avatarSizeType);

private:
	int EnsureSelfNpId(void);
	void EnqueueRetryLocked(const SceNpId *npId);

	bool m_selfNpIdValid;
	SceNpId m_selfNpId;
	NpFriendStorage *m_storage;
	paf::list<SceNpId> m_retryQueue;
	paf::thread::RMutex m_lock;
	bool m_selfRetryPending;
};

/* SceFriendCacheCreater bridge (0x81404694, 0x8140479C, 0x8140511C) */
extern int npFriendCacheInit(void);
extern int npFriendCacheTerm(void);
extern int npFriendCacheDownloadAvatar(const char *imageUrl);

#endif /* SHELL_NP_FRIEND_IMPORTER_INTERNAL_H */
