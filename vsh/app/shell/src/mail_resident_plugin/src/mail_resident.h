/*
 * SceShell mail_resident_plugin: internal types and declarations
 */

#ifndef MAIL_RESIDENT_H
#define MAIL_RESIDENT_H

#include <scetypes.h>
#include <std/list>
#include <common/ref_obj.h>
#include <ipmi/client.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <thread/thread.h>
#include "shell_cron.h"

extern void *mailResidentMalloc(SceSize len);			/* 0x811C6ECC */
extern void *mailResidentRealloc(void *ptr, SceSize len);	/* 0x811C6EE2 */
extern void mailResidentFree(void *ptr);			/* 0x811C6EFA */

class MailResidentSession : public paf::common::RefObj {
public:
	MailResidentSession();					/* 0x811C867C */
	virtual ~MailResidentSession();				/* 0x811C86EE / 0x811C8746 */

	void Close();						/* 0x811C86B8 */
	int Open(IPMI::Server *server, const IPMI::Session::Config *cfg,
		IPMI::Session **outSession);			/* 0x811C87A2 */
	void NotifyZero();					/* 0x811C87F2 */

	IPMI::Session *m_session;				/* +8 */
	void *m_sessionBuf;					/* +12 */
	SceUID m_pid;						/* +16 */
	int m_state;						/* +20 */
	paf::thread::Mutex m_mutex;				/* +24 */
};

class MailResidentMwManager {
public:
	MailResidentMwManager();				/* 0x811C8858 */
	~MailResidentMwManager();				/* 0x811C8884 */

	int Connect();						/* 0x811C88A0 */
	void Disconnect(SceUInt8 graceful);			/* 0x811C89B2 */
	int IsRunning(int *outStatus);				/* 0x811C89EA */
	int SignalReady();					/* 0x811C8A42 */
	void LaunchAndConnect();				/* 0x811C8A5A */
	void AddRefKey(SceInt64 key);				/* 0x811C8B94 */
	void RemoveRefKey(SceInt64 key);			/* 0x811C8BEC */
	int SyncCall(SceUInt32 methodId, const void *inBuf, SceSize inSize,
		int *outRes, void *outBuf, SceSize *outSize, SceSize maxOut); /* 0x811C8C94 */

	static int LaunchCallback(int notifyId, int notifyCount, int notifyArg, void *userData); /* 0x811C8CBE */

	SceUInt8 m_destroyed;					/* +0 */
	SceUID m_evf;						/* +4 */
	IPMI::Client *m_client;					/* +8 */
	SceUInt32 m_keyCount;					/* +12 */
	SceInt64 *m_keys;					/* +16 */
};

struct MailResidentEvent : public paf::common::RefObj {
	int m_type;						/* +8 */
	MailResidentEvent(int type) : m_type(type) {}
	virtual ~MailResidentEvent() {}
};

struct MailResidentAppEvent : public MailResidentEvent {
	SceUInt32 m_pid;					/* +12 */
	int m_appState;						/* +16 */
	MailResidentAppEvent(SceUInt32 pid, int appState)
		: MailResidentEvent(0), m_pid(pid), m_appState(appState) {}
};

struct MailResidentSyncEvent : public MailResidentEvent {
	SceUInt32 m_arg;					/* +12 */
	MailResidentSyncEvent(int type, SceUInt32 arg)
		: MailResidentEvent(type), m_arg(arg) {}
};

struct MailResidentMwPairEvent : public MailResidentEvent {
	SceInt64 m_closeSessionKey;				/* +16 */
	SceInt64 m_flushKey;					/* +24 */
	MailResidentMwPairEvent(SceInt64 closeSessionKey, SceInt64 flushKey)
		: MailResidentEvent(2), m_closeSessionKey(closeSessionKey),
		  m_flushKey(flushKey) {}
};

struct MailResidentSyncReq : public paf::common::RefObj {
	SceInt64 m_key;						/* +8 */
	int m_method;						/* +16 */
	int m_bit;						/* +20 */
	int m_evType;						/* +24 */
	MailResidentSyncReq(SceInt64 k, int m, int b, int et)
		: m_key(k), m_method(m), m_bit(b), m_evType(et) {}
	virtual ~MailResidentSyncReq() {}
};

struct MailResidentDoneEvent : public MailResidentEvent {
	MailResidentSyncReq *m_req;				/* +12 */
	MailResidentDoneEvent(MailResidentSyncReq *req)
		: MailResidentEvent(3), m_req(req)
	{
		if (m_req)
			m_req->AddRef();
	}
	virtual ~MailResidentDoneEvent()
	{
		if (m_req)
			m_req->Release();
	}
};

class MailResidentApp : public IPMI::Server::EventHandler {
public:
	MailResidentApp();					/* 0x811C6F94 */
	virtual ~MailResidentApp();				/* 0x811C7090 / 0x811C7178 */

	int Start();						/* 0x811C7476 */
	void Stop();						/* 0x811C76B6 */
	void EnqueueEvent(int kind, SceUInt32 eventParam, int appState); /* 0x811C7188 */
	void CancelPendingSyncs();				/* 0x811C77F8 */
	int EnsureMwRunning(SceInt64 key);			/* 0x811C7878 */
	int OnNewSession(IPMI::Server *srv, const IPMI::Session::Config *cfg); /* 0x811C7918 */
	void ReleaseSessionKey(SceInt64 key);			/* 0x811C7A0A */
	void OnSessionClosed(IPMI::Session *session);		/* 0x811C7AC8 */
	void SendMwRequest(SceInt64 key, int evType, int method, const void *inBuf, SceSize inSize); /* 0x811C7C6E */
	void UpdateFetchTimer();				/* 0x811C7D88 */
	void ProcessQueue();					/* 0x811C7E82 */
	void ThreadLoop();					/* 0x811C8388 */

	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
		const void *pUserData, SceSize userDataSize)
	{
		(void)pUserData; (void)userDataSize;
		return OnNewSession(pServer, pConfig);
	}
	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
		const IPMI::DataInfo *pInList, SceUInt32 inCount,
		IPMI::BufferInfo *pOutList, SceUInt32 outCount)
	{
		(void)pSession; (void)methodId; (void)pInList; (void)inCount; (void)pOutList; (void)outCount;
	}
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 reqId, SceUInt32 methodId,
		const IPMI::DataInfo *pInList, SceUInt32 inCount);
	virtual void onSessionClosedByClient(IPMI::Session *pSession) { OnSessionClosed(pSession); }
	virtual void onSessionKilled(IPMI::Session *pSession) { OnSessionClosed(pSession); }

	static int ThreadEntry(SceSize args, void *argp);	/* 0x811C8666 */

	SceUInt8 m_stopFlag;					/* +4 */
	SceUInt32 m_netState;					/* +8 */
	int m_initStage;					/* +12 */
	int m_reserved;						/* +16 */
	IPMI::Server *m_server;					/* +20 */
	void *m_serverBuf;					/* +24 */
	SceSize m_workBufSize;					/* +28 */
	paf::thread::Mutex m_sessionMutex;			/* +32 */
	paf::list<MailResidentSession *> m_sessions;		/* +64 */
	SceUID m_threadId;					/* +76 */
	SceUID m_evfProxy;					/* +80 */
	SceUID m_evfPsnMail;					/* +84 */
	SceUID m_evfReady;					/* +88 */
	int m_netCbId;						/* +92 */
	SceUID m_cronHandle;					/* +96 */
	ShellCronOption m_cronOption;				/* +100 */
	paf::thread::Mutex m_queueMutex;			/* +104 */
	paf::list<MailResidentEvent *> m_queue;			/* +136 */
	SceUID m_queueCb;					/* +148 */
	MailResidentMwManager m_mw;				/* +152 */
	paf::thread::Mutex m_syncMutex;				/* +184 */
	paf::list<MailResidentSyncReq *> m_syncReqs;		/* +216 */
	SceUInt8 m_pendingPush;					/* +228 */
	SceUInt8 m_pendingTimer;				/* +229 */
	SceUInt32 m_seqNo;					/* +232 */
};

#endif /* MAIL_RESIDENT_H */
