/*
 * np/np_sns_facebook.h: the NP SNS Facebook library's access token types, under Sony's
 * names and with Sony's definitions (the NP SNS Facebook Library Reference; SDK Release
 * 03.570.011 does not ship the header). SceNpSnsFacebookAccessTokenResult, the short
 * token sceNpSnsFacebookDialogGetResult returns, is our name, after the long one's.
 */
#ifndef _SCE_NP_SNS_FACEBOOK_H_
#define _SCE_NP_SNS_FACEBOOK_H_

#include <scetypes.h>

#define SCE_NP_SNS_FACEBOOK_PERMISSIONS_LENGTH_MAX	(255)
#define SCE_NP_SNS_FACEBOOK_ACCESS_TOKEN_LENGTH_MAX	(255)
#define SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX	(4096)

typedef struct SceNpSnsFacebookAccessTokenParam {
	SceUInt64 fbAppId;
	char permissions[SCE_NP_SNS_FACEBOOK_PERMISSIONS_LENGTH_MAX + 1];
	SceUInt8 reserved[32];
} SceNpSnsFacebookAccessTokenParam;

typedef struct SceNpSnsFacebookAccessTokenResult {
	SceUInt64 expiration;
	char accessToken[SCE_NP_SNS_FACEBOOK_ACCESS_TOKEN_LENGTH_MAX + 1];
	SceUInt8 reserved[32];
} SceNpSnsFacebookAccessTokenResult;

typedef struct SceNpSnsFacebookLongAccessTokenResult {
	SceUInt64 expiration;
	char accessToken[SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX + 1];
	SceUInt8 reserved[39];
} SceNpSnsFacebookLongAccessTokenResult;

#endif /* _SCE_NP_SNS_FACEBOOK_H_ */
