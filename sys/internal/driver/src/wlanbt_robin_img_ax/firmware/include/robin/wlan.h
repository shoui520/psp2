#ifndef ROBIN_WLAN_H
#define ROBIN_WLAN_H

#include <stdint.h>
#include <wlanbt/wlanbt_protocol.h>
#include <net_ps/ethernet.h>
#include "robin/buffer.h"

/* IEEE 802.11 capability information field (Sony builds it bit by bit in a local) */
struct __attribute__((packed)) WlanCapabilityInfo {
	uint16_t ess : 1;
	uint16_t ibss : 1;
	uint16_t cfPollable : 1;
	uint16_t cfPollRequest : 1;
	uint16_t privacy : 1;
	uint16_t shortPreamble : 1;
	uint16_t pbcc : 1;
	uint16_t channelAgility : 1;
	uint16_t spectrumManagement : 1;
	uint16_t qos : 1;
	uint16_t shortSlotTime : 1;
	uint16_t apsd : 1;
	uint16_t radioMeasurement : 1;
	uint16_t dsssOfdm : 1;
	uint16_t delayedBlockAck : 1;
	uint16_t immediateBlockAck : 1;
};

typedef struct WlanSnapHeader {
	WlanLlcSnapPrefix prefix;
	union {
		uint16_t native;
		uint8_t wire[2];
	} protocol;
} WlanSnapHeader;

typedef struct WlanEthernetSnapFrame {
	struct ether_header ethernet;
	WlanSnapHeader snap;
} WlanEthernetSnapFrame;

/*
 * A frame's sequence control field as the firmware writes it in place: packed bit-fields, read
 * and written a byte at a time (WlanWirelessFrame.sequenceControl).
 */
typedef struct __attribute__((packed)) WlanFrameSequence {
	uint16_t fragment : 4;
	uint16_t number : 12;
} WlanFrameSequence;

/* Firmware's fixed four-address MAC header, preceded by the body length. */
typedef struct WlanWirelessFrame {
	uint8_t bodyLength[2];
	uint8_t frameControl[2];
	uint8_t duration[2];
	uint8_t address1[6];
	uint8_t address2[6];
	uint8_t address3[6];
	uint8_t sequenceControl[2];
	uint8_t address4[6];
	uint8_t body[];
} WlanWirelessFrame;

_Static_assert(sizeof(WlanSnapHeader) == 8, "SNAP header");
_Static_assert(offsetof(WlanSnapHeader, protocol) == 6, "SNAP protocol field");
_Static_assert(sizeof(WlanEthernetSnapFrame) == 22, "Ethernet SNAP frame");
_Static_assert(offsetof(WlanEthernetSnapFrame, snap.protocol) == 20, "SNAP protocol");
_Static_assert(__alignof__(WlanEthernetSnapFrame) == 2, "native SNAP protocol alignment");
_Static_assert(sizeof(WlanWirelessFrame) == 32, "wireless frame header");
_Static_assert(offsetof(WlanWirelessFrame, frameControl) == 2, "wireless frame control");
_Static_assert(offsetof(WlanWirelessFrame, address1) == 6, "wireless address1");
_Static_assert(offsetof(WlanWirelessFrame, address2) == 12, "wireless address2");
_Static_assert(offsetof(WlanWirelessFrame, address3) == 18, "wireless address3");
_Static_assert(offsetof(WlanWirelessFrame, address4) == 26, "wireless address4");
_Static_assert(offsetof(WlanWirelessFrame, body) == 32, "wireless frame body");

typedef int (*WlanHostCallback)(void *device, OsaBuffer *buffer, uintptr_t argument,
				unsigned int event);

/* Callback the host side registers with ioctl 0x20 on the "wlan" device. */
extern WlanHostCallback wlanHostCallback;
extern void *wlanDevice;
extern void *wlanSdioDev;
extern void (*wlanReceiveErrorHandler)(void *context);

int wlanInitializationHook(void);
int wlanDriverInit(void);
void wlanIrqRegisterAll(void);
void wlanIrqUnregisterAll(void);
void wlanIrqEnableHostLines(void);
int wlanForwardSdioEvent(void *device, OsaBuffer *buffer,
			 uintptr_t argument, unsigned int event);
int wlanReceiveDeviceEvent(void *device, OsaBuffer *buffer,
			   uintptr_t argument, unsigned int event);
int wlanHandleSecurityFrame(OsaBuffer *buffer);
int wlanBufferNeedsSecurity(OsaBuffer *buffer);
int wlanBufferUsesEthernetFrames(OsaBuffer *buffer);
int wlanContextUsesEthernetFrames(const void *context);
int wlanInterfaceNeedsSecurity(const void *context);
int wlanDispatchSecurityFrame(OsaBuffer *buffer, const uint8_t *source,
			      const uint8_t *destination);
int wlanHandleAmpFrame(OsaBuffer *buffer);
int wlanBufferUsesAmp(OsaBuffer *buffer);
void *wlanGetBufferDevice(OsaBuffer *buffer);
void *wlanOpenSdioDevice(unsigned int flags);
int wlanConnectSdioDevice(void);
int wlanDisconnectSdioDevice(void);
int wlanInitializeDevice(void);
uint32_t wlanCompletePacket(OsaBuffer *buffer, uint32_t completion);
int wlanDecodeDataFrame(OsaBuffer *buffer, WlanPacketHeader *header,
			const void *wirelessHeader);
int wlanConvertDataFrame(OsaBuffer *buffer, WlanPacketHeader *header,
			uintptr_t argument, const void *wirelessHeader);
int wlanPrepareAmsduFrame(OsaBuffer *buffer, WlanPacketHeader *header);
void wlanDefaultReceiveError(void *context);
struct OsaBufferPool *wlanInitializeReceiveErrorHandler(void);
/* The hardware information block in wlanDtcmState (0x040088ac). */
struct WlanHwInfo {
	uint8_t reserved0[5];
	uint8_t version[2];		/* the hardware version, from the SoC (HostCmd_DS_GET_HW_SPEC) */
	uint8_t reserved7[4];
	uint8_t txPowerLevel[2];	/* HostCmd_CMD_RF_TX_POWER's current level */
	uint8_t txPowerSteps[16];	/* the levels it can be set to, 16-bit each */
};
extern struct WlanHwInfo wlanHwInfo;			/* 0x040088ac */
extern struct OsaBufferPool wlanEventPool;				/* 0x04000eac: the events to the host (wlan/wlaninit.c) */
void wlanForwardEventPacket(OsaBuffer *buffer);
void wlanSetEventPacketLength(OsaBuffer *buffer, uint32_t length);
int wlanReleasePacket(OsaBuffer *buffer);
int wlanIsSecurityProtocol(uint32_t protocol);
uint32_t wlanPacketHeadroom(void);
int wlanConvertWirelessToEthernet(OsaBuffer *buffer, struct ether_header **output,
				 const WlanWirelessFrame *wirelessHeader,
				 uint8_t *payload, int stripSnap);

/* Debug trace hook (tag, value), empty in this firmware. */
void wlanTrace(uint32_t tag, uint32_t value);

#endif /* ROBIN_WLAN_H */
