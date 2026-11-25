/*
 * SceCalendarDialogPlugin (vs0:sys/external/libcdlg_calendar_review.suprx):
 * plugin entry points, CommonDialogMain work-memory bridge, and SceCalendarReviewCtrl.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <std/string.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "calendar_review_dialog_plugin.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static SceCalendarReviewCtrl *s_calendarCtrl = NULL;

static void calendarPluginSetParam(Plugin::InitParam2 *param);
static void calendarPluginInit(Plugin *plugin);
static void calendarPluginStart(Plugin *plugin);
static void calendarPluginStop(Plugin *plugin);
static void calendarPluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	calendarPluginSetParam,
	calendarPluginInit,
	calendarPluginStart,
	calendarPluginStop,
	calendarPluginExit
};

class CalendarReviewApiScope {
public:
	CalendarReviewApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogCalendarReviewParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
		if (m_work == NULL) {
			err = CALENDAR_REVIEW_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~CalendarReviewApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
		}
	}

	CommonDialogCalendarReviewParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogCalendarReviewParam *m_work;
	const char *m_apiName;
};

static void OnUpdate(void *userData);

static SceInt32 GetReviewParam(CalendarReviewDialogParam *outParam)
{
	SceInt32 ret = SCE_OK;
	CalendarReviewApiScope scope("GetReviewParam", ret);
	CommonDialogCalendarReviewParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && outParam != NULL)
		sce_paf_memcpy(outParam, &work->param, sizeof(CalendarReviewDialogParam));
	return ret;
}

static SceInt32 GetCommonParam(SceInt32 *outDrawPriority, SceCommonDialogColor *outBgColor)
{
	SceInt32 ret = SCE_OK;
	CalendarReviewApiScope scope("GetCommonParam", ret);
	CommonDialogCalendarReviewParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		if (outDrawPriority != NULL)
			*outDrawPriority = work->drawPriority;
		if (outBgColor != NULL)
			*outBgColor = work->bgColor;
	}
	return ret;
}

static void CalendarReviewDialogCallback(const CalendarReviewDialogResult *result, void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	CalendarReviewApiScope scope("CalendarReviewDialogCallback", ret);
	CommonDialogCalendarReviewParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && result != NULL) {
		sce_paf_memcpy(&work->result, result, sizeof(CalendarReviewDialogResult));
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
	}
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	CalendarReviewApiScope scope("OnUpdate", ret);
	CommonDialogCalendarReviewParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_CALENDAR_REVIEW)) {
			CalendarReviewDialogResult abortResult;
			sce_paf_memset(&abortResult, 0, sizeof(abortResult));
			abortResult.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
			sce_paf_memcpy(&work->result, &abortResult, sizeof(abortResult));
			common::MainThreadCallList::Unregister(OnUpdate, NULL);
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
		}
	}
}

SceCalendarReviewCtrl::SceCalendarReviewCtrl(Plugin *plugin)
	: m_plugin(plugin),
	  m_scene(NULL),
	  m_event(),
	  m_callback(NULL),
	  m_callbackUserData(NULL),
	  m_drawPriority(120),
	  m_bgColor(0.149f, 0.157f, 0.157f, 1.0f),
	  m_opened(false)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

SceCalendarReviewCtrl::~SceCalendarReviewCtrl()
{
	OnExit(m_plugin);
}

SceInt32 SceCalendarReviewCtrl::SetCommonParam(SceInt32 drawPriority,
					       const SceCommonDialogColor *bgColor)
{
	m_drawPriority = drawPriority;
	if (bgColor != NULL) {
		m_bgColor = math::v4((float)bgColor->r / 255.0f,
				     (float)bgColor->g / 255.0f,
				     (float)bgColor->b / 255.0f,
				     (float)bgColor->a / 255.0f);
	}
	return SCE_OK;
}

SceInt32 SceCalendarReviewCtrl::SetReviewParam(const CalendarReviewDialogParam *param,
					       CalendarReviewResultCallback callback,
					       void *userData)
{
	if (param != NULL)
		m_param = *param;
	m_callback = callback;
	m_callbackUserData = userData;
	return SCE_OK;
}

SceInt32 SceCalendarReviewCtrl::OnInit(Plugin *plugin)
{
	m_plugin = plugin;
	return SCE_OK;
}

SceInt32 SceCalendarReviewCtrl::OnStart(Plugin *plugin)
{
	(void)plugin;
	m_opened = true;
	return SCE_OK;
}

SceInt32 SceCalendarReviewCtrl::OnStop(Plugin *plugin)
{
	(void)plugin;
	m_opened = false;
	return SCE_OK;
}

SceInt32 SceCalendarReviewCtrl::OnExit(Plugin *plugin)
{
	(void)plugin;
	if (m_opened) {
		m_scene = NULL;
		m_opened = false;
	}
	return SCE_OK;
}

void SceCalendarReviewCtrl::NotifyResult(const CalendarReviewDialogResult *result)
{
	if (result != NULL)
		m_result = *result;
	if (m_callback != NULL)
		m_callback(&m_result, m_callbackUserData);
}

static void calendarPluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void calendarPluginInit(Plugin *plugin)
{
	s_calendarCtrl = new SceCalendarReviewCtrl(plugin);
	if (s_calendarCtrl != NULL) {
		SceInt32 drawPriority = 120;
		SceCommonDialogColor bgColor;
		bgColor.r = 38;
		bgColor.g = 40;
		bgColor.b = 40;
		bgColor.a = 255;

		CalendarReviewDialogParam param;
		sce_paf_memset(&param, 0, sizeof(param));
		GetCommonParam(&drawPriority, &bgColor);
		GetReviewParam(&param);
		s_calendarCtrl->SetCommonParam(drawPriority, &bgColor);
		s_calendarCtrl->SetReviewParam(&param, CalendarReviewDialogCallback, NULL);
		s_calendarCtrl->OnInit(plugin);
	}
}

static void calendarPluginStart(Plugin *plugin)
{
	if (s_calendarCtrl != NULL)
		s_calendarCtrl->OnStart(plugin);
	common::MainThreadCallList::Register(OnUpdate, NULL);
	SceCommonDialogMain_877BB562();
}

static void calendarPluginStop(Plugin *plugin)
{
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	if (s_calendarCtrl != NULL)
		s_calendarCtrl->OnStop(plugin);
}

static void calendarPluginExit(Plugin *plugin)
{
	if (s_calendarCtrl != NULL) {
		s_calendarCtrl->OnExit(plugin);
		delete s_calendarCtrl;
		s_calendarCtrl = NULL;
	}
}

static SceInt32 calendarModuleStart(SceSize arglen, const void *argp)
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

static SceInt32 calendarModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	calendarModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	calendarModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
