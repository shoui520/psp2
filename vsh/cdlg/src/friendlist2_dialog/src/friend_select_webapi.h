/*
 * SceFriendList2DialogPlugin: SceFriendSelectWebApi context and base job.
 */
#ifndef FRIENDLIST2_DIALOG_FRIEND_SELECT_WEBAPI_H
#define FRIENDLIST2_DIALOG_FRIEND_SELECT_WEBAPI_H

#include <stdint.h>
#include <scetypes.h>
#include <job/job.h>

/* Base callback interface for FriendLib async tasks */
class FriendLibTaskCallback {
public:
	virtual ~FriendLibTaskCallback() {}
	virtual void OnFinished(int result) = 0;
};

/* Base async task wrapper owning a PAF JobItem */
class FriendLibAsyncTask {
public:
	explicit FriendLibAsyncTask(FriendLibTaskCallback *callback);
	virtual ~FriendLibAsyncTask();

	virtual void Start() = 0;
	virtual void Cancel() = 0;
	virtual void OnJobComplete(int result);
	virtual void NotifyCallback(int result);

	FriendLibTaskCallback *m_callback;
};

/* JobQueue-backed async task base */
class FriendLibQueuedTask : public FriendLibAsyncTask, public paf::job::JobItem {
public:
	FriendLibQueuedTask(paf::job::JobQueue *queue, FriendLibTaskCallback *callback,
			    const char *jobName);
	virtual ~FriendLibQueuedTask();

	virtual void Start();
	virtual void Cancel();
	virtual void OnJobComplete(int result);
	virtual void Finish(int result);

	paf::job::JobQueue *m_queue;
};

/* JSON stream parser hook for SceNpWebApi responses */
class FriendSelectJsonReceiver {
public:
	virtual ~FriendSelectJsonReceiver() {}
	virtual int InitParser(void *parserContext) = 0;
};

/* Global SceFriendSelectWebApi lifecycle */
int FriendSelectWebApiInit(void);
void FriendSelectWebApiTerm(void);
void *FriendSelectAlloc(size_t size);
void FriendSelectFree(void *ptr);

/* Base SceNpWebApi GET request job ("SceFriendSelectWebApi") */
class FriendSelectWebApiJob : public FriendLibQueuedTask {
public:
	explicit FriendSelectWebApiJob(FriendLibTaskCallback *callback);
	virtual ~FriendSelectWebApiJob();
	virtual void Cancel();

protected:
	int ExecuteGetRequest(const char *apiGroup, const char *path,
			      FriendSelectJsonReceiver *receiver);

	int64_t m_requestId;
};

/* Single-active-task scheduler for FriendLibAsyncTask */
bool FriendLibIsTaskRunning(void);
void FriendLibCancelActiveTask(void);
void FriendLibStartTask(FriendLibAsyncTask *task);

#endif /* FRIENDLIST2_DIALOG_FRIEND_SELECT_WEBAPI_H */
