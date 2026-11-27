/*
 * system_update_svc.cpp: the shell's system update service (0x8112CFD8-0x8112F2D0). It keeps
 * the update's download task, answers the "SceSystemUpdate" IPMI service's clients (check,
 * resume, cancel, start) and queues the jobs that fetch the update list and the update. The
 * download plugin makes it (Bgdl_8112D38C), starts it (Bgdl_8112D508) and stops it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/localfile.h>
#include <common/encoded_string.h>
#include <versatileparam/idparam.h>
#include <kernel/iofilemgr.h>
#include <job/job.h>
#include <plugin.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <regmgr/regmgr.h>

#include "src/livespace_db/livespace_db_internal.h"
#include "src/trigger_mgr/trigger_mgr.h"
#include "download_plugin.h"
#include "system_update_svc.h"

using namespace paf;

#define SYSTEM_UPDATE_SETTING		"SHELL0001"
#define SYSTEM_UPDATE_TASK_KIND		2	/* the bgdl library's kind of a system update */
#define SYSTEM_UPDATE_TASKS_MAX		32

SystemUpdateSvc *g_systemUpdateSvc;			/* (0x81544898) */

extern int liveareaCronEnqueueJob(common::SharedPtr<job::JobItem> &job);

/* the update's check given back: its job queued */
int SystemUpdateSvc::Bgdl_8112CFD8(SystemUpdateResult result)
{
	SystemUpdateInfo info;

	if (result.error == 0 && m_url.length() != 0 && m_size > 0 && m_lastModified.length() <= 32) {
		SceInt64 offset = 0;

		result = m_ctrl->Bgdl_81131A18(m_version, m_url, m_lastModified, offset, m_size, 1, &info);
		if (result.error == 0 && !m_busy) {
			SystemUpdateInfo jobInfo = info;
			common::SharedPtr<job::JobItem> job(new SystemUpdateSvcJob(this, jobInfo));

			job::JobQueue::default_queue->Enqueue(job);
			return 1;
		}
	}
	m_checking = false;
	return 0;
}

/* the service made */
bool SystemUpdateSvc::Bgdl_8112D38C(void)
{
	if (g_systemUpdateSvc != NULL)
		return false;
	g_systemUpdateSvc = new SystemUpdateSvc();
	return g_systemUpdateSvc != NULL;
}

/* and destroyed */
void SystemUpdateSvc::Bgdl_8112D42A(void)
{
	if (g_systemUpdateSvc != NULL) {
		delete g_systemUpdateSvc;
		g_systemUpdateSvc = NULL;
	}
}

/* the IPMI service started */
SystemUpdateResult SystemUpdateSvc::Bgdl_8112D450(void)
{
	SystemUpdateResult result;

	m_server = NULL;
	m_server = new SystemUpdateIpmiServer();
	if (m_server == NULL) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = 0;
		return result;
	}
	if (m_server->Init(this) != 0) {
		result.error = SYSTEM_UPDATE_ERROR;
	} else {
		m_server->SetHandler(SYSTEM_UPDATE_METHOD_CANCEL, Bgdl_8112EE04);
		m_server->SetHandler(SYSTEM_UPDATE_METHOD_RESUME, Bgdl_8112EE58);
		m_server->SetHandler(SYSTEM_UPDATE_METHOD_START, Bgdl_8112EE72);
		m_server->SetHandler(SYSTEM_UPDATE_METHOD_CHECK, Bgdl_8112EE7C);
		result.error = 0;
	}
	result.detail = 0;
	return result;
}

/* the service started for the plugin */
int SystemUpdateSvc::Bgdl_8112D508(Plugin *plugin, DownloadPluginObject *object)
{
	m_plugin = plugin;
	m_hasTask = false;
	m_started = false;
	m_busy = false;
	m_checking = false;
	m_unusedFlag = false;
	m_job = NULL;
	m_taskId = 0;
	m_object = object;
	m_ctrl = NULL;

	m_infoPath.assign(Bgdl_8137C40C(Bgdl_8137C448()));
	m_infoPath += "/";
	m_infoPath += "PupInfo";

	m_registLock = new thread::RMutex("SceDonwloadUpdateRegistTask", 0);
	m_seqLock = new thread::Mutex("SceDownloadUpdateSvcSeq", 0);
	m_ctrl = new SystemUpdateCtrl(0, m_infoPath.c_str(), Bgdl_8137C484(0));
	m_state = 2;
	m_autoUpdate = 0;

	shellTriggerMgrRegisterService(SYSTEM_UPDATE_SETTING);

	int value = 0;
	bool enabled = sceRegMgrGetKeyInt("/CONFIG/NP2/AUTOUPDATE", "system", &value) == 0 &&
		       value == 1;
	bool current = false;

	shellTriggerMgrIsServiceEnabled(SYSTEM_UPDATE_SETTING, &current);
	if (enabled != current)
		shellTriggerMgrSetServiceEnabled(SYSTEM_UPDATE_SETTING, enabled);
	shellTriggerMgrSetServiceCallback(SYSTEM_UPDATE_SETTING, Bgdl_8112ED3C, this);
	m_object->Bgdl_8112F2C4(2, Bgdl_8112ED58, this);

	int count = 0;
	BgdlTaskId ids[SYSTEM_UPDATE_TASKS_MAX];

	Bgdl_81139392(0, SYSTEM_UPDATE_TASK_KIND, SYSTEM_UPDATE_TASKS_MAX, &count, ids);
	if (count != 0) {
		if (count != 1)
			return 1;
		m_taskId = ids[0].id;

		BgdlTaskDetail detail;

		sce_paf_memset(&detail, 0, sizeof(detail));
		if (Bgdl_8113942C(0, 1, m_taskId, &detail) != 0)
			return 1;
		m_hasTask = true;
		if (m_ctrl->Bgdl_81131B54() != true || m_ctrl->Bgdl_81131ABC() != true)
			m_ctrl->Bgdl_811310CA();
	}
	Bgdl_8112D450();
	Bgdl_81132986(false);
	Bgdl_81132992();
	Bgdl_811329A2(NULL);
	m_started = true;
	return 1;
}

/* the service stopped */
int SystemUpdateSvc::Bgdl_8112D7F0(void)
{
	shellTriggerMgrClearServiceCallback(SYSTEM_UPDATE_SETTING);
	if (m_registLock != NULL) {
		delete m_registLock;
		m_registLock = NULL;
	}
	if (m_seqLock != NULL) {
		delete m_seqLock;
		m_seqLock = NULL;
	}
	if (m_ctrl != NULL) {
		delete m_ctrl;
		m_ctrl = NULL;
	}
	if (m_object != NULL)
		m_object->Bgdl_8112F270(2, Bgdl_8112ED58);
	if (m_server != NULL) {
		int ret;

		do {
			ret = m_server->Stop();
			thread::Sleep(0);
		} while (ret == -2);
		delete m_server;
		m_server = NULL;
	}
	m_started = false;
	return 1;
}

/* the update's task given up: its log status cleared */
SystemUpdateResult SystemUpdateSvc::Bgdl_8112D88C(SceUInt32 id, BgdlTaskState *state)
{
	SystemUpdateResult result;
	int error = 0;

	m_registLock->Lock();

	LsdbNewEventKey key;
	int ret = Bgdl_8113906E(0, id, state);
	char buf[16] = "";

	sce_paf_snprintf(buf, 15, "%u", id);
	key.itemId = buf;
	key.titleId = "LOGSTATUS0";
	SceLsdb_AE04DDEB(&key, 1);
	m_hasTask = false;
	m_registLock->Unlock();
	if (ret < 0)
		error = SYSTEM_UPDATE_ERROR;
	result.error = error;
	result.detail = 0;
	return result;
}

/* the update's task, when there is one, given up */
SystemUpdateResult SystemUpdateSvc::Bgdl_8112D9CA(void)
{
	SystemUpdateResult result;
	int error = 0;
	int reserved = 0;
	int count = 0;
	BgdlTaskId ids[SYSTEM_UPDATE_TASKS_MAX];
	BgdlTaskState state;
	BgdlTaskDetail detail;

	Bgdl_81139392(0, SYSTEM_UPDATE_TASK_KIND, SYSTEM_UPDATE_TASKS_MAX, &count, ids);
	if (count == 1) {
		SceUInt32 id = ids[0].id;

		sce_paf_memset(&detail, 0, sizeof(detail));
		if (Bgdl_8113942C(0, 1, id, &detail) == 0) {
			SystemUpdateResult ret = Bgdl_8112D88C(id, &state);

			error = ret.error;
			reserved = ret.detail;
		}
	}
	result.error = error;
	result.detail = reserved;
	return result;
}

/* the update's local files removed */
void SystemUpdateSvc::Bgdl_8112DA56(void)
{
	m_seqLock->Lock();
	m_ctrl->Bgdl_811310CA();
	m_seqLock->Unlock();
}

/* the update's download task registered, its file made first when it starts over */
SystemUpdateResult SystemUpdateSvc::Bgdl_8112DA94(SystemUpdateInfo *info)
{
	string reserved("");
	SystemUpdateResult result;
	common::SharedPtr<File> file;
	int error = 0;
	string path;

	m_registLock->Lock();
	if (m_busy) {
		m_registLock->Unlock();
		result.error = 0;
		result.detail = 0;
		return result;
	}
	result = Bgdl_8112D9CA();
	if (result.error != 0) {
		m_registLock->Unlock();
		return result;
	}
	if (info->offset == 0) {
		result = m_ctrl->Bgdl_811310CA();
		if (result.error != 0) {
			int ret;

			for (int i = 0; i < 10; i++) {
				ret = Bgdl_81130EC4("ud0:PSP2UPDATE/PSP2UPDAT.PUP");
				if (ret == 0)
					break;
				thread::Sleep(100);
			}
			if (ret == 1)
				m_ctrl->Bgdl_811310CA();
		}
	}
	sce_paf_memset(&m_detail, 0, sizeof(m_detail));

	common::EncodedString title;

	ScePafMisc_DCBA2BD4(&title, m_plugin->GetString(IDParam("", 0x7F8F9DBC)), 2);
	sce_paf_strncat(m_detail.title, ScePafMisc_A7C6AF5C(&title), sizeof(m_detail.title) - 1);
	sce_paf_strncat(m_detail.path, info->url, sizeof(m_detail.path) - 1);
	sce_paf_strncat(m_detail.fileName, "PSP2UPDAT.PUP", sizeof(m_detail.fileName) - 1);
	sce_paf_strncat(m_detail.dir, "ud0:PSP2UPDATE/", sizeof(m_detail.dir) - 1);
	sce_paf_strncat(m_detail.type, "SETTINGS", sizeof(m_detail.type) - 1);
	do {
		if (info->offset == 0) {
			result = m_ctrl->Bgdl_81131280(*info);
			if (result.error != 0)
				break;
			path = m_detail.dir;
			path += m_detail.fileName;
			file = LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &error);
			if (file.get() == NULL || error != 0) {
				result.error = SYSTEM_UPDATE_ERROR;
				result.detail = error;
				break;
			}
			file->Close();
		}

		SceInt64 times;		/* never set: Sony's code stores whatever its register holds */

		m_detail.offset = info->offset;
		m_detail.size = info->size;
		m_detail.times[0] = times;
		m_detail.times[1] = times;
		m_detail.times[2] = times;
		m_detail.times[3] = times;
		m_detail.times[4] = times;
		m_detail.times[5] = times;
		m_detail.times[6] = times;
		m_detail.times[7] = times;
		if (Bgdl_81138F80(0, SYSTEM_UPDATE_TASK_KIND, &m_detail, &m_taskId) != 0) {
			result.error = SYSTEM_UPDATE_ERROR;
			break;
		}
		m_hasTask = true;
		m_state = 1;
		if (info->offset != 0) {
			SystemUpdateInfo current;

			result = m_ctrl->Bgdl_81131CE6(0, &current, 0);
		}
	} while (0);
	m_registLock->Unlock();
	ScePafMisc_B45A7521(&title);
	return result;
}

/* the update's job run: the update checked or fetched */
SystemUpdateResult SystemUpdateSvc::Bgdl_8112E7F8(SystemUpdateInfo *info)
{
	SystemUpdateResult result;
	bool have = false;
	SceUInt32 id;

	m_seqLock->Lock();

	SystemUpdateStatus status;
	SystemUpdateResult ret = m_ctrl->Bgdl_81132734(&status);

	bool shouldFetch = true;
	bool resetOffset = true;

	if (ret.error == 0) {
		if (status.state == 2) {
			bool ok = false;

			result = m_ctrl->Bgdl_8113276C(string(info->url), string(info->lastModified),
						       info->size, &ok);
			if (result.error == 0 && ok == true)
				shouldFetch = false;
		} else {
			int error = 0;
			int count = 0;
			BgdlTaskId ids[SYSTEM_UPDATE_TASKS_MAX];

			Bgdl_81139392(0, SYSTEM_UPDATE_TASK_KIND, SYSTEM_UPDATE_TASKS_MAX, &count, ids);
			if (count != 0) {
				if (count == 1) {
					BgdlTaskDetail detail;

					id = ids[0].id;
					have = true;
					sce_paf_memset(&detail, 0, sizeof(detail));
					if (Bgdl_8113942C(0, 1, id, &detail) != 0)
						error = SYSTEM_UPDATE_ERROR;
				} else {
					error = SYSTEM_UPDATE_ERROR;
				}
			} else {
				have = false;
				error = 0;
			}
			result.error = error;
			result.detail = 0;
			if (error != 0) {
				shouldFetch = false;
			} else if (have == true) {
				bool ok = false;

				result = m_ctrl->Bgdl_8113276C(string(info->url), string(info->lastModified),
							       info->size, &ok);
				if (result.error == 0 && ok == true) {
					Bgdl_81139190(0, id);
					shouldFetch = false;
				}
			} else if (status.state == 0) {
				bool ok = false;
				SystemUpdateResult check = m_ctrl->Bgdl_8113276C(string(info->url),
										 string(info->lastModified),
										 info->size, &ok);

				if (check.error == 0 && ok == true) {
					info->offset = status.value;
					resetOffset = false;
				}
			}
		}
	}
	if (shouldFetch) {
		if (resetOffset)
			info->offset = 0;
		result = Bgdl_8112DA94(info);
	}
	m_checking = false;
	m_seqLock->Unlock();
	return result;
}

/* the update list's job queued, when the automatic update is on */
int SystemUpdateSvc::Bgdl_8112EBB0(void)
{
	if (m_started == true && m_busy != true) {
		int value = 0;

		if (sceRegMgrGetKeyInt("/CONFIG/NP2/AUTOUPDATE", "system", &value) == 0 &&
		    value == 1) {
			m_updateList.list.clear();

			SystemUpdateResult result = Bgdl_81132B02(1, &value, true);

			if (result.error == 0 && m_checking != true) {
				m_updateList.list.clear();
				m_version.erase(0, string::npos);
				m_url.erase(0, string::npos);
				m_size = 0;
				m_lastModified.erase(0, string::npos);
				m_job = new SystemUpdateListJob(this, &m_version, &m_url, &m_size,
								&m_lastModified, &m_updateList, 0);
				if (m_job != NULL) {
					common::SharedPtr<job::JobItem> job(m_job);

					liveareaCronEnqueueJob(job);
				}
				m_checking = true;
			}
		}
	}
	return 0;
}

/* the SHELL0001 setting changed: the automatic update's registry key follows it */
void Bgdl_8112ED3C(bool value, void *arg)
{
	if (arg != NULL)
		sceRegMgrSetKeyInt("/CONFIG/NP2/AUTOUPDATE", "system", value);
}

/* a download plugin event of the update's task */
void Bgdl_8112ED58(int kind, SceUInt32 id, SceUInt64 rowid, void *arg)
{
	SystemUpdateSvc *svc = (SystemUpdateSvc *)arg;

	if (svc != NULL) {
		svc->m_seqLock->Lock();
		if (kind == 0) {
			svc->m_autoUpdate = 1;
		} else if (kind == 1) {
		} else if (kind == 2) {
			svc->m_autoUpdate = 2;

			SystemUpdateInfo info;
			SystemUpdateResult result = svc->m_ctrl->Bgdl_81131CE6(2, &info, 0);

			if (result.error != 0)
				svc->m_ctrl->Bgdl_811310CA();
			svc->m_state = 0;
		}
		svc->m_seqLock->Unlock();
	}
}

/* the IPMI service's 1003: the update cancelled */
void Bgdl_8112EE04(int reserved, void *arg, const void *data, SceSize size)
{
	SystemUpdateSvc *svc = (SystemUpdateSvc *)arg;

	if (svc != NULL && svc->m_started == true) {
		svc->m_seqLock->Lock();
		svc->m_busy = true;
		svc->m_state = 2;
		svc->m_autoUpdate = 0;
		svc->Bgdl_8112D9CA();
		svc->m_seqLock->Unlock();
	}
}

/* 1002: the update resumed, its list fetched again when asked */
void Bgdl_8112EE58(int reserved, void *arg, const void *data, SceSize size)
{
	SystemUpdateSvc *svc = (SystemUpdateSvc *)arg;

	if (svc != NULL && data != NULL) {
		svc->m_busy = false;
		if (*(const bool *)data == true)
			svc->Bgdl_8112EBB0();
	}
}

/* 1004: the update list fetched */
void Bgdl_8112EE72(int reserved, void *arg, const void *data, SceSize size)
{
	((SystemUpdateSvc *)arg)->Bgdl_8112EBB0();
}

/* 1001: a client's update checked */
void Bgdl_8112EE7C(int reserved, void *arg, const void *data, SceSize size)
{
	SystemUpdateSvc *svc = (SystemUpdateSvc *)arg;

	if (svc != NULL) {
		string id((const char *)data, size);
		const char *str = id.c_str();

		svc->Bgdl_8112D9CA();
		Bgdl_81130ECC(str);
	}
}

/* the service made and its dispatcher started */
int SystemUpdateIpmiServer::Init(void *arg)
{
	if (m_server != NULL)
		return -2;
	m_arg = arg;
	m_onCancel = NULL;
	m_onResume = NULL;
	m_onStart = NULL;
	m_onCheck = NULL;
	sce_paf_memset(m_sessions, 0, sizeof(m_sessions));
	m_sessionCount = 0;

	IPMI::Server::Config config;

	sce_paf_strncpy(config.serviceName, "SceSystemUpdate", sizeof(config.serviceName));
	config.pEventHandler = this;
	m_serverMemory = new SceUInt8[256];
	if (m_serverMemory == NULL)
		return -22;
	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);
	m_workBuffer = new SceUInt8[m_workBufferSize];
	if (m_workBuffer == NULL) {
		delete[] m_serverMemory;
		return -22;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, m_serverMemory);

	if (ret != 0) {
		delete[] m_serverMemory;
		delete[] m_workBuffer;
		return ret;
	}
	m_stopped = false;
	m_thread = new SystemUpdateServerThread(NULL);
	if (m_thread == NULL) {
		m_server->destroy();
		delete[] m_serverMemory;
		delete[] m_workBuffer;
		return -99;
	}
	m_thread->Start(Bgdl_8112F1F8, this);
	return 0;
}

/* the dispatcher stopped and the service destroyed, once no client is left */
int SystemUpdateIpmiServer::Stop(void)
{
	if (m_server == NULL)
		return -4;
	if (m_sessionCount != 0)
		return -2;
	if (m_thread != NULL) {
		m_thread->Join();
		delete m_thread;
		m_thread = NULL;
	}

	int ret = m_server->destroy();

	if (ret != 0)
		return ret;
	m_server = NULL;
	delete[] m_serverMemory;
	m_serverMemory = NULL;
	delete[] m_workBuffer;
	m_workBuffer = NULL;
	return 0;
}

int SystemUpdateIpmiServer::SetHandler(int method, SystemUpdateHandler handler)
{
	if (handler == NULL)
		return -99;
	if (method == SYSTEM_UPDATE_METHOD_CANCEL)
		m_onCancel = handler;
	else if (method == SYSTEM_UPDATE_METHOD_RESUME)
		m_onResume = handler;
	else if (method == SYSTEM_UPDATE_METHOD_START)
		m_onStart = handler;
	else if (method == SYSTEM_UPDATE_METHOD_CHECK)
		m_onCheck = handler;
	else
		return -99;
	return 0;
}

void SystemUpdateIpmiServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
						  void *pIn, SceSize inSize, SceUInt32 outCount,
						  void *pOut, SceSize outSize)
{
	switch (methodId) {
	case SYSTEM_UPDATE_METHOD_CHECK:
		if (m_onCheck != NULL)
			m_onCheck(1, m_arg, pIn, inSize);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	case SYSTEM_UPDATE_METHOD_RESUME:
		if (m_onResume != NULL)
			m_onResume(1, m_arg, pIn, inSize);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	case SYSTEM_UPDATE_METHOD_CANCEL:
		if (m_onCancel != NULL)
			m_onCancel(1, m_arg, pIn, inSize);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	case SYSTEM_UPDATE_METHOD_START:
		if (m_onStart != NULL)
			m_onStart(1, m_arg, pIn, inSize);
		pSession->respondToSyncMethodRequest(0, (const void *)NULL, 0);
		break;
	}
}

int SystemUpdateIpmiServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
					const void *pParam, SceSize paramSize)
{
	if (paramSize != 256)
		return -1;
	if (m_sessionCount == 8)
		return -21;
	m_sessionCount++;

	IPMI::Session **slot = NULL;

	for (SceUInt32 i = 0; i < 8; i++) {
		if (m_sessions[i] == NULL) {
			slot = &m_sessions[i];
			break;
		}
	}

	SceUInt8 *memory = new SceUInt8[pConfig->bufferSize];

	if (memory == NULL) {
		m_sessionCount--;
		return -22;
	}
	if (pServer->createSession(slot, this, memory) == (int)0x8002050C) {
		delete[] memory;
		return -3;
	}
	return 0;
}

void SystemUpdateIpmiServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	IPMI::Session **slot = NULL;

	for (SceUInt32 i = 0; i < 8; i++) {
		if (m_sessions[i] == pSession) {
			slot = &m_sessions[i];
			break;
		}
	}
	pSession->destroy();
	delete[] (SceUInt8 *)pSession;
	*slot = NULL;
	m_sessionCount--;
}

void SystemUpdateIpmiServer::onSessionKilled(IPMI::Session *pSession)
{
	onSessionClosedByClient(pSession);
}

/* the dispatcher thread's function */
int Bgdl_8112F1F8(void *arg)
{
	SystemUpdateIpmiServer *server = (SystemUpdateIpmiServer *)arg;

	server->m_server->runDispatcher(server->m_workBuffer, server->m_workBufferSize);
	return 0;
}

SystemUpdateServerThread::SystemUpdateServerThread(const thread::Thread::Option *option)
	: thread::Thread(0x10000100, 0x4000, "SceSystemUpdate", NULL), m_arg(NULL), m_func(NULL),
	  m_running(false)
{
}

bool SystemUpdateServerThread::Start(int (*func)(void *), void *arg)
{
	if (m_running)
		return false;
	m_arg = arg;
	m_func = func;
	m_running = true;
	thread::Thread::Start();
	return true;
}

void SystemUpdateServerThread::EntryFunction()
{
	if (m_func != NULL)
		m_func(m_arg);
}

/* the download plugin's object cleared */
void DownloadPluginObject::Bgdl_8112F270(void)
{
	sce_paf_memset(this, 0, sizeof(*this));
}

/* its callback removed: the same code */
void DownloadPluginObject::Bgdl_8112F270(int type, DownloadPluginCallback func)
{
	sce_paf_memset(this, 0, sizeof(*this));
}

int DownloadPluginObject::Bgdl_8112F28C(void)
{
	sce_paf_memset(this, 0, sizeof(*this));
	return 0;
}

/* a download's event given to the callback set for its type */
void DownloadPluginObject::Bgdl_8112F29A(int type, int kind, SceUInt32 id, SceUInt64 rowid)
{
	if (m_func != NULL && type == m_type)
		m_func(kind, id, rowid, m_arg);
}

/* the callback set for a type of event */
int DownloadPluginObject::Bgdl_8112F2C4(int type, DownloadPluginCallback func, void *arg)
{
	if (func != NULL) {
		m_type = type;
		m_func = func;
		m_arg = arg;
	}
	return 0;
}
