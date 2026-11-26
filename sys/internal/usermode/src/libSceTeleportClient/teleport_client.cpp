/*
 * teleport_client.cpp: TeleportClient core discovery, HTTP/TCP transport,
 * remote app control, and registry cache implementation (0x81000A88-0x81003663).
 */
#include "teleport_internal.h"

static SceUInt8 s_searchStopFlag = 0;
static SceUInt32 s_threadRunningMask = 0;
static TeleportDiscoveredDevice *s_discoveredDevices = NULL;
static SceInt32 s_discoveredWriteIdx = 0;
static char *s_sendBuf = NULL;
static char *s_recvBuf = NULL;
static int s_udpSocket = -1;
static SceUID s_wolEventFlag = -1;
static SceUID s_searchEventFlag = -1;
static SceKernelLwMutexWork s_mutex;

static int parseSignedDecimal(const char *str, SceSize maxLen)
{
	int value = 0;
	int negative = 0;
	for (SceSize i = 0; i <= sceClibStrnlen(str, maxLen); ++i) {
		int ch = (int)(signed char)str[i];
		if (ch < '0' || ch > '9') {
			if (str[0] != '-' || i != 0)
				break;
			negative = 1;
		} else {
			value = value * 10 + (ch - '0');
		}
	}
	if (negative == 1)
		value = -value;
	return value;
}

static inline SceUInt8 parseLowerHexNibble(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return (SceUInt8)(ch - 'a' + 10);
	return (SceUInt8)(ch - '0');
}

static inline char formatLowerHexNibble(SceUInt8 nibble)
{
	if (nibble >= 10)
		return (char)(nibble + 'a' - 10);
	return (char)(nibble + '0');
}

TeleportClient::TeleportClient()
{
}

TeleportClient::~TeleportClient()
{
}

SceInt32 TeleportClient::initialize(SceTeleportClientInitializeInfo *info)
{
	cachedDeviceCount = 0;
	deviceSocketCount = 0;
	searchSendThreadId = 0;
	wolThreadId = 0;
	callback = NULL;
	allocFunc = info->_malloc;
	freeFunc = info->_free;
	initInfo = info;
	searchRecvThreadId = 0;

	char addrStr[16];
	memset(addrStr, 0, sizeof(addrStr));
	sceClibSnprintf(addrStr, sizeof(addrStr), TELEPORT_MULTICAST_ADDR);
	sceNetInetPton(SCE_NET_AF_INET, addrStr, &multicastAddr.sin_addr);
	multicastAddr.sin_port = sceNetHtons(TELEPORT_MULTICAST_PORT);
	multicastAddr.sin_family = SCE_NET_AF_INET;

	SceInt32 ret = sceKernelCreateLwMutex(&s_mutex, "SceTeleportMutex",
					      SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret >= 0) {
		s_wolEventFlag = sceKernelCreateEventFlag("SceTeleportWolEvent", 0, 0, NULL);
		if (s_wolEventFlag >= 0) {
			s_searchEventFlag = sceKernelCreateEventFlag("SceTeleportSeachEvent", 0, 0, NULL);
			if (s_searchEventFlag >= 0) {
				s_sendBuf = (char *)allocFunc(TELEPORT_IO_BUFFER_SIZE);
				if (s_sendBuf != NULL) {
					s_recvBuf = (char *)allocFunc(TELEPORT_IO_BUFFER_SIZE);
					if (s_recvBuf != NULL) {
						s_discoveredDevices = (TeleportDiscoveredDevice *)allocFunc(
							sizeof(TeleportDiscoveredDevice) * TELEPORT_DISCOVERED_MAX);
						if (s_discoveredDevices != NULL) {
							s_discoveredWriteIdx = 0;
							memset(s_discoveredDevices, 0,
								      sizeof(TeleportDiscoveredDevice) * TELEPORT_DISCOVERED_MAX);
							if (ret == 0)
								return 0;
						} else {
							ret = SCE_TELEPORT_ERROR_NO_MEMORY;
						}
					} else {
						ret = SCE_TELEPORT_ERROR_NO_MEMORY;
					}
				} else {
					ret = SCE_TELEPORT_ERROR_NO_MEMORY;
				}
			} else {
				ret = SCE_TELEPORT_ERROR_FATAL;
			}
		} else {
			ret = SCE_TELEPORT_ERROR_FATAL;
		}
	} else {
		ret = SCE_TELEPORT_ERROR_FATAL;
	}

	if (s_sendBuf != NULL) {
		freeFunc(s_sendBuf);
		s_sendBuf = NULL;
	}
	if (s_recvBuf != NULL) {
		freeFunc(s_recvBuf);
		s_recvBuf = NULL;
	}
	if (s_discoveredDevices != NULL) {
		freeFunc(s_discoveredDevices);
		s_discoveredDevices = NULL;
	}
	if (s_searchEventFlag != -1) {
		sceKernelDeleteEventFlag(s_searchEventFlag);
		s_searchEventFlag = -1;
	}
	if (s_wolEventFlag != -1) {
		sceKernelDeleteEventFlag(s_wolEventFlag);
		s_wolEventFlag = -1;
	}
	return ret;
}

SceInt32 TeleportClient::finalize()
{
	if (s_sendBuf != NULL) {
		freeFunc(s_sendBuf);
		s_sendBuf = NULL;
	}
	if (s_recvBuf != NULL) {
		freeFunc(s_recvBuf);
		s_recvBuf = NULL;
	}
	if (extraBuf != NULL) {
		freeFunc(extraBuf);
		extraBuf = NULL;
	}
	for (SceInt32 i = 0; i < cachedDeviceCount; ++i)
		sceNetSocketClose(deviceSockets[i].sockFd);
	if (deviceSockets != NULL) {
		freeFunc(deviceSockets);
		deviceSockets = NULL;
		deviceSocketCount = 0;
	}
	if (s_searchEventFlag != -1) {
		sceKernelCancelEventFlag(s_searchEventFlag, 0, NULL);
		sceKernelDeleteEventFlag(s_searchEventFlag);
		s_searchEventFlag = -1;
	}
	if (s_wolEventFlag != -1) {
		sceKernelCancelEventFlag(s_wolEventFlag, 0, NULL);
		sceKernelDeleteEventFlag(s_wolEventFlag);
		s_wolEventFlag = -1;
	}
	sceKernelDeleteLwMutex(&s_mutex);
	return 0;
}

SceInt32 TeleportClient::registerCallback(SceTeleportClientGetDeviceInfoCallback func)
{
	if (callback != NULL)
		return SCE_TELEPORT_ERROR_PARAM;
	callback = func;
	return 0;
}

SceInt32 TeleportClient::createSocket(int *outSock, const char *ipAddr, SceUInt16 port, int sockType)
{
	int ret = 0;
	SceNetSockaddrIn addr;
	addr.sin_len = 0;
	memset(&addr.sin_addr, 0, 12);
	addr.sin_port = sceNetHtons(port);
	addr.sin_family = SCE_NET_AF_INET;

	int sockFd;
	if (sockType == SCE_NET_SOCK_DGRAM) {
		sockFd = sceNetSocket("SceTeleportUdpClient", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	} else {
		sockFd = sceNetSocket("SceTeleportTcpClient", SCE_NET_AF_INET, sockType, 0);
		sceNetInetPton(SCE_NET_AF_INET, ipAddr, &addr.sin_addr);
		ret = sceNetConnect(sockFd, (const SceNetSockaddr *)&addr, sizeof(addr));
		if (ret >= 0) {
			int keepAlive = 1;
			ret = sceNetSetsockopt(sockFd, SCE_NET_SOL_SOCKET, SCE_NET_SO_KEEPALIVE,
					       &keepAlive, sizeof(keepAlive));
		}
	}

	if (ret >= 0 && sockType == SCE_NET_SOCK_DGRAM) {
		SceNetIpMreq mreq;
		sceNetInetPton(SCE_NET_AF_INET, ipAddr, &mreq.imr_multiaddr);
		mreq.imr_interface.s_addr = 0;
		ret = sceNetSetsockopt(sockFd, SCE_NET_IPPROTO_IP, SCE_NET_IP_ADD_MEMBERSHIP,
				       &mreq, sizeof(mreq));
		if (ret >= 0) {
			int ttl = 1;
			ret = sceNetSetsockopt(sockFd, SCE_NET_IPPROTO_IP, SCE_NET_IP_MULTICAST_TTL,
					       &ttl, sizeof(ttl));
		}
		if (ret >= 0) {
			int loop = 0;
			ret = sceNetSetsockopt(sockFd, SCE_NET_IPPROTO_IP, SCE_NET_IP_MULTICAST_LOOP,
					       &loop, sizeof(loop));
		}
	}

	if (ret >= 0) {
		if (sockFd >= 0)
			*outSock = sockFd;
	} else if (sockFd >= 0) {
		sceNetSocketAbort(sockFd, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(sockFd);
	}
	return ret;
}

SceInt32 TeleportClient::startSearchDevice()
{
	SceUID recvThid = searchRecvThreadId;
	SceUID sendThid = searchSendThreadId;
	SceInt32 ret = 0;

	if (callback == NULL) {
		ret = SCE_TELEPORT_ERROR_STATE;
	} else {
		if (s_udpSocket == -1) {
			int sockFd = -1;
			char addrStr[16];
			memset(addrStr, 0, sizeof(addrStr));
			sceClibSnprintf(addrStr, sizeof(addrStr), TELEPORT_MULTICAST_ADDR);
			ret = createSocket(&sockFd, addrStr, TELEPORT_MULTICAST_PORT, SCE_NET_SOCK_DGRAM);
			if (ret < 0) {
				s_udpSocket = -1;
				ret = SCE_TELEPORT_ERROR_COMMUNICATION;
			} else {
				s_udpSocket = sockFd;
			}
		}

		if (ret >= 0 && recvThid == 0) {
			recvThid = sceKernelCreateThread("SceTeleportStartSearch",
							 searchRecvThread,
							 TELEPORT_THREAD_PRIORITY,
							 0x2000, 0, 0, NULL);
			if (recvThid < 0)
				ret = SCE_TELEPORT_ERROR_FATAL;
		}
		if (ret >= 0 && sendThid == 0) {
			sendThid = sceKernelCreateThread("SceTeleportSendSeachCommand",
							 searchSendThread,
							 TELEPORT_THREAD_PRIORITY,
							 0x1000, 0, 0, NULL);
			if (sendThid < 0)
				ret = SCE_TELEPORT_ERROR_FATAL;
		}
		if (ret >= 0 && sendThid != 0) {
			sceKernelLockLwMutex(&s_mutex, 1, NULL);
			bool running = (s_threadRunningMask & TELEPORT_THREAD_SEND_BIT) != 0;
			sceKernelUnlockLwMutex(&s_mutex, 1);
			if (!running) {
				ret = sceKernelStartThread(sendThid, sizeof(*this), this);
				if (ret < 0) {
					ret = SCE_TELEPORT_ERROR_FATAL;
				} else {
					sceKernelLockLwMutex(&s_mutex, 1, NULL);
					s_threadRunningMask |= TELEPORT_THREAD_SEND_BIT;
					sceKernelUnlockLwMutex(&s_mutex, 1);
					searchSendThreadId = sendThid;
				}
			}
		}
		if (ret >= 0 && recvThid != 0) {
			sceKernelLockLwMutex(&s_mutex, 1, NULL);
			bool running = (s_threadRunningMask & TELEPORT_THREAD_RECV_BIT) != 0;
			sceKernelUnlockLwMutex(&s_mutex, 1);
			if (!running) {
				ret = sceKernelStartThread(recvThid, sizeof(*this), this);
				if (ret < 0) {
					ret = SCE_TELEPORT_ERROR_FATAL;
				} else {
					sceKernelLockLwMutex(&s_mutex, 1, NULL);
					s_threadRunningMask |= TELEPORT_THREAD_RECV_BIT;
					sceKernelUnlockLwMutex(&s_mutex, 1);
					searchRecvThreadId = recvThid;
				}
			}
		}
		if (ret == 0)
			return 0;
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_searchStopFlag = 1;
	sceKernelUnlockLwMutex(&s_mutex, 1);

	if (s_searchEventFlag != -1)
		sceKernelCancelEventFlag(s_searchEventFlag, 0, NULL);
	if (s_wolEventFlag != -1)
		sceKernelCancelEventFlag(s_searchEventFlag, 0, NULL);

	if (s_udpSocket != 0) {
		int sockFd = s_udpSocket;
		sceNetSocketAbort(sockFd, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(sockFd);
		s_udpSocket = -1;
	}
	if (recvThid != 0) {
		sceKernelWaitThreadEnd(recvThid, NULL, NULL);
		sceKernelDeleteThread(recvThid);
		searchRecvThreadId = 0;
	}
	if (sendThid != 0) {
		sceKernelWaitThreadEnd(sendThid, NULL, NULL);
		sceKernelDeleteThread(sendThid);
		searchSendThreadId = 0;
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_searchStopFlag = 0;
	sceKernelUnlockLwMutex(&s_mutex, 1);

	s_discoveredWriteIdx = 0;
	if (s_discoveredDevices != NULL) {
		memset(s_discoveredDevices, 0,
			      sizeof(TeleportDiscoveredDevice) * TELEPORT_DISCOVERED_MAX);
	}
	return ret;
}

SceInt32 TeleportClient::endSearchDevice()
{
	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_searchStopFlag = 1;
	sceKernelUnlockLwMutex(&s_mutex, 1);

	if (s_searchEventFlag != -1)
		sceKernelCancelEventFlag(s_searchEventFlag, 0, NULL);
	if (s_wolEventFlag != -1)
		sceKernelCancelEventFlag(s_searchEventFlag, 0, NULL);

	if (s_udpSocket != 0) {
		int sockFd = s_udpSocket;
		sceNetSocketAbort(sockFd, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(sockFd);
		s_udpSocket = -1;
	}
	if (searchRecvThreadId != 0) {
		sceKernelWaitThreadEnd(searchRecvThreadId, NULL, NULL);
		sceKernelDeleteThread(searchRecvThreadId);
		searchRecvThreadId = 0;
	}
	if (searchSendThreadId != 0) {
		sceKernelWaitThreadEnd(searchSendThreadId, NULL, NULL);
		sceKernelDeleteThread(searchSendThreadId);
		searchSendThreadId = 0;
	}
	if (wolThreadId != 0) {
		sceKernelWaitThreadEnd(wolThreadId, NULL, NULL);
		sceKernelDeleteThread(wolThreadId);
		wolThreadId = 0;
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_searchStopFlag = 0;
	sceKernelUnlockLwMutex(&s_mutex, 1);

	s_discoveredWriteIdx = 0;
	if (s_discoveredDevices != NULL) {
		memset(s_discoveredDevices, 0,
			      sizeof(TeleportDiscoveredDevice) * TELEPORT_DISCOVERED_MAX);
	}
	return 0;
}

SceInt32 TeleportClient::appendCachedDevice(const SceTeleportDeviceInfo *info,
					    const SceTeleportDeviceDescription *desc)
{
	SceUInt32 oldBytes = (SceUInt32)(sizeof(TeleportCachedDevice) * cachedDeviceCount);
	SceUInt32 newBytes = (SceUInt32)(sizeof(TeleportCachedDevice) * (cachedDeviceCount + 1));
	TeleportCachedDevice *oldArr = cachedDevices;
	TeleportCachedDevice *newArr = NULL;

	if (oldArr != NULL) {
		if (newBytes != 0) {
			newArr = (TeleportCachedDevice *)allocFunc(newBytes);
			if (newArr != NULL) {
				sceClibMemcpyChk(newArr, oldArr, oldBytes, newBytes);
				freeFunc(oldArr);
			}
		} else {
			freeFunc(oldArr);
		}
	} else {
		newArr = (TeleportCachedDevice *)allocFunc(newBytes);
	}

	cachedDevices = newArr;
	TeleportCachedDevice *dst = &cachedDevices[cachedDeviceCount];
	sceClibStrncpy(asChar(dst->info.deviceId), asConstChar(info->deviceId),
		       sizeof(dst->info.deviceId));
	sceClibStrncpy(asChar(cachedDevices[cachedDeviceCount].info.ipAddress),
		       asConstChar(info->ipAddress), sizeof(info->ipAddress));
	sceClibMemcpy_safe(cachedDevices[cachedDeviceCount].info.reserved,
			   info->reserved, sizeof(info->reserved));
	cachedDevices[cachedDeviceCount].info.innerData = info->innerData;

	sceClibStrncpy(asChar(cachedDevices[cachedDeviceCount].desc.name),
		       asConstChar(desc->name), sizeof(desc->name));
	sceClibStrncpy(asChar(cachedDevices[cachedDeviceCount].desc.innerData),
		       asConstChar(desc->innerData), sizeof(desc->innerData));
	sceClibMemcpy_safe(cachedDevices[cachedDeviceCount].desc.reserved,
			   desc->reserved, sizeof(desc->reserved));
	cachedDevices[cachedDeviceCount].desc.protocolVersion = desc->protocolVersion;
	cachedDevices[cachedDeviceCount].desc.systemVersion = desc->systemVersion;
	++cachedDeviceCount;
	return 0;
}

int TeleportClient::splitString(const char *src, const char *delim, char *outRows,
				int rowMaxLen, int maxRows)
{
	SceSize pos = 0;
	int rowCount = 0;
	const char *cursor = src;
	char *rowDst = outRows;

	for (;;) {
		char *found = sceClibStrstr(cursor, delim);
		int segLen = (found != NULL) ? (int)(found - cursor) : -1;
		if (segLen == -1)
			break;
		if (segLen > 0) {
			if (rowCount < maxRows && segLen < rowMaxLen) {
				sceClibStrncpy(rowDst, cursor, (SceSize)segLen);
				rowDst += 128;
				++rowCount;
			}
			pos += (SceSize)segLen + sceClibStrnlen(delim, 8) - 1;
		}
		++pos;
		cursor = src + pos;
	}

	if (sceClibStrnlen(cursor, (SceSize)rowMaxLen) != 0 && rowCount < maxRows) {
		sceClibStrncpy(outRows + 128 * rowCount, cursor, (SceSize)rowMaxLen);
		++rowCount;
	}
	return rowCount;
}

unsigned int TeleportClient::parseContentLength(const char *response, unsigned int receivedLen)
{
	char lenDigits[16];
	char headerBuf[128];
	char upperBuf[128];
	char tokens[3][128];
	char lines[5][128];

	SceSize keyLen = sceClibStrnlen("CONTENT-LENGTH:", 32);
	memset(headerBuf, 0, sizeof(headerBuf));
	memset(upperBuf, 0, sizeof(upperBuf));
	memset(lenDigits, 0, 10);

	int defaultLen = (receivedLen < 128) ? (int)receivedLen : 127;
	int headerLen;
	char *sep = sceClibStrstr(response, "\r\n\r\n");
	if (sep != NULL) {
		headerLen = (int)(sep - response) + (int)sceClibStrnlen("\r\n\r\n", 4);
		if (headerLen > 128 || headerLen < 0)
			return receivedLen;
	} else {
		headerLen = defaultLen;
	}

	sceClibStrncpy(headerBuf, response, (SceSize)headerLen);
	for (unsigned int i = 0; i < sizeof(headerBuf); ++i) {
		if (headerBuf[i] == '\0')
			break;
		upperBuf[i] = (char)sceClibToupper((int)(signed char)headerBuf[i]);
	}

	int contentLen = 0;
	memset(lines, 0, sizeof(lines));
	int lineCount = splitString(upperBuf, "\r\n", lines[0], 128, 5);
	for (int i = 0; i < lineCount; ++i) {
		if (sceClibStrncasecmp(lines[i], "CONTENT-LENGTH:", keyLen) == 0) {
			memset(tokens, 0, sizeof(tokens));
			splitString(lines[i], " ", tokens[0], 128, 3);
			for (int k = 0; k < 10; ++k) {
				int ch = (int)(signed char)tokens[1][k];
				if (ch < '0' || ch > '9')
					break;
				lenDigits[k] = (char)ch;
			}
			contentLen = parseSignedDecimal(lenDigits, 8);
		}
	}

	if ((unsigned int)(headerLen + contentLen) <= TELEPORT_IO_BUFFER_SIZE)
		return (unsigned int)(headerLen + contentLen);
	return receivedLen;
}

SceInt32 TeleportClient::sendAndReceive(int sockFd, const char *sendBuf, SceSize sendMaxLen,
					char *recvBuf, unsigned int recvBufSize, SceInt32 timeout)
{
	int sendTimeout = (timeout > 0) ? timeout : TELEPORT_DEFAULT_TIMEOUT_US;
	if (sceNetSetsockopt(sockFd, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
			     &sendTimeout, sizeof(sendTimeout)) < 0)
		return SCE_TELEPORT_ERROR_COMMUNICATION;

	SceInt64 startTime = sceKernelGetSystemTimeWide();
	SceSize sendLen = sceClibStrnlen(sendBuf, sendMaxLen);
	int sent = sceNetSend(sockFd, sendBuf, sendLen, 0);
	if (sent == 35)
		return SCE_TELEPORT_ERROR_CONNECTION_TIMEOUT;
	if (sent < 0)
		return SCE_TELEPORT_ERROR_COMMUNICATION;

	int elapsed = (int)(sceKernelGetSystemTimeWide() - startTime);
	int remTimeout = sendTimeout - elapsed;
	if (remTimeout <= 0)
		return SCE_TELEPORT_ERROR_CONNECTION_TIMEOUT;

	if (sceNetSetsockopt(sockFd, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
			     &remTimeout, sizeof(remTimeout)) < 0)
		return SCE_TELEPORT_ERROR_COMMUNICATION;

	memset(recvBuf, 0, recvBufSize);
	int totalReceived = 0;
	int expectedLen = 0;
	do {
		int avail = sceNetRecv(sockFd, NULL, recvBufSize - (unsigned int)totalReceived,
				       SCE_NET_MSG_PEEKLEN);
		if (avail < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		if ((unsigned int)(avail + totalReceived) > recvBufSize)
			break;
		int chunk = sceNetRecv(sockFd, recvBuf + totalReceived, (unsigned int)avail,
				       SCE_NET_MSG_WAITALL);
		if (chunk == 35)
			return SCE_TELEPORT_ERROR_CONNECTION_TIMEOUT;
		if (chunk < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		if (totalReceived == 0)
			expectedLen = (int)parseContentLength(recvBuf, (unsigned int)chunk);
		totalReceived += chunk;
	} while (totalReceived < expectedLen);

	return 0;
}

SceInt32 TeleportClient::storeDeviceSocket(const char *deviceId, int sockFd)
{
	for (SceInt32 i = 0; i < deviceSocketCount; ++i) {
		if (sceClibStrcmp(deviceId, deviceSockets[i].deviceId) == 0)
			deviceSockets[i].sockFd = sockFd;
	}

	SceUInt32 oldBytes = (SceUInt32)(sizeof(TeleportDeviceSocket) * deviceSocketCount);
	SceUInt32 newBytes = (SceUInt32)(sizeof(TeleportDeviceSocket) * (deviceSocketCount + 1));
	TeleportDeviceSocket *oldArr = deviceSockets;
	TeleportDeviceSocket *newArr = NULL;

	if (oldArr != NULL) {
		if (newBytes != 0) {
			newArr = (TeleportDeviceSocket *)allocFunc(newBytes);
			if (newArr != NULL) {
				sceClibMemcpyChk(newArr, oldArr, oldBytes, newBytes);
				freeFunc(oldArr);
			}
		} else {
			freeFunc(oldArr);
		}
	} else {
		newArr = (TeleportDeviceSocket *)allocFunc(newBytes);
	}

	deviceSockets = newArr;
	sceClibStrncpy(deviceSockets[deviceSocketCount].deviceId, deviceId, 18);
	deviceSockets[deviceSocketCount].sockFd = sockFd;
	++deviceSocketCount;
	return 0;
}

int TeleportClient::findDeviceSocket(const char *deviceId, int *outSock)
{
	SceNetSockInfo sockInfo;
	for (SceInt32 i = 0; i < cachedDeviceCount; ++i) {
		if (sceClibStrcmp(deviceId, deviceSockets[i].deviceId) != 0)
			continue;

		sceNetGetSockInfo(deviceSockets[i].sockFd, &sockInfo, 1, 0);
		bool portMismatch = false;
		for (int j = 0; j < TELEPORT_DISCOVERED_MAX; ++j) {
			if (sceClibStrcmp(deviceId, s_discoveredDevices[j].deviceId) == 0 &&
			    sceNetHtons(sockInfo.remote_port) != s_discoveredDevices[j].port) {
				portMismatch = true;
				break;
			}
		}
		if (portMismatch)
			return 0;
		if (sockInfo.state == SCE_NET_SOCKINFO_STATE_ESTABLISHED) {
			*outSock = deviceSockets[i].sockFd;
			return 1;
		}
	}
	return 0;
}

SceInt32 TeleportClient::fetchTeleportXml(const SceTeleportDeviceInfo *device,
					  char *recvBuf, unsigned int recvBufSize, SceInt32 timeout)
{
	char reqBuf[256];
	int sockFd;
	memset(reqBuf, 0, sizeof(reqBuf));

	if (findDeviceSocket(asConstChar(device->deviceId), &sockFd) == 0) {
		int foundIdx = -1;
		for (int i = 0; i < TELEPORT_DISCOVERED_MAX; ++i) {
			if (sceClibStrcmp(asConstChar(device->ipAddress),
					  s_discoveredDevices[i].ipAddress) == 0) {
				foundIdx = i;
				break;
			}
		}
		if (foundIdx < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		if (createSocket(&sockFd, asConstChar(device->ipAddress),
				 s_discoveredDevices[foundIdx].port, SCE_NET_SOCK_STREAM) < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		storeDeviceSocket(asConstChar(device->deviceId), sockFd);
	}

	memset(reqBuf, 0, sizeof(reqBuf));
	sceClibSnprintf(reqBuf, sizeof(reqBuf), "GET /teleport.xml HTTP/1.1\r\n\r\n");
	return sendAndReceive(sockFd, reqBuf, sizeof(reqBuf), recvBuf, recvBufSize, timeout);
}

SceInt32 TeleportClient::getDeviceDescription(const SceTeleportDeviceInfo *device,
					      SceTeleportDeviceDescription *desc, SceUInt32 timeout)
{
	memset(s_recvBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	SceInt32 ret = fetchTeleportXml(device, s_recvBuf, TELEPORT_IO_BUFFER_SIZE, (SceInt32)timeout);
	if (ret < 0)
		return ret;

	TeleportXmlParser parser(initInfo);
	char *buf = s_recvBuf;
	SceSize len = sceClibStrnlen(buf, TELEPORT_IO_BUFFER_SIZE);
	if (parser.parseXml(buf, (int)(len + 1), TELEPORT_XML_MODE_DEVICE_DESC, desc, NULL) < 0)
		return SCE_TELEPORT_ERROR_XML_PARSE;

	bool alreadyCached = false;
	for (SceInt32 i = 0; i < cachedDeviceCount; ++i) {
		if (sceClibStrcmp(asConstChar(device->deviceId),
				  asConstChar(cachedDevices[i].info.deviceId)) == 0 &&
		    sceClibStrnlen(asConstChar(cachedDevices[i].desc.name), 32) != 0) {
			alreadyCached = true;
			break;
		}
	}
	if (!alreadyCached)
		appendCachedDevice(device, desc);
	return 0;
}

SceInt32 TeleportClient::getRemoteAppInfoNum(const SceTeleportDeviceInfo *device,
					     SceTeleportRemoteAppInfoNum *infoNum, SceUInt32 timeout)
{
	memset(s_recvBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	SceInt32 ret = fetchTeleportXml(device, s_recvBuf, TELEPORT_IO_BUFFER_SIZE, (SceInt32)timeout);
	if (ret < 0)
		return ret;

	TeleportXmlParser parser(initInfo);
	char *buf = s_recvBuf;
	SceSize len = sceClibStrnlen(buf, TELEPORT_IO_BUFFER_SIZE);
	if (parser.parseXml(buf, (int)(len + 1), TELEPORT_XML_MODE_APP_NUM, infoNum, NULL) < 0)
		return SCE_TELEPORT_ERROR_XML_PARSE;
	return 0;
}

SceInt32 TeleportClient::getRemoteAppInfo(const SceTeleportDeviceInfo *device, SceUInt16 size,
					  SceTeleportRemoteAppInfo **info, SceUInt32 timeout)
{
	(void)size;
	memset(s_recvBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	SceInt32 ret = fetchTeleportXml(device, s_recvBuf, TELEPORT_IO_BUFFER_SIZE, (SceInt32)timeout);
	if (ret < 0)
		return ret;

	TeleportXmlParser parser(initInfo);
	TeleportRemoteAppList appList;
	sceClibMemset(&appList, 0, sizeof(appList));
	char *buf = s_recvBuf;
	SceSize len = sceClibStrnlen(buf, TELEPORT_IO_BUFFER_SIZE);
	if (parser.parseXml(buf, (int)(len + 1), TELEPORT_XML_MODE_APP_LIST, &appList, NULL) < 0) {
		if (appList.items != NULL) {
			freeFunc(appList.items);
			appList.items = NULL;
		}
		return SCE_TELEPORT_ERROR_XML_PARSE;
	}

	for (SceInt32 i = 0; i < appList.parsedCount; ++i) {
		sceClibSnprintf(asChar((*info)[i].name), sizeof((*info)[i].name),
				"%s", appList.items[i].name);
		sceClibSnprintf(asChar((*info)[i].appId), sizeof((*info)[i].appId),
				"%s", appList.items[i].appId);
		(*info)[i].appVersion = appList.items[i].appVersion;
	}

	if (appList.items != NULL) {
		freeFunc(appList.items);
		appList.items = NULL;
	}
	return 0;
}

SceInt32 TeleportClient::saveDeviceCache(const SceTeleportDeviceInfo *device, const char *passCode)
{
	SceInt32 foundIdx = -1;
	for (SceInt32 i = 0; i < cachedDeviceCount; ++i) {
		if (sceClibStrcmp(asConstChar(device->deviceId),
				  asConstChar(cachedDevices[i].info.deviceId)) == 0 &&
		    sceClibStrnlen(asConstChar(cachedDevices[i].desc.name), 32) != 0) {
			foundIdx = i;
			break;
		}
	}
	if (foundIdx < 0)
		return -1;

	const char *devName = asConstChar(cachedDevices[foundIdx].desc.name);
	SceSize nameLen = sceClibStrnlen(devName, 32) + 1;
	sceRegMgrSystemParamSetStr(TELEPORT_REG_DEVICE_NAME, devName, nameLen);

	SceUInt8 macBin[32];
	memset(macBin, 0, sizeof(macBin));
	const char *id = asConstChar(cachedDevices[foundIdx].info.deviceId);
	for (int b = 0; b < 6; ++b) {
		SceUInt8 hi = parseLowerHexNibble(id[b * 3]);
		SceUInt8 lo = parseLowerHexNibble(id[b * 3 + 1]);
		macBin[b] = (SceUInt8)((hi << 4) + lo);
	}
	sceRegMgrSystemParamSetBin(TELEPORT_REG_DEVICE_MAC, macBin, sizeof(macBin));
	sceRegMgrSystemParamSetInt(TELEPORT_REG_INITIAL_TARGET, 1);

	char passBuf[32];
	memset(passBuf, 0, sizeof(passBuf));
	passBuf[0] = passCode[0];
	passBuf[1] = passCode[1];
	passBuf[2] = passCode[2];
	passBuf[3] = passCode[3];
	sceRegMgrSystemParamSetStr(TELEPORT_REG_PASSCODE, passBuf, sizeof(passBuf));
	return 0;
}

unsigned int TeleportClient::decodeHex(const char *hexStr, unsigned int hexLen,
				       SceUInt8 *outBuf, unsigned int outMax)
{
	unsigned int written = 0;
	for (unsigned int i = 0; i < hexLen && written < outMax; ++i) {
		if ((int)i % 2 == 0) {
			int hi = (int)(signed char)hexStr[i];
			if (hi >= 'A' && hi <= 'F')
				outBuf[written] = (SceUInt8)(((hi - 55) & 0xF) << 4);
			else if (hi >= '0' && hi <= '9')
				outBuf[written] = (SceUInt8)(((hi - '0') & 0xF) << 4);

			int lo = (int)(signed char)hexStr[i + 1];
			if (lo >= 'A' && lo <= 'F')
				outBuf[written] = (SceUInt8)(outBuf[written] + ((lo - 55) & 0xF));
			else if (lo >= '0' && lo <= '9')
				outBuf[written] = (SceUInt8)(outBuf[written] + ((lo - '0') & 0xF));
			++written;
		}
	}
	return written;
}

int TeleportClient::escapeXml(const char *src, int srcLen, char *dst, int dstMax, SceChar8 measureOnly)
{
	int outPos = 0;
	int measured = 0;
	int i = 0;

	for (; i < srcLen && outPos < dstMax; ++i) {
		int ch = (int)(signed char)src[i];
		if (measureOnly == 1)
			outPos = 0;

		int n;
		if (ch == '<') {
			n = sceClibSnprintf(dst + outPos, (SceSize)(dstMax - outPos), "&lt;");
			if (measureOnly == 1)
				measured += n;
			else
				outPos += n;
		} else if (ch == '>') {
			n = sceClibSnprintf(dst + outPos, (SceSize)(dstMax - outPos), "&gt;");
			if (measureOnly == 1)
				measured += n;
			else
				outPos += n;
		} else if (ch == '&') {
			n = sceClibSnprintf(dst + outPos, (SceSize)(dstMax - outPos), "&amp;");
			if (measureOnly == 1)
				measured += n;
			else
				outPos += n;
		} else if (ch == '"') {
			n = sceClibSnprintf(dst + outPos, (SceSize)(dstMax - outPos), "&quot;");
			if (measureOnly == 1)
				measured += n;
			else
				outPos += n;
		} else if (ch == '\'') {
			n = sceClibSnprintf(dst + outPos, (SceSize)(dstMax - outPos), "&apos;");
			if (measureOnly == 1)
				measured += n;
			else
				outPos += n;
		} else {
			if (measureOnly == 1)
				++measured;
			else
				dst[outPos++] = (char)ch;
		}
	}

	if (i != srcLen)
		return -1;
	return (measureOnly == 1) ? measured : 0;
}

SceInt32 TeleportClient::launchRemoteApp(const SceTeleportDeviceInfo *device,
					 const SceTeleportRemoteAppInfo *appInfo,
					 const SceTeleportRemoteAppArg *argInfo,
					 SceTeleportLaunchResponse *res,
					 int argLen, SceUInt32 timeout)
{
	SceTeleportDeviceDescription desc;
	SceInt32 ret = getDeviceDescription(device, &desc, timeout);
	if (ret != 0)
		return ret;

	char scratchBuf[16];
	memset(scratchBuf, 0, sizeof(scratchBuf));
	int escapedSize = escapeXml(asConstChar(argInfo->arg), argLen,
				    scratchBuf, sizeof(scratchBuf), 1) + 1;
	char *escapedArg = (char *)allocFunc((SceUInt32)escapedSize);
	if (escapedArg == NULL)
		return SCE_TELEPORT_ERROR_NO_MEMORY;

	memset(escapedArg, 0, (SceSize)escapedSize);
	if (escapeXml(asConstChar(argInfo->arg), argLen, escapedArg, escapedSize, 0) != 0)
		return SCE_TELEPORT_ERROR_NO_MEMORY;

	memset(s_recvBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);

	int sockFd;
	if (findDeviceSocket(asConstChar(device->deviceId), &sockFd) == 0) {
		int foundIdx = -1;
		for (int i = 0; i < TELEPORT_DISCOVERED_MAX; ++i) {
			if (sceClibStrcmp(asConstChar(device->ipAddress),
					  s_discoveredDevices[i].ipAddress) == 0) {
				foundIdx = i;
				break;
			}
		}
		if (foundIdx < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		if (createSocket(&sockFd, asConstChar(device->ipAddress),
				 s_discoveredDevices[foundIdx].port, SCE_NET_SOCK_STREAM) > 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		storeDeviceSocket(asConstChar(device->deviceId), sockFd);
	}

	if (argInfo->passCode[0] != '\0') {
		char authToken[68];
		memset(authToken, 0, 65);

		char passKey[4];
		passKey[0] = argInfo->passCode[0];
		passKey[1] = argInfo->passCode[1];
		passKey[2] = argInfo->passCode[2];
		passKey[3] = argInfo->passCode[3];

		SceUInt8 outerDigest[32];
		SceUInt8 innerDigest[32];
		SceUInt8 saltBytes[16];
		SceUInt8 saltCopy[16];
		memset(outerDigest, 0, sizeof(outerDigest));
		memset(innerDigest, 0, sizeof(innerDigest));
		memset(saltBytes, 0, sizeof(saltBytes));
		decodeHex(asConstChar(desc.innerData), 32, saltBytes, sizeof(saltBytes));
		memset(saltCopy, 0, sizeof(saltCopy));
		for (int i = 0; i < 16; ++i)
			saltCopy[i] = saltBytes[i];

		sceHmacSha256Digest(saltCopy, sizeof(saltCopy), passKey, sizeof(passKey), innerDigest);
		sceHmacSha256Digest(innerDigest, sizeof(innerDigest), passKey, sizeof(passKey), outerDigest);

		char hexChars[16];
		memcpy(hexChars, "0123456789ABCDEF", sizeof(hexChars));
		unsigned int dstPos = 0;
		for (unsigned int i = 0; i < sizeof(outerDigest) && dstPos + 1 < 65; ++i) {
			authToken[dstPos] = hexChars[outerDigest[i] >> 4];
			authToken[dstPos + 1] = hexChars[outerDigest[i] & 0xFu];
			dstPos += 2;
		}

		if (argInfo->option == SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH) {
			int bodyLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri><option>0x%08X</option></app></teleport>",
				appInfo->appId, escapedArg, SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH);
			memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);
			int hdrLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"POST /\r\nContent-Length: %d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nAuthorization: Teleport %s\r\n\r\n",
				bodyLen, authToken);
			sceClibSnprintf(
				s_sendBuf + hdrLen, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri><option>0x%08X</option></app></teleport>",
				appInfo->appId, escapedArg, SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH);
		} else {
			int bodyLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri></app></teleport>",
				appInfo->appId, escapedArg);
			memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);
			int hdrLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"POST /\r\nContent-Length: %d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\nAuthorization: Teleport %s\r\n\r\n",
				bodyLen, authToken);
			sceClibSnprintf(
				s_sendBuf + hdrLen, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri></app></teleport>",
				appInfo->appId, escapedArg);
		}
	} else {
		if (argInfo->option == SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH) {
			int bodyLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri><option>0x%08X</option></app></teleport>",
				appInfo->appId, escapedArg, SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH);
			memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);
			int hdrLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"POST /\r\nContent-Length: %d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n\r\n",
				bodyLen);
			sceClibSnprintf(
				s_sendBuf + hdrLen, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri><option>0x%08X</option></app></teleport>",
				appInfo->appId, escapedArg, SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH);
		} else {
			int bodyLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri></app></teleport>",
				appInfo->appId, escapedArg);
			memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);
			int hdrLen = sceClibSnprintf(
				s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				"POST /\r\nContent-Length: %d\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n\r\n",
				bodyLen);
			sceClibSnprintf(
				s_sendBuf + hdrLen, TELEPORT_IO_BUFFER_SIZE,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s%s</uri></app></teleport>",
				appInfo->appId, escapedArg);
		}
	}

	SceInt32 status = sendAndReceive(sockFd, s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
					 s_recvBuf, TELEPORT_IO_BUFFER_SIZE, (SceInt32)timeout);
	if (status == 0) {
		int httpStatus = TeleportXmlParser::parseHttpStatusCode(s_recvBuf, TELEPORT_IO_BUFFER_SIZE);
		if (httpStatus == 200 || httpStatus == 201 || httpStatus == 403) {
			TeleportXmlParser parser(initInfo);
			SceUInt32 serverErr = 0;
			SceSize respLen = sceClibStrnlen(s_recvBuf, TELEPORT_IO_BUFFER_SIZE);
			parser.parseXml(s_recvBuf, (int)(respLen + 1),
					TELEPORT_XML_MODE_LAUNCH_RESP, res, &serverErr);
			if (httpStatus == 200 || httpStatus == 201)
				saveDeviceCache(device, asConstChar(argInfo->passCode));
			res->status = (httpStatus == 201)
					? SCE_TELEPORT_CLIENT_RESPONSE_STATUS_ALREADY_LAUNCHED
					: SCE_TELEPORT_CLIENT_RESPONSE_STATUS_DEFAULT;
			if (httpStatus == 403) {
				status = SCE_TELEPORT_ERROR_LAUNCH_FAILED;
				if (serverErr > 0x80108400u && serverErr <= 0x80108438u)
					status = (SceInt32)serverErr;
			}
		} else {
			status = (httpStatus == 401)
				   ? SCE_TELEPORT_ERROR_LAUNCH_FAILED_INVALID_PASSCODE
				   : SCE_TELEPORT_ERROR_LAUNCH_FAILED;
		}
	}

	if (escapedArg != NULL)
		freeFunc(escapedArg);
	return status;
}

SceInt32 TeleportClient::deleteRemoteApp(const SceTeleportDeviceInfo *device,
					 const SceTeleportRemoteAppInfo *appInfo, SceUInt32 timeout)
{
	memset(s_recvBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);

	int sockFd;
	if (findDeviceSocket(asConstChar(device->deviceId), &sockFd) == 0) {
		int foundIdx = -1;
		for (int i = 0; i < TELEPORT_DISCOVERED_MAX; ++i) {
			if (sceClibStrcmp(asConstChar(device->ipAddress),
					  s_discoveredDevices[i].ipAddress) == 0) {
				foundIdx = i;
				break;
			}
		}
		if (foundIdx < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		if (createSocket(&sockFd, asConstChar(device->ipAddress),
				 s_discoveredDevices[foundIdx].port, SCE_NET_SOCK_STREAM) < 0)
			return SCE_TELEPORT_ERROR_COMMUNICATION;
		storeDeviceSocket(asConstChar(device->deviceId), sockFd);
	}

	int bodyLen = sceClibSnprintf(
		s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s</uri></app></teleport>",
		appInfo->appId);
	memset(s_sendBuf, 0, TELEPORT_IO_BUFFER_SIZE);
	int hdrLen = sceClibSnprintf(
		s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
		"DELETE /\r\nContent-Length: %d\r\nContent-Type: text/xml; charset=utf-8\r\n\r\n",
		bodyLen);
	sceClibSnprintf(
		s_sendBuf + hdrLen, TELEPORT_IO_BUFFER_SIZE,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?><teleport><app><uri>%s</uri></app></teleport>",
		appInfo->appId);

	SceInt32 ret = sendAndReceive(sockFd, s_sendBuf, TELEPORT_IO_BUFFER_SIZE,
				      s_recvBuf, TELEPORT_IO_BUFFER_SIZE, (SceInt32)timeout);
	if (ret != 0)
		return ret;

	int httpStatus = TeleportXmlParser::parseHttpStatusCode(s_recvBuf, TELEPORT_IO_BUFFER_SIZE);
	if (httpStatus == 200) {
		sceRegMgrSystemParamSetInt(TELEPORT_REG_INITIAL_TARGET, 0);
		return 0;
	}
	return (httpStatus == 403)
		? SCE_TELEPORT_ERROR_DELETE_FAILED_FORBIDDEN
		: SCE_TELEPORT_ERROR_DELETE_FAILED;
}

SceInt32 TeleportClient::wakeupLatestDevice(const SceTeleportWakeupInfo *info)
{
	(void)info;
	SceUID thid = wolThreadId;
	SceInt32 ret = 0;

	SceUInt8 macBin[32];
	sceRegMgrSystemParamGetBin(TELEPORT_REG_DEVICE_MAC, macBin, sizeof(macBin));
	bool hasMac = false;
	for (int i = 0; i < 6; ++i) {
		if (macBin[i] != 0) {
			hasMac = true;
			break;
		}
	}

	if (!hasMac) {
		ret = SCE_TELEPORT_ERROR_STATE;
	} else {
		if (thid == 0) {
			thid = sceKernelCreateThread("SceTeleportSendWOL",
						     wolSendThread,
						     TELEPORT_THREAD_PRIORITY,
						     0x1000, 0, 0, NULL);
			if (thid < 0)
				ret = SCE_TELEPORT_ERROR_FATAL;
		}
		if (ret >= 0) {
			if (thid == 0)
				return 0;
			sceKernelLockLwMutex(&s_mutex, 1, NULL);
			bool running = (s_threadRunningMask & TELEPORT_THREAD_WOL_BIT) != 0;
			sceKernelUnlockLwMutex(&s_mutex, 1);
			if (running)
				return 0;

			if (sceKernelStartThread(thid, sizeof(*this), this) >= 0) {
				sceKernelLockLwMutex(&s_mutex, 1, NULL);
				s_threadRunningMask |= TELEPORT_THREAD_WOL_BIT;
				sceKernelUnlockLwMutex(&s_mutex, 1);
				wolThreadId = thid;

				SceInt32 searchRet = startSearchDevice();
				if (searchRet >= 0)
					return 0;
				ret = searchRet;
			} else {
				ret = SCE_TELEPORT_ERROR_FATAL;
			}
		}
	}

	if (s_wolEventFlag != -1)
		sceKernelCancelEventFlag(s_wolEventFlag, 0, NULL);
	if (thid != 0) {
		sceKernelWaitThreadEnd(thid, NULL, NULL);
		sceKernelDeleteThread(thid);
		wolThreadId = 0;
	}
	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_threadRunningMask &= ~TELEPORT_THREAD_WOL_BIT;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return ret;
}

SceInt32 TeleportClient::getCacheInfo(SceTeleportCacheInfo *cacheInfo)
{
	int initialTargetVal = 0;
	SceUInt8 macBin[32];
	char macStr[20];
	char nameBuf[64];
	char passBuf[32];

	memset(macBin, 0, sizeof(macBin));
	memset(macStr, 0, 18);
	memset(nameBuf, 0, sizeof(nameBuf));
	memset(passBuf, 0, sizeof(passBuf));

	if (sceRegMgrSystemParamGetBin(TELEPORT_REG_DEVICE_MAC, macBin, sizeof(macBin)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	bool hasMac = false;
	for (int i = 0; i < 32; ++i) {
		if (macBin[i] != 0) {
			hasMac = true;
			break;
		}
	}

	if (!hasMac) {
		memset(cacheInfo->deviceId, 0, sizeof(cacheInfo->deviceId));
	} else {
		for (int b = 0; b < 6; ++b) {
			macStr[b * 3] = formatLowerHexNibble(macBin[b] >> 4);
			macStr[b * 3 + 1] = formatLowerHexNibble(macBin[b] & 0xFu);
			if (b < 5)
				macStr[b * 3 + 2] = ':';
		}
		sceClibSnprintf(asChar(cacheInfo->deviceId), sizeof(cacheInfo->deviceId),
				"%s", macStr);
	}

	if (sceRegMgrSystemParamGetStr(TELEPORT_REG_DEVICE_NAME, nameBuf, sizeof(nameBuf)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;
	sceClibSnprintf(asChar(cacheInfo->deviceName), sizeof(cacheInfo->deviceName),
			"%s", nameBuf);

	if (sceRegMgrSystemParamGetInt(TELEPORT_REG_INITIAL_TARGET, &initialTargetVal) < 0)
		return SCE_TELEPORT_ERROR_FATAL;
	cacheInfo->initialTarget = (initialTargetVal != 0) ? SCE_TRUE : SCE_FALSE;

	if (sceRegMgrSystemParamGetStr(TELEPORT_REG_PASSCODE, passBuf, sizeof(passBuf)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	if (passBuf[0] != '\0') {
		cacheInfo->passCode[0] = passBuf[0];
		cacheInfo->passCode[1] = passBuf[1];
		cacheInfo->passCode[2] = passBuf[2];
		cacheInfo->passCode[3] = passBuf[3];
	} else {
		memset(cacheInfo->passCode, 0, sizeof(cacheInfo->passCode));
	}
	return 0;
}

SceInt32 TeleportClient::clearCacheInfo()
{
	char zeroBuf[32];
	memset(zeroBuf, 0, sizeof(zeroBuf));
	if (sceRegMgrSystemParamSetStr(TELEPORT_REG_DEVICE_NAME, zeroBuf, sizeof(zeroBuf)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	SceUInt8 zeroMac[32];
	memset(zeroMac, 0, sizeof(zeroMac));
	if (sceRegMgrSystemParamSetBin(TELEPORT_REG_DEVICE_MAC, zeroMac, sizeof(zeroMac)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	if (sceRegMgrSystemParamSetInt(TELEPORT_REG_INITIAL_TARGET, 0) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	char zeroPass[32];
	memset(zeroPass, 0, sizeof(zeroPass));
	if (sceRegMgrSystemParamSetStr(TELEPORT_REG_PASSCODE, zeroPass, sizeof(zeroPass)) < 0)
		return SCE_TELEPORT_ERROR_FATAL;

	return 0;
}

SceInt32 TeleportClient::searchRecvThread(SceSize args, void *argp)
{
	(void)args;
	TeleportClient *self = (TeleportClient *)argp;
	int sendTimeout = TELEPORT_DEFAULT_TIMEOUT_US;
	SceUInt16 parsedPort = 0;
	SceInt32 exitStatus = 0;
	int sockFd = s_udpSocket;

	char searchMsg[512];
	char recvMsg[512];
	char upperMsg[512];
	char portStr[8];
	char ipStr[16];
	SceNetSockaddr fromAddr;
	SceNetSocklen_t fromLen = sizeof(fromAddr);

	memset(searchMsg, 0, sizeof(searchMsg));
	memset(recvMsg, 0, sizeof(recvMsg));
	memset(upperMsg, 0, sizeof(upperMsg));
	memset(portStr, 0, sizeof(portStr));
	memset(ipStr, 0, sizeof(ipStr));
	memset(&fromAddr, 0, sizeof(fromAddr));

	int msgLen = sceClibSnprintf(
		searchMsg, sizeof(searchMsg),
		"SEARCH *\r\nHOST: 239.255.255.250:41899\r\nUSER-AGENT: PlayStation Vita Teleport/2.00\r\nX-TELEPORT-ST:\r\n\r\n");

	if (sceNetSetsockopt(sockFd, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
			     &sendTimeout, sizeof(sendTimeout)) < 0 ||
	    sceNetSendto(sockFd, searchMsg, (unsigned int)msgLen, SCE_NET_MSG_DONTWAIT,
			 (const SceNetSockaddr *)&self->multicastAddr,
			 sizeof(self->multicastAddr)) < 0) {
		exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
	} else {
		for (;;) {
			sceKernelLockLwMutex(&s_mutex, 1, NULL);
			bool stop = (s_searchStopFlag != 0);
			sceKernelUnlockLwMutex(&s_mutex, 1);
			if (stop) {
				exitStatus = 0;
				break;
			}

			int curSock = s_udpSocket;
			memset(recvMsg, 0, sizeof(recvMsg));
			memset(upperMsg, 0, sizeof(upperMsg));
			int recvTimeout = 0;
			memset(portStr, 0, sizeof(portStr));
			memset(ipStr, 0, sizeof(ipStr));

			if (sceNetSetsockopt(curSock, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO,
					     &recvTimeout, sizeof(recvTimeout)) < 0) {
				exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
				break;
			}

			int peekLen = sceNetRecvfrom(curSock, NULL, sizeof(recvMsg),
						     SCE_NET_MSG_PEEKLEN,
						     &fromAddr, &fromLen);
			if (peekLen < 0) {
				exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
				break;
			}
			if (peekLen > (int)sizeof(recvMsg))
				continue;
			if (sceNetRecvfrom(curSock, recvMsg, (unsigned int)peekLen,
					   SCE_NET_MSG_WAITALL, &fromAddr, &fromLen) < 0) {
				exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
				break;
			}

			SceSize httpPrefixLen = sceClibStrnlen("http://", 16);
			SceSize code200Len = sceClibStrnlen("200", 4);
			SceSize okLen = sceClibStrnlen("OK", 4);
			bool isStatusOk = false;

			char lines[6][128];
			char devAddrLower[20];
			char devAddrRaw[20];
			SceTeleportDeviceInfo foundDev;
			memset(lines, 0, sizeof(lines));
			memset(devAddrLower, 0, 19);
			memset(&foundDev, 0, sizeof(foundDev));

			for (unsigned int i = 0; i < sizeof(recvMsg) && recvMsg[i] != '\0'; ++i)
				upperMsg[i] = (char)sceClibToupper((int)(signed char)recvMsg[i]);

			int lineCount = self->splitString(recvMsg, "\r\n", lines[0], 128, 6);
			if (sceClibStrncmp(lines[0], "200", code200Len) == 0) {
				char statusTokens[4][128];
				memset(statusTokens, 0, sizeof(statusTokens));
				self->splitString(lines[0], " ", statusTokens[0], 128, 4);
				if (sceClibStrncmp(statusTokens[1], "OK", okLen) == 0)
					isStatusOk = true;
			}

			bool hasTeleportSt = false;
			for (int i = 0; i < lineCount; ++i) {
				SceSize stLen = sceClibStrnlen("X-TELEPORT-ST:", 32);
				if (sceClibStrncasecmp(lines[i], "X-TELEPORT-ST:", stLen) == 0) {
					hasTeleportSt = true;
					break;
				}
			}
			if (!hasTeleportSt || !isStatusOk)
				continue;

			for (int i = 0; i < lineCount; ++i) {
				SceSize locLen = sceClibStrnlen("LOCATION:", 32);
				if (sceClibStrncasecmp(lines[i], "LOCATION:", locLen) == 0) {
					char locTokens[4][128];
					memset(locTokens, 0, sizeof(locTokens));
					self->splitString(lines[i], " ", locTokens[0], 128, 4);
					const char *hostPart = locTokens[1] + httpPrefixLen;
					char *colon = sceClibStrstr(hostPart, ":");
					int colonIdx = (colon != NULL) ? (int)(colon - hostPart) : -1;
					char *slash = sceClibStrstr(hostPart, "/");
					int slashIdx = (slash != NULL) ? (int)(slash - hostPart) : -1;
					if (colonIdx != -1 && slashIdx != -1 && colonIdx < slashIdx) {
						SceSize portLen = (SceSize)(slashIdx - colonIdx - 1);
						sceClibMemcpyChk(portStr, hostPart + colonIdx + 1, portLen, portLen);
						sceClibMemcpyChk(ipStr, hostPart, (SceSize)colonIdx, (SceSize)colonIdx);
						parsedPort = (SceUInt16)parseSignedDecimal(portStr, 8);
					}
				} else {
					SceSize addrKeyLen = sceClibStrnlen("X-TELEPORT-ADDRESS:", 32);
					if (sceClibStrncasecmp(lines[i], "X-TELEPORT-ADDRESS:", addrKeyLen) == 0) {
						char addrTokens[4][128];
						memset(devAddrRaw, 0, 19);
						memset(addrTokens, 0, sizeof(addrTokens));
						self->splitString(lines[i], " ", addrTokens[0], 128, 4);
						sceClibMemcpyChk(devAddrRaw, addrTokens[1], 17, 17);
						for (int k = 0; k < 19; ++k)
							devAddrLower[k] = (char)sceClibTolower((int)(signed char)devAddrRaw[k]);
					}
				}
			}

			if (parsedPort == 0 || devAddrLower[0] == '\0' || ipStr[0] == '\0')
				continue;

			sceClibSnprintf(asChar(foundDev.deviceId), sizeof(foundDev.deviceId),
					"%s", devAddrLower);
			sceClibSnprintf(asChar(foundDev.ipAddress), sizeof(foundDev.ipAddress),
					"%s", ipStr);
			foundDev.innerData = parsedPort;

			bool known = false;
			for (int i = 0; i < TELEPORT_DISCOVERED_MAX; ++i) {
				if (sceClibStrcmp(devAddrLower, s_discoveredDevices[i].deviceId) == 0) {
					known = true;
					if (s_discoveredDevices[i].port != parsedPort ||
					    sceClibStrcmp(ipStr, s_discoveredDevices[i].ipAddress) != 0) {
						s_discoveredDevices[i].port = parsedPort;
						sceClibSnprintf(s_discoveredDevices[i].ipAddress,
								sizeof(s_discoveredDevices[i].ipAddress),
								"%s", ipStr);
						self->callback(foundDev);
					}
				}
			}

			if (!known) {
				sceClibSnprintf(s_discoveredDevices[s_discoveredWriteIdx].deviceId,
						sizeof(s_discoveredDevices[s_discoveredWriteIdx].deviceId),
						"%s", devAddrLower);
				sceClibSnprintf(s_discoveredDevices[s_discoveredWriteIdx].ipAddress,
						sizeof(s_discoveredDevices[s_discoveredWriteIdx].ipAddress),
						"%s", ipStr);
				s_discoveredDevices[s_discoveredWriteIdx].port = parsedPort;
				++s_discoveredWriteIdx;
				if (s_discoveredWriteIdx >= TELEPORT_DISCOVERED_MAX)
					s_discoveredWriteIdx = 0;
				self->callback(foundDev);
			}
		}
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_threadRunningMask &= ~TELEPORT_THREAD_RECV_BIT;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return exitStatus;
}

SceInt32 TeleportClient::searchSendThread(SceSize args, void *argp)
{
	(void)args;
	TeleportClient *self = (TeleportClient *)argp;
	char msgBuf[512];
	SceNetSockInfo sockInfo;
	SceInt32 exitStatus = 0;

	memset(msgBuf, 0, sizeof(msgBuf));
	for (;;) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		bool stop = (s_searchStopFlag != 0);
		sceKernelUnlockLwMutex(&s_mutex, 1);
		if (stop) {
			exitStatus = 0;
			break;
		}

		int sockFd = s_udpSocket;
		if (sockFd < 0) {
			exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
			break;
		}

		memset(msgBuf, 0, sizeof(msgBuf));
		if (sceNetGetSockInfo(sockFd, &sockInfo, 1, 0) < 0) {
			exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
			break;
		}

		if (sockInfo.state == SCE_NET_SOCKINFO_STATE_OPENED) {
			int sendTimeout = TELEPORT_DEFAULT_TIMEOUT_US;
			int msgLen = sceClibSnprintf(
				msgBuf, sizeof(msgBuf),
				"SEARCH *\r\nHOST: 239.255.255.250:41899\r\nUSER-AGENT: PlayStation Vita Teleport/2.00\r\nX-TELEPORT-ST:\r\n\r\n");
			if (sceNetSetsockopt(sockFd, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
					     &sendTimeout, sizeof(sendTimeout)) < 0 ||
			    sceNetSendto(sockFd, msgBuf, (unsigned int)msgLen, SCE_NET_MSG_DONTWAIT,
					 (const SceNetSockaddr *)&self->multicastAddr,
					 sizeof(self->multicastAddr)) < 0) {
				exitStatus = SCE_TELEPORT_ERROR_COMMUNICATION;
				break;
			}
		}

		SceUInt waitUs = 2000000;
		if (sceKernelWaitEventFlag(s_searchEventFlag, 1,
					   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					   NULL, &waitUs) != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			exitStatus = 0;
			break;
		}
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_threadRunningMask &= ~TELEPORT_THREAD_SEND_BIT;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return exitStatus;
}

SceInt32 TeleportClient::wolSendThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	SceUInt8 macBin[32];
	memset(macBin, 0, sizeof(macBin));
	SceInt32 exitStatus = 0;

	if (sceRegMgrSystemParamGetBin(TELEPORT_REG_DEVICE_MAC, macBin, sizeof(macBin)) < 0) {
		exitStatus = SCE_TELEPORT_ERROR_FATAL;
	} else {
		for (int i = 0; i < 10; ++i) {
			sceKernelLockLwMutex(&s_mutex, 1, NULL);
			bool stop = (s_searchStopFlag != 0);
			sceKernelUnlockLwMutex(&s_mutex, 1);
			if (stop)
				break;
			teleportSendWakeOnLan(macBin);
			SceUInt waitUs = 1000000;
			if (sceKernelWaitEventFlag(s_wolEventFlag, 1,
						   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						   NULL, &waitUs) != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				break;
		}
	}

	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_threadRunningMask &= ~TELEPORT_THREAD_WOL_BIT;
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return exitStatus;
}
