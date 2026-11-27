/*
 * ime_service.cpp: the shell's IME service (0x8113E2E4-0x8113FD2C). Applications connect to the
 * "SceImeService" IPMI server; the one in front gets the IME: its requests become commands the
 * main thread gives ime_plugin, which the server loads as an exclusive function, and the
 * plugin's events wait in a queue until the client asks for them. The kernel object and job item
 * names are Sony's; the file's and the other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <plugin.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <inputdevice/inputdevice.h>
#include <kernel/threadmgr.h>
#include "shell.h"
#include "ime_service.h"

using namespace paf;

/* the server's IPMI service and dispatcher */
#define IME_SERVICE_REQUEST_SIZE	3840
#define IME_SERVICE_SERVER_MEMORY	256
#define IME_SERVICE_THREAD_PRIORITY	0x100000EB
#define IME_SERVICE_THREAD_STACK	4096

/* what a client connects with: its parameter's size */
#define IME_SERVICE_CLIENT_PARAM_SIZE	4

/* the application events the server follows: ended, activated, deactivated and 8 */
#define IME_APP_EVENT_ENDED		0
#define IME_APP_EVENT_ACTIVATED		5
#define IME_APP_EVENT_DEACTIVATED	6
#define IME_APP_EVENT_CLOSED		8

/* the shell events the server follows: the shell itself in front, and away */
#define IME_SHELL_EVENT_ACTIVATED	6
#define IME_SHELL_EVENT_DEACTIVATED	7

/* the input devices whose ownership keeps the IME away from a client */
#define IME_SERVICE_INPUT_DEVICES	0x6F

/* the synchronous methods */
#define IME_METHOD_CLOSE		2
#define IME_METHOD_UPDATE		3
#define IME_METHOD_STATE		5
#define IME_METHOD_CONFIG		20

/* the asynchronous methods: the events (with the IME opened), and the dictionary requests */
#define IME_METHOD_EVENTS		4
#define IME_METHOD_DIC_GET_WORDS	0x21
#define IME_METHOD_DIC_ADD_WORD		0x22
#define IME_METHOD_DIC_DELETE_WORD	0x23
#define IME_METHOD_DIC_REPLACE_WORD	0x24
#define IME_METHOD_DIC_CLEAR_LEARNING	0x25
#define IME_METHOD_DIC_CLEAR_USER	0x26

/* how many events one answer carries, and the dictionary word buffer */
#define IME_SERVICE_EVENTS_MAX		10
#define IME_SERVICE_DIC_WORD_SIZE	104
#define IME_SERVICE_DIC_WORDS_MAX	50

/* the state method's changes */
#define IME_STATE_TEXT			0x1
#define IME_STATE_VISIBLE		0x2
#define IME_STATE_CARET			0x4

/* the state method's input (40 bytes) */
struct ImeServiceStateParam {
	SceUInt32 flags;
	int text;
	int visible;
	int caret;
	SceUInt8 reserved[24];
};

/* the config method's input: set or get one setting */
#define IME_CONFIG_OP_SET		0
#define IME_CONFIG_OP_GET		1

struct ImeServiceConfigParam {
	int op;
	int reserved;
	SceUInt64 language;
	SceUInt32 key;
	int value;
};

/* Sony's stop for what must not happen */
#define IME_SERVICE_ABORT()				\
	do {						\
		sce_paf_printf_dummy();			\
		sceKernelDelayThread(100000);		\
		__builtin_trap();			\
	} while (0)

static ImeServer *s_imeServer;			/* (0x815448C8) */

static void imeServerOnAppEvent(int code, const void *data, SceSize size, void *arg);
static void imeServerOnShellEvent(int code, const void *data, SceSize size, void *arg);
static void imeServerUpdate(void *arg);

/* ime_plugin's interface while it is loaded and ready */
static inline ImePluginInterface *imeServiceGetPlugin(void)
{
	Plugin *plugin = Plugin::Find("ime_plugin");

	if (plugin == NULL)
		return NULL;

	ImePluginInterface *iface = (ImePluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return NULL;
	if (iface->IsReady() == 0)
		return NULL;
	return iface;
}

/* a request done with the session: the session freed with its last request */
static inline void imeServiceReleaseSession(IPMI::Session *session)
{
	int32_t *ref = (int32_t *)session->getUserData();

	if (sceAtomicDecrement32AcqRel(ref) == 1) {
		delete ref;
		session->destroy();
		sce_paf_free(session);
	}
}

/* (0x8113E2E4) the service made, its dispatcher started and ime_plugin's function requested */
int ImeServer::Init()
{
	IPMI::Server::Config config;

	config.maxRequestSize = IME_SERVICE_REQUEST_SIZE;
	config.maxMessageSize = IME_SERVICE_REQUEST_SIZE;
	config.budget = IPMI::USER_BUDGET;
	sce_paf_strncpy(config.serviceName, "SceImeService", sizeof(config.serviceName));
	config.pEventHandler = this;
	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);

	void *memory = sce_paf_malloc(IME_SERVICE_SERVER_MEMORY);

	m_workBuffer = sce_paf_malloc(m_workBufferSize);
	if (memory == NULL || m_workBuffer == NULL) {
		sce_paf_free(memory);
		sce_paf_free(m_workBuffer);
		return IME_SERVICE_ERROR_NO_MEMORY;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, memory);

	if (ret != 0) {
		sce_paf_free(memory);
		sce_paf_free(m_workBuffer);
		return ret;
	}
	ret = Start();
	if (ret < 0)
		return ret;
	m_func = ExclusiveFuncPtr(new ExclusiveFuncIme(this, ImeConfig::GetInstance()));
	ExclusiveFuncMgr::GetInstance()->Request(&m_func);
	return 0;
}

/* (0x8113E4D8) */
ImeServer::ImeServer()
	: paf::thread::Thread(IME_SERVICE_THREAD_PRIORITY, IME_SERVICE_THREAD_STACK,
			      "SceImeServerDispatch", NULL),
	  m_server(NULL), m_workBuffer(NULL), m_clientLock("SceImeClientList", 0), m_activePid(-1),
	  m_openPid(-1), m_closedPid(0), m_commands(NULL), m_eventLock("SceImeEventQueue", 0)
{
	m_commands = new ImeCommandQueue();
	common::MainThreadCallList::Register(imeServerUpdate, this);
	SceShell_83D16CA8(IME_APP_EVENT_ACTIVATED, imeServerOnAppEvent, this);
	SceShell_83D16CA8(IME_APP_EVENT_DEACTIVATED, imeServerOnAppEvent, this);
	SceShell_83D16CA8(IME_APP_EVENT_ENDED, imeServerOnAppEvent, this);
	SceShell_83D16CA8(IME_APP_EVENT_CLOSED, imeServerOnAppEvent, this);
	SceShell_7FE4E732(IME_SHELL_EVENT_DEACTIVATED, imeServerOnShellEvent, this);
	SceShell_7FE4E732(IME_SHELL_EVENT_ACTIVATED, imeServerOnShellEvent, this);
}

/* (0x8113E642) the IME settings read and the service started */
int shellImeServerCreate(void)
{
	if (s_imeServer != NULL)
		return IME_SERVICE_ERROR_STARTED;
	ImeConfig::Create();
	s_imeServer = new ImeServer();
	return s_imeServer->Init();
}

/* (0x8113E680) a command for the main thread; an open for the client already waiting is not
   queued again */
void ImeServer::PushCommand(int type, SceUID pid, const void *data, SceSize size)
{
	if (type == IME_COMMAND_OPEN) {
		ImeCommandQueue *queue = m_commands;

		queue->m_lock.Lock();
		if (queue->m_commands.size() != 0) {
			ImeCommand *last = queue->m_commands.back();

			if (last->m_type == IME_COMMAND_OPEN && pid == last->m_pid) {
				queue->m_lock.Unlock();
				return;
			}
		}
		queue->m_lock.Unlock();
	}

	ImeCommandQueue *queue = m_commands;

	queue->m_lock.Lock();
	queue->m_commands.push_back(new ImeCommand(type, pid, data, size));
	queue->m_lock.Unlock();
}

/* a client in front: its requests are taken */
inline void ImeServer::ActivateClient(SceUID pid)
{
	m_clientLock.Lock();
	for (list<ImeServiceClient>::iterator it = m_clients.begin(); it != m_clients.end();
	     ++it) {
		if (it->m_pid == pid) {
			m_activePid = pid;
			it->m_active = true;
			break;
		}
	}
	m_clientLock.Unlock();
}

/* (0x8113E770) a client away: its IME closed and the plugin's work stopped */
void ImeServer::DeactivateClient(SceUID pid)
{
	m_clientLock.Lock();
	for (list<ImeServiceClient>::iterator it = m_clients.begin(); it != m_clients.end();
	     ++it) {
		if (it->m_pid == pid) {
			it->m_active = false;
			m_activePid = -1;
			PushCommand(IME_COMMAND_CLOSE, -1, NULL, 0);

			Plugin *plugin = Plugin::Find("ime_plugin");

			if (plugin != NULL) {
				ImePluginInterface *iface =
					(ImePluginInterface *)plugin->GetInterface(1);

				if (iface != NULL && iface->IsReady() != 0)
					iface->Abort();
			}
		}
	}
	m_clientLock.Unlock();
}

/* (0x8113E800) */
static void imeServerOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	ImeServer *server = (ImeServer *)arg;

	switch (code) {
	case IME_APP_EVENT_ENDED:
	case IME_APP_EVENT_DEACTIVATED:
	case IME_APP_EVENT_CLOSED:
		server->DeactivateClient(*(const SceUID *)data);
		break;
	case IME_APP_EVENT_ACTIVATED:
		server->ActivateClient(*(const SceUID *)data);
		break;
	default:
		IME_SERVICE_ABORT();
		break;
	}
}

/* (0x8113E870) */
static void imeServerOnShellEvent(int code, const void *data, SceSize size, void *arg)
{
	ImeServer *server = (ImeServer *)arg;

	switch (code) {
	case IME_SHELL_EVENT_ACTIVATED:
		server->ActivateClient(sceKernelGetProcessId());
		break;
	case IME_SHELL_EVENT_DEACTIVATED:
		server->DeactivateClient(sceKernelGetProcessId());
		break;
	default:
		IME_SERVICE_ABORT();
		break;
	}
}

/* (0x8113E8E0, 0x8113EAB0) */
ImeServer::~ImeServer()
{
	SceShell_E3D3A162(IME_APP_EVENT_ACTIVATED, imeServerOnAppEvent, this);
	SceShell_E3D3A162(IME_APP_EVENT_DEACTIVATED, imeServerOnAppEvent, this);
	SceShell_E3D3A162(IME_APP_EVENT_ENDED, imeServerOnAppEvent, this);
	SceShell_E3D3A162(IME_APP_EVENT_CLOSED, imeServerOnAppEvent, this);
	SceShell_D4C7D2B7(IME_SHELL_EVENT_DEACTIVATED, imeServerOnShellEvent, this);
	SceShell_D4C7D2B7(IME_SHELL_EVENT_ACTIVATED, imeServerOnShellEvent, this);
	common::MainThreadCallList::Unregister(imeServerUpdate, this);
	if (m_commands != NULL)
		delete m_commands;
	m_commands = NULL;
}

/*
 * (0x8113EAC2) the main thread: the waiting commands given to the plugin, those of the client in
 * front only; a command the plugin cannot take yet goes to the back of the queue
 */
void ImeServer::Update()
{
	ImePluginInterface *iface = imeServiceGetPlugin();
	ImeCommandQueue *queue = m_commands;
	thread::Mutex *lock = &queue->m_lock;

	lock->Lock();

	list<ImeCommand *>::iterator it = queue->m_commands.begin();
	ImeCommand *cmd;

	while (it != queue->m_commands.end()) {
		cmd = *it;
		SceUID active = m_activePid;
		bool requeue = false;

		if ((cmd->m_pid == -1 || active == cmd->m_pid) &&
		    (!inputdevice::IsOwnershipGotten(IME_SERVICE_INPUT_DEVICES, 0) ||
		     active == sceKernelGetProcessId() || cmd->m_pid == -1)) {
			switch (cmd->m_type) {
			case IME_COMMAND_OPEN: {
				bool open = false;

				m_clientLock.Lock();
				if (m_activePid != -1)
					open = true;
				m_clientLock.Unlock();
				if (!open)
					break;
				if (iface == NULL) {
					lock->Unlock();
					return;
				}
				OnPluginLoaded(NULL);
				cmd->m_data.open.pid = active;
				iface->Open(&cmd->m_data.open);
				m_openPid = active;
				break;
			}
			case IME_COMMAND_CLOSE:
				if (iface != NULL) {
					iface->Close();
					m_openPid = -1;
				}
				break;
			case IME_COMMAND_UPDATE:
				if (iface == NULL || iface->IsOpen() == 0) {
					requeue = true;
					break;
				}
				cmd->m_data.open.pid = active;
				iface->Update(&cmd->m_data.open);
				break;
			case IME_COMMAND_VISIBLE:
				if (iface == NULL) {
					requeue = true;
					break;
				}
				iface->SetVisible(cmd->m_data.value != 0);
				if (iface->IsOpen() == 0)
					requeue = true;
				break;
			case IME_COMMAND_TEXT:
				if (iface == NULL) {
					requeue = true;
					break;
				}
				iface->SetText(cmd->m_data.value);
				break;
			case IME_COMMAND_CARET:
				if (iface == NULL) {
					requeue = true;
					break;
				}
				iface->SetCaret(cmd->m_data.value);
				break;
			default:
				break;
			}
		}
		if (requeue) {
			queue->m_commands.erase(it);
			queue->m_commands.push_back(cmd);
			lock->Unlock();
			return;
		}
		if (cmd != NULL)
			delete cmd;
		it = queue->m_commands.erase(it);
	}
	lock->Unlock();
}

/* (0x8113ECAE) */
static void imeServerUpdate(void *arg)
{
	((ImeServer *)arg)->Update();
}

/* (0x8113ECB6) a client connected: in front from now; 0 when it is known already */
int ImeServer::AddClient(SceUID pid)
{
	m_clientLock.Lock();
	for (list<ImeServiceClient>::iterator it = m_clients.begin(); it != m_clients.end();
	     ++it) {
		if (pid == it->m_pid) {
			m_clientLock.Unlock();
			return 0;
		}
	}

	ImeServiceClient client(pid, true);

	m_clients.push_back(client);
	m_activePid = pid;
	m_clientLock.Unlock();
	return 1;
}

/* (0x8113ED42) a client accepted with its session, which counts its requests */
int ImeServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
			   const void *pParam, SceSize paramSize)
{
	if (AddClient(pConfig->processId) == 0)
		return IME_SERVICE_ERROR_CLIENT_EXISTS;
	if (paramSize != IME_SERVICE_CLIENT_PARAM_SIZE)
		return IME_SERVICE_ERROR_CLIENT_PARAM;

	void *memory = sce_paf_malloc(pConfig->bufferSize);

	if (memory == NULL)
		return IME_SERVICE_ERROR_NO_MEMORY;

	int32_t *ref = new int32_t(1);
	IPMI::Session *session;

	if (pServer->createSession(&session, ref, memory) == (int)0x8002050C) {
		sce_paf_free(memory);
		return IME_SERVICE_ERROR_SESSION;
	}
	return 0;
}

/* (0x8113EDE4) a client gone: the next one still active in front */
void ImeServer::RemoveClient(SceUID pid)
{
	m_clientLock.Lock();
	for (list<ImeServiceClient>::iterator it = m_clients.begin(); it != m_clients.end();
	     ++it) {
		if (pid == it->m_pid) {
			m_clients.erase(it);
			m_activePid = -1;
			break;
		}
	}
	for (list<ImeServiceClient>::iterator it = m_clients.begin(); it != m_clients.end();
	     ++it) {
		if (it->m_active) {
			m_activePid = it->m_pid;
			break;
		}
	}
	m_clientLock.Unlock();
}

/* (0x8113EE6C) */
void ImeServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	SceUID pid = pSession->getProcessId();

	RemoveClient(pid);
	imeServiceReleaseSession(pSession);
	if (m_openPid == pid)
		m_openPid = -1;
}

/* (0x8113EEC6) */
void ImeServer::onSessionKilled(IPMI::Session *pSession)
{
	SceUID pid = pSession->getProcessId();

	RemoveClient(pid);
	imeServiceReleaseSession(pSession);
	if (m_openPid == pid)
		m_openPid = -1;
	PushCommand(IME_COMMAND_CLOSE, -1, NULL, 0);
}

/* (0x8113EF36) */
void ImeServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
				     SceSize inSize, SceUInt32 outCount, void *pOut,
				     SceSize outSize)
{
	SceUID pid = pSession->getProcessId();

	switch (methodId) {
	case IME_METHOD_CLOSE:
		if (m_asyncJob.get() != NULL) {
			m_asyncJob->Cancel();
			m_asyncJob.reset();
		}
		PushCommand(IME_COMMAND_CLOSE, -1, NULL, 0);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	case IME_METHOD_UPDATE:
		PushCommand(IME_COMMAND_UPDATE, pid, pIn, inSize);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	case IME_METHOD_STATE: {
		if (inSize != sizeof(ImeServiceStateParam))
			IME_SERVICE_ABORT();

		const ImeServiceStateParam *param = (const ImeServiceStateParam *)pIn;

		if ((param->flags & IME_STATE_TEXT) && param->text != 0)
			PushCommand(IME_COMMAND_TEXT, pid, &param->text, sizeof(param->text));
		if (param->flags & IME_STATE_CARET)
			PushCommand(IME_COMMAND_CARET, pid, &param->caret, sizeof(param->caret));
		if (param->flags & IME_STATE_VISIBLE)
			PushCommand(IME_COMMAND_VISIBLE, pid, &param->visible,
				    sizeof(param->visible));
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	}
	case IME_METHOD_CONFIG: {
		const ImeServiceConfigParam *param = (const ImeServiceConfigParam *)pIn;
		int value;
		void *out = NULL;
		SceSize size = 0;
		int ret = -1;
		ImeConfig *config = ImeConfig::GetInstance();

		if (param->op == IME_CONFIG_OP_SET) {
			ret = config->SetValue(param->language, param->key, param->value);
		} else if (param->op == IME_CONFIG_OP_GET) {
			out = &value;
			ret = config->GetValue(param->language, param->key, &value);
			size = sizeof(value);
		}
		pSession->respondToSyncMethodRequest(ret, out, size);
		break;
	}
	default:
		break;
	}
}

/*
 * (0x8113F138) the client in front only: its events answered at once and the IME opened for it,
 * any other request run on the job queue
 */
void ImeServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
				      SceUInt32 methodId, void *pIn, SceSize inSize, void *pOut,
				      SceSize outSize)
{
	sceAtomicIncrement32AcqRel((int32_t *)pSession->getUserData());
	if (m_activePid != pSession->getProcessId()) {
		pSession->respondToAsyncMethodRequest(requestId, 0, NULL, 0);
		imeServiceReleaseSession(pSession);
		return;
	}

	common::SharedPtr<job::JobItem> job;

	if (methodId == IME_METHOD_EVENTS) {
		ImeServiceEvent events[IME_SERVICE_EVENTS_MAX];

		m_eventLock.Lock();

		SceUInt32 count = m_events.size() < IME_SERVICE_EVENTS_MAX ? m_events.size() :
			IME_SERVICE_EVENTS_MAX;

		for (SceUInt32 i = 0; i < count; i++) {
			events[i] = m_events.front();
			m_events.pop_front();
		}
		pSession->respondToAsyncMethodRequest(requestId, count, events,
						      count * sizeof(ImeServiceEvent));

		SceUID pid = pSession->getProcessId();

		imeServiceReleaseSession(pSession);
		if (m_asyncJob.get() != NULL && !((ImeAsyncJob *)m_asyncJob.get())->m_done) {
			m_eventLock.Unlock();
			return;
		}
		job = common::SharedPtr<job::JobItem>(new ImeAsyncJob(this, NULL, 0, methodId, pIn,
								      inSize, pid));
		m_eventLock.Unlock();
	} else {
		job = common::SharedPtr<job::JobItem>(new ImeAsyncJob(this, pSession, requestId,
								      methodId, pIn, inSize, -1));
	}
	m_asyncJob = job;
	job::JobQueue::default_queue->Enqueue(job);
}

/* (0x8113F7AC) the plugin loaded: given the settings and the server for its events */
void ImeServer::OnPluginLoaded(Plugin *plugin)
{
	ImePluginInterface *iface = imeServiceGetPlugin();

	iface->SetConfig(ImeConfig::GetInstance());
	iface->SetEventListener(this);
}

/* (0x8113F7F0) the plugin unloaded: the commands for it dropped */
void ImeServer::OnPluginUnloaded()
{
	ImeCommandQueue *queue = m_commands;

	if (m_activePid == m_openPid && m_openPid != -1)
		m_closedPid = m_openPid;
	queue->m_lock.Lock();
	queue->m_commands.clear();
	queue->m_lock.Unlock();
}

/* (0x8113F83C) the client asks for events: the IME opened for it unless it is open already */
void ImeServer::RunEvents(SceUInt32 methodId, const void *data, SceSize size, SceUID pid)
{
	if (methodId != IME_METHOD_EVENTS)
		return;

	ImePluginInterface *iface = imeServiceGetPlugin();

	if (iface == NULL || iface->IsOpen() == 0) {
		PushCommand(IME_COMMAND_OPEN, pid, data, sizeof(ImeServiceOpenParam));
		return;
	}
	if (m_openPid != -1 && m_openPid != pid && pid == sceKernelGetProcessId()) {
		PushCommand(IME_COMMAND_CLOSE, -1, NULL, 0);
		PushCommand(IME_COMMAND_OPEN, pid, data, sizeof(ImeServiceOpenParam));
	}
}

/*
 * (0x8113F8DE) a request given to the plugin and answered; while the plugin is away or commands
 * wait, it goes back to the job queue
 */
void ImeServer::RunRequest(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			   const void *data, SceSize size)
{
	ImePluginInterface *iface = imeServiceGetPlugin();

	if (methodId == IME_METHOD_EVENTS)
		return;

	bool idle = false;

	if (iface != NULL) {
		ImeCommandQueue *queue = m_commands;

		queue->m_lock.Lock();
		idle = queue->m_commands.size() == 0;
		queue->m_lock.Unlock();
	}
	if (!idle) {
		common::SharedPtr<job::JobItem> job(new ImeAsyncJob(this, session, requestId,
								    methodId, data, size, -1));

		m_asyncJob = job;
		job::JobQueue::default_queue->Enqueue(job);
		return;
	}

	const ImeServiceAsyncParam *param = (const ImeServiceAsyncParam *)data;
	SceUInt32 languageLow = param->languageLow;
	SceUInt32 languageHigh = param->languageHigh;
	int ret;

	switch (methodId) {
	case IME_METHOD_DIC_GET_WORDS: {
		SceUInt8 words[IME_SERVICE_DIC_WORDS_MAX * IME_SERVICE_DIC_WORD_SIZE];

		ret = iface->GetDicWords(languageLow, languageHigh, words);
		if (ret < 0)
			session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		else
			session->respondToAsyncMethodRequest(requestId, ret, words,
							     ret * IME_SERVICE_DIC_WORD_SIZE);
		break;
	}
	case IME_METHOD_DIC_ADD_WORD:
		ret = iface->AddDicWord(languageLow, languageHigh, (void *)param->word);
		session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		break;
	case IME_METHOD_DIC_DELETE_WORD:
		ret = iface->DeleteDicWord(languageLow, languageHigh, (void *)param->word);
		session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		break;
	case IME_METHOD_DIC_REPLACE_WORD:
		ret = iface->ReplaceDicWord(languageLow, languageHigh, (void *)param->word,
					    (void *)param->newWord);
		session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		break;
	case IME_METHOD_DIC_CLEAR_LEARNING:
		ret = iface->ClearLearningDic();
		session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		break;
	case IME_METHOD_DIC_CLEAR_USER:
		ret = iface->ClearUserDic();
		session->respondToAsyncMethodRequest(requestId, ret, NULL, 0);
		break;
	default:
		IME_SERVICE_ABORT();
		break;
	}
	imeServiceReleaseSession(session);
}

/* (0x8113FC88) the dispatcher thread */
void ImeServer::EntryFunction()
{
	if (m_server->runDispatcher(m_workBuffer, m_workBufferSize) != 0)
		IME_SERVICE_ABORT();
}
