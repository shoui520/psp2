/*
 * netctl_services.h: internal types and interfaces shared across SceNetCtl,
 * SceNetStun, SceNetUpnp, and SceNetWispr inside SceShell.
 */
#ifndef SHELL_NETCTL_SERVICES_H
#define SHELL_NETCTL_SERVICES_H

#include <scetypes.h>
#include <net.h>
#include <libnetctl.h>
#include "../../shell.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SceClibMspace;

/* libnetctl UPnP IGD port-mapping helpers (sys/internal/usermode/src/libnetctl/upnp.c) */
int SceNetCtl_743E9FCE(void *upnpCtx);
int SceNetCtl_9CB6FEE1(void *upnpCtx, int protocol, SceUInt16 internalPort,
		       SceUInt16 externalPort, char **outExternalIp,
		       int *outStatus, int *outResult);
void SceNetCtl_9B678FEE(void *upnpCtx);
int SceNetCtl_68C081D1(void *upnpCtx);

/* SceHttp / SceHttps internal options */
int sceHttpsDisableOptionPrivate(SceInt32 tmplId, SceUInt32 sslFlags);
int sceHttpSetIcmOption(SceInt32 reqId, SceInt32 option);

/* SceErrorHistory */
int sceErrorHistorySetDefaultFormat(const void *format);

/* SceVshBridge */
int vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const void *inBuf, void *outBuf);

/* Statically linked HMAC-SHA1 and MD5 helpers in SceShell */
int sceHmacSha1Digest(const void *key, SceSize keyLen, const void *data, SceSize dataLen,
			void *outDigest);
int sceMd5Digest(const void *data, SceSize dataLen, void *outDigest);

/* ---------- SceNetStun (0x812181CC..0x8121A038) ---------- */

#define NET_STUN_ERROR_ALREADY_INITIALIZED	-2143214591	/* 0x80412401 */
#define NET_STUN_ERROR_NOT_INITIALIZED		-2143214590	/* 0x80412402 */
#define NET_STUN_ERROR_TIMEOUT			-2143214589	/* 0x80412403 */
#define NET_STUN_ERROR_INVALID_PACKET		-2143214588	/* 0x80412404 */
#define NET_STUN_ERROR_MISSING_ATTR		-2143214587	/* 0x80412405 */
#define NET_STUN_ERROR_BUFFER_TOO_SMALL		-2143214586	/* 0x80412406 */
#define NET_STUN_ERROR_INTEGRITY_MISMATCH	-2143214585	/* 0x80412407 */
#define NET_STUN_ERROR_NO_FREE_SLOT		-2143214583	/* 0x80412409 */
#define NET_STUN_ERROR_INVALID_PARAM		-2143214582	/* 0x8041240A */
#define NET_STUN_ERROR_EXT_ATTR_LEN_MISMATCH	-2143214581	/* 0x8041240B */
#define NET_STUN_ERROR_EXT_ATTR_MISMATCH	-2143214580	/* 0x8041240C */
#define NET_STUN_ERROR_NO_MEMORY		-2143214579	/* 0x8041240D */
#define NET_STUN_ERROR_INVALID_HOST		-2143214578	/* 0x8041240E */
#define NET_STUN_ERROR_RESOLVER_CREATE		-2143214577	/* 0x8041240F */
#define NET_STUN_ERROR_MSPACE_CREATE		-2143214576	/* 0x80412410 */
#define NET_STUN_ERROR_UPNP_PORT_MAP_FAILED	-2143214574	/* 0x80412412 */

#define NET_STUN_EVENT_NAT_TYPE_DONE		1
#define NET_STUN_EVENT_MAPPED_ADDR_DONE		2
#define NET_STUN_EVENT_ABORTED			4
#define NET_STUN_EVENT_MAPPED_ADDR_NOTIFY	5
#define NET_STUN_EVENT_FILTER_CHECK_DONE	6

#define NET_STUN_UPNP_ENABLE			0x1u
#define NET_STUN_UPNP_STRICT			0x2u

typedef void (*NetStunCallback)(int stunId, void *userArg, int eventOrError, const void *result);

typedef struct NetStunOption {
	int requestXorMapped;		/* +0x00 */
	SceUInt32 username;		/* +0x04 */
	SceUInt8 hmacKey[20];		/* +0x08 */
	int hasCustomAttr;		/* +0x1C */
	const void *customAttrData;	/* +0x20 */
	SceSize customAttrSize;		/* +0x24 */
	SceUInt32 upnpFlags;		/* +0x28 */
} NetStunOption;

typedef struct NetStunMappedResult {
	SceNetInAddr mappedAddr;	/* +0x00 */
	SceUInt16 mappedPort;		/* +0x04 */
	SceUInt16 pad;			/* +0x06 */
	int upnpStatus;			/* +0x08 */
	SceNetInAddr upnpExternalAddr;	/* +0x0C */
} NetStunMappedResult;

typedef struct NetStunNatResult {
	SceNetInAddr mappedAddr;	/* +0x00 */
	int natMappingType;		/* +0x04: 0=cone, 1=addr-dep, 2=port-dep */
	int portPreservation;		/* +0x08: 1 if mapped port == local port */
	int portDelta;			/* +0x0C */
	int upnpResult;			/* +0x10 */
	SceNetInAddr upnpExternalAddr;	/* +0x14 */
} NetStunNatResult;

typedef struct NetStunErrorDetail {
	int upnpStatus;			/* +0x00 */
	int upnpResult;			/* +0x04 */
	SceNetInAddr upnpExternalAddr;	/* +0x08 */
} NetStunErrorDetail;

typedef struct NetStunFilterResult {
	int filterBehavior;		/* +0x00: 0=endpoint-indep, 1=addr-dep, 2=addr+port-dep */
	int hairpinSupported;		/* +0x04 */
	int reservedHead;		/* +0x08 */
	int reservedTail;		/* +0x0C */
} NetStunFilterResult;

int netStunCreate(int threadPriority, SceUInt32 cpuAffinity, const char *serverHost,
		  SceUInt16 serverPort, const NetStunOption *option,
		  NetStunCallback callback, int *outId);
int netStunDestroy(int stunId);
int netStunStartMappedAddrCheck(int stunId, SceUInt16 localPort, void *userArg);
int netStunStartNatTypeCheckOnSocket(int stunId, int sockId, void *userArg);
int netStunAbort(int stunId, void *userArg);

/* ---------- SceNetUpnp (0x8121A038..0x8121ADB0) ---------- */

#define NET_UPNP_ERROR_NOT_INITIALIZED		-2143214335	/* 0x80412501 */
#define NET_UPNP_ERROR_ALREADY_INITIALIZED	-2143214334	/* 0x80412502 */
#define NET_UPNP_ERROR_ALREADY_STARTED		-2143214333	/* 0x80412503 */
#define NET_UPNP_ERROR_INVALID_PARAM		-2143214331	/* 0x80412505 */

typedef struct NetUpnpNatInfo {
	int stunStatus;			/* +0x00 */
	int detailedStatus;		/* +0x04 */
	int natType;			/* +0x08 */
	int historyNatType;		/* +0x0C */
	NetStunNatResult stunResult;	/* +0x10..0x27 */
	int largePacketOk;		/* +0x28 */
	SceNetInAddr mappedAddr;	/* +0x2C */
	int upnpStatus;			/* +0x30 */
} NetUpnpNatInfo;

typedef struct NetCtlNatDetailInfo {
	SceSize size;			/* +0x00 (40) */
	int stunStatus;			/* +0x04 */
	int detailedStatus;		/* +0x08 */
	int natType;			/* +0x0C */
	int historyNatType;		/* +0x10 */
	SceNetInAddr mappedAddr;	/* +0x14 */
	int natMappingType;		/* +0x18 */
	int portPreservation;		/* +0x1C */
	int portDelta;			/* +0x20 */
	int largePacketOk;		/* +0x24 */
} NetCtlNatDetailInfo;

int netUpnpGetNatInfo(NetUpnpNatInfo *outInfo);
int netUpnpGetLastError(int *outError);
int netUpnpEnablePortCheck(void *ownerSession);
int netUpnpDisablePortCheck(void);
int netUpnpReleaseOwnerSession(void *session);

/* ---------- SceNetWispr (0x8121ADB0..0x8121CFE2) ---------- */

#define NET_WISPR_ERROR_NOT_INITIALIZED		-2143213823	/* 0x80412701 */
#define NET_WISPR_ERROR_ALREADY_INITIALIZED	-2143213822	/* 0x80412702 */
#define NET_WISPR_ERROR_BUSY			-2143213821	/* 0x80412703 */
#define NET_WISPR_ERROR_BUFFER_TOO_SMALL	-2143213820	/* 0x80412704 */
#define NET_WISPR_ERROR_NO_LOGOFF_URL		-2143213817	/* 0x80412707 */
#define NET_WISPR_ERROR_CREDENTIAL_INVALID	-2143213816	/* 0x80412708 */
#define NET_WISPR_ERROR_HTTP_STATUS		-2143213807	/* 0x80412711 */
#define NET_WISPR_ERROR_NO_XML_ROOT		-2143213806	/* 0x80412712 */
#define NET_WISPR_ERROR_AUTH_FAILED		-2143213805	/* 0x80412713 */
#define NET_WISPR_ERROR_NON_HTTPS_LOGIN_URL	-2143213804	/* 0x80412714 */
#define NET_WISPR_ERROR_MISSING_REDIRECT	-2143213801	/* 0x80412717 */
#define NET_WISPR_ERROR_MISSING_AUTH_REPLY	-2143213800	/* 0x80412718 */
#define NET_WISPR_ERROR_MSG_TYPE_REDIRECT	-2143213785	/* 0x80412727 */
#define NET_WISPR_ERROR_MSG_TYPE_PROXY		-2143213784	/* 0x80412728 */
#define NET_WISPR_ERROR_MSG_TYPE_AUTH_REPLY	-2143213783	/* 0x80412729 */
#define NET_WISPR_ERROR_MSG_TYPE_LOGOFF_REPLY	-2143213782	/* 0x8041272A */
#define NET_WISPR_ERROR_MSG_TYPE_AUTH_POLL	-2143213781	/* 0x8041272B */
#define NET_WISPR_ERROR_MSG_TYPE_ABORT_REPLY	-2143213780	/* 0x8041272C */
#define NET_WISPR_ERROR_MSG_TYPE_UNKNOWN	-2143213779	/* 0x8041272D */
#define NET_WISPR_ERROR_RESP_NO_ERROR		-2143213778	/* 0x8041272E */
#define NET_WISPR_ERROR_RESP_LOGIN_SUCCEEDED	-2143213777	/* 0x8041272F */
#define NET_WISPR_ERROR_RESP_LOGIN_FAILED	-2143213776	/* 0x80412730 */
#define NET_WISPR_ERROR_RESP_RADIUS_ERROR	-2143213775	/* 0x80412731 */
#define NET_WISPR_ERROR_RESP_NETWORK_ADMIN	-2143213774	/* 0x80412732 */
#define NET_WISPR_ERROR_RESP_LOGOFF_SUCCEEDED	-2143213773	/* 0x80412733 */
#define NET_WISPR_ERROR_RESP_LOGIN_ABORTED	-2143213772	/* 0x80412734 */
#define NET_WISPR_ERROR_RESP_PROXY_RETRY	-2143213771	/* 0x80412735 */
#define NET_WISPR_ERROR_RESP_AUTH_PENDING	-2143213770	/* 0x80412736 */
#define NET_WISPR_ERROR_RESP_GW_INTERNAL	-2143213769	/* 0x80412737 */
#define NET_WISPR_ERROR_RESP_OTHER		-2143213768	/* 0x80412738 */
#define NET_WISPR_ERROR_MISSING_LOGIN_URL	-2143213767	/* 0x80412739 */
#define NET_WISPR_ERROR_MISSING_MSG_TYPE	-2143213766	/* 0x8041273A */
#define NET_WISPR_ERROR_MISSING_RESP_CODE	-2143213765	/* 0x8041273B */
#define NET_WISPR_ERROR_MISSING_AUTH_SEED	-2143213764	/* 0x8041273C */
#define NET_WISPR_ERROR_XML_INVALID_ARG		-2143214176	/* 0x804125A0 */
#define NET_WISPR_ERROR_XML_NEED_MORE		-2143214174	/* 0x804125A2 */

typedef struct NetWisprResultInfo {
	int responseCode;		/* +0x00 */
	char replyMessage[256];		/* +0x04 */
	char loginUrl[256];		/* +0x104 */
	char logoffUrl[256];		/* +0x204 */
} NetWisprResultInfo;

#ifdef __cplusplus
}
#endif

#endif
