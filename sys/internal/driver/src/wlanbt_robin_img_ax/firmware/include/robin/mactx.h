/* Transmit-path state of the MAC: window queues, power-save mode, counters. */
#ifndef ROBIN_MACTX_H
#define ROBIN_MACTX_H

#include <stddef.h>
#include <stdint.h>
#include <wlanbt/wlanbt_protocol.h>
#include "robin/conn.h"
#include "robin/threadx.h"
#include "robin/wlan.h"

/* Queue of frames waiting in one hardware window (144 bytes, two windows). */
struct WlanTxWindowQueue {
	uint8_t pending;	/* frames handed to the hardware and not yet completed */
	uint8_t slot;		/* block-ack slot of the window being changed */
	uint16_t startSequence;	/* sequence control of the frame that queued first */
	uint16_t head;
	uint16_t tail;
	uint32_t entries[32];	/* frame data pointers (OsaBuffer.data) */
	uint32_t count;
	ULONG queuedAt;		/* sysGetTime() when the window started queueing */
};

_Static_assert(sizeof(struct WlanTxWindowQueue) == 144, "transmit window queue");
_Static_assert(offsetof(struct WlanTxWindowQueue, head) == 4, "window queue head");
_Static_assert(offsetof(struct WlanTxWindowQueue, count) == 136, "window queue count");

/* Power-save state kept per connection (partial; its owner is not converted yet). */
/* WlanPsPeerState.noaMode */
#define NOA_MODE_CTWINDOW	1
#define NOA_MODE_ABSENCE	2

/*
 * A persistent group the host stored (MRVL_WFD_PERSISTENT_GROUP_TLV,
 * hostCmd00ea()): its Group ID and Group BSSID attributes, the PSK, and the
 * clients a group owner keeps.
 */
struct WlanP2pPersistentGroup {
	struct {
		uint8_t id;			/* 15: the P2P Group ID attribute */
		uint8_t length[2];		/* 6 and the SSID's length */
		uint8_t deviceAddress[6];	/* the group owner's device address; zero: the slot is free */
		uint8_t ssid[32];
	} groupId;
	struct {
		uint8_t id;			/* 7: the P2P Group BSSID attribute */
		uint8_t length[2];
		uint8_t bssid[6];
	} groupBssid;
	uint8_t pskLength;
	uint8_t psk[40];
	uint8_t clients[2][6];			/* a group owner's clients */
	uint8_t clientCount;
	uint8_t role;				/* 2: the group owner, 3: a client */
};

_Static_assert(sizeof(struct WlanP2pPersistentGroup) == 105, "P2P persistent group");
_Static_assert(offsetof(struct WlanP2pPersistentGroup, clients) == 91, "P2P persistent group clients");

extern struct WlanP2pPersistentGroup p2pPersistentGroupStorage[2];	/* 0x59c70: the persistent groups (p2pPersistentGroupsInit()) */

struct WlanPsPeerState {
	uint8_t beaconTimestamp[8];	/* the TSF of the last beacon heard (a P2P group owner's) */
	uint32_t beaconTimerLow;	/* the MAC timer when it was heard */
	uint32_t beaconTimerHigh;
	uint32_t configuredMask;	/* what the host has configured of the group; the group starts when every bit p2pDirectAcquireState() requires is set */
	uint32_t discoveryParamsSet;	/* the discovery parameters the host set (hostCmd00ea()): bit 0 the discovery period, 1 scan enable, 2 the peer device ID, 3 the requested device type, 4 the device state */
	uint16_t minDiscoveryPeriod;	/* Marvell's MRVL_WFD_DISC_PERIOD_TLV: the listen state's shortest ... */
	uint16_t maxDiscoveryPeriod;	/* ... and longest period (100 TU units); 1 and 3 when the group starts */
	uint16_t deviceState;		/* MRVL_WFD_DEVICE_STATE_TLV, as the host set it */
	uint8_t p2pFlags;		/* what waits for the peer to be heard (p2pPeerFound()): bit 0 a GO negotiation, bit 1 an invitation (the P2P element names the device address), bit 2 a discoverability request (a P2P group owner's beacon carries the P2P element), bit 3 a provision discovery, bit 4 the pending frame */
	uint8_t tieBreaker;		/* rand() & 1 when the group starts (the GO Intent tie-breaker bit) */
	uint8_t scanEnable;		/* MRVL_WFD_SCAN_ENABLE_TLV */
	uint8_t requestedDeviceType[8];	/* MRVL_WFD_REQ_DEVICE_TYPE_TLV: the WPS primary device type searched for */
	uint8_t p2pAuthCount;		/* authentications under way with a P2P peer that has no context (decremented when one ends) */
	uint8_t groupClientCount;	/* the clients groupInfoAttribute lists */
	uint8_t triggerPending;		/* set by the transmit task before a direct-link frame */
	uint8_t noaMode;		/* 1: the Notice of Absence has a CTWindow, 2: it has absence periods */
	uint8_t powerManagement;	/* the PM bit of the last null frame */
	uint8_t noaPresent;		/* wlanDirectLinkPoll(): 1 outside the absence periods of the Notice of Absence, 0 inside one */
	uint8_t ssidPending;		/* 1: the group's SSID is still to be made ("DIRECT-" and two random characters) */
	uint8_t channelShared;		/* an infrastructure connection or an access point is up: the group shares its channel */
	uint8_t reserved49[3];
	uint32_t directLinkTimer;	/* osaCreateTimer() handle: the delay wlanDirectLinkSend() waits for */
	uint32_t provisionTimer;		/* osaCreateTimer() handle: the provision discovery request's 200 ms */
	uint32_t negotiationTimer;	/* osaCreateTimer() handle: the GO negotiation request's 10 s */
	uint32_t invitationTimer;	/* osaCreateTimer() handle: 120 ms after an invitation frame was sent, 1 s after an invitation request (p2pInvitationTimerExpired()) */
	uint32_t discoverabilityTimer;	/* osaCreateTimer() handle: the device discoverability request's 2 s */
	OsaBuffer *deferredFrame;	/* a P2P action frame that came while the device was busy: p2pActionFrameReceive() takes it up when the group owner's channel is back */
	struct {
		uint8_t id;		/* 11: the Channel List attribute of the channels both sides support */
		uint8_t length[2];
		uint8_t country[3];
		uint8_t operatingClass;
		uint8_t count;
		uint8_t channels[35];
	} commonChannels;
	uint8_t savedP2pState;		/* the connection's p2pState, mode and channel before an invitation or a provision discovery made it a device (p2pDeviceStateSave()) */
	uint8_t savedMode;
	uint8_t savedChannel;
	uint8_t presenceNoaAttribute[31];	/* the Notice of Absence attribute a presence request carries (p2pVendorActionSend()) */
	uint8_t noaAttribute[31];	/* the Notice of Absence attribute of the last beacon: ID, length, index, CTWindow, descriptors */
	union {
		uint8_t capabilityAttribute[5];	/* the P2P Capability attribute: device and group capability */
		struct {
			uint8_t reserved184[4];
			uint8_t p2pOptions;	/* the group capability; bit 1: the Extended Listen Timing attribute goes into the P2P element */
		};
	};
	struct {
		uint8_t id;		/* the attribute the P2P element of a probe request carries, when its length is not 0 */
		uint8_t length[2];
	} p2pAttribute;
	uint8_t scanPeerDeviceId[6];	/* MRVL_WFD_SCAN_PEER_DEVICE_ID_TLV: the device the probe requests ask for (p2pAttribute is its Device ID attribute) */
	union {
		uint8_t deviceIdAttribute[9];	/* the P2P Device ID attribute */
		struct {
			uint8_t reserved198[3];
			uint8_t interfaceAddress[6];	/* the group's P2P interface address */
		};
	};
	union {
		uint8_t listenChannelAttribute[8];
		struct {
			uint8_t reserved207[7];
			uint8_t defaultChannel;		/* the listen channel: the channel the group's connection starts on */
		};
	};
	union {
		uint8_t operatingChannelAttribute[8];
		struct {
			uint8_t reserved215[7];
			uint8_t requestedChannel;	/* the operating channel the host asks for: when nonzero, groupChannel takes it */
		};
	};
	uint8_t groupOwnerIntentAttribute[4];
	uint8_t channelListAttribute[43];
	struct {
		uint8_t id;		/* 14: the P2P Group Info attribute */
		uint8_t length[2];
		uint8_t clients[712];	/* struct WlanP2pClient, one after the other */
	} groupInfoAttribute;
	uint8_t deviceInfoAttribute[84];	/* the P2P Device Info attribute */
	uint8_t groupIdType;			/* the P2P Group ID attribute: its id, ... */
	uint16_t groupIdLength;			/* ... its length (15: the device address and "DIRECT-" with two characters), ... */
	uint8_t groupDeviceAddress[6];		/* ... the group owner's device address ... */
	uint8_t groupSsid[32];			/* ... and the SSID: "DIRECT-xy..." */
	uint8_t groupBssidAttribute[13];	/* the P2P Group BSSID attribute */
	uint8_t extendedListenAttribute[3];	/* the Extended Listen Timing attribute's header; the period and interval follow */
	uint16_t availabilityPeriod;	/* the Extended Listen Timing attribute: how long the device stays on its listen channel (ms) ... */
	uint16_t availabilityInterval;	/* ... and how often (ms); the scan engine dwells away for the difference */
	uint8_t interfaceAttribute[22];		/* the P2P Interface attribute */
	uint8_t configTimeoutAttribute[5];	/* the Configuration Timeout attribute */
	union {
		uint8_t intendedAddressAttribute[9];	/* the Intended P2P Interface Address attribute */
		struct {
			uint8_t reserved1157[3];
			uint8_t groupAddress[6];	/* the BSSID of the group */
		};
	};
	struct {
		uint8_t id;			/* 0: the Status attribute */
		uint8_t length[2];
		uint8_t value;			/* 0: success, 3: limit reached */
	} p2pStatus;
	uint8_t invitationFlagsAttribute[4];
	uint8_t reserved1174[2];
	uint32_t persistentGroupIndex;	/* the persistent group an invitation is for (MRVL_WFD_PERSISTENT_GROUP_TLV with only the index); 255: none, when the group starts */
	struct WlanP2pPersistentGroup *persistentGroups[2];	/* the persistent groups the host stored */
	uint8_t p2pBssActive;		/* 1: the channel of the group owner is the one the radio is on */
	uint8_t clientDiscoverabilityPending;	/* a GO discoverability request went to a client for a device discoverability request (p2pDiscoverabilityRequestAnswer()) */
	uint8_t discoverabilityPending;	/* a device discoverability request is out (p2pDiscoverabilityCheck()); cleared by its timer and when the group starts */
	uint8_t discoverabilityDialogToken;	/* the dialog token of the device discoverability request being answered */
	uint8_t p2pChannel;		/* the channel the group owner's beacon was heard on; 255: none */
	uint8_t discoverabilityRequester[6];	/* the device that sent it */
	uint8_t reserved1199[9];
	uint32_t groupSettingA;		/* 1 when the group starts */
	uint32_t groupSettingB;		/* 1 */
	OsaBuffer *pendingFrame;	/* a frame p2pPendingFrameSend() queues later (a retry or a timer's) */
	uint32_t ssidFixed;		/* nonzero: the host supplied the group's SSID, which is not generated */
	uint32_t p2pEntryPending;	/* a direct-link entry waits for the next beacon (macirq.c) */
	uint8_t groupChannel;		/* the channel the group is on */
	uint8_t extendedListenEnabled;	/* Wi-Fi Direct mode 9 sets it, mode 6 clears it: availabilityPeriod and availabilityInterval apply */
	uint8_t reserved1230[2];
	uint8_t *wpsAttributes[19];	/* the attributes of the WPS element the host gave (custom IE slot 1), by WPS_SLOT_* (wpsAttributesParse()) */
	uint8_t eventPeerAddress[6];	/* the peer the listen channel's event (type 2) names (p2pListenTimerExpired()) */
	uint8_t discoveryDeviceAddress[6];	/* the client the host wants discovered: its group owner gets a device discoverability request */
	uint8_t p2pDeviceAddress[6];	/* the P2P Device ID attribute of a probe request */
	uint8_t invitationList[2][6];	/* MRVL_WFD_INVITATION_LIST_TLV: the devices whose invitations are accepted */
	uint8_t invitationCount;
	uint8_t dialogToken;		/* the dialog token of the P2P public action frames sent */
	uint32_t provisioningAction;	/* MRVL_WIFI_DIRECT_PROVISIONING_PARAMS_TLV's action: nonzero, the config method goes into the provision discovery frames; 1 when the group starts */
	uint32_t wpsConfigMethod;		/* the WPS config method provisioning uses (256: keypad) */
	uint32_t wpsDevicePasswordId;	/* its WPS device password ID (5: registrar specified) */
	uint32_t wpsParamsAction;	/* MRVL_WIFI_DIRECT_WPS_PARAMS_TLV's action; 1 when the group starts */
	uint8_t p2pGroupId[6];		/* the group owner a beacon names: the one the discoverability request went to */
};

_Static_assert(offsetof(struct WlanPsPeerState, commonChannels) == 76, "P2P common channels");
_Static_assert(offsetof(struct WlanPsPeerState, capabilityAttribute) == 184, "P2P capability attribute");
_Static_assert(offsetof(struct WlanPsPeerState, deviceIdAttribute) == 198, "P2P device ID attribute");
_Static_assert(offsetof(struct WlanPsPeerState, listenChannelAttribute) == 207, "P2P listen channel attribute");
_Static_assert(offsetof(struct WlanPsPeerState, operatingChannelAttribute) == 215, "P2P operating channel attribute");
_Static_assert(offsetof(struct WlanPsPeerState, groupOwnerIntentAttribute) == 223, "P2P GO intent attribute");
_Static_assert(offsetof(struct WlanPsPeerState, channelListAttribute) == 227, "P2P channel list attribute");
_Static_assert(offsetof(struct WlanPsPeerState, groupInfoAttribute) == 270, "P2P group info attribute");
_Static_assert(offsetof(struct WlanPsPeerState, deviceInfoAttribute) == 985, "P2P device info attribute");
_Static_assert(offsetof(struct WlanPsPeerState, groupBssidAttribute) == 1110, "P2P group BSSID attribute");
_Static_assert(offsetof(struct WlanPsPeerState, extendedListenAttribute) == 1123, "P2P extended listen attribute");
_Static_assert(offsetof(struct WlanPsPeerState, interfaceAttribute) == 1130, "P2P interface attribute");
_Static_assert(offsetof(struct WlanPsPeerState, configTimeoutAttribute) == 1152, "P2P configuration timeout attribute");
_Static_assert(offsetof(struct WlanPsPeerState, intendedAddressAttribute) == 1157, "P2P intended address attribute");
_Static_assert(offsetof(struct WlanPsPeerState, invitationFlagsAttribute) == 1170, "P2P invitation flags attribute");
_Static_assert(offsetof(struct WlanPsPeerState, tieBreaker) == 31, "P2P tie-breaker");
_Static_assert(offsetof(struct WlanPsPeerState, ssidPending) == 47, "P2P SSID pending");
_Static_assert(offsetof(struct WlanPsPeerState, interfaceAddress) == 201, "P2P interface address");
_Static_assert(offsetof(struct WlanPsPeerState, requestedChannel) == 222, "P2P requested channel");
_Static_assert(offsetof(struct WlanPsPeerState, groupIdType) == 1069, "P2P group ID");
_Static_assert(offsetof(struct WlanPsPeerState, groupSsid) == 1078, "P2P group SSID");
_Static_assert(offsetof(struct WlanPsPeerState, groupAddress) == 1160, "P2P group address");
_Static_assert(offsetof(struct WlanPsPeerState, persistentGroupIndex) == 1176, "P2P persistent group index");
_Static_assert(offsetof(struct WlanPsPeerState, invitationCount) == 1338, "P2P invitation list");
_Static_assert(offsetof(struct WlanPsPeerState, availabilityPeriod) == 1126, "P2P availability period");
_Static_assert(offsetof(struct WlanPsPeerState, discoverabilityPending) == 1190, "P2P discoverability request");
_Static_assert(offsetof(struct WlanPsPeerState, p2pChannel) == 1192, "P2P channel");
_Static_assert(offsetof(struct WlanPsPeerState, ssidFixed) == 1220, "P2P fixed SSID");
_Static_assert(offsetof(struct WlanPsPeerState, groupChannel) == 1228, "P2P group channel");
_Static_assert(offsetof(struct WlanPsPeerState, wpsAttributes) == 1232, "WPS attributes");
_Static_assert(offsetof(struct WlanPsPeerState, provisioningAction) == 1340, "P2P provisioning action");
_Static_assert(offsetof(struct WlanPsPeerState, p2pOptions) == 188, "P2P options");
_Static_assert(offsetof(struct WlanPsPeerState, p2pAttribute) == 189, "P2P attribute");
_Static_assert(offsetof(struct WlanPsPeerState, p2pStatus) == 1166, "P2P status");
_Static_assert(offsetof(struct WlanPsPeerState, p2pEntryPending) == 1224, "P2P entry pending");
_Static_assert(offsetof(struct WlanPsPeerState, p2pDeviceAddress) == 1320, "P2P device address");
_Static_assert(offsetof(struct WlanPsPeerState, p2pGroupId) == 1356, "P2P group");
_Static_assert(offsetof(struct WlanPsPeerState, noaAttribute) == 153, "Notice of Absence");
_Static_assert(offsetof(struct WlanPsPeerState, p2pBssActive) == 1188, "P2P BSS active");
_Static_assert(offsetof(struct WlanPsPeerState, p2pChannel) == 1192, "P2P channel");
_Static_assert(offsetof(struct WlanPsPeerState, noaMode) == 44, "Notice of Absence mode");
_Static_assert(offsetof(struct WlanPsPeerState, noaPresent) == 46, "Notice of Absence verdict");
_Static_assert(offsetof(struct WlanPsPeerState, directLinkTimer) == 52, "direct link timer");

/*
 * What the transmit task hands the frame builder (wlanTxBuildFrame())
 * for one host frame: where the transmit data and the payload are, the
 * TID and sequence number, and room for the 802.11 header.
 */
struct MacTxFrameBuild {
	uint8_t *data;			/* the transmit data (struct WlanReplyFrame) */
	uint8_t *payload;
	uint16_t payloadLength;
	uint8_t fragments;		/* how many frames wlanTxBuildFrame() made of it */
	uint8_t tid;
	uint8_t reserved12[4];
	uint16_t sequenceControl;	/* bits 4-15: sequence number */
	uint8_t reserved18[2];
	uint8_t header[32];
};

_Static_assert(sizeof(struct MacTxFrameBuild) == 52, "frame build");
_Static_assert(offsetof(struct MacTxFrameBuild, header) == 20, "frame build header");

/*
 * The key a frame is sent with (the pointer at 112 of its data, from
 * wlanTxBuildFrame()): a frame of an AES key carries a 16-byte header to
 * strip, one of a WAPI key goes back as it is.
 */
#define TX_DATA_KEY(data)	(*(const uint8_t *const *)((data) + 112))
#define TX_KEY_TYPE(key)	((key)[7])

/* Hardware counters of one channel-access queue accumulated in software. */
struct MacTxQueueStats {
	uint32_t prepared;
	uint32_t checked;
	uint32_t validated;
};

/* Mode block of the local station; byte 4 is the power-save mode. */
/* A WEP default key as the local state keeps it (wlanLocalStateInit()). */
struct WlanLocalWepKey {
	uint8_t index;			/* 1..4 */
	uint8_t length;			/* 0: not set */
	uint8_t key[13];
};

_Static_assert(sizeof(struct WlanLocalWepKey) == 15, "local WEP key");

/* The station's MIB values the host sets (wlanDeviceAddress points here). */
struct WlanDeviceConfig {
	uint8_t address[6];
	uint8_t rtsThreshold[2];	/* 2347: off */
	uint8_t shortRetryLimit;	/* 9 */
	uint8_t longRetryLimit;		/* 4 */
	uint8_t fragThreshold[2];	/* 2346: off */
	uint32_t parameters[2];		/* 512 and 96 at start; readers not converted yet */
	uint16_t timeouts[4];		/* 500 each at start; readers not converted yet */
};

_Static_assert(sizeof(struct WlanDeviceConfig) == 28, "device configuration");

/* The local station's state (fmCoexScheduleBuffer() sets its defaults). */
struct WlanLocalState {
	uint8_t mode;			/* 0xff at start */
	uint8_t reserved1;
	uint16_t sequence;		/* 0 at start */
	uint8_t powerSave;
	uint8_t rates[14];		/* the rates of a Bluetooth AMP link's IBSS, 0-terminated */
	uint8_t reserved19[5];
	uint32_t mibOid17;		/* HostCmd_CMD_802_11_SNMP_MIB's object 17: its value; non-zero starts macMgmtScheduleHeader() */
	struct WlanLocalWepKey wepKeys[4];	/* 28 */
	uint32_t keyFlags;		/* 88 */
	uint8_t keyAddress[6];		/* 92 */
	uint8_t keyIndex;		/* 98 */
	uint8_t keyData[12];		/* 99 */
	uint8_t reserved111;
	uint8_t keyValid;		/* 112 */
	uint8_t reserved113[3];
	uint8_t linkFlags[2];		/* 116 */
	uint8_t reserved118[2];
	uint32_t linkLimit;		/* 120: 10 at start */
	uint8_t linkState;		/* 124 */
	uint8_t reserved125[3];
	uint32_t linkCounters[2];	/* 128 */
	uint8_t linkActive;		/* 136 */
	uint8_t reserved137[3];
	struct WlanDeviceConfig device;	/* 140 */
	uint8_t reserved168[14];
	uint16_t enabled;		/* 182: 1 at start */
	uint8_t reserved184[4];
	uint8_t context[];		/* 188: wlanLocalContext points here */
};
_Static_assert(offsetof(struct WlanLocalState, mibOid17) == 24, "SNMP MIB object 17");
_Static_assert(offsetof(struct WlanLocalState, device) == 140, "local device configuration");
_Static_assert(offsetof(struct WlanLocalState, enabled) == 182, "local state enabled");
_Static_assert(offsetof(struct WlanLocalState, context) == 188, "local context");

/* Bits of wlanTxWindowFlags, one set per window (shifted by the window number). */
#define WLAN_TX_WINDOW_OPEN		0x001u
#define WLAN_TX_WINDOW_ENABLED		0x010u
#define WLAN_TX_WINDOW_DRAINING		0x100u

extern struct WlanTxWindowQueue wlanTxWindowQueues[2];
extern uint32_t macTxWakeBits;
extern uint32_t wlanTxWindowFlags;
extern struct MacTxQueueStats macTxQueueStats[];
extern uint16_t wlanSessionId;
extern struct WlanLocalState *wlanLocalState;
extern struct WlanDeviceConfig *wlanDeviceAddress;		/* 0x54a7c */
extern uint8_t *wlanLocalContext;				/* 0x54a84 */
extern uint32_t macTxMaxWait;
extern uint8_t macTxDrainWaits;
extern WlanTxDescriptor macTxDescriptor;
extern struct OsaSemaphore *macTxSemaphore;
extern uint8_t macTxBuildOverride;
extern uint8_t wlanTxRateFlags;				/* 0x040088a6, bit 6: no low rate for protected frames */

/* Block-ack window operations of macTxBlockAckWindow(). */
enum {
	MAC_TX_WINDOW_ADD = 1,
	MAC_TX_WINDOW_DELETE = 2,
};

/* Events of the transmit task (macTxEventFlags). */
#define MAC_TX_EVENT_DATA		0x001	/* host data queued */
#define MAC_TX_EVENT_MGMT		0x002	/* a management frame in macTxMgmtQueue */
#define MAC_TX_EVENT_TRACK		0x004
#define MAC_TX_EVENT_NOTIFY		0x008	/* frames from the notify task */
#define MAC_TX_EVENT_FLUSH		0x010
#define MAC_TX_EVENT_STAGE		0x020	/* back to connection stage 0 */
#define MAC_TX_EVENT_RESUME		0x040
#define MAC_TX_EVENT_REKEY		0x200	/* group rekey timeouts in uapGroupRekeyQueue */
#define MAC_TX_EVENT_ALL		0x3ff

int macWaitPendingClear(unsigned int mask, unsigned int limit);
int macWaitBasebandStable(uint32_t mask, uint32_t *iterations, uint32_t samples);
extern uint64_t macWaitElapsed;
int macTxPostEvent(uint32_t events);
int macTxWakeup(void);
int macTxMeasureWait(unsigned int mask);
int macTxPathReady(void);
void macTxAdvanceStage(void);
int wlanTxAllocFrame(OsaBuffer **buffer, WlanTxDescriptor **descriptor, uint32_t size,
		     uint16_t priority);
OsaBuffer *wlanTxAllocFrameType2(uint16_t priority, int (*release)(OsaBuffer *buffer));

/*
 * An event frame for the host, as wlanTxAllocFrame() lays it out with the
 * event as its priority. The data (code, address, or any data) follows.
 */
struct WlanHostEventFrame {
	WlanPacketHeader header;
	uint16_t kind;			/* WLAN_HOST_FRAME_EVENT: the TX descriptor's control word */
	uint16_t length;		/* bytes of data, from code on */
	uint16_t event;			/* WLAN_HOST_EVENT_* */
	uint8_t bssNumber;
	uint8_t bssType;
	uint16_t code;
	uint8_t address[6];
};

_Static_assert(sizeof(struct WlanHostEventFrame) == 24, "host event frame");

#define WLAN_HOST_FRAME_EVENT		1

/* Events to the host (the event numbers of Marvell's host interface, as Linux's mwifiex names them). */
#define WLAN_HOST_EVENT_LINK_LOST		0x03
#define WLAN_HOST_EVENT_LINK_SENSED		0x04
#define WLAN_HOST_EVENT_DEAUTHENTICATED		0x08
#define WLAN_HOST_EVENT_DISASSOCIATED		0x09
#define WLAN_HOST_EVENT_PS_AWAKE		0x0a
#define WLAN_HOST_EVENT_PS_SLEEP		0x0b
#define WLAN_HOST_EVENT_MIC_ERR_MULTICAST	0x0d
#define WLAN_HOST_EVENT_MIC_ERR_UNICAST		0x0e
#define WLAN_HOST_EVENT_DEEP_SLEEP_AWAKE	0x10
#define WLAN_HOST_EVENT_HOST_SLEEP_AWAKE	0x12
#define WLAN_HOST_EVENT_WMM_STATUS		0x17
#define WLAN_HOST_EVENT_RSSI_LOW		0x19	/* the subscribed events: wlan/subscribe.c */
#define WLAN_HOST_EVENT_SNR_LOW			0x1a
#define WLAN_HOST_EVENT_MAX_FAIL		0x1b
#define WLAN_HOST_EVENT_RSSI_HIGH		0x1c
#define WLAN_HOST_EVENT_SNR_HIGH		0x1d
#define WLAN_HOST_EVENT_IBSS_COALESCED		0x1e
#define WLAN_HOST_EVENT_DATA_RSSI_LOW		0x24
#define WLAN_HOST_EVENT_DATA_SNR_LOW		0x25
#define WLAN_HOST_EVENT_DATA_RSSI_HIGH		0x26
#define WLAN_HOST_EVENT_DATA_SNR_HIGH		0x27
#define WLAN_HOST_EVENT_LINK_QUALITY		0x28
#define WLAN_HOST_EVENT_EVENT_41		0x29	/* no Marvell name known */
#define WLAN_HOST_EVENT_PORT_RELEASE		0x2b
#define WLAN_HOST_EVENT_PRE_BEACON_LOST		0x31
#define WLAN_HOST_EVENT_HS_ACT_REQ		0x47
#define WLAN_HOST_EVENT_WIFIDIRECT_GENERIC	0x49	/* mlan's EVENT_WIFIDIRECT_GENERIC_EVENT: a P2P peer's frame (p2pDirectNotifyContext()) */
#define WLAN_HOST_EVENT_UAP_MIC_COUNTERMEASURES_END	0x4c	/* the access point's TKIP countermeasures ended (inferred) */
#define WLAN_HOST_EVENT_RSN_CONNECT		0x51	/* MICRO_AP_EV_ID_RSN_CONNECT: a station's handshake is done */
#define WLAN_HOST_EVENT_IBSS_PEER_SILENT	0x21	/* the IBSS's peer missed three link checks (inferred) */

extern struct OsaMsgList wlanHostEventQueue;
extern uint16_t wlanHostSleepEndOnWake;

void wlanHostEventQueueInit(void);
int wlanHostEventSend(struct WlanConnection *connection, OsaBuffer *buffer);
int wlanHostEventSendRelease(struct WlanConnection *connection, uint16_t event,
			     int (*release)(OsaBuffer *buffer));
int wlanHostEventSendPlain(struct WlanConnection *connection, uint16_t event);
int wlanHostEventSendCode(struct WlanConnection *connection, uint16_t event, uint16_t code);
int wlanHostEventSendAddress(struct WlanConnection *connection, uint16_t event,
			     const uint8_t *address);
int wlanHostEventSendData(struct WlanConnection *connection, uint16_t event, const void *data,
			  uint16_t length);
int wlanHostEventSendCodeAddress(struct WlanConnection *connection, uint16_t event, uint16_t code,
				 const uint8_t *address);
int wlanHostEventSendWithAddress(struct WlanConnection *connection, const uint8_t *address,
				 OsaBuffer *buffer);
int wlanHostEventDeliver(OsaBuffer *buffer);
void wlanHostEventFlush(void);
void wlanHostAwake(void);
void wlanHostSleepCancel(void);
void wlanHostSleepEnter(void);
int wlanHostSleepArmed(void);
unsigned int wlanHostSleepWakeMatch(unsigned int classes, unsigned int conditions);
void wlanHostSleepGap(unsigned int gap);
int wlanHostEventNotify(unsigned int classes, OsaBuffer *buffer);
void macWakeSettle(uint32_t mask);
uint32_t macTxAcquireWakeBit(void);
void macTxReleaseWakeBits(uint32_t *bits);
void wlanTxWindowReset(unsigned int window);
void wlanTxWindowsClearPending(void);
void wlanTxWindowComplete(unsigned int window);
int macTxQueueMgmtMessage(uintptr_t message);
int macTxDequeue(unsigned int window, uint32_t *entry);
int macTxResetQueues(void);
void macTxSendMgmtFrame(OsaBuffer *buffer);
int macTxResetStage(void);
void macTxSendDataFrame(OsaBuffer *buffer);
int macTxEnqueue(struct WlanConnection *connection, unsigned int window, uint32_t entry);
int macTxBlockAckWindow(struct WlanConnection *connection, int operation, unsigned int ring,
			int slot, uint32_t entry, uint8_t *data);
void macTxTaskMain(ULONG input);
unsigned int macTxActivateWindow(unsigned int window);
void macTxNullReleased(void);
uint32_t wlanTxBaTimeoutRegister(uintptr_t owner, uint32_t timeout);
struct WlanBaRequest;
int macTxBaDelete(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid);
int macTxBaAbort(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid);
int macRxBaDelete(const uint8_t *peer, unsigned int tid);
int macTxBaAdd(struct WlanConnection *connection, const struct WlanBaRequest *request);
void macTxBaResyncSequences(void);
void macTxSendNullFrame(struct WlanConnection *connection, int powerManagement,
			unsigned int priority, int exclusive, int trigger, uint8_t kind);
void macTxPsPollReleased(void);
int macTxSendPsPoll(struct WlanConnection *connection, int urgent);
void wlanTxWindowFlush(unsigned int ring);
void macTxFlushConnection(struct WlanConnection *connection);
int macTxGetDrainWaits(void);
void macTxSetDrainWaits(int value);
int macTxHardwareIdle(void);
int macTxIdle(void);
int macTxPoolsIdle(void);
void macTxWaitIdle(void);
void macTxSetPowerSave(int mode);
int macTxGetPowerSave(void);
void macTxStatsReset(unsigned int queue);
int macTxStatsAddPrepared(unsigned int queue, int *last);
int macTxStatsAddValidated(unsigned int queue, int *last);
int macTxStatsAddChecked(unsigned int queue, int *last);
int8_t wlanClampSnr(int value);
int wlanClampNoise(int value);
void macTxBuildDataHeader(struct WlanConnection *connection, WlanTxDescriptor *descriptor,
			  WlanWirelessFrame *frame, const uint8_t *addresses, int qos);

#endif /* ROBIN_MACTX_H */
