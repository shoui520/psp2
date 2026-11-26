/*
 * rudp_internal.h: internal C++ classes and OSAL declarations for SceLibRudp.
 */
#ifndef _RUDP_INTERNAL_H
#define _RUDP_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>
#include <rudp.h>
#include <libsha256.h>
#include <libkernel/libkernel.h>
#include <kernel/error.h>
#include <kernel/processmgr.h>
#include <net/socket.h>
#include <net/epoll.h>
#include <net/inet.h>
#include <net/errno.h>

/* Internal context/option/mux constants beyond the public SDK subset */
#define SCE_RUDP_MUXMODE_NONE				(0)
#define SCE_RUDP_MUXMODE_CLIENT_SERVER			(2)

#define SCE_RUDP_OPTION_SACK				(12)
#define SCE_RUDP_OPTION_COOKIE_TIMEOUT			(13)
#define SCE_RUDP_OPTION_INTERNAL_DEBUG			(255)

#define SCE_RUDP_CONTEXT_EVENT_ACCEPTED			(7)

#define SCE_RUDP_STATE_LISTEN				(6)

/* Internal DNTP OSAL error codes */
#define RUDP_OSAL_ETIMEDOUT				(-2080440260)
#define RUDP_OSAL_ESRCH					(-2080440309)
#define RUDP_OSAL_EINVAL				(-2080440298)
#define RUDP_OSAL_EWOULDBLOCK				(-2080440285)

#define RUDP_NET_ERRNO_EINTR				(4)
#define RUDP_NET_ERRNO_EAGAIN				(35)
#define RUDP_NET_ERRNO_ECONNRESET			(54)

#define RUDP_CONTEXT_MAGIC				(0x7BE434CB)
#define RUDP_DEFAULT_MSS				(1410)
#define RUDP_HEADER_OVERHEAD				(64)
#define RUDP_DEFAULT_BUFFER_SIZE			(0x10000)
#define RUDP_DEFAULT_CONN_TIMEOUT_MS			(60000)
#define RUDP_DEFAULT_AGGR_TIMEOUT_MS			(30)
#define RUDP_DEFAULT_KEEPALIVE_TIMEOUT_MS		(20000)
#define RUDP_DEFAULT_COOKIE_LIFETIME_SEC		(10)
#define RUDP_INITIAL_RTO_USEC				(1000000ULL)
#define RUDP_MAX_RTO_USEC				(16000000ULL)
#define RUDP_MIN_CLOCK_GRANULARITY_USEC			(16000ULL)
#define RUDP_DEFAULT_TIMER_INTERVAL_USEC		(125000ULL)
#define RUDP_DELAYED_ACK_USEC				(100000ULL)
#define RUDP_ZERO_WINDOW_PROBE_INIT_MS			(3000)
#define RUDP_ZERO_WINDOW_PROBE_MAX_MS			(16000)
#define RUDP_VPORT_EPHEMERAL_BASE			(10000)
#define RUDP_MAX_SACK_BLOCKS				(8)
#define RUDP_MAX_COOKIE_SIZE				(32)
#define RUDP_MAX_AGGREGATED_PACKETS			(64)
#define RUDP_MAX_RING_CAPACITY				(0x10000)

/* Protocol packet types */
enum RudpPacketType {
	RUDP_PKT_DATA = 0,
	RUDP_PKT_PING = 1,
	RUDP_PKT_SYN  = 2,
	RUDP_PKT_RST  = 3
};

/* Protocol wire header flags */
#define RUDP_HDR_FLAG_MORE_FRAGMENTS			(0x02)
#define RUDP_HDR_FLAG_ACK_DELAY				(0x04)
#define RUDP_HDR_FLAG_LATENCY_CRITICAL			(0x08)
#define RUDP_HDR_FLAG_HAS_OPTIONS			(0x10)
#define RUDP_HDR_FLAG_ACK				(0x20)

/* Protocol SYN capability bits */
#define RUDP_SYN_CAP_STREAM				(0x01)
#define RUDP_SYN_CAP_RELIABLE				(0x02)
#define RUDP_SYN_CAP_ORDERED				(0x04)

/* Protocol header extension presence bits */
#define RUDP_EXT_COOKIE					(0x01)
#define RUDP_EXT_MSS					(0x02)

/* Protocol header extra option bits */
#define RUDP_OPT_RETRY_INFO				(0x01)
#define RUDP_OPT_TX_TIMESTAMP				(0x02)

/* Protocol packet queue entry flags */
#define RUDP_QPKT_SENT					(0x0001)
#define RUDP_QPKT_RETRANSMIT				(0x0002)
#define RUDP_QPKT_SACKED				(0x0004)
#define RUDP_QPKT_FLUSH_MARKER				(0x0008)
#define RUDP_QPKT_ALIGN_32				(0x0010)
#define RUDP_QPKT_ALIGN_64				(0x0020)
#define RUDP_QPKT_LATENCY_CRITICAL			(0x0100)
#define RUDP_QPKT_MORE_FRAGMENTS			(0x0200)
#define RUDP_QPKT_RETRANSMITTED				(0x0400)
#define RUDP_QPKT_HAS_TIMESTAMP				(0x0800)
#define RUDP_QPKT_TX_POOL				(0x8000)

/* Protocol state bitflags (m_flags at +308) */
#define RUDP_PROTO_FLAG_INITIATING			(0x00000001U)
#define RUDP_PROTO_FLAG_READABLE_NOTIFIED		(0x00000002U)
#define RUDP_PROTO_FLAG_WRITE_BLOCKED			(0x00000004U)
#define RUDP_PROTO_FLAG_FLUSH_PENDING			(0x00000008U)
#define RUDP_PROTO_FLAG_SEND_SCHEDULED			(0x00000010U)
#define RUDP_PROTO_FLAG_ACK_SCHEDULED			(0x00000020U)
#define RUDP_PROTO_FLAG_ACTIVATING			(0x00000080U)
#define RUDP_PROTO_FLAG_ORDERED				(0x00010000U)
#define RUDP_PROTO_FLAG_RELIABLE			(0x00020000U)
#define RUDP_PROTO_FLAG_STREAM				(0x00040000U)
#define RUDP_PROTO_FLAG_NODELAY				(0x00080000U)
#define RUDP_PROTO_FLAG_SACK				(0x00100000U)
#define RUDP_PROTO_FLAG_NEED_INITIAL_SEQ		(0x00400000U)
#define RUDP_PROTO_FLAG_DEBUG				(0x80000000U)
#define RUDP_PROTO_CONFIG_FLAGS_MASK			(0xFFFFFF00U)
#define RUDP_PROTO_DEFAULT_FLAGS			(0x00730000U)

/* Context state bitflags (m_contextFlags at +628) */
#define RUDP_CTX_FLAG_LISTENING				(0x01)
#define RUDP_CTX_FLAG_REMOTE_BOUND			(0x02)
#define RUDP_CTX_FLAG_TERMINATING			(0x04)

/* Context blocking wait wakeup bits (m_waitFlags at +629) */
#define RUDP_WAIT_ESTABLISHED				(0x01)
#define RUDP_WAIT_CLOSED				(0x02)
#define RUDP_WAIT_WRITABLE				(0x04)
#define RUDP_WAIT_READABLE				(0x08)
#define RUDP_WAIT_FLUSHED				(0x10)
#define RUDP_WAIT_ACCEPTED				(0x20)
#define RUDP_WAIT_CANCELLED				(0x80)

/* Internal scheduler event types */
enum RudpEventType {
	RUDP_EV_CONNECTED       = 0,
	RUDP_EV_CONNECT_FAILED  = 1,
	RUDP_EV_DISCONNECTED    = 2,
	RUDP_EV_READABLE        = 3,
	RUDP_EV_WRITABLE        = 4,
	RUDP_EV_FLUSHED         = 5,
	RUDP_EV_SEND_SYN        = 6,
	RUDP_EV_SEND_ACK        = 7,
	RUDP_EV_SEND_RST        = 8,
	RUDP_EV_SEND_DATA       = 9,
	RUDP_EV_SEND_PING       = 10,
	RUDP_EV_CHILD_ACCEPTED  = 11,
	RUDP_EV_CHILD_FAILED    = 12,
	RUDP_EV_CLOSED          = 13,
	RUDP_EV_SEND_COOKIE     = 14,
	RUDP_EV_OUT_OF_MEMORY   = 15,
	RUDP_EV_SOCKET_ERROR    = 16,
	RUDP_EV_SOCKET_RELEASED = 17
};

/* Disconnect / reset reason codes */
enum RudpDisconnectReason {
	RUDP_REASON_NORMAL                  = 0,
	RUDP_REASON_RESET                   = 1,
	RUDP_REASON_REFUSED                 = 2,
	RUDP_REASON_TIMEOUT                 = 3,
	RUDP_REASON_VERSION_MISMATCH        = 4,
	RUDP_REASON_TRANSPORT_TYPE_MISMATCH = 5,
	RUDP_REASON_QUALITY_LEVEL_MISMATCH  = 6,
	RUDP_REASON_KEEP_ALIVE_FAILURE      = 8,
	RUDP_REASON_OUT_OF_MEMORY           = 9
};

/* Allocator descriptor passed to rudpInitInternal */
struct RudpAllocator {
	void *(*allocFn)(SceSize size);
	void (*freeFn)(void *ptr);
};

/* OSAL poll handle and event types */
struct RudpOsalPollEvent {
	int sock;
	uint16_t events;
	uint16_t revents;
};

struct RudpOsalPoll {
	SceNetId epollId;
	SceNetEpollEvent *events;
	uint32_t capacity;
	void *(*allocFn)(SceSize size);
	void (*freeFn)(void *ptr);
};

/* Packed 34-byte network address storage */
struct RudpSockaddr {
	uint8_t sa_len;
	uint8_t sa_family;
	uint8_t sa_data[32];
};

/* 36-byte peer endpoint key (sockaddr + vport) */
struct RudpEndpoint {
	RudpSockaddr addr;
	uint16_t vport;
};

/* 40-byte aggregator key (socket + sockaddr) */
struct RudpAggregatorKey {
	int sock;
	RudpSockaddr addr;
	uint16_t reserved;
};

/* SACK range pair */
struct RudpSackBlock {
	uint16_t start;
	uint16_t end;
};

/* Scheduler event payload (12 bytes) */
struct RudpEvent {
	uint32_t handlerId;
	uint8_t type;
	uint8_t detail;
	int arg;
};

/* Forward declarations */
class RudpBuffer;
class RudpHeader;
class RudpProtocol;
class RudpContext;
class RudpContextManager;
class RudpScheduler;
class RudpAggregator;
class RudpNetworker;
class RudpPoll;
class RudpPollManager;
class RudpCore;
class RudpResult;

/* Packet transmit request passed from RudpProtocol to RudpContext */
struct RudpTransmitRequest {
	RudpProtocol *protocol;
	RudpHeader *header;
	RudpBuffer *payload;
	uint32_t aggrDelayMs;
	uint32_t *txTimestampOut;
};

/* Protocol statistics snapshot (56 bytes) */
struct RudpProtocolStats {
	uint32_t lostPackets;
	uint32_t reorderedPackets;
	uint32_t duplicatePackets;
	uint32_t reserved;
	uint32_t sentPackets;
	uint32_t rcvdPackets;
	uint64_t sentBytes;
	uint64_t rcvdBytes;
	uint32_t retransmissions;
	uint32_t reservedTail;
	uint64_t smoothedRttUsec;
};

/* OSAL functions (rudp_osal.cpp) */
uint32_t rudpAtomicCompareAndSwap(uint32_t *target, uint32_t expected, uint32_t desired);
uint32_t rudpAtomicSwap(uint32_t *target, uint32_t desired);
uint32_t rudpAtomicAdd(uint32_t *target, int32_t delta);
uint32_t rudpAtomicSub(uint32_t *target, int32_t delta);
uint32_t rudpAtomicAnd(uint32_t *target, uint32_t mask);
uint32_t rudpAtomicOr(uint32_t *target, uint32_t mask);

int rudpCondInit(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex);
int rudpCondTerm(SceKernelLwCondWork *cond);
int rudpCondSignal(SceKernelLwCondWork *cond);
int rudpCondBroadcast(SceKernelLwCondWork *cond);
int rudpCondWait(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex);
int rudpCondTimedWait(SceKernelLwCondWork *cond, SceKernelLwMutexWork *mutex, uint64_t timeoutUsec);

int rudpSha256Init(SceSha256Context *ctx);
int rudpSha256Update(SceSha256Context *ctx, const void *data, uint32_t len);
int rudpSha256Final(SceSha256Context *ctx, uint8_t *digest);

void *rudpMemcpy(void *dst, const void *src, SceSize size);
int rudpMemcmp(const void *lhs, const void *rhs, SceSize size);
void *rudpMemset(void *dst, int value, SceSize size);

int rudpMutexInit(SceKernelLwMutexWork *mutex, int recursive);
int rudpMutexTerm(SceKernelLwMutexWork *mutex);
int rudpMutexLock(SceKernelLwMutexWork *mutex);
int rudpMutexUnlock(SceKernelLwMutexWork *mutex);

int rudpNetSocket(int domain, int type);
int rudpNetBind(int sock, const SceNetSockaddr *addr);
int rudpNetClose(int sock);
int rudpNetSendto(int sock, const void *buf, uint32_t len, int flags, const SceNetSockaddr *to);
int rudpNetRecvfrom(int sock, void *buf, uint32_t len, int flags, SceNetSockaddr *from);
int rudpNetGetsockname(int sock, SceNetSockaddr *addr);
int rudpNetSetNonBlocking(int sock, uint8_t enable);
int rudpNetInetPton(int family, const char *src, RudpSockaddr *dst);
int rudpNetPollCreate(RudpOsalPoll **outPoll, int initialCapacity, const RudpAllocator *alloc);
int rudpNetPollDestroy(RudpOsalPoll *poll);
int rudpNetPollControl(RudpOsalPoll *poll, int op, int sock, uint16_t events);
int rudpNetPollWait(RudpOsalPoll *poll, RudpOsalPollEvent *events, uint32_t maxEvents, int unused, int timeoutMs);
int rudpNetGetLastError(void);

void rudpOsalNoop(void);
int rudpSetComponentName(const char *name);
uint64_t rudpGetTimeUsec(void);
uint32_t rudpRandU32(void);
extern "C" void abort(void);
inline void rudpAbort(void) { abort(); }
SceSize rudpStrlenHelper(const char *str);
void *rudpMemcpySafeHelper(void *dst, const void *src, SceSize size);
int rudpThreadCreate(SceUID *outUid, int (*entry)(void *), void *arg, SceSize stackSize, int priority, int unused, uint64_t affinity);
int rudpThreadJoin(SceUID uid, int *exitStatus);
int rudpDelay1Ms(void);
int rudpOsalInit(int flags);
void rudpOsalTerm(void);

/* RAII scoped lightweight mutex lock */
class RudpMutexLock {
public:
	explicit RudpMutexLock(SceKernelLwMutexWork *mutex) : m_mutex(mutex) {
		rudpMutexLock(m_mutex);
	}
	~RudpMutexLock() {
		rudpMutexUnlock(m_mutex);
	}
private:
	SceKernelLwMutexWork *m_mutex;
};

/* Utility / allocator functions (rudp_util.cpp) */
void *operator new(size_t size);
void operator delete(void *ptr);
void rudpFree(void *ptr);
inline void *rudpAlloc(SceSize size) { return ::operator new(size); }
#define RUDP_MEM_DELETE static void operator delete(void *ptr) { rudpFree(ptr); }
int rudpSockaddrEqual(const RudpSockaddr *lhs, const RudpSockaddr *rhs);
int rudpSockaddrLess(const RudpSockaddr *lhs, const RudpSockaddr *rhs);
int rudpRecordError(const char *file, int line);
int rudpMemInit(const RudpAllocator *alloc, void *pool, SceSize poolSize);
void rudpMemTerm(void);
void rudpMemGetStats(uint32_t *allocs, uint32_t *frees, uint32_t *memCurrent, uint32_t *memPeak);
int rudpInitInternal(const RudpAllocator *alloc, void *memPool, SceSize memPoolSize);

/* Error-checked 8-byte result wrapper (non-trivial dtor -> sret in r0) */
class RudpResult {
public:
	RudpResult();
	RudpResult(int code);
	RudpResult(const RudpResult &other);
	~RudpResult();

	RudpResult &operator=(int code);
	RudpResult &operator=(const RudpResult &other);
	int getCode();

private:
	int m_code;
	mutable bool m_unchecked;
};

/* Byte buffer with read/write cursor (16 bytes) */
class RudpBuffer {
public:
	RudpBuffer();
	RudpBuffer(void *data, uint32_t capacity);
	~RudpBuffer();

	int writeU8(uint8_t value);
	int writeU16(uint16_t value);
	int writeU32(uint32_t value);
	int putBytes(const void *src, uint32_t len);

	int readU8(uint8_t *out);
	int readU16(uint16_t *out);
	int readU32(uint32_t *out);
	int getBytes(void *dst, uint32_t len);

	int writeAt(uint32_t offset, const void *src, uint32_t len);
	void setLength(uint32_t len);
	void reset();

	uint8_t *getCurrentPtr() const { return m_data + m_offset; }
	uint32_t getLength() const { return m_length; }
	uint32_t getCapacity() const { return m_capacity; }
	uint32_t getOffset() const { return m_offset; }
	uint32_t getRemainingSpace() const { return m_capacity - m_offset - m_length; }
	void setBuffer(uint8_t *data, uint32_t capacity) {
		reset();
		m_data = data;
		m_capacity = capacity;
	}

	uint8_t *m_data;
	uint32_t m_length;
	uint32_t m_capacity;
	uint32_t m_offset;
};

/* Pooled packet buffer (40 bytes) */
class RudpPacket {
public:
	RUDP_MEM_DELETE
	RudpBuffer m_buf;
	uint32_t m_timestamp;
	uint32_t m_retryInfo;
	uint16_t m_flags;
	uint16_t m_seq;
	RudpPacket *m_next;
	uint8_t *m_storage;
	int m_allocError;
};

/* Packet pool (20 bytes) */
class RudpPacketPool {
public:
	RudpPacketPool();
	~RudpPacketPool();

	int clear();
	RudpPacket *acquire(uint8_t forTx);
	void release(RudpPacket *pkt);

	RudpPacket *m_freeList;
	uint16_t m_freeCount;
	uint16_t m_totalAllocated;
	uint16_t m_txInUse;
	uint16_t m_rxInUse;
	uint16_t m_txPeak;
	uint16_t m_rxPeak;
	uint16_t m_packetCapacity;
};

/* Intrusive Red-Black Tree (rudp_rbtree.cpp) */
typedef int (*RudpRbCompareFn)(const void *lhs, const void *rhs);

class RudpRbNode {
public:
	RudpRbNode() : m_left(0), m_right(0), m_parent(0), m_color(0) {}
	virtual ~RudpRbNode() {}
	virtual const void *getKey() const = 0;

	RudpRbNode *m_left;
	RudpRbNode *m_right;
	RudpRbNode *m_parent;
	uint8_t m_color;
};

class RudpRbSentinel : public RudpRbNode {
public:
	RudpRbSentinel() {}
	virtual ~RudpRbSentinel() {}
	virtual const void *getKey() const { return (const void *)0xBAD; }
};

class RudpRbTree {
public:
	RudpRbTree(RudpRbCompareFn compareFn);
	~RudpRbTree() { clear(); }

	void rotateLeft(RudpRbNode *node);
	void rotateRight(RudpRbNode *node);
	void insertFixup(RudpRbNode *node);
	int insert(RudpRbNode *node);
	RudpRbNode *prev(RudpRbNode *node) const;
	RudpRbNode *find(const void *key) const;
	void removeFixup(RudpRbNode *node);
	RudpRbNode *next(RudpRbNode *node, int *depth) const;
	void remove(RudpRbNode *node);
	RudpRbNode *removeByKey(const void *key);
	RudpRbNode *first(int *depth) const;
	void clear();
	uint32_t getCount() const;

	uint32_t m_count;
	RudpRbCompareFn m_compareFn;
	RudpRbNode *m_root;
	RudpRbSentinel m_sentinel;
};

/* Typed RB-tree node subclasses */
struct RudpTimerNode : public RudpRbNode {
	RUDP_MEM_DELETE
	uint64_t m_expireTime;
	uint32_t m_handlerId;

	virtual const void *getKey() const { return &m_expireTime; }
};

struct RudpContextMapNode : public RudpRbNode {
	RUDP_MEM_DELETE
	int m_ctxId;
	RudpContext *m_context;
	RudpContextMapNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_ctxId; }
};

struct RudpPendingChildNode : public RudpRbNode {
	RUDP_MEM_DELETE
	uint64_t m_timestamp;
	RudpContext *m_context;
	RudpPendingChildNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_timestamp; }
};

struct RudpEndpointContextNode : public RudpRbNode {
	RUDP_MEM_DELETE
	RudpEndpoint m_endpoint;
	RudpContext *m_context;
	RudpEndpointContextNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_endpoint; }
};

struct RudpContextPtrNode : public RudpRbNode {
	RUDP_MEM_DELETE
	RudpContext *m_key;
	RudpContext *m_value;
	RudpContextPtrNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_key; }
};

struct RudpVportNode : public RudpRbNode {
	RUDP_MEM_DELETE
	uint16_t m_vport;
	RudpContext *m_context;
	RudpVportNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_vport; }
};

struct RudpSocketEntryNode : public RudpRbNode {
	RUDP_MEM_DELETE
	int m_key;
	int m_sock;
	uint16_t m_nextEphemeralVport;
	RudpRbTree m_contextSet;
	RudpRbTree m_endpointMap;
	RudpRbTree m_vportMap;
	RudpSocketEntryNode *m_cleanupNext;

	RudpSocketEntryNode(RudpRbCompareFn ctxCmp, RudpRbCompareFn epCmp, RudpRbCompareFn vpCmp);
	virtual ~RudpSocketEntryNode();
	virtual const void *getKey() const { return &m_key; }
};

struct RudpPollContextNode : public RudpRbNode {
	RUDP_MEM_DELETE
	int m_ctxId;
	uint16_t m_reqEvents;
	uint16_t m_rtnEvents;
	RudpPollContextNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_ctxId; }
};

struct RudpPollEntryNode : public RudpRbNode {
	RUDP_MEM_DELETE
	int m_pollId;
	RudpPoll *m_poll;
	RudpPollEntryNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_pollId; }
};

struct RudpAggregatorBuffer {
	RUDP_MEM_DELETE
	RudpBuffer m_buf;
	uint8_t *m_storage;
	uint32_t m_padding;
	uint64_t m_startTimeUsec;
	struct ContextEntry {
		int ctxId;
		uint32_t aggrTimeoutMs;
		ContextEntry *next;
	} *m_contextList;
	uint32_t m_contextCount;
	uint32_t m_maxTimeoutMs;
	uint32_t m_deadlineMs;
	uint32_t m_packetEnqueuedMs[RUDP_MAX_AGGREGATED_PACKETS];
	uint32_t *m_packetTxTimestampOut[RUDP_MAX_AGGREGATED_PACKETS];
	uint16_t m_packetCount;
};

struct RudpAggregatorNode : public RudpRbNode {
	RUDP_MEM_DELETE
	RudpAggregatorKey m_key;
	RudpAggregatorBuffer *m_buffer;
	RudpAggregatorNode *m_cleanupNext;

	virtual const void *getKey() const { return &m_key; }
};

/* Ring buffer helper (20 bytes) */
template <typename T>
struct RudpRingBuffer {
	T *m_items;
	uint32_t m_capacity;
	uint32_t m_head;
	uint32_t m_tail;
	bool m_full;

	uint32_t getSize() const {
		if (m_items == 0 || m_capacity == 0) {
			return 0;
		}
		if (m_tail == m_head) {
			return m_full ? m_capacity : 0;
		}
		if (m_head >= m_tail) {
			return m_tail + m_capacity - m_head;
		}
		return m_tail - m_head;
	}
};

/* Scheduler interfaces & class (rudp_scheduler.cpp) */
class IRudpEventHandler {
public:
	IRudpEventHandler() : m_timerIntervalUsec(0) {}
	virtual ~IRudpEventHandler() {}
	virtual void onEvent(const RudpEvent *ev) = 0;
	virtual void onTimer(uint64_t *nextIntervalUsec) = 0;

	uint64_t m_timerIntervalUsec;
};

class IRudpSchedulerDelegate {
public:
	virtual ~IRudpSchedulerDelegate() {}
	virtual void onEventPosted(RudpScheduler *scheduler) = 0;
};

struct RudpHandlerBucket {
	uint32_t handlerId;
	IRudpEventHandler *handler;
	RudpHandlerBucket *next;
};

class RudpScheduler {
public:
	RUDP_MEM_DELETE
	RudpScheduler();
	~RudpScheduler();

	void cancelWaits();
	void setDelegate(IRudpSchedulerDelegate *delegate);
	RudpResult registerHandler(uint32_t *outId, IRudpEventHandler *handler);
	RudpResult unregisterHandler(uint32_t handlerId);
	RudpResult postEvent(const RudpEvent *ev);
	RudpResult startTimer(uint32_t handlerId, uint64_t intervalUsec);
	RudpResult stopTimer(uint32_t handlerId);
	RudpResult processEvents(uint64_t timeoutUsec);
	bool isValid() const;

	IRudpSchedulerDelegate *m_delegate;
	RudpRingBuffer<RudpEvent> m_eventQueue;
	uint32_t m_bucketCount;
	RudpHandlerBucket **m_buckets;
	uint32_t m_handlerCount;
	uint32_t m_lastHandlerId;
	RudpRbTree m_timerTree;
	SceKernelLwCondWork m_cond;
	bool m_eventSignalled;
	int m_waiterCount;
	bool m_cancelling;
};

/* Wire header codec (rudpHeader.cpp, 108 bytes) */
class RudpHeader {
public:
	RudpHeader();

	void encodeWindow(uint16_t windowPackets);
	uint16_t decodeWindow() const;
	void setCookie(const void *cookie, uint32_t len);
	void getCookie(const uint8_t **outData, uint32_t *outLen) const;
	void addSackBlock(const RudpSackBlock *block);
	uint32_t getSackCount() const;
	const RudpSackBlock *getSackBlock(uint32_t index) const;
	void reset();
	void setPaddingAlignment(uint8_t alignment);
	RudpResult serialize(RudpBuffer *buf);
	RudpResult parse(RudpBuffer *buf);

	static RudpResult readAckDelay(const uint8_t *rawHeader, uint16_t *outAckSeq);
	static RudpResult writeAckDelay(uint8_t *rawHeader, uint16_t delayMs);

	uint8_t m_type;
	uint8_t m_flags;
	uint16_t m_maxVersion;
	uint16_t m_minVersion;
	uint8_t m_synCaps;
	uint8_t m_extFlags;
	uint16_t m_seq;
	uint16_t m_ack;
	uint32_t m_connectionId;
	uint16_t m_ackDelayMs;
	uint16_t m_mss;
	uint8_t m_windowEncoded;
	uint8_t m_synQuality;
	uint8_t m_rstReason;
	uint8_t m_reservedByte;
	uint8_t m_cookie[RUDP_MAX_COOKIE_SIZE];
	uint32_t m_cookieLen;
	RudpSackBlock m_sackBlocks[RUDP_MAX_SACK_BLOCKS];
	uint32_t m_sackCount;
	uint8_t m_padAlign;
	uint8_t m_optFlags;
	uint16_t m_reservedHalf;
	uint32_t m_retryInfo;
	uint32_t m_txTimestamp;
};

/* Worker thread base class (rudp_thread.cpp) */
class RudpThread {
public:
	RUDP_MEM_DELETE
	RudpThread();
	virtual ~RudpThread();

	RudpResult setStackSize(uint32_t stackSize);
	RudpResult setPriority(int priority);
	RudpResult setAffinityMask(uint64_t affinityMask);
	virtual RudpResult start();
	virtual RudpResult stop();
	virtual int run() = 0;

	static int threadMain(void *arg);

	SceUID m_threadId;
	uint32_t m_stackSize;
	int m_priority;
	uint64_t m_affinityMask;
	uint32_t m_reservedThread;
	SceKernelLwMutexWork m_mutex;
	SceKernelLwCondWork m_cond;
	bool m_running;
	bool m_started;
};

/* Network IO thread (rudpNetworker.cpp) */
class IRudpNetworkerDelegate {
public:
	virtual ~IRudpNetworkerDelegate() {}
	virtual int onNetworkerReceived(RudpNetworker *net, int sock, const uint8_t *data, uint32_t len, const SceNetSockaddr *addr, uint32_t addrLen) = 0;
	virtual void onNetworkerSocketError(RudpNetworker *net, int sock) = 0;
};

class RudpNetworker : public RudpThread {
public:
	RudpNetworker(RudpScheduler *scheduler);
	virtual ~RudpNetworker();

	void setDelegate(IRudpNetworkerDelegate *delegate);
	void setBufferSize(uint16_t size);
	virtual RudpResult start();
	void wakeUp();
	virtual RudpResult stop();
	RudpResult addSocket(int sock);
	RudpResult removeSocket(int sock);
	virtual int run();

	RudpScheduler *m_scheduler;
	IRudpNetworkerDelegate *m_delegate;
	RudpOsalPoll *m_poll;
	int m_wakeSock;
	RudpSockaddr m_wakeAddr;
	uint16_t m_reservedAddr;
	uint32_t m_reservedWake;
	uint32_t m_wakePending;
	uint16_t m_recvBufSize;
	uint8_t *m_recvBuf;
};

/* Packet aggregator (rudpAggregator.cpp) */
class IRudpAggregatorDelegate {
public:
	virtual ~IRudpAggregatorDelegate() {}
	virtual int onAggregatorSend(RudpAggregator *aggr, int sock, const RudpSockaddr *addr, RudpBuffer *buf) = 0;
};

class RudpAggregator : public IRudpEventHandler {
public:
	RudpAggregator(RudpScheduler *scheduler);
	virtual ~RudpAggregator();

	void setDelegate(IRudpAggregatorDelegate *delegate);
	void setMaxSegmentSize(uint16_t mss);
	uint16_t getMaxSegmentSize() const;
	RudpResult registerContext(const RudpAggregatorKey *key, int ctxId, uint32_t aggrTimeoutMs);
	void unregisterContext(const RudpAggregatorKey *key, int ctxId);
	RudpResult flushBuffer(const RudpAggregatorKey *key, RudpAggregatorBuffer *buf, uint64_t nowUsec);
	RudpResult appendPacket(RudpAggregatorBuffer *buf, uint8_t muxMode, uint16_t srcVport, uint16_t dstVport, RudpHeader *header, RudpBuffer *payload, uint32_t nowMs, uint32_t *txTimestampOut);
	void enqueuePacket(const RudpAggregatorKey *key, int ctxId, uint8_t muxMode, uint16_t srcVport, uint16_t dstVport, RudpHeader *header, RudpBuffer *payload, uint32_t aggrDelayMs, uint32_t *txTimestampOut);
	void invalidateContextTimestamps(const RudpAggregatorKey *key, uint16_t srcVport);
	bool isValid() const;

	virtual void onEvent(const RudpEvent *ev);
	virtual void onTimer(uint64_t *nextIntervalUsec);

	RudpScheduler *m_scheduler;
	IRudpAggregatorDelegate *m_delegate;
	uint32_t m_handlerId;
	uint16_t m_maxSegmentSize;
	uint32_t m_timerIntervalMs;
	RudpRbTree m_entries;
	uint8_t m_flags;
};

/* Poll object & manager (rudp_poll.cpp) */
class IRudpPollDelegate {
public:
	virtual ~IRudpPollDelegate() {}
	virtual void onPollQueryEvents(int ctxId, uint16_t *outEvents) = 0;
	virtual void onPollDetachContext(int ctxId) = 0;
};

class RudpPoll {
public:
	RUDP_MEM_DELETE
	RudpPoll(int pollId, uint32_t initialCapacity, IRudpPollDelegate *delegate);
	~RudpPoll();

	int getId() const;
	void cancel(bool setPendingFlag, bool waitAll);
	RudpResult addContext(int ctxId, uint16_t reqEvents, uint16_t currentEvents);
	RudpResult modifyContext(int ctxId, uint16_t reqEvents, uint16_t currentEvents);
	RudpResult removeContext(int ctxId);
	int wait(SceRudpPollEvent *events, uint32_t maxEvents, uint64_t timeoutUsec);
	void notifyContext(int ctxId, uint16_t events);

	int m_pollId;
	IRudpPollDelegate *m_delegate;
	RudpRbTree m_contexts;
	RudpRingBuffer<int> m_readyQueue;
	uint32_t m_lastReportedCount;
	SceKernelLwCondWork m_cond;
	int m_waiterCount;
	int m_cancellerCount;
	bool m_cancelPending;
	bool m_destroying;
};

class RudpPollManager {
public:
	RudpPollManager();
	~RudpPollManager();

	void clear();
	RudpResult createPoll(uint32_t size, IRudpPollDelegate *delegate);
	RudpResult destroyPoll(int pollId);
	RudpPoll *findPoll(int pollId);
	void cancelAll();

	RudpRbTree m_polls;
	int m_nextPollId;
};

/* Protocol state machine (rudp_protocol.cpp, 368 bytes) */
class IRudpProtocolDelegate {
public:
	virtual ~IRudpProtocolDelegate() {}
	virtual void onProtocolConnected(RudpProtocol *proto) = 0;
	virtual void onProtocolConnectFailed(RudpProtocol *proto, uint8_t reason) = 0;
	virtual void onProtocolDisconnected(RudpProtocol *proto, uint8_t reason) = 0;
	virtual void onProtocolReadable(RudpProtocol *proto) = 0;
	virtual void onProtocolWritable(RudpProtocol *proto) = 0;
	virtual void onProtocolFlushed(RudpProtocol *proto) = 0;
	virtual void onProtocolScheduleSyn(RudpProtocol *proto) = 0;
	virtual void onProtocolScheduleAck(RudpProtocol *proto) = 0;
	virtual void onProtocolScheduleRst(RudpProtocol *proto, uint32_t connId, uint8_t reason) = 0;
	virtual void onProtocolScheduleData(RudpProtocol *proto) = 0;
	virtual void onProtocolSchedulePing(RudpProtocol *proto) = 0;
	virtual void onProtocolSetTimer(RudpProtocol *proto, uint64_t intervalUsec) = 0;
	virtual void onProtocolTransmit(const RudpTransmitRequest *req) = 0;
	virtual void onProtocolClosed(RudpProtocol *proto) = 0;
	virtual void onProtocolCancelAggregator(RudpProtocol *proto) = 0;
	virtual void onProtocolGenerateRandom(RudpProtocol *proto, uint8_t *outDigest32) = 0;
};

class RudpProtocol {
public:
	RUDP_MEM_DELETE
	RudpProtocol();
	virtual ~RudpProtocol();

	int clearSendQueue();
	void setDelegate(IRudpProtocolDelegate *delegate);

	RudpResult setMaxSegmentSize(uint32_t mss);
	uint32_t getMaxSegmentSize() const;
	RudpResult setSendBufferSize(uint32_t size);
	uint32_t getSendBufferSize() const;
	RudpResult setRecvBufferSize(uint32_t size);
	uint32_t getRecvBufferSize() const;
	RudpResult setConnectionTimeout(uint32_t ms);
	uint32_t getConnectionTimeout() const;
	RudpResult setCloseWaitTimeout(uint32_t ms);
	uint32_t getCloseWaitTimeout() const;
	RudpResult setAggregationTimeout(uint32_t ms);
	uint32_t getAggregationTimeout() const;
	RudpResult setStreamMode(int enable);
	bool getStreamMode() const;
	RudpResult setDeliveryCritical(uint8_t enable);
	bool getDeliveryCritical() const;
	RudpResult setOrderCritical(uint8_t enable);
	bool getOrderCritical() const;
	RudpResult setNoDelay(uint8_t enable);
	bool getNoDelay() const;
	RudpResult setSackEnabled(uint8_t enable);
	bool getSackEnabled() const;
	RudpResult setKeepAliveInterval(uint32_t ms);
	uint32_t getKeepAliveInterval() const;
	RudpResult setKeepAliveTimeout(uint32_t ms);
	uint32_t getKeepAliveTimeout() const;
	void setDebugFlag(uint8_t enable);
	bool getDebugFlag() const;

	int resetState(uint32_t flagMask);
	__attribute__((noinline)) void resetConnectionIds(uint32_t flagMask);
	RudpResult activate();
	RudpResult initiate();
	int getSizeWritable() const;
	RudpResult enqueueWrite(RudpBuffer *src, uint8_t flags);
	RudpResult write(RudpBuffer *src, int flags);
	int getSizeReadable() const;
	void updateReadableSize();
	RudpResult dequeueRead(RudpBuffer *dst, SceRudpReadInfo *info, bool *outLatencyCritical);
	RudpResult read(RudpBuffer *dst, SceRudpReadInfo *info);
	void recordFailedConnection(int reason);
	void transmitPacket(RudpBuffer *buf, RudpHeader *header, uint8_t immediate, uint32_t *txTimestampOut);
	void sendReset(uint32_t connId, uint8_t reason);
	__attribute__((noinline)) int mapReasonToWire(int reason);
	void enterCloseOrReset(int reason, int sendRstMode);
	RudpResult terminate();
	RudpResult flush();
	__attribute__((noinline)) void sendPing();
	void checkKeepAlive();
	void fillSackBlocks(RudpHeader *header);
	void sendAck();
	void sendData();
	void sendSyn();
	void handleRetransmitTimeout();
	void onTimerTick(uint64_t *nextIntervalUsec);
	void sendPingAck();
	void handlePingPacket(RudpHeader *header);
	__attribute__((noinline)) void handleCookieChallenge(RudpHeader *header);
	__attribute__((noinline)) void handleResetPacket(RudpHeader *header);
	void completeHandshakeFromSynRcvd(RudpHeader *header);
	void completeHandshakeFromSynSent(RudpHeader *header);
	void handleSimultaneousSyn(RudpHeader *header);
	void handleIncomingSyn(RudpHeader *header);
	void handleSynPacket(RudpHeader *header);
	__attribute__((noinline)) void rebuildContiguousTail();
	void advanceContiguousTail(uint8_t hasRetryInfo);
	void insertRecvPacket(RudpBuffer *payload, RudpHeader *header);
	void processIncomingData(RudpHeader *header, RudpBuffer *payload);
	void handleDataPacket(RudpHeader *header, RudpBuffer *payload);
	void markPacketForRetransmit(RudpPacket *pkt);
	void updateRtt(uint32_t sampleMs);
	RudpResult processAck(RudpHeader *header);
	RudpResult onPacketReceived(RudpBuffer *buf);
	void getStats(RudpProtocolStats *outStats) const;
	uint32_t getPacketsToRead() const;

	IRudpProtocolDelegate *m_delegate;
	uint32_t m_localConnId;
	uint32_t m_remoteConnId;
	uint32_t m_mss;
	uint32_t m_sendBufSize;
	uint32_t m_recvBufSize;
	RudpPacketPool m_packetPool;
	uint16_t m_sendUnackedSeq;
	uint16_t m_sendNextSeq;
	uint16_t m_peerMaxWindow;
	uint16_t m_peerWindow;
	uint32_t m_cwnd;
	uint32_t m_ssthresh;
	int m_fastRecoveryEndSeq;
	uint16_t m_fastRecoveryHighSeq;
	uint32_t m_packetsToRead;
	uint16_t m_recvExpectedSeq;
	uint16_t m_recvHighestSeq;
	uint16_t m_recvReadSeq;
	uint16_t m_recvLastSeq;
	RudpPacket *m_sendHead;
	RudpPacket *m_sendTail;
	uint32_t m_sendTotalBytes;
	uint32_t m_sendQueuedBytes;
	RudpPacket *m_recvHead;
	RudpPacket *m_recvTail;
	RudpPacket *m_recvContiguousTail;
	uint32_t m_recvTotalBytes;
	uint32_t m_recvContiguousBytes;
	uint16_t m_recvUrgentCount;
	uint32_t m_readablePacketSize;
	RudpPacket *m_nextUrgentPacket;
	uint16_t m_dupAckCount;
	uint16_t m_retransmitCount;
	uint16_t m_unackedRecvCount;
	uint32_t m_connTimeoutMs;
	uint32_t m_closeWaitTimeoutMs;
	uint32_t m_aggrTimeoutMs;
	uint64_t m_rttVarUsec;
	uint64_t m_srttUsec;
	uint64_t m_rtoUsec;
	uint64_t m_rtoDeadlineUsec;
	uint64_t m_stateDeadlineUsec;
	uint64_t m_clockGranularityUsec;
	uint64_t m_lastRecvSeqTimeUsec;
	uint64_t m_delayedAckStartUsec;
	uint64_t m_zeroWinProbeStartUsec;
	uint32_t m_zeroWinProbeDelayMs;
	uint32_t m_keepAliveIntervalMs;
	uint32_t m_keepAliveTimeoutMs;
	uint64_t m_keepAliveStartUsec;
	uint64_t m_keepAliveLastSentUsec;
	uint64_t m_keepAliveLastAckUsec;
	uint32_t m_keepAliveRetryCount;
	uint64_t m_keepAliveNextDelayUsec;
	uint64_t m_lastRxTimeUsec;
	uint64_t m_lastTxTimeUsec;
	uint8_t *m_cookieData;
	uint32_t m_cookieLen;
	uint32_t m_state;
	uint32_t m_flags;
	RudpProtocolStats m_stats;
};

/* Context delegate, Context, and ContextManager (rudp_context.cpp) */
class IRudpContextDelegate {
public:
	virtual ~IRudpContextDelegate() {}
	virtual RudpResult onContextRegisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId, uint32_t aggrTimeoutMs) = 0;
	virtual void onContextUnregisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId) = 0;
	virtual void onContextSendPacket(RudpContext *ctx, RudpHeader *header, RudpBuffer *payload, uint32_t aggrDelayMs, uint32_t *txTimestampOut) = 0;
	virtual void onContextSendDirect(RudpContext *ctx, uint8_t muxMode, RudpHeader *header, int sock, const RudpEndpoint *srcEp, const RudpEndpoint *dstEp) = 0;
	virtual void onContextCancelAggregator(RudpContext *ctx) = 0;
	virtual void onContextNotifyPoll(RudpContext *ctx, int ctxId, int pollId, uint16_t events) = 0;
};

class RudpContext : public IRudpProtocolDelegate, public IRudpEventHandler {
public:
	RUDP_MEM_DELETE
	RudpContext(RudpScheduler *scheduler, int ctxId);
	virtual ~RudpContext();

	void detachChild(const RudpEndpoint *ep);
	void setEventHandler(SceRudpContextEventHandler handler, void *arg);
	void noopHelper();
	int getLastError();

	RudpResult setMaxSegmentSize(uint32_t mss);
	uint32_t getMaxSegmentSize() const;
	RudpResult setSendBufferSize(uint32_t size);
	uint32_t getSendBufferSize() const;
	RudpResult setRecvBufferSize(uint32_t size);
	uint32_t getRecvBufferSize() const;
	RudpResult setConnectionTimeout(uint32_t ms);
	uint32_t getConnectionTimeout() const;
	RudpResult setCloseWaitTimeout(uint32_t ms);
	uint32_t getCloseWaitTimeout() const;
	RudpResult setAggregationTimeout(uint32_t ms);
	uint32_t getAggregationTimeout() const;
	RudpResult setNonBlock(bool nonBlock);
	bool getNonBlock() const;
	RudpResult setStreamMode(int enable);
	bool getStreamMode() const;
	RudpResult setDeliveryCritical(uint8_t enable);
	bool getDeliveryCritical() const;
	RudpResult setOrderCritical(uint8_t enable);
	bool getOrderCritical() const;
	RudpResult setNoDelay(uint8_t enable);
	bool getNoDelay() const;
	RudpResult setSackEnabled(uint8_t enable);
	bool getSackEnabled() const;
	RudpResult setCookieTimeout(uint32_t seconds);
	uint32_t getCookieTimeout() const;
	RudpResult setKeepAliveInterval(uint32_t ms);
	uint32_t getKeepAliveInterval() const;
	RudpResult setKeepAliveTimeout(uint32_t ms);
	uint32_t getKeepAliveTimeout() const;
	void setDebugFlag(uint8_t enable);

	RudpResult bind(int sock, uint16_t vport, uint8_t muxMode);
	int waitBlocking(uint8_t *ioMask, uint32_t timeoutMs);
	RudpResult activate();
	RudpResult initiate();
	RudpResult write(RudpBuffer *buf, uint8_t flags);
	RudpResult read(RudpBuffer *buf, uint8_t flags, SceRudpReadInfo *info);
	RudpResult terminate();
	RudpResult flush();
	int getSizeWritable() const;
	int getSizeReadable() const;
	uint32_t getPacketsToRead() const;
	RudpResult getLocalInfo(int *sock, void *addr, uint32_t *addrLen, uint16_t *vport, uint8_t *muxMode);
	RudpResult getRemoteInfo(void *addr, int *addrLen, uint16_t *vport);

	RudpResult setReadTimeout(uint32_t ms);
	RudpResult getReadTimeout(uint32_t *outMs);
	RudpResult setWriteTimeout(uint32_t ms);
	RudpResult getWriteTimeout(uint32_t *outMs);
	RudpResult setFlushTimeout(uint32_t ms);
	RudpResult getFlushTimeout(uint32_t *outMs);

	RudpResult setRemoteEndpointInternal(const RudpSockaddr *addr, uint16_t vport);
	RudpResult setRemoteEndpoint(const RudpSockaddr *addr, uint16_t vport);
	uint32_t getState(bool *outTerminating) const;
	int verifyCookie(const uint8_t *cookie);
	RudpResult onPacketReceived(RudpBuffer *buf, uint8_t muxMode, uint16_t dstVport, uint16_t srcVport, const RudpSockaddr *from);
	void postSocketError(int errCode);
	void getProtocolStats(RudpProtocolStats *outStats) const;
	RudpResult attachPoll(int pollId);
	int getPollId() const;
	void detachPoll();
	uint16_t computePollEvents() const;
	uint16_t queryPollEvents() const;
	int getParentId() const;
	uint32_t getChildCount() const;
	void processCookieChallenges();

	/* IRudpProtocolDelegate overrides */
	virtual void onProtocolConnected(RudpProtocol *proto);
	virtual void onProtocolConnectFailed(RudpProtocol *proto, uint8_t reason);
	virtual void onProtocolDisconnected(RudpProtocol *proto, uint8_t reason);
	virtual void onProtocolReadable(RudpProtocol *proto);
	virtual void onProtocolWritable(RudpProtocol *proto);
	virtual void onProtocolFlushed(RudpProtocol *proto);
	virtual void onProtocolScheduleSyn(RudpProtocol *proto);
	virtual void onProtocolScheduleAck(RudpProtocol *proto);
	virtual void onProtocolScheduleRst(RudpProtocol *proto, uint32_t connId, uint8_t reason);
	virtual void onProtocolScheduleData(RudpProtocol *proto);
	virtual void onProtocolSchedulePing(RudpProtocol *proto);
	virtual void onProtocolSetTimer(RudpProtocol *proto, uint64_t intervalUsec);
	virtual void onProtocolTransmit(const RudpTransmitRequest *req);
	virtual void onProtocolClosed(RudpProtocol *proto);
	virtual void onProtocolCancelAggregator(RudpProtocol *proto);
	virtual void onProtocolGenerateRandom(RudpProtocol *proto, uint8_t *outDigest32);

	/* IRudpEventHandler overrides */
	__attribute__((noinline)) int mapReasonToErrorCode(int reason);
	virtual void onEvent(const RudpEvent *ev);
	virtual void onTimer(uint64_t *nextIntervalUsec);

	RudpScheduler *m_scheduler;
	IRudpContextDelegate *m_delegate;
	int m_ctxId;
	uint32_t m_handlerId;
	SceRudpContextEventHandler m_eventHandler;
	void *m_eventHandlerArg;
	RudpProtocol m_protocol;
	SceKernelLwCondWork m_waitCond;
	bool m_blocking;
	int m_waiterCount;
	int m_sock;
	RudpEndpoint m_localEndpoint;
	RudpEndpoint m_remoteEndpoint;
	RudpContext *m_parent;
	bool m_isChild;
	RudpRbTree m_childrenByEndpoint;
	RudpRbTree m_pendingChildren;
	uint16_t m_backlog;
	uint32_t m_cookieTimeoutSec;
	RudpRingBuffer<RudpEndpoint> *m_cookieChallengeQueue;
	uint32_t m_cookieSecret;
	uint8_t m_muxMode;
	int m_pollId;
	int m_lastError;
	uint32_t m_readTimeoutMs;
	uint32_t m_writeTimeoutMs;
	uint32_t m_flushTimeoutMs;
	uint8_t m_contextFlags;
	uint8_t m_waitFlags;
	uint32_t m_magic;
};

class IRudpContextManagerDelegate {
public:
	virtual ~IRudpContextManagerDelegate() {}
	virtual void onContextDestroyed(RudpContext *ctx) = 0;
};

class RudpContextManager {
public:
	RudpContextManager(RudpScheduler *scheduler);
	~RudpContextManager();

	void clear();
	void setDelegate(IRudpContextManagerDelegate *delegate);
	RudpContext *findContext(int ctxId, uint8_t includeTerminating, RudpResult *outRes);
	RudpResult destroyContext(int ctxId);
	RudpResult createContext(RudpContext **outCtx, IRudpContextDelegate *delegate);
	RudpResult terminateContext(int ctxId);
	uint32_t getContextCount() const;
	void cancelAllWaits();

	int m_nextCtxId;
	RudpRbTree m_contexts;
	RudpScheduler *m_scheduler;
	IRudpContextManagerDelegate *m_delegate;
};

/* Core orchestrator (rudp_core.cpp, 504 bytes) */
class RudpCore :
	public IRudpContextDelegate,
	public IRudpContextManagerDelegate,
	public IRudpAggregatorDelegate,
	public IRudpNetworkerDelegate,
	public IRudpPollDelegate,
	public IRudpEventHandler,
	public IRudpSchedulerDelegate
{
public:
	RUDP_MEM_DELETE
	RudpCore();
	virtual ~RudpCore();

	void cancelAll();
	bool isValid() const;
	void setEventHandler(SceRudpEventHandler handler, void *arg);
	SceRudpEventHandler getEventHandler() const;
	RudpResult setMaxSegmentSize(uint16_t mss);
	uint16_t getMaxSegmentSize() const;
	RudpResult bind(int ctxId, int sock, uint16_t vport, uint8_t muxMode);
	RudpResult registerRemoteEndpoint(RudpContext *ctx, const RudpSockaddr *addr, uint16_t vport);
	RudpResult activate(int ctxId, const RudpSockaddr *addr);
	RudpResult initiate(int ctxId, const RudpSockaddr *addr, uint16_t vport);
	RudpResult pollCreate(uint32_t size);
	RudpResult pollDestroy(int pollId);
	RudpResult pollControl(int pollId, int op, int ctxId, uint16_t events);
	RudpResult pollWait(int pollId, SceRudpPollEvent *events, uint32_t maxEvents, uint64_t timeoutUsec);
	RudpResult pollCancel(int pollId, uint8_t setPendingFlag);
	RudpResult enableInternalIOThread(uint32_t stackSize, int priority, uint64_t affinityMask);
	RudpResult shutdown();
	RudpResult processEvents(uint64_t timeoutUsec);
	int netReceived(int sock, const uint8_t *data, uint32_t len, const RudpSockaddr *from, uint8_t fromExternal);
	RudpContextManager *getContextManager();
	__attribute__((noinline)) void notifySocketError(int sock, int errCode);
	int sendUdpPacket(int sock, const RudpSockaddr *addr, RudpBuffer *buf);

	/* Interface overrides */
	virtual RudpResult onContextRegisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId, uint32_t aggrTimeoutMs);
	virtual void onContextUnregisterAggregator(RudpContext *ctx, int sock, const RudpSockaddr *addr, int ctxId);
	virtual void onContextSendPacket(RudpContext *ctx, RudpHeader *header, RudpBuffer *payload, uint32_t aggrDelayMs, uint32_t *txTimestampOut);
	virtual void onContextSendDirect(RudpContext *ctx, uint8_t muxMode, RudpHeader *header, int sock, const RudpEndpoint *srcEp, const RudpEndpoint *dstEp);
	virtual void onContextCancelAggregator(RudpContext *ctx);
	virtual void onContextNotifyPoll(RudpContext *ctx, int ctxId, int pollId, uint16_t events);
	virtual void onContextDestroyed(RudpContext *ctx);
	virtual int onAggregatorSend(RudpAggregator *aggr, int sock, const RudpSockaddr *addr, RudpBuffer *buf);
	virtual int onNetworkerReceived(RudpNetworker *net, int sock, const uint8_t *data, uint32_t len, const SceNetSockaddr *addr, uint32_t addrLen);
	virtual void onNetworkerSocketError(RudpNetworker *net, int sock);
	virtual void onPollQueryEvents(int ctxId, uint16_t *outEvents);
	virtual void onPollDetachContext(int ctxId);
	virtual void onEvent(const RudpEvent *ev);
	virtual void onTimer(uint64_t *nextIntervalUsec);
	virtual void onEventPosted(RudpScheduler *scheduler);

	RudpScheduler m_scheduler;
	RudpRbTree m_sockets;
	RudpContextManager m_contextManager;
	uint32_t m_reservedCore;
	SceRudpEventHandler m_eventHandler;
	void *m_eventHandlerArg;
	RudpAggregator m_aggregator;
	RudpNetworker m_networker;
	bool m_ioThreadEnabled;
	RudpPollManager m_pollManager;
	uint32_t m_handlerId;
};

/* Global library status object (248 bytes, matches SceRudpStatus layout) */
class RudpStatus {
public:
	RUDP_MEM_DELETE
	RudpStatus();
	~RudpStatus();
	void noop();

	SceRudpStatus m_data;
};

/* Global library state (rudp_api.cpp) */
extern RudpCore *g_rudpCore;
extern RudpContextManager *g_rudpContextManager;
extern RudpStatus *g_rudpStatus;
extern bool g_rudpEnding;
extern bool g_rudpInitialized;
extern SceKernelLwMutexWork g_rudpMutex;

/* Comparator helpers */
int rudpCompareInt(const void *lhs, const void *rhs);
int rudpCompareU16(const void *lhs, const void *rhs);
int rudpCompareU64(const void *lhs, const void *rhs);
int rudpComparePtr(const void *lhs, const void *rhs);
int rudpCompareEndpoint(const void *lhs, const void *rhs);
int rudpCompareEndpointAlt(const void *lhs, const void *rhs);
int rudpCompareAggregatorKey(const void *lhs, const void *rhs);

#endif /* _RUDP_INTERNAL_H */
