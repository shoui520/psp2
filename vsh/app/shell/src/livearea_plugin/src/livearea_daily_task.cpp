/*
 * livearea_daily_task.cpp: livearea_plugin daily background update task ("SHELL0005";
 * 0x811A2CBC-0x811A3426, sti 0x811A33B2). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <plugin.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/lsdb_keys.h"

extern "C" {
unsigned char SceActivityDb_6D8000C1(void);
const char *SceActivityDb_E6C23731(void);
void SceLsdb_D322A870(void *ctx);
void SceLsdb_FA25AF8D(void *ctx, const char **cats, int cnt, int flags, paf::vector<paf::string> *out);
void SceLsdb_56D8DCF0(void *ctx);
int SceLsdb_87EE0526(void *ctx, const paf::string *title, void *outSp);
int SceLsdb_D22CFA8C(void *ctx, const paf::string *title, SceUInt64 *outTs);
int SceLsdb_DA0D7B0A(void *sp);
int SceLsdb_44CD0D07(void *sp);
int SceLsdb_F4325AD0(void *sp);
int sceLsdbGetType(void *sp);
}

extern void *liveareaTaskMgrCtor(void *mem);			/* 0x811B204C */
extern void *liveareaTaskMgrDtor(void *obj);			/* 0x811B2068 */
extern int liveareaTaskMgrSub07C(void *obj);			/* 0x811B207C */
extern int liveareaTaskMgrSub0AC(void *obj);			/* 0x811B20AC */
extern int liveareaTaskMgrSub0C8(void *obj);			/* 0x811B20C8 */
extern void liveareaCronRegister(void (*cb)(void *), void *arg); /* 0x81199F26 */
extern void liveareaCronUnregister(void (*cb)(void *));		/* 0x81199FC0 */

extern int shellAutoSyncOpen(const char *id);			/* 0x812A4C6C */
extern int shellAutoSyncClose(const char *id);			/* 0x812A4CCE */
extern int shellAutoSyncIsEnabled(const char *id, bool *out);	/* 0x812A4FC8 */
extern int shellAutoSyncRegCb(const char *id, int (*cb)(char, void *), void *arg); /* 0x812A5032 */
extern int shellAutoSyncUnregCb(const char *id);		/* 0x812A509E */

static void liveareaDailyTaskRun(void *arg);			/* 0x811A2D9A */
static int liveareaDailyTaskOnToggle(char enabled, void *arg);	/* 0x811A3390 */

/* 0x811A2CBC */
void *liveareaDailyTaskCtor(void *mem, int arg)
{
	(void)arg;
	liveareaTaskMgrCtor(mem);
	return mem;
}

/* 0x811A2CC8 */
int liveareaDailyTaskStop(void *obj)
{
	(void)obj;
	liveareaCronUnregister(liveareaDailyTaskRun);
	shellAutoSyncUnregCb("SHELL0005");
	return shellAutoSyncClose("SHELL0005");
}

/* 0x811A2CEC */
void *liveareaDailyTaskDtor(void *obj)
{
	liveareaCronUnregister(liveareaDailyTaskRun);
	shellAutoSyncUnregCb("SHELL0005");
	shellAutoSyncClose("SHELL0005");
	liveareaTaskMgrDtor(obj);
	return obj;
}

/* 0x811A2D1C */
int liveareaDailyTaskStart(void *obj)
{
	int ret = shellAutoSyncOpen("SHELL0005");
	if (ret != 0)
		return ret;
	ret = shellAutoSyncRegCb("SHELL0005", liveareaDailyTaskOnToggle, obj);
	if (ret != 0)
		return ret;
	bool enabled = false;
	ret = shellAutoSyncIsEnabled("SHELL0005", &enabled);
	if (ret != 0)
		return ret;
	if (enabled)
		liveareaCronRegister(liveareaDailyTaskRun, obj);
	return 0;
}

/* 0x811A2D9A */
static void liveareaDailyTaskRun(void *arg)
{
	paf::vector<paf::string> titles;
	unsigned char hasAct = SceActivityDb_6D8000C1();

	unsigned char lsdbCtx[4];
	SceLsdb_D322A870(lsdbCtx);
	const char *cats[2] = { "gd", "gp" };
	SceLsdb_FA25AF8D(lsdbCtx, cats, 2, 6, &titles);
	SceLsdb_56D8DCF0(lsdbCtx);

	titles.push_back(paf::string("NPXS10002"));
	titles.push_back(paf::string("NPXS10010"));
	if (hasAct != 0) {
		const char *actTitle = SceActivityDb_E6C23731();
		titles.push_back(paf::string(actTitle));
	}

	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin != NULL) {
		void *telIf = telPlugin->GetInterface(1);
		if (telIf != NULL) {
			typedef void *(*GetTelObjFn)(void);
			void *telObj = (*(GetTelObjFn *)((char *)telIf + 8))();
			if (telObj != NULL) {
				char *telTitle = NULL;
				typedef int (*GetTelTitleFn)(void *, char **);
				if ((*(GetTelTitleFn **)(*(void **)telObj))[13](telObj, &telTitle) == 0 && telTitle != NULL)
					titles.push_back(paf::string(telTitle));
			}
		}
	}

	for (size_t i = 0; i < titles.size(); ++i) {
		paf::common::SharedPtr<int> itemSp(new int(0));
		paf::common::SharedPtr<int> querySp(itemSp);
		unsigned char qCtx[4];
		SceLsdb_D322A870(qCtx);
		int r = SceLsdb_87EE0526(qCtx, &titles[i], &querySp);
		SceLsdb_56D8DCF0(qCtx);
		if (r != 0)
			return;

		SceUInt64 ts = 0;
		unsigned char tsCtx[4];
		if (SceLsdb_D22CFA8C(tsCtx, &titles[i], &ts) == 0) {
			if (SceLsdb_DA0D7B0A(&itemSp) != 0)
				liveareaTaskMgrSub07C(arg);
			if (SceLsdb_44CD0D07(&itemSp) != 0) {
				sceLsdbGetType(&itemSp);
				liveareaTaskMgrSub0AC(arg);
			}
			if (hasAct != 0) {
				const char *actTitle = SceActivityDb_E6C23731();
				if (sce_paf_strcmp(titles[i].c_str(), actTitle) == 0 && SceLsdb_F4325AD0(&itemSp) != 0)
					liveareaTaskMgrSub0C8(arg);
			}
		}
	}
	liveareaTaskMgrSub0C8(arg);
}

/* 0x811A3390 */
static int liveareaDailyTaskOnToggle(char enabled, void *arg)
{
	if (enabled != 0)
		liveareaCronRegister(liveareaDailyTaskRun, arg);
	else
		liveareaCronUnregister(liveareaDailyTaskRun);
	return 0;
}
