/*
 * SceNpGameCustomDataDlgImpl (vs0:sys/external/libcdlg_game_custom_data_impl.suprx):
 * plugin entry, CommonDialogMain work-memory bridge, and SceGameCustomDataDialog.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <game_custom_data_dialog.h>
#include <std/string.h>
#include <std/stdio.h>
#include <plugin.h>
#include <module/module.h>
#include "gcd_dialog_impl.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

struct GcdShellLaunchParam {
	const char *cmd;
};

extern "C" {
int SceNpManager_96DF1B76(const char **outCommId);
int SceNpManager_9554802B(const char *serviceName, const char *commIdStr, const char *title);
int sceGameCustomDataDialogAbort(void);
int sceShellUtilRequestLaunchApp(const GcdShellLaunchParam *param);
}

static SceGameCustomDataDialog *s_dialog = NULL;

static void gcdImplSetParam(Plugin::InitParam2 *param);
static void gcdImplInit(Plugin *plugin);
static void gcdImplStart(Plugin *plugin);
static void gcdImplStop(Plugin *plugin);
static void gcdImplExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	gcdImplSetParam,
	gcdImplInit,
	gcdImplStart,
	gcdImplStop,
	gcdImplExit
};

SceGameCustomDataDialog::SceGameCustomDataDialog(Plugin *plugin)
	: m_plugin(plugin),
	  m_core(),
	  m_useMapLock("SceGcdDialogUseMapLock", 0),
	  m_itemInfoLock("SceGcdDialogItemInfoLock", 0),
	  m_iconLock("SceGcdDialogIconLock", 0),
	  m_reqLock("SceGameCustomDataDlgReqLock", 0),
	  m_npCommId(),
	  m_launchUri(),
	  m_dialogStatus(0),
	  m_running(false)
{
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

SceGameCustomDataDialog::~SceGameCustomDataDialog()
{
	Stop();
}

SceInt32 SceGameCustomDataDialog::Start(const CommonDialogGameCustomDataParam *work,
					const char *npCommId)
{
	if (work == NULL)
		return -1;
	if (npCommId != NULL)
		m_npCommId = npCommId;
	m_running = true;
	m_core.Initialize(0, SceGcdCoreImpl::NormalizeNpLanguage(1));
	return SCE_OK;
}

void SceGameCustomDataDialog::Stop()
{
	if (!m_running)
		return;
	m_core.Terminate();
	m_running = false;
}

void SceGameCustomDataDialog::WriteResults(CommonDialogGameCustomDataParam *work)
{
	if (work == NULL)
		return;
	work->result = (SceCommonDialogResult)m_result.result;
}

void SceGameCustomDataDialog::WriteSendData(CommonDialogGameCustomDataParam *work)
{
	if (work == NULL)
		return;
	work->result = (SceCommonDialogResult)m_result.result;
}

const paf::string &SceGameCustomDataDialog::GetLaunchUri() const
{
	return m_launchUri;
}

static void gcdImplSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void gcdImplInit(Plugin *plugin)
{
	s_dialog = new SceGameCustomDataDialog(plugin);
}

static void gcdImplStart(Plugin *plugin)
{
	(void)plugin;
	char commIdBuf[16];
	sce_paf_memset(commIdBuf, 0, sizeof(commIdBuf));
	const char *commIdPtr = NULL;
	SceUInt8 commNum = 0;
	if (SceNpManager_96DF1B76(&commIdPtr) == 0 && commIdPtr != NULL) {
		sce_paf_memcpy(commIdBuf, commIdPtr, 10);
		commNum = (SceUInt8)commIdPtr[10];
	}

	CommonDialogGameCustomDataParam localWork;
	sce_paf_memset(&localWork, 0, sizeof(localWork));
	SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	CommonDialogGameCustomDataParam *work = (CommonDialogGameCustomDataParam *)
		SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	if (work != NULL) {
		sce_paf_strncpy(work->apiName, "getParams", sizeof(work->apiName) - 1);
		work->apiName[sizeof(work->apiName) - 1] = '\0';
		localWork = *work;
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
	}
	SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);

	char commStr[32];
	sce_paf_snprintf(commStr, sizeof(commStr), "%s_%02d", commIdBuf, (unsigned int)commNum);
	SceNpManager_9554802B("NpGameCustomDataDialog", commStr, "NpGameCustomDataDialog");

	if (s_dialog != NULL && s_dialog->Start(&localWork, commStr) != SCE_OK) {
		SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
		CommonDialogGameCustomDataParam *resWork = (CommonDialogGameCustomDataParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
		if (resWork != NULL) {
			sce_paf_strncpy(resWork->apiName, "setDialogResults",
					sizeof(resWork->apiName) - 1);
			resWork->apiName[sizeof(resWork->apiName) - 1] = '\0';
			s_dialog->WriteResults(resWork);
			sce_paf_memset(resWork->apiName, 0, sizeof(resWork->apiName));
		}
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
		sceGameCustomDataDialogAbort();
	}
}

static void gcdImplStop(Plugin *plugin)
{
	(void)plugin;
	SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	CommonDialogGameCustomDataParam *work = (CommonDialogGameCustomDataParam *)
		SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	if (work != NULL) {
		sce_paf_strncpy(work->apiName, "setDialogResults", sizeof(work->apiName) - 1);
		work->apiName[sizeof(work->apiName) - 1] = '\0';
		if (s_dialog != NULL)
			s_dialog->WriteResults(work);
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
	}
	SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
}

static void gcdImplExit(Plugin *plugin)
{
	(void)plugin;
	SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	CommonDialogGameCustomDataParam *work = (CommonDialogGameCustomDataParam *)
		SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);
	if (work != NULL) {
		sce_paf_strncpy(work->apiName, "setSendData", sizeof(work->apiName) - 1);
		work->apiName[sizeof(work->apiName) - 1] = '\0';
		if (s_dialog != NULL)
			s_dialog->WriteSendData(work);
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
	}
	SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA);

	paf::string launchUri;
	if (s_dialog != NULL) {
		launchUri = s_dialog->GetLaunchUri();
		s_dialog->Stop();
		delete s_dialog;
		s_dialog = NULL;
	}

	if (!launchUri.empty()) {
		if (sce_paf_strncmp(launchUri.c_str(), "http:", 5) == 0 ||
		    sce_paf_strncmp(launchUri.c_str(), "https:", 6) == 0) {
			GcdShellLaunchParam launchParam;
			launchParam.cmd = launchUri.c_str();
			sceShellUtilRequestLaunchApp(&launchParam);
		} else {
			sceAppMgrLaunchAppByUri(0x120000u, launchUri.c_str());
		}
	}
}

static void gcdImplRunInitArrays(void)
{
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	gcdImplRunInitArrays();
	Module *module = *(Module *const *)argp;
	module->SetInterface(1, &s_moduleInterface);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}
