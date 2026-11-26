/*
 * teleport_internal.h: internal classes, constants, and helpers for SceTeleportClient.
 */
#ifndef _TELEPORT_INTERNAL_H_
#define _TELEPORT_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sceerror.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <regmgr/regmgr.h>
#include <net/net.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/inet.h>
#include <net/sockinfo.h>
#include <libhmac.h>
#include <ces/libces_ucs.h>
#include <xml/xml.h>
#include <teleport/teleport_client.h>

extern "C" {
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int ch, size_t n);
}

inline void *operator new(size_t, void *ptr) throw()
{
	return ptr;
}

static inline char *asChar(SceChar8 *p)
{
	return reinterpret_cast<char *>(p);
}

static inline const char *asConstChar(const SceChar8 *p)
{
	return reinterpret_cast<const char *>(p);
}

/* Registry parameter IDs for Teleport cached device */
#define TELEPORT_REG_DEVICE_NAME	5143062		/* /CONFIG/TEL_CLIENT/device_name (str) */
#define TELEPORT_REG_DEVICE_MAC		9275883		/* /CONFIG/TEL_CLIENT/mac_addr (bin) */
#define TELEPORT_REG_INITIAL_TARGET	9990528		/* /CONFIG/TEL_CLIENT/initial_target (int) */
#define TELEPORT_REG_PASSCODE		328932		/* /CONFIG/TEL_CLIENT/passcode (str) */

/* Model capability checked in sceTeleportClientInitialize */
#define TELEPORT_MODEL_CAPABILITY_CLIENT	10

/* Multicast SSDP-like search address & port, and Wake-on-LAN port */
#define TELEPORT_MULTICAST_ADDR		"239.255.255.250"
#define TELEPORT_MULTICAST_PORT		41899		/* 0xA3AB */
#define TELEPORT_WOL_PORT		9294		/* 0x244E */

#define TELEPORT_IO_BUFFER_SIZE		0x4000
#define TELEPORT_DISCOVERED_MAX		16
#define TELEPORT_DEFAULT_TIMEOUT_US	120000000
#define TELEPORT_THREAD_PRIORITY	0x10000100

/* Thread running bitmask flags */
#define TELEPORT_THREAD_RECV_BIT	0x01u
#define TELEPORT_THREAD_SEND_BIT	0x02u
#define TELEPORT_THREAD_WOL_BIT		0x04u

/* Cached discovery entry (36 bytes; array of 16 = 576 bytes) */
struct TeleportDiscoveredDevice {
	char deviceId[18];
	char ipAddress[16];
	SceUInt16 port;
};

/* Cached device info + description pair (196 bytes: 60 + 136) */
struct TeleportCachedDevice {
	SceTeleportDeviceInfo info;
	SceTeleportDeviceDescription desc;
};

/* Cached TCP socket per discovered device ID (24 bytes) */
struct TeleportDeviceSocket {
	char deviceId[18];
	SceUInt16 pad;
	int sockFd;
};

/* Intermediate list filled by TeleportXmlParser::parseRemoteAppList (12 bytes) */
struct TeleportRemoteAppList {
	SceTeleportRemoteAppInfo *items;
	SceInt32 parsedCount;
	SceInt32 totalCount;
};

/* Parse modes for TeleportXmlParser::parseXml */
enum TeleportXmlParseMode {
	TELEPORT_XML_MODE_DEVICE_DESC = 0,
	TELEPORT_XML_MODE_APP_NUM = 1,
	TELEPORT_XML_MODE_APP_LIST = 2,
	TELEPORT_XML_MODE_LAUNCH_RESP = 3
};

class TeleportClient
{
public:
	TeleportClient();
	~TeleportClient();

	SceInt32 initialize(SceTeleportClientInitializeInfo *info);
	SceInt32 finalize();
	SceInt32 registerCallback(SceTeleportClientGetDeviceInfoCallback func);
	SceInt32 createSocket(int *outSock, const char *ipAddr, SceUInt16 port, int sockType);
	SceInt32 startSearchDevice();
	SceInt32 endSearchDevice();
	SceInt32 appendCachedDevice(const SceTeleportDeviceInfo *info,
				    const SceTeleportDeviceDescription *desc);
	static int splitString(const char *src, const char *delim, char *outRows,
			       int rowMaxLen, int maxRows);
	unsigned int parseContentLength(const char *response, unsigned int receivedLen);
	SceInt32 sendAndReceive(int sockFd, const char *sendBuf, SceSize sendMaxLen,
				char *recvBuf, unsigned int recvBufSize, SceInt32 timeout);
	SceInt32 storeDeviceSocket(const char *deviceId, int sockFd);
	int findDeviceSocket(const char *deviceId, int *outSock);
	SceInt32 fetchTeleportXml(const SceTeleportDeviceInfo *device,
				  char *recvBuf, unsigned int recvBufSize, SceInt32 timeout);
	SceInt32 getDeviceDescription(const SceTeleportDeviceInfo *device,
				      SceTeleportDeviceDescription *desc, SceUInt32 timeout);
	SceInt32 getRemoteAppInfoNum(const SceTeleportDeviceInfo *device,
				     SceTeleportRemoteAppInfoNum *infoNum, SceUInt32 timeout);
	SceInt32 getRemoteAppInfo(const SceTeleportDeviceInfo *device, SceUInt16 size,
				  SceTeleportRemoteAppInfo **info, SceUInt32 timeout);
	SceInt32 saveDeviceCache(const SceTeleportDeviceInfo *device, const char *passCode);
	unsigned int decodeHex(const char *hexStr, unsigned int hexLen,
			       SceUInt8 *outBuf, unsigned int outMax);
	int escapeXml(const char *src, int srcLen, char *dst, int dstMax, SceChar8 measureOnly);
	SceInt32 launchRemoteApp(const SceTeleportDeviceInfo *device,
				 const SceTeleportRemoteAppInfo *appInfo,
				 const SceTeleportRemoteAppArg *argInfo,
				 SceTeleportLaunchResponse *res,
				 int argLen, SceUInt32 timeout);
	SceInt32 deleteRemoteApp(const SceTeleportDeviceInfo *device,
				 const SceTeleportRemoteAppInfo *appInfo, SceUInt32 timeout);
	SceInt32 wakeupLatestDevice(const SceTeleportWakeupInfo *info);
	SceInt32 getCacheInfo(SceTeleportCacheInfo *cacheInfo);
	SceInt32 clearCacheInfo();

	static SceInt32 searchRecvThread(SceSize args, void *argp);
	static SceInt32 searchSendThread(SceSize args, void *argp);
	static SceInt32 wolSendThread(SceSize args, void *argp);

	SceTeleportClientGetDeviceInfoCallback callback;
	SceVoid *(*allocFunc)(SceUInt32 size);
	SceVoid (*freeFunc)(void *ptr);
	void *extraBuf;
	SceTeleportClientInitializeInfo *initInfo;
	SceUID searchRecvThreadId;
	SceUID searchSendThreadId;
	SceUID wolThreadId;
	SceNetSockaddrIn multicastAddr;
	TeleportCachedDevice *cachedDevices;
	SceInt32 cachedDeviceCount;
	TeleportDeviceSocket *deviceSockets;
	SceInt32 deviceSocketCount;
};

class TeleportXmlMemAllocator : public sce::Xml::MemAllocator
{
public:
	virtual ~TeleportXmlMemAllocator() {}
	virtual void *allocate(size_t size, void *userData);
	virtual void deallocate(void *ptr, void *userData);

	SceVoid *(*allocFunc)(SceUInt32 size);
	SceVoid (*freeFunc)(void *ptr);
};

class TeleportXmlParser
{
public:
	TeleportXmlParser(const SceTeleportClientInitializeInfo *info);
	~TeleportXmlParser();

	int parseErrorResponse(sce::Xml::Dom::Document *doc, sce::Xml::Dom::NodeId rootNode,
			       SceTeleportLaunchResponse *res, SceUInt32 *outErrCode);
	int parseLaunchResponse(sce::Xml::Dom::Document *doc, sce::Xml::Dom::NodeId rootNode,
				SceTeleportLaunchResponse *res);
	static int parseHttpStatusCode(const char *httpResp, int respLen);
	int parseVersionString(const char *verStr, unsigned int maxLen, SceUInt32 *outVer);
	int parseRemoteAppList(sce::Xml::Dom::Document *doc, sce::Xml::Dom::NodeId rootNode,
			       TeleportRemoteAppList *outList);
	int parseDeviceDescription(sce::Xml::Dom::Document *doc, sce::Xml::Dom::NodeId rootNode,
				   SceTeleportDeviceDescription *outDesc);
	int parseXml(const char *httpResp, int respLen, int mode,
		     void *outData, SceUInt32 *outErrCode);
};

/* Wake-on-LAN helpers (teleport_wol.cpp) */
int teleportSendMagicPacket(const void *macAddr);
int teleportSendWakeOnLan(const void *macAddr);

#endif /* _TELEPORT_INTERNAL_H_ */
