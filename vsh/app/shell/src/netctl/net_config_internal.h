/*
 * net_config_internal.h: internal structures, ioctl definitions, and
 * cross-file prototypes for SceNetConfig, SceNetDhcp, SceNetWlan,
 * SceNetPppoe, SceNetEth, and SceNetPhone inside SceShell.
 */
#ifndef SHELL_NET_CONFIG_INTERNAL_H
#define SHELL_NET_CONFIG_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include <libnet/netinternal.h>

#ifdef __cplusplus
extern "C" {
#endif

void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);

/* NetConfig error codes (0x804113xx) */
#define NETCFG_ERR_SOCKET			-2143218943	/* 0x80411301 */
#define NETCFG_ERR_UP_INTERFACE			-2143218942	/* 0x80411302 */
#define NETCFG_ERR_DOWN_INTERFACE		-2143218941	/* 0x80411303 */
#define NETCFG_ERR_SET_MTU			-2143218939	/* 0x80411305 */
#define NETCFG_ERR_SET_IFADDR			-2143218937	/* 0x80411307 */
#define NETCFG_ERR_DEL_IFADDR			-2143218936	/* 0x80411308 */
#define NETCFG_ERR_SET_ROUTE			-2143218935	/* 0x80411309 */
#define NETCFG_ERR_NO_DEFAULT_ROUTE		-2143218932	/* 0x8041130C */
#define NETCFG_ERR_INVALID_ARG			-2143218930	/* 0x8041130E */
#define NETCFG_ERR_ADD_ARP			-2143218929	/* 0x8041130F */
#define NETCFG_ERR_DEL_ARP			-2143218928	/* 0x80411310 */
#define NETCFG_ERR_ROUTE_SEND			-2143218927	/* 0x80411311 */
#define NETCFG_ERR_ROUTE_RECV			-2143218926	/* 0x80411312 */
#define NETCFG_ERR_FLUSH_ROUTES			-2143218924	/* 0x80411314 */

/* NetDhcp error codes (0x804115xx) */
#define NETDHCP_ERR_LEASE_EXPIRED		-2143218428	/* 0x80411504 */
#define NETDHCP_ERR_INVALID_PARAM		-2143218426	/* 0x80411506 */
#define NETDHCP_ERR_HOSTNAME_TOO_LONG		-2143218424	/* 0x80411508 */
#define NETDHCP_ERR_NO_MEMORY			-2143218423	/* 0x80411509 */
#define NETDHCP_ERR_NOT_INITIALIZED		-2143218422	/* 0x8041150A */
#define NETDHCP_ERR_NOT_FOUND			-2143218421	/* 0x8041150B */
#define NETDHCP_ERR_ALREADY_STARTED		-2143218420	/* 0x8041150C */
#define NETDHCP_ERR_VENDOR_TOO_LONG		-2143218419	/* 0x8041150D */

/* NetPppoe error codes (0x804114xx) */
#define NETPPPOE_ERR_ALREADY_STARTED		-2143218687	/* 0x80411401 */
#define NETPPPOE_ERR_SOCKET			-2143218686	/* 0x80411402 */
#define NETPPPOE_ERR_IF_CREATE			-2143218685	/* 0x80411403 */
#define NETPPPOE_ERR_AUTH_CONFIG		-2143218684	/* 0x80411404 */
#define NETPPPOE_ERR_DNS_CONFIG			-2143218683	/* 0x80411405 */
#define NETPPPOE_ERR_NOT_STARTED		-2143218682	/* 0x80411406 */
#define NETPPPOE_ERR_STATUS_IOCTL		-2143218681	/* 0x80411407 */
#define NETPPPOE_ERR_SESSION_IOCTL		-2143218680	/* 0x80411408 */
#define NETPPPOE_ERR_STATE_INITIAL		-2143218679	/* 0x80411409 */
#define NETPPPOE_ERR_STATE_PADI			-2143218678	/* 0x8041140A */
#define NETPPPOE_ERR_STATE_PADR			-2143218677	/* 0x8041140B */
#define NETPPPOE_ERR_STATE_CLOSING		-2143218675	/* 0x8041140D */
#define NETPPPOE_ERR_AUTH_STATUS_IOCTL		-2143218674	/* 0x8041140E */
#define NETPPPOE_ERR_AUTH_PAP			-2143218673	/* 0x8041140F */
#define NETPPPOE_ERR_AUTH_CHAP			-2143218672	/* 0x80411410 */
#define NETPPPOE_ERR_AUTH_UNKNOWN		-2143218671	/* 0x80411411 */
#define NETPPPOE_ERR_LCP_TIMEOUT		-2143218670	/* 0x80411412 */
#define NETPPPOE_ERR_AUTH_FAILED		-2143218669	/* 0x80411413 */
#define NETPPPOE_ERR_NO_CONCENTRATOR		-2143218668	/* 0x80411414 */
#define NETPPPOE_ERR_GET_DNS			-2143218667	/* 0x80411415 */
#define NETPPPOE_ERR_LINK_PARAMS		-2143218666	/* 0x80411416 */

/* BSD socket / routing / interface ioctl constants */
#define NET_AF_INET				2
#define NET_AF_ROUTE				17
#define NET_AF_LINK				18
#define NET_SOCK_DGRAM				2
#define NET_SOCK_RAW				3
#define NET_SOCK_ARP				7
#define NET_IFT_ETHER				6

#define NET_IFF_UP				0x0001u

#define NET_RTM_VERSION				3
#define NET_RTM_ADD				1
#define NET_RTM_DELETE				2
#define NET_RTM_GET				4

#define NET_RTA_DST				0x0001u
#define NET_RTA_GATEWAY				0x0002u
#define NET_RTA_NETMASK				0x0004u

#define NET_RTF_UP				0x0001
#define NET_RTF_GATEWAY				0x0002
#define NET_RTF_HOST				0x0004
#define NET_RTF_LLINFO				0x0400
#define NET_RTF_STATIC				0x0800
#define NET_RTV_EXPIRE				0x0004

#define NET_SIOCSIFFLAGS			0x80206910u
#define NET_SIOCGIFFLAGS			0xC0206911u
#define NET_SIOCDIFADDR				0x80206919u
#define NET_SIOCAIFADDR				0x8040691Au
#define NET_SIOCIFDESTROY			0x80206979u
#define NET_SIOCIFCREATE			0x8020697Au
#define NET_SIOCSIFMTU				0x8020697Fu
#define NET_PPPOESETPARMS			0x8030696Eu
#define NET_PPPOEGETSESSION			0xC0146974u
#define NET_PPPOEGETAUTHSTATUS			0xC1146975u
#define NET_SPPPSETAUTHCFG			0x80406979u
#define NET_PPPOEGETLINK			0xC014697Cu
#define NET_SPPPSETLCP				0x80146980u
#define NET_SPPPSETDNS				0x80146981u
#define NET_SPPPGETDNS				0xC0186983u
#define NET_SPPPGETSTATUS			0xC0146988u

#define NET_ERR_EADDRNOTAVAIL			-2143223503	/* 0x80410131 */

/* SceNetPs interface address record (16 bytes) */
typedef struct NetIfAddrRecord {
	SceUInt32 address;
	SceUInt32 broadcastAddress;
	SceUInt32 broadcastAddressCopy;
	SceUInt32 subnetMask;
} NetIfAddrRecord;

/* SceNetPs interface info record (320 bytes, matches SceNetPsInterfaceInfo) */
typedef struct NetIfInfoRecord {
	char name[16];
	char parentName[16];
	SceUInt32 interfaceIndex;
	SceUInt32 parentInterfaceIndex;
	NetIfAddrRecord addresses[2];
	SceUInt32 flags;
	SceUInt32 driverType;
	SceNetEtherAddr hardwareAddress;
	SceUInt8 hwAlign[2];
	SceUInt32 mtu;
	SceUInt32 driverMtu;
	SceUInt32 trailingWords[56];
} NetIfInfoRecord;

/* Link-level socket address (20 bytes, BSD sockaddr_dl) */
typedef struct NetSockaddrDl {
	SceUInt8 sdl_len;
	SceUInt8 sdl_family;
	SceUInt16 sdl_index;
	SceUInt8 sdl_type;
	SceUInt8 sdl_nlen;
	SceUInt8 sdl_alen;
	SceUInt8 sdl_slen;
	SceUInt8 sdl_data[12];
} NetSockaddrDl;

typedef union NetRouteAddr {
	SceNetSockaddr sa;
	SceNetSockaddrIn in;
	NetSockaddrDl dl;
} NetRouteAddr;

/* Route table entry returned by SceNetInternal_5B0E370C / BDA92B76 (88 bytes) */
typedef struct NetRouteTableEntry {
	char ifName[16];
	NetRouteAddr destination;
	NetRouteAddr netmask;
	NetRouteAddr gateway;
	int ifIndex;
	int flags;
	int mtu;
} NetRouteTableEntry;

/* Libnet sync object and thread parameter (matching libnet_internal.h) */
typedef struct NetSyncObj {
	SceUID eventFlag;
	int bits;
	char padding[24];
} NetSyncObj;

typedef struct NetThreadParam {
	int (*entry)(void);
	void *argument;
	int priority;
	int stackSize;
	int flags;
} NetThreadParam;

/* SceNetInternal imports used across NetConfig / NetDhcp / NetWlan / NetPppoe */
int SceNetInternal_104E4820(int usec);
int SceNetInternal_16222ED6(const void *dnsPair, int flags);
int SceNetInternal_225C4801(SceUID *threadId, NetThreadParam *param, const char *name);
int SceNetInternal_235DE96C(int ifId, void *ifInfo, int count);
int SceNetInternal_2FEC8B74(SceUInt64 *outUsec);
int SceNetInternal_333F9CB3(int sock, SceUInt32 command, void *ioctlArg);
int SceNetInternal_3B5F94B7(SceUID threadId);
int SceNetInternal_4EF96CD8(void);
NetRouteTableEntry *SceNetInternal_5B0E370C(int ifId, int *outCount);
int SceNetInternal_694F8996(const char *format, ...);
int SceNetInternal_9D12D8A4(NetSyncObj *sync, int bits);
void *SceNetInternal_A2C2BC5F(int size, int flags);
int SceNetInternal_AA414742(int ifId, int kernelIndex, SceUInt32 command, void *ioctlBuf,
			    SceSize size);
int SceNetInternal_BDA92B76(int family, void *entries, int count, int flags);
int SceNetInternal_C2FF615E(NetSyncObj *sync, SceUInt32 *timeout);
int SceNetInternal_CF49E9C4(NetRouteTableEntry *entries);
void SceNetInternal_E62A2FAA(void *ptr);
const char *SceNetInternal_F3917021(int ifId);
NetSyncObj *SceNetInternal_F6291476(int which);

/* net_config.cpp */
int sceNetConfigIsInterfaceUp(int ifId);
int sceNetConfigUpInterface(int ifId);
int sceNetConfigDownInterface(int ifId);
int sceNetConfigCleanUpAllInterfaces(void);
int sceNetConfigSetIfmtu(int ifId, int mtu);
int sceNetConfigGetIfaddrs(int ifId, void *outAddrs, int maxCount);
int sceNetConfigDelIfaddr(int ifId);
int sceNetConfigSetIfaddr(int ifId, const void *ipAddr, const void *peerAddr, const void *netmask);
int sceNetConfigAddIfaddr(int ifId, const void *ipAddr, const void *peerAddr, const void *netmask);
int sceNetConfigAddRoute(const void *dstAddr, const void *gateway, const void *netmask, int isHost);
int sceNetConfigAddMRoute(const void *gateway);
int sceNetConfigDelRoute(const void *dstAddr, const void *gateway, const void *netmask);
int sceNetConfigDelMRoute(const void *gateway);
int sceNetConfigDelDefaultRoute(void);
int sceNetConfigSetDefaultRoute(const void *gateway);
int sceNetConfigGetDefaultRoute(SceNetInAddr *outGateway);
int netConfigFlushRoutes(int ifId);
int sceNetConfigAddArp(const SceNetEtherAddr *etherAddr, const SceNetInAddr *ipAddr);
int sceNetConfigDelArp(int unusedIfId, const SceNetInAddr *ipAddr);

/* net_dhcp.cpp */
typedef struct NetDhcpInfo {
	int state;
	SceUInt32 leaseTime;
	SceUInt32 serverIp;
} NetDhcpInfo;

typedef struct NetDhcpLinkLocalInfo {
	int state;
	SceUInt32 ipAddr;
	SceUInt32 netmask;
} NetDhcpLinkLocalInfo;

int sceNetDhcpGetInfo(int ifId, NetDhcpInfo *outInfo);
int sceNetDhcpGetLinkLocalInfo(int ifId, NetDhcpLinkLocalInfo *outInfo);
int sceNetDhcpStart(int ifId, void (*callback)(void *userArg, int event, int err), void *userArg,
		    SceSize stackSize, int priority, const void *dhcpParam);
int sceNetDhcpStop(int ifId);

/* net_wlan.cpp */
int sceNetWlanStopBackgroundScan(int ifId);
int sceNetWlanScan(int ifId, const void *req, SceSize reqSize, void *outBuf, SceSize outBufSize,
		   void **outBssList, SceUInt32 *outBssCount);
int sceNetWlanConnect(int ifId, const void *req, void *outBuf, SceSize outBufSize, void **outBss);
int sceNetWlanDisconnect(int ifId);
int sceNetWlanGetRssi(int ifId, void *outRssiBuf);
int sceNetWlanAdhocCreate(int ifId, void *req);
int sceNetWlanAdhocJoin(int ifId, void *req);
int sceNetWlanAdhocLeave(int ifId);
int sceNetEthGetLinkStatus(int ifId, int *outStatus);
int sceNetPhoneGetStatus(int ifId, int *outStatus);
int sceNetPhoneNotifyEvent(int ifId, int eventType);

/* net_pppoe.cpp */
int sceNetPppoeStart(int ifId, int pppoeIfId, void (*callback)(void *userArg, int event, int err),
		     void *userArg, SceSize stackSize, int priority, const void *pppoeParam);
int sceNetPppoeStop(int pppoeIfId);

#ifdef __cplusplus
}
#endif

#endif
