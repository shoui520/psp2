/*
 * Connection control block: one object per BSS the MAC works on (station,
 * AMP physical link, access point). The blocks form a linked list; the
 * lookup helpers are in wlan/conn.c. Only the fields converted code touches
 * are named; the remaining regions are reserved until their readers are
 * converted.
 */
#ifndef ROBIN_CONN_H
#define ROBIN_CONN_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"

/*
 * The 37 bytes in front of a frame sent through the WLAN SDIO device: the
 * transmit descriptor (offset 15 is the frame) and the Ethernet and LLC/SNAP
 * headers.
 */
struct WlanTxFrameHeader {
	uint8_t bssType;
	uint8_t bssNumber;
	uint8_t packetLength[2];
	uint8_t packetOffset[2];
	uint8_t packetType[2];
	uint8_t control[4];
	uint8_t priority;
	uint8_t flags;
	uint8_t delay;
	uint8_t destination[6];
	uint8_t source[6];
	uint8_t lengthBigEndian[2];
	uint8_t snap[6];
	uint8_t protocol[2];
};
typedef struct WlanTxFrameHeader WlanTxFrameHeader;
_Static_assert(sizeof(struct WlanTxFrameHeader) == 37, "TX frame header");
_Static_assert(offsetof(struct WlanTxFrameHeader, destination) == 15, "TX frame destination");

/* Interface types stored in WlanConnection.interfaceType. */
enum {
	WLAN_IF_IBSS = 1,
	WLAN_IF_STATION = 2,
	WLAN_IF_AP = 3,
	WLAN_IF_AMP = 4,
};

/* WlanConnection.bssType of an AMP physical link; bssNumber is its index. */
enum { WLAN_BSS_TYPE_AMP = 7 };

/* Periodic access window requested by the peer (14 bytes). */
struct WlanScheduleRequest {
	uint8_t type;
	uint8_t flags;
	uint8_t start[4];	/* unaligned word, microseconds */
	uint8_t length[4];	/* unaligned word, microseconds */
	uint8_t interval[4];	/* unaligned word, microseconds */
};

/* Per-association information of a station or access point connection (partial). */
struct WlanConnectionInfo {
	uint8_t reserved0[356];
	uint8_t ready;
};

struct WlanConnection {
	struct WlanConnection *next;
	uint8_t interfaceType;
	uint8_t mode;
	uint8_t bssType;
	uint8_t bssNumber;
	uint8_t flags;
	uint8_t flags2;
	uint8_t reserved0[2];
	void *device;
	uint8_t reserved1a[38];
	uint8_t hostFlags;		/* bit 4: frames go to the host as Ethernet II */
	uint8_t reserved1d[9];
	uint8_t supportedRates[12];
	uint8_t reserved1b[2];
	uint8_t supportedRatesLength;
	uint8_t reserved1c[73];
	uint8_t localAddress[6];
	uint8_t reserved2[14];
	uint8_t channel;
	uint8_t reserved3a[33];
	int16_t noise;			/* dBm, last measured */
	uint8_t reserved3b[2];
	int16_t snr;
	uint8_t reserved3c[168];
	uint8_t state;
	uint8_t peerAddress[6];
	uint8_t reserved4[345];
	uint8_t interfaceAddress[6];
	uint8_t interfaceIndex;
	uint8_t reserved4b[5];
	union {
		struct {
			uint8_t length;
			const char *name;
		} localName;			/* AMP link */
		struct WlanConnectionInfo *stationInfo;	/* station */
	};
	union {
		char ssid[32];			/* AMP link */
		struct WlanConnectionInfo *apInfo;	/* access point */
	};
	uint8_t initiator;
	uint8_t reserved785;
	uint16_t capability;
	uint8_t reserved5[4];
	uint32_t scheduleReference;
	uint8_t reserved5b[4];
	uint8_t scheduleValid;
	struct WlanScheduleRequest schedule;
	uint8_t linkFlags;
	WlanTxFrameHeader txHeader;
	uint8_t reserved6[11];
	uint32_t reserved7;
	uint32_t timerHandle;
	uint32_t secondTimerHandle;
	uint8_t retryCount;
	uint8_t reserved8[63];
	uint8_t transmitBusy;
};

_Static_assert(offsetof(struct WlanConnection, device) == 12, "device");
_Static_assert(offsetof(struct WlanConnection, hostFlags) == 54, "host flags");
_Static_assert(offsetof(struct WlanConnection, supportedRates) == 64, "rates");
_Static_assert(offsetof(struct WlanConnection, localAddress) == 152, "local address");
_Static_assert(offsetof(struct WlanConnection, channel) == 172, "channel");
_Static_assert(offsetof(struct WlanConnection, noise) == 206, "noise");
_Static_assert(offsetof(struct WlanConnection, snr) == 210, "SNR");
_Static_assert(offsetof(struct WlanConnection, state) == 380, "state");
_Static_assert(offsetof(struct WlanConnection, peerAddress) == 381, "peer address");
_Static_assert(offsetof(struct WlanConnection, interfaceAddress) == 732, "interface address");
_Static_assert(offsetof(struct WlanConnection, localName) == 744, "local name");
_Static_assert(offsetof(struct WlanConnection, ssid) == 752, "SSID");
_Static_assert(offsetof(struct WlanConnection, initiator) == 784, "initiator");
_Static_assert(offsetof(struct WlanConnection, capability) == 786, "capability");
_Static_assert(offsetof(struct WlanConnection, scheduleReference) == 792, "schedule reference");
_Static_assert(offsetof(struct WlanConnection, scheduleValid) == 800, "schedule flag");
_Static_assert(offsetof(struct WlanConnection, schedule) == 801, "schedule");
_Static_assert(offsetof(struct WlanConnection, linkFlags) == 815, "link flags");
_Static_assert(offsetof(struct WlanConnection, txHeader) == 816, "TX header");
_Static_assert(offsetof(struct WlanConnection, timerHandle) == 868, "timer");
_Static_assert(offsetof(struct WlanConnection, retryCount) == 876, "retry count");
_Static_assert(offsetof(struct WlanConnection, transmitBusy) == 940, "transmit busy");

/* wlan/conn.c */
extern struct WlanConnection *wlanConnectionList;
extern struct WlanConnection *wlanConnectionFreeList;
extern struct WlanConnection *wlanConnectionSession;
extern struct WlanConnection *wlanConnectionPrimary;
extern uint8_t wlanConnectionCount;

struct WlanConnection *wlanConnectionFind(unsigned int bssType, unsigned int bssNumber);
int wlanConnectionExists(unsigned int interfaceType);
struct WlanConnection *wlanConnectionFirst(unsigned int interfaceType);
struct WlanConnection *wlanConnectionFindByPeer(const uint8_t *peer);
struct WlanConnection *wlanConnectionFindByLocal(unsigned int interfaceType, const uint8_t *local);
void wlanConnectionRelease(struct WlanConnection *connection);
struct WlanConnection *wlanConnectionGetSession(void);
int wlanConnectionSetSession(struct WlanConnection *connection);
int wlanConnectionDecrement(void);
struct WlanConnectionInfo **wlanConnectionInfoSlot(struct WlanConnection *connection);
struct WlanConnectionInfo *wlanConnectionInfo(struct WlanConnection *connection);
int wlanConnectionIsReady(struct WlanConnection *connection);
void wlanConnectionSetActiveFlag(struct WlanConnection *connection, int active);
#define WLAN_CONN_HOST_ETHERNET		0x10
#define WLAN_CONN_HOST_FLAG_8		0x08

int wlanConnectionsIdle(void);
int wlanNoConnectionActive(void);
struct WlanConnection *wlanConnectionNextByBss(struct WlanConnection *after, unsigned int bssType,
					       unsigned int bssNumber);
int wlanAnyConnectionHostFlag8(void);
int wlanAnyConnectionFlag4(void);
int wlanAnyConnectionFlag10(void);
int wlanAnyActiveConnectionFlags2Bit0(void);
int wlanAnyConnectionFlags2Bit4(void);
struct WlanConnection *wlanConnectionFindByTypeNumber(unsigned int interfaceType,
						      unsigned int bssNumber);
int wlanConnectionHasLocalAddress(const struct WlanConnection *connection, const void *address);
int wlanContextUsesEthernetFrames(const void *context);
struct WlanConnection *wlanConnectionNextFlagged(struct WlanConnection *after,
						 unsigned int interfaceType);
int wlanBufferUsesAmp(OsaBuffer *buffer);
void *wlanGetBufferDevice(OsaBuffer *buffer);

/* wlan/connctl.c */
extern struct WlanConnection *wlanConnectionClient;
extern struct WlanConnection *wlanConnectionOther;
extern uint8_t wlanRoleMask;
extern uint8_t wlanConnectionPhase;
extern uint8_t wlanConnectionStage;

int wlanRoleMaskIs(unsigned int mask);
int wlanHasApRole(void);
struct WlanConnection *wlanConnectionByRole(int role);
int wlanConnectionHasPrimary(void);
struct WlanConnection *wlanConnectionGetPrimary(void);
int wlanConnectionSetPrimary(struct WlanConnection *connection);
int wlanConnectionTypeCount(void);
struct WlanConnection *wlanConnectionFindFlaggedByLocal(const void *address);
int wlanConnectionActivate(struct WlanConnection *connection);
int wlanPostConnectionEvent(struct WlanConnection *connection, uint32_t event);
int wlanConnectEventTryWait(void);
int wlanReleaseStateSemaphore(void);
int wlanPsCancelPendingTimer(void);
int wlanConnectionMarkReady(void);
unsigned int wlanRaiseEvent(unsigned int bit);
int wlanRoleOtherOnly(void);
int wlanRoleIbssOnly(void);
int wlanPsEnabled(void);
int wlanPsDisabled(void);
int wlanIbssPsEnabled(void);
int wlanApPsEnabled(void);

#endif /* ROBIN_CONN_H */
