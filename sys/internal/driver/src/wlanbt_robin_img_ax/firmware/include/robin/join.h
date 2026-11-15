/*
 * Joining a BSS as a station (0x11a60..0x12900): the host's
 * HostCmd_CMD_802_11_ASSOCIATE, the channel time it runs in, the
 * authentication and (re)association frames, and the result the host is
 * given. The names are inferred from use; see wlan/join.c.
 */
#ifndef ROBIN_JOIN_H
#define ROBIN_JOIN_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/ie.h"
#include "robin/protection.h"

struct WlanConnection;
struct WlanTxStatus;

/* HostCmd_DS_802_11_ASSOCIATE: the fixed part; the host's TLVs follow. */
typedef struct __attribute__((packed)) HostCmd_DS_802_11_ASSOCIATE {
	uint8_t peerStaAddr[6];
	uint8_t capInfo[2];
	uint8_t listenInterval[2];
	uint8_t beaconPeriod[2];
	uint8_t dtimPeriod;
	uint8_t tlvBuffer[];
} HostCmd_DS_802_11_ASSOCIATE;

_Static_assert(offsetof(HostCmd_DS_802_11_ASSOCIATE, tlvBuffer) == 13, "associate command");

/* The host's TLVs in the associate command (MrvlIEtypes_*). */
#define TLV_TYPE_SSID			0x0000
#define TLV_TYPE_RATES			0x0001
#define TLV_TYPE_PHY_DS			0x0003
#define TLV_TYPE_HT_CAPABILITY_IE	0x002d	/* an 802.11 element passed through */
#define TLV_TYPE_HT_INFORMATION_IE	0x003d
#define TLV_TYPE_OBSS_SCAN_PARAM_IE	0x004a
#define TLV_TYPE_RSN_IE			0x0030
#define TLV_TYPE_2040BSS_COEX_IE	0x0048
#define TLV_TYPE_EXTCAP_IE		0x007f
#define TLV_TYPE_VENDOR_IE		0x00dd
#define TLV_TYPE_CHANLIST		0x0101
#define TLV_TYPE_NUMPROBES		0x0102
#define TLV_TYPE_PASSTHROUGH		0x010a
#define TLV_TYPE_TSFTIMESTAMP		0x0113
#define TLV_TYPE_AUTH_TYPE		0x011f
#define TLV_TYPE_BSSID			0x0123
#define TLV_TYPE_PASSPHRASE		0x013c
#define TLV_TYPE_PMK			0x0144
#define TLV_TYPE_HT_CAPABILITY		0x014a
#define TLV_TYPE_HT_INFORMATION		0x014b
#define TLV_TYPE_2040BSS_COEX		0x014d
#define TLV_TYPE_OBSS_SCAN_PARAM	0x014e
#define TLV_TYPE_POWER_CAPABILITY_IE	0x0021
#define TLV_TYPE_EXTCAP			0x014f
#define TLV_TYPE_WAPI_IE		0x015e

/* The authentication types of TLV_TYPE_AUTH_TYPE. */
enum {
	WLAN_JOIN_AUTH_OPEN = 0,
	WLAN_JOIN_AUTH_SHARED = 1,
	WLAN_JOIN_AUTH_FT = 3,		/* reassociates without authenticating */
	WLAN_JOIN_AUTH_NETWORK_EAP = 4,
	WLAN_JOIN_AUTH_AUTO = 5,	/* open, then shared key if the AP refuses */
};

/* The 802.11 authentication algorithm numbers sent. */
#define WLAN_AUTH_ALG_OPEN		0x00
#define WLAN_AUTH_ALG_SHARED		0x01
#define WLAN_AUTH_ALG_NETWORK_EAP	0x80

/* 802.11 status code: the authentication algorithm is not supported. */
#define WLAN_STATUS_UNSUPPORTED_AUTH_ALG 13

/* WlanJoin.state */
enum {
	WLAN_JOIN_IDLE = 0,
	WLAN_JOIN_AUTH_SENT = 1,
	WLAN_JOIN_AUTH_ACKED = 2,
	WLAN_JOIN_AUTH_DONE = 3,
	WLAN_JOIN_ASSOC_SENT = 4,
	WLAN_JOIN_ASSOC_ACKED = 5,
	WLAN_JOIN_ASSOC_DONE = 6,
	WLAN_JOIN_FINISHED = 7,
};

/* Results of a join (WlanJoin.result); 0 is success. */
enum {
	WLAN_JOIN_OK = 0,
	WLAN_JOIN_REFUSED = 2,		/* the AP answered with a failure status */
	WLAN_JOIN_ASSOC_TX_FAILED = 3,
	WLAN_JOIN_ASSOC_TIMEOUT = 4,
	WLAN_JOIN_AUTH_TX_FAILED = 8,
	WLAN_JOIN_AUTH_TIMEOUT = 10,
	WLAN_JOIN_PREEMPTED = 12,	/* another channel request replaced ours */
};

/* What the slot request's callbacks are told. */
#define WLAN_SLOT_EVENT_JOIN_START	1
#define WLAN_SLOT_EVENT_JOIN_EXPIRED	4
#define WLAN_SLOT_EVENT_JOIN_FAILED	6
#define WLAN_SLOT_EVENT_JOIN_DONE	7

/* A BSS by address and SSID (40 bytes, byte aligned). */
struct WlanJoinBss {
	uint8_t bssid[6];
	uint8_t ssid[32];
	uint8_t ssidLength;
	uint8_t reserved39;
};

/* The authentication request macTxNotifyFilterFrame() sends. */
struct WlanJoinAuthRequest {
	uint8_t peer[6];
	uint8_t algorithm;		/* WLAN_AUTH_ALG_* */
	uint8_t sequence[2];		/* the transaction sequence number; 0: 1 */
	uint8_t reserved9[3];
};

/* The (re)association request macTxNotifyReleaseFrame()/SetupFrame() send. */
struct WlanJoinAssocRequest {
	uint8_t peer[6];
	uint16_t capability;
	uint16_t listenInterval;
	uint8_t ssid[32];
	uint8_t rates[14];
	const uint8_t *ies;		/* the host's extra elements */
	uint16_t iesLength;
	uint8_t reserved62[2];
};

_Static_assert(sizeof(struct WlanJoinAssocRequest) == 64, "association request");
_Static_assert(offsetof(struct WlanJoinAssocRequest, ies) == 56, "association elements");

/* IEEEtypes_AssocRsp_t as the host reads it when the join failed. */
struct WlanJoinFailure {
	uint16_t capability;		/* 0xfffc..0xffff: why it failed */
	uint8_t statusCode[2];
	uint8_t aid[2];
};

/* The join in progress (0x5e3e8). */
struct WlanJoin {
	uint8_t active;
	uint8_t retries;		/* authentications sent */
	uint8_t starts;			/* times the slot started */
	uint8_t savedPowerSave;
	struct WlanJoinBss previous;	/* the BSS joined last; a reassociation's peer */
	uint8_t reassociation;		/* the connection was associated when the join began */
	uint8_t reserved2d[3];
	uint32_t slot;			/* the slot request's handle */
	uint8_t state;			/* WLAN_JOIN_* */
	uint8_t authType;		/* WLAN_JOIN_AUTH_* the host asked for */
	uint8_t authAlgorithm;		/* WLAN_AUTH_ALG_* sent */
	uint8_t reserved37;
	uint32_t reassociationTime;	/* ms of channel time for a reassociation */
	uint32_t associationTime;	/* ms of channel time for an association */
	uint32_t authTimeout;		/* ms to wait for the authentication response */
	uint32_t portTimeout;		/* ms the port may stay closed after a reassociation */
	uint32_t reassociationWindow;	/* s after start-up a reassociation request is used */
	uint8_t privacy;		/* WPA or RSN */
	uint8_t wapi;
	uint8_t reserved4e[2];
	WlanIePointers ies;		/* of the association response */
	uint8_t result;			/* WLAN_JOIN_OK or a failure */
	uint8_t reservedad;
	uint16_t statusCode;		/* the AP's status code */
	uint32_t reassociationRequest;	/* send a reassociation request */
	uint32_t sameBss;		/* reassociating with the BSS joined last */
	uint32_t newBss;		/* reassociating with another BSS */
	uint32_t portPending;		/* the port opens when the host has the result */
	uint32_t portOpen;
	uint32_t reservedc4;
	uint64_t tsfOffset;		/* the BSS's TSF less ours, from the host's scan */
	uint64_t initTime;
	uint32_t associatedAt;		/* s since start-up; 0 for a reassociation */
	struct WlanIeBuffer ieBuffer;	/* the host's extra elements, then the response's */
	uint8_t rates[14];
	uint8_t rateCount;
	struct WlanJoinBss bss;		/* the BSS being joined */
	uint8_t capability[2];
	uint8_t reserved11d;
	uint8_t beaconPeriod[2];
	uint8_t dtimPeriod;
	uint8_t channel;
	uint8_t band;			/* bits 0-1: band; bits 2-3: channel width */
	uint8_t reserved123;
	struct WlanConnection *connection;
	WlanProtection savedProtection;	/* restored when the join fails */
};

_Static_assert(offsetof(struct WlanJoin, slot) == 0x30, "join slot");
_Static_assert(offsetof(struct WlanJoin, ies) == 0x50, "join elements");
_Static_assert(offsetof(struct WlanJoin, result) == 0xac, "join result");
_Static_assert(offsetof(struct WlanJoin, tsfOffset) == 0xc8, "join TSF offset");
_Static_assert(offsetof(struct WlanJoin, ieBuffer) == 0xdc, "join element buffer");
_Static_assert(offsetof(struct WlanJoin, bss) == 0xf3, "join BSS");
_Static_assert(offsetof(struct WlanJoin, channel) == 0x121, "join channel");
_Static_assert(offsetof(struct WlanJoin, connection) == 0x124, "join connection");
_Static_assert(offsetof(struct WlanJoin, savedProtection) == 0x128, "join saved protection");

extern struct WlanJoin wlanJoin;
extern uint16_t wlanAssocListenInterval;		/* 0x55a34, not converted yet */

int wlanJoinSendAuth(int shared);
int wlanJoinSendAssoc(void);
void wlanJoinClosePort(void);
uint32_t wlanJoinPortIsOpen(void);
void keyMgmtControlledPortOpen(void);
void wlanJoinSetBss(const struct WlanJoinBss *bss);
void wlanJoinFinish(uint8_t result);
int wlanJoinSlotEvent(uint32_t handle, int event, uint32_t argument);
int wlanJoinHandleFrame(uint32_t handle, const struct WlanTxStatus *status, OsaBuffer *rx,
			uint32_t unused, int argument);
int wlanJoinSetup(HostCmd_DS_802_11_ASSOCIATE *command, unsigned int length);
int wlanJoinAssociate(struct WlanConnection *connection, HostCmd_DS_802_11_ASSOCIATE *command,
		      unsigned int length);
void wlanJoinAuthResult(int result, uint16_t statusCode);
void wlanJoinAssocResult(int result, uint16_t statusCode);
void wlanJoinInit(void);
int wlanJoinReported(OsaBuffer *buffer);
uint16_t wlanJoinListenInterval(void);
unsigned int wlanJoinGetResponse(uint8_t *response);
void wlanJoinHook(void);
int wlanJoinFilterIdle(void);
int wlanJoinSetBusy(struct WlanConnection *connection, uintptr_t busy);
int wlanJoinFilterActive(void);

#endif /* ROBIN_JOIN_H */
