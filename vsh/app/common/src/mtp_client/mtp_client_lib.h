/*
 * mtp_client_lib.h: the MTP client library the shell links (a header of ours: no record
 * names one), the IPMI connection to SceMtpService that MTPR2's protocol 2 downloads over.
 * Sony's messages name finalizeClient, initializeClient, observerThreadEntry,
 * BeginHttpGetDataWithUrl, EndHttpGetDataWithUrl and RecvData, and the context manager's
 * methods; the class names and every other name are ours (PLACEHOLDER).
 */
#ifndef _MTP_CLIENT_LIB_H
#define _MTP_CLIENT_LIB_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/map>

#include "../../../../lowlayer/src/ipmi/client.h"

/* the errors the library returns (PLACEHOLDER names) */
#define MTP_CLIENT_ERROR_NOT_CONNECTED		((int)0x80540001)
#define MTP_CLIENT_ERROR_BUSY			((int)0x80540002)
#define MTP_CLIENT_ERROR_NO_MEMORY		((int)0x80540007)
#define MTP_CLIENT_ERROR_OUT_OF_MEMORY		((int)0x80540101)
#define MTP_CLIENT_ERROR_INVALID_ARGUMENT	((int)0x80540102)
#define MTP_CLIENT_ERROR_NO_TICKET		((int)0x80540103)
#define MTP_CLIENT_ERROR_ALREADY_INITIALIZED	((int)0x80540106)
#define MTP_CLIENT_ERROR_NOT_INITIALIZED	((int)0x80540107)
#define MTP_CLIENT_ERROR_THREAD			((int)0x805401FF)

/* what the service answers that the library tests for (PLACEHOLDER: meaning unknown) */
#define MTP_SERVICE_ERROR_80540014		((int)0x80540014)
#define MTP_SERVICE_ERROR_80540016		((int)0x80540016)
#define MTP_SERVICE_ERROR_8054001A		((int)0x8054001A)
#define MTP_SERVICE_ERROR_80540040		((int)0x80540040)

/* the connection's status */
struct MtpClientStatus {
	int state;			/* 3 and 4: connected */
	int reserved[20];		/* PLACEHOLDER: not read */
	int kind;			/* the object's kind: 1, 2 */
	int reserved2[4];		/* PLACEHOLDER: not read */
};

/* what the status observer hands a message of its type to (PLACEHOLDER) */
class MtpClientEventListener {
public:
	virtual ~MtpClientEventListener();
	virtual void onEvent(const void *data) = 0;
};

/*
 * The calls under way: a ticket per call, each with its own client event flag (Sony's class
 * name is unknown; its messages say CLTCNT and name the methods).
 */
class MtpClientContextMgr {
public:
	MtpClientContextMgr()
		: m_count(0), m_tickets(NULL), m_eventFlagBase(0)
	{
	}

	~MtpClientContextMgr();

	int Initialize(int count, SceUInt32 eventFlagBase);
	int Finalize(void);
	int Create(int *id, IPMI::EventFlagInfo *eventFlag);
	int Set(int id, int requestId);
	int Get(int id, int *requestId);
	int WaitResult(IPMI::Client *client, int id, int *requestId);
	int Delete(int id);
	int CreateSendRecvEvent(int id, IPMI::EventFlagInfo *eventFlag);
	int WaitSendRecvResult(IPMI::Client *client, int id);

	SceKernelLwMutexWork m_lock;
	int m_count;
	int *m_tickets;			/* the request of each call, -1 when free */
	SceUInt32 m_eventFlagBase;	/* the client event flag of ticket 0 */

	static int s_serial;		/* the serial of the next id */

private:
	void lock(void);
	void unlock(void);
};

class MtpClient {
public:
	MtpClient()
		: m_client(NULL), m_callbackClient(NULL), m_contexts(), m_requests(),
		  m_observerThread(-1), m_observerRunning(0), m_session(-1)
	{
		m_listeners[0] = NULL;
		m_listeners[1] = NULL;
		m_listeners[2] = NULL;
	}

	~MtpClient();

	int finalizeClient(void);
	int Bgdl_81137ADE(void);				/* finalize, if initialized */
	int initializeClient(IPMI::Client **client, IPMI::Client::Config config);
	int Bgdl_81137C7C(IPMI::Client::Config *config, int callback);	/* the config */
	int Bgdl_81137CF0(void);				/* connect */
	int Bgdl_81138068(int handle, int mode);		/* abort the transfer */
	int BeginHttpGetDataWithUrl(const char *url, SceSize size, SceInt64 offset, int *handle);
	int EndHttpGetDataWithUrl(int handle);
	int RecvData(int handle, void *buf, SceSize size, SceSize *received,
		     SceUInt64 *remaining);
	int Bgdl_81138432(MtpClientStatus *status);		/* the status */
	int Bgdl_8113844A(int handle);				/* cancel the transfer */
	int Bgdl_81138498(int *mode);
	int Bgdl_811384AE(int arg);
	int Bgdl_811384C4(void);

	IPMI::Client *m_client;
	IPMI::Client *m_callbackClient;		/* SceMtpServiceCa */
	MtpClientContextMgr m_contexts;
	int m_ended;				/* the transfer has been ended or aborted */
	paf::map<int, int> m_requests;		/* PLACEHOLDER: not used here */
	MtpClientEventListener *m_listeners[3];	/* by message type */
	SceUID m_observerThread;		/* SceMtpStatusObserver */
	int m_observerRunning;
	int m_session;

private:
	int startObserver(SceKernelThreadOptParam *opt, MtpClient **arg, SceUID *thread);
};

#endif
