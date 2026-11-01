/*
 * libnetctl.h: the network connection library, under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libnetctl.h).
 */
#ifndef _SCE_LIBNETCTL_H
#define _SCE_LIBNETCTL_H

#include <net.h>
#include <np/np_npid.h>
#include <system_param.h>
#include <rtc.h>

#define SCE_NET_CTL_ERROR_NOT_INITIALIZED		-2143215359	/* 0x80412101 */
#define SCE_NET_CTL_ERROR_NOT_TERMINATED			-2143215358	/* 0x80412102 */
#define SCE_NET_CTL_ERROR_CALLBACK_MAX			-2143215357	/* 0x80412103 */
#define SCE_NET_CTL_ERROR_ID_NOT_FOUND			-2143215356	/* 0x80412104 */
#define SCE_NET_CTL_ERROR_INVALID_ID			-2143215355	/* 0x80412105 */
#define SCE_NET_CTL_ERROR_INVALID_CODE			-2143215354	/* 0x80412106 */
#define SCE_NET_CTL_ERROR_INVALID_ADDR			-2143215353	/* 0x80412107 */
#define SCE_NET_CTL_ERROR_NOT_CONNECTED			-2143215352	/* 0x80412108 */
#define SCE_NET_CTL_ERROR_NOT_AVAIL			-2143215351	/* 0x80412109 */
#define SCE_NET_CTL_ERROR_AUTO_CONNECT_DISABLED		-2143215350	/* 0x8041210a */
#define SCE_NET_CTL_ERROR_AUTO_CONNECT_FAILED		-2143215349	/* 0x8041210b */
#define SCE_NET_CTL_ERROR_NO_SUITABLE_SETTING_FOR_AUTO_CONNECT	-2143215348	/* 0x8041210c */
#define SCE_NET_CTL_ERROR_DISCONNECTED_FOR_ADHOC_USE	-2143215347	/* 0x8041210d */
#define SCE_NET_CTL_ERROR_DISCONNECT_REQ			-2143215346	/* 0x8041210e */
#define SCE_NET_CTL_ERROR_INVALID_TYPE			-2143215345	/* 0x8041210f */
#define SCE_NET_CTL_ERROR_AUTO_DISCONNECT		-2143215344	/* 0x80412110 */
#define SCE_NET_CTL_ERROR_INVALID_SIZE			-2143215343	/* 0x80412111 */
#define SCE_NET_CTL_ERROR_FLIGHT_MODE_ENABLED		-2143215342	/* 0x80412112 */
#define SCE_NET_CTL_ERROR_WIFI_DISABLED			-2143215341	/* 0x80412113 */
#define SCE_NET_CTL_ERROR_WIFI_IN_ADHOC_USE		-2143215340	/* 0x80412114 */
#define SCE_NET_CTL_ERROR_ETHERNET_PLUGOUT		-2143215339	/* 0x80412115 */
#define SCE_NET_CTL_ERROR_WIFI_DEAUTHED			-2143215338	/* 0x80412116 */
#define SCE_NET_CTL_ERROR_WIFI_BEACON_LOST		-2143215337	/* 0x80412117 */
#define SCE_NET_CTL_ERROR_DISCONNECTED_FOR_SUSPEND	-2143215336	/* 0x80412118 */
#define SCE_NET_CTL_ERROR_COMMUNICATION_ID_NOT_EXIST	-2143215335	/* 0x80412119 */
#define SCE_NET_CTL_ERROR_ADHOC_ALREADY_CONNECTED	-2143215334	/* 0x8041211a */
#define SCE_NET_CTL_ERROR_DHCP_TIMEOUT			-2143215333	/* 0x8041211b */
#define SCE_NET_CTL_ERROR_PPPOE_TIMEOUT			-2143215332	/* 0x8041211c */
#define SCE_NET_CTL_ERROR_INSUFFICIENT_MEMORY		-2143215331	/* 0x8041211d */
#define SCE_NET_CTL_ERROR_PSP_ADHOC_JOIN_TIMEOUT		-2143215330	/* 0x8041211e */
#define SCE_NET_CTL_ERROR_UNKNOWN_DEVICE			-2143215224	/* 0x80412188 */
#define SCE_NET_CTL_STATE_DISCONNECTED			0
#define SCE_NET_CTL_STATE_CONNECTING			1
#define SCE_NET_CTL_STATE_IPOBTAINING			2
#define SCE_NET_CTL_STATE_IPOBTAINED			3
#define SCE_NET_CTL_EVENT_TYPE_DISCONNECTED		1
#define SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED	2
#define SCE_NET_CTL_EVENT_TYPE_IPOBTAINED		3
#define SCE_NET_CTL_INFO_CNF_NAME			1
#define SCE_NET_CTL_INFO_DEVICE				2
#define SCE_NET_CTL_INFO_ETHER_ADDR			3
#define SCE_NET_CTL_INFO_MTU				4
#define SCE_NET_CTL_INFO_LINK				5
#define SCE_NET_CTL_INFO_BSSID				6
#define SCE_NET_CTL_INFO_SSID				7
#define SCE_NET_CTL_INFO_WIFI_SECURITY			8
#define SCE_NET_CTL_INFO_RSSI_DBM			9
#define SCE_NET_CTL_INFO_RSSI_PERCENTAGE			10
#define SCE_NET_CTL_INFO_CHANNEL				11
#define SCE_NET_CTL_INFO_IP_CONFIG			12
#define SCE_NET_CTL_INFO_DHCP_HOSTNAME			13
#define SCE_NET_CTL_INFO_PPPOE_AUTH_NAME			14
#define SCE_NET_CTL_INFO_IP_ADDRESS			15
#define SCE_NET_CTL_INFO_NETMASK				16
#define SCE_NET_CTL_INFO_DEFAULT_ROUTE			17
#define SCE_NET_CTL_INFO_PRIMARY_DNS			18
#define SCE_NET_CTL_INFO_SECONDARY_DNS			19
#define SCE_NET_CTL_INFO_HTTP_PROXY_CONFIG		20
#define SCE_NET_CTL_INFO_HTTP_PROXY_SERVER		21
#define SCE_NET_CTL_INFO_HTTP_PROXY_PORT			22
#define SCE_NET_CTL_INFO_RESERVED1			23
#define SCE_NET_CTL_INFO_RESERVED2			24
#define SCE_NET_CTL_DEVICE_WIRELESS			0
#define SCE_NET_CTL_DEVICE_WIRED				1
#define SCE_NET_CTL_DEVICE_PHONE				2
#define SCE_NET_CTL_LINK_DISCONNECTED			0
#define SCE_NET_CTL_LINK_CONNECTED			1
#define SCE_NET_CTL_WIFI_SECURITY_NOAUTH			0
#define SCE_NET_CTL_WIFI_SECURITY_WEP			1
#define SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK		2
#define SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP		3
#define SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES		4
#define SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP		5
#define SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES		6
#define SCE_NET_CTL_WIFI_SECURITY_UNSUPPORTED		7
#define SCE_NET_CTL_IP_DHCP				0
#define SCE_NET_CTL_IP_STATIC				1
#define SCE_NET_CTL_IP_PPPOE				2
#define SCE_NET_CTL_HTTP_PROXY_OFF			0
#define SCE_NET_CTL_HTTP_PROXY_ON			1
#define SCE_NET_CTL_CNF_NAME_LEN				(64 + 1)
#define SCE_NET_CTL_SSID_LEN				(32 + 1)
#define SCE_NET_CTL_WIFI_SECURITY_KEY_LEN		(64 + 1)
#define SCE_NET_CTL_AUTH_NAME_LEN			(127 + 1)
#define SCE_NET_CTL_AUTH_KEY_LEN				(127 + 1)
#define SCE_NET_CTL_HOSTNAME_LEN				(255 + 1)
#define SCE_NET_CTL_IPV4_ADDR_STR_LEN			16

typedef union SceNetCtlInfo {
	char cnf_name[SCE_NET_CTL_CNF_NAME_LEN];
	SceUInt device;
	SceNetEtherAddr ether_addr;
	SceUInt mtu;
	SceUInt link;
	SceNetEtherAddr bssid;
	char ssid[SCE_NET_CTL_SSID_LEN];
	SceUInt wifi_security;
	SceUInt rssi_dbm;
	SceUInt rssi_percentage;
	SceUInt channel;
	SceUInt ip_config;
	char dhcp_hostname[SCE_NET_CTL_HOSTNAME_LEN];
	char pppoe_auth_name[SCE_NET_CTL_AUTH_NAME_LEN];
	char ip_address[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	char netmask[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	char default_route[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	char primary_dns[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	char secondary_dns[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	SceUInt http_proxy_config;
	char http_proxy_server[SCE_NET_CTL_HOSTNAME_LEN];
	SceUInt http_proxy_port;
} SceNetCtlInfo;

typedef struct SceNetCtlNatInfo {
	SceSize size;
	int stun_status;
	int nat_type;
	SceNetInAddr mapped_addr;
} SceNetCtlNatInfo;

#define SCE_NET_CTL_NATINFO_STUN_UNCHECKED		0
#define SCE_NET_CTL_NATINFO_STUN_FAILED			1
#define SCE_NET_CTL_NATINFO_STUN_OK			2
#define SCE_NET_CTL_NATINFO_NAT_TYPE_1			1
#define SCE_NET_CTL_NATINFO_NAT_TYPE_2			2
#define SCE_NET_CTL_NATINFO_NAT_TYPE_3			3

typedef struct SceNetCtlIfStat {
	SceSize size;
	SceUInt32 totalSec;
	SceUInt64 txBytes;
	SceUInt64 rxBytes;
	SceRtcTick resetTick;
	SceUInt32 reserved[8];
} SceNetCtlIfStat;

typedef struct SceNetCtlAdhocPeerInfo {
	struct SceNetInAddr inetAddr;
	SceNpId npId;
	SceUInt64 lastRecv;
	int appVer;
	SceBool isValidNpId;
	char username[SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE];
	char padding[7];
} SceNetCtlAdhocPeerInfo;

#ifdef __cplusplus
extern "C" {
#endif
int sceNetCtlInit(void);
void sceNetCtlTerm(void);
int sceNetCtlCheckCallback(void);
int sceNetCtlInetGetResult(int eventType, int *errorCode);
int sceNetCtlAdhocGetResult(int eventType, int *errorCode);
int sceNetCtlInetGetInfo(int code, SceNetCtlInfo *info);
int sceNetCtlInetGetState(int *state);
int sceNetCtlGetNatInfo(SceNetCtlNatInfo *natinfo);
int sceNetCtlGetIfStat(int device, SceNetCtlIfStat *ifstat);

typedef void (*SceNetCtlCallback)(int event_type, void *arg);

#define SCE_NET_CTL_CALLBACK_MAX				8
int sceNetCtlInetRegisterCallback(SceNetCtlCallback func, void *arg, int *cid);
int sceNetCtlInetUnregisterCallback(int cid);
int sceNetCtlAdhocRegisterCallback(SceNetCtlCallback func, void *arg, int *cid);
int sceNetCtlAdhocUnregisterCallback(int cid);
int sceNetCtlAdhocGetState(int *state);
int sceNetCtlAdhocDisconnect(void);
int sceNetCtlAdhocGetPeerList(SceSize *peerInfoNum, SceNetCtlAdhocPeerInfo *peerInfo);
int sceNetCtlAdhocGetInAddr(SceNetInAddr *inaddr);

#define SCE_NET_CTL_DOWNLOADABLE_SIZE_UNLIMITED		(0x7fffffffffffffffLL)
int sceNetCtlGetPhoneMaxDownloadableSize(SceInt64 *maxDownloadableSize);

#ifdef __cplusplus
}
#endif

#endif
