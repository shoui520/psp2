/*
 * MTP/IP ("MTPIP:"): the PTP/IP packets of the content manager's network connection. The
 * host opens two TCP connections to the listening port, one for commands and data and one
 * for events, and says which is which with its first packet.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/inet.h>
#include <net/id.h>
#include <net/epoll.h>
#include <net/errno.h>
#include "mtp_internal.h"

#define MTP_TAG_MTPIP			"MTPIP:"

/* the packets that open the two connections */
#define MTP_IP_INIT_COMMAND_REQUEST	1
#define MTP_IP_INIT_COMMAND_ACK		2
#define MTP_IP_INIT_EVENT_REQUEST	3
#define MTP_IP_INIT_EVENT_ACK		4
#define MTP_IP_OPERATION_RESPONSE	7
#define MTP_IP_EVENT			8

/* what determine_socket_type found a connection to be */
#define MTP_IP_SOCKET_UNKNOWN		0
#define MTP_IP_SOCKET_COMMAND		1
#define MTP_IP_SOCKET_EVENT		2

#define MTP_IP_SOCKET_TIMEOUT		15000000
#define MTP_IP_LISTEN_BACKLOG		2
#define MTP_IP_SELECT_TIMEOUT		1000000
#define MTP_IP_SELECT_EVENTS		3
#define MTP_IP_IGNORE_CHUNK		64
#define MTP_IP_GUID_SIZE		16

#define MTP_IP_OPERATION_PARAMS		5
#define MTP_IP_EVENT_PARAMS		3

/* an operation request after its header: the data phase, the code and the transaction */
typedef struct MtpIpOperationRequest {
	MtpIpHeader header;
	SceUInt32 dataPhase;
	SceUInt16 code;
	SceUInt32 transactionId;
	SceUInt32 param[MTP_IP_OPERATION_PARAMS];
} __attribute__((packed, aligned(4))) MtpIpOperationRequest;

typedef struct MtpIpOperationResponse {
	MtpIpHeader header;
	SceUInt16 code;
	SceUInt32 transactionId;
	SceUInt32 param[MTP_IP_OPERATION_PARAMS];
} __attribute__((packed, aligned(4))) MtpIpOperationResponse;

typedef struct MtpIpEvent {
	MtpIpHeader header;
	SceUInt16 code;
	SceUInt32 transactionId;
	SceUInt32 param[MTP_IP_EVENT_PARAMS];
} __attribute__((packed, aligned(4))) MtpIpEvent;

/* a start data packet after its header */
typedef struct MtpIpStartData {
	SceUInt32 transactionId;
	SceUInt64 totalSize;
} __attribute__((packed)) MtpIpStartData;

typedef struct MtpIpStartDataPacket {
	MtpIpHeader header;
	MtpIpStartData data;
} __attribute__((packed)) MtpIpStartDataPacket;

typedef struct MtpIpDataPacket {
	MtpIpHeader header;
	SceUInt32 transactionId;
} MtpIpDataPacket;

typedef struct MtpIpInitCommandAck {
	MtpIpHeader header;
	SceUInt32 connectionNumber;
} MtpIpInitCommandAck;

/* the connections' state; this module reads only the fields named here */
typedef struct MtpIp {
	SceUChar8 reserved[0x116];
	SceUChar8 hostGuid[MTP_IP_GUID_SIZE];	/* from the host's INIT_COMMAND_REQUEST */
	SceUInt16 padding;
	SceUInt32 connectionNumber;		/* sent back in INIT_COMMAND_ACK */
	int listenSocket;
} MtpIp;

static SceNetFdSet s_readFds;
static MtpIp s_mtpIp;

static int establish_tcp_connection(SceUInt16 port, int *first, int *second);
static int determine_socket_type(int s);
static int get_header(int s, MtpIpHeader *header);
static int mtp_ip_select(int nfds, SceNetFdSet *readfds, SceNetFdSet *writefds,
			 SceNetFdSet *exceptfds, int timeout);

/* wait up to timeout for the command connection to have data, without reading it */
int mtp_ip_wait_for_recv(int s, int timeout)
{
	int optval = timeout;
	char peek[1];
	int ret;

	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &optval,
			       sizeof(optval));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 52,
			"failed to setsockopt for SO_RCVTIMEO option 0x%x", ret);
		return ret;
	}
	ret = sceNetRecv(s, peek, sizeof(peek), SCE_NET_MSG_PEEK);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_DISCONNECTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 60, "failed to receive data 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	optval = MTP_IP_SOCKET_TIMEOUT;
	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &optval,
			       sizeof(optval));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 67,
			"failed to setsockopt for SO_RCVTIMEO option 0x%x", ret);
		return ret;
	}
	return 0;
}

/* accept the host's two connections and tell the event one from the command one */
int mtp_ip_establish_connection(SceUInt16 port, int *eventSocket, int *commandSocket)
{
	int first;
	int second;
	int found;
	int nfds;
	int ret;

	if (eventSocket == NULL || commandSocket == NULL) {
		MTP_ERR(MTP_TAG_MTPIP, 94, "invalid parameter\n");
		return MTP_IP_ERROR_INVALID_ARGUMENT;
	}
	ret = establish_tcp_connection(port, &first, &second);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 101, "failed to establish tcp connections 0x%x\n", ret);
		return MTP_IP_ERROR_CONNECTION;
	}
	found = 0;
	ret = 0;
	for (;;) {
		SCE_NET_FD_ZERO(&s_readFds);
		SCE_NET_FD_SET(first, &s_readFds);
		SCE_NET_FD_SET(second, &s_readFds);
		nfds = first > second ? first : second;
		if (mtp_ip_select(nfds + 1, &s_readFds, NULL, NULL, MTP_IP_SELECT_TIMEOUT) <= 0)
			break;
		if (SCE_NET_FD_ISSET(first, &s_readFds)) {
			switch (determine_socket_type(first)) {
			case MTP_IP_SOCKET_UNKNOWN:
				goto error;
			case MTP_IP_SOCKET_COMMAND:
				*commandSocket = first;
				break;
			case MTP_IP_SOCKET_EVENT:
				*eventSocket = first;
				break;
			default:
				break;
			}
			found |= 1;
		}
		if (SCE_NET_FD_ISSET(second, &s_readFds)) {
			switch (determine_socket_type(second)) {
			case MTP_IP_SOCKET_UNKNOWN:
				goto error;
			case MTP_IP_SOCKET_COMMAND:
				*commandSocket = second;
				break;
			case MTP_IP_SOCKET_EVENT:
				*eventSocket = second;
				break;
			default:
				break;
			}
			found |= 2;
		}
		if ((found & 3) == 3)
			return ret;
	}
error:
	sceNetShutdown(first, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(first);
	sceNetShutdown(second, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(second);
	sceNetShutdown(s_mtpIp.listenSocket, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(s_mtpIp.listenSocket);
	s_mtpIp.listenSocket = -1;
	return MTP_IP_ERROR_CONNECTION;
}

int mtp_ip_close_connection(int eventSocket, int commandSocket)
{
	sceNetShutdown(eventSocket, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(eventSocket);
	sceNetShutdown(commandSocket, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(commandSocket);
	sceNetShutdown(s_mtpIp.listenSocket, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(s_mtpIp.listenSocket);
	s_mtpIp.listenSocket = -1;
	return 0;
}

/* whether the command connection has data waiting, without waiting for it */
int mtp_ip_is_data_received(int s, int *received)
{
	char peek[1];
	int ret;

	ret = sceNetRecv(s, peek, sizeof(peek), SCE_NET_MSG_PEEK | SCE_NET_MSG_DONTWAIT);
	if (ret <= 0) {
		if (ret != SCE_NET_ERROR_EAGAIN) {
			if (ret == SCE_NET_ERROR_EINTR)
				return MTP_IP_ERROR_ABORTED;
			if (ret < 0) {
				MTP_ERR(MTP_TAG_MTPIP, 189, "failed to receive 0x%x\n", ret);
				return MTP_IP_ERROR_SOCKET;
			}
			MTP_ERR(MTP_TAG_MTPIP, 192, "disconected\n");
			return MTP_IP_ERROR_DISCONNECTED;
		}
		*received = 0;
	} else {
		*received = 1;
	}
	return 0;
}

int mtp_ip_get_header(int s, MtpIpHeader *header)
{
	return get_header(s, header);
}

int mtp_ip_read_cancel(int s, SceUInt32 *transactionId)
{
	MtpIpDataPacket packet;
	int ret;

	mtpUtilMemset(&packet, 0, sizeof(packet));
	ret = sceNetRecv(s, &packet.transactionId, sizeof(packet.transactionId),
			 SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 215, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 218, "disconected\n");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	*transactionId = packet.transactionId;
	return 0;
}

/* the rest of an operation request, length bytes after its header */
int mtp_ip_read_operation_request(int s, MtpOperation *operation, SceUInt32 length)
{
	MtpIpOperationRequest packet;
	SceUInt32 numParams;
	SceUInt32 i;
	int ret;

	mtpUtilMemset(&packet, 0, sizeof(packet));
	ret = sceNetRecv(s, &packet.dataPhase, length, SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 240, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 243, "disconected\n");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	numParams = (length - 10) >> 2;
	operation->code = packet.code;
	operation->transactionId = packet.transactionId;
	operation->numParams = numParams;
	for (i = 0; i < numParams; i++)
		operation->param[i] = packet.param[i];
	return 0;
}

int mtp_ip_send_operation_response(int s, const MtpOperation *response)
{
	MtpIpOperationResponse packet;
	SceUInt32 i;
	int ret;

	packet.header.length = 4 * response->numParams + 14;
	packet.header.type = MTP_IP_OPERATION_RESPONSE;
	packet.code = response->code;
	packet.transactionId = response->transactionId;
	for (i = 0; i < response->numParams; i++)
		packet.param[i] = response->param[i];
	ret = sceNetSend(s, &packet, packet.header.length, 0);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 295, "failed to send 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if ((SceUInt32)ret < packet.header.length)
		return MTP_IP_ERROR_ABORTED;
	return 0;
}

int mtp_ip_send_event(int s, const MtpEvent *event)
{
	MtpIpEvent packet;
	int ret;

	packet.header.length = sizeof(packet) - 4 * (MTP_IP_EVENT_PARAMS - event->numParams);
	packet.header.type = MTP_IP_EVENT;
	packet.code = event->code;
	packet.transactionId = event->transactionId;
	mtpUtilMemcpy(packet.param, event->param, 4 * event->numParams);
	ret = sceNetSend(s, &packet, packet.header.length, 0);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 321, "failed to send 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if ((SceUInt32)ret < packet.header.length)
		return MTP_IP_ERROR_ABORTED;
	return 0;
}

int mtp_ip_read_start_data(int s, SceUInt32 *transactionId, SceUInt64 *totalSize)
{
	MtpIpStartData data;
	int ret;

	ret = sceNetRecv(s, &data, sizeof(data), SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 341, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 344, "disconected");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	*transactionId = data.transactionId;
	*totalSize = data.totalSize;
	return 0;
}

int mtp_ip_send_start_data(int s, SceUInt32 transactionId, SceUInt64 totalSize)
{
	MtpIpStartDataPacket packet;
	int ret;

	packet.header.length = sizeof(packet);
	packet.header.type = MTP_IP_START_DATA;
	packet.data.transactionId = transactionId;
	packet.data.totalSize = totalSize;
	ret = sceNetSend(s, &packet, sizeof(packet), 0);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 370, "failed to send 0x%x\n", ret);
		return ret;
	}
	if ((SceUInt32)ret < packet.header.length)
		return MTP_IP_ERROR_ABORTED;
	return 0;
}

int mtp_ip_read_transaction_id(int s, SceUInt32 *transactionId)
{
	int ret;

	ret = sceNetRecv(s, transactionId, sizeof(*transactionId), SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 389, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 392, "disconected");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	return 0;
}

int mtp_ip_read_data(int s, void *buffer, SceUInt32 size)
{
	int ret;

	ret = sceNetRecv(s, buffer, size, SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 409, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 412, "disconected");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	return 0;
}

/* a data packet (END_DATA for the last one), its header first */
int mtp_ip_send_data(int s, SceUInt32 transactionId, const void *buffer, SceUInt32 size,
		     int last)
{
	MtpIpDataPacket packet;
	int ret;

	packet.header.length = size + sizeof(packet);
	packet.header.type = last ? MTP_IP_END_DATA : MTP_IP_DATA;
	packet.transactionId = transactionId;
	ret = sceNetSend(s, &packet, sizeof(packet), 0);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 436, "failed to send 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if ((SceUInt32)ret < sizeof(packet))
		return MTP_IP_ERROR_ABORTED;
	ret = sceNetSend(s, buffer, size, 0);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 447, "failed to send 0x%x\n", ret);
		return ret;
	}
	if ((SceUInt32)ret < size)
		return MTP_IP_ERROR_ABORTED;
	return 0;
}

/* read and drop length bytes */
int mtp_ip_ignore_recv_data(int s, SceUInt32 length)
{
	char buf[MTP_IP_IGNORE_CHUNK];
	SceUInt32 size;
	int ret;

	while (length != 0) {
		size = length;
		if (size > MTP_IP_IGNORE_CHUNK)
			size = MTP_IP_IGNORE_CHUNK;
		ret = sceNetRecv(s, buf, size, SCE_NET_MSG_WAITALL);
		if (ret == SCE_NET_ERROR_EINTR)
			return MTP_IP_ERROR_ABORTED;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_MTPIP, 470, "failed to receive 0x%x\n", ret);
			return MTP_IP_ERROR_SOCKET;
		}
		if (ret == 0) {
			MTP_ERR(MTP_TAG_MTPIP, 473, "disconected\n");
			return MTP_IP_ERROR_DISCONNECTED;
		}
		length -= size;
	}
	return 0;
}

/* the listening socket on port, or -1 */
static int create_server_socket(SceUInt16 port)
{
	SceNetSockaddrIn addr;
	int optval = 1;
	int timeout = MTP_IP_SOCKET_TIMEOUT;
	int s;
	int ret;

	s = sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (s < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 496, "failed to create socket 0x%x\n", s);
		return -1;
	}
	ret = sceNetSetsockopt(s, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &optval,
			       sizeof(optval));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 508, "failed to setsockopt for TCP_NODELAY 0x%x\n", ret);
		goto error;
	}
	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_KEEPALIVE, &optval,
			       sizeof(optval));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 514, "failed to setsockopt for SO_KEEPALIVE 0x%x\n", ret);
		goto error;
	}
	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &timeout,
			       sizeof(timeout));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 520, "failed to setsockopt for SO_RCVTIMEO 0x%x\n", ret);
		goto error;
	}
	mtpUtilMemset(&addr, 0, sizeof(addr));
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_addr.s_addr = SCE_NET_INADDR_ANY;
	addr.sin_port = sceNetHtons(port);
	ret = sceNetBind(s, (SceNetSockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 531, "failed to bind 0x%x\n", ret);
		goto error;
	}
	ret = sceNetListen(s, MTP_IP_LISTEN_BACKLOG);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 537, "failed to listen 0x%x\n", ret);
		goto error;
	}
	return s;
error:
	sceNetSocketClose(s);
	return -1;
}

static int get_header(int s, MtpIpHeader *header)
{
	int ret;

	ret = sceNetRecv(s, header, sizeof(*header), SCE_NET_MSG_WAITALL);
	if (ret == SCE_NET_ERROR_EINTR)
		return MTP_IP_ERROR_ABORTED;
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 558, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0)
		return MTP_IP_ERROR_DISCONNECTED;
	return 0;
}

static int set_socket_option(int s)
{
	int sndTimeout = MTP_IP_SOCKET_TIMEOUT;
	int rcvTimeout = MTP_IP_SOCKET_TIMEOUT;
	int ret;

	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &sndTimeout,
			       sizeof(sndTimeout));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 576, "failed to setsockopt for SO_SNDTIMEO 0x%x\n", ret);
		return SCE_MTP_ERROR_NET;
	}
	ret = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &rcvTimeout,
			       sizeof(rcvTimeout));
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 582, "failed to setsockopt for SO_RCVTIMEO 0x%x\n", ret);
		return SCE_MTP_ERROR_NET;
	}
	return 0;
}

/* the host's two connections, accepted in the order they come */
static int establish_tcp_connection(SceUInt16 port, int *first, int *second)
{
	int listenSocket;
	int s;
	int ret;

	*second = -1;
	*first = -1;
	s_mtpIp.listenSocket = -1;
	listenSocket = create_server_socket(port);
	if (listenSocket < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 599, "failed to create socket 0x%x\n", listenSocket);
		return listenSocket;
	}
	s = sceNetAccept(listenSocket, NULL, NULL);
	if (s < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 608, "failed to accept 1st tcp connection 0x%x\n", s);
		ret = SCE_MTP_ERROR_NET;
		goto error;
	}
	ret = set_socket_option(s);
	if (ret < 0)
		goto error;
	*first = s;
	s = sceNetAccept(listenSocket, NULL, NULL);
	if (s < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 623, "failed to accept 2nd tcp connection 0x%x\n", s);
		ret = SCE_MTP_ERROR_NET;
		goto error;
	}
	ret = set_socket_option(s);
	if (ret < 0)
		goto error;
	*second = s;
	s_mtpIp.listenSocket = listenSocket;
	return 0;
error:
	if (*first != -1) {
		sceNetShutdown(*first, SCE_NET_SHUT_RDWR);
		sceNetSocketClose(*first);
	}
	if (*second != -1) {
		sceNetShutdown(*second, SCE_NET_SHUT_RDWR);
		sceNetSocketClose(*second);
	}
	sceNetShutdown(listenSocket, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(listenSocket);
	*second = -1;
	*first = -1;
	return ret;
}

/* INIT_COMMAND_REQUEST: the host's GUID */
static int read_init_cmd_request(int s)
{
	SceUChar8 guid[MTP_IP_GUID_SIZE];
	int ret;

	ret = sceNetRecv(s, guid, sizeof(guid), SCE_NET_MSG_WAITALL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 668, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 671, "disconected\n");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	mtpUtilMemcpy(s_mtpIp.hostGuid, guid, sizeof(guid));
	return 0;
}

static int send_init_cmd_ack(int s)
{
	MtpIpInitCommandAck packet;
	int ret;

	packet.header.length = sizeof(packet);
	packet.header.type = MTP_IP_INIT_COMMAND_ACK;
	packet.connectionNumber = s_mtpIp.connectionNumber;
	ret = sceNetSend(s, &packet, sizeof(packet), 0);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 693, "failed to send 0x%x\n", ret);
		return ret;
	}
	return 0;
}

/* INIT_EVENT_REQUEST: the connection number */
static int read_init_event_request(int s)
{
	SceUInt32 connectionNumber;
	int ret;

	ret = sceNetRecv(s, &connectionNumber, sizeof(connectionNumber), SCE_NET_MSG_WAITALL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 710, "failed to receive 0x%x\n", ret);
		return MTP_IP_ERROR_SOCKET;
	}
	if (ret == 0) {
		MTP_ERR(MTP_TAG_MTPIP, 713, "disconected");
		return MTP_IP_ERROR_DISCONNECTED;
	}
	return 0;
}

static int send_init_event_ack(int s)
{
	MtpIpHeader packet;
	int ret;

	packet.length = sizeof(packet);
	packet.type = MTP_IP_INIT_EVENT_ACK;
	ret = sceNetSend(s, &packet, sizeof(packet), 0);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_MTPIP, 735, "failed to send 0x%x\n", ret);
		return ret;
	}
	return 0;
}

/* a connection's first packet says what it is for, and is acknowledged */
static int determine_socket_type(int s)
{
	MtpIpHeader header;
	int ret;

	if (get_header(s, &header) < 0)
		return MTP_IP_SOCKET_UNKNOWN;
	switch (header.type) {
	case MTP_IP_INIT_COMMAND_REQUEST:
		ret = read_init_cmd_request(s);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_MTPIP, 786, "failed to read INIT_COMMAND_REQUEST 0x%x\n",
				ret);
			return MTP_IP_SOCKET_UNKNOWN;
		}
		ret = send_init_cmd_ack(s);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_MTPIP, 792, "failed to send INIT_COMMAND_ACK 0x%x\n", ret);
			return MTP_IP_SOCKET_UNKNOWN;
		}
		return MTP_IP_SOCKET_COMMAND;
	case MTP_IP_INIT_EVENT_REQUEST:
		ret = read_init_event_request(s);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_MTPIP, 801, "failed to read INIT_EVENT_REQUEST 0x%x\n",
				ret);
			return MTP_IP_SOCKET_UNKNOWN;
		}
		ret = send_init_event_ack(s);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_MTPIP, 807, "failed to send INIT_EVENT_ACK 0x%x\n", ret);
			return MTP_IP_SOCKET_UNKNOWN;
		}
		return MTP_IP_SOCKET_EVENT;
	default:
		return MTP_IP_SOCKET_UNKNOWN;
	}
}

int mtp_ip_abort(int s)
{
	return sceNetSocketAbort(s, 0);
}

/* select() over an epoll: the sets name the sockets to wait for and come back as found */
static __attribute__((noinline)) int pf_socket_select(int nfds, SceNetFdSet *readfds,
						      SceNetFdSet *writefds,
						      SceNetFdSet *exceptfds, int timeout)
{
	SceNetEpollEvent event;
	SceNetEpollEvent events[MTP_IP_SELECT_EVENTS];
	SceUInt32 flags;
	int ret = 0;
	int eid;
	int count;
	int found;
	int i;

	eid = sceNetEpollCreate("pf_socket_select", 0);
	if (eid < 0)
		return eid;
	for (i = 0; i < nfds; i++) {
		flags = 0;
		if (readfds != NULL)
			flags = SCE_NET_FD_ISSET(i, readfds) != 0;
		if (writefds != NULL && SCE_NET_FD_ISSET(i, writefds))
			flags |= SCE_NET_EPOLLOUT;
		if (flags != 0 || (exceptfds != NULL && SCE_NET_FD_ISSET(i, exceptfds))) {
			mtpUtilMemset(&event, 0, sizeof(event));
			event.events = flags;
			event.data.fd = i;
			ret = sceNetEpollControl(eid, SCE_NET_EPOLL_CTL_ADD, i, &event);
			if (ret < 0)
				goto error;
		}
	}
	count = sceNetEpollWait(eid, events, MTP_IP_SELECT_EVENTS, timeout);
	if (count < 0)
		goto error;
	if (readfds != NULL)
		SCE_NET_FD_ZERO(readfds);
	if (writefds != NULL)
		SCE_NET_FD_ZERO(writefds);
	if (exceptfds != NULL)
		SCE_NET_FD_ZERO(exceptfds);
	/* without an exception set, an error or a hang-up interrupts the call */
	if (exceptfds == NULL) {
		for (i = 0; i < count; i++) {
			if (events[i].events & (SCE_NET_EPOLLERR | SCE_NET_EPOLLHUP)) {
				if (readfds != NULL)
					SCE_NET_FD_SET(events[i].data.u32, readfds);
				if (writefds != NULL)
					SCE_NET_FD_SET(events[i].data.u32, writefds);
				ret = SCE_NET_ERROR_EINTR;
				sce_net_errno = SCE_NET_EINTR;
				goto error;
			}
		}
	}
	found = 0;
	for (i = 0; i < count; i++) {
		flags = events[i].events;
		if (flags & SCE_NET_EPOLLIN) {
			if (readfds != NULL)
				SCE_NET_FD_SET(events[i].data.u32, readfds);
			found++;
		}
		if (flags & SCE_NET_EPOLLOUT) {
			if (writefds != NULL)
				SCE_NET_FD_SET(events[i].data.u32, writefds);
			found++;
		}
		if (flags & (SCE_NET_EPOLLERR | SCE_NET_EPOLLHUP)) {
			if (exceptfds != NULL)
				SCE_NET_FD_SET(events[i].data.u32, exceptfds);
			found++;
		}
	}
	sceNetEpollDestroy(eid);
	return found;
error:
	sceNetEpollDestroy(eid);
	return ret;
}

static __attribute__((noinline)) int mtp_ip_select(int nfds, SceNetFdSet *readfds,
						   SceNetFdSet *writefds,
						   SceNetFdSet *exceptfds, int timeout)
{
	return pf_socket_select(nfds, readfds, writefds, exceptfds, timeout);
}
