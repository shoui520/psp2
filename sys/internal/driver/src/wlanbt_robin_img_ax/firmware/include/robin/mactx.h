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
struct WlanPsPeerState {
	uint8_t beaconTimestamp[8];	/* the TSF of the last beacon heard (a P2P group owner's) */
	uint32_t beaconTimerLow;	/* the MAC timer when it was heard */
	uint32_t beaconTimerHigh;
	uint8_t reserved16[14];
	uint8_t p2pFlags;		/* bit 1: the P2P element names the device address; bit 2: a P2P group owner's beacon carries the P2P element */
	uint8_t reserved31[10];
	uint8_t p2pAuthCount;		/* authentications under way with a P2P peer that has no context (decremented when one ends) */
	uint8_t reserved42;
	uint8_t triggerPending;		/* set by the transmit task before a direct-link frame */
	uint8_t noaMode;		/* 1: the Notice of Absence has a CTWindow, 2: it has absence periods */
	uint8_t powerManagement;	/* the PM bit of the last null frame */
	uint8_t reserved46[26];
	uint32_t p2pGroupActive;	/* nonzero while the P2P group is up (inferred: the state machine reads it when it enters or leaves a state) */
	uint8_t reserved76[77];
	uint8_t noaAttribute[31];	/* the Notice of Absence attribute of the last beacon: ID, length, index, CTWindow, descriptors */
	uint8_t reserved184[4];
	uint8_t p2pOptions;		/* bit 1: the Extended Listen Timing attribute goes into the P2P element */
	struct {
		uint8_t id;		/* the attribute the P2P element of a probe request carries, when its length is not 0 */
		uint8_t length[2];
	} p2pAttribute;
	uint8_t reserved192[974];
	struct {
		uint8_t id;			/* 0: the Status attribute */
		uint8_t length[2];
		uint8_t value;			/* 0: success, 3: limit reached */
	} p2pStatus;
	uint8_t reserved1170[18];
	uint8_t p2pBssActive;		/* 1: the channel of the group owner is the one the radio is on */
	uint8_t reserved1189[3];
	uint8_t p2pChannel;		/* the channel the group owner's beacon was heard on */
	uint8_t reserved1193[31];
	uint32_t p2pEntryPending;	/* a direct-link entry waits for the next beacon (macirq.c) */
	uint8_t reserved1228[4];
	uint8_t p2pIeState[4];		/* kept by the P2P code from custom IE slot 1 */
	uint8_t reserved1236[84];
	uint8_t p2pDeviceAddress[6];	/* the P2P Device ID attribute of a probe request */
	uint8_t reserved1326[30];
	uint8_t p2pGroupId[6];		/* the group owner a beacon names */
};

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
struct WlanLocalState {
	uint8_t reserved0[4];
	uint8_t powerSave;
	uint8_t reserved1[3];
	uint8_t reserved8[16];
	uint32_t mibOid17;		/* HostCmd_CMD_802_11_SNMP_MIB's object 17: its value; non-zero starts macMgmtScheduleHeader() */
};
_Static_assert(offsetof(struct WlanLocalState, mibOid17) == 24, "SNMP MIB object 17");

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
