/*
 * SceShell shell_svc: Activity / Status service (0x00080000..0x0008FFFF) handlers
 * 3.65-CEX: 0x811B99E0 - 0x811BCFBA (sti 0x811BCF48)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <ipmi/session.h>
#include <job/job.h>

#include "../../livespace_db/lsdb_keys.h"
#include "shell_svc_activity.h"

extern void *g_pafFramework;					/* 0x81488474 */

#include <activity_db/activity_db_internal.h>

extern "C" {
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *statusBuf);
}

static inline int shellSvcPostActivityStatus(void *db, const ShellSvcActivityRecord *rec)
{
	return static_cast<ActivityDbNetDaemon *>(db)->PostStatus(
		reinterpret_cast<const ActivityDbStatus *>(rec), 0x7FFFFFFFFFFFFFFFLL);
}

extern int shellSvcLiveAreaSyncDefault(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32); /* 0x8115B554 */

static paf::job::JobQueue *shellSvcActivityJobQueue()
{
	return paf::job::JobQueue::default_queue;
}

/* 0x811B99E0 */
int shellSvcActivityPostSystemSoftware(IPMI::Session *session, const char *titleId)
{
	(void)session;
	return shellSvcActivityPostSystemSoftwareImpl(titleId);
}

/* 0x811B99EA */
int shellSvcActivityPostTrophy(IPMI::Session *session, const char *titleId, const char *gameName,
	const char *trophyName, int grade, const char *uri)
{
	(void)session;
	ShellSvcActivityRecord rec;
	rec.m_type = 1;
	rec.m_titleId = titleId ? titleId : "";
	rec.m_uri = uri ? uri : "";

	int r = 0;
	if (sce_paf_strncmp(titleId, "NPXS10999", 0x20) != 0 &&
	    sce_paf_strncmp(titleId, "NPXS10998", 0x20) != 0) {
		r = shellSvcActivityCheckEnabled();
		if (r < 0)
			return 0;
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostTrophyStatusJob(1, rec,
			paf::string(gameName ? gameName : ""),
			paf::string(trophyName ? trophyName : ""),
			grade));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811B9F8E */
int shellSvcActivityPostHiddenTrophy(IPMI::Session *session, const char *titleId,
	const char *gameName, const char *uri)
{
	(void)session;
	ShellSvcActivityRecord rec;
	rec.m_type = 1;
	rec.m_titleId = titleId ? titleId : "";
	rec.m_uri = uri ? uri : "";

	int r = 0;
	if (sce_paf_strncmp(titleId, "NPXS10999", 0x20) != 0 &&
	    sce_paf_strncmp(titleId, "NPXS10998", 0x20) != 0) {
		r = shellSvcActivityCheckEnabled();
		if (r < 0)
			return 0;
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostTrophyStatusJob(2, rec,
			paf::string(gameName ? gameName : ""),
			paf::string(""),
			0));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811BA7DA */
int shellSvcActivityPostPlatinumTrophy(IPMI::Session *session, const char *titleId,
	const char *gameName, const char *uri, const char *trophyName)
{
	(void)session;
	ShellSvcActivityRecord rec;
	rec.m_type = 1;
	rec.m_titleId = titleId ? titleId : "";
	rec.m_uri = uri ? uri : "";

	int r = 0;
	if (sce_paf_strncmp(titleId, "NPXS10999", 0x20) != 0 &&
	    sce_paf_strncmp(titleId, "NPXS10998", 0x20) != 0) {
		r = shellSvcActivityCheckEnabled();
		if (r < 0)
			return 0;
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostTrophyStatusJob(3, rec,
			paf::string(gameName ? gameName : ""),
			paf::string(trophyName ? trophyName : ""),
			1));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811BAD7C */
int shellSvcActivityPostStorePurchase(IPMI::Session *session, const char *contentId, int isMedia,
	const char *itemName, const char *subName, const char *uri, const char *iconUri,
	int isPreorder, int mediaKind, int isFree)
{
	(void)session;
	ShellSvcActivityRecord rec;
	rec.m_type = 2;
	rec.m_uri = uri ? uri : "";
	rec.m_iconUri = iconUri ? iconUri : "";

	int r = shellSvcActivityCheckEnabled();
	if (r < 0)
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostStoreStatusJob(4, rec,
			paf::string(contentId ? contentId : ""), isMedia,
			paf::string(itemName ? itemName : ""),
			paf::string(subName ? subName : ""),
			0, isPreorder, mediaKind, isFree));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811BB362 */
int shellSvcActivityPostStoreRating(IPMI::Session *session, const char *contentId, int isMedia,
	const char *itemName, int count, const char *uri, const char *iconUri)
{
	(void)session;
	ShellSvcActivityRecord rec;
	rec.m_type = 2;
	rec.m_uri = uri ? uri : "";
	rec.m_iconUri = iconUri ? iconUri : "";

	int r = shellSvcActivityCheckEnabled();
	if (r < 0)
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostStoreStatusJob(5, rec,
			paf::string(contentId ? contentId : ""), isMedia,
			paf::string(itemName ? itemName : ""),
			paf::string(""),
			count, 0, 0, 0));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811BB91E */
int shellSvcActivityPostGameStatus(IPMI::Session *session, int actType, int useAppMgr,
	const char *titleId, const char *contentId, const char *body,
	const char *iconUri, const char *uri)
{
	ShellSvcActivityRecord rec;
	rec.m_type = actType;
	rec.m_body = body ? body : "";
	rec.m_iconUri = iconUri ? iconUri : "";
	rec.m_uri = uri ? uri : "";

	if (useAppMgr != 0) {
		int status[32];
		sce_paf_memset(status, 0, sizeof(status));
		int r = sceAppMgrGetStatusByAppId(session->getAppId(), status);
		if (r < 0)
			return r;

		rec.m_titleId = (const char *)&status[3];
		if (sce_paf_strncmp(rec.m_titleId.c_str(), "NPXS10999", 0x20) == 0 ||
		    sce_paf_strncmp(rec.m_titleId.c_str(), "NPXS10998", 0x20) == 0) {
			rec.m_contentId = "";
			return shellSvcPostActivityStatus(SceActivityDb_7B68F69D(), &rec);
		}

		r = shellSvcActivityCheckEnabled();
		if (r < 0)
			return r;

		paf::common::SharedPtr<paf::job::JobItem> job(
			new PostGameStatusJob(rec, 0, 0, paf::string("")));
		shellSvcActivityJobQueue()->Enqueue(job);
		return r;
	}

	rec.m_titleId = titleId ? titleId : "";
	rec.m_contentId = contentId ? contentId : "";
	return shellSvcPostActivityStatus(SceActivityDb_7B68F69D(), &rec);
}

/* 0x811BC434 */
int shellSvcActivityPostGameStatusWithUri(IPMI::Session *session, int actType,
	const char *body, SceUInt32 reqVer, const char *param)
{
	ShellSvcActivityRecord rec;
	rec.m_type = actType;
	rec.m_body = body ? body : "";
	rec.m_iconUri = "";
	rec.m_uri = "";

	int status[32];
	sce_paf_memset(status, 0, sizeof(status));
	int r = sceAppMgrGetStatusByAppId(session->getAppId(), status);
	if (r < 0)
		return r;

	rec.m_titleId = (const char *)&status[3];
	if (sce_paf_strncmp(rec.m_titleId.c_str(), "NPXS10999", 0x20) == 0 ||
	    sce_paf_strncmp(rec.m_titleId.c_str(), "NPXS10998", 0x20) == 0) {
		rec.m_contentId = "";
		paf::string paramStr(param ? param : "");
		r = shellSvcActivityBuildUri(&rec, reqVer, &paramStr);
		if (r < 0)
			return r;
		return shellSvcPostActivityStatus(SceActivityDb_7B68F69D(), &rec);
	}

	r = shellSvcActivityCheckEnabled();
	if (r < 0)
		return r;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PostGameStatusJob(rec, 1, reqVer, paf::string(param ? param : "")));
	shellSvcActivityJobQueue()->Enqueue(job);
	return r;
}

/* 0x811BCF22 */
int shellSvcDisp_08_Sync(IPMI::Session *s, SceUInt32 m, const IPMI::DataInfo *in,
	SceUInt32 inCount, IPMI::BufferInfo *out, SceUInt32 outCount)
{
	return shellSvcActivityDispatchSync(s, m, in, inCount, out, outCount);
}

/* 0x811BCF36 */
int shellSvcDisp_08_Async(IPMI::Session *s, SceUInt32 reqId, SceUInt32 m,
	const IPMI::DataInfo *in, SceUInt32 inCount)
{
	(void)reqId;
	return shellSvcLiveAreaSyncDefault(s, m, in, inCount, NULL, 0);
}

/* 0x811BCF46 */
void shellSvcDisp_08_Disconn(IPMI::Session *)
{
}
