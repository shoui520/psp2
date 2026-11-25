/*
 * SceShell mail_resident_plugin: MailResidentApp
 * 3.65-CEX: 0x811C6F94 - 0x811C867C
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <kernel/threadmgr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

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
extern int SceShell_83D16CA8(int ev, void *cb, void *arg);
extern int SceShell_E3D3A162(int ev, void *cb, void *arg);
}

extern void *shellGetCronMgr(void);				/* 0x810FB632 */
extern int shellRegisterPowerCb(void (*cb)(void *));		/* 0x8131F3E8 */
extern void shellUnregisterPowerCb(void);			/* 0x8131F422 */
extern int shellActivityIsEnabled(void);			/* 0x81326D30 */
extern int shellRegisterNetCb(void (*cb)(SceUInt32, void *), void *arg); /* 0x8132773E */
extern void shellUnregisterNetCb(int id);			/* 0x813277B8 */
extern int shellNpGetSignInState(int *outState);		/* 0x81329E20 */
extern void shellMailCronRegister(const char *id);		/* 0x812A4C6C */
extern void shellMailCronIsEnabled(const char *id, bool *out);	/* 0x812A4FC8 */
extern void shellMailCronSetCb(const char *id, void (*cb)(int, void *), void *arg); /* 0x812A5032 */
extern int shellMailHandleCustomCmd(const void *data, int flag); /* 0x813607BE */
extern int shellNpPushRegister1(const char *svc, int mode, void *cb, void *arg, int *outId); /* 0x813AEFFC */
extern int shellNpPushRegister2(const char *svc, int mode, void *cb, void *arg, int *outId); /* 0x813AF03A */
extern void shellNpPushUnregister(int id);			/* 0x813AF078 */

static MailResidentApp *s_mailAppInstance = NULL;		/* 0x81544E10 */

/* 0x811C6F94 */
MailResidentApp::MailResidentApp()
	: m_stopFlag(0), m_netState(1), m_initStage(0), m_reserved(0),
	  m_server(NULL), m_serverBuf(NULL), m_workBufSize(0),
	  m_sessionMutex("SceMailResidentAppSession", 0),
	  m_threadId(-1), m_evfProxy(-1), m_evfPsnMail(-1), m_evfReady(-1),
	  m_netCbId(-1), m_cronHandle(0), m_cronFlag0(1), m_cronFlag1(0),
	  m_queueMutex("SceMailResidentAppQueue", 0),
	  m_queueCb(-1),
	  m_syncMutex("SceMailResidentAppSync", 0),
	  m_pendingPush(0), m_pendingTimer(0), m_seqNo(0)
{
	s_mailAppInstance = this;
}

/* 0x811C7090 / 0x811C7178 */
MailResidentApp::~MailResidentApp()
{
	s_mailAppInstance = NULL;
	for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
	     it != m_syncReqs.end(); ++it) {
		if (*it) (*it)->Release();
	}
	for (paf::list<MailResidentEvent *>::iterator it = m_queue.begin();
	     it != m_queue.end(); ++it) {
		if (*it) (*it)->Release();
	}
	for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if (*it) (*it)->Release();
	}
}

static void pushQueueEvent(MailResidentApp *app, MailResidentEvent *ev)
{
	app->m_queueMutex.Lock();
	ev->AddRef();
	app->m_queue.push_back(ev);
	sceKernelNotifyCallback(app->m_queueCb, 0);
	app->m_queueMutex.Unlock();
}

/* 0x811C7188 */
void MailResidentApp::EnqueueEvent(int kind, SceUInt32 eventParam, int appState)
{
	switch (kind) {
	case 0:
		m_netState = eventParam;
		/* fallthrough */
	case 1:
	case 7: {
		void *mem = mailResidentMalloc(sizeof(MailResidentEvent));
		if (mem != NULL) {
			MailResidentEvent *ev = new (mem) MailResidentEvent(eventParam ? 7 : 6);
			pushQueueEvent(this, ev);
		}
		return;
	}
	case 2:
	case 3:
	case 6: {
		int evType = (kind == 6) ? 5 : 4;
		SceUInt8 *pendingFlag = (kind == 6) ? &m_pendingTimer : &m_pendingPush;
		if (*pendingFlag != 0)
			return;
		m_queueMutex.Lock();
		for (paf::list<MailResidentEvent *>::iterator it = m_queue.begin();
		     it != m_queue.end(); ++it) {
			MailResidentEvent *e = *it;
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
		for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
		     it != m_syncReqs.end(); ++it) {
			if ((*it)->m_evType == evType) {
				*pendingFlag = 1;
				m_syncMutex.Unlock();
				return;
			}
		}
		m_syncMutex.Unlock();

		void *mem = mailResidentMalloc(sizeof(MailResidentSyncEvent));
		if (mem != NULL) {
			MailResidentSyncEvent *ev = new (mem) MailResidentSyncEvent(evType, eventParam);
			pushQueueEvent(this, ev);
		}
		return;
	}
	case 4: {
		void *mem = mailResidentMalloc(sizeof(MailResidentAppEvent));
		if (mem != NULL) {
			MailResidentAppEvent *ev = new (mem) MailResidentAppEvent(eventParam, appState);
			pushQueueEvent(this, ev);
		}
		return;
	}
	case 5: {
		m_mw.SignalReady();
		void *mem = mailResidentMalloc(sizeof(MailResidentEvent));
		if (mem != NULL) {
			MailResidentEvent *ev = new (mem) MailResidentEvent(1);
			pushQueueEvent(this, ev);
		}
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
	if (arg == NULL && s_mailAppInstance != NULL)
		s_mailAppInstance->EnqueueEvent(3, 0, 0);
}

/* 0x811C7C14 */
static void mailAppEventCb(int ev, const void *info, int flags, void *arg)
{
	(void)flags;
	const SceUInt32 *p = (const SceUInt32 *)info;
	if (ev == 0 && sce_paf_strcmp("NPXS10063", (const char *)info + 8) == 0)
		((MailResidentApp *)arg)->EnqueueEvent(5, 0, 0);
	else
		((MailResidentApp *)arg)->EnqueueEvent(4, p[0], ev);
}

/* 0x811C7C4C */
static int mailTimerCb(int handle, void *arg)
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
	sce_paf_memset(&cfg, 0, sizeof(cfg));
	cfg.openLimit = 4;
	cfg.maxRequestSize = 200;
	cfg.pEventHandler = this;
	sce_paf_strncpy(cfg.serviceName, "ScePsnMailRs", 16);
	cfg.budget = 2;
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
	if (SceShell_83D16CA8(0, (void *)mailAppEventCb, this) < 0) return -1;
	m_initStage = 3;
	if (SceShell_83D16CA8(1, (void *)mailAppEventCb, this) < 0) return -1;
	m_initStage = 4;
	if (SceShell_83D16CA8(5, (void *)mailAppEventCb, this) < 0) return -1;
	m_initStage = 5;
	if (SceShell_83D16CA8(6, (void *)mailAppEventCb, this) < 0) return -1;
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
	case 6: SceShell_E3D3A162(6, (void *)mailAppEventCb, this); /* fallthrough */
	case 5: SceShell_E3D3A162(5, (void *)mailAppEventCb, this); /* fallthrough */
	case 4: SceShell_E3D3A162(1, (void *)mailAppEventCb, this); /* fallthrough */
	case 3: SceShell_E3D3A162(0, (void *)mailAppEventCb, this); /* fallthrough */
	case 2: shellUnregisterPowerCb(); /* fallthrough */
	case 1: shellUnregisterNetCb(m_netCbId); break;
	default: break;
	}
	m_initStage = 0;

	if (m_threadId != -1) {
		int stat = 0;
		sceKernelSetEventFlag(m_evfProxy, 0x80000000);
		sceKernelWaitThreadEnd(m_threadId, &stat, NULL);
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
	if (m_evfPsnMail != -1) {
		sceKernelDeleteEventFlag(m_evfPsnMail);
		m_evfPsnMail = -1;
	}
	for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if (*it) (*it)->Release();
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
		void *cron = shellGetCronMgr();
		(*(void (**)(void *, int))(*(void ***)cron + 8))(cron, m_cronHandle);
		(*(void (**)(void *, int))(*(void ***)cron + 5))(cron, m_cronHandle);
		m_cronHandle = 0;
	}
}

/* 0x811C77F8 */
void MailResidentApp::CancelPendingSyncs()
{
	m_syncMutex.Lock();
	for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
	     it != m_syncReqs.end(); ++it) {
		m_mw.RemoveRefKey((*it)->m_key);
	}
	m_pendingTimer = 0;
	m_pendingPush = 0;
	for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
	     it != m_syncReqs.end(); ++it) {
		if (*it) (*it)->Release();
	}
	m_syncReqs.clear();
	m_syncMutex.Unlock();
}

/* 0x811C7878 */
int MailResidentApp::EnsureMwRunning(SceInt64 key)
{
	m_mw.AddRefKey(key);
	int st = 0;
	if (m_mw.IsRunning(&st) != 0)
		return 0;
	CancelPendingSyncs();
	if ((((SceUInt32)(key >> 32)) & 0x11u) != 0x11u) {
		m_sessionMutex.Lock();
		m_mw.LaunchAndConnect();
		for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
		     it != m_sessions.end(); ++it) {
			(*it)->NotifyZero();
		}
		m_sessionMutex.Unlock();
	}
	return 1;
}

/* 0x811C7918 */
int MailResidentApp::OnNewSession(IPMI::Server *srv, const IPMI::Session::Config *cfg)
{
	void *mem = mailResidentMalloc(sizeof(MailResidentSession));
	MailResidentSession *sess = (mem != NULL) ? new (mem) MailResidentSession() : NULL;
	if (sess != NULL)
		sess->AddRef();
	IPMI::Session *ipmiSess = NULL;
	if (sess->Open(srv, cfg, &ipmiSess) != 0) {
		if (sess != NULL) sess->Release();
		return -1;
	}
	m_sessionMutex.Lock();
	for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if ((*it)->m_session == ipmiSess) {
			m_sessionMutex.Unlock();
			if (sess != NULL) sess->Release();
			return -1;
		}
	}
	sess->m_state = 5;
	sess->AddRef();
	m_sessions.push_back(sess);
	m_sessionMutex.Unlock();

	if (EnsureMwRunning((SceInt64)(SceUIntPtr)ipmiSess) == 0)
		sess->NotifyZero();
	if (sess != NULL) sess->Release();
	return 0;
}

/* 0x811C7A0A */
void MailResidentApp::ReleaseSessionKey(SceInt64 key)
{
	SceInt64 k1 = key | 0x0000040100000000LL;
	SceInt64 k2 = key | 0x0000080100000000LL;
	EnsureMwRunning(key | 0x0000000100000000LL);
	EnsureMwRunning(k2);
	void *mem = mailResidentMalloc(sizeof(MailResidentMwPairEvent));
	if (mem != NULL) {
		MailResidentMwPairEvent *ev = new (mem) MailResidentMwPairEvent(k1, k2);
		pushQueueEvent(this, ev);
	}
}

/* 0x811C7AC8 */
void MailResidentApp::OnSessionClosed(IPMI::Session *session)
{
	m_sessionMutex.Lock();
	for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
	     it != m_sessions.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->Close();
			(*it)->Release();
			m_sessions.erase(it);
			break;
		}
	}
	m_sessionMutex.Unlock();
	int st = 0;
	if (m_mw.IsRunning(&st) != 0)
		ReleaseSessionKey((SceInt64)(SceUIntPtr)session);
	m_mw.RemoveRefKey((SceInt64)(SceUIntPtr)session);
}

/* 0x811C7B90 */
void MailResidentApp::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 reqId,
	SceUInt32 methodId, const IPMI::DataInfo *pInList, SceUInt32 inCount)
{
	(void)inCount;
	int res = 0;
	if (methodId == 0x2010) {
		const void *inData = (pInList != NULL) ? pInList[0].pData : NULL;
		res = (inData != NULL) ? shellMailHandleCustomCmd(inData, 1) : (int)0x8068000F;
	}
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
	int outBuf[2] = { 0, 0 };
	int r = m_mw.SyncCall(method, inBuf, inSize, &res, outBuf, &outSize, 8);
	if ((r | res) != 0) {
		if (!isSpecial)
			m_mw.RemoveRefKey(key);
	} else {
		m_syncMutex.Lock();
		void *mem = mailResidentMalloc(sizeof(MailResidentSyncReq));
		if (mem != NULL) {
			MailResidentSyncReq *req = new (mem) MailResidentSyncReq(key, method, outBuf[1], evType);
			req->AddRef();
			m_syncReqs.push_back(req);
		}
		m_syncMutex.Unlock();
	}
}

/* 0x811C7D88 */
void MailResidentApp::UpdateFetchTimer()
{
	if (m_cronHandle != 0) {
		void *cron = shellGetCronMgr();
		(*(void (**)(void *, int))(*(void ***)cron + 8))(cron, m_cronHandle);
		(*(void (**)(void *, int))(*(void ***)cron + 5))(cron, m_cronHandle);
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
				void *cron = shellGetCronMgr();
				int r = (*(int (**)(void *, int *, int (*)(int, void *), void *, int, int, SceUInt8 *))
					(*(void ***)cron + 2))(cron, &m_cronHandle, mailTimerCb, this,
						interval, interval, &m_cronFlag0);
				if (r < 0 && m_cronHandle != 0) {
					(*(void (**)(void *, int))(*(void ***)cron + 8))(cron, m_cronHandle);
					(*(void (**)(void *, int))(*(void ***)cron + 5))(cron, m_cronHandle);
					m_cronHandle = 0;
				}
			}
		}
	}
}

/* 0x811C7E82 */
void MailResidentApp::ProcessQueue()
{
	m_queueMutex.Lock();
	if (m_queue.size() == 0) {
		m_queueMutex.Unlock();
		return;
	}
	MailResidentEvent *ev = m_queue.front();
	if (ev != NULL)
		ev->AddRef();
	m_queue.pop_front();
	m_queueMutex.Unlock();

	switch (ev->m_type) {
	case 0: {
		MailResidentAppEvent *ae = (MailResidentAppEvent *)ev;
		for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
		     it != m_sessions.end(); ++it) {
			if ((SceUInt32)(*it)->m_pid == ae->m_pid) {
				(*it)->m_state = ae->m_appState;
				void *mem = mailResidentMalloc(sizeof(MailResidentEvent));
				if (mem != NULL) {
					MailResidentEvent *e2 = new (mem) MailResidentEvent(1);
					pushQueueEvent(this, e2);
				}
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
			for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
			     it != m_sessions.end(); ++it) {
				if ((*it)->m_state == 5) {
					hasActive = true;
					break;
				}
			}
			if (hasActive) {
				m_mw.LaunchAndConnect();
				for (paf::list<MailResidentSession *>::iterator it = m_sessions.begin();
				     it != m_sessions.end(); ++it) {
					(*it)->NotifyZero();
				}
			}
			m_sessionMutex.Unlock();
		}
		break;
	}
	case 2: {
		MailResidentMwPairEvent *pe = (MailResidentMwPairEvent *)ev;
		int z1 = 0, z2 = 0;
		SendMwRequest(pe->m_key1, ev->m_type, 17, &z1, 4);
		SendMwRequest(pe->m_key2, ev->m_type, 13, &z2, 4);
		break;
	}
	case 3: {
		MailResidentDoneEvent *de = (MailResidentDoneEvent *)ev;
		int inPair[2] = { de->m_req->m_method, de->m_req->m_bit };
		int res = 0;
		SceSize outSz = 0;
		m_mw.SyncCall(25, inPair, 8, &res, NULL, &outSz, 0);

		MailResidentSyncReq *found = NULL;
		m_syncMutex.Lock();
		for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
		     it != m_syncReqs.end(); ++it) {
			if ((*it)->m_bit == inPair[1] && (*it)->m_method == inPair[0]) {
				found = *it;
				found->AddRef();
				found->Release();
				m_syncReqs.erase(it);
				break;
			}
		}
		m_syncMutex.Unlock();
		if (found != NULL) {
			if (found->m_evType == 5 && m_pendingTimer != 0) {
				m_pendingTimer = 0;
				int z[2] = { 0, 0 };
				SendMwRequest(found->m_key + 1, found->m_evType, 24, z, 8);
				ReleaseSessionKey(found->m_key + 1);
			} else if (found->m_evType == 4 && m_pendingPush != 0) {
				m_pendingPush = 0;
				int z[2] = { 1, 0 };
				SendMwRequest(found->m_key + 1, found->m_evType, 24, z, 8);
				ReleaseSessionKey(found->m_key + 1);
			}
			m_mw.RemoveRefKey(found->m_key);
			found->Release();
		}
		break;
	}
	case 4: {
		MailResidentSyncEvent *se = (MailResidentSyncEvent *)ev;
		SceUInt32 seq = ++m_seqNo;
		int reqBuf[2] = { 1, (int)se->m_arg };
		SceInt64 key = ((SceInt64)0x101 << 32) | seq;
		SendMwRequest(key, ev->m_type, 24, reqBuf, 8);
		ReleaseSessionKey(key);
		break;
	}
	case 5: {
		SceUInt32 seq = ++m_seqNo;
		int reqBuf[2] = { 0, 0 };
		SceInt64 key = ((SceInt64)0x201 << 32) | seq;
		SendMwRequest(key, ev->m_type, 24, reqBuf, 8);
		ReleaseSessionKey(key);
		break;
	}
	case 6:
	case 7:
		UpdateFetchTimer();
		break;
	default:
		break;
	}
	if (ev != NULL)
		ev->Release();
}

/* 0x811C82E0 */
static int mailQueueCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId; (void)notifyCount; (void)notifyArg;
	if (userData != NULL)
		((MailResidentApp *)userData)->ProcessQueue();
	return 0;
}

/* 0x811C82F8 */
static void mailNpPushCb1(int eventId, int status, const char *svc, int param1, int param2, void *arg)
{
	(void)eventId; (void)status; (void)param1; (void)param2;
	if (arg == NULL)
		return;
	if (sce_paf_strcasecmp(svc, "np:service:groupMessaging") == 0)
		((MailResidentApp *)arg)->EnqueueEvent(2, 1, 0);
	else if (sce_paf_strcasecmp(svc, "np:service:systemMessage") == 0)
		((MailResidentApp *)arg)->EnqueueEvent(2, 8, 0);
}

/* 0x811C8340 */
static void mailNpPushCb2(int eventId, int status, int subType, const char *svc, int param1, int param2, void *arg)
{
	(void)eventId; (void)status; (void)subType; (void)param1; (void)param2;
	if (arg == NULL)
		return;
	if (sce_paf_strcasecmp(svc, "np:service:gameCustomData") == 0)
		((MailResidentApp *)arg)->EnqueueEvent(2, 2, 0);
	else if (sce_paf_strcasecmp(svc, "np:service:invitation") == 0)
		((MailResidentApp *)arg)->EnqueueEvent(2, 4, 0);
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

	int pushId1 = -1;
	if (r >= 0)
		r = shellNpPushRegister1("np:service", 1, (void *)mailNpPushCb1, this, &pushId1);
	int pushId2 = -1;
	if (r >= 0)
		shellNpPushRegister2("np:service", 1, (void *)mailNpPushCb2, this, &pushId2);

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
			int msg[28];
			sce_paf_memset(msg, 0, sizeof(msg));
			SceSize recvSz = 0;
			while (sceKernelTryReceiveMsgPipe(pipeId, msg, sizeof(msg), 0, &recvSz) == 0 &&
			       recvSz == sizeof(msg)) {
				if (msg[0] == 2)
					EnqueueEvent(6, 0, 0);
			}
		} else {
			outBits &= ~0x80000000u;
			for (SceUInt32 bit = 0; bit < 32; ++bit) {
				if ((outBits & (1u << bit)) != 0) {
					m_syncMutex.Lock();
					for (paf::list<MailResidentSyncReq *>::iterator it = m_syncReqs.begin();
					     it != m_syncReqs.end(); ++it) {
						if ((*it)->m_bit == (int)bit) {
							void *mem = mailResidentMalloc(sizeof(MailResidentDoneEvent));
							if (mem != NULL) {
								MailResidentDoneEvent *de = new (mem) MailResidentDoneEvent(*it);
								pushQueueEvent(this, de);
							}
							break;
						}
					}
					m_syncMutex.Unlock();
				}
			}
		}
	}

	mailResidentFree(workBuf);
	if (pushId2 >= 0) shellNpPushUnregister(pushId2);
	if (pushId1 >= 0) shellNpPushUnregister(pushId1);
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
