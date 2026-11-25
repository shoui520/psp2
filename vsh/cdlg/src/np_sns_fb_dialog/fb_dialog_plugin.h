/*
 * SceNpSnsFacebookDialogPlugin (vs0:sys/external/libcdlg_np_sns_fb.suprx): shared types,
 * SceNpSnsFacebook imports, and internal plugin interfaces.
 */
#ifndef NP_SNS_FB_DIALOG_PLUGIN_H
#define NP_SNS_FB_DIALOG_PLUGIN_H

#include <stdint.h>
#include <scetypes.h>
#include <common_dialog.h>
#include <np_sns_facebook_dialog.h>
#include <std/string>
#include <plugin.h>

/* internal plugin errors */
#define FB_DIALOG_ERROR_NO_WORK_MEMORY		((SceInt32)0x80106123)
#define FB_DIALOG_ERROR_NO_MEMORY		((SceInt32)0x80106181)
#define FB_DIALOG_ERROR_INVALID_STATE		((SceInt32)0x80106183)
#define FB_DIALOG_ERROR_COMMON_DIALOG_FAIL	((SceInt32)0x800204FF)

/* SceNpSnsFacebook internal error codes (0x805594xx) */
#define SNSFB_ERR_ALREADY_INITIALIZED		((SceInt32)0x80559401)
#define SNSFB_ERR_NOT_INITIALIZED		((SceInt32)0x80559402)
#define SNSFB_ERR_INVALID_ARGUMENT		((SceInt32)0x80559403)
#define SNSFB_ERR_OUT_OF_MEMORY			((SceInt32)0x80559404)
#define SNSFB_ERR_EXCEEDS_MAX			((SceInt32)0x80559405)
#define SNSFB_ERR_ABORTED			((SceInt32)0x80559406)
#define SNSFB_ERR_REQUEST_FAILED		((SceInt32)0x80559407)
#define SNSFB_ERR_SERVER_08			((SceInt32)0x80559408)
#define SNSFB_ERR_AUTH_09			((SceInt32)0x80559409)
#define SNSFB_ERR_AUTH_0A			((SceInt32)0x8055940A)
#define SNSFB_ERR_SERVER_0B			((SceInt32)0x8055940B)
#define SNSFB_ERR_NOT_SIGNIN			((SceInt32)0x8055940C)
#define SNSFB_ERR_ACCESS_NOT_ALLOWED		((SceInt32)0x8055940E)
#define SNSFB_ERR_ACCOUNT_NOT_BOUND		((SceInt32)0x8055940F)
#define SNSFB_ERR_CONFIG_DISABLED		((SceInt32)0x80559410)
#define SNSFB_ERR_CANCELED_BY_SYSTEM		((SceInt32)0x80559411)
#define SNSFB_ERR_CHAT_RESTRICTION		((SceInt32)0x80559412)
#define SNSFB_ERR_FB_13				((SceInt32)0x80559413)
#define SNSFB_ERR_FB_14				((SceInt32)0x80559414)
#define SNSFB_ERR_FB_15				((SceInt32)0x80559415)
#define SNSFB_ERR_FB_16				((SceInt32)0x80559416)
#define SNSFB_ERR_FB_17				((SceInt32)0x80559417)
#define SNSFB_ERR_FB_18				((SceInt32)0x80559418)

/* SceNpSnsFacebook public error codes (0x805526xx) */
#define SCE_NP_SNS_FB_ERROR_ALREADY_INITIALIZED	((SceInt32)0x80552602)
#define SCE_NP_SNS_FB_ERROR_NOT_INITIALIZED	((SceInt32)0x80552603)
#define SCE_NP_SNS_FB_ERROR_INVALID_ARGUMENT	((SceInt32)0x80552604)
#define SCE_NP_SNS_FB_ERROR_OUT_OF_MEMORY	((SceInt32)0x80552605)
#define SCE_NP_SNS_FB_ERROR_EXCEEDS_MAX		((SceInt32)0x80552606)
#define SCE_NP_SNS_FB_ERROR_NOT_SIGNIN		((SceInt32)0x80552607)
#define SCE_NP_SNS_FB_ERROR_CHAT_RESTRICTION	((SceInt32)0x80552608)
#define SCE_NP_SNS_FB_ERROR_ABORTED		((SceInt32)0x80552609)
#define SCE_NP_SNS_FB_ERROR_SERVER		((SceInt32)0x8055260A)
#define SCE_NP_SNS_FB_ERROR_ACCESS_NOT_ALLOWED	((SceInt32)0x8055260B)
#define SCE_NP_SNS_FB_ERROR_AUTH		((SceInt32)0x8055260C)
#define SCE_NP_SNS_FB_ERROR_ACCOUNT_NOT_BOUND	((SceInt32)0x8055260D)
#define SCE_NP_SNS_FB_ERROR_CONFIG_DISABLED	((SceInt32)0x8055260E)
#define SCE_NP_SNS_FB_ERROR_CANCELED_BY_SYSTEM	((SceInt32)0x8055260F)

/* work memory layout matching vsh/cdlg/src/cdlg_mgr/np_sns_facebook_dialog.cpp */
#define FB_DIALOG_DATA_SIZE_MAX			0x2CD0

typedef struct FbDialogInitArg {
	SceInt32 mode;
	const void *data;
	SceUInt8 reserved[28];
} FbDialogInitArg;

typedef struct CommonDialogNpSnsFacebookParam {
	SceInt32 abortResult;
	SceInt32 abortRequested;
	SceNpSnsFacebookDialogParam param;
	FbDialogInitArg initArg;
	SceInt32 dataType;
	SceUInt8 data[FB_DIALOG_DATA_SIZE_MAX];
	SceCommonDialogColor bgColor;
	SceUInt8 hasBgColor;
	SceUInt8 padding[3];
	SceInt32 internalResult[9];
	SceNpSnsFacebookDialogResultLongToken result;
	char apiName[64];
} CommonDialogNpSnsFacebookParam;

/* parameter passed from OnLoadFbDialogPlugin into fb_dialog_plugin's StartDialog interface */
typedef struct FbPluginStartParam {
	SceUInt8 hasBgColor;
	SceCommonDialogColor bgColor;
	char titleId[10];
	char titleName[128];
	SceUInt8 titlePadding[1];
	SceUInt64 fbAppId;
	SceInt32 dialogMode;
	char permissions[256];
	SceUInt32 reserved;
	SceInt32 feedDataType;
	const void *feedData;
} FbPluginStartParam;

typedef void (*FbDialogFinishCallback)(SceInt32 result, SceInt32 internalResult,
				       const SceNpSnsFacebookLongAccessTokenResult *token);

typedef struct FbDialogPluginInterface {
	SceInt32 (*Start)(SceInt32 mode, const FbPluginStartParam *param,
			  FbDialogFinishCallback cb);
} FbDialogPluginInterface;

/* SceNpSnsFacebook imports (matches vsh/app/common/src/np/np_sns_facebook/np_sns_facebook.c) */
extern "C" {
int SceNpSnsFacebook_18041141(int index, int *shareTrophy, int *shareRating, int *sharePurchase,
			      int *shareGame);
int SceNpSnsFacebook_3574800C(int index);
int SceNpSnsFacebook_46C23078(int index, int requestId);
int SceNpSnsFacebook_4A453D97(int index, int value, const void *data, const char *str, void *out);
int SceNpSnsFacebook_5069C04E(int index, char *buf, SceSize size);
int SceNpSnsFacebook_6A170F5D(int requestId, int *signedIn, int *chatAllowed);
int SceNpSnsFacebook_6CAB3583(int index);
int SceNpSnsFacebook_8770E114(int index, int requestId);
int SceNpSnsFacebook_9A0A4832(int index, void *out);
int SceNpSnsFacebook_9DE0B1BC(int index, int requestId, int visibility, const char *story,
			      const char *caption, const char *comment, void *out);
int SceNpSnsFacebook_A9C661E9(int index);
int SceNpSnsFacebook_ABF56333(int index, int requestId, const char *email, const char *password);
int SceNpSnsFacebook_B22D89FD(int requestId, const char *titleId, const char *appIdStr,
			      const char *permissions, void *out);
int SceNpSnsFacebook_C10D390C(int index, int requestId, int visibility, const char *story,
			      const char *comment, const char *extra, void *out);
int SceNpSnsFacebook_C9ACC1C1(int requestId);
int SceNpSnsFacebook_CB2ED352(int requestId, const void *data, const char *str, void *out);
int SceNpSnsFacebook_FA6FBAAE(int index, int *shareTrophy, int *shareRating, int *sharePurchase,
			      int *shareGame);
int SceNpSnsFacebook_FB99FF8E(int index);
int SceNpSnsFacebook_FD842B21(int requestId, const char *titleId, const char *appIdStr,
			      const char *permissions, void *out);
}

/* Facebook account info (324 bytes) and profile info (205 bytes) */
typedef struct FbAccountId {
	SceInt32 reserved;
	char email[320];
} FbAccountId;

typedef struct FbUserProfile {
	char raw[205];
} FbUserProfile;

/* Base dialog controller (defined in fb_dialog_controller.h) */
class FbDialogBase;

/* Helpers in fb_dialog_plugin.cpp */
int FbGetAccountEmail(int *hasEmail, FbAccountId *account, int index);
int FbGetUserProfile(const FbAccountId *account, int *hasProfile, FbUserProfile *profile,
		     int index);
int FbGetShareEnabled(int *enabled, int index);
int FbSetShareEnabled(int enabled, int index);
int FbInitService(int index);
int FbTermService(int index);
void FbFormatSingleLine(paf::string &text, bool truncate);
int FbIsRetryableError(int err);
int FbConvertError(int err);

#endif /* NP_SNS_FB_DIALOG_PLUGIN_H */
