/*
 * SceShell mtp: MTP event handler and account-link result job
 * (0x811C99E4 - 0x811CA094)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <plugin.h>
#include <job/job.h>

#include "../../livespace_db/lsdb_keys.h"

extern "C" {
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *status);
extern int sceUdcdGetDeviceInfo(void *info);
extern int sceClibPrintf(const char *fmt, ...);
extern int SceLibMtp_4C1B7DBB(void);
}

extern int shellIsBooting(void);				/* 0x81000000 */
extern void mtpCloseUsbErrorDialogs(void);			/* 0x811CA094 */
extern int mtpShowUsbError1Dialog(void);			/* 0x811CA30E */
extern void mtpCloseBusyDialog(void);				/* 0x811CA37C */
extern void mtpOpenBusyDialog(void);				/* 0x811CA3A2 */
extern int mtpIsUsbError2Open(void);				/* 0x811CA570 */
extern int mtpIsUsbError1Open(void);				/* 0x811CA588 */
extern int mtpRegisterHandler(void *handler);			/* 0x812B1C22 */
extern int mtpIsDialogSuppressed(void);				/* 0x812B1C4A */
extern int mtpIsCmaActive(void);				/* 0x812B1C6A */
extern int mtpRegisterAccountLinkCb(void (*cb)(const void *, int)); /* 0x813BFDD4 */
extern void mtpJobQueueInit(paf::job::JobQueue::Option *opt);	/* 0x813C028C */
extern void mtpJobQueueFini(void);				/* 0x813C0322 */
extern paf::job::JobQueue *shellGetMtpJobQueue(void);		/* 0x81488474[146] */

class MtpEventHandler;

static MtpEventHandler *s_mtpEventHandler = NULL;		/* 0x81544E18 */
static SceUInt8 s_mtpDialogAllowed = 0;				/* 0x81544E1C */

class MtpNotifyAccountLinkJob : public paf::job::JobItem {
public:
	MtpNotifyAccountLinkJob(const void *info, int result)
		: paf::job::JobItem("SceMtpNotifyAccountLinkResult", NULL),
		  m_result(result)
	{
		sce_paf_memcpy(m_info, info, sizeof(m_info));
	}
	virtual ~MtpNotifyAccountLinkJob() {}
	virtual int Run() { return 0; }
	virtual void Finish(int result) { (void)result; }

	SceUInt8 m_info[208];					/* +32 */
	int m_result;						/* +240 */
};

class MtpEventHandler {
public:
	MtpEventHandler();					/* 0x811C9A18 */
	virtual ~MtpEventHandler();				/* 0x811C9A6C / 0x811C9A92 */

	int SetCmaIconLock(SceUInt8 unlock);			/* 0x811C9ABE */
	void OnDisconnected();					/* 0x811C9B86 */
	void OnUsbError();					/* 0x811C9C92 */
	void OnNoop();						/* 0x811C9CAA */
	void OnSessionStarted();				/* 0x811C9CAC */
	void OnConnected();					/* 0x811C9E02 */
	void OnBusyEnd();					/* 0x811C9EE8 */

	SceUInt8 m_isConnected;					/* +4 */
	SceUInt8 m_pendingClose;				/* +5 */
};

static bool isCurrentActiveApp(const char *titleId)
{
	SceUInt8 status[0x80];
	int ret = sceAppMgrGetStatusByAppId(-4, status);
	if (ret >= 0)
		return (sce_paf_strcmp((const char *)(status + 12), titleId) == 0);
	if (ret != (int)0x80802012)
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceAppMgrGetStatusByAppId failed: 0x%x\n",
			"", "isCurrentActiveApp", 239, ret);
	return false;
}

/* 0x811C99E4 */
void mtpEventHandlerCreate(void)
{
	if (s_mtpEventHandler == NULL) {
		s_mtpEventHandler = new MtpEventHandler();
		if (s_mtpEventHandler != NULL)
			mtpRegisterHandler(s_mtpEventHandler);
	}
}

/* 0x811C9A18 */
MtpEventHandler::MtpEventHandler()
	: m_isConnected(1), m_pendingClose(0)
{
	paf::job::JobQueue::Option opt;
	opt.numThreads = 1;
	opt.priority = 0x1000010E;
	mtpJobQueueInit(&opt);
}

/* 0x811C9A6C / 0x811C9A92 */
MtpEventHandler::~MtpEventHandler()
{
	mtpJobQueueFini();
}

/* 0x811C9ABE */
int MtpEventHandler::SetCmaIconLock(SceUInt8 unlock)
{
	paf::Plugin *plugin = paf::Plugin::Find("applauncher_plugin");
	if (plugin == NULL)
		return -1;
	void *iface = plugin->GetInterface(1);
	if (iface == NULL)
		return -1;

	paf::string titleId("NPXS10026");
	if (unlock != 0) {
		typedef void (*UnlockFn)(const paf::string *, int);
		(*(UnlockFn *)((char *)iface + 16))(&titleId, 1);
	} else {
		typedef void (*LockFn)(const paf::string *);
		(*(LockFn *)((char *)iface + 20))(&titleId);
	}
	return 0;
}

/* 0x811C9B86 */
void MtpEventHandler::OnDisconnected()
{
	if (m_pendingClose == 0 && m_isConnected == 0)
		return;

	m_pendingClose = 0;
	if (mtpIsUsbError2Open() != 0 || mtpIsUsbError1Open() != 0) {
		m_pendingClose = 1;
	} else {
		mtpCloseBusyDialog();
		mtpCloseUsbErrorDialogs();
	}

	if (isCurrentActiveApp("NPXS10026"))
		return;

	paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		void *iface = topmenu->GetInterface(1);
		if (iface != NULL) {
			typedef const char *(*GetActiveTitleFn)(void);
			const char *active = (*(GetActiveTitleFn *)((char *)iface + 184))();
			if (active != NULL && sce_paf_strcmp(active, "NPXS10026") == 0)
				return;
		}
	}

	if (s_mtpDialogAllowed != 0) {
		s_mtpDialogAllowed = 0;
		SetCmaIconLock(1);
	}
}

/* 0x811C9C92 */
void MtpEventHandler::OnUsbError()
{
	if (m_isConnected != 0 && shellIsBooting() == 0) {
		mtpCloseBusyDialog();
		mtpShowUsbError1Dialog();
	}
}

/* 0x811C9CAA */
void MtpEventHandler::OnNoop()
{
}

/* 0x811C9CAC */
void MtpEventHandler::OnSessionStarted()
{
	mtpCloseBusyDialog();
	mtpCloseUsbErrorDialogs();
	SetCmaIconLock(0);
}

/* 0x811C9CC2 */
bool toSkipOpenDialog(void)
{
	if (SceLibMtp_4C1B7DBB() == 2)
		return true;
	if (s_mtpDialogAllowed == 0)
		return true;
	if (mtpIsDialogSuppressed() != 0)
		return true;
	if (isCurrentActiveApp("NPXS10026") && mtpIsCmaActive() == 0)
		return true;
	if (isCurrentActiveApp("NPXS10015"))
		return true;

	SceUInt32 devInfo[16];
	sce_paf_memset(devInfo, 0, sizeof(devInfo));
	int ret = sceUdcdGetDeviceInfo(devInfo);
	if (ret >= 0)
		return ((devInfo[0] & devInfo[1]) == 0);

	sceClibPrintf(
		"[MTP %s] Error %s(%u): sceUdcdGetDeviceInfo failed: 0x%x\n",
		"", "toSkipOpenDialog", 284, ret);
	return false;
}

/* 0x811C9E02 */
void MtpEventHandler::OnConnected()
{
	m_isConnected = 0;
	if (shellIsBooting() != 0 || toSkipOpenDialog())
		return;

	if (isCurrentActiveApp("NPXS10026")) {
		m_isConnected = 1;
		mtpOpenBusyDialog();
		return;
	}

	paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		void *iface = topmenu->GetInterface(1);
		if (iface != NULL) {
			typedef const char *(*GetActiveTitleFn)(void);
			const char *active = (*(GetActiveTitleFn *)((char *)iface + 184))();
			if (active != NULL && sce_paf_strcmp(active, "NPXS10026") == 0) {
				m_isConnected = 1;
				mtpOpenBusyDialog();
				return;
			}
			typedef int (*IsHomeFn)(void);
			if ((*(IsHomeFn *)((char *)iface + 108))() != 0) {
				m_isConnected = 1;
				mtpOpenBusyDialog();
				return;
			}
		}
	}
}

/* 0x811C9EE8 */
void MtpEventHandler::OnBusyEnd()
{
	mtpCloseBusyDialog();
	SetCmaIconLock(0);
}

/* 0x811C9F0A */
static void mtpOnAccountLinkResult(const void *info, int result)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new MtpNotifyAccountLinkJob(info, result));
	shellGetMtpJobQueue()->Enqueue(job);
}

/* 0x811C9EFA */
int mtpRegisterAccountLinkCallback(void)
{
	return mtpRegisterAccountLinkCb(mtpOnAccountLinkResult);
}
