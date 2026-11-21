/*
 * The receive job: one packet from the PS4, its header first and then the rest, decrypted
 * when the type is one the PS4 encrypts, and what each type must carry checked. The hello
 * gives both IVs.
 */
#include <libkernel/libkernel.h>
#include <net/socket.h>
#include <net/errno.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

#define COMPANION_UTIL_TEXT_MAX	0x1000

static union {
	CompanionUtilPacket packet;
	SceUInt8 data[COMPANION_UTIL_BUFFER_SIZE];
} s_recvBuffer;

int CTcpRecvJob::Request(CompanionUtilPacket *header, void *body, bool decrypt)
{
	if (m_state == JOB_RUNNING)
		return -1;
	m_header = header;
	sceClibMemset(header, 0, COMPANION_UTIL_PACKET_SIZE);
	m_header->type = 0;
	m_bodyBuffer = body;
	if (body != NULL)
		sceClibMemset(body, 0, COMPANION_UTIL_BODY_SIZE);
	m_decrypt = decrypt;
	m_body = NULL;
	m_bodySize = 0;
	m_state = JOB_RUNNING;
	return 0;
}

int CTcpRecvJob::Run(void)
{
	CompanionUtilPacket *packet = &s_recvBuffer.packet;
	SceUInt8 *rest = &s_recvBuffer.data[16];
	const void *body;
	int size, length;

	sceClibMemset(&s_recvBuffer, 0, sizeof(s_recvBuffer));
	if (sceNetRecv(g_tcpSocket, &s_recvBuffer, 16, SCE_NET_MSG_WAITALL) <= 0) {
		sceNetErrnoLoc();
		return COMPANION_UTIL_NET_ERROR_IO;
	}
	if (m_decrypt)
		Decrypt(s_recvBuffer.data, 16);
	size = packet->size;
	if (size <= 0 || size > COMPANION_UTIL_BUFFER_SIZE)
		return COMPANION_UTIL_NET_ERROR_BAD_SIZE;

	/* the types the PS4 encrypts */
	switch ((SceUInt32)packet->type) {
	case 3:
	case 7:
	case 9:
	case 11:
	case 13:
	case 14:
	case 16:
	case 18:
	case 22:
	case 24:
	case 27:
	case 28:
	case 32:
	case 35:
	case 37:
	case COMPANION_UTIL_PACKET_HELLO:
		break;
	default:
		m_decrypt = false;
		break;
	}

	if (size > 16) {
		size -= 16;
		if (m_decrypt)
			size = GetPaddedSize(size);
		if (sceNetRecv(g_tcpSocket, rest, size, SCE_NET_MSG_WAITALL) <= 0)
			return COMPANION_UTIL_NET_ERROR_IO;
		if (m_decrypt)
			Decrypt(rest, size);
	}

	switch ((SceUInt32)packet->type) {
	case COMPANION_UTIL_PACKET_HELLO:
		if (packet->arg[1] != 0)
			return COMPANION_UTIL_NET_ERROR_BAD_HELLO;
		body = &packet->arg[3];
		SetDecryptIv(body);
		SetEncryptIv(body);
		return 0;
	case 7:
		if (packet->arg[0] != 0)
			return COMPANION_UTIL_NET_ERROR_REFUSED;
		return 0;
	case 11:
	case 18:
	case 22:
	case 24:
	case 27:
	case 28:
	case 32:
	case 35:
		return 0;
	case 13:
		body = &packet->arg[3];
		length = GetTextSize(packet);
		if (length > COMPANION_UTIL_TEXT_MAX)
			length = COMPANION_UTIL_TEXT_MAX;
		if (length > 0) {
			m_body = body;
			m_bodySize = length;
		}
		return 0;
	case 14:
		body = &packet->arg[5];
		length = GetBodySize(packet);
		if (length > COMPANION_UTIL_TEXT_MAX)
			length = COMPANION_UTIL_TEXT_MAX;
		if (length > 0) {
			m_body = body;
			m_bodySize = length;
		}
		return 0;
	case 16:
		if (packet->arg[0] == 1)
			return 0;
		return COMPANION_UTIL_NET_ERROR_UNEXPECTED;
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
	case 9:
	case 10:
	case 12:
	case 15:
	case 17:
	case 19:
	case 20:
	case 21:
	case 23:
	case 25:
	case 26:
	case 29:
	case 30:
	case 31:
	case 33:
	case 34:
	case 36:
	case 37:
		return COMPANION_UTIL_NET_ERROR_UNEXPECTED;
	default:
		return COMPANION_UTIL_NET_ERROR_UNEXPECTED;
	}
}

void CTcpRecvJob::Finish(int result)
{
	int lock;

	if (m_header == NULL)
		return;
	if (m_bodyBuffer == NULL)
		return;
	lock = LockMutex();
	if (lock < 0)
		return;
	m_result = result;
	if (result != 0) {
		sceClibMemcpy_safe(m_header, &s_recvBuffer, COMPANION_UTIL_PACKET_SIZE);
	} else {
		sceClibMemcpy_safe(m_header, &s_recvBuffer, COMPANION_UTIL_PACKET_SIZE);
		if (m_bodyBuffer != NULL && m_body != NULL) {
			if (m_bodySize > COMPANION_UTIL_TEXT_MAX)
				m_bodySize = COMPANION_UTIL_TEXT_MAX;
			sceClibMemcpy_safe(m_bodyBuffer, m_body, m_bodySize);
		}
	}
	m_bodySize = 0;
	m_header = NULL;
	m_bodyBuffer = NULL;
	m_body = NULL;
	m_state = JOB_DONE;
	UnlockMutex(lock);
}

} /* namespace SceCompanionUtil */
