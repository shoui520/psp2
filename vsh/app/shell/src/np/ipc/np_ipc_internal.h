/*
 * np_ipc_internal.h: shared internal declarations for SceShell's NP IPC servers
 * (JGA real-time bridge, SceNpAuthServer, SceNpManager, SceNpCommon,
 * SceNpCommerce2SessionManager, and RT netinfo/udpp2p signaling).
 */
#ifndef SHELL_NP_IPC_INTERNAL_H
#define SHELL_NP_IPC_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <ipmi/session.h>
#include <np_common/np_common.h>
#include <np/np_common.h>
#include <np/np_auth.h>
#include <np/np_auth_oauth.h>

/* bXCe DOM/XML lightweight types matching sys/internal/usermode/src/bXCe/bxce_internal.h */
struct BxceDocument;
struct BxceAttributeList;
typedef SceUInt32 BxceNodeId;

struct BxceNodeList {
	const BxceDocument *doc;
	void *head;
	void *tail;
	SceUInt32 count;
};

#define BXCE_DOCUMENT_SIZE		1124
#define BXCE_ATTRIBUTE_LIST_SIZE	20
#define BXCE_NODE_TYPE_ELEMENT		4
#define BXCE_NODE_TYPE_EMPTY_ELEMENT	5

/* NP real-time (XMPP IQ) stanza and handler descriptors */
struct NpRtJidPair {
	const SceNpId *selfNpId;
	const SceNpId *peerNpId;
};

struct NpRtStanza {
	SceUInt32 refCount;		/* +0x00 */
	NpJid fromJid;			/* +0x04 (16 bytes) */
	NpJid toJid;			/* +0x14 (16 bytes) */
	char *stanzaId;			/* +0x24 */
	int stanzaType;			/* +0x28 */
	int reserved;			/* +0x2C */
	BxceDocument *xmlDoc;		/* +0x30 */
};

struct NpRtStanzaInit {
	NpJid fromJid;			/* +0x00 */
	NpJid toJid;			/* +0x10 */
	const char *stanzaId;		/* +0x20 */
	int stanzaType;			/* +0x24 */
	BxceDocument *xmlDoc;		/* +0x28 */
};

struct NpXmppErrorStanza {
	SceUInt32 reserved[3];		/* +0x00 */
	int errorKind;			/* +0x0C (1: stanza error, 2: app error) */
	SceUInt8 errorCode;		/* +0x10 */
	SceUInt8 pad[3];		/* +0x11 */
	NpRtStanza *origStanza;		/* +0x14 */
};

struct NpRtHandlerConfig {
	const char *const *namespaces;	/* +0x00 */
	void (*onConnect)(void);	/* +0x04 */
	void (*onRecv)(int handlerId, NpRtStanza *stanza);	/* +0x08 */
	int reservedFirst;		/* +0x0C */
	int reservedSecond;		/* +0x10 */
};

typedef void (*NpRtIqResponseCallback)(int sessionHandle, int transactionId, int result,
				       NpRtStanza *response, void *userArg);

/* IMPS raw message channel registration callbacks */
struct NpImpsRawMsgServerCallbacks {
	void (*onClientConnected)(IPMI::Session *session);
	void (*onClientDisconnected)(IPMI::Session *session);
	int (*onClientMessage)(IPMI::Session *session, const void *msgBuf);
};

/* NP common LW mutex wrapper (32 bytes = SceKernelLwMutexWork) */
struct NpIpcLwMutex {
	SceKernelLwMutexWork work;
};

/* NP Job Queue and Job Base (vsh/app/shell NP manager job system) */
class NpJobQueue {
public:
	virtual ~NpJobQueue();
};

class NpJobBase {
public:
	NpJobBase(const char *name);
	virtual ~NpJobBase();
	virtual int Cancel(int cancelError);
	virtual bool IsCanceled() const;
	virtual int Run() = 0;
	virtual void Finish(int result) = 0;

	int m_requestId;		/* +0x04 */
	const char *m_name;		/* +0x08 */
	int m_state;			/* +0x0C */
	NpJobBase *m_next;		/* +0x10 */
	NpJobBase *m_prev;		/* +0x14 */
	NpJobQueue *m_queue;		/* +0x18 */
	int m_cancelError;		/* +0x1C */
	SceUInt8 m_canceled;		/* +0x20 */
};

/* NpAuth internal request parameter (36 bytes) */
struct NpAuthInternalParam {
	SceSize size;			/* +0x00 */
	SceUInt32 version;		/* +0x04 */
	const char *serviceId;		/* +0x08 */
	const void *cookie;		/* +0x0C */
	SceSize cookieSize;		/* +0x10 */
	const char *entitlementId;	/* +0x14 */
	SceUInt32 consumedCount;	/* +0x18 */
	int (*callback)(int reqId, int result);	/* +0x1C */
	void *callbackArg;		/* +0x20 */
};

/* OAuth authorization code request buffers (matching SceNpClientId + scope + redirectUri) */
struct NpOAuthAuthCodeRequest {
	SceNpClientId clientId;		/* +0x00 (136 bytes) */
	char scope[1025];		/* +0x88 */
};

struct NpOAuthAuthCodeWithRedirectRequest {
	SceNpClientId clientId;		/* +0x00 (136 bytes) */
	char scope[1025];		/* +0x88 */
	char redirectUri[2049];		/* +0x489 */
};

/* Error codes */
#define NP_JGA_ERR_NO_MEMORY		-2141873662	/* 0x80559A02 */
#define NP_JGA_ERR_TOO_LARGE		-2141873660	/* 0x80559A04 */
#define NP_JGA_ERR_XML_FAIL		-2141873659	/* 0x80559A05 */
#define NP_JGA_ERR_NOT_FOUND		-2141873658	/* 0x80559A06 */
#define NP_JGA_ERR_INVALID_TYPE		-2141873657	/* 0x80559A07 */
#define NP_JGA_ERR_INVALID_ID		-2141873656	/* 0x80559A08 */
#define NP_JGA_ERR_NO_CLIENT		-2141873655	/* 0x80559A09 */

#define NP_RT_ERR_TOO_LARGE		-2141874686	/* 0x80559602 */
#define NP_RT_ERR_INVALID_OP		-2141874685	/* 0x80559603 */
#define NP_RT_ERR_NO_MEMORY		-2141874683	/* 0x80559605 */
#define NP_RT_ERR_XML_FAIL		-2141874682	/* 0x80559606 */
#define NP_RT_ERR_UNKNOWN_NS		-2141874681	/* 0x80559607 */
#define NP_RT_ERR_NO_CLIENT		-2141903089	/* 0x8055270F */

#define NP_AUTH_ERR_ALREADY_INIT	-2141912319	/* 0x80550301 */
#define NP_AUTH_ERR_NO_MEMORY		-2141912316	/* 0x80550304 */
#define NP_AUTH_ERR_NOT_FOUND		-2141912315	/* 0x80550305 */
#define NP_AUTH_ERR_TABLE_FULL		-2141912314	/* 0x80550306 */

#define NP_MGR_ERR_INVALID_ARG		-2141911805	/* 0x80550503 */
#define NP_MGR_ERR_NO_MEMORY		-2141911804	/* 0x80550504 */
#define NP_MGR_ERR_ABORTED		-2141911801	/* 0x80550507 */
#define NP_MGR_ERR_NOT_FOUND		-2141911799	/* 0x80550509 */
#define NP_MGR_ERR_INVALID_TYPE		-2141911679	/* 0x80550581 */
#define NP_MGR_ERR_NO_SLOT		-2141911671	/* 0x80550589 */
#define NP_MGR_ERR_DESTROYED		-2141911668	/* 0x8055058C */

#define NP_COMM2_ERR_INVALID_ARG	-2141909245	/* 0x80550F03 */
#define NP_COMM2_ERR_ABORTED		-2141909232	/* 0x80550F10 */
#define NP_COMM2_ERR_SIGNED_OUT		-2141909134	/* 0x80550F72 */
#define NP_COMM2_ERR_NO_PLUGIN		-2141909113	/* 0x80550F87 */
#define NP_COMM2_ERR_INVALID_OP		-2141909100	/* 0x80550F94 */
#define NP_COMM2_ERR_ALREADY_INIT	-2141909056	/* 0x80550FC0 */
#define NP_COMM2_ERR_BUF_TOO_SMALL	-2141909055	/* 0x80550FC1 */
#define NP_COMM2_ERR_NOT_FOUND		-2141909054	/* 0x80550FC2 */
#define NP_COMM2_ERR_TABLE_FULL		-2141909053	/* 0x80550FC3 */
#define NP_COMM2_ERR_NO_MEMORY		-2141909052	/* 0x80550FC4 */
#define NP_COMM2_ERR_QUEUE_EMPTY	-2141909051	/* 0x80550FC5 */
#define NP_COMM2_ERR_QUEUE_FULL		-2141909050	/* 0x80550FC6 */

#define IPMI_ERR_UNSUPPORTED_METHOD	-2147351103	/* 0x800205C1 */

/* Cross-file NP IPC helpers */
extern int shellSvcNotifyEvent(IPMI::Session *session, int eventId);	/* 0x811B7560 */
extern int ipmiAsyncNotSupported(void);					/* 0x8115B554 */

/* NetCtl process refcount exports (implemented in netctl_api.cpp) */
extern "C" {
extern int SceShell_6B8772A2(SceUID pid);
extern int SceShell_794BF5BE(unsigned int token);
extern int sceAppMgrAppParamGetString(SceUID pid, int param, char *buf, SceSize size);
extern int sceAppMgrGetStatusById(SceUID pid, void *statusBuf);
extern int sceKernelGetOpenPsId(void *openPsId);
extern int vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *outVersion);
extern int SceNpCommon_A0C5C10E(const char *src, SceSize size);
extern void SceNpCommonPs4_F50BD96A(int printed);
extern int sceClibPrintf(const char *fmt, ...);
extern SceInt64 sceClibStrtoll(const char *str, char **endptr, int base);
extern SceUInt64 sceNetHtonll(SceUInt64 host64);
extern SceUInt16 sceNetHtons(SceUInt16 host16);
extern SceUInt16 sceNetNtohs(SceUInt16 net16);
extern int sceNetInetNtop(int af, const void *src, char *dst, SceSize size);
extern int sceNetInetPton(int af, const char *src, void *dst);

/* bXCe DOM / attribute list C exports */
extern int ScebXCe_FAB50608(BxceDocument *doc, SceUInt8 isDom);
extern int ScebXCe_B0891B13(BxceDocument *doc);
extern BxceNodeId ScebXCe_75E1B095(const BxceDocument *doc);
extern BxceNodeId ScebXCe_CED6BECF(const BxceDocument *doc);
extern BxceNodeId ScebXCe_FEA6E4A1(const BxceDocument *doc, BxceNodeId node);
extern BxceNodeId ScebXCe_6FC9DC3F(const BxceDocument *doc, BxceNodeId node);
extern int ScebXCe_FA18A039(const BxceDocument *doc, BxceNodeId node);
extern const char *ScebXCe_1189A2B1(const BxceDocument *doc, BxceNodeId node);
extern const char *ScebXCe_33825EFF(const BxceDocument *doc, BxceNodeId node);
extern const char *ScebXCe_753D9382(const BxceDocument *doc, BxceNodeId node, const char *name);
extern BxceNodeList *ScebXCe_1FA24DC0(const BxceDocument *doc, BxceNodeId node, const char *name);
extern BxceNodeId ScebXCe_AC0E4F67(BxceDocument *doc, BxceNodeId parent, const char *name,
				   const BxceAttributeList *attrs, const char *text, SceUInt32 textLen);
extern BxceNodeId ScebXCe_6C46D786(const BxceNodeList *list, SceUInt32 index);
extern void ScebXCe_1F59F0C4(BxceNodeList *list);
extern void ScebXCe_EC560647(void *ptr);
extern int ScebXCe_E7C8B20D(BxceAttributeList *list);
extern int ScebXCe_9C2F9897(BxceAttributeList *list);
extern int ScebXCe_3DD761CF(BxceAttributeList *list, const char *name, const char *value);
}

/* Shared NP RT stanza helpers in jga_msg_server.cpp (0x8121DCDC..0x8121DF68) */
extern int npRtCreateStanza(const NpRtJidPair *npIds, const char *stanzaId, int stanzaType,
			    NpRtStanza **outStanza);
extern int npRtSendIq(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId,
		      NpRtIqResponseCallback callback, void *userArg, int *outTransactionId);
extern int npRtSendIqWithTimeout(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId,
				 int unusedFlag, int timeoutMs, int retryCount,
				 NpRtIqResponseCallback callback, void *userArg,
				 int *outTransactionId);
extern int npRtSendResult(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId);
extern int npRtSendError(const SceNpId *peerNpId, int handlerId, const NpXmppErrorStanza *errStanza);

/* Shared NpAuthServer ticket reader (0x8121E41C) */
extern int npAuthServerReadTicket(IPMI::Session *session, int requestId, void *outBuf,
				  SceSize offset, SceSize readSize);

/* Shared NpManager server accessors & response senders (npmanager_ipc_server.cpp) */
extern int npManagerResolveRequestHandle(int clientReqId, SceUID clientPid);
extern NpJobQueue *npManagerGetJobQueue(void);
extern NpIpcLwMutex *npManagerGetIpcMutex(void);
extern int npManagerSendGetNpIdResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const SceNpId *npId);
extern int npManagerSendGetNpIdForceResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					     const SceNpId *npId);
extern int npManagerSendGetTicketResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					  const SceSize *totalSize, const SceSize *copySize,
					  const void *ticketBuf);
extern int npManagerSendGetTicketForceResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					       const SceSize *totalSize, const SceSize *copySize,
					       const void *ticketBuf);
extern int npManagerSendGetTicketCachedResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
						const SceSize *totalSize, const SceSize *copySize,
						const void *ticketBuf);
extern int npManagerSendGetAccountInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					       const void *profileBuf, const void *accountMetaBuf);
extern int npManagerSendUpdateAccountResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result);
extern int npManagerSendCheckSysUpdateResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					       const int *outStatus);
extern int npManagerSendGetProfileResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					   const void *profileBuf);
extern int npManagerSendUpdateProfileResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					      const void *profileBuf, const int *outStatus,
					      const int *outFlags, const void *outStamp);
extern int npManagerSendGetAvatarListResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					      const SceUInt32 *outCount, const void *entries,
					      const int *outTotal);
extern int npManagerSendGetAvatarUrlsResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					      const SceUInt32 *outCount, const int *avatarIds,
					      const void *urlEntries, const int *outTotal);
extern int npManagerSendGetPanelCategoriesResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
						   const SceUInt32 *outCount, const void *entries,
						   const int *outTotal);
extern int npManagerSendGetPanelUrlsResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					     const SceUInt32 *outCount, const int *panelIds,
					     const int *bgColors, const void *urlEntries,
					     const int *outTotal);
extern int npManagerSendGetCatalogListResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					       const SceUInt32 *outCount, const void *entries,
					       const int *outTotal);
extern int npManagerSendGetAvatarBatchResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					       const SceUInt32 *outCount, const int *avatarIds,
					       const void *urlEntries, const SceUInt32 *outExtraCount,
					       const int *extraIds, const int *outTotal,
					       const int *outNextOffset);
extern int npManagerSendGetAvatarUrlResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					     const void *urlBuf);
extern int npManagerSendGetTicketAvatarBatchResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						     int result, const SceUInt32 *outCount,
						     const int *avatarIds, const void *urlEntries,
						     const SceUInt32 *outExtraCount,
						     const int *extraIds, const int *outTotal,
						     const int *outNextOffset);
extern int npManagerSendGetTicketAvatarUrlResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						   int result, const void *urlBuf);
extern int npManagerSendOAuthGetAccessTokenResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						    int result, const void *tokenBuf);
extern int npManagerSendOAuthGetAuthCodeResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						 int result, const SceNpAuthorizationCode *authCode,
						 const int *issuerId);
extern int npManagerSendOAuthGetAuthCodeWithRedirectResponse(IPMI::Session *session,
							     SceUInt32 ipcReqId, int result,
							     const SceNpAuthorizationCode *authCode,
							     const int *issuerId);
extern void npDebugShowNotice(const char *tag, const char *detail, const char *noticeName);
extern void npDebugShowRawNotice(const char *message, const char *noticeName);

/* NpManager job enqueue entrypoints (npmanager_job.cpp) */
extern int npManagerEnqueueGetNpIdJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId);
extern int npManagerEnqueueGetNpIdForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId);
extern int npManagerEnqueueGetTicketJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					SceSize bufferSize);
extern int npManagerEnqueueGetTicketForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					     SceSize bufferSize);
extern int npManagerEnqueueGetTicketCachedJob(IPMI::Session *session, SceUInt32 ipcReqId,
					      int clientReqId, SceSize bufferSize);
extern int npManagerEnqueueGetAccountInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int clientReqId, int wantProfile, int wantMeta);
extern int npManagerEnqueueUpdateAccountJob(IPMI::Session *session, SceUInt32 ipcReqId,
					    int clientReqId, const void *loginId,
					    const void *password);
extern int npManagerEnqueueCheckSysUpdateJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int clientReqId, int checkType, int checkMode,
					     int checkFlag);
extern int npManagerEnqueueOAuthGetAccessTokenJob(IPMI::Session *session, SceUInt32 ipcReqId,
						  int clientReqId);
extern int npManagerEnqueueOAuthGetAuthCodeJob(IPMI::Session *session, SceUInt32 ipcReqId,
					       int clientReqId, const SceNpClientId *clientId,
					       const char *scope);
extern int npManagerEnqueueOAuthGetAuthCodeWithRedirectJob(IPMI::Session *session, SceUInt32 ipcReqId,
							   int clientReqId,
							   const SceNpClientId *clientId,
							   const char *scope,
							   const char *redirectUri);
extern int npManagerEnqueueGetProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					 const void *npIdOrJid, int langCode);
extern int npManagerEnqueueUpdateProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					    const void *aboutMe, int aboutMeAbsent,
					    const void *avatarUrl, int avatarUrlAbsent,
					    const void *languages, int languagesAbsent,
					    const int *panelColor, int panelColorAbsent,
					    const void *panelId, int panelIdAbsent,
					    int countryCode, const int *privacyFlag,
					    int privacyFlagAbsent, const void *commId,
					    int commIdAbsent, int avatarId, const void *customBuf,
					    SceSize customSize);
extern int npManagerEnqueueGetAvatarListJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					    int categoryId, int startOffset, SceUInt32 maxCount);
extern int npManagerEnqueueGetAvatarUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					    const int *categoryIdPtr, int startOffset, int sortMode,
					    SceUInt32 maxCount);
extern int npManagerEnqueueGetPanelCategoriesJob(IPMI::Session *session, SceUInt32 ipcReqId,
						 int clientReqId, int startOffset, int sortMode,
						 SceUInt32 maxCount);
extern int npManagerEnqueueGetPanelUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					   const int *categoryIdPtr, int startOffset, int sortMode,
					   SceUInt32 maxCount);
extern int npManagerEnqueueGetCatalogListJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int clientReqId, int categoryId, int startOffset,
					     SceUInt32 maxCount);
extern int npManagerEnqueueGetAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId,
					     int clientReqId, const void *ticketBuf,
					     SceSize ticketSize, const int *categoryIdPtr,
					     int startOffset, int sortMode, SceUInt32 maxCount,
					     int skipExtraIds);
extern int npManagerEnqueueGetAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
					   const void *ticketBuf, SceSize ticketSize, int avatarId,
					   int sizeType);
extern int npManagerEnqueueGetTicketAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId,
						   int clientReqId, int authReqId, SceSize ticketSize,
						   const int *categoryIdPtr, int startOffset,
						   int sortMode, SceUInt32 maxCount, int skipExtraIds);
extern int npManagerEnqueueGetTicketAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId,
						 int clientReqId, int authReqId, SceSize ticketSize,
						 int avatarId, int sizeType);

/* Lower-layer NP service functions (implemented in later Step 5 NP service files) */
extern int npImpsRawMsgRegisterChannel(int channelId, const NpImpsRawMsgServerCallbacks *callbacks);	/* 0x812F11E4 */
extern int npImpsRawMsgUnregisterChannel(int channelId);						/* 0x812F1208 */
extern int npImpsRawMsgSend(IPMI::Session *session, const void *msgBuf, SceSize msgSize, int mode);	/* 0x812F122A */

extern void *npRtHeapAlloc(SceSize size, void *customAlloc);						/* 0x813126E6 */
extern void npRtHeapFree(void *ptr, void *customAlloc);							/* 0x81312748 */
extern int npRtRegisterHandler(const NpRtHandlerConfig *config, int *outHandlerId);			/* 0x813109DE */
extern int npRtUnregisterHandler(int handlerId);							/* 0x81310A14 */
extern int npRtGetSessionByNpId(const SceNpId *npId, int *outSessionHandle);				/* 0x81310C5E */
extern int npRtAllocTransactionId(int sessionHandle, int *outTransactionId);				/* 0x81310CA2 */
extern int npRtSendIqRequest(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
			     NpRtIqResponseCallback callback, void *userArg);				/* 0x81310CE6 */
extern int npRtSendIqRequestEx(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
			       int timeoutMs, int retryCount, NpRtIqResponseCallback callback,
			       void *userArg);								/* 0x81310D64 */
extern int npRtSendIqResult(int sessionHandle, int handlerId, NpRtStanza *stanza);			/* 0x81310ED0 */
extern int npRtSendIqError(int sessionHandle, int handlerId, const NpXmppErrorStanza *errStanza);	/* 0x81310F22 */
extern int npRtParseXmppError(NpXmppErrorStanza *outError, const NpRtStanza *stanza);			/* 0x8130F766 */
extern void npRtReleaseStanza(NpRtStanza *stanza);							/* 0x8130F9C6 */
extern int npRtAllocStanza(NpRtStanza **outStanza, const NpRtStanzaInit *init);				/* 0x8130FA1E */

/* NpAuth internal service (0x81324874..0x81324D76) */
extern int npAuthCoreAbortRequest(int reqId);								/* 0x81324874 */
extern int npAuthCoreDestroyRequest(int reqId);								/* 0x81324972 */
extern int npAuthCoreCreateAsyncRequest(const NpAuthInternalParam *param);				/* 0x81324D62 */
extern int npAuthCoreCreateAsyncRequestEx(const NpAuthInternalParam *param);				/* 0x81324D6C */
extern int npAuthCoreGetTicket(int reqId, void *outBuf, SceSize bufSize);				/* 0x81324D76 */

/* NP LW Mutex & JobQueue runtime (0x812EAF70..0x812EB0D0, 0x81325EC4..0x81326562, 0x81341444) */
extern void npLwMutexZero(NpIpcLwMutex *mutex);								/* 0x812EAF70 */
extern void npLwMutexDestroy(NpIpcLwMutex *mutex);							/* 0x812EAF80 */
extern void npLwMutexDtor(NpIpcLwMutex *mutex);								/* 0x812EAFCE */
extern void npLwMutexLock(NpIpcLwMutex *mutex);								/* 0x812EB094 */
extern void npLwMutexUnlock(NpIpcLwMutex *mutex);							/* 0x812EB0D0 */
extern int npLwMutexCreate(NpIpcLwMutex *mutex, const char *name, int recursive);			/* 0x81326562 */

extern NpJobQueue *npJobQueueConstruct(void *mem);							/* 0x81325FE0 */
extern int npJobQueueInit(NpJobQueue *queue, const char *name, SceUInt32 attr, SceSize stackSize);	/* 0x81326144 */
extern int npJobQueueStart(NpJobQueue *queue);								/* 0x81341444 */
extern int npJobQueueEnqueue(NpJobQueue *queue, int reqHandle, NpJobBase *job);				/* 0x813261E6 */
extern int npJobQueueCancel(NpJobQueue *queue, int reqHandle, int cancelError);				/* 0x8132628C */

/* NP Manager core service (0x813268BC..0x8132A378, 0x8132DA12..0x8132F16A, 0x8136408C..0x813A96E6) */
extern int npAccountCreateRequest(void);								/* 0x813268BC */
extern int npAccountDestroyRequest(int reqHandle);							/* 0x813268DC */
extern int npAccountGetTicket(int reqHandle, void *ticketBuf, SceSize *ioSize);				/* 0x81326906 */
extern int npAccountGetTicketCached(int reqHandle, void *ticketBuf, SceSize *ioSize);			/* 0x81326976 */
extern int npAccountGetTicketForce(int reqHandle, void *ticketBuf, SceSize *ioSize);			/* 0x813269AC */
extern int npAccountAbortRequest(int reqHandle);							/* 0x813269E2 */
extern int npAccountGetNpId(int reqHandle, SceNpId *outNpId);						/* 0x81326A0C */
extern int npAccountGetNpIdForce(int reqHandle, SceNpId *outNpId);					/* 0x81326A92 */
extern int npAccountGetProfileAndMeta(int reqHandle, void *profileBuf, void *metaBuf);			/* 0x81326B18 */
extern int npAccountUpdateCredentials(int reqHandle, const void *loginId, const void *password);	/* 0x81326BDE */
extern int npAccountClearSession(void);									/* 0x81326CDC */
extern int npAccountIsSignedIn(void);									/* 0x81326D30 */
extern int npAccountSetCredentials(SceUInt64 accountId, const void *loginId, const void *password);	/* 0x81326D38 */
extern int npAccountGetOnlineId(void *outOnlineId);							/* 0x81326D40 */
extern int npAccountClearTicketCache(void);								/* 0x81326D6C */
extern int npAccountGetCachedAccountInfo(void *outInfo);						/* 0x81326D94 */
extern int npAccountHasCachedTicket(void);								/* 0x81326D9C */
extern int npAccountCopyCachedTicket(void *outBuf, SceSize *ioSize);					/* 0x81326DB0 */
extern int npAccountCheckTicketValid(void *outTicket, SceSize *outSize);				/* 0x81326DFA */
extern int npAccountGetRegion(SceNpCountryCode *countryCode, int *languageCode);			/* 0x81326E1E */
extern int npAccountGetRegionAndLocale(SceNpCountryCode *countryCode, char *localeStr);			/* 0x81326E96 */
extern int npAccountGetContentRating(int *isRestricted, int *age);					/* 0x81326F12 */
extern int npAccountGetChatRestriction(int *isRestricted);						/* 0x81326F8A */
extern int npAccountReadRegistryCredentials(void *loginId, void *password, SceUInt64 *accountId);	/* 0x81326FF4 */
extern int npAccountWriteRegistryCredentials(const void *loginId, const void *password,
					     const SceUInt64 *accountId);				/* 0x813270D8 */
extern int npAccountWriteRegistryCredentialsEx(const void *loginId, const void *password,
					       const SceUInt64 *accountId, const char *onlineId);	/* 0x81327226 */
extern int npAccountGetTicketVersion(void);								/* 0x813273AA */
extern int npAccountGetCachedNpId(SceNpId *outNpId);							/* 0x813273B6 */
extern int npAccountGetMemoryCredentials(void *loginId, void *password, SceUInt64 *accountId);		/* 0x813273E0 */
extern int npAccountResetState(void);									/* 0x81327434 */
extern int npAccountRegisterCallback(int (*cb)(int eventId, IPMI::Session *session),
				     IPMI::Session *session);						/* 0x8132781C */
extern int npAccountUnregisterCallback(int callbackId);							/* 0x81327896 */
extern int npSignInDialogCleanup(void);									/* 0x81328FAA */
extern int npSignInDialogStartModeA(int *outReqId, int dialogParam, SceUID pid);			/* 0x8132932C */
extern int npSignInDialogStartModeB(int *outReqId, int dialogParam, SceUID pid);			/* 0x81329338 */
extern int npSignInDialogStartModeC(int *outReqId, int dialogParam, SceUID pid);			/* 0x81329344 */
extern int npSignInDialogGetStatus(int dialogReqId);							/* 0x81329350 */
extern int npSignInDialogAbort(int dialogReqId);							/* 0x813293AE */
extern int npSignInDialogGetResult(int dialogReqId, int *outResult);					/* 0x8132945E */

extern int npCommonServiceIsSignedIn(int *outSignedIn);							/* 0x81329E20 */
extern int npCommonServiceCompareAccountId(SceUInt64 accountId);					/* 0x81329E4A */
extern int npCommonServiceGetSettings(void *outSettings);						/* 0x81329E64 */
extern int npCommonServiceGetPrivacyConfirm(int *outConfirmed);						/* 0x81329FAC */
extern int npCommonServiceSetPrivacyConfirm(int confirmed);						/* 0x8132A016 */

extern int npProfileCreateRequest(const void *onlineId, int reservedFlag, const char *optName);		/* 0x8132A252 */
extern int npProfileAbortRequest(int reqHandle);							/* 0x8132A322 */
extern int npProfileDestroyRequest(int reqHandle);							/* 0x8132A378 */
extern int npProfileGetProfile(int reqHandle, const void *npIdOrJid, int langCode, void *outProfile);	/* 0x8132DA12 */
extern int npProfileUpdateProfile(int reqHandle, const void *aboutMe, const void *avatarUrl,
				  const void *languages, const int *panelColor, const void *panelId,
				  int countryCode, const int *privacyFlag, const void *commId,
				  int avatarId, void *outProfile, int *outStatus, int *outFlags,
				  void *outStamp, const void *customBuf, SceSize customSize);		/* 0x8132DCA2 */
extern int npProfileGetAvatarList(int reqHandle, int categoryId, int startOffset, void *outEntries,
				  SceUInt32 maxCount, int *outTotal);					/* 0x8132E9A8 */
extern int npProfileGetCatalogList(int reqHandle, int categoryId, int startOffset, void *outEntries,
				   SceUInt32 maxCount, int *outTotal);					/* 0x8132E9C0 */
extern int npProfileGetAvatarUrls(int reqHandle, const int *categoryIdPtr, int startOffset,
				  int sortMode, int *outIds, void *outUrls, SceUInt32 maxCount,
				  int *outTotal);							/* 0x8132ECCE */
extern int npProfileGetAvatarBatch(int reqHandle, const void *ticketBuf, SceSize ticketSize,
				   const int *categoryIdPtr, int startOffset, int sortMode,
				   int *outIds, void *outUrls, int *outExtraIds, SceUInt32 maxCount,
				   int *outTotal, int *outNextOffset);					/* 0x8132ED02 */
extern int npProfileGetAvatarUrl(int reqHandle, const void *ticketBuf, SceSize ticketSize,
				 int avatarId, int sizeType, void *outUrl);				/* 0x8132ED42 */
extern int npProfileGetPanelCategories(int reqHandle, int startOffset, int sortMode, void *outEntries,
				       SceUInt32 maxCount, int *outTotal);				/* 0x8132EEB4 */
extern int npProfileGetPanelUrls(int reqHandle, const int *categoryIdPtr, int startOffset,
				 int sortMode, int *outIds, int *outBgColors, void *outUrls,
				 SceUInt32 maxCount, int *outTotal);					/* 0x8132F16A */

extern int npSysUpdateCreateRequest(int *outReqHandle);							/* 0x8136408C */
extern int npSysUpdateDestroyRequest(int reqHandle);							/* 0x813641AC */
extern int npSysUpdateCheck(int reqHandle, int checkType, int *outStatus, int checkMode,
			    int checkFlag);								/* 0x8136422C */
extern int npSysUpdateAbortRequest(int reqHandle);							/* 0x813643F4 */

extern int npOAuthCreateRequest(int *outReqHandle);							/* 0x813A74BE */
extern int npOAuthDestroyRequest(int reqHandle);							/* 0x813A7530 */
extern int npOAuthAbortRequest(int reqHandle);								/* 0x813A7596 */
extern int npOAuthGetAuthorizationCode(int reqHandle, const NpOAuthAuthCodeRequest *req,
				       SceNpAuthorizationCode *outCode, int *outIssuerId);		/* 0x813A89E4 */
extern int npOAuthGetAccessToken(int reqHandle, void *outToken);					/* 0x813A8F90 */
extern int npOAuthClearAccessTokenCache(void);								/* 0x813A9598 */
extern int npOAuthGetCachedAccessToken(void *outToken, bool *outValid);					/* 0x813A95C6 */
extern int npOAuthGetAuthorizationCodeWithRedirect(int reqHandle,
						   const NpOAuthAuthCodeWithRedirectRequest *req,
						   SceNpAuthorizationCode *outCode, int *outIssuerId);	/* 0x813A96E6 */

/* Crypto helpers for device token & account hash (0x81417F9C, 0x8144A2C0..0x8144A482) */
extern int shellSha256Digest(const void *input, SceSize inputSize, void *outDigest);			/* 0x81417F9C */
extern int shellDesSetKey(SceUInt32 *subkeys, SceUInt32 reserved, SceUInt32 keyLo, SceUInt32 keyHi);	/* 0x8144A482 */
extern SceUInt64 shellDesEncryptBlock(const SceUInt32 *subkeys, SceUInt32 reserved,
				      SceUInt32 blockLo, SceUInt32 blockHi);				/* 0x8144A2C0 */
extern SceUInt64 shellDesDecryptBlock(const SceUInt32 *subkeys, SceUInt32 reserved,
				      SceUInt32 blockLo, SceUInt32 blockHi);				/* 0x8144A30A */

/* Commerce2 lower-layer session/HTTP helpers (0x812BD5D4..0x812BDAA4, 0x8132DA00..0x8132DA08) */
#include "../commerce2/np_commerce2_core.h"

#endif /* SHELL_NP_IPC_INTERNAL_H */
