/*
 * SceNpSnsFacebookDialogPlugin (vs0:sys/external/libcdlg_np_sns_fb.suprx): module entry,
 * SceCommonDialogMain integration, fb_dialog_plugin lifecycle, and SceNpSnsFacebook helpers.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <std/new>
#include <std/string>
#include <std/string.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <framework.h>
#include <module/module.h>
#include <plugin.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"
#include "fb_dialog_plugin.h"
#include "fb_dialog_controller.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
size_t sce_paf_wcsnlen(const wchar_t *s, size_t maxlen);
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static Plugin::ModuleInterface s_moduleInterface;
static Plugin *s_fbPlugin = NULL;
static FbDialogBase *s_controller = NULL;
static int32_t s_savedDrawPriority = -1;
static FbDialogFinishCallback s_finishCallback = NULL;

static SceInt32 StartFbDialog(SceInt32 mode, const FbPluginStartParam *param,
			      FbDialogFinishCallback cb);

static FbDialogPluginInterface s_pluginInterface = {
	StartFbDialog
};

static void fbDialogInit(SceSize arglen, const void *argp);
static void fbDialogExit(void);
static SceInt32 SetDialogResult(CommonDialogNpSnsFacebookParam *work, SceInt32 result,
				SceInt32 internalResult,
				const SceNpSnsFacebookLongAccessTokenResult *token);
static void OnUpdate(void *userdata);
static void OnLoadFbDialogPlugin(Plugin *plugin);
static void OnUnloadFbDialogPlugin(const char *name);
static void OnReturnPlugin(SceInt32 result, SceInt32 internalResult,
			   const SceNpSnsFacebookLongAccessTokenResult *token);
static void FbDialogPluginSetParam(Plugin::InitParam2 *param);
static void FbDialogPluginInit(Plugin *plugin);
static void FbDialogPluginStart(Plugin *plugin);
static void FbDialogPluginStop(Plugin *plugin);
static void FbDialogPluginExit(Plugin *plugin);

static void FbInnerSetParam(Plugin::InitParam2 *param);
static void FbInnerInit(Plugin *plugin);
static void FbInnerStart(Plugin *plugin);
static void FbInnerStop(Plugin *plugin);
static void FbInnerExit(Plugin *plugin);

extern "C" int module_start(SceSize arglen, const void *argp)
{
	fbDialogInit(arglen, argp);
	return 0;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	fbDialogExit();
	return 0;
}

static __attribute__((noinline)) void fbDialogInit(SceSize arglen, const void *argp)
{
	void (**init)(void);
	Module *module;

	(void)arglen;
	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	module = *(Module *const *)argp;
	s_moduleInterface.set_param_func = FbDialogPluginSetParam;
	s_moduleInterface.init_func = FbDialogPluginInit;
	s_moduleInterface.start_func = FbDialogPluginStart;
	s_moduleInterface.stop_func = FbDialogPluginStop;
	s_moduleInterface.exit_func = FbDialogPluginExit;
	module->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void fbDialogExit(void)
{
	__cxa_finalize(&__dso_handle);
}

static __attribute__((noinline)) SceInt32
SetDialogResult(CommonDialogNpSnsFacebookParam *work, SceInt32 result, SceInt32 internalResult,
		const SceNpSnsFacebookLongAccessTokenResult *token)
{
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	sce_paf_memset(work->internalResult, 0, sizeof(work->internalResult));
	work->internalResult[0] = internalResult;
	if (token != NULL)
		sce_paf_memcpy(&work->result.accessTokenResult, token,
			       sizeof(work->result.accessTokenResult));
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NP_SNS_FB) != 0) {
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		return SCE_COMMON_DIALOG_RESULT_ABORTED;
	}
	if (result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL ||
	    result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED ||
	    result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM ||
	    result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_NOT_SIGNIN ||
	    result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_CHAT_RESTRICTION ||
	    result == SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_LIBRARY_NOT_INITIALIZED) {
		work->result.result = result;
		return result;
	}
	if (result >= 0) {
		work->result.result = result;
		return result;
	}
	work->result.result = FB_DIALOG_ERROR_COMMON_DIALOG_FAIL;
	return FB_DIALOG_ERROR_COMMON_DIALOG_FAIL;
}

static void OnUpdate(void *userdata)
{
	SceInt32 ret;
	CommonDialogNpSnsFacebookParam *work = NULL;

	(void)userdata;
	ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NP_SNS_FB);
	if (ret == 0) {
		work = (CommonDialogNpSnsFacebookParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_NP_SNS_FB);
		if (work != NULL) {
			sce_paf_strncpy(work->apiName, "OnUpdate", sizeof(work->apiName));
			work->apiName[sizeof(work->apiName) - 1] = '\0';
			if (work->abortResult == 1) {
				if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NP_SNS_FB) != 0)
					Plugin::UnloadAsync("fb_dialog_plugin",
							    OnUnloadFbDialogPlugin, 0);
				SetDialogResult(work, SCE_COMMON_DIALOG_RESULT_ABORTED, 0, NULL);
				work->abortResult = 2;
			}
		} else {
			ret = FB_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
		}
	}
	if (ret < 0) {
		SetDialogResult(work, ret, 0, NULL);
		work->abortResult = 2;
		Plugin::UnloadAsync("fb_dialog_plugin", OnUnloadFbDialogPlugin, 0);
	}
	if (work != NULL && sce_paf_strncmp(work->apiName, "OnUpdate", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
	}
}

static void FbDialogPluginSetParam(Plugin::InitParam2 *param)
{
	param->graphics_flag = 6;
}

static void FbDialogPluginInit(Plugin *plugin)
{
	(void)plugin;
}

static void FbDialogPluginStart(Plugin *plugin)
{
	Plugin::InitParam initParam;

	(void)plugin;
	initParam.name = "fb_dialog_plugin";
	initParam.resource_file = "vs0:data/external/cdlg/fb_dialog_plugin.rco";
	initParam.caller_name = "np_sns_fb_plugin";
	initParam.set_param_func = FbInnerSetParam;
	initParam.init_func = FbInnerInit;
	initParam.start_func = FbInnerStart;
	initParam.stop_func = FbInnerStop;
	initParam.exit_func = FbInnerExit;
	Plugin::LoadAsync(initParam, OnLoadFbDialogPlugin, NULL);
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void FbDialogPluginStop(Plugin *plugin)
{
	plugin->PageCloseAll();
}

static void FbDialogPluginExit(Plugin *plugin)
{
	(void)plugin;
}

static void OnReturnPlugin(SceInt32 result, SceInt32 internalResult,
			   const SceNpSnsFacebookLongAccessTokenResult *token)
{
	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NP_SNS_FB) == 0) {
		CommonDialogNpSnsFacebookParam *work =
			(CommonDialogNpSnsFacebookParam *)SceCommonDialogMain_DD2CAC30(
				COMMON_DIALOG_TYPE_NP_SNS_FB);

		if (work != NULL) {
			sce_paf_strncpy(work->apiName, "OnReturnPlugin", sizeof(work->apiName));
			work->apiName[sizeof(work->apiName) - 1] = '\0';
			SetDialogResult(work, result, internalResult, token);
		} else {
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
		}
		if (work != NULL &&
		    sce_paf_strncmp(work->apiName, "OnReturnPlugin", sizeof(work->apiName)) == 0) {
			sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
		}
	}
	Plugin::UnloadAsync("fb_dialog_plugin", OnUnloadFbDialogPlugin, 0);
}

static void OnLoadFbDialogPlugin(Plugin *plugin)
{
	FbPluginStartParam startParam;
	CommonDialogNpSnsFacebookParam *work = NULL;
	SceInt32 ret;
	SceInt32 startMode = 0;

	sce_paf_memset(&startParam, 0, sizeof(startParam));
	ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NP_SNS_FB);
	if (ret == 0) {
		work = (CommonDialogNpSnsFacebookParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_NP_SNS_FB);
		if (work != NULL) {
			sce_paf_strncpy(work->apiName, "OnLoadFbDialogPlugin",
					sizeof(work->apiName));
			work->apiName[sizeof(work->apiName) - 1] = '\0';
		} else {
			ret = FB_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
		}
	}
	if (ret == 0) {
		switch (work->initArg.mode) {
		case 0:
		case 2:
			startMode = 1;
			break;
		case 1:
			startMode = 2;
			break;
		case 3:
			startMode = 3;
			break;
		default:
			ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
			break;
		}
	}
	if (ret == 0) {
		if (work->hasBgColor != 0) {
			startParam.hasBgColor = 1;
			sce_paf_memcpy(&startParam.bgColor, &work->bgColor,
				       sizeof(startParam.bgColor));
		} else {
			startParam.hasBgColor = 0;
		}
		startParam.dialogMode = work->param.mode;
		if (startMode == 1) {
			SceUID pid = sceKernelGetProcessId();

			ret = sceAppMgrAppParamGetString(pid, 12, startParam.titleId,
							 sizeof(startParam.titleId));
			if (ret == 0) {
				pid = sceKernelGetProcessId();
				ret = sceAppMgrAppParamGetString(pid, 9, startParam.titleName,
								 sizeof(startParam.titleName));
			}
			if (ret == 0) {
				startParam.fbAppId = work->param.accessTokenParam.fbAppId;
				sce_paf_memset(startParam.permissions, 0,
					       sizeof(startParam.permissions));
				sce_paf_strncpy(startParam.permissions,
						work->param.accessTokenParam.permissions,
						sizeof(startParam.permissions) - 1);
			}
		} else if (startMode == 3) {
			startParam.feedDataType = 0;
			if (work->dataType >= 1 && work->dataType <= 4) {
				startParam.feedData = work->data;
			} else {
				ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
			}
		}
	}
	if (ret == 0) {
		FbDialogPluginInterface *iface =
			(FbDialogPluginInterface *)plugin->GetInterface(1);

		if (iface == NULL) {
			ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL;
		} else {
			ret = iface->Start(startMode, &startParam, OnReturnPlugin);
			if (ret == SNSFB_ERR_NOT_INITIALIZED)
				ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_LIBRARY_NOT_INITIALIZED;
		}
	}
	if (ret < 0) {
		if (work != NULL) {
			SetDialogResult(work, ret, 0, NULL);
			work->abortResult = 0;
		}
		Plugin::UnloadAsync("fb_dialog_plugin", OnUnloadFbDialogPlugin, 0);
	}
	if (work != NULL &&
	    sce_paf_strncmp(work->apiName, "OnLoadFbDialogPlugin", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NP_SNS_FB);
	}
}

static void OnUnloadFbDialogPlugin(const char *name)
{
	(void)name;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NP_SNS_FB);
}

static void FinishTrampoline(SceInt32 result, SceInt32 internalResult,
			     const SceNpSnsFacebookLongAccessTokenResult *token)
{
	if (s_finishCallback != NULL)
		s_finishCallback(result, internalResult, token);
}

static void FbInnerSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 90;
	param->graphics_flag = 6;
}

static void FbInnerInit(Plugin *plugin)
{
	s_savedDrawPriority = Framework::Instance()->GetLocale();
	s_fbPlugin = plugin;
	s_finishCallback = NULL;
	plugin->SetInterface(1, &s_pluginInterface);
}

static void FbInnerStart(Plugin *plugin)
{
	(void)plugin;
}

static void FbInnerStop(Plugin *plugin)
{
	if (s_controller != NULL)
		s_controller->Abort();
	plugin->PageCloseAll();
}

static void FbInnerExit(Plugin *plugin)
{
	(void)plugin;
	if (s_controller != NULL) {
		s_controller->WaitEnd();
		delete s_controller;
	}
	s_controller = NULL;
	Framework::Instance()->SetCommonResourceLocale((Locale)s_savedDrawPriority);
	s_savedDrawPriority = -1;
}

static SceInt32 StartFbDialog(SceInt32 mode, const FbPluginStartParam *param,
			      FbDialogFinishCallback cb)
{
	s_finishCallback = cb;
	switch (mode) {
	case 1:
		s_controller = new FbAccessTokenController();
		if (s_controller == NULL)
			return FB_DIALOG_ERROR_NO_MEMORY;
		return s_controller->Start(param, s_fbPlugin, FinishTrampoline);
	case 2:
		s_controller = new FbLongAccessTokenController();
		if (s_controller == NULL)
			return FB_DIALOG_ERROR_NO_MEMORY;
		return s_controller->Start(param, s_fbPlugin, FinishTrampoline);
	case 3:
		s_controller = new FbFeedController();
		if (s_controller == NULL)
			return FB_DIALOG_ERROR_NO_MEMORY;
		return s_controller->Start(param, s_fbPlugin, FinishTrampoline);
	default:
		return FB_DIALOG_ERROR_INVALID_STATE;
	}
}

void FbSetPluginLocale(Plugin *plugin, int locale)
{
	if (locale != -1) {
		if (plugin != NULL)
			plugin->SetLocale((Locale)locale);
		Framework::Instance()->SetCommonResourceLocale((Locale)locale);
	}
}

const wchar_t *FbGetPluginString(Plugin *plugin, const IDParam &id)
{
	if (plugin != NULL)
		return plugin->GetString(id);
	return NULL;
}

int FbGetAccountEmail(int *hasEmail, FbAccountId *account, int index)
{
	int ret;

	if (account == NULL)
		return SNSFB_ERR_INVALID_ARGUMENT;
	ret = SceNpSnsFacebook_5069C04E(index, account->email, sizeof(account->email));
	if (ret >= 0)
		*hasEmail = (account->email[0] != '\0');
	return ret;
}

int FbGetUserProfile(const FbAccountId *account, int *hasProfile, FbUserProfile *profile,
		     int index)
{
	int ret;

	*hasProfile = 0;
	if (account->email[0] == '\0') {
		*hasProfile = 0;
		return 0;
	}
	if (profile == NULL)
		return SNSFB_ERR_INVALID_ARGUMENT;
	ret = SceNpSnsFacebook_9A0A4832(index, profile->raw);
	if (ret >= 0)
		*hasProfile = (profile->raw[0] != '\0' && profile->raw[151] != '\0' &&
			       profile->raw[184] != '\0');
	return ret;
}

int FbGetShareEnabled(int *enabled, int index)
{
	*enabled = 0;
	return SceNpSnsFacebook_FA6FBAAE(index, NULL, NULL, NULL, enabled);
}

int FbSetShareEnabled(int enabled, int index)
{
	int val = enabled;

	return SceNpSnsFacebook_18041141(index, NULL, NULL, NULL, &val);
}

int FbInitService(int index)
{
	return SceNpSnsFacebook_FB99FF8E(index);
}

int FbTermService(int index)
{
	return SceNpSnsFacebook_3574800C(index);
}

void FbFormatSingleLine(string &text, bool truncate)
{
	size_t pos;

	while ((pos = text.find("\n")) != string::npos)
		text.replace(pos, 1, " ");
	if (truncate) {
		wstring wtext;

		common::Utf8ToUtf16(text, &wtext, 0);
		if (sce_paf_wcsnlen(wtext.c_str(), 31) > 30) {
			wtext = wtext.substr(0, 30);
			wtext += L"...";
		}
		wstring copy(wtext.c_str());
		string out;

		common::Utf16ToUtf8(copy, &out);
		text = out;
	}
}

int FbIsRetryableError(int err)
{
	switch (err) {
	case SNSFB_ERR_AUTH_09:
	case SNSFB_ERR_AUTH_0A:
	case SNSFB_ERR_SERVER_0B:
	case SNSFB_ERR_FB_13:
	case SNSFB_ERR_FB_14:
	case SNSFB_ERR_FB_15:
	case SNSFB_ERR_FB_16:
	case SNSFB_ERR_FB_17:
	case SNSFB_ERR_FB_18:
		return 1;
	default:
		return 0;
	}
}

int FbConvertError(int err)
{
	if (err < 0) {
		switch (err) {
		case SNSFB_ERR_ALREADY_INITIALIZED:
			return SCE_NP_SNS_FB_ERROR_ALREADY_INITIALIZED;
		case SNSFB_ERR_NOT_INITIALIZED:
			return SCE_NP_SNS_FB_ERROR_NOT_INITIALIZED;
		case SNSFB_ERR_INVALID_ARGUMENT:
			return SCE_NP_SNS_FB_ERROR_INVALID_ARGUMENT;
		case SNSFB_ERR_OUT_OF_MEMORY:
			return SCE_NP_SNS_FB_ERROR_OUT_OF_MEMORY;
		case SNSFB_ERR_EXCEEDS_MAX:
			return SCE_NP_SNS_FB_ERROR_EXCEEDS_MAX;
		case SNSFB_ERR_ABORTED:
			return SCE_NP_SNS_FB_ERROR_ABORTED;
		case SNSFB_ERR_REQUEST_FAILED:
		case SNSFB_ERR_SERVER_08:
		case SNSFB_ERR_SERVER_0B:
		case SNSFB_ERR_FB_13:
		case SNSFB_ERR_FB_14:
		case SNSFB_ERR_FB_15:
		case SNSFB_ERR_FB_16:
		case SNSFB_ERR_FB_17:
		case SNSFB_ERR_FB_18:
			return SCE_NP_SNS_FB_ERROR_SERVER;
		case SNSFB_ERR_AUTH_09:
		case SNSFB_ERR_AUTH_0A:
			return SCE_NP_SNS_FB_ERROR_AUTH;
		case SNSFB_ERR_NOT_SIGNIN:
			return SCE_NP_SNS_FB_ERROR_NOT_SIGNIN;
		case SNSFB_ERR_ACCESS_NOT_ALLOWED:
			return SCE_NP_SNS_FB_ERROR_ACCESS_NOT_ALLOWED;
		case SNSFB_ERR_ACCOUNT_NOT_BOUND:
			return SCE_NP_SNS_FB_ERROR_ACCOUNT_NOT_BOUND;
		case SNSFB_ERR_CONFIG_DISABLED:
			return SCE_NP_SNS_FB_ERROR_CONFIG_DISABLED;
		case SNSFB_ERR_CANCELED_BY_SYSTEM:
			return SCE_NP_SNS_FB_ERROR_CANCELED_BY_SYSTEM;
		case SNSFB_ERR_CHAT_RESTRICTION:
			return SCE_NP_SNS_FB_ERROR_CHAT_RESTRICTION;
		default:
			return err;
		}
	}
	return err;
}
