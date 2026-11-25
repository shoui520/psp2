/*
 * SceNpSnsFacebookDialogPlugin: event bus, mutex, job queue manager, and async Facebook SNS jobs.
 */
#ifndef NP_SNS_FB_DIALOG_JOB_H
#define NP_SNS_FB_DIALOG_JOB_H

#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/vector>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "fb_dialog_plugin.h"

class FbDialogBase;

/* Component attached to FbDialogBase */
class FbComponent {
public:
	FbComponent() : m_owner(NULL) {}
	virtual ~FbComponent() {}
	virtual void OnAttach(FbDialogBase *owner) { m_owner = owner; }
	virtual void OnDetach() { m_owner = NULL; }

	FbDialogBase *m_owner;
};

/* Listener registered on FbEventBus */
class FbEventListener {
public:
	virtual ~FbEventListener() {}
	virtual int OnEvent(int event, void *arg) = 0;
};

/* Event bus owned by FbDialogBase */
class FbEventBus {
public:
	FbEventBus();
	virtual ~FbEventBus();
	virtual void PostEvent(int event, void *arg);
	virtual int AddListener(FbEventListener *listener);
	virtual int RemoveListener(FbEventListener *listener);

	paf::vector<FbEventListener *> m_listeners;
};

/* Lightweight mutex wrapper ("SceFbDialogJobLock") */
class FbMutex {
public:
	FbMutex(const char *name);
	virtual ~FbMutex();

	int Lock();
	int Unlock();

	SceKernelLwMutexWork m_mutex;
};

/* RAII scoped lock */
class FbScopedLock {
public:
	FbScopedLock(FbMutex *mutex);
	virtual ~FbScopedLock();

	FbMutex *m_mutex;
};

/* Localized Facebook permission description (513 bytes) */
typedef struct FbPermissionDesc {
	char name[256];
	char desc[256];
	uint8_t flag;
} FbPermissionDesc;

/* Batch feed post items (716 bytes and 524 bytes) */
typedef struct FbFeedBatchTrophyItem {
	int32_t visibility;
	char story[257];
	char caption[183];
	int32_t rating;
	char comment[260];
	int32_t trophyGrade;
	int32_t trophyFlags;
} FbFeedBatchTrophyItem;

typedef struct FbFeedBatchActivityItem {
	int32_t visibility;
	char story[257];
	char comment[259];
	int32_t activityFlags;
} FbFeedBatchActivityItem;

/* Base async job item ("FbDialogJob") */
class FbDialogJob : public paf::job::JobItem {
public:
	FbDialogJob(FbEventBus *bus);
	virtual ~FbDialogJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);
	virtual void SetRequestId(int reqId);
	virtual int CreateRequest(int index);
	virtual int AbortRequest(int index);

	FbEventBus *m_bus;
	int m_requestId;
};

/* Manages async FbDialogJob execution on SceFbDialogJobQueue */
class FbJobManager : public FbComponent, public FbEventListener {
public:
	FbJobManager(FbEventBus *bus, paf::job::JobQueue *queue);
	virtual ~FbJobManager();
	virtual int OnEvent(int event, void *arg);

	int CancelCurrent();
	void Abort();
	int StartAuthCheck();
	int StartBind(int step, const FbAccountId *account, const char *password, int index);
	int StartCheckPermissions(const char *titleId, SceUInt64 appId, const char *permissions,
				  paf::vector<FbPermissionDesc> *permListOut,
				  char *missingPermsOut,
				  SceNpSnsFacebookLongAccessTokenResult *tokenOut, int shortMode);
	int StartGetAccessToken(const char *titleId, SceUInt64 appId, const char *permissions,
				SceNpSnsFacebookLongAccessTokenResult *tokenOut, int shortMode);
	int StartPostFeed(int index, int postType, int visibility, const char *story,
			  const char *caption, const char *comment, int rating, int trophyGrade,
			  int trophyFlags, int activityFlags);
	int StartBatchPost(int index, int postType, int count,
			   const FbFeedBatchTrophyItem *trophyItems,
			   const FbFeedBatchActivityItem *activityItems);
	int StartSyncBarrier();

	int m_aborting;
	FbEventBus *m_bus;
	paf::job::JobQueue *m_queue;
	int m_busy;
	int m_reserved;
	FbMutex *m_mutex;
};

/* Controller callback interface invoked by FbJobManager::OnEvent */
class FbJobCallbackTarget {
public:
	virtual ~FbJobCallbackTarget() {}
	virtual int OnJobSyncDone() = 0;
	virtual int OnJobAuthCheckDone(int result, int signedIn, int chatAllowed) = 0;
	virtual int OnJobBindDone(int result, int step) = 0;
	virtual int OnJobCheckPermissionsDone(int result, int needPerms) = 0;
	virtual int OnJobGetAccessTokenDone(int result) = 0;
	virtual int OnJobPostFeedDone(int result, int subResult) = 0;
};

#endif /* NP_SNS_FB_DIALOG_JOB_H */
