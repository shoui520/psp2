/*
 * np/np_lookup.h: the NP lookup, under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/np/np_lookup.h).
 */
#ifndef _SCE_NP_LOOKUP_H_
#define _SCE_NP_LOOKUP_H_

#include <scetypes.h>
#include <np/np_common.h>

#define SCE_NP_LOOKUP_MAX_CTX_NUM	(32)
#define SCE_NP_LOOKUP_MAX_REQUEST_NUM	(32)
#define SCE_NP_LOOKUP_TIMEOUT_NO_EFFECT	(0)

#ifdef __cplusplus
extern "C" {
#endif

int sceNpLookupInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
		    void *option);
int sceNpLookupTerm(void);
int sceNpLookupCreateTitleCtx(const SceNpCommunicationId *titleId, const SceNpId *selfNpId);
int sceNpLookupDeleteTitleCtx(SceInt32 titleCtxId);
#define sceNpLookupDestroyTitleCtx(id)		sceNpLookupDeleteTitleCtx(id)
int sceNpLookupCreateRequest(SceInt32 titleCtxId);
#define sceNpLookupCreateTransactionCtx(id)	sceNpLookupCreateRequest(id)
int sceNpLookupDeleteRequest(SceInt32 reqId);
#define sceNpLookupDestroyTransactionCtx(id)	sceNpLookupDeleteRequest(id)
int sceNpLookupAbortRequest(SceInt32 reqId);
#define sceNpLookupAbortTransaction(id)		sceNpLookupAbortRequest(id)
int sceNpLookupSetTimeout(SceInt32 id, SceInt32 resolveRetry, SceUInt32 resolveTimeout,
			  SceUInt32 connTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout);
int sceNpLookupWaitAsync(SceInt32 reqId, SceInt32 *result);
int sceNpLookupPollAsync(SceInt32 reqId, SceInt32 *result);
int sceNpLookupNpId(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId, void *option);
int sceNpLookupNpIdAsync(SceInt32 reqId, const SceNpOnlineId *onlineId, SceNpId *npId,
			 void *option);
int sceNpLookupUserProfile(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
			   SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
			   SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
			   void *avatarImageData, SceSize avatarImageDataMaxSize,
			   SceSize *avatarImageDataSize, void *option);
int sceNpLookupUserProfileAsync(SceInt32 reqId, SceInt32 avatarSizeType, const SceNpId *npId,
				SceNpUserInformation *userInfo, SceNpAboutMe *aboutMe,
				SceNpMyLanguages *languages, SceNpCountryCode *countryCode,
				void *avatarImageData, SceSize avatarImageDataMaxSize,
				SceSize *avatarImageDataSize, void *option);
int sceNpLookupAvatarImage(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
			   SceNpAvatarImage *avatarImage, void *option);
int sceNpLookupAvatarImageAsync(SceInt32 reqId, const SceNpAvatarUrl *avatarUrl,
				SceNpAvatarImage *avatarImage, void *option);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_LOOKUP_H_ */
