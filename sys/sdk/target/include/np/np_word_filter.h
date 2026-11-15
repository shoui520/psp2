/*
 * np/np_word_filter.h: the word filter, under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/np/np_word_filter.h).
 */
#ifndef _SCE_NP_WORD_FILTER_H_
#define _SCE_NP_WORD_FILTER_H_

#include <np/np_common.h>

#define SCE_NP_WORD_FILTER_MAX_CTX_NUM			(32)
#define SCE_NP_WORD_FILTER_MAX_REQUEST_NUM		(32)
#define SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN	(1024)
#define SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN	(1024)
#define SCE_NP_WORD_FILTER_TIMEOUT_NO_EFFECT		(0)

#ifdef __cplusplus
extern "C" {
#endif

int sceNpWordFilterInit(SceBool usesAsync, SceInt32 threadPriority, SceInt32 cpuAffinityMask,
			void *option);
int sceNpWordFilterTerm(void);
int sceNpWordFilterCreateTitleCtx(const SceNpCommunicationId *titleId, const SceNpId *selfNpId);
int sceNpWordFilterDeleteTitleCtx(SceInt32 titleCtxId);
int sceNpWordFilterCreateRequest(SceInt32 titleCtxId);
int sceNpWordFilterDeleteRequest(SceInt32 reqId);
int sceNpWordFilterAbortRequest(SceInt32 reqId);
int sceNpWordFilterSetTimeout(SceInt32 id, SceInt32 resolveRetry, SceUInt32 resolveTimeout,
			      SceUInt32 connTimeout, SceUInt32 sendTimeout, SceUInt32 recvTimeout);
int sceNpWordFilterWaitAsync(SceInt32 reqId, SceInt32 *result);
int sceNpWordFilterPollAsync(SceInt32 reqId, SceInt32 *result);
SceInt32 sceNpWordFilterCensorComment(SceInt32 reqId, const char *comment, void *option);
SceInt32 sceNpWordFilterCensorCommentAsync(SceInt32 reqId, const char *comment, void *option);
SceInt32 sceNpWordFilterSanitizeComment(SceInt32 reqId, const char *comment,
					char *sanitizedComment, void *option);
SceInt32 sceNpWordFilterSanitizeCommentAsync(SceInt32 reqId, const char *comment,
					     char *sanitizedComment, void *option);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_WORD_FILTER_H_ */
