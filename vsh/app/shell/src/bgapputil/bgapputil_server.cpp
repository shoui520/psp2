/*
 * bgapputil_server.cpp: SceBgAppUtil shell IPMI handler (SceShellSvcServer 0x00180000..0x0018FFFF).
 * Launches a title's companion background executable (app0:eboot2.bin and its companion SFO).
 * 3.65-CEX: 0x813C3C7E - 0x813C45A3
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include <plugin.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <ipmi/session.h>
#include <ipmi/ipmi_error.h>

#include "../../../game/game_plugin/game_plugin.h"
#include "../livespace_db/lsdb_keys.h"
#include "../livespace_db/livespace_db_internal.h"

#define BGAPPUTIL_MAX_REQUESTS			16
#define BGAPPUTIL_METHOD_LAUNCH_BG_APP		0x00180000u
#define BGAPPUTIL_METHOD_LAUNCH_BG_APP_REPLY	0x00180001u
#define BGAPPUTIL_ERROR_BUSY			(-2146409216)	/* 0x80106500 */
#define BGAPPUTIL_IPMI_ERROR_UNSUPPORTED_METHOD	(-2147351103)	/* 0x800205C1 */
#define BGAPPUTIL_LAUNCH_ORIGIN_SELF_PATH	0x02u
#define BGAPPUTIL_SELF_CAPABILITY_VALID		0x01u

#define GAME_PLUGIN_SLOT_READ_PARAM_SFO		15		/* +60 */
#define GAME_PLUGIN_SLOT_GET_BG_SELF_INFO	16		/* +64 */

struct BgAppSelfLaunchInfo {
	SceUInt32 capabilityFlags;
	SceUInt32 phycontSizeKb;
	SceUInt32 budgetSizeKb;
	SceUInt32 reserved[4];
};

typedef int (*GamePluginReadParamSfoFn)(const char *sfoPath, LsdbAppInfoPtr *outInfo);
typedef int (*GamePluginGetBgSelfInfoFn)(const char *ebootPath, SceUInt8 *outAttr,
					 BgAppSelfLaunchInfo *outLaunchInfo);

extern "C" {
extern const char *sceLsdbGetDiscId(const LsdbAppInfoPtr *appInfo);
}

extern int shellSvcLiveAreaSyncDefault(IPMI::Session *session, SceUInt32 methodId,
				       const IPMI::DataInfo *inList, SceUInt32 inCount,
				       IPMI::BufferInfo *outList, SceUInt32 outCount); /* 0x8115B554 */
extern int shellFiosOverlayResolveSync(SceUID processId, int resolveFlags,
				       const char *inputPath, char *outPath,
				       SceSize outSize); /* 0x8144DA0C */

struct BgAppUtilRequestSlot {
	IPMI::Session *session;
	int requestId;
	paf::common::SharedPtr<paf::job::JobItem> job;
};

static paf::thread::Mutex s_bgAppUtilMutex("SceBgAppUtil", 0);	/* 0x815CC280 */
static BgAppUtilRequestSlot s_bgAppUtilSlots[BGAPPUTIL_MAX_REQUESTS]; /* 0x815CC2A0 */

/* 0x813C4568 */
static int bgAppUtilSendAsyncReply(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(BGAPPUTIL_METHOD_LAUNCH_BG_APP_REPLY,
						    requestId, result, NULL, 0);
}

class BgAppUtilLaunchJob : public paf::job::JobItem {
public:
	BgAppUtilLaunchJob(BgAppUtilRequestSlot *slot, IPMI::Session *session, int requestId)
		: paf::job::JobItem("SceBgAppUtil", NULL),
		  m_slot(slot),
		  m_session(session),
		  m_requestId(requestId),
		  m_clientPid(session->getProcessId()),
		  m_gameIface(NULL)
	{
	}

	virtual ~BgAppUtilLaunchJob() {}
	virtual int Run();
	virtual void Finish(int result) { (void)result; }

private:
	int ResolveDiscId(char *outTitleId);
	void CompleteSlot(int result);

	BgAppUtilRequestSlot *m_slot;
	IPMI::Session *m_session;
	int m_requestId;
	SceUID m_clientPid;
	GamePluginInterface *m_gameIface;
};

/* 0x813C3D00 */
int BgAppUtilLaunchJob::ResolveDiscId(char *outTitleId)
{
	char sfoPath[256];
	int ret = shellFiosOverlayResolveSync(m_clientPid, 0, "app0:sce_sys/param" "2.sfo",
					      sfoPath, sizeof(sfoPath));
	if (ret != 0)
		return ret;

	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	GamePluginReadParamSfoFn readParamSfo =
		reinterpret_cast<GamePluginReadParamSfoFn>(
			m_gameIface->func[GAME_PLUGIN_SLOT_READ_PARAM_SFO]);
	ret = readParamSfo(sfoPath, &appInfo);
	if (ret == 0) {
		const char *discId = sceLsdbGetDiscId(&appInfo);
		sce_paf_strlcpy(outTitleId, discId, 10);
	}
	return ret;
}

void BgAppUtilLaunchJob::CompleteSlot(int result)
{
	s_bgAppUtilMutex.Lock();
	if (m_slot->session == m_session && m_slot->requestId == m_requestId) {
		bgAppUtilSendAsyncReply(m_session, m_requestId, result);
		m_slot->session = NULL;
		m_slot->requestId = -1;
		m_slot->job.reset();
	}
	s_bgAppUtilMutex.Unlock();
}

/* 0x813C3E2A */
int BgAppUtilLaunchJob::Run()
{
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL) {
		CompleteSlot(-1);
		return 0;
	}

	m_gameIface = static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
	if (m_gameIface == NULL) {
		CompleteSlot(-1);
		return 0;
	}

	char ebootPath[256];
	SceIoStat fileStat;
	int ret = shellFiosOverlayResolveSync(m_clientPid, 0, "app0:eboot2.bin",
					      ebootPath, sizeof(ebootPath));
	if (ret == 0)
		ret = sceIoGetstat(ebootPath, &fileStat);

	if (ret != 0) {
		CompleteSlot(ret);
		return 0;
	}

	char titleId[12];
	if (ResolveDiscId(titleId) != 0)
		sce_paf_strlcpy(titleId, "NPXS10999", 10);

	SceAppMgrAppStatus callerStatus;
	ret = sceAppMgrGetStatusByAppId(m_session->getAppId(), &callerStatus);
	if (ret == 0) {
		if ((callerStatus.launchOrigin & BGAPPUTIL_LAUNCH_ORIGIN_SELF_PATH) != 0) {
			SceUInt8 selfAttr[4];
			BgAppSelfLaunchInfo selfInfo;
			GamePluginGetBgSelfInfoFn getBgSelfInfo =
				reinterpret_cast<GamePluginGetBgSelfInfoFn>(
					m_gameIface->func[GAME_PLUGIN_SLOT_GET_BG_SELF_INFO]);
			ret = getBgSelfInfo(ebootPath, selfAttr, &selfInfo);
			if (ret == 0 && (selfInfo.capabilityFlags & BGAPPUTIL_SELF_CAPABILITY_VALID) != 0) {
				AppmgrLaunchAppOption launchOption;
				sce_paf_memset(&launchOption, 0, sizeof(launchOption));
				launchOption.size = sizeof(launchOption);
				launchOption.pycontSize = selfInfo.phycontSizeKb << 10;
				ret = sceAppMgrLaunchAppByPath4(ebootPath, titleId,
								selfInfo.budgetSizeKb << 10,
								NULL, 1, &launchOption);
			}
		} else {
			ret = sceAppMgrLaunchAppByName(0, titleId, NULL);
		}
	}

	CompleteSlot(ret);
	return 0;
}

/* 0x813C41AA */
static int bgAppUtilEnqueueLaunch(IPMI::Session *session, int requestId)
{
	s_bgAppUtilMutex.Lock();

	BgAppUtilRequestSlot *freeSlot = NULL;
	for (unsigned int i = 0; i < BGAPPUTIL_MAX_REQUESTS; ++i) {
		if (s_bgAppUtilSlots[i].session == NULL) {
			freeSlot = &s_bgAppUtilSlots[i];
			break;
		}
	}

	if (freeSlot != NULL) {
		BgAppUtilLaunchJob *launchJob =
			new BgAppUtilLaunchJob(freeSlot, session, requestId);
		if (launchJob != NULL) {
			freeSlot->session = session;
			freeSlot->requestId = requestId;
			freeSlot->job = paf::common::SharedPtr<paf::job::JobItem>(launchJob);
			paf::job::JobQueue::default_queue->Enqueue(freeSlot->job);
			s_bgAppUtilMutex.Unlock();
			return 0;
		}
	}

	bgAppUtilSendAsyncReply(session, requestId, BGAPPUTIL_ERROR_BUSY);
	s_bgAppUtilMutex.Unlock();
	return 0;
}

/* 0x813C458A */
static int bgAppUtilDispatchAsync(IPMI::Session *session, SceUInt32 requestId,
				  SceUInt32 methodId)
{
	if (methodId == BGAPPUTIL_METHOD_LAUNCH_BG_APP)
		return bgAppUtilEnqueueLaunch(session, static_cast<int>(requestId));
	return BGAPPUTIL_IPMI_ERROR_UNSUPPORTED_METHOD;
}

/* 0x813C4354 */
int shellSvcDisp_18_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return shellSvcLiveAreaSyncDefault(session, methodId, inList, inCount, outList, outCount);
}

/* 0x813C4368 */
int shellSvcDisp_18_Async(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)inList;
	(void)inCount;
	return bgAppUtilDispatchAsync(session, requestId, methodId);
}

/* 0x813C4378 */
void shellSvcDisp_18_Disconn(IPMI::Session *session)
{
	s_bgAppUtilMutex.Lock();
	for (unsigned int i = 0; i < BGAPPUTIL_MAX_REQUESTS; ++i) {
		if (s_bgAppUtilSlots[i].session == session) {
			s_bgAppUtilSlots[i].job->Cancel();
			s_bgAppUtilSlots[i].session = NULL;
			s_bgAppUtilSlots[i].requestId = -1;
			s_bgAppUtilSlots[i].job.reset();
			break;
		}
	}
	s_bgAppUtilMutex.Unlock();
}
