/*
 * dbutil::DCNServer: the DCN server, which queues every client's events for the other clients.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel.h>
#include <std/string.h>
#include "connection.h"
#include "dcn_client.h"
#include "dcn_server.h"

namespace dbutil {

DCNServer *DCNServer::s_instance;

DCNServer::DCNServer() : m_reserved(false), m_server(NULL), m_workBuffer(NULL),
			 m_workBufferSize(0)
{
}

/* drops a session and its queued events; erase also takes it out of the map */
int DCNServer::RemoveSession(IPMI::Session *session, bool erase)
{
	s_lock.Lock();

	SessionMap::iterator it = m_sessions.find(session);

	if (it == m_sessions.end()) {
		s_lock.Unlock();
		return DBUTIL_ERROR_NOT_FOUND;
	}
	session->destroy();
	sce_paf_free(session);
	for (EventList::iterator event = it->second.begin(); event != it->second.end(); ++event)
		delete *event;
	if (erase)
		m_sessions.erase(it);
	s_lock.Unlock();
	return 0;
}

DCNServer::~DCNServer()
{
	for (SessionMap::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
		RemoveSession(it->first, false);
}

/* makes the IPMI server and starts its dispatcher thread */
int DCNServer::Start()
{
	IPMI::Server::Config config;
	int ret;

	m_workBuffer = NULL;
	sce_paf_strncpy(config.serviceName, "SceDbUtilDcn", sizeof(config.serviceName));
	config.pEventHandler = this;
	config.budget = IPMI::USER_BUDGET;

	void *memory = sce_paf_malloc(0x100);

	m_workBufferSize = 0x6000;
	m_workBuffer = sce_paf_memalign(16, 0x6000);
	if (memory == NULL || m_workBuffer == NULL) {
		ret = DBUTIL_ERROR_NO_MEMORY;
	} else {
		ret = IPMI::Server::create(&m_server, &config, this, memory);
		if (ret == 0) {
			m_thread = new ServerThread(this);
			if (m_thread != NULL) {
				m_thread->Start();
				return ret;
			}
			ret = DBUTIL_ERROR_NO_MEMORY;
		}
	}
	sce_paf_free(memory);
	sce_paf_free(m_workBuffer);
	m_workBuffer = NULL;
	return ret;
}

/* stops the dispatcher and destroys the server */
int DCNServer::Stop()
{
	int ret = 0;

	if (m_thread != NULL) {
		m_thread->Cancel();
		if (m_server != NULL)
			ret = m_server->shutdownDispatcher();
		m_thread->Join();
		delete m_thread;
		m_thread = NULL;
	}
	if (m_server != NULL) {
		ret = m_server->destroy();
		sce_paf_free(m_server);
		m_server = NULL;
		sce_paf_free(m_workBuffer);
	}
	return ret;
}

void DCNServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
				     const IPMI::DataInfo *pInList, SceUInt32 inCount,
				     IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	DCNDispatchSyncMethod(pSession, methodId, pInList, inCount, pOutList, outCount);
}

void DCNServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
				      SceUInt32 methodId, const IPMI::DataInfo *pInList,
				      SceUInt32 inCount)
{
	DCNDispatchAsyncMethod(pSession, requestId, methodId, pInList, inCount);
}

/* accepts every client, with an empty queue */
int DCNServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
			   const void *pParam, SceSize paramSize)
{
	IPMI::Session *session = NULL;
	EventList events;
	int ret;

	void *memory = sce_paf_malloc(pConfig->bufferSize);

	if (memory == NULL) {
		ret = DBUTIL_ERROR_NO_MEMORY;
	} else {
		ret = pServer->createSession(&session, this, memory);
		if (ret == 0) {
			s_lock.Lock();
			m_sessions.insert(SessionMap::value_type(session, EventList(events)));
			s_lock.Unlock();
		}
	}
	if (ret != 0)
		sce_paf_free(memory);
	return ret;
}

void DCNServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	RemoveSession(pSession, true);
}

void DCNServer::onSessionKilled(IPMI::Session *pSession)
{
	RemoveSession(pSession, true);
}

/* the first call makes the server; each call counts */
int DCNServer::Initialize()
{
	int ret = 0;

	s_lock.Lock();
	if (s_refCount == 0) {
		s_instance = new DCNServer;
		if (s_instance == NULL) {
			s_lock.Unlock();
			return DBUTIL_ERROR_NO_MEMORY;
		}
		ret = s_instance->Start();
		if (ret != 0) {
			delete s_instance;
			s_lock.Unlock();
			return ret;
		}
	}
	s_refCount++;
	s_lock.Unlock();
	return ret;
}

/* the last call, or a forced one, ends the server */
int DCNServer::Finalize(bool force)
{
	s_lock.Lock();
	if (s_refCount <= 0) {
		s_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	if (--s_refCount == 0 || force) {
		s_instance->Stop();
		delete s_instance;
		s_instance = NULL;
		s_refCount = 0;
	}
	s_lock.Unlock();
	return 0;
}

/*
 * Queues a client's event for every session, or for the posting session alone when it is local,
 * and wakes each session whose queue was empty. A full queue is replaced by one overflow event,
 * and a queue holding only that event takes nothing more until the client reads it.
 */
int DCNServer::PostEvent(IPMI::Session *session, SceUID processId, SceBool local,
			 Connection *source, int type, SceInt64 rowid, const char *database,
			 const char *table)
{
	int ret;

	s_lock.Lock();
	if (s_instance == NULL) {
		s_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}

	SessionMap::iterator it = local ? s_instance->m_sessions.find(session) :
					  s_instance->m_sessions.begin();

	ret = 0;
	for (; it != s_instance->m_sessions.end(); ++it) {
		IPMI::Session *target = it->first;
		EventList *events = &it->second;
		bool wasEmpty = events->size() == 0;

		if (events->size() >= DCN_QUEUE_LIMIT) {
			for (EventList::iterator event = events->begin(); event != events->end();
			     ++event)
				delete *event;
			events->clear();

			Event *overflow = new Event(DBUTIL_EVENT_OVERFLOW);

			if (overflow == NULL) {
				ret = DBUTIL_ERROR_NO_MEMORY;
				break;
			}
			events->push_back(overflow);
		} else if (events->size() == 1 && events->back()->type == DBUTIL_EVENT_OVERFLOW) {
			continue;
		} else {
			Event *event = new Event(processId, local, source, type, database, table,
						 rowid);

			if (event == NULL) {
				ret = DBUTIL_ERROR_NO_MEMORY;
				break;
			}
			events->push_back(event);
		}
		if (wasEmpty)
			ret = target->setEventFlag(0, DCN_EVENT_PENDING);
		if (local)
			break;
	}
	s_lock.Unlock();
	return ret;
}

/* hands a session its oldest event and wakes it again while more are queued */
int DCNServer::GetEvent(IPMI::Session *session, SceUID *processId, SceBool *local,
			Connection **source, int *type, SceInt64 *rowid, int databaseSize,
			SceSize *databaseLength, char *database, int tableSize,
			SceSize *tableLength, char *table)
{
	int ret;

	if (databaseSize > DCN_DATABASE_SIZE_MAX || tableSize > DCN_TABLE_SIZE_MAX)
		return DBUTIL_ERROR_INVALID_ARGUMENT;
	s_lock.Lock();
	sce_paf_memset(database, 0, databaseSize);
	sce_paf_memset(table, 0, tableSize);

	SessionMap::iterator it = s_instance->m_sessions.find(session);

	if (it == s_instance->m_sessions.end() || it->second.size() == 0) {
		s_lock.Unlock();
		return DBUTIL_ERROR_INVALID_TYPE;
	}

	Event *event = it->second.front();

	if (event != NULL) {
		*processId = event->processId;
		*local = event->local;
		*source = event->source;
		*type = event->type;
		*rowid = event->rowid;
		*databaseLength = event->database.size() + 1;
		*tableLength = event->table.size() + 1;
		sce_paf_strncpy(database, event->database.c_str(), databaseSize);
		sce_paf_strncpy(table, event->table.c_str(), tableSize);
		delete event;
	}
	it->second.pop_front();
	ret = 0;
	if (it->second.size() != 0)
		ret = session->setEventFlag(0, DCN_EVENT_PENDING);
	s_lock.Unlock();
	return ret;
}

/* tells the session's client thread to end */
int DCNServer::StopSession(IPMI::Session *session)
{
	return session->setEventFlag(0, DCN_EVENT_STOPPED);
}

DCNServer::ServerThread::~ServerThread()
{
}

void DCNServer::ServerThread::EntryFunction()
{
	IPMI::Server *server = m_server->m_server;

	while (!IsCanceled())
		server->runDispatcher(m_server->m_workBuffer, m_server->m_workBufferSize);
}

paf::thread::RMutex DCNServer::s_lock("SceDbUtilDcnServer", 0);
int DCNServer::s_refCount;

}
