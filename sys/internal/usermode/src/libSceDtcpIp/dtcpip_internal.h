/*
 * SceDtcpIp (vs0:sys/external/libSceDtcpIp.suprx): the DTCP-IP sink library. The player
 * application holds the library's lock and hands each request to its worker thread, which
 * forwards it to the middleware application (NPXS10080, "swagner") over the shared-memory
 * IPC the middleware owns. No Sony record names the library's files or its internal
 * functions; the file names and every name below the exports are ours.
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <dtcpip/error.h>

/* the library's states (the state word of the context) */
#define SCE_DTCP_IP_STATE_STOPPED		0
#define SCE_DTCP_IP_STATE_IDLE			1
#define SCE_DTCP_IP_STATE_SINK_OPEN		2
#define SCE_DTCP_IP_STATE_URI_SET		3
#define SCE_DTCP_IP_STATE_STREAM_OPEN		4
#define SCE_DTCP_IP_STATE_PLAYING		5
#define SCE_DTCP_IP_STATE_STREAM_DONE		6
#define SCE_DTCP_IP_STATE_IPC_ERROR		7
#define SCE_DTCP_IP_STATE_IPC_TIMEOUT		8

/* the worker's command states (the worker and the API hand a command over through them) */
#define SCE_DTCP_IP_COMMAND_IDLE			1
#define SCE_DTCP_IP_COMMAND_QUEUED		2
#define SCE_DTCP_IP_COMMAND_RUNNING		3
#define SCE_DTCP_IP_COMMAND_DONE			4

/* the commands the worker carries out; 14 marks an empty command block */
#define SCE_DTCP_IP_COMMAND_NONE			14
#define SCE_DTCP_IP_COMMAND_EXIT				1
#define SCE_DTCP_IP_COMMAND_CONNECT			2
#define SCE_DTCP_IP_COMMAND_ACTIVATE			3
#define SCE_DTCP_IP_COMMAND_CREATE_SINK			4
#define SCE_DTCP_IP_COMMAND_CLOSE_SINK			5
#define SCE_DTCP_IP_COMMAND_SET_URI			6
#define SCE_DTCP_IP_COMMAND_CLEAR_URI			7
#define SCE_DTCP_IP_COMMAND_OPEN_STREAM			8
#define SCE_DTCP_IP_COMMAND_SEEK_TIME			9
#define SCE_DTCP_IP_COMMAND_SEEK_BYTE			10
#define SCE_DTCP_IP_COMMAND_READ			11
#define SCE_DTCP_IP_COMMAND_CLOSE_STREAM		12
#define SCE_DTCP_IP_COMMAND_STOP_STREAM			13

/* the shared memory heap the library allocates from (dtcpip_memory.c) */
void *dtcpIpAlloc(SceSize size);
void dtcpIpFree(void *pointer);
void *dtcpIpMalloc(SceSize size);
void dtcpIpMspaceFree(void *pointer);
void *dtcpIpHeapMalloc(SceSize size);
void dtcpIpHeapFree(void *pointer);
void *dtcpIpAllocWith(void *(*allocator)(SceSize size), SceSize size);
void dtcpIpFreeWith(void (*deallocator)(void *pointer), void *pointer);

int dtcpIpCreateHeap(SceSize size);
int dtcpIpDestroyHeap(void);

int dtcpIpPrintf(const char *format, ...);
int dtcpIpSnprintf(char *buffer, const char *format, ...);
SceSize dtcpIpStrnlen(const char *string);
char *dtcpIpStrncpy(char *destination, const char *source, SceSize count);

/* the library's locks and conditions (dtcpip_core.c) */
enum {
	SCE_DTCP_IP_LOCK_MANAGER = 0,
	SCE_DTCP_IP_LOCK_HTTP,
	SCE_DTCP_IP_LOCK_COMMAND,
	SCE_DTCP_IP_LOCK_HTTP_WORKER,
	SCE_DTCP_IP_LOCK_AUTH,
	SCE_DTCP_IP_LOCK_COUNT
};

enum {
	SCE_DTCP_IP_COND_COMMAND = 2,
	SCE_DTCP_IP_COND_HTTP_WORKER = 3,
	SCE_DTCP_IP_COND_AUTH = 4
};

int dtcpIpCreateLocks(void);
int dtcpIpDestroyLocks(void);
int dtcpIpLock(unsigned int lock);
int dtcpIpUnlock(unsigned int lock);
int dtcpIpWaitCond(unsigned int cond, SceUInt *timeout);
int dtcpIpSignalCond(unsigned int cond);

/* the middleware connection (dtcpip_session.c) */
int dtcpIpMapError(int status);
void dtcpIpSessionClosed(void);
void dtcpIpSessionCancelled(void);
void dtcpIpSessionClear(void);

/* the stream clients the sink classes hold (dtcpip_stream.c defines their tables): the
 * SW IPC session and stream objects, one for the sink and one for the move sink */
struct SceDtcpIpStreamClient {
	int (*connect)(void *session);
	int (*setUri)(const char *uri, unsigned short port);
	int (*closeSession)(void);
	int (*flush)(void);
	int (*open)(int receiveSize, int sendSize, int blockSize, SceInt32 affinity);
	int (*closeStream)(void);
	int (*getInfo)(void);
	int (*setParameter)(int parameter);
	int (*receive)(void **buffer, SceSize *size, unsigned char *type, int timeout);
	int (*release)(SceSize size);
	int (*send)(void **buffer, SceSize *size, int timeout);
	int (*sendMessage)(int message);
	int (*abort)(void);
	int (*isStreamOpen)(void);
	int (*close)(void);
	int (*isSessionOpen)(void);
	int (*getMoveHeader)(void);
};

extern const struct SceDtcpIpStreamClient g_systemStreamClient;
extern const struct SceDtcpIpStreamClient g_streamStreamClient;

/* the tables the sink managers point the session's client at (dtcpip_stream.c) */
const struct SceDtcpIpStreamClient *dtcpIpGetSystemStreamClient(void);
const struct SceDtcpIpStreamClient *dtcpIpGetStreamStreamClient(void);

/* the sink's stream client, set when a sink manager is created (dtcpip_sink.c) */
extern const struct SceDtcpIpStreamClient *g_streamClient;

/* the sink managers (dtcpip_session.c defines their tables; the sink and the move sink
 * classes implement them): the middleware's sink and move-sink devices */
struct SceDtcpIpSinkManager {
	int (*create)(void);
	int (*teardown)(void);
	int (*setParameter)(const char *parameter);
	int (*close)(void);
	int (*setUri)(const char *uri, unsigned short port);
	int (*clearUri)(void);
	int (*startStream)(void);
	int (*pauseStream)(void);
	int (*openStream)(const char *uri, int mode, void *streamInfo, int reserved, SceInt32 affinity);
	int (*read)(void *buffer, SceSize size, int argument, int *outSize);
	int (*seekByte)(SceUInt64 start, SceUInt64 end);
	int (*seekTime)(SceUInt64 start, SceUInt64 end);
	int (*stopStream)(void);
	int (*closeStream)(void);
	int (*addRequestHeader)(const char *name, const char *value, unsigned int mode);
	int (*removeRequestHeader)(const char *name);
	int (*getStreamInfo)(int *outInfo);
	int (*getSessionBuffer)(int *outBuffer);
};

extern const struct SceDtcpIpSinkManager g_sinkManager;
extern const struct SceDtcpIpSinkManager g_moveSinkManager;

/* the sink session, through the middleware's shared memory (dtcpip_session.c) */
struct SceDtcpIpSession {
	void *connection;	/* the middleware's IPC connection, NULL while closed */
	int stream;		/* the stream's state: 0 none, 1 seek pending, 2 open, 3 done */
	int buffer;		/* the stream buffer's status */
	int status;		/* the last middleware status an IPC error reported */
#if FW_VERSION == 0x03630011
	int closed;		/* 0x10: the session was closed (the RESET export) */
	int reserved;		/* 0x14: the 3.630 block keeps the http thread here */
#else
	int cancelled;		/* 0x10: the API asked to cancel what it is holding out on */
	int closed;		/* 0x14: the session was closed (the RESET export) */
#endif
	char *moveUri;		/* the move sink's uri, until its manager closes */
	unsigned short movePort;	/* the move sink's port */
};

extern struct SceDtcpIpSession g_session;

/* the HTTP worker thread (dtcpip_session.c) */
struct SceDtcpIpHttpWorker {
	SceUID thread;		/* -1 while stopped */
	int stopRequest;	/* 2 asks the worker to exit */
	int running;
	int status;		/* the worker's last status */
	int status2;		/* the worker's second status word */
};

extern struct SceDtcpIpHttpWorker g_httpWorker;

int dtcpIpStartHttpWorker(SceInt32 priority, SceInt32 affinity);
int dtcpIpStopHttpWorker(void);
int dtcpIpHttpWorkerThread(SceSize args, void *argp);

/* the middleware connection check (dtcpip_session.c): the pair the context holds at
 * its middleware connection word */
struct SceDtcpIpMwConnection {
	void *handle;		/* the IPC connection the check holds */
	int closed;		/* set when the check is not to ask again */
};

int dtcpIpCheckActivation(struct SceDtcpIpMwConnection *connection);
int dtcpIpOpenMwSession(void);
int dtcpIpCloseMwSession(struct SceDtcpIpMwConnection *connection);

/* the connection's kind (dtcpip_http.c): a WLAN that is not secured */
int dtcpIpIsWlanConnection(int *outIsWlan, int *outIsSecured);

/* the SW IPC the middleware application owns (dtcpip_swipc.c) */
extern const unsigned int g_dtcpIpIpcConfig[35];
extern const unsigned int g_dtcpIpActivateConfig[35];
extern int g_dtcpIpIpcFlag;

/* an SW IPC request message: the header the pipe carries and its reply */
struct SceDtcpIpIpcMessage {
	int command;		/* 0x00 */
	unsigned char channel;	/* 0x04: the stream channel, 255 for the session */
	unsigned char padAfterChannel;
	unsigned short size;	/* 0x06: the payload's size */
	void *payload;		/* 0x08 */
	unsigned int value0;	/* 0x0C */
	unsigned int value1;	/* 0x10 */
	unsigned int value2;	/* 0x14 */
	unsigned int value3;	/* 0x18 */
	int result;		/* 0x1C: the middleware's status */
	unsigned short outSize;	/* 0x20: the reply payload's size */
	unsigned short padAfterOutSize;
	void *outPayload;	/* 0x24: the reply payload in the shared memory */
	unsigned int out0;	/* 0x28 */
	unsigned int out1;	/* 0x2C */
	unsigned int out2;	/* 0x30 */
	unsigned int out3;	/* 0x34 */
};

/* an SW IPC session object: the two clients over one shared block */
struct SceDtcpIpIpcObject {
	void *session;		/* the session client's object */
	void *stream;		/* the stream client's object */
	int shared;		/* the shared memory's handle */
	int sharedSize;
	int reserved;
};

int dtcpIpIpcInitSession(struct SceDtcpIpIpcObject *object, const unsigned int *config, SceSize size);
int dtcpIpIpcInitStream(void **outStream, void *session, int reserved, int bufferSize,
			int streams, int affinity);
int dtcpIpIpcFiniStream(void *stream, int status);
int dtcpIpIpcFiniSession(void *session);
int dtcpIpIpcInitShared(void *stream, int *outShared);
int dtcpIpIpcRequest(void *stream, struct SceDtcpIpIpcMessage *message);


int dtcpIpIpcConnect(void **outConnection);
int dtcpIpIpcDisconnect(void *connection);
int dtcpIpIpcCreate(int *outConnection, const int *sessionInfo, const unsigned int *config,
		    SceSize size);
int dtcpIpIpcWaitReady(int connection, int *outStatus);
int dtcpIpIpcDestroy(int connection);
int dtcpIpIpcClose(int connection);
int dtcpIpIpcCloseSession(void *connection);
int dtcpIpIpcOpenSession(void *connection);
int dtcpIpIpcGetSessionStatus(void *connection, int *outStatus);
int dtcpIpMapIpcStatus(int status);

/* the library's context (dtcpip.c) */
struct SceDtcpIpContext {
	SceUID workerThread;		/* the worker thread, -1 while stopped */
	SceUID workerEvent;		/* the event flag the API signals, -1 while stopped */
	int state;			/* SCE_DTCP_IP_STATE_* */
	int commandState;		/* SCE_DTCP_IP_COMMAND_* */
	SceInt32 streamThreadAffinity;	/* the streaming thread's CPU affinity */
	const struct SceDtcpIpSinkManager *manager;
	struct SceDtcpIpMwConnection mw;	/* the activation check's connection */
};

extern struct SceDtcpIpContext g_dtcpIp;

/* the worker's command block: an id, the result the worker writes back and the arguments */
union SceDtcpIpCommand {
	struct {
		int id;
		int result;
	} common;
	/* CREATE_SINK: the manager class and the parameter string the worker frees */
	struct {
		int id;
		int result;
		int sinkType;
		char *parameter;
		int unused[4];
	} createSink;
	/* SET_URI: the uri and its port */
	struct {
		int id;
		int result;
		char *uri;
		unsigned short port;
		int unused[4];
	} setUri;
	/* OPEN_STREAM: the uri, its mode, the by-value parameter and the reserved word */
	struct {
		int id;
		int result;
		char *uri;
		int mode;
		unsigned char parameter[12];
		int reserved;
	} openStream;
	/* SEEK_TIME, SEEK_BYTE: the range */
	struct {
		int id;
		int result;
		SceUInt64 start;
		SceUInt64 end;
		int unused[2];
	} range;
	/* READ: the caller's buffer */
	struct {
		int id;
		int result;
		void *buffer;
		SceSize size;
		int argument;
		int *outSize;
		int unused[2];
	} read;
};

extern union SceDtcpIpCommand g_command;

/* the worker thread (dtcpip.c) */
int dtcpIpWorkerThread(SceSize args, void *argp);

/* the manager classes' function tables (dtcpip_core.c) */
const struct SceDtcpIpSinkManager *dtcpIpGetSinkManager(void);
const struct SceDtcpIpSinkManager *dtcpIpGetMoveSinkManager(void);
