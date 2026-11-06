/*
 * dbutil::DCNClient: the process's one client of the DCN server ("SceDbUtilDcn", an IPMI
 * service), which passes every connection's changes to the other processes' connections. It
 * keeps the process's connections, posts their events to the server, and a thread of its own
 * takes the other processes' events from the server and delivers them.
 *
 * Sony's names: DCNClient and its Initialize ("dbutil::DCNClient::Initialize() failed"), the
 * service, thread and mutex names. Every other name is ours.
 */
#ifndef DBUTIL_DCN_CLIENT_H
#define DBUTIL_DCN_CLIENT_H

#include <scetypes.h>
#include <std/vector>
#include <thread/thread.h>
#include <ipmi/client.h>
#include "connection.h"

/* the DCN server's methods */
#define DCN_METHOD_POST_EVENT		0x12340000	/* an event for the other processes */
#define DCN_METHOD_GET_EVENT		0x12340001	/* the next event from another process */
#define DCN_METHOD_STOP			0x12340002	/* no more events for this client */

/* the bits of the client's event flag 0 */
#define DCN_EVENT_PENDING		0x2		/* an event waits on the server */
#define DCN_EVENT_STOPPED		0x4		/* the server stopped the client */

/* DCNClient::Entry::type */
#define DCN_ENTRY_CONNECTION		2

namespace dbutil {

/* DCN_METHOD_POST_EVENT's first input; the database and table names follow it */
struct DCNEventHeader {
	SceUID processId;
	SceBool local;
	Connection *source;
	int type;
	SceInt64 rowid;
};

class DCNClient {
public:
	/* DCNClient::Initialize's parameters */
	struct InitParam {
		int priority;		/* the client thread's */
		int budget;		/* the IPMI client's (IPMI::USER_BUDGET without them) */
	};

	/* what the client delivers events to */
	struct Entry {
		Entry(Connection *connection)
			: type(DCN_ENTRY_CONNECTION), reserved(0), connection(connection)
		{
		}

		int type;		/* DCN_ENTRY_* */
		int reserved;
		Connection *connection;
	};

	/* takes the other processes' events from the server */
	class ClientThread : public paf::thread::Thread {
	public:
		ClientThread(int priority, DCNClient *client)
			: Thread(priority, 0x4000, "SceDbUtilDcnClient", NULL), m_client(client)
		{
		}
		virtual ~ClientThread();
		virtual void EntryFunction();

		DCNClient *m_client;
	};

	DCNClient() : m_thread(NULL), m_client(NULL), m_processId(sceKernelGetProcessId())
	{
	}
	virtual ~DCNClient();

	int Connect(const InitParam *param);
	int Disconnect();

	static int Initialize(const InitParam *param);
	static int Finalize(bool force);
	static int Post(const Event *event);
	static int Broadcast(const Event *event);
	static int AddConnection(Connection *connection);
	static int RemoveConnection(Connection *connection);

	static DCNClient *s_instance;
	static paf::thread::RMutex s_lock;		/* s_instance and s_refCount */
	static paf::thread::RMutex s_entryLock;	/* every instance's m_entries */
	static int s_refCount;				/* Initialize calls not finalized */

	paf::vector<Entry> m_entries;
	ClientThread *m_thread;
	IPMI::Client *m_client;
	SceUID m_processId;
};

}

#endif
