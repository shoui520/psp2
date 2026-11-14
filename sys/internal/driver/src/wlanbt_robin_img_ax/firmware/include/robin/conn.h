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

struct WlanApPsEntry;

/* Frames an access point keeps for a peer in power save (see robin/apps.h). */
struct WlanApPsQueue {
	uint32_t count;
	struct WlanApPsEntry *head;
	struct WlanApPsEntry *tail;
};

/* Per-association information of a station or access point connection (partial). */
struct WlanConnectionInfo {
	uint8_t reserved0[31];
	uint8_t txRate;			/* fixed rate code for data frames; 0: chosen per frame */
	uint8_t maxStations;		/* stations an access point admits */
	uint8_t reserved33[35];
	uint32_t securityEnabled;	/* nonzero: securityParams apply */
	uint8_t securityParams[28];
	uint8_t securityState[256];
	uint8_t ready;
	uint8_t reserved357[184];
	uint8_t protectionFlags;	/* bit 0: protection forced on */
	uint8_t reserved542[10];
	const uint8_t *smState;		/* the association state machine (mdevRomQueryState) */
	uint8_t reserved556[24];
	uint8_t capability[2];		/* bit 5: short preamble; bit 10: short slot time */
	uint8_t reserved582[2];
	uint32_t dataAllowed;		/* 0: data frames to this peer are dropped */
	uint8_t phyMode;		/* 0, 1, 2: 802.11b only */
	uint8_t erpInfo;		/* the ERP information element's value */
	uint8_t nonErpStations;		/* nonzero: protection needed */
	uint8_t longSlotStations;	/* nonzero: the long slot time is needed */
	uint8_t rateFlags;		/* bit 3: send null frames at a low rate; bit 4: protection */
	uint8_t reserved593[3];
	uint8_t olbcDetected;		/* an overlapping legacy BSS was seen */
	uint8_t olbcActive;
	uint8_t olbcCount;
	uint8_t reserved599;
	uint32_t olbcTimeout;		/* beacon intervals in 5 s, plus one */
	uint8_t reserved604[156];
	uint8_t bssStarted;		/* 1: the BSS of a station or access point role is up */
	uint8_t reserved761[3];
	uint32_t dozingStations;	/* peers in power save */
	struct WlanApPsQueue groupQueue;	/* group-addressed frames kept for the DTIM beacon */
	uint8_t reserved780[104];
	uint32_t listenInterval;	/* the peer is in power save */
	uint32_t listenIntervalPs;	/* the same with WMM power save */
	uint8_t reserved892[252];
	uint8_t groupDeliveryBusy;
	uint8_t reserved1145[3];
	uint16_t groupDeliveryMode;	/* 2: group frames may wake the host side */
	uint8_t reserved1150[70];
	uint32_t groupDeliveryFlags;	/* bit 0: data frames follow the group frames */
};

struct WlanSubscribeEvents;

/*
 * The block of a station (at stationInfo) or access point (at apInfo)
 * connection that starts with its information pointer (partial).
 */
struct WlanConnectionRole {
	struct WlanConnectionInfo *info;
	uint8_t reserved4[4];
	uint8_t securityFlags;		/* bits 0-1: a key handshake is under way */
	uint8_t reserved9[3];
	uint32_t listenInterval;
	uint8_t reserved16[4];
	struct WlanApPsQueue psQueue;	/* the peer's frames while it dozes */
	uint8_t peerPowerSave;		/* 1: the peer is in power save */
	uint8_t psCounted;		/* counted in the stations sharing the buffers */
	uint8_t reserved34[54];
	uint8_t state;			/* 802.11 state: 1 unauthenticated, 2 authenticated, 3 associated */
	uint8_t reserved89[7];
	struct WlanApPsQueue uapsdQueue;	/* frames of delivery-enabled access categories */
	uint8_t reserved108[4];
	struct {
		uint8_t reserved[5];
		uint8_t deliveryEnabled;	/* bits 3..0: AC 0..3 is delivery-enabled */
	} uapsd;
	uint8_t fullReported;		/* the host was told the peer's buffers are full */
};

_Static_assert(offsetof(struct WlanConnectionRole, state) == 88, "role state");
_Static_assert(offsetof(struct WlanConnectionRole, psQueue) == 20, "role power-save queue");
_Static_assert(offsetof(struct WlanConnectionRole, uapsdQueue) == 96, "role U-APSD queue");
_Static_assert(offsetof(struct WlanConnectionRole, fullReported) == 118, "role buffers full");
_Static_assert(offsetof(struct WlanConnectionRole, listenInterval) == 12, "role listen interval");

/*
 * The record an event is posted to a connection's state machine with
 * (mdevRomQueryState): a received frame, a host command or a message.
 */
struct WlanSmRecord {
	uintptr_t argument;
	const void *frame;
	const uint8_t *command;		/* the host command's parameters */
	const void *message;
};

struct WlanMacTxRequest;

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
	/* The station state machine (driven through mdevRomQueryState from here). */
	const uint8_t *smState;		/* the current state's entry in smStates */
	uint32_t reserved20;
	const uint8_t *smStates;
	uint8_t reserved1a[16];
	int (*rxManagement)(OsaBuffer *buffer, uintptr_t argument);	/* management frames for this connection */
	uint8_t reserved1f[4];
	uint8_t dataRate;		/* fixed rate index for unicast data; 254: chosen per frame */
	uint8_t groupRate;		/* fixed rate index for group-addressed frames; 254: none */
	uint8_t hostFlags;		/* bit 4: frames go to the host as Ethernet II */
	uint8_t txOptions;		/* bit 1: passed to the frame builder; bit 3: any rate allowed;
				   bit 5: the BSS uses ERP protection */
	uint8_t bssCapability[2];	/* capability information of the IBSS started or joined */
	uint8_t reserved58[6];
	uint8_t supportedRates[14];
	uint8_t supportedRatesLength;
	uint8_t reserved1c[6];
	uint8_t beaconMissThreshold;	/* beacons missed before the link is lost (subscribed) */
	uint8_t preBeaconLostThreshold;
	uint8_t htCapability[28];	/* WlanHtCapabilityIe; its rates count in the rate bitmap */
	uint8_t reserved115;
	uint8_t networkSsid[32];	/* the SSID of the IBSS started */
	uint8_t networkSsidLength;
	uint8_t listenIntervalBase;	/* an unauthenticated peer's listen interval is 2n + 1 */
	uint16_t beaconPeriod;		/* TU, of the IBSS started or joined */
	uint8_t localAddress[6];
	uint8_t reserved2[14];
	uint8_t channel;
	uint8_t groupRing;		/* the transmit ring of the BSS's group frames */
	uint8_t reserved174[2];
	struct WlanMacTxRequest *beaconRequest;	/* the MAC's request for the beacon template */
	OsaBuffer *beaconBuffer;	/* the beacon template; the frame at its data offset */
	struct WlanMacTxRequest *probeResponseRequest;
	OsaBuffer *probeResponseBuffer;	/* the probe response template, from wlanMgmtAllocFrame() */
	uint8_t reserved192[14];
	int16_t noise;			/* dBm, last measured */
	uint8_t reserved3b[2];
	int16_t snr;
	uint8_t reserved3c[168];
	uint8_t state;
	uint8_t peerAddress[6];
	uint8_t reserved4a;
	uint16_t associationId;		/* AID the access point assigned */
	uint8_t reserved4[6];
	union {
		uint8_t peerLinkState[240];	/* cleared when the connection takes a new peer */
		struct {
			uint8_t reserved396[12];
			uint8_t rateControl[228];	/* rate adaptation state (not converted yet) */
		};
	};
	uint8_t reserved636[52];
	struct WlanSubscribeEvents *subscribeEvents[1];	/* HostCmd_CMD_802_11_SUBSCRIBE_EVENT */
	uint16_t subscribeTriggered;	/* WLAN_SUBSCRIBE_* events met, not yet reported */
	uint8_t reserved4c[30];
	int32_t txPowerLimit;		/* dBm from the channel power table; 255: none */
	uint32_t qosPeer;		/* 1: the peer associated with WMM (QoS) */
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
		struct {
			uint8_t reserved752[8];
			uint32_t stationFlags;	/* station; bit 7: host frames are not routed by destination;
						   IBSS: bit 0 merges with other networks, bit 1 the
						   beacon timing changed, bit 6 a peer was heard */
			uint8_t reserved764[4];
			uint32_t lastBeaconTime[2];	/* IBSS: when a beacon of ours was last matched */
			uint8_t reserved776[6];
			uint8_t securitySuites[2];	/* IBSS: compared with macMgmtDispatchEntry()'s fourth, third results */
		};
	};
	uint8_t initiator;
	uint8_t reserved785;
	uint16_t capability;
	uint8_t reserved5[4];
	union {
		uint32_t scheduleReference;	/* low word of peerTimestamp */
		uint8_t peerTimestamp[8];	/* the TSF of the peer's beacon, taken when scheduleValid */
	};
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
_Static_assert(offsetof(struct WlanConnection, smStates) == 24, "state machine states");
_Static_assert(offsetof(struct WlanConnection, rxManagement) == 44, "management handler");
_Static_assert(offsetof(struct WlanConnection, peerLinkState) == 396, "peer link state");
_Static_assert(offsetof(struct WlanConnection, dataRate) == 52, "data rate");
_Static_assert(offsetof(struct WlanConnection, hostFlags) == 54, "host flags");
_Static_assert(offsetof(struct WlanConnection, state) == 380, "state");
_Static_assert(offsetof(struct WlanConnection, stationFlags) == 760, "station flags");
_Static_assert(offsetof(struct WlanConnection, supportedRates) == 64, "rates");
_Static_assert(offsetof(struct WlanConnection, localAddress) == 152, "local address");
_Static_assert(offsetof(struct WlanConnection, rateControl) == 408, "rate control");
_Static_assert(offsetof(struct WlanConnection, beaconMissThreshold) == 85, "beacon miss threshold");
_Static_assert(offsetof(struct WlanConnection, subscribeEvents) == 688, "subscribed events");
_Static_assert(offsetof(struct WlanConnection, subscribeTriggered) == 692, "subscribed events met");
_Static_assert(offsetof(struct WlanConnectionInfo, ready) == 356, "info ready");
_Static_assert(offsetof(struct WlanConnectionInfo, smState) == 552, "info state machine");
_Static_assert(offsetof(struct WlanConnectionInfo, securityEnabled) == 68, "info security");
_Static_assert(offsetof(struct WlanConnectionInfo, protectionFlags) == 541, "info protection");
_Static_assert(offsetof(struct WlanConnectionInfo, capability) == 580, "info capability");
_Static_assert(offsetof(struct WlanConnectionInfo, phyMode) == 588, "info PHY mode");
_Static_assert(offsetof(struct WlanConnectionInfo, olbcTimeout) == 600, "info OLBC timeout");
_Static_assert(offsetof(struct WlanConnectionInfo, bssStarted) == 760, "info BSS started");
_Static_assert(offsetof(struct WlanConnectionInfo, listenInterval) == 884, "info listen interval");
_Static_assert(offsetof(struct WlanConnectionInfo, groupQueue) == 768, "info group queue");
_Static_assert(offsetof(struct WlanConnectionInfo, groupDeliveryMode) == 1148, "info group delivery");
_Static_assert(offsetof(struct WlanConnectionInfo, groupDeliveryFlags) == 1220, "info group delivery flags");
_Static_assert(offsetof(struct WlanConnection, txPowerLimit) == 724, "transmit power limit");
_Static_assert(offsetof(struct WlanConnectionInfo, dataAllowed) == 584, "info data allowed");
_Static_assert(offsetof(struct WlanConnectionInfo, rateFlags) == 592, "info rate flags");
_Static_assert(offsetof(struct WlanConnection, channel) == 172, "channel");
_Static_assert(offsetof(struct WlanConnection, beaconBuffer) == 180, "beacon template");
_Static_assert(offsetof(struct WlanConnection, probeResponseBuffer) == 188, "probe response template");
_Static_assert(offsetof(struct WlanConnection, noise) == 206, "noise");
_Static_assert(offsetof(struct WlanConnection, snr) == 210, "SNR");
_Static_assert(offsetof(struct WlanConnection, state) == 380, "state");
_Static_assert(offsetof(struct WlanConnection, peerAddress) == 381, "peer address");
_Static_assert(offsetof(struct WlanConnection, bssCapability) == 56, "BSS capability");
_Static_assert(offsetof(struct WlanConnection, networkSsid) == 116, "network SSID");
_Static_assert(offsetof(struct WlanConnection, beaconPeriod) == 150, "beacon period");
_Static_assert(offsetof(struct WlanConnection, beaconRequest) == 176, "beacon request");
_Static_assert(offsetof(struct WlanConnection, lastBeaconTime) == 768, "last beacon time");
_Static_assert(offsetof(struct WlanConnection, securitySuites) == 782, "security suites");
_Static_assert(offsetof(struct WlanConnection, associationId) == 388, "association ID");
_Static_assert(offsetof(struct WlanConnection, interfaceAddress) == 732, "interface address");
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
struct WlanConnectionRole *wlanConnectionInfoSlot(struct WlanConnection *connection);
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
unsigned int wlanRaiseEvent(unsigned int bit);
int wlanRoleOtherOnly(void);
int wlanRoleIbssOnly(void);
int wlanPsEnabled(void);
int wlanPsDisabled(void);
int wlanIbssPsEnabled(void);
int wlanApPsEnabled(void);

#endif /* ROBIN_CONN_H */
