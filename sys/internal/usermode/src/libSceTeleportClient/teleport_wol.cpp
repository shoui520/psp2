/*
 * teleport_wol.cpp: Wake-on-LAN magic packet broadcast (0x81003664-0x8100377B).
 */
#include "teleport_internal.h"

int teleportSendMagicPacket(const void *macAddr)
{
	int sockFd = -1;
	int ret = sceNetSocket("SceTeleportWol", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (ret >= 0) {
		int optBroadcast = 1;
		int optRet = sceNetSetsockopt(ret, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST,
					      &optBroadcast, sizeof(optBroadcast));
		if (optRet >= 0) {
			sockFd = ret;
			ret = 0;
		} else {
			sceNetSocketClose(ret);
			ret = optRet;
		}
	}

	if (ret >= 0) {
		SceUInt8 packet[6 + 16 * 6];
		packet[0] = 0xFFu;
		packet[1] = 0xFFu;
		packet[2] = 0xFFu;
		packet[3] = 0xFFu;
		packet[4] = 0xFFu;
		packet[5] = 0xFFu;

		SceUInt8 *dst = packet + 6;
		for (int i = 0; i < 16; ++i) {
			sceClibMemcpy_safe(dst, macAddr, 6);
			dst += 6;
		}

		for (int attempt = 0; attempt < 2; ++attempt) {
			SceNetSockaddrIn bcastAddr;
			sceClibMemset(&bcastAddr, 0, sizeof(bcastAddr));
			bcastAddr.sin_len = sizeof(bcastAddr);
			bcastAddr.sin_family = SCE_NET_AF_INET;
			bcastAddr.sin_port = sceNetHtons(TELEPORT_WOL_PORT);
			bcastAddr.sin_addr.s_addr = 0xFFFFFFFFu;

			int sent = sceNetSendto(sockFd, packet, sizeof(packet),
						SCE_NET_MSG_DONTWAIT,
						(const SceNetSockaddr *)&bcastAddr,
						sizeof(bcastAddr));
			ret = (sent < 0) ? sent : 0;
			if (ret != 0)
				break;
			if (attempt + 1 >= 2) {
				ret = 0;
				break;
			}
			sceKernelDelayThread(2000);
		}
		sceNetSocketClose(sockFd);
	}

	return ret;
}

int teleportSendWakeOnLan(const void *macAddr)
{
	return teleportSendMagicPacket(macAddr);
}
