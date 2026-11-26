/*
 * rudp.h: SceLibRudp public SDK header (target/include/rudp.h).
 */
#ifndef _SCE_RUDP_H
#define _SCE_RUDP_H

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <net/nettypes.h>
#include <net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t SceRudpUsec;

/* Library common event IDs */
#define SCE_RUDP_EVENT_SEND			(1)
#define SCE_RUDP_EVENT_SOCKET_RELEASED		(2)
#define SCE_RUDP_EVENT_DIAG_SENT		(100)
#define SCE_RUDP_EVENT_DIAG_RCVD		(101)

typedef int (*SceRudpEventHandler)(
	int eventId,
	int soc,
	const uint8_t *data,
	size_t dataLen,
	const struct SceNetSockaddr *addr,
	SceNetSocklen_t addrLen,
	void *arg);

/* Context event IDs */
#define SCE_RUDP_CONTEXT_EVENT_CLOSED		(1)
#define SCE_RUDP_CONTEXT_EVENT_ESTABLISHED	(2)
#define SCE_RUDP_CONTEXT_EVENT_ERROR		(3)
#define SCE_RUDP_CONTEXT_EVENT_WRITABLE		(4)
#define SCE_RUDP_CONTEXT_EVENT_READABLE		(5)
#define SCE_RUDP_CONTEXT_EVENT_FLUSHED		(6)

typedef void (*SceRudpContextEventHandler)(
	int ctxId,
	int eventId,
	int errorCode,
	void *arg);

/* Multiplexing modes */
#define SCE_RUDP_MUXMODE_P2P			(1)

/* Message flags */
#define SCE_RUDP_MSG_DONTWAIT			(0x01)
#define SCE_RUDP_MSG_LATENCY_CRITICAL		(0x08)
#define SCE_RUDP_MSG_ALIGN_32			(0x10)
#define SCE_RUDP_MSG_ALIGN_64			(0x20)
#define SCE_RUDP_MSG_WITH_TX_TIMESTAMP		(0x40)

typedef struct SceRudpReadInfo {
	uint8_t size;
	uint8_t retransmissionCount;
	uint16_t retransmissionDelay;
	uint8_t retransmissionDelay2;
	uint8_t flags;
	uint16_t sequenceNumber;
	uint32_t timestamp;
} SceRudpReadInfo;

/* Option IDs */
#define SCE_RUDP_OPTION_MAX_PAYLOAD		(1)
#define SCE_RUDP_OPTION_SNDBUF			(2)
#define SCE_RUDP_OPTION_RCVBUF			(3)
#define SCE_RUDP_OPTION_NODELAY			(4)
#define SCE_RUDP_OPTION_DELIVERY_CRITICAL	(5)
#define SCE_RUDP_OPTION_ORDER_CRITICAL		(6)
#define SCE_RUDP_OPTION_NONBLOCK		(7)
#define SCE_RUDP_OPTION_STREAM			(8)
#define SCE_RUDP_OPTION_CONNECTION_TIMEOUT	(9)
#define SCE_RUDP_OPTION_CLOSE_WAIT_TIMEOUT	(10)
#define SCE_RUDP_OPTION_AGGREGATION_TIMEOUT	(11)
#define SCE_RUDP_OPTION_LAST_ERROR		(14)
#define SCE_RUDP_OPTION_READ_TIMEOUT		(15)
#define SCE_RUDP_OPTION_WRITE_TIMEOUT		(16)
#define SCE_RUDP_OPTION_FLUSH_TIMEOUT		(17)
#define SCE_RUDP_OPTION_KEEP_ALIVE_INTERVAL	(18)
#define SCE_RUDP_OPTION_KEEP_ALIVE_TIMEOUT	(19)

/* Context states */
#define SCE_RUDP_STATE_IDLE			(0)
#define SCE_RUDP_STATE_CLOSED			(1)
#define SCE_RUDP_STATE_SYN_SENT			(2)
#define SCE_RUDP_STATE_SYN_RCVD			(3)
#define SCE_RUDP_STATE_ESTABLISHED		(4)
#define SCE_RUDP_STATE_CLOSE_WAIT		(5)

typedef struct SceRudpContextStatus {
	uint32_t state;
	int parentId;
	uint32_t children;
	uint32_t lostPackets;
	uint32_t sentPackets;
	uint32_t rcvdPackets;
	uint64_t sentBytes;
	uint64_t rcvdBytes;
	uint32_t retransmissions;
	uint32_t rtt;
} SceRudpContextStatus;

/* Poll operations */
#define SCE_RUDP_POLL_OP_ADD			(1)
#define SCE_RUDP_POLL_OP_MODIFY			(2)
#define SCE_RUDP_POLL_OP_REMOVE			(3)

/* Poll events */
#define SCE_RUDP_POLL_EV_READ			(0x0001)
#define SCE_RUDP_POLL_EV_WRITE			(0x0002)
#define SCE_RUDP_POLL_EV_FLUSH			(0x0004)
#define SCE_RUDP_POLL_EV_ERROR			(0x0008)

typedef struct SceRudpPollEvent {
	int ctxId;
	uint16_t reqEvents;
	uint16_t rtnEvents;
} SceRudpPollEvent;

typedef struct SceRudpStatus {
	uint64_t sentUdpBytes;
	uint64_t rcvdUdpBytes;
	uint32_t sentUdpPackets;
	uint32_t rcvdUdpPackets;

	uint64_t sentUserBytes;
	uint32_t sentUserPackets;
	uint32_t sentLatencyCriticalPackets;

	uint64_t rcvdUserBytes;
	uint32_t rcvdUserPackets;
	uint32_t rcvdLatencyCriticalPackets;

	uint32_t sentSynPackets;
	uint32_t rcvdSynPackets;
	uint32_t sentUsrPackets;
	uint32_t rcvdUsrPackets;
	uint32_t sentPrbPackets;
	uint32_t rcvdPrbPackets;
	uint32_t sentRstPackets;
	uint32_t rcvdRstPackets;

	uint32_t lostPackets;
	uint32_t retransmittedPackets;
	uint32_t reorderedPackets;

	uint32_t currentContexts;

	uint64_t sentQualityLevel1Bytes;
	uint64_t rcvdQualityLevel1Bytes;
	uint32_t sentQualityLevel1Packets;
	uint32_t rcvdQualityLevel1Packets;

	uint64_t sentQualityLevel2Bytes;
	uint64_t rcvdQualityLevel2Bytes;
	uint32_t sentQualityLevel2Packets;
	uint32_t rcvdQualityLevel2Packets;

	uint64_t sentQualityLevel3Bytes;
	uint64_t rcvdQualityLevel3Bytes;
	uint32_t sentQualityLevel3Packets;
	uint32_t rcvdQualityLevel3Packets;

	uint64_t sentQualityLevel4Bytes;
	uint64_t rcvdQualityLevel4Bytes;
	uint32_t sentQualityLevel4Packets;
	uint32_t rcvdQualityLevel4Packets;

	uint32_t allocs;
	uint32_t frees;
	uint32_t memCurrent;
	uint32_t memPeak;

	uint32_t establishedConnections;
	uint32_t failedConnections;

	uint32_t failedConnectionsReset;
	uint32_t failedConnectionsRefused;
	uint32_t failedConnectionsTimeout;
	uint32_t failedConnectionsVersionMismatch;
	uint32_t failedConnectionsTransportTypeMismatch;
	uint32_t failedConnectionsQualityLevelMismatch;
} SceRudpStatus;

#define SCE_RUDP_USEC_INDEFINITE		(0xffffffffffffffffULL)

/* Error codes */
#define SCE_RUDP_ERROR(_sts) \
	((int)(0x80000000U | ((unsigned)SCE_ERROR_FACILITY_RUDP << 16) | (unsigned)(_sts)))

#define SCE_RUDP_SUCCESS				(0)
#define SCE_RUDP_ERROR_NOT_INITIALIZED			SCE_RUDP_ERROR(1)
#define SCE_RUDP_ERROR_ALREADY_INITIALIZED		SCE_RUDP_ERROR(2)
#define SCE_RUDP_ERROR_INVALID_CONTEXT_ID		SCE_RUDP_ERROR(3)
#define SCE_RUDP_ERROR_INVALID_ARGUMENT			SCE_RUDP_ERROR(4)
#define SCE_RUDP_ERROR_INVALID_OPTION			SCE_RUDP_ERROR(5)
#define SCE_RUDP_ERROR_INVALID_MUXMODE			SCE_RUDP_ERROR(6)
#define SCE_RUDP_ERROR_MEMORY				SCE_RUDP_ERROR(7)
#define SCE_RUDP_ERROR_INTERNAL				SCE_RUDP_ERROR(8)
#define SCE_RUDP_ERROR_CONN_RESET			SCE_RUDP_ERROR(9)
#define SCE_RUDP_ERROR_CONN_REFUSED			SCE_RUDP_ERROR(10)
#define SCE_RUDP_ERROR_CONN_TIMEOUT			SCE_RUDP_ERROR(11)
#define SCE_RUDP_ERROR_CONN_VERSION_MISMATCH		SCE_RUDP_ERROR(12)
#define SCE_RUDP_ERROR_CONN_TRANSPORT_TYPE_MISMATCH	SCE_RUDP_ERROR(13)
#define SCE_RUDP_ERROR_CONN_QUALITY_LEVEL_MISMATCH	SCE_RUDP_ERROR(14)
#define SCE_RUDP_ERROR_THREAD				SCE_RUDP_ERROR(15)
#define SCE_RUDP_ERROR_THREAD_IN_USE			SCE_RUDP_ERROR(16)
#define SCE_RUDP_ERROR_NOT_ACCEPTABLE			SCE_RUDP_ERROR(17)
#define SCE_RUDP_ERROR_MSG_TOO_LARGE			SCE_RUDP_ERROR(18)
#define SCE_RUDP_ERROR_NOT_BOUND			SCE_RUDP_ERROR(19)
#define SCE_RUDP_ERROR_CANCELLED			SCE_RUDP_ERROR(20)
#define SCE_RUDP_ERROR_INVALID_VPORT			SCE_RUDP_ERROR(21)
#define SCE_RUDP_ERROR_WOULDBLOCK			SCE_RUDP_ERROR(22)
#define SCE_RUDP_ERROR_VPORT_IN_USE			SCE_RUDP_ERROR(23)
#define SCE_RUDP_ERROR_VPORT_EXHAUSTED			SCE_RUDP_ERROR(24)
#define SCE_RUDP_ERROR_INVALID_SOCKET			SCE_RUDP_ERROR(25)
#define SCE_RUDP_ERROR_BUFFER_TOO_SMALL			SCE_RUDP_ERROR(26)
#define SCE_RUDP_ERROR_MSG_MALFORMED			SCE_RUDP_ERROR(27)
#define SCE_RUDP_ERROR_ADDR_IN_USE			SCE_RUDP_ERROR(28)
#define SCE_RUDP_ERROR_ALREADY_BOUND			SCE_RUDP_ERROR(29)
#define SCE_RUDP_ERROR_ALREADY_EXISTS			SCE_RUDP_ERROR(30)
#define SCE_RUDP_ERROR_INVALID_POLL_ID			SCE_RUDP_ERROR(31)
#define SCE_RUDP_ERROR_TOO_MANY_CONTEXTS		SCE_RUDP_ERROR(32)
#define SCE_RUDP_ERROR_IN_PROGRESS			SCE_RUDP_ERROR(33)
#define SCE_RUDP_ERROR_NO_EVENT_HANDLER			SCE_RUDP_ERROR(34)
#define SCE_RUDP_ERROR_PAYLOAD_TOO_LARGE		SCE_RUDP_ERROR(35)
#define SCE_RUDP_ERROR_END_OF_DATA			SCE_RUDP_ERROR(36)
#define SCE_RUDP_ERROR_ALREADY_ESTABLISHED		SCE_RUDP_ERROR(37)
#define SCE_RUDP_ERROR_KEEP_ALIVE_FAILURE		SCE_RUDP_ERROR(38)

int sceRudpInit(void *memPool, int memPoolSize);
int sceRudpEnd(void);
int sceRudpEnableInternalIOThread(uint32_t stackSize, uint32_t priority);
int sceRudpEnableInternalIOThread2(uint32_t stackSize, uint32_t priority,
				   uint64_t affinityMask);
int sceRudpSetEventHandler(SceRudpEventHandler handler, void *arg);
int sceRudpSetMaxSegmentSize(uint16_t mss);
int sceRudpGetMaxSegmentSize(uint16_t *mss);
int sceRudpProcessEvents(SceRudpUsec timeout);
int sceRudpCreateContext(SceRudpContextEventHandler handler, void *arg,
			 int *ctxId);
int sceRudpBind(int ctxId, int soc, uint16_t vport, uint8_t muxMode);
int sceRudpActivate(int ctxId, SceNetSockaddrConstArg to,
		    SceNetSocklen_t toLen);
int sceRudpInitiate(int ctxId, SceNetSockaddrConstArg to,
		    SceNetSocklen_t toLen, uint16_t vport);
int sceRudpTerminate(int ctxId);
int sceRudpWrite(int ctxId, const void *data, size_t len, uint8_t flags);
int sceRudpRead(int ctxId, void *data, size_t len, uint8_t flags,
		SceRudpReadInfo *info);
SceSSize sceRudpGetSizeWritable(int ctxId);
SceSSize sceRudpGetSizeReadable(int ctxId);
int sceRudpGetNumberOfPacketsToRead(int ctxId);
int sceRudpFlush(int ctxId);
int sceRudpNetReceived(int soc, const uint8_t *data, size_t dataLen,
		       SceNetSockaddrConstArg from, SceNetSocklen_t fromLen);
int sceRudpSetOption(int ctxId, int option, const void *optVal, size_t optLen);
int sceRudpGetOption(int ctxId, int option, void *optVal, size_t optLen);
int sceRudpGetLocalInfo(int ctxId, int *soc, SceNetSockaddrArg addr,
			SceNetSocklen_t *addrLen, uint16_t *vport,
			uint8_t *muxMode);
int sceRudpGetRemoteInfo(int ctxId, SceNetSockaddrArg addr,
			 SceNetSocklen_t *addrLen, uint16_t *vport);
int sceRudpGetContextStatus(int ctxId, SceRudpContextStatus *status,
			    size_t statusSize);
int sceRudpPollCreate(size_t size);
int sceRudpPollDestroy(int pollId);
int sceRudpPollControl(int pollId, int op, int ctxId, uint16_t events);
int sceRudpPollWait(int pollId, SceRudpPollEvent *events, size_t eventLen,
		    SceRudpUsec timeout);
int sceRudpPollCancel(int pollId);
int sceRudpGetStatus(SceRudpStatus *status, size_t statusSize);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_RUDP_H */
