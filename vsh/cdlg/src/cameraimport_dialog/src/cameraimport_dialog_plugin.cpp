/*
 * SceCamImportDialogPlugin (vs0:sys/external/libcdlg_cameraimport.suprx):
 * Sony path: vsh/cdlg/src/cameraimport_dialog/src/cameraimport_dialog_plugin.cpp.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <camera.h>
#include <common_dialog.h>
#include <cameraimport_dialog.h>
#include <sysmodule/sysmodule.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <framework.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <versatileparam/idparam.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"
#include "cameraimport_dialog_plugin.h"
#include "xui_shell_util.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;
using namespace paf::graph;

/* Work memory scope lock matching CommonDialogApiScope */
class CameraImportPluginApiScope {
public:
	CameraImportPluginApiScope(const char *name, int &ret)
		: m_ret(ret), m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_CAMERAIMPORT);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogCameraImportParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_CAMERAIMPORT);
		if (m_work == NULL) {
			ret = CAMERAIMPORT_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_CAMERAIMPORT);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~CameraImportPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_CAMERAIMPORT);
		}
	}

	CommonDialogCameraImportParam *work() const { return m_work; }

private:
	int &m_ret;
	const char *m_name;
	CommonDialogCameraImportParam *m_work;
};

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static CameraImportDialog *s_dialog = NULL;
static Plugin::ModuleInterface s_moduleInterface;

static void cameraImportDialogInit(SceSize argSize, const void *argp);
static void cameraImportDialogTerm(void);
static void cameraImportPluginInit(Plugin *plugin);
static void cameraImportPluginStart(Plugin *plugin);
static void cameraImportPluginStop(Plugin *plugin);
static void cameraImportPluginExit(Plugin *plugin);
static void cameraImportPluginSetParam(Plugin::InitParam2 *param);
static void OnUpdate(void *userData);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	cameraImportDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	(void)argSize;
	(void)argp;
	cameraImportDialogTerm();
	return 0;
}

static __attribute__((noinline)) void cameraImportDialogInit(SceSize argSize, const void *argp)
{
	(void)argSize;
	__dso_handle = &__dso_handle;
	for (void (**init)(void) = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (void (**init)(void) = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = cameraImportPluginInit;
	s_moduleInterface.start_func = cameraImportPluginStart;
	s_moduleInterface.stop_func = cameraImportPluginStop;
	s_moduleInterface.exit_func = cameraImportPluginExit;
	s_moduleInterface.set_param_func = cameraImportPluginSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void cameraImportDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static void CameraImportDialogCallback(const SceCameraImportDialogResult *result, void *userData)
{
	(void)userData;
	int ret;
	CameraImportPluginApiScope scope("CameraImportDialogCallback", ret);

	if (ret != SCE_OK || scope.work() == NULL)
		return;
	if (result != NULL)
		sce_paf_memcpy(&scope.work()->result, result, sizeof(SceCameraImportDialogResult));
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	if (SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CAMERAIMPORT) != 0)
		__builtin_trap();
}

static void OnUpdate(void *userData)
{
	(void)userData;
	int ret;
	CameraImportPluginApiScope scope("OnUpdate", ret);

	if (ret < 0 || SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_CAMERAIMPORT) != 0) {
		SceCameraImportDialogResult abortedResult;

		sce_paf_memset(&abortedResult, 0, sizeof(abortedResult));
		abortedResult.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		if (scope.work() != NULL)
			sce_paf_memcpy(&scope.work()->result, &abortedResult,
				       sizeof(abortedResult));
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		if (SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CAMERAIMPORT) != 0)
			__builtin_trap();
	}
}

static void cameraImportPluginInit(Plugin *plugin)
{
	s_dialog = new CameraImportDialog(plugin);
	if (s_dialog == NULL)
		return;
	{
		int ret;
		CameraImportPluginApiScope scope("Init", ret);
		const SceCameraImportDialogParam *param =
			(scope.work() != NULL) ? &scope.work()->param : NULL;

		s_dialog->SetParam(param, CameraImportDialogCallback, NULL);
	}
	s_dialog->Init(plugin);
}

static void cameraImportPluginStart(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Start(plugin);
	common::MainThreadCallList::Register(OnUpdate, NULL);
}

static void cameraImportPluginStop(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Stop(plugin);
}

static void cameraImportPluginExit(Plugin *plugin)
{
	if (s_dialog != NULL) {
		s_dialog->Exit(plugin);
		delete s_dialog;
		s_dialog = NULL;
	}
}

static void cameraImportPluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

/* CameraImportDialog */
CameraImportDialog::CameraImportDialog(Plugin *plugin)
	: PluginApp(plugin), m_view(plugin), m_finishCb(NULL), m_finishCbArg(NULL),
	  m_errorDialog(), m_initError(0), m_pendingError(0), m_stopping(false),
	  m_surfacePool(NULL), m_surfaceInstancePool()
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_memRawParam, 0, sizeof(m_memRawParam));
	sce_paf_memset(&m_memJpegParam, 0, sizeof(m_memJpegParam));
	sce_paf_memset(&m_overlayImage, 0, sizeof(m_overlayImage));
	sce_paf_memset(&m_bgColor, 0, sizeof(m_bgColor));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

CameraImportDialog::~CameraImportDialog()
{
}

int CameraImportDialog::SetParam(const SceCameraImportDialogParam *param,
				 CameraImportFinishCallback cb, void *userData)
{
	if (param != NULL) {
		sce_paf_memcpy(&m_param, param, sizeof(m_param));
		if (param->commonParam.bgColor != NULL) {
			sce_paf_memcpy(&m_bgColor, param->commonParam.bgColor, sizeof(m_bgColor));
			m_param.commonParam.bgColor = &m_bgColor;
		}
		if (param->memRawParam != NULL) {
			sce_paf_memcpy(&m_memRawParam, param->memRawParam, sizeof(m_memRawParam));
			m_param.memRawParam = &m_memRawParam;
		}
		if (param->memJpegParam != NULL) {
			sce_paf_memcpy(&m_memJpegParam, param->memJpegParam,
				       sizeof(m_memJpegParam));
			m_param.memJpegParam = &m_memJpegParam;
		}
		if (param->overlayImage != NULL) {
			sce_paf_memcpy(&m_overlayImage, param->overlayImage,
				       sizeof(m_overlayImage));
			m_param.overlayImage = &m_overlayImage;
		}
	}
	m_finishCb = cb;
	m_finishCbArg = userData;
	m_view.SetDialogParam(&m_param);
	return 0;
}

int CameraImportDialog::InitSurfacePool()
{
	if (m_param.workingBuffer != NULL &&
	    m_param.workingBufferSize >= SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE) {
		m_surfacePool = new SurfacePool(m_param.workingBuffer,
						SCE_CAMERAIMPORT_DIALOG_WORKING_BUFFER_SIZE,
						"SceCameraCoreSurfacePool", NULL,
						SurfacePool::MemoryType_Normal);
		if (m_surfacePool != NULL)
			m_surfaceInstancePool.Initialize(m_surfacePool, 4, (ImageMode)28);
	}
	int devices[2] = { CAMERA_CORE_DEVICE_FRONT, CAMERA_CORE_DEVICE_REAR };

	for (int i = 0; i < 2; ++i) {
		CameraCoreSetSurfacePool(devices[i], &m_surfaceInstancePool);
		CameraCoreSetConfig(devices[i], 0, 1, 2);
		CameraCoreSetProperty(devices[i], CAMERA_CORE_PROP_NIGHTMODE, 3);
		CameraCoreSetProperty(devices[i], CAMERA_CORE_PROP_BACKLIGHT, 1);
	}
	return 0;
}

int CameraImportDialog::Init(Plugin *plugin)
{
	(void)plugin;
	m_errorDialog.AddListener(this);
	m_view.AddListener(this);

	int modRet;

	if (Framework::Instance() != NULL && Framework::Instance()->GetMode() == 5) {
		int status = 0;
		SysmoduleUserOpt opt = { 16, &status, { 0, 0 } };

		modRet = sceSysmoduleLoadModuleInternalWithArg(
			SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM, 0, NULL, &opt);
	} else {
		modRet = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM);
	}
	if (modRet != 0) {
		m_initError = modRet;
		return 0;
	}
	CameraImportJobQueueInit();
	CreateMotionListener();
	CreateInputListener();
	XuiShellUtilInit();

	int coreRet = CameraCoreInit(this);

	if (coreRet < 0) {
		m_initError = coreRet;
		return 0;
	}
	InitSurfacePool();
	m_view.OnCreate();
	return 0;
}

void CameraImportDialog::OpenErrorDialog(int errorCode)
{
	if (m_plugin == NULL || m_errorDialog.IsOpen() || m_stopping)
		return;
	m_pendingError = errorCode;

	GuiDialogParam param(IDParam("", 0xC51B20F4U));

	param.plugin = m_plugin;
	param.type = errorCode;
	param.defaultButton = 1;
	param.buttonCount = 1;
	param.option.flags = 13;
	if (!m_errorDialog.IsOpen() && m_errorDialog.Open(param) == 0) {
		m_view.SetDialogBlocked();
		DisableInputHandler(&m_view);
	}
}

int CameraImportDialog::Start(Plugin *plugin)
{
	(void)plugin;
	if (m_initError != 0) {
		OpenErrorDialog(m_initError);
	} else {
		StartMotionListener();
		StartInputListener();
		m_view.Open();
	}
	return 0;
}

int CameraImportDialog::Stop(Plugin *plugin)
{
	(void)plugin;
	m_stopping = true;
	m_view.Close();
	StopInputListener();
	StopMotionListener();
	return 0;
}

int CameraImportDialog::Exit(Plugin *plugin)
{
	(void)plugin;
	m_errorDialog.RemoveListener(this);
	m_view.RemoveListener(this);
	m_view.OnDestroy();

	int devices[2] = { CAMERA_CORE_DEVICE_FRONT, CAMERA_CORE_DEVICE_REAR };

	for (int i = 0; i < 2; ++i)
		CameraCoreSetSurfacePool(devices[i], NULL);
	m_surfaceInstancePool.Terminate();
	if (m_surfacePool != NULL) {
		delete m_surfacePool;
		m_surfacePool = NULL;
	}
	CameraCoreTerm();
	XuiShellUtilTerm();
	DestroyInputListener();
	DestroyMotionListener();
	CameraImportJobQueueTerm();

	if (Framework::Instance() != NULL && Framework::Instance()->GetMode() == 5) {
		int status = 0;
		SysmoduleUserOpt opt = { 0, &status, { 0, 0 } };

		sceSysmoduleUnloadModuleInternalWithArg(
			SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM, 0, NULL, &opt);
	} else {
		sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM);
	}
	return 0;
}

void CameraImportDialog::CompleteDialog()
{
	int notifyStatus = 0;

	if (m_result.result != SCE_COMMON_DIALOG_RESULT_OK) {
		SceInt32 savedResult = m_result.result;

		sce_paf_memset(&m_result, 0, sizeof(m_result));
		m_result.result = savedResult;
		if (m_param.memRawParam != NULL && m_param.memRawParam->buffer != NULL &&
		    m_param.memRawParam->bufferSize != 0)
			sce_paf_memset(m_param.memRawParam->buffer, 0,
				       m_param.memRawParam->bufferSize);
		if (m_param.memJpegParam != NULL && m_param.memJpegParam->buffer != NULL &&
		    m_param.memJpegParam->bufferSize != 0)
			sce_paf_memset(m_param.memJpegParam->buffer, 0,
				       m_param.memJpegParam->bufferSize);
		notifyStatus = 1;
	}
	if (m_finishCb != NULL)
		m_finishCb(&m_result, m_finishCbArg);
	Notify(notifyStatus);
}

void CameraImportDialog::HandleViewEvent(int32_t event, void *arg)
{
	switch (event) {
	case CAMERAIMPORT_EVENT_READY:
		m_view.ShowPage();
		break;
	case CAMERAIMPORT_EVENT_CLOSED:
		CompleteDialog();
		break;
	case CAMERAIMPORT_EVENT_CAPTURE_FAILED:
		if (arg != NULL) {
			CameraImportEncodeResult *res = (CameraImportEncodeResult *)arg;

			if (res->result < 0)
				OpenErrorDialog(res->result == -1
							? CAMERAIMPORT_PLUGIN_ERROR_GENERIC
							: res->result);
		}
		break;
	case CAMERAIMPORT_EVENT_ENCODE_DONE:
		if (arg != NULL && ((CameraImportEncodeResult *)arg)->result < 0) {
			int err = ((CameraImportEncodeResult *)arg)->result;

			OpenErrorDialog(err == -1 ? CAMERAIMPORT_PLUGIN_ERROR_GENERIC : err);
		} else {
			m_view.EnterConfirmMode();
		}
		break;
	case CAMERAIMPORT_EVENT_MOTION_ERROR:
		OpenErrorDialog(CAMERAIMPORT_PLUGIN_ERROR_DEVICE_UNAVAILABLE);
		break;
	case CAMERAIMPORT_EVENT_ACCEPT: {
		const SceCameraImportDialogResult *capRes = m_view.GetCaptureResult();

		if (capRes != NULL) {
			sce_paf_memcpy(&m_result, capRes, sizeof(m_result));
			m_result.result = SCE_COMMON_DIALOG_RESULT_OK;
		} else {
			m_result.result = CAMERAIMPORT_PLUGIN_ERROR_GENERIC;
		}
		m_view.HidePage();
		break;
	}
	case CAMERAIMPORT_EVENT_CANCEL:
		m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
		m_view.HidePage();
		break;
	default:
		break;
	}
}

void CameraImportDialog::OnEvent(EventDispatcher *sender, int32_t event, void *arg)
{
	if (sender == &m_view) {
		HandleViewEvent(event, arg);
	} else if (sender == &m_errorDialog) {
		if (event == 0 || event == GUI_DIALOG_EVENT_CLOSED) {
			EnableInputHandler(&m_view);
			m_result.result = m_pendingError;
			CompleteDialog();
		}
	}
}

void CameraImportDialog::OnCameraEvent(int event, int arg, int result)
{
	(void)arg;
	if (event != CAMERA_CORE_EVENT_DEVICE_ERROR || result == 0)
		return;
	int dialogErr = result;

	if (result == SCE_CAMERA_ERROR_ALREADY_START || result == SCE_CAMERA_ERROR_ALREADY_OPEN)
		dialogErr = CAMERAIMPORT_PLUGIN_ERROR_CAMERA_IN_USE;
	else if (result == CAMERA_CORE_ERROR_FORCED_STOP)
		dialogErr = CAMERAIMPORT_PLUGIN_ERROR_DEVICE_UNAVAILABLE;
	OpenErrorDialog(dialogErr);
}
