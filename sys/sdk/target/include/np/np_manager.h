/*
 * np/np_manager.h: the NP manager's user information, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_manager.h).
 */
#ifndef _SCE_NP_MANAGER_H_
#define _SCE_NP_MANAGER_H_

#include <np/np_common.h>

typedef struct SceNpManagerCacheParam {
	SceNpId npId;
	SceNpAvatarUrl avatarUrl;
} SceNpManagerCacheParam;

#ifdef __cplusplus
extern "C" {
#endif

int sceNpManagerGetNpId(SceNpId *npId);
int sceNpManagerGetAccountRegion(SceNpCountryCode *countryCode, int *languageCode);
int sceNpManagerGetContentRatingFlag(int *isRestricted, int *age);
int sceNpManagerGetChatRestrictionFlag(int *isRestricted);
int sceNpManagerGetCachedParam(SceNpManagerCacheParam *param);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_MANAGER_H_ */
