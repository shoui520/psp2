/*
 * teleport_multicast.cpp: SceShellTeleportCommThread UDP multicast discovery
 * (239.255.255.250:41899), TeleportNetMonitor (NetCtl, registry, power
 * callbacks), and string/UTF-8 helpers (0x813A6194..0x813A7253).
 */
#include "teleport_internal.h"

#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <libnetctl.h>
#include <regmgr/regmgr.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <timer/timerlistenerlist.h>
#include "../../console_info/console_info.h"

extern "C" {
int SceShell_2E0B6FA6(void (*callback)(SceInt32, SceInt32, SceInt32, SceInt32, SceInt32, void *),
		      void *arg, int *outId);
int scePowerRegisterCallback(SceUID cbid);
SceUInt32 vshPowerGetWakeupFactor(void);
int sceNetInetPton(int af, const char *src, void *dst);
int sceNetEtherNtostr(const SceNetEtherAddr *n, char *str, SceSize len);
}

static const SceUInt8 kTeleportMulticastGroupIpv4[4] = { 239, 255, 255, 250 };
static const SceInt32 kTeleportPowerWakeupMetricId = 225;
static const SceInt32 kIoEnoentError = -2147418110;	/* 0x80010002 */

static SceUID s_teleportRegistryCallbackId = 0;	/* 0x815CAC84 */
static SceUID s_teleportPowerCallbackId = 0;	/* 0x815CAC88 */

/* ============================================================================
 * TeleportMulticastThread & TeleportMulticastServer (0x813A6194..0x813A6543)
 * ============================================================================ */

TeleportMulticastThread::TeleportMulticastThread(const TeleportMulticastCallbacks &callbacks)
	: paf::thread::Thread(0x1000011F, 0x400, "SceShellTeleportCommThread", NULL),
	  m_socket(0),
	  m_eventFlag(0),
	  m_stopState(0),
	  m_port(0)
{
	sce_paf_memset(&m_callbacks, 0, sizeof(m_callbacks));
	m_callbacks.onRecv = callbacks.onRecv;
	m_callbacks.recvBuffer = callbacks.recvBuffer;
	m_callbacks.recvBufferSize = callbacks.recvBufferSize;
	m_callbacks.userData = callbacks.userData;
	sce_paf_memset(&m_peerAddr, 0, sizeof(m_peerAddr));
	sce_paf_memset(m_localIpv4, 0, sizeof(m_localIpv4));
	sce_paf_memset(m_groupIpv4, 0, sizeof(m_groupIpv4));
	m_eventFlag = sceKernelCreateEventFlag("SceTeleportMulticastEvent", 0x1000, 0, NULL);
}

TeleportMulticastThread::~TeleportMulticastThread()
{
}

/* 0x813A62CE */
SceInt32 TeleportMulticastThread::SendReply(TeleportMulticastThread *thread, SceNetId sock,
					    const void *buffer, SceSize length)
{
	char groupStr[16];
	sce_paf_memset(groupStr, 0, sizeof(groupStr));
	sce_paf_snprintf(groupStr, sizeof(groupStr), "%d.%d.%d.%d",
			 thread->m_groupIpv4[0], thread->m_groupIpv4[1],
			 thread->m_groupIpv4[2], thread->m_groupIpv4[3]);
	return sceNetSendto(sock, buffer, length, 0, &thread->m_peerAddr,
			    sizeof(thread->m_peerAddr));
}

/* 0x813A634A */
SceInt32 TeleportMulticastThread::BindMulticastSocket(void)
{
	SceInt32 ttl = 1;
	SceInt32 loopDisabled = 0;

	char groupStr[16];
	sce_paf_memset(groupStr, 0, sizeof(groupStr));
	sce_paf_snprintf(groupStr, sizeof(groupStr), "%d.%d.%d.%d",
			 m_groupIpv4[0], m_groupIpv4[1], m_groupIpv4[2], m_groupIpv4[3]);

	SceNetSockaddrIn bindAddr;
	sce_paf_memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = SCE_NET_AF_INET;
	bindAddr.sin_port = sceNetHtons(m_port);
	bindAddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);

	m_socket = sceNetSocket("SceTeleportUdpServer", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	SceInt32 ret = sceNetBind(m_socket, reinterpret_cast<const SceNetSockaddr *>(&bindAddr),
				  sizeof(bindAddr));
	if (ret < 0)
		return ret;

	SceNetIpMreq mreq;
	sce_paf_memset(&mreq, 0, sizeof(mreq));
	sceNetInetPton(SCE_NET_AF_INET, groupStr, &mreq.imr_multiaddr);
	mreq.imr_interface.s_addr = SCE_NET_INADDR_ANY;

	ret = sceNetSetsockopt(m_socket, SCE_NET_IPPROTO_IP, SCE_NET_IP_ADD_MEMBERSHIP,
			       &mreq, sizeof(mreq));
	if (ret < 0)
		return ret;

	ret = sceNetSetsockopt(m_socket, SCE_NET_IPPROTO_IP, SCE_NET_IP_MULTICAST_TTL,
			       &ttl, sizeof(ttl));
	if (ret < 0)
		return ret;

	return sceNetSetsockopt(m_socket, SCE_NET_IPPROTO_IP, SCE_NET_IP_MULTICAST_LOOP,
				&loopDisabled, sizeof(loopDisabled));
}

/* 0x813A6436 */
void TeleportMulticastThread::EntryFunction()
{
	for (;;) {
		SceUInt32 outBits = 0;
		sceKernelWaitEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT, 5, &outBits, NULL);

		if (m_socket == 0 || m_port == 0) {
			BindMulticastSocket();
			if (m_stopState == 2)
				m_stopState = 0;
			else
				sceKernelSetEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT);
			continue;
		}

		sce_paf_memset(m_callbacks.recvBuffer, 0, m_callbacks.recvBufferSize);
		sce_paf_memset(&m_peerAddr, 0, sizeof(m_peerAddr));
		SceNetSocklen_t peerLen = sizeof(m_peerAddr);

		SceInt32 peekLen = sceNetRecvfrom(m_socket, NULL, m_callbacks.recvBufferSize,
						  SCE_NET_MSG_PEEK | SCE_NET_MSG_USECRYPTO,
						  &m_peerAddr, &peerLen);
		bool recvOk = false;
		if (peekLen > 0 && static_cast<SceSize>(peekLen) <= m_callbacks.recvBufferSize) {
			SceInt32 totalRead = 0;
			recvOk = true;
			while (totalRead < peekLen) {
				SceInt32 chunkLen = sceNetRecvfrom(
					m_socket, m_callbacks.recvBuffer + totalRead,
					m_callbacks.recvBufferSize - static_cast<SceSize>(totalRead),
					SCE_NET_MSG_WAITALL, &m_peerAddr, &peerLen);
				if (chunkLen <= 0) {
					recvOk = false;
					break;
				}
				totalRead += chunkLen;
			}
		}

		if (!recvOk) {
			if (m_socket != 0) {
				sceNetSocketAbort(m_socket,
						  SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
				sceNetSocketClose(m_socket);
				m_socket = 0;
				m_port = 0;
			}
			SceInt32 netState = 0;
			sceNetCtlInetGetState(&netState);
			if (netState == SCE_NET_CTL_STATE_IPOBTAINED)
				sceKernelSetEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT);
			continue;
		}

		if (m_callbacks.onRecv != NULL) {
			TeleportUdpPacket packet;
			packet.buffer = m_callbacks.recvBuffer;
			packet.capacity = m_callbacks.recvBufferSize;
			m_callbacks.onRecv(this, m_socket, &packet, SendReply,
					   m_callbacks.userData);
		}

		if (m_stopState == 2)
			m_stopState = 0;
		else
			sceKernelSetEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT);
	}
}

TeleportMulticastServer::~TeleportMulticastServer()
{
	DestroyThread();
}

TeleportMulticastServer *TeleportMulticastServer::GetInstance(void)
{
	static TeleportMulticastServer instance;
	return &instance;
}

/* 0x813A6194 */
SceInt32 TeleportMulticastServer::Start(const TeleportMulticastCallbacks &callbacks)
{
	m_thread = new TeleportMulticastThread(callbacks);
	if (m_thread != NULL)
		m_thread->Start();
	return 0;
}

/* 0x813A625A */
SceInt32 TeleportMulticastServer::Stop(void)
{
	if (m_thread == NULL)
		return 0;

	m_thread->m_stopState = 2;
	if (m_thread->m_socket != 0) {
		sceNetSocketAbort(m_thread->m_socket, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(m_thread->m_socket);
		m_thread->m_socket = 0;
		m_thread->m_port = 0;
	}
	return 0;
}

/* 0x813A627C */
SceInt32 TeleportMulticastServer::Reconfigure(const SceUInt8 localIpv4[4],
					      const SceUInt8 groupIpv4[4], SceUInt16 port)
{
	if (m_thread == NULL)
		return 0;

	m_thread->m_stopState = 0;
	if (m_thread->m_socket != 0) {
		sceNetSocketAbort(m_thread->m_socket, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(m_thread->m_socket);
		m_thread->m_socket = 0;
		m_thread->m_port = 0;
	}

	sce_paf_memcpy(m_thread->m_localIpv4, localIpv4, 4);
	sce_paf_memcpy(m_thread->m_groupIpv4, groupIpv4, 4);
	m_thread->m_port = port;
	return sceKernelSetEventFlag(m_thread->m_eventFlag, TELEPORT_EVENT_READY_BIT);
}

void TeleportMulticastServer::DestroyThread(void)
{
	if (m_thread != NULL) {
		delete m_thread;
		m_thread = NULL;
	}
}

/* ============================================================================
 * TeleportDiscoveryProtocol (0x813A6544..0x813A6957)
 * ============================================================================ */

TeleportDiscoveryProtocol *TeleportDiscoveryProtocol::GetInstance(void)
{
	static TeleportDiscoveryProtocol instance;
	return &instance;
}

/* 0x813A6544 */
SceInt32 TeleportDiscoveryProtocol::SendSearchReply(TeleportMulticastThread *thread,
						    SceNetId sock, SceInt32 replyType,
						    TeleportUdpSendFunc sendFn)
{
	if (sock == 0)
		return 0;

	TeleportNetMonitor *netMonitor = TeleportNetMonitor::GetInstance();
	SceUInt16 httpPort = TeleportHttpServer::GetInstance()->GetListenPort();

	SceUInt8 localIpv4[4];
	char macAddress[20];
	if (netMonitor->GetLocalIpv4(localIpv4) != 0 ||
	    netMonitor->GetMacAddress(macAddress) != 0)
		return 0;

	char replyBuf[512];
	sce_paf_memset(replyBuf, 0, sizeof(replyBuf));
	SceSize replyLen = 0;
	if (replyType == 1) {
		static const char kReplyFmt[] =
			"200 OK\r\n"
			"LOCATION: http://%d.%d.%d.%d:%d/teleport.xml\r\n"
			"SERVER: PlayStation Vita TV Teleport/2.00\r\n"
			"X-TELEPORT-ST:\r\n"
			"X-TELEPORT-ADDRESS: %s\r\n";
		sce_paf_snprintf(replyBuf, sizeof(replyBuf), kReplyFmt,
				 localIpv4[0], localIpv4[1], localIpv4[2], localIpv4[3],
				 static_cast<int>(httpPort), macAddress);
		replyLen = sce_paf_strlen(replyBuf) + 1;
	}

	return sendFn(thread, sock, replyBuf, replyLen);
}

/* 0x813A66B4 */
SceInt32 TeleportDiscoveryProtocol::HandleSearchPacket(TeleportMulticastThread *thread,
						       SceNetId sock,
						       const TeleportUdpPacket *packet,
						       TeleportUdpSendFunc sendFn, void *userData)
{
	(void)userData;
	TeleportNetMonitor::GetInstance();
	TeleportHttpServer::GetInstance();

	char upperBuf[512];
	sce_paf_memset(upperBuf, 0, sizeof(upperBuf));
	for (SceSize idx = 0; idx < packet->capacity && idx < sizeof(upperBuf) - 1; ++idx) {
		char ch = packet->buffer[idx];
		if (ch == '\0')
			break;
		upperBuf[idx] = static_cast<char>(sce_paf_toupper(ch));
	}

	paf::string message(upperBuf);
	paf::string crlfDelim("\r\n");
	paf::vector<paf::string> lines;
	teleportSplitString(&lines, message, crlfDelim, 10);

	if (message.find("SEARCH") == 0) {
		const char *stHeader = "X-TELEPORT-ST:";
		SceSize stHeaderLen = sce_paf_strlen(stHeader);
		for (SceSize lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
			if (sce_paf_strncasecmp(lines[lineIdx].c_str(), stHeader,
						stHeaderLen) == 0) {
				SendSearchReply(thread, sock, 1, sendFn);
				break;
			}
		}
	}
	return 0;
}

/* ============================================================================
 * TeleportNetMonitor & TeleportCallbackTimerListener (0x813A6958..0x813A7253)
 * ============================================================================ */

TeleportNetMonitor::TeleportNetMonitor()
	: m_netDisconnected(1),
	  m_mediaTransferDisabled(1),
	  m_timerListener(NULL)
{
	sce_paf_memset(m_localIpv4, 0, sizeof(m_localIpv4));
	sce_paf_memset(m_macAddress, 0, sizeof(m_macAddress));
}

TeleportNetMonitor *TeleportNetMonitor::GetInstance(void)
{
	static TeleportNetMonitor instance;
	return &instance;
}

/* 0x813A6958 */
void TeleportNetMonitor::RefreshNetworkInfo(void)
{
	SceNetCtlInfo ipInfo;
	SceNetCtlInfo macInfo;
	sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &ipInfo);
	sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_ETHER_ADDR, &macInfo);
	sceNetEtherNtostr(&macInfo.ether_addr, m_macAddress, sizeof(m_macAddress));

	paf::string remaining(ipInfo.ip_address);
	SceInt32 octetIdx = 0;
	for (;;) {
		paf::string::size_type dotPos = remaining.find_first_of(".");
		if (dotPos == paf::string::npos)
			break;
		if (dotPos > 0 && octetIdx < 4) {
			paf::string part = remaining.substr(0, dotPos);
			m_localIpv4[octetIdx] = static_cast<SceUInt8>(sce_paf_atoi(part.c_str()));
			++octetIdx;
		}
		remaining = remaining.substr(dotPos + 1, paf::string::npos);
	}
	if (octetIdx < 4)
		m_localIpv4[octetIdx] = static_cast<SceUInt8>(sce_paf_atoi(remaining.c_str()));
}

/* 0x813A6AB8 */
SceInt32 TeleportNetMonitor::Initialize(void)
{
	int netCtlCbId = 0;
	SceShell_2E0B6FA6(OnNetCtlEvent, this, &netCtlCbId);

	SceInt32 netState = 0;
	sceNetCtlInetGetState(&netState);
	m_netDisconnected = 1;
	if (netState == SCE_NET_CTL_STATE_IPOBTAINED ||
	    netState == SCE_NET_CTL_STATE_IPOBTAINING) {
		RefreshNetworkInfo();
		m_netDisconnected = 0;
	}

	m_mediaTransferDisabled = 1;
	SceInt32 enabled = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/NP2/TELEPORT", "enable_media_transfer", &enabled) == 0 &&
	    enabled == 1)
		m_mediaTransferDisabled = 0;

	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(0.0f, paf::Timer::FUNC_LINEAR, paf::Timer::REPEAT_NONE, -1, NULL);
	m_timerListener = new TeleportCallbackTimerListener(timer, this);

	paf::TimerListenerList::ListenerParam listenerParam(m_timerListener, true, NULL, NULL);
	paf::TimerListenerList::s_default_list->Register(listenerParam);
	return 0;
}

/* 0x813A6BD0 */
SceInt32 TeleportNetMonitor::GetLocalIpv4(SceUInt8 outIpv4[4])
{
	if (m_localIpv4[0] == 0 || m_macAddress[0] == '\0')
		RefreshNetworkInfo();
	if (m_localIpv4[0] == 0 || m_macAddress[0] == '\0')
		return -1;
	outIpv4[0] = m_localIpv4[0];
	outIpv4[1] = m_localIpv4[1];
	outIpv4[2] = m_localIpv4[2];
	outIpv4[3] = m_localIpv4[3];
	return 0;
}

/* 0x813A6C08 */
SceInt32 TeleportNetMonitor::GetMacAddress(char *outMac)
{
	if (m_localIpv4[0] == 0 || m_macAddress[0] == '\0')
		RefreshNetworkInfo();
	if (m_localIpv4[0] == 0 || m_macAddress[0] == '\0')
		return -1;
	sce_paf_strncpy(outMac, m_macAddress, sizeof(m_macAddress));
	return 0;
}

/* 0x813A6C40 */
SceInt32 TeleportNetMonitor::StartServersIfEnabled(void)
{
	if (m_mediaTransferDisabled == 0) {
		TeleportMulticastServer::GetInstance()->Reconfigure(
			m_localIpv4, kTeleportMulticastGroupIpv4, TELEPORT_MULTICAST_PORT);
		TeleportHttpServer::GetInstance()->Reconfigure(m_localIpv4, 0);
	}
	return 0;
}

/* 0x813A6D18 */
void TeleportNetMonitor::OnNetCtlEvent(SceInt32 prevState, SceInt32 curState, SceInt32 event,
				       SceInt32 errorCode, SceInt32 info, void *userArg)
{
	(void)errorCode;
	(void)info;
	TeleportNetMonitor *monitor = static_cast<TeleportNetMonitor *>(userArg);
	if (curState == SCE_NET_CTL_STATE_IPOBTAINED &&
	    (event == 6 || prevState == 5)) {
		monitor->RefreshNetworkInfo();
		monitor->StartServersIfEnabled();
	}
	if (curState != SCE_NET_CTL_STATE_IPOBTAINED) {
		TeleportMulticastServer::GetInstance()->Stop();
		TeleportHttpServer::GetInstance()->Stop();
	}
}

/* 0x813A6DC4 */
SceInt32 TeleportNetMonitor::OnRegistryChanged(SceInt32 notifyId, SceInt32 notifyCount,
					       SceInt32 notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;

	SceInt32 enabled = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/NP2/TELEPORT", "enable_media_transfer", &enabled) == 0) {
		TeleportNetMonitor *monitor = TeleportNetMonitor::GetInstance();
		if (enabled == 1) {
			monitor->m_mediaTransferDisabled = 0;
			monitor->StartServersIfEnabled();
		} else {
			monitor->m_mediaTransferDisabled = 1;
			TeleportMulticastServer::GetInstance()->Stop();
			TeleportHttpServer::GetInstance()->Stop();
		}
	}
	return 0;
}

/* 0x813A6F06 */
SceInt32 TeleportNetMonitor::OnPowerCallback(SceInt32 notifyId, SceInt32 notifyCount,
					     SceInt32 powerInfo, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)common;
	if ((powerInfo & 0x40000) != 0 && (vshPowerGetWakeupFactor() & 0x7Fu) == 0x1Eu)
		consoleInfoFileAddInt(kTeleportPowerWakeupMetricId, 1, 0);
	return 0;
}

/* 0x813A71DC */
void TeleportCallbackTimerListener::OnFinish(void *timerData, void *userData)
{
	(void)timerData;
	(void)userData;
	if (m_callbacksRegistered) {
		sceKernelCheckCallback();
		return;
	}

	s_teleportRegistryCallbackId = sceKernelCreateCallback(
		"SceShellTeleport", 0, TeleportNetMonitor::OnRegistryChanged, NULL);
	sceRegMgrRegisterCallback(s_teleportRegistryCallbackId);
	sceRegMgrAddRegistryCallback(s_teleportRegistryCallbackId,
				     "/CONFIG/NP2/TELEPORT/enable_media_transfer");
	sceRegMgrStartCallback(s_teleportRegistryCallbackId);

	s_teleportPowerCallbackId = sceKernelCreateCallback(
		"SceTeleportPowerCallback", 0, TeleportNetMonitor::OnPowerCallback, NULL);
	scePowerRegisterCallback(s_teleportPowerCallbackId);
	m_callbacksRegistered = true;
}

/* ============================================================================
 * Utility functions (0x813A6F26..0x813A71DB)
 * ============================================================================ */

/* 0x813A6F26 */
void teleportSplitString(paf::vector<paf::string> *outParts, const paf::string &input,
			 const paf::string &delimiters, SceSize maxParts)
{
	outParts->clear();
	paf::string remaining = input;
	for (;;) {
		paf::string::size_type delimPos = remaining.find_first_of(delimiters.c_str());
		if (delimPos == paf::string::npos)
			break;
		if (delimPos > 0) {
			if (maxParts == 0 || outParts->size() < maxParts)
				outParts->push_back(remaining.substr(0, delimPos));
		}
		remaining = remaining.substr(delimPos + 1, paf::string::npos);
	}
	if (!remaining.empty())
		outParts->push_back(remaining);
}

/* 0x813A7146 */
SceInt32 teleportHasNoControlChars(const paf::string &text)
{
	const SceUInt8 *bytes = reinterpret_cast<const SceUInt8 *>(text.c_str());
	SceSize len = text.size();
	for (SceSize pos = 0; pos < len; ++pos) {
		SceUInt8 ch = bytes[pos];
		if (ch <= 0x1Fu || ch == 0x7Fu)
			return 0;
		if (ch == 0xC2u && (pos + 1) < len &&
		    bytes[pos + 1] >= 0x80u && bytes[pos + 1] <= 0x9Fu)
			return 0;
	}
	return 1;
}

/* 0x813A7194 */
void teleportEnsureVideoTempDir(void)
{
	SceIoStat st;
	if (sceIoGetstat("ur0:temp/videostreaming", &st) == kIoEnoentError)
		sceIoMkdir("ur0:temp/videostreaming", 6);
}
