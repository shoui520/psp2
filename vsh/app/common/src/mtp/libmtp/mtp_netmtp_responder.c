/*
 * The network interface's responder ("NET MTP RESPONDER:"): the MTP operations of the
 * network interface (SceMtpIf's for USB) over an MTP/IP connection, a command channel for
 * the operations, their data and responses, and an event channel.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <net/socket.h>
#include "mtp_internal.h"

#define MTP_TAG_NET_MTP_RESPONDER	"NET MTP RESPONDER:"

/* the responder's state: the low half is what netMtpResponderGetState reports */
#define NET_MTP_RESPONDER_CONNECTED	0x1
#define NET_MTP_RESPONDER_STARTED	0x10000
#define NET_MTP_RESPONDER_RESET_MASK	0xFFFDFFFE

/* the phase of the transaction in progress */
#define NET_MTP_PHASE_IDLE		0
#define NET_MTP_PHASE_COMMAND		1
#define NET_MTP_PHASE_DATA_OUT		2
#define NET_MTP_PHASE_DATA_IN		4
#define NET_MTP_PHASE_RESPONSE		8

/* the most data one MTP/IP data packet carries */
#define NET_MTP_DATA_MAX		0x10000
/* the most data a PS3 host takes in one packet */
#define NET_MTP_DATA_MAX_PS3		0x4000

#define NET_MTP_SOCKET_TIMEOUT		15000000
#define NET_MTP_SOCKET_TIMEOUT_LONG	60000000

/* the platform class and interface a PS3 host connects with */
#define MTP_PLATFORM_CLASS_PS3		2
#define MTP_IF_TYPE_NET			2

typedef struct NetMtpResponder {
	SceUID mutex;
	SceUInt32 state;
	int phase;
	SceUInt32 canceledTransactionId;
	SceUInt32 transactionId;
	SceUInt32 reserved;
	int eventSocket;
	int commandSocket;
	SceUInt8 reserved2[0x10116];
	SceUInt8 initiatorGuid[16];	/* set to all ones, not read */
	SceUInt8 reserved3[26];
	/* the data phase in progress */
	SceUInt64 sent;
	SceUInt64 recvTotal;
	SceUInt64 recvPacketSize;
	SceUInt64 received;
	SceUInt64 recvPacketDone;
} NetMtpResponder;

static NetMtpResponder s_responder;

static inline void closeConnection(void)
{
	mtp_ip_close_connection(s_responder.eventSocket, s_responder.commandSocket);
	s_responder.eventSocket = -1;
	s_responder.commandSocket = -1;
	s_responder.canceledTransactionId = 0xFFFFFFFF;
	s_responder.transactionId = 0xFFFFFFFF;
	s_responder.reserved = 0xFFFFFFFF;
	s_responder.state &= NET_MTP_RESPONDER_RESET_MASK;
	s_responder.phase = NET_MTP_PHASE_IDLE;
}

static inline void clearDataPhase(void)
{
	s_responder.sent = 0;
	s_responder.recvTotal = 0;
	s_responder.recvPacketSize = 0;
	s_responder.received = 0;
	s_responder.recvPacketDone = 0;
}

int netMtpResponderStart(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) != 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 150, "already started\n");
		return MTP_ERROR_BUSY;
	}
	mtpUtilMemset(&s_responder, 0, sizeof(s_responder));
	mtpUtilMemset(s_responder.initiatorGuid, 0xFF, sizeof(s_responder.initiatorGuid));
	s_responder.state &= NET_MTP_RESPONDER_RESET_MASK;
	createMutex(&s_responder.mutex);
	s_responder.eventSocket = -1;
	s_responder.commandSocket = -1;
	s_responder.canceledTransactionId = 0xFFFFFFFF;
	s_responder.transactionId = 0xFFFFFFFF;
	s_responder.reserved = 0xFFFFFFFF;
	s_responder.state |= NET_MTP_RESPONDER_STARTED;
	s_responder.phase = NET_MTP_PHASE_IDLE;
	return 0;
}

int netMtpResponderStop(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 169, "not started\n");
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.eventSocket & s_responder.commandSocket) != -1) {
		mtp_ip_abort(s_responder.commandSocket);
		mtp_ip_abort(s_responder.eventSocket);
		mtp_ip_close_connection(s_responder.eventSocket, s_responder.commandSocket);
		s_responder.eventSocket = -1;
		s_responder.commandSocket = -1;
	}
	deleteMutex(s_responder.mutex);
	s_responder.state = 0;
	return 0;
}

int netMtpResponderWaitConnect(SceUInt16 port)
{
	int ret;

	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 213, "not started\n");
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) != 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 218, "already connected\n");
		return MTP_ERROR_BUSY;
	}
	ret = mtp_ip_establish_connection(port, &s_responder.eventSocket,
					  &s_responder.commandSocket);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 225, "failed to establish connections 0x%x\n",
			ret);
		return MTP_ERROR_PROTOCOL;
	}
	clearDataPhase();
	s_responder.state = (s_responder.state & NET_MTP_RESPONDER_RESET_MASK) |
			    NET_MTP_RESPONDER_CONNECTED;
	return 0;
}

int netMtpResponderReset(void)
{
	mutexLock(s_responder.mutex);
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 243, "not started\n");
		mutexUnlock(s_responder.mutex);
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 249, "not connected\n");
		mutexUnlock(s_responder.mutex);
		return MTP_ERROR_DISCONNECTED;
	}
	mtp_ip_abort(s_responder.commandSocket);
	mtp_ip_abort(s_responder.eventSocket);
	clearDataPhase();
	closeConnection();
	mutexUnlock(s_responder.mutex);
	return 0;
}

int netMtpResponderRecvCommand(MtpOperation *operation)
{
	int ret;
	SceUInt32 length = 0;
	MtpIpHeader header;

	if (operation == NULL) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 276, "invalid parameter\n");
		return MTP_ERROR_INVALID;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 282, "not started\n");
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 287, "not connected\n");
		return MTP_ERROR_DISCONNECTED;
	}
	ret = mtp_ip_wait_for_recv(s_responder.commandSocket, 0);
	if (ret == MTP_ERROR_DISCONNECTED)
		return MTP_ERROR_DISCONNECTED;
	if (ret < 0) {
		closeConnection();
		return ret;
	}
	mutexLock(s_responder.mutex);
	if (s_responder.phase != NET_MTP_PHASE_IDLE) {
		ret = MTP_ERROR_CANCELLED;
	} else {
		/* everything before the next operation request is dropped */
		for (;;) {
			ret = mtp_ip_get_header(s_responder.commandSocket, &header);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 319, "failed to get header 0x%x\n",
					ret);
				break;
			}
			length = header.length - sizeof(header);
			if (header.type == MTP_IP_OPERATION_REQUEST) {
				ret = 0;
				break;
			}
			ret = mtp_ip_ignore_recv_data(s_responder.commandSocket, length);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 351, "failed to read data 0x%x\n",
					ret);
				break;
			}
		}
		if (ret == 0) {
			ret = mtp_ip_read_operation_request(s_responder.commandSocket, operation,
							    length);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 361,
					"failed to read operation request 0x%x\n", ret);
			} else {
				ret = 0;
				s_responder.transactionId = operation->transactionId;
				s_responder.phase = NET_MTP_PHASE_COMMAND;
			}
		}
		if (ret == 0) {
			mutexUnlock(s_responder.mutex);
			return ret;
		}
	}
	if (ret != MTP_ERROR_CANCELLED)
		closeConnection();
	mutexUnlock(s_responder.mutex);
	return ret;
}

static inline int send_canceled_operation_response(SceUInt32 transactionId)
{
	MtpOperation response;
	int ret;

	mtpUtilMemset(&response, 0, sizeof(response));
	response.transactionId = transactionId;
	response.code = MTP_RESPONSE_TRANSACTION_CANCELLED;
	ret = mtp_ip_send_operation_response(s_responder.commandSocket, &response);
	if (ret < 0)
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1090,
			"failed to send operation response 0x%x\n", ret);
	return ret;
}

static __attribute__((noinline)) int read_cancel_packet_and_handle_it(int s,
								    SceUInt32 transactionId,
								    int *canceled)
{
	int ret;
	int i;
	SceUInt32 canceledId;
	SceUInt8 null;

	*canceled = 0;
	canceledId = 0xFFFFFFFF;
	ret = mtp_ip_read_cancel(s_responder.commandSocket, &canceledId);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1106,
			"failed to read operation request 0x%x\n", ret);
		return ret;
	}
	if (transactionId != canceledId) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1111, "unexpected transaction id 0x%x\n",
			canceledId);
		return MTP_ERROR_PROTOCOL;
	}
	i = 99;
	do
		sceKernelDelayThread(10000);
	while (i-- != 0);
	null = 0;
	if (mtp_ip_send_data(s, transactionId, &null, 1, 0) < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1124, "Error :SENDING NULL PACKET\n");
		return MTP_ERROR_PROTOCOL;
	}
	if (send_canceled_operation_response(canceledId) < 0)
		return MTP_ERROR_PROTOCOL;
	*canceled = 1;
	return 0;
}

static __attribute__((noinline)) int check_cancel_packet_and_handle_it(int s,
								     SceUInt32 transactionId,
								     int *canceled)
{
	int ret;
	int received;
	MtpIpHeader header;

	*canceled = 0;
	received = 0;
	ret = mtp_ip_is_data_received(s_responder.commandSocket, &received);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1149, "failed to check received data 0x%x\n",
			ret);
		return ret;
	}
	if (received == 0)
		return 0;
	ret = mtp_ip_get_header(s_responder.commandSocket, &header);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1159, "failed to get header 0x%x\n", ret);
		return ret;
	}
	if (header.type == MTP_IP_CANCEL) {
		ret = read_cancel_packet_and_handle_it(s_responder.commandSocket, transactionId,
						       canceled);
		if (ret < 0)
			return ret;
	} else {
		ret = mtp_ip_ignore_recv_data(s_responder.commandSocket,
					      header.length - sizeof(header));
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1166, "failed to ignore data 0x%x\n",
				ret);
			return ret;
		}
	}
	return 0;
}

int netMtpResponderSendResponse(const MtpOperation *response)
{
	int ret = 0;
	int res;
	int canceled = 0;

	mutexLock(s_responder.mutex);
	if (response == NULL) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 396, "invalid parameter\n");
		closeConnection();
		mutexUnlock(s_responder.mutex);
		return MTP_ERROR_INVALID;
	}
	/* the transaction was canceled: its response has been sent */
	if (s_responder.canceledTransactionId == response->transactionId) {
		s_responder.phase = NET_MTP_PHASE_IDLE;
		mutexUnlock(s_responder.mutex);
		return 0;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 415, "not started\n");
		ret = MTP_ERROR_UNAVAILABLE;
		goto error;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 421, "not connected\n");
		ret = MTP_ERROR_DISCONNECTED;
		goto error;
	}
	res = check_cancel_packet_and_handle_it(s_responder.commandSocket,
						s_responder.transactionId, &canceled);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 436, "failed to check cancel packet 0x%x\n",
			res);
		ret = res;
	} else if (canceled != 0) {
		s_responder.phase = NET_MTP_PHASE_IDLE;
		ret = MTP_ERROR_CANCELLED;
		goto error;
	} else {
		res = mtp_ip_send_operation_response(s_responder.commandSocket, response);
		if (res >= 0) {
			s_responder.phase = NET_MTP_PHASE_IDLE;
			goto end;
		}
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 450, "failed to send operation response 0x%x\n",
			res);
		ret = res;
	}
	if (ret != 0) {
error:
		if (ret != MTP_ERROR_CANCELLED)
			closeConnection();
	}
end:
	mutexUnlock(s_responder.mutex);
	return ret;
}

/* a PS3 host on the network takes smaller packets */
static inline int isPs3Host(void)
{
	return mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS3 &&
	       mtpDevGetIfType() == MTP_IF_TYPE_NET;
}

int netMtpResponderSendData(MtpIfTransfer *transfer)
{
	int ret;
	int res;
	int canceled;
	int last;
	int size;
	int remaining;
	SceUInt8 *buffer;

	mutexLock(s_responder.mutex);
	ret = 0;
	canceled = 0;
	if (transfer == NULL) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 479, "invalid parameter\n");
		ret = MTP_ERROR_INVALID;
		goto check;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 485, "not started\n");
		ret = MTP_ERROR_UNAVAILABLE;
		goto check;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 491, "not connected\n");
		ret = MTP_ERROR_DISCONNECTED;
		goto check;
	}
	if (s_responder.canceledTransactionId == transfer->transactionId) {
		if (isPs3Host()) {
			/* a PS3 waits for the data it asked for: end it with one packet */
			size = (int)transfer->size;
			if (size > NET_MTP_DATA_MAX_PS3)
				size = NET_MTP_DATA_MAX_PS3;
			res = mtp_ip_send_data(s_responder.commandSocket, transfer->transactionId,
					       transfer->buffer, size, 0);
			if (res < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 513, "failed to send data 0x%x\n",
					res);
				ret = res;
			}
		} else {
			res = send_canceled_operation_response(s_responder.canceledTransactionId);
			if (res < 0)
				ret = MTP_ERROR_PROTOCOL;
			else
				ret = MTP_ERROR_CANCELLED;
		}
		s_responder.phase = NET_MTP_PHASE_IDLE;
		s_responder.canceledTransactionId = 0xFFFFFFFF;
		s_responder.sent = 0;
		goto end;
	}
	if (s_responder.sent == 0) {
		res = mtp_ip_send_start_data(s_responder.commandSocket, transfer->transactionId,
					     transfer->totalSize);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 541, "failed to send start data 0x%x\n",
				res);
			ret = res;
			goto check;
		}
		s_responder.phase = NET_MTP_PHASE_DATA_OUT;
	}
	remaining = (int)transfer->size;
	buffer = transfer->buffer;
	while (remaining > 0) {
		/* let a cancel in */
		mutexUnlock(s_responder.mutex);
		mutexLock(s_responder.mutex);
		size = remaining;
		if (isPs3Host()) {
			if (size > NET_MTP_DATA_MAX_PS3)
				size = NET_MTP_DATA_MAX_PS3;
		} else {
			if (size > NET_MTP_DATA_MAX)
				size = NET_MTP_DATA_MAX;
		}
		last = 0;
		if (s_responder.sent + (SceUInt32)size == transfer->totalSize)
			last = 1;
		res = mtp_ip_send_data(s_responder.commandSocket, transfer->transactionId, buffer,
				       size, last);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 575, "failed to send data 0x%x\n", res);
			ret = res;
			break;
		}
		transfer->transferred += (SceUInt32)size;
		buffer += size;
		s_responder.sent += (SceUInt32)size;
		remaining -= size;
		if (s_responder.canceledTransactionId == transfer->transactionId) {
			ret = MTP_ERROR_CANCELLED;
			break;
		}
		if (last != 0) {
			s_responder.sent = 0;
			s_responder.phase = NET_MTP_PHASE_RESPONSE;
		}
	}
check:
	if (s_responder.phase == NET_MTP_PHASE_DATA_OUT) {
		res = check_cancel_packet_and_handle_it(s_responder.commandSocket,
							s_responder.transactionId, &canceled);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 610,
				"failed to check cancel packet 0x%x\n", res);
			ret = res;
		} else if (canceled != 0) {
			ret = MTP_ERROR_CANCELLED;
		}
		if (ret == MTP_ERROR_CANCELLED) {
			s_responder.phase = NET_MTP_PHASE_IDLE;
			clearDataPhase();
		}
	}
end:
	if (ret != 0 && ret != MTP_ERROR_CANCELLED) {
		closeConnection();
		transfer->transferred = 0;
		s_responder.sent = 0;
	}
	mutexUnlock(s_responder.mutex);
	return ret;
}

int netMtpResponderRecvData(MtpIfTransfer *transfer)
{
	int ret = 0;
	int res;
	int canceled;
	SceUInt32 transactionId = 0;
	SceUInt32 length = 0;
	SceUInt32 size;
	SceUInt32 remaining;
	SceUInt64 total;
	SceUInt8 *buffer;
	MtpIpHeader header;

	mutexLock(s_responder.mutex);
	if (transfer == NULL) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 651, "invalid parameter\n");
		ret = MTP_ERROR_INVALID;
		goto fail;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 657, "not started\n");
		ret = MTP_ERROR_UNAVAILABLE;
		goto fail;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		ret = MTP_ERROR_DISCONNECTED;
		goto fail;
	}
	remaining = (SceUInt32)transfer->size;
	if (s_responder.received == 0) {
		if (transfer->size == 0) {
			sceClibPrintf("[MTP %s%s] Data size is zero.\n", MTP_TAG_NET_MTP_RESPONDER,
				      __FUNCTION__);
			goto done;
		}
		res = mtp_ip_get_header(s_responder.commandSocket, &header);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 681, "failed to get header 0x%x\n", res);
			ret = res;
			goto result;
		}
		if (header.type == MTP_IP_CANCEL) {
			canceled = 0;
			res = read_cancel_packet_and_handle_it(s_responder.commandSocket,
							       s_responder.transactionId, &canceled);
			ret = MTP_ERROR_CANCELLED;
			if (res < 0)
				ret = res;
			goto result;
		}
		if (header.type != MTP_IP_START_DATA) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 717,
				"received invalid packet type : 0x%x, length : %d\n", header.type,
				header.length);
			length = header.length - sizeof(header);
			ret = MTP_ERROR_PROTOCOL;
			goto ignore;
		}
		total = 0;
		res = mtp_ip_read_start_data(s_responder.commandSocket, &transactionId, &total);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 690,
				"failed to read operation request 0x%x\n", res);
			ret = res;
			goto result;
		}
		s_responder.recvTotal = total;
		s_responder.recvPacketSize = 0;
		s_responder.recvPacketDone = 0;
		remaining = (SceUInt32)transfer->size;
	}
	buffer = transfer->buffer;
	while (remaining != 0) {
		/* let a cancel in */
		mutexUnlock(s_responder.mutex);
		mutexLock(s_responder.mutex);
		if (s_responder.recvPacketDone == 0) {
			if (s_responder.canceledTransactionId == transfer->transactionId) {
				ret = MTP_ERROR_CANCELLED;
				break;
			}
			res = mtp_ip_get_header(s_responder.commandSocket, &header);
			if (res < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 739, "failed to get header 0x%x\n",
					res);
				ret = res;
				break;
			}
			if (header.type != MTP_IP_DATA && header.type != MTP_IP_END_DATA) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 746,
					"received invalid packet type : 0x%x, length : %d\n",
					header.type, header.length);
				length = header.length - sizeof(header);
				ret = MTP_ERROR_PROTOCOL;
				break;
			}
			res = mtp_ip_read_transaction_id(s_responder.commandSocket, &transactionId);
			if (res < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 753,
					"failed to get transaction id 0x%x\n", res);
				ret = res;
				break;
			}
			s_responder.transactionId = transactionId;
			s_responder.recvPacketSize = header.length - (sizeof(header) + 4);
			s_responder.phase = NET_MTP_PHASE_DATA_IN;
		}
		if (s_responder.recvPacketSize != 0) {
			size = (SceUInt32)transfer->size;
			if (transfer->size > NET_MTP_DATA_MAX)
				size = NET_MTP_DATA_MAX;
			if (size > s_responder.recvPacketSize - s_responder.recvPacketDone)
				size = s_responder.recvPacketSize - s_responder.recvPacketDone;
			if (size > transfer->size - transfer->transferred)
				size = transfer->size - transfer->transferred;
			res = mtp_ip_read_data(s_responder.commandSocket, buffer, size);
			if (res < 0) {
				MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 779, "failed to read data 0x%x\n",
					res);
				ret = res;
				break;
			}
			s_responder.received += size;
			s_responder.recvPacketDone += size;
			buffer += size;
			remaining -= size;
			transfer->transferred += size;
			if (s_responder.received == s_responder.recvTotal)
				break;
		}
		if (s_responder.recvPacketDone == s_responder.recvPacketSize) {
			s_responder.recvPacketSize = 0;
			s_responder.recvPacketDone = 0;
		}
		if (transfer->transferred == transfer->size)
			break;
	}
	if (s_responder.received == s_responder.recvTotal) {
		s_responder.recvTotal = 0;
		s_responder.received = 0;
		s_responder.phase = NET_MTP_PHASE_RESPONSE;
	}
ignore:
	if (length != 0) {
		res = mtp_ip_ignore_recv_data(s_responder.commandSocket, length);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 829, "failed to read data 0x%x\n", res);
			ret = res;
		}
	}
result:
	if (ret != 0)
		goto canceled;
done:
	transfer->totalSize = s_responder.recvTotal;
canceled:
	if (ret == MTP_ERROR_CANCELLED) {
		s_responder.recvPacketSize = 0;
		s_responder.received = 0;
		s_responder.recvPacketDone = 0;
		s_responder.phase = NET_MTP_PHASE_IDLE;
		s_responder.recvTotal = 0;
		transfer->transferred = 0;
		transfer->totalSize = 0;
		goto end;
	}
	if (ret >= 0)
		goto end;
fail:
	if (transfer != NULL) {
		transfer->transferred = 0;
		transfer->totalSize = 0;
	}
	s_responder.recvPacketSize = 0;
	s_responder.recvPacketDone = 0;
	s_responder.recvTotal = 0;
	s_responder.received = 0;
	closeConnection();
end:
	mutexUnlock(s_responder.mutex);
	return ret;
}

int netMtpResponderSendEvent(const MtpEvent *event)
{
	int ret = 0;
	int res;
	SceUInt32 transactionId;

	mutexLock(s_responder.mutex);
	if (event == NULL) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 866, "invalid parameter\n");
		ret = MTP_ERROR_INVALID;
		goto error;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 872, "not started\n");
		ret = MTP_ERROR_UNAVAILABLE;
		goto error;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		ret = MTP_ERROR_DISCONNECTED;
		goto error;
	}
	/* the host's cancel is answered in the data phase, not as an event */
	if (event->code == MTP_EVENT_CANCEL_TRANSACTION)
		goto end;
	res = mtp_ip_send_event(s_responder.eventSocket, event);
	if (res == MTP_ERROR_CANCELLED && event->code == MTP_OPERATION_TERMINATE) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 891, "0xc126 retry\n");
		res = mtp_ip_send_event(s_responder.eventSocket, event);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 894, "failed to send event 0x%x\n", res);
			ret = res;
			goto error;
		}
	} else if (res < 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 899, "failed to send event 0x%x\n", res);
		ret = res;
		goto error;
	}
	if (event->code != MTP_OPERATION_CANCEL && event->code != MTP_OPERATION_PAUSE_EXPORT_OBJECT)
		goto end;
	transactionId = s_responder.transactionId;
	if (event->transactionId != transactionId || s_responder.phase == NET_MTP_PHASE_IDLE)
		goto end;
	switch (s_responder.phase) {
	case NET_MTP_PHASE_COMMAND:
	case NET_MTP_PHASE_RESPONSE:
		res = send_canceled_operation_response(transactionId);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 957,
				"failed to send cancel operation response 0x%x\n", res);
			ret = MTP_ERROR_PROTOCOL;
			goto error;
		}
		s_responder.canceledTransactionId = s_responder.transactionId;
		break;
	case NET_MTP_PHASE_DATA_OUT:
		s_responder.canceledTransactionId = s_responder.transactionId;
		break;
	case NET_MTP_PHASE_DATA_IN:
		res = send_canceled_operation_response(transactionId);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 923,
				"failed to send cancel operation response 0x%x\n", res);
			ret = MTP_ERROR_PROTOCOL;
			goto error;
		}
		s_responder.canceledTransactionId = s_responder.transactionId;
		break;
	default:
		break;
	}
	goto end;
error:
	if (ret != MTP_ERROR_CANCELLED)
		closeConnection();
end:
	mutexUnlock(s_responder.mutex);
	return ret;
}

int netMtpResponderCancelTransfer(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 988, "not started\n");
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0)
		return MTP_ERROR_DISCONNECTED;
	mtp_ip_abort(s_responder.commandSocket);
	mtp_ip_abort(s_responder.eventSocket);
	clearDataPhase();
	return 0;
}

SceUInt16 netMtpResponderGetState(void)
{
	SceUInt16 state;

	mutexLock(s_responder.mutex);
	state = (SceUInt16)s_responder.state;
	mutexUnlock(s_responder.mutex);
	return state;
}

int netMtpResponderSetPhase(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1030, "not started\n");
		return MTP_ERROR_UNAVAILABLE;
	}
	if ((s_responder.state & NET_MTP_RESPONDER_CONNECTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1035, "not connected\n");
		return MTP_ERROR_DISCONNECTED;
	}
	return 0;
}

void netMtpResponderDisconnect(void)
{
	closeConnection();
}

static inline void setSocketTimeout(int option, int timeout, int line)
{
	int res;

	if (s_responder.commandSocket == -1)
		return;
	res = sceNetSetsockopt(s_responder.commandSocket, SCE_NET_SOL_SOCKET, option, &timeout,
			       sizeof(timeout));
	if (res < 0)
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, line, "failed pf_socket_setsockopt 0x%x\n",
			res);
}

void NetMtpExpandSocketSendTimeout(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1187, "not started\n");
		return;
	}
	setSocketTimeout(SCE_NET_SO_SNDTIMEO, NET_MTP_SOCKET_TIMEOUT_LONG, 1196);
}

void NetMtpResetSocketSendTimeout(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1206, "not started\n");
		return;
	}
	setSocketTimeout(SCE_NET_SO_SNDTIMEO, NET_MTP_SOCKET_TIMEOUT, 1215);
}

void NetMtpExpandSocketReceiveTimeout(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1225, "not started\n");
		return;
	}
	setSocketTimeout(SCE_NET_SO_RCVTIMEO, NET_MTP_SOCKET_TIMEOUT_LONG, 1234);
}

void NetMtpResetSocketReceiveTimeout(void)
{
	if ((s_responder.state & NET_MTP_RESPONDER_STARTED) == 0) {
		MTP_ERR(MTP_TAG_NET_MTP_RESPONDER, 1244, "not started\n");
		return;
	}
	setSocketTimeout(SCE_NET_SO_RCVTIMEO, NET_MTP_SOCKET_TIMEOUT, 1253);
}
