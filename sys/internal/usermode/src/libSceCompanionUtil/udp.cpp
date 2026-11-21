/*
 * The search socket (UDP) and its jobs: the search request broadcast to port 987, the LAUNCH
 * and WAKEUP requests sent to one PS4, and the answers read into a list of the PS4s that
 * answered, each with when it last did.
 */
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <net/socket.h>
#include <net/inet.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

SceNetId g_udpSocket = -1;

static char s_searchRequest[] =
	"SRCH * HTTP/1.1\ndevice-discovery-protocol-version:00010010\n";
static char s_protocolVersion[] = "device-discovery-protocol-version:00020020";

static char s_recvBuffer[0x1000];
static char s_sendBuffer[0xa0];

int CUdpSocket::Close(void)
{
	if (g_udpSocket < 0)
		return -1;
	sceNetShutdown(g_udpSocket, SCE_NET_SHUT_RDWR);
	if (sceNetSocketClose(g_udpSocket) < 0)
		return -1;
	g_udpSocket = -1;
	return 0;
}

int CUdpSocket::Open(void)
{
	SceNetSockaddrIn sin;
	int optval = 0;

	if (g_udpSocket >= 0)
		return -1;
	g_udpSocket = sceNetSocket("send_udp_socket", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (g_udpSocket < 0)
		return -1;

	optval = 1;
	if (sceNetSetsockopt(g_udpSocket, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
			     sizeof(optval)) < 0) {
		CUdpSocket::Close();
		return -1;
	}
	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(0);
	sin.sin_vport = 0;
	if (sceNetBind(g_udpSocket, (const SceNetSockaddr *)&sin, sizeof(sin)) < 0) {
		CUdpSocket::Close();
		return -1;
	}
	return 0;
}

int CUdpSocket::Abort(void)
{
	return sceNetSocketAbort(g_udpSocket, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
}

int CUdpSearchSendJob::Request(int port)
{
	if (m_state == JOB_RUNNING)
		return -1;
	m_port = port;
	m_state = JOB_RUNNING;
	return 0;
}

int CUdpSearchSendJob::Run(void)
{
	SceNetSockaddrIn sin;
	int ret;

	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(m_port);
	if (m_abort)
		return -1;
	sin.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;
	sin.sin_vport = 0;
	ret = sceNetSendto(g_udpSocket, s_searchRequest, sizeof(s_searchRequest), 0,
			   (const SceNetSockaddr *)&sin, sizeof(sin));
	if (ret < 0)
		return -1;
	if (m_abort)
		return -1;
	return ret;
}

void CUdpSearchSendJob::Finish(void)
{
	int lock;

	lock = LockMutex();
	m_state = JOB_DONE;
	UnlockMutex(lock);
}

int CUdpWakeupSendJob::Request(const SceNetSockaddrIn *address, int type, int userCredential)
{
	if (m_state == JOB_RUNNING)
		return -1;
	m_address = *address;
	m_type = type;
	m_userCredential = userCredential;
	m_state = JOB_RUNNING;
	return 0;
}

int CUdpWakeupSendJob::Run(void)
{
	const char *model;
	SceSize length;
	int ret;

	switch (m_type) {
	case 0:
		sceClibSnprintf(s_sendBuffer, sizeof(s_sendBuffer), "LAUNCH * HTTP/1.1\n%s\n",
				s_protocolVersion);
		break;
	case 1:
		sceClibSnprintf(s_sendBuffer, sizeof(s_sendBuffer),
				"WAKEUP * HTTP/1.1\nclient-type:%s\nauth-type:R\nmodel:%s\n"
				"app-type:%s\nuser-credential:%d\n%s\n",
				"vc", "v", "s", m_userCredential, s_protocolVersion);
		break;
	case 2:
		model = IsDolce() ? "d" : "v";
		sceClibSnprintf(s_sendBuffer, sizeof(s_sendBuffer),
				"WAKEUP * HTTP/1.1\nclient-type:%s\nauth-type:R\nmodel:%s\n"
				"app-type:%s\nuser-credential:%d\n%s\n",
				"vr", model, "r", m_userCredential, s_protocolVersion);
		break;
	}
	length = sceClibStrnlen(s_sendBuffer, sizeof(s_sendBuffer));
	if (m_abort)
		return -1;
	ret = sceNetSendto(g_udpSocket, s_sendBuffer, length, 0,
			   (const SceNetSockaddr *)&m_address, sizeof(m_address));
	if (ret < 0)
		return -1;
	return ret;
}

void CUdpWakeupSendJob::Finish(void)
{
	int lock;

	lock = LockMutex();
	m_state = JOB_DONE;
	UnlockMutex(lock);
}

int CUdpSearchRecvJob::Request(CSearchList *searchList, SceUInt64 timeout)
{
	SceUInt32 capacity;

	if (m_state == JOB_RUNNING)
		return -1;
	m_searchList = searchList;
	m_abort = false;
	m_timeout = timeout;
	capacity = searchList->GetCapacity();
	if (m_hosts.m_hosts == NULL) {
		m_hosts.m_hosts = (CompanionUtilHost *)Allocate(capacity * sizeof(CompanionUtilHost));
		if (m_hosts.m_hosts != NULL) {
			sceClibMemset(m_hosts.m_hosts, 0, capacity * sizeof(CompanionUtilHost));
			m_hosts.m_capacity = capacity;
			m_hosts.m_count = 0;
		}
	}
	searchList->CopyTo(&m_hosts);
	m_state = JOB_RUNNING;
	return 0;
}

/* a header's value up to the end of its line: 0, -1 if it is missing, -2 if it is too long */
static inline int GetHeader(const char *message, const char *name, char *value, int size)
{
	const char *p;
	int i;

	p = sceClibStrstr(message, name);
	if (p == NULL)
		return -1;
	p += sceClibStrnlen(name, sceClibStrnlen(message, sizeof(s_recvBuffer)));
	for (i = 0; i < size; i++) {
		if (p[i] == '\n' || p[i] == '\0' || p[i] == '\r') {
			value[i] = '\0';
			return 0;
		}
		value[i] = p[i];
	}
	return -2;
}

int CUdpSearchRecvJob::ParseStatus(const char *message)
{
	char status[64];
	int ret;

	GetHeader(message, "HTTP/1.1", status, sizeof(status));
	ret = 1;
	if (sceClibStrstr(status, "200") == NULL) {
		ret = 0;
		if (sceClibStrstr(status, "620") != NULL)
			ret = 2;
	}
	return ret;
}

int CUdpSearchRecvJob::Run(void)
{
	SceNetSockaddrIn from;
	char port[64];
	SceNetSocklen_t fromLength;
	CompanionUtilHost host;
	CompanionUtilHost *entry;
	SceUInt64 deadline, now;
	const char *p;
	bool changed;
	int portNumber, portResult;
	SceUInt32 i;
	int ret;

	sceClibMemset(&from, 0, sizeof(from));
	if (g_udpSocket < 0)
		return -1;
	deadline = CTimer::GetTime() + 1000000;
	fromLength = sizeof(from);
	changed = false;
	for (;;) {
		if (m_abort)
			return -2;
		ret = sceNetRecvfrom(g_udpSocket, s_recvBuffer, sizeof(s_recvBuffer),
				     SCE_NET_MSG_DONTWAIT, (SceNetSockaddr *)&from, &fromLength);
		if (ret > 0) {
			sceClibMemset(&host, 0, sizeof(host));
			host.address = from.sin_addr.s_addr;
			if (GetHeader(s_recvBuffer, "host-name:", host.hostName,
				      sizeof(host.hostName)) != 0)
				goto age;
			if (GetHeader(s_recvBuffer, "host-id:", host.hostId,
				      sizeof(host.hostId)) != 0)
				goto age;
			portResult = GetHeader(s_recvBuffer, "host-request-port:", port, sizeof(port));
			portNumber = 0;
			for (p = port; *p >= '0' && *p <= '9'; p++)
				portNumber = portNumber * 10 + *p - '0';
			host.port = sceNetHtons(portNumber);
			host.status = ParseStatus(s_recvBuffer);
			host.lastSeen = CTimer::GetTime();
			if (portResult < 0)
				goto age;

			entry = NULL;
			for (i = 0; i < m_hosts.m_count; i++) {
				if (sceClibStrncmp(m_hosts.At(i)->hostId, host.hostId,
						   sizeof(host.hostId)) == 0) {
					entry = m_hosts.At(i);
					break;
				}
			}
			if (entry != NULL) {
				entry->lastSeen = host.lastSeen;
				if (entry->status != host.status) {
					entry->status = host.status;
					changed = true;
				}
			} else {
				if (m_hosts.m_count >= m_hosts.m_capacity)
					return ret;
				sceClibMemcpy_safe(&m_hosts.m_hosts[m_hosts.m_count], &host,
						   sizeof(host));
				m_hosts.m_count++;
				changed = true;
			}
		}
age:
		now = CTimer::GetTime();
		for (i = 0; i < m_hosts.m_count; i++) {
			entry = m_hosts.At(i);
			if (entry->lastSeen + m_timeout < now) {
				entry->lastSeen = 0;
				changed = true;
			}
		}
		if (changed == true && now >= deadline)
			break;
		sceKernelDelayThread(1000);
	}
	return 0;
}

void CUdpSearchRecvJob::Finish(int result)
{
	int lock;

	lock = LockMutex();
	if (lock < 0)
		return;
	if (result >= 0 || result == -2)
		m_searchList->Update(&m_hosts);
	if (m_hosts.m_hosts != NULL) {
		Free(m_hosts.m_hosts);
		m_hosts.m_hosts = NULL;
	}
	m_state = JOB_DONE;
	UnlockMutex(lock);
}

} /* namespace SceCompanionUtil */
