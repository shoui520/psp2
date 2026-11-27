/*
 * np_sns_fb_server.cpp: SceNpSnsFb BlockingIpc server, client session manager,
 * job queue, and async/sync IPMI method handlers (0x8133F6B0..0x8134258F,
 * 0x81440634..0x8144065C).
 */
#include "np_sns_fb_internal.h"

#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <blocking_ipc/blocking_ipc_server.h>
#include <ipmi/session.h>
#include "../ipc/np_ipc_internal.h"

/* SceNpSnsFb IPMI sync/async method IDs (0x10010000..0x10010021) */
#define NP_SNS_FB_METHOD_INIT_SESSION			0x10010000u
#define NP_SNS_FB_METHOD_TERM_SESSION			0x10010001u
#define NP_SNS_FB_METHOD_CHECK_ACCOUNT			0x10010002u
#define NP_SNS_FB_METHOD_CHECK_ACCOUNT_REPLY		0x10010003u
#define NP_SNS_FB_METHOD_AUTH_AND_BIND			0x10010004u
#define NP_SNS_FB_METHOD_AUTH_AND_BIND_REPLY		0x10010005u
#define NP_SNS_FB_METHOD_POST_FEED			0x10010006u
#define NP_SNS_FB_METHOD_POST_FEED_REPLY		0x10010007u
#define NP_SNS_FB_METHOD_SHARE_PURCHASE			0x10010008u
#define NP_SNS_FB_METHOD_SHARE_PURCHASE_REPLY		0x10010009u
#define NP_SNS_FB_METHOD_SHARE_RATING			0x1001000Au
#define NP_SNS_FB_METHOD_SHARE_RATING_REPLY		0x1001000Bu
#define NP_SNS_FB_METHOD_GET_PERMISSIONS_INFO		0x1001000Cu
#define NP_SNS_FB_METHOD_GET_PERMISSIONS_INFO_REPLY	0x1001000Du
#define NP_SNS_FB_METHOD_CHECK_PERMISSIONS		0x1001000Eu
#define NP_SNS_FB_METHOD_CHECK_PERMISSIONS_REPLY	0x1001000Fu
#define NP_SNS_FB_METHOD_GET_ACCESS_TOKEN		0x10010010u
#define NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_REPLY		0x10010011u
#define NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_CHECK		0x10010012u
#define NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_CHECK_REPLY	0x10010013u
#define NP_SNS_FB_METHOD_GET_ACCOUNT			0x10010014u
#define NP_SNS_FB_METHOD_GET_SESSION_SHORT		0x10010015u
#define NP_SNS_FB_METHOD_GET_SHARE_CONFIG		0x10010016u
#define NP_SNS_FB_METHOD_SET_SHARE_CONFIG		0x10010017u
#define NP_SNS_FB_METHOD_CLEAR_ALL			0x10010018u
#define NP_SNS_FB_METHOD_CLEAR_SESSION			0x10010019u
#define NP_SNS_FB_METHOD_CLEAR_SHARE			0x1001001Au
#define NP_SNS_FB_METHOD_IS_SHARE_TROPHY		0x1001001Bu
#define NP_SNS_FB_METHOD_IS_SHARE_PURCHASE		0x1001001Cu
#define NP_SNS_FB_METHOD_IS_SHARE_RATING		0x1001001Du
#define NP_SNS_FB_METHOD_GET_LONG_TOKEN			0x1001001Eu
#define NP_SNS_FB_METHOD_GET_LONG_TOKEN_REPLY		0x1001001Fu
#define NP_SNS_FB_METHOD_GET_LONG_TOKEN_CHECK		0x10010020u
#define NP_SNS_FB_METHOD_GET_LONG_TOKEN_CHECK_REPLY	0x10010021u

#define NP_SNS_FB_BLOCKING_IPC_ERR_INVALID_SIZE		-2146429120	/* 0x80101740 */
#define NP_SNS_FB_IPMI_ERR_UNSUPPORTED			-2147351103	/* 0x80024501 */

#define NP_SNS_FB_JOB_ERR_GENERIC			-2141911538	/* 0x8055060E */
#define NP_SNS_FB_JOB_ERR_ALREADY_INIT			-2141911423	/* 0x80550681 */
#define NP_SNS_FB_JOB_ERR_NOT_INIT			-2141911422	/* 0x80550682 */
#define NP_SNS_FB_JOB_ERR_DUPLICATE			-2141911421	/* 0x80550683 */
#define NP_SNS_FB_JOB_ERR_ALREADY_RUNNING		-2141911420	/* 0x80550684 */
#define NP_SNS_FB_JOB_ERR_QUEUE_STOPPED			-2141911419	/* 0x80550685 */
#define NP_SNS_FB_JOB_ERR_NOT_FOUND			-2141911418	/* 0x80550686 */

enum NpSnsFbJobState {
	NP_SNS_FB_JOB_CREATED		= 1,
	NP_SNS_FB_JOB_QUEUED		= 3,
	NP_SNS_FB_JOB_RUNNING		= 4,
	NP_SNS_FB_JOB_CANCELED_PENDING	= 5,
	NP_SNS_FB_JOB_FINISHED		= 6
};

/* IPMI request/reply wire structures */
struct NpSnsFbInitSessionIn {
	SceUInt32 clientVersion;
};

struct NpSnsFbGetShareConfigIn {
	SceInt32 wantTrophy;
	SceInt32 wantPurchase;
	SceInt32 wantRating;
	SceInt32 wantGameIntegrations;
};

struct NpSnsFbSetShareConfigIn {
	SceInt32 hasTrophy;
	SceInt32 shareTrophy;
	SceInt32 hasPurchase;
	SceInt32 sharePurchase;
	SceInt32 hasRating;
	SceInt32 shareRating;
	SceInt32 hasGameIntegrations;
	SceInt32 gameIntegrations;
};

struct NpSnsFbAsyncReqHeader {
	SceInt32 reqId;
};

struct NpSnsFbPostFeedHeader {
	SceInt32 reqId;
	SceUInt32 padBeforeErrBuf;
	char *errMsgBuf;
	SceSize errMsgBufSize;
};

struct NpSnsFbSharePurchaseHeader {
	SceInt32 reqId;
	SceInt32 serviceType;
	SceInt32 isRental;
	SceInt32 contentType;
	SceInt32 isPreOrder;
};

struct NpSnsFbShareRatingHeader {
	SceInt32 reqId;
	SceInt32 serviceType;
	SceUInt32 ratingScore;
};

struct NpSnsFbGetPermsHeader {
	SceInt32 reqId;
	SceInt32 langIndex;
};

struct NpSnsFbCheckAccountReply {
	SceInt32 result;
	SceInt32 signedIn;
	SceInt32 chatAllowed;
};

struct NpSnsFbFbErrorReply {
	SceInt32 result;
	SceInt32 fbError;
};

struct NpSnsFbPermissionsInfoReply {
	SceInt32 result;
	NpSnsFbPermissionsInfoResult info;
};

struct NpSnsFbCheckPermissionsReply {
	SceInt32 result;
	NpSnsFbCheckPermissionsResult info;
};

struct NpSnsFbShortTokenReply {
	SceInt32 result;
	SceUInt32 padBeforeExpiration;
	SceUInt64 expiration;
	char accessToken[256];
};

struct NpSnsFbLongTokenData {
	SceUInt64 expiration;
	char accessToken[4136];
};

/* Permission-request token buffer copied into job (4144 = 0x1030 bytes) */
struct NpSnsFbTokenInputBuffer {
	SceUInt64 expiration;
	char token[4136];
};

/* --- Per-session tracked request ID linked list (0x8144064C..0x8144065C) --- */

class NpSnsFbRequestIdNode {
public:
	NpSnsFbRequestIdNode() : m_reqId(0), m_next(NULL), m_prev(NULL) {}
	virtual ~NpSnsFbRequestIdNode() {}

	SceInt32 m_reqId;				/* +0x04 */
	NpSnsFbRequestIdNode *m_next;			/* +0x08 */
	NpSnsFbRequestIdNode *m_prev;			/* +0x0C */
};

typedef int (*NpSnsFbAbortCallback)(SceInt32 reqId);

/* Client session entry (48 bytes = 0x30, 0x813423AC..0x8134258F) */
class NpSnsFbClientSession : public NpSnsFbAllocObject {
public:
	NpSnsFbClientSession(IPMI::Session *session, SceUID processId,
			     NpSnsFbAbortCallback abortCb);
	virtual ~NpSnsFbClientSession();

	int addRequestId(SceInt32 reqId);
	int removeRequestId(SceInt32 reqId);
	int abortAllBySystem();
	int abortAndDestroyAll();
	void resume();
	void suspend();

	IPMI::Session *m_session;			/* +0x04 */
	SceUID m_processId;				/* +0x08 */
	SceUInt32 m_clientVersion;			/* +0x0C */
	NpSnsFbRequestIdNode m_reqListHead;		/* +0x10 (16 bytes: vptr, id, next, prev) */
	SceInt32 m_suspended;				/* +0x20 */
	NpSnsFbAbortCallback m_abortCallback;		/* +0x24 */
	NpSnsFbClientSession *m_next;			/* +0x28 */
	NpSnsFbClientSession *m_prev;			/* +0x2C */
};

/* Client session manager (8 bytes, 0x81340C00..0x8134115B) */
class NpSnsFbClientManager : public NpSnsFbAllocObject {
public:
	NpSnsFbClientManager();
	virtual ~NpSnsFbClientManager();

	int init(NpSnsFbLwMutex *mutex);
	int addSession(IPMI::Session *session, SceUID processId, NpSnsFbAbortCallback abortCb);
	int removeSession(IPMI::Session *session);
	int setClientVersion(IPMI::Session *session, SceUInt32 version);
	SceBool hasSession(IPMI::Session *session);
	int addRequestId(IPMI::Session *session, SceInt32 reqId);
	int removeRequestId(IPMI::Session *session, SceInt32 reqId);
	int resumeProcess(SceUID processId);
	int suspendProcess(SceUID processId);
	void abortOtherProcesses(IPMI::Session *session, SceUID processId);

private:
	NpSnsFbLwMutex *m_mutex;			/* +0x04 */
};

/* --- SceNpSnsFb Job Queue & Worker Thread (0x8134115C..0x813417CB) --- */

class NpSnsFbJobQueue;

class NpSnsFbJobItem : public NpSnsFbAllocObject {
public:
	explicit NpSnsFbJobItem(const char *name);
	virtual ~NpSnsFbJobItem();
	virtual int cancel(int cancelError);
	virtual int run() = 0;
	virtual void finish(int result) = 0;

	void unlinkFromQueue();

	SceInt32 m_reqId;				/* +0x04 */
	const char *m_name;				/* +0x08 */
	SceInt32 m_state;				/* +0x0C */
	NpSnsFbJobItem *m_next;				/* +0x10 */
	NpSnsFbJobItem *m_prev;				/* +0x14 */
	NpSnsFbJobQueue *m_queue;			/* +0x18 */
	SceInt32 m_cancelError;				/* +0x1C */
};

class NpSnsFbJobWorkerThread : public NpThreadBase, public NpSnsFbAllocObject {
public:
	explicit NpSnsFbJobWorkerThread(NpSnsFbJobQueue *queue);
	virtual ~NpSnsFbJobWorkerThread();
	virtual int Run();

	NpSnsFbJobQueue *m_queue;			/* +0x10 */
	SceInt32 m_reservedWord;			/* +0x14 */
	SceUInt8 m_exitRequested;			/* +0x18 */
};

class NpSnsFbJobQueue : public NpSnsFbAllocObject {
public:
	NpSnsFbJobQueue();
	virtual ~NpSnsFbJobQueue();

	int init(const char *name, SceInt32 priority, SceSize stackSize);
	int start();
	void shutdown();
	int enqueue(SceInt32 reqId, NpSnsFbJobItem *job);
	int cancel(SceInt32 reqId, int cancelError);
	int cancelAll(int cancelError);
	int dequeue(NpSnsFbJobItem **outJob);
	void completeJob(NpSnsFbJobItem *job, int result);

	NpSnsFbJobItem *findJobLocked(SceInt32 reqId) const;
	void drainFinishedByState(int targetState, bool destroyItem);

	SceUInt8 m_stopRequested;			/* +0x04 */
	SceKernelLwMutexWork m_mutexWork;		/* +0x08 (32 bytes) */
	SceKernelLwCondWork m_condWork;			/* +0x28 (16 bytes) */
	NpSnsFbJobItem *m_pendingHead;			/* +0x38 */
	NpSnsFbJobItem *m_pendingTail;			/* +0x3C */
	NpSnsFbJobItem *m_runningHead;			/* +0x40 */
	NpSnsFbJobItem *m_runningTail;			/* +0x44 */
	NpSnsFbJobItem *m_finishedHead;			/* +0x48 */
	NpSnsFbJobItem *m_finishedTail;			/* +0x4C */
	NpSnsFbJobWorkerThread *m_worker;		/* +0x50 */
};

/* Base server job item carrying session/asyncReqId/fbReqId (0x81440634) */
class NpSnsFbServerJobItem : public NpSnsFbJobItem {
public:
	NpSnsFbServerJobItem(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId)
		: NpSnsFbJobItem("SceNpSnsFbServerJobItem"),
		  m_session(session),
		  m_asyncReqId(asyncReqId),
		  m_fbReqId(fbReqId)
	{
	}
	virtual ~NpSnsFbServerJobItem() {}

	virtual int cancel(int cancelError)
	{
		npSnsFbUtilAbortRequest(m_fbReqId);
		return NpSnsFbJobItem::cancel(cancelError);
	}

	IPMI::Session *m_session;			/* +0x20 */
	SceUInt32 m_asyncReqId;				/* +0x24 */
	SceInt32 m_fbReqId;				/* +0x28 */
};

/* --- Globals (0x815468B4..0x815468C4) --- */
static BlockingIpc::Server *s_fbIpcServer = NULL;
static NpSnsFbJobQueue *s_fbJobQueue = NULL;
static NpSnsFbLwMutex *s_fbServerMutex = NULL;
static NpSnsFbClientManager *s_fbClientManager = NULL;
static NpSnsFbClientSession *s_fbSessionListHead = NULL;

/* --- Lightweight mutex/cond helpers for NpSnsFbJobQueue --- */

static void fbMutexLock(SceKernelLwMutexWork *work)
{
	if (sceKernelLockLwMutex(work, 1, NULL) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(work, &info);
	}
}

static void fbMutexUnlock(SceKernelLwMutexWork *work)
{
	if (sceKernelUnlockLwMutex(work, 1) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(work, &info);
	}
}

static void fbMutexDestroy(SceKernelLwMutexWork *work)
{
	SceKernelLwMutexWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(work, &zeroWork, sizeof(*work)) != 0) {
		sceKernelDeleteLwMutex(work);
		sceClibMemset(work, 0, sizeof(*work));
	}
}

static void fbCondDestroy(SceKernelLwCondWork *work)
{
	SceKernelLwCondWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(work, &zeroWork, sizeof(*work)) != 0) {
		sceKernelDeleteLwCond(work);
		sceClibMemset(work, 0, sizeof(*work));
	}
}

/* --- Async IPMI reply wrappers (0x813417CC..0x81341AC1) --- */

static int sendSingleAsyncReply(IPMI::Session *session, SceUInt32 methodId, SceUInt32 asyncReqId,
				int resultCode, const void *replyBuf, SceSize replySize)
{
	IPMI::DataInfo replyInfo;
	replyInfo.pData = replyBuf;
	replyInfo.dataSize = replySize;
	return session->respondToAsyncMethodRequest(methodId, static_cast<int>(asyncReqId),
						    resultCode, &replyInfo, 1);
}

/* 0x813417CC */
static int replyCheckAccount(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
			     const NpSnsFbCheckAccountReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_CHECK_ACCOUNT_REPLY, asyncReqId,
				    resultCode, reply, sizeof(*reply));
}

/* 0x8134180E */
static int replyAuthAndBind(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
			    const SceInt32 *replyResult)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_AUTH_AND_BIND_REPLY, asyncReqId,
				    resultCode, replyResult, sizeof(*replyResult));
}

/* 0x81341850 */
static int replyPostFeed(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
			 const NpSnsFbFbErrorReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_POST_FEED_REPLY, asyncReqId,
				    resultCode, reply, sizeof(*reply));
}

/* 0x81341892 */
static int replySharePurchase(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
			      const NpSnsFbFbErrorReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_SHARE_PURCHASE_REPLY, asyncReqId,
				    resultCode, reply, sizeof(*reply));
}

/* 0x813418D6 */
static int replyShareRating(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
			    const NpSnsFbFbErrorReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_SHARE_RATING_REPLY, asyncReqId,
				    resultCode, reply, sizeof(*reply));
}

/* 0x8134191A */
static int replyGetPermissionsInfo(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
				   const NpSnsFbPermissionsInfoReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_GET_PERMISSIONS_INFO_REPLY,
				    asyncReqId, resultCode, reply, 6164);
}

/* 0x8134195E */
static int replyCheckPermissions(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
				 const NpSnsFbCheckPermissionsReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_CHECK_PERMISSIONS_REPLY, asyncReqId,
				    resultCode, reply, 520);
}

/* 0x813419A2 */
static int replyGetAccessTokenShort(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
				    const NpSnsFbShortTokenReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_REPLY, asyncReqId,
				    resultCode, reply, 272);
}

/* 0x813419E6 */
static int replyGetAccessTokenWithPermCheckShort(IPMI::Session *session, SceUInt32 asyncReqId,
						 int resultCode,
						 const NpSnsFbShortTokenReply *reply)
{
	return sendSingleAsyncReply(session, NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_CHECK_REPLY,
				    asyncReqId, resultCode, reply, 272);
}

/* 0x81341A2A */
static int replyGetAccessTokenLong(IPMI::Session *session, SceUInt32 asyncReqId, int resultCode,
				   const SceInt32 *replyResult,
				   const NpSnsFbLongTokenData *tokenData)
{
	IPMI::DataInfo replyList[2];
	replyList[0].pData = replyResult;
	replyList[0].dataSize = sizeof(*replyResult);
	replyList[1].pData = tokenData;
	replyList[1].dataSize = sizeof(*tokenData);
	return session->respondToAsyncMethodRequest(NP_SNS_FB_METHOD_GET_LONG_TOKEN_REPLY,
						    static_cast<int>(asyncReqId), resultCode,
						    replyList, 2);
}

/* 0x81341A76 */
static int replyGetAccessTokenWithPermCheckLong(IPMI::Session *session, SceUInt32 asyncReqId,
						int resultCode, const SceInt32 *replyResult,
						const NpSnsFbLongTokenData *tokenData)
{
	IPMI::DataInfo replyList[2];
	replyList[0].pData = replyResult;
	replyList[0].dataSize = sizeof(*replyResult);
	replyList[1].pData = tokenData;
	replyList[1].dataSize = sizeof(*tokenData);
	return session->respondToAsyncMethodRequest(NP_SNS_FB_METHOD_GET_LONG_TOKEN_CHECK_REPLY,
						    static_cast<int>(asyncReqId), resultCode,
						    replyList, 2);
}

/* --- Concrete Async Job Classes (0x81341E44..0x813423AB) --- */

class NpSnsFbCheckAccountJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbCheckAccountJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId)
	{
		sceClibMemset(&m_reply, 0, sizeof(m_reply));
	}

	/* 0x81341E44 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		return npSnsFbUtilCheckAccount(m_fbReqId, &m_reply.signedIn, &m_reply.chatAllowed);
	}

	/* 0x81341E68 */
	virtual void finish(int result)
	{
		m_reply.result = result;
		replyCheckAccount(m_session, m_asyncReqId, result, &m_reply);
	}

	NpSnsFbCheckAccountReply m_reply;		/* +0x2C */
};

class NpSnsFbAuthAndBindJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbAuthAndBindJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
			      const char *email, const char *password)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_email(email),
		  m_password(password),
		  m_fbError(0)
	{
	}

	/* 0x81341E80 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		return npSnsFbUtilAuthAndBind(m_fbReqId, m_email, m_password, &m_fbError);
	}

	/* 0x81341EA6 */
	virtual void finish(int result)
	{
		SceInt32 status = result;
		replyAuthAndBind(m_session, m_asyncReqId, result, &status);
	}

	const char *m_email;				/* +0x2C */
	const char *m_password;				/* +0x30 */
	SceInt32 m_fbError;				/* +0x34 */
};

class NpSnsFbPostFeedJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbPostFeedJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
			   const char *message, const char *attachment, const char *actionLinks,
			   char *errMsgBuf, SceSize errMsgBufSize)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_message(message),
		  m_attachment(attachment),
		  m_actionLinks(actionLinks),
		  m_errMsgBuf(errMsgBuf),
		  m_errMsgBufSize(errMsgBufSize),
		  m_fbError(0)
	{
	}

	/* 0x81341ED6 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		return npSnsFbUtilPostFeed(m_fbReqId, m_message, m_attachment, m_actionLinks,
					   m_errMsgBuf, m_errMsgBufSize, &m_fbError);
	}

	/* 0x81341F0E */
	virtual void finish(int result)
	{
		NpSnsFbFbErrorReply reply;
		reply.result = result;
		reply.fbError = m_fbError;
		replyPostFeed(m_session, m_asyncReqId, result, &reply);
	}

	const char *m_message;				/* +0x2C */
	const char *m_attachment;			/* +0x30 */
	const char *m_actionLinks;			/* +0x34 */
	char *m_errMsgBuf;				/* +0x38 */
	SceSize m_errMsgBufSize;			/* +0x3C */
	SceInt32 m_fbError;				/* +0x40 */
};

class NpSnsFbSharePurchaseJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbSharePurchaseJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				SceInt32 serviceType, const char *titleName, const char *contentId,
				SceInt32 isRental, const char *imageUrl, SceInt32 contentType,
				SceInt32 isPreOrder)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_serviceType(serviceType),
		  m_titleName(titleName),
		  m_contentId(contentId),
		  m_isRental(isRental),
		  m_imageUrl(imageUrl),
		  m_contentType(contentType),
		  m_isPreOrder(isPreOrder),
		  m_fbError(0)
	{
	}

	/* 0x81341F44 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		return npSnsFbSharePurchase(m_fbReqId, m_serviceType, m_titleName, m_contentId,
					    m_isRental, m_imageUrl, m_contentType, m_isPreOrder,
					    &m_fbError);
	}

	/* 0x81341F8E */
	virtual void finish(int result)
	{
		NpSnsFbFbErrorReply reply;
		reply.result = result;
		reply.fbError = m_fbError;
		replySharePurchase(m_session, m_asyncReqId, result, &reply);
	}

	SceInt32 m_serviceType;				/* +0x2C */
	const char *m_titleName;			/* +0x30 */
	const char *m_contentId;			/* +0x34 */
	SceInt32 m_isRental;				/* +0x38 */
	const char *m_imageUrl;				/* +0x3C */
	SceInt32 m_contentType;				/* +0x40 */
	SceInt32 m_isPreOrder;				/* +0x44 */
	SceInt32 m_fbError;				/* +0x48 */
};

class NpSnsFbShareRatingJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbShareRatingJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
			      SceInt32 serviceType, const char *titleName, const char *imageUrl,
			      SceUInt32 ratingScore)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_serviceType(serviceType),
		  m_titleName(titleName),
		  m_imageUrl(imageUrl),
		  m_ratingScore(ratingScore),
		  m_fbError(0)
	{
	}

	/* 0x81341FC4 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		return npSnsFbShareRating(m_fbReqId, m_serviceType, m_titleName, m_imageUrl,
					  m_ratingScore, &m_fbError);
	}

	/* 0x81341FF6 */
	virtual void finish(int result)
	{
		NpSnsFbFbErrorReply reply;
		reply.result = result;
		reply.fbError = m_fbError;
		replyShareRating(m_session, m_asyncReqId, result, &reply);
	}

	SceInt32 m_serviceType;				/* +0x2C */
	const char *m_titleName;			/* +0x30 */
	const char *m_imageUrl;				/* +0x34 */
	SceUInt32 m_ratingScore;			/* +0x38 */
	SceInt32 m_fbError;				/* +0x3C */
};

class NpSnsFbGetPermissionsInfoJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbGetPermissionsInfoJob(IPMI::Session *session, SceUInt32 asyncReqId,
				     SceInt32 fbReqId, SceInt32 langIndex,
				     const NpSnsFbTokenInputBuffer *tokenBuf,
				     const char *permList)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_langIndex(langIndex),
		  m_permList(permList)
	{
		sceClibMemcpy_safe(&m_tokenBuf, tokenBuf, sizeof(m_tokenBuf));
		sceClibMemset(&m_reply, 0, sizeof(m_reply));
	}

	/* 0x8134202C */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		char errMsg[256];
		sceClibMemset(errMsg, 0, sizeof(errMsg));
		sceClibMemset(&m_reply, 0, sizeof(m_reply));
		return npSnsFbUtilGetPermissionsInfo(m_fbReqId, m_langIndex, m_permList,
						     &m_reply.info, errMsg, sizeof(errMsg) - 1,
						     NULL);
	}

	/* 0x813420A0 */
	virtual void finish(int result)
	{
		m_reply.result = result;
		replyGetPermissionsInfo(m_session, m_asyncReqId, result, &m_reply);
	}

	SceInt32 m_langIndex;				/* +0x2C */
	NpSnsFbTokenInputBuffer m_tokenBuf;		/* +0x30 (0x1030 bytes) */
	const char *m_permList;				/* +0x1060 */
	NpSnsFbPermissionsInfoReply m_reply;		/* +0x1064 */
};

class NpSnsFbCheckPermissionsJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbCheckPermissionsJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				   const NpSnsFbTokenInputBuffer *tokenBuf, const char *permList)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_padBeforeToken(0),
		  m_permList(permList)
	{
		sceClibMemcpy_safe(&m_tokenBuf, tokenBuf, sizeof(m_tokenBuf));
		sceClibMemset(&m_reply, 0, sizeof(m_reply));
	}

	/* 0x813420B8 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		char errMsg[256];
		sceClibMemset(errMsg, 0, sizeof(errMsg));
		sceClibMemset(&m_reply, 0, sizeof(m_reply));
		return npSnsFbUtilCheckPermissions(m_fbReqId, m_tokenBuf.token, m_permList,
						   &m_reply.info, errMsg, sizeof(errMsg) - 1,
						   NULL);
	}

	/* 0x8134212A */
	virtual void finish(int result)
	{
		m_reply.result = result;
		replyCheckPermissions(m_session, m_asyncReqId, result, &m_reply);
	}

	SceUInt32 m_padBeforeToken;			/* +0x2C */
	NpSnsFbTokenInputBuffer m_tokenBuf;		/* +0x30 (0x1030 bytes) */
	const char *m_permList;				/* +0x1060 */
	NpSnsFbCheckPermissionsReply m_reply;		/* +0x1064 */
};

class NpSnsFbGetAccessTokenJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbGetAccessTokenJob(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				 const char *proxiedAppId, const char *remoteAppId,
				 const char *permissions, SceInt32 isLongToken)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_proxiedAppId(proxiedAppId),
		  m_remoteAppId(remoteAppId),
		  m_permissions(permissions),
		  m_isLongToken(isLongToken)
	{
		sceClibMemset(&m_tokenOut, 0, sizeof(m_tokenOut));
	}

	/* 0x81342142 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		char errMsg[256];
		sceClibMemset(errMsg, 0, sizeof(errMsg));
		sceClibMemset(&m_tokenOut, 0, sizeof(m_tokenOut));
		return npSnsFbUtilGetAccessToken(m_fbReqId, m_proxiedAppId, m_remoteAppId,
						 m_permissions, &m_tokenOut, errMsg,
						 sizeof(errMsg) - 1, NULL);
	}

	/* 0x813421AC */
	virtual void finish(int result)
	{
		SceInt32 status = result;
		if (m_isLongToken != 0) {
			NpSnsFbLongTokenData longData;
			sceClibMemset(&longData, 0, sizeof(longData));
			if (status >= 0) {
				longData.expiration = m_tokenOut.expiration;
				sceClibStrncpy(longData.accessToken, m_tokenOut.accessToken,
					       0x1000u);
			}
			replyGetAccessTokenLong(m_session, m_asyncReqId, status, &status,
						&longData);
		} else {
			NpSnsFbShortTokenReply shortReply;
			sceClibMemset(&shortReply, 0, sizeof(shortReply));
			shortReply.result = status;
			if (status >= 0) {
				if (static_cast<SceInt32>(
					    sceClibStrnlen(m_tokenOut.accessToken, 0x1000u)) <= 255) {
					sceClibStrncpy(shortReply.accessToken,
						       m_tokenOut.accessToken, 0xFFu);
					shortReply.expiration = m_tokenOut.expiration;
				} else {
					status = NP_SNS_FB_ERR_XML_PARSE;
					shortReply.result = NP_SNS_FB_ERR_XML_PARSE;
				}
			}
			replyGetAccessTokenShort(m_session, m_asyncReqId, status, &shortReply);
		}
	}

	const char *m_proxiedAppId;			/* +0x2C */
	const char *m_remoteAppId;			/* +0x30 */
	const char *m_permissions;			/* +0x34 */
	SceInt32 m_isLongToken;				/* +0x38 */
	SceUInt32 m_padBeforeToken;			/* +0x3C */
	NpSnsFbAccessTokenOutput m_tokenOut;		/* +0x40 */
};

class NpSnsFbGetAccessTokenWithPermCheckJob : public NpSnsFbServerJobItem {
public:
	NpSnsFbGetAccessTokenWithPermCheckJob(IPMI::Session *session, SceUInt32 asyncReqId,
					      SceInt32 fbReqId, const char *proxiedAppId,
					      const char *remoteAppId, const char *permissions,
					      SceInt32 isLongToken)
		: NpSnsFbServerJobItem(session, asyncReqId, fbReqId),
		  m_proxiedAppId(proxiedAppId),
		  m_remoteAppId(remoteAppId),
		  m_permissions(permissions),
		  m_isLongToken(isLongToken)
	{
		sceClibMemset(&m_tokenOut, 0, sizeof(m_tokenOut));
	}

	/* 0x81342276 */
	virtual int run()
	{
		if (m_fbReqId <= 0)
			return NP_SNS_FB_ERR_GENERIC;
		char errMsg[256];
		sceClibMemset(errMsg, 0, sizeof(errMsg));
		sceClibMemset(&m_tokenOut, 0, sizeof(m_tokenOut));
		return npSnsFbUtilGetAccessTokenWithPermCheck(m_fbReqId, m_proxiedAppId,
							      m_remoteAppId, m_permissions,
							      &m_tokenOut, errMsg,
							      sizeof(errMsg) - 1, NULL);
	}

	/* 0x813422E0 */
	virtual void finish(int result)
	{
		SceInt32 status = result;
		if (m_isLongToken != 0) {
			NpSnsFbLongTokenData longData;
			sceClibMemset(&longData, 0, sizeof(longData));
			if (status >= 0) {
				longData.expiration = m_tokenOut.expiration;
				sceClibStrncpy(longData.accessToken, m_tokenOut.accessToken,
					       0x1000u);
			}
			replyGetAccessTokenWithPermCheckLong(m_session, m_asyncReqId, status,
							     &status, &longData);
		} else {
			NpSnsFbShortTokenReply shortReply;
			sceClibMemset(&shortReply, 0, sizeof(shortReply));
			shortReply.result = status;
			if (status >= 0) {
				if (static_cast<SceInt32>(
					    sceClibStrnlen(m_tokenOut.accessToken, 0x1000u)) <= 255) {
					sceClibStrncpy(shortReply.accessToken,
						       m_tokenOut.accessToken, 0xFFu);
					shortReply.expiration = m_tokenOut.expiration;
				} else {
					status = NP_SNS_FB_ERR_XML_PARSE;
					shortReply.result = NP_SNS_FB_ERR_XML_PARSE;
				}
			}
			replyGetAccessTokenWithPermCheckShort(m_session, m_asyncReqId, status,
							      &shortReply);
		}
	}

	const char *m_proxiedAppId;			/* +0x2C */
	const char *m_remoteAppId;			/* +0x30 */
	const char *m_permissions;			/* +0x34 */
	SceInt32 m_isLongToken;				/* +0x38 */
	SceUInt32 m_padBeforeToken;			/* +0x3C */
	NpSnsFbAccessTokenOutput m_tokenOut;		/* +0x40 */
};

/* --- NpSnsFbClientSession implementation (0x813423AC..0x8134258F) --- */

/* 0x813423AC */
NpSnsFbClientSession::NpSnsFbClientSession(IPMI::Session *session, SceUID processId,
					   NpSnsFbAbortCallback abortCb)
	: m_session(session),
	  m_processId(processId),
	  m_clientVersion(0),
	  m_suspended(0),
	  m_abortCallback(abortCb),
	  m_next(NULL),
	  m_prev(NULL)
{
}

/* 0x813423FC / 0x8134243A */
NpSnsFbClientSession::~NpSnsFbClientSession()
{
	NpSnsFbRequestIdNode *cur = m_reqListHead.m_next;
	while (cur != NULL) {
		NpSnsFbRequestIdNode *next = cur->m_next;
		delete cur;
		cur = next;
	}
	m_reqListHead.m_next = NULL;
}

/* 0x8134247E */
int NpSnsFbClientSession::addRequestId(SceInt32 reqId)
{
	for (NpSnsFbRequestIdNode *cur = m_reqListHead.m_next; cur != NULL; cur = cur->m_next) {
		if (cur->m_reqId == reqId)
			return NP_SNS_FB_ERR_INTERNAL_ERROR;
	}

	NpSnsFbRequestIdNode *node = new NpSnsFbRequestIdNode();
	if (node == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	node->m_reqId = reqId;
	NpSnsFbRequestIdNode *tail = &m_reqListHead;
	while (tail->m_next != NULL)
		tail = tail->m_next;

	tail->m_next = node;
	node->m_next = NULL;
	node->m_prev = tail;
	return 0;
}

/* 0x813424EA */
int NpSnsFbClientSession::removeRequestId(SceInt32 reqId)
{
	NpSnsFbRequestIdNode *cur = m_reqListHead.m_next;
	while (cur != NULL) {
		if (cur->m_reqId == reqId) {
			if (cur->m_prev != NULL)
				cur->m_prev->m_next = cur->m_next;
			if (cur->m_next != NULL)
				cur->m_next->m_prev = cur->m_prev;
			delete cur;
			return 0;
		}
		cur = cur->m_next;
	}
	return NP_SNS_FB_ERR_INTERNAL_ERROR;
}

/* 0x8134253E */
int NpSnsFbClientSession::abortAllBySystem()
{
	for (NpSnsFbRequestIdNode *cur = m_reqListHead.m_next; cur != NULL; cur = cur->m_next) {
		if (m_abortCallback != NULL) {
			m_abortCallback(cur->m_reqId);
			npSnsFbUtilAbortRequestBySystem(cur->m_reqId);
		}
	}
	return 0;
}

/* 0x81342560 */
int NpSnsFbClientSession::abortAndDestroyAll()
{
	for (NpSnsFbRequestIdNode *cur = m_reqListHead.m_next; cur != NULL; cur = cur->m_next) {
		if (m_abortCallback != NULL) {
			m_abortCallback(cur->m_reqId);
			npSnsFbUtilDestroyRequest(cur->m_reqId);
		}
	}
	return 0;
}

/* 0x81342582 */
void NpSnsFbClientSession::resume()
{
	m_suspended = 0;
}

/* 0x81342588 */
void NpSnsFbClientSession::suspend()
{
	m_suspended = 1;
}

/* --- NpSnsFbClientManager implementation (0x81340C00..0x8134115B) --- */

/* 0x81340C00 */
NpSnsFbClientManager::NpSnsFbClientManager()
	: m_mutex(NULL)
{
	s_fbSessionListHead = NULL;
}

/* 0x81340C24 / 0x81340C40 */
NpSnsFbClientManager::~NpSnsFbClientManager()
{
}

/* 0x81340C60 */
int NpSnsFbClientManager::init(NpSnsFbLwMutex *mutex)
{
	if (mutex == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	m_mutex = mutex;
	if (s_fbSessionListHead != NULL)
		return 0;

	s_fbSessionListHead = new NpSnsFbClientSession(NULL, -1, NULL);
	return (s_fbSessionListHead != NULL) ? 0 : NP_SNS_FB_ERR_OUT_OF_MEMORY;
}

/* 0x81340CB2 */
int NpSnsFbClientManager::addSession(IPMI::Session *session, SceUID processId,
				     NpSnsFbAbortCallback abortCb)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbClientSession *client = new NpSnsFbClientSession(session, processId, abortCb);
	if (client == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	{
		NpSnsFbScopedLock guard(m_mutex);
		NpSnsFbClientSession *tail = s_fbSessionListHead;
		while (tail->m_next != NULL)
			tail = tail->m_next;
		tail->m_next = client;
		client->m_next = NULL;
		client->m_prev = tail;
	}
	if (s_fbSessionListHead != NULL) {
		NpSnsFbScopedLock guard(m_mutex);
	}
	return 0;
}

/* 0x81340D70 */
int NpSnsFbClientManager::removeSession(IPMI::Session *session)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	int ret = NP_SNS_FB_ERR_INVALID_ARGUMENT;
	{
		NpSnsFbScopedLock guard(m_mutex);
		NpSnsFbClientSession *cur = s_fbSessionListHead->m_next;
		while (cur != NULL) {
			if (cur->m_session == session) {
				if (cur->m_prev != NULL)
					cur->m_prev->m_next = cur->m_next;
				if (cur->m_next != NULL)
					cur->m_next->m_prev = cur->m_prev;
				ret = cur->abortAndDestroyAll();
				delete cur;
				break;
			}
			cur = cur->m_next;
		}
	}
	if (ret != NP_SNS_FB_ERR_INVALID_ARGUMENT && s_fbSessionListHead != NULL) {
		NpSnsFbScopedLock guard(m_mutex);
	}
	return ret;
}

/* 0x81340E26 */
int NpSnsFbClientManager::setClientVersion(IPMI::Session *session, SceUInt32 version)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_session == session) {
			cur->m_clientVersion = version;
			return 0;
		}
	}
	return NP_SNS_FB_ERR_INVALID_ARGUMENT;
}

/* 0x81340EA4 */
SceBool NpSnsFbClientManager::hasSession(IPMI::Session *session)
{
	if (s_fbSessionListHead == NULL)
		return static_cast<SceBool>(NP_SNS_FB_ERR_INTERNAL_ERROR);

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_session == session)
			return SCE_TRUE;
	}
	return SCE_FALSE;
}

/* 0x81340F1E */
int NpSnsFbClientManager::addRequestId(IPMI::Session *session, SceInt32 reqId)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_session == session)
			return cur->addRequestId(reqId);
	}
	return NP_SNS_FB_ERR_INVALID_ARGUMENT;
}

/* 0x81340FA0 */
int NpSnsFbClientManager::removeRequestId(IPMI::Session *session, SceInt32 reqId)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_session == session)
			return cur->removeRequestId(reqId);
	}
	return NP_SNS_FB_ERR_INVALID_ARGUMENT;
}

/* 0x81341024 */
int NpSnsFbClientManager::resumeProcess(SceUID processId)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_processId == processId)
			cur->resume();
	}
	return 0;
}

/* 0x81341090 */
int NpSnsFbClientManager::suspendProcess(SceUID processId)
{
	if (s_fbSessionListHead == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_processId == processId)
			cur->suspend();
	}
	return 0;
}

/* 0x813410FC */
void NpSnsFbClientManager::abortOtherProcesses(IPMI::Session *session, SceUID processId)
{
	(void)session;
	if (s_fbSessionListHead == NULL)
		return;

	NpSnsFbScopedLock guard(m_mutex);
	for (NpSnsFbClientSession *cur = s_fbSessionListHead->m_next; cur != NULL;
	     cur = cur->m_next) {
		if (cur->m_processId != processId)
			cur->abortAllBySystem();
	}
}

/* --- NpSnsFbJobItem, NpSnsFbJobQueue, NpSnsFbJobWorkerThread (0x8134115C..0x813417CB) --- */

/* 0x8134115C */
NpSnsFbJobItem::NpSnsFbJobItem(const char *name)
	: m_reqId(-1),
	  m_name(name),
	  m_state(NP_SNS_FB_JOB_CREATED),
	  m_next(NULL),
	  m_prev(NULL),
	  m_queue(NULL),
	  m_cancelError(0)
{
}

/* 0x8134118C / 0x813411A8 */
NpSnsFbJobItem::~NpSnsFbJobItem()
{
}

void NpSnsFbJobItem::unlinkFromQueue()
{
	NpSnsFbJobItem **headPtr = NULL;
	NpSnsFbJobItem **tailPtr = NULL;

	if (m_state == NP_SNS_FB_JOB_QUEUED) {
		headPtr = &m_queue->m_pendingHead;
		tailPtr = &m_queue->m_pendingTail;
	} else if (m_state == NP_SNS_FB_JOB_RUNNING) {
		headPtr = &m_queue->m_runningHead;
		tailPtr = &m_queue->m_runningTail;
	} else if (m_state == NP_SNS_FB_JOB_CANCELED_PENDING ||
		   m_state == NP_SNS_FB_JOB_FINISHED) {
		headPtr = &m_queue->m_finishedHead;
		tailPtr = &m_queue->m_finishedTail;
	} else {
		return;
	}

	if (m_next != NULL)
		m_next->m_prev = m_prev;
	else
		*tailPtr = m_prev;

	if (m_prev != NULL)
		m_prev->m_next = m_next;
	else
		*headPtr = m_next;

	m_next = NULL;
	m_prev = NULL;
}

/* 0x813411CA */
int NpSnsFbJobItem::cancel(int cancelError)
{
	int prevState = m_state;
	if (prevState == NP_SNS_FB_JOB_QUEUED) {
		unlinkFromQueue();
		if (m_state == NP_SNS_FB_JOB_RUNNING)
			m_state = NP_SNS_FB_JOB_FINISHED;
		else if (m_state == NP_SNS_FB_JOB_QUEUED)
			m_state = NP_SNS_FB_JOB_CANCELED_PENDING;

		m_next = NULL;
		m_prev = m_queue->m_finishedTail;
		if (m_queue->m_finishedTail != NULL)
			m_queue->m_finishedTail->m_next = this;
		if (m_queue->m_finishedHead == NULL)
			m_queue->m_finishedHead = this;
		m_queue->m_finishedTail = this;
		m_cancelError = cancelError;
	}
	return prevState;
}

/* 0x81341704 / 0x8134172E / 0x813417B0 / 0x813417BE */
NpSnsFbJobWorkerThread::NpSnsFbJobWorkerThread(NpSnsFbJobQueue *queue)
	: m_queue(queue),
	  m_reservedWord(0),
	  m_exitRequested(0)
{
}

NpSnsFbJobWorkerThread::~NpSnsFbJobWorkerThread()
{
}

/* 0x8134175C */
int NpSnsFbJobWorkerThread::Run()
{
	while (m_exitRequested == 0) {
		NpSnsFbJobItem *job = NULL;
		if (m_queue->dequeue(&job) < 0)
			break;
		int result = job->run();
		m_queue->completeJob(job, result);
	}
	return 0;
}

/* 0x81341210 */
NpSnsFbJobQueue::NpSnsFbJobQueue()
	: m_stopRequested(0),
	  m_pendingHead(NULL),
	  m_pendingTail(NULL),
	  m_runningHead(NULL),
	  m_runningTail(NULL),
	  m_finishedHead(NULL),
	  m_finishedTail(NULL),
	  m_worker(NULL)
{
	sceClibMemset(&m_mutexWork, 0, sizeof(m_mutexWork));
	sceClibMemset(&m_condWork, 0, sizeof(m_condWork));
}

/* 0x81341328 / 0x8134135A */
NpSnsFbJobQueue::~NpSnsFbJobQueue()
{
	shutdown();
	fbCondDestroy(&m_condWork);
	fbMutexDestroy(&m_mutexWork);
}

NpSnsFbJobItem *NpSnsFbJobQueue::findJobLocked(SceInt32 reqId) const
{
	for (NpSnsFbJobItem *cur = m_pendingHead; cur != NULL; cur = cur->m_next) {
		if (cur->m_reqId == reqId)
			return cur;
	}
	for (NpSnsFbJobItem *cur = m_runningHead; cur != NULL; cur = cur->m_next) {
		if (cur->m_reqId == reqId)
			return cur;
	}
	for (NpSnsFbJobItem *cur = m_finishedHead; cur != NULL; cur = cur->m_next) {
		if (cur->m_reqId == reqId)
			return cur;
	}
	return NULL;
}

void NpSnsFbJobQueue::drainFinishedByState(int targetState, bool destroyItem)
{
	for (;;) {
		NpSnsFbJobItem *found = NULL;
		for (NpSnsFbJobItem *cur = m_finishedHead; cur != NULL; cur = cur->m_next) {
			if (cur->m_state == targetState) {
				found = cur;
				break;
			}
		}
		if (found == NULL)
			return;
		found->unlinkFromQueue();
		found->finish(found->m_cancelError);
		if (destroyItem)
			delete found;
	}
}

/* 0x81341248 */
int NpSnsFbJobQueue::cancelAll(int cancelError)
{
	fbMutexLock(&m_mutexWork);
	for (NpSnsFbJobItem *cur = m_pendingHead; cur != NULL; cur = cur->m_next)
		cur->cancel(cancelError);
	for (NpSnsFbJobItem *cur = m_runningHead; cur != NULL; cur = cur->m_next)
		cur->cancel(cancelError);
	fbMutexUnlock(&m_mutexWork);
	drainFinishedByState(NP_SNS_FB_JOB_CANCELED_PENDING, true);
	return 0;
}

/* 0x813412C0 */
void NpSnsFbJobQueue::shutdown()
{
	cancelAll(NP_SNS_FB_JOB_ERR_GENERIC);
	if (m_worker != NULL) {
		if (m_worker->IsStarted()) {
			m_worker->m_exitRequested = 1;
			m_stopRequested = 1;
			if (sceKernelSignalLwCond(&m_condWork) >= 0)
				m_worker->Join(NULL);
		}
		m_worker->Delete();
		delete m_worker;
		m_worker = NULL;
	}
	fbCondDestroy(&m_condWork);
	fbMutexDestroy(&m_mutexWork);
}

/* 0x81341392 */
int NpSnsFbJobQueue::init(const char *name, SceInt32 priority, SceSize stackSize)
{
	int ret = sceKernelCreateLwMutex(&m_mutexWork, name, 0x2000u, 0, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelCreateLwCond(&m_condWork, name, 0x2000u, &m_mutexWork, NULL);
	if (ret < 0) {
		fbMutexDestroy(&m_mutexWork);
		return ret;
	}

	m_worker = new NpSnsFbJobWorkerThread(this);
	if (m_worker == NULL) {
		fbCondDestroy(&m_condWork);
		fbMutexDestroy(&m_mutexWork);
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;
	}

	ret = m_worker->Create(name, priority, stackSize);
	if (ret < 0) {
		delete m_worker;
		m_worker = NULL;
		fbCondDestroy(&m_condWork);
		fbMutexDestroy(&m_mutexWork);
		return ret;
	}
	return 0;
}

/* 0x81341444 */
int NpSnsFbJobQueue::start()
{
	if (m_worker == NULL)
		return NP_SNS_FB_JOB_ERR_NOT_INIT;
	if (m_worker->IsStarted())
		return NP_SNS_FB_JOB_ERR_ALREADY_RUNNING;

	m_stopRequested = 0;
	int ret = m_worker->Start();
	if (ret >= 0)
		return 0;
	m_stopRequested = 1;
	return ret;
}

/* 0x81341480 */
int NpSnsFbJobQueue::enqueue(SceInt32 reqId, NpSnsFbJobItem *job)
{
	if (m_stopRequested != 0)
		return NP_SNS_FB_JOB_ERR_QUEUE_STOPPED;

	fbMutexLock(&m_mutexWork);
	if (findJobLocked(reqId) != NULL) {
		fbMutexUnlock(&m_mutexWork);
		return NP_SNS_FB_JOB_ERR_DUPLICATE;
	}

	job->m_queue = this;
	job->m_reqId = reqId;
	job->m_next = NULL;
	job->m_prev = m_pendingTail;
	job->m_state = NP_SNS_FB_JOB_QUEUED;
	if (m_pendingTail != NULL)
		m_pendingTail->m_next = job;
	if (m_pendingHead == NULL)
		m_pendingHead = job;
	m_pendingTail = job;

	fbMutexUnlock(&m_mutexWork);
	int ret = sceKernelSignalLwCond(&m_condWork);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8134152A */
int NpSnsFbJobQueue::cancel(SceInt32 reqId, int cancelError)
{
	fbMutexLock(&m_mutexWork);
	NpSnsFbJobItem *job = findJobLocked(reqId);
	if (job == NULL || job->m_state == NP_SNS_FB_JOB_FINISHED) {
		fbMutexUnlock(&m_mutexWork);
		return NP_SNS_FB_JOB_ERR_NOT_FOUND;
	}

	job->cancel(cancelError);
	fbMutexUnlock(&m_mutexWork);
	drainFinishedByState(NP_SNS_FB_JOB_CANCELED_PENDING, true);
	return 0;
}

/* 0x813415DA */
int NpSnsFbJobQueue::dequeue(NpSnsFbJobItem **outJob)
{
	fbMutexLock(&m_mutexWork);
	NpSnsFbJobItem *job = NULL;
	while (m_stopRequested == 0) {
		job = m_pendingHead;
		if (job != NULL)
			break;
		int ret = sceKernelWaitLwCond(&m_condWork, NULL);
		if (ret < 0) {
			fbMutexUnlock(&m_mutexWork);
			return ret;
		}
	}

	if (m_stopRequested != 0) {
		fbMutexUnlock(&m_mutexWork);
		return NP_SNS_FB_JOB_ERR_QUEUE_STOPPED;
	}

	job->unlinkFromQueue();
	job->m_next = NULL;
	job->m_prev = m_runningTail;
	job->m_state = NP_SNS_FB_JOB_RUNNING;
	if (m_runningTail != NULL)
		m_runningTail->m_next = job;
	if (m_runningHead == NULL)
		m_runningHead = job;
	m_runningTail = job;

	*outJob = job;
	fbMutexUnlock(&m_mutexWork);
	return 0;
}

/* 0x8134166A */
void NpSnsFbJobQueue::completeJob(NpSnsFbJobItem *job, int result)
{
	fbMutexLock(&m_mutexWork);
	job->unlinkFromQueue();
	if (job->m_state == NP_SNS_FB_JOB_RUNNING)
		job->m_state = NP_SNS_FB_JOB_FINISHED;
	else if (job->m_state == NP_SNS_FB_JOB_QUEUED)
		job->m_state = NP_SNS_FB_JOB_CANCELED_PENDING;

	job->m_next = NULL;
	job->m_prev = m_finishedTail;
	if (m_finishedTail != NULL)
		m_finishedTail->m_next = job;
	if (m_finishedHead == NULL)
		m_finishedHead = job;
	m_finishedTail = job;
	job->m_cancelError = result;
	fbMutexUnlock(&m_mutexWork);

	drainFinishedByState(NP_SNS_FB_JOB_FINISHED, true);
}

/* --- Common job submission helper --- */

static int enqueueFbServerJob(IPMI::Session *session, SceInt32 fbReqId, NpSnsFbJobItem *job)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	int ret = s_fbJobQueue->enqueue(fbReqId, job);
	if (ret < 0)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	s_fbClientManager->abortOtherProcesses(session, session->getProcessId());
	return ret;
}

/* --- BlockingIpc callbacks & handlers (0x8133F9D2..0x81340BFF) --- */

/* 0x81340B9A */
static int abortTrackedRequest(SceInt32 reqId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	int ret = s_fbJobQueue->cancel(reqId, NP_SNS_FB_ERR_ABORTED);
	if (ret == NP_SNS_FB_JOB_ERR_NOT_FOUND)
		return npSnsFbUtilAbortRequest(reqId);
	return ret;
}

/* 0x8133F9D2 */
static int onCreateRequest(IPMI::Session *session, const void *param, SceSize paramSize)
{
	(void)param;
	if (paramSize != 0)
		return NP_SNS_FB_BLOCKING_IPC_ERR_INVALID_SIZE;
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int reqId = npSnsFbUtilCreateRequest();
	if (reqId > 0) {
		if (s_fbClientManager->addRequestId(session, reqId) < 0) {
			npSnsFbUtilDestroyRequest(reqId);
			return NP_SNS_FB_ERR_INTERNAL_ERROR;
		}
	}
	return reqId;
}

/* 0x8133FA40 */
static int onDestroyRequest(IPMI::Session *session, int reqId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	if (s_fbClientManager->removeRequestId(session, reqId) < 0) {
		npSnsFbUtilDestroyRequest(reqId);
		return NP_SNS_FB_ERR_INTERNAL_ERROR;
	}
	return npSnsFbUtilDestroyRequest(reqId);
}

/* 0x8133FA9E */
static int onAbortRequest(IPMI::Session *session, int reqId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int ret = s_fbJobQueue->cancel(reqId, NP_SNS_FB_ERR_ABORTED);
	if (ret == NP_SNS_FB_JOB_ERR_NOT_FOUND)
		return npSnsFbUtilAbortRequest(reqId);
	return ret;
}

/* 0x8133FB0E */
static void onSessionCreated(IPMI::Session *session)
{
	if (s_fbClientManager != NULL)
		s_fbClientManager->addSession(session, session->getProcessId(), abortTrackedRequest);
}

/* 0x8133FB3A */
static void onSessionClosedByClient(IPMI::Session *session)
{
	if (s_fbClientManager != NULL)
		s_fbClientManager->removeSession(session);
}

/* 0x81340BE0 */
static void onSessionKilled(IPMI::Session *session)
{
	if (s_fbClientManager != NULL)
		s_fbClientManager->removeSession(session);
}

/* 0x8133FB50 */
static int handleInitSession(IPMI::Session *session, const NpSnsFbInitSessionIn *in)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	if (in == NULL || in->clientVersion == 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	s_fbClientManager->setClientVersion(session, in->clientVersion);
	return 0;
}

/* 0x8133FBAE */
static int handleTermSession(IPMI::Session *session)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	return s_fbClientManager->hasSession(session) ? 0 : NP_SNS_FB_ERR_INVALID_ARGUMENT;
}

/* 0x8133FBDE */
static int handleCheckAccountAsync(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbCheckAccountJob *job = new NpSnsFbCheckAccountJob(session, asyncReqId, fbReqId);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbCheckAccountReply reply;
	reply.result = err;
	reply.signedIn = 0;
	reply.chatAllowed = 0;
	replyCheckAccount(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x8133FCE2 */
static int handleAuthAndBindAsync(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				  const char *email, const char *password)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbAuthAndBindJob *job = new NpSnsFbAuthAndBindJob(session, asyncReqId, fbReqId,
							       email, password);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	SceInt32 status = err;
	replyAuthAndBind(session, asyncReqId, 0, &status);
	return 0;
}

/* 0x8133FDF0 */
static int handlePostFeedAsync(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
			       const char *message, const char *attachment,
			       const char *actionLinks, char *errMsgBuf, SceSize errMsgBufSize)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	const char *msgPtr = (message != NULL && message[0] != '\0') ? message : NULL;
	const char *attPtr = (attachment != NULL && attachment[0] != '\0') ? attachment : NULL;
	const char *actPtr = (actionLinks != NULL && actionLinks[0] != '\0') ? actionLinks : NULL;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbPostFeedJob *job = new NpSnsFbPostFeedJob(session, asyncReqId, fbReqId, msgPtr,
							 attPtr, actPtr, errMsgBuf, errMsgBufSize);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbFbErrorReply reply;
	reply.result = err;
	reply.fbError = 0;
	replyPostFeed(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x8133FF48 */
static int handleSharePurchaseAsync(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				    SceInt32 serviceType, const char *titleName,
				    const char *contentId, SceInt32 isRental, const char *imageUrl,
				    SceInt32 contentType, SceInt32 isPreOrder)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	const char *titlePtr = (titleName != NULL && titleName[0] != '\0') ? titleName : NULL;
	const char *cidPtr = (contentId != NULL && contentId[0] != '\0') ? contentId : NULL;
	const char *imgPtr = (imageUrl != NULL && imageUrl[0] != '\0') ? imageUrl : NULL;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbSharePurchaseJob *job = new NpSnsFbSharePurchaseJob(
		session, asyncReqId, fbReqId, serviceType, titlePtr, cidPtr, isRental, imgPtr,
		contentType, isPreOrder);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbFbErrorReply reply;
	reply.result = err;
	reply.fbError = 0;
	replySharePurchase(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x813400B6 */
static int handleShareRatingAsync(IPMI::Session *session, SceUInt32 asyncReqId, SceInt32 fbReqId,
				  SceInt32 serviceType, const char *titleName, const char *imageUrl,
				  SceUInt32 ratingScore)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	const char *titlePtr = (titleName != NULL && titleName[0] != '\0') ? titleName : NULL;
	const char *imgPtr = (imageUrl != NULL && imageUrl[0] != '\0') ? imageUrl : NULL;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbShareRatingJob *job = new NpSnsFbShareRatingJob(
		session, asyncReqId, fbReqId, serviceType, titlePtr, imgPtr, ratingScore);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbFbErrorReply reply;
	reply.result = err;
	reply.fbError = 0;
	replyShareRating(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x813401EC */
static int handleGetPermissionsInfoAsync(IPMI::Session *session, SceUInt32 asyncReqId,
					 SceInt32 fbReqId, SceInt32 langIndex,
					 const NpSnsFbTokenInputBuffer *tokenBuf,
					 const char *permList)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbGetPermissionsInfoJob *job = new NpSnsFbGetPermissionsInfoJob(
		session, asyncReqId, fbReqId, langIndex, tokenBuf, permList);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbPermissionsInfoReply reply;
	reply.result = err;
	replyGetPermissionsInfo(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x8134032C */
static int handleCheckPermissionsAsync(IPMI::Session *session, SceUInt32 asyncReqId,
				       SceInt32 fbReqId, const NpSnsFbTokenInputBuffer *tokenBuf,
				       const char *permList)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbCheckPermissionsJob *job = new NpSnsFbCheckPermissionsJob(
		session, asyncReqId, fbReqId, tokenBuf, permList);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbCheckPermissionsReply reply;
	reply.result = err;
	replyCheckPermissions(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x8134044A */
static int handleGetAccessTokenShortAsync(IPMI::Session *session, SceUInt32 asyncReqId,
					  SceInt32 fbReqId, const char *proxiedAppId,
					  const char *remoteAppId, const char *permissions)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbGetAccessTokenJob *job = new NpSnsFbGetAccessTokenJob(
		session, asyncReqId, fbReqId, proxiedAppId, remoteAppId, permissions, 0);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbShortTokenReply reply;
	reply.result = err;
	replyGetAccessTokenShort(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x81340562 */
static int handleGetAccessTokenWithPermCheckShortAsync(IPMI::Session *session, SceUInt32 asyncReqId,
						       SceInt32 fbReqId, const char *proxiedAppId,
						       const char *remoteAppId,
						       const char *permissions)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbGetAccessTokenWithPermCheckJob *job = new NpSnsFbGetAccessTokenWithPermCheckJob(
		session, asyncReqId, fbReqId, proxiedAppId, remoteAppId, permissions, 0);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	NpSnsFbShortTokenReply reply;
	reply.result = err;
	replyGetAccessTokenWithPermCheckShort(session, asyncReqId, 0, &reply);
	return 0;
}

/* 0x8134067A */
static int handleGetAccessTokenLongAsync(IPMI::Session *session, SceUInt32 asyncReqId,
					 SceInt32 fbReqId, const char *proxiedAppId,
					 const char *remoteAppId, const char *permissions)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbGetAccessTokenJob *job = new NpSnsFbGetAccessTokenJob(
		session, asyncReqId, fbReqId, proxiedAppId, remoteAppId, permissions, 1);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	SceInt32 status = err;
	NpSnsFbLongTokenData longData;
	replyGetAccessTokenLong(session, asyncReqId, 0, &status, &longData);
	return 0;
}

/* 0x813407B0 */
static int handleGetAccessTokenWithPermCheckLongAsync(IPMI::Session *session, SceUInt32 asyncReqId,
						      SceInt32 fbReqId, const char *proxiedAppId,
						      const char *remoteAppId,
						      const char *permissions)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbGetAccessTokenWithPermCheckJob *job = new NpSnsFbGetAccessTokenWithPermCheckJob(
		session, asyncReqId, fbReqId, proxiedAppId, remoteAppId, permissions, 1);
	if (job != NULL) {
		err = enqueueFbServerJob(session, fbReqId, job);
		if (err >= 0)
			return 0;
	}

	SceInt32 status = err;
	NpSnsFbLongTokenData longData;
	replyGetAccessTokenWithPermCheckLong(session, asyncReqId, 0, &status, &longData);
	return 0;
}

/* 0x813408E6 */
static int handleGetAccount(IPMI::Session *session, char *outAccount)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilGetAccountName(outAccount, NP_SNS_FB_ACCOUNT_MAX);
}

/* 0x81340920 */
static int handleGetSessionShort(IPMI::Session *session, NpSnsFbSessionShort *outSession)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilGetSessionShort(outSession);
}

/* 0x81340956 */
static int handleGetShareConfig(IPMI::Session *session, SceInt32 wantTrophy, SceInt32 *outTrophy,
				SceInt32 wantPurchase, SceInt32 *outPurchase, SceInt32 wantRating,
				SceInt32 *outRating, SceInt32 wantGameIntegrations,
				SceInt32 *outGameIntegrations)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	return npSnsFbUtilGetShareConfig(
		(wantTrophy == 1) ? outTrophy : NULL,
		(wantPurchase == 1) ? outPurchase : NULL,
		(wantRating == 1) ? outRating : NULL,
		(wantGameIntegrations == 1) ? outGameIntegrations : NULL);
}

/* 0x813409D2 */
static int handleSetShareConfig(IPMI::Session *session, SceInt32 hasTrophy, SceInt32 shareTrophy,
				SceInt32 hasPurchase, SceInt32 sharePurchase, SceInt32 hasRating,
				SceInt32 shareRating, SceInt32 hasGameIntegrations,
				SceInt32 gameIntegrations)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	return npSnsFbUtilSetShareConfig(
		(hasTrophy == 1) ? &shareTrophy : NULL,
		(hasPurchase == 1) ? &sharePurchase : NULL,
		(hasRating == 1) ? &shareRating : NULL,
		(hasGameIntegrations == 1) ? &gameIntegrations : NULL);
}

/* 0x81340A62 */
static int handleClearAll(IPMI::Session *session)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilClearAllConfig();
}

/* 0x81340A94 */
static int handleClearSession(IPMI::Session *session)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilClearSessionConfig();
}

/* 0x81340AC6 */
static int handleClearShare(IPMI::Session *session)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilClearShareConfig();
}

/* 0x81340AF8 */
static int handleIsShareTrophy(IPMI::Session *session, SceInt32 *outEnabled)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilIsShareTrophyEnabled(outEnabled);
}

/* 0x81340B2E */
static int handleIsSharePurchase(IPMI::Session *session, SceInt32 *outEnabled)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilIsSharePurchaseEnabled(outEnabled);
}

/* 0x81340B64 */
static int handleIsShareRating(IPMI::Session *session, SceInt32 *outEnabled)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	if (!s_fbClientManager->hasSession(session))
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	return npSnsFbUtilIsShareRatingEnabled(outEnabled);
}

/* 0x81341AC2 */
static int onSyncMethod(IPMI::Session *session, SceUInt32 methodId, const IPMI::DataInfo *inList,
			SceUInt32 inCount, IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int status = 0;

	switch (methodId) {
	case NP_SNS_FB_METHOD_INIT_SESSION:
		status = handleInitSession(
			session, static_cast<const NpSnsFbInitSessionIn *>(inList[0].pData));
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_TERM_SESSION:
		status = handleTermSession(session);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_GET_ACCOUNT:
		status = handleGetAccount(session, static_cast<char *>(outList[0].pBuffer));
		outList[0].outSize = NP_SNS_FB_ACCOUNT_MAX;
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_GET_SESSION_SHORT:
		status = handleGetSessionShort(
			session, static_cast<NpSnsFbSessionShort *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(NpSnsFbSessionShort);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_GET_SHARE_CONFIG: {
		const NpSnsFbGetShareConfigIn *cfgIn =
			static_cast<const NpSnsFbGetShareConfigIn *>(inList[0].pData);
		status = handleGetShareConfig(
			session,
			cfgIn->wantTrophy, static_cast<SceInt32 *>(outList[0].pBuffer),
			cfgIn->wantPurchase, static_cast<SceInt32 *>(outList[1].pBuffer),
			cfgIn->wantRating, static_cast<SceInt32 *>(outList[2].pBuffer),
			cfgIn->wantGameIntegrations, static_cast<SceInt32 *>(outList[3].pBuffer));
		outList[0].outSize = sizeof(SceInt32);
		outList[1].outSize = sizeof(SceInt32);
		outList[2].outSize = sizeof(SceInt32);
		outList[3].outSize = sizeof(SceInt32);
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}

	case NP_SNS_FB_METHOD_SET_SHARE_CONFIG: {
		const NpSnsFbSetShareConfigIn *cfgIn =
			static_cast<const NpSnsFbSetShareConfigIn *>(inList[0].pData);
		status = handleSetShareConfig(session, cfgIn->hasTrophy, cfgIn->shareTrophy,
					      cfgIn->hasPurchase, cfgIn->sharePurchase,
					      cfgIn->hasRating, cfgIn->shareRating,
					      cfgIn->hasGameIntegrations, cfgIn->gameIntegrations);
		return session->respondToSyncMethodRequest(status, outList, outCount);
	}

	case NP_SNS_FB_METHOD_CLEAR_ALL:
		status = handleClearAll(session);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_CLEAR_SESSION:
		status = handleClearSession(session);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_CLEAR_SHARE:
		status = handleClearShare(session);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_IS_SHARE_TROPHY:
		status = handleIsShareTrophy(session, static_cast<SceInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceInt32);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_IS_SHARE_PURCHASE:
		status = handleIsSharePurchase(session, static_cast<SceInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceInt32);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	case NP_SNS_FB_METHOD_IS_SHARE_RATING:
		status = handleIsShareRating(session, static_cast<SceInt32 *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceInt32);
		return session->respondToSyncMethodRequest(status, outList, outCount);

	default:
		return NP_SNS_FB_IPMI_ERR_UNSUPPORTED;
	}
}

/* 0x81341CAA */
static int onAsyncMethod(IPMI::Session *session, SceUInt32 asyncReqId, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_SNS_FB_METHOD_CHECK_ACCOUNT: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleCheckAccountAsync(session, asyncReqId, hdr->reqId);
	}

	case NP_SNS_FB_METHOD_AUTH_AND_BIND: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleAuthAndBindAsync(session, asyncReqId, hdr->reqId,
					      static_cast<const char *>(inList[1].pData),
					      static_cast<const char *>(inList[2].pData));
	}

	case NP_SNS_FB_METHOD_POST_FEED: {
		const NpSnsFbPostFeedHeader *hdr =
			static_cast<const NpSnsFbPostFeedHeader *>(inList[0].pData);
		return handlePostFeedAsync(session, asyncReqId, hdr->reqId,
					   static_cast<const char *>(inList[1].pData),
					   static_cast<const char *>(inList[2].pData),
					   static_cast<const char *>(inList[3].pData),
					   hdr->errMsgBuf, hdr->errMsgBufSize);
	}

	case NP_SNS_FB_METHOD_SHARE_PURCHASE: {
		const NpSnsFbSharePurchaseHeader *hdr =
			static_cast<const NpSnsFbSharePurchaseHeader *>(inList[0].pData);
		return handleSharePurchaseAsync(session, asyncReqId, hdr->reqId, hdr->serviceType,
						static_cast<const char *>(inList[1].pData),
						static_cast<const char *>(inList[2].pData),
						hdr->isRental,
						static_cast<const char *>(inList[3].pData),
						hdr->contentType, hdr->isPreOrder);
	}

	case NP_SNS_FB_METHOD_SHARE_RATING: {
		const NpSnsFbShareRatingHeader *hdr =
			static_cast<const NpSnsFbShareRatingHeader *>(inList[0].pData);
		return handleShareRatingAsync(session, asyncReqId, hdr->reqId, hdr->serviceType,
					      static_cast<const char *>(inList[1].pData),
					      static_cast<const char *>(inList[2].pData),
					      hdr->ratingScore);
	}

	case NP_SNS_FB_METHOD_GET_PERMISSIONS_INFO: {
		const NpSnsFbGetPermsHeader *hdr =
			static_cast<const NpSnsFbGetPermsHeader *>(inList[0].pData);
		return handleGetPermissionsInfoAsync(
			session, asyncReqId, hdr->reqId, hdr->langIndex,
			static_cast<const NpSnsFbTokenInputBuffer *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData));
	}

	case NP_SNS_FB_METHOD_CHECK_PERMISSIONS: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleCheckPermissionsAsync(
			session, asyncReqId, hdr->reqId,
			static_cast<const NpSnsFbTokenInputBuffer *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData));
	}

	case NP_SNS_FB_METHOD_GET_ACCESS_TOKEN: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleGetAccessTokenShortAsync(session, asyncReqId, hdr->reqId,
						      static_cast<const char *>(inList[1].pData),
						      static_cast<const char *>(inList[2].pData),
						      static_cast<const char *>(inList[3].pData));
	}

	case NP_SNS_FB_METHOD_GET_ACCESS_TOKEN_CHECK: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleGetAccessTokenWithPermCheckShortAsync(
			session, asyncReqId, hdr->reqId,
			static_cast<const char *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData),
			static_cast<const char *>(inList[3].pData));
	}

	case NP_SNS_FB_METHOD_GET_LONG_TOKEN: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleGetAccessTokenLongAsync(session, asyncReqId, hdr->reqId,
						     static_cast<const char *>(inList[1].pData),
						     static_cast<const char *>(inList[2].pData),
						     static_cast<const char *>(inList[3].pData));
	}

	case NP_SNS_FB_METHOD_GET_LONG_TOKEN_CHECK: {
		const NpSnsFbAsyncReqHeader *hdr =
			static_cast<const NpSnsFbAsyncReqHeader *>(inList[0].pData);
		return handleGetAccessTokenWithPermCheckLongAsync(
			session, asyncReqId, hdr->reqId,
			static_cast<const char *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData),
			static_cast<const char *>(inList[3].pData));
	}

	default:
		return NP_SNS_FB_IPMI_ERR_UNSUPPORTED;
	}
}

/* --- Public Server Lifecycle (0x8133F6B0, 0x8133F992, 0x8133F9B2) --- */

/* 0x8133F6B0 */
int npSnsFbServerStart(void)
{
	BlockingIpc::Server::Config cfg;
	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x10000u;
	cfg.maxSessions = 16;
	cfg.syncCallCount = 1;
	cfg.maxSyncCallInSize = 3840;
	cfg.maxSyncCallOutSize = 3840;
	cfg.asyncCallCount = 0;
	cfg.maxAsyncCallInSize = 3840;
	cfg.maxAsyncCallOutSize = 3840;
	cfg.blockingIpcCount = 1;
	cfg.maxBlockingInSize = 3840;
	cfg.maxBlockingOutSize = 3840;
	cfg.eventFlagCount = 0;
	cfg.msgQueueCount = 1;
	for (int idx = 0; idx < 32; idx++) {
		cfg.msgQueue[idx].msgSize = 256;
		cfg.msgQueue[idx].msgCount = 8;
	}

	if (s_fbIpcServer != NULL)
		return NP_SNS_FB_ERR_ALREADY_INITIALIZED;

	if (!npSnsFbUtilIsInitialized())
		return NP_SNS_FB_ERR_NOT_INITIALIZED;

	int err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
	NpSnsFbJobQueue *jobQueue = NULL;

	s_fbServerMutex = new NpSnsFbLwMutex("SceNpSnsFbServerMutex");
	if (s_fbServerMutex != NULL) {
		s_fbClientManager = new NpSnsFbClientManager();
		if (s_fbClientManager != NULL) {
			err = s_fbClientManager->init(s_fbServerMutex);
			if (err >= 0) {
				jobQueue = new NpSnsFbJobQueue();
				if (jobQueue != NULL) {
					err = jobQueue->init("SceNpSnsFbServerJob", 0x10000100,
							     0x4000u);
					if (err >= 0)
						err = jobQueue->start();
				} else {
					err = NP_SNS_FB_ERR_OUT_OF_MEMORY;
				}
			}
		}
	}

	if (err >= 0 && jobQueue != NULL) {
		sceClibStrlcpy(cfg.name, "SceNpSnsFb", sizeof(cfg.name));
		cfg.threadStackSize = 0x4000u;
		cfg.maxSessions = 8;
		cfg.syncCallCount = 4;
		cfg.maxSyncCallInSize = 0x2000u;
		cfg.maxSyncCallOutSize = 0x2000u;
		cfg.asyncCallCount = 4;
		cfg.maxAsyncCallInSize = 0x2000u;
		cfg.maxAsyncCallOutSize = 0x2000u;
		cfg.blockingIpcCount = 4;
		cfg.maxBlockingInSize = 0x2000u;
		cfg.maxBlockingOutSize = 0x2000u;
		cfg.msgQueueCount = 1;
		cfg.msgQueue[0].msgSize = 256;
		cfg.msgQueue[0].msgCount = 2;
		cfg.callbacks.createRequest = onCreateRequest;
		cfg.callbacks.destroyRequest = onDestroyRequest;
		cfg.callbacks.abortRequest = onAbortRequest;
		cfg.callbacks.onSessionCreated = onSessionCreated;
		cfg.callbacks.onSessionClosedByClient = onSessionClosedByClient;
		cfg.callbacks.onSyncMethod = onSyncMethod;
		cfg.callbacks.onAsyncMethod = onAsyncMethod;
		cfg.callbacks.onSessionKilled = onSessionKilled;

		SceSize bufSize = BlockingIpc::Server::getBufferSize(&cfg);
		void *serverMem = npSnsFbMemAlloc(bufSize);
		if (serverMem == NULL)
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;

		int createRet = BlockingIpc::Server::create(&s_fbIpcServer, &cfg, serverMem);
		if (createRet != 0) {
			npSnsFbMemFree(serverMem);
			return createRet;
		}

		int startRet = s_fbIpcServer->start();
		s_fbJobQueue = jobQueue;
		return startRet;
	}

	if (jobQueue != NULL) {
		delete jobQueue;
		s_fbJobQueue = NULL;
	}
	if (s_fbIpcServer != NULL) {
		s_fbIpcServer->destroy();
		npSnsFbMemFree(NULL);
		s_fbIpcServer = NULL;
	}
	if (s_fbClientManager != NULL) {
		delete s_fbClientManager;
		s_fbClientManager = NULL;
	}
	if (s_fbServerMutex != NULL) {
		delete s_fbServerMutex;
		s_fbServerMutex = NULL;
	}
	return err;
}

/* 0x8133F992 */
int npSnsFbServerSuspend(SceUID processId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	return s_fbClientManager->suspendProcess(processId);
}

/* 0x8133F9B2 */
int npSnsFbServerResume(SceUID processId)
{
	if (s_fbClientManager == NULL)
		return NP_SNS_FB_ERR_NOT_INITIALIZED;
	return s_fbClientManager->resumeProcess(processId);
}
