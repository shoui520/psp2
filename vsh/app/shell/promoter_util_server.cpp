/*
 * promoter_util_server.cpp: SceShell ScePromoterUtil IPMI service (methods 0x130000-0x13001E).
 * 3.65-CEX: 0x812416D4 - 0x812443B4
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <plugin.h>

#include "shell.h"
#include "plugins.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/livespace_db/lsdb_keys.h"

namespace {

static const SceUInt32 kPromoterMethodAddPath = 0x130000U;
static const SceUInt32 kPromoterMethodAddPkg = 0x130001U;
static const SceUInt32 kPromoterMethodAddRif = 0x130002U;
static const SceUInt32 kPromoterMethodGetState = 0x130003U;
static const SceUInt32 kPromoterMethodGetResult = 0x130004U;
static const SceUInt32 kPromoterMethodGetSize = 0x130005U;
static const SceUInt32 kPromoterMethodGetStatus = 0x130006U;
static const SceUInt32 kPromoterMethodGetInfo = 0x130007U;
static const SceUInt32 kPromoterMethodGetProgress = 0x130008U;
static const SceUInt32 kPromoterMethodDeletePkg = 0x130009U;
static const SceUInt32 kPromoterMethodSetFlag = 0x13000AU;
static const SceUInt32 kPromoterMethodStartGetSize = 0x13000BU;
static const SceUInt32 kPromoterMethodUpdateUpgradable = 0x13000CU;
static const SceUInt32 kPromoterMethodPromoteImport = 0x13000DU;
static const SceUInt32 kPromoterMethodUpdateLiveArea = 0x13000EU;
static const SceUInt32 kPromoterMethodInit = 0x13000FU;
static const SceUInt32 kPromoterMethodExit = 0x130010U;
static const SceUInt32 kPromoterMethodGetPendingCount = 0x130011U;
static const SceUInt32 kPromoterMethodGetPendingTitle = 0x130012U;
static const SceUInt32 kPromoterMethodCheckExist = 0x130013U;
static const SceUInt32 kPromoterMethodStartStatus = 0x130014U;
static const SceUInt32 kPromoterMethodEndStatus = 0x130015U;
static const SceUInt32 kPromoterMethodStartName = 0x130016U;
static const SceUInt32 kPromoterMethodEndName = 0x130017U;
static const SceUInt32 kPromoterMethodGetPkgInfo = 0x130018U;
static const SceUInt32 kPromoterMethodGetPkgStatus = 0x130019U;
static const SceUInt32 kPromoterMethodStartInfo = 0x13001AU;
static const SceUInt32 kPromoterMethodEndInfo = 0x13001BU;
static const SceUInt32 kPromoterMethodStartProgress = 0x13001CU;
static const SceUInt32 kPromoterMethodEndProgress = 0x13001DU;
static const SceUInt32 kPromoterMethodRemoveSavedata = 0x13001EU;

static const int kPromoterErrNoPlugin = -2136014811;	/* 0x80AF0025 */
static const int kPromoterErrBusy = -2136014815;	/* 0x80AF0021 */
static const int kPromoterErrInvalidParam = -2136014705; /* 0x80AF008F */
static const int kPromoterErrNoMemory = -2147418100;	/* 0x8001000C */
static const int kPromoterErrBadState = -2147418090;	/* 0x80010016 */
static const int kIpmiErrUnsupported = -2147351103;	/* 0x800205C1 */

static const int kGameSlotGetStorage = 1;		/* +4 */
static const int kGameSlotGetPendingTitles = 9;		/* +36 */
static const int kGameSlotPromotePath = 11;		/* +44 */
static const int kGameSlotPromoteImport = 12;		/* +48 */
static const int kGameSlotInstallPkg = 13;		/* +52 */
static const int kGameSlotDeletePkg = 18;		/* +72 */
static const int kGameSlotSetFlag = 19;			/* +76 */
static const int kGameSlotGetSize = 20;			/* +80 */
static const int kGameSlotUpdateUpgradable = 22;	/* +88 */
static const int kGameSlotGetPkgInfo = 23;		/* +92 */
static const int kGameSlotStartStatus = 54;		/* +216 */
static const int kGameSlotStartName = 55;		/* +220 */
static const int kGameSlotPromoteRif = 57;		/* +228 */
static const int kGameSlotGetPkgStatus = 58;		/* +232 */
static const int kGameSlotStartInfo = 60;		/* +240 */
static const int kGameSlotStartProgress = 61;		/* +244 */
static const int kGameSlotRemoveSavedata = 64;		/* +256 */

static const int kLiveAreaSlotUpdate = 10;		/* +40 */

static const SceSize kPromoterPathBufSize = 128;
static const SceSize kPromoterPkgInfoSize = 232;
static const SceSize kPromoterPkgStatusSize = 72;
static const SceSize kPromoterStatusBufSize = 48;
static const SceSize kPromoterInfoBufSize = 16;

typedef void (*PromoterProgressFn)(float progress);
typedef void (*PromoterResultFn)(int *result);
typedef void (*PromoterSizeResultFn)(int *result, int unused, SceUInt32 low, SceUInt32 high);
typedef void (*PromoterBufferResultFn)(int *result, const void *buf);

struct PromoterImportJobParam {
	SceUInt8 header[148];
	PromoterProgressFn progressCallback;
	void *progressUserData;
	SceUInt8 tail[20];
};

struct PromoterPkgJobParam {
	SceUInt8 header[136];
	PromoterProgressFn progressCallback;
	void *progressUserData;
	SceUInt8 tail[144];
};

struct PromoterRifJobParam {
	SceUInt8 header[132];
	PromoterProgressFn progressCallback;
	void *progressUserData;
};

static paf::Plugin *s_gamePlugin = NULL;
static paf::job::JobQueue *s_promoterJobQueue = NULL;
static int s_promoterState = 0;
static int s_promoterResult = 0;
static SceUInt32 s_promoterSizeLow = 0;
static SceUInt32 s_promoterSizeHigh = 0;
static SceUInt8 s_promoterInfoBuf[kPromoterInfoBufSize];
static SceUInt8 s_promoterStatusBuf[kPromoterStatusBufSize];
static float s_promoterProgress = 0.0f;
static SceUInt32 s_promoterDeleteAttr = 0;
static paf::vector<SceUID> s_clientProcesses;

static void onPromoterProgress(float progress)
{
	s_promoterProgress = progress;
}

static void onPromoterFinishBasic(int *result)
{
	s_promoterResult = *result;
	s_promoterState = 0;
}

static void onPromoterFinishSize(int *result, int unused, SceUInt32 low, SceUInt32 high)
{
	(void)unused;
	s_promoterResult = *result;
	s_promoterSizeLow = low;
	s_promoterSizeHigh = high;
	s_promoterState = 0;
}

static void onPromoterFinishStatus(int *result, const void *statusBuf)
{
	s_promoterResult = *result;
	s_promoterState = 0;
	sce_paf_memcpy(s_promoterStatusBuf, statusBuf, kPromoterStatusBufSize);
}

static void onPromoterFinishInfo(int *result, const void *infoBuf)
{
	s_promoterResult = *result;
	s_promoterState = 0;
	sce_paf_memcpy(s_promoterInfoBuf, infoBuf, kPromoterInfoBufSize);
}

static void onLiveAreaUpdateComplete(int result)
{
	s_promoterResult = result;
	s_promoterState = 0;
}

static int respondEndStatus(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(kPromoterMethodEndStatus, requestId, result,
						    NULL, 0);
}

static int respondEndName(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(kPromoterMethodEndName, requestId, result,
						    NULL, 0);
}

static int respondEndInfo(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(kPromoterMethodEndInfo, requestId, result,
						    NULL, 0);
}

static int respondEndProgress(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(kPromoterMethodEndProgress, requestId, result,
						    NULL, 0);
}

static bool isPromoterJobCanceled(void *jobPtr)
{
	if (jobPtr == NULL)
		return false;
	return ((paf::job::JobItem *)jobPtr)->IsCanceled();
}

/* --- PromoterUtil JobItem subclasses --- */

class PromoterAddPathJob : public paf::job::JobItem {
public:
	PromoterAddPathJob(paf::Plugin *gamePlugin, const char *path, PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_path(NULL),
		  m_doneCb(doneCb)
	{
		m_path = new char[kPromoterPathBufSize];
		if (m_path != NULL) {
			sce_paf_strncpy(m_path, path, kPromoterPathBufSize - 1);
			m_path[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterAddPathJob()
	{
		if (m_path != NULL)
			delete[] m_path;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*promotePath)(const char *) =
			(int (*)(const char *))iface->func[kGameSlotPromotePath];
		return promotePath(m_path);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	char *m_path;
	PromoterResultFn m_doneCb;
};

class PromoterAddPkgJob : public paf::job::JobItem {
public:
	PromoterAddPkgJob(paf::Plugin *gamePlugin, const PromoterPkgJobParam *pkgParam,
			  PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_param(NULL),
		  m_doneCb(doneCb)
	{
		m_param = new PromoterPkgJobParam;
		if (m_param != NULL)
			sce_paf_memcpy(m_param, pkgParam, sizeof(PromoterPkgJobParam));
	}

	virtual ~PromoterAddPkgJob()
	{
		if (m_param != NULL)
			delete m_param;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*installPkg)(const PromoterPkgJobParam *) =
			(int (*)(const PromoterPkgJobParam *))iface->func[kGameSlotInstallPkg];
		return installPkg(m_param);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	PromoterPkgJobParam *m_param;
	PromoterResultFn m_doneCb;
};

class PromoterImportJob : public paf::job::JobItem {
public:
	PromoterImportJob(paf::Plugin *gamePlugin, const PromoterImportJobParam *importParam,
			  PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_param(NULL),
		  m_doneCb(doneCb)
	{
		m_param = new PromoterImportJobParam;
		if (m_param != NULL)
			sce_paf_memcpy(m_param, importParam, sizeof(PromoterImportJobParam));
	}

	virtual ~PromoterImportJob()
	{
		if (m_param != NULL)
			delete m_param;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*promoteImport)(const PromoterImportJobParam *) =
			(int (*)(const PromoterImportJobParam *))iface->func[kGameSlotPromoteImport];
		return promoteImport(m_param);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	PromoterImportJobParam *m_param;
	PromoterResultFn m_doneCb;
};

class PromoterAddRifJob : public paf::job::JobItem {
public:
	PromoterAddRifJob(paf::Plugin *gamePlugin, const PromoterRifJobParam *rifParam,
			  PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_param(NULL),
		  m_doneCb(doneCb)
	{
		m_param = new PromoterRifJobParam;
		if (m_param != NULL)
			sce_paf_memcpy(m_param, rifParam, sizeof(PromoterRifJobParam));
	}

	virtual ~PromoterAddRifJob()
	{
		if (m_param != NULL)
			delete m_param;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*promoteRif)(const PromoterRifJobParam *) =
			(int (*)(const PromoterRifJobParam *))iface->func[kGameSlotPromoteRif];
		return promoteRif(m_param);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	PromoterRifJobParam *m_param;
	PromoterResultFn m_doneCb;
};

class PromoterDeletePkgJob : public paf::job::JobItem {
public:
	PromoterDeletePkgJob(paf::Plugin *gamePlugin, const char *titleId, SceUInt32 *attrOut,
			     PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_attrOut(attrOut),
		  m_doneCb(doneCb)
	{
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterDeletePkgJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*deletePkg)(const char *, SceUInt32 *, int, int) =
			(int (*)(const char *, SceUInt32 *, int, int))
				iface->func[kGameSlotDeletePkg];
		return deletePkg(m_titleId, m_attrOut, 0, 0);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	SceUInt32 *m_attrOut;
	PromoterResultFn m_doneCb;
};

class PromoterSetFlagJob : public paf::job::JobItem {
public:
	PromoterSetFlagJob(paf::Plugin *gamePlugin, const char *titleId, int flagValue,
			   SceUInt32 *attrOut, PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_flagValue(flagValue),
		  m_attrOut(attrOut),
		  m_doneCb(doneCb)
	{
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterSetFlagJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*setFlag)(const char *, int, SceUInt32 *, int, int) =
			(int (*)(const char *, int, SceUInt32 *, int, int))
				iface->func[kGameSlotSetFlag];
		return setFlag(m_titleId, m_flagValue, m_attrOut, 0, 0);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	int m_flagValue;
	SceUInt32 *m_attrOut;
	PromoterResultFn m_doneCb;
};

class PromoterGetSizeJob : public paf::job::JobItem {
public:
	PromoterGetSizeJob(paf::Plugin *gamePlugin, const char *titleId,
			   PromoterSizeResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_sizeOut(0),
		  m_doneCb(doneCb)
	{
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterGetSizeJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*getSize)(const char *, int, int, SceUInt64 *) =
			(int (*)(const char *, int, int, SceUInt64 *))iface->func[kGameSlotGetSize];
		return getSize(m_titleId, 0, 0, &m_sizeOut);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code, result, (SceUInt32)(m_sizeOut & 0xFFFFFFFFULL),
				 (SceUInt32)(m_sizeOut >> 32));
		}
	}

	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	SceUInt64 m_sizeOut;
	PromoterSizeResultFn m_doneCb;
};

class PromoterUpdateUpgradableJob : public paf::job::JobItem {
public:
	PromoterUpdateUpgradableJob(paf::Plugin *gamePlugin, const char *titleId,
				    PromoterResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_doneCb(doneCb)
	{
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterUpdateUpgradableJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*updateUpgradable)(const char *) =
			(int (*)(const char *))iface->func[kGameSlotUpdateUpgradable];
		return updateUpgradable(m_titleId);
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	PromoterResultFn m_doneCb;
};

class PromoterAsyncStatusJob : public paf::job::JobItem {
public:
	PromoterAsyncStatusJob(IPMI::Session *session, int requestId, paf::Plugin *gamePlugin,
			       const char *titleId, int queryOption, PromoterSizeResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_session(session),
		  m_requestId(requestId),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_queryOption(queryOption),
		  m_doneCb(doneCb),
		  m_sizeOut(0)
	{
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterAsyncStatusJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*queryStatus)(const char *, int, int, int, SceUInt64 *) =
			(int (*)(const char *, int, int, int, SceUInt64 *))
				iface->func[kGameSlotStartStatus];
		return queryStatus(m_titleId, m_queryOption, 0, 0, &m_sizeOut);
	}

	virtual void Finish(int result)
	{
		int code = result;
		if (m_doneCb != NULL) {
			m_doneCb(&code, result, (SceUInt32)(m_sizeOut & 0xFFFFFFFFULL),
				 (SceUInt32)(m_sizeOut >> 32));
		}
		if (m_session != NULL && m_requestId != -1)
			respondEndStatus(m_session, m_requestId, code);
	}

	IPMI::Session *m_session;
	int m_requestId;
	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	int m_queryOption;
	PromoterSizeResultFn m_doneCb;
	SceUInt64 m_sizeOut;
};

class PromoterAsyncNameJob : public paf::job::JobItem {
public:
	PromoterAsyncNameJob(IPMI::Session *session, int requestId, paf::Plugin *gamePlugin,
			     const char *titleId, PromoterBufferResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_session(session),
		  m_requestId(requestId),
		  m_gamePlugin(gamePlugin),
		  m_titleId(NULL),
		  m_doneCb(doneCb)
	{
		sce_paf_memset(m_infoBuf, 0, sizeof(m_infoBuf));
		m_titleId = new char[kPromoterPathBufSize];
		if (m_titleId != NULL) {
			sce_paf_strncpy(m_titleId, titleId, kPromoterPathBufSize - 1);
			m_titleId[kPromoterPathBufSize - 1] = '\0';
		}
	}

	virtual ~PromoterAsyncNameJob()
	{
		if (m_titleId != NULL)
			delete[] m_titleId;
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*queryName)(const char *, int, int, void *) =
			(int (*)(const char *, int, int, void *))iface->func[kGameSlotStartName];
		return queryName(m_titleId, 0, 0, m_infoBuf);
	}

	virtual void Finish(int result)
	{
		int code = result;
		if (m_doneCb != NULL)
			m_doneCb(&code, m_infoBuf);
		if (m_session != NULL && m_requestId != -1)
			respondEndName(m_session, m_requestId, code);
	}

	IPMI::Session *m_session;
	int m_requestId;
	paf::Plugin *m_gamePlugin;
	char *m_titleId;
	PromoterBufferResultFn m_doneCb;
	SceUInt8 m_infoBuf[kPromoterInfoBufSize];
};

class PromoterAsyncInfoJob : public paf::job::JobItem {
public:
	PromoterAsyncInfoJob(IPMI::Session *session, int requestId, paf::Plugin *gamePlugin,
			     const char *titleId, int queryOption, PromoterBufferResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_session(session),
		  m_requestId(requestId),
		  m_gamePlugin(gamePlugin),
		  m_titleId((titleId != NULL) ? titleId : ""),
		  m_queryOption(queryOption),
		  m_doneCb(doneCb)
	{
		sce_paf_memset(m_statusBuf, 0, sizeof(m_statusBuf));
	}

	virtual ~PromoterAsyncInfoJob()
	{
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*queryInfo)(const char *, int, int, int, void *) =
			(int (*)(const char *, int, int, int, void *))
				iface->func[kGameSlotStartInfo];
		return queryInfo(m_titleId.c_str(), m_queryOption, 0, 0, m_statusBuf);
	}

	virtual void Finish(int result)
	{
		int code = result;
		if (m_doneCb != NULL)
			m_doneCb(&code, m_statusBuf);
		if (m_session != NULL && m_requestId != -1)
			respondEndInfo(m_session, m_requestId, code);
	}

	IPMI::Session *m_session;
	int m_requestId;
	paf::Plugin *m_gamePlugin;
	paf::string m_titleId;
	int m_queryOption;
	PromoterBufferResultFn m_doneCb;
	SceUInt8 m_statusBuf[kPromoterStatusBufSize];
};

class PromoterAsyncProgressJob : public paf::job::JobItem {
public:
	PromoterAsyncProgressJob(IPMI::Session *session, int requestId, paf::Plugin *gamePlugin,
				 const char *titleId, int queryOption,
				 PromoterBufferResultFn doneCb)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_session(session),
		  m_requestId(requestId),
		  m_gamePlugin(gamePlugin),
		  m_titleId((titleId != NULL) ? titleId : ""),
		  m_queryOption(queryOption),
		  m_doneCb(doneCb)
	{
		sce_paf_memset(m_statusBuf, 0, sizeof(m_statusBuf));
	}

	virtual ~PromoterAsyncProgressJob()
	{
	}

	virtual int Run()
	{
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return -1;
		int (*queryProgress)(const char *, int, int, int, void *) =
			(int (*)(const char *, int, int, int, void *))
				iface->func[kGameSlotStartProgress];
		return queryProgress(m_titleId.c_str(), m_queryOption, 0, 0, m_statusBuf);
	}

	virtual void Finish(int result)
	{
		int code = result;
		if (m_doneCb != NULL)
			m_doneCb(&code, m_statusBuf);
		if (m_session != NULL && m_requestId != -1)
			respondEndProgress(m_session, m_requestId, code);
	}

	IPMI::Session *m_session;
	int m_requestId;
	paf::Plugin *m_gamePlugin;
	paf::string m_titleId;
	int m_queryOption;
	PromoterBufferResultFn m_doneCb;
	SceUInt8 m_statusBuf[kPromoterStatusBufSize];
};

class PromoterRemoveSavedataJob : public paf::job::JobItem {
public:
	PromoterRemoveSavedataJob(paf::Plugin *gamePlugin, PromoterResultFn doneCb,
				  const char *titleId, int removeScope, int removeFlags)
		: paf::job::JobItem("ScePromoterUtilJob", NULL),
		  m_gamePlugin(gamePlugin),
		  m_doneCb(doneCb),
		  m_titleId((titleId != NULL) ? titleId : ""),
		  m_removeScope(removeScope),
		  m_removeFlags(removeFlags)
	{
	}

	virtual ~PromoterRemoveSavedataJob()
	{
	}

	virtual int Run()
	{
		if (m_gamePlugin == NULL)
			return kPromoterErrBadState;
		ShellPluginInterface *iface =
			(ShellPluginInterface *)m_gamePlugin->GetInterface(1);
		if (iface == NULL)
			return kPromoterErrNoPlugin;
		int (*removeSavedata)(const char *, int, int, bool (*)(void *), void *) =
			(int (*)(const char *, int, int, bool (*)(void *), void *))
				iface->func[kGameSlotRemoveSavedata];
		int ret = removeSavedata(m_titleId.c_str(), m_removeScope, m_removeFlags,
					 isPromoterJobCanceled, this);
		return (ret >= 0) ? 0 : ret;
	}

	virtual void Finish(int result)
	{
		if (m_doneCb != NULL) {
			int code = result;
			m_doneCb(&code);
		}
	}

	paf::Plugin *m_gamePlugin;
	PromoterResultFn m_doneCb;
	paf::string m_titleId;
	int m_removeScope;
	int m_removeFlags;
};

/* --- Method handlers --- */

static int handleGetState(int *outState)
{
	*outState = s_promoterState;
	return 0;
}

static int handleGetResult(int *outResult)
{
	*outResult = s_promoterResult;
	return 0;
}

static int handleGetSize(int *outResult, SceUInt32 *outSizePair)
{
	*outResult = s_promoterResult;
	outSizePair[0] = s_promoterSizeLow;
	outSizePair[1] = s_promoterSizeHigh;
	return 0;
}

static int handleGetStatus(int *outResult, void *outStatus)
{
	*outResult = s_promoterResult;
	sce_paf_memcpy(outStatus, s_promoterStatusBuf, kPromoterStatusBufSize);
	return 0;
}

static int handleGetInfo(int *outResult, void *outInfo)
{
	*outResult = s_promoterResult;
	sce_paf_memcpy(outInfo, s_promoterInfoBuf, kPromoterInfoBufSize);
	return 0;
}

static int handleGetProgress(float *outProgress)
{
	if (s_promoterState != 1)
		return kPromoterErrBusy;
	if (outProgress != NULL)
		*outProgress = s_promoterProgress;
	return 0;
}

static int handleInit(IPMI::Session *session)
{
	s_gamePlugin = paf::Plugin::Find("game_plugin");
	if (s_gamePlugin == NULL)
		return kPromoterErrNoPlugin;

	SceUID pid = session->getProcessId();
	bool found = false;
	for (size_t idx = 0; idx < s_clientProcesses.size(); ++idx) {
		if (s_clientProcesses[idx] == pid) {
			found = true;
			break;
		}
	}
	if (!found)
		s_clientProcesses.push_back(pid);

	if (s_promoterJobQueue != NULL)
		return 0;

	paf::job::JobQueue::Option option;
	option.priority = 0x10000100;
	option.stackSize = 0x4000;
	option.numThreads = 1;
	s_promoterJobQueue = new paf::job::JobQueue("ScePromoterJobQueue", &option);
	return (s_promoterJobQueue != NULL) ? 0 : kPromoterErrNoMemory;
}

static int handleExit(IPMI::Session *session)
{
	SceUID pid = session->getProcessId();
	for (paf::vector<SceUID>::iterator it = s_clientProcesses.begin();
	     it != s_clientProcesses.end(); ++it) {
		if (*it == pid) {
			s_clientProcesses.erase(it);
			break;
		}
	}

	if (s_clientProcesses.empty() && s_promoterJobQueue != NULL) {
		delete s_promoterJobQueue;
		s_promoterJobQueue = NULL;
	}
	return 0;
}

static int handleAddPath(const char *path)
{
	s_promoterState = 1;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (path == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAddPathJob(gamePlugin, path, onPromoterFinishBasic));
	int ret = s_promoterJobQueue->Enqueue(job);
	return (ret >= 0) ? 0 : ret;
}

static int handleAddPkg(const PromoterPkgJobParam *request)
{
	s_promoterState = 1;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (request == NULL)
		return kPromoterErrInvalidParam;

	PromoterPkgJobParam localParam;
	sce_paf_memcpy(&localParam, request, sizeof(localParam));
	localParam.progressCallback = onPromoterProgress;
	localParam.progressUserData = NULL;
	s_promoterProgress = 0.0f;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAddPkgJob(gamePlugin, &localParam, onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleAddRif(const PromoterRifJobParam *request)
{
	s_promoterState = 1;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (request == NULL)
		return kPromoterErrInvalidParam;

	PromoterRifJobParam localParam;
	sce_paf_memcpy(&localParam, request, sizeof(localParam));
	localParam.progressCallback = onPromoterProgress;
	localParam.progressUserData = NULL;
	s_promoterProgress = 0.0f;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAddRifJob(gamePlugin, &localParam, onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handlePromoteImport(const PromoterImportJobParam *request)
{
	s_promoterState = 1;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (request == NULL)
		return kPromoterErrInvalidParam;

	PromoterImportJobParam localParam;
	sce_paf_memcpy(&localParam, request, sizeof(localParam));
	localParam.progressCallback = onPromoterProgress;
	localParam.progressUserData = NULL;
	s_promoterProgress = 0.0f;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterImportJob(gamePlugin, &localParam, onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleDeletePkg(const char *titleId)
{
	s_promoterState = 2;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterDeletePkgJob(gamePlugin, titleId, &s_promoterDeleteAttr,
					 onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleSetFlag(const char *titleId, int flagValue)
{
	s_promoterState = 2;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterSetFlagJob(gamePlugin, titleId, flagValue, &s_promoterDeleteAttr,
				       onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleStartGetSize(const char *titleId)
{
	s_promoterState = 3;
	s_promoterResult = -1;
	s_promoterSizeLow = 0;
	s_promoterSizeHigh = 0;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterGetSizeJob(gamePlugin, titleId, onPromoterFinishSize));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleUpdateUpgradable(const char *titleId)
{
	s_promoterState = 4;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterUpdateUpgradableJob(gamePlugin, titleId, onPromoterFinishBasic));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleUpdateLiveArea(const char *titleId, const char *path)
{
	s_promoterState = 1;
	s_promoterResult = -1;
	paf::Plugin *liveAreaPlugin = paf::Plugin::Find("livearea_plugin");
	if (liveAreaPlugin == NULL)
		return kPromoterErrNoPlugin;
	ShellPluginInterface *iface =
		(ShellPluginInterface *)liveAreaPlugin->GetInterface(1);
	if (iface == NULL)
		return kPromoterErrNoPlugin;

	int (*updateLiveArea)(const char *, const char *, void (*)(int), void *) =
		(int (*)(const char *, const char *, void (*)(int), void *))
			iface->func[kLiveAreaSlotUpdate];
	return updateLiveArea(titleId, path, onLiveAreaUpdateComplete, NULL);
}

static int handleGetPendingCount(int *outCount)
{
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (outCount == NULL)
		return kPromoterErrInvalidParam;

	ShellPluginInterface *iface = (ShellPluginInterface *)gamePlugin->GetInterface(1);
	if (iface == NULL)
		return -1;

	paf::vector<paf::string> titles;
	int (*getPending)(paf::vector<paf::string> *) =
		(int (*)(paf::vector<paf::string> *))iface->func[kGameSlotGetPendingTitles];
	int ret = getPending(&titles);
	if (ret == 0)
		*outCount = (int)titles.size();
	return ret;
}

static int handleGetPendingTitle(const int *indexPtr, char *outBuf, SceSize bufSize)
{
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (indexPtr == NULL || outBuf == NULL || bufSize == 0)
		return kPromoterErrInvalidParam;

	ShellPluginInterface *iface = (ShellPluginInterface *)gamePlugin->GetInterface(1);
	if (iface == NULL)
		return -1;

	paf::vector<paf::string> titles;
	int (*getPending)(paf::vector<paf::string> *) =
		(int (*)(paf::vector<paf::string> *))iface->func[kGameSlotGetPendingTitles];
	int ret = getPending(&titles);
	if (ret == 0) {
		SceUInt32 index = (SceUInt32)(*indexPtr);
		if (index < (SceUInt32)titles.size())
			sce_paf_strncpy(outBuf, titles[index].c_str(), bufSize - 1);
	}
	return ret;
}

static int handleCheckExist(const char *titleId, int *outStorage)
{
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL || outStorage == NULL)
		return kPromoterErrInvalidParam;

	ShellPluginInterface *iface = (ShellPluginInterface *)gamePlugin->GetInterface(1);
	if (iface == NULL)
		return -1;

	AppInfoQClient client;
	SceLsdb_A6D8772C(&client);
	int ret = SceLsdb_E90B81CD(&client);
	if (ret == 0) {
		paf::common::SharedPtr<LsdbAppInfo> appInfo(new LsdbAppInfo());
		paf::string titleStr(titleId);
		ret = SceLsdb_D239031A(&client, &titleStr, &appInfo);
		if (ret == 0) {
			int storage = 0;
			int (*getStorage)(paf::common::SharedPtr<LsdbAppInfo> *, int *) =
				(int (*)(paf::common::SharedPtr<LsdbAppInfo> *, int *))
					iface->func[kGameSlotGetStorage];
			ret = getStorage(&appInfo, &storage);
			if (ret == 0)
				*outStorage = storage;
		}
	}
	SceLsdb_E2101B03(&client);
	return ret;
}

static int handleGetPkgInfo(const char *titleId, void *outInfo, SceSize outSize)
{
	if (outInfo == NULL || outSize == 0)
		return kPromoterErrInvalidParam;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	ShellPluginInterface *iface = (ShellPluginInterface *)gamePlugin->GetInterface(1);
	if (iface == NULL)
		return kPromoterErrNoPlugin;

	char pkgInfoBuf[kPromoterPkgInfoSize];
	sce_paf_memset(pkgInfoBuf, 0, sizeof(pkgInfoBuf));
	sce_paf_strncpy(pkgInfoBuf, titleId, kPromoterPathBufSize);
	int (*getPkgInfo)(void *) = (int (*)(void *))iface->func[kGameSlotGetPkgInfo];
	int ret = getPkgInfo(pkgInfoBuf);
	if (ret >= 0)
		sce_paf_memcpy(outInfo, pkgInfoBuf, outSize);
	return ret;
}

static int handleGetPkgStatus(const char *titleId, void *outStatus, SceSize outSize)
{
	if (titleId == NULL || outStatus == NULL || outSize == 0)
		return kPromoterErrInvalidParam;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	ShellPluginInterface *iface = (ShellPluginInterface *)gamePlugin->GetInterface(1);
	if (iface == NULL)
		return kPromoterErrNoPlugin;

	SceUInt8 pkgStatusBuf[kPromoterPkgStatusSize];
	sce_paf_memset(pkgStatusBuf, 0, sizeof(pkgStatusBuf));
	int (*getPkgStatus)(const char *, void *) =
		(int (*)(const char *, void *))iface->func[kGameSlotGetPkgStatus];
	int ret = getPkgStatus(titleId, pkgStatusBuf);
	if (ret >= 0)
		sce_paf_memcpy(outStatus, pkgStatusBuf, outSize);
	return ret;
}

static int handleStartStatus(IPMI::Session *session, int requestId, const char *titleId,
			     int queryOption)
{
	s_promoterState = 3;
	s_promoterResult = -1;
	s_promoterSizeLow = 0;
	s_promoterSizeHigh = 0;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAsyncStatusJob(session, requestId, gamePlugin, titleId, queryOption,
					   onPromoterFinishSize));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleStartName(IPMI::Session *session, int requestId, const char *titleId)
{
	s_promoterState = 3;
	s_promoterResult = -1;
	sce_paf_memset(s_promoterInfoBuf, 0, sizeof(s_promoterInfoBuf));
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAsyncNameJob(session, requestId, gamePlugin, titleId,
					 onPromoterFinishInfo));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleStartInfo(IPMI::Session *session, int requestId, const char *titleId,
			   int queryOption)
{
	s_promoterState = 3;
	s_promoterResult = -1;
	s_promoterSizeLow = 0;
	s_promoterSizeHigh = 0;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAsyncInfoJob(session, requestId, gamePlugin, titleId, queryOption,
					 onPromoterFinishStatus));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleStartProgress(IPMI::Session *session, int requestId, const char *titleId,
			       int queryOption)
{
	s_promoterState = 3;
	s_promoterResult = -1;
	s_promoterSizeLow = 0;
	s_promoterSizeHigh = 0;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return kPromoterErrNoPlugin;
	if (titleId == NULL)
		return kPromoterErrInvalidParam;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterAsyncProgressJob(session, requestId, gamePlugin, titleId, queryOption,
					     onPromoterFinishStatus));
	return s_promoterJobQueue->Enqueue(job);
}

static int handleRemoveSavedata(const char *titleId, int removeScope, int removeFlags)
{
	s_promoterState = 5;
	s_promoterResult = -1;
	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL) {
		s_promoterResult = kPromoterErrNoPlugin;
		s_promoterState = 0;
		return kPromoterErrNoPlugin;
	}
	if (s_promoterJobQueue == NULL) {
		s_promoterResult = kPromoterErrNoMemory;
		s_promoterState = 0;
		return kPromoterErrNoMemory;
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new PromoterRemoveSavedataJob(gamePlugin, onPromoterFinishBasic, titleId,
					      removeScope, removeFlags));
	if (job.get() == NULL) {
		s_promoterResult = kPromoterErrNoMemory;
		s_promoterState = 0;
		return kPromoterErrNoMemory;
	}

	int ret = s_promoterJobQueue->Enqueue(job);
	if (ret < 0) {
		s_promoterResult = ret;
		s_promoterState = 0;
		return ret;
	}
	return 0;
}

} /* anonymous namespace */

/* SceShellSvcServer dispatch table entrypoints for 0x00130000-0x0013FFFF */

int shellSvcDisp_13_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int result = 0;

	switch (methodId) {
	case kPromoterMethodAddPath:
		result = handleAddPath((const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodAddPkg:
		result = handleAddPkg((const PromoterPkgJobParam *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodAddRif:
		result = handleAddRif((const PromoterRifJobParam *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetState:
		result = handleGetState((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetResult:
		result = handleGetResult((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetSize:
		result = handleGetSize((int *)outList[0].pBuffer,
				       (SceUInt32 *)outList[1].pBuffer);
		outList[0].outSize = sizeof(int);
		outList[1].outSize = sizeof(SceUInt64);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetStatus:
		result = handleGetStatus((int *)outList[0].pBuffer, outList[1].pBuffer);
		outList[0].outSize = sizeof(int);
		outList[1].outSize = kPromoterStatusBufSize;
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetInfo:
		result = handleGetInfo((int *)outList[0].pBuffer, outList[1].pBuffer);
		outList[0].outSize = sizeof(int);
		outList[1].outSize = kPromoterInfoBufSize;
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetProgress:
		result = handleGetProgress((float *)outList[0].pBuffer);
		outList[0].outSize = sizeof(float);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodDeletePkg:
		result = handleDeletePkg((const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodSetFlag:
		result = handleSetFlag((const char *)inList[1].pData,
				       *(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodStartGetSize:
		result = handleStartGetSize((const char *)inList[0].pData);
		outList[0].outSize = sizeof(SceUInt64);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodUpdateUpgradable:
		result = handleUpdateUpgradable((const char *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodPromoteImport:
		result = handlePromoteImport((const PromoterImportJobParam *)inList[0].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodUpdateLiveArea:
		result = handleUpdateLiveArea((const char *)inList[0].pData,
					      (const char *)inList[1].pData);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodInit:
		result = handleInit(session);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodExit:
		result = handleExit(session);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetPendingCount:
		result = handleGetPendingCount((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetPendingTitle: {
		SceSize copySize = *(const SceSize *)inList[0].pData;
		result = handleGetPendingTitle((const int *)inList[1].pData,
					       (char *)outList[0].pBuffer, copySize);
		outList[0].outSize = copySize;
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}

	case kPromoterMethodCheckExist:
		result = handleCheckExist((const char *)inList[0].pData,
					  (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetPkgInfo:
		result = handleGetPkgInfo((const char *)inList[1].pData, outList[0].pBuffer,
					  *(const SceSize *)inList[0].pData);
		outList[0].outSize = kPromoterPkgInfoSize;
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodGetPkgStatus:
		result = handleGetPkgStatus((const char *)inList[1].pData, outList[0].pBuffer,
					    *(const SceSize *)inList[0].pData);
		outList[0].outSize = kPromoterPkgStatusSize;
		return session->respondToSyncMethodRequest(result, outList, outCount);

	case kPromoterMethodRemoveSavedata: {
		const int *removeArgs = (const int *)inList[0].pData;
		result = handleRemoveSavedata((const char *)inList[1].pData,
					      removeArgs[0], removeArgs[1]);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}

	default:
		return kIpmiErrUnsupported;
	}
}

int shellSvcDisp_13_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)inCount;

	switch (methodId) {
	case kPromoterMethodStartStatus:
		return handleStartStatus(session, (int)reqId, (const char *)inList[1].pData,
					 *(const int *)inList[0].pData);

	case kPromoterMethodStartName:
		return handleStartName(session, (int)reqId, (const char *)inList[0].pData);

	case kPromoterMethodStartInfo:
		return handleStartInfo(session, (int)reqId, (const char *)inList[1].pData,
				       *(const int *)inList[0].pData);

	case kPromoterMethodStartProgress:
		return handleStartProgress(session, (int)reqId, (const char *)inList[1].pData,
					   *(const int *)inList[0].pData);

	default:
		return kIpmiErrUnsupported;
	}
}

void shellSvcDisp_13_Disconn(IPMI::Session *session)
{
	handleExit(session);
}
