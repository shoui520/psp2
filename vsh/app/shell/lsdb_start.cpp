/*
 * The shell's databases at start: livespace_db.suprx loaded and told about the boot, the
 * activity database opened (ux0, else ur0), every content database checked, and the registry
 * callback that sends the LiveArea back to its defaults when the PSN country or language
 * changes. The file's and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <plugin.h>
#include <module/module.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include <appmgr/appmgr.h>
#include <sqlite3.h>
#include <dbutil/dcn_server.h>
#include <dbutil/dcn_client.h>
#include <activity_db/activity_db.h>
#include "boot.h"
#include "plugins.h"
#include "shell.h"
#include "db_check.h"
#include "src/livespace_db/livespace_db.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the activity database's open flags */
#define ACTIVITY_DB_OPEN_FLAGS	0x1000010A

/* the content databases the shell checks at start, by their DbCheck IDs */
#define SHELL_DB_CHECK_COUNT	8

static Module *s_lsdbModule;
static SceUID s_lsdbRegCallback = -1;
static int s_databaseNeedsRecovery;

void shellLsdbStart(void)
{
	s_lsdbModule = new Module("vs0:vsh/shell/livespace_db.suprx");
	SceLsdb_2944E643(shellStartupNeedsInitialSetup() ? -60 : 60);
	SceLsdb_AADCDCFC((void *)shellLsdbCallback);

	const char *keys[2] = { "/CONFIG/NP/country", "/CONFIG/NP/lang" };

	if (s_lsdbRegCallback == -1)
		return;
	if (sceRegMgrRegisterCallback(s_lsdbRegCallback) < 0) {
		sceKernelDeleteCallback(s_lsdbRegCallback);
		s_lsdbRegCallback = -1;
		return;
	}
	for (unsigned int i = 0; i < 2; i++) {
		if (sceRegMgrAddRegistryCallback(s_lsdbRegCallback, keys[i]) < 0) {
			sceKernelDeleteCallback(s_lsdbRegCallback);
			s_lsdbRegCallback = -1;
			return;
		}
	}
	if (sceRegMgrStartCallback(s_lsdbRegCallback) < 0) {
		sceRegMgrUnregisterCallback(s_lsdbRegCallback);
		sceKernelDeleteCallback(s_lsdbRegCallback);
		s_lsdbRegCallback = -1;
	}
}

/* the activity database: made again when the initial setup is pending or livespace_db's
   trigger table is stale */
void shellActivityDbOpen(void)
{
	char path[256];

	if (sceAppMgrGetUserDirPath(1, path, sizeof(path)) != 0) {
		sce_paf_strncpy(path, "ur0:shell/db/act.db", sizeof(path));
	} else {
		sce_paf_strncat(path, "/shell/db", sizeof(path));
		if (!Dir::IsDirectory(path))
			Dir::CreateRecursive(path, 6);
		sce_paf_strncat(path, "/act.db", sizeof(path));
	}

	bool reset = false;

	if (shellStartupNeedsInitialSetup() ||
	    SHELL_LSDB_TRIGGER_TABLE_STALE(SceLsdb_B0E42B67())) {
		reset = true;
		LocalFile::RemoveFile(path);
	}
	SceActivityDb_B5012F40(path, ACTIVITY_DB_OPEN_FLAGS);
	shellWhatsNewInit(reset);
}

static int shellLsdbOnRegistryChange(int notifyId, int notifyCount, int notifyArg, void *common);

/* the callback the registry changes come to (made before shellLsdbStart registers it) */
int shellLsdbRegCallbackCreate(void)
{
	if (s_lsdbRegCallback != -1)
		return s_lsdbRegCallback;

	SceUID cb = sceKernelCreateCallback("SceLsdbRegCb", 0, shellLsdbOnRegistryChange, NULL);

	if (cb >= 0)
		s_lsdbRegCallback = cb;
	return cb;
}

/* the databases' servers started and each content database checked: one that needs it is
   reset, one that is damaged is marked */
void shellDbCheckAll(void)
{
	DbCheckResult result;
	int ids[SHELL_DB_CHECK_COUNT] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	sqlite3_initialize();
	dbutil::DCNServer::Initialize();
	dbutil::DCNClient::Initialize(NULL);
	for (unsigned int i = 0; i < SHELL_DB_CHECK_COUNT; i++) {
		DbCheckParam param;
		int id = ids[i];

		param.path = NULL;
		param.initialPath = NULL;
		param.copyFlags = 6;
		param.dirMode = 0x106;
		param.reserved1 = 0;
		param.reserved2 = 0;
		param.id = id;
		param.option = DB_CHECK_OPTION_RESET_ON_VERSION;

		int ret = DbCheckOpen(&param, &result);

		if (ret == 0)
			continue;
		sce_paf_printf_dummy();
		if (ret == (int)SCE_DB_CHECK_ERROR_VERSION) {
			sce_paf_printf_dummy();
			DbCheckReset(id);
		} else if (ret == SCE_ERROR_ERRNO_ENODEV) {
			sce_paf_printf_dummy();
		} else {
			sce_paf_printf_dummy();
			DbCheckSetCorrupt(id, 1);
		}
	}
}

void shellDatabaseCheckRecovery(void)
{
	DbCheckNeedsRecovery(&s_databaseNeedsRecovery);
	sce_paf_printf_dummy();
}

int shellDatabaseNeedsRecovery(void)
{
	return s_databaseNeedsRecovery;
}

/* livearea_plugin's and topmenu_plugin's functions the revert job calls (indexes in their
   PLACEHOLDER tables) */
enum {
	LIVEAREA_PLUGIN_FUNC_REVERT	= 10,
};

enum {
	TOPMENU_PLUGIN_FUNC_UPDATE_LIVEAREA	= 45,
	TOPMENU_PLUGIN_FUNC_LIVEAREA_REVERT	= 64,
};

/* what topmenu_plugin is told about an application whose LiveArea was reverted */
struct ShellLiveAreaRevertInfo {
	const char *titleId;
	const char *liveareaPath;
	int index;
};

/* sends the LiveArea of the applications livespace_db finds back to its defaults */
class ShellLsdbLiveAreaRevertJob : public job::JobItem {
public:
	ShellLsdbLiveAreaRevertJob() : job::JobItem("SceLsdbLiveAreaRevertJob", NULL)
	{
	}

	virtual ~ShellLsdbLiveAreaRevertJob()
	{
	}

	virtual int Run()
	{
		int ret = FindTitles(1, &m_gameTitles);

		if (ret != 0)
			return ret;

		Plugin *livearea = Plugin::Find("livearea_plugin");

		if (livearea != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)livearea->GetInterface(1);

			if (iface != NULL) {
				for (vector<string>::iterator it = m_gameTitles.begin();
				     it != m_gameTitles.end(); ++it)
					((void (*)(const string *, int, int, int))
					 iface->func[LIVEAREA_PLUGIN_FUNC_REVERT])(&*it, 0, 0, 0);
			}
		}
		ret = FindTitles(0, &m_appTitles);
		if (ret != 0)
			return ret;
		return 0;
	}

	virtual void Finish(int result)
	{
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		if (result != 0)
			return;

		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu == NULL)
			return;

		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface == NULL)
			return;
		for (vector<string>::iterator it = m_gameTitles.begin(); it != m_gameTitles.end();
		     ++it) {
			ShellLiveAreaRevertInfo info;

			info.titleId = it->c_str();
			info.liveareaPath = m_liveareaPaths[*it].c_str();
			info.index = -1;
			((void (*)(ShellLiveAreaRevertInfo *))
			 iface->func[TOPMENU_PLUGIN_FUNC_UPDATE_LIVEAREA])(&info);
		}
		for (vector<string>::iterator it = m_appTitles.begin(); it != m_appTitles.end();
		     ++it) {
			ShellLiveAreaRevertInfo info;

			info.titleId = it->c_str();
			info.liveareaPath = m_liveareaPaths[*it].c_str();
			info.index = -1;
			((void (*)(ShellLiveAreaRevertInfo *))
			 iface->func[TOPMENU_PLUGIN_FUNC_UPDATE_LIVEAREA])(&info);
		}
	}

	/* the applications of one category group with a LiveArea of their own, and each one's
	   LiveArea path (0x8140A7EC; its query's conditions are livespace_db's, not written yet) */
	int FindTitles(int attribute, vector<string> *titles);

	vector<string> m_gameTitles;
	vector<string> m_appTitles;
	map<string, string> m_liveareaPaths;
};

/* the PSN country or language changed: the LiveAreas go back to their defaults */
static int shellLsdbOnRegistryChange(int notifyId, int notifyCount, int notifyArg, void *common)
{
	if (s_lsdbModule == NULL || shellBootIsLaunchRestricted())
		return 0;

	ShellLsdbLiveAreaRevertJob *job = new ShellLsdbLiveAreaRevertJob();
	common::SharedPtr<job::JobItem> revertJob(job);

	if (job != NULL) {
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		if (job::JobQueue::default_queue->Enqueue(revertJob) != 0)
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	}

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_LIVEAREA_REVERT]();
	}
	return 0;
}
