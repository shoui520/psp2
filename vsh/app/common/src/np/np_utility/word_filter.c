/*
 * SceNpUtility: the word filter (sceNpWordFilter*), the lookup library's censor service. A
 * comment is checked or sanitized by the community server's censor_message; the asynchronous
 * requests run on the worker thread this file starts.
 */

#include "np_utility_internal.h"

static int s_initialized;
static int s_async;
static void *s_worker;
static SceUID s_waitEventFlag = -1;

/* the asynchronous requests, as the worker runs them */
typedef struct CensorJob {
	int reqId;
	NpLookupTrans *trans;
	const char *comment;		/* a copy the transaction owns */
	void *option;
} CensorJob;

typedef struct SanitizeJob {
	int reqId;
	NpLookupTrans *trans;
	const char *comment;		/* a copy the transaction owns */
	char *sanitizedComment;
	void *option;
} SanitizeJob;

static __attribute__((noinline)) int censorComment(int reqId, const char *comment, void *option);
static __attribute__((noinline)) int sanitizeComment(int reqId, const char *comment, char *sanitizedComment, void *option);
static __attribute__((noinline)) int censorCommentJob(SceUID thread, int arg, void *job, int flags);
static __attribute__((noinline)) int sanitizeCommentJob(SceUID thread, int arg, void *job,
						     int flags);
static __attribute__((noinline)) int censorCommentDone(SceUID thread, int arg, void *job,
						     int flags, int result);
static __attribute__((noinline)) int sanitizeCommentDone(SceUID thread, int arg, void *job,
						     int flags, int result);

static const NpLookupJobHandler s_jobHandlers[] = {
	{ 0, censorCommentJob, censorCommentDone },
	{ 1, sanitizeCommentJob, sanitizeCommentDone },
};

static __attribute__((noinline)) int wordFilterInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
			  void *option);
static __attribute__((noinline)) int wordFilterTerm(void);
static __attribute__((noinline)) int isInitialized(void);
static __attribute__((noinline)) int wordFilterCreateTitleCtx(const SceNpCommunicationId *titleId,
				    const SceNpId *selfNpId);
static __attribute__((noinline)) int wordFilterDeleteTitleCtx(SceInt32 titleCtxId);
static __attribute__((noinline)) int wordFilterCreateRequest(SceInt32 titleCtxId);
static __attribute__((noinline)) int wordFilterDeleteRequest(SceInt32 reqId);
static __attribute__((noinline)) int wordFilterWaitAsync(SceInt32 reqId, SceInt32 *result);
static __attribute__((noinline)) int wordFilterPollAsync(SceInt32 reqId, SceInt32 *result);
static __attribute__((noinline)) int censorCommentAsync(int reqId, const char *comment, void *option);
static __attribute__((noinline)) int sanitizeCommentAsync(int reqId, const char *comment, char *sanitizedComment,
				void *option);

int sceNpWordFilterInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
			void *option)
{
	return wordFilterInit(usesAsync, threadPriority, cpuAffinityMask, option);
}

int sceNpWordFilterTerm(void)
{
	return wordFilterTerm();
}

int sceNpWordFilterCreateTitleCtx(const SceNpCommunicationId *titleId, const SceNpId *selfNpId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterCreateTitleCtx(titleId, selfNpId);
}

int sceNpWordFilterDeleteTitleCtx(SceInt32 titleCtxId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterDeleteTitleCtx(titleCtxId);
}

int sceNpWordFilterCreateRequest(SceInt32 titleCtxId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterCreateRequest(titleCtxId);
}

int sceNpWordFilterDeleteRequest(SceInt32 reqId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterDeleteRequest(reqId);
}

int sceNpWordFilterAbortRequest(SceInt32 reqId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_944FE83C(reqId);
}

int sceNpWordFilterSetTimeout(SceInt32 id, SceInt32 resolveRetry, SceUInt32 resolveTimeout,
			      SceUInt32 connTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_A0991531(id, resolveRetry, resolveTimeout, connTimeout, sendTimeout,
				     recvTimeout);
}

int sceNpWordFilterWaitAsync(SceInt32 reqId, SceInt32 *result)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterWaitAsync(reqId, result);
}

int sceNpWordFilterPollAsync(SceInt32 reqId, SceInt32 *result)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return wordFilterPollAsync(reqId, result);
}

SceInt32 sceNpWordFilterCensorComment(SceInt32 reqId, const char *comment, void *option)
{
	int ret;

	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = censorComment(reqId, comment, option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

SceInt32 sceNpWordFilterCensorCommentAsync(SceInt32 reqId, const char *comment, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return censorCommentAsync(reqId, comment, option);
}

SceInt32 sceNpWordFilterSanitizeComment(SceInt32 reqId, const char *comment,
					char *sanitizedComment, void *option)
{
	int ret;

	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = sanitizeComment(reqId, comment, sanitizedComment, option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

SceInt32 sceNpWordFilterSanitizeCommentAsync(SceInt32 reqId, const char *comment,
					     char *sanitizedComment, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return sanitizeCommentAsync(reqId, comment, sanitizedComment, option);
}

/* the worker's allocator: the censor service's heap */
static void *workerMalloc(SceSize size)
{
	return SceNpUtility_8CA24D19(size, NP_LOOKUP_SERVICE_CENSOR);
}

static void workerFree(void *ptr)
{
	SceNpUtility_6C1161AD(ptr, NP_LOOKUP_SERVICE_CENSOR);
}

static int wordFilterInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
			  void *option)
{
	int clientsUp = 0;
	int ret;

	if (s_initialized != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;
	if (option != NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;

	ret = SceNpUtility_74B0D0AC(NP_LOOKUP_SERVICE_CENSOR);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_E38548AB(NP_LOOKUP_SERVICE_CENSOR);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_9F142A92(NP_LOOKUP_SERVICE_CENSOR);
	if (ret < 0)
		goto titles;
	ret = SceNpUtility_17385829(NP_LOOKUP_SERVICE_CENSOR);
	if (ret < 0)
		goto transactions;
	s_async = 0;
	clientsUp = 1;

	if (usesAsync != 0) {
		s_worker = SceNpUtility_8CA24D19(40, NP_LOOKUP_SERVICE_CENSOR);
		if (s_worker == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			s_waitEventFlag = sceKernelCreateEventFlag("SceNpCensorForWaitAsync", 0, 0,
								   NULL);
			if (s_waitEventFlag < 0) {
				ret = s_waitEventFlag;
			} else {
				ret = SceNpUtility_6EB4C8FE(s_worker, threadPriority, 0x4000,
							    cpuAffinityMask, 32, workerMalloc,
							    workerFree);
				if (ret < 0) {
					ret = npLookupWorkerError(ret);
				} else {
					ret = SceNpUtility_575BBBC4(s_worker, 2, s_jobHandlers);
					if (ret < 0) {
						ret = npLookupWorkerError(ret);
					} else {
						ret = SceNpUtility_F73C4411(s_worker);
						if (ret < 0)
							ret = npLookupWorkerError(ret);
					}
				}
			}
		}
		if (ret < 0)
			goto transactions;
		s_async = 1;
	}
	s_initialized = 1;
	return 0;

transactions:
	SceNpUtility_6DF884DA(NP_LOOKUP_SERVICE_CENSOR);
titles:
	SceNpUtility_26B4AC00(NP_LOOKUP_SERVICE_CENSOR);
	if (clientsUp == 1)
		SceNpUtility_7A86C37E(NP_LOOKUP_SERVICE_CENSOR);
	return ret;
}

static int wordFilterTerm(void)
{
	int ret;

	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	ret = SceNpUtility_200BBAC5(NP_LOOKUP_SERVICE_CENSOR);
	if (s_async != 0) {
		ret = SceNpUtility_5631E596(s_worker);
		if (s_worker != NULL) {
			SceNpUtility_6C1161AD(s_worker, NP_LOOKUP_SERVICE_CENSOR);
			s_worker = NULL;
		}
		if (s_waitEventFlag != -1) {
			sceKernelDeleteEventFlag(s_waitEventFlag);
			s_waitEventFlag = -1;
		}
	}
	s_async = 0;
	SceNpUtility_6DF884DA(NP_LOOKUP_SERVICE_CENSOR);
	SceNpUtility_26B4AC00(NP_LOOKUP_SERVICE_CENSOR);
	SceNpUtility_7A86C37E(NP_LOOKUP_SERVICE_CENSOR);
	SceNpUtility_418E1324(NP_LOOKUP_SERVICE_CENSOR);
	s_initialized = 0;
	return ret;
}

static int isInitialized(void)
{
	return s_initialized == 1;
}

/* tells NpManager the word filter uses commId */
static inline void notifyTitle(const SceNpCommunicationId *commId)
{
	char commIdText[16];

	if (commId == NULL)
		return;
	sceClibMemset(commIdText, 0, 13);
	if (SceNpCommon_E426FEE8(commId, commIdText, 13) >= 0) {
		char name[32];

		sceClibMemset(name, 0, sizeof(name));
		sceClibStrlcpy(name, "NpWordFilter", sizeof(name));
		SceNpManager_9554802B("NpWordFilter", commIdText, name);
	}
}

static int wordFilterCreateTitleCtx(const SceNpCommunicationId *titleId,
				    const SceNpId *selfNpId)
{
	const SceNpCommunicationId *commId = titleId;
	SceNpCommunicationConfig commConf;
	int clientId;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (selfNpId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (commId == NULL) {
		ret = SceNpManager_96DF1B76(&commConf);
		if (ret < 0)
			goto out;
		commId = commConf.commId;
		if (commId == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
			goto out;
		}
	}

	ret = SceNpUtility_F4B21646(&selfNpId->handle, selfNpId, NP_LOOKUP_SERVICE_CENSOR);
	if (ret == SCE_NP_COMMUNITY_ERROR_CLIENT_HANDLE_ALREADY_EXISTS)
		ret = 0;
	else if (ret < 0)
		goto out;
	clientId = ret;

	ret = SceNpUtility_44A2F444(commId, selfNpId, clientId, NP_LOOKUP_SERVICE_CENSOR);
	if (ret >= 0)
		notifyTitle(commId);
	else
		SceNpUtility_ED2B86ED(clientId, NP_LOOKUP_SERVICE_CENSOR);
out:
	return ret;
}

static int wordFilterDeleteTitleCtx(SceInt32 titleCtxId)
{
	NpLookupTitle *title;
	int ret;

	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	title = NULL;
	ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_CENSOR);
	if (ret >= 0) {
		SceNpUtility_CD3A3712(title->client);
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_CENSOR);
		title = NULL;
		ret = SceNpUtility_4013244C(titleCtxId, NP_LOOKUP_SERVICE_CENSOR);
	}
	return ret;
}

static int wordFilterCreateRequest(SceInt32 titleCtxId)
{
	NpLookupTitle *title = NULL;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	} else {
		ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_CENSOR);
		if (ret >= 0) {
			if (title == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				ret = SceNpUtility_23414285(title, 0, &title->npId, title->id,
							    NP_LOOKUP_SERVICE_CENSOR);
				if (ret >= 0) {
					SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_CENSOR);
					return ret;
				}
			}
		}
	}
	if (title != NULL)
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_CENSOR);
	return ret;
}

static int wordFilterDeleteRequest(SceInt32 reqId)
{
	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_B4423DAF(reqId, NP_LOOKUP_SERVICE_CENSOR);
}

static int wordFilterWaitAsync(SceInt32 reqId, SceInt32 *result)
{
	SceUInt32 timeout;
	SceUInt32 outBits;
	int ret;

	if (result == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (s_async != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	for (;;) {
		ret = SceNpUtility_8904FE15(reqId, result);
		if (ret < 0)
			break;
		if (ret == 0)
			break;
		outBits = 0;
		timeout = 10000000;
		ret = sceKernelWaitEventFlag(s_waitEventFlag, 1,
					     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					     &outBits, &timeout);
		if (ret < 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return SCE_NP_COMMUNITY_ERROR_NO_RESOURCE;
	}
	return ret;
}

static int wordFilterPollAsync(SceInt32 reqId, SceInt32 *result)
{
	if (result == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (s_async != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_8904FE15(reqId, result);
}

static int censorComment(int reqId, const char *comment, void *option)
{
	NpLookupTrans *trans;
	int ret;

	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (comment == NULL)
		return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	if (option != NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;

	trans = NULL;
	if ((int)sceClibStrnlen(comment, SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN + 1) >
	    SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceNpUtility_25D26B6F(reqId, &trans);
		if (ret >= 0) {
			if (sceClibStrnlen(trans->title->commId.data, 0x7FFFFFFF) == 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
			} else if (trans->operation != 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			} else {
				trans->operation = 6;
				trans->data = NULL;
				ret = SceNpUtility_C9283605(reqId, &trans->title->npId, comment, NULL,
							    0);
				if (ret < 0 && ret != SCE_NP_COMMUNITY_SERVER_ERROR_CENSORED)
					goto error;
				if (ret == SCE_NP_COMMUNITY_SERVER_ERROR_CENSORED)
					goto error;
				SceNpUtility_31D3542E(trans);
				return 0;
			}
		}
	}
error:
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}

static int censorCommentAsync(int reqId, const char *comment, void *option)
{
	NpLookupTrans *trans = NULL;
	CensorJob *job;
	char *copy;
	SceSize length;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (s_async != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (comment == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	length = sceClibStrnlen(comment, SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN);
	if (sceClibStrnlen(comment, SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN + 1) >
	    SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto release;
	}
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto release;
	if (sceClibStrnlen(trans->title->commId.data, 0x7FFFFFFF) == 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto release;
	}
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto release;
	}
	trans->data = NULL;

	job = SceNpUtility_8CA24D19(sizeof(CensorJob), trans->service);
	if (job == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto release;
	}
	job->reqId = reqId;
	job->option = option;

	copy = SceNpUtility_8CA24D19(length + 1, trans->service);
	if (copy == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(copy, 0, length + 1);
		sceClibMemcpy_safe(copy, comment, length);
		job->comment = copy;
		ret = SceNpUtility_46543307(trans, copy);
		if (ret >= 0) {
			ret = SceNpUtility_46543307(trans, job);
			if (ret >= 0) {
				job->trans = trans;
				trans->state = NP_LOOKUP_TRANS_QUEUED;
				ret = SceNpUtility_44FE1097(s_worker, 0, 0, job, 0);
				if (ret >= 0)
					return ret;
				trans->state = NP_LOOKUP_TRANS_IDLE;
				ret = npLookupWorkerError(ret);
				goto release;
			}
			goto free_job;
		}
	}
	if (copy != NULL && trans != NULL)
		SceNpUtility_6C1161AD(copy, trans->service);
free_job:
	if (trans == NULL)
		goto out;
	SceNpUtility_6C1161AD(job, trans->service);
release:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
out:
	if (ret != SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED)
		SceNpUtility_F5086697(reqId, ret);
	return ret;
}

static int sanitizeComment(int reqId, const char *comment, char *sanitizedComment, void *option)
{
	NpLookupTrans *trans;
	int ret;

	if (comment == NULL || sanitizedComment == NULL)
		return SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
	if (option != NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;

	trans = NULL;
	if ((int)sceClibStrnlen(comment, SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN + 1) >
	    SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceNpUtility_25D26B6F(reqId, &trans);
		if (ret >= 0) {
			if (sceClibStrnlen(trans->title->commId.data, 0x7FFFFFFF) == 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
			} else if (trans->operation != 0) {
				ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
			} else {
				trans->operation = 7;
				trans->data = NULL;
				ret = SceNpUtility_C9283605(reqId, &trans->title->npId, comment,
							    sanitizedComment, 1);
				if (ret >= 0) {
					SceNpUtility_31D3542E(trans);
					return 0;
				}
			}
		}
	}
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}

static int sanitizeCommentAsync(int reqId, const char *comment, char *sanitizedComment,
				void *option)
{
	NpLookupTrans *trans = NULL;
	SanitizeJob *job;
	char *copy;
	SceSize length;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (s_async != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (comment == NULL || sanitizedComment == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	length = sceClibStrnlen(comment, SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN);
	if (sceClibStrnlen(comment, SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN + 1) >
	    SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto release;
	}
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto release;
	if (sceClibStrnlen(trans->title->commId.data, 0x7FFFFFFF) == 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto release;
	}
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto release;
	}
	trans->data = NULL;

	job = SceNpUtility_8CA24D19(sizeof(SanitizeJob), trans->service);
	if (job == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto release;
	}
	job->reqId = reqId;
	job->sanitizedComment = sanitizedComment;
	job->option = NULL;

	copy = SceNpUtility_8CA24D19(length + 1, trans->service);
	if (copy == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(copy, 0, length + 1);
		sceClibMemcpy_safe(copy, comment, length);
		job->comment = copy;
		ret = SceNpUtility_46543307(trans, copy);
		if (ret >= 0) {
			ret = SceNpUtility_46543307(trans, job);
			if (ret >= 0) {
				job->trans = trans;
				trans->state = NP_LOOKUP_TRANS_QUEUED;
				ret = SceNpUtility_44FE1097(s_worker, 1, 0, job, 0);
				if (ret >= 0)
					return ret;
				trans->state = NP_LOOKUP_TRANS_IDLE;
				ret = npLookupWorkerError(ret);
				goto release;
			}
			goto free_job;
		}
	}
	if (copy != NULL && trans != NULL)
		SceNpUtility_6C1161AD(copy, trans->service);
free_job:
	if (trans == NULL)
		goto out;
	SceNpUtility_6C1161AD(job, trans->service);
release:
	if (trans != NULL) {
		if (trans->aborted == 1)
			ret = SCE_NP_COMMUNITY_ERROR_ABORTED;
		SceNpUtility_31D3542E(trans);
	}
out:
	if (ret != SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED)
		SceNpUtility_F5086697(reqId, ret);
	return ret;
}

static int censorCommentJob(SceUID thread, int arg, void *job, int flags)
{
	CensorJob *request = job;
	int ret;

	if (request == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	ret = censorComment(request->reqId, request->comment, request->option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

static int sanitizeCommentJob(SceUID thread, int arg, void *job, int flags)
{
	SanitizeJob *request = job;
	int ret;

	if (request == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	ret = sanitizeComment(request->reqId, request->comment, request->sanitizedComment,
			      request->option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

static int censorCommentDone(SceUID thread, int arg, void *job, int flags, int result)
{
	CensorJob *request = job;
	int ret = 0;

	if (request != NULL) {
		ret = SceNpUtility_F5086697(request->reqId, result);
		SceNpUtility_31D3542E(request->trans);
	}
	if (s_waitEventFlag != -1)
		return sceKernelSetEventFlag(s_waitEventFlag, 1);
	return ret;
}

static int sanitizeCommentDone(SceUID thread, int arg, void *job, int flags, int result)
{
	SanitizeJob *request = job;
	int ret = 0;

	if (request != NULL) {
		ret = SceNpUtility_F5086697(request->reqId, result);
		SceNpUtility_31D3542E(request->trans);
	}
	if (s_waitEventFlag != -1)
		return sceKernelSetEventFlag(s_waitEventFlag, 1);
	return ret;
}
