/*
 * np_sns_facebook_dialog.h: Facebook Permission Dialog, under Sony's names and with
 * Sony's definitions (the NP SNS Facebook Library Reference; SDK Release 03.570.011 does
 * not ship the header). SceNpSnsFacebookDialogResult, the short token's
 * result, is our name, after SceNpSnsFacebookDialogResultLongToken.
 */
#ifndef _SCE_NP_SNS_FACEBOOK_DIALOG_H_
#define _SCE_NP_SNS_FACEBOOK_DIALOG_H_

#include <scetypes.h>
#include <common_dialog.h>
#include <np/np_sns_facebook.h>

#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL			-2146410239	/* 0x80106101 */
#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED	-2146410238	/* 0x80106102 */
#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM				-2146410237	/* 0x80106103 */
#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_NOT_SIGNIN			-2146410236	/* 0x80106104 */
#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_CHAT_RESTRICTION		-2146410235	/* 0x80106105 */
#define SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_LIBRARY_NOT_INITIALIZED	-2146410234	/* 0x80106106 */

#define SCE_NP_SNS_FACEBOOK_DIALOG_MODE_LONG_ACCESS_TOKEN	1

typedef struct SceNpSnsFacebookDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceNpSnsFacebookAccessTokenParam accessTokenParam;
	SceInt32 mode;
	SceUInt8 reserved[28];
} SceNpSnsFacebookDialogParam;

typedef struct SceNpSnsFacebookDialogResult {
	SceNpSnsFacebookAccessTokenResult accessTokenResult;
	SceInt32 result;
	SceUInt8 reserved[36];
} SceNpSnsFacebookDialogResult;

typedef struct SceNpSnsFacebookDialogResultLongToken {
	SceNpSnsFacebookLongAccessTokenResult accessTokenResult;
	SceInt32 result;
	SceUInt8 reserved[36];
} SceNpSnsFacebookDialogResultLongToken;

#endif /* _SCE_NP_SNS_FACEBOOK_DIALOG_H_ */
