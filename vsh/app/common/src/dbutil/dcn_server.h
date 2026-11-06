/*
 * dbutil::DCNServer: the DCN server ("SceDbUtilDcn", an IPMI service), run in one process for
 * the system. It keeps a queue of events for each client session, fills every queue with the
 * events one client posts and wakes each client with its event flag.
 *
 * Sony's names: DCNServer (its "dbutil::DCNServer" messages in the shell), the service, thread
 * and mutex names. Every other name is ours.
 */
#ifndef DBUTIL_DCN_SERVER_H
#define DBUTIL_DCN_SERVER_H

#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include <ipmi/server.h>
#include "connection.h"

/* a session's queue holds this many events; one more replaces them by an overflow event */
#define DCN_QUEUE_LIMIT			32

/* the most a client may ask for */
#define DCN_DATABASE_SIZE_MAX		512
#define DCN_TABLE_SIZE_MAX		128

namespace dbutil {

class DCNServer : public IPMI::Server::EventHandler {
public:
	typedef paf::list<Event *> EventList;
	typedef paf::map<IPMI::Session *, EventList> SessionMap;

	/* runs the server's dispatcher */
	class ServerThread : public paf::thread::Thread {
	public:
		ServerThread(DCNServer *server)
			: Thread(0x1000010A, 0x4000, "SceDbUtilDcnServer", NULL), m_server(server)
		{
		}
		virtual ~ServerThread();
		virtual void EntryFunction();

		DCNServer *m_server;
	};

	DCNServer();
	virtual ~DCNServer();

	int RemoveSession(IPMI::Session *session, bool erase);
	int Start();
	int Stop();

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
					  SceSize inSize, SceUInt32 outCount, void *pOut,
					  SceSize outSize)
	{
	}
	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, void *pIn, SceSize inSize,
					   void *pOut, SceSize outSize)
	{
	}
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

	static int Initialize();
	static int Finalize(bool force);
	static int PostEvent(IPMI::Session *session, SceUID processId, SceBool local,
			     Connection *source, int type, SceInt64 rowid, const char *database,
			     const char *table);
	static int GetEvent(IPMI::Session *session, SceUID *processId, SceBool *local,
			    Connection **source, int *type, SceInt64 *rowid, int databaseSize,
			    SceSize *databaseLength, char *database, int tableSize,
			    SceSize *tableLength, char *table);
	static int StopSession(IPMI::Session *session);

	static DCNServer *s_instance;
	static paf::thread::RMutex s_lock;	/* s_instance, s_refCount and every queue */
	static int s_refCount;			/* Initialize calls not finalized */

	bool m_reserved;			/* cleared by the constructor, read nowhere */
	ServerThread *m_thread;
	SessionMap m_sessions;
	IPMI::Server *m_server;
	void *m_workBuffer;			/* the dispatcher's */
	SceSize m_workBufferSize;
};

/* the server's method stubs (dcn_server_stub.cpp) */
int DCNDispatchSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount,
			  IPMI::BufferInfo *outList, SceUInt32 outCount);
int DCNDispatchAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount);

}

#endif
