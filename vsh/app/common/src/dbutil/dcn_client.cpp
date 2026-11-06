/*
 * dbutil::DCNClient: the process's client of the DCN server, which carries each connection's
 * changes to the connections of the other processes.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel.h>
#include <std/string.h>
#include "connection.h"
#include "dcn_client.h"

namespace dbutil {

/* Sony's build calls these whole */
static __attribute__((noinline)) int PostEvent(IPMI::Client *client, SceUID processId,
					       SceBool local, Connection *source, int type,
					       SceInt64 rowid, const char *database,
					       const char *table);
static __attribute__((noinline)) int GetEvent(IPMI::Client *client, SceUID *processId,
					      SceBool *local, Connection **source, int *type,
					      SceInt64 *rowid, SceSize databaseSize,
					      SceBool *hasDatabase, char *database,
					      SceSize tableSize, SceBool *hasTable, char *table);
static __attribute__((noinline)) int StopClient(IPMI::Client *client);

DCNClient *DCNClient::s_instance;

DCNClient::~DCNClient()
{
}

/* connects to the server and starts the client thread */
__attribute__((noinline)) int DCNClient::Connect(const InitParam *param)
{
	IPMI::Client::Config config;
	int ret;
	int result;

	config.eventFlagCount = 1;
	config.budget = param != NULL ? param->budget : IPMI::USER_BUDGET;
	sce_paf_strncpy(config.serviceName, "SceDbUtilDcn", sizeof(config.serviceName));

	void *memory = sce_paf_malloc(IPMI::Client::getBufferSize(&config));

	if (memory == NULL) {
		ret = DBUTIL_ERROR_NO_MEMORY;
	} else {
		ret = IPMI::Client::create(&m_client, &config, this, memory);
		if (ret == 0) {
			ret = m_client->connect(NULL, 0, &result);
			if (ret == 0) {
				m_thread = new ClientThread(param != NULL ? param->priority :
							    SCE_KERNEL_DEFAULT_PRIORITY_USER,
							    this);
				if (m_thread != NULL) {
					m_thread->Start();
					return ret;
				}
				ret = DBUTIL_ERROR_NO_MEMORY;
			}
		}
	}
	if (m_thread != NULL) {
		m_thread->Cancel();
		m_thread->Join();
		delete m_thread;
		m_thread = NULL;
	}
	if (m_client != NULL) {
		m_client->disconnect();
		m_client->destroy();
		sce_paf_free(m_client);
		m_client = NULL;
	}
	return ret;
}

/* tells the server to stop, then ends the thread and the connection */
__attribute__((noinline)) int DCNClient::Disconnect()
{
	int ret = 0;

	if (m_client == NULL || (ret = StopClient(m_client)) == 0) {
		if (m_thread != NULL) {
			m_thread->Cancel();
			m_thread->Join();
			delete m_thread;
		}
		if (m_client != NULL) {
			m_client->disconnect();
			ret = m_client->destroy();
			sce_paf_free(m_client);
		}
	}
	return ret;
}

/* the first call makes the client; each call counts */
int DCNClient::Initialize(const InitParam *param)
{
	int ret = 0;

	s_lock.Lock();
	if (s_refCount == 0) {
		s_instance = new DCNClient;
		if (s_instance == NULL) {
			s_lock.Unlock();
			return DBUTIL_ERROR_NO_MEMORY;
		}
		ret = s_instance->Connect(param);
		if (ret != 0) {
			delete s_instance;
			s_instance = NULL;
			s_lock.Unlock();
			return ret;
		}
	}
	s_refCount++;
	s_lock.Unlock();
	return ret;
}

/* the last call, or a forced one, ends the client */
int DCNClient::Finalize(bool force)
{
	int ret = 0;

	s_lock.Lock();
	if (s_refCount <= 0) {
		ret = DBUTIL_ERROR_INVALID_STATE;
	} else if (--s_refCount == 0 || force) {
		s_instance->Disconnect();
		delete s_instance;
		s_instance = NULL;
		s_refCount = 0;
	}
	s_lock.Unlock();
	return ret;
}

int DCNClient::Post(const Event *event)
{
	int ret;

	s_lock.Lock();
	if (s_instance == NULL) {
		s_lock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	ret = PostEvent(s_instance->m_client, s_instance->m_processId, event->local, event->source,
			event->type, event->rowid, event->database.c_str(), event->table.c_str());
	s_lock.Unlock();
	return ret;
}

/* hands another process's event to every connection, a local one to its source only */
__attribute__((noinline)) int DCNClient::Broadcast(const Event *event)
{
	int ret = 0;

	s_entryLock.Lock();
	if (s_instance == NULL) {
		s_entryLock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	for (paf::vector<Entry>::iterator it = s_instance->m_entries.begin();
	     it != s_instance->m_entries.end(); ++it) {
		if (!event->local || it->connection == event->source)
			ret = it->connection->Deliver(event);
	}
	s_entryLock.Unlock();
	return ret;
}

int DCNClient::AddConnection(Connection *connection)
{
	s_entryLock.Lock();
	if (s_instance == NULL) {
		s_entryLock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}
	s_instance->m_entries.push_back(Entry(connection));
	s_entryLock.Unlock();
	return 0;
}

int DCNClient::RemoveConnection(Connection *connection)
{
	s_entryLock.Lock();
	if (s_instance == NULL) {
		s_entryLock.Unlock();
		return DBUTIL_ERROR_INVALID_STATE;
	}

	paf::vector<Entry>::iterator end = s_instance->m_entries.end();

	for (paf::vector<Entry>::iterator it = s_instance->m_entries.begin(); it != end; ++it) {
		if (it->type == DCN_ENTRY_CONNECTION && it->connection == connection) {
			s_instance->m_entries.erase(it);
			s_entryLock.Unlock();
			return 0;
		}
	}
	s_entryLock.Unlock();
	return DBUTIL_ERROR_INVALID_ARGUMENT;
}

DCNClient::ClientThread::~ClientThread()
{
}

/* waits on the server's event flag and delivers each event it holds */
void DCNClient::ClientThread::EntryFunction()
{
	SceUInt32 pattern = 0;

	while (!IsCanceled()) {
		m_client->m_client->waitEventFlag(0, DCN_EVENT_PENDING | DCN_EVENT_STOPPED,
						  SCE_KERNEL_EVF_WAITMODE_OR |
						  SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						  &pattern, NULL);
		if (pattern & DCN_EVENT_STOPPED)
			break;
		if (pattern & DCN_EVENT_PENDING) {
			Event event(0);
			SceBool hasDatabase = 0;
			SceBool hasTable = 0;
			char table[128];
			char database[512];

			if (GetEvent(s_instance->m_client, &event.processId, &event.local,
				     &event.source, &event.type, &event.rowid, sizeof(database),
				     &hasDatabase, database, sizeof(table), &hasTable, table) == 0) {
				if (hasDatabase)
					event.database = database;
				if (hasTable)
					event.table = table;
				Broadcast(&event);
			}
		}
	}
}

paf::thread::RMutex DCNClient::s_lock("SceDbUtilDcnClientClt", 0);
paf::thread::RMutex DCNClient::s_entryLock("SceDbUtilDcnClientElist", 0);
int DCNClient::s_refCount;

/* a name's size with its terminator */
static inline SceSize StringSize(const char *str)
{
	SceSize length = 0;

	while (str[length] != '\0')
		length++;
	return length + 1;
}

/* the event's fields, then its database and table names */
static int PostEvent(IPMI::Client *client, SceUID processId, SceBool local, Connection *source,
		     int type, SceInt64 rowid, const char *database, const char *table)
{
	struct {
		SceUID processId;
		SceBool local;
		Connection *source;
		int type;
		SceInt64 rowid;
	} header;
	IPMI::DataInfo in[3];
	int result = 0;
	int ret;

	header.processId = processId;
	header.local = local;
	header.source = source;
	header.type = type;
	header.rowid = rowid;
	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = database;
	in[1].dataSize = StringSize(database);
	in[2].pData = table;
	in[2].dataSize = StringSize(table);
	ret = client->invokeSyncMethod(DCN_METHOD_POST_EVENT, in, 3, &result, NULL, 0);
	if (ret == 0)
		ret = result;
	return ret;
}

/* each field into its own buffer; the names only where their flags say */
static int GetEvent(IPMI::Client *client, SceUID *processId, SceBool *local,
		    Connection **source, int *type, SceInt64 *rowid, SceSize databaseSize,
		    SceBool *hasDatabase, char *database, SceSize tableSize, SceBool *hasTable,
		    char *table)
{
	int result = 0;
	SceSize sizes[2];
	IPMI::DataInfo in;
	IPMI::BufferInfo out[9];
	int ret;

	in.pData = sizes;
	in.dataSize = sizeof(sizes);
	out[0].pBuffer = processId;
	out[0].bufferSize = sizeof(*processId);
	out[1].pBuffer = local;
	out[1].bufferSize = sizeof(*local);
	out[2].pBuffer = source;
	out[2].bufferSize = sizeof(*source);
	out[3].pBuffer = type;
	out[3].bufferSize = sizeof(*type);
	out[4].pBuffer = rowid;
	out[4].bufferSize = sizeof(*rowid);
	sizes[0] = databaseSize;
	out[5].pBuffer = hasDatabase;
	out[5].bufferSize = sizeof(*hasDatabase);
	out[6].pBuffer = database;
	out[6].bufferSize = databaseSize;
	sizes[1] = tableSize;
	out[7].pBuffer = hasTable;
	out[7].bufferSize = sizeof(*hasTable);
	out[8].pBuffer = table;
	out[8].bufferSize = tableSize;
	ret = client->invokeSyncMethod(DCN_METHOD_GET_EVENT, &in, 1, &result, out, 9);
	if (ret == 0)
		ret = result;
	return ret;
}

static int StopClient(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(DCN_METHOD_STOP, (const IPMI::DataInfo *)NULL, 0, &result,
				       NULL, 0);
	if (ret == 0)
		ret = result;
	return ret;
}

}
