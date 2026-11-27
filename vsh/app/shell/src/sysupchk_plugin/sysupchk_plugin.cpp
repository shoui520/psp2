/*
 * sysupchk_plugin.cpp: system update check plugin (sysupchk_plugin), its
 * ShellSysupchkParam / ShellSysupchkInitParam loader interface, background
 * check job, updater launcher, and local PUP scanner (0x813623D8..0x8136408B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <common/main_thread_call_list.h>
#include <job/job.h>
#include <thread/thread.h>
#include <versatileparam/idparam.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <system_update/system_update_file.h>
#include <system_update/system_update_version.h>
#include "../../sysupchk.h"
#include "../../plugins.h"

#define SYSUPCHK_ERR_NO_MEMORY		-2145902591	/* 0x80182001 */
#define SYSUPCHK_ERR_ALREADY_STARTED	-2145902588	/* 0x80182004 */
#define SYSUPCHK_ERR_INVALID_ARG	-2145902587	/* 0x80182005 */
#define SYSUPCHK_ERR_INVALID_STATE	-2145902584	/* 0x80182008 */
#define SYSUPCHK_ERR_CANCELED		-2145902583	/* 0x80182009 */

#define SYSUPCHK_DIALOG_BTN_UPDATE	10
#define SYSUPCHK_DIALOG_BTN_CANCEL	11
#define SYSUPCHK_DIALOG_BTN_PART_OK	12
#define SYSUPCHK_DIALOG_BTN_PART_CANCEL	13

#define SYSUPCHK_MSG_REQUIRED_VER	1802228963
#define SYSUPCHK_MSG_CURRENT_VER	-495324263
#define SYSUPCHK_MSG_UPDATE_AVAILABLE	-387398702
#define SYSUPCHK_MSG_MUST_UPDATE	1702383983

#define SYSUPCHK_BTN_UPDATE_LABEL	1814789746
#define SYSUPCHK_BTN_UPDATE_STYLE	541143668
#define SYSUPCHK_BTN_CANCEL_LABEL	-1379034401
#define SYSUPCHK_BTN_CANCEL_STYLE	-1048741504
#define SYSUPCHK_DIALOG_SOUND_ID	-522585832

enum SysupchkState {
	SYSUPCHK_STATE_IDLE	= 0,
	SYSUPCHK_STATE_INIT	= 1,
	SYSUPCHK_STATE_CHECKING	= 2,
	SYSUPCHK_STATE_DIALOG	= 3,
	SYSUPCHK_STATE_DONE	= 4,
	SYSUPCHK_STATE_STOPPED	= 5
};

struct ShellSysupchkInterface {
	paf::wstring (*getVersionString)(void);
	int (*checkRequiredVersion)(SceUInt32 requiredVersion, int *outNeedsUpdate);
	int (*checkLocalPup)(SceUInt32 requiredVersion);
};

struct SysupchkPupVersionInfo {
	SceUInt32 headerReserved[2];
	SceUInt32 version;
	SceUInt32 bodyReserved[9];
};

struct SceKernelSwVersionInfo {
	SceSize size;
	char versionString[28];
	SceUInt32 version;
	SceUInt32 versionExtra;
};

extern "C" {
extern int sceUpdaterGetPupInformation(const char *fileName, void *information);
extern int sceUpdaterNetworkPreCheckVersion(SceUInt32 version, int *result);
}

static inline int updaterCoreGetPupVersion(const char *pupPath, SysupchkPupVersionInfo *outInfo)
{
	return sceUpdaterGetPupInformation(pupPath, outInfo);
}

static inline int updaterCoreCheckRequiredVersion(SceUInt32 requiredVersion, int *outNeedsUpdate)
{
	return sceUpdaterNetworkPreCheckVersion(requiredVersion, outNeedsUpdate);
}

extern "C" {
extern int sceKernelGetSystemSwVersion(SceKernelSwVersionInfo *outInfo);
extern int sceAppMgrLaunchAppByPath4(const char *path, const char *cmd,
				     SceUInt32 flags, const char *param,
				     void *optParam, void *reserved);
extern int sceAppMgrLaunchAppByUri(SceUInt32 flags, const char *uri);
extern int SceNpManager_D157DC12(int reqType);
extern int SceNpManager_0F6AC47E(int reqHandle, int checkLevel, int cacheOpt,
				 int downloadOpt, int *outNeedsUpdate);
extern int SceNpManager_80036A44(int reqHandle);
extern int SceNpManager_29C9B8A5(int reqHandle, int flags);
}

static paf::wstring sysupchkDefaultGetVersionString(void);
static int sysupchkDefaultCheckVersion(SceUInt32 requiredVersion, int *outNeedsUpdate);
static int sysupchkDefaultCheckLocalPup(SceUInt32 requiredVersion);

/* 0x8154753C: default shell interface for version/PUP queries */
ShellSysupchkInterface shellSysupchkDefaultInterface = {
	sysupchkDefaultGetVersionString,
	sysupchkDefaultCheckVersion,
	sysupchkDefaultCheckLocalPup,
};

/* 0x81547534: active shell interface pointer */
ShellSysupchkInterface *shellSysupchkInterface = &shellSysupchkDefaultInterface;

/* Forward declarations of plugin lifecycle callbacks */
static void sysupchkOnPluginInit(paf::Plugin *plugin);
static void sysupchkOnPluginStart(paf::Plugin *plugin);
static void sysupchkOnPluginStop(paf::Plugin *plugin);
static void sysupchkOnPluginDestroy(paf::Plugin *plugin);

/* 0x813623D8 */
ShellSysupchkParam::ShellSysupchkParam()
	: callback(NULL),
	  reserved(NULL),
	  arg(NULL),
	  checkMode(0),
	  dialogMode(0),
	  launchMode(1),
	  launchApiType(0),
	  acceptedFlag(0),
	  requiredVersion(0),
	  npCacheOpt(-1),
	  npDownloadOpt(0),
	  npCheckLevel(3),
	  msgDlgParam(NULL)
{
}

/* 0x81362410 */
ShellSysupchkInitParam::ShellSysupchkInitParam()
{
	m_initParam.name = "sysupchk_plugin";
	m_initParam.resource_file = "vs0:data/external/cdlg/sysupchk_plugin.rco";
	m_initParam.init_func = sysupchkOnPluginInit;
	m_initParam.start_func = sysupchkOnPluginStart;
	m_initParam.stop_func = sysupchkOnPluginStop;
	m_initParam.exit_func = sysupchkOnPluginDestroy;
}

ShellSysupchkInitParam::~ShellSysupchkInitParam()
{
}

/* 0x81362476 */
int ShellSysupchkInitParam::SetParam(const ShellSysupchkParam &inParam)
{
	m_initParam.caller_name = inParam.caller;
	m_initParam.notify_callback = (paf::Plugin::NotifyCallback)inParam.callback;
	m_initParam.unload_callback = (paf::Plugin::UnloadCallback)inParam.reserved;
	m_initParam.param = inParam.arg;

	m_initParam.arguments["check_mode"] = paf::Plugin::Argument(inParam.checkMode);
	m_initParam.arguments["dialog_mode"] = paf::Plugin::Argument(inParam.dialogMode);
	m_initParam.arguments["launch_mode"] = paf::Plugin::Argument(inParam.launchMode);
	m_initParam.arguments["launch_api_type"] = paf::Plugin::Argument(inParam.launchApiType);
	m_initParam.arguments["accepted_flag"] = paf::Plugin::Argument(inParam.acceptedFlag);
	m_initParam.arguments["required_version"] = paf::Plugin::Argument((int)inParam.requiredVersion);

	paf::Plugin::Argument reqVerStrArg;
	reqVerStrArg.type = paf::Plugin::ArgumentType_String;
	reqVerStrArg.str = inParam.requiredVersionString;
	m_initParam.arguments["required_version_string"] = reqVerStrArg;

	m_initParam.arguments["np_cache_opt"] = paf::Plugin::Argument(inParam.npDownloadOpt);
	m_initParam.arguments["np_download_opt"] = paf::Plugin::Argument(inParam.npCheckLevel);
	m_initParam.arguments["np_check_level"] = paf::Plugin::Argument(inParam.npCacheOpt);

	int drawPriority = (inParam.msgDlgParam != NULL) ? inParam.msgDlgParam->priority : -1;
	m_initParam.arguments["msg_dlg_param_draw_priority"] = paf::Plugin::Argument(drawPriority);
	return 0;
}

/* 0x81362AA0 */
int ShellSysupchkInitParam::Load(paf::Plugin::LoadFinishCallback callback, int option)
{
	paf::Plugin::LoadAsync(m_initParam, callback, option);
	return 0;
}

/* 0x81362AA8: read plugin arguments back into ShellSysupchkParam */
static int sysupchkParsePluginArgs(paf::Plugin *plugin, ShellSysupchkParam *outParam,
				   CommonGuiLayoutDialogParam **outDlgParam)
{
	outParam->checkMode = (int)plugin->GetArg("check_mode", 0);
	outParam->dialogMode = (int)plugin->GetArg("dialog_mode", 0);
	outParam->launchMode = (int)plugin->GetArg("launch_mode", 0);
	outParam->launchApiType = (int)plugin->GetArg("launch_api_type", 0);
	outParam->acceptedFlag = (int)plugin->GetArg("accepted_flag", 0);
	outParam->requiredVersion = (SceUInt32)(int)plugin->GetArg("required_version", 0);
	outParam->requiredVersionString = (paf::string)plugin->GetArg("required_version_string", 0);
	outParam->npDownloadOpt = (int)plugin->GetArg("np_cache_opt", 0);
	outParam->npCheckLevel = (int)plugin->GetArg("np_download_opt", 0);
	outParam->npCacheOpt = (int)plugin->GetArg("np_check_level", 0);

	int drawPriority = (int)plugin->GetArg("msg_dlg_param_draw_priority", 0);
	if (drawPriority != -1) {
		CommonGuiLayoutDialogParam *dlg = new CommonGuiLayoutDialogParam();
		*outDlgParam = dlg;
		if (dlg == NULL)
			return SYSUPCHK_ERR_NO_MEMORY;
		dlg->priority = drawPriority;
	}
	return 0;
}

/* 0x81363420: launch Settings updater screen or standalone updater app */
static int sysupchkLaunchUpdaterUri(int launchApiType, const char *target)
{
	if (launchApiType == 1) {
		return sceAppMgrLaunchAppByPath4("vs0:app/NPXS10015/eboot.bin",
						 "NPXS10016", 0x02000000U,
						 target, NULL, NULL);
	}
	paf::string uri("settings_dlg:");
	uri += target;
	return sceAppMgrLaunchAppByUri(0x00050000U, uri.c_str());
}

/* 0x8136350E: select updater destination based on launchMode and available PUP */
static int sysupchkLaunchUpdater(const ShellSysupchkParam *param)
{
	if (param->launchMode == 0)
		return 0;
	if (param->launchMode < 0 || param->launchMode > 3)
		return SYSUPCHK_ERR_INVALID_ARG;

	if (param->launchMode > 1) {
		if (shellSysupchkInterface == NULL ||
		    shellSysupchkInterface->checkLocalPup == NULL) {
			return SYSUPCHK_ERR_INVALID_ARG;
		}
		int sourceType = shellSysupchkInterface->checkLocalPup(param->requiredVersion);
		if (sourceType != 0) {
			if (sourceType == 3)
				return sysupchkLaunchUpdaterUri(param->launchApiType, "updater::via_mc");
			return sysupchkLaunchUpdaterUri(param->launchApiType, "updater::via_game");
		}
	}
	return sysupchkLaunchUpdaterUri(param->launchApiType, "updater");
}

/* Per-check controller managing parameters, NP request handle, and prompt dialog (112B) */
class SysupchkController {
public:
	explicit SysupchkController(paf::Plugin *plugin);
	~SysupchkController();

	int Initialize();
	void CloseDialog();
	int ExecuteCheck(int *outNeedsUpdate);
	int BuildDialogMessage();
	int OpenDialog();
	int HandleCheckCompleted(int needsUpdate);
	void OnDialogButton(SceInt32 value, SceInt32 button);
	int Abort();

	paf::Plugin *GetPlugin() const { return m_plugin; }

private:
	bool m_aborted;
	ShellSysupchkParam m_param;
	paf::Plugin *m_plugin;
	paf::wstring m_message;
	CommonGuiLayoutDialogParam *m_customDlgParam;
	int m_npReqHandle;
	int m_dialogId;
	int m_notifyFlags;
};

class SysupchkDialogHandler : public CommonGuiDialogHandler {
public:
	explicit SysupchkDialogHandler(SysupchkController *controller)
		: m_controller(controller)
	{
	}

	virtual ~SysupchkDialogHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_controller != NULL)
			m_controller->OnDialogButton(value, button);
	}

private:
	SysupchkController *m_controller;
};

/* 0x8136358C */
SysupchkController::SysupchkController(paf::Plugin *plugin)
	: m_aborted(false),
	  m_plugin(plugin),
	  m_customDlgParam(NULL),
	  m_npReqHandle(0),
	  m_dialogId(-1),
	  m_notifyFlags(0)
{
}

/* 0x813635BA */
SysupchkController::~SysupchkController()
{
	if (m_npReqHandle > 0) {
		SceNpManager_80036A44(m_npReqHandle);
		m_npReqHandle = 0;
	}
}

/* 0x81363620 */
void SysupchkController::OnDialogButton(SceInt32 value, SceInt32 button)
{
	(void)value;
	if (button == SYSUPCHK_DIALOG_BTN_UPDATE) {
		m_notifyFlags |= 0x10;
		bool keepOpen = (m_param.acceptedFlag == 1);
		sysupchkLaunchUpdater(&m_param);
		if (keepOpen)
			return;
	} else if (button == SYSUPCHK_DIALOG_BTN_CANCEL) {
		m_notifyFlags |= 0x20;
	} else {
		m_notifyFlags = button;
	}

	if (m_dialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	m_plugin->Notify(m_notifyFlags);
}

/* 0x81363672 */
int SysupchkController::Initialize()
{
	int err = sysupchkParsePluginArgs(m_plugin, &m_param, &m_customDlgParam);
	if (err < 0)
		return err;

	if (m_param.checkMode == 2) {
		int handle = SceNpManager_D157DC12(3);
		if (handle < 0)
			return handle;
		m_npReqHandle = handle;
	}
	return 0;
}

/* 0x813636AA */
void SysupchkController::CloseDialog()
{
	if (m_dialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	if (m_customDlgParam != NULL) {
		delete m_customDlgParam;
		m_customDlgParam = NULL;
	}
}

/* 0x81363728 */
int SysupchkController::ExecuteCheck(int *outNeedsUpdate)
{
	if (m_aborted)
		return SYSUPCHK_ERR_CANCELED;

	int err = 0;
	if (m_param.checkMode == 1) {
		*outNeedsUpdate = 1;
	} else if (m_param.checkMode == 2) {
		int status = 0;
		err = SceNpManager_0F6AC47E(m_npReqHandle, m_param.npCacheOpt,
					    m_param.npDownloadOpt, m_param.npCheckLevel,
					    &status);
		*outNeedsUpdate = status;
		if (err < 0)
			return err;
	} else if (m_param.checkMode == 3) {
		if (shellSysupchkInterface == NULL ||
		    shellSysupchkInterface->checkRequiredVersion == NULL) {
			return SYSUPCHK_ERR_INVALID_ARG;
		}
		err = shellSysupchkInterface->checkRequiredVersion(m_param.requiredVersion,
								   outNeedsUpdate);
		if (err < 0)
			return err;
	} else {
		return SYSUPCHK_ERR_INVALID_ARG;
	}

	if (*outNeedsUpdate == 1)
		m_notifyFlags |= 1;
	else if (*outNeedsUpdate == -2)
		m_notifyFlags |= 3;
	else
		m_notifyFlags |= 2;

	return 0;
}

/* 0x813637EE */
int SysupchkController::BuildDialogMessage()
{
	m_message.clear();

	if (m_param.dialogMode == 1)
		return 0;

	if (m_param.dialogMode == 2) {
		paf::IDParam reqId("", (uint32_t)SYSUPCHK_MSG_REQUIRED_VER);
		const wchar_t *reqFmt = m_plugin->GetString(reqId);
		paf::wstring reqVerWide;
		paf::common::Utf8ToUtf16(m_param.requiredVersionString.c_str(), &reqVerWide);

		m_message = shellFormatText(reqFmt, reqVerWide.c_str());
		m_message += L"\n";
		m_message += L"\n";

		paf::IDParam curId("", (uint32_t)SYSUPCHK_MSG_CURRENT_VER);
		m_message += m_plugin->GetString(curId);
		m_message += L" ";

		if (shellSysupchkInterface != NULL &&
		    shellSysupchkInterface->getVersionString != NULL) {
			paf::wstring currentVer = shellSysupchkInterface->getVersionString();
			m_message += currentVer.c_str();
		} else {
			SceKernelSwVersionInfo swInfo;
			swInfo.size = sizeof(swInfo);
			paf::string verStr;
			if (sceKernelGetSystemSwVersion(&swInfo) == 0)
				verStr = swInfo.versionString;
			paf::wstring verWide;
			paf::common::Utf8ToUtf16(verStr.c_str(), &verWide);
			m_message += verWide.c_str();
		}
		return 0;
	}

	if (m_param.dialogMode == 3) {
		paf::IDParam msgId("", (uint32_t)SYSUPCHK_MSG_UPDATE_AVAILABLE);
		const wchar_t *text = m_plugin->GetString(msgId);
		if (text != NULL)
			m_message = text;
		return 0;
	}

	if (m_param.dialogMode == 4) {
		paf::IDParam msgId("", (uint32_t)SYSUPCHK_MSG_MUST_UPDATE);
		const wchar_t *text = m_plugin->GetString(msgId);
		if (text != NULL)
			m_message = text;
		return 0;
	}

	return SYSUPCHK_ERR_INVALID_ARG;
}

/* 0x81363BBA */
int SysupchkController::OpenDialog()
{
	CommonGuiDialogButton buttons[3] = {
		{ SYSUPCHK_DIALOG_BTN_PART_OK, (SceUInt32)SYSUPCHK_BTN_UPDATE_LABEL,
		  SYSUPCHK_DIALOG_BTN_UPDATE, (SceUInt32)SYSUPCHK_BTN_UPDATE_STYLE },
		{ SYSUPCHK_DIALOG_BTN_PART_CANCEL, (SceUInt32)SYSUPCHK_BTN_CANCEL_LABEL,
		  SYSUPCHK_DIALOG_BTN_CANCEL, (SceUInt32)SYSUPCHK_BTN_CANCEL_STYLE },
		{ 0, 0, 0, 0 }
	};

	int err = BuildDialogMessage();
	if (err < 0)
		return err;

	SysupchkDialogHandler *handler = new SysupchkDialogHandler(this);
	if (handler == NULL)
		return SYSUPCHK_ERR_NO_MEMORY;

	if (m_customDlgParam != NULL) {
		m_customDlgParam->layout.page = SceCommonGuiDialog_6CD802FC.page;
		m_customDlgParam->layout.parts = SceCommonGuiDialog_6CD802FC.parts;
		m_customDlgParam->layout.buttons = buttons;
		m_customDlgParam->resourcePlugin = m_plugin;
		m_customDlgParam->text = m_message;
		m_customDlgParam->handler = handler;
		m_customDlgParam->soundId = (SceUInt32)SYSUPCHK_DIALOG_SOUND_ID;
		m_dialogId = SceCommonGuiDialog_CCFCD9A6(m_customDlgParam);
	} else {
		CommonGuiLayoutDialogParam localParam;
		localParam.layout.page = SceCommonGuiDialog_6CD802FC.page;
		localParam.layout.parts = SceCommonGuiDialog_6CD802FC.parts;
		localParam.layout.buttons = buttons;
		localParam.resourcePlugin = m_plugin;
		localParam.text = m_message;
		localParam.handler = handler;
		localParam.soundId = (SceUInt32)SYSUPCHK_DIALOG_SOUND_ID;
		m_dialogId = SceCommonGuiDialog_CCFCD9A6(&localParam);
	}

	return 0;
}

/* 0x81363D62 */
int SysupchkController::HandleCheckCompleted(int needsUpdate)
{
	if (needsUpdate != 1 || m_param.dialogMode == 1) {
		m_plugin->Notify(m_notifyFlags);
		return 0;
	}
	int err = OpenDialog();
	return (err >= 0) ? 0 : err;
}

/* 0x81363D86 */
int SysupchkController::Abort()
{
	m_aborted = true;
	if (m_npReqHandle != 0)
		return SceNpManager_29C9B8A5(m_npReqHandle, 0);
	return 0;
}

/* Background job running ExecuteCheck (0x81445148..0x814451C0, 44 bytes) */
class SceSysupChkJob : public paf::job::JobItem {
public:
	explicit SceSysupChkJob(SysupchkController *controller)
		: paf::job::JobItem("SceSysupChkJob", NULL),
		  m_needsUpdate(0),
		  m_error(0),
		  m_canceled(false),
		  m_finished(false),
		  m_running(false),
		  m_controller(controller)
	{
	}

	virtual ~SceSysupChkJob() {}

	virtual int32_t Run()
	{
		m_running = true;
		if (m_canceled)
			return SYSUPCHK_ERR_CANCELED;
		if (m_controller == NULL)
			return SYSUPCHK_ERR_INVALID_STATE;
		int err = m_controller->ExecuteCheck(&m_needsUpdate);
		return (err >= 0) ? 0 : err;
	}

	virtual void Finish(int32_t result)
	{
		m_error = result;
		m_controller = NULL;
		m_finished = true;
	}

	void Cancel()
	{
		if (!m_canceled) {
			m_canceled = true;
			if (!m_running) {
				m_error = SYSUPCHK_ERR_CANCELED;
				m_finished = true;
			}
			if (m_controller != NULL)
				m_controller->Abort();
		}
	}

	bool IsFinished() const { return m_finished; }
	int GetNeedsUpdate() const { return m_needsUpdate; }
	int GetError() const { return m_error; }

private:
	int m_needsUpdate;
	int m_error;
	bool m_canceled;
	bool m_finished;
	bool m_running;
	SysupchkController *m_controller;
};

/* Global active plugin session state (0x81547538, 24 bytes) */
struct SysupchkPluginState {
	SysupchkPluginState()
		: controller(NULL),
		  registeredMainThread(false),
		  state(SYSUPCHK_STATE_IDLE),
		  stopRequested(false)
	{
	}

	SysupchkController *controller;
	paf::common::SharedPtr<SceSysupChkJob> job;
	bool registeredMainThread;
	int state;
	bool stopRequested;
};

static SysupchkPluginState *s_sysupchkState = NULL;

/* 0x81363330: main-thread state machine polling job completion */
static void sysupchkMainThreadUpdate(void *userArg)
{
	SysupchkPluginState *ctx = (SysupchkPluginState *)userArg;
	if (ctx == NULL)
		return;

	switch (ctx->state) {
	case SYSUPCHK_STATE_INIT:
	case SYSUPCHK_STATE_DIALOG:
		if (ctx->stopRequested)
			ctx->state = SYSUPCHK_STATE_DONE;
		break;

	case SYSUPCHK_STATE_CHECKING:
		if (ctx->stopRequested || ctx->controller == NULL) {
			ctx->state = SYSUPCHK_STATE_DONE;
		} else if (ctx->job.get() == NULL) {
			ctx->controller->GetPlugin()->Notify(SYSUPCHK_ERR_INVALID_STATE);
			ctx->state = SYSUPCHK_STATE_DONE;
		} else if (ctx->job->IsFinished()) {
			int needsUpdate = ctx->job->GetNeedsUpdate();
			int jobErr = ctx->job->GetError();
			ctx->job.reset();
			if (jobErr != 0) {
				ctx->controller->GetPlugin()->Notify(jobErr);
				ctx->state = SYSUPCHK_STATE_DONE;
			} else {
				int err = ctx->controller->HandleCheckCompleted(needsUpdate);
				if (err >= 0) {
					ctx->state = SYSUPCHK_STATE_DIALOG;
				} else {
					ctx->controller->GetPlugin()->Notify(err);
					ctx->state = SYSUPCHK_STATE_DONE;
				}
			}
		}
		break;

	default:
		break;
	}
}

/* 0x81362E0C */
static int sysupchkStart(paf::Plugin *plugin)
{
	if (s_sysupchkState != NULL)
		return SYSUPCHK_ERR_ALREADY_STARTED;

	s_sysupchkState = new SysupchkPluginState();
	if (s_sysupchkState == NULL)
		return SYSUPCHK_ERR_NO_MEMORY;

	s_sysupchkState->registeredMainThread = false;
	s_sysupchkState->state = SYSUPCHK_STATE_INIT;
	s_sysupchkState->stopRequested = false;

	SysupchkController *controller = new SysupchkController(plugin);
	s_sysupchkState->controller = controller;
	if (controller == NULL)
		return SYSUPCHK_ERR_NO_MEMORY;

	int err = controller->Initialize();
	if (err < 0)
		return err;

	if (paf::job::JobQueue::default_queue == NULL)
		return SYSUPCHK_ERR_INVALID_STATE;

	paf::common::SharedPtr<SceSysupChkJob> job(new SceSysupChkJob(controller));
	s_sysupchkState->job = job;
	if (s_sysupchkState->job.get() == NULL)
		return SYSUPCHK_ERR_NO_MEMORY;

	paf::common::SharedPtr<paf::job::JobItem> baseJob(s_sysupchkState->job.get());
	err = paf::job::JobQueue::default_queue->Enqueue(baseJob);
	if (err != 0)
		return err;

	paf::common::MainThreadCallList::Register(sysupchkMainThreadUpdate, s_sysupchkState);
	s_sysupchkState->state = SYSUPCHK_STATE_CHECKING;
	s_sysupchkState->registeredMainThread = true;
	return 0;
}

static void sysupchkOnPluginInit(paf::Plugin *plugin)
{
	(void)plugin;
}

/* 0x8136313A */
static void sysupchkOnPluginStart(paf::Plugin *plugin)
{
	if (plugin != NULL) {
		int err = sysupchkStart(plugin);
		if (err != 0)
			plugin->Notify(err);
	}
}

/* 0x81363154 */
static void sysupchkOnPluginStop(paf::Plugin *plugin)
{
	if (plugin == NULL || s_sysupchkState == NULL)
		return;

	s_sysupchkState->stopRequested = true;
	if (s_sysupchkState->controller != NULL) {
		s_sysupchkState->controller->Abort();
		s_sysupchkState->controller->CloseDialog();
	}
	if (s_sysupchkState->job.get() != NULL) {
		if (s_sysupchkState->job->IsFinished())
			s_sysupchkState->job.reset();
		else
			s_sysupchkState->job->Cancel();
	}
}

/* 0x81363214 */
static void sysupchkOnPluginDestroy(paf::Plugin *plugin)
{
	if (plugin == NULL || s_sysupchkState == NULL)
		return;

	while (s_sysupchkState->job.get() != NULL && !s_sysupchkState->job->IsFinished())
		paf::thread::Sleep(1);

	s_sysupchkState->job.reset();

	if (s_sysupchkState->controller != NULL) {
		delete s_sysupchkState->controller;
		s_sysupchkState->controller = NULL;
	}

	if (s_sysupchkState->registeredMainThread) {
		paf::common::MainThreadCallList::Unregister(sysupchkMainThreadUpdate,
							    s_sysupchkState);
		paf::thread::Sleep(2);
	}

	s_sysupchkState->registeredMainThread = false;
	s_sysupchkState->state = SYSUPCHK_STATE_STOPPED;
	delete s_sysupchkState;
	s_sysupchkState = NULL;
}

/* 0x81363DA0 */
static int sysupchkDefaultCheckVersion(SceUInt32 requiredVersion, int *outNeedsUpdate)
{
	if (outNeedsUpdate != NULL) {
		int updateFlag = 0;
		if (updaterCoreCheckRequiredVersion(requiredVersion, &updateFlag) >= 0)
			*outNeedsUpdate = (updateFlag != 0) ? 1 : 0;
	}
	return 0;
}

int shellSystemVersionCheck(SceUInt32 requiredVersion, int *outNeedsUpdate)
{
	return sysupchkDefaultCheckVersion(requiredVersion, outNeedsUpdate);
}

/* 0x81363DF2 */
static paf::wstring sysupchkDefaultGetVersionString(void)
{
	return Bgdl_8113614A();
}

/* 0x81363DFA: check downloaded PUP on ud0:/PSP2UPDATE */
static int sysupchkCheckUd0Pup(SceUInt32 requiredVersion, SceUInt32 *outVersion)
{
	paf::string pupInfoPath("ud0:/PSP2UPDATE");
	pupInfoPath += "/";
	pupInfoPath += "PupInfo";

	SystemUpdateCtrl ctrl(0, pupInfoPath.c_str(), "ud0:/PSP2UPDATE/PSP2UPDAT.PUP");
	SystemUpdateInfo updateInfo;
	SystemUpdateResult readRes = ctrl.Bgdl_81131B5E(&updateInfo, true);
	if (readRes.error != 0 || updateInfo.state != 2)
		return 0;

	SysupchkPupVersionInfo pupInfo;
	pupInfo.version = 0;
	*outVersion = 0;
	if (updaterCoreGetPupVersion("ud0:/PSP2UPDATE/PSP2UPDAT.PUP", &pupInfo) == 0 &&
	    pupInfo.version >= requiredVersion) {
		*outVersion = pupInfo.version;
		return 1;
	}
	return 0;
}

/* 0x81363FE8: locate highest-version local PUP across game card, memory card, and ud0 */
static int sysupchkDefaultCheckLocalPup(SceUInt32 requiredVersion)
{
	SceUInt32 gamePupVer = 0;
	SceUInt32 mcPupVer = 0;
	SceUInt32 ud0PupVer = 0;

	SysupchkPupVersionInfo gamePupInfo;
	gamePupInfo.version = 0;
	if (updaterCoreGetPupVersion("gro0:/PSP2/UPDATE/PSP2UPDAT.PUP", &gamePupInfo) == 0 &&
	    gamePupInfo.version >= requiredVersion) {
		gamePupVer = gamePupInfo.version;
	}

	SysupchkPupVersionInfo mcPupInfo;
	mcPupInfo.version = 0;
	if (updaterCoreGetPupVersion("ux0:/data/PSP2/UPDATE/PSP2UPDAT.PUP", &mcPupInfo) == 0 &&
	    mcPupInfo.version >= requiredVersion) {
		mcPupVer = mcPupInfo.version;
	}

	sysupchkCheckUd0Pup(requiredVersion, &ud0PupVer);

	if ((gamePupVer | mcPupVer | ud0PupVer) == 0)
		return 0;
	if (gamePupVer > mcPupVer)
		return (ud0PupVer >= gamePupVer) ? 1 : 2;
	return (ud0PupVer >= mcPupVer) ? 1 : 3;
}
