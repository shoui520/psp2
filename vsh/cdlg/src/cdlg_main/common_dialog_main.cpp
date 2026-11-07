/*
 * SceCommonDialogMain: what libcdlg's worker thread runs to show a common
 * dialog. It loads paf (with the dialog's heap) and the common GUI dialog
 * module, makes the paf framework (or shares the application's), loads the
 * dialog's plugin with the parameters of the dialog table, runs the framework
 * and draws the dimmer behind the dialogs; the plugins call its exports.
 *
 * Sony's names (the 0.9x builds' messages): OnFrameworkCreation,
 * OnFrameworkDestruction, OnPluginLoad, load_finish_callback and
 * unload_finish_callback. The other names are ours.
 */
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include <system_param.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <common_dialog.h>
#include <sce_atomic.h>
#include <sysmodule/sysmodule.h>
#include <framework.h>
#include <plugin.h>
#include <std/stdlib.h>
#include <thread/thread.h>
#include <inputdevice/inputdevice.h>
#include <common/transition.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_plane.h>
#include <widget/core/environment.h>
#include "../../../paf/src/module.h"
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "common_dialog_main.h"

using namespace paf;

extern "C" {
int sceAppMgrAppParamGetString(int pid, int param, char *string, SceSize length);
int sceAppMgrSetInfobarStateForCommonDialog(int mode, int color, int transparency);
int sceKernelIsCDialogAvailable(void);
int sceSblQafMgrIsAllowRemoteSysmoduleLoad(void);
int sceRegMgrSystemParamGetInt(int id, int *value);
}

#define CDLG_ASSERT(cond)							\
	do {								\
		if (!(cond)) {						\
			sceClibPrintf("[SceCommonDialogMain] INTERNAL ASSERTION FAILURE\n"); \
			__breakpoint(0);				\
		}							\
	} while (0)

#define CDLG_ASSERT_RESULT(ret)						\
	do {								\
		if ((ret) != SCE_OK) {					\
			sceClibPrintf("[SceCommonDialogMain] INTERNAL ASSERTION FAILURE (0x%x)\n", \
				      (ret));				\
			__breakpoint(0);				\
		}							\
	} while (0)

/* the system parameter of the system's language */
#define CDLG_REG_LANGUAGE		226562

/* the application parameter of the title ID */
#define CDLG_APP_PARAM_TITLE_ID		12

/* SceCommonDialogConfigParam::enterButtonAssign */
#define CDLG_ENTER_BUTTON_CIRCLE	0
#define CDLG_ENTER_BUTTON_CROSS		1

/* the module load flag of a process that may show common dialogs */
#define CDLG_MODULE_LOAD_FLAG_CDIALOG	0x10

/* a common dialog plugin's load parameters: paf's, and what the dialog table adds */
struct CommonDialogPluginParam {
	CommonDialogPluginParam()
		: extraCount(0), extraSize(0), extraFlag(false), extraEnabled(true),
		  extraValue1(0), extraValue2(0)
	{
	}

	Plugin::InitParam init;
	int32_t extraCount;
	uint32_t extraSize;
	bool extraFlag;
	bool extraEnabled;
	int32_t extraValue1;
	int32_t extraValue2;
};

/* the frame buffer Framework::Update draws the dialogs into */
struct CommonDialogFrameBuffer {
	SceUInt32 strideInPixels;
	SceUInt32 width;
	SceUInt32 height;
	SceGxmColorFormat colorFormat;
	SceGxmColorSurfaceType surfaceType;
	ScePVoid colorSurfaceData;
	SceUInt32 option;
	SceGxmSyncObject *displaySyncObject;
};

/* a dialog's entry of the dialog table */
struct CommonDialogModuleInfo {
	int dialogType;
	/* paf's global heap */
	SceSize heapSize;
	void (*setFrameworkParam)(Framework::InitParam *param);
	void (*setPluginParam)(CommonDialogPluginParam *param);
	void (*term)(void *arg);
};

/* the work area libcdlg handed over */
CommonDialogWork *g_work = NULL;
void emptyFunction(void);
void (*g_emptyFunction)(void) = emptyFunction;

static ui::Scene *s_dimmerScene;
static ui::Camera *s_dimmerCamera;
static ui::Plane *s_dimmerPlane;
/* the dialogs gave no dimmer color: the dimmer is not shown */
static bool s_noDimmerColor;
static bool s_dimmerCreated;
static int s_dimmerShowCount;

extern const CommonDialogModuleInfo s_moduleInfoTable[];

__attribute__((noinline)) int loadModuleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags,
			     int *status);
__attribute__((noinline)) int unloadModuleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags,
			       int *status);

static inline int findSlot(int dialogType)
{
	SceUInt32 i;

	for (i = 0; i < COMMON_DIALOG_SLOT_NUM; i++) {
		if (g_work->slot[i].dialogType == dialogType)
			return i;
	}
	return -1;
}

static inline const CommonDialogModuleInfo *findModuleInfo(int dialogType)
{
	const CommonDialogModuleInfo *info = NULL;
	int i;

	for (i = 0; s_moduleInfoTable[i].dialogType != 0; i++) {
		if (s_moduleInfoTable[i].dialogType == dialogType) {
			info = &s_moduleInfoTable[i];
			break;
		}
	}
	return info;
}

/* the same search, as OnFrameworkDestruction writes it: the terminator checked after the step */
static inline const CommonDialogModuleInfo *findModuleInfoAfterStep(int dialogType)
{
	int i;

	for (i = 0; s_moduleInfoTable[i].dialogType != dialogType;) {
		if (s_moduleInfoTable[++i].dialogType == 0)
			return NULL;
	}
	return &s_moduleInfoTable[i];
}

/* the system's language as the paf Locale, which numbers the languages as the system does */
int getLocale(int language)
{
	switch (language) {
	case SCE_SYSTEM_PARAM_LANG_JAPANESE:
		return SCE_SYSTEM_PARAM_LANG_JAPANESE;
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
	case SCE_SYSTEM_PARAM_LANG_FRENCH:
		return SCE_SYSTEM_PARAM_LANG_FRENCH;
	case SCE_SYSTEM_PARAM_LANG_SPANISH:
		return SCE_SYSTEM_PARAM_LANG_SPANISH;
	case SCE_SYSTEM_PARAM_LANG_GERMAN:
		return SCE_SYSTEM_PARAM_LANG_GERMAN;
	case SCE_SYSTEM_PARAM_LANG_ITALIAN:
		return SCE_SYSTEM_PARAM_LANG_ITALIAN;
	case SCE_SYSTEM_PARAM_LANG_DUTCH:
		return SCE_SYSTEM_PARAM_LANG_DUTCH;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
		return SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT;
	case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
		return SCE_SYSTEM_PARAM_LANG_RUSSIAN;
	case SCE_SYSTEM_PARAM_LANG_KOREAN:
		return SCE_SYSTEM_PARAM_LANG_KOREAN;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
		return SCE_SYSTEM_PARAM_LANG_CHINESE_T;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
		return SCE_SYSTEM_PARAM_LANG_CHINESE_S;
	case SCE_SYSTEM_PARAM_LANG_FINNISH:
		return SCE_SYSTEM_PARAM_LANG_FINNISH;
	case SCE_SYSTEM_PARAM_LANG_SWEDISH:
		return SCE_SYSTEM_PARAM_LANG_SWEDISH;
	case SCE_SYSTEM_PARAM_LANG_DANISH:
		return SCE_SYSTEM_PARAM_LANG_DANISH;
	case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:
		return SCE_SYSTEM_PARAM_LANG_NORWEGIAN;
	case SCE_SYSTEM_PARAM_LANG_POLISH:
		return SCE_SYSTEM_PARAM_LANG_POLISH;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
		return SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR;
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_GB;
	case SCE_SYSTEM_PARAM_LANG_TURKISH:
		return SCE_SYSTEM_PARAM_LANG_TURKISH;
	default:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
	}
}

/* the framework of the dialog, when the application has none */
Framework *createFramework(int dialogType)
{
	Framework::InitParam param;
	char titleId[10];
	const CommonDialogModuleInfo *info;
	Framework *framework;

	Framework::SampleInit(&param);
	param.display_list_size = 0x80000;
	param.surface_pool_size = 0x100000;
	param.graph_heap_size_on_main_memory = 0x80000;
	param.text_surface_pool_size = 0x80000;
	param.pvf_heap_size = 0x100000;
	param.surf_dev_mem_addr = NULL;
	param.surf_dev_mem_size = 0;
	if (g_work->hasConfig) {
		param.locale = getLocale(g_work->language);
		switch (g_work->enterButtonAssign) {
		case CDLG_ENTER_BUTTON_CIRCLE:
			param.enter_button_assign = CDLG_ENTER_BUTTON_CIRCLE;
			break;
		case CDLG_ENTER_BUTTON_CROSS:
			param.enter_button_assign = CDLG_ENTER_BUTTON_CROSS;
			break;
		}
	}
	if (g_work->focusState == SCE_COMMON_DIALOG_FOCUS_VISIBLE)
		param.show_focus = true;
	else if (g_work->focusState == SCE_COMMON_DIALOG_FOCUS_INVISIBLE)
		param.show_focus = false;

	/* the titles that draw their dialogs themselves */
	sce_paf_memset(titleId, 0, sizeof(titleId));
	if (sceAppMgrAppParamGetString(0, CDLG_APP_PARAM_TITLE_ID, titleId, sizeof(titleId)) == 0 &&
	    sce_paf_strncmp(titleId, "PCS", 3) == 0) {
		if (sce_paf_strncmp(&titleId[3], "G00", 3) == 0 &&
		    (sce_paf_strncmp(&titleId[6], "047", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "052", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "056", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "072", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "071", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "073", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "085", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "086", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "100", 3) == 0 ||
		     sce_paf_strncmp(&titleId[6], "099", 3) == 0))
			param.option |= Framework::Option_4;
		if (sce_paf_strncmp(&titleId[3], "E00075", 6) == 0)
			param.option |= Framework::Option_4;
	}

	info = findModuleInfo(dialogType);
	CDLG_ASSERT(info != NULL);
	if (info->setFrameworkParam != NULL)
		info->setFrameworkParam(&param);
	param.mode = Framework::Mode_CommonDialog;
	framework = new Framework(param);
	Framework::Instance()->LoadCommonResourceAsync(NULL);
	return framework;
}

void load_finish_callback(Plugin *plugin)
{
	SceUInt32 i;

	for (i = 0; i <= g_work->lastSlot; i++) {
		if (sceClibStrcmp(plugin->m_name.c_str(), g_work->slot[i].pluginName) == 0) {
			g_work->slot[i].plugin = plugin;
			g_work->slot[i].status = COMMON_DIALOG_SLOT_RUNNING;
			if (sceAtomicIncrement32AcqRel(&g_work->openCount) == 0 &&
			    !g_work->keepInputLocked)
				inputdevice::Unlock(0x7F);
			return;
		}
	}
	CDLG_ASSERT(0);
}

void unload_finish_callback(const char *name)
{
	CommonDialogSlot *slot = NULL;
	SceUInt32 i;
	int ret;

	for (i = 0;;) {
		if (sceClibStrcmp(name, g_work->slot[i].pluginName) == 0) {
			slot = &g_work->slot[i];
			break;
		}
		if (++i > g_work->lastSlot)
			break;
	}
	CDLG_ASSERT(slot != NULL);
	slot->status = COMMON_DIALOG_SLOT_UNLOADED;
	if (sceAtomicDecrement32AcqRel(&g_work->openCount) == 1) {
		if (!g_work->frameworkShared) {
			Framework::Instance()->UnLoadCommonResourceAsync(NULL);
			Framework::Instance()->RequestShutdown();
		}
		if (!s_noDimmerColor)
			s_dimmerShowCount = 0;
		if (s_dimmerCreated) {
			s_dimmerScene->DestroyWidget();
			s_dimmerCamera->DestroyWidget();
			s_dimmerScene->DestroyWidget();
			s_dimmerCreated = false;
		}
		ret = sceKernelSignalSema(g_work->frameworkSema, 1);
		CDLG_ASSERT_RESULT(ret);
	} else {
		slot->closeState = COMMON_DIALOG_CLOSE_DONE;
	}
	ret = sceKernelSignalSema(slot->doneSema, 1);
	CDLG_ASSERT_RESULT(ret);
}

int OnFrameworkUpdate(const SceCommonDialogUpdateParam *updateParam)
{
	CommonDialogFrameBuffer frameBuffer;
	Framework *framework;

	framework = Framework::Instance();
	if (framework == NULL) {
		sceClibPrintf("[SceCommonDialogMain] INTERNAL ASSERTION FAILURE\n");
		return COMMON_DIALOG_ERROR_INTERNAL;
	}
	frameBuffer.strideInPixels = updateParam->renderTarget.strideInPixels;
	frameBuffer.width = updateParam->renderTarget.width;
	frameBuffer.height = updateParam->renderTarget.height;
	frameBuffer.colorFormat = updateParam->renderTarget.colorFormat;
	frameBuffer.surfaceType = updateParam->renderTarget.surfaceType;
	frameBuffer.colorSurfaceData = updateParam->renderTarget.colorSurfaceData;
	frameBuffer.option = 0;
	frameBuffer.displaySyncObject = updateParam->displaySyncObject;
	framework->Update(&frameBuffer);
	return SCE_OK;
}

/* the dimmer: a plane over the application, under the dialogs */
void createDimmer(void *arg)
{
	const CommonDialogColor *color = NULL;
	int priority = g_work->drawPriority;
	ui::Environment *env;

	if (g_work->hasDimmerColor)
		color = &g_work->dimmerColor;
	s_dimmerCreated = false;
	s_dimmerShowCount = 0;
	env = Framework::Instance()->GetEnvironmentInstance();
	s_dimmerScene = new ui::Scene(env, priority, NULL, NULL, 128);
	s_dimmerCamera = new ui::Camera(s_dimmerScene, NULL);
	s_dimmerPlane = new ui::Plane(s_dimmerScene, NULL);
	if (s_dimmerScene == NULL || s_dimmerCamera == NULL || s_dimmerPlane == NULL) {
		s_dimmerScene->DestroyWidget();
		s_dimmerCamera->DestroyWidget();
		s_dimmerScene->DestroyWidget();
		CDLG_ASSERT_RESULT(COMMON_DIALOG_ERROR_NO_MEMORY);
		return;
	}
	s_dimmerPlane->m_transition_complete = false;
	s_dimmerPlane->m_transition = 0xFF;
	s_dimmerPlane->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(s_dimmerPlane, NULL, 2, 0.0f, NULL);
	if (s_dimmerPlane->m_event_enable & 0x400)
		s_dimmerPlane->EnableFocusEvent(false);
	s_dimmerPlane->m_event_enable &= 0xFFFF000F;
	if (color != NULL) {
		math::v4 c(color->r / 255.0f, color->g / 255.0f, color->b / 255.0f,
			   color->a / 255.0f);

		s_dimmerPlane->SetColor(c);
		s_noDimmerColor = false;
	} else {
		s_noDimmerColor = true;
	}
	s_dimmerPlane->SetAdjust(1, 1, 1);
	s_dimmerCreated = true;
}

int OnFrameworkCreation(int dialogType)
{
	SceUInt32 flags = 0;
	const CommonDialogModuleInfo *info = NULL;
	int status;
	ScePafInit pafInit;
	int ret;

	if (sceKernelIsCDialogAvailable() != 0)
		flags = CDLG_MODULE_LOAD_FLAG_CDIALOG;
	if (sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_PAF) != SCE_OK) {
		info = findModuleInfo(dialogType);
		pafInit.global_heap_size = info->heapSize;
		pafInit.cdlg_mode = 1;
		ret = loadModuleWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(pafInit), &pafInit, flags,
					&status);
		CDLG_ASSERT_RESULT(ret);
		g_work->pafLoaded = true;
	}
	ret = loadModuleWithArg(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG, 0, NULL, flags, &status);
	CDLG_ASSERT_RESULT(ret);
	if (Framework::Instance() != NULL)
		g_work->frameworkShared = true;
	else
		CDLG_ASSERT(createFramework(dialogType) != NULL);
	thread::CallQueue::main_thread_callqueue.Enqueue("SceCommonDialogMain", createDimmer, NULL);
	return SCE_OK;
}

int OnFrameworkDestruction(int dialogType)
{
	const CommonDialogModuleInfo *info;
	Framework *framework;
	ui::Environment *env;
	int status;
	int termArg;
	int ret;

	ret = sceKernelWaitSema(g_work->frameworkSema, 1, NULL);
	if (ret != SCE_OK) {
		sceClibPrintf("[SceCommonDialogMain] INTERNAL ASSERTION FAILURE (0x%x)\n", ret);
		return COMMON_DIALOG_ERROR_INTERNAL;
	}
	CDLG_ASSERT(g_work->slot[0].dialogType == dialogType);
	info = findModuleInfoAfterStep(dialogType);
	CDLG_ASSERT(info != NULL);
	if (!g_work->frameworkShared) {
		framework = Framework::Instance();
		if (framework != NULL) {
			env = framework->GetEnvironmentInstance();
			if (env != NULL) {
				if (ScePafWidget_529FB877(env, -1))
					g_work->focusState = SCE_COMMON_DIALOG_FOCUS_VISIBLE;
				else
					g_work->focusState = SCE_COMMON_DIALOG_FOCUS_INVISIBLE;
			}
			delete framework;
		}
		if (info->term != NULL)
			info->term(&termArg);
	} else {
		thread::CallQueue::main_thread_callqueue.WaitEmpty();
	}
	ret = unloadModuleWithArg(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG, 0, NULL, 0, &status);
	CDLG_ASSERT(ret == SCE_OK);
	if (g_work->pafLoaded) {
		ret = unloadModuleWithArg(SCE_SYSMODULE_INTERNAL_PAF, 0, NULL, 0, &status);
		CDLG_ASSERT_RESULT(ret);
		g_work->pafLoaded = false;
	}
	g_work->slot[0].status = COMMON_DIALOG_SLOT_FINISHED;
	g_work->slot[0].closeState = COMMON_DIALOG_CLOSE_DONE;
	return SCE_OK;
}

int OnPluginLoad(int dialogType)
{
	int i = findSlot(dialogType);
	const CommonDialogModuleInfo *info = NULL;
	CommonDialogSlot *slot;

	CDLG_ASSERT(i >= 0);
	Framework::Instance();

	CommonDialogPluginParam param;

	param.init.caller_name = "__main__";
	param.init.module_option = 1;
	param.init.private_heap_size = 0x100000;
	/* the dialogs open later draw under the earlier ones */
	param.init.draw_priority = 70 - i;
	info = findModuleInfo(dialogType);
	info->setPluginParam(&param);
	slot = &g_work->slot[i];
	slot->status = COMMON_DIALOG_SLOT_LOADING;
	sceClibStrncpy(slot->pluginName, param.init.name.c_str(), sizeof(slot->pluginName));
	g_work->slot[i].pluginName[sizeof(slot->pluginName) - 1] = '\0';
	Plugin::LoadAsync(param.init, load_finish_callback, 0);
	return SCE_OK;
}

int OnPluginStop(void)
{
	return SCE_OK;
}

int OnFrameworkRun(void)
{
	if (!g_work->frameworkShared)
		Framework::Instance()->Run();
	return SCE_OK;
}

void showDimmer(void *arg)
{
	ui::Plane *plane;

	if (!s_dimmerCreated || s_noDimmerColor)
		return;
	if (s_dimmerShowCount == 0) {
		plane = s_dimmerPlane;
		if (!plane->m_transition_complete) {
			plane->m_transition = 0xFF;
			plane->m_transition_complete = true;
			plane->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(plane, NULL, 2, 0.0f, NULL);
		ScePafCommon_6CF3A3C8(0.0f, s_dimmerPlane, common::transition::Type_FadeinSlow, false,
				      false);
	}
	s_dimmerShowCount++;
}

void hideDimmer(void *arg)
{
	if (!s_dimmerCreated || s_dimmerShowCount == 0)
		return;
	if (--s_dimmerShowCount == 0)
		ScePafCommon_B48C9BDC(0.0f, s_dimmerPlane, common::transition::Type_FadeinSlow, false,
				      false);
}

__attribute__((noinline)) int setWork(CommonDialogWork *work)
{
	g_work = work;
	g_work->slot[0].status = COMMON_DIALOG_SLOT_IDLE;
	g_work->initFunc = (void *)OnFrameworkCreation;
	g_work->termFunc = (void *)OnFrameworkDestruction;
	g_work->loadFunc = (void *)OnPluginLoad;
	g_work->stopFunc = (void *)OnPluginStop;
	g_work->runFunc = (void *)OnFrameworkRun;
	g_work->updateFunc = (void *)OnFrameworkUpdate;
	return SCE_OK;
}

__attribute__((noinline)) int clearWork(void)
{
	return SCE_OK;
}

int SceCommonDialogMain_05EFC635(int dialogType)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return COMMON_DIALOG_ERROR_SLOT_NOT_FOUND;
	if (sceAtomicCompareAndSwap32AcqRel(&g_work->slot[i].status,
					    COMMON_DIALOG_SLOT_RUNNING,
					    COMMON_DIALOG_SLOT_CLOSING) != COMMON_DIALOG_SLOT_RUNNING)
		return COMMON_DIALOG_ERROR_SLOT_NOT_RUNNING;
	Plugin::UnloadAsync(g_work->slot[i].plugin, unload_finish_callback, 0);
	return SCE_OK;
}

__attribute__((noinline)) void *SceCommonDialogMain_DD2CAC30(int dialogType)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return NULL;
	return g_work->slot[i].param;
}

bool SceCommonDialogMain_75F10F83(int dialogType)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return false;
	return g_work->slot[i].paramReady;
}

Plugin *SceCommonDialogMain_2E153774(int dialogType)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return NULL;
	return g_work->slot[i].plugin;
}

int SceCommonDialogMain_B9D0E5A8(int dialogType)
{
	int i = findSlot(dialogType);
	int ret;

	if (i < 0)
		return COMMON_DIALOG_ERROR_SLOT_NOT_FOUND;
	ret = sceKernelLockLwMutex(&g_work->slot[i].mutex, 1, NULL);
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE)
		return COMMON_DIALOG_ERROR_LOCK_RECURSIVE;
	CDLG_ASSERT(ret == SCE_OK);
	return ret;
}

int SceCommonDialogMain_36DF1E1B(int dialogType)
{
	int i = findSlot(dialogType);
	int ret;

	if (i < 0)
		return COMMON_DIALOG_ERROR_SLOT_NOT_FOUND;
	ret = sceKernelTryLockLwMutex(&g_work->slot[i].mutex, 1);
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return COMMON_DIALOG_ERROR_LOCK_BUSY;
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE)
		return COMMON_DIALOG_ERROR_LOCK_RECURSIVE;
	CDLG_ASSERT(ret == SCE_OK);
	return ret;
}

int SceCommonDialogMain_5739BC42(int dialogType)
{
	int i = findSlot(dialogType);
	int ret;

	if (i < 0)
		return COMMON_DIALOG_ERROR_SLOT_NOT_FOUND;
	ret = sceKernelUnlockLwMutex(&g_work->slot[i].mutex, 1);
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED)
		return COMMON_DIALOG_ERROR_LOCK_NOT_OWNED;
	CDLG_ASSERT(ret == SCE_OK);
	return ret;
}

int SceCommonDialogMain_68330971(int dialogType, int userData)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return COMMON_DIALOG_ERROR_SLOT_NOT_FOUND;
	g_work->slot[i].userData = userData;
	return SCE_OK;
}

int SceCommonDialogMain_E33EFFA5(int dialogType)
{
	int i = findSlot(dialogType);

	if (i < 0)
		return 0;
	return g_work->slot[i].userData;
}

__attribute__((noinline)) int SceCommonDialogMain_1DE9E1A7(char *path, SceSize size, const char *name,
						      int option)
{
	sceSblQafMgrIsAllowRemoteSysmoduleLoad();
	sceClibSnprintf(path, size, "vs0:data/external/cdlg/%s", name);
	return SCE_OK;
}

__attribute__((noinline)) int SceCommonDialogMain_C224F846(char *path, SceSize size, const char *name,
						      int option)
{
	sceSblQafMgrIsAllowRemoteSysmoduleLoad();
	sceClibSnprintf(path, size, "vs0:sys/external/%s", name);
	return SCE_OK;
}

__attribute__((noinline)) int SceCommonDialogMain_60E5D4E7(void)
{
	int language;
	int ret;

	ret = sceRegMgrSystemParamGetInt(CDLG_REG_LANGUAGE, &language);
	CDLG_ASSERT_RESULT(ret);
	return getLocale(language);
}

__attribute__((noinline)) void SceCommonDialogMain_877BB562(void)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("SceCommonDialogMain", showDimmer, NULL);
}

void SceCommonDialogMain_FDED47DB(void)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("SceCommonDialogMain", hideDimmer, NULL);
}

void SceCommonDialogMain_C62B845D(void)
{
	SceAppMgrAppStatus status;
	int ret;

	ret = sceAppMgrGetStatusByName(NULL, &status);
	CDLG_ASSERT_RESULT(ret);
	if (status.infobar.visibility == 1) {
		ret = sceAppMgrSetInfobarStateForCommonDialog(1, (SceInt8)status.infobar.color,
							      (SceInt8)status.infobar.transparency);
		CDLG_ASSERT_RESULT(ret);
	} else {
		ret = sceAppMgrSetInfobarStateForCommonDialog(1, 0, 0);
		CDLG_ASSERT_RESULT(ret);
	}
}

/* an empty function, and below g_work a global that points to it; nothing reads either (the
   dialog plugins of 3.65 carry the same pair) */
void emptyFunction(void)
{
}

extern "C" int module_start(SceSize args, const void *argp)
{
	return setWork(*(CommonDialogWork *const *)argp);
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	return clearWork();
}

/*
 * The dialogs: the framework and plugin parameters of each, in the order of
 * the original, and the table OnFrameworkCreation, OnPluginLoad and
 * OnFrameworkDestruction look them up in.
 */

void msgSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void msgSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "msg_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_msg.suprx", 1);
	param->init.name = "msg_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void savedataSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x1C0000;
	param->allow_button_control = true;
}

void savedataSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "savedata_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_savedata.suprx", 1);
	param->init.name = "savedata_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.option = 20;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void netcheckSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void netcheckSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "netcheck_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_netcheck.suprx", 1);
	param->init.name = "NetCheckDialog";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void netcheckTerm(void *arg)
{
}

void imeSetFrameworkParam(Framework::InitParam *param)
{
}

void imeSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "ime_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_ime.suprx", 1);
	param->init.name = "ime_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void npMessageSetFrameworkParam(Framework::InitParam *param)
{
	param->display_list_size = 0x80000;
	param->text_surface_pool_size = 0x40000;
	param->surface_pool_size = 0xE7000;
	param->text_style = 7;
}

void npMessageTerm(void *arg)
{
}

void npMessageSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "np_message_dialog_impl_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_np_message.suprx", 1);
	param->init.name = "np_message_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.option = 16;
	param->extraCount = 32;
	param->extraSize = 0x300000;
	param->init.private_heap_size = 0;
	SceCommonDialogMain_877BB562();
}

void friendlistSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->allow_button_control = true;
	param->text_surface_pool_size = 0x50000;
}

void friendlistSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "friendlist_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_friendlist.suprx", 1);
	param->init.name = "friendlist_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->extraCount = 32;
	param->extraSize = 0x20000;
	SceCommonDialogMain_877BB562();
}

void friendlistTerm(void *arg)
{
}

void photoImportSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->text_surface_pool_size = 0x30000;
	param->allow_button_control = true;
}

void photoImportSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogPhotoImportParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "photoimport_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_photoimport.suprx", 1);
	dialogParam = (const CommonDialogPhotoImportParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_PHOTOIMPORT);
	param->init.name = "photoimport_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x100000;
	param->init.draw_priority = dialogParam->drawPriority;
}

void npProfile2SetFrameworkParam(Framework::InitParam *param)
{
	sce_paf_printf_dummy();
	param->allow_button_control = true;
	param->locale = SceCommonDialogMain_60E5D4E7();
	param->surface_pool_size = 0x160000;
	param->text_surface_pool_size = 0x50000;
}

void npProfile2SetPluginParam(CommonDialogPluginParam *param)
{
	char modulePath[128];

	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_npprofile2.suprx", 1);
	param->init.name = "npprofile2_dialog_plugin";
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->init.draw_priority = 100;
	param->extraCount = 32;
	param->extraSize = 0x20000;
}

void npProfile2Term(void *arg)
{
}

void trophySetupSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void trophySetupSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "np_trophy_setup_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_np_trophy_setup.suprx", 1);
	param->init.name = "trophysetup_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void trophySetupTerm(void *arg)
{
}

void photoReviewSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x80000;
	param->text_surface_pool_size = 0x30000;
	param->allow_button_control = true;
}

void photoReviewSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogPhotoReviewParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "photoreview_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_photoreview.suprx", 1);
	dialogParam = (const CommonDialogPhotoReviewParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_PHOTOREVIEW);
	param->init.name = "photoreview_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x80000;
	param->init.draw_priority = dialogParam->drawPriority;
}

void checkoutSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x200000;
	param->text_surface_pool_size = 0x80000;
	param->allow_button_control = true;
}

void checkoutSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "store_checkout_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_checkout.suprx", 1);
	param->init.name = "store_checkout_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void cameraImportSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x200000;
	param->text_surface_pool_size = 0x30000;
	param->allow_button_control = true;
}

void cameraImportSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogCameraImportParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "cameraimport_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_cameraimport.suprx", 1);
	dialogParam = (const CommonDialogCameraImportParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CAMERAIMPORT);
	param->init.name = "cameraimport_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x100000;
	param->init.draw_priority = dialogParam->drawPriority;
	SceCommonDialogMain_877BB562();
}

void compatSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x1E4000;
	param->text_surface_pool_size = 0x34400;
	param->allow_button_control = true;
}

void compatSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "compat_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_compat.suprx", 1);
	param->init.name = "compat_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->init.option = 20;
	SceCommonDialogMain_877BB562();
}

void compatTerm(void *arg)
{
}

void npSnsFbSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void npSnsFbSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "np_sns_fb_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_np_sns_fb.suprx", 1);
	param->init.name = "np_sns_fb_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->extraCount = 32;
	param->extraSize = 0x300000;
	SceCommonDialogMain_877BB562();
}

void npEulaSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->allow_button_control = true;
}

void npEulaSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogNpEulaParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "npeula_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_npeula.suprx", 1);
	dialogParam = (const CommonDialogNpEulaParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_NPEULA);
	param->init.name = "npeula_dialog";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->init.draw_priority = dialogParam->drawPriority;
	SceCommonDialogMain_877BB562();
}

void crossControllerSetFrameworkParam(Framework::InitParam *param)
{
}

void crossControllerSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogCrossControllerParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "cross_controller_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_cross_controller.suprx", 1);
	dialogParam = (const CommonDialogCrossControllerParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
	param->init.name = "cross_controller_dialog";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.draw_priority = dialogParam->drawPriority;
	SceCommonDialogMain_877BB562();
}

void crossControllerTerm(void *arg)
{
}

void twLoginSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0;
	param->text_style = 7;
}

void twLoginSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "tw_login_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_tw_login.suprx", 1);
	param->init.name = "tw_login_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void twitterSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0xCCCCC;
	param->allow_button_control = true;
	param->text_style = 7;
}

void twitterSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "twitter_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_twitter.suprx", 1);
	param->init.name = "twitter_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x11800;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void nearSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void nearSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "near_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_near.suprx", 1);
	param->init.name = "near_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->extraCount = 32;
	param->extraSize = 0x300000;
}

void videoImportSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->text_surface_pool_size = 0x80000;
	param->allow_button_control = true;
}

void videoImportSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogVideoImportParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "videoimport_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_videoimport.suprx", 1);
	dialogParam = (const CommonDialogVideoImportParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_VIDEOIMPORT);
	param->init.name = "videoimport_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x100000;
	param->init.draw_priority = dialogParam->drawPriority;
}

void calendarReviewSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->text_surface_pool_size = 0x30000;
	param->allow_button_control = true;
}

void calendarReviewSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogCalendarReviewParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "calendar_review_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_calendar_review.suprx", 1);
	dialogParam = (const CommonDialogCalendarReviewParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
	param->init.name = "calendar_review_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0x100000;
	param->init.draw_priority = dialogParam->drawPriority;
}

void friendlist2SetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x100000;
	param->allow_button_control = true;
	param->text_surface_pool_size = 0x50000;
}

void friendlist2SetPluginParam(CommonDialogPluginParam *param)
{
	char modulePath[128];

	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_friendlist2.suprx", 1);
	param->init.name = "friendlist2_dialog_plugin";
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->extraCount = 32;
	param->extraSize = 0x20000;
}

void friendlist2Term(void *arg)
{
}

void companionSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void companionSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogCompanionParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "companion_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_companion.suprx", 1);
	dialogParam = (const CommonDialogCompanionParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_COMPANION);
	param->init.name = "companion_dialog";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.draw_priority = dialogParam->drawPriority;
	SceCommonDialogMain_877BB562();
}

void gameCustomDataSetFrameworkParam(Framework::InitParam *param)
{
	param->display_list_size = 0x80000;
	param->text_surface_pool_size = 0x40000;
	param->surface_pool_size = 0x180000;
	param->text_style = 7;
}

void gameCustomDataTerm(void *arg)
{
}

void gameCustomDataSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "game_custom_data_dialog_impl_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_game_custom_data.suprx", 1);
	param->init.name = "game_custom_data_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.option = 16;
	param->extraCount = 32;
	param->extraSize = 0x300000;
	param->init.private_heap_size = 0;
	SceCommonDialogMain_877BB562();
}

void invitationSetFrameworkParam(Framework::InitParam *param)
{
	param->display_list_size = 0x80000;
	param->text_surface_pool_size = 0x80000;
	param->surface_pool_size = 0x180000;
	param->text_style = 7;
}

void invitationTerm(void *arg)
{
}

void invitationSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "invitation_dialog_impl_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_invitation.suprx", 1);
	param->init.name = "invitation_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.option = 16;
	param->extraCount = 32;
	param->extraSize = 0x300000;
	param->init.private_heap_size = 0;
	SceCommonDialogMain_877BB562();
}

void pocketStationSetFrameworkParam(Framework::InitParam *param)
{
	param->surface_pool_size = 0x219999;
	param->text_surface_pool_size = 0x34400;
	param->allow_button_control = true;
}

void pocketStationSetPluginParam(CommonDialogPluginParam *param)
{
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "pocketstation_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_pocketstation.suprx", 1);
	param->init.name = "pocketstation_dialog_plugin";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.private_heap_size = 0;
	param->init.option = 20;
	SceCommonDialogMain_877BB562();
}

void pocketStationTerm(void *arg)
{
}

void remoteOskSetFrameworkParam(Framework::InitParam *param)
{
	param->allow_button_control = true;
}

void remoteOskSetPluginParam(CommonDialogPluginParam *param)
{
	const CommonDialogRemoteOskParam *dialogParam;
	char resourcePath[128];
	char modulePath[128];

	SceCommonDialogMain_1DE9E1A7(resourcePath, sizeof(resourcePath), "remote_osk_dialog_plugin.rco", 1);
	SceCommonDialogMain_C224F846(modulePath, sizeof(modulePath), "libcdlg_remote_osk.suprx", 1);
	dialogParam = (const CommonDialogRemoteOskParam *)SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_REMOTE_OSK);
	param->init.name = "remote_osk_dialog";
	param->init.resource_file = resourcePath;
	param->init.module_file = modulePath;
	param->init.draw_priority = dialogParam->drawPriority;
	SceCommonDialogMain_877BB562();
}

const CommonDialogModuleInfo s_moduleInfoTable[] = {

	{ COMMON_DIALOG_TYPE_MSG, 0x180000, msgSetFrameworkParam, msgSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_SAVEDATA, 0x1C0000, savedataSetFrameworkParam, savedataSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_NETCHECK, 0x200000, netcheckSetFrameworkParam, netcheckSetPluginParam,
	  netcheckTerm },

	{ COMMON_DIALOG_TYPE_IME, 0x180000, imeSetFrameworkParam, imeSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_NP_MESSAGE, 0x219999, npMessageSetFrameworkParam, npMessageSetPluginParam,
	  npMessageTerm },

	{ COMMON_DIALOG_TYPE_INVITATION, 0x219999, invitationSetFrameworkParam, invitationSetPluginParam,
	  invitationTerm },

	{ COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA, 0x219999, gameCustomDataSetFrameworkParam, gameCustomDataSetPluginParam,
	  gameCustomDataTerm },

	{ COMMON_DIALOG_TYPE_FRIENDLIST, 0x200000, friendlistSetFrameworkParam, friendlistSetPluginParam,
	  friendlistTerm },

	{ COMMON_DIALOG_TYPE_FRIENDLIST2, 0x200000, friendlist2SetFrameworkParam, friendlist2SetPluginParam,
	  friendlist2Term },

	{ COMMON_DIALOG_TYPE_NPPROFILE2, 0x240000, npProfile2SetFrameworkParam, npProfile2SetPluginParam,
	  npProfile2Term },

	{ COMMON_DIALOG_TYPE_TROPHY_SETUP, 0x180000, trophySetupSetFrameworkParam, trophySetupSetPluginParam,
	  trophySetupTerm },

	{ COMMON_DIALOG_TYPE_PHOTOIMPORT, 0x200000, photoImportSetFrameworkParam, photoImportSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_PHOTOREVIEW, 0x180000, photoReviewSetFrameworkParam, photoReviewSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_CHECKOUT, 0x200000, checkoutSetFrameworkParam, checkoutSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_CAMERAIMPORT, 0x180000, cameraImportSetFrameworkParam, cameraImportSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_COMPAT, 0x300000, compatSetFrameworkParam, compatSetPluginParam,
	  compatTerm },

	{ COMMON_DIALOG_TYPE_NP_SNS_FB, 0x180000, npSnsFbSetFrameworkParam, npSnsFbSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_NPEULA, 0x200000, npEulaSetFrameworkParam, npEulaSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_CROSS_CONTROLLER, 0x180000, crossControllerSetFrameworkParam, crossControllerSetPluginParam,
	  crossControllerTerm },

	{ COMMON_DIALOG_TYPE_TWITTER, 0x299999, twitterSetFrameworkParam, twitterSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_TW_LOGIN, 0x0, twLoginSetFrameworkParam, twLoginSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_NEAR, 0x180000, nearSetFrameworkParam, nearSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_CALENDAR_REVIEW, 0x180000, calendarReviewSetFrameworkParam, calendarReviewSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_COMPANION, 0x180000, companionSetFrameworkParam, companionSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_REMOTE_OSK, 0x180000, remoteOskSetFrameworkParam, remoteOskSetPluginParam,
	  NULL },

	{ COMMON_DIALOG_TYPE_POCKETSTATION, 0x300000, pocketStationSetFrameworkParam, pocketStationSetPluginParam,
	  pocketStationTerm },

	{ COMMON_DIALOG_TYPE_VIDEOIMPORT, 0x200000, videoImportSetFrameworkParam, videoImportSetPluginParam,
	  NULL },

	{ 0, 0, NULL, NULL, NULL }
};

/* the system libraries, with the flags and the status the option carries */
__attribute__((noinline)) int loadModuleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags,
			     int *status)
{
	SysmoduleUserOpt opt;

	opt.flags = flags;
	opt.status = status;
	return sceSysmoduleLoadModuleInternalWithArg(id, args, argp, &opt);
}

__attribute__((noinline)) int unloadModuleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags,
			       int *status)
{
	SysmoduleUserOpt opt;

	opt.flags = flags;
	opt.status = status;
	return sceSysmoduleUnloadModuleInternalWithArg(id, args, argp, &opt);
}
