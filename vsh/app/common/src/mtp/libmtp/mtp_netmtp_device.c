/*
 * The network device ("NETMTP DEVICE:"): finds the hosts on the local network (a broadcast
 * search, answered by each host), registers the console with a host (the host shows a PIN,
 * the user enters it here) and connects to a registered host. The requests are HTTP-like
 * lines over TCP to the host's request port.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/inet.h>
#include <net/epoll.h>
#include <net/errno.h>
#include <std/stdio.h>
#include <std/string.h>
#include <vshbridge/vshbridge.h>
#include "mtp_internal.h"

#define MTP_TAG_NETMTP_DEVICE	"NETMTP DEVICE:"

/* the version of the protocol between the console and its hosts */
#define NETMTP_WIRELESS_PROTOCOL_VERSION	1000000
/* a host with this protocol version or later is told the console's model */
#define NETMTP_MTP_PROTOCOL_VERSION_MODEL	1600010

#define NETMTP_SEARCH_PORT		9309
#define NETMTP_DEVICE_PORT		999
#define NETMTP_REQUEST_TIMEOUT		15000000
#define NETMTP_DISCOVERY_TIMEOUT	1000000
#define NETMTP_RETRY_DELAY		500000
#define NETMTP_MAX_HOSTS		128

/* the host discovery's state */
#define NETMTP_DISCOVERY_STOPPED	0
#define NETMTP_DISCOVERY_SEARCHING	1
#define NETMTP_DISCOVERY_ERROR		2

/* the registration's state */
#define NETMTP_REGISTER_NONE		0
#define NETMTP_REGISTER_DONE		1
#define NETMTP_REGISTER_FAILED		2
#define NETMTP_REGISTER_WAITING_PIN	3


typedef struct NetMtpDevice {
	SceUInt8 deviceId[16];
	SceUInt8 macAddress[6];
	char name[256];
	SceUInt8 padding[2];
	/* the host discovery */
	int discoverySocket;
	int discoveryRunning;
	NetMtpThread *discoveryThread;
	SceUInt32 numHosts;
	MtpNetHost hosts[NETMTP_MAX_HOSTS];
	SceUID mutex;
	SceUID hostsMutex;		/* the hosts and the discovery's state */
	int discoveryState;
	/* the registration */
	int registerSocket;
	MtpNetHost registerHost;
	int registerState;
	NetMtpRegisterCallback registerCallback;
	NetMtpThread *observerThread;
	int epoll;
	/* the host answered the connection with 210 */
	int found;
	SceUInt32 reserved;
} NetMtpDevice;

/* "ux0:data/dolceModel": the model a PS TV names itself by */
static char s_dolceModel[32];
static char s_recvBuf[1024];
static NetMtpDevice s_device;

static const SceUInt8 s_noMacAddress[6] = { 0 };

static int hostDiscoveryThreadHandler(void *arg);
static int registrationObserverThreadHandler(void *arg);

/* what the console calls itself to the host */
static inline const char *getDeviceType(void)
{
	int size = mtpFileGetData("ux0:data/dolceModel", s_dolceModel, sizeof(s_dolceModel));

	if (size <= 0 || (SceUInt32)size >= sizeof(s_dolceModel)) {
		if (vshSblAimgrIsDolce() != 0)
			return "PS TV";
		return "PS Vita";
	}
	s_dolceModel[size] = '\0';
	return s_dolceModel;
}

static inline int checkWirelessProtocolVersion(const MtpNetHost *host)
{
	if (host->wirelessProtocolVersion == NETMTP_WIRELESS_PROTOCOL_VERSION)
		return 0;
	if (NETMTP_WIRELESS_PROTOCOL_VERSION - host->wirelessProtocolVersion < 0)
		return SCE_MTP_ERROR_NET_UPDATE_DEVICE;
	switch (host->type) {
	case MTP_NET_HOST_TYPE_WIN:
	case MTP_NET_HOST_TYPE_MAC:
		return SCE_MTP_ERROR_NET_UPDATE_PC_HOST;
	case MTP_NET_HOST_TYPE_NASNE:
		return SCE_MTP_ERROR_NET_UPDATE_NASNE;
	case MTP_NET_HOST_TYPE_PS3:
		return SCE_MTP_ERROR_NET_UPDATE_PS3;
	default:
		return SCE_MTP_ERROR_NET_UNKNOWN_HOST_TYPE;
	}
}

/* a PS TV registers only with the hosts that list it */
static inline int isDolceUnsupported(const MtpNetHost *host)
{
	return vshSblAimgrIsDolce() != 0 &&
	       (host->type == MTP_NET_HOST_TYPE_WIN || host->type == MTP_NET_HOST_TYPE_MAC) &&
	       (host->supportedDevice & MTP_NET_HOST_SUPPORTS_DOLCE) == 0;
}

int netMtpDeviceInitialize(void)
{
	char buf[256];

	mtpUtilMemset(&s_device, 0, sizeof(s_device));
	netMtpMakeDeviceId(s_device.deviceId);
	getSystemParamUserName(s_device.name, 0);
	getMacAddr(s_device.macAddress);
	netMtpIdToString(buf, sizeof(buf) - 1, s_device.deviceId);
	netMtpMacAddressToString(buf, sizeof(buf) - 1, s_device.macAddress);
	netMtpNameToString(buf, sizeof(buf) - 1, s_device.name);
	s_device.discoverySocket = -1;
	s_device.registerSocket = -1;
	sce_paf_memset(&s_device.registerHost, 0, sizeof(s_device.registerHost));
	s_device.registerState = NETMTP_REGISTER_NONE;
	s_device.found = 0;
	createMutex(&s_device.mutex);
	createMutex(&s_device.hostsMutex);
	loadSettingsDataFromRegistry();
	return 0;
}

int netMtpDeviceTerminate(void)
{
	mtpRegistryFinalize();
	deleteMutex(s_device.hostsMutex);
	deleteMutex(s_device.mutex);
	return 0;
}

int netMtpDeviceGetDeviceId(SceUInt8 *id)
{
	if (id == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 280, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mtpUtilMemcpy(id, s_device.deviceId, sizeof(s_device.deviceId));
	return 0;
}

static inline int createHostDiscoverySocket(void)
{
	int s;
	int optval;

	s = sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
	if (s == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1071, "failed to pf_socket_socket %d\n",
			sce_net_errno);
		return -1;
	}
	optval = 1;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_BROADCAST, &optval,
			     sizeof(optval)) != 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1078, "setsockopt SO_BROADCAST : %d\n",
			sce_net_errno);
		sceNetSocketClose(s);
		return -1;
	}
	optval = NETMTP_DISCOVERY_TIMEOUT;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &optval,
			     sizeof(optval)) != 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1087,
			"failed to setsockopt for SO_SNDTIMEO : %d\n", sce_net_errno);
		sceNetSocketClose(s);
		return -1;
	}
	optval = NETMTP_DISCOVERY_TIMEOUT;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &optval,
			     sizeof(optval)) != 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1096,
			"failed to setsockopt for SO_SNDTIMEO : %d\n", sce_net_errno);
		sceNetSocketClose(s);
		return -1;
	}
	optval = 1;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, &optval,
			     sizeof(optval)) < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1105, "failed to set SO_REUSEADDR option %d",
			sce_net_errno);
		sceNetSocketClose(s);
		return -1;
	}
	optval = 1;
	if (sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_ONESBCAST, &optval,
			     sizeof(optval)) < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1117, "sceNetSetsockopt SCE_NET_SO_ONESBCAST : %d",
			sce_net_errno);
		sceNetSocketClose(s);
		return -1;
	}
	return s;
}

int startHostDiscovery(void)
{
	int ret = 0;
	int s;
	NetMtpThread *thread;

	mutexLock(s_device.mutex);
	if (s_device.discoveryState != NETMTP_DISCOVERY_STOPPED)
		goto end;
	s = createHostDiscoverySocket();
	if (s == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1275, "failed to create socket\n");
	} else {
		thread = netMtpThreadCreate(hostDiscoveryThreadHandler, NULL, 0);
		if (thread == NULL) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1282, "failed to create thread\n");
		} else {
			s_device.discoverySocket = s;
			s_device.discoveryRunning = 1;
			s_device.discoveryThread = thread;
			s_device.numHosts = 0;
			s_device.discoveryState = NETMTP_DISCOVERY_SEARCHING;
			ret = netMtpThreadStart(thread);
			if (ret >= 0) {
				ret = 0;
				goto end;
			}
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1295, "failed to start thread 0x%x\n", ret);
		}
		if (thread != NULL)
			netMtpThreadDelete(thread);
	}
	if (s != -1)
		sceNetSocketClose(s);
	s_device.discoveryRunning = 0;
	s_device.discoverySocket = -1;
	s_device.discoveryThread = NULL;
	s_device.numHosts = 0;
	s_device.discoveryState = NETMTP_DISCOVERY_STOPPED;
	ret = SCE_MTP_ERROR_NET;
end:
	mutexUnlock(s_device.mutex);
	return ret;
}

int stopHostDiscovery(void)
{
	int ret = 0;
	int res;

	mutexLock(s_device.mutex);
	if (s_device.discoveryState == NETMTP_DISCOVERY_STOPPED) {
		mutexUnlock(s_device.mutex);
		return 0;
	}
	s_device.discoveryRunning = 0;
	res = sceNetSocketAbort(s_device.discoverySocket, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1335, "failed to abort socket 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	res = sceNetShutdown(s_device.discoverySocket, SCE_NET_SHUT_RDWR);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1341, "failed to shutdown socket 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	res = sceNetSocketClose(s_device.discoverySocket);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1347, "failed to close socket 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	res = netMtpThreadJoin(s_device.discoveryThread, NULL);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1353, "failed to join thread 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	res = netMtpThreadDelete(s_device.discoveryThread);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1359, "failed to delete thread 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	s_device.discoveryThread = NULL;
	s_device.discoverySocket = -1;
	s_device.discoveryState = NETMTP_DISCOVERY_STOPPED;
	mutexUnlock(s_device.mutex);
	return ret;
}

int netMtpDeviceGetHostDiscoveryState(int *state)
{
	if (state == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 302, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mutexLock(s_device.hostsMutex);
	*state = s_device.discoveryState;
	mutexUnlock(s_device.hostsMutex);
	return 0;
}

int netMtpDeviceGetNumDiscoveredHosts(SceUInt32 *num)
{
	if (num == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 318, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mutexLock(s_device.hostsMutex);
	*num = s_device.numHosts;
	mutexUnlock(s_device.hostsMutex);
	return 0;
}

int netMtpDeviceGetDiscoveredHost(SceUInt32 index, MtpNetHost *host)
{
	if (host == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 333, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	mutexLock(s_device.hostsMutex);
	if (index >= s_device.numHosts) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 340, "invalid index %d\n", index);
		mutexUnlock(s_device.hostsMutex);
		return SCE_MTP_ERROR_NET_BAD_TIMING;
	}
	mtpUtilMemcpy(host, &s_device.hosts[index], sizeof(*host));
	mutexUnlock(s_device.hostsMutex);
	return 0;
}

/* asks the host to update itself: it is too old for the console */
static __attribute__((noinline)) int requestHostUpdate(const MtpNetHost *pHost)
{
	int ret = 0;
	int s;
	int res;
	int status;
	int sndTimeout;
	int rcvTimeout;
	SceNetSockaddrIn addr;
	SceNetInAddr inAddr;
	char hostId[37];
	char hostType[7];
	char macAddress[13];
	char buf[512];
	const char *deviceType;

	if (pHost == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1380, "pHost is null\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0) == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1386, "failed to create socket\n");
		return SCE_MTP_ERROR_NET;
	}
	sndTimeout = NETMTP_REQUEST_TIMEOUT;
	rcvTimeout = NETMTP_REQUEST_TIMEOUT;
	s = sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (s == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1403, "failed to create socket\n");
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &sndTimeout,
			       sizeof(sndTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1410,
			"failed to setsockopt for SO_SNDTIMEO 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &rcvTimeout,
			       sizeof(rcvTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1417,
			"failed to setsockopt for SO_RCVTIMEO 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	mtpUtilMemset(&addr, 0, sizeof(addr));
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(pHost->requestPort);
	sceNetInetPton(SCE_NET_AF_INET, pHost->address, &inAddr);
	addr.sin_addr = inAddr;
	res = sceNetConnect(s, (const SceNetSockaddr *)&addr, sizeof(addr));
	if (res < 0) {
		ret = SCE_MTP_ERROR_NET;
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1429, "failed to connect 0x%x\n", res);
		goto close;
	}
	if (pHost->mtpProtocolVersion >= NETMTP_MTP_PROTOCOL_VERSION_MODEL)
		deviceType = getDeviceType();
	else
		deviceType = "vita";
	netMtpUuidToString(hostId, sizeof(hostId), pHost->id);
	res = sce_paf_snprintf(buf, sizeof(buf),
			       "UPDATE * HTTP/1.1\r\nhost-id:%s\r\ndevice-type:%s\r\n\r\n",
			       hostId, deviceType);
	res = sceNetSend(s, buf, res, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1451, "failed to send 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	mtpUtilMemset(buf, 0, sizeof(buf));
	res = sceNetRecv(s, buf, sizeof(buf), 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1461, "failed to receive 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	if (res == 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1465, "disconected\n");
		ret = SCE_MTP_ERROR_NET_DISCONNECTED;
		goto close;
	}
	res = parse_response(&status, 0, buf);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1473, "invalid response 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET_INVALID_RESPONSE;
		goto close;
	}
	if (status != 200) {
		switch (status) {
		case 601:
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1481, "invalid host id\n");
			ret = SCE_MTP_ERROR_NET;
			break;
		case 602:
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1484, "invalid device id\n");
			ret = SCE_MTP_ERROR_NET;
			break;
		case 503:
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1487, "server unavailable\n");
			ret = SCE_MTP_ERROR_NET;
			break;
		case 512:
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1490, "bad timing\n");
			ret = SCE_MTP_ERROR_NET_BAD_TIMING;
			break;
		default:
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1493, "unknown error %d\n", status);
			ret = SCE_MTP_ERROR_NET;
			break;
		}
	}
close:
	sceNetShutdown(s, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(s);
	return ret;
}

int netMtpDeviceRegisterStart(const MtpNetHost *host, NetMtpRegisterCallback callback)
{
	char hostId[37];
	char hostType[7];
	char hostMacAddress[13];
	int status;
	SceUInt32 numHosts;
	int sndTimeout;
	int rcvTimeout;
	SceNetInAddr inAddr;
	SceNetSockaddrIn addr;
	char macAddress[13];
	char deviceId[33];
	char uuid[37];
	char name[256];
	char buf[512];
	int ret = 0;
	int res;
	int s;
	int epoll;
	NetMtpThread *thread;
	const char *deviceType;

	if (host == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	numHosts = 0;
	if (s_device.registerState != NETMTP_REGISTER_NONE)
		return SCE_MTP_ERROR_ALREADY;
	if (host->type == MTP_NET_HOST_TYPE_NASNE) {
		mtpRegistryGetNumRegisteredNasneHosts(&numHosts);
		if (numHosts >= NETMTP_MAX_REGISTERED_HOSTS) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 381, "Max hosts (nasne)\n");
			return SCE_MTP_ERROR_NET_MAX_HOSTS;
		}
	} else if (host->type == MTP_NET_HOST_TYPE_PS3) {
		mtpRegistryGetNumRegisteredPs3Hosts(&numHosts);
		if (numHosts >= NETMTP_MAX_REGISTERED_HOSTS) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 387, "Max hosts (ps3)\n");
			return SCE_MTP_ERROR_NET_MAX_HOSTS;
		}
	} else {
		mtpRegistryGetNumRegisteredPcHosts(&numHosts);
		if (numHosts >= NETMTP_MAX_REGISTERED_HOSTS) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 393, "Max hosts (pc or win)\n");
			return SCE_MTP_ERROR_NET_MAX_HOSTS;
		}
	}
	netMtpUuidToString(hostId, sizeof(hostId), host->id);
	netMtpHostTypeToString(hostType, sizeof(hostType), host->type);
	netMtpMacAddressToString(hostMacAddress, sizeof(hostMacAddress), host->macAddress);
	ret = checkWirelessProtocolVersion(host);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 402,
			"Wireless version is incompatible need %d instead of %d\n",
			NETMTP_WIRELESS_PROTOCOL_VERSION, host->wirelessProtocolVersion);
		if (ret == SCE_MTP_ERROR_NET_UPDATE_PC_HOST || ret == SCE_MTP_ERROR_NET_UPDATE_NASNE)
			requestHostUpdate(host);
		return ret;
	}
	if (isDolceUnsupported(host)) {
		requestHostUpdate(host);
		return SCE_MTP_ERROR_NET_UPDATE_PC_HOST;
	}
	if (!netMtpIsWlanAvailable())
		return SCE_MTP_ERROR_NET;
	sndTimeout = NETMTP_REQUEST_TIMEOUT;
	rcvTimeout = NETMTP_REQUEST_TIMEOUT;
	s = sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (s == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 430, "failed to create socket\n");
		return SCE_MTP_ERROR_NET;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &sndTimeout,
			       sizeof(sndTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 437,
			"failed to setsockopt for SO_SNDTIMEO 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &rcvTimeout,
			       sizeof(rcvTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 444,
			"failed to setsockopt for SO_RCVTIMEO 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	mtpUtilMemset(&addr, 0, sizeof(addr));
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(host->requestPort);
	sceNetInetPton(SCE_NET_AF_INET, host->address, &inAddr);
	addr.sin_addr = inAddr;
	res = sceNetConnect(s, (const SceNetSockaddr *)&addr, sizeof(addr));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 456, "failed to connect 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	netMtpUuidToString(uuid, sizeof(uuid), host->id);
	netMtpIdToString(deviceId, sizeof(deviceId), s_device.deviceId);
	netMtpMacAddressToString(macAddress, sizeof(macAddress), s_device.macAddress);
	getSystemParamUserName(s_device.name, 0);
	netMtpNameToString(name, sizeof(name), s_device.name);
	if (host->mtpProtocolVersion >= NETMTP_MTP_PROTOCOL_VERSION_MODEL)
		deviceType = getDeviceType();
	else
		deviceType = "vita";
	res = sce_paf_snprintf(buf, sizeof(buf),
			       "SHOWPIN * HTTP/1.1\r\n"
			       "host-id:%s\r\n"
			       "device-id:%s\r\n"
			       "device-type:%s\r\n"
			       "device-mac-address:%s\r\n"
			       "device-name:%s\r\n"
			       "\r\n",
			       uuid, deviceId, deviceType, macAddress, name);
	res = sceNetSend(s, buf, res, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 479, "failed to send 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	mtpUtilMemset(buf, 0, sizeof(buf));
	res = sceNetRecv(s, buf, sizeof(buf), 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 490, "failed to receive 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	if (res == 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 494, "disconected\n");
		return SCE_MTP_ERROR_NET_DISCONNECTED;
	}
	res = parse_response(&status, 0, buf);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 502, "invalid response 0x%x\n", res);
		return SCE_MTP_ERROR_NET_INVALID_RESPONSE;
	}
	switch (status) {
	case 200:
		s_device.registerSocket = s;
		s_device.registerState = NETMTP_REGISTER_WAITING_PIN;
		mtpUtilMemcpy(&s_device.registerHost, host, sizeof(s_device.registerHost));
		s_device.registerCallback = callback;
		epoll = sceNetEpollCreate("pf_socket_select", 0);
		if (epoll < 0) {
			ret = SCE_MTP_ERROR_NET;
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 549, "failed to create epoll\n");
			break;
		}
		s_device.epoll = epoll;
		thread = netMtpThreadCreate(registrationObserverThreadHandler, NULL, 0);
		if (thread == NULL) {
			ret = SCE_MTP_ERROR_NET;
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 557, "failed to create thread\n");
			break;
		}
		res = netMtpThreadStart(thread);
		if (res < 0) {
			ret = SCE_MTP_ERROR_NET;
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 564, "failed to start thread 0x%x\n", res);
			break;
		}
		s_device.observerThread = thread;
		break;
	case 503:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 510, "Host unavailable\n");
		ret = SCE_MTP_ERROR_NET_HOST_UNAVAILABLE;
		break;
	case 600:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 513, "Host canceled\n");
		ret = SCE_MTP_ERROR_NET_HOST_CANCELED;
		break;
	case 601:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 516, "invalid host id\n");
		ret = SCE_MTP_ERROR_NET_REJECTED;
		break;
	case 602:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 519, "invalid device id\n");
		ret = SCE_MTP_ERROR_NET_REJECTED;
		break;
	case 603:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 522, "invalid register info\n");
		ret = SCE_MTP_ERROR_NET_REJECTED;
		break;
	case 604:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 525, "register count max\n");
		ret = SCE_MTP_ERROR_NET_REGISTER_COUNT_MAX;
		break;
	case 611:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 528, "invalid method\n");
		ret = SCE_MTP_ERROR_NET_INVALID_METHOD;
		break;
	case 512:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 531, "bad timing\n");
		ret = SCE_MTP_ERROR_NET_BAD_TIMING;
		break;
	default:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 534, "unknown error %d\n", status);
		ret = SCE_MTP_ERROR_NET_REJECTED;
		break;
	}
	return ret;
}

/* ends the thread that waits for the host to cancel the registration */
static inline void terminateRegistrationObserverThread(int *exitStatus)
{
	int res;

	if (s_device.observerThread == NULL)
		return;
	if (s_device.epoll >= 0) {
		sceNetEpollAbort(s_device.epoll, 0);
		sceNetEpollDestroy(s_device.epoll);
		s_device.epoll = -1;
	}
	res = netMtpThreadJoin(s_device.observerThread, exitStatus);
	if (res < 0)
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1678, "failed to join thread 0x%x\n", res);
	res = netMtpThreadDelete(s_device.observerThread);
	if (res < 0)
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1683, "failed to delete thread 0x%x\n", res);
	s_device.observerThread = NULL;
}

int netMtpDeviceRegisterSendPin(const char *pin)
{
	int status;
	int ret = SCE_MTP_ERROR_NET;
	char pinCode[9];
	char macAddress[13];
	char deviceId[33];
	char name[256];
	char buf[512];
	int s = s_device.registerSocket;
	int res;
	const char *deviceType;

	if (pin == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 594, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (s_device.registerState == NETMTP_REGISTER_NONE)
		return SCE_MTP_ERROR_DEVICE;
	if (s_device.registerState == NETMTP_REGISTER_DONE)
		return SCE_MTP_ERROR_ALREADY;
	terminateRegistrationObserverThread(&ret);
	/* the host canceled while the user entered the PIN */
	if (s_device.registerState == NETMTP_REGISTER_FAILED)
		return ret;
	netMtpIdToString(deviceId, sizeof(deviceId), s_device.deviceId);
	netMtpMacAddressToString(macAddress, sizeof(macAddress), s_device.macAddress);
	netMtpNameToString(name, sizeof(name), s_device.name);
	netMtpPinToString(pinCode, sizeof(pinCode), pin);
	deviceType = getDeviceType();
	res = sce_paf_snprintf(buf, sizeof(buf),
			       "REGISTER * HTTP/1.1\r\n"
			       "device-id:%s\r\n"
			       "device-type:%s\r\n"
			       "device-mac-address:%s\r\n"
			       "device-name:%s\r\n"
			       "pin-code:%s\r\n"
			       "\r\n",
			       deviceId, deviceType, macAddress, name, pinCode);
	res = sceNetSend(s, buf, res, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 627, "failed to send 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	res = sceNetRecv(s, buf, sizeof(buf), 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 636, "failed to receive 0x%x\n", res);
		return SCE_MTP_ERROR_NET;
	}
	if (res == 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 639, "disconected\n");
		return SCE_MTP_ERROR_NET_DISCONNECTED;
	}
	if (parse_response(&status, 0, buf) < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 646, "invalid response\n");
		return SCE_MTP_ERROR_NET_INVALID_RESPONSE;
	}
	switch (status) {
	case 200:
		s_device.registerState = NETMTP_REGISTER_DONE;
		return 0;
	case 408:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 653, "host timeout\n");
		return SCE_MTP_ERROR_NET_HOST_TIMEOUT;
	case 600:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 656, "host canceled\n");
		return SCE_MTP_ERROR_NET_HOST_CANCELED;
	case 604:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 659, "register count max\n");
		return SCE_MTP_ERROR_NET_REGISTER_COUNT_MAX;
	case 610:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 662, "invalid pin\n");
		return SCE_MTP_ERROR_NET_INVALID_PIN;
	case 611:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 665, "invalid method\n");
		return SCE_MTP_ERROR_NET_INVALID_METHOD;
	case 512:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 668, "bad timing\n");
		return SCE_MTP_ERROR_NET_BAD_TIMING;
	default:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 671, "Register error %d\n", status);
		return SCE_MTP_ERROR_NET_REJECTED;
	}
}

int netMtpDeviceRegisterEnd(void)
{
	int ret = 0;
	int s = s_device.registerSocket;
	int result;
	int res;
	char buf[512];

	if (s_device.registerState == NETMTP_REGISTER_NONE)
		return SCE_MTP_ERROR_DEVICE;
	if (s_device.registerState == NETMTP_REGISTER_DONE)
		mtpRegistryRegisterNetMtpHost(&s_device.registerHost);
	terminateRegistrationObserverThread(NULL);
	switch (s_device.registerState) {
	case NETMTP_REGISTER_DONE:
		result = 200;
		break;
	case NETMTP_REGISTER_FAILED:
		result = 600;
		break;
	default:
		result = 500;
		break;
	}
	res = sce_paf_snprintf(buf, sizeof(buf), "REGISTERRESULT * HTTP/1.1\r\nresult:%d\r\n\r\n",
			       result);
	res = sceNetSend(s, buf, res, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 726, "failed to send 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
	}
	sceNetShutdown(s, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(s);
	s_device.registerSocket = -1;
	sce_paf_memset(&s_device.registerHost, 0, sizeof(s_device.registerHost));
	s_device.registerState = NETMTP_REGISTER_NONE;
	return ret;
}

int netMtpDeviceCheckOnline(const MtpNetHost *host, int *online)
{
	SceUInt32 i;

	if (host == NULL || online == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 750, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	*online = 0;
	if (s_device.numHosts == 0)
		return 0;
	for (i = 0; sce_paf_memcmp(host->id, s_device.hosts[i].id, sizeof(host->id)) != 0; i++) {
		if (i + 1 >= s_device.numHosts)
			return 0;
	}
	*online = 1;
	return 0;
}

int netMtpDeviceCheckRegistered(const MtpNetHost *host, int *registered)
{
	SceUInt32 num = 0;
	SceUInt32 i;
	MtpNetHost entry;
	int ret;

	if (host == NULL || registered == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 773, "invalid parameter\n");
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	*registered = 0;
	ret = mtpRegistryGetNumRegisteredNetMtpHosts(&num);
	if (ret < 0)
		return ret;
	for (i = 0; i < num; i++) {
		if (mtpRegistryGetRegisteredNetMtpHost(i, &entry) < 0)
			return SCE_MTP_ERROR_NET;
		if (mtpUtilMemcmp(host->id, entry.id, sizeof(host->id)) == 0) {
			*registered = 1;
			break;
		}
	}
	return 0;
}

int netMtpDeviceConnectToHost(const MtpNetHost *host)
{
	char hostId[37];
	char hostType[7];
	char hostMacAddress[13];
	int status;
	int sndTimeout;
	int rcvTimeout;
	SceNetInAddr inAddr;
	SceNetSockaddrIn addr;
	char deviceId[33];
	char name[256];
	char buf[512];
	int ret;
	int res;
	int s;
	int deviceClass;
	const char *deviceType;

	if (host == NULL)
		return SCE_MTP_ERROR_NET_BAD_TIMING;
	netMtpUuidToString(hostId, sizeof(hostId), host->id);
	netMtpHostTypeToString(hostType, sizeof(hostType), host->type);
	netMtpMacAddressToString(hostMacAddress, sizeof(hostMacAddress), host->macAddress);
	ret = checkWirelessProtocolVersion(host);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 824,
			"Wireless version is incompatible need %d instead of %d\n",
			NETMTP_WIRELESS_PROTOCOL_VERSION, host->wirelessProtocolVersion);
		if (ret == SCE_MTP_ERROR_NET_UPDATE_PC_HOST || ret == SCE_MTP_ERROR_NET_UPDATE_NASNE)
			requestHostUpdate(host);
		return ret;
	}
	if (isDolceUnsupported(host)) {
		requestHostUpdate(host);
		return SCE_MTP_ERROR_NET_UPDATE_PC_HOST;
	}
	sndTimeout = NETMTP_REQUEST_TIMEOUT;
	rcvTimeout = NETMTP_REQUEST_TIMEOUT;
	s = sceNetSocket("net mtp socket", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (s == -1) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 856, "failed to create socket\n");
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &sndTimeout,
			       sizeof(sndTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 863,
			"failed to setsockopt for SO_SNDTIMEO 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	res = sceNetSetsockopt(s, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &rcvTimeout,
			       sizeof(rcvTimeout));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 870,
			"failed to setsockopt for SO_RCVTIMEO 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	addr.sin_family = SCE_NET_AF_INET;
	addr.sin_port = sceNetHtons(host->requestPort);
	sceNetInetPton(SCE_NET_AF_INET, host->address, &inAddr);
	addr.sin_addr = inAddr;
	if (sceNetConnect(s, (const SceNetSockaddr *)&addr, sizeof(addr)) < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 880, "failed to connect\n");
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	netMtpIdToString(deviceId, sizeof(deviceId), s_device.deviceId);
	deviceClass = vshSblAimgrIsDolce() != 0;
	netMtpNameToString(name, sizeof(name), s_device.name);
	deviceType = getDeviceType();
	res = sce_paf_snprintf(buf, sizeof(buf),
			       "CONNECT * HTTP/1.1\r\n"
			       "device-id:%s\r\n"
			       "device-port:%d\r\n"
			       "device-class:%d\r\n"
			       "device-type:%s\r\n"
			       "device-name:%s\r\n"
			       "\r\n",
			       deviceId, NETMTP_DEVICE_PORT, deviceClass, deviceType, name);
	res = sceNetSend(s, buf, res, 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 896, "failed to send 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	res = sceNetRecv(s, buf, sizeof(buf), 0);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 906, "failed to receive 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	if (res == 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 910, "disconected\n");
		ret = SCE_MTP_ERROR_NET_DISCONNECTED;
		goto close;
	}
	res = parse_response(&status, 0, buf);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 920, "invalid response 0x%x\n", res);
		ret = SCE_MTP_ERROR_NET;
		goto close;
	}
	switch (status) {
	case 200:
	case 210:
	case 211:
		s_device.found = status == 210;
		res = sceNetSend(s, "STANDBY * HTTP/1.1\r\n\r\n", 22, 0);
		if (res < 0) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 956, "failed to send 0x%x\n", res);
			ret = SCE_MTP_ERROR_NET;
		}
		break;
	case 503:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 931, "server unavailable\n");
		ret = SCE_MTP_ERROR_NET_HOST_UNAVAILABLE;
		break;
	case 605:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 934, "authentication failed\n");
		ret = SCE_MTP_ERROR_NET_REJECTED;
		break;
	case 512:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 937, "bad timing\n");
		ret = SCE_MTP_ERROR_NET_BAD_TIMING;
		break;
	default:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 940, "unknown error %d\n", status);
		ret = SCE_MTP_ERROR_NET;
		break;
	}
close:
	sceNetShutdown(s, SCE_NET_SHUT_RDWR);
	sceNetSocketClose(s);
	return ret;
}

int netMtpDeviceTakeFound(void)
{
	int found = s_device.found;

	s_device.found = 0;
	return found;
}

void netMtpSendWakeOnLanPacket(const MtpNetHost *host)
{
	int ret;

	if (host == NULL) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1556, "invalid argument\n");
		return;
	}
	/* a nasne gives no MAC address: its ID ends with it */
	if (mtpUtilMemcmp(s_noMacAddress, host->macAddress, sizeof(s_noMacAddress)) != 0) {
		ret = sceHostCollaboUtilSendWakeOnLanRequestTo(host->macAddress);
		if (ret < 0)
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1571,
				"sceHostCollaboUtilSendWakeOnLanRequestTo ret=%x\n", ret);
	} else {
		ret = sceHostCollaboUtilSendWakeOnLanRequestTo(&host->id[10]);
		if (ret < 0)
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1564,
				"sceHostCollaboUtilSendWakeOnLanRequestTo ret=0x%x\n", ret);
	}
}

/* collects the hosts' answers to a search for a second, renaming registered hosts */
static __attribute__((noinline)) int updateHostName(int s)
{
	char addr[16];
	char hostId[37];
	char hostType[7];
	char macAddress[13];
	SceNetSocklen_t fromlen;
	SceNetInAddr src;
	SceUInt32 num;
	SceNetSockaddrIn from;
	MtpNetHost host;
	MtpNetHost entry;
	SceUInt32 start = netMtpGetTick();
	SceUInt32 i;
	SceUInt32 j;
	int timedOut = 0;
	int ret = 0;
	int res;

	while (s_device.discoveryRunning != 0 && !timedOut) {
		fromlen = sizeof(from);
		res = sceNetRecvfrom(s, s_recvBuf, sizeof(s_recvBuf), 0, (SceNetSockaddr *)&from,
				     &fromlen);
		if (res != SCE_NET_ERROR_EWOULDBLOCK) {
			if (res <= 0) {
				ret = res;
				break;
			}
			s_recvBuf[res] = '\0';
			mutexLock(s_device.hostsMutex);
			if (s_device.numHosts < NETMTP_MAX_HOSTS &&
			    parse_host_discovery_responce(&host, s_recvBuf) >= 0 &&
			    host.requestPort != 0) {
				for (i = 0; i < s_device.numHosts; i++) {
					if (mtpUtilMemcmp(host.id, s_device.hosts[i].id,
							  sizeof(host.id)) == 0)
						break;
				}
				if (i >= s_device.numHosts) {
					src = from.sin_addr;
					sce_paf_strncpy(host.address,
							sceNetInetNtop(SCE_NET_AF_INET, &src, addr,
								       sizeof(addr)),
							sizeof(host.address) - 1);
					mtpUtilMemcpy(&s_device.hosts[s_device.numHosts], &host,
						      sizeof(host));
					s_device.numHosts++;
					num = 0;
					res = mtpRegistryGetNumRegisteredNetMtpHosts(&num);
					if (res < 0) {
						MTP_ERR(MTP_TAG_NETMTP_DEVICE, 994,
							"failed mtpRegistryGetNumRegisteredNetMtpHosts() 0x%x\n",
							res);
					} else {
						for (j = 0; j < num; j++) {
							mtpUtilMemset(&entry, 0, sizeof(entry));
							res = mtpRegistryGetRegisteredNetMtpHost(j, &entry);
							if (res < 0) {
								MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1007,
									"failed mtpRegistryGetRegisteredNetMtpHost() 0x%x\n",
									res);
								continue;
							}
							if (mtpUtilMemcmp(entry.id, host.id,
									  sizeof(host.id)) == 0 &&
							    sce_paf_strncmp(entry.name, host.name,
									    sizeof(host.name) - 1) != 0) {
								sce_paf_strncpy(entry.name, host.name,
										sizeof(host.name) - 1);
								res = mtpRegistryUpdateNetMtpHost(&entry);
								if (res < 0)
									MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1020,
										"failed mtpRegistryUpdateNetMtpHost() 0x%x\n",
										res);
								break;
							}
						}
					}
					netMtpUuidToString(hostId, sizeof(hostId), host.id);
					netMtpHostTypeToString(hostType, sizeof(hostType), host.type);
					netMtpMacAddressToString(macAddress, sizeof(macAddress),
								 host.macAddress);
				}
			}
			mutexUnlock(s_device.hostsMutex);
		}
		if (netMtpGetTick() - start > NETMTP_DISCOVERY_TIMEOUT)
			timedOut = 1;
	}
	return ret;
}

static inline int sendSearchRequest(int s)
{
	SceNetSockaddrIn to;
	char deviceId[33];
	char macAddress[13];
	char buf[512];
	const char *deviceType;
	int deviceClass;
	int res;

	mtpUtilMemset(&to, 0, sizeof(to));
	to.sin_family = SCE_NET_AF_INET;
	to.sin_port = sceNetHtons(NETMTP_SEARCH_PORT);
	to.sin_addr.s_addr = 0xFFFFFFFF;
	netMtpIdToString(deviceId, sizeof(deviceId), s_device.deviceId);
	netMtpMacAddressToString(macAddress, sizeof(macAddress), s_device.macAddress);
	deviceType = getDeviceType();
	deviceClass = vshSblAimgrIsDolce() != 0;
	res = sce_paf_snprintf(buf, sizeof(buf),
			       "SRCH3 * HTTP/1.1\r\n"
			       "device-id:%s\r\n"
			       "device-type:%s\r\n"
			       "device-class:%d\r\n"
			       "device-mac-address:%s\r\n"
			       "device-wireless-protocol-version:%08d\r\n"
			       "\r\n",
			       deviceId, deviceType, deviceClass, macAddress,
			       NETMTP_WIRELESS_PROTOCOL_VERSION);
	res = sceNetSendto(s, buf, res, 0, (const SceNetSockaddr *)&to, sizeof(to));
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1157, "failed to sendto 0x%x\n", res);
		return res;
	}
	return 0;
}

static inline void setDiscoveryState(int state)
{
	mutexLock(s_device.hostsMutex);
	s_device.discoveryState = state;
	mutexUnlock(s_device.hostsMutex);
}

static int hostDiscoveryThreadHandler(void *arg)
{
	int ret = 0;

	while (s_device.discoveryRunning != 0) {
		if (netMtpIsPhoneConnection()) {
			setDiscoveryState(NETMTP_DISCOVERY_ERROR);
			sceKernelDelayThread(NETMTP_RETRY_DELAY);
			continue;
		}
		ret = sendSearchRequest(s_device.discoverySocket);
		if (ret == SCE_NET_ERROR_EINTR)
			break;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1222, "failed to send search request\n");
			setDiscoveryState(NETMTP_DISCOVERY_ERROR);
			sceKernelDelayThread(NETMTP_RETRY_DELAY);
			continue;
		}
		setDiscoveryState(NETMTP_DISCOVERY_SEARCHING);
		ret = updateHostName(s_device.discoverySocket);
		if (ret == SCE_NET_ERROR_EINTR)
			break;
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1241, "failed to wait response\n");
			setDiscoveryState(NETMTP_DISCOVERY_ERROR);
			sceKernelDelayThread(NETMTP_RETRY_DELAY);
		}
	}
	return ret;
}

/* waits for the host to end the registration while the user enters the PIN */
static int registrationObserverThreadHandler(void *arg)
{
	int s = s_device.registerSocket;
	int epoll = s_device.epoll;
	int ret;
	int res;
	int status;
	int timedOut;
	SceNetEpollEvent event;
	SceNetEpollEvent events;
	char buf[512];

	mtpUtilMemset(&event, 0, sizeof(event));
	event.events |= SCE_NET_EPOLLIN;
	res = sceNetEpollControl(epoll, SCE_NET_EPOLL_CTL_ADD, s, &event);
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1595, "failed sceNetEpollControl 0x%x\n", res);
		return 0;
	}
	res = sceNetEpollWait(epoll, &events, 1, -1);
	if (res == 0 || res == SCE_NET_ERROR_EINTR)
		return 0;
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1608, "failed sceNetEpollWait 0x%x\n", res);
		return 0;
	}
	res = sceNetRecv(s, buf, sizeof(buf), 0);
	if (res == SCE_NET_ERROR_EINTR)
		return 0;
	if (res < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1618, "failed to receive 0x%x\n", res);
		return 0;
	}
	if (res == 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1621, "disconected\n");
		return 0;
	}
	if (parse_register_cancel_request(&status, buf) < 0) {
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1628, "invalid response\n");
		return 0;
	}
	switch (status) {
	case 408:
		timedOut = 1;
		ret = SCE_MTP_ERROR_NET_HOST_TIMEOUT;
		break;
	case 600:
		timedOut = 0;
		ret = SCE_MTP_ERROR_NET_HOST_CANCELED;
		break;
	default:
		MTP_ERR(MTP_TAG_NETMTP_DEVICE, 1647, "Unknown error %d\n", status);
		timedOut = 0;
		ret = SCE_MTP_ERROR_NET_HOST_CANCELED;
		break;
	}
	s_device.registerState = NETMTP_REGISTER_FAILED;
	if (s_device.registerCallback != NULL)
		s_device.registerCallback(&timedOut);
	return ret;
}
