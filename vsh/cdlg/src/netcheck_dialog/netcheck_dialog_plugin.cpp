/*
 * SceNetCheckDialogPlugin (vs0:sys/external/libcdlg_netcheck.suprx):
 * plugin entry points, CommonDialogMain work-memory bridge, SceNetCheckJob,
 * and SceNetCheckDialogCtrl.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <netcheck_dialog.h>
#include <std/string.h>
#include <std/stdio.h>
#include <math/ngp/v4.h>
#include <framework.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "netcheck_dialog_plugin.h"

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

extern "C" {
int sceNetCtlInit(void);
void sceNetCtlTerm(void);
}

static SceNetCheckDialogCtrl *s_netCheckCtrl = NULL;

static void netCheckPluginSetParam(Plugin::InitParam2 *param);
static void netCheckPluginInit(Plugin *plugin);
static void netCheckPluginStart(Plugin *plugin);
static void netCheckPluginStop(Plugin *plugin);
static void netCheckPluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	netCheckPluginSetParam,
	netCheckPluginInit,
	netCheckPluginStart,
	netCheckPluginStop,
	netCheckPluginExit
};

class NetCheckApiScope {
public:
	NetCheckApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_NETCHECK);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogNetCheckParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NETCHECK);
		if (m_work == NULL) {
			err = -1;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NETCHECK);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~NetCheckApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_NETCHECK);
		}
	}

	CommonDialogNetCheckParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogNetCheckParam *m_work;
	const char *m_apiName;
};

SceNetCheckJob::SceNetCheckJob(SceNetCheckDialogCtrl *controller, SceInt32 mode)
	: job::JobItem("SceNetCheckJob", NULL),
	  m_controller(controller),
	  m_mode(mode)
{
}

SceNetCheckJob::~SceNetCheckJob()
{
}

SceInt32 SceNetCheckJob::Run()
{
	if (IsCanceled())
		return SCE_COMMON_DIALOG_RESULT_ABORTED;

	SceInt32 ret = sceNetCtlInit();
	if (ret < 0)
		return ret;
	sceNetCtlTerm();
	return SCE_OK;
}

void SceNetCheckJob::Finish(SceInt32 result)
{
	if (m_controller != NULL)
		m_controller->OnJobFinished(result);
}

SceNetCheckDialogCtrl::SceNetCheckDialogCtrl(Plugin *plugin)
	: m_plugin(plugin),
	  m_jobQueue(NULL),
	  m_impsMutex("SceNpImpsUtilitySystemForNetCheckMutex", 0),
	  m_dialogResult(SCE_COMMON_DIALOG_RESULT_OK),
	  m_resultCode(SCE_OK),
	  m_finished(false),
	  m_aborted(false),
	  m_signInLoaded(false),
	  m_patchChkLoaded(false)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_ps3Info, 0, sizeof(m_ps3Info));
}

SceNetCheckDialogCtrl::~SceNetCheckDialogCtrl()
{
	Stop();
}

const char *SceNetCheckDialogCtrl::ModeName(SceInt32 mode)
{
	switch (mode) {
	case SCE_NETCHECK_DIALOG_MODE_ADHOC_CONN:
		return "AdhocMode";
	case SCE_NETCHECK_DIALOG_MODE_PSN:
		return "PSNMode";
	case SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE:
		return "PSNOnlineMode";
	case SCE_NETCHECK_DIALOG_MODE_PS3_CONNECT:
		return "PS3ConnMode";
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN:
		return "PSPNETAdhocMode";
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CREATE:
		return "PSPNETAdhocCreateMode";
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_JOIN:
		return "PSPNETAdhocJoinMode";
	default:
		return "PSNMode";
	}
}

void SceNetCheckDialogCtrl::ApplyBackground(const CommonDialogNetCheckParam *work,
					    bool useAgeRestriction)
{
	(void)SceCommonDialogMain_C62B845D();
	SceCommonDialogMain_877BB562();

	math::v4 color(0.29f, 0.286f, 0.337f, 1.0f);
	if (work != NULL) {
		if (work->param.mode == SCE_NETCHECK_DIALOG_MODE_PSN ||
		    work->param.mode == SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE) {
			Framework *fw = Framework::Instance();
			if ((fw != NULL && fw->GetMode() == 5) || useAgeRestriction)
				color = math::v4(0.0f, 0.215f, 0.568f, 1.0f);
		} else if (work->param.commonParam.bgColor != NULL) {
			const SceCommonDialogColor *bg = work->param.commonParam.bgColor;
			color = math::v4((float)bg->r / 255.0f,
					 (float)bg->g / 255.0f,
					 (float)bg->b / 255.0f,
					 (float)bg->a / 255.0f);
		}
	}
	(void)color;
}

SceInt32 SceNetCheckDialogCtrl::Start(const CommonDialogNetCheckParam *work)
{
	if (work != NULL)
		m_param = work->param;

	job::JobQueue::Option queueOpt;
	queueOpt.priority = 120;
	queueOpt.stackSize = 0x4000;
	queueOpt.numThreads = 1;
	m_jobQueue = new job::JobQueue("SceNetCheckJobQueue", &queueOpt);
	if (m_jobQueue == NULL)
		return -1;

	SceNetCheckJob *job = new SceNetCheckJob(this, (SceInt32)m_param.mode);
	if (job == NULL)
		return -1;
	common::SharedPtr<job::JobItem> item(job);
	m_jobQueue->Enqueue(item);
	return SCE_OK;
}

void SceNetCheckDialogCtrl::Stop()
{
	if (m_jobQueue != NULL) {
		m_jobQueue->CancelAllItems();
		m_jobQueue->WaitEmpty();
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	if (m_signInLoaded) {
		Plugin::UnloadAsync("signin_plugin", NULL, Plugin::LoadUnloadOption_None);
		m_signInLoaded = false;
	}
	if (m_patchChkLoaded) {
		Plugin::UnloadAsync("patch_chk_plugin", NULL, Plugin::LoadUnloadOption_None);
		m_patchChkLoaded = false;
	}
}

void SceNetCheckDialogCtrl::RequestAbort()
{
	m_aborted = true;
	m_dialogResult = SCE_COMMON_DIALOG_RESULT_ABORTED;
	m_finished = true;
}

void SceNetCheckDialogCtrl::OnJobFinished(SceInt32 result)
{
	m_resultCode = result;
	if (m_aborted)
		m_dialogResult = SCE_COMMON_DIALOG_RESULT_ABORTED;
	else if (result >= 0)
		m_dialogResult = SCE_COMMON_DIALOG_RESULT_OK;
	else
		m_dialogResult = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
	m_finished = true;
}

void SceNetCheckDialogCtrl::LaunchSignInPlugin()
{
	if (m_signInLoaded)
		return;
	Plugin::InitParam initParam;
	initParam.name = "signin_plugin";
	initParam.resource_file = "vs0:data/external/cdlg/signin_plugin.rco";
	Plugin::LoadAsync(initParam, NULL, Plugin::LoadUnloadOption_None);
	m_signInLoaded = true;
}

void SceNetCheckDialogCtrl::LaunchPatchCheckPlugin()
{
	if (m_patchChkLoaded)
		return;
	Plugin::InitParam initParam;
	initParam.name = "patch_chk_plugin";
	initParam.resource_file = "vs0:data/external/cdlg/patch_chk_plugin.rco";
	Plugin::LoadAsync(initParam, NULL, Plugin::LoadUnloadOption_None);
	m_patchChkLoaded = true;
}

bool SceNetCheckDialogCtrl::IsFinished() const
{
	return m_finished;
}

SceInt32 SceNetCheckDialogCtrl::GetDialogResult() const
{
	return m_dialogResult;
}

SceInt32 SceNetCheckDialogCtrl::GetResultCode() const
{
	return m_resultCode;
}

const SceNetCheckDialogPS3ConnectInfo &SceNetCheckDialogCtrl::GetPs3ConnectInfo() const
{
	return m_ps3Info;
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	NetCheckApiScope scope("OnUpdate", ret);
	CommonDialogNetCheckParam *work = scope.work();
	if (ret != SCE_OK || work == NULL || s_netCheckCtrl == NULL)
		return;

	sce_paf_memset(&work->result, 0, sizeof(work->result));
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_NETCHECK))
		s_netCheckCtrl->RequestAbort();

	if (s_netCheckCtrl->IsFinished()) {
		work->ps3ConnectInfo = s_netCheckCtrl->GetPs3ConnectInfo();
		work->result.result = s_netCheckCtrl->GetDialogResult();
		work->result.psnModeSucceeded =
			(s_netCheckCtrl->GetResultCode() >= 0) ? SCE_TRUE : SCE_FALSE;
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_NETCHECK);
	}
}

static void netCheckPluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void netCheckPluginInit(Plugin *plugin)
{
	s_netCheckCtrl = new SceNetCheckDialogCtrl(plugin);
}

static void netCheckPluginStart(Plugin *plugin)
{
	(void)plugin;
	if (s_netCheckCtrl == NULL)
		return;

	SceInt32 ret = SCE_OK;
	NetCheckApiScope scope("Start", ret);
	CommonDialogNetCheckParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		s_netCheckCtrl->ApplyBackground(work, work->initArg.useAgeRestriction != 0);
		s_netCheckCtrl->Start(work);
	}
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void netCheckPluginStop(Plugin *plugin)
{
	(void)plugin;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	if (s_netCheckCtrl != NULL)
		s_netCheckCtrl->Stop();
}

static void netCheckPluginExit(Plugin *plugin)
{
	(void)plugin;
	if (s_netCheckCtrl != NULL) {
		delete s_netCheckCtrl;
		s_netCheckCtrl = NULL;
	}
}

static SceInt32 netCheckModuleStart(SceSize arglen, const void *argp)
{
	(void)arglen;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	Module *module = *(Module *const *)argp;
	module->SetInterface(0, &s_moduleInterface);
	return SCE_KERNEL_START_SUCCESS;
}

static SceInt32 netCheckModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	netCheckModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	netCheckModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
