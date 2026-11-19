/*
 * Wakes a host (a PC with Content Manager) with a Wake-on-LAN magic packet: six 0xFF bytes,
 * then its MAC address sixteen times, broadcast twice over UDP.
 */

#include <scetypes.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/inet.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <thread/thread.h>

#include "wake_on_lan.h"

#define WOL_PORT		9300
#define WOL_MAC_LENGTH		6
#define WOL_MAC_REPEATS		16
#define WOL_SYNC_LENGTH		6
#define WOL_PACKET_LENGTH	(WOL_SYNC_LENGTH + WOL_MAC_LENGTH * WOL_MAC_REPEATS)
#define WOL_SEND_COUNT		2
#define WOL_SEND_INTERVAL	2	/* between the sends */

/* a UDP socket that may broadcast */
static inline int openBroadcastSocket(int *sock)
{
	int optval;
	int ret;
	int s;

	s = sceNetSocket("WOL", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (s < 0) {
		sce_paf_printf_dummy();
		return s;
	}
	optval = 1;
	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
		sizeof(optval));
	if (ret < 0) {
		sce_paf_printf_dummy();
		sceNetSocketClose(s);
		return ret;
	}
	*sock = s;
	return 0;
}

static inline int sendPacket(int sock, const SceUInt8 *packet)
{
	SceNetSockaddrIn addr;
	int ret;

	sce_paf_memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(WOL_PORT);
	addr.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;
	ret = sceNetSendto(sock, packet, WOL_PACKET_LENGTH, SCE_NET_MSG_DONTWAIT,
		(const SceNetSockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		sce_paf_printf_dummy();
		return ret;
	}
	return 0;
}

static __attribute__((noinline)) int sendMagicPacket(const SceUInt8 *mac)
{
	SceUInt8 packet[WOL_PACKET_LENGTH];
	SceUInt8 *p;
	int sock = -1;
	int ret;
	int i;

	ret = openBroadcastSocket(&sock);
	if (ret < 0)
		return ret;
	packet[0] = 0xFF;
	packet[1] = 0xFF;
	packet[2] = 0xFF;
	packet[3] = 0xFF;
	packet[4] = 0xFF;
	packet[5] = 0xFF;
	p = &packet[WOL_SYNC_LENGTH];
	for (i = WOL_MAC_REPEATS; i != 0; i--) {
		sce_paf_memcpy(p, mac, WOL_MAC_LENGTH);
		p += WOL_MAC_LENGTH;
	}
	for (i = 0; ; ) {
		ret = sendPacket(sock, packet);
		if (ret != 0)
			break;
		if (++i >= WOL_SEND_COUNT) {
			ret = 0;
			break;
		}
		paf::thread::Sleep(WOL_SEND_INTERVAL);
	}
	if (sceNetSocketClose(sock) < 0)
		sce_paf_printf_dummy();
	return ret;
}

int sceHostCollaboUtilSendWakeOnLanRequestTo(const SceUInt8 *mac)
{
	return sendMagicPacket(mac);
}
