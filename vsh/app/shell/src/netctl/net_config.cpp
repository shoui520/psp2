/*
 * net_config.cpp: SceNetConfig interface, address, route, and ARP helpers
 * statically linked into SceShell (0x81378924..0x813794FB).
 */
#include "net_config_internal.h"

#include <stddef.h>

typedef struct NetIfReq {
	char ifrName[16];
	union {
		SceUInt32 flags;
		int mtu;
		SceUInt8 storage[16];
	} value;
} NetIfReq;

typedef struct NetIfAliasReq {
	char ifraName[16];
	SceNetSockaddrIn addr;
	SceNetSockaddrIn broadAddr;
	SceNetSockaddrIn mask;
} NetIfAliasReq;

typedef struct NetRouteMetrics {
	SceUInt32 locks;
	SceUInt32 mtu;
	SceUInt32 hopcount;
	SceUInt32 expire;
	SceUInt32 recvpipe;
	SceUInt32 sendpipe;
	SceUInt32 ssthresh;
	SceUInt32 rtt;
	SceUInt32 rttvar;
	SceUInt32 pksent;
	SceUInt32 filler[4];
} NetRouteMetrics;

typedef struct NetRouteMsgHdr {
	SceUInt16 rtmMsglen;
	SceUInt8 rtmVersion;
	SceUInt8 rtmType;
	SceUInt16 rtmIndex;
	SceUInt16 rtmPad;
	int rtmFlags;
	int rtmAddrs;
	SceUID rtmPid;
	int rtmSeq;
	int rtmErrno;
	int rtmUse;
	SceUInt32 rtmInits;
	NetRouteMetrics rtmRmx;
} NetRouteMsgHdr;

typedef struct NetRouteMessage {
	NetRouteMsgHdr hdr;
	SceUInt8 sockaddrs[512];
} NetRouteMessage;

static int g_netConfigRouteSeq = 0;
static int g_netConfigArpSeq = 0;

static SceSize roundSockaddrLen(SceUInt8 saLen)
{
	if (saLen == 0)
		return 4;
	return (SceSize)(((unsigned int)(saLen - 1) | 3u) + 1u);
}

int sceNetConfigIsInterfaceUp(int ifId)
{
	NetIfInfoRecord ifInfo;

	if (SceNetInternal_235DE96C(ifId, &ifInfo, 1) == 1)
		return (int)(ifInfo.flags & NET_IFF_UP);
	return 0;
}

int sceNetConfigUpInterface(int ifId)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	int sock = sceNetSocket("SceNetConfigUpInterface", NET_AF_INET, NET_SOCK_DGRAM, 0);
	int ret;

	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigUpInterface", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetIfReq req;
	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.ifrName, ifName, sizeof(req.ifrName) - 1);

	if (SceNetInternal_333F9CB3(sock, NET_SIOCGIFFLAGS, &req) < 0) {
		ret = NETCFG_ERR_UP_INTERFACE;
	} else {
		req.value.flags |= NET_IFF_UP;
		if (SceNetInternal_333F9CB3(sock, NET_SIOCSIFFLAGS, &req) < 0)
			ret = NETCFG_ERR_UP_INTERFACE;
		else
			ret = 0;
	}

	sceNetSocketClose(sock);
	return ret;
}

int sceNetConfigDownInterface(int ifId)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	int sock;
	int ret;

	netConfigFlushRoutes(ifId);

	sock = sceNetSocket("SceNetConfigDownInterface", NET_AF_INET, NET_SOCK_DGRAM, 0);
	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigDownInterface", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetIfReq req;
	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.ifrName, ifName, sizeof(req.ifrName) - 1);

	if (SceNetInternal_333F9CB3(sock, NET_SIOCGIFFLAGS, &req) < 0) {
		ret = NETCFG_ERR_DOWN_INTERFACE;
	} else {
		req.value.flags &= ~NET_IFF_UP;
		if (SceNetInternal_333F9CB3(sock, NET_SIOCSIFFLAGS, &req) < 0)
			ret = NETCFG_ERR_DOWN_INTERFACE;
		else
			ret = 0;
	}

	sceNetSocketClose(sock);
	return ret;
}

int sceNetConfigCleanUpAllInterfaces(void)
{
	if (sceNetConfigIsInterfaceUp(1) != 0)
		sceNetConfigDownInterface(1);

	if (sceNetConfigIsInterfaceUp(4) != 0) {
		sceNetWlanAdhocLeave(4);
		sceNetWlanStopBackgroundScan(4);
		sceNetConfigDownInterface(4);
	}

	if (sceNetConfigIsInterfaceUp(8) != 0)
		sceNetConfigDownInterface(8);

	if (sceNetConfigIsInterfaceUp(7) != 0)
		sceNetConfigDownInterface(8);

	return 0;
}

int sceNetConfigSetIfmtu(int ifId, int mtu)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	int sock = sceNetSocket("SceNetConfigSetIfmtu", NET_AF_INET, NET_SOCK_DGRAM, 0);
	int ioctlRet;
	int ret;

	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigSetIfmtu", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetIfReq req;
	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.ifrName, ifName, sizeof(req.ifrName) - 1);
	req.value.mtu = mtu;

	ioctlRet = SceNetInternal_333F9CB3(sock, NET_SIOCSIFMTU, &req);
	ret = (ioctlRet < 0) ? NETCFG_ERR_SET_MTU : ioctlRet;

	sceNetSocketClose(sock);
	return ret;
}

int sceNetConfigGetIfaddrs(int ifId, void *outAddrs, int maxCount)
{
	NetIfInfoRecord ifInfo;
	int ret = SceNetInternal_235DE96C(ifId, &ifInfo, 1);

	if (ret < 0)
		return ret;

	NetIfAddrRecord *dest = (NetIfAddrRecord *)outAddrs;
	if (maxCount <= 0)
		return 0;

	dest[0] = ifInfo.addresses[0];
	if (maxCount == 1)
		return 0;

	dest[1] = ifInfo.addresses[1];
	return 0;
}

int sceNetConfigDelIfaddr(int ifId)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	int sock = sceNetSocket("SceNetConfigDelIfaddr", NET_AF_INET, NET_SOCK_DGRAM, 0);
	int ioctlRet;
	int ret;

	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigDelIfaddr", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetIfReq req;
	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.ifrName, ifName, sizeof(req.ifrName) - 1);

	ioctlRet = SceNetInternal_333F9CB3(sock, NET_SIOCDIFADDR, &req);
	if (ioctlRet == NET_ERR_EADDRNOTAVAIL)
		ret = 0;
	else
		ret = (ioctlRet < 0) ? NETCFG_ERR_DEL_IFADDR : 0;

	sceNetSocketClose(sock);
	return ret;
}

static int configSetIfaddr(int ifId, const SceNetInAddr *ipAddr,
			   const SceNetInAddr *peerAddr,
			   const SceNetInAddr *netmask, int keepExisting)
{
	const char *ifName = SceNetInternal_F3917021(ifId);
	int sock;
	int ioctlRet;
	int ret;

	if (keepExisting == 0)
		sceNetConfigDelIfaddr(ifId);

	sock = sceNetSocket("SceNetConfigSetIfaddr", NET_AF_INET, NET_SOCK_DGRAM, 0);
	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "config_set_ifaddr", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetIfAliasReq aliasReq;
	sceClibMemset(&aliasReq, 0, sizeof(aliasReq));
	sceClibStrncpy(aliasReq.ifraName, ifName, sizeof(aliasReq.ifraName) - 1);

	if (ipAddr != NULL) {
		aliasReq.addr.sin_len = sizeof(SceNetSockaddrIn);
		aliasReq.addr.sin_family = NET_AF_INET;
		aliasReq.addr.sin_addr = *ipAddr;
	}
	if (netmask != NULL) {
		aliasReq.mask.sin_len = sizeof(SceNetSockaddrIn);
		aliasReq.mask.sin_family = NET_AF_INET;
		aliasReq.mask.sin_addr = *netmask;
	}
	if (peerAddr != NULL) {
		aliasReq.broadAddr.sin_len = sizeof(SceNetSockaddrIn);
		aliasReq.broadAddr.sin_family = NET_AF_INET;
		aliasReq.broadAddr.sin_addr = *peerAddr;
	}

	ioctlRet = SceNetInternal_333F9CB3(sock, NET_SIOCAIFADDR, &aliasReq);
	ret = (ioctlRet < 0) ? NETCFG_ERR_SET_IFADDR : ioctlRet;

	sceNetSocketClose(sock);
	return ret;
}

int sceNetConfigSetIfaddr(int ifId, const void *ipAddr, const void *peerAddr, const void *netmask)
{
	return configSetIfaddr(ifId, (const SceNetInAddr *)ipAddr,
			       (const SceNetInAddr *)peerAddr,
			       (const SceNetInAddr *)netmask, 0);
}

int sceNetConfigAddIfaddr(int ifId, const void *ipAddr, const void *peerAddr, const void *netmask)
{
	return configSetIfaddr(ifId, (const SceNetInAddr *)ipAddr,
			       (const SceNetInAddr *)peerAddr,
			       (const SceNetInAddr *)netmask, 1);
}

static int sendRouteMessage(SceUInt8 msgType, const SceNetSockaddr *dst,
			    const SceNetSockaddr *gateway,
			    const SceNetSockaddr *netmask, int routeFlags)
{
	int sock = sceNetSocket("SceNetConfigSetRoute", NET_AF_ROUTE, NET_SOCK_RAW, 0);
	int ret;

	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "set_route", sock);
		return NETCFG_ERR_SOCKET;
	}

	NetRouteMessage msg;
	sceClibMemset(&msg, 0, sizeof(msg));

	g_netConfigRouteSeq++;
	msg.hdr.rtmVersion = NET_RTM_VERSION;
	msg.hdr.rtmType = msgType;
	msg.hdr.rtmFlags = routeFlags;
	msg.hdr.rtmSeq = g_netConfigRouteSeq;

	SceUInt8 *cursor = msg.sockaddrs;
	if (dst != NULL) {
		SceSize step = roundSockaddrLen(dst->sa_len);
		msg.hdr.rtmAddrs |= (int)NET_RTA_DST;
		sceClibMemmove(cursor, dst, step);
		cursor += step;
	}
	if (gateway != NULL) {
		SceSize step = roundSockaddrLen(gateway->sa_len);
		msg.hdr.rtmAddrs |= (int)NET_RTA_GATEWAY;
		sceClibMemmove(cursor, gateway, step);
		cursor += step;
	}
	if (netmask != NULL) {
		SceSize step = roundSockaddrLen(netmask->sa_len);
		msg.hdr.rtmAddrs |= (int)NET_RTA_NETMASK;
		sceClibMemmove(cursor, netmask, step);
		cursor += step;
	}

	msg.hdr.rtmMsglen = (SceUInt16)(sizeof(NetRouteMsgHdr) + (SceSize)(cursor - msg.sockaddrs));
	ret = (sceNetSend(sock, &msg, msg.hdr.rtmMsglen, 0) < 0) ? NETCFG_ERR_SET_ROUTE : 0;

	sceNetSocketClose(sock);
	return ret;
}

static int arpRouteExchange(int sock, NetRouteMessage *msg, int msgType,
			    const SceNetSockaddr *dst, const SceNetSockaddr *gateway,
			    int routeFlags, int seq)
{
	SceSize sendLen;

	sceClibMemset(msg, 0, sizeof(*msg));
	msg->hdr.rtmFlags = routeFlags;
	msg->hdr.rtmVersion = NET_RTM_VERSION;

	if (msgType == NET_RTM_DELETE) {
		sendLen = msg->hdr.rtmMsglen;
	} else {
		if (msgType == NET_RTM_GET) {
			msg->hdr.rtmAddrs = (int)NET_RTA_DST;
		} else if (msgType == NET_RTM_ADD) {
			msg->hdr.rtmAddrs = (int)NET_RTA_GATEWAY;
			msg->hdr.rtmInits = NET_RTV_EXPIRE;
			msg->hdr.rtmRmx.expire = 0;
		} else {
			return 0;
		}

		SceUInt8 *cursor = msg->sockaddrs;
		if (dst != NULL) {
			SceSize step = roundSockaddrLen(dst->sa_len);
			msg->hdr.rtmAddrs |= (int)NET_RTA_DST;
			sceClibMemmove(cursor, dst, step);
			cursor += step;
		}
		if (gateway != NULL) {
			SceSize step = roundSockaddrLen(gateway->sa_len);
			msg->hdr.rtmAddrs |= (int)NET_RTA_GATEWAY;
			sceClibMemmove(cursor, gateway, step);
			cursor += step;
		}

		sendLen = sizeof(NetRouteMsgHdr) + (SceSize)(cursor - msg->sockaddrs);
		msg->hdr.rtmMsglen = (SceUInt16)sendLen;
	}

	msg->hdr.rtmSeq = seq;
	msg->hdr.rtmType = (SceUInt8)msgType;

	if (sceNetSend(sock, msg, sendLen, 0) < 0)
		return NETCFG_ERR_ROUTE_SEND;

	for (;;) {
		int recvRet = sceNetRecv(sock, msg, sizeof(*msg), 0);
		if (recvRet <= 0)
			return (recvRet != 0) ? NETCFG_ERR_ROUTE_RECV : 0;
		if (msg->hdr.rtmSeq == seq)
			return 0;
	}
}

int sceNetConfigAddRoute(const void *dstAddr, const void *gateway, const void *netmask, int isHost)
{
	int routeFlags = (isHost != 0)
		? (NET_RTF_UP | NET_RTF_GATEWAY | NET_RTF_STATIC | 8)
		: (NET_RTF_UP | NET_RTF_GATEWAY | NET_RTF_STATIC);
	SceNetSockaddrIn dstSa;
	SceNetSockaddrIn gwSa;
	SceNetSockaddrIn maskSa;

	sceClibMemset(&dstSa, 0, sizeof(dstSa));
	sceClibMemset(&gwSa, 0, sizeof(gwSa));
	sceClibMemset(&maskSa, 0, sizeof(maskSa));

	dstSa.sin_len = sizeof(dstSa);
	dstSa.sin_family = NET_AF_INET;
	dstSa.sin_addr = *(const SceNetInAddr *)dstAddr;

	gwSa.sin_len = sizeof(gwSa);
	gwSa.sin_family = NET_AF_INET;
	gwSa.sin_addr = *(const SceNetInAddr *)gateway;

	maskSa.sin_len = sizeof(maskSa);
	maskSa.sin_family = NET_AF_INET;
	maskSa.sin_addr = *(const SceNetInAddr *)netmask;

	int ret = sendRouteMessage(NET_RTM_ADD, (const SceNetSockaddr *)&dstSa,
				   (const SceNetSockaddr *)&gwSa,
				   (const SceNetSockaddr *)&maskSa, routeFlags);
	if (ret < 0) {
		SceNetInternal_694F8996("%s:ret=%x,dst=%x,gate=%x,mask=%x,flags=%x\n",
					"sceNetConfigAddRoute", ret, dstSa.sin_addr.s_addr,
					gwSa.sin_addr.s_addr, maskSa.sin_addr.s_addr, routeFlags);
	}
	return ret;
}

int sceNetConfigAddMRoute(const void *gateway)
{
	SceNetSockaddrIn dstSa;
	SceNetSockaddrIn gwSa;
	SceNetSockaddrIn maskSa;

	sceClibMemset(&dstSa, 0, sizeof(dstSa));
	sceClibMemset(&gwSa, 0, sizeof(gwSa));
	sceClibMemset(&maskSa, 0, sizeof(maskSa));

	dstSa.sin_len = sizeof(dstSa);
	dstSa.sin_family = NET_AF_INET;
	dstSa.sin_addr.s_addr = sceNetHtonl(0xE0000000u);

	gwSa.sin_len = sizeof(gwSa);
	gwSa.sin_family = NET_AF_INET;
	gwSa.sin_addr = *(const SceNetInAddr *)gateway;

	maskSa.sin_len = sizeof(maskSa);
	maskSa.sin_family = NET_AF_INET;
	maskSa.sin_addr.s_addr = sceNetHtonl(0xF0000000u);

	int ret = sendRouteMessage(NET_RTM_ADD, (const SceNetSockaddr *)&dstSa,
				   (const SceNetSockaddr *)&gwSa,
				   (const SceNetSockaddr *)&maskSa,
				   NET_RTF_UP | NET_RTF_STATIC);
	if (ret < 0) {
		SceNetInternal_694F8996("%s:ret=%x,dst=%x,gate=%x,mask=%x,flags=%x\n",
					"sceNetConfigAddMRoute", ret, dstSa.sin_addr.s_addr,
					gwSa.sin_addr.s_addr, maskSa.sin_addr.s_addr,
					NET_RTF_UP | NET_RTF_STATIC);
	}
	return ret;
}

int sceNetConfigDelRoute(const void *dstAddr, const void *gateway, const void *netmask)
{
	SceNetSockaddrIn dstSa;
	SceNetSockaddrIn gwSa;
	SceNetSockaddrIn maskSa;

	sceClibMemset(&dstSa, 0, sizeof(dstSa));
	sceClibMemset(&gwSa, 0, sizeof(gwSa));
	sceClibMemset(&maskSa, 0, sizeof(maskSa));

	dstSa.sin_len = sizeof(dstSa);
	dstSa.sin_family = NET_AF_INET;
	dstSa.sin_addr = *(const SceNetInAddr *)dstAddr;

	gwSa.sin_len = sizeof(gwSa);
	gwSa.sin_family = NET_AF_INET;
	gwSa.sin_addr = *(const SceNetInAddr *)gateway;

	maskSa.sin_len = sizeof(maskSa);
	maskSa.sin_family = NET_AF_INET;
	maskSa.sin_addr = *(const SceNetInAddr *)netmask;

	int ret = sendRouteMessage(NET_RTM_DELETE, (const SceNetSockaddr *)&dstSa,
				   (const SceNetSockaddr *)&gwSa,
				   (const SceNetSockaddr *)&maskSa,
				   NET_RTF_GATEWAY | NET_RTF_STATIC);
	if (ret < 0) {
		SceNetInternal_694F8996("%s:ret=%x,dst=%x,gate=%x,mask=%x,flags=%x\n",
					"del_route", ret, dstSa.sin_addr.s_addr,
					gwSa.sin_addr.s_addr, maskSa.sin_addr.s_addr,
					NET_RTF_GATEWAY | NET_RTF_STATIC);
	}
	return ret;
}

int sceNetConfigDelMRoute(const void *gateway)
{
	SceNetSockaddrIn dstSa;
	SceNetSockaddrIn gwSa;
	SceNetSockaddrIn maskSa;

	sceClibMemset(&dstSa, 0, sizeof(dstSa));
	sceClibMemset(&gwSa, 0, sizeof(gwSa));
	sceClibMemset(&maskSa, 0, sizeof(maskSa));

	dstSa.sin_len = sizeof(dstSa);
	dstSa.sin_family = NET_AF_INET;
	dstSa.sin_addr.s_addr = sceNetHtonl(0xE0000000u);

	gwSa.sin_len = sizeof(gwSa);
	gwSa.sin_family = NET_AF_INET;
	gwSa.sin_addr = *(const SceNetInAddr *)gateway;

	maskSa.sin_len = sizeof(maskSa);
	maskSa.sin_family = NET_AF_INET;
	maskSa.sin_addr.s_addr = sceNetHtonl(0xF0000000u);

	int ret = sendRouteMessage(NET_RTM_DELETE, (const SceNetSockaddr *)&dstSa,
				   (const SceNetSockaddr *)&gwSa,
				   (const SceNetSockaddr *)&maskSa, NET_RTF_STATIC);
	if (ret < 0) {
		SceNetInternal_694F8996("%s:ret=%x,dst=%x,gate=%x,mask=%x,flags=%x\n",
					"sceNetConfigDelMRoute", ret, dstSa.sin_addr.s_addr,
					gwSa.sin_addr.s_addr, maskSa.sin_addr.s_addr,
					NET_RTF_STATIC);
	}
	return ret;
}

int sceNetConfigDelDefaultRoute(void)
{
	SceNetSockaddrIn dstSa;
	SceNetSockaddrIn maskSa;

	sceClibMemset(&dstSa, 0, sizeof(dstSa));
	sceClibMemset(&maskSa, 0, sizeof(maskSa));

	dstSa.sin_len = sizeof(dstSa);
	dstSa.sin_family = NET_AF_INET;
	maskSa.sin_len = sizeof(maskSa);
	maskSa.sin_family = NET_AF_INET;

	int ret = sendRouteMessage(NET_RTM_DELETE, (const SceNetSockaddr *)&dstSa,
				   NULL, (const SceNetSockaddr *)&maskSa,
				   NET_RTF_GATEWAY | NET_RTF_STATIC);
	if (ret < 0)
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigDelDefaultRoute", ret);
	return ret;
}

int sceNetConfigSetDefaultRoute(const void *gateway)
{
	SceNetInAddr zeroDst;
	SceNetInAddr zeroMask;

	sceNetConfigDelDefaultRoute();
	zeroDst.s_addr = 0;
	zeroMask.s_addr = 0;

	int ret = sceNetConfigAddRoute(&zeroDst, gateway, &zeroMask, 0);
	if (ret < 0)
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigSetDefaultRoute", ret);
	return ret;
}

int sceNetConfigGetDefaultRoute(SceNetInAddr *outGateway)
{
	NetRouteTableEntry entry;
	int ret = SceNetInternal_BDA92B76(NET_AF_INET, &entry, 1, 1);

	if (ret < 0)
		return ret;
	if (ret != 1)
		return NETCFG_ERR_NO_DEFAULT_ROUTE;

	*outGateway = entry.gateway.in.sin_addr;
	return ret;
}

int netConfigFlushRoutes(int ifId)
{
	int count = 0;
	NetRouteTableEntry *entries = SceNetInternal_5B0E370C(ifId, &count);
	int ret;

	if (entries != NULL) {
		for (int index = 0; index < count; index++) {
			NetRouteTableEntry *route = &entries[index];
			if ((route->flags & NET_RTF_GATEWAY) != 0)
				continue;
			sendRouteMessage(NET_RTM_DELETE, &route->destination.sa,
					 &route->gateway.sa, &route->netmask.sa, route->flags);
		}
		ret = 0;
	} else {
		ret = NETCFG_ERR_FLUSH_ROUTES;
	}

	SceNetInternal_CF49E9C4(entries);
	return ret;
}

int sceNetConfigAddArp(const SceNetEtherAddr *etherAddr, const SceNetInAddr *ipAddr)
{
	int sock = sceNetSocket("SceNetConfigAddArp", NET_AF_ROUTE, NET_SOCK_RAW, 0);
	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigAddArp", sock);
		return NETCFG_ERR_SOCKET;
	}

	SceNetSockaddrIn targetSa;
	sceClibMemset(&targetSa, 0, sizeof(targetSa));
	targetSa.sin_len = sizeof(targetSa);
	targetSa.sin_family = NET_AF_INET;
	targetSa.sin_addr = *ipAddr;

	NetRouteMessage msg;
	g_netConfigArpSeq++;
	int ret = arpRouteExchange(sock, &msg, NET_RTM_GET,
				   (const SceNetSockaddr *)&targetSa, NULL, 0,
				   g_netConfigArpSeq);
	if (ret >= 0) {
		const SceNetSockaddrIn *respDst = (const SceNetSockaddrIn *)msg.sockaddrs;
		SceSize dstStep = roundSockaddrLen(respDst->sin_len);
		const NetSockaddrDl *respGw = (const NetSockaddrDl *)(msg.sockaddrs + dstStep);

		bool canAdd = false;
		if (respDst->sin_addr.s_addr == targetSa.sin_addr.s_addr) {
			if (respGw->sdl_family == NET_AF_LINK &&
			    (msg.hdr.rtmFlags & (NET_RTF_LLINFO | NET_RTF_GATEWAY)) == NET_RTF_LLINFO &&
			    respGw->sdl_type == NET_IFT_ETHER) {
				canAdd = true;
			}
		} else if (respGw->sdl_family == NET_AF_LINK) {
			canAdd = true;
		}

		if (canAdd) {
			NetSockaddrDl linkSa;
			sceClibMemset(&linkSa, 0, sizeof(linkSa));
			linkSa.sdl_len = sizeof(linkSa);
			linkSa.sdl_family = NET_AF_LINK;
			linkSa.sdl_type = respGw->sdl_type;
			linkSa.sdl_index = respGw->sdl_index;
			sceClibMemcpy_safe(&linkSa.sdl_data[linkSa.sdl_nlen],
					   etherAddr, sizeof(SceNetEtherAddr));
			linkSa.sdl_alen = sizeof(SceNetEtherAddr);

			g_netConfigArpSeq++;
			ret = arpRouteExchange(sock, &msg, NET_RTM_ADD,
					       (const SceNetSockaddr *)&targetSa,
					       (const SceNetSockaddr *)&linkSa,
					       NET_RTF_STATIC | NET_RTF_HOST,
					       g_netConfigArpSeq);
		} else {
			ret = NETCFG_ERR_ADD_ARP;
		}
	}

	sceNetSocketClose(sock);
	return ret;
}

int sceNetConfigDelArp(int unusedIfId, const SceNetInAddr *ipAddr)
{
	(void)unusedIfId;
	int sock = sceNetSocket("SceNetConfigDelArp", NET_AF_ROUTE, NET_SOCK_RAW, 0);
	if (sock < 0) {
		SceNetInternal_694F8996("%s:ret=%x\n", "sceNetConfigDelArp", sock);
		return NETCFG_ERR_SOCKET;
	}

	SceNetSockaddrIn targetSa;
	sceClibMemset(&targetSa, 0, sizeof(targetSa));
	targetSa.sin_len = sizeof(targetSa);
	targetSa.sin_family = NET_AF_INET;
	targetSa.sin_addr = *ipAddr;

	NetRouteMessage msg;
	g_netConfigArpSeq++;
	int ret = arpRouteExchange(sock, &msg, NET_RTM_GET,
				   (const SceNetSockaddr *)&targetSa, NULL, 0,
				   g_netConfigArpSeq);
	if (ret >= 0) {
		const SceNetSockaddrIn *respDst = (const SceNetSockaddrIn *)msg.sockaddrs;
		SceSize dstStep = roundSockaddrLen(respDst->sin_len);
		const NetSockaddrDl *respGw = (const NetSockaddrDl *)(msg.sockaddrs + dstStep);

		if (respDst->sin_addr.s_addr == targetSa.sin_addr.s_addr) {
			if (respGw->sdl_family == NET_AF_LINK &&
			    (msg.hdr.rtmFlags & (NET_RTF_LLINFO | NET_RTF_GATEWAY)) == NET_RTF_LLINFO &&
			    respGw->sdl_type == NET_IFT_ETHER) {
				g_netConfigArpSeq++;
				ret = arpRouteExchange(sock, &msg, NET_RTM_DELETE, NULL, NULL, 0,
						       g_netConfigArpSeq);
			} else {
				ret = NETCFG_ERR_DEL_ARP;
			}
		}
	}

	sceNetSocketClose(sock);
	return ret;
}
