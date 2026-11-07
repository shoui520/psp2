/*
 * The IME dialog's API (sceImeDialog*): its Init, Init2, GetResult and
 * Abort on the worker thread. The dialog itself is libcdlg_ime's plugin.
 * OnInit, OnInit2, GetResult and Abort are Sony's names (the names the calls
 * keep in the work memory); the file's name, the namespace and the other
 * names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libime/libime_api.h>
#include <libime/libime_error.h>
#include <ime_dialog.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the languages an application may ask for, and the ones the system's own callers may */
#define IME_DIALOG_SUPPORTED_LANGUAGES			0x00000000000FFFFFULL
#define IME_DIALOG_SUPPORTED_LANGUAGES_INTERNAL		0x00000300000FFFFFULL

/* the IME types the system's own callers may ask for besides the SDK's */
#define IME_DIALOG_TYPE_INTERNAL_FIRST			0x100
#define IME_DIALOG_TYPE_INTERNAL_LAST			0x105

/* the options an application may ask for */
#define IME_DIALOG_OPTIONS	(SCE_IME_OPTION_MULTILINE | SCE_IME_OPTION_NO_AUTO_CAPITALIZATION | \
				 SCE_IME_OPTION_NO_ASSISTANCE)

/* the SDK versions that changed what the dialog accepts */
#define IME_DIALOG_SDK_VERSION_DIALOG_MODE		0x01500000	/* WITH_CANCEL, the result's button */
#define IME_DIALOG_SDK_VERSION_TEXT_BOX_CLEAR		0x01800000	/* WITH_CLEAR */
#define IME_DIALOG_SDK_VERSION_LONG_TEXT		0x02000000	/* above it: 2048 characters */
#define IME_DIALOG_SDK_VERSION_MAIL			0x02500000	/* URL and MAIL types */
#define IME_DIALOG_SHORT_TEXT_LENGTH			512

typedef CommonDialogApiScope<CommonDialogImeParam, COMMON_DIALOG_TYPE_IME,
			     SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY,
			     SAVEDATA_DIALOG_ERROR_API_NAME> ImeApiScope;

namespace ImeDialog {

int GetResult(SceImeDialogResult *result);
int Abort(void);

}

extern "C" SceInt32 sceImeDialogInit(const SceImeDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_IME, (void *)param);
}

extern "C" SceInt32 SceCommonDialog_9800F871(const SceImeDialogParam *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_IME, (void *)param, arg);
}

extern "C" SceCommonDialogStatus sceImeDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_IME);
}

extern "C" SceInt32 sceImeDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_IME, (void *)ImeDialog::Abort, 0);
}

extern "C" SceInt32 sceImeDialogGetResult(SceImeDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_IME, (void *)ImeDialog::GetResult, 1, result);
}

extern "C" SceInt32 sceImeDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_IME);
}

/* the IME dialog nested in a running dialog */
extern "C" SceInt32 SceCommonDialog_2CD0649B(const SceImeDialogParam *param)
{
	return NestInit(COMMON_DIALOG_TYPE_IME, (void *)param, NULL);
}

namespace ImeDialog {

/* what a call returns: the dialog's parameter error as it is, else as any
   dialog's call */
static inline int convertResult(int ret)
{
	if (ret == (int)SCE_IME_DIALOG_ERROR_PARAM)
		return ret;
	return toCommonDialogResult(ret);
}

/* the caller's parameter, as the SDK the application was built with allows
   it (internal: the system's own callers, through Init2) */
int checkParam(const SceImeDialogParam *param, bool internal)
{
	SceKernelSystemSwVersion version;
	SceUInt32 sdkVersion;
	SceUInt32 maxType;
	SceUInt32 maxTextLength;
	SceUInt32 option;
	SceUInt32 multiline;
	const SceWChar16 *text;
	SceUInt32 i;
	int length;
	int n;
	int ret;

	sdkVersion = param->sdkVersion;
	version.size = sizeof(version);
	if (sceKernelGetSystemSwVersion(&version) < 0)
		return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	if (sdkVersion > version.version)
		sdkVersion = version.version;
	if (internal) {
		if ((param->supportedLanguages & ~IME_DIALOG_SUPPORTED_LANGUAGES_INTERNAL) != 0)
			return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;
		if (param->type > SCE_IME_TYPE_MAIL &&
		    (param->type < IME_DIALOG_TYPE_INTERNAL_FIRST || param->type > IME_DIALOG_TYPE_INTERNAL_LAST))
			return SCE_IME_ERROR_INVALID_TYPE;
		option = param->option;
		multiline = option & SCE_IME_OPTION_MULTILINE;
	} else {
		if ((param->supportedLanguages & ~IME_DIALOG_SUPPORTED_LANGUAGES) != 0)
			return SCE_IME_ERROR_INVALID_SUPPORTED_LANGUAGES;
		maxType = SCE_IME_TYPE_EXTENDED_NUMBER;
		if (sdkVersion >= IME_DIALOG_SDK_VERSION_MAIL)
			maxType = SCE_IME_TYPE_MAIL;
		if (param->type > maxType)
			return SCE_IME_ERROR_INVALID_TYPE;
		option = param->option;
		if ((option & ~IME_DIALOG_OPTIONS) != 0)
			return SCE_IME_ERROR_INVALID_OPTION;
		multiline = param->option & SCE_IME_OPTION_MULTILINE;
	}
	if (option & SCE_IME_OPTION_MULTILINE) {
		if (param->textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD)
			return (int)SCE_IME_DIALOG_ERROR_INVALID_TEXT_BOX_MODE;
		if (sdkVersion >= IME_DIALOG_SDK_VERSION_TEXT_BOX_CLEAR &&
		    param->textBoxMode == SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR)
			return (int)SCE_IME_DIALOG_ERROR_INVALID_TEXT_BOX_MODE;
	}
	if (param->languagesForced != SCE_FALSE && param->languagesForced != SCE_TRUE)
		return SCE_IME_ERROR_INTERNAL;
	if (param->inputMethod != 0)
		return SCE_IME_ERROR_INVALID_INPUT_METHOD;
	if (param->enterLabel > SCE_IME_ENTER_LABEL_GO)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;
	if (param->enterLabel != SCE_IME_ENTER_LABEL_DEFAULT && option == SCE_IME_OPTION_MULTILINE)
		return SCE_IME_ERROR_INVALID_ENTER_LABEL;
	if (sdkVersion >= IME_DIALOG_SDK_VERSION_TEXT_BOX_CLEAR) {
		if (param->textBoxMode > SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR)
			return (int)SCE_IME_DIALOG_ERROR_INVALID_TEXT_BOX_MODE;
	} else if (param->textBoxMode >= SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR) {
		return (int)SCE_IME_DIALOG_ERROR_INVALID_TEXT_BOX_MODE;
	}
	ret = (int)SCE_IME_DIALOG_ERROR_INVALID_DIALOG_MODE;
	if (sdkVersion >= IME_DIALOG_SDK_VERSION_DIALOG_MODE) {
		if (param->dialogMode > SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL)
			return ret;
	} else if (param->dialogMode != SCE_IME_DIALOG_DIALOG_MODE_DEFAULT) {
		return ret;
	}
	maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
	if (param->sdkVersion <= IME_DIALOG_SDK_VERSION_LONG_TEXT)
		maxTextLength = IME_DIALOG_SHORT_TEXT_LENGTH;
	if (param->maxTextLength == 0 || param->maxTextLength > maxTextLength)
		return SCE_IME_ERROR_INVALID_MAX_TEXT_LENGTH;
	text = param->initialText;
	if (text != NULL) {
		/* Sony's: the last character before the end is not checked */
		length = 0;
		for (i = 0; text[i] != 0 && i < param->maxTextLength; i++)
			length = i;
		ret = SCE_IME_ERROR_INVALID_TEXT;
		for (n = 0; n < length; n++) {
			if (param->initialText[n] == '\n' || param->initialText[n] == '\r') {
				if (!multiline)
					return ret;
			} else if (param->initialText[n] >= 0xD800 && param->initialText[n] <= 0xDFFF) {
				return ret;
			}
		}
	}
	if (param->inputTextBuffer == NULL)
		return SCE_IME_ERROR_INVALID_INPUT_TEXT_BUFFER;
	if (param->title == NULL)
		return (int)SCE_IME_DIALOG_ERROR_INVALID_TITLE;
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return SCE_IME_ERROR_INVALID_RESERVED;
	}
	return SCE_OK;
}

/* a string copied with its end, cut at max characters */
static inline void copyText(SceWChar16 *dst, const SceWChar16 *src, int max)
{
	int n;

	for (n = 0; src[n] != 0 && n < max; n++)
		dst[n] = src[n];
	if (n < max)
		dst[n] = 0;
	else
		dst[n - 1] = 0;
}

/* the caller's parameter into the work memory, with copies of its title and its text */
int copyParam(CommonDialogImeParam *work, const SceImeDialogParam *param)
{
	sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
	copyText(work->title, param->title, SCE_IME_DIALOG_MAX_TITLE_LENGTH);
	work->param.title = work->title;
	if (param->initialText != NULL)
		copyText(work->text, param->initialText, SCE_IME_DIALOG_MAX_TEXT_LENGTH);
	else
		work->text[0] = 0;
	work->param.initialText = work->text;
	return SCE_OK;
}

/* Init and Init2: the parameter checked, then into the work memory */
static inline int init(const SceImeDialogParam *param, bool internal, const char *name)
{
	bool infobarShown;
	int ret;

	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		ret = checkParam(param, internal);
		if (ret != SCE_OK)
			return ret;
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK)
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_IME, sizeof(CommonDialogImeParam));
		if (ret == SCE_OK) {
			ImeApiScope scope(name, ret);

			if (ret == SCE_OK) {
				CommonDialogImeParam *work = scope.work();

				ret = copyParam(work, param);
				if (ret == SCE_OK) {
					SceKernelSystemSwVersion version;
					SceUInt32 sdkVersion;

					sdkVersion = param->sdkVersion;
					version.size = sizeof(version);
					if (sceKernelGetSystemSwVersion(&version) >= 0) {
						if (sdkVersion > version.version)
							sdkVersion = version.version;
						work->hasButton = sdkVersion >= IME_DIALOG_SDK_VERSION_DIALOG_MODE;
					} else {
						work->hasButton = false;
					}
					work->paramSet = 1;
					work->infobarShown = infobarShown;
				}
			}
		}
	}
	return convertResult(ret);
}

int OnInit(void *param)
{
	return init((const SceImeDialogParam *)param, false, __FUNCTION__);
}

int OnInit2(void *param, void *arg)
{
	return init((const SceImeDialogParam *)param, true, __FUNCTION__);
}

/*
 * GetResult takes the work memory by hand: on a reserved byte that is not
 * zero it returns with the dialog still locked (Sony's).
 */
int GetResult(SceImeDialogResult *result)
{
	CommonDialogImeParam *work;
	const SceChar8 *reserved;
	SceUInt32 size;
	SceUInt32 i;
	int unlockRet;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_IME) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	work = NULL;
	ret = LockDialog(COMMON_DIALOG_TYPE_IME);
	if (ret == SCE_OK) {
		work = (CommonDialogImeParam *)GetWorkMemory(COMMON_DIALOG_TYPE_IME);
		if (work != NULL) {
			sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName));
			ret = SCE_OK;
			work->apiName[sizeof(work->apiName) - 1] = '\0';
		} else {
			ret = SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY;
			UnlockDialog(COMMON_DIALOG_TYPE_IME);
		}
	}
	if (ret == SCE_OK) {
		/* without the button (before SDK 1.50) the button is reserved too */
		if (work->hasButton) {
			reserved = result->reserved;
			size = sizeof(result->reserved);
		} else {
			reserved = (const SceChar8 *)&result->button;
			size = sizeof(result->button) + sizeof(result->reserved);
		}
		for (i = 0; i < size; i++) {
			if (reserved[i] != 0) {
				ret = SCE_IME_ERROR_INVALID_RESERVED;
				break;
			}
		}
		if (ret != SCE_OK)
			return ret;
		sceClibMemcpy_safe(result, &work->result, sizeof(*result));
	}
	if (work != NULL) {
		if (sceClibStrncmp(work->apiName, __FUNCTION__, sizeof(work->apiName)) != 0) {
			unlockRet = SAVEDATA_DIALOG_ERROR_API_NAME;
		} else {
			sceClibMemset(work->apiName, 0, sizeof(work->apiName));
			unlockRet = UnlockDialog(COMMON_DIALOG_TYPE_IME);
		}
		if (unlockRet != SCE_OK && ret == SCE_OK)
			ret = unlockRet;
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		ImeApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			if (scope.work()->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_IME);
				if (ret == SCE_OK) {
					scope.work()->abortRequested = 1;
					if (scope.work()->abortResult == 0)
						scope.work()->abortResult = 1;
				}
			}
		}
	}
	return convertResult(ret);
}

}
