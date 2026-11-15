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
#include "robin/ratectl.h"
#include "robin/softtimer.h"
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/esatypes.h"
#include "robin/threadx.h"

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

/*
 * The power-save state machine of a connection (wlan/pssm.c), driven
 * through mdevRomQueryState(). Off: power save is off; On: it is on, Confirm
 * (the host and the peers are told), Sleep and Awake below it; Ieee: the
 * 802.11 power-management exchange of null frames, IeeeNull and IeeeExit
 * below it.
 */
struct WlanPsMachine {
	struct RomHsmState *state;	/* current state */
	struct RomHsmState root;
	struct RomHsmState off;
	struct RomHsmState on;
	struct RomHsmState confirm;
	struct RomHsmState sleep;
	struct RomHsmState awake;
	struct RomHsmState ieee;
	struct RomHsmState ieeeNull;
	struct RomHsmState ieeeExit;
	TX_TIMER confirmTimer;		/* "SleepConfirmTmr": the host's sleep confirmation */
	uint32_t awakeReportDue;	/* tell the host when awake (event 0x1000) */
	uint32_t sleepReportDue;	/* tell the host before sleeping (event 0x2000) */
	uint32_t announced;		/* the sleep announcement went out, or none is needed */
	uint32_t hostConfirmed;		/* the host confirmed the sleep */
	uint32_t sleepBlocked;		/* traffic: do not sleep */
	uint32_t nullAcked;		/* the power-management null frame is done */
	uint32_t txPending;		/* frames are waiting to be sent */
	uint8_t nullRetries;
	uint8_t nullFailed;
	uint8_t reserved186[2];
	TX_TIMER nullTimer;		/* "AP_NullPktDoneTmr" */
	struct RomHsmState *resume;	/* where IeeeNull and IeeeExit return to */
	uint8_t ieeeState;		/* 1: PM=1 sent, 2: PM=0 sent, 3: gave up */
	uint8_t ieeeTries;
	uint8_t eventPending;		/* control-task event 17 is owed */
	uint8_t reserved239;
	uint32_t wakeBits;		/* macTxAcquireWakeBit() */
	struct WlanConnection *connection;
	uint8_t reserved248[4];
};

_Static_assert(offsetof(struct WlanPsMachine, confirmTimer) == 112, "PS confirm timer");
_Static_assert(offsetof(struct WlanPsMachine, nullTimer) == 188, "PS null timer");
_Static_assert(offsetof(struct WlanPsMachine, resume) == 232, "PS resume state");
_Static_assert(offsetof(struct WlanPsMachine, connection) == 244, "PS connection");
_Static_assert(sizeof(struct WlanPsMachine) == 252, "PS machine");

/* WlanPsSleep.mode */
#define WLAN_PS_SLEEP_INACTIVITY	1	/* sleep after the inactivity timeout */
#define WLAN_PS_SLEEP_DTIM		2	/* sleep between DTIM beacons */

/* When the device sleeps (the host's sleep parameters, times in us). */
struct __attribute__((packed, aligned(4))) WlanPsSleep {
	uint8_t recentActivity;		/* the inactivity timer runs */
	uint8_t sleepAllowed;
	uint8_t sleepTimerArmed;
	uint8_t reserved3;
	uint16_t mode;			/* WLAN_PS_SLEEP_* */
	uint8_t reserved6[2];
	uint32_t sleepDuration;
	uint32_t wakeLatency;		/* the longest wake-up seen */
	uint64_t wakeAt;
	uint64_t sleptAt;
	uint64_t lastActivity;
	uint64_t tbttReference;		/* when beacon count 0 was due */
	uint64_t tsfReference;
	uint32_t dtimCount;		/* beacons to the next DTIM at tbttReference */
	uint16_t beaconPeriod;		/* TU */
	uint8_t reserved62[2];
	uint32_t beaconInterval;
	uint32_t sleepTimer;		/* osaCreateTimer() handles */
	uint32_t activityTimer;
	uint32_t control;		/* bit 0: announce the sleep */
	uint32_t minSleep;
	uint32_t maxSleep;
	uint32_t inactivityTimeout;
	uint32_t minAwake;
	uint32_t maxAwake;
};

_Static_assert(offsetof(struct WlanPsSleep, wakeAt) == 16, "PS wake time");
_Static_assert(offsetof(struct WlanPsSleep, tbttReference) == 40, "PS TBTT reference");
_Static_assert(offsetof(struct WlanPsSleep, sleepTimer) == 68, "PS sleep timer");
_Static_assert(sizeof(struct WlanPsSleep) == 100, "PS sleep parameters");

/* Per-association information of a station or access point connection (partial). */
/* WlanConnectionInfo.rateFlags (names inferred from use) */
#define WLAN_RATE_FLAG_OLBC		0x01	/* an overlapping legacy BSS is around */
#define WLAN_RATE_FLAG_OLBC_ERP		0x02
#define WLAN_RATE_FLAG_ERP		0x04	/* the ERP element is not zero */
#define WLAN_RATE_FLAG_LOW_NULL		0x08	/* send null frames at a low rate */
#define WLAN_RATE_FLAG_PROTECTION	0x10
#define WLAN_RATE_FLAG_HT_OBSS		0x20	/* HT protection for another BSS's stations */

struct WlanConnectionInfo {
	uint8_t reserved0[30];
	uint8_t unicastRate;		/* fixed rate code for unicast data frames; 0: chosen per frame */
	uint8_t txRate;			/* fixed rate code for data frames; 0: chosen per frame */
	uint8_t maxStations;		/* stations an access point admits */
	uint8_t reserved33[3];
	uint32_t qosEnabled;		/* 1: frames go out with WMM queues */
	uint8_t reserved40[10];
	uint8_t wmmRecords[4][4];	/* the AP's WMM parameter records of the four access categories (info, contention, TXOP) */
	uint8_t reserved66[2];
	uint32_t htEnabled;		/* nonzero: the access point's HT elements apply */
	uint8_t htCapability[28];	/* the access point's HT capability element */
	uint8_t htOperation[24];	/* its HT operation element */
	struct WlanUapContext *uapContext;	/* the SQRAM context of the access point's association table */
	uint8_t reserved128[104];
	apRsnConfig_t apRsn;		/* the access point's RSN settings */
	uint8_t ready;
	uint8_t reserved357[184];
	uint8_t protectionFlags;	/* bit 0: protection forced on */
	uint8_t reserved542[2];
	uint8_t pktFwdCtl;		/* WLAN_PKT_FWD_*: what the access point does with frames between its stations (inferred) */
	uint8_t reserved545[7];
	const uint8_t *smState;		/* the association state machine (mdevRomQueryState) */
	uint8_t reserved556[20];
	struct WlanConnection *connection;	/* the connection that owns this block */
	uint8_t capability[2];		/* bit 5: short preamble; bit 10: short slot time */
	uint8_t reserved582[2];
	uint32_t dataAllowed;		/* 0: data frames to this peer are dropped */
	uint8_t phyMode;		/* 0, 1, 2: 802.11b only */
	uint8_t erpInfo;		/* the ERP information element's value */
	uint8_t nonErpStations;		/* nonzero: protection needed */
	uint8_t longSlotStations;	/* nonzero: the long slot time is needed */
	uint8_t rateFlags;		/* WLAN_RATE_FLAG_* */
	uint8_t htMixedStations;	/* what the HT operating mode is derived from (inferred): non-HT stations */
	uint8_t htNonGreenfieldStations;
	uint8_t ht20MhzStations;
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
	uint8_t reserved780[4];
	uint32_t updatePassPhrase;	/* the PSK is to be derived again from the pass phrase */
	uint32_t grpRekeyTimer;		/* osaCreateTimer() handle of the group rekey timer */
	uint32_t grpRekeyCntRemaining;	/* group rekey timer ticks left; 0: no periodic rekey */
	MIC_Error_t apMicError;		/* the TKIP countermeasures */
	uint32_t apMicTimer;		/* osaCreateTimer() handle of their 60 s timer */
	KeyData_t grpKeyData;		/* the group key; its index is what group-addressed frames are sent with */
	uint8_t GNonce[NONCE_SIZE];
	uint32_t listenInterval;	/* the peer is in power save */
	uint32_t listenIntervalPs;	/* the same with WMM power save */
	struct WlanPsMachine psMachine;
	struct WlanPsSleep sleep;
};

struct WlanSubscribeEvents;
struct WlanUapContext;

/*
 * The block of a station (at stationInfo) or access point (at apInfo)
 * connection that starts with its information pointer (partial).
 */
struct WlanConnectionRole {
	struct WlanConnectionInfo *info;
	OsaBuffer *infoBuffer;		/* the buffer the information block is in */
	uint8_t channelFlags;		/* WlanChannelFlags of the role's channel: the band is in bits 0-1 */
	uint8_t channel;		/* the primary channel */
	uint8_t reserved10[2];
	uint32_t listenInterval;
	uint8_t reserved16[4];
	struct WlanApPsQueue psQueue;	/* the peer's frames while it dozes */
	uint8_t peerPowerSave;		/* 1: the peer is in power save */
	uint8_t psCounted;		/* counted in the stations sharing the buffers */
	uint8_t reserved34[2];
	apKeyMgmtInfoSta_t keyMgmt;	/* an access point's handshake with the peer */
	uint8_t state;			/* 802.11 state: 1 unauthenticated, 2 authenticated, 3 associated */
	uint8_t reserved89[7];
	struct WlanApPsQueue uapsdQueue;	/* frames of delivery-enabled access categories */
	uint8_t reserved108[4];
	struct {
		uint8_t inServicePeriod;	/* a trigger frame started a service period */
		uint8_t spRemaining;		/* frames of the service period not yet sent */
		uint8_t spSent;			/* frames sent in the service period so far */
		uint8_t qosInfo;		/* the peer's QoS Info: bits 5-6 Max SP Length */
		uint8_t triggerEnabled;		/* bits 3..0: AC 0..3 is trigger-enabled */
		uint8_t deliveryEnabled;	/* bits 3..0: AC 0..3 is delivery-enabled */
	} uapsd;
	uint8_t fullReported;		/* the host was told the peer's buffers are full */
};

_Static_assert(offsetof(struct WlanConnectionRole, state) == 88, "role state");
_Static_assert(offsetof(struct WlanConnectionRole, keyMgmt) == 36, "role key management");
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

/*
 * The signal measurements of a connection (wlan/signal.c). A filter keeps
 * the last three samples: their mean while they lie within 3 dB of each
 * other, else the lowest.
 */
#define WLAN_SIGNAL_FILTERS		3	/* 0: frame noise floor, 1: join noise floor */
#define WLAN_SIGNAL_UNKNOWN		(-98)	/* dBm, nothing measured yet */

struct WlanSignalFilter {
	int32_t samples[3];		/* oldest first */
	int32_t value;
	uint8_t count;			/* samples taken, up to 3 */
	uint8_t reserved17[3];
};

/* A running average: the sum of 1 << shift samples, then average << shift. */
struct WlanSignalAverage {
	int32_t sum;
	int32_t count;
};

/* The link check: frames heard, or beacons missed since silentSince. */
struct WlanLinkMonitor {
	uint64_t silentSince;		/* when the silence began (us) */
	uint32_t timer;			/* the check's timer */
	uint32_t frameSeen;		/* a frame was received or acknowledged */
	uint32_t silent;		/* nothing was heard in the last check */
	uint8_t nullFrames;		/* null frames sent to probe the peer */
	uint8_t reserved21[3];
};

_Static_assert(sizeof(struct WlanSignalFilter) == 20, "signal filter");
_Static_assert(sizeof(struct WlanLinkMonitor) == 24, "link monitor");

/* The pointers a station, IBSS or AMP link keeps to the HT elements it sends (at its own place in the connection). */
struct __attribute__((may_alias)) WlanHtElements {
	const uint8_t *htOperation;	/* the HT operation element (24 bytes) */
	const uint8_t *htCoexistence;	/* the 20/40 BSS coexistence element (3 bytes) */
	const void *reserved8;
	const uint8_t *overlapScan;	/* the overlapping BSS scan parameters element (16 bytes) */
};

/* WlanConnection.flags */
#define WLAN_CONN_ENCRYPTED		0x01	/* a frame without a key is dropped (inferred from wlanTxBuildFrame()) */
#define WLAN_CONN_JOIN_OPEN		0x02	/* the join ended without privacy */
#define WLAN_CONN_PRIVACY		0x04	/* WPA or RSN */
#define WLAN_CONN_WAPI			0x10

/*
 * WlanConnectionInfo.pktFwdCtl of an access point (Marvell's pkt_fwd_ctl; the
 * meaning of each bit is inferred from the receive path).
 */
#define WLAN_PKT_FWD_RELAY		0x01	/* the firmware relays the frames between its stations */
#define WLAN_PKT_FWD_NO_GROUP_RELAY	0x02	/* ... but not the group-addressed frames */
#define WLAN_PKT_FWD_DROP_INTRA_BSS	0x04	/* frames for another station of the BSS are dropped */
#define WLAN_PKT_FWD_DROP_INTER_BSS	0x08	/* frames for a station of another BSS are dropped */

/* WlanConnection.flags2 */
#define WLAN_CONN2_QOS			0x08	/* the peer uses QoS (inferred) */
#define WLAN_CONN2_ASSOCIATED		0x10	/* the peer is associated */

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
	uint8_t reserved1a[12];
	struct WlanConnection *owner;	/* itself: the connection's state machine is handed this */
	int (*rxManagement)(OsaBuffer *buffer, uintptr_t argument);	/* management frames for this connection */
	int (*rxControl)(OsaBuffer *buffer, uintptr_t argument);	/* control frames for this connection */
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
	uint8_t dtimPeriod;		/* beacons per DTIM; an unauthenticated peer is held to 2n + 1 */
	uint16_t beaconPeriod;		/* TU, of the IBSS started or joined */
	uint8_t localAddress[6];
	/* HostCmd_CMD_802_11_SNMP_MIB's values (Marvell: RTS and fragmentation thresholds, retry limits). */
	uint8_t rtsThreshold[2];
	uint8_t fragThreshold[2];
	uint8_t shortRetryLimit;
	uint8_t longRetryLimit;
	uint8_t dtimHold;		/* nonzero: the group frames are not released at the DTIM */
	uint8_t beaconsPerDtim;		/* the DTIM period */
	uint8_t beaconsSinceDtim;	/* beacons since the last DTIM */
	uint8_t reserved167[5];
	uint8_t channel;
	uint8_t groupRing;		/* the transmit ring of the BSS's group frames */
	uint8_t reserved174[2];
	struct WlanMacTxRequest *beaconRequest;	/* the MAC's request for the beacon template */
	OsaBuffer *beaconBuffer;	/* the beacon template; the frame at its data offset */
	struct WlanMacTxRequest *probeResponseRequest;
	OsaBuffer *probeResponseBuffer;	/* the probe response template, from wlanMgmtAllocFrame() */
	uint32_t rateSet;		/* bit n: rate index n is in the BSS's rate set (wlanRateSetLimits()) */
	uint8_t ofdmHigh;		/* the highest and lowest rate index of each class in it */
	uint8_t ofdmLow;
	uint8_t cckHigh;
	uint8_t cckLow;
	/* The signal of the data frames and the beacons (wlan/signal.c), dBm. */
	int16_t dataNoiseFloor;		/* the last data frame's */
	int16_t dataRssi;
	int16_t dataSnr;
	int16_t noise;			/* the last beacon's noise floor */
	int16_t beaconRssi;
	int16_t snr;			/* the last beacon's SNR */
	int16_t beaconRssiLast;		/* HostCmd_CMD_RSSI_INFO's value while beaconRssi is -98 */
	int16_t noiseLast;		/* ... and the noise floor's */
	struct WlanLinkMonitor link;
	struct WlanSignalFilter signalFilters[WLAN_SIGNAL_FILTERS];
	struct WlanSignalAverage noiseFloorAverage;	/* the beacons' noise floor */
	struct WlanSignalAverage beaconRssiCck;		/* beacons at a DSSS/CCK rate */
	struct WlanSignalAverage beaconRssiOfdm;	/* beacons at an OFDM rate */
	struct WlanSignalAverage joinNoiseFloor;	/* the frames of the join */
	struct WlanSignalAverage joinRssiCck;
	struct WlanSignalAverage joinRssiOfdm;
	struct WlanSignalAverage spareAverage;		/* cleared by wlanSignalInit() only */
	struct WlanSignalAverage beaconRssiAverage;	/* over 1 << beaconRssiShift beacons */
	struct WlanSignalAverage dataRssiAverage;	/* over 1 << dataRssiShift frames */
	uint16_t beaconRssiShift;
	uint16_t dataRssiShift;
	uint16_t lastSequence;		/* 0xffff: none seen yet */
	uint16_t lastFragment;		/* the fragment number of the last frame; 0: none under way (inferred) */
	uint8_t state;
	uint8_t peerAddress[6];
	uint8_t reserved4a;
	uint16_t associationId;		/* AID the access point assigned */
	uint8_t reserved4[6];
	union {
		uint8_t peerLinkState[240];	/* cleared when the connection takes a new peer */
		struct {
			uint8_t reserved396[12];
			struct WlanRateControl rateControl;	/* rate adaptation state */
		};
	};
	struct WlanSoftTimer softTimer;	/* the connection's timeout */
	uint32_t reserved660;
	uint32_t reserved664;		/* cleared when the connection is configured */
	uint32_t reserved668;
	uint8_t *keyBlocks[2];		/* [0]: the group key's (HostCmd 0x37's), [1]: the pairwise key's (0x36's and 0x5e's); each +28 is its cipher_key_t */
	union {
		uint8_t *radioParams;	/* the radio configuration's block this connection's SNMP values use */
		eapolHskData_t *hskData;	/* an access point's peer: the handshake's nonce and key */
	};
	uint8_t reserved684[4];
	struct WlanSubscribeEvents *subscribeEvents[1];	/* HostCmd_CMD_802_11_SUBSCRIBE_EVENT */
	uint16_t subscribeTriggered;	/* WLAN_SUBSCRIBE_* events met, not yet reported */
	uint8_t reserved694[14];
	uint16_t txSequence[8];		/* the next sequence number of each TID towards the peer */
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
			uint32_t peerCheckTime;	/* IBSS: the link check's last time (wlanIbssCheckPeers) */
			uint8_t peerCheckMisses;	/* ... and the checks that found the peer silent */
			uint8_t reserved781;
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
	uint8_t linkFlags;		/* AMP link: 3..6 the 4-way handshake (esa/btampKeyMgmt.c), 7 up */
	WlanTxFrameHeader txHeader;
	uint8_t reserved6[11];
	uint32_t reserved7;
	uint32_t timerHandle;
	uint32_t secondTimerHandle;
	uint8_t retryCount;
	uint8_t rsnElement[63];		/* AMP link: its RSN element (ID, length, body), for EAPOL-Key */
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
_Static_assert(offsetof(struct WlanConnection, rtsThreshold) == 158, "SNMP MIB values");
_Static_assert(offsetof(struct WlanConnection, localName) == 744, "AMP link name");
_Static_assert(offsetof(struct WlanConnection, keyBlocks) == 672, "SNMP BSS parameters");
_Static_assert(offsetof(struct WlanConnection, beaconMissThreshold) == 85, "beacon miss threshold");
_Static_assert(offsetof(struct WlanConnection, subscribeEvents) == 688, "subscribed events");
_Static_assert(offsetof(struct WlanConnection, subscribeTriggered) == 692, "subscribed events met");
_Static_assert(offsetof(struct WlanConnectionInfo, ready) == 356, "info ready");
_Static_assert(offsetof(struct WlanConnectionInfo, smState) == 552, "info state machine");
_Static_assert(offsetof(struct WlanConnectionInfo, htEnabled) == 68, "info HT enabled");
_Static_assert(offsetof(struct WlanConnectionInfo, protectionFlags) == 541, "info protection");
_Static_assert(offsetof(struct WlanConnectionInfo, pktFwdCtl) == 544, "info packet forwarding");
_Static_assert(offsetof(struct WlanConnectionInfo, capability) == 580, "info capability");
_Static_assert(offsetof(struct WlanConnectionInfo, phyMode) == 588, "info PHY mode");
_Static_assert(offsetof(struct WlanConnectionInfo, olbcTimeout) == 600, "info OLBC timeout");
_Static_assert(offsetof(struct WlanConnectionInfo, bssStarted) == 760, "info BSS started");
_Static_assert(offsetof(struct WlanConnectionInfo, listenInterval) == 884, "info listen interval");
_Static_assert(offsetof(struct WlanConnectionInfo, groupQueue) == 768, "info group queue");
_Static_assert(offsetof(struct WlanConnectionInfo, grpKeyData.KeyIndex) == 850, "info group key index");
_Static_assert(offsetof(struct WlanConnectionInfo, apRsn) == 232, "info RSN configuration");
_Static_assert(offsetof(struct WlanConnectionInfo, GNonce) == 852, "info group nonce");
_Static_assert(offsetof(struct WlanConnectionInfo, psMachine) == 892, "info PS machine");
_Static_assert(offsetof(struct WlanConnectionInfo, sleep) == 1144, "info sleep parameters");
_Static_assert(offsetof(struct WlanConnectionInfo, qosEnabled) == 36, "info QoS");
_Static_assert(offsetof(struct WlanConnection, txPowerLimit) == 724, "transmit power limit");
_Static_assert(offsetof(struct WlanConnectionInfo, dataAllowed) == 584, "info data allowed");
_Static_assert(offsetof(struct WlanConnectionInfo, rateFlags) == 592, "info rate flags");
_Static_assert(offsetof(struct WlanConnection, channel) == 172, "channel");
_Static_assert(offsetof(struct WlanConnection, rateSet) == 192, "rate set");
_Static_assert(offsetof(struct WlanConnection, cckLow) == 199, "CCK low rate");
_Static_assert(offsetof(struct WlanConnection, beaconBuffer) == 180, "beacon template");
_Static_assert(offsetof(struct WlanConnection, probeResponseBuffer) == 188, "probe response template");
_Static_assert(offsetof(struct WlanConnection, noise) == 206, "noise");
_Static_assert(offsetof(struct WlanConnection, snr) == 210, "SNR");
_Static_assert(offsetof(struct WlanConnection, dataNoiseFloor) == 200, "data noise floor");
_Static_assert(offsetof(struct WlanConnection, link) == 216, "link monitor");
_Static_assert(offsetof(struct WlanConnection, signalFilters) == 240, "signal filters");
_Static_assert(offsetof(struct WlanConnection, noiseFloorAverage) == 300, "noise floor average");
_Static_assert(offsetof(struct WlanConnection, beaconRssiAverage) == 356, "beacon RSSI average");
_Static_assert(offsetof(struct WlanConnection, beaconRssiShift) == 372, "RSSI shifts");
_Static_assert(offsetof(struct WlanConnection, beaconMissThreshold) == 85, "beacon miss threshold");
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
_Static_assert(offsetof(struct WlanConnection, rsnElement) == 877, "AMP RSN element");
_Static_assert(offsetof(struct WlanConnection, transmitBusy) == 940, "transmit busy");

/* wlan/conn.c */
extern struct WlanConnection *wlanConnectionList;
extern struct WlanConnection *wlanConnectionFreeList;
extern struct WlanConnection *wlanConnectionSession;
extern struct WlanConnection *wlanConnectionPrimary;
#define WLAN_CONNECTION_SLOTS	4

/* The timing record of a BSS the join and start code keep (the beacon time and the next target time). */
struct WlanBssTiming {
	uint32_t timestamp[2];
	uint32_t next[2];		/* 64-bit time of the next target; 0: none yet */
	uint32_t reserved16[2];
	uint32_t interval;		/* the target's period */
};

extern uint32_t wlanConnectionCount;
extern uint8_t wlanChannelInUse[1];
extern uint8_t wlanConnectionSlotMask;
extern uint32_t wlanConnectionDtimHolds;
extern struct WlanConnection *wlanConnectionSlots[WLAN_CONNECTION_SLOTS];

int wlanConnectionGetCount(void);						/* 0x1cb00 */
int wlanConnectionIncrement(void);						/* 0x1cb06 */
uint32_t wlanConnectionDtimHoldCount(void);					/* 0x1cb1a */
int wlanConnectionSlotAcquire(struct WlanConnection *connection);		/* 0x1cb20 */
int wlanConnectionSlotAttach(struct WlanConnection *connection);		/* 0x1cbce */
int wlanConnectionSlotDetach(struct WlanConnection *connection);		/* 0x1cc26 */
int wlanConnectionTeardownBss(struct WlanConnection *connection);		/* 0x1cc8e */
int wlanConnectionAnyStarted(unsigned int interfaceType);			/* 0x1cd74 */
int wlanConnectionCommandPrepare(unsigned int command, uintptr_t argument);	/* 0x1cdc8 */
int wlanConnectionCommandCheck(unsigned int command, uintptr_t argument);	/* 0x1ce46 */
int wlanConnectionIndexRelease(struct WlanConnection *connection);		/* 0x1ceb6 */
int wlanConnectionInfoAttach(struct WlanConnection *connection);		/* 0x1cf54 */
int wlanConnectionIndexAcquire(struct WlanConnection *connection);		/* 0x1cfb2 */
void wlanConnectionReset(struct WlanConnection *connection);			/* 0x1d052 */
void wlanConnectionClearSecurity(struct WlanConnection *connection);		/* 0x1d098 */
void wlanConnectionTxPowerUpdate(struct WlanConnection *connection, int enable);	/* 0x1d0ba */
void wlanConnectionTeardownIbss(struct WlanConnection *connection);		/* 0x1d0ea */
int wlanConnectionTeardownWds(struct WlanConnection *connection);		/* 0x1d146 */
void wlanConnectionSetupType3Info(struct WlanConnection *connection);			/* 0x1d20e */
void wlanConnectionSetupType2(struct WlanConnection *connection);			/* 0x1d2fc */
void wlanConnectionDtimHold(struct WlanConnection *connection);		/* 0x1d39c */
void wlanConnectionDtimRelease(struct WlanConnection *connection);		/* 0x1d3d2 */
struct WlanHtElements *wlanConnectionTypeBlock(struct WlanConnection *connection);	/* 0x1d3f2 */
void wlanConnectionSetupType3(struct WlanConnection *connection);		/* 0x1d412 */
void wlanConnectionSetupType0(struct WlanConnection *connection);		/* 0x1d452 */
void wlanConnectionSetupType1(struct WlanConnection *connection);		/* 0x1d486 */
int wlanConnectionIdle(struct WlanConnection *connection);			/* 0x1d4d0 */
int wlanConnectionFreeListClean(void);						/* 0x1d4e2 */
int wlanConnectionsInit(void);							/* 0x1d52e */
int wlanConnectionBssCount(struct WlanConnection *connection);		/* 0x1d5ba */
struct WlanConnection *wlanConnectionConfigure(int interfaceType, int bssType, int bssNumber,
					       const uint8_t *local, const uint8_t *peer,
					       int channel, void *driver);		/* 0x1d604 */
int wlanConnectionReconfigure(struct WlanConnection *connection, int interfaceType,
			      const uint8_t *local, const uint8_t *peer);			/* 0x1d934 */
struct WlanConnection *wlanConnectionDuplicateAp(const uint8_t *peer,
						 const struct WlanConnection *source);	/* 0x1da20 */
struct WlanConnection *wlanConnectionDuplicateType2(const uint8_t *peer,
						    const struct WlanConnection *source);	/* 0x1da78 */
void wlanConnectionDuplicateIbss(const uint8_t *peer, struct WlanConnection *source);		/* 0x1daba */
void wlanConnectionDuplicate(const uint8_t *peer, struct WlanConnection *source);		/* 0x1dc8c */

struct WlanConnection *wlanConnectionFindIbssBySsid(const uint8_t *ssidElement);	/* 0x1ddd6 */
int wlanIbssSecurityActive(void);							/* 0x1ded2 */
int wlanAnyConnectionTxAllowed(void);							/* 0x1e032 */
int wlanBssSelectAddress(unsigned int kind, const void *descriptor);			/* 0x1e064 */
int wlanBssAllowStart(unsigned int stage);						/* 0x1e0e8 */
int wlanBssRecordTiming(unsigned int kind, struct WlanBssTiming *timing);		/* 0x1e12a */
int wlanConnectionUpdatePeer(struct WlanConnection *connection, int argument);		/* 0x1e220 */
int wlanPeerQosEnabled(struct WlanConnection *connection, const uint8_t *peer);	/* 0x1e234 */
unsigned int wlanPeerNextSequence(struct WlanConnection *connection, unsigned int tid,
				  int advance);						/* 0x1e246 */
void wlanIbssLeave(struct WlanConnection *connection, int teardown);			/* 0x1e260 */
void wlanIbssLeaveAll(int teardown);							/* 0x1e29c */
void wlanIbssCheckPeers(void);								/* 0x1e2c8 */
int wlanIbssResetPeerCheck(struct WlanConnection *connection);				/* 0x1e378 */
int wlanBssQueuedFrames(const struct WlanConnection *bss);				/* 0x1e38e */
int wlanConnectionCountMode2(int stage);						/* 0x1e3cc */
int wlanTxPowerLimitMin(void);								/* 0x1e42e */
int wlanConnectionIndexAssign(const uint8_t *address);					/* 0x1e458 */
int wlanConnectionIndexUnassign(const uint8_t *address);				/* 0x1e476 */
struct WlanConnection *wlanConnectionSlotGet(unsigned int index);			/* 0x1e510 */
int wlanConnectionTeardownStation(struct WlanConnection *connection);			/* 0x1e51a */
int wlanConnectionTeardownType0(struct WlanConnection *connection);			/* 0x1e53c */

struct WlanConnection *wlanConnectionFind(unsigned int bssType, unsigned int bssNumber);
int wlanConnectionExists(unsigned int interfaceType);
struct WlanConnection *wlanConnectionFirst(unsigned int interfaceType);
struct WlanConnection *wlanConnectionFindByPeer(const uint8_t *peer);
struct WlanConnection *wlanConnectionFindByLocal(unsigned int interfaceType, const uint8_t *local);
struct WlanConnection *wlanConnectionFindDirectPeer(struct WlanConnection *connection,
						    const uint8_t *peer);	/* 0xfbf2 */
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

int wlanRoleMaskIs(unsigned int mask);
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
int wlanPsDisabled(void);

/* wlan/hostps.c */
extern uint8_t wlanConnectionPhase;
extern uint8_t wlanConnectionStage;

int wlanPsCancelPendingTimer(void);

/* wlan/psctl.c, used by most of the MAC; the rest is in robin/psctl.h */
extern uint32_t wlanPsOverride;

int wlanHasApRole(void);
int wlanRoleOtherOnly(void);
int wlanRoleIbssOnly(void);
int wlanPsEnabled(void);
int wlanIbssPsEnabled(void);
int wlanApPsEnabled(void);
unsigned int wlanRaiseEvent(unsigned int bit);

/* wlan/mgmtinfo.c: the ERP and HT state of an access point; erpInfo points to WlanConnectionInfo.erpInfo. */
void wlanApRateFlagsUpdate(unsigned int phyMode, uint8_t *erpInfo);			/* 0x2a50e */
void wlanApOlbcForget(uint8_t *erpInfo);						/* 0x2a53c */
int wlanRatesSelect(const uint8_t *rates, const uint8_t *first, const uint8_t *second,
		    uint8_t *out, uint8_t *count);					/* 0x2a594 */
int wlanFrameHasWpsIe(const uint8_t *request);						/* 0x2a5e2 */
int wlanRatesHaveOfdm(const uint8_t *rates, unsigned int count);			/* 0x2a62c */
int wlanRatesHaveCck(const uint8_t *rates, unsigned int count);			/* 0x2a664 */
unsigned int wlanTlvFrameDataAppend(uint8_t *tlv, const uint8_t *frame, uint16_t length); /* 0x2a68e */
void wlanHtOperationUpdate(struct WlanConnection *connection, uint8_t *erpInfo,
			   const uint8_t *params, uint8_t *htOperation);		/* 0x2a6de */
void wlanApHtStationsForget(uint8_t *erpInfo);						/* 0x2a778 */

#endif /* ROBIN_CONN_H */
