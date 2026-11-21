/*
 * The connection's socket (TCP) and the packets the library sends on it: each request type's
 * packet, encrypted under the session key except the hello and the key itself.
 */
#include <libkernel/libkernel.h>
#include <net/socket.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

SceNetId g_tcpSocket = -1;

/* the hello's protocol version, and the 16 bytes that follow it */
int g_helloVersion = 0x00020000;
static SceUInt8 s_helloReserved[16];

int CTcpSocket::Connect(const SceUInt32 *address, const SceUInt16 *port)
{
	int bufferSize;
	int noTimeout;
	int timeout;
	SceNetSockaddrIn sin;
	SceNetId s;

	s = g_tcpSocket;
	bufferSize = 0;
	if (g_tcpSocket < 0) {
		s = sceNetSocket("tcp_socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
		g_tcpSocket = s;
		if (s < 0)
			return -1;
	}
	bufferSize = 0x4000;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVBUF, &bufferSize,
			     sizeof(bufferSize)) < 0)
		return -1;
	timeout = 100000;
	if (sceNetSetsockopt(g_tcpSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &timeout,
			     sizeof(timeout)) < 0)
		return -1;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_addr.s_addr = *address;
	sin.sin_port = *port;
	if (sceNetConnect(g_tcpSocket, (const SceNetSockaddr *)&sin, sizeof(sin)) < 0) {
		sceNetShutdown(g_tcpSocket, SCE_NET_SHUT_RDWR);
		sceNetSocketClose(g_tcpSocket);
		g_tcpSocket = -1;
		return -1;
	}
	noTimeout = 0;
	if (sceNetSetsockopt(g_tcpSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &noTimeout,
			     sizeof(noTimeout)) < 0)
		return -1;
	CreateSession();
	return 0;
}

int CTcpSocket::Close(void)
{
	int ret;

	if (g_tcpSocket < 0)
		return -1;
	DestroySession();
	sceNetShutdown(g_tcpSocket, SCE_NET_SHUT_RDWR);
	ret = sceNetSocketClose(g_tcpSocket);
	g_tcpSocket = -1;
	return ret;
}

int CTcpSocket::Abort(void)
{
	return sceNetSocketAbort(g_tcpSocket, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
}

/* a text's length in characters, at most max */
static inline int TextLength(const SceWChar16 *text, int max)
{
	int i;

	for (i = 0; *text != 0; text++) {
		if (++i >= max)
			return max;
	}
	return i;
}

int CTcpSendJob::BuildPacket(CompanionUtilPacket *packet, const CompanionUtilRequestParam *param)
{
	int composeStart, composeLength, replaceStart, replaceLength;
	SceUInt32 caret;
	const SceWChar16 *text;
	int length;

	switch (m_type) {
	case 0:
		packet->type = COMPANION_UTIL_PACKET_HELLO;
		packet->size = 28;
		packet->arg[0] = g_helloVersion;
		sceClibMemcpy_safe(&packet->arg[1], s_helloReserved, sizeof(s_helloReserved));
		return 0;
	case 4:
		packet->size = 8;
		packet->type = 4;
		return 0;
	case 5:
		packet->size = 44;
		packet->type = 6;
		sceClibMemcpy_safe(&packet->arg[0], "1234567890123456", 16);
		packet->arg[4] = param->value.arg[0];
		return 0;
	case 11:
		packet->size = 24;
		packet->type = 10;
		sceClibMemcpy_safe(&packet->arg[0], param->host.hostId, 16);
		return 0;
	case 14:
		packet->size = 12;
		packet->type = 20;
		packet->arg[0] = 0;
		return 0;
	case 15:
		packet->size = 8;
		packet->type = 12;
		return 0;
	case 17:
		composeStart = param->text.composeStart;
		composeLength = param->text.composeLength;
		caret = param->text.caret;
		replaceStart = param->text.replaceStart;
		replaceLength = param->text.replaceLength;
		text = param->text.text;
		length = TextLength(text, 2048);
		packet->size = length * sizeof(SceWChar16) + 28;
		packet->type = 14;
		packet->arg[0] = composeStart;
		packet->arg[1] = composeLength;
		packet->arg[2] = caret;
		packet->arg[3] = replaceStart;
		packet->arg[4] = replaceLength;
		sceClibMemcpy_safe(&packet->arg[5], text, length * sizeof(SceWChar16));
		return 0;
	case 18:
		packet->size = 12;
		packet->type = 16;
		packet->arg[0] = param->value.arg[0];
		return 0;
	case 21:
		packet->size = 168;
		packet->type = 38;
		GetAppTitle((char *)&packet->arg[0], 0x80);
		GetProtocolVersionString((char *)&packet->arg[32], 0x20);
		return 0;
	case 22:
		packet->size = 8;
		packet->type = 26;
		return 0;
	case 24:
		packet->size = 16;
		packet->type = 28;
		packet->arg[0] = param->value.arg[0];
		packet->arg[1] = param->value.arg[1];
		return 0;
	case 25:
		packet->size = COMPANION_UTIL_PACKET_SIZE;
		packet->type = 30;
		sceClibMemcpy_safe(&packet->arg[0], param->login.registKey,
				   sizeof(param->login.registKey));
		packet->arg[1] = param->login.mode | 0x14;
		packet->arg[2] = param->login.userCredential;
		sceClibSnprintf((char *)&packet->arg[18], 0x100, "PS Vita");
		GetSystemSwVersionString((char *)&packet->arg[82], 0x10);
		GetPsCodeString((char *)&packet->arg[86], 0x10);
		sceClibSnprintf((char *)&packet->arg[90], 0x10, "");
		return 0;
	case 26:
		packet->size = 280;
		packet->type = 32;
		EncryptSessionKey((unsigned char *)&packet->arg[0]);
		sceClibMemcpy_safe(&packet->arg[64], g_encryptIv, sizeof(g_encryptIv));
		return 0;
	case 27:
		packet->size = 8;
		packet->type = 34;
		return 0;
	case 29:
		return 0;
	default:
		return COMPANION_UTIL_NET_ERROR_UNKNOWN_TYPE;
	}
}

int CTcpSendJob::Send(void)
{
	unsigned char *data = (unsigned char *)m_packet;
	bool encrypt = true;
	int size, sent;

	m_sending = true;
	size = m_packet->size;
	switch (m_type) {
	case 0:
	case 26:
		encrypt = false;
		break;
	}
	if (encrypt) {
		size = GetPaddedSize(m_packet->size);
		Encrypt(data, data, size);
	}
	while (size > 0) {
		sent = sceNetSend(g_tcpSocket, data, size, 0);
		if (sent < 0)
			return COMPANION_UTIL_NET_ERROR_IO;
		size -= sent;
		data += sent;
	}
	return 0;
}

} /* namespace SceCompanionUtil */
