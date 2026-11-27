/*
 * DTCPIP-SW OSAL C++ foundation: strings, network addresses, lists,
 * mutex/condition primitives, TCP sockets, threads, and time utilities.
 */

#ifndef SW_OSAL_CPP_H
#define SW_OSAL_CPP_H

#include <stddef.h>
#include <stdint.h>
namespace std {
using ::size_t;
}
#include <new>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <net.h>

/* ---- C network and TTL helpers (sw_net_vita.c) ---- */
extern "C" {

struct SwFdSet {
	uint32_t bits[32];
};

int swNetSocketCreate(int *outSock, int domain, int type);
int swNetSocketCreateWithEpoll(int *outSock, int domain, int type, int *outEpoll);
int swNetSocketClose(int sock);
int swNetSocketCloseWithEpoll(int sock, int epollId);
int swNetSocketBind(int sock, const SceNetSockaddr *addr, unsigned int addrLen, int reuseAddr);
int swNetSocketListen(int sock, int backlog);
int swNetSocketAccept(int sock, SceNetSockaddr *addr, unsigned int *addrLen, int *outSock);
int swNetSocketSend(int sock, const void *buf, unsigned int len, int *outSent);
int swNetSocketRecv(int sock, void *buf, unsigned int len, int *outRecv);
int swNetSocketConnect(int sock, const SceNetSockaddr *addr, unsigned int addrLen);
int swNetSocketConnectTimeout(int sock, const SceNetSockaddr *addr, unsigned int addrLen,
			      int timeoutMs, int epollId, const int *cancelFlag);
int swNetGetSockName(int sock, SceNetSockaddr *addr, unsigned int *addrLen);
int swNetGetPeerName(int sock, SceNetSockaddr *addr, unsigned int *addrLen);
int swNetSetTcpNoDelay(int sock, int enable);
int swNetSetIpTtl(int sock, int ttl);
int SwMwNetResolveName(const char *hostname, uint16_t port, unsigned char family,
		       SceNetSockaddrIn *outAddr);
int SwMwNetParseAddress(int family, const char *src, void *dst);
int SwMwNetFormatAddress(int family, const void *src, char *dst, unsigned int size);
int swNetWaitReadable(int sock, int timeoutMs, int epollId, const int *cancelFlag);
int swNetWaitWritable(int sock, int timeoutMs, int epollId, const int *cancelFlag);
int swNetSelect(int nfds, struct SwFdSet *readFds, struct SwFdSet *writeFds,
		int timeoutMs, const int *cancelFlag);

int swTtlOpenSink(int *outHandle, int sock);
int swTtlCloseSink(int handle);
int swTtlOpenSource(int *outHandle, int sock);
int swTtlOpenSourceWithCtx(void *unusedCtx, int *outHandle, int sock);
int swTtlGetPeerTtl(int handle, int *outTtl);
int swTtlCloseSource(int handle);

int SwMwThreadTrampoline(SceSize args, void *argument);
int SwsGetToolMode(void);
int SwsGetAffinity(void);

} /* extern "C" */

/* ---- SwStringBuf (28 bytes, 15-byte small-string optimization) ---- */

class SwStringBuf {
public:
	SwStringBuf(const char *src, size_t len);
	SwStringBuf(size_t cap, const char *src, size_t len);
	virtual ~SwStringBuf();

	bool isInline() const { return m_capacity <= 15; }
	const char *data() const { return isInline() ? m_inline : m_heap; }
	char *mutableData() { return isInline() ? m_inline : m_heap; }
	size_t length() const { return m_length; }
	size_t capacity() const { return m_capacity; }
	SwStringBuf *clone() const;
	int assign(const char *src, size_t len);

private:
	int init(size_t cap, const char *src, size_t len);

	size_t m_capacity;
	size_t m_length;
	union {
		char *m_heap;
		char m_inline[16];
	};
};

/* ---- SwString (12 bytes; ABI-compatible with SwMwUriText) ---- */

class SwString {
public:
	SwString();
	SwString(const char *src);
	SwString(const char *src, size_t len);
	SwString(const SwString &other);
	virtual ~SwString();

	SwString &operator=(const SwString &other);
	SwString &operator=(const char *src);

	const char *c_str() const;
	size_t length() const;
	bool empty() const;
	bool hasError() const { return m_error != 0; }

	int assign(const SwString &other);
	int assign(const char *src);
	int assign(const char *src, size_t len);
	int append(const SwString &other);
	int append(const char *src);
	int append(const char *src, size_t len);
	int append(char ch);
	int erase(size_t pos, size_t count);
	int replace(size_t pos, size_t count, const char *src);
	SwString substr(size_t pos, size_t count) const;
	char &at(size_t pos);
	char at(size_t pos) const;
	int find(const char *needle, size_t startPos = 0) const;
	int findChar(char ch, size_t startPos = 0) const;
	int compare(const SwString &other) const;
	int compare(const char *other) const;
	bool equals(const char *other) const { return compare(other) == 0; }
	void clear();

private:
	int initFrom(const char *src, size_t len);

	SwStringBuf *m_buf;
	uint8_t m_error;
	uint8_t m_reserved[3];
};

/* ---- SwNetAddress (20 bytes; ABI-compatible with SwMwUri) ---- */

class SwNetAddress {
public:
	SwNetAddress();
	SwNetAddress(const SwString &host, uint16_t port);
	SwNetAddress(const SwNetAddress &other);
	virtual ~SwNetAddress();

	int set(const SwString &host, uint16_t port);
	int get(SwString &outHost, uint16_t &outPort) const;
	void toString(SwString &out) const;
	bool isValid() const;
	bool isAnyHost() const;
	uint16_t port() const { return m_port; }
	const SwString &host() const { return m_host; }

private:
	SwString m_host;
	uint16_t m_port;
	uint16_t m_reserved;
};

/* ---- SwScopedArray ---- */

template <typename T>
class SwScopedArray {
public:
	explicit SwScopedArray(size_t count)
		: m_ptr(new (std::nothrow) T[count]) {}
	~SwScopedArray()
	{
		delete[] m_ptr;
		m_ptr = NULL;
	}
	T *get() const { return m_ptr; }
	T *release()
	{
		T *p = m_ptr;
		m_ptr = NULL;
		return p;
	}
	void reset(T *p = NULL)
	{
		if (m_ptr != p) {
			delete[] m_ptr;
			m_ptr = p;
		}
	}
	T &operator[](size_t idx) const { return m_ptr[idx]; }

private:
	SwScopedArray(const SwScopedArray &);
	SwScopedArray &operator=(const SwScopedArray &);
	T *m_ptr;
};

/* ---- SwList<T> (doubly-linked circular sentinel list, pointer/uint32 elements) ---- */

struct SwListNode {
	SwListNode *next;
	SwListNode *prev;
	uintptr_t value;
};

class SwListIterator {
public:
	SwListIterator() : m_node(NULL) {}
	explicit SwListIterator(SwListNode *node) : m_node(node) {}
	SwListIterator(const SwListIterator &other) : m_node(other.m_node) {}
	virtual ~SwListIterator() {}

	SwListIterator &operator=(const SwListIterator &other)
	{
		m_node = other.m_node;
		return *this;
	}
	bool operator==(const SwListIterator &other) const { return m_node == other.m_node; }
	bool operator!=(const SwListIterator &other) const { return m_node != other.m_node; }
	SwListIterator &operator++()
	{
		if (m_node)
			m_node = m_node->next;
		return *this;
	}
	SwListIterator operator++(int)
	{
		SwListIterator tmp(*this);
		++(*this);
		return tmp;
	}
	SwListIterator &operator--()
	{
		if (m_node)
			m_node = m_node->prev;
		return *this;
	}
	uintptr_t &operator*() const { return m_node->value; }
	SwListNode *node() const { return m_node; }

private:
	SwListNode *m_node;
};

class SwListBase {
public:
	SwListBase();
	virtual ~SwListBase();

	bool empty() const;
	size_t size() const;
	SwListIterator begin();
	SwListIterator end();
	int pushFront(uintptr_t val);
	int pushBack(uintptr_t val);
	SwListIterator erase(SwListIterator it);
	void clear();

private:
	SwListNode *m_sentinel;
};

template <typename T>
class SwPtrList : public SwListBase {
public:
	int pushFrontPtr(T *p) { return pushFront(reinterpret_cast<uintptr_t>(p)); }
	int pushBackPtr(T *p) { return pushBack(reinterpret_cast<uintptr_t>(p)); }
	static T *deref(const SwListIterator &it) { return reinterpret_cast<T *>(*it); }
};

/* ---- SwMutex (48 bytes) & SwCondition (72 bytes) ---- */

class SwCondition;

class SwMutex {
public:
	SwMutex();
	virtual ~SwMutex();

	bool lock();
	bool unlock();
	int createCondition(SwCondition **outCond);
	SceKernelLwMutexWork *nativeWork() { return &m_work; }

private:
	uint32_t m_alignPad;
	SceKernelLwMutexWork m_work;
	uint8_t m_error;
	uint8_t m_reserved[7];
};

class SwCondition {
public:
	explicit SwCondition(SceKernelLwMutexWork *extMutex);
	virtual ~SwCondition();

	int destroy();
	int signal();
	int wait(int timeoutMs);

private:
	SceKernelLwMutexWork *m_extMutex;
	SceKernelLwCondWork m_cond;
	SceKernelLwMutexWork m_innerMutex;
	uint32_t m_signaled;
	uint8_t m_destroyed;
	uint8_t m_reserved[3];
};

class SwScopedLock {
public:
	explicit SwScopedLock(SwMutex &mtx) : m_mtx(&mtx) { m_mtx->lock(); }
	~SwScopedLock() { m_mtx->unlock(); }

private:
	SwMutex *m_mtx;
};

/* ---- SwSocket hierarchy ---- */

class SwSocket {
public:
	SwSocket();
	SwSocket(int fd, bool ownsFd);
	virtual ~SwSocket();

	virtual int close();
	virtual bool waitReadable(int timeoutMs, const int *cancelFlag);
	virtual bool waitWritable(int timeoutMs, const int *cancelFlag);
	virtual int setSendBufferSize(int size);
	virtual int getSendBufferSize();
	virtual int setRecvBufferSize(int size);
	virtual int getLocalAddressString(SwString &out);
	virtual int getLocalAddress(SwNetAddress &out);
	virtual int getPeerAddressString(SwString &out);
	virtual int getPeerAddress(SwNetAddress &out);
	virtual bool setIpTtl(uint8_t ttl);
	virtual int fd() const;

protected:
	int m_fd;
	int m_epollId;
	uint8_t m_ownsFd;
	uint8_t m_reserved[3];
};

class SwTcpSocket : public SwSocket {
public:
	SwTcpSocket();
	explicit SwTcpSocket(int fd);
	virtual ~SwTcpSocket();

	virtual int setKeepAlive();
	virtual int getKeepAlive();
	virtual int send(const void *buf, unsigned int len);
	virtual int recv(void *buf, unsigned int len);
	virtual bool setNoDelay(uint8_t enable);
};

class SwTcpClientSocket : public SwTcpSocket {
public:
	SwTcpClientSocket() {}
	virtual ~SwTcpClientSocket() {}

	virtual int open();
	virtual int connect(const SwNetAddress &addr, const int *cancelFlag, int timeoutMs);
};

class SwTcpServerSocket : public SwTcpSocket {
public:
	SwTcpServerSocket() {}
	virtual ~SwTcpServerSocket() {}

	virtual int listen(const SwNetAddress &addr, uint8_t flag, int backlog);
	virtual int accept(SwTcpSocket **outClient);
};

/* ---- SwThread (132 bytes) ---- */

struct SwThreadStartArg {
	int (*entry)(void *);
	SceUID thid;
	void *arg;
};

class SwThread {
public:
	enum Type {
		TYPE_ANONYMOUS = 0,
		TYPE_AKE_CMD_TIMER = 1,
		TYPE_KX_EXP = 2,
		TYPE_SINK_AUTH = 3,
		TYPE_SINK_AKE_TIMER = 4,
		TYPE_SINK_CKC = 5,
		TYPE_SRC_AUTH = 6,
		TYPE_SRC_AUTH_SERVER = 7,
		TYPE_SRC_AKE_TIMER = 8,
		TYPE_RTT_REG = 9,
		TYPE_TTL_CHECK = 10,
		TYPE_MEDIA_DISCONNECT = 11,
		TYPE_SW_TIMER = 16,
		TYPE_SW_DEP = 17,
		TYPE_TOOL = 18,
		TYPE_ANONYMOUS_AFFINITY = 19
	};

	explicit SwThread(int threadType);
	virtual ~SwThread();

	virtual int getResult();
	virtual bool isStopped();
	virtual int getState();
	virtual int join();
	virtual void run() = 0;

	int start();

protected:
	static int threadMain(void *self);
	static int spawnNative(SwThreadStartArg *outArg, int (*entry)(void *),
			       void *arg, int threadType);
	static int joinNative(SwThreadStartArg *arg);

	int m_threadType;
	int m_state;
	SwThreadStartArg m_native;
	SwMutex m_stateMutex;
	uint32_t m_reserved;
	int m_result;
	SwMutex m_condMutex;
	SwCondition *m_cond;
};

/* ---- OSAL utility functions ---- */

struct SwLocalTime {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
	uint32_t microsecond;
};

int swStringToUpper(SwString &str);
int swStringToLower(SwString &str);
void swStringTrim(SwString &str);
uint32_t swSwap32(uint32_t val);
uint16_t swSwap16(uint16_t val);
int swSleepMs(int ms);
int swYield(void);
void swSleepCancelable(int stepMs, unsigned int totalMs, const volatile uint8_t *runFlag);
int swGetLocalTime(SwLocalTime *out);
void swFormatTimestamp(SwString &out);
uint32_t swGetTickMs(void);

#endif /* SW_OSAL_CPP_H */
