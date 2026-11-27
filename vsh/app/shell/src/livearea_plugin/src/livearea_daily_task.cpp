/*
 * livearea_daily_task.cpp: livearea_plugin daily background update task ("SHELL0005";
 * 0x811A2CBC-0x811A3426, sti 0x811A33B2).
 */
#include <std/string.h>
#include "livearea_internal.h"

extern "C" {
const char *SceActivityDb_E6C23731(void);
void SceLsdb_FA25AF8D(AppInfoSClient *client, const char **categories, int count, int flags,
		      paf::vector<paf::string> *out);
int SceLsdb_87EE0526(AppInfoSClient *client, const paf::string *title, LsdbAppInfoPtr *outInfo);
int SceLsdb_D22CFA8C(void *ctx, const paf::string *title, SceUInt64 *outTimestamp);
int SceLsdb_DA0D7B0A(LsdbAppInfoPtr *appInfo);
int SceLsdb_44CD0D07(LsdbAppInfoPtr *appInfo);
int SceLsdb_F4325AD0(LsdbAppInfoPtr *appInfo);
}

#include "src/trigger_mgr/trigger_mgr.h"

static void liveareaDailyTaskRun(void *userdata);		/* 0x811A2D9A */
static void liveareaDailyTaskOnToggle(bool enabled, void *userdata); /* 0x811A3390 */

/* 0x811A2CBC */
LiveAreaTaskMgr *liveareaDailyTaskCtor(void *mem, int option)
{
	(void)option;
	return liveareaTaskMgrCtor(mem);
}

/* 0x811A2CC8 */
int liveareaDailyTaskStop(LiveAreaTaskMgr *taskMgr)
{
	(void)taskMgr;
	liveareaCronUnregister(liveareaDailyTaskRun);
	shellTriggerMgrClearServiceCallback("SHELL0005");
	return shellTriggerMgrUnregisterService("SHELL0005");
}

/* 0x811A2CEC */
LiveAreaTaskMgr *liveareaDailyTaskDtor(LiveAreaTaskMgr *taskMgr)
{
	liveareaCronUnregister(liveareaDailyTaskRun);
	shellTriggerMgrClearServiceCallback("SHELL0005");
	shellTriggerMgrUnregisterService("SHELL0005");
	return liveareaTaskMgrDtor(taskMgr);
}

/* 0x811A2D1C */
int liveareaDailyTaskStart(LiveAreaTaskMgr *taskMgr)
{
	int ret = shellTriggerMgrRegisterService("SHELL0005");
	if (ret != 0)
		return ret;
	ret = shellTriggerMgrSetServiceCallback("SHELL0005", liveareaDailyTaskOnToggle, taskMgr);
	if (ret != 0)
		return ret;
	bool enabled = false;
	ret = shellTriggerMgrIsServiceEnabled("SHELL0005", &enabled);
	if (ret != 0)
		return ret;
	if (enabled)
		liveareaCronRegister(liveareaDailyTaskRun, taskMgr);
	return 0;
}

/* 0x811A2D9A */
static void liveareaDailyTaskRun(void *userdata)
{
	LiveAreaTaskMgr *taskMgr = (LiveAreaTaskMgr *)userdata;
	paf::vector<paf::string> titles;
	int hasActivity = SceActivityDb_6D8000C1();

	AppInfoSClient lsdbClient;
	SceLsdb_D322A870(&lsdbClient);
	const char *categories[2] = { "gd", "gp" };
	SceLsdb_FA25AF8D(&lsdbClient, categories, 2, 6, &titles);
	SceLsdb_56D8DCF0(&lsdbClient);

	titles.push_back(paf::string("NPXS10002"));
	titles.push_back(paf::string("NPXS10010"));
	if (hasActivity != 0) {
		const char *activityTitle = SceActivityDb_E6C23731();
		titles.push_back(paf::string(activityTitle));
	}

	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin != NULL) {
		LiveAreaTelephonyInterface *telIf =
			(LiveAreaTelephonyInterface *)telPlugin->GetInterface(1);
		if (telIf != NULL) {
			LiveAreaTelephonyModem *modem = telIf->getModem();
			if (modem != NULL) {
				char *telTitle = NULL;
				if (modem->vtable->getCarrierAppTitleId(modem, &telTitle) == 0 &&
				    telTitle != NULL)
					titles.push_back(paf::string(telTitle));
			}
		}
	}

	SceUInt64 taskId = 0;
	for (size_t i = 0; i < titles.size(); ++i) {
		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		LsdbAppInfoPtr queryInfo(appInfo);
		AppInfoSClient queryClient;
		SceLsdb_D322A870(&queryClient);
		int ret = SceLsdb_87EE0526(&queryClient, &titles[i], &queryInfo);
		SceLsdb_56D8DCF0(&queryClient);
		if (ret != 0)
			return;

		SceUInt64 timestamp = 0;
		unsigned char tsHandle[4];
		if (SceLsdb_D22CFA8C(tsHandle, &titles[i], &timestamp) == 0) {
			if (SceLsdb_DA0D7B0A(&appInfo) != 0)
				liveareaTaskMgrRequestContentUpdate(taskMgr, &taskId,
								    titles[i].c_str(), &timestamp,
								    false, 0, 1, NULL, NULL, NULL);
			if (SceLsdb_44CD0D07(&appInfo) != 0) {
				int appType = sceLsdbGetType(&appInfo);
				liveareaTaskMgrRequestPatchCheck(taskMgr, &taskId,
								 titles[i].c_str(), false,
								 appType, NULL, NULL);
			}
			if (hasActivity != 0) {
				const char *activityTitle = SceActivityDb_E6C23731();
				if (sce_paf_strcmp(titles[i].c_str(), activityTitle) == 0 &&
				    SceLsdb_F4325AD0(&appInfo) != 0)
					liveareaTaskMgrRequestActivityUpdate(taskMgr, &taskId,
									     titles[i].c_str(),
									     0, false, NULL, NULL);
			}
		}
	}
	liveareaTaskMgrRequestActivityUpdate(taskMgr, &taskId, "NPXS10006", 0, false, NULL, NULL);
}

/* 0x811A3390 */
static void liveareaDailyTaskOnToggle(bool enabled, void *userdata)
{
	if (enabled)
		liveareaCronRegister(liveareaDailyTaskRun, userdata);
	else
		liveareaCronUnregister(liveareaDailyTaskRun);
}
