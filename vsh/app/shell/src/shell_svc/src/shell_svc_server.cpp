/*
 * SceShell shell_svc: SceShellSvcServer IPMI server & LiveAreaUtil async enqueuers
 * 3.65-CEX: 0x811B7538 - 0x811B84F2 (sti 0x811B8480)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <job/job.h>
#include <kernel/threadmgr.h>

#include "../../livespace_db/lsdb_keys.h"
#include "shell_svc_livearea_job.h"

extern void *g_pafFramework;					/* 0x81488474 */

extern "C" {
extern int sceClibPrintf(const char *fmt, ...);
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *statusBuf);
}

/* Dispatch table handlers */
typedef int (*ShellSvcSyncFn)(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
typedef int (*ShellSvcAsyncFn)(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
typedef void (*ShellSvcDisconnFn)(IPMI::Session *);

struct ShellSvcDispatchEntry {
	SceUInt32 minId;
	SceUInt32 maxId;
	ShellSvcSyncFn syncFn;
	ShellSvcAsyncFn asyncFn;
	ShellSvcDisconnFn disconnFn;
};

/* LiveAreaUtil (0x110000..0x11FFFF) handlers */
extern int shellSvcLiveAreaSyncDefault(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32); /* 0x8115B554 */
extern int shellSvcLiveAreaDispatchAsync(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId, const IPMI::DataInfo *inBufs, SceUInt32 inCount); /* 0x811B86A4 */
extern void shellSvcLiveAreaCancelSession(IPMI::Session *session); /* 0x811B8826 */
extern void shellSvcLiveAreaSendUpdateResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811B84F2 */
extern void shellSvcLiveAreaSendSetResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811B8512 */
extern void shellSvcLiveAreaSendSetMultiResp(IPMI::Session *session, SceUInt32 reqId, int err); /* 0x811B85F2 */
extern void shellSvcLiveAreaSendGetTsResp(IPMI::Session *session, SceUInt32 reqId, int err, SceUInt64 *ts); /* 0x811B8614 */
extern void shellSvcLiveAreaSendGetContentResp(IPMI::Session *session, SceUInt32 reqId, int err, const void *buf, SceUInt32 *size); /* 0x811B8658 */
extern void *shellSvcLiveAreaJobBaseCtor(void *mem, IPMI::Session *session, SceUInt32 reqId,
	const char *path, const char *titleId, SceUInt8 flag);	/* 0x811B8810 */

extern int shellSvcLiveAreaUpdateJobRun(void *job);		/* 0x811B886C */
extern int shellSvcLiveAreaSetContentJobRun(void *job);		/* 0x811B8B4A */
extern int shellSvcLiveAreaGetTsJobRun(void *job);		/* 0x811B8E58 */
extern int shellSvcLiveAreaGetContentJobRun(void *job);		/* 0x811B8F74 */

/* Service table external dispatchers */
extern int shellSvcDisp_02_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_02_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_02_Disconn(IPMI::Session *);
extern int shellSvcDisp_03_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_03_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_03_Disconn(IPMI::Session *);
extern int shellSvcDisp_05_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_05_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_05_Disconn(IPMI::Session *);
extern int shellSvcDisp_07_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_07_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_07_Disconn(IPMI::Session *);
extern int shellSvcDisp_08_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_08_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern int shellSvcDisp_09_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_09_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_09_Disconn(IPMI::Session *);
extern int shellSvcDisp_0A_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0A_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern int shellSvcDisp_0B_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0B_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_0B_Disconn(IPMI::Session *);
extern int shellSvcDisp_0C_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0C_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_0C_Disconn(IPMI::Session *);
extern int shellSvcDisp_0D_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0D_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_0D_Disconn(IPMI::Session *);
extern int shellSvcDisp_0E_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0E_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_0E_Disconn(IPMI::Session *);
extern int shellSvcDisp_0F_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_0F_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_0F_Disconn(IPMI::Session *);
extern int shellSvcDisp_10_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_10_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_10_Disconn(IPMI::Session *);
extern int shellSvcDisp_12_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_12_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern int shellSvcDisp_13_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_13_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_13_Disconn(IPMI::Session *);
extern int shellSvcDisp_14_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_14_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern int shellSvcDisp_15_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_15_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern int shellIncomingDialogServerOnSyncMethod(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellIncomingDialogServerOnAsyncMethod(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellIncomingDialogServerOnSessionClosed(IPMI::Session *);
extern int shellSvcDisp_18_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_18_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_18_Disconn(IPMI::Session *);
extern int shellSvcDisp_19_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_19_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);
extern void shellSvcDisp_19_Disconn(IPMI::Session *);
extern int shellSvcDisp_1A_Sync(IPMI::Session *, SceUInt32, const IPMI::DataInfo *, SceUInt32, IPMI::BufferInfo *, SceUInt32);
extern int shellSvcDisp_1A_Async(IPMI::Session *, SceUInt32, SceUInt32, const IPMI::DataInfo *, SceUInt32);

static void shellSvcNoopDisconn(IPMI::Session *) {}
static int shellSvcLiveAreaSyncEntry(IPMI::Session *s, SceUInt32 m, const IPMI::DataInfo *i, SceUInt32 ic, IPMI::BufferInfo *o, SceUInt32 oc);
static int shellSvcLiveAreaAsyncEntry(IPMI::Session *s, SceUInt32 r, SceUInt32 m, const IPMI::DataInfo *i, SceUInt32 ic);
static void shellSvcLiveAreaDisconnEntry(IPMI::Session *s);

static const ShellSvcDispatchEntry s_shellSvcDispatchTable[] = { /* 0x81511DDC */
	{ 0x00020000, 0x0002FFFF, shellSvcDisp_02_Sync, shellSvcDisp_02_Async, shellSvcDisp_02_Disconn },
	{ 0x00030000, 0x0003FFFF, shellSvcDisp_03_Sync, shellSvcDisp_03_Async, shellSvcDisp_03_Disconn },
	{ 0x00050000, 0x0005FFFF, shellSvcDisp_05_Sync, shellSvcDisp_05_Async, shellSvcDisp_05_Disconn },
	{ 0x00070000, 0x0007FFFF, shellSvcDisp_07_Sync, shellSvcDisp_07_Async, shellSvcDisp_07_Disconn },
	{ 0x00080000, 0x0008FFFF, shellSvcDisp_08_Sync, shellSvcDisp_08_Async, shellSvcNoopDisconn },
	{ 0x00090000, 0x0009FFFF, shellSvcDisp_09_Sync, shellSvcDisp_09_Async, shellSvcDisp_09_Disconn },
	{ 0x000A0000, 0x000AFFFF, shellSvcDisp_0A_Sync, shellSvcDisp_0A_Async, NULL },
	{ 0x000B0000, 0x000BFFFF, shellSvcDisp_0B_Sync, shellSvcDisp_0B_Async, shellSvcDisp_0B_Disconn },
	{ 0x000C0000, 0x000CFFFF, shellSvcDisp_0C_Sync, shellSvcDisp_0C_Async, shellSvcDisp_0C_Disconn },
	{ 0x000D0000, 0x000DFFFF, shellSvcDisp_0D_Sync, shellSvcDisp_0D_Async, shellSvcDisp_0D_Disconn },
	{ 0x000E0000, 0x000EFFFF, shellSvcDisp_0E_Sync, shellSvcDisp_0E_Async, shellSvcDisp_0E_Disconn },
	{ 0x000F0000, 0x000FFFFF, shellSvcDisp_0F_Sync, shellSvcDisp_0F_Async, shellSvcDisp_0F_Disconn },
	{ 0x00100000, 0x0010FFFF, shellSvcDisp_10_Sync, shellSvcDisp_10_Async, shellSvcDisp_10_Disconn },
	{ 0x00110000, 0x0011FFFF, shellSvcLiveAreaSyncEntry, shellSvcLiveAreaAsyncEntry, shellSvcLiveAreaDisconnEntry },
	{ 0x00120000, 0x0012FFFF, shellSvcDisp_12_Sync, shellSvcDisp_12_Async, shellSvcNoopDisconn },
	{ 0x00130000, 0x0013FFFF, shellSvcDisp_13_Sync, shellSvcDisp_13_Async, shellSvcDisp_13_Disconn },
	{ 0x00140000, 0x0014FFFF, shellSvcDisp_14_Sync, shellSvcDisp_14_Async, shellSvcNoopDisconn },
	{ 0x00150000, 0x0015FFFF, shellSvcDisp_15_Sync, shellSvcDisp_15_Async, NULL },
	{ 0x00170000, 0x0017FFFF, shellIncomingDialogServerOnSyncMethod, shellIncomingDialogServerOnAsyncMethod, shellIncomingDialogServerOnSessionClosed },
	{ 0x00180000, 0x0018FFFF, shellSvcDisp_18_Sync, shellSvcDisp_18_Async, shellSvcDisp_18_Disconn },
	{ 0x00190000, 0x0019FFFF, shellSvcDisp_19_Sync, shellSvcDisp_19_Async, shellSvcDisp_19_Disconn },
	{ 0x001A0000, 0x001AFFFF, shellSvcDisp_1A_Sync, shellSvcDisp_1A_Async, shellSvcNoopDisconn },
	{ 0, 0, NULL, NULL, NULL }
};

struct ShellSvcSessionSlot {
	ShellSvcSessionSlot *next;				/* +0 */
	IPMI::Session *session;					/* +4 */
	SceUInt8 buf[640];					/* +8 */
};

class ShellSvcServer : public IPMI::Server::EventHandler {
public:
	ShellSvcServer() : m_server(NULL), m_activeCount(0), m_freeHead(NULL), m_state(0), m_threadId(-1), m_workBuf(NULL), m_workBufSize(0) {}
	virtual ~ShellSvcServer() {}

	int Init();
	int Start();

	virtual void onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
		const IPMI::DataInfo *inList, SceUInt32 inCount,
		IPMI::BufferInfo *outList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
		const IPMI::DataInfo *inList, SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *server, const IPMI::Session::Config *cfg,
		const void *userData, SceSize userDataSize);
	virtual void onSessionClosedByClient(IPMI::Session *session);
	virtual void onSessionKilled(IPMI::Session *session);

private:
	static int ThreadEntry(SceSize args, void *argp);

	IPMI::Server *m_server;					/* +4 */
	ShellSvcSessionSlot m_slots[16];			/* +8 (16 * 648 = 10368 bytes) */
	int m_activeCount;					/* +10376 */
	ShellSvcSessionSlot *m_freeHead;			/* +10380 */
	int m_state;						/* +10384 */
	SceUID m_threadId;					/* +10388 */
	void *m_workBuf;					/* +10392 */
	SceSize m_workBufSize;					/* +10396 */
};

static ShellSvcServer g_shellSvcServer;				/* 0x81568D90 */

/* 0x811B7538 */
int shellSvcServerInit(void)
{
	int r = g_shellSvcServer.Init();
	return (r == 0) ? 0 : r;
}

/* 0x811B7550 */
int shellSvcServerStart(void)
{
	return g_shellSvcServer.Start();
}

/* 0x811B7560 */
int shellSvcNotifyEvent(IPMI::Session *session, int ev)
{
	if (ev < 128)
		return session->setEventFlag(ev / 32, 1u << (ev % 32));
	return (int)0x80100910;
}

/* 0x811B75C0 */
int ShellSvcServer::Init()
{
	if (m_server != NULL)
		return (int)0x80100901;

	sce_paf_memset(m_slots, 0, sizeof(m_slots));
	m_activeCount = 0;
	m_freeHead = NULL;
	m_state = 0;

	for (SceUInt32 i = 0; i < 16; ++i) {
		ShellSvcSessionSlot *slot = &m_slots[i];
		if (slot->session != NULL) {
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE\n");
			sceKernelDelayThread(1000000);
			__builtin_trap();
		}
		if (m_freeHead == NULL) {
			m_freeHead = slot;
		} else {
			ShellSvcSessionSlot *tail = m_freeHead;
			while (tail->next != NULL)
				tail = tail->next;
			tail->next = slot;
		}
		slot->next = NULL;
	}

	IPMI::Server::Config cfg;
	cfg.maxRequestSize = 3840;
	cfg.maxMessageSize = 16128;
	cfg.pEventHandler = this;
	sce_paf_strncpy(cfg.serviceName, "SceShellSvc", 16);
	cfg.budget = 0;
	cfg.openLimit = -1;

	m_workBufSize = IPMI::Server::getWorkBufferSize(&cfg);
	void *srvMem = sce_paf_malloc(0x100);
	m_workBuf = sce_paf_memalign(0x10, m_workBufSize);
	if (!srvMem || !m_workBuf) {
		sce_paf_free(srvMem);
		sce_paf_free(m_workBuf);
		return (int)0x80100941;
	}

	int r = IPMI::Server::create(&m_server, &cfg, this, srvMem);
	if (r != 0) {
		sce_paf_free(srvMem);
		sce_paf_free(m_workBuf);
		return r;
	}

	m_threadId = sceKernelCreateThread("SceShellSvcServer", ThreadEntry, 80, 0xC000, 0, 0, NULL);
	if (m_threadId < 0) {
		int dr = m_server->destroy();
		if (dr != 0) {
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE (0x%x)\n", dr);
			sceKernelDelayThread(1000000);
			__builtin_trap();
		}
		sce_paf_free(srvMem);
		sce_paf_free(m_workBuf);
		return m_threadId;
	}

	m_state = 1;
	return 0;
}

/* 0x811B7760 */
int ShellSvcServer::Start()
{
	if (m_state != 1)
		return (int)0x80100902;
	m_state = 2;
	ShellSvcServer *self = this;
	sceKernelStartThread(m_threadId, sizeof(self), &self);
	return 0;
}

/* 0x811B77AC */
void ShellSvcServer::onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount,
	IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	const ShellSvcDispatchEntry *e = s_shellSvcDispatchTable;
	while (e->maxId != 0) {
		if (e->minId <= methodId && methodId <= e->maxId)
			break;
		++e;
	}
	if (!e->syncFn) {
		sceClibPrintf("[SceShellSvc] Ignored invalid syncMethodId 0x%x\n", methodId);
		return;
	}
	int r = e->syncFn(session, methodId, inList, inCount, outList, outCount);
	if (r != 0)
		sceClibPrintf("[SceShellSvc] Calling sync method failed. (MethodId 0x%x, error 0x%x)\n", methodId, r);
}

/* 0x811B781C */
void ShellSvcServer::onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	const ShellSvcDispatchEntry *e = s_shellSvcDispatchTable;
	while (e->maxId != 0) {
		if (e->minId <= methodId && methodId <= e->maxId)
			break;
		++e;
	}
	if (!e->asyncFn) {
		sceClibPrintf("[SceShellSvc] Ignored invalid asyncMethodId 0x%x\n", methodId);
		return;
	}
	int r = e->asyncFn(session, reqId, methodId, inList, inCount);
	if (r != 0)
		sceClibPrintf("[SceShellSvc] Calling async method failed. (MethodId 0x%x, error 0x%x)\n", methodId, r);
}

/* 0x811B7890 */
int ShellSvcServer::onNewClient(IPMI::Server *server, const IPMI::Session::Config *cfg,
	const void *, SceSize userDataSize)
{
	if (userDataSize != 16)
		return (int)0x80100921;
	if (m_activeCount == 16)
		return (int)0x80100942;

	if (cfg->bufferSize > 0x280u) {
		sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE\n");
		sceKernelDelayThread(1000000);
		__builtin_trap();
	}

	ShellSvcSessionSlot *slot = m_freeHead;
	if (!slot) {
		sceClibPrintf("[SceShellSvcServer] *ERROR* Too many sessions. Rejected new client request.\n");
		return (int)0x80100941;
	}
	m_freeHead = slot->next;
	slot->next = NULL;

	int r = server->createSession(&slot->session, slot, slot->buf);
	if (r != 0) {
		if (slot->session != NULL) {
			sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE\n");
			sceKernelDelayThread(1000000);
			__builtin_trap();
		}
		if (m_freeHead == NULL) {
			m_freeHead = slot;
		} else {
			ShellSvcSessionSlot *tail = m_freeHead;
			while (tail->next != NULL)
				tail = tail->next;
			tail->next = slot;
		}
		slot->next = NULL;
		return (r == (int)0x8002050C) ? (int)0x80100903 : (int)0x801009FF;
	}

	char statusBuf[128];
	sce_paf_memset(statusBuf, 0, sizeof(statusBuf));
	int st = sceAppMgrGetStatusByAppId(slot->session->getAppId(), statusBuf);
	sce_paf_printf_dummy();
	m_activeCount++;
	return 0;
}

/* 0x811B79B4 */
void ShellSvcServer::onSessionClosedByClient(IPMI::Session *session)
{
	char statusBuf[128];
	sce_paf_memset(statusBuf, 0, sizeof(statusBuf));
	int st = sceAppMgrGetStatusByAppId(session->getAppId(), statusBuf);
	sce_paf_printf_dummy();

	for (const ShellSvcDispatchEntry *e = s_shellSvcDispatchTable; e->maxId != 0; ++e) {
		if (e->disconnFn)
			e->disconnFn(session);
	}

	ShellSvcSessionSlot *slot = NULL;
	for (SceUInt32 i = 0; i < 16; ++i) {
		if (m_slots[i].session == session) {
			slot = &m_slots[i];
			break;
		}
	}

	int dr = session->destroy();
	if (dr != 0) {
		sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE (0x%x)\n", dr);
		sceKernelDelayThread(1000000);
		__builtin_trap();
	}

	slot->session = NULL;
	m_activeCount--;
	if (m_freeHead == NULL) {
		m_freeHead = slot;
	} else {
		ShellSvcSessionSlot *tail = m_freeHead;
		while (tail->next != NULL)
			tail = tail->next;
		tail->next = slot;
	}
	slot->next = NULL;
}

/* 0x811B7A90 */
void ShellSvcServer::onSessionKilled(IPMI::Session *session)
{
	sce_paf_printf_dummy();
	for (const ShellSvcDispatchEntry *e = s_shellSvcDispatchTable; e->maxId != 0; ++e) {
		if (e->disconnFn)
			e->disconnFn(session);
	}

	ShellSvcSessionSlot *slot = NULL;
	for (SceUInt32 i = 0; i < 16; ++i) {
		if (m_slots[i].session == session) {
			slot = &m_slots[i];
			break;
		}
	}

	int dr = session->destroy();
	if (dr != 0) {
		sceClibPrintf("[SceIpmi] INTERNAL ASSERTION FAILURE (0x%x)\n", dr);
		sceKernelDelayThread(1000000);
		__builtin_trap();
	}

	slot->session = NULL;
	m_activeCount--;
	if (m_freeHead == NULL) {
		m_freeHead = slot;
	} else {
		ShellSvcSessionSlot *tail = m_freeHead;
		while (tail->next != NULL)
			tail = tail->next;
		tail->next = slot;
	}
	slot->next = NULL;
}

/* 0x811B7B30 */
int ShellSvcServer::ThreadEntry(SceSize args, void *argp)
{
	(void)args;
	sce_paf_printf_dummy();
	ShellSvcServer *self = *(ShellSvcServer **)argp;
	self->m_server->runDispatcher(self->m_workBuf, self->m_workBufSize);
	sce_paf_printf_dummy();
	return 0;
}

/* ------------------------------------------------------------------ */
/* LiveAreaUtil async job enqueuers (0x811B7B54 - 0x811B8480)         */
/* ------------------------------------------------------------------ */

struct LiveAreaUtilUpdateJob : public LiveAreaUtilJobBase {
	LiveAreaUtilUpdateJob(IPMI::Session *session, SceUInt32 reqId,
		const char *path, const char *titleId, const SceUInt64 *ts,
		SceUInt8 allowAny, SceUInt8 flag)
		: LiveAreaUtilJobBase(session, reqId, path, titleId, flag),
		  m_ts(ts ? *ts : 0),
		  m_allowAny(allowAny) {}

	SceUInt64 m_ts;						/* +64 */
	SceUInt8 m_allowAny;					/* +72 */
	virtual int Run() { return shellSvcLiveAreaUpdateJobRun(this); }
};

struct LiveAreaUtilSetContentJob : public LiveAreaUtilJobBase {
	LiveAreaUtilSetContentJob(IPMI::Session *session, SceUInt32 reqId,
		const char *path, const char *titleId, int flags,
		SceUInt8 allowAny, SceUInt8 isMulti, SceUInt8 flag)
		: LiveAreaUtilJobBase(session, reqId, path, titleId, flag),
		  m_flags(flags),
		  m_allowAny(allowAny),
		  m_isMulti(isMulti),
		  m_xmls() {}

	int m_flags;						/* +64 */
	SceUInt8 m_allowAny;					/* +68 */
	SceUInt8 m_isMulti;					/* +69 */
	paf::vector<paf::string> m_xmls;			/* +72 */
	virtual int Run() { return shellSvcLiveAreaSetContentJobRun(this); }
};

struct LiveAreaUtilGetTsJob : public LiveAreaUtilJobBase {
	LiveAreaUtilGetTsJob(IPMI::Session *session, SceUInt32 reqId,
		const char *frameId, SceUInt8 flag)
		: LiveAreaUtilJobBase(session, reqId, NULL, "", flag),
		  m_frameId(frameId ? frameId : "") {}

	paf::string m_frameId;					/* +64 */
	virtual int Run() { return shellSvcLiveAreaGetTsJobRun(this); }
};

struct LiveAreaUtilGetContentJob : public LiveAreaUtilJobBase {
	LiveAreaUtilGetContentJob(IPMI::Session *session, SceUInt32 reqId,
		const char *frameId, SceUInt32 maxSize, SceUInt8 flag)
		: LiveAreaUtilJobBase(session, reqId, NULL, "", flag),
		  m_frameId(frameId ? frameId : ""),
		  m_maxSize(maxSize) {}

	paf::string m_frameId;					/* +64 */
	SceUInt32 m_maxSize;					/* +76 */
	virtual int Run() { return shellSvcLiveAreaGetContentJobRun(this); }
};

/* 0x811B7B54 */
int shellSvcLiveAreaEnqueueUpdate(IPMI::Session *session, SceUInt32 reqId,
	const char *path, const char *titleId, const SceUInt64 *ts,
	SceUInt8 allowAny, SceUInt8 flag)
{
	LiveAreaUtilUpdateJob *job = new LiveAreaUtilUpdateJob(
		session, reqId, path, titleId, ts, allowAny, flag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (paf::job::JobQueue::default_queue->Enqueue(sp) != 0)
		shellSvcLiveAreaSendUpdateResp(session, reqId, (int)0x801040FF);
	return 0;
}

/* 0x811B7C8C */
int shellSvcLiveAreaEnqueueSetContent(IPMI::Session *session, SceUInt32 reqId,
	int, const paf::common::SharedPtr<paf::Buffer> *bufs, SceUInt32 bufCount,
	const char *path, int flags, const char *titleId,
	SceUInt8 allowAny, SceUInt8 isMulti, SceUInt8 flag)
{
	LiveAreaUtilSetContentJob *job = new LiveAreaUtilSetContentJob(
		session, reqId, path, titleId, flags, allowAny, isMulti, flag);
	if (job) {
		job->m_xmls.resize(bufCount);
		for (SceUInt32 i = 0; i < bufCount; ++i)
			job->m_xmls[i].assign((const char *)bufs[i]->GetAddr(), bufs[i]->GetSize());
	}
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	int r = paf::job::JobQueue::default_queue->Enqueue(sp);
	if (r != 0)
		r = (int)0x801040FF;
	return r;
}

/* 0x811B7E32 */
int shellSvcLiveAreaSetContentSingle(IPMI::Session *session, SceUInt32 reqId,
	int methodId, const void *xmlData, SceUInt32 xmlSize,
	const char *path, int flags, const char *titleId,
	SceUInt8 allowAny, SceUInt8 flag)
{
	paf::common::SharedPtr<paf::Buffer> buf = paf::UnmanagedConstBuffer::Create(xmlData, xmlSize);
	int r;
	if (!buf.get()) {
		r = (int)0x801040FF;
		shellSvcLiveAreaSendSetResp(session, reqId, r);
	} else {
		r = shellSvcLiveAreaEnqueueSetContent(session, reqId, methodId, &buf, 1, path, flags, titleId, allowAny, 0, flag);
		if (r != 0)
			shellSvcLiveAreaSendSetResp(session, reqId, r);
	}
	return 0;
}

/* 0x811B7F1C */
int shellSvcLiveAreaSetContentMulti(IPMI::Session *session, SceUInt32 reqId,
	int methodId, const char **xmlList, SceUInt32 count,
	const char *path, int flags, const char *titleId, SceUInt8 allowAny)
{
	paf::vector<paf::common::SharedPtr<paf::Buffer> > bufs;
	int r = 0;
	for (SceUInt32 i = 0; i < count; ++i) {
		paf::common::SharedPtr<paf::Buffer> b =
			paf::UnmanagedConstBuffer::Create(xmlList[i], sce_paf_strlen(xmlList[i]));
		if (!b.get()) {
			r = (int)0x801040FF;
			break;
		}
		bufs.push_back(b);
	}
	if (r == 0)
		r = shellSvcLiveAreaEnqueueSetContent(session, reqId, methodId,
			bufs.size() ? &bufs[0] : NULL, bufs.size(), path, flags, titleId, allowAny, 1, 0);
	if (r != 0)
		shellSvcLiveAreaSendSetMultiResp(session, reqId, r);
	return 0;
}

/* 0x811B804C */
int shellSvcLiveAreaEnqueueGetTs(IPMI::Session *session, SceUInt32 reqId,
	const char *frameId, SceUInt8 flag)
{
	LiveAreaUtilGetTsJob *job = new LiveAreaUtilGetTsJob(session, reqId, frameId, flag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (paf::job::JobQueue::default_queue->Enqueue(sp) != 0) {
		SceUInt64 zeroTs = 0;
		shellSvcLiveAreaSendGetTsResp(session, reqId, (int)0x801040FF, &zeroTs);
	}
	return 0;
}

/* 0x811B8250 */
int shellSvcLiveAreaEnqueueGetContent(IPMI::Session *session, SceUInt32 reqId,
	const char *frameId, SceUInt32 maxSize, SceUInt8 flag)
{
	LiveAreaUtilGetContentJob *job = new LiveAreaUtilGetContentJob(
		session, reqId, frameId, maxSize, flag);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	if (paf::job::JobQueue::default_queue->Enqueue(sp) != 0) {
		SceUInt32 zeroSz = 0;
		shellSvcLiveAreaSendGetContentResp(session, reqId, (int)0x801040FF, NULL, &zeroSz);
	}
	return 0;
}

/* 0x811B8454 */
static int shellSvcLiveAreaSyncEntry(IPMI::Session *s, SceUInt32 m,
	const IPMI::DataInfo *i, SceUInt32 ic, IPMI::BufferInfo *o, SceUInt32 oc)
{
	return shellSvcLiveAreaSyncDefault(s, m, i, ic, o, oc);
}

/* 0x811B8468 */
static int shellSvcLiveAreaAsyncEntry(IPMI::Session *s, SceUInt32 r, SceUInt32 m,
	const IPMI::DataInfo *i, SceUInt32 ic)
{
	return shellSvcLiveAreaDispatchAsync(s, r, m, i, ic);
}

/* 0x811B8478 */
static void shellSvcLiveAreaDisconnEntry(IPMI::Session *s)
{
	shellSvcLiveAreaCancelSession(s);
}
