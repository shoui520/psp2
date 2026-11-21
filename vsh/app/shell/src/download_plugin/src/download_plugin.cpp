/*
 * download_plugin.cpp: the resident download_plugin, the background downloads' front end. Its
 * init makes the bgdl manager, its job queue and dispatcher and the system update service
 * unless the "wakeup_bgdl" argument is "false", its exit destroys them, and its interface
 * queues notices and permission changes for the manager. The names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <plugin.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseparam.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "../../esecron/include/esehttp.hpp"
#include "../../../shell.h"
#include "../../../boot.h"
#include "../../../plugins.h"
#include "../../../bgdl_api.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "download_plugin.h"

using namespace paf;

static Plugin *s_plugin;				/* (0x8154483C) */
static bool s_active;					/* (0x81544840) the manager was made */
static BgdlJobDispatcher *s_dispatcher;			/* (0x81544844) */
static LogJobManager *s_logManager;				/* (0x81544848) */
static DownloadPluginObject *s_object;			/* (0x8154484C) */
static InitEventFunctionEntry s_initEvents[] = {	/* (0x81544850) */
	{ IDParam(0u), NULL },
};

static int downloadPluginNotify(void (*func)(void *), void *arg);
static int downloadPluginSetPermission(void (*func)(void *), void *arg);
static int downloadPluginBgdl_811239D4(const FgdlCaller *caller, int reserved, SceUInt32 taskId);
static int downloadPluginBgdl_811239DC(void *reserved, const SceUInt32 *taskId);
static int downloadPluginBgdl_811239E4(const FgdlTitleRequest *request);
static int downloadPluginTerminate(void);
static int downloadPluginBgdl_81123ACA(SceUInt32 taskId);
int downloadPluginReserved(void);			/* (0x81409E2C) */

static DownloadPluginInterface s_interface = {		/* (0x81544864) */
	downloadPluginNotify,
	downloadPluginSetPermission,
	downloadPluginBgdl_811239D4,
	downloadPluginBgdl_811239DC,
	downloadPluginTerminate,
	downloadPluginBgdl_811239E4,
	Bgdl_81123DD0,
	downloadPluginBgdl_81123ACA,
	downloadPluginReserved,
};

Plugin *downloadPluginGet(void)
{
	return s_plugin;
}

void downloadPluginInit(Plugin *plugin)
{
	s_plugin = plugin;
	plugin->SetInitEventHandler(s_initEvents);
	plugin->SetInterface(1, &s_interface);

	string wakeup = plugin->GetArg("wakeup_bgdl", NULL);

	if (wakeup.c_str()[0] == 'f' && sce_paf_strcmp(wakeup.c_str(), "false") == 0) {
		s_active = false;
	} else {
		s_active = true;

		bool clean = shellStartupNeedsInitialSetup();
		bool recovered;

		switch (shellBootGetDatabaseState()) {
		case -1:
			recovered = false;
			break;
		case 0:
			recovered = false;
			break;
		case 1:
			recovered = false;
			break;
		case 2:
			recovered = true;
			break;
		case 3:
			recovered = true;
			break;
		case 4:
			recovered = true;
			break;
		default:
			recovered = false;
			break;
		}
		if (!clean && recovered == true)
			clean = true;

		int quota[2] = { -1, -2 };

		s_logManager = new LogJobManager(clean);
		if (s_logManager != NULL)
			s_logManager->Bgdl_811240B6(quota);
		shellBgdlCreate(clean, quota);
		Fgdl_8112CD1A();
		s_dispatcher = new BgdlJobDispatcher();
		if (s_dispatcher != NULL)
			s_dispatcher->Bgdl_8113A6A8();
		s_object = new DownloadPluginObject();
		if (s_object != NULL)
			s_object->Bgdl_8112F28C();
		if (s_logManager != NULL) {
			int reserved[2] = { 0, 0 };

			s_logManager->Bgdl_81126418(&s_object, reserved);
			shellBgdlStart();
			SystemUpdateSvc::Bgdl_8112D38C();
			g_systemUpdateSvc->Bgdl_8112D508(plugin, s_object);
			g_systemUpdateSvc->Bgdl_81413CB0(false);
		}
	}
}

void downloadPluginStart(Plugin *plugin)
{
	if (s_active == true)
		shellBgdlSetPermission();
}

/* the manager and the objects made with it destroyed */
static inline void downloadPluginDestroy(void)
{
	if (s_active == true) {
		shellBgdlNotify();
		g_systemUpdateSvc->Bgdl_8112D7F0();
		Fgdl_8112CDB2();
		shellBgdlDestroy();
		if (s_logManager != NULL) {
			s_logManager->Bgdl_81123EF8();
			delete s_logManager;
			s_logManager = NULL;
		}
		if (s_object != NULL) {
			s_object->Bgdl_8112F270();
			delete s_object;
			s_object = NULL;
		}
		if (s_dispatcher != NULL) {
			s_dispatcher->Bgdl_8113A644();
			delete s_dispatcher;
			s_dispatcher = NULL;
		}
		SystemUpdateSvc::Bgdl_8112D42A();
		s_active = false;
	}
	s_plugin = NULL;
}

void downloadPluginExit(Plugin *plugin)
{
	downloadPluginDestroy();
}

/* a job of the given type queued for the dispatcher */
static inline void downloadPluginQueue(int type, void (*func)(void *), void *arg)
{
	common::SharedPtr<job::JobItem> job(new DownloadPluginApiJob(type, func, arg));

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
}

static int downloadPluginNotify(void (*func)(void *), void *arg)
{
	if (s_active == true && func != NULL)
		downloadPluginQueue(0, func, arg);
	return 0;
}

static int downloadPluginSetPermission(void (*func)(void *), void *arg)
{
	if (s_active == true && func != NULL)
		downloadPluginQueue(1, func, arg);
	return 0;
}

static int downloadPluginBgdl_811239D4(const FgdlCaller *caller, int reserved, SceUInt32 taskId)
{
	return Fgdl_8112BB36(caller, reserved, taskId);
}

static int downloadPluginBgdl_811239DC(void *reserved, const SceUInt32 *taskId)
{
	return Fgdl_8112B9A2(reserved, taskId);
}

static int downloadPluginBgdl_811239E4(const FgdlTitleRequest *request)
{
	return Fgdl_8112BCEC(request);
}

static int downloadPluginTerminate(void)
{
	downloadPluginDestroy();
	shellBgdlRemoveFolders();
	return 0;
}

static int downloadPluginBgdl_81123ACA(SceUInt32 taskId)
{
	BgdlManager *manager = bgdlManagerGet();

	if (manager != NULL) {
		EseTask *task = manager->FindTask(taskId);

		if (task != NULL && task->m_ParamCache.key111 == 2) {
			bgdlCancelTask(taskId);
			g_systemUpdateSvc->Bgdl_8112DA56();
		}
	}
	return 0;
}

int DownloadPluginApiJob::Run()
{
	switch (m_type) {
	case 0:
		g_systemUpdateSvc->Bgdl_81413CB0(true);
		shellBgdlNotify();
		if (m_func != NULL)
			m_func(m_arg);
		break;
	case 1:
		shellBgdlSetPermission();
		g_systemUpdateSvc->Bgdl_81413CB0(true);
		if (m_func != NULL)
			m_func(m_arg);
		break;
	}
	return -1;
}
