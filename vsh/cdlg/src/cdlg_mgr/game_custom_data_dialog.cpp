/*
 * The game custom data dialog's API (sceGameCustomDataDialog*, in the
 * SceNpWebApiCommonDialog library): the check of the data to send (its
 * strings, the languages of its translations, the platforms and the
 * addresses), its Init and its GetResult on the worker thread. The dialog
 * itself is libcdlg_game_custom_data's plugin. OnInit and GetResult are
 * Sony's names (the names the calls keep in the work memory); the file's
 * name, the namespace and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <system_param.h>
#include <common_dialog.h>
#include <game_custom_data_dialog.h>
#include <np/np_npid.h>
#include <sysmodule/sysmodule.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define GAME_CUSTOM_DATA_LANGUAGE_NUM		20
#define GAME_CUSTOM_DATA_MODULE_NUM		5
/* the longest online ID, URL and thumbnail the dialog takes */
#define GAME_CUSTOM_DATA_MAX_THUMBNAIL_SIZE	0x28000

/* a language of a translated string: the system's language and its code */
typedef struct GameCustomDataLanguage {
	int language;
	const char *code;
} GameCustomDataLanguage;

GameCustomDataLanguage s_languages[GAME_CUSTOM_DATA_LANGUAGE_NUM] = {
	{ SCE_SYSTEM_PARAM_LANG_JAPANESE, "ja" },
	{ SCE_SYSTEM_PARAM_LANG_ENGLISH_US, "en" },
	{ SCE_SYSTEM_PARAM_LANG_FRENCH, "fr" },
	{ SCE_SYSTEM_PARAM_LANG_SPANISH, "es" },
	{ SCE_SYSTEM_PARAM_LANG_GERMAN, "de" },
	{ SCE_SYSTEM_PARAM_LANG_ITALIAN, "it" },
	{ SCE_SYSTEM_PARAM_LANG_DUTCH, "nl" },
	{ SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT, "pt" },
	{ SCE_SYSTEM_PARAM_LANG_RUSSIAN, "ru" },
	{ SCE_SYSTEM_PARAM_LANG_KOREAN, "ko" },
	{ SCE_SYSTEM_PARAM_LANG_CHINESE_T, "zh-TW" },
	{ SCE_SYSTEM_PARAM_LANG_CHINESE_S, "zh-CN" },
	{ SCE_SYSTEM_PARAM_LANG_FINNISH, "fi" },
	{ SCE_SYSTEM_PARAM_LANG_SWEDISH, "sv" },
	{ SCE_SYSTEM_PARAM_LANG_DANISH, "da" },
	{ SCE_SYSTEM_PARAM_LANG_NORWEGIAN, "no" },
	{ SCE_SYSTEM_PARAM_LANG_POLISH, "pl" },
	{ SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR, "pt-BR" },
	{ SCE_SYSTEM_PARAM_LANG_ENGLISH_GB, "en-GB" },
	{ SCE_SYSTEM_PARAM_LANG_TURKISH, "tr" },
};

namespace GameCustomDataDialog {

int GetResult(SceGameCustomDataDialogResult *result);

}

extern "C" int sceGameCustomDataDialogInit(const SceGameCustomDataDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA, (void *)param);
}

extern "C" SceCommonDialogStatus sceGameCustomDataDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
}

extern "C" int sceGameCustomDataDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
}

extern "C" int sceGameCustomDataDialogGetResult(SceGameCustomDataDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA, (void *)GameCustomDataDialog::GetResult, 1,
			       result);
}

extern "C" int sceGameCustomDataDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
}

namespace GameCustomDataDialog {

/* -1 when the UTF-8 string has max characters or more, else 0 */
int checkUtf8Length(const char *str, SceUInt32 max)
{
	const char *p;
	SceUInt32 n;
	char c;

	p = str;
	c = *p;
	n = 0;
	if (c != '\0') {
		do {
			if (n >= max)
				break;
			if ((c & 0x80) != 0 && (c & 0xc0) != 0x80) {
				if ((c & 0xe0) == 0xc0)
					p += 2;
				else if ((c & 0xf0) == 0xe0)
					p += 3;
				else if ((c & 0xf8) == 0xf0)
					p += 4;
				else if ((c & 0xfc) == 0xf8)
					p += 5;
				else if ((c & 0xfe) == 0xfc)
					p += 6;
				else
					p++;
			} else {
				p++;
			}
			c = *p;
			n++;
		} while (*p != '\0');
	}
	if (n == max && c != '\0')
		return -1;
	return 0;
}

/* true when every string is valid and no language comes twice */
bool checkMultiLanguageStrings(SceUInt32 num, SceUInt32 max, const SceGameCustomDataMultiLanguageString *strings)
{
	bool used[GAME_CUSTOM_DATA_LANGUAGE_NUM];
	SceUInt32 i;
	SceUInt32 j;
	int language;

	if (num == 0)
		return true;
	if (strings == NULL)
		return false;
	for (i = 0; i < GAME_CUSTOM_DATA_LANGUAGE_NUM; i++)
		used[i] = false;
	for (i = 0; i < num; i++) {
		if (strings[i].language == NULL)
			return false;
		if (strings[i].str == NULL || strings[i].str[0] == '\0')
			return false;
		if (checkUtf8Length((const char *)strings[i].str, max) != 0)
			return false;
		language = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
		for (j = 0; j < GAME_CUSTOM_DATA_LANGUAGE_NUM; j++) {
			if (sceClibStrcmp(s_languages[j].code, (const char *)strings[i].language) == 0) {
				language = s_languages[j].language;
				break;
			}
		}
		if (used[language])
			return false;
		used[language] = true;
	}
	return true;
}

/* -1 when the string is empty (unless allowed) or has max characters or more, else 0;
   checkSendParam's copy of checkStringLength, which reads the first character once */
static inline int stringLength(const char *str, SceUInt32 max, bool allowEmpty)
{
	const char *p;
	SceUInt32 n;
	char c;

	p = str;
	c = *p;
	if (!allowEmpty && c == '\0')
		return -1;
	n = 0;
	if (c != '\0') {
		do {
			if (n >= max)
				break;
			c = *++p;
			n++;
		} while (c != '\0');
	}
	if (n == max && c != '\0')
		return -1;
	return 0;
}

int checkStringLength(const char *str, SceUInt32 max, bool allowEmpty)
{
	const char *p;
	SceUInt32 n;
	char c;

	if (!allowEmpty && *str == '\0')
		return -1;
	p = str;
	c = *p;
	n = 0;
	if (c != '\0') {
		do {
			if (n >= max)
				break;
			c = *++p;
			n++;
		} while (c != '\0');
	}
	if (n == max && c != '\0')
		return -1;
	return 0;
}

int checkSendParam(const SceGameCustomDataDialogParam *param)
{
	const SceGameCustomDataDialogDataParam *dataParam;
	const SceNpOnlineId *onlineId;
	SceUInt32 i;

	dataParam = param->dataParam;
	if (dataParam == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if ((dataParam->SendInfo.options & ~(SCE_GAME_CUSTOM_DATA_DIALOG_OPTION_ASSUME_SEND
					     | SCE_GAME_CUSTOM_DATA_DIALOG_OPTION_SUSPEND_BLOCK)) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.userMessage != NULL && dataParam->SendInfo.userMessage[0] != '\0'
	    && checkUtf8Length((const char *)dataParam->SendInfo.userMessage,
			       SCE_GAME_CUSTOM_DATA_DIALOG_MAX_USER_MESSAGE_LENGTH) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.dataName == NULL || dataParam->SendInfo.dataName[0] == '\0'
	    || checkUtf8Length((const char *)dataParam->SendInfo.dataName,
			       SCE_GAME_CUSTOM_DATA_DIALOG_MAX_ATTACHMENT_DATA_NAME_LENGTH) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.dataDetail == NULL || dataParam->SendInfo.dataDetail[0] == '\0'
	    || checkUtf8Length((const char *)dataParam->SendInfo.dataDetail,
			       SCE_GAME_CUSTOM_DATA_DIALOG_MAX_ATTACHMENT_DETAIL_LENGTH) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.availablePlatform.count == 0
	    || dataParam->SendInfo.availablePlatform.platformName == NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < dataParam->SendInfo.availablePlatform.count; i++) {
		if (dataParam->SendInfo.availablePlatform.platformName[i] == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (sceClibStrcmp((const char *)dataParam->SendInfo.availablePlatform.platformName[i],
				  SCE_GAME_CUSTOM_DATA_DIALOG_AVAILABLE_PLATFORM_PS4) != 0
		    && sceClibStrcmp((const char *)dataParam->SendInfo.availablePlatform.platformName[i],
				     SCE_GAME_CUSTOM_DATA_DIALOG_AVAILABLE_PLATFORM_PSVITA) != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	switch (dataParam->SendInfo.addressParam.addressType) {
	case SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_TYPE_USERDISABLE:
		if (dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount == 0
		    || dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount
		       > SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		for (i = 0; i < dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount;
		     i++) {
			onlineId = &dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIds[i];
			if (onlineId == NULL)
				return SCE_COMMON_DIALOG_ERROR_NULL;
			if (stringLength(onlineId->data, SCE_NP_ONLINEID_MAX_LENGTH, false) != 0)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		}
		break;
	case SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_TYPE_USERENABLE:
		if (dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount == 0
		    || dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount
		       > SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if ((dataParam->SendInfo.options & SCE_GAME_CUSTOM_DATA_DIALOG_OPTION_ASSUME_SEND) != 0
	    && dataParam->SendInfo.addressParam.addressType != SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_TYPE_USERDISABLE)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	switch (param->mode) {
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND:
		if (dataParam->SendInfo.data == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (dataParam->SendInfo.dataSize == 0
		    || dataParam->SendInfo.dataSize > SCE_GAME_CUSTOM_DATA_DIALOG_MAX_ATTACHMENT_SIZE)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND_URL:
		if (dataParam->SendUrlInfo.dataUrl == NULL
		    || stringLength((const char *)dataParam->SendUrlInfo.dataUrl,
					 SCE_GAME_CUSTOM_DATA_DIALOG_MAX_URL_LENGTH, false) != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if (dataParam->SendInfo.thumbnail == NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.thumbnailSize == 0
	    || dataParam->SendInfo.thumbnailSize > GAME_CUSTOM_DATA_MAX_THUMBNAIL_SIZE)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.expireMinutes < 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (!checkMultiLanguageStrings(dataParam->SendInfo.dataNameMultiLangNum,
				       SCE_GAME_CUSTOM_DATA_DIALOG_MAX_ATTACHMENT_DATA_NAME_LENGTH,
				       dataParam->SendInfo.dataNameMultiLang))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (checkMultiLanguageStrings(dataParam->SendInfo.dataDetailMultiLangNum,
				      SCE_GAME_CUSTOM_DATA_DIALOG_MAX_ATTACHMENT_DETAIL_LENGTH,
				      dataParam->SendInfo.dataDetailMultiLang))
		return SCE_OK;
	return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
}

/* true when the size bytes are all zero */
bool isZeroFilled(const SceUChar8 *data, SceUInt32 size)
{
	SceUInt32 i;

	for (i = 0; i < size; i++) {
		if (data[i] != 0)
			return false;
	}
	return true;
}

int checkParam(const SceGameCustomDataDialogParam *param)
{
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (param->dataParam == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	switch (param->mode) {
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND:
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND_URL:
		ret = checkSendParam(param);
		break;
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_RECV:
		ret = SCE_OK;
		if ((param->dataParam->RecvInfo.options & ~SCE_GAME_CUSTOM_DATA_DIALOG_OPTION_SUSPEND_BLOCK) != 0)
			ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if (ret != SCE_OK)
		return ret;
	if (!isZeroFilled(param->reserved, sizeof(param->reserved)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	return SCE_OK;
}

int OnInit(void *arg)
{
	const SceGameCustomDataDialogParam *param = (const SceGameCustomDataDialogParam *)arg;
	CommonDialogGameCustomDataParam *work;
	bool infobarShown;
	SceUInt16 modules[GAME_CUSTOM_DATA_MODULE_NUM];
	SceUInt32 i;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	ret = checkParam(param);
	if (ret != SCE_OK)
		return ret;
	/* the network modules the dialog needs loaded by the application */
	modules[0] = SCE_SYSMODULE_NET;
	modules[1] = SCE_SYSMODULE_HTTPS;
	modules[2] = SCE_SYSMODULE_NP;
	modules[3] = SCE_SYSMODULE_NP_BASIC;
	modules[4] = SCE_SYSMODULE_NP_UTILITY;
	for (i = 0; i < GAME_CUSTOM_DATA_MODULE_NUM; i++) {
		ret = sceSysmoduleIsLoaded(modules[i]);
		if (ret != SCE_OK)
			return ret;
	}
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA, sizeof(CommonDialogGameCustomDataParam));
	if (ret != SCE_OK)
		return ret;
	LockDialog(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	work = (CommonDialogGameCustomDataParam *)GetWorkMemory(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName) - 1);
	work->apiName[sizeof(work->apiName) - 1] = '\0';
	sceClibMemset(work, 0, __builtin_offsetof(CommonDialogGameCustomDataParam, apiName));
	work->param.mode = param->mode;
	work->param.sdkVersion = param->sdkVersion;
	sceClibMemcpy_safe(&work->param.commonParam, &param->commonParam, sizeof(work->param.commonParam));
	work->param.callbackArg = param->callbackArg;
	work->param.dataParam = param->dataParam;
	work->drawPriority = -1;
	if (work->param.commonParam.bgColor != NULL) {
		sceClibMemcpy_safe(&work->bgColor, work->param.commonParam.bgColor, sizeof(work->bgColor));
		work->param.commonParam.bgColor = &work->bgColor;
	} else {
		work->bgColor.r = 38;
		work->bgColor.g = 40;
		work->bgColor.b = 46;
		work->bgColor.a = 255;
	}
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	sceClibMemset(work->apiName, 0, sizeof(work->apiName));
	UnlockDialog(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int GetResult(SceGameCustomDataDialogResult *result)
{
	CommonDialogGameCustomDataParam *work;
	SceUInt32 i;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	LockDialog(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	work = (CommonDialogGameCustomDataParam *)GetWorkMemory(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName) - 1);
	work->apiName[sizeof(work->apiName) - 1] = '\0';
	result->callbackArg = work->param.callbackArg;
	result->result = work->result;
	result->errorCode = work->errorCode;
	switch (work->param.mode) {
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND:
	case SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND_URL:
		if (work->result == SCE_COMMON_DIALOG_RESULT_OK && work->errorCode == SCE_OK
		    && result->sentOnlineIds != NULL) {
			for (i = 0; i < work->sentNpIdCount; i++)
				sceClibMemcpy_safe(&result->sentOnlineIds->onlineId[i], &work->sentNpId[i].handle,
						   sizeof(SceNpOnlineId));
			result->sentOnlineIds->count = work->sentNpIdCount;
		}
		break;
	}
	sceClibMemset(work->apiName, 0, sizeof(work->apiName));
	UnlockDialog(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	return SCE_OK;
}

}
