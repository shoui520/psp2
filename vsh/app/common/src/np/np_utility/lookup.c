/*
 * SceNpUtility: the profile lookup (sceNpLookup*), the lookup library's lookup service: an
 * online ID's NP ID, a user's profile and avatar image from the community server. The
 * asynchronous requests run on the worker thread this file starts.
 */

#include "np_utility_internal.h"

static int s_initialized;
static int s_async;
static void *s_worker;
static SceUID s_waitEventFlag = -1;

/* the asynchronous requests, as the worker runs them */
typedef struct NpIdJob {
	int reqId;
	NpLookupTrans *trans;
	const SceNpOnlineId *onlineId;		/* a copy the transaction owns */
	SceNpId *npId;
	void *option;
} NpIdJob;

typedef struct UserProfileJob {
	int reqId;
	NpLookupTrans *trans;
	SceInt32 avatarSizeType;
	const SceNpId *npId;			/* a copy the transaction owns */
	SceNpUserInformation *userInfo;
	SceNpAboutMe *aboutMe;
	SceNpMyLanguages *languages;
	SceNpCountryCode *countryCode;
	void *avatarImageData;
	SceSize avatarImageDataMaxSize;
	SceSize *avatarImageDataSize;
	int avatarN;
	void *option;
} UserProfileJob;

typedef struct AvatarImageJob {
	int reqId;
	NpLookupTrans *trans;
	const SceNpAvatarUrl *avatarUrl;	/* a copy the transaction owns */
	SceNpAvatarImage *avatarImage;
	void *option;
} AvatarImageJob;

static __attribute__((noinline)) int npIdJob(void *worker, int type, void *job);
static __attribute__((noinline)) int userProfileJob(void *worker, int type, void *job);
static __attribute__((noinline)) int avatarImageJob(void *worker, int type, void *job);
static __attribute__((noinline)) int npIdDone(void *worker, int type, void *job, int arg, int result);
static __attribute__((noinline)) int userProfileDone(void *worker, int type, void *job, int arg,
						     int result);
static __attribute__((noinline)) int avatarImageDone(void *worker, int type, void *job, int arg,
						     int result);

static const NpLookupJobHandler s_jobHandlers[] = {
	{ 0, npIdJob, npIdDone },
	{ 1, userProfileJob, userProfileDone },
	{ 2, avatarImageJob, avatarImageDone },
};

static __attribute__((noinline)) int lookupInit(SceBool usesAsync, SceInt32 threadPriority,
						SceInt32 cpuAffinityMask, void *option);
static __attribute__((noinline)) int lookupTerm(void);
static __attribute__((noinline)) int isInitialized(void);
static __attribute__((noinline)) int lookupCreateTitleCtx(const SceNpCommunicationId *titleId,
							  const SceNpId *selfNpId);
static __attribute__((noinline)) int lookupDeleteTitleCtx(SceInt32 titleCtxId);
static __attribute__((noinline)) int lookupCreateRequest(SceInt32 titleCtxId);
static __attribute__((noinline)) int lookupDeleteRequest(SceInt32 reqId);
static __attribute__((noinline)) int lookupWaitAsync(SceInt32 reqId, SceInt32 *result);
static __attribute__((noinline)) int lookupPollAsync(SceInt32 reqId, SceInt32 *result);
static __attribute__((noinline)) int npIdAsync(SceInt32 reqId, const SceNpOnlineId *onlineId,
					       SceNpId *npId, void *option);
static __attribute__((noinline)) int userProfileAsync(SceInt32 reqId, SceInt32 avatarSizeType,
						      const SceNpId *npId,
						      SceNpUserInformation *userInfo,
						      SceNpAboutMe *aboutMe,
						      SceNpMyLanguages *languages,
						      SceNpCountryCode *countryCode,
						      void *avatarImageData,
						      SceSize avatarImageDataMaxSize,
						      SceSize *avatarImageDataSize, int avatarN,
						      void *option);
static __attribute__((noinline)) int avatarImageAsync(SceInt32 reqId,
						      const SceNpAvatarUrl *avatarUrl,
						      SceNpAvatarImage *avatarImage, void *option);

int sceNpLookupInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
		    void *option)
{
	return lookupInit(usesAsync, threadPriority, cpuAffinityMask, option);
}

int sceNpLookupTerm(void)
{
	return lookupTerm();
}

int sceNpLookupCreateTitleCtx(const SceNpCommunicationId *titleId, const SceNpId *selfNpId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupCreateTitleCtx(titleId, selfNpId);
}

int sceNpLookupDeleteTitleCtx(SceInt32 titleCtxId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupDeleteTitleCtx(titleCtxId);
}

int sceNpLookupCreateRequest(SceInt32 titleCtxId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupCreateRequest(titleCtxId);
}

int sceNpLookupDeleteRequest(SceInt32 reqId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupDeleteRequest(reqId);
}

int sceNpLookupAbortRequest(SceInt32 reqId)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_944FE83C(reqId);
}

int sceNpLookupSetTimeout(SceInt32 id, SceInt32 resolveRetry, SceUInt32 resolveTimeout,
			  SceUInt32 connTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_A0991531(id, resolveRetry, resolveTimeout, connTimeout, sendTimeout,
				     recvTimeout);
}

int sceNpLookupWaitAsync(SceInt32 reqId, SceInt32 *result)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupWaitAsync(reqId, result);
}

int sceNpLookupPollAsync(SceInt32 reqId, SceInt32 *result)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return lookupPollAsync(reqId, result);
}

int sceNpLookupNpId(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId, void *option)
{
	int ret;

	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = SceNpUtility_23B387A5(reqId, onlineId, npId, option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

int sceNpLookupNpIdAsync(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
			 void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return npIdAsync(reqId, onlineId, npId, option);
}

int sceNpLookupUserProfile(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			   SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			   SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			   void *avatarImageData, SceSize avatarImageDataMaxSize,
			   SceSize *avatarImageDataSize, void *option)
{
	int ret;

	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = SceNpUtility_9D8E0730(reqId, avatarSizeType, npId, userInfo, aboutMe, languages,
				    countryCode, avatarImageData, avatarImageDataMaxSize,
				    avatarImageDataSize, 0, option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

int sceNpLookupUserProfileAsync(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				void *avatarImageData, SceSize avatarImageDataMaxSize,
				SceSize *avatarImageDataSize, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return userProfileAsync(reqId, avatarSizeType, npId, userInfo, aboutMe, languages,
				countryCode, avatarImageData, avatarImageDataMaxSize,
				avatarImageDataSize, 0, option);
}

int sceNpLookupAvatarImage(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			   SceNpAvatarImage *avatarImage, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_B2FD5E9D(reqId, avatarUrl, avatarImage, option);
}

int sceNpLookupAvatarImageAsync(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
				SceNpAvatarImage *avatarImage, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return avatarImageAsync(reqId, avatarUrl, avatarImage, option);
}

/* sceNpLookupUserProfile with the avatar of the N size table (SceNpUtilityAvatarN) */
int SceNpUtilityAvatarN_4A989C1C(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				 SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				 SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				 void *avatarImageData, SceSize avatarImageDataMaxSize,
				 SceSize *avatarImageDataSize, void *option)
{
	int ret;

	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = SceNpUtility_9D8E0730(reqId, avatarSizeType, npId, userInfo, aboutMe, languages,
				    countryCode, avatarImageData, avatarImageDataMaxSize,
				    avatarImageDataSize, 1, option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

int SceNpUtilityAvatarN_DC2B7B72(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				 SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				 SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				 void *avatarImageData, SceSize avatarImageDataMaxSize,
				 SceSize *avatarImageDataSize, void *option)
{
	if (!isInitialized())
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return userProfileAsync(reqId, avatarSizeType, npId, userInfo, aboutMe, languages,
				countryCode, avatarImageData, avatarImageDataMaxSize,
				avatarImageDataSize, 1, option);
}

/* the worker's allocator: the lookup service's heap */
static void *workerMalloc(SceSize size)
{
	return SceNpUtility_8CA24D19(size, NP_LOOKUP_SERVICE_LOOKUP);
}

static void workerFree(void *ptr)
{
	SceNpUtility_6C1161AD(ptr, NP_LOOKUP_SERVICE_LOOKUP);
}

static int lookupInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
			  void *option)
{
	int clientsUp = 0;
	int ret;

	if (s_initialized != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	ret = SceNpUtility_74B0D0AC(NP_LOOKUP_SERVICE_LOOKUP);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_E38548AB(NP_LOOKUP_SERVICE_LOOKUP);
	if (ret < 0)
		return ret;
	ret = SceNpUtility_9F142A92(NP_LOOKUP_SERVICE_LOOKUP);
	if (ret < 0)
		goto titles;
	ret = SceNpUtility_17385829(NP_LOOKUP_SERVICE_LOOKUP);
	if (ret < 0)
		goto transactions;
	s_async = 0;
	clientsUp = 1;

	if (usesAsync != 0) {
		s_worker = SceNpUtility_8CA24D19(40, NP_LOOKUP_SERVICE_LOOKUP);
		if (s_worker == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		} else {
			s_waitEventFlag = sceKernelCreateEventFlag("SceNpLookupForWaitAsync", 0, 0,
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
					ret = SceNpUtility_575BBBC4(s_worker, 3, s_jobHandlers);
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
	SceNpUtility_6DF884DA(NP_LOOKUP_SERVICE_LOOKUP);
titles:
	SceNpUtility_26B4AC00(NP_LOOKUP_SERVICE_LOOKUP);
	if (clientsUp == 1)
		SceNpUtility_7A86C37E(NP_LOOKUP_SERVICE_LOOKUP);
	return ret;
}

static int lookupTerm(void)
{
	int ret;

	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	ret = SceNpUtility_200BBAC5(NP_LOOKUP_SERVICE_LOOKUP);
	if (s_async != 0) {
		ret = SceNpUtility_5631E596(s_worker);
		if (s_worker != NULL) {
			SceNpUtility_6C1161AD(s_worker, NP_LOOKUP_SERVICE_LOOKUP);
			s_worker = NULL;
		}
		if (s_waitEventFlag != -1) {
			sceKernelDeleteEventFlag(s_waitEventFlag);
			s_waitEventFlag = -1;
		}
	}
	s_async = 0;
	SceNpUtility_6DF884DA(NP_LOOKUP_SERVICE_LOOKUP);
	SceNpUtility_26B4AC00(NP_LOOKUP_SERVICE_LOOKUP);
	SceNpUtility_7A86C37E(NP_LOOKUP_SERVICE_LOOKUP);
	SceNpUtility_418E1324(NP_LOOKUP_SERVICE_LOOKUP);
	s_initialized = 0;
	return ret;
}

static int isInitialized(void)
{
	return s_initialized == 1;
}

/* tells NpManager the lookup uses commId */
static inline void notifyTitle(const SceNpCommunicationId *commId)
{
	char commIdText[16];

	if (commId == NULL)
		return;
	sceClibMemset(commIdText, 0, 13);
	if (SceNpCommon_E426FEE8(commId, commIdText, 13) >= 0) {
		char name[32];

		sceClibMemset(name, 0, sizeof(name));
		sceClibStrlcpy(name, "NpLookup", sizeof(name));
		SceNpManager_9554802B("NpLookup", commIdText, name);
	}
}

static int lookupCreateTitleCtx(const SceNpCommunicationId *titleId,
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

	ret = SceNpUtility_F4B21646(&selfNpId->handle, selfNpId, NP_LOOKUP_SERVICE_LOOKUP);
	if (ret == SCE_NP_COMMUNITY_ERROR_CLIENT_HANDLE_ALREADY_EXISTS)
		ret = 0;
	else if (ret < 0)
		goto out;
	clientId = ret;

	ret = SceNpUtility_44A2F444(commId, selfNpId, clientId, NP_LOOKUP_SERVICE_LOOKUP);
	if (ret >= 0)
		notifyTitle(commId);
	else
		SceNpUtility_ED2B86ED(clientId, NP_LOOKUP_SERVICE_LOOKUP);
out:
	return ret;
}

static int lookupDeleteTitleCtx(SceInt32 titleCtxId)
{
	NpLookupTitle *title;
	int ret;

	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	title = NULL;
	ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_LOOKUP);
	if (ret >= 0) {
		SceNpUtility_CD3A3712(title->client);
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_LOOKUP);
		title = NULL;
		ret = SceNpUtility_4013244C(titleCtxId, NP_LOOKUP_SERVICE_LOOKUP);
	}
	return ret;
}

static int lookupCreateRequest(SceInt32 titleCtxId)
{
	NpLookupTitle *title = NULL;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	} else {
		ret = SceNpUtility_B9FFB253(titleCtxId, &title, NP_LOOKUP_SERVICE_LOOKUP);
		if (ret >= 0) {
			if (title == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			} else {
				ret = SceNpUtility_23414285(title, 0, &title->npId, title->id,
							    NP_LOOKUP_SERVICE_LOOKUP);
				if (ret >= 0) {
					SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_LOOKUP);
					return ret;
				}
			}
		}
	}
	if (title != NULL)
		SceNpUtility_485F368A(title, NP_LOOKUP_SERVICE_LOOKUP);
	return ret;
}

static int lookupDeleteRequest(SceInt32 reqId)
{
	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_B4423DAF(reqId, NP_LOOKUP_SERVICE_LOOKUP);
}

static int lookupWaitAsync(SceInt32 reqId, SceInt32 *result)
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

static int lookupPollAsync(SceInt32 reqId, SceInt32 *result)
{
	if (result == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	if (s_initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (s_async != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	return SceNpUtility_8904FE15(reqId, result);
}

static int npIdAsync(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
		     void *option)
{
	NpLookupTrans *trans = NULL;
	NpIdJob *job;
	SceNpOnlineId *copy;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (s_async != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (onlineId == NULL || npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto release;
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto release;
	}

	job = SceNpUtility_8CA24D19(sizeof(NpIdJob), trans->service);
	if (job == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto release;
	}
	job->reqId = reqId;
	job->npId = npId;
	job->option = NULL;

	copy = SceNpUtility_8CA24D19(sizeof(SceNpOnlineId), trans->service);
	if (copy == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemcpy_safe(copy, onlineId, sizeof(SceNpOnlineId));
		job->onlineId = copy;
		ret = SceNpUtility_46543307(trans, copy);
		if (ret >= 0) {
			ret = SceNpUtility_46543307(trans, job);
			if (ret >= 0) {
				job->trans = trans;
				trans->state = NP_LOOKUP_TRANS_QUEUED;
				ret = SceNpUtility_44FE1097(s_worker, 0, 0, job, 0);
				if (ret >= 0)
					return ret;
				trans->state = NP_LOOKUP_TRANS_RUNNING;
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

static int userProfileAsync(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			    SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			    SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			    void *avatarImageData, SceSize avatarImageDataMaxSize,
			    SceSize *avatarImageDataSize, int avatarN, void *option)
{
	NpLookupTrans *trans = NULL;
	UserProfileJob *job;
	SceNpId *copy;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (s_async != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto release;
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto release;
	}

	job = SceNpUtility_8CA24D19(sizeof(UserProfileJob), trans->service);
	if (job == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto free_job;
	}
	job->reqId = reqId;
	job->avatarSizeType = avatarSizeType;
	job->userInfo = userInfo;
	job->aboutMe = aboutMe;
	job->languages = languages;
	job->countryCode = countryCode;
	job->avatarImageData = avatarImageData;
	job->avatarImageDataMaxSize = avatarImageDataMaxSize;
	job->avatarImageDataSize = avatarImageDataSize;
	job->avatarN = avatarN;
	job->option = NULL;

	copy = SceNpUtility_8CA24D19(sizeof(SceNpId), trans->service);
	if (copy == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto free_copy;
	}
	sceClibMemcpy_safe(copy, npId, sizeof(SceNpId));
	job->npId = copy;
	ret = SceNpUtility_46543307(trans, copy);
	if (ret < 0)
		goto free_copy;
	ret = SceNpUtility_46543307(trans, job);
	if (ret < 0)
		goto free_job;
	job->trans = trans;
	trans->state = NP_LOOKUP_TRANS_QUEUED;
	ret = SceNpUtility_44FE1097(s_worker, 1, 0, job, 0);
	if (ret >= 0)
		return ret;
	trans->state = NP_LOOKUP_TRANS_RUNNING;
	ret = npLookupWorkerError(ret);
	goto release;

free_copy:
	if (copy != NULL && trans != NULL)
		SceNpUtility_6C1161AD(copy, trans->service);
free_job:
	if (job != NULL) {
		if (trans == NULL)
			goto out;
		SceNpUtility_6C1161AD(job, trans->service);
	}
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

static int avatarImageAsync(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			    SceNpAvatarImage *avatarImage, void *option)
{
	NpLookupTrans *trans = NULL;
	AvatarImageJob *job;
	SceNpAvatarUrl *copy;
	int ret;

	if (s_initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (s_async != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto out;
	}
	if (avatarUrl == NULL || avatarImage == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto out;
	}
	if (option != NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
		goto out;
	}

	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto release;
	if (trans->operation != 0) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_TYPE;
		goto release;
	}

	job = SceNpUtility_8CA24D19(sizeof(AvatarImageJob), trans->service);
	if (job == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto free_job;
	}
	job->reqId = reqId;
	job->avatarImage = avatarImage;
	job->option = NULL;

	copy = SceNpUtility_8CA24D19(sizeof(SceNpAvatarUrl), trans->service);
	if (copy == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto free_copy;
	}
	sceClibMemcpy_safe(copy, avatarUrl, sizeof(SceNpAvatarUrl));
	job->avatarUrl = copy;
	ret = SceNpUtility_46543307(trans, copy);
	if (ret < 0)
		goto free_copy;
	ret = SceNpUtility_46543307(trans, job);
	if (ret < 0)
		goto free_job;
	job->trans = trans;
	trans->state = NP_LOOKUP_TRANS_QUEUED;
	ret = SceNpUtility_44FE1097(s_worker, 2, 0, job, 0);
	if (ret >= 0)
		return ret;
	trans->state = NP_LOOKUP_TRANS_RUNNING;
	ret = npLookupWorkerError(ret);
	goto release;

free_copy:
	if (copy != NULL && trans != NULL)
		SceNpUtility_6C1161AD(copy, trans->service);
free_job:
	if (job != NULL) {
		if (trans == NULL)
			goto out;
		SceNpUtility_6C1161AD(job, trans->service);
	}
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

static int npIdJob(void *worker, int type, void *job)
{
	NpIdJob *request = job;
	int ret;

	if (request == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	ret = SceNpUtility_23B387A5(request->reqId, request->onlineId, request->npId,
				    request->option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

static int userProfileJob(void *worker, int type, void *job)
{
	UserProfileJob *request = job;
	int ret;

	if (request == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	ret = SceNpUtility_9D8E0730(request->reqId, request->avatarSizeType, request->npId,
				    request->userInfo, request->aboutMe, request->languages,
				    request->countryCode, request->avatarImageData,
				    request->avatarImageDataMaxSize, request->avatarImageDataSize,
				    request->avatarN, request->option);
	if (ret == 0 && SceNpUtility_2835BC9B() != 0)
		SceNpUtility_191D314C();
	return ret;
}

static int avatarImageJob(void *worker, int type, void *job)
{
	AvatarImageJob *request = job;

	if (request == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	return SceNpUtility_B2FD5E9D(request->reqId, request->avatarUrl, request->avatarImage,
				     request->option);
}

static int npIdDone(void *worker, int type, void *job, int arg, int result)
{
	NpIdJob *request = job;
	int ret = 0;

	if (request != NULL) {
		ret = SceNpUtility_F5086697(request->reqId, result);
		SceNpUtility_31D3542E(request->trans);
	}
	if (s_waitEventFlag != -1)
		return sceKernelSetEventFlag(s_waitEventFlag, 1);
	return ret;
}

static int userProfileDone(void *worker, int type, void *job, int arg, int result)
{
	UserProfileJob *request = job;
	int ret = 0;

	if (request != NULL) {
		ret = SceNpUtility_F5086697(request->reqId, result);
		SceNpUtility_31D3542E(request->trans);
	}
	if (s_waitEventFlag != -1)
		return sceKernelSetEventFlag(s_waitEventFlag, 1);
	return ret;
}

static int avatarImageDone(void *worker, int type, void *job, int arg, int result)
{
	AvatarImageJob *request = job;
	int ret = 0;

	if (request != NULL) {
		ret = SceNpUtility_F5086697(request->reqId, result);
		SceNpUtility_31D3542E(request->trans);
	}
	if (s_waitEventFlag != -1)
		return sceKernelSetEventFlag(s_waitEventFlag, 1);
	return ret;
}
