/*
 * DTCPIP-SW OSAL C++ implementation: SwString, SwNetAddress, SwListBase,
 * SwMutex, SwCondition, SwSocket hierarchy, SwThread, and utilities.
 */

#include "sw_osal_cpp.h"

#include <rtc.h>
#include <stdio.h>
#include <string.h>

/* ---- SwStringBuf ---- */

SwStringBuf::SwStringBuf(const char *src, size_t len)
	: m_capacity(0), m_length(0)
{
	m_heap = NULL;
	init(len, src, len);
}

SwStringBuf::SwStringBuf(size_t cap, const char *src, size_t len)
	: m_capacity(0), m_length(0)
{
	m_heap = NULL;
	init(cap, src, len);
}

int SwStringBuf::init(size_t cap, const char *src, size_t len)
{
	m_length = len;
	if (cap > 15) {
		m_capacity = cap;
		m_heap = new (std::nothrow) char[m_capacity + 1];
		if (!m_heap)
			return 0;
		if (src && m_length > 0)
			memcpy(m_heap, src, m_length);
		m_heap[m_length] = '\0';
		return 0;
	}
	if (src && m_length > 0)
		memcpy(m_inline, src, m_length);
	m_inline[m_length] = '\0';
	m_capacity = 15;
	return 15;
}

SwStringBuf::~SwStringBuf()
{
	if (!isInline())
		delete[] m_heap;
}

SwStringBuf *SwStringBuf::clone() const
{
	return new (std::nothrow) SwStringBuf(m_capacity, data(), m_length);
}

int SwStringBuf::assign(const char *src, size_t len)
{
	m_length = len;
	if (isInline() && m_length <= 15) {
		if (src && m_length > 0)
			memcpy(m_inline, src, m_length);
		m_inline[m_length] = '\0';
		return 0;
	}
	if (m_length > m_capacity) {
		if (!isInline())
			delete[] m_heap;
		m_capacity = m_length;
		m_heap = new (std::nothrow) char[m_capacity + 1];
	}
	if (!m_heap)
		return 0;
	if (src && m_length > 0)
		memcpy(m_heap, src, m_length);
	m_heap[m_length] = '\0';
	return 0;
}

/* ---- SwString ---- */

SwString::SwString()
	: m_buf(NULL), m_error(0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	initFrom("", 0);
}

SwString::SwString(const char *src)
	: m_buf(NULL), m_error(0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	size_t len = src ? strlen(src) : 0;
	initFrom(src, len);
}

SwString::SwString(const char *src, size_t len)
	: m_buf(NULL), m_error(0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	initFrom(src, len);
}

SwString::SwString(const SwString &other)
	: m_buf(NULL), m_error(0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
	if (other.m_buf)
		m_buf = other.m_buf->clone();
	else
		initFrom("", 0);
}

SwString::~SwString()
{
	delete m_buf;
	m_buf = NULL;
}

int SwString::initFrom(const char *src, size_t len)
{
	if (!src) {
		m_buf = NULL;
		return 0;
	}
	m_buf = new (std::nothrow) SwStringBuf(src, len);
	if (!m_buf || (!m_buf->isInline() && m_buf->data() == NULL)) {
		delete m_buf;
		m_buf = NULL;
		m_error = 1;
		return 0;
	}
	return 1;
}

const char *SwString::c_str() const
{
	if (!m_buf)
		return NULL;
	return m_buf->data();
}

size_t SwString::length() const
{
	if (!m_buf)
		return 0;
	return m_buf->length();
}

bool SwString::empty() const
{
	if (!m_buf)
		return true;
	return m_buf->length() == 0;
}

SwString &SwString::operator=(const SwString &other)
{
	if (this != &other)
		assign(other);
	return *this;
}

SwString &SwString::operator=(const char *src)
{
	assign(src);
	return *this;
}

int SwString::assign(const SwString &other)
{
	if (this == &other)
		return 0;
	return assign(other.c_str(), other.length());
}

int SwString::assign(const char *src)
{
	if (!src)
		return assign("", 0);
	return assign(src, strlen(src));
}

int SwString::assign(const char *src, size_t len)
{
	if (!src) {
		delete m_buf;
		m_buf = NULL;
		return 0;
	}
	if (!m_buf)
		return initFrom(src, len) ? 0 : 1;
	m_buf->assign(src, len);
	return 0;
}

int SwString::append(const SwString &other)
{
	if (other.empty())
		return 0;
	return append(other.c_str(), other.length());
}

int SwString::append(const char *src)
{
	if (!src)
		return 0;
	return append(src, strlen(src));
}

int SwString::append(const char *src, size_t len)
{
	if (!src || len == 0)
		return 0;
	size_t oldLen = length();
	size_t newLen = oldLen + len;
	SwScopedArray<char> tmp(newLen + 1);
	if (!tmp.get()) {
		m_error = 1;
		return 1;
	}
	if (oldLen > 0)
		memcpy(tmp.get(), c_str(), oldLen);
	memcpy(tmp.get() + oldLen, src, len);
	tmp.get()[newLen] = '\0';
	return assign(tmp.get(), newLen);
}

int SwString::append(char ch)
{
	return append(&ch, 1);
}

int SwString::erase(size_t pos, size_t count)
{
	size_t curLen = length();
	if (pos >= curLen || count == 0)
		return 0;
	if (count > curLen - pos)
		count = curLen - pos;
	size_t newLen = curLen - count;
	SwScopedArray<char> tmp(newLen + 1);
	if (!tmp.get()) {
		m_error = 1;
		return 1;
	}
	if (pos > 0)
		memcpy(tmp.get(), c_str(), pos);
	if (pos + count < curLen)
		memcpy(tmp.get() + pos, c_str() + pos + count, curLen - (pos + count));
	tmp.get()[newLen] = '\0';
	return assign(tmp.get(), newLen);
}

int SwString::replace(size_t pos, size_t count, const char *src)
{
	size_t curLen = length();
	size_t repLen = src ? strlen(src) : 0;
	if (pos > curLen)
		return 1;
	if (count > curLen - pos)
		count = curLen - pos;
	size_t newLen = curLen - count + repLen;
	SwScopedArray<char> tmp(newLen + 1);
	if (!tmp.get()) {
		m_error = 1;
		return 1;
	}
	if (pos > 0)
		memcpy(tmp.get(), c_str(), pos);
	if (repLen > 0)
		memcpy(tmp.get() + pos, src, repLen);
	if (pos + count < curLen)
		memcpy(tmp.get() + pos + repLen, c_str() + pos + count,
		       curLen - (pos + count));
	tmp.get()[newLen] = '\0';
	return assign(tmp.get(), newLen);
}

SwString SwString::substr(size_t pos, size_t count) const
{
	size_t curLen = length();
	if (pos >= curLen)
		return SwString("");
	if (count > curLen - pos)
		count = curLen - pos;
	return SwString(c_str() + pos, count);
}

char &SwString::at(size_t pos)
{
	static char s_dummy = '\0';
	if (!m_buf || pos >= m_buf->length())
		return s_dummy;
	return m_buf->mutableData()[pos];
}

char SwString::at(size_t pos) const
{
	if (!m_buf || pos >= m_buf->length())
		return '\0';
	return m_buf->data()[pos];
}

int SwString::find(const char *needle, size_t startPos) const
{
	if (!needle || !m_buf)
		return -1;
	size_t len = m_buf->length();
	if (startPos > len)
		return -1;
	const char *found = strstr(m_buf->data() + startPos, needle);
	if (!found)
		return -1;
	return (int)(found - m_buf->data());
}

int SwString::findChar(char ch, size_t startPos) const
{
	if (!m_buf)
		return -1;
	size_t len = m_buf->length();
	if (startPos > len)
		return -1;
	const char *found = strchr(m_buf->data() + startPos, (unsigned char)ch);
	if (!found)
		return -1;
	return (int)(found - m_buf->data());
}

int SwString::compare(const SwString &other) const
{
	const char *lhs = c_str();
	const char *rhs = other.c_str();
	if (!lhs)
		lhs = "";
	if (!rhs)
		rhs = "";
	return strcmp(lhs, rhs);
}

int SwString::compare(const char *other) const
{
	const char *lhs = c_str();
	if (!lhs)
		lhs = "";
	if (!other)
		other = "";
	return strcmp(lhs, other);
}

void SwString::clear()
{
	assign("", 0);
}

/* ---- SwNetAddress ---- */

SwNetAddress::SwNetAddress()
	: m_host(), m_port(0), m_reserved(0)
{
}

SwNetAddress::SwNetAddress(const SwString &host, uint16_t port)
	: m_host(host), m_port(port), m_reserved(0)
{
}

SwNetAddress::SwNetAddress(const SwNetAddress &other)
	: m_host(other.m_host), m_port(other.m_port), m_reserved(0)
{
}

SwNetAddress::~SwNetAddress()
{
}

int SwNetAddress::set(const SwString &host, uint16_t port)
{
	if (host.empty() || port == 0)
		return 5;
	m_host.assign(host);
	m_port = port;
	return 0;
}

int SwNetAddress::get(SwString &outHost, uint16_t &outPort) const
{
	outHost.assign(m_host);
	outPort = m_port;
	return 0;
}

void SwNetAddress::toString(SwString &out) const
{
	char portBuf[8];
	out.assign(m_host);
	out.append(":");
	memset(portBuf, 0, sizeof(portBuf));
	snprintf(portBuf, 6, "%hu", m_port);
	out.append(portBuf);
}

bool SwNetAddress::isValid() const
{
	if (m_port == 0 || m_host.empty())
		return false;
	return true;
}

bool SwNetAddress::isAnyHost() const
{
	return m_host.compare("0.0.0.0") == 0;
}

/* ---- C bridge exports used by DTCPIP-MW ---- */

extern "C" int SwMwUriInit(void *uri, const char *text)
{
	new (uri) SwString(text);
	return 0;
}

extern "C" int SwMwUriDone(void *uri)
{
	reinterpret_cast<SwString *>(uri)->~SwString();
	return 0;
}

extern "C" const char *SwMwUriTextGet(const void *text)
{
	return reinterpret_cast<const SwString *>(text)->c_str();
}

extern "C" int SwMwUriSet(void *uri, const void *text, unsigned short port)
{
	return reinterpret_cast<SwNetAddress *>(uri)->set(
		*reinterpret_cast<const SwString *>(text), port);
}

extern "C" int SwMwUriFormatText(void *outText, const void *uri)
{
	SwString *dst = new (outText) SwString();
	reinterpret_cast<const SwNetAddress *>(uri)->toString(*dst);
	return 0;
}

extern "C" int SwMwUriIsSet(const void *uri)
{
	return reinterpret_cast<const SwNetAddress *>(uri)->isValid() ? 1 : 0;
}

/* ---- SwListBase ---- */

SwListBase::SwListBase()
	: m_sentinel(NULL)
{
	m_sentinel = new (std::nothrow) SwListNode;
	if (m_sentinel) {
		m_sentinel->next = m_sentinel;
		m_sentinel->prev = m_sentinel;
		m_sentinel->value = 0;
	}
}

SwListBase::~SwListBase()
{
	clear();
	delete m_sentinel;
	m_sentinel = NULL;
}

bool SwListBase::empty() const
{
	if (!m_sentinel)
		return true;
	return m_sentinel->next == m_sentinel;
}

size_t SwListBase::size() const
{
	if (!m_sentinel)
		return 0;
	size_t n = 0;
	for (SwListNode *p = m_sentinel->next; p != m_sentinel; p = p->next)
		++n;
	return n;
}

SwListIterator SwListBase::begin()
{
	return SwListIterator(m_sentinel ? m_sentinel->next : NULL);
}

SwListIterator SwListBase::end()
{
	return SwListIterator(m_sentinel);
}

int SwListBase::pushFront(uintptr_t val)
{
	if (!m_sentinel)
		return 1;
	SwListNode *node = new (std::nothrow) SwListNode;
	if (!node)
		return 2;
	node->value = val;
	node->next = m_sentinel->next;
	node->prev = m_sentinel;
	m_sentinel->next->prev = node;
	m_sentinel->next = node;
	return 0;
}

int SwListBase::pushBack(uintptr_t val)
{
	if (!m_sentinel)
		return 1;
	SwListNode *node = new (std::nothrow) SwListNode;
	if (!node)
		return 2;
	node->value = val;
	node->next = m_sentinel;
	node->prev = m_sentinel->prev;
	m_sentinel->prev->next = node;
	m_sentinel->prev = node;
	return 0;
}

SwListIterator SwListBase::erase(SwListIterator it)
{
	SwListNode *node = it.node();
	if (!node || node == m_sentinel)
		return end();
	SwListNode *next = node->next;
	node->prev->next = next;
	next->prev = node->prev;
	delete node;
	return SwListIterator(next);
}

void SwListBase::clear()
{
	if (!m_sentinel)
		return;
	SwListNode *cur = m_sentinel->next;
	while (cur != m_sentinel) {
		SwListNode *next = cur->next;
		delete cur;
		cur = next;
	}
	m_sentinel->next = m_sentinel;
	m_sentinel->prev = m_sentinel;
}

/* ---- SwMutex & SwCondition ---- */

SwMutex::SwMutex()
	: m_alignPad(0), m_error(0)
{
	memset(&m_work, 0, sizeof(m_work));
	memset(m_reserved, 0, sizeof(m_reserved));
	if (sceKernelCreateLwMutex(&m_work, "sce_sws_mutex", 0x2000u, 0, NULL) < 0)
		m_error = 1;
}

SwMutex::~SwMutex()
{
	sceKernelDeleteLwMutex(&m_work);
}

bool SwMutex::lock()
{
	if (m_error)
		return false;
	if (sceKernelLockLwMutex(&m_work, 1, NULL) != 0) {
		m_error = 1;
		return false;
	}
	return true;
}

bool SwMutex::unlock()
{
	if (m_error)
		return false;
	if (sceKernelUnlockLwMutex(&m_work, 1) != 0) {
		m_error = 1;
		return false;
	}
	return true;
}

int SwMutex::createCondition(SwCondition **outCond)
{
	if (!outCond)
		return 5;
	if (m_error)
		return 1;
	SwCondition *cond = new (std::nothrow) SwCondition(&m_work);
	*outCond = cond;
	return cond ? 0 : 2;
}

static uint32_t swElapsedTickMs(uint32_t now, uint32_t start)
{
	if (start > now)
		return now - start - 1;
	return now - start;
}

SwCondition::SwCondition(SceKernelLwMutexWork *extMutex)
	: m_extMutex(extMutex), m_signaled(0), m_destroyed(0)
{
	memset(&m_cond, 0, sizeof(m_cond));
	memset(&m_innerMutex, 0, sizeof(m_innerMutex));
	memset(m_reserved, 0, sizeof(m_reserved));
	if (sceKernelCreateLwMutex(&m_innerMutex, "sce_sws_event_mutex", 0x2000u, 0, NULL) >= 0) {
		if (sceKernelCreateLwCond(&m_cond, "sce_sws_event_cond", 0x2000u,
					  &m_innerMutex, NULL) < 0) {
			sceKernelDeleteLwMutex(&m_innerMutex);
		}
	}
}

SwCondition::~SwCondition()
{
	if (!m_destroyed) {
		while (destroy() == 8) {
			sceKernelLockLwMutex(&m_innerMutex, 1, NULL);
			m_signaled = 1;
			sceKernelSignalLwCond(&m_cond);
			sceKernelUnlockLwMutex(&m_innerMutex, 1);
			swYield();
		}
	}
}

int SwCondition::destroy()
{
	if (m_destroyed == 1)
		return 1;
	sceKernelDeleteLwCond(&m_cond);
	sceKernelDeleteLwMutex(&m_innerMutex);
	m_destroyed = 1;
	return 0;
}

int SwCondition::signal()
{
	if (m_destroyed == 1)
		return 8;
	sceKernelLockLwMutex(&m_innerMutex, 1, NULL);
	m_signaled = 1;
	sceKernelSignalLwCond(&m_cond);
	sceKernelUnlockLwMutex(&m_innerMutex, 1);
	return 0;
}

int SwCondition::wait(int timeoutMs)
{
	if (m_destroyed == 1)
		return 8;

	sceKernelUnlockLwMutex(m_extMutex, 1);

	int ret = 0;
	if (timeoutMs < 0) {
		if (sceKernelLockLwMutex(&m_innerMutex, 1, NULL) < 0) {
			ret = 22;
		} else {
			while (!m_signaled) {
				if (sceKernelWaitLwCond(&m_cond, NULL) < 0) {
					sceKernelUnlockLwMutex(&m_innerMutex, 1);
					ret = 22;
					break;
				}
			}
			if (ret == 0) {
				m_signaled = 0;
				ret = (sceKernelUnlockLwMutex(&m_innerMutex, 1) != 0) ? 22 : 0;
			}
		}
	} else if (timeoutMs == 0) {
		sceKernelLockLwMutex(&m_innerMutex, 1, NULL);
		if (m_signaled) {
			m_signaled = 0;
			ret = 0;
		} else {
			ret = 116;
		}
		sceKernelUnlockLwMutex(&m_innerMutex, 1);
	} else {
		int remain = timeoutMs;
		sceKernelLockLwMutex(&m_innerMutex, 1, NULL);
		if (m_signaled) {
			m_signaled = 0;
			ret = 0;
		} else {
			for (;;) {
				uint32_t tStart = swGetTickMs();
				SceUInt32 us = (SceUInt32)(1000 * remain);
				int wret = sceKernelWaitLwCond(&m_cond, &us);
				uint32_t tEnd = swGetTickMs();
				if (wret == (int)0x80028005) {
					ret = 116;
					break;
				}
				if (wret != 0) {
					ret = wret;
					break;
				}
				if (m_signaled) {
					m_signaled = 0;
					ret = 0;
					break;
				}
				remain -= (int)swElapsedTickMs(tEnd, tStart);
				if (remain <= 0) {
					ret = 0;
					break;
				}
			}
		}
		sceKernelUnlockLwMutex(&m_innerMutex, 1);
	}

	sceKernelLockLwMutex(m_extMutex, 1, NULL);
	if (ret == 0)
		return 0;
	if (ret == 116)
		return 9;
	return 8;
}

/* ---- SwSocket hierarchy ---- */

SwSocket::SwSocket()
	: m_fd(-1), m_epollId(-1), m_ownsFd(1)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
}

SwSocket::SwSocket(int fd, bool ownsFd)
	: m_fd(fd), m_epollId(-1), m_ownsFd(ownsFd ? 1 : 0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
}

SwSocket::~SwSocket()
{
	if (m_fd != -1)
		SwSocket::close();
}

int SwSocket::close()
{
	if (m_ownsFd && m_fd != -1) {
		if (swNetSocketCloseWithEpoll(m_fd, m_epollId) != 0)
			return 211;
	}
	m_fd = -1;
	return 0;
}

bool SwSocket::waitReadable(int timeoutMs, const int *cancelFlag)
{
	if (m_fd == -1)
		return false;
	if (m_epollId >= 0)
		return swNetWaitReadable(m_fd, timeoutMs, m_epollId, cancelFlag) != 0;

	struct SwFdSet rd;
	memset(&rd, 0, sizeof(rd));
	if ((unsigned int)m_fd < 1024u)
		rd.bits[(unsigned int)m_fd >> 5] |= 1u << ((unsigned int)m_fd & 31u);
	if (swNetSelect(m_fd + 1, &rd, NULL, timeoutMs, cancelFlag) == 0 &&
	    (unsigned int)m_fd < 1024u &&
	    (rd.bits[(unsigned int)m_fd >> 5] & (1u << ((unsigned int)m_fd & 31u))) != 0)
		return true;
	return false;
}

bool SwSocket::waitWritable(int timeoutMs, const int *cancelFlag)
{
	if (m_fd == -1)
		return false;
	if (m_epollId >= 0)
		return swNetWaitWritable(m_fd, timeoutMs, m_epollId, cancelFlag) != 0;

	struct SwFdSet wr;
	memset(&wr, 0, sizeof(wr));
	if ((unsigned int)m_fd < 1024u)
		wr.bits[(unsigned int)m_fd >> 5] |= 1u << ((unsigned int)m_fd & 31u);
	if (swNetSelect(m_fd + 1, NULL, &wr, timeoutMs, cancelFlag) == 0 &&
	    (unsigned int)m_fd < 1024u &&
	    (wr.bits[(unsigned int)m_fd >> 5] & (1u << ((unsigned int)m_fd & 31u))) != 0)
		return true;
	return false;
}

int SwSocket::setSendBufferSize(int) { return 0; }
int SwSocket::getSendBufferSize() { return 0; }
int SwSocket::setRecvBufferSize(int) { return 0; }

int SwSocket::getLocalAddressString(SwString &out)
{
	SwNetAddress addr;
	int ret = getLocalAddress(addr);
	if (ret == 0) {
		SwString formatted;
		addr.toString(formatted);
		out.assign(formatted);
	}
	return ret;
}

int SwSocket::getLocalAddress(SwNetAddress &out)
{
	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} sa;
	unsigned int len = sizeof(sa.in);
	char ipStr[16];
	if (swNetGetSockName(m_fd, &sa.generic, &len) != 0)
		return 209;
	SwMwNetFormatAddress(SCE_NET_AF_INET, &sa.in.sin_addr, ipStr, sizeof(ipStr));
	SwString host(ipStr);
	return out.set(host, swSwap16(sa.in.sin_port));
}

int SwSocket::getPeerAddressString(SwString &out)
{
	SwNetAddress addr;
	int ret = getPeerAddress(addr);
	if (ret == 0) {
		SwString formatted;
		addr.toString(formatted);
		out.assign(formatted);
	}
	return ret;
}

int SwSocket::getPeerAddress(SwNetAddress &out)
{
	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} sa;
	unsigned int len = sizeof(sa.in);
	char ipStr[16];
	if (swNetGetPeerName(m_fd, &sa.generic, &len) != 0)
		return 209;
	SwMwNetFormatAddress(SCE_NET_AF_INET, &sa.in.sin_addr, ipStr, sizeof(ipStr));
	SwString host(ipStr);
	return out.set(host, swSwap16(sa.in.sin_port));
}

bool SwSocket::setIpTtl(uint8_t ttl)
{
	return (m_fd != -1) && (swNetSetIpTtl(m_fd, ttl) == 0);
}

int SwSocket::fd() const
{
	return m_fd;
}

SwTcpSocket::SwTcpSocket() : SwSocket() {}
SwTcpSocket::SwTcpSocket(int fd) : SwSocket(fd, true) {}
SwTcpSocket::~SwTcpSocket() {}

int SwTcpSocket::setKeepAlive() { return 0; }
int SwTcpSocket::getKeepAlive() { return 0; }

int SwTcpSocket::send(const void *buf, unsigned int len)
{
	int sent = 0;
	if (swNetSocketSend(m_fd, buf, len, &sent) != 0)
		return -1;
	return sent;
}

int SwTcpSocket::recv(void *buf, unsigned int len)
{
	int got = 0;
	if (swNetSocketRecv(m_fd, buf, len, &got) != 0)
		return -1;
	return got;
}

bool SwTcpSocket::setNoDelay(uint8_t enable)
{
	return swNetSetTcpNoDelay(m_fd, enable) == 0;
}

int SwTcpClientSocket::open()
{
	if (m_fd != -1)
		return 8;
	if (swNetSocketCreateWithEpoll(&m_fd, SCE_NET_AF_INET, SCE_NET_SOCK_STREAM,
				       &m_epollId) != 0)
		return 200;
	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} sa;
	memset(&sa, 0, sizeof(sa));
	sa.in.sin_family = SCE_NET_AF_INET;
	if (swNetSocketBind(m_fd, &sa.generic, sizeof(sa.in), 0) != 0) {
		close();
		return 201;
	}
	return 0;
}

int SwTcpClientSocket::connect(const SwNetAddress &addr, const int *cancelFlag, int timeoutMs)
{
	SwString host;
	uint16_t port = 0;
	addr.get(host, port);

	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} sa;
	memset(&sa, 0, sizeof(sa));
	if (SwMwNetParseAddress(SCE_NET_AF_INET, host.c_str(), &sa.in.sin_addr) != 0)
		return 214;
	sa.in.sin_family = SCE_NET_AF_INET;
	sa.in.sin_port = swSwap16(port);
	if (swNetSocketConnectTimeout(m_fd, &sa.generic, sizeof(sa.in),
				      timeoutMs, m_epollId, cancelFlag) != 0)
		return 205;
	return 0;
}

int SwTcpServerSocket::listen(const SwNetAddress &addr, uint8_t flag, int backlog)
{
	if (!addr.isValid())
		return 5;
	if (m_fd != -1)
		return 8;
	if (swNetSocketCreateWithEpoll(&m_fd, SCE_NET_AF_INET, SCE_NET_SOCK_STREAM,
				       &m_epollId) != 0)
		return 200;

	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} sa;
	memset(&sa, 0, sizeof(sa));
	if (addr.isAnyHost()) {
		sa.in.sin_family = SCE_NET_AF_INET;
		sa.in.sin_port = swSwap16(addr.port());
	} else {
		SwString host;
		uint16_t port = 0;
		addr.get(host, port);
		(void)flag;
		if (SwMwNetResolveName(host.c_str(), port, SCE_NET_AF_INET, &sa.in) != 0) {
			close();
			return 214;
		}
	}

	int bindErr = swNetSocketBind(m_fd, &sa.generic, sizeof(sa.in), 1);
	if (bindErr != 0) {
		close();
		return (bindErr == 48) ? 202 : 201;
	}
	if (swNetSocketListen(m_fd, backlog) != 0) {
		close();
		return 203;
	}
	return 0;
}

int SwTcpServerSocket::accept(SwTcpSocket **outClient)
{
	if (!outClient)
		return 5;
	if (m_fd == -1)
		return 4;
	union {
		SceNetSockaddr generic;
		SceNetSockaddrIn in;
	} peer;
	unsigned int peerLen = sizeof(peer.in);
	int clientFd = -1;
	if (swNetSocketAccept(m_fd, &peer.generic, &peerLen, &clientFd) != 0)
		return 204;
	SwTcpSocket *sock = new (std::nothrow) SwTcpSocket(clientFd);
	*outClient = sock;
	return sock ? 0 : 2;
}

/* ---- SwThread ---- */

int SwThread::threadMain(void *selfPtr)
{
	SwThread *self = static_cast<SwThread *>(selfPtr);
	if (self) {
		self->m_result = 0;
		self->m_stateMutex.lock();
		self->m_state = 1;
		self->m_stateMutex.unlock();
		self->run();
		self->m_state = 2;
	}
	return 0;
}

SwThread::SwThread(int threadType)
	: m_threadType(threadType), m_state(0), m_reserved(0), m_result(4), m_cond(NULL)
{
	memset(&m_native, 0, sizeof(m_native));
}

SwThread::~SwThread()
{
	SwThread::join();
	delete m_cond;
	m_cond = NULL;
}

int SwThread::getResult() { return m_result; }
bool SwThread::isStopped() { return m_state == 2; }
int SwThread::getState() { return m_state; }

int SwThread::spawnNative(SwThreadStartArg *outArg, int (*entry)(void *),
			  void *arg, int threadType)
{
	char name[32];
	SceSize stackSize = 0x4000;
	int initPriority = SwsGetToolMode() ? 111 : 0x10000100;
	int cpuAffinityMask = 0;

	outArg->entry = entry;
	outArg->arg = arg;
	memset(name, 0, sizeof(name));
	memcpy(name, "SWS_THREAD_NO_NAME", 19);

	switch (threadType) {
	case TYPE_ANONYMOUS:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 31);
		break;
	case TYPE_AKE_CMD_TIMER:
		strncpy(name, "SWS_THREAD_TYPE_AKE_CMD_TIMER", 31);
		break;
	case TYPE_KX_EXP:
		strncpy(name, "SWS_THREAD_TYPE_KX_EXP", 31);
		break;
	case TYPE_SINK_AUTH:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AUTH", 31);
		stackSize = 0x8000;
		break;
	case TYPE_SINK_AKE_TIMER:
		strncpy(name, "SWS_THREAD_TYPE_SINK_AKE_TIMER", 31);
		break;
	case TYPE_SINK_CKC:
		strncpy(name, "SWS_THREAD_TYPE_SINK_CKC", 31);
		break;
	case TYPE_SRC_AUTH:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH", 31);
		stackSize = 0x8000;
		break;
	case TYPE_SRC_AUTH_SERVER:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AUTH_SERVER", 31);
		stackSize = 0x8000;
		break;
	case TYPE_SRC_AKE_TIMER:
		strncpy(name, "SWS_THREAD_TYPE_SRC_AKE_TIMER", 31);
		stackSize = 0x8000;
		break;
	case TYPE_RTT_REG:
		strncpy(name, "SWS_THREAD_TYPE_RTT_REG", 31);
		stackSize = 0x8000;
		break;
	case TYPE_TTL_CHECK:
		strncpy(name, "SWS_THREAD_TYPE_TTL_CHECK", 31);
		break;
	case TYPE_MEDIA_DISCONNECT:
		strncpy(name, "SWS_THREAD_TYPE_MEDIA_DISCONNECT", 31);
		break;
	case TYPE_SW_TIMER:
		strncpy(name, "SWS_THREAD_TYPE_SW_TIMER", 31);
		break;
	case TYPE_SW_DEP:
		strncpy(name, "SWS_THREAD_TYPE_SW_DEP", 31);
		cpuAffinityMask = SwsGetAffinity();
		if (cpuAffinityMask != 0) {
			cpuAffinityMask |= 0x80000;
			if (cpuAffinityMask != 0x80000)
				initPriority = 192;
		}
		break;
	case TYPE_TOOL:
		stackSize = 0x20000;
		strncpy(name, "SWS_THREAD_TYPE_TOOL", 31);
		break;
	case TYPE_ANONYMOUS_AFFINITY:
		strncpy(name, "SWS_THREAD_TYPE_ANONYMOUS", 31);
		cpuAffinityMask = SwsGetAffinity();
		if (cpuAffinityMask != 0) {
			cpuAffinityMask |= 0x80000;
			if (cpuAffinityMask != 0x80000)
				initPriority = 192;
		}
		break;
	default:
		break;
	}
	name[31] = '\0';

	SceUID thid = sceKernelCreateThread(name, SwMwThreadTrampoline, initPriority,
					    stackSize, 0, cpuAffinityMask, NULL);
	if (thid < 0)
		return thid;
	outArg->thid = thid;
	return sceKernelStartThread(thid, sizeof(*outArg), outArg);
}

int SwThread::joinNative(SwThreadStartArg *arg)
{
	int ret = sceKernelWaitThreadEnd(arg->thid, NULL, NULL);
	sceKernelDeleteThread(arg->thid);
	return ret;
}

int SwThread::start()
{
	if (m_state != 0)
		return 8;
	int err = m_condMutex.createCondition(&m_cond);
	if (err != 0)
		return err;
	if (spawnNative(&m_native, threadMain, this, m_threadType) != 0)
		return 3;
	bool waiting = true;
	while (waiting) {
		m_stateMutex.lock();
		if (m_state != 0)
			waiting = false;
		m_stateMutex.unlock();
		if (waiting)
			swYield();
	}
	return 0;
}

int SwThread::join()
{
	if (m_state != 0) {
		joinNative(&m_native);
		m_state = 0;
	}
	return 0;
}

/* ---- Utility helpers ---- */

int swStringToUpper(SwString &str)
{
	char rep[2] = { 0, 0 };
	for (size_t i = 0; i < str.length(); ++i) {
		char ch = str.at(i);
		if (ch >= 'a' && ch <= 'z') {
			rep[0] = (char)(ch - ('a' - 'A'));
			str.replace(i, 1, rep);
		}
	}
	return 0;
}

int swStringToLower(SwString &str)
{
	char rep[2] = { 0, 0 };
	for (size_t i = 0; i < str.length(); ++i) {
		char ch = str.at(i);
		if (ch >= 'A' && ch <= 'Z') {
			rep[0] = (char)(ch + ('a' - 'A'));
			str.replace(i, 1, rep);
		}
	}
	return 0;
}

void swStringTrim(SwString &str)
{
	while (!str.empty()) {
		size_t last = str.length() - 1;
		char ch = str.at(last);
		if (ch == ' ' || ch == '\t')
			str.erase(last, 1);
		else
			break;
	}
	if (!str.empty()) {
		size_t lead = 0;
		while (lead < str.length()) {
			char ch = str.at(lead);
			if (ch == ' ' || ch == '\t')
				++lead;
			else
				break;
		}
		if (lead > 0)
			str.erase(0, lead);
	}
}

uint32_t swSwap32(uint32_t val)
{
	return (val << 24) | ((val & 0x0000FF00u) << 8) |
	       ((val & 0x00FF0000u) >> 8) | (val >> 24);
}

uint16_t swSwap16(uint16_t val)
{
	return (uint16_t)((val << 8) | (val >> 8));
}

int swSleepMs(int ms)
{
	return sceKernelDelayThread((SceUInt32)(1000 * ms));
}

int swYield(void)
{
	return swSleepMs(10);
}

void swSleepCancelable(int stepMs, unsigned int totalMs, const volatile uint8_t *runFlag)
{
	for (unsigned int elapsed = 0; *runFlag != 0 && elapsed < totalMs; elapsed += (unsigned int)stepMs)
		swSleepMs(stepMs);
}

int swGetLocalTime(SwLocalTime *out)
{
	SceDateTime time;
	sceRtcGetCurrentClockLocalTime(&time);
	out->year = time.year;
	out->month = time.month;
	out->day = time.day;
	out->hour = time.hour;
	out->minute = time.minute;
	out->second = time.second;
	out->microsecond = time.microsecond;
	return 0;
}

void swFormatTimestamp(SwString &out)
{
	SwLocalTime lt;
	char buf[64];
	if (swGetLocalTime(&lt) != 0) {
		out.assign("");
		return;
	}
	snprintf(buf, sizeof(buf), "%04u/%02u/%02u %02u:%02u:%02u.%03u ",
		 lt.year, lt.month, lt.day, lt.hour, lt.minute, lt.second, lt.microsecond);
	out.assign(buf);
}

uint32_t swGetTickMs(void)
{
	SceRtcTick tick;
	sceRtcGetCurrentTick(&tick);
	return (uint32_t)(tick.tick / 1000ULL);
}
