/*
 * ime_service.h: the shell's IME service. The "SceImeService" IPMI server takes the
 * applications' IME requests and runs ime_plugin for them as an exclusive function, one client
 * at a time; the IME settings ("/CONFIG/IME/...") are kept per language and written back by a
 * job. The kernel object names and the job item names are Sony's; the rest of the names are
 * ours (PLACEHOLDER).
 */
#ifndef _SHELL_IME_SERVICE_H
#define _SHELL_IME_SERVICE_H

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <job/job.h>
#include <plugin.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <std/string.h>
#include "exclusive_func.h"

/* the service's errors (PLACEHOLDER names) */
#define IME_SERVICE_ERROR_INVALID_PARAM		(-2146433136)	/* 0x80100790 */
#define IME_SERVICE_ERROR_STARTED		(-2146433088)	/* 0x801007C0 */
#define IME_SERVICE_ERROR_CLIENT_EXISTS		(-2146433096)	/* 0x801007B8 */
#define IME_SERVICE_ERROR_CLIENT_PARAM		(-2146433100)	/* 0x801007B4 */
#define IME_SERVICE_ERROR_SESSION		(-2146433101)	/* 0x801007B3 */
#define IME_SERVICE_ERROR_NO_MEMORY		(-2146433104)	/* 0x801007B0 */
#define IME_CONFIG_ERROR_LANGUAGE		(-2146433086)	/* 0x801007C2 */
#define IME_CONFIG_ERROR_KEY			(-2146433085)	/* 0x801007C3 */
#define IME_CONFIG_ERROR_INPUT_TYPE		(-2146433084)	/* 0x801007C4 */

/* the languages whose input type takes the predictive bit (US and UK English) */
#define IME_CONFIG_LANGUAGE_PREDICTIVE		0x40004ULL

/* the settings' keys (PLACEHOLDER names, from their registry names) */
#define IME_CONFIG_KEY_INPUT_TYPE		1	/* "input_type", low 16 bits */
#define IME_CONFIG_KEY_AUTO_CAPITALIZATION	2
#define IME_CONFIG_KEY_PREDICTIVE_TEXT		3
#define IME_CONFIG_KEY_PREDICTIVE		4	/* "input_type", bit 31 */
#define IME_CONFIG_KEY_KB_TYPE			5
#define IME_CONFIG_KEY_KB_ZH_T_INPUT_METHOD	6
#define IME_CONFIG_KEY_KB_JA_INPUT_METHOD	7
#define IME_CONFIG_KEY_KB_KEY_REPEAT_START	8
#define IME_CONFIG_KEY_KB_KEY_REPEAT_INTERVAL	9
#define IME_CONFIG_KEY_LAST_UNIT		0x100
#define IME_CONFIG_KEY_UI_HINT			0x101

/* one table for no language and one per language bit */
#define IME_CONFIG_TABLE_COUNT			64

/* the IME settings as the plugin and the clients see them (PLACEHOLDER) */
class ImeConfigInterface {
public:
	virtual int SetValue(SceUInt64 language, SceUInt32 key, int value) = 0;
	virtual int GetValue(SceUInt64 language, SceUInt32 key, int *value) = 0;
	virtual int GetLastUnit() = 0;
	virtual int SetLastUnit(int unit) = 0;

	virtual ~ImeConfigInterface()
	{
	}
};

/* one language's settings, by key */
typedef paf::map<SceUInt32, int> ImeConfigTable;

/* the IME settings (776 bytes, 0x815448CC) */
class ImeConfig : public ImeConfigInterface {
public:
	ImeConfig();
	virtual int SetValue(SceUInt64 language, SceUInt32 key, int value);
	virtual int GetValue(SceUInt64 language, SceUInt32 key, int *value);
	virtual int GetLastUnit();
	virtual int SetLastUnit(int unit);
	virtual ~ImeConfig();

	static void Create();
	static ImeConfig *GetInstance();

	const char *GetKeyName(SceUInt32 key);
	const char *GetLanguagePath(SceUInt64 language);
	void Load(SceUInt64 language, SceUInt32 key);
	int Save(SceUInt64 language, SceUInt32 key, int value);

	ImeConfigTable m_tables[IME_CONFIG_TABLE_COUNT];
	int m_reserved;
};

/* a setting written back to the registry ("SceImeConfigJobItem", 48 bytes) */
class ImeConfigJob : public paf::job::JobItem {
public:
	ImeConfigJob(SceUInt64 language, SceUInt32 key, int value)
		: paf::job::JobItem("SceImeConfigJobItem", NULL), m_language(language), m_key(key),
		  m_value(value)
	{
	}

	virtual ~ImeConfigJob()
	{
	}

	virtual int Run()
	{
		ImeConfig::GetInstance()->Save(m_language, m_key, m_value);
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	SceUInt64 m_language;
	SceUInt32 m_key;
	int m_value;
};

/* what ExclusiveFuncIme tells the server: ime_plugin loaded and unloaded */
class ImePluginLoadListener {
public:
	virtual ~ImePluginLoadListener()
	{
	}

	virtual void OnPluginLoaded(paf::Plugin *plugin) = 0;
	virtual void OnPluginUnloaded() = 0;
};

/* an event of the open IME for its client (188 bytes) */
struct ImeServiceEvent {
	SceUInt32 id;
	SceUInt8 param[184];
};

/* what ime_plugin tells the server: its events */
class ImePluginEventListener {
public:
	virtual ~ImePluginEventListener()
	{
	}

	virtual void PushEvent(const ImeServiceEvent *event) = 0;
	virtual void ClearEvents() = 0;
};

/* the IME an application opens (40 bytes): the plugin's, with the client's process */
struct ImeServiceOpenParam {
	SceUInt8 param[20];
	SceUID pid;
	SceUInt8 param2[16];
};

/* an asynchronous request's parameters (256 bytes): two values, then what the plugin fills */
struct ImeServiceAsyncParam {
	int arg0;
	int arg1;
	SceUInt8 data[104];
	SceUInt8 data2[144];
};

/* ime_plugin's interface 1: a table of functions */
struct ImePluginInterface {
	int (*IsReady)(void);
	void (*Open)(ImeServiceOpenParam *param);
	void (*Close)(void);
	void (*Update)(ImeServiceOpenParam *param);
	void (*SetCaret)(int value);
	void (*SetText)(int value);
	void (*SetEventListener)(ImePluginEventListener *listener);
	void (*SetVisible)(bool visible);
	int (*IsOpen)(void);
	void (*SetConfig)(ImeConfig *config);
	void (*Abort)(void);
	int (*GetCandidates)(int arg0, int arg1, void *out);
	int (*Request22)(int arg0, int arg1, void *data);
	int (*Request23)(int arg0, int arg1, void *data);
	int (*Request24)(int arg0, int arg1, void *data, void *data2);
	int (*Request25)(void);
	int (*Request26)(void);
};

/* the commands the main thread gives the plugin */
#define IME_COMMAND_OPEN	0
#define IME_COMMAND_CLOSE	1
#define IME_COMMAND_UPDATE	2
#define IME_COMMAND_VISIBLE	3
#define IME_COMMAND_TEXT	4
#define IME_COMMAND_CARET	5

/* a command for the plugin (272 bytes) */
class ImeCommand {
public:
	ImeCommand(int type, SceUID pid, const void *data, SceSize size)
		: m_type(type), m_pid(pid), m_size(size)
	{
		if (data != NULL && size < sizeof(m_data))
			sce_paf_memcpy(m_data.bytes, data, size);
		else
			sce_paf_memset(m_data.bytes, 0, sizeof(m_data));
	}

	virtual ~ImeCommand()
	{
	}

	int m_type;
	SceUID m_pid;
	union {
		SceUInt8 bytes[256];
		ImeServiceOpenParam open;
		int value;
	} m_data;
	SceSize m_size;
};

/* the commands waiting for the main thread (48 bytes) */
class ImeCommandQueue {
public:
	ImeCommandQueue() : m_lock("SceImeCommandQueue", 0)
	{
	}

	virtual ~ImeCommandQueue()
	{
		for (paf::list<ImeCommand *>::iterator it = m_commands.begin();
		     it != m_commands.end(); ++it) {
			if (*it != NULL)
				delete *it;
		}
	}

	paf::list<ImeCommand *> m_commands;
	paf::thread::Mutex m_lock;
};

/* a client application */
class ImeServiceClient {
public:
	ImeServiceClient(SceUID pid, bool active) : m_pid(pid), m_active(active)
	{
	}

	virtual ~ImeServiceClient()
	{
	}

	SceUID m_pid;
	bool m_active;
};

class ImeServer;

/* an asynchronous request run on the job queue ("SceImeAsyncServiceJobItem", 344 bytes) */
class ImeAsyncJob : public paf::job::JobItem {
public:
	ImeAsyncJob(ImeServer *server, IPMI::Session *session, SceUInt32 requestId,
		    SceUInt32 methodId, const void *data, SceSize size, SceUID pid)
		: paf::job::JobItem("SceImeAsyncServiceJobItem", NULL), m_server(server),
		  m_session(session), m_requestId(requestId), m_methodId(methodId), m_size(size),
		  m_pid(pid), m_done(false), m_cancelLock("SceImeAsyncServiceCancelLock", 0)
	{
		sce_paf_memset(m_data, 0, sizeof(m_data));
		if (size < sizeof(m_data))
			sce_paf_memcpy(m_data, data, size);
		else
			m_size = 0;
	}

	virtual ~ImeAsyncJob()
	{
	}

	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	ImeServer *m_server;
	IPMI::Session *m_session;
	SceUInt32 m_requestId;
	SceUInt32 m_methodId;
	SceUInt8 m_data[256];
	SceSize m_size;
	SceUID m_pid;
	bool m_done;
	paf::thread::Mutex m_cancelLock;
};

/* the IME service's server (160 bytes, 0x815448C8) */
class ImeServer : public IPMI::Server::EventHandler, public paf::thread::Thread,
		  public ImePluginLoadListener, public ImePluginEventListener {
public:
	ImeServer();
	virtual ~ImeServer();

	int Init();
	void PushCommand(int type, SceUID pid, const void *data, SceSize size);
	void ActivateClient(SceUID pid);
	void DeactivateClient(SceUID pid);
	void Update();
	int AddClient(SceUID pid);
	void RemoveClient(SceUID pid);
	void RunEvents(SceUInt32 methodId, const void *data, SceSize size, SceUID pid);
	void RunRequest(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			const void *data, SceSize size);

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
					  SceSize inSize, SceUInt32 outCount, void *pOut,
					  SceSize outSize);

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount)
	{
		IPMI::Server::EventHandler::onSyncMethodDispatch(pSession, methodId, pInList,
								 inCount, pOutList, outCount);
	}

	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, void *pIn, SceSize inSize,
					   void *pOut, SceSize outSize);

	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount)
	{
		IPMI::Server::EventHandler::onAsyncMethodDispatch(pSession, requestId, methodId,
								  pInList, inCount);
	}

	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

	virtual void OnPluginLoaded(paf::Plugin *plugin);
	virtual void OnPluginUnloaded();

	virtual void PushEvent(const ImeServiceEvent *event)
	{
		paf::thread::Mutex *lock = &m_eventLock;

		lock->Lock();
		if (m_closedPid != -1 && m_closedPid == m_activePid && event->id == 0)
			m_closedPid = -1;
		else if (m_events.size() < IME_SERVICE_EVENT_MAX)
			m_events.push_back(*event);
		lock->Unlock();
	}

	virtual void ClearEvents()
	{
		paf::thread::Mutex *lock = &m_eventLock;

		lock->Lock();
		m_events.clear();
		lock->Unlock();
	}

	virtual void EntryFunction();

	enum {
		IME_SERVICE_EVENT_MAX = 100
	};

	IPMI::Server *m_server;
	SceSize m_workBufferSize;
	void *m_workBuffer;
	ExclusiveFuncPtr m_func;
	paf::list<ImeServiceClient> m_clients;
	paf::thread::Mutex m_clientLock;
	SceUID m_activePid;		/* the client in front */
	SceUID m_openPid;		/* the client the IME is open for */
	SceUID m_closedPid;		/* the client whose IME was closed by the unload */
	ImeCommandQueue *m_commands;
	paf::list<ImeServiceEvent> m_events;
	paf::thread::Mutex m_eventLock;
	paf::common::SharedPtr<paf::job::JobItem> m_asyncJob;
};

inline void ImeAsyncJob::Cancel()
{
	m_cancelLock.Lock();
	paf::job::JobItem::Cancel();
	m_cancelLock.Unlock();
}

/* the events asked for: the IME opened for the client once the plugin can take it */
inline int ImeAsyncJob::Run()
{
	m_cancelLock.Lock();
	if (!IsCanceled())
		m_server->RunEvents(m_methodId, m_data, m_size, m_pid);
	m_cancelLock.Unlock();
	return 0;
}

/* any other request: given to the plugin and answered */
inline void ImeAsyncJob::Finish(int result)
{
	if (!IsCanceled())
		m_server->RunRequest(m_session, m_requestId, m_methodId, m_data, m_size);
	m_done = true;
}

/*
 * ime_plugin as an exclusive function (24 bytes): the shell's block holds its surface pool and
 * private heap while it is loaded.
 */
class ExclusiveFuncIme : public ExclusiveFunc {
public:
	ExclusiveFuncIme(ImePluginLoadListener *listener, ImeConfig *config);
	virtual ~ExclusiveFuncIme();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();

	ImePluginLoadListener *m_listener;
	paf::graph::SurfacePool *m_surfacePool;
	paf::memory::HeapAllocator *m_heap;
	ImeConfig *m_config;
};

#endif
