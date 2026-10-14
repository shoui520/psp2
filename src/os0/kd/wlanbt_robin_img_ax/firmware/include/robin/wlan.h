#ifndef ROBIN_WLAN_H
#define ROBIN_WLAN_H

#include <stdint.h>
#include <sce/wlanbt_protocol.h>
#include <sce/ethernet.h>
#include "robin/buffer.h"

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

int wlanInitializationHook(void);
int wlanForwardSdioEvent(void *device, OsaBuffer *buffer,
			 uintptr_t argument, unsigned int event);
int wlanReceiveDeviceEvent(void *device, OsaBuffer *buffer,
			   uintptr_t argument, unsigned int event);
int wlanHandleSecurityFrame(OsaBuffer *buffer);
int wlanBufferNeedsSecurity(OsaBuffer *buffer);
int wlanSecurityOverrideEnabled(void);
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
void wlanForwardEventPacket(OsaBuffer *buffer);
void wlanSetEventPacketLength(OsaBuffer *buffer, uint32_t length);
int wlanReleasePacket(OsaBuffer *buffer);
int wlanReleaseDmaSpan(uint32_t start, uint32_t end, unsigned int queue);
int wlanIsSecurityProtocol(uint32_t protocol);
uint32_t wlanPacketHeadroom(void);
int wlanConvertWirelessToEthernet(OsaBuffer *buffer, struct ether_header **output,
				 const WlanWirelessFrame *wirelessHeader,
				 uint8_t *payload, int stripSnap);

#endif /* ROBIN_WLAN_H */
