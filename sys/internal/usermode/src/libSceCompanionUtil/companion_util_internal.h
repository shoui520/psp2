/*
 * SceCompanionUtil's own classes: the link to a PS4 (an "Orbis") for PS4 Link and Remote Play.
 * The library finds the PS4s on the network by UDP (port 987: "SRCH", "WAKEUP" and "LAUNCH"
 * requests), connects to the one chosen by TCP and exchanges packets with it, AES-CBC encrypted
 * under a key it sends RSA-OAEP encrypted. Three threads run it: the event thread steps the state
 * machine, the send thread runs the UDP send jobs and the TCP send queue, the receive thread the
 * UDP search and the TCP receive job.
 *
 * Sony's names are the namespace and two classes (the mutexes' names:
 * "SceCompanionUtil::CTcpSendJobQueueMutex", "SceCompanionUtil::CSearchListMutex") and the
 * exports PS4 Link's messages name; every other name is ours, from what the code does.
 */
#ifndef _COMPANION_UTIL_INTERNAL_H
#define _COMPANION_UTIL_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <net/socket.h>
#include <companion_util/companion_util.h>

/* the library's errors (no SDK header names them) */
#define COMPANION_UTIL_ERROR_INVALID_ARGUMENT	((int)0x80930001)
#define COMPANION_UTIL_ERROR_ALREADY_INIT	((int)0x80930002)
#define COMPANION_UTIL_ERROR_NOT_INIT		((int)0x80930003)
#define COMPANION_UTIL_ERROR_NO_MEMORY		((int)0x80930004)
#define COMPANION_UTIL_ERROR_NOT_STOPPED	((int)0x80930005)
#define COMPANION_UTIL_ERROR_IN_CALLBACK	((int)0x80930007)
#define COMPANION_UTIL_ERROR_SEARCHING		((int)0x80930021)
#define COMPANION_UTIL_ERROR_CONNECTING		((int)0x80930022)
#define COMPANION_UTIL_ERROR_CONNECTED		((int)0x80930023)
#define COMPANION_UTIL_ERROR_STOPPING		((int)0x80930024)
#define COMPANION_UTIL_ERROR_INVALID_STATE	((int)0x80930025)

/* what a TCP receive or send gives back besides 0 */
#define COMPANION_UTIL_NET_ERROR_IO		0x00800001
#define COMPANION_UTIL_NET_ERROR_UNEXPECTED	0x00800002
#define COMPANION_UTIL_NET_ERROR_BAD_SIZE	0x00800003
#define COMPANION_UTIL_NET_ERROR_BAD_HELLO	0x00800004
#define COMPANION_UTIL_NET_ERROR_REFUSED	0x00800008
#define COMPANION_UTIL_NET_ERROR_LOGIN		0x00800010
#define COMPANION_UTIL_NET_ERROR_DENIED		0x00800020
#define COMPANION_UTIL_NET_ERROR_UNKNOWN_TYPE	0x00880000

/* the UDP port the PS4s answer on */
#define COMPANION_UTIL_DISCOVERY_PORT		987

/* the first packet of each side: its type is "orbp" read as a word */
#define COMPANION_UTIL_PACKET_HELLO		0x6F636370

/* one PS4 as the UDP search found it */
typedef struct CompanionUtilHost {
	SceUInt32 address;
	SceUInt16 port;
	char hostName[126];
	char hostId[13];
	SceUInt8 padding;
	SceUInt16 status;		/* 1: on (HTTP 200), 2: standby (620) */
	SceUInt32 reserved;
	SceUInt64 lastSeen;		/* 0: gone */
} CompanionUtilHost;

/* the hosts as the search handler gets them (event 2) */
typedef struct CompanionUtilHostInfo {
	SceUInt32 address;
	SceUInt16 port;
	SceUInt16 status;
	char hostName[126];
	char hostId[13];
	SceUInt8 padding[1];
} CompanionUtilHostInfo;

typedef struct CompanionUtilSearchResult {
	SceUInt32 count;
	CompanionUtilHostInfo hosts[256];
} CompanionUtilSearchResult;

/*
 * A packet as both sides send it: its size, its type and what the type carries. The receive job
 * keeps its first 384 bytes; a text or a key follows the words in the 5 KiB buffers.
 */
#define COMPANION_UTIL_PACKET_SIZE	0x180
#define COMPANION_UTIL_BUFFER_SIZE	0x1400
#define COMPANION_UTIL_BODY_SIZE	0x1002

typedef struct CompanionUtilPacket {
	int size;
	int type;
	int arg[94];
} CompanionUtilPacket;

/* what a request to the send queue carries, by its type */
typedef union CompanionUtilRequestParam {
	struct {
		int arg[2];
	} value;			/* 5, 18 (entered), 24 */
	struct {
		const char *hostId;
	} host;				/* 11 */
	struct {
		int composeStart;
		int composeLength;
		SceUInt32 caret;
		int replaceStart;
		int replaceLength;
		SceWChar16 *text;
	} text;				/* 17 */
	struct {
		int mode;
		char registKey[4];
		int userCredential;
	} login;			/* 25 */
	SceUInt8 data[0x110];
} CompanionUtilRequestParam;

/* what the handlers get with a text (events 9 and 17) */
typedef struct CompanionUtilTextEvent {
	const void *text;
	int length;
	int arg[2];
} CompanionUtilTextEvent;

namespace SceCompanionUtil {

/* platform.cpp: what the packets tell the PS4 about this system */
int GetSystemSwVersionString(char *buf, SceSize size);
int GetPsCodeString(char *buf, SceSize size);
SceUInt32 IsDolce(void);
int GetAppTitle(char *buf, SceSize size);
int GetProtocolVersionString(char *buf, SceSize size);

/* callback_lock.cpp: which thread is inside the system callback */
class CCallbackLock {
public:
	SceUID m_threadId;
	bool m_inCallback;

	void Init(void);
	void Term(void);
	void SetOwner(void);
	void ClearOwner(void);
	void Enter(void);
	void Leave(void);
	bool IsInCallback(void);
};

/* timer.cpp: a period and its next expiry (microseconds) */
class CTimer {
public:
	SceUInt64 m_period;
	SceUInt64 m_deadline;

	static SceUInt64 GetTime(void);
	void Start(SceUInt32 seconds);
	bool IsExpired(void);
};

/* mutex.cpp: the library's mutex */
int CreateMutex(void);
int DeleteMutex(void);
int LockMutex(void);
int UnlockMutex(int lock);

/* memory.cpp: the work memory's heap */
int CreateHeap(void *base, SceSize size);
int DestroyHeap(void);
void *Allocate(SceSize size);
void Free(void *pointer);

/* search_list.cpp: the PS4s found */
class CSearchList {
public:
	CompanionUtilHost *m_hosts;
	SceUInt32 m_count;
	SceUInt32 m_capacity;

	/* a host by index, the first one past the end */
	CompanionUtilHost *At(SceUInt32 index)
	{
		if (index < m_count)
			return &m_hosts[index];
		return &m_hosts[0];
	}

	static int CreateMutex(void);
	static int DeleteMutex(void);
	void Init(SceUInt32 capacity);
	void Term(void);
	SceUInt32 GetCapacity(void);
	void Release(void);
	void Clear(void);
	void GetResult(CompanionUtilSearchResult *result);
	void CopyTo(CSearchList *list);
	void Update(CSearchList *list);
	SceUInt16 GetStatus(const char *hostId);
	SceUInt32 GetAddress(const char *hostId);
	bool IsSeenAfter(const char *hostId, SceUInt64 time);
};

/* udp.cpp: the search socket and its jobs */
class CUdpSocket {
public:
	static int Close(void);
	static int Open(void);
	static int Abort(void);
};

enum {
	JOB_IDLE,
	JOB_RUNNING,
	JOB_DONE
};

/* sends the search request to everyone */
class CUdpSearchSendJob {
public:
	int m_state;
	bool m_abort;
	int m_port;

	int Request(int port);
	int Run(void);
	void Finish(void);
};

/* sends one PS4 a launch or wakeup request */
class CUdpWakeupSendJob {
public:
	int m_state;
	bool m_abort;
	SceNetSockaddrIn m_address;
	int m_type;			/* 0: LAUNCH, 1: WAKEUP as PS4 Link, 2: as Remote Play */
	int m_userCredential;

	int Request(const SceNetSockaddrIn *address, int type, int userCredential);
	int Run(void);
	void Finish(void);
};

/* reads the PS4s' answers into a list of its own, then gives them to the search list */
class CUdpSearchRecvJob {
public:
	int m_state;
	bool m_abort;
	CSearchList *m_searchList;
	CSearchList m_hosts;
	SceUInt64 m_timeout;

	int Request(CSearchList *searchList, SceUInt64 timeout);
	int ParseStatus(const char *message);
	int Run(void);
	void Finish(int result);
};

/* tcp.cpp: the connection's socket */
class CTcpSocket {
public:
	static int Connect(const SceUInt32 *address, const SceUInt16 *port);
	static int Close(void);
	static int Abort(void);
};

/* builds one packet and sends it */
class CTcpSendJob {
public:
	int m_state;
	bool m_abort;
	SceUInt16 m_type;
	int m_result;
	bool m_sending;
	SceUInt8 reserved[51];
	CompanionUtilPacket *m_packet;
	int m_lastResult;

	int BuildPacket(CompanionUtilPacket *packet, const CompanionUtilRequestParam *param);
	int Send(void);
};

/* the messages to send, five at most, each with what frees its parameter */
typedef void (*CompanionUtilParamRelease)(void *param);

typedef struct CompanionUtilSendEntry {
	int type;
	void *param;
	CompanionUtilParamRelease release;
} CompanionUtilSendEntry;

class CTcpSendJobQueue {
public:
	int m_head;
	int m_count;
	int m_peak;
	CompanionUtilSendEntry m_entries[5];
	SceUInt8 reserved[56];
	CTcpSendJob m_job;

	static int CreateMutex(void);
	static int DeleteMutex(void);
	void Flush(void);
	void Init(void);
	void Term(void);
	void Run(bool force);
	void Abort(void);
	int GetCount(void);
	int Push(int type, void *param, CompanionUtilParamRelease release);
};

/* reads one packet */
class CTcpRecvJob {
public:
	int m_state;
	bool m_abort;
	int m_result;
	bool m_decrypt;
	const void *m_body;
	int m_bodySize;
	CompanionUtilPacket *m_header;
	void *m_bodyBuffer;

	int Request(CompanionUtilPacket *header, void *body, bool decrypt);
	int Run(void);
	void Finish(int result);
};

/* crypto.cpp: the session's AES-CBC key and IVs */
int CreateSession(void);
int DestroySession(void);
int EncryptSessionKey(unsigned char *out);
int SetEncryptIv(const void *iv);
int SetDecryptIv(const void *iv);
int GetPaddedSize(int size);
int GetTextSize(const CompanionUtilPacket *header);
int GetBodySize(const CompanionUtilPacket *header);
int Encrypt(const unsigned char *in, unsigned char *out, SceSize size);
int Decrypt(unsigned char *data, SceSize size);
int GenerateRandom(unsigned char *data, int size);

extern bool g_sessionReady;
extern unsigned char g_sessionKey[16];
extern unsigned char g_decryptIv[16];
extern unsigned char g_encryptIv[16];

/* the library: one instance, in the work memory */
enum {
	STATE_NONE,
	STATE_READY,
	STATE_SEARCHING,		/* the search export: reporting what answers */
	STATE_CONNECT,			/* a target given: is it on or in standby? */
	STATE_WAKING,			/* in standby: WAKEUP until it is on */
	STATE_LAUNCHING,		/* on: LAUNCH, then the TCP connection */
	STATE_HELLO,
	STATE_LOGIN,
	STATE_SEND_KEY,
	STATE_WAIT_LOGIN,
	STATE_LOGIN_RESULT,
	STATE_WAIT_TARGET,		/* the PS4 asked for another target */
	STATE_RELOGIN,
	STATE_SEND_HOST_ID,
	STATE_WAIT_HOST_ID,
	STATE_CONNECTED,
	STATE_STOPPING,
	STATE_STOPPED
};

/* what sceCompanionUtilGetStatus reports */
enum {
	STATUS_NONE,
	STATUS_RUNNING,
	STATUS_STOPPED,
	STATUS_INVALID = 0x8001
};

class CCompanionUtil {
public:
	int m_state;
	int m_textState;		/* 1: idle, 2: a text field is open */
	int m_terminate;
	int m_threadCount;
	SceUID m_eventThread;
	SceUID m_sendThread;
	SceUID m_recvThread;
	bool m_launchRequested;
	bool m_wakeupRequested;
	int m_connectMode;		/* -1, or what the connect export asked for */
	SceUInt64 m_wakeStart;
	CTimer m_searchTimer;
	CTimer m_resendTimer;
	CTimer m_stateTimer;
	CTimer m_keepAliveTimer;
	CTimer m_aliveTimer;
	CTimer m_wakeTimer;
	CTimer m_retryTimer;
	CUdpSearchSendJob m_searchSendJob;
	CUdpWakeupSendJob m_launchSendJob;
	CUdpWakeupSendJob m_wakeupSendJob;
	CUdpSearchRecvJob m_searchRecvJob;
	SceUInt8 reserved[48];
	CTcpSendJobQueue m_sendQueue;
	SceUInt8 reserved2[56];
	CTcpRecvJob m_recvJob;
	char **m_hostIds;
	SceUInt32 m_hostIdCount;
	SceUInt32 m_hostIdCapacity;
	CSearchList m_searchList;
	CCallbackLock m_callbackLock;
	CompanionUtilTarget m_target;
	bool m_targetSet;
	CompanionUtilPacket m_packet;
	void (*m_systemCallback)(int event);
	SceCompanionUtilEventHandler m_handlers[2];
	SceUInt8 m_body[COMPANION_UTIL_BODY_SIZE];
	bool m_textRequested;
	CTimer m_textRequestTimer;

	void Finalize(void);
	void Reserved(void);
	int Shutdown(void);
	int Start(const CompanionUtilInitParam *param);
	void Initialize(void);
	static int Init(const CompanionUtilInitParam *param);
	static int Term(void);
	int Stop(void);
	static int Abort(void);
	int SetHostIds(const CompanionUtilHostIdList *list);
	int GetStatus(SceUInt16 *status);
	static int QueryStatus(SceUInt16 *status);
	static int SetCallbackHandler(SceCompanionUtilEventHandler handler, SceUInt16 index);
	int SearchToOrbis(void);
	static int Search(void);
	static int UpdateTarget(CompanionUtilTarget *target);
	int Connect(int mode, const CompanionUtilTarget *target, const CompanionUtilHostIdList *list);
	static int ConnectToOrbis(const CompanionUtilTarget *target,
				  const CompanionUtilHostIdList *list);
	static int LaunchOrbis(const CompanionUtilTarget *target);
	static int PowerOnOrbisForRemotePlay(const CompanionUtilTarget *target);
	static int RequestText(void);
	int SendText(int composeStart, int composeLength, SceUInt32 caret, int replaceStart,
		     int replaceLength, const SceWChar16 *text);
	static int InputText(int composeStart, int composeLength, SceUInt32 caret, int replaceStart,
			     int replaceLength, const SceWChar16 *text);
	static int CloseText(int entered);
	static int SendValue(int arg0, int arg1);
	int UpdateConnected(void);
	bool IsRecvFailed(void);
	bool PostRequest(int type, const CompanionUtilRequestParam *param);
	int HandleLoginResult(void);
	int SendLaunch(void);
	int SendWakeup(bool remotePlay);
	int Update(void);
	static int EventThread(SceSize argSize, void *argBlock);
	int RunSendThread(void);
	static int SendThread(SceSize argSize, void *argBlock);
	int RunRecvThread(void);
	static int RecvThread(SceSize argSize, void *argBlock);
	static void ReleaseTextParam(void *param);
	static void ReleaseCloseParam(void *param);
	static void ReleaseValueParam(void *param);
	static void ReleaseParam(void *param);

	/* the handlers, in order */
	void Notify(SceUInt16 event, void *data)
	{
		int i;

		for (i = 0; i < 2; i++) {
			if (m_handlers[i] != NULL)
				m_handlers[i](event, data);
		}
	}

	/* the connection failed: stop and tell the handlers why */
	int Fail(int reason)
	{
		Stop();
		Notify(8, &reason);
		return -1;
	}

	/* the PS4 went away: stop and tell the system if a client was connected */
	int Lose(void)
	{
		bool connected = m_textState != 0;

		if (Stop() == 0 && connected) {
			m_callbackLock.Enter();
			m_systemCallback(6);
			m_callbackLock.Leave();
		}
		return -1;
	}

	SceUInt32 GetSearchTimeout(void)
	{
		return m_wakeupRequested ? 180000000 : 10000000;
	}
};

extern CCompanionUtil *g_pCompanionUtil;
extern bool g_started;
extern SceNetId g_udpSocket;
extern SceNetId g_tcpSocket;

} /* namespace SceCompanionUtil */

#endif /* _COMPANION_UTIL_INTERNAL_H */
