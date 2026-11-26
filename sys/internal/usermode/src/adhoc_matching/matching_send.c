/*
 * SceNetAdhocMatching packet builders and transmission socket (0x81002040..0x81002750).
 */
#include "matching_internal.h"

int matchingSendSocketOpen(AdhocMatchingContext *ctx)
{
	SceNetSockaddrIn sin;
	int optval;
	int sock;
	int ret;

	ret = matchingUpdateLocalAddr();
	if (ret < 0)
		return ret;

	sock = sceNetSocket("SceNetAdhocMatchingSend", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0);
	if (sock < 0)
		return sock;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)(ctx->port + 1));

	ret = sceNetBind(sock, (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret >= 0) {
		optval = 1;
		ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST,
				       &optval, sizeof(optval));
		if (ret >= 0) {
			ctx->sendSock = sock;
			return 0;
		}
	}

	if (sock > 0) {
		sceNetShutdown(sock, 0);
		sceNetSocketClose(sock);
	}
	return ret;
}

int matchingSendSocketClose(AdhocMatchingContext *ctx)
{
	if (SceLibKernel_9F793F84() >= ADHOC_MATCHING_SDK_V150)
		sceNetSocketAbort(ctx->sendSock, 0);
	sceNetShutdown(ctx->sendSock, 0);
	sceNetSocketClose(ctx->sendSock);
	ctx->sendSock = 0;
	return 0;
}

int matchingSendControlPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int pktType,
			      int optlen, const void *opt)
{
	SceSize packetSize;
	unsigned char *packet;
	SceUShort16 netLen;
	unsigned int netSessionId;
	SceNetSockaddrIn sin;
	int flags;
	int ret;

	packetSize = optlen + 4;
	if (pktType == ADHOC_MATCHING_PKT_REQUEST || pktType == ADHOC_MATCHING_PKT_ACCEPT)
		packetSize = optlen + 20;

	packet = matchingAlloc(packetSize);
	if (packet == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	packet[0] = ADHOC_MATCHING_PROTOCOL_VERSION;
	packet[1] = (unsigned char)pktType;
	netLen = sceNetHtons((SceUShort16)optlen);
	sceClibMemcpy_safe(packet + 2, &netLen, sizeof(netLen));
	if (optlen > 0)
		sceClibMemcpy_safe(packet + 4, opt, optlen);
	if (pktType == ADHOC_MATCHING_PKT_REQUEST || pktType == ADHOC_MATCHING_PKT_ACCEPT) {
		netSessionId = sceNetHtonl(peer->localSessionId);
		sceClibMemcpy_safe(packet + optlen + 4, &netSessionId, sizeof(netSessionId));
		sceClibMemset(packet + optlen + 8, 0, 12);
	}

	flags = SCE_NET_MSG_USECRYPTO;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		flags |= SCE_NET_MSG_DONTWAIT;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)ctx->port);
	sin.sin_addr = peer->addr;

	ret = sceNetSendto(ctx->sendSock, packet, packetSize, flags,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	matchingFree(packet);
	if (ret == SCE_NET_ERROR_EAGAIN)
		ret = 0;
	return ret;
}

int matchingSendDataPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int pktType,
			   int datalen, const void *data)
{
	unsigned char *packet;
	SceUShort16 netLen;
	unsigned int netWord;
	SceNetSockaddrIn sin;
	int flags;
	int ret;

	packet = matchingAlloc(datalen + 12);
	if (packet == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	packet[0] = ADHOC_MATCHING_PROTOCOL_VERSION;
	packet[1] = (unsigned char)pktType;
	netLen = sceNetHtons((SceUShort16)(datalen + 8));
	sceClibMemcpy_safe(packet + 2, &netLen, sizeof(netLen));

	netWord = sceNetHtonl(peer->localSessionId);
	sceClibMemcpy_safe(packet + 4, &netWord, sizeof(netWord));

	if (pktType == ADHOC_MATCHING_PKT_DATA)
		netWord = sceNetHtonl(peer->sendDataSeq);
	else
		netWord = sceNetHtonl(peer->recvDataSeq - 1);
	sceClibMemcpy_safe(packet + 8, &netWord, sizeof(netWord));

	if (datalen > 0)
		sceClibMemcpy_safe(packet + 12, data, datalen);

	flags = SCE_NET_MSG_USECRYPTO;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		flags |= SCE_NET_MSG_DONTWAIT;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)ctx->port);
	sin.sin_addr = peer->addr;

	ret = sceNetSendto(ctx->sendSock, packet, datalen + 12, flags,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	matchingFree(packet);
	if (ret == SCE_NET_ERROR_EAGAIN)
		ret = 0;
	return ret;
}

int matchingBuildHelloPacket(AdhocMatchingContext *ctx, int optlen, const void *opt)
{
	int packetSize = optlen + 28;
	unsigned char *packet;
	SceUShort16 netLen;
	unsigned int netWord;

	packet = matchingAlloc(packetSize);
	if (packet == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	packet[0] = ADHOC_MATCHING_PROTOCOL_VERSION;
	packet[1] = ADHOC_MATCHING_PKT_HELLO;
	netLen = sceNetHtons((SceUShort16)(optlen + 8));
	sceClibMemcpy_safe(packet + 2, &netLen, sizeof(netLen));

	netWord = sceNetHtonl(ctx->helloInterval);
	sceClibMemcpy_safe(packet + 4, &netWord, sizeof(netWord));
	netWord = sceNetHtonl(ctx->keepaliveInterval);
	sceClibMemcpy_safe(packet + 8, &netWord, sizeof(netWord));

	if (optlen > 0)
		sceClibMemcpy_safe(packet + 12, opt, optlen);

	sceClibMemcpy_safe(packet + optlen + 12, ctx->localCaps, sizeof(ctx->localCaps));
	sceClibMemset(packet + optlen + 16, 0, 12);

	if (ctx->helloPacket != NULL)
		matchingFree(ctx->helloPacket);
	ctx->helloPacketLen = packetSize;
	ctx->helloPacket = packet;
	return 0;
}

int matchingFreeHelloPacket(AdhocMatchingContext *ctx)
{
	if (ctx->helloPacketLen > 0) {
		matchingFree(ctx->helloPacket);
		ctx->helloPacketLen = 0;
		ctx->helloPacket = NULL;
		return 0;
	}
	return ctx->helloPacketLen;
}

int matchingSendHello(AdhocMatchingContext *ctx)
{
	SceNetSockaddrIn sin;
	int flags;
	int ret;

	flags = SCE_NET_MSG_USECRYPTO;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		flags |= SCE_NET_MSG_DONTWAIT;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)ctx->port);
	sin.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;

	ret = sceNetSendto(ctx->sendSock, ctx->helloPacket, ctx->helloPacketLen, flags,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret == SCE_NET_ERROR_EAGAIN)
		ret = 0;
	return ret;
}

int matchingBuildKeepalivePacket(AdhocMatchingContext *ctx)
{
	int peerCount;
	SceSize packetSize;
	unsigned char *packet;
	SceUShort16 netLen;

	peerCount = matchingCountPeers(ctx, ADHOC_MATCHING_PEER_ESTABLISHED);
	packetSize = (peerCount + 2) * sizeof(SceNetInAddr);
	packet = matchingAlloc(packetSize);
	if (packet == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	packet[0] = ADHOC_MATCHING_PROTOCOL_VERSION;
	packet[1] = ADHOC_MATCHING_PKT_KEEPALIVE;
	netLen = sceNetHtons((SceUShort16)((peerCount + 1) * sizeof(SceNetInAddr)));
	sceClibMemcpy_safe(packet + 2, &netLen, sizeof(netLen));
	sceClibMemcpy_safe(packet + 4, &g_adhocMatchingLocalAddr, sizeof(SceNetInAddr));
	matchingCollectPeers(ctx, ADHOC_MATCHING_PEER_ESTABLISHED,
			     (SceNetInAddr *)(packet + 8), &peerCount);

	if (ctx->keepalivePacket != NULL)
		matchingFree(ctx->keepalivePacket);
	ctx->keepalivePacketLen = (int)packetSize;
	ctx->keepalivePacket = packet;
	return 0;
}

int matchingSendKeepalive(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer)
{
	SceNetSockaddrIn sin;
	int flags;
	int ret;

	flags = SCE_NET_MSG_USECRYPTO;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		flags |= SCE_NET_MSG_DONTWAIT;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)ctx->port);
	sin.sin_addr = peer->addr;

	ret = sceNetSendto(ctx->sendSock, ctx->keepalivePacket, ctx->keepalivePacketLen, flags,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret == SCE_NET_ERROR_EAGAIN)
		ret = 0;
	return ret;
}

int matchingSaveKeepalivePacket(AdhocMatchingContext *ctx, const void *packet, SceSize len)
{
	unsigned char *copy = matchingAlloc(len);

	if (copy == NULL)
		return SCE_NET_ADHOC_MATCHING_ERROR_NO_SPACE;

	sceClibMemcpy_safe(copy, packet, len);
	if (ctx->keepalivePacket != NULL)
		matchingFree(ctx->keepalivePacket);
	ctx->keepalivePacketLen = (int)len;
	ctx->keepalivePacket = copy;
	return 0;
}

int matchingFreeKeepalivePacket(AdhocMatchingContext *ctx)
{
	if (ctx->keepalivePacketLen > 0) {
		matchingFree(ctx->keepalivePacket);
		ctx->keepalivePacketLen = 0;
		ctx->keepalivePacket = NULL;
		return 0;
	}
	return ctx->keepalivePacketLen;
}

int matchingSendBye(AdhocMatchingContext *ctx)
{
	unsigned char packet[4];
	SceNetSockaddrIn sin;
	int flags;
	int ret;

	packet[0] = ADHOC_MATCHING_PROTOCOL_VERSION;
	packet[1] = ADHOC_MATCHING_PKT_BYE;
	sceClibMemset(packet + 2, 0, 2);

	flags = SCE_NET_MSG_USECRYPTO;
	if (SceLibKernel_9F793F84() < ADHOC_MATCHING_SDK_V150)
		flags |= SCE_NET_MSG_DONTWAIT;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(ADHOC_MATCHING_UDP_PORT);
	sin.sin_vport = sceNetHtons((SceUShort16)ctx->port);
	sin.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;

	ret = sceNetSendto(ctx->sendSock, packet, sizeof(packet), flags,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret == SCE_NET_ERROR_EAGAIN)
		ret = 0;
	return ret;
}
