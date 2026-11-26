/*
 * net_dhcp.cpp: SceNetDhcp DHCPv4 + IPv4 Link-Local (RFC 3927) client daemon
 * statically linked into SceShell (0x813794FC..0x8137B3E7 + 0x81449D18).
 */
#include "net_config_internal.h"

#include <stdlib.h>

enum NetDhcpState {
	DHCP_STATE_INIT = 0,
	DHCP_STATE_SELECTING = 1,
	DHCP_STATE_REQUESTING = 2,
	DHCP_STATE_BOUND = 3,
	DHCP_STATE_RENEWING = 4,
	DHCP_STATE_REBINDING = 5
};

enum NetDhcpLinkLocalState {
	LL_STATE_NONE = 0,
	LL_STATE_PROBING = 1,
	LL_STATE_ASSIGNED = 2,
	LL_STATE_DISABLED = 3
};

enum NetDhcpCallbackEvent {
	DHCP_EV_BOUND = 0,
	DHCP_EV_STOPPED = 1,
	DHCP_EV_RENEWED = 2,
	DHCP_EV_RENEWING = 3,
	DHCP_EV_REBINDING = 4,
	DHCP_EV_EXPIRED = 5,
	DHCP_EV_LINKLOCAL = 6
};

/* DHCP start parameter flags */
#define DHCP_FLAG_USE_STATIC_DNS		0x01u
#define DHCP_FLAG_NO_ROUTE_OR_DNS		0x02u
#define DHCP_FLAG_NO_RELEASE_ON_STOP		0x04u
#define DHCP_FLAG_LINKLOCAL_ONLY		0x08u
#define DHCP_FLAG_ALLOW_LINKLOCAL_FALLBACK	0x10u
#define DHCP_FLAG_NO_VENDOR_CLASS		0x20u

/* Daemon state flags */
#define DHCP_DAEMON_INITIALIZED			0x00000001u
#define DHCP_DAEMON_IN_CALLBACK			0x00000002u
#define DHCP_DAEMON_STOP_REQUESTED		0x00000004u
#define DHCP_DAEMON_WAITING_EPOLL		0x00010000u

/* DHCP options and wire constants */
#define DHCP_OP_BOOTREQUEST			1
#define DHCP_OP_BOOTREPLY			2
#define DHCP_HTYPE_ETHER			1
#define DHCP_HLEN_ETHER				6
#define DHCP_SERVER_PORT			67
#define DHCP_CLIENT_PORT			68
#define DHCP_MIN_PACKET_LEN			300
#define DHCP_FIXED_HEADER_LEN			236
#define DHCP_MAX_RECV_LEN			2048

#define DHCP_MSG_DISCOVER			1
#define DHCP_MSG_OFFER				2
#define DHCP_MSG_REQUEST			3
#define DHCP_MSG_DECLINE			4
#define DHCP_MSG_ACK				5
#define DHCP_MSG_NAK				6
#define DHCP_MSG_RELEASE			7

#define DHCP_OPT_PAD				0
#define DHCP_OPT_SUBNET_MASK			1
#define DHCP_OPT_ROUTER				3
#define DHCP_OPT_DNS_SERVER			6
#define DHCP_OPT_HOSTNAME			12
#define DHCP_OPT_REQUESTED_IP			50
#define DHCP_OPT_LEASE_TIME			51
#define DHCP_OPT_MSG_TYPE			53
#define DHCP_OPT_SERVER_ID			54
#define DHCP_OPT_PARAM_REQ_LIST			55
#define DHCP_OPT_RENEWAL_TIME			58
#define DHCP_OPT_REBINDING_TIME			59
#define DHCP_OPT_VENDOR_CLASS			60
#define DHCP_OPT_CLIENT_ID			61
#define DHCP_OPT_END				255

#define DHCP_SOL_SOCKET				0xFFFF
#define DHCP_SO_BROADCAST			0x0020
#define DHCP_SO_REUSEADDR			0x0200
#define DHCP_SO_DONTROUTE			0x0800
#define DHCP_SO_NBIO				0x1101
#define DHCP_SO_ONESBCAST			0x8000
#define DHCP_SO_KERNEL_PRIV			0x10000
#define DHCP_IPPROTO_IP				0
#define DHCP_IP_MULTICAST_IF			9
#define DHCP_IP_MULTICAST_TTL			10

typedef void (*NetDhcpEventCallback)(void *userArg, int event, int err);
typedef void (*NetDhcpTimerCallback)(void *userArg);
typedef void (*NetDhcpWakeupCallback)(void);
typedef int (*NetDhcpSendRequestFunc)(struct NetDhcpClient *client, SceUInt32 requestedIp,
				      SceUInt32 serverIp, SceUInt32 xid);

typedef struct NetDhcpBuf {
	SceUInt8 *head;
	SceUInt8 *tail;
} NetDhcpBuf;

typedef struct NetDhcpTimer {
	struct NetDhcpTimer *next;
	struct NetDhcpTimer *prev;
	SceUInt64 expireUsec;
	NetDhcpTimerCallback callback;
	void *userArg;
} NetDhcpTimer;

typedef struct NetDhcpTimerQueue {
	NetDhcpTimer *activeHead;
	NetDhcpTimer *activeTail;
	NetDhcpTimer *freeHead;
	NetDhcpTimer *freeTail;
} NetDhcpTimerQueue;

typedef struct NetDhcpStartParam {
	SceNetInAddr staticDnsMain;		/* +0x00 */
	SceNetInAddr staticDnsAlt;		/* +0x04 */
	char hostname[256];			/* +0x08 */
	char vendorClass[16];			/* +0x108 */
	SceNetInAddr requestedIp;		/* +0x118 */
	SceUInt32 flags;			/* +0x11C */
} NetDhcpStartParam;

typedef struct NetDhcpOffer {
	SceUInt32 offeredIp;			/* +0x00 */
	SceUInt32 subnetMask;			/* +0x04 */
	SceUInt32 routerIp;			/* +0x08 */
	SceUInt32 dnsMain;			/* +0x0C */
	SceUInt32 dnsAlt;			/* +0x10 */
	SceUInt32 serverId;			/* +0x14 */
	SceUInt32 leaseTime;			/* +0x18 */
	SceUInt32 renewTime;			/* +0x1C */
	SceUInt32 rebindTime;			/* +0x20 */
} NetDhcpOffer;

typedef struct NetDhcpClient {
	struct NetDhcpClient *next;		/* +0x000 */
	struct NetDhcpClient *prev;		/* +0x004 */
	int ifId;				/* +0x008 */
	const char *hostnamePtr;		/* +0x00C */
	SceSize hostnameLen;			/* +0x010 */
	const char *vendorClassPtr;		/* +0x014 */
	SceSize vendorClassLen;			/* +0x018 */
	SceUInt32 activeIp;			/* +0x01C */
	SceUInt32 activeSubnetMask;		/* +0x020 */
	SceUInt32 activeRouterIp;		/* +0x024 */
	SceUInt32 activeDnsMain;		/* +0x028 */
	SceUInt32 activeDnsAlt;			/* +0x02C */
	SceUInt32 linkLocalIp;			/* +0x030 */
	SceUInt32 linkLocalMask;		/* +0x034 */
	SceNetEtherAddr macAddr;		/* +0x038 */
	SceUInt8 macPad[2];			/* +0x03E */
	SceUInt32 kernelIfIndex;		/* +0x040 */
	NetDhcpEventCallback callback;		/* +0x044 */
	void *callbackArg;			/* +0x048 */
	NetDhcpStartParam config;		/* +0x04C..+0x16B (288B) */
	int dhcpState;				/* +0x16C */
	int linkLocalState;			/* +0x170 */
	NetDhcpTimer *retryTimer;		/* +0x174 */
	NetDhcpTimer *renewTimer;		/* +0x178 */
	NetDhcpTimer *rebindTimer;		/* +0x17C */
	NetDhcpTimer *arpCheckTimer;		/* +0x180 */
	NetDhcpTimer *llProbeTimer;		/* +0x184 */
	NetDhcpOffer pendingOffer;		/* +0x188..+0x1AB (36B) */
	SceUInt32 xid;				/* +0x1AC */
	SceUInt32 boundServerId;		/* +0x1B0 */
	SceUInt32 boundLeaseTime;		/* +0x1B4 */
	SceUInt32 boundRenewTime;		/* +0x1B8 */
	SceUInt32 boundRebindTime;		/* +0x1BC */
	int requestRetryActive;			/* +0x1C0 */
	int requestRetryCount;			/* +0x1C4 */
	NetDhcpSendRequestFunc requestSender;	/* +0x1C8 */
	int discoverRetryRemaining;		/* +0x1CC */
	int renewAttempts;			/* +0x1D0 */
	int rebindAttempts;			/* +0x1D4 */
	int llProbesRemaining;			/* +0x1D8 */
	SceUInt32 routeFlags;			/* +0x1DC */
} NetDhcpClient;

typedef struct NetDhcpDaemon {
	NetDhcpClient *clientHead;		/* +0x00 */
	NetDhcpClient *clientTail;		/* +0x04 */
	NetDhcpTimerQueue timerQueue;		/* +0x08..+0x17 */
	int udpSock;				/* +0x18 */
	int arpSock;				/* +0x1C */
	int epollId;				/* +0x20 */
	SceUID threadId;			/* +0x24 */
	NetDhcpBuf *recvBuf;			/* +0x28 */
	SceUInt32 flags;			/* +0x2C */
} NetDhcpDaemon;

typedef struct NetDhcpWireHeader {
	SceUInt8 op;
	SceUInt8 htype;
	SceUInt8 hlen;
	SceUInt8 hops;
	SceUInt32 xid;
	SceUInt16 secs;
	SceUInt16 flags;
	SceUInt32 ciaddr;
	SceUInt32 yiaddr;
	SceUInt32 siaddr;
	SceUInt32 giaddr;
	SceUInt8 chaddr[16];
	SceUInt8 sname[64];
	SceUInt8 file[128];
} NetDhcpWireHeader;

typedef struct NetArpWirePacket {
	SceUInt16 hwType;
	SceUInt16 protoType;
	SceUInt8 hwLen;
	SceUInt8 protoLen;
	SceUInt16 opcode;
	SceUInt8 senderMac[6];
	SceUInt8 senderIp[4];
	SceUInt8 targetMac[6];
	SceUInt32 targetIp;
} NetArpWirePacket;

typedef struct NetArpProbeReq {
	char ifrName[16];
	SceUInt32 unusedWord;
	SceUInt32 senderIp;
	SceUInt32 fillerWords[3];
	SceUInt32 targetIp;
	SceUInt32 trailingWords[6];
} NetArpProbeReq;

static const SceUInt8 kDhcpEndOption = DHCP_OPT_END;
static const SceUInt8 kDhcpMagicCookie[4] = { 0x63, 0x82, 0x53, 0x63 };
static const SceUInt8 kDhcpOptMsgRequest[3] = { DHCP_OPT_MSG_TYPE, 1, DHCP_MSG_REQUEST };
static const SceUInt8 kDhcpOptParamList[6] = {
	DHCP_OPT_PARAM_REQ_LIST, 4,
	DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, 15, DHCP_OPT_DNS_SERVER
};
static const SceNetInAddr kZeroInAddr = { 0x00000000u };
static const SceNetInAddr kBroadcastInAddr = { 0xFFFFFFFFu };
static const SceUInt8 kDhcpOptMsgDiscover[3] = { DHCP_OPT_MSG_TYPE, 1, DHCP_MSG_DISCOVER };
static const SceUInt8 kDhcpOptMsgDecline[3] = { DHCP_OPT_MSG_TYPE, 1, DHCP_MSG_DECLINE };
static const SceUInt8 kDhcpOptMsgRelease[3] = { DHCP_OPT_MSG_TYPE, 1, DHCP_MSG_RELEASE };

static NetDhcpDaemon g_dhcpDaemon;
static NetThreadParam g_dhcpThreadParam;

static void dhcpWakeEpoll(void);
static void dhcpOnDiscoverOrExpire(void *userArg);
static void dhcpOnRenewTimer(void *userArg);
static void dhcpOnRebindTimer(void *userArg);
static void dhcpOnRequestRetryTimer(void *userArg);
static void dhcpOnLinkLocalProbeTimer(void *userArg);
static void dhcpOnArpCheckDone(void *userArg);

static SceUInt32 readBe32(const SceUInt8 *bytes)
{
	return ((SceUInt32)bytes[0] << 24) | ((SceUInt32)bytes[1] << 16) |
	       ((SceUInt32)bytes[2] << 8) | (SceUInt32)bytes[3];
}

static void writeBe32(SceUInt8 *bytes, SceUInt32 value)
{
	bytes[0] = (SceUInt8)(value >> 24);
	bytes[1] = (SceUInt8)(value >> 16);
	bytes[2] = (SceUInt8)(value >> 8);
	bytes[3] = (SceUInt8)value;
}

static NetDhcpBuf *dhcpBufAlloc(const void *initialBytes, SceSize size)
{
	NetDhcpBuf *buf = (NetDhcpBuf *)SceNetInternal_A2C2BC5F((int)(size + sizeof(NetDhcpBuf)), 4);
	if (buf == NULL)
		return NULL;

	SceUInt8 *payload = (SceUInt8 *)(buf + 1);
	buf->head = payload;
	buf->tail = payload;
	if (initialBytes != NULL) {
		sceClibMemcpy_safe(payload, initialBytes, size);
		buf->tail += size;
	}
	return buf;
}

static int dhcpBufFree(NetDhcpBuf *buf)
{
	SceNetInternal_E62A2FAA(buf);
	return 0;
}

static NetDhcpTimer *dhcpTimerCancel(NetDhcpTimerQueue *queue, NetDhcpTimer *timer)
{
	if (timer == NULL)
		return NULL;

	NetDhcpTimer *cursor = queue->activeHead;
	while (cursor != NULL) {
		if (cursor == timer) {
			if (timer->next != NULL)
				timer->next->prev = timer->prev;
			else
				queue->activeTail = timer->prev;

			if (timer->prev != NULL)
				timer->prev->next = timer->next;
			else
				queue->activeHead = timer->next;

			timer->prev = queue->freeTail;
			if (queue->freeTail != NULL)
				queue->freeTail->next = timer;
			else
				queue->freeHead = timer;
			timer->next = NULL;
			queue->freeTail = timer;
			return NULL;
		}
		cursor = cursor->next;
	}
	return NULL;
}

static int dhcpTimerProcess(NetDhcpTimerQueue *queue)
{
	SceUInt64 nowUsec = 0;

	SceNetInternal_2FEC8B74(&nowUsec);
	NetDhcpTimer *head = queue->activeHead;
	if (head == NULL)
		return -1;

	while (nowUsec + 1000ULL >= head->expireUsec) {
		if (head->next != NULL)
			head->next->prev = head->prev;
		else
			queue->activeTail = head->prev;

		if (head->prev != NULL)
			head->prev->next = head->next;
		else
			queue->activeHead = head->next;

		head->callback(head->userArg);

		head->prev = queue->freeTail;
		if (queue->freeTail != NULL)
			queue->freeTail->next = head;
		else
			queue->freeHead = head;
		head->next = NULL;
		queue->freeTail = head;

		head = queue->activeHead;
		if (head == NULL)
			return -1;
	}

	SceUInt64 remainUsec = head->expireUsec - nowUsec;
	if (remainUsec > 0x7FFFFFFFULL)
		remainUsec = 0x7FFFFFFFULL;
	return (int)((SceInt32)remainUsec / 1000);
}

static NetDhcpTimer *dhcpTimerAdd(NetDhcpTimerQueue *queue, NetDhcpTimerCallback callback,
				  void *userArg, SceUInt64 delayMs,
				  NetDhcpWakeupCallback wakeup)
{
	NetDhcpTimer *timer = queue->freeHead;
	if (timer != NULL) {
		if (timer->next != NULL)
			timer->next->prev = timer->prev;
		else
			queue->freeTail = timer->prev;

		if (timer->prev != NULL)
			timer->prev->next = timer->next;
		else
			queue->freeHead = timer->next;
	} else {
		timer = (NetDhcpTimer *)SceNetInternal_A2C2BC5F(sizeof(NetDhcpTimer), 4);
		if (timer == NULL)
			return NULL;
	}

	SceNetInternal_2FEC8B74(&timer->expireUsec);
	timer->callback = callback;
	timer->userArg = userArg;
	timer->expireUsec += delayMs * 1000ULL;

	NetDhcpTimer *cursor = queue->activeHead;
	int position = 0;
	while (cursor != NULL && timer->expireUsec >= cursor->expireUsec) {
		cursor = cursor->next;
		position++;
	}

	if (cursor == NULL) {
		timer->prev = queue->activeTail;
		if (queue->activeTail != NULL)
			queue->activeTail->next = timer;
		else
			queue->activeHead = timer;
		queue->activeTail = timer;
		timer->next = NULL;
	} else {
		timer->prev = cursor->prev;
		if (cursor->prev != NULL)
			cursor->prev->next = timer;
		else
			queue->activeHead = timer;
		timer->next = cursor;
		cursor->prev = timer;
	}

	if (position == 0 && wakeup != NULL)
		wakeup();
	return timer;
}

static int dhcpGetInterfaceHwInfo(int ifId, SceNetEtherAddr *outMac, SceUInt32 *outKernelIndex)
{
	NetIfInfoRecord ifInfo;
	int ret = SceNetInternal_235DE96C(ifId, &ifInfo, 1);

	if (ret < 0)
		return ret;
	if (outMac != NULL)
		sceClibMemcpy_safe(outMac, &ifInfo.hardwareAddress, sizeof(SceNetEtherAddr));
	if (outKernelIndex != NULL)
		*outKernelIndex = ifInfo.interfaceIndex;
	return 0;
}

static int dhcpTimerDestroyAll(NetDhcpTimerQueue *queue)
{
	NetDhcpTimer *cursor = queue->activeHead;
	while (cursor != NULL) {
		queue->activeHead = cursor->next;
		SceNetInternal_E62A2FAA(cursor);
		cursor = queue->activeHead;
	}
	queue->activeTail = NULL;

	cursor = queue->freeHead;
	while (cursor != NULL) {
		queue->freeHead = cursor->next;
		SceNetInternal_E62A2FAA(cursor);
		cursor = queue->freeHead;
	}
	queue->freeTail = NULL;
	return 0;
}

/* 0x81449D18: normalize lease / T1 / T2 timers according to RFC 2131 */
static int dhcpNormalizeLeaseTimers(const NetDhcpOffer *incoming, NetDhcpOffer *target)
{
	SceUInt32 leaseSec = incoming->leaseTime;
	SceUInt32 renewSec = incoming->renewTime;

	target->leaseTime = leaseSec;
	if ((leaseSec <= renewSec && renewSec != 0xFFFFFFFFu) || renewSec == 0)
		renewSec = leaseSec >> 1;
	target->renewTime = renewSec;

	SceUInt32 rebindSec = incoming->rebindTime;
	if ((rebindSec <= renewSec || leaseSec <= rebindSec) && rebindSec != 0xFFFFFFFFu)
		rebindSec = (renewSec >> 2) + 3u * (leaseSec >> 2);
	target->rebindTime = rebindSec;

	if (leaseSec == 0 || renewSec == 0 || rebindSec == 0)
		return -1;
	return 0;
}

static void dhcpResetClientLease(NetDhcpClient *client, int cancelArpAndLinkLocal)
{
	client->activeIp = 0;
	client->activeSubnetMask = 0;
	client->activeRouterIp = 0;
	client->activeDnsMain = 0;
	client->activeDnsAlt = 0;

	client->retryTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->retryTimer);
	client->renewTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->renewTimer);
	client->rebindTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->rebindTimer);

	if (cancelArpAndLinkLocal != 0) {
		client->arpCheckTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue,
							client->arpCheckTimer);
		client->llProbeTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue,
						       client->llProbeTimer);
	}

	sceClibMemset(&client->pendingOffer, 0, sizeof(client->pendingOffer));
	client->requestRetryActive = 0;
	client->requestRetryCount = 0;
	client->requestSender = NULL;
	client->boundServerId = 0;
	client->boundLeaseTime = 0;
	client->boundRenewTime = 0;
	client->boundRebindTime = 0;
}

static SceUInt32 dhcpGenerateUniqueXid(void)
{
	unsigned int candidate = 0;

	for (;;) {
		do {
			SceNetInternal_689B9D7D(&candidate);
		} while (candidate == 0);

		bool collision = false;
		for (NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
		     cursor != NULL; cursor = cursor->next) {
			if (cursor->xid == candidate) {
				collision = true;
				break;
			}
		}
		if (!collision)
			return candidate;
	}
}

static void dhcpSelectLinkLocalAddress(NetDhcpClient *client, SceUInt16 hostSeed)
{
	unsigned int randVal = 0;
	SceUInt16 hostPart = hostSeed;

	client->llProbesRemaining = 2;
	while ((SceUInt16)(hostPart - 1u) > 0xFFFDu) {
		SceNetInternal_689B9D7D(&randVal);
		hostPart = (SceUInt16)(hostPart + (SceUInt16)randVal);
	}
	client->linkLocalMask = 0xFFFF0000u;
	client->linkLocalIp = 0xA9FE0000u | (SceUInt32)hostPart;
}

static void dhcpScheduleLeaseTimers(NetDhcpClient *client)
{
	client->retryTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->retryTimer);
	client->renewTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->renewTimer);
	client->rebindTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->rebindTimer);

	if (client->boundLeaseTime != 0xFFFFFFFFu) {
		client->retryTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
						  dhcpOnDiscoverOrExpire, client,
						  (SceUInt64)client->boundLeaseTime * 1000ULL,
						  dhcpWakeEpoll);
	}

	client->renewAttempts = 0;
	client->rebindAttempts = 0;

	if (client->boundRenewTime != 0xFFFFFFFFu) {
		client->renewTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
						  dhcpOnRenewTimer, client,
						  (SceUInt64)client->boundRenewTime * 1000ULL,
						  dhcpWakeEpoll);
	}

	if (client->boundRebindTime != 0xFFFFFFFFu) {
		client->rebindTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
						   dhcpOnRebindTimer, client,
						   (SceUInt64)client->boundRebindTime * 1000ULL,
						   dhcpWakeEpoll);
	}
}

static int dhcpSendArpProbe(int ifId, SceUInt32 senderHostIp, SceUInt32 targetHostIp)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	NetArpProbeReq req;

	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.ifrName, ifName, sizeof(req.ifrName) - 1);
	req.senderIp = sceNetHtonl(senderHostIp);
	req.targetIp = sceNetHtonl(targetHostIp);
	return SceNetInternal_333F9CB3(g_dhcpDaemon.arpSock, NET_SIOCAIFADDR, &req);
}

static void dhcpStartLinkLocalProbe(NetDhcpClient *client)
{
	if (client->linkLocalState != LL_STATE_PROBING)
		return;

	dhcpSendArpProbe(client->ifId, client->linkLocalIp, client->linkLocalIp);
	client->llProbeTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
					    dhcpOnLinkLocalProbeTimer, client, 1000ULL,
					    dhcpWakeEpoll);
}

static void dhcpOnLinkLocalProbeTimer(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	client->llProbeTimer = NULL;
	if (client->llProbesRemaining > 0) {
		client->llProbesRemaining--;
		dhcpStartLinkLocalProbe(client);
		return;
	}

	client->linkLocalState = LL_STATE_ASSIGNED;
	SceNetInAddr ipNet;
	SceNetInAddr maskNet;
	ipNet.s_addr = sceNetHtonl(client->linkLocalIp);
	maskNet.s_addr = sceNetHtonl(client->linkLocalMask);
	sceNetConfigAddIfaddr(client->ifId, &ipNet, NULL, &maskNet);

	if ((client->routeFlags & 1u) == 0) {
		sceNetConfigAddMRoute(&ipNet);
		client->routeFlags |= 1u;
	}

	if (client->callback != NULL) {
		int ev = ((client->config.flags & DHCP_FLAG_LINKLOCAL_ONLY) != 0)
			? DHCP_EV_BOUND : DHCP_EV_LINKLOCAL;
		g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
		client->callback(client->callbackArg, ev, 0);
		g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
	}
}

static void dhcpOnArpCheckDone(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	client->arpCheckTimer = NULL;
	if (client->dhcpState != DHCP_STATE_REQUESTING)
		return;

	client->activeIp = client->pendingOffer.offeredIp;
	client->activeSubnetMask = client->pendingOffer.subnetMask;
	client->activeRouterIp = client->pendingOffer.routerIp;
	client->activeDnsMain = client->pendingOffer.dnsMain;
	client->activeDnsAlt = client->pendingOffer.dnsAlt;
	client->boundServerId = client->pendingOffer.serverId;
	client->boundLeaseTime = client->pendingOffer.leaseTime;
	client->boundRenewTime = client->pendingOffer.renewTime;
	client->boundRebindTime = client->pendingOffer.rebindTime;
	client->dhcpState = DHCP_STATE_BOUND;

	SceNetInAddr ipNet;
	SceNetInAddr maskNet;
	if ((client->config.flags & DHCP_FLAG_ALLOW_LINKLOCAL_FALLBACK) == 0) {
		if (client->linkLocalState == LL_STATE_ASSIGNED) {
			sceNetConfigDelIfaddr(client->ifId);
			ipNet.s_addr = sceNetHtonl(client->linkLocalIp);
			sceNetConfigDelMRoute(&ipNet);
			client->routeFlags &= ~1u;
		}
		client->linkLocalState = LL_STATE_DISABLED;
	}

	ipNet.s_addr = sceNetHtonl(client->activeIp);
	maskNet.s_addr = sceNetHtonl(client->activeSubnetMask);
	sceNetConfigSetIfaddr(client->ifId, &ipNet, NULL, &maskNet);

	if ((client->config.flags & DHCP_FLAG_NO_ROUTE_OR_DNS) == 0) {
		if (client->activeRouterIp != 0) {
			SceNetInAddr gwNet;
			gwNet.s_addr = sceNetHtonl(client->activeRouterIp);
			sceNetConfigSetDefaultRoute(&gwNet);
		}
		if ((client->config.flags & DHCP_FLAG_USE_STATIC_DNS) != 0) {
			SceNetInAddr dnsPair[2];
			dnsPair[0] = client->config.staticDnsMain;
			dnsPair[1] = client->config.staticDnsAlt;
			SceNetInternal_16222ED6(dnsPair, 0);
		} else {
			SceNetInAddr dnsPair[2];
			dnsPair[0].s_addr = sceNetHtonl(client->activeDnsMain);
			dnsPair[1].s_addr = sceNetHtonl(client->activeDnsAlt);
			SceNetInternal_16222ED6(dnsPair, 0);
		}
		if ((client->routeFlags & 1u) == 0) {
			sceNetConfigAddMRoute(&ipNet);
			client->routeFlags |= 1u;
		}
	}

	if (client->callback != NULL) {
		g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
		client->callback(client->callbackArg, DHCP_EV_BOUND, 0);
		g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
	}

	dhcpScheduleLeaseTimers(client);
}

static void dhcpBeginLinkLocal(NetDhcpClient *client)
{
	client->linkLocalState = LL_STATE_PROBING;

	SceUInt32 seed = client->macAddr.data[0];
	for (int index = 1; index < 6; index++) {
		SceUInt32 octet = client->macAddr.data[index];
		seed += octet;
		if ((index & 1) != 0)
			seed += (octet << 8);
	}

	dhcpSelectLinkLocalAddress(client, (SceUInt16)seed);
	dhcpStartLinkLocalProbe(client);
}

static void dhcpWakeEpoll(void)
{
	if ((g_dhcpDaemon.flags & DHCP_DAEMON_WAITING_EPOLL) != 0)
		sceNetEpollAbort(g_dhcpDaemon.epollId, 1);
}

static int dhcpSendPacket(NetDhcpClient *client, SceUInt32 ciaddrHost,
			  SceUInt32 dstIpHost, SceUInt32 xid,
			  const void *optionsBytes, SceSize optionsLen)
{
	SceSize totalOptLen = optionsLen + 9u;
	if (client->hostnameLen != 0)
		totalOptLen += client->hostnameLen + 2u;
	if (client->vendorClassLen != 0)
		totalOptLen += client->vendorClassLen + 2u;

	SceSize pktSize = (totalOptLen <= 62u)
		? DHCP_MIN_PACKET_LEN
		: (((totalOptLen + 4u) & ~3u) + DHCP_FIXED_HEADER_LEN);

	NetDhcpBuf *buf = dhcpBufAlloc(NULL, pktSize);
	if (buf == NULL)
		return 0;

	SceUInt8 *raw = buf->tail;
	sceClibMemset(raw, 0, pktSize);

	NetDhcpWireHeader *hdr = (NetDhcpWireHeader *)raw;
	hdr->op = DHCP_OP_BOOTREQUEST;
	hdr->htype = DHCP_HTYPE_ETHER;
	hdr->hlen = DHCP_HLEN_ETHER;
	hdr->xid = xid;
	hdr->ciaddr = ciaddrHost;
	for (int index = 0; index < 6; index++)
		hdr->chaddr[index] = client->macAddr.data[index];

	SceUInt8 *optCursor = raw + DHCP_FIXED_HEADER_LEN;
	sceClibMemcpy_safe(optCursor, optionsBytes, optionsLen);
	optCursor += optionsLen;

	/* Option 61: Client Identifier */
	optCursor[0] = DHCP_OPT_CLIENT_ID;
	optCursor[1] = (SceUInt8)(hdr->hlen + 1u);
	optCursor[2] = hdr->htype;
	sceClibMemcpy_safe(optCursor + 3, hdr->chaddr, hdr->hlen);
	optCursor += 3u + hdr->hlen;

	/* Option 12: Host Name */
	if (client->hostnameLen != 0) {
		optCursor[0] = DHCP_OPT_HOSTNAME;
		optCursor[1] = (SceUInt8)client->hostnameLen;
		sceClibMemcpy_safe(optCursor + 2, client->hostnamePtr, client->hostnameLen);
		optCursor += 2u + client->hostnameLen;
	}

	/* Option 60: Vendor Class Identifier */
	if (client->vendorClassLen != 0) {
		optCursor[0] = DHCP_OPT_VENDOR_CLASS;
		optCursor[1] = (SceUInt8)client->vendorClassLen;
		sceClibMemcpy_safe(optCursor + 2, client->vendorClassPtr, client->vendorClassLen);
		optCursor += 2u + client->vendorClassLen;
	}

	sceClibMemcpy_safe(optCursor, &kDhcpEndOption, 1u);
	buf->tail += pktSize;

	hdr->xid = sceNetHtonl(hdr->xid);
	hdr->secs = sceNetHtons(hdr->secs);
	hdr->flags = sceNetHtons(hdr->flags);
	hdr->ciaddr = sceNetHtonl(hdr->ciaddr);
	hdr->yiaddr = sceNetHtonl(hdr->yiaddr);
	hdr->siaddr = sceNetHtonl(hdr->siaddr);
	hdr->giaddr = sceNetHtonl(hdr->giaddr);

	SceUInt32 outIfNet = sceNetHtonl(client->kernelIfIndex);
	if (sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_IPPROTO_IP, DHCP_IP_MULTICAST_IF,
			     &outIfNet, sizeof(outIfNet)) >= 0) {
		SceNetSockaddrIn dstSa;
		sceClibMemset(&dstSa, 0, sizeof(dstSa));
		dstSa.sin_family = NET_AF_INET;
		dstSa.sin_addr.s_addr = sceNetHtonl(dstIpHost);
		dstSa.sin_port = sceNetHtons(DHCP_SERVER_PORT);
		sceNetSendto(g_dhcpDaemon.udpSock, buf->head,
			     (unsigned int)(buf->tail - buf->head), 0,
			     (const SceNetSockaddr *)&dstSa, sizeof(dstSa));
	}

	return dhcpBufFree(buf);
}

static int dhcpSendSelectingRequest(NetDhcpClient *client, SceUInt32 requestedIp,
				    SceUInt32 serverIp, SceUInt32 xid)
{
	SceUInt8 optBuf[36];

	sceClibMemcpy_safe(optBuf, kDhcpMagicCookie, sizeof(kDhcpMagicCookie));
	sceClibMemcpy_safe(optBuf + 4, kDhcpOptMsgRequest, sizeof(kDhcpOptMsgRequest));
	optBuf[7] = DHCP_OPT_REQUESTED_IP;
	optBuf[8] = 4;
	writeBe32(optBuf + 9, requestedIp);
	optBuf[13] = DHCP_OPT_SERVER_ID;
	optBuf[14] = 4;
	writeBe32(optBuf + 15, serverIp);
	sceClibMemcpy_safe(optBuf + 19, kDhcpOptParamList, sizeof(kDhcpOptParamList));
	return dhcpSendPacket(client, 0, 0xFFFFFFFFu, xid, optBuf, 25u);
}

static int dhcpSendRenewRequest(NetDhcpClient *client, SceUInt32 ciaddrHost,
				SceUInt32 dstIpHost, SceUInt32 xid)
{
	SceUInt8 optBuf[36];

	sceClibMemcpy_safe(optBuf, kDhcpMagicCookie, sizeof(kDhcpMagicCookie));
	sceClibMemcpy_safe(optBuf + 4, kDhcpOptMsgRequest, sizeof(kDhcpOptMsgRequest));
	return dhcpSendPacket(client, ciaddrHost, dstIpHost, xid, optBuf, 7u);
}

static void dhcpOnRebindTimer(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	client->renewTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->renewTimer);
	client->dhcpState = DHCP_STATE_REBINDING;

	if (client->callback != NULL) {
		g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
		client->callback(client->callbackArg, DHCP_EV_REBINDING, 0);
		g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
	}

	client->xid = dhcpGenerateUniqueXid();
	dhcpSendRenewRequest(client, client->pendingOffer.offeredIp, 0xFFFFFFFFu, client->xid);

	if (client->rebindAttempts == 0 && client->boundLeaseTime > client->boundRebindTime) {
		SceUInt32 remainMs = (client->boundLeaseTime - client->boundRebindTime) * 1000u;
		client->rebindTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
						   dhcpOnRebindTimer, client,
						   (SceUInt64)(remainMs >> 1), dhcpWakeEpoll);
	}
	client->rebindAttempts++;
}

static void dhcpOnDiscoverOrExpire(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	dhcpResetClientLease(client, 0);
	client->pendingOffer.offeredIp = sceNetNtohl(client->config.requestedIp.s_addr);

	if (client->dhcpState == DHCP_STATE_INIT) {
		client->xid = dhcpGenerateUniqueXid();
	} else if (client->dhcpState >= DHCP_STATE_BOUND &&
		   client->dhcpState <= DHCP_STATE_REBINDING) {
		client->dhcpState = DHCP_STATE_INIT;
		client->xid = dhcpGenerateUniqueXid();
		client->discoverRetryRemaining = 3;
		if (client->callback != NULL) {
			g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
			client->callback(client->callbackArg, DHCP_EV_EXPIRED,
					 NETDHCP_ERR_LEASE_EXPIRED);
			g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
		}
	}

	if ((client->config.flags & DHCP_FLAG_LINKLOCAL_ONLY) != 0) {
		if (client->linkLocalState == LL_STATE_NONE)
			dhcpBeginLinkLocal(client);
		return;
	}

	if ((client->config.flags & DHCP_FLAG_ALLOW_LINKLOCAL_FALLBACK) != 0 &&
	    client->linkLocalState == LL_STATE_NONE) {
		dhcpBeginLinkLocal(client);
	}

	SceUInt8 optBuf[16];
	sceClibMemcpy_safe(optBuf, kDhcpMagicCookie, sizeof(kDhcpMagicCookie));
	sceClibMemcpy_safe(optBuf + 4, kDhcpOptMsgDiscover, sizeof(kDhcpOptMsgDiscover));
	SceSize optLen = 7u;
	if (client->pendingOffer.offeredIp != 0) {
		optBuf[7] = DHCP_OPT_REQUESTED_IP;
		optBuf[8] = 4;
		writeBe32(optBuf + 9, client->pendingOffer.offeredIp);
		optLen = 13u;
	}

	dhcpSendPacket(client, 0, 0xFFFFFFFFu, client->xid, optBuf, optLen);

	SceUInt64 retryDelayMs;
	if (client->discoverRetryRemaining > 0) {
		unsigned int randVal = 0;
		client->discoverRetryRemaining--;
		SceNetInternal_689B9D7D(&randVal);
		retryDelayMs = (SceUInt64)((randVal % 1000u) + 1500u);
	} else {
		if (client->linkLocalState == LL_STATE_NONE)
			dhcpBeginLinkLocal(client);
		retryDelayMs = 20000ULL;
	}

	client->dhcpState = DHCP_STATE_SELECTING;
	client->retryTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue, dhcpOnDiscoverOrExpire,
					  client, retryDelayMs, dhcpWakeEpoll);
}

static void dhcpOnRenewTimer(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	client->dhcpState = DHCP_STATE_RENEWING;
	if (client->boundServerId == 0)
		return;

	if (client->callback != NULL) {
		g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
		client->callback(client->callbackArg, DHCP_EV_RENEWING, 0);
		g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
	}

	client->xid = dhcpGenerateUniqueXid();
	dhcpSendRenewRequest(client, client->pendingOffer.offeredIp,
			     client->boundServerId, client->xid);

	if (client->renewAttempts == 0 && client->boundRebindTime > client->boundRenewTime) {
		SceUInt32 remainMs = (client->boundRebindTime - client->boundRenewTime) * 1000u;
		client->renewTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
						  dhcpOnRenewTimer, client,
						  (SceUInt64)(remainMs >> 1), dhcpWakeEpoll);
	}
	client->renewAttempts++;
}

static void dhcpOnRequestRetryTimer(void *userArg)
{
	NetDhcpClient *client = (NetDhcpClient *)userArg;

	if (client->requestRetryActive != 1)
		return;

	if (client->requestRetryCount == 4) {
		dhcpOnDiscoverOrExpire(client);
		return;
	}

	client->requestSender(client, client->pendingOffer.offeredIp,
			      client->pendingOffer.serverId, client->xid);

	unsigned int randVal = 0;
	SceNetInternal_689B9D7D(&randVal);

	int attempt = client->requestRetryCount;
	client->requestRetryCount = attempt + 1;
	int jitterMs = (int)(randVal % 2000u) - 1000;
	SceInt64 delayMs = (SceInt64)(1000 * (4 << attempt) + jitterMs);

	client->retryTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue, dhcpOnRequestRetryTimer,
					  client, (SceUInt64)delayMs, dhcpWakeEpoll);
}

static void dhcpHandleArpPacket(SceUInt8 *packetBytes, SceSize packetLen)
{
	if (packetLen <= 27u)
		return;

	NetArpWirePacket *arp = (NetArpWirePacket *)packetBytes;
	arp->hwType = sceNetNtohs(arp->hwType);
	arp->protoType = sceNetNtohs(arp->protoType);
	arp->opcode = sceNetNtohs(arp->opcode);
	SceUInt32 senderIpHost = readBe32(arp->senderIp);
	arp->senderIp[0] = (SceUInt8)senderIpHost;
	arp->senderIp[1] = (SceUInt8)(senderIpHost >> 8);
	arp->senderIp[2] = (SceUInt8)(senderIpHost >> 16);
	arp->senderIp[3] = (SceUInt8)(senderIpHost >> 24);
	arp->targetIp = sceNetNtohl(arp->targetIp);

	if (arp->hwType != 1 || arp->protoType != 0x0800 ||
	    arp->hwLen != 6 || arp->protoLen != 4) {
		return;
	}

	for (NetDhcpClient *client = g_dhcpDaemon.clientHead;
	     client != NULL; client = client->next) {
		bool sameMac = true;
		for (int index = 0; index < 6; index++) {
			if (arp->senderMac[index] != client->macAddr.data[index]) {
				sameMac = false;
				break;
			}
		}
		if (sameMac)
			continue;

		if (senderIpHost == client->pendingOffer.offeredIp) {
			if (client->linkLocalState == LL_STATE_PROBING) {
				unsigned int randVal = 0;
				client->llProbeTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue,
								       client->llProbeTimer);
				SceUInt16 prevHost = (SceUInt16)client->linkLocalIp;
				SceNetInternal_689B9D7D(&randVal);
				dhcpSelectLinkLocalAddress(client,
							   (SceUInt16)(prevHost + (SceUInt16)randVal));
				dhcpStartLinkLocalProbe(client);
			}
			if (client->dhcpState == DHCP_STATE_REQUESTING) {
				client->arpCheckTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue,
									client->arpCheckTimer);
				client->retryTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue,
								     client->retryTimer);

				SceUInt8 declineOpts[20];
				sceClibMemcpy_safe(declineOpts, kDhcpMagicCookie,
						   sizeof(kDhcpMagicCookie));
				sceClibMemcpy_safe(declineOpts + 4, kDhcpOptMsgDecline,
						   sizeof(kDhcpOptMsgDecline));
				declineOpts[7] = DHCP_OPT_REQUESTED_IP;
				declineOpts[8] = 4;
				writeBe32(declineOpts + 9, client->pendingOffer.offeredIp);
				declineOpts[13] = DHCP_OPT_SERVER_ID;
				declineOpts[14] = 4;
				writeBe32(declineOpts + 15, client->pendingOffer.serverId);

				dhcpSendPacket(client, 0, 0xFFFFFFFFu, client->xid,
					       declineOpts, 19u);
				client->dhcpState = DHCP_STATE_INIT;
				dhcpOnDiscoverOrExpire(client);
			}
			break;
		}
	}
}

static void dhcpHandleUdpPacket(SceUInt8 *packetBytes, SceSize packetLen)
{
	if (packetLen <= 243u)
		return;

	NetDhcpWireHeader *hdr = (NetDhcpWireHeader *)packetBytes;
	hdr->xid = sceNetNtohl(hdr->xid);
	hdr->secs = sceNetNtohs(hdr->secs);
	hdr->flags = sceNetNtohs(hdr->flags);
	hdr->ciaddr = sceNetNtohl(hdr->ciaddr);
	hdr->yiaddr = sceNetNtohl(hdr->yiaddr);
	hdr->siaddr = sceNetNtohl(hdr->siaddr);
	hdr->giaddr = sceNetNtohl(hdr->giaddr);

	if (hdr->op != DHCP_OP_BOOTREPLY || hdr->htype != DHCP_HTYPE_ETHER ||
	    hdr->hlen != DHCP_HLEN_ETHER) {
		return;
	}

	NetDhcpClient *client = g_dhcpDaemon.clientHead;
	while (client != NULL && client->xid != hdr->xid)
		client = client->next;
	if (client == NULL)
		return;

	if (packetBytes[236] != kDhcpMagicCookie[0] ||
	    packetBytes[237] != kDhcpMagicCookie[1] ||
	    packetBytes[238] != kDhcpMagicCookie[2] ||
	    packetBytes[239] != kDhcpMagicCookie[3]) {
		return;
	}

	NetDhcpOffer parsed;
	sceClibMemset(&parsed, 0, sizeof(parsed));
	int msgType = 0;

	const SceUInt8 *optPtr = packetBytes + 240;
	const SceUInt8 *optEnd = packetBytes + packetLen;
	while (optPtr < optEnd) {
		SceUInt8 code = *optPtr;
		if (code == DHCP_OPT_PAD) {
			optPtr++;
			continue;
		}
		if (code == DHCP_OPT_END || optPtr + 1 >= optEnd)
			break;

		SceUInt8 len = optPtr[1];
		if (optPtr + 2 + len > optEnd)
			break;

		switch (code) {
		case DHCP_OPT_SUBNET_MASK:
			if (len == 4)
				parsed.subnetMask = readBe32(optPtr + 2);
			break;
		case DHCP_OPT_ROUTER:
			if (len >= 4)
				parsed.routerIp = readBe32(optPtr + 2);
			break;
		case DHCP_OPT_DNS_SERVER:
			if (len >= 4) {
				parsed.dnsMain = readBe32(optPtr + 2);
				if (len >= 8)
					parsed.dnsAlt = readBe32(optPtr + 6);
			}
			break;
		case DHCP_OPT_LEASE_TIME:
			if (len == 4)
				parsed.leaseTime = readBe32(optPtr + 2);
			break;
		case DHCP_OPT_MSG_TYPE:
			if (len == 1)
				msgType = optPtr[2];
			break;
		case DHCP_OPT_SERVER_ID:
			if (len == 4)
				parsed.serverId = readBe32(optPtr + 2);
			break;
		case DHCP_OPT_RENEWAL_TIME:
			if (len == 4)
				parsed.renewTime = readBe32(optPtr + 2);
			break;
		case DHCP_OPT_REBINDING_TIME:
			if (len == 4)
				parsed.rebindTime = readBe32(optPtr + 2);
			break;
		default:
			break;
		}
		optPtr += (SceSize)len + 2u;
	}

	if (msgType == 0)
		return;

	switch (client->dhcpState) {
	case DHCP_STATE_SELECTING:
		if (msgType != DHCP_MSG_OFFER)
			break;
		if (client->pendingOffer.offeredIp != 0 &&
		    client->pendingOffer.offeredIp != hdr->yiaddr) {
			client->config.requestedIp.s_addr = 0;
			break;
		}
		if (parsed.serverId == 0)
			break;

		client->pendingOffer.offeredIp = hdr->yiaddr;
		client->pendingOffer.subnetMask = parsed.subnetMask;
		client->pendingOffer.routerIp = parsed.routerIp;
		client->pendingOffer.dnsMain = parsed.dnsMain;
		client->pendingOffer.dnsAlt = parsed.dnsAlt;
		client->pendingOffer.serverId = parsed.serverId;
		client->pendingOffer.leaseTime = parsed.leaseTime;
		client->discoverRetryRemaining = 0;

		client->retryTimer = dhcpTimerCancel(&g_dhcpDaemon.timerQueue, client->retryTimer);
		client->requestSender = dhcpSendSelectingRequest;
		client->requestRetryActive = 1;
		client->requestRetryCount = 0;
		dhcpOnRequestRetryTimer(client);
		client->dhcpState = DHCP_STATE_REQUESTING;
		break;

	case DHCP_STATE_REQUESTING:
		if (msgType == DHCP_MSG_ACK) {
			if (client->arpCheckTimer != NULL || parsed.serverId == 0)
				break;
			if (dhcpNormalizeLeaseTimers(&parsed, &client->pendingOffer) != 0)
				break;

			if (client->pendingOffer.subnetMask == 0) {
				client->pendingOffer.subnetMask = parsed.subnetMask;
				if (client->pendingOffer.subnetMask == 0) {
					switch (client->pendingOffer.offeredIp >> 30) {
					case 0:
					case 1:
						client->pendingOffer.subnetMask = 0xFF000000u;
						break;
					case 2:
						client->pendingOffer.subnetMask = 0xFFFF0000u;
						break;
					case 3:
						client->pendingOffer.subnetMask = 0xFFFFFF00u;
						break;
					default:
						abort();
					}
				}
			}
			if (client->pendingOffer.routerIp == 0)
				client->pendingOffer.routerIp = parsed.routerIp;
			if (client->pendingOffer.dnsMain == 0)
				client->pendingOffer.dnsMain = parsed.dnsMain;
			if (client->pendingOffer.dnsAlt == 0)
				client->pendingOffer.dnsAlt = parsed.dnsAlt;
			client->pendingOffer.serverId = parsed.serverId;

			dhcpSendArpProbe(client->ifId, client->activeIp,
					 client->pendingOffer.offeredIp);
			client->arpCheckTimer = dhcpTimerAdd(&g_dhcpDaemon.timerQueue,
							     dhcpOnArpCheckDone, client,
							     500ULL, dhcpWakeEpoll);
		} else if (msgType == DHCP_MSG_NAK && client->arpCheckTimer == NULL) {
			SceUInt32 nakServer = (parsed.serverId != 0)
				? parsed.serverId : hdr->siaddr;
			if (nakServer == client->pendingOffer.serverId) {
				client->dhcpState = DHCP_STATE_INIT;
				dhcpOnDiscoverOrExpire(client);
			}
		}
		break;

	case DHCP_STATE_RENEWING:
		if (msgType == DHCP_MSG_ACK) {
			if (dhcpNormalizeLeaseTimers(&parsed, &client->pendingOffer) != 0)
				break;
			client->boundLeaseTime = client->pendingOffer.leaseTime;
			client->boundRenewTime = client->pendingOffer.renewTime;
			client->boundRebindTime = client->pendingOffer.rebindTime;
			dhcpScheduleLeaseTimers(client);
			client->dhcpState = DHCP_STATE_BOUND;
			if (client->callback != NULL) {
				g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
				client->callback(client->callbackArg, DHCP_EV_RENEWED, 0);
				g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
			}
		} else if (msgType == DHCP_MSG_NAK) {
			SceUInt32 nakServer = (parsed.serverId != 0)
				? parsed.serverId : hdr->siaddr;
			if (nakServer == client->pendingOffer.serverId)
				dhcpOnDiscoverOrExpire(client);
		}
		break;

	case DHCP_STATE_REBINDING:
		if (msgType == DHCP_MSG_ACK) {
			if (parsed.serverId == 0)
				break;
			if (dhcpNormalizeLeaseTimers(&parsed, &client->pendingOffer) != 0)
				break;
			client->pendingOffer.serverId = parsed.serverId;
			client->boundLeaseTime = client->pendingOffer.leaseTime;
			client->boundRenewTime = client->pendingOffer.renewTime;
			client->boundRebindTime = client->pendingOffer.rebindTime;
			dhcpScheduleLeaseTimers(client);
			client->dhcpState = DHCP_STATE_BOUND;
			if (client->callback != NULL) {
				g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
				client->callback(client->callbackArg, DHCP_EV_RENEWED, 0);
				g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
			}
		} else if (msgType == DHCP_MSG_NAK) {
			SceUInt32 nakServer = (parsed.serverId != 0)
				? parsed.serverId : hdr->siaddr;
			if (nakServer == client->pendingOffer.serverId)
				dhcpOnDiscoverOrExpire(client);
		}
		break;

	default:
		break;
	}
}

static int dhcpDaemonThread(void)
{
	NetDhcpBuf *recvBuf = g_dhcpDaemon.recvBuf;
	SceNetEpollEvent events[2];
	SceNetSockaddr fromAddr;
	unsigned int fromLen = sizeof(fromAddr);
	bool delayNextWait = false;

	SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

	for (;;) {
		int waitMs = dhcpTimerProcess(&g_dhcpDaemon.timerQueue);
		g_dhcpDaemon.flags |= DHCP_DAEMON_WAITING_EPOLL;
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);

		if (delayNextWait)
			SceNetInternal_104E4820(1000000);

		int readyCount = sceNetEpollWait(g_dhcpDaemon.epollId, events, 2,
						 (waitMs < 0) ? -1000 : (waitMs * 1000));
		SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

		if ((g_dhcpDaemon.flags & DHCP_DAEMON_STOP_REQUESTED) != 0)
			break;

		g_dhcpDaemon.flags &= ~DHCP_DAEMON_WAITING_EPOLL;
		if (readyCount < 0) {
			delayNextWait = true;
			continue;
		}
		delayNextWait = false;

		for (int index = 0; index < readyCount; index++) {
			int received = sceNetRecvfrom(events[index].data.fd, recvBuf->tail,
						      DHCP_MAX_RECV_LEN, 0,
						      &fromAddr, &fromLen);
			if (received < 0)
				continue;

			recvBuf->tail += received;
			SceSize packetLen = (SceSize)(recvBuf->tail - recvBuf->head);
			if (events[index].data.fd == g_dhcpDaemon.udpSock)
				dhcpHandleUdpPacket(recvBuf->head, packetLen);
			else if (events[index].data.fd == g_dhcpDaemon.arpSock)
				dhcpHandleArpPacket(recvBuf->head, packetLen);

			recvBuf->tail -= received;
		}
	}

	SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
	return SceNetInternal_4EF96CD8();
}

int sceNetDhcpGetInfo(int ifId, NetDhcpInfo *outInfo)
{
	if ((g_dhcpDaemon.flags & DHCP_DAEMON_INITIALIZED) == 0)
		return NETDHCP_ERR_NOT_INITIALIZED;

	bool alreadyLocked = (g_dhcpDaemon.flags & DHCP_DAEMON_IN_CALLBACK) != 0;
	if (!alreadyLocked)
		SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

	int ret;
	if (outInfo == NULL || ifId == -1) {
		ret = NETDHCP_ERR_INVALID_PARAM;
	} else {
		NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
		while (cursor != NULL && cursor->ifId != ifId)
			cursor = cursor->next;

		if (cursor != NULL) {
			outInfo->state = cursor->dhcpState;
			outInfo->leaseTime = cursor->boundLeaseTime;
			outInfo->serverIp = cursor->boundServerId;
			ret = 0;
		} else {
			ret = NETDHCP_ERR_NOT_FOUND;
		}
	}

	if (!alreadyLocked)
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
	return ret;
}

int sceNetDhcpGetLinkLocalInfo(int ifId, NetDhcpLinkLocalInfo *outInfo)
{
	if ((g_dhcpDaemon.flags & DHCP_DAEMON_INITIALIZED) == 0)
		return NETDHCP_ERR_NOT_INITIALIZED;

	bool alreadyLocked = (g_dhcpDaemon.flags & DHCP_DAEMON_IN_CALLBACK) != 0;
	if (!alreadyLocked)
		SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

	int ret;
	if (outInfo == NULL || ifId == -1) {
		ret = NETDHCP_ERR_INVALID_PARAM;
	} else {
		NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
		while (cursor != NULL && cursor->ifId != ifId)
			cursor = cursor->next;

		if (cursor != NULL) {
			outInfo->state = cursor->linkLocalState;
			outInfo->ipAddr = cursor->linkLocalIp;
			outInfo->netmask = cursor->linkLocalMask;
			ret = 0;
		} else {
			ret = NETDHCP_ERR_NOT_FOUND;
		}
	}

	if (!alreadyLocked)
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
	return ret;
}

static int dhcpInitDaemon(SceSize stackSize, int priority)
{
	int optEnable = 1;
	int nbioVal = 29;
	SceUInt8 mcastTtl = 16;

	int udpSock = sceNetSocket("SceNetDhcp", NET_AF_INET, NET_SOCK_DGRAM, 0);
	if (udpSock < 0) {
		sceClibMemset(&g_dhcpDaemon, 0, sizeof(g_dhcpDaemon));
		return 0;
	}
	g_dhcpDaemon.udpSock = udpSock;

	int arpSock = sceNetSocket("SceNetDhcp", NET_AF_INET, NET_SOCK_ARP, 0);
	if (arpSock < 0) {
		sceNetSocketClose(g_dhcpDaemon.udpSock);
		sceClibMemset(&g_dhcpDaemon, 0, sizeof(g_dhcpDaemon));
		return 0;
	}
	g_dhcpDaemon.arpSock = arpSock;

	int epollId = sceNetEpollCreate("SceNetDhcp", 0);
	int ret = 0;
	if (epollId >= 0) {
		g_dhcpDaemon.epollId = epollId;
		g_dhcpDaemon.recvBuf = dhcpBufAlloc(NULL, DHCP_MAX_RECV_LEN);
		if (g_dhcpDaemon.recvBuf == NULL) {
			ret = NETDHCP_ERR_NO_MEMORY;
		} else {
			SceNetSockaddrIn bindSa;
			sceClibMemset(&bindSa, 0, sizeof(bindSa));
			bindSa.sin_len = sizeof(bindSa);
			bindSa.sin_family = NET_AF_INET;
			bindSa.sin_port = sceNetHtons(DHCP_CLIENT_PORT);

			SceNetEpollEvent udpEvent;
			SceNetEpollEvent arpEvent;
			sceClibMemset(&udpEvent, 0, sizeof(udpEvent));
			udpEvent.events = 1;
			udpEvent.data.fd = g_dhcpDaemon.udpSock;

			sceClibMemset(&arpEvent, 0, sizeof(arpEvent));
			arpEvent.events = 1;
			arpEvent.data.fd = g_dhcpDaemon.arpSock;

			ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
					       DHCP_SO_REUSEADDR, &optEnable, sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetBind(g_dhcpDaemon.udpSock,
						 (const SceNetSockaddr *)&bindSa, sizeof(bindSa));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_BROADCAST, &optEnable,
						       sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_IPPROTO_IP,
						       DHCP_IP_MULTICAST_TTL, &mcastTtl,
						       sizeof(mcastTtl));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_DONTROUTE, &optEnable,
						       sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_KERNEL_PRIV, &optEnable,
						       sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.arpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_KERNEL_PRIV, &optEnable,
						       sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_NBIO, &nbioVal, sizeof(nbioVal));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.arpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_NBIO, &nbioVal, sizeof(nbioVal));
			if (ret >= 0)
				ret = sceNetSetsockopt(g_dhcpDaemon.udpSock, DHCP_SOL_SOCKET,
						       DHCP_SO_ONESBCAST, &optEnable,
						       sizeof(optEnable));
			if (ret >= 0)
				ret = sceNetEpollControl(g_dhcpDaemon.epollId, 1,
							 g_dhcpDaemon.udpSock, &udpEvent);
			if (ret >= 0)
				ret = sceNetEpollControl(g_dhcpDaemon.epollId, 1,
							 g_dhcpDaemon.arpSock, &arpEvent);
			if (ret >= 0) {
				g_dhcpThreadParam.entry = dhcpDaemonThread;
				g_dhcpThreadParam.argument = NULL;
				g_dhcpThreadParam.priority = priority;
				g_dhcpThreadParam.stackSize = (int)stackSize;
				g_dhcpThreadParam.flags = 0;
				ret = SceNetInternal_225C4801(&g_dhcpDaemon.threadId,
							      &g_dhcpThreadParam, "SceNetDhcp");
				if (ret >= 0) {
					g_dhcpDaemon.flags |= DHCP_DAEMON_INITIALIZED;
					return 0;
				}
			}

			dhcpBufFree(g_dhcpDaemon.recvBuf);
		}
		sceNetEpollDestroy(g_dhcpDaemon.epollId);
	}

	sceNetSocketClose(g_dhcpDaemon.arpSock);
	sceNetSocketClose(g_dhcpDaemon.udpSock);
	sceClibMemset(&g_dhcpDaemon, 0, sizeof(g_dhcpDaemon));
	return ret;
}

int sceNetDhcpStart(int ifId, void (*callback)(void *userArg, int event, int err), void *userArg,
		    SceSize stackSize, int priority, const void *dhcpParam)
{
	const NetDhcpStartParam *param = (const NetDhcpStartParam *)dhcpParam;
	int ret = 0;

	SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

	if ((g_dhcpDaemon.flags & DHCP_DAEMON_INITIALIZED) == 0) {
		ret = dhcpInitDaemon(stackSize, priority);
		if (ret != 0) {
			SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
			return ret;
		}
	}

	const SceUInt32 conflictMask = DHCP_FLAG_LINKLOCAL_ONLY |
				       DHCP_FLAG_ALLOW_LINKLOCAL_FALLBACK;
	if (param == NULL || ifId == -1 || (param->flags & conflictMask) == conflictMask) {
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		return NETDHCP_ERR_INVALID_PARAM;
	}

	for (NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
	     cursor != NULL; cursor = cursor->next) {
		if (cursor->ifId == ifId) {
			SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
			return NETDHCP_ERR_ALREADY_STARTED;
		}
	}

	if ((param->flags & DHCP_FLAG_LINKLOCAL_ONLY) == 0) {
		ret = sceNetConfigSetIfaddr(ifId, &kZeroInAddr, &kBroadcastInAddr, &kZeroInAddr);
		if (ret < 0) {
			SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
			return ret;
		}
	}

	SceSize hostLen = sceClibStrnlen(param->hostname, sizeof(param->hostname));
	if (hostLen != 0) {
		hostLen++;
		if (hostLen > 0xFFu) {
			SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
			return NETDHCP_ERR_HOSTNAME_TOO_LONG;
		}
	}

	SceSize vendorLen = sceClibStrnlen(param->vendorClass, sizeof(param->vendorClass));
	bool useDefaultVendor = false;
	if (vendorLen == 0) {
		useDefaultVendor = true;
	} else {
		vendorLen++;
		if (vendorLen > 0xFu) {
			SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
			return NETDHCP_ERR_VENDOR_TOO_LONG;
		}
	}

	NetDhcpClient *client = (NetDhcpClient *)SceNetInternal_A2C2BC5F(sizeof(NetDhcpClient), 6);
	if (client == NULL) {
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		return NETDHCP_ERR_NO_MEMORY;
	}

	client->ifId = ifId;
	client->callback = callback;
	client->callbackArg = userArg;
	client->dhcpState = DHCP_STATE_INIT;
	client->linkLocalState = LL_STATE_NONE;
	client->discoverRetryRemaining = 15;
	sceClibMemcpy_safe(&client->config, param, sizeof(client->config));

	if (useDefaultVendor) {
		if ((client->config.flags & DHCP_FLAG_NO_VENDOR_CLASS) != 0) {
			vendorLen = 0;
		} else {
			vendorLen = sceClibStrnlen("PS Vita", 8u) + 1u;
			sceClibMemcpy_safe(client->config.vendorClass, "PS Vita", vendorLen);
		}
	}

	client->hostnamePtr = client->config.hostname;
	client->hostnameLen = hostLen;
	client->vendorClassPtr = client->config.vendorClass;
	client->vendorClassLen = vendorLen;

	ret = dhcpGetInterfaceHwInfo(client->ifId, &client->macAddr, &client->kernelIfIndex);
	if (ret >= 0) {
		dhcpOnDiscoverOrExpire(client);
		client->prev = g_dhcpDaemon.clientTail;
		if (g_dhcpDaemon.clientTail != NULL)
			g_dhcpDaemon.clientTail->next = client;
		else
			g_dhcpDaemon.clientHead = client;
		client->next = NULL;
		g_dhcpDaemon.clientTail = client;
	}

	SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
	return ret;
}

int sceNetDhcpStop(int ifId)
{
	int ret;

	SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

	if ((g_dhcpDaemon.flags & DHCP_DAEMON_INITIALIZED) == 0) {
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		return NETDHCP_ERR_NOT_INITIALIZED;
	}
	if (ifId == -1) {
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		return NETDHCP_ERR_INVALID_PARAM;
	}

	NetDhcpClient *client = g_dhcpDaemon.clientHead;
	while (client != NULL && client->ifId != ifId)
		client = client->next;
	if (client == NULL) {
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		return NETDHCP_ERR_NOT_FOUND;
	}

	if ((client->config.flags & DHCP_FLAG_NO_RELEASE_ON_STOP) == 0) {
		SceUInt8 releaseOpts[16];
		sceClibMemcpy_safe(releaseOpts, kDhcpMagicCookie, sizeof(kDhcpMagicCookie));
		sceClibMemcpy_safe(releaseOpts + 4, kDhcpOptMsgRelease, sizeof(kDhcpOptMsgRelease));
		releaseOpts[7] = DHCP_OPT_SERVER_ID;
		releaseOpts[8] = 4;
		writeBe32(releaseOpts + 9, client->boundServerId);

		dhcpSendPacket(client, client->pendingOffer.offeredIp,
			       client->boundServerId, client->xid, releaseOpts, 13u);
		SceNetInternal_104E4820(500000);
	}

	int mrouteHolders = 0;
	for (NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
	     cursor != NULL; cursor = cursor->next) {
		if ((cursor->routeFlags & 1u) != 0)
			mrouteHolders++;
	}
	if (mrouteHolders == 1) {
		SceNetInAddr activeNet;
		activeNet.s_addr = sceNetHtonl(client->activeIp);
		sceNetConfigDelMRoute(&activeNet);
		client->routeFlags &= ~1u;
	}

	sceNetConfigDelIfaddr(client->ifId);
	if (client->linkLocalState == LL_STATE_ASSIGNED)
		sceNetConfigDelIfaddr(client->ifId);

	if ((client->config.flags & DHCP_FLAG_NO_ROUTE_OR_DNS) == 0) {
		SceNetInAddr zeroDns[2];
		sceNetConfigDelDefaultRoute();
		sceClibMemset(zeroDns, 0, sizeof(zeroDns));
		SceNetInternal_16222ED6(zeroDns, 0);
	}

	if (client->callback != NULL) {
		g_dhcpDaemon.flags |= DHCP_DAEMON_IN_CALLBACK;
		client->callback(client->callbackArg, DHCP_EV_STOPPED, 0);
		g_dhcpDaemon.flags &= ~DHCP_DAEMON_IN_CALLBACK;
	}

	dhcpResetClientLease(client, 1);

	if (client->next != NULL)
		client->next->prev = client->prev;
	else
		g_dhcpDaemon.clientTail = client->prev;

	if (client->prev != NULL)
		client->prev->next = client->next;
	else
		g_dhcpDaemon.clientHead = client->next;

	SceNetInternal_E62A2FAA(client);

	if (g_dhcpDaemon.clientHead != NULL) {
		ret = 0;
		for (NetDhcpClient *cursor = g_dhcpDaemon.clientHead;
		     cursor != NULL; cursor = cursor->next) {
			ret++;
		}
	} else {
		ret = 0;
		g_dhcpDaemon.flags |= DHCP_DAEMON_STOP_REQUESTED;
		sceNetEpollAbort(g_dhcpDaemon.epollId, 1);
		SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
		SceNetInternal_3B5F94B7(g_dhcpDaemon.threadId);
		SceNetInternal_C2FF615E(SceNetInternal_F6291476(0), NULL);

		dhcpTimerDestroyAll(&g_dhcpDaemon.timerQueue);
		dhcpBufFree(g_dhcpDaemon.recvBuf);
		sceNetEpollDestroy(g_dhcpDaemon.epollId);
		sceNetSocketClose(g_dhcpDaemon.arpSock);
		sceNetSocketClose(g_dhcpDaemon.udpSock);
		sceClibMemset(&g_dhcpDaemon, 0, sizeof(g_dhcpDaemon));
	}

	SceNetInternal_9D12D8A4(SceNetInternal_F6291476(0), 1);
	return ret;
}
