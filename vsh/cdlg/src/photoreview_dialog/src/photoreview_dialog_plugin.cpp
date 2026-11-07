/*
 * ScePhotoReviewDialogPlugin (vs0:sys/external/libcdlg_photoreview.suprx): the photo review
 * dialog's plugin, which SceCommonDialogMain loads for libcdlg's scePhotoReviewDialog*. It makes
 * the dialog (PhotoReviewDialog) with the caller's parameter and colours from the work memory
 * (CommonDialogPhotoReviewParam), and puts the dialog's result there when the dialog ends, or
 * when the caller aborts. The names the plugin writes into the work memory (GetReviewParam,
 * GetCommonParam, PhotoReviewDialogCallback, OnUpdate) are Sony's function names, as is the
 * file's (vsh/cdlg/src/photoreview_dialog/src/photoreview_dialog_plugin.cpp, 0.990's path
 * string).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <photoreview_dialog.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <math/ngp/v4.h>
#include <memory/heap_allocator.h>
#include "../../cdlg_mgr/common_dialog_mgr.h"
#include "../../cdlg_main/common_dialog_main.h"
#include "photoreview_dialog_plugin.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

/*
 * The work memory, locked for one of the plugin's functions under the function's name, as
 * libcdlg's own calls lock it (CommonDialogApiScope): ret is the lock's error, then the first
 * error of the call.
 */
class PhotoReviewPluginApiScope {
public:
	PhotoReviewPluginApiScope(const char *name, int &ret) : m_ret(ret), m_name(name), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_PHOTOREVIEW);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogPhotoReviewParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_PHOTOREVIEW);
		if (m_work == NULL) {
			ret = PHOTOREVIEW_PLUGIN_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_PHOTOREVIEW);
			return;
		}
		sce_paf_strncpy(m_work->apiName, name, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~PhotoReviewPluginApiScope()
	{
		int ret;

		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_name, sizeof(m_work->apiName)) != 0) {
			ret = PHOTOREVIEW_PLUGIN_ERROR_API_NAME;
		} else {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			ret = SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_PHOTOREVIEW);
			if (ret == SCE_OK)
				return;
		}
		if (m_ret == SCE_OK)
			m_ret = ret;
	}

	CommonDialogPhotoReviewParam *work(void) const
	{
		return m_work;
	}

private:
	int &m_ret;
	const char *m_name;
	CommonDialogPhotoReviewParam *m_work;
};

/* the dialog's colours before the work memory gives them */
#define PHOTOREVIEW_DEFAULT_BG_COLOR	{0x26, 0x28, 0x28, 0xFF}
/* the surface pool the plugin asks for (Plugin::InitParam2) */
#define PHOTOREVIEW_SURFACE_POOL_SIZE	0x200000

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static PhotoReviewDialog *s_dialog = NULL;

static Plugin::ModuleInterface s_moduleInterface;

static void photoReviewDialogInit(SceSize argSize, const void *argp);
static void photoReviewDialogTerm(void);
static void photoReviewDialogPluginInit(Plugin *plugin);
static void photoReviewDialogPluginStart(Plugin *plugin);
static void photoReviewDialogPluginStop(Plugin *plugin);
static void photoReviewDialogPluginExit(Plugin *plugin);
static void photoReviewDialogPluginSetParam(Plugin::InitParam2 *param);
static void OnUpdate(void *userdata);

extern "C" int module_start(SceSize argSize, const void *argp)
{
	photoReviewDialogInit(argSize, argp);
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	photoReviewDialogTerm();
	return 0;
}

/* the static objects' constructors, and the plugin's functions for SceCommonDialogMain's
   paf::Module */
static __attribute__((noinline)) void photoReviewDialogInit(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	s_moduleInterface.init_func = photoReviewDialogPluginInit;
	s_moduleInterface.start_func = photoReviewDialogPluginStart;
	s_moduleInterface.stop_func = photoReviewDialogPluginStop;
	s_moduleInterface.exit_func = photoReviewDialogPluginExit;
	s_moduleInterface.set_param_func = photoReviewDialogPluginSetParam;
	(*(Module *const *)argp)->SetInterface(0, &s_moduleInterface);
}

static __attribute__((noinline)) void photoReviewDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static __attribute__((noinline)) int GetReviewParam(ScePhotoReviewDialogParam *param)
{
	int ret;

	{
		PhotoReviewPluginApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sce_paf_memcpy(param, &scope.work()->param, sizeof(*param));
	}
	return ret;
}

static __attribute__((noinline)) int GetCommonParam(int *infobarShown, SceCommonDialogColor *bgColor)
{
	int ret;

	{
		PhotoReviewPluginApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			*infobarShown = scope.work()->infobarShown;
			*bgColor = scope.work()->bgColor;
		}
	}
	return ret;
}

/* the dialog has ended with its result: the plugin stops updating, and SceCommonDialogMain
   finishes the call */
static inline void EndDialog(CommonDialogPhotoReviewParam *work, const ScePhotoReviewDialogResult *result)
{
	sce_paf_memcpy(&work->result, result, sizeof(work->result));
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	/* Sony's assertion: its message is left out of the release build */
	if (SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_PHOTOREVIEW) != SCE_OK)
		__breakpoint(0);
}

static void PhotoReviewDialogCallback(const ScePhotoReviewDialogResult *result, void *userdata)
{
	int ret;
	PhotoReviewPluginApiScope scope(__FUNCTION__, ret);

	if (ret == SCE_OK)
		EndDialog(scope.work(), result);
}

/* on the main thread: the dialog ends as aborted once the caller aborts, or on an error */
static void OnUpdate(void *userdata)
{
	int ret;
	PhotoReviewPluginApiScope scope(__FUNCTION__, ret);

	if (ret < 0 || (ret == SCE_OK && SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_PHOTOREVIEW))) {
		ScePhotoReviewDialogResult result;

		memset(result.reserved, 0, sizeof(result.reserved));
		result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		EndDialog(scope.work(), &result);
	}
}

static void photoReviewDialogPluginInit(Plugin *plugin)
{
	s_dialog = new PhotoReviewDialog(plugin);
	if (s_dialog != NULL) {
		int infobarShown = 0;
		SceCommonDialogColor bgColor = PHOTOREVIEW_DEFAULT_BG_COLOR;
		ScePhotoReviewDialogParam param;

		GetCommonParam(&infobarShown, &bgColor);
		s_dialog->SetBgColor(infobarShown != 0, &bgColor);
		GetReviewParam(&param);
		s_dialog->SetParam(&param, PhotoReviewDialogCallback, NULL);
		s_dialog->Init(plugin);
	}
}

static void photoReviewDialogPluginStart(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Start(plugin);
	common::MainThreadCallList::Register(OnUpdate, NULL);
	SceCommonDialogMain_877BB562();
}

static void photoReviewDialogPluginStop(Plugin *plugin)
{
	if (s_dialog != NULL)
		s_dialog->Stop(plugin);
}

static void photoReviewDialogPluginExit(Plugin *plugin)
{
	if (s_dialog != NULL) {
		s_dialog->Exit(plugin);
		delete s_dialog;
	}
	s_dialog = NULL;
}

static void photoReviewDialogPluginSetParam(Plugin::InitParam2 *param)
{
	param->surface_pool_size = PHOTOREVIEW_SURFACE_POOL_SIZE;
}

__attribute__((noinline)) PhotoReviewDialog::PhotoReviewDialog(Plugin *plugin)
	: PluginApp(plugin), m_view(plugin), m_callback(NULL), m_userdata(NULL), m_decodeHeap(NULL)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

PhotoReviewDialog::~PhotoReviewDialog()
{
}

__attribute__((noinline)) int PhotoReviewDialog::SetBgColor(bool infobarShown, const SceCommonDialogColor *color)
{
	m_view.SetBgColor(infobarShown, math::v4(color->r, color->g, color->b, color->a) / 255.0f);
	return 0;
}

__attribute__((noinline)) int PhotoReviewDialog::SetParam(const ScePhotoReviewDialogParam *param, Callback callback, void *userdata)
{
	m_callback = callback;
	m_userdata = userdata;
	m_param = *param;
	return 0;
}

/*
 * The decode heap at the start of the caller's work memory, the photo from the plugin's path_d
 * argument when it has one (the file name is then the argument's), else from the caller's file
 * name, and the metadata to export it with.
 */
int PhotoReviewDialog::Init(Plugin *plugin)
{
	void *membase;
	void *workMemory;
	bool exist;

	m_view.AddListener(this);
	workMemory = NULL;
	if ((membase = m_param.workMemory) != NULL) {
		m_decodeHeap = new memory::HeapAllocator(membase, PHOTOREVIEW_DECODE_HEAP_SIZE,
							 "ScePhotoReviewDecodeHeap");
		workMemory = (SceUInt8 *)m_param.workMemory + PHOTOREVIEW_DECODE_HEAP_SIZE;
	}
	exist = false;
	string path = plugin->GetArg("path_d", &exist);

	if (exist) {
		sce_paf_strncpy((char *)m_param.fileName, path.c_str(), sizeof(m_param.fileName));
		m_view.SetPhoto(path, m_param.mode, m_decodeHeap, workMemory);
	} else {
		m_view.SetPhoto(string((const char *)m_param.fileName), m_param.mode, m_decodeHeap,
				workMemory);
	}
	if (m_param.exportParam != NULL) {
		m_view.SetExportParam(string((const char *)m_param.exportParam->photoTitle),
				      string((const char *)m_param.exportParam->gameTitle),
				      string((const char *)m_param.exportParam->gameComment));
	}
	m_view.StartDecode();
	CreateInputListener();
	return 0;
}

int PhotoReviewDialog::Start(Plugin *plugin)
{
	StartInputListener();
	m_view.Start();
	return 0;
}

int PhotoReviewDialog::Stop(Plugin *plugin)
{
	m_view.Stop();
	StopInputListener();
	return 0;
}

int PhotoReviewDialog::Exit(Plugin *plugin)
{
	DestroyInputListener();
	m_view.RemoveListener(this);
	if (m_decodeHeap != NULL)
		delete m_decodeHeap;
	return 0;
}

void PhotoReviewDialog::OnEvent(EventDispatcher *sender, int32_t event, void *arg)
{
	if (sender != &m_view)
		return;
	switch (event) {
	case PHOTOREVIEW_EVENT_RESULT:
		m_result.result = *(SceInt32 *)arg;
		memset(m_result.reserved, 0, sizeof(m_result.reserved));
		m_view.Close(2, 500.0f);
		break;
	case PHOTOREVIEW_EVENT_OK:
		m_result.result = SCE_COMMON_DIALOG_RESULT_OK;
		memset(m_result.reserved, 0, sizeof(m_result.reserved));
		m_view.Close(2, 500.0f);
		break;
	case PHOTOREVIEW_EVENT_CLOSED:
		if (m_callback != NULL)
			m_callback(&m_result, m_userdata);
		break;
	}
}
