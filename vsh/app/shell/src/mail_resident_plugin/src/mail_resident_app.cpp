/*
 * SceShell mail_resident_plugin: MailResidentApp
 * 3.65-CEX: 0x811C6F94 - 0x811C867C
 */

#include <stddef.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <std/string.h>
#include <kernel/threadmgr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

#include "shell.h"
#include "shell_cron.h"
#include "mail_resident.h"

extern "C" {
extern int sceKernelCreateCallback(const char *name, unsigned int attr,
	SceKernelCallbackFunction func, void *arg);
extern int sceKernelDeleteCallback(SceUID cbId);
extern int sceKernelNotifyCallback(SceUID cbId, int arg);
extern int sceKernelDeleteMsgPipe(SceUID uid);
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *buf);
extern int sceRegMgrRegisterCallback(SceUID cbId);
extern int sceRegMgrAddRegistryCallback(SceUID cbId, const char *path);
extern int sceRegMgrStartCallback(SceUID cbId);
extern int sceRegMgrStopCallback(SceUID cbId);
extern int sceRegMgrUnregisterCallback(SceUID cbId);
}

typedef void (*NpImpsNotifOnlineCallback)(int stateCode);
extern int npImpsNotifRegisterOnlineCallback(NpImpsNotifOnlineCallback callback);
extern int npImpsNotifUnregisterOnlineCallback(void);
extern int npAccountRegisterNetCallback(void (*callback)(int, void *), void *userArg);
extern int npAccountUnregisterNetCallback(int callbackId);
extern int npCommonServiceIsSignedIn(int *outSignedIn);
extern int shellTriggerMgrRegisterService(const char *serviceId);
extern int shellTriggerMgrIsServiceEnabled(const char *serviceId, bool *outEnabled);
extern int shellTriggerMgrSetServiceCallback(const char *serviceId,
					     void (*callback)(bool, void *), void *userData);
extern int DbCheckSetCorruptByPath(const char *path, int writeMagic);

static inline int shellRegisterPowerCb(void (*cb)(void *))
{
	return npImpsNotifRegisterOnlineCallback(reinterpret_cast<NpImpsNotifOnlineCallback>(cb));
}
static inline void shellUnregisterPowerCb(void)
{
	npImpsNotifUnregisterOnlineCallback();
}
extern bool shellActivityIsEnabled(void);			/* 0x81326D30 */
static inline int shellRegisterNetCb(void (*cb)(SceUInt32, void *), void *arg)
{
	return npAccountRegisterNetCallback(reinterpret_cast<void (*)(int, void *)>(cb), arg);
}
static inline void shellUnregisterNetCb(int id)
{
	npAccountUnregisterNetCallback(id);
}
static inline int shellNpGetSignInState(int *outState)
{
	return npCommonServiceIsSignedIn(outState);
}
static inline void shellMailCronRegister(const char *id)
{
	shellTriggerMgrRegisterService(id);
}
static inline void shellMailCronIsEnabled(const char *id, bool *out)
{
	shellTriggerMgrIsServiceEnabled(id, out);
}
static inline void shellMailCronSetCb(const char *id, void (*cb)(int, void *), void *arg)
{
	shellTriggerMgrSetServiceCallback(id, reinterpret_cast<void (*)(bool, void *)>(cb), arg);
}
static inline int shellMailHandleCustomCmd(const void *data, int flag)
{
	return DbCheckSetCorruptByPath(static_cast<const char *>(data), flag);
}
extern int shellNpPushRegisterService(const char *servicePrefix, int matchMode,
	void *callback, void *userArg, int *outHandle);		/* 0x813AEFFC */
extern int shellNpPushRegisterTitle(const char *servicePrefix, int matchMode,
	void *callback, void *userArg, int *outHandle);		/* 0x813AF03A */
extern void shellNpPushUnregister(int handle);			/* 0x813AF078 */

static MailResidentApp *s_mailAppInstance = NULL;		/* 0x81544E10 */

/* 0x811C6F94 */
MailResidentApp::MailResidentApp()
	: m_stopFlag(0), m_netState(1), m_initStage(0),
	  m_server(NULL), m_serverBuf(NULL), m_workBufSize(0),
	  m_sessionMutex("SceMailResidentAppSession", 0),
	  m_evfProxy(-1), m_evfPsnMail(-1), m_evfReady(-1),
	  m_netCbId(-1), m_cronHandle(0), m_cronOption(),
	  m_queueMutex("SceMailResidentAppQueue", 0),
	  m_queueCb(-1),
	  m_syncMutex("SceMailResidentAppSync", 0),
	  m_pendingPush(0), m_pendingTimer(0), m_seqNo(0)
{
	m_padding[0] = 0;
	m_padding[1] = 0;
	s_mailAppInstance = this;
	m_cronOption.wakeUp = true;
}

/* 0x811C7090 / 0x811C7178 */
MailResidentApp::~MailResidentApp()
{
	s_mailAppInstance = NULL;
}

static inline void pushQueueEvent(MailResidentApp *app, MailResidentEvent *ev)
{
	app->m_queueMutex.Lock();
	paf::intrusive_ptr<MailResidentEvent> ref(ev);
	app->m_queue.push_back(ref);
	sceKernelNotifyCallback(app->m_queueCb, 0);
	app->m_queueMutex.Unlock();
}

/* 0x811C7188 */
void MailResidentApp::EnqueueEvent(int kind, SceUInt32 eventParam, int appState)
{
	switch (kind) {
	case 0:
		sceAtomicStore32((volatile SceInt32 *)&m_netState, (SceInt32)eventParam);
		/* fallthrough */
	case 1:
	case 7:
		if (eventParam != 0) {
			MailResidentEvent *ev = new MailResidentEvent(7);
			if (ev != NULL)
				pushQueueEvent(this, ev);
		} else {
			MailResidentEvent *ev = new MailResidentEvent(6);
			if (ev != NULL)
				pushQueueEvent(this, ev);
		}
		return;
	case 2:
	case 3:
	case 6: {
		int evType = (kind == 6) ? 5 : 4;
		SceUInt8 *pendingFlag = (kind == 6) ? &m_pendingTimer : &m_pendingPush;
		if (*pendingFlag != 0)
			return;
		m_queueMutex.Lock();
		for (paf::list<paf::intrusive_ptr<MailResidentEvent> >::iterator it = m_queue.begin();
		     it != m_queue.end(); ++it) {
			MailResidentEvent *e = it->get();
			if (e->m_type == 4) {
				if (((MailResidentSyncEvent *)e)->m_arg == eventParam) {
					m_queueMutex.Unlock();
					return;
				}
			} else if (e->m_type == evType) {
				m_queueMutex.Unlock();
				return;
			}
		}
		m_queueMutex.Unlock();

		m_syncMutex.Lock();
		for (paf::list<paf::intrusive_ptr<MailResidentSyncReq> >::iterator it = m_syncReqs.begin();
		     it != m_syncReqs.end(); ++it) {
			if ((*it)->m_evType == evType) {
				*pendingFlag = 1;
				m_syncMutex.Unlock();
				return;
			}
		}
		m_syncMutex.Unlock();

		MailResidentSyncEvent *ev = new MailResidentSyncEvent(evType, eventParam);
		if (ev != NULL)
			pushQueueEvent(this, ev);
		return;
	}
	case 4: {
		MailResidentAppEvent *ev = new MailResidentAppEvent(eventParam, appState);
		if (ev != NULL)
			pushQueueEvent(this, ev);
		return;
	}
	case 5: {
		m_mw.SignalReady();
		MailResidentEvent *ev = new MailResidentEvent(1);
		if (ev != NULL)
			pushQueueEvent(this, ev);
		return;
	}
	default:
		return;
	}
}

/* 0x811C7466 */
static void mailNetCallback(SceUInt32 state, void *arg)
{
	((MailResidentApp *)arg)->EnqueueEvent(0, state, 0);
}

/* 0x811C7BEA */
static void mailCronReadyCb(int eventType, void *arg)
{
	(void)eventType;
	((MailResidentApp *)arg)->EnqueueEvent(1, 0, 0);
}

/* 0x811C7BFA */
static void mailPowerCb(void *arg)
{
	if (arg == NULL)
		s_mailAppInstance->EnqueueEvent(3, 0, 0);
}

/* 0x811C7C14 */
static void mailAppEventCb(int ev, const void *info, SceSize size, void *arg)
{
	(void)size;
	const ShellAppEvent *appEvent = (const ShellAppEvent *)info;
	if (ev == SHELL_APP_EVENT_ENDED && sce_paf_strcmp("NPXS10063", appEvent->titleId) == 0)
		((MailResidentApp *)arg)->EnqueueEvent(5, 0, 0);
	else
		((MailResidentApp *)arg)->EnqueueEvent(4, (SceUInt32)appEvent->processId, ev);
}

/* 0x811C7C4C */
static int mailTimerCb(SceUID handle, void *arg)
{
	(void)handle;
	((MailResidentApp *)arg)->EnqueueEvent(6, 0, 0);
	return 0;
}

/* 0x811C7C5E */
static int mailRegCb(int notifyId, int notifyCount, SceUInt32 notifyArg, void *arg)
{
	(void)notifyId; (void)notifyCount;
	((MailResidentApp *)arg)->EnqueueEvent(7, notifyArg, 0);
	return 0;
}

/* 0x811C7476 */
int MailResidentApp::Start()
{
	m_stopFlag = 0;
	m_evfProxy = sceKernelCreateEventFlag("ScePsnMailProxy", 0x1080, 0, NULL);
	if (m_evfProxy < 0) { m_evfProxy = -1; return -1; }
	m_evfPsnMail = sceKernelCreateEventFlag("ScePsnMailResidentPsnMail", 0x1080, 0, NULL);
	if (m_evfPsnMail < 0) { m_evfPsnMail = -1; return -1; }
	m_evfReady = sceKernelCreateEventFlag("", 0x1000, 0, NULL);
	if (m_evfReady < 0) { m_evfReady = -1; return -1; }

	IPMI::Server::Config cfg;
	sce_paf_strncpy(cfg.serviceName, "ScePsnMailRs", 16);
	cfg.budget = 2;
	cfg.maxRequestSize = 4;
	cfg.maxMessageSize = 200;
	cfg.pEventHandler = this;
	m_workBufSize = IPMI::Server::getWorkBufferSize(&cfg);
	m_serverBuf = mailResidentMalloc(0x100);
	if (IPMI::Server::create(&m_server, &cfg, this, m_serverBuf) != 0)
		return -1;

	m_threadId = sceKernelCreateThread("SceMailResidentApplication",
		(SceKernelThreadEntry)ThreadEntry, 0x10000100, 0x1000, 0, 0, NULL);
	if (m_threadId < 0) { m_threadId = -1; return -1; }

	MailResidentApp *self = this;
	if (sceKernelStartThread(m_threadId, 4, &self) < 0)
		return -1;

	SceUInt32 outBits = 0;
	sceKernelWaitEventFlag(m_evfReady, 0xFFFFFFFF, 3, &outBits, NULL);
	sceKernelDeleteEventFlag(m_evfReady);
	m_evfReady = -1;

	m_netCbId = shellRegisterNetCb(mailNetCallback, this);
	if (m_netCbId < 0) { m_netCbId = -1; return -1; }
	m_initStage = 1;
	if (shellRegisterPowerCb(mailPowerCb) < 0) return -1;
	m_initStage = 2;
	if (SceShell_83D16CA8(0, mailAppEventCb, this) < 0) return -1;
	m_initStage = 3;
	if (SceShell_83D16CA8(1, mailAppEventCb, this) < 0) return -1;
	m_initStage = 4;
	if (SceShell_83D16CA8(5, mailAppEventCb, this) < 0) return -1;
	m_initStage = 5;
	if (SceShell_83D16CA8(6, mailAppEventCb, this) < 0) return -1;
	m_initStage = 6;

	int signIn = 0;
	if (shellNpGetSignInState(&signIn) == 0 && signIn == 1 && shellActivityIsEnabled() != 0)
		EnqueueEvent(0, 0, 0);
	return 0;
}

/* 0x811C76B6 */
void MailResidentApp::Stop()
{
	m_stopFlag = 1;
	switch (m_initStage) {
	case 6: SceShell_E3D3A162(6, mailAppEventCb, this); /* fallthrough */
	case 5: SceShell_E3D3A162(5, mailAppEventCb, this); /* fallthrough */
	case 4: SceShell_E3D3A162(1, mailAppEventCb, this); /* fallthrough */
	case 3: SceShell_E3D3A162(0, mailAppEventCb, this); /* fallthrough */
	case 2: shellUnregisterPowerCb(); /* fallthrough */
	case 1: shellUnregisterNetCb(m_netCbId); break;
	default: break;
	}
	m_initStage = 0;

	int stat = 0;
	if (m_threadId != -1) {
		sceKernelSetEventFlag(m_evfProxy, 0x80000000);
		sceKernelWaitThreadEnd(m_threadId, &stat, NULL);
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
	if (m_evfPsnMail != -1) {
		sceKernelDeleteEventFlag(m_evfPsnMail);
		m_evfPsnMail = -1;
	}
	m_sessions.clear();
	if (m_server != NULL) {
		m_server->destroy();
		m_server = NULL;
	}
	if (m_serverBuf != NULL) {
		mailResidentFree(m_serverBuf);
		m_serverBuf = NULL;
	}
	if (m_evfProxy != -1) {
		sceKernelDeleteEventFlag(m_evfProxy);
		m_evfProxy = -1;
	}
	if (m_cronHandle != 0) {
		shellCronGetInstance()->MarkRemove(m_cronHandle);
		shellCronGetInstance()->Remove(m_cronHandle);
		m_cronHandle = 0;
	}
}

/* 0x811C77F8 */
void MailResidentApp::CancelPendingSyncs()
{
	m_syncMutex.Lock();
	for (paf::list<paf::intrusive_ptr<MailResidentSyncReq> >::iterator it = m_syncReqs.begin();
	     it != m_syncReqs.end(); ++it) {
		m_mw.RemoveRefKey((*it)->m_key);
	}
	m_pendingTimer = 0;
	m_pendingPush = 0;
	m_syncReqs.clear();
	m_syncMutex.Unlock();
}

/* 0x811C7878 */
int MailResidentApp::EnsureMwRunning(SceInt64 key)
{
	m_mw.AddRefKey(key);
	int st;
	if (m_mw.IsRunning(&st) != 0)
		return 0;
	CancelPendingSyncs();
	if ((((SceUInt32)(key >> 32)) & 0x11u) != 0x11u) {
		m_sessionMutex.Lock();
		m_mw.LaunchAndConnect();
		for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
		     it != m_sessions.end(); ++it) {
			(*it)->NotifyZero();
		}
		m_sessionMutex.Unlock();
	}
	return 1;
}

/* 0x811C7918 */
int MailResidentApp::onNewClient(IPMI::Server *srv, const IPMI::Session::Config *cfg,
	const void *userData, SceSize userDataSize)
{
	(void)userData; (void)userDataSize;
	paf::intrusive_ptr<MailResidentSession> sess(new MailResidentSession());
	IPMI::Session *ipmiSess = NULL;
	if (sess->Open(srv, cfg, &ipmiSess) != 0)
		return -1;
	m_sessionMutex.Lock();
	for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if ((*it)->m_session == ipmiSess) {
			m_sessionMutex.Unlock();
			return -1;
		}
	}
	sess->m_state = 5;
	m_sessions.push_back(sess);
	m_sessionMutex.Unlock();

	if (EnsureMwRunning((SceInt64)(SceUIntPtr)ipmiSess) == 0)
		sess->NotifyZero();
	return 0;
}

/* 0x811C7A00 */
void MailResidentApp::onSessionClosedByClient(IPMI::Session *session)
{
	onSessionKilled(session);
}

/* 0x811C7A0A */
void MailResidentApp::ReleaseSessionKey(SceInt64 key)
{
	SceInt64 closeSessionKey = key | 0x0000040100000000LL;
	SceInt64 flushKey = key | 0x0000080100000000LL;
	EnsureMwRunning(closeSessionKey);
	EnsureMwRunning(flushKey);
	MailResidentMwPairEvent *ev = new MailResidentMwPairEvent(closeSessionKey, flushKey);
	if (ev != NULL)
		pushQueueEvent(this, ev);
}

/* 0x811C7AC8 */
void MailResidentApp::onSessionKilled(IPMI::Session *session)
{
	m_sessionMutex.Lock();
	for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Close();
			m_sessions.erase(it);
			break;
		}
	}
	m_sessionMutex.Unlock();
	int st;
	if (m_mw.IsRunning(&st) != 0)
		ReleaseSessionKey((SceInt64)(SceUIntPtr)session);
	m_mw.RemoveRefKey((SceInt64)(SceUIntPtr)session);
}

/* 0x811C7B90 */
void MailResidentApp::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 reqId,
	SceUInt32 methodId, void *pIn, SceSize inSize, void *pOut, SceSize outSize)
{
	(void)inSize; (void)pOut; (void)outSize;
	int res = 0;
	if (methodId == 0x2010)
		res = (pIn != NULL) ? shellMailHandleCustomCmd(pIn, 1) : (int)0x8068620F;
	pSession->respondToAsyncMethodRequest(reqId, 0, &res, 4);
}

/* 0x811C7C6E */
void MailResidentApp::SendMwRequest(SceInt64 key, int evType, int method, const void *inBuf, SceSize inSize)
{
	bool isSpecial = ((((SceUInt32)(key >> 32)) & 0x11u) == 0x11u);
	if (!isSpecial)
		EnsureMwRunning(key);
	int res = 0;
	SceSize outSize = 0;
	int outBuf[2];
	int r = m_mw.SyncCall(method, inBuf, inSize, &res, outBuf, &outSize, 8);
	if ((r | res) != 0) {
		if (!isSpecial)
			m_mw.RemoveRefKey(key);
	} else {
		m_syncMutex.Lock();
		paf::intrusive_ptr<MailResidentSyncReq> req(
			new MailResidentSyncReq(key, method, outBuf[1], evType));
		m_syncReqs.push_back(req);
		m_syncMutex.Unlock();
	}
}

/* 0x811C7D88 */
void MailResidentApp::UpdateFetchTimer()
{
	if (m_cronHandle != 0) {
		shellCronGetInstance()->MarkRemove(m_cronHandle);
		shellCronGetInstance()->Remove(m_cronHandle);
		m_cronHandle = 0;
	}
	if (m_netState == 0) {
		shellMailCronRegister("SHELL0004");
		shellMailCronSetCb("SHELL0004", mailCronReadyCb, this);
		bool enabled = false;
		shellMailCronIsEnabled("SHELL0004", &enabled);
		if (enabled) {
			int interval = 0;
			if (sceRegMgrGetKeyInt("/CONFIG/NP2/PSNMSG", "fetch_interval", &interval) == 0 &&
			    interval > 0) {
				interval *= 60;
				int r = shellCronGetInstance()->AddInterval(&m_cronHandle, mailTimerCb, this,
					(unsigned int)interval, (unsigned int)interval, &m_cronOption);
				if (r < 0 && m_cronHandle != 0) {
					shellCronGetInstance()->MarkRemove(m_cronHandle);
					shellCronGetInstance()->Remove(m_cronHandle);
					m_cronHandle = 0;
				}
			}
		}
	}
}

/* 0x811C7E82 */
void MailResidentApp::ProcessQueue()
{
	paf::intrusive_ptr<MailResidentEvent> ev;
	m_queueMutex.Lock();
	if (m_queue.size() == 0) {
		m_queueMutex.Unlock();
		return;
	}
	ev = m_queue.front();
	m_queue.pop_front();
	m_queueMutex.Unlock();

	switch (ev->m_type) {
	case 0: {
		MailResidentAppEvent *ae = (MailResidentAppEvent *)ev.get();
		for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
		     it != m_sessions.end(); ++it) {
			if ((SceUInt32)(*it)->m_pid == ae->m_pid) {
				(*it)->m_state = ae->m_appState;
				MailResidentEvent *refreshEv = new MailResidentEvent(1);
				if (refreshEv != NULL)
					pushQueueEvent(this, refreshEv);
				break;
			}
		}
		break;
	}
	case 1: {
		int st = 0;
		if (m_mw.IsRunning(&st) == 0) {
			m_mw.Disconnect(0);
			CancelPendingSyncs();
			m_sessionMutex.Lock();
			bool hasActive = false;
			for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
			     it != m_sessions.end(); ++it) {
				if ((*it)->m_state == 5) {
					hasActive = true;
					break;
				}
			}
			if (hasActive) {
				m_mw.LaunchAndConnect();
				for (paf::list<paf::intrusive_ptr<MailResidentSession> >::iterator it = m_sessions.begin();
				     it != m_sessions.end(); ++it) {
					(*it)->NotifyZero();
				}
			}
			m_sessionMutex.Unlock();
		}
		break;
	}
	case 2: {
		MailResidentMwPairEvent *pe = (MailResidentMwPairEvent *)ev.get();
		int closeReq;
		sce_paf_memset(&closeReq, 0, sizeof(closeReq));
		SendMwRequest(pe->m_closeSessionKey, ev->m_type, 17, &closeReq, 4);
		int flushReq;
		sce_paf_memset(&flushReq, 0, sizeof(flushReq));
		SendMwRequest(pe->m_flushKey, ev->m_type, 13, &flushReq, 4);
		break;
	}
	case 3: {
		MailResidentDoneEvent *de = (MailResidentDoneEvent *)ev.get();
		int inPair[2];
		sce_paf_memset(inPair, 0, sizeof(inPair));
		inPair[0] = de->m_req->m_method;
		inPair[1] = de->m_req->m_bit;
		int res = 0;
		SceSize outSz = 0;
		m_mw.SyncCall(25, inPair, 8, &res, NULL, &outSz, 0);

		paf::intrusive_ptr<MailResidentSyncReq> found;
		m_syncMutex.Lock();
		for (paf::list<paf::intrusive_ptr<MailResidentSyncReq> >::iterator it = m_syncReqs.begin();
		     it != m_syncReqs.end(); ++it) {
			if ((*it)->m_bit == inPair[1] && (*it)->m_method == inPair[0]) {
				found = *it;
				m_syncReqs.erase(it);
				break;
			}
		}
		m_syncMutex.Unlock();
		if (found.get() != NULL) {
			if (found->m_evType == 5 && m_pendingTimer != 0) {
				m_pendingTimer = 0;
				int reqBuf[2];
				sce_paf_memset(reqBuf, 0, sizeof(reqBuf));
				SendMwRequest(found->m_key + 1, found->m_evType, 24, reqBuf, 8);
				ReleaseSessionKey(found->m_key + 1);
			} else if (found->m_evType == 4 && m_pendingPush != 0) {
				m_pendingPush = 0;
				int reqBuf[2];
				sce_paf_memset(reqBuf, 0, sizeof(reqBuf));
				reqBuf[0] = 1;
				SendMwRequest(found->m_key + 1, found->m_evType, 24, reqBuf, 8);
				ReleaseSessionKey(found->m_key + 1);
			}
			m_mw.RemoveRefKey(found->m_key);
		}
		break;
	}
	case 4: {
		MailResidentSyncEvent *se = (MailResidentSyncEvent *)ev.get();
		int reqBuf[2];
		sce_paf_memset(reqBuf, 0, sizeof(reqBuf));
		reqBuf[0] = 1;
		reqBuf[1] = (int)se->m_arg;
		SceUInt32 seq = ++m_seqNo;
		SceInt64 key = ((SceInt64)0x101 << 32) | seq;
		SendMwRequest(key, ev->m_type, 24, reqBuf, 8);
		ReleaseSessionKey(key);
		break;
	}
	case 5: {
		int reqBuf[2];
		sce_paf_memset(reqBuf, 0, sizeof(reqBuf));
		SceUInt32 seq = ++m_seqNo;
		SceInt64 key = ((SceInt64)0x201 << 32) | seq;
		SendMwRequest(key, ev->m_type, 24, reqBuf, 8);
		ReleaseSessionKey(key);
		break;
	}
	case 6:
		UpdateFetchTimer();
		break;
	case 7: {
		int cancelReq;
		sce_paf_memset(&cancelReq, 0, sizeof(cancelReq));
		SendMwRequest(0x1100000000LL, ev->m_type, 23, &cancelReq, 4);
		break;
	}
	default:
		break;
	}
}

/* 0x811C82E0 */
static int mailQueueCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId; (void)notifyArg;
	for (int i = notifyCount; i != 0; --i)
		((MailResidentApp *)userData)->ProcessQueue();
	return 0;
}

/* 0x811C82F8 */
static void mailNpPushServiceCb(const void *toPeer, const void *fromPeer, const char *dataType,
	const char *data, SceSize dataLen, void *userArg)
{
	(void)toPeer; (void)fromPeer; (void)data; (void)dataLen;
	if (userArg == NULL)
		return;
	if (sce_paf_strcasecmp(dataType, "np:service:groupMessaging") == 0)
		((MailResidentApp *)userArg)->EnqueueEvent(2, 1, 0);
	else if (sce_paf_strcasecmp(dataType, "np:service:systemMessage") == 0)
		((MailResidentApp *)userArg)->EnqueueEvent(2, 8, 0);
}

/* 0x811C8340 */
static void mailNpPushTitleCb(const void *toPeer, const void *fromPeer, const char *titleId,
	const char *dataType, const char *data, SceSize dataLen, void *userArg)
{
	(void)toPeer; (void)fromPeer; (void)titleId; (void)data; (void)dataLen;
	if (userArg == NULL)
		return;
	if (sce_paf_strcasecmp(dataType, "np:service:gameCustomData") == 0)
		((MailResidentApp *)userArg)->EnqueueEvent(2, 2, 0);
	else if (sce_paf_strcasecmp(dataType, "np:service:invitation") == 0)
		((MailResidentApp *)userArg)->EnqueueEvent(2, 4, 0);
}

/* 0x811C8388 */
void MailResidentApp::ThreadLoop()
{
	SceUID regCb = sceKernelCreateCallback("ScePsnMailResidentRegist", 0,
		(SceKernelCallbackFunction)mailRegCb, this);
	int r = (int)0x800280A3;
	if (regCb >= 0) {
		r = sceRegMgrRegisterCallback(regCb);
		if (r >= 0) r = sceRegMgrAddRegistryCallback(regCb, "/CONFIG/NP2/PSNMSG/fetch_interval");
		if (r >= 0) r = sceRegMgrAddRegistryCallback(regCb, "/CONFIG/POWER_SAVING/suspend_interval");
		if (r >= 0) r = sceRegMgrStartCallback(regCb);
	} else {
		regCb = -1;
	}

	int servicePushHandle = -1;
	if (r >= 0)
		r = shellNpPushRegisterService("np:service", 1, (void *)mailNpPushServiceCb, this,
			&servicePushHandle);
	int titlePushHandle = -1;
	if (r >= 0)
		shellNpPushRegisterTitle("np:service", 1, (void *)mailNpPushTitleCb, this,
			&titlePushHandle);

	SceUID pipeId = sceKernelCreateMsgPipe("SceMailResidentPipe", 64, 396, 0x460, NULL);
	m_queueCb = sceKernelCreateCallback("ScePsnMailResidentQueue", 0,
		(SceKernelCallbackFunction)mailQueueCallback, this);
	sceKernelSetEventFlag(m_evfReady, 1);

	void *workBuf = mailResidentMalloc(m_workBufSize);
	SceUInt32 outBits = 0;
	while (1) {
		SceUInt timeout = 16000;
		int wr = sceKernelWaitEventFlagCB(m_evfProxy, 0xFFFFFFFF, 3, &outBits, &timeout);
		if (m_stopFlag != 0)
			break;
		if (wr == (int)0x80028005) {
			m_server->tryDispatch(workBuf, m_workBufSize);
			struct {
				int type;
				char payload[108];
			} msg;
			msg.type = 0;
			sce_paf_memset(msg.payload, 0, sizeof(msg.payload));
			SceSize recvSz;
			while (sceKernelTryReceiveMsgPipe(pipeId, &msg, sizeof(msg), 0, &recvSz) == 0 &&
			       recvSz == sizeof(msg)) {
				if (msg.type == 2)
					EnqueueEvent(6, 0, 0);
			}
		} else {
			outBits &= ~0x80000000u;
			for (SceUInt32 bit = 0; bit < 32; ++bit) {
				if ((outBits & (1u << bit)) != 0) {
					m_syncMutex.Lock();
					for (paf::list<paf::intrusive_ptr<MailResidentSyncReq> >::iterator it = m_syncReqs.begin();
					     it != m_syncReqs.end(); ++it) {
						if ((*it)->m_bit == (int)bit) {
							MailResidentDoneEvent *de = new MailResidentDoneEvent(*it);
							if (de != NULL)
								pushQueueEvent(this, de);
							break;
						}
					}
					m_syncMutex.Unlock();
				}
			}
		}
	}

	mailResidentFree(workBuf);
	if (titlePushHandle >= 0) shellNpPushUnregister(titlePushHandle);
	if (servicePushHandle >= 0) shellNpPushUnregister(servicePushHandle);
	sceKernelDeleteCallback(m_queueCb);
	sceKernelDeleteMsgPipe(pipeId);
	if (regCb != -1) {
		sceRegMgrStopCallback(regCb);
		sceRegMgrUnregisterCallback(regCb);
		sceKernelDeleteCallback(regCb);
	}
}

/* 0x811C8666 */
int MailResidentApp::ThreadEntry(SceSize args, void *argp)
{
	if (argp != NULL && args == 4)
		(*(MailResidentApp **)argp)->ThreadLoop();
	return 0;
}

/* 0x811C8678 */
void mailResidentAppNoop(void)
{
}
