/*
 * teleport_internal.h: internal types, error codes, and class declarations for
 * SceShellTeleport (0x8139AB6C..0x813A7253).
 */
#ifndef SHELL_TELEPORT_INTERNAL_H
#define SHELL_TELEPORT_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/map>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <timer/timerlistener.h>
#include <net/epoll.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/sockinfo.h>
#include <appmgr/appmgr.h>
#include "../livespace_db/livespace_db_internal.h"

/* SceShellTeleport error codes (0x801084xx) */
#define TELEPORT_ERR_INVALID_PARAM		(-2146401278)	/* 0x80108402 */
#define TELEPORT_ERR_XML_PARSE			(-2146401274)	/* 0x80108406 */
#define TELEPORT_ERR_SEND_TIMEOUT		(-2146401272)	/* 0x80108408 */
#define TELEPORT_ERR_SEND_FAILED		(-2146401271)	/* 0x80108409 */
#define TELEPORT_ERR_SUSPENDED_APP		(-2146401231)	/* 0x80108431 */
#define TELEPORT_ERR_FOREGROUND_APP		(-2146401230)	/* 0x80108432 */
#define TELEPORT_ERR_APP_LAUNCH_REJECTED	(-2146401229)	/* 0x80108433 */
#define TELEPORT_ERR_LAUNCH_FAILED		(-2146401228)	/* 0x80108434 */
#define TELEPORT_ERR_NOT_ALLOWED		(-2146401225)	/* 0x80108437 */
#define TELEPORT_ERR_UNSUPPORTED_LIST		(-2146401224)	/* 0x80108438 */

/* IPMI method IDs and error codes */
#define TELEPORT_IPMI_CMD_LAUNCH_NOTIFY		0x001A0000u
#define TELEPORT_IPMI_ERR_UNSUPPORTED		(-2147351103)	/* 0x80021001 */

/* Network & protocol constants */
#define TELEPORT_MULTICAST_PORT			41899
#define TELEPORT_HTTP_BUFFER_SIZE		0x4000u
#define TELEPORT_UDP_BUFFER_SIZE		512u
#define TELEPORT_DEFAULT_SEND_TIMEOUT_US	120000000
#define TELEPORT_EVENT_READY_BIT		4u
#define TELEPORT_MAX_EPOLL_EVENTS		5

/* HTTP method enum returned by TeleportHttpServerThread::ParseHttpRequestHeader */
enum TeleportHttpMethod {
	TELEPORT_HTTP_POST = 0,
	TELEPORT_HTTP_GET = 1,
	TELEPORT_HTTP_DELETE = 2,
	TELEPORT_HTTP_OPTIONS = 3,
	TELEPORT_HTTP_UNKNOWN = 4
};

/* Signed launch allowlist header parsed by teleportListParseHeader (0x810A4C0A) */
struct TeleportSignedListHeader {
	SceInt32 formatVersion;
	SceInt32 listRevision;
	SceInt32 listMode;
	SceUInt32 bodyOffset;
};

/* IPMI 0x001A0000 launch notification payload from launched app */
struct TeleportIpmiLaunchResponse {
	SceUInt32 port;
	SceInt32 rejected;
	char extText[128];
};

/* Parsed HTTP request metadata passed to GET/POST/DELETE handlers */
struct TeleportHttpRequestInfo {
	SceInt32 contentLength;
	SceSize totalLength;
	paf::string uri;
	paf::string authToken;

	TeleportHttpRequestInfo() : contentLength(-1), totalLength(0) {}
};

/* Pending HTTP launch request awaiting IPMI 0x001A0000 completion */
struct TeleportPendingLaunch {
	SceNetId clientSocket;
	SceInt32 launchMode;
	paf::string appXml;

	TeleportPendingLaunch() : clientSocket(0), launchMode(0) {}
};

/* UDP receive buffer descriptor */
struct TeleportUdpPacket {
	char *buffer;
	SceSize capacity;
};

class TeleportHttpServerThread;
class TeleportMulticastThread;
class TeleportNetMonitor;

/* Socket send function signatures */
typedef SceInt32 (*TeleportHttpSendFunc)(TeleportHttpServerThread *thread, SceNetId sock,
					 const void *buffer, SceSize length, SceInt32 timeoutUs);
typedef SceInt32 (*TeleportUdpSendFunc)(TeleportMulticastThread *thread, SceNetId sock,
					const void *buffer, SceSize length);

/* HTTP protocol handler table registered with TeleportHttpServer */
struct TeleportHttpCallbacks {
	SceInt32 (*onGet)(TeleportHttpServerThread *thread, SceNetId sock,
			  const char *request, SceSize length,
			  TeleportHttpSendFunc sendFn, TeleportHttpRequestInfo *reqInfo);
	SceInt32 (*onPost)(TeleportHttpServerThread *thread, SceNetId sock,
			   const char *request, SceSize length,
			   TeleportHttpSendFunc sendFn, TeleportHttpRequestInfo *reqInfo);
	SceInt32 (*onDelete)(TeleportHttpServerThread *thread, SceNetId sock,
			     const char *request, SceSize length,
			     TeleportHttpSendFunc sendFn, TeleportHttpRequestInfo *reqInfo);
	SceInt32 (*onOptions)(TeleportHttpServerThread *thread, SceNetId sock,
			      const char *request, SceSize length,
			      TeleportHttpSendFunc sendFn, void *userData);
	SceInt32 (*onDisconnect)(TeleportHttpServerThread *thread, SceNetId sock,
				 void *userData);
	SceInt32 (*onIpmiResponse)(TeleportHttpServerThread *thread,
				   const TeleportIpmiLaunchResponse *response,
				   SceInt32 timeoutUs, TeleportHttpSendFunc sendFn,
				   void *userData);
	char *recvBuffer;
	SceSize recvBufferSize;
	void *userData;
};

/* Multicast discovery callback table registered with TeleportMulticastServer */
struct TeleportMulticastCallbacks {
	SceInt32 (*onRecv)(TeleportMulticastThread *thread, SceNetId sock,
			   const TeleportUdpPacket *packet, TeleportUdpSendFunc sendFn,
			   void *userData);
	SceInt32 (*onSend)(TeleportMulticastThread *thread, SceNetId sock,
			   SceInt32 replyType, TeleportUdpSendFunc sendFn);
	char *recvBuffer;
	SceSize recvBufferSize;
	void *userData;
};

/* External signed allowlist helpers (0x810A4B6C, 0x810A4C0A, 0x810A4C12) */
extern SceInt32 teleportListVerifySignedFile(paf::common::SharedPtr<paf::File> *file,
					     void *outBuffer, SceSize *inOutSize);
extern SceInt32 teleportListParseHeader(const char *buffer, SceSize size,
					TeleportSignedListHeader *outHeader);
extern SceInt32 teleportListMarkForceRefresh(void);

/* Utility functions (teleport_multicast.cpp, 0x813A6F26..0x813A71DB) */
void teleportSplitString(paf::vector<paf::string> *outParts, const paf::string &input,
			 const paf::string &delimiters, SceSize maxParts);
SceInt32 teleportHasNoControlChars(const paf::string &text);
void teleportEnsureVideoTempDir(void);

/* Device/LSDB/auth/allowlist helper (teleport_app_helper.cpp, 0x8139B32E..0x813A0F43) */
class TeleportAppInfoHelper {
public:
	TeleportAppInfoHelper() {}
	virtual ~TeleportAppInfoHelper() {}

	SceInt32 GetDeviceInfo(char *outName, char *outVersion);
	void GetAppVersionAndTitle(LsdbAppInfoPtr appInfo, paf::string *outVersion,
				   paf::string *outTitle);
	SceInt32 QueryLaunchableTitles(paf::vector<paf::string> *outTitleIds);
	SceInt32 BuildAppsListXml(char *outBuffer, SceSize bufferSize);
	SceUInt32 ParseMacSeed(const char *macStr);
	SceInt32 GenerateAuthSalt(char *outSaltHex, const char *macStr);
	SceInt32 CheckForegroundAppConflict(const char *uri, SceSize uriLen,
					    paf::string *outConflictAppXml,
					    paf::string *outForegroundTitle);
	SceInt32 ResolveTitleIdByUri(const paf::string &uri, paf::string *outTitleId);
	SceInt32 IsAppRunningByUri(const paf::string &uri);
	SceInt32 CheckRunningAppByStatus(const SceAppMgrAppStatus &status,
					 const paf::string &uri, paf::string *outAppXml,
					 paf::string *outRunningTitle);
	SceInt32 CheckSuspendedAppConflict(const paf::string &uri, paf::string *outErrorXml,
					   paf::string *outAppXml);
	SceInt32 DecodeHexString(const paf::string &hexStr, SceUInt8 *outBytes,
				 SceSize maxBytes);
	SceInt32 VerifyPasscodeAuth(const paf::string &clientDigestHex,
				    const paf::string &saltHex);
	SceInt32 ValidateAllowlistEntry(const char *entry, SceSize length);
	SceInt32 ResolveAppXmlAndTitle(const paf::string &uri, paf::string *outAppFieldsXml,
				       paf::string *outTitleId, paf::string *outVersion);
	SceInt32 CheckLaunchAllowlist(const paf::string &uri);
	bool ValidateUtf8NoBom(const paf::string &text);
	SceInt32 ValidateLaunchRequest(const paf::string &uri,
				       const paf::string &clientDigestHex,
				       const paf::string &saltHex, SceInt32 launchOption,
				       paf::string *outErrorXml, paf::string *outAppXml);
};

/* HTTP/TCP epoll server thread (teleport_http_server.cpp, 0x813A0FB4..0x813A1E33) */
class TeleportHttpServerThread : public paf::thread::Thread {
public:
	explicit TeleportHttpServerThread(const TeleportHttpCallbacks &callbacks);
	virtual ~TeleportHttpServerThread();
	virtual void EntryFunction();

	SceInt32 BindAndListen(void);
	SceInt32 ParseHttpRequestHeader(const char *rawRequest, SceSize length,
					SceSize *outExpectedTotal,
					TeleportHttpRequestInfo *outInfo);

	static SceInt32 SendWithTimeout(TeleportHttpServerThread *thread, SceNetId sock,
					const void *buffer, SceSize length, SceInt32 timeoutUs);

	SceNetId m_listenSocket;
	SceNetId m_epollId;
	SceNetEpollEvent *m_epollEvents;
	SceUInt8 m_localIpv4[4];
	SceUInt16 m_listenPort;
	SceUID m_eventFlag;
	SceInt32 m_stopState;
	TeleportHttpCallbacks m_callbacks;
	bool m_useEphemeralPort;
	bool m_receivingBody;
	SceSize m_receivedBytes;
	SceSize m_expectedBytes;
};

/* HTTP server singleton controller */
class TeleportHttpServer {
public:
	TeleportHttpServer() : m_thread(NULL) {}
	~TeleportHttpServer();

	static TeleportHttpServer *GetInstance(void);

	SceInt32 Start(const TeleportHttpCallbacks &callbacks);
	SceInt32 Stop(void);
	SceInt32 Reconfigure(const SceUInt8 localIpv4[4], SceUInt16 port);
	SceInt32 DispatchIpmiResponse(const TeleportIpmiLaunchResponse *response,
				      SceInt32 timeoutUs);
	void DestroyThread(void);

	SceUInt16 GetListenPort(void) const
	{
		return (m_thread != NULL) ? m_thread->m_listenPort : 0;
	}

private:
	TeleportHttpServerThread *m_thread;
};

/* HTTP protocol handler singleton (teleport_http_protocol.cpp, 0x813A1E34..0x813A6123) */
class TeleportHttpProtocol {
public:
	TeleportHttpProtocol();
	~TeleportHttpProtocol();

	static TeleportHttpProtocol *GetInstance(void);

	static SceInt32 HandleGet(TeleportHttpServerThread *thread, SceNetId sock,
				  const char *request, SceSize length,
				  TeleportHttpSendFunc sendFn,
				  TeleportHttpRequestInfo *reqInfo);
	static SceInt32 HandlePost(TeleportHttpServerThread *thread, SceNetId sock,
				   const char *request, SceSize length,
				   TeleportHttpSendFunc sendFn,
				   TeleportHttpRequestInfo *reqInfo);
	static SceInt32 HandleDelete(TeleportHttpServerThread *thread, SceNetId sock,
				     const char *request, SceSize length,
				     TeleportHttpSendFunc sendFn,
				     TeleportHttpRequestInfo *reqInfo);
	static SceInt32 HandleOptions(TeleportHttpServerThread *thread, SceNetId sock,
				      const char *request, SceSize length,
				      TeleportHttpSendFunc sendFn, void *userData);
	static SceInt32 HandleDisconnect(TeleportHttpServerThread *thread, SceNetId sock,
					 void *userData);
	static SceInt32 HandleIpmiResponse(TeleportHttpServerThread *thread,
					   const TeleportIpmiLaunchResponse *response,
					   SceInt32 timeoutUs, TeleportHttpSendFunc sendFn,
					   void *userData);

	SceInt32 SendLaunchResponse(TeleportHttpServerThread *thread,
				    TeleportHttpSendFunc sendFn, SceNetId sock,
				    const paf::string &appFieldsXml,
				    const paf::string &errorXml, SceInt32 statusCode,
				    SceInt32 timeoutUs, SceUInt16 port, const char *extText);
	void BuildErrorXml(paf::string *outXml, const paf::string &appXml, SceInt32 errorCode);
	void BuildEmptyAppXml(paf::string *outXml);
	SceInt32 LaunchVideoStreaming(const char *launchParam);
	bool IsUpperHexString(const paf::string &text, bool hasHexPrefix);
	void PruneExitedApps(void);
	void EraseSocketSalt(SceNetId sock);

	static SceInt32 SetPendingLaunch(const TeleportPendingLaunch &pending);
	static bool HasPendingLaunch(void);
	static SceInt32 TakePendingLaunch(TeleportPendingLaunch *outPending);
	static void ReplaceAllSubstrings(paf::string *target, const paf::string &fromStr,
					 const paf::string &toStr);
	static void UnescapeXmlEntities(paf::string *text);
	static SceInt32 ParseLaunchRequestXml(const paf::string &httpMessage,
					      paf::string *outUri, paf::string *outOption);

	paf::thread::RMutex m_mutex;
	paf::map<SceNetId, paf::string> m_socketSalts;
	paf::map<SceUInt32, SceUID> m_remoteLaunchedPids;
	char *m_responseBuffer;
	TeleportPendingLaunch m_pendingLaunch;
};

/* UDP multicast discovery thread (teleport_multicast.cpp, 0x813A6194..0x813A6957) */
class TeleportMulticastThread : public paf::thread::Thread {
public:
	explicit TeleportMulticastThread(const TeleportMulticastCallbacks &callbacks);
	virtual ~TeleportMulticastThread();
	virtual void EntryFunction();

	SceInt32 BindMulticastSocket(void);

	static SceInt32 SendReply(TeleportMulticastThread *thread, SceNetId sock,
				  const void *buffer, SceSize length);

	SceNetId m_socket;
	SceUID m_eventFlag;
	SceInt32 m_stopState;
	TeleportMulticastCallbacks m_callbacks;
	SceNetSockaddr m_peerAddr;
	SceUInt8 m_localIpv4[4];
	SceUInt16 m_port;
	SceUInt8 m_groupIpv4[4];
};

/* UDP multicast discovery singleton */
class TeleportMulticastServer {
public:
	TeleportMulticastServer() : m_thread(NULL) {}
	~TeleportMulticastServer();

	static TeleportMulticastServer *GetInstance(void);

	SceInt32 Start(const TeleportMulticastCallbacks &callbacks);
	SceInt32 Stop(void);
	SceInt32 Reconfigure(const SceUInt8 localIpv4[4], const SceUInt8 groupIpv4[4],
			     SceUInt16 port);
	void DestroyThread(void);

private:
	TeleportMulticastThread *m_thread;
};

/* SSDP-style multicast discovery protocol handlers */
class TeleportDiscoveryProtocol {
public:
	static TeleportDiscoveryProtocol *GetInstance(void);

	static SceInt32 SendSearchReply(TeleportMulticastThread *thread, SceNetId sock,
					SceInt32 replyType, TeleportUdpSendFunc sendFn);
	static SceInt32 HandleSearchPacket(TeleportMulticastThread *thread, SceNetId sock,
					   const TeleportUdpPacket *packet,
					   TeleportUdpSendFunc sendFn, void *userData);
};

/* Frame-ticked timer listener that registers/polls registry & power callbacks */
class TeleportCallbackTimerListener : public paf::TimerListener {
public:
	TeleportCallbackTimerListener(paf::Timer *timer, TeleportNetMonitor *monitor)
		: paf::TimerListener(timer, monitor), m_callbacksRegistered(false) {}
	virtual ~TeleportCallbackTimerListener() {}
	virtual void OnFinish(void *timerData, void *userData);

private:
	bool m_callbacksRegistered;
};

/* Network, registry, and power monitor singleton (0x813A6958..0x813A6F25) */
class TeleportNetMonitor {
public:
	TeleportNetMonitor();
	~TeleportNetMonitor() {}

	static TeleportNetMonitor *GetInstance(void);

	void RefreshNetworkInfo(void);
	SceInt32 Initialize(void);
	SceInt32 GetLocalIpv4(SceUInt8 outIpv4[4]);
	SceInt32 GetMacAddress(char *outMac);
	SceInt32 StartServersIfEnabled(void);

	static void OnNetCtlEvent(SceInt32 prevState, SceInt32 curState, SceInt32 event,
				  SceInt32 errorCode, SceInt32 info, void *userArg);
	static SceInt32 OnRegistryChanged(SceInt32 notifyId, SceInt32 notifyCount,
					  SceInt32 notifyArg, void *common);
	static SceInt32 OnPowerCallback(SceInt32 notifyId, SceInt32 notifyCount,
					SceInt32 powerInfo, void *common);

	SceInt32 m_netDisconnected;
	SceInt32 m_mediaTransferDisabled;
	SceUInt8 m_localIpv4[4];
	char m_macAddress[18];
	TeleportCallbackTimerListener *m_timerListener;
};

/* Top-level SceShellTeleport service singleton (teleport_service.cpp, 0x8139ABDE..0x8139B2BB) */
class TeleportService {
public:
	TeleportService() {}
	virtual ~TeleportService();

	static TeleportService *GetInstance(void);

	SceInt32 Init(void);
	SceInt32 HandleIpmiLaunchResponse(const TeleportIpmiLaunchResponse *response,
					  SceInt32 timeoutUs);
};

#endif /* SHELL_TELEPORT_INTERNAL_H */
