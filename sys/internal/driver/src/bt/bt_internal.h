#ifndef BT_INTERNAL_H
#define BT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <psp2/types.h>
#include <net/header/ip.h>
#include "udp.h"
#include "tcp.h"
#include <bt/bt_event.h>
#include <bt/bt_hid.h>
#include <bt/bt_registered.h>
#include <net_ps/netdrv.h>
#include <net_ps/ethernet.h>
#include "bt_sbc_codec.h"

typedef struct BtStack BtStack;
typedef struct BtConnection BtConnection;
typedef struct BtChannel BtChannel;

typedef struct BtEirRecord {
	uint8_t length;
	uint8_t type;
	uint8_t payload[];
} BtEirRecord;

typedef struct BtEirDeviceId {
	uint8_t length;
	uint8_t type;
	uint8_t sourceLe[2];
	uint8_t vendorLe[2];
	uint8_t productLe[2];
	uint8_t versionLe[2];
} BtEirDeviceId;

_Static_assert(sizeof(BtEirRecord) == 2, "EIR record header");
_Static_assert(offsetof(BtEirRecord, payload) == 2, "EIR payload");
_Static_assert(sizeof(BtEirDeviceId) == 10, "EIR device identity");
_Static_assert(offsetof(BtEirDeviceId, sourceLe) == 2, "EIR identity source");
_Static_assert(offsetof(BtEirDeviceId, vendorLe) == 4, "EIR vendor");
_Static_assert(offsetof(BtEirDeviceId, productLe) == 6, "EIR product");
_Static_assert(offsetof(BtEirDeviceId, versionLe) == 8, "EIR version");

typedef struct BtHciEventPacket {
	uint8_t eventCode;
	uint8_t parameterLength;
	uint8_t parameters[];
} BtHciEventPacket;

typedef struct BtRtpPacket {
	uint8_t flags;
	uint8_t payloadType;
	uint8_t sequenceBe[2];
	uint8_t timestampBe[4];
	uint8_t synchronizationSourceBe[4];
	uint8_t payload[];
} BtRtpPacket;

typedef struct BtSbcTransportPacket {
	uint8_t flagsAndFrameCount;
	uint8_t frames[];
} BtSbcTransportPacket;

typedef struct BtSbcFrame {
	uint8_t syncword;
	uint8_t configuration;
	uint8_t bitpool;
	uint8_t crc;
	uint8_t payload[];
} BtSbcFrame;

_Static_assert(sizeof(BtRtpPacket) == 12, "RTP header");
_Static_assert(sizeof(BtSbcTransportPacket) == 1, "SBC transport header");
_Static_assert(sizeof(BtSbcFrame) == 4, "SBC frame header");

typedef struct BtHciScoPacket {
	uint8_t handleLe[2];
	uint8_t length;
	uint8_t payload[];
} BtHciScoPacket;

_Static_assert(sizeof(BtHciScoPacket) == 3, "HCI SCO header");
_Static_assert(offsetof(BtHciScoPacket, payload) == 3, "HCI SCO payload");

_Static_assert(sizeof(BtHciEventPacket) == 2, "HCI event header");
_Static_assert(offsetof(BtHciEventPacket, eventCode) == 0, "HCI event code");
_Static_assert(offsetof(BtHciEventPacket, parameterLength) == 1,
	       "HCI parameter length");
_Static_assert(offsetof(BtHciEventPacket, parameters) == 2, "HCI parameters");

static inline void BtWriteNetwork32(uint8_t *bytes, unsigned int address)
{
	bytes[0] = address >> 24;
	bytes[1] = address >> 16;
	bytes[2] = address >> 8;
	bytes[3] = address;
}

static inline void BtWriteNetworkMac(uint8_t *bytes, uint64_t address)
{
	bytes[0] = address >> 40;
	bytes[1] = address >> 32;
	bytes[2] = address >> 24;
	bytes[3] = address >> 16;
	bytes[4] = address >> 8;
	bytes[5] = address;
}

typedef struct BtEthernetFrame {
	struct ether_header ethernet;
	uint8_t payload[];
} BtEthernetFrame;

typedef struct BtBnepCompressedPacket {
	uint8_t packetType;
	uint8_t protocolBe[2];
	uint8_t payload[];
} BtBnepCompressedPacket;

typedef struct BtBnepDestinationPacket {
	uint8_t packetType;
	uint8_t destination[6];
	uint8_t protocolBe[2];
	uint8_t payload[];
} BtBnepDestinationPacket;

typedef struct BtBnepGeneralPacket {
	uint8_t packetType;
	struct ether_header ethernet;
	uint8_t payload[];
} BtBnepGeneralPacket;

typedef struct BtBnepSourcePacket {
	uint8_t packetType;
	uint8_t source[6];
	uint8_t protocolBe[2];
	uint8_t payload[];
} BtBnepSourcePacket;

typedef struct BtBnepControlPacket {
	uint8_t command;
	uint8_t parameters[];
} BtBnepControlPacket;

typedef struct BtBnepSetupRequest {
	uint8_t command;
	uint8_t uuidLength;
	uint8_t uuids[];
} BtBnepSetupRequest;

typedef struct BtBnepSetupResponse {
	uint8_t command;
	uint8_t responseCodeBe[2];
} BtBnepSetupResponse;

typedef struct BtBnepFilterRequest {
	uint8_t command;
	uint8_t listLengthBe[2];
	uint8_t filters[];
} BtBnepFilterRequest;

typedef struct BtBnepExtension {
	uint8_t type;
	uint8_t length;
	uint8_t data[];
} BtBnepExtension;

typedef struct BtBnepUuid {
	uint8_t serviceBe[4];
	uint8_t suffix[12];
} BtBnepUuid;

_Static_assert(sizeof(BtBnepGeneralPacket) == 15, "BNEP general header");
_Static_assert(offsetof(BtBnepGeneralPacket, ethernet) == 1, "BNEP Ethernet header");
_Static_assert(offsetof(BtBnepGeneralPacket, payload) == 15, "BNEP general payload");
_Static_assert(sizeof(BtBnepSourcePacket) == 9, "BNEP source header");
_Static_assert(offsetof(BtBnepSourcePacket, source) == 1, "BNEP source address");
_Static_assert(offsetof(BtBnepSourcePacket, protocolBe) == 7, "BNEP source protocol");
_Static_assert(offsetof(BtBnepSourcePacket, payload) == 9, "BNEP source payload");
_Static_assert(sizeof(BtBnepControlPacket) == 1, "BNEP control header");
_Static_assert(offsetof(BtBnepControlPacket, parameters) == 1, "BNEP control parameters");
_Static_assert(sizeof(BtBnepSetupRequest) == 2, "BNEP setup request");
_Static_assert(offsetof(BtBnepSetupRequest, uuidLength) == 1, "BNEP UUID length");
_Static_assert(offsetof(BtBnepSetupRequest, uuids) == 2, "BNEP UUID list");
_Static_assert(sizeof(BtBnepSetupResponse) == 3, "BNEP setup response");
_Static_assert(offsetof(BtBnepSetupResponse, responseCodeBe) == 1, "BNEP setup status");
_Static_assert(sizeof(BtBnepFilterRequest) == 3, "BNEP filter request");
_Static_assert(offsetof(BtBnepFilterRequest, listLengthBe) == 1, "BNEP filter length");
_Static_assert(offsetof(BtBnepFilterRequest, filters) == 3, "BNEP filters");
_Static_assert(sizeof(BtBnepExtension) == 2, "BNEP extension header");
_Static_assert(offsetof(BtBnepExtension, length) == 1, "BNEP extension length");
_Static_assert(offsetof(BtBnepExtension, data) == 2, "BNEP extension data");
_Static_assert(sizeof(BtBnepUuid) == 16, "BNEP 128-bit UUID");
_Static_assert(offsetof(BtBnepUuid, suffix) == 4, "BNEP UUID suffix");

_Static_assert(sizeof(BtEthernetFrame) == 14, "Ethernet frame header");
_Static_assert(offsetof(BtEthernetFrame, payload) == 14, "Ethernet payload");
_Static_assert(sizeof(BtBnepCompressedPacket) == 3, "BNEP compressed header");
_Static_assert(offsetof(BtBnepCompressedPacket, protocolBe) == 1,
	       "BNEP compressed protocol");
_Static_assert(offsetof(BtBnepCompressedPacket, payload) == 3,
	       "BNEP compressed payload");
_Static_assert(sizeof(BtBnepDestinationPacket) == 9, "BNEP destination header");
_Static_assert(offsetof(BtBnepDestinationPacket, destination) == 1,
	       "BNEP destination address");
_Static_assert(offsetof(BtBnepDestinationPacket, protocolBe) == 7,
	       "BNEP destination protocol");
_Static_assert(offsetof(BtBnepDestinationPacket, payload) == 9,
	       "BNEP destination payload");

typedef struct BtBnepInterface {
	BtStack *stack;
	uint64_t localAddress;
	uint64_t deviceAddress;
	struct NetDriver driver;
	struct NetDriverPacket *currentPacket;
	int pendingConnection;
	int connected;
} BtBnepInterface;

_Static_assert(sizeof(BtBnepInterface) == 216, "BNEP interface size");
_Static_assert(_Alignof(BtBnepInterface) == 8, "BNEP interface alignment");
_Static_assert(offsetof(BtBnepInterface, stack) == 0, "BNEP stack");
_Static_assert(offsetof(BtBnepInterface, localAddress) == 8, "BNEP local address");
_Static_assert(offsetof(BtBnepInterface, deviceAddress) == 16, "BNEP peer address");
_Static_assert(offsetof(BtBnepInterface, driver) == 24, "BNEP net driver");
_Static_assert(offsetof(BtBnepInterface, currentPacket) == 200, "BNEP TX packet");
_Static_assert(offsetof(BtBnepInterface, pendingConnection) == 204, "BNEP pending link");
_Static_assert(offsetof(BtBnepInterface, connected) == 208, "BNEP connected link");

extern BtBnepInterface gBtBnepInterface;

typedef struct BtNatMapping {
	BtConnection *connection;
	uint32_t sourceAddress;
	uint32_t destinationAddress;
	uint32_t sourcePort;
	uint32_t destinationPort;
	uint32_t translatedDestinationAddress;
	uint64_t lastUsed;
} BtNatMapping;

typedef struct BtDhcpLease {
	uint8_t hardwareAddress[6];
	uint16_t state;
} BtDhcpLease;

typedef struct BtDhcpOptions {
	uint32_t serverAddress;
	uint32_t assignedAddress;
	uint32_t subnetMask;
	uint8_t hardwareAddress[6];
	uint8_t messageType;
	uint32_t flags;
} BtDhcpOptions;

/* BOOTP fixed fields followed by the DHCP cookie and variable options. */
typedef struct BtDhcpPacket {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint8_t xidBe[4];
	uint8_t secsBe[2];
	uint8_t flagsBe[2];
	uint8_t ciaddrBe[4];
	uint8_t yiaddrBe[4];
	uint8_t siaddrBe[4];
	uint8_t giaddrBe[4];
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
	uint8_t cookie[4];
	uint8_t options[];
} BtDhcpPacket;

typedef struct BtDhcpOption {
	uint8_t code;
	uint8_t length;
	uint8_t data[];
} BtDhcpOption;

_Static_assert(sizeof(BtDhcpOptions) == 24, "DHCP parsed options");
_Static_assert(offsetof(BtDhcpOptions, serverAddress) == 0, "DHCP server");
_Static_assert(offsetof(BtDhcpOptions, assignedAddress) == 4, "DHCP assigned address");
_Static_assert(offsetof(BtDhcpOptions, subnetMask) == 8, "DHCP subnet");
_Static_assert(offsetof(BtDhcpOptions, hardwareAddress) == 12, "DHCP client MAC");
_Static_assert(offsetof(BtDhcpOptions, messageType) == 18, "DHCP message type");
_Static_assert(offsetof(BtDhcpOptions, flags) == 20, "DHCP requested options");
_Static_assert(sizeof(BtDhcpPacket) == 240, "DHCP fixed packet");
_Static_assert(offsetof(BtDhcpPacket, op) == 0, "BOOTP operation");
_Static_assert(offsetof(BtDhcpPacket, htype) == 1, "BOOTP hardware type");
_Static_assert(offsetof(BtDhcpPacket, hlen) == 2, "BOOTP hardware length");
_Static_assert(offsetof(BtDhcpPacket, hops) == 3, "BOOTP relay hops");
_Static_assert(offsetof(BtDhcpPacket, xidBe) == 4, "BOOTP transaction ID");
_Static_assert(offsetof(BtDhcpPacket, secsBe) == 8, "BOOTP elapsed seconds");
_Static_assert(offsetof(BtDhcpPacket, flagsBe) == 10, "BOOTP flags");
_Static_assert(offsetof(BtDhcpPacket, ciaddrBe) == 12, "BOOTP client address");
_Static_assert(offsetof(BtDhcpPacket, yiaddrBe) == 16, "BOOTP assigned address");
_Static_assert(offsetof(BtDhcpPacket, siaddrBe) == 20, "BOOTP server address");
_Static_assert(offsetof(BtDhcpPacket, giaddrBe) == 24, "BOOTP relay address");
_Static_assert(offsetof(BtDhcpPacket, chaddr) == 28, "BOOTP client hardware address");
_Static_assert(offsetof(BtDhcpPacket, sname) == 44, "BOOTP server name");
_Static_assert(offsetof(BtDhcpPacket, file) == 108, "BOOTP boot filename");
_Static_assert(offsetof(BtDhcpPacket, cookie) == 236, "DHCP cookie");
_Static_assert(offsetof(BtDhcpPacket, options) == 240, "DHCP options");
_Static_assert(sizeof(BtDhcpOption) == 2, "DHCP option header");
_Static_assert(offsetof(BtDhcpOption, code) == 0, "DHCP option code");
_Static_assert(offsetof(BtDhcpOption, length) == 1, "DHCP option length");
_Static_assert(offsetof(BtDhcpOption, data) == 2, "DHCP option payload");

extern const uint32_t gBtSniffIntervals[7];
extern const uint8_t gBtDhcpMagicCookie[4];

typedef struct BtNetworkState {
	uint32_t networkAddress;
	uint32_t subnetMask;
	BtNatMapping mappings[1000];
	BtDhcpLease leases[256];
} BtNetworkState;

typedef union BtNetworkPacketBuffer {
	uint8_t bytes[2048];
	struct EthernetArpFrame arp;
} BtNetworkPacketBuffer;

typedef struct BtNetworkPacketContext {
	BtConnection *connection;
	uint32_t sourceAddress;
	uint32_t destinationAddress;
	uint16_t sourcePort;
	uint16_t destinationPort;
} BtNetworkPacketContext;

_Static_assert(sizeof(BtNatMapping) == 32, "NAT mapping");
_Static_assert(offsetof(BtNatMapping, connection) == 0, "NAT connection");
_Static_assert(offsetof(BtNatMapping, sourceAddress) == 4, "NAT source address");
_Static_assert(offsetof(BtNatMapping, destinationAddress) == 8, "NAT destination address");
_Static_assert(offsetof(BtNatMapping, sourcePort) == 12, "NAT source port");
_Static_assert(offsetof(BtNatMapping, destinationPort) == 16, "NAT destination port");
_Static_assert(offsetof(BtNatMapping, translatedDestinationAddress) == 20, "NAT translated destination");
_Static_assert(offsetof(BtNatMapping, lastUsed) == 24, "NAT timestamp");
_Static_assert(sizeof(BtDhcpLease) == 8, "DHCP lease");
_Static_assert(offsetof(BtDhcpLease, hardwareAddress) == 0, "DHCP hardware address");
_Static_assert(offsetof(BtDhcpLease, state) == 6, "DHCP state");
_Static_assert(sizeof(BtNetworkState) == 34056, "Network state");
_Static_assert(offsetof(BtNetworkState, networkAddress) == 0, "Network address");
_Static_assert(offsetof(BtNetworkState, subnetMask) == 4, "Network subnet mask");
_Static_assert(offsetof(BtNetworkState, mappings) == 8, "Network NAT mappings");
_Static_assert(offsetof(BtNetworkState, leases) == 32008, "Network DHCP leases");
_Static_assert(sizeof(BtNetworkPacketBuffer) == 2048, "Network packet working buffer");
_Static_assert(sizeof(BtNetworkPacketContext) == 16, "Network packet context");
_Static_assert(offsetof(BtNetworkPacketContext, connection) == 0, "Packet connection");
_Static_assert(offsetof(BtNetworkPacketContext, sourceAddress) == 4, "Packet source address");
_Static_assert(offsetof(BtNetworkPacketContext, destinationAddress) == 8, "Packet destination address");
_Static_assert(offsetof(BtNetworkPacketContext, sourcePort) == 12, "Packet source port");
_Static_assert(offsetof(BtNetworkPacketContext, destinationPort) == 14, "Packet destination port");

extern BtNetworkState gBtNetworkState;
extern BtNetworkPacketBuffer gBtNetworkPacketBuffer;

int BtSendDhcpReply(BtNetworkPacketContext *context,
		    unsigned int transactionId, const BtDhcpOptions *options,
		    uint8_t messageType);

typedef struct BtTimer {
	struct BtTimer *next;
	struct BtTimer *prev;
	void (*callback)(void *argument);
	void *argument;
	SceInt64 relativeDelay;
	unsigned int *id;
} BtTimer;

typedef struct BtAclPacket {
	uint8_t handleAndFlagsLe[2];
	uint8_t payloadLengthLe[2];
	uint8_t payload[];
} BtAclPacket;

_Static_assert(sizeof(BtAclPacket) == 4, "ACL header");
_Static_assert(offsetof(BtAclPacket, payload) == 4, "ACL payload");

_Static_assert(sizeof(BtTimer) == 32, "Bluetooth timer size");
_Static_assert(_Alignof(BtTimer) == 8, "Bluetooth timer alignment");
_Static_assert(offsetof(BtTimer, next) == 0, "timer next");
_Static_assert(offsetof(BtTimer, prev) == 4, "timer previous");
_Static_assert(offsetof(BtTimer, callback) == 8, "timer callback");
_Static_assert(offsetof(BtTimer, argument) == 12, "timer argument");
_Static_assert(offsetof(BtTimer, relativeDelay) == 16, "timer relative delay");
_Static_assert(offsetof(BtTimer, id) == 24, "timer identifier output");

typedef struct BtHspCommand {
	const char *prefix;
	int (*handler)(BtConnection *connection, int command,
		       const char *text, const char *end);
} BtHspCommand;

_Static_assert(sizeof(BtHspCommand) == 8, "BtHspCommand size");
_Static_assert(offsetof(BtHspCommand, prefix) == 0, "prefix offset");
_Static_assert(offsetof(BtHspCommand, handler) == 4, "handler offset");
extern const BtHspCommand gBtHspCommands[3];
int BtHspSetSpeakerGain(BtConnection *connection, int command,
			const char *text, const char *end);
int BtHspSetMicrophoneGain(BtConnection *connection, int command,
			   const char *text, const char *end);
int BtHspPressButton(BtConnection *connection, int command,
		     const char *text, const char *end);

typedef struct BtEventSubscriber {
	SceUID processId;
	SceUID threadId;
	SceUID callbackId;
	uint64_t eventMask;
	int capacity;
	int count;
	int writePosition;
	int readPosition;
	uint32_t droppedEvents;
	uint32_t reserved;
	SceBtEvent events[16];
} BtEventSubscriber;

_Static_assert(sizeof(BtEventSubscriber) == 304, "BtEventSubscriber size");
_Static_assert(_Alignof(BtEventSubscriber) == 8, "BtEventSubscriber alignment");
_Static_assert(offsetof(BtEventSubscriber, processId) == 0, "processId offset");
_Static_assert(offsetof(BtEventSubscriber, threadId) == 4, "threadId offset");
_Static_assert(offsetof(BtEventSubscriber, callbackId) == 8, "callbackId offset");
_Static_assert(offsetof(BtEventSubscriber, eventMask) == 16, "eventMask offset");
_Static_assert(offsetof(BtEventSubscriber, capacity) == 24, "capacity offset");
_Static_assert(offsetof(BtEventSubscriber, count) == 28, "count offset");
_Static_assert(offsetof(BtEventSubscriber, writePosition) == 32,
	       "writePosition offset");
_Static_assert(offsetof(BtEventSubscriber, readPosition) == 36,
	       "readPosition offset");
_Static_assert(offsetof(BtEventSubscriber, droppedEvents) == 40,
	       "droppedEvents offset");
_Static_assert(offsetof(BtEventSubscriber, reserved) == 44, "reserved offset");
_Static_assert(offsetof(BtEventSubscriber, events) == 48, "events offset");

typedef struct BtHidVuEvent {
	uint64_t deviceAddress;
	uint8_t eventId;
	uint8_t status;
	uint16_t length;
	uint32_t reserved;
	uint8_t data[128];
} BtHidVuEvent;

typedef struct BtHidVuSubscriber {
	SceUID processId;
	SceUID threadId;
	SceUID callbackId;
	int capacity;
	int count;
	int writePosition;
	int readPosition;
	uint16_t vendorId;
	uint16_t productId;
	BtHidVuEvent events[8];
} BtHidVuSubscriber;

_Static_assert(sizeof(BtHidVuEvent) == 144, "HIDVU event size");
_Static_assert(_Alignof(BtHidVuEvent) == 8, "HIDVU event alignment");
_Static_assert(offsetof(BtHidVuEvent, deviceAddress) == 0, "HIDVU address");
_Static_assert(offsetof(BtHidVuEvent, eventId) == 8, "HIDVU event identifier");
_Static_assert(offsetof(BtHidVuEvent, status) == 9, "HIDVU event status");
_Static_assert(offsetof(BtHidVuEvent, length) == 10, "HIDVU data length");
_Static_assert(offsetof(BtHidVuEvent, reserved) == 12, "HIDVU unused word");
_Static_assert(offsetof(BtHidVuEvent, data) == 16, "HIDVU event data");
_Static_assert(sizeof(BtHidVuSubscriber) == 1184, "HIDVU subscriber size");
_Static_assert(_Alignof(BtHidVuSubscriber) == 8, "HIDVU subscriber alignment");
_Static_assert(offsetof(BtHidVuSubscriber, processId) == 0, "HIDVU process");
_Static_assert(offsetof(BtHidVuSubscriber, threadId) == 4, "HIDVU thread");
_Static_assert(offsetof(BtHidVuSubscriber, callbackId) == 8, "HIDVU callback");
_Static_assert(offsetof(BtHidVuSubscriber, capacity) == 12, "HIDVU capacity");
_Static_assert(offsetof(BtHidVuSubscriber, count) == 16, "HIDVU count");
_Static_assert(offsetof(BtHidVuSubscriber, writePosition) == 20,
	       "HIDVU write position");
_Static_assert(offsetof(BtHidVuSubscriber, readPosition) == 24,
	       "HIDVU read position");
_Static_assert(offsetof(BtHidVuSubscriber, vendorId) == 28, "HIDVU vendor");
_Static_assert(offsetof(BtHidVuSubscriber, productId) == 30, "HIDVU product");
_Static_assert(offsetof(BtHidVuSubscriber, events) == 32, "HIDVU events");

typedef struct BtAvrcpVendorHeader {
	uint8_t opcode;
	uint8_t companyId[3];
} BtAvrcpVendorHeader;

_Static_assert(sizeof(BtAvrcpVendorHeader) == 4, "AVRCP vendor header size");
_Static_assert(offsetof(BtAvrcpVendorHeader, companyId) == 1,
	       "AVRCP company identifier offset");
extern const BtAvrcpVendorHeader gBtAvrcpVendorHeader;

void BtDeliverMediaPacket(BtConnection *connection, BtChannel *channel,
			       uint8_t *packet, int length);

/* EIR updates both bytes together; SDP also updates them independently. */
typedef union BtDeviceEirIdentity {
	struct {
		uint8_t vendorIdSource;
		uint8_t handsfreeChannel;
	} fields;
	uint8_t bytes[2];
} BtDeviceEirIdentity;

/* The first 768 bytes are the persistent registered-device record. */
typedef struct BtDevice {
	uint64_t deviceAddress;
	uint32_t deviceClass;
	uint32_t registrationSequence;
	uint8_t linkKey[16];
	uint32_t profiles;
	uint32_t flags;
	union {
		uint32_t vendorProductId;
		struct {
			uint16_t vendorId;
			uint16_t productId;
		};
	};
	uint16_t version;
	uint8_t primary;
	BtDeviceEirIdentity eirIdentity;
	uint8_t handsfreeAgChannel;
	uint8_t headsetChannel;
	uint8_t headsetAgChannel;
	uint8_t objectPushChannel;
	uint8_t imagingChannel;
	uint8_t serialPortChannel;
	uint16_t avrcpVersion;
	uint16_t avrcpFeatures;
	uint8_t reserved[4];
	uint8_t remoteNameStatus;
	uint8_t pageScanRepetitionMode;
	uint16_t clockOffset;
	uint16_t hidDescriptorLength;
	uint8_t reservedExtended[58];
	char name[128];
	uint8_t hidDescriptor[512];
	struct BtDevice *next;
	struct BtDevice *prev;
	uint32_t generation;
	uint64_t lastSeen;
} BtDevice;

_Static_assert(sizeof(BtDeviceEirIdentity) == 2, "EIR identity size");
_Static_assert(_Alignof(BtDeviceEirIdentity) == 1, "EIR identity alignment");
_Static_assert(offsetof(BtDeviceEirIdentity, fields.vendorIdSource) == 0,
	       "vendorIdSource offset");
_Static_assert(offsetof(BtDeviceEirIdentity, fields.handsfreeChannel) == 1,
	       "handsfreeChannel offset");
_Static_assert(sizeof(BtDevice) == 792, "BtDevice size");
_Static_assert(_Alignof(BtDevice) == 8, "BtDevice alignment");
_Static_assert(offsetof(BtDevice, deviceAddress) == 0, "deviceAddress offset");
_Static_assert(offsetof(BtDevice, deviceClass) == 8, "deviceClass offset");
_Static_assert(offsetof(BtDevice, registrationSequence) == 12,
	       "registrationSequence offset");
_Static_assert(offsetof(BtDevice, linkKey) == 16, "linkKey offset");
_Static_assert(offsetof(BtDevice, profiles) == 32, "profiles offset");
_Static_assert(offsetof(BtDevice, flags) == 36, "flags offset");
_Static_assert(offsetof(BtDevice, vendorId) == 40, "vendorId offset");
_Static_assert(offsetof(BtDevice, vendorProductId) == 40,
	       "vendorProductId offset");
_Static_assert(offsetof(BtDevice, productId) == 42, "productId offset");
_Static_assert(offsetof(BtDevice, version) == 44, "version offset");
_Static_assert(offsetof(BtDevice, primary) == 46, "primary offset");
_Static_assert(offsetof(BtDevice, eirIdentity) == 47, "eirIdentity offset");
_Static_assert(offsetof(BtDevice, handsfreeAgChannel) == 49,
	       "handsfreeAgChannel offset");
_Static_assert(offsetof(BtDevice, headsetChannel) == 50, "headsetChannel offset");
_Static_assert(offsetof(BtDevice, headsetAgChannel) == 51,
	       "headsetAgChannel offset");
_Static_assert(offsetof(BtDevice, objectPushChannel) == 52,
	       "objectPushChannel offset");
_Static_assert(offsetof(BtDevice, imagingChannel) == 53, "imagingChannel offset");
_Static_assert(offsetof(BtDevice, serialPortChannel) == 54,
	       "serialPortChannel offset");
_Static_assert(offsetof(BtDevice, avrcpVersion) == 56, "avrcpVersion offset");
_Static_assert(offsetof(BtDevice, avrcpFeatures) == 58, "avrcpFeatures offset");
_Static_assert(offsetof(BtDevice, reserved) == 60, "reserved offset");
_Static_assert(offsetof(BtDevice, remoteNameStatus) == 64,
	       "remoteNameStatus offset");
_Static_assert(offsetof(BtDevice, pageScanRepetitionMode) == 65,
	       "pageScanRepetitionMode offset");
_Static_assert(offsetof(BtDevice, clockOffset) == 66, "clockOffset offset");
_Static_assert(offsetof(BtDevice, hidDescriptorLength) == 68,
	       "hidDescriptorLength offset");
_Static_assert(offsetof(BtDevice, reservedExtended) == 70,
	       "reservedExtended offset");
_Static_assert(offsetof(BtDevice, name) == 128, "name offset");
_Static_assert(offsetof(BtDevice, hidDescriptor) == 256, "hidDescriptor offset");
_Static_assert(offsetof(BtDevice, next) == 768, "next offset");
_Static_assert(offsetof(BtDevice, prev) == 772, "prev offset");
_Static_assert(offsetof(BtDevice, generation) == 776, "generation offset");
_Static_assert(offsetof(BtDevice, lastSeen) == 784, "lastSeen offset");

int BtUpdateDeviceClass(BtDevice *device, uint32_t deviceClass);

typedef struct BtDeviceHistory {
	uint64_t deviceAddress;
	uint64_t lastSeen;
	uint32_t deviceClass;
	uint16_t vendorId;
	uint16_t productId;
	char name[128];
} BtDeviceHistory;

_Static_assert(sizeof(BtDeviceHistory) == 152, "BtDeviceHistory size");
_Static_assert(offsetof(BtDeviceHistory, deviceAddress) == 0,
	       "history deviceAddress offset");
_Static_assert(offsetof(BtDeviceHistory, lastSeen) == 8,
	       "history lastSeen offset");
_Static_assert(offsetof(BtDeviceHistory, deviceClass) == 16,
	       "history deviceClass offset");
_Static_assert(offsetof(BtDeviceHistory, vendorId) == 20,
	       "history vendorId offset");
_Static_assert(offsetof(BtDeviceHistory, productId) == 22,
	       "history productId offset");
_Static_assert(offsetof(BtDeviceHistory, name) == 24, "history name offset");

extern BtDeviceHistory gBtDeviceHistory[8];
BtDevice *BtRestoreDeviceFromHistory(BtStack *stack, uint64_t deviceAddress);

uint8_t *BtReadSdpElement(uint8_t *data, int *length, uint8_t *type);
uint8_t *BtReadSdpUuids(uint8_t *data, uint8_t *end, uint32_t *uuids, int *count);
uint8_t *BtReadSdpAttributeRanges(uint8_t *data, uint8_t *end,
				uint16_t *lower, uint16_t *upper, int *count);
int BtGetLocalServiceClasses(uint8_t *output);
unsigned int BtFindSdpServiceRecords(const uint32_t *uuids, int uuidCount,
				     uint8_t *handles);

typedef struct BtRingBuffer {
	int writePosition;
	int readPosition;
	int used;
	int capacity;
	uint8_t *data;
} BtRingBuffer;

_Static_assert(sizeof(BtRingBuffer) == 20, "BtRingBuffer size");
_Static_assert(offsetof(BtRingBuffer, writePosition) == 0, "writePosition offset");
_Static_assert(offsetof(BtRingBuffer, readPosition) == 4, "readPosition offset");
_Static_assert(offsetof(BtRingBuffer, used) == 8, "used offset");
_Static_assert(offsetof(BtRingBuffer, capacity) == 12, "capacity offset");
_Static_assert(offsetof(BtRingBuffer, data) == 16, "data offset");

/* L2CAP configuration values retain their original little-endian wire form. */
typedef struct BtL2capQos {
	uint8_t flags;
	uint8_t serviceType;
	uint8_t tokenRate[4];
	uint8_t tokenBucketSize[4];
	uint8_t peakBandwidth[4];
	uint8_t latency[4];
	uint8_t delayVariation[4];
} BtL2capQos;

typedef struct BtL2capRetransmission {
	uint8_t mode;
	uint8_t txWindowSize;
	uint8_t maxTransmit;
	uint8_t retransmissionTimeout[2];
	uint8_t monitorTimeout[2];
	uint8_t maxPduSize[2];
} BtL2capRetransmission;

typedef struct BtRfcommChannelState {
	BtRingBuffer transmit;
	uint8_t data[512];
	uint8_t transmitCredits;
	uint8_t pendingCredits;
	uint8_t reserved[6];
	uint32_t dlci;
	uint32_t flags;
	uint32_t profile;
} BtRfcommChannelState;

typedef struct BtAvrcpChannelState {
	uint32_t profiles;
	int continuationPosition;
	uint8_t transactionLabel;
	int receivedButton;
	unsigned int buttonReleaseTimer;
	int sentButton;
	unsigned int buttonRepeatTimer;
	int translatedButton;
	unsigned int translatedButtonTimer;
	uint8_t playStatusRegistered;
	uint8_t trackRegistered;
	uint8_t playStatusLabel;
	uint8_t trackLabel;
	unsigned int requestTimer;
	int volume;
	int queuedButton;
	int translatePlayPause;
} BtAvrcpChannelState;

typedef struct BtBnepChannelState {
	uint32_t profile;
	uint32_t remoteService;
	uint32_t localService;
	unsigned int retryTimer;
	int connected;
} BtBnepChannelState;

typedef union BtChannelProtocol {
	BtRfcommChannelState rfcomm;
	BtAvrcpChannelState avrcp;
	BtBnepChannelState bnep;
} BtChannelProtocol;

/* Callback signatures follow the observed lifecycle and data contracts. */
typedef struct BtChannelOps {
	uintptr_t (*opened)(BtConnection *connection, BtChannel *channel);
	void (*closed)(BtConnection *connection, BtChannel *channel);
	void (*receive)(BtConnection *connection, BtChannel *channel,
			     uint8_t *packet, int length);
	uintptr_t (*requestDisconnect)(BtConnection *connection, BtChannel *channel);
} BtChannelOps;

struct BtChannel {
	BtConnection *connection;
	uint32_t flags;
	int mtu;
	int flushTimeout;
	uint8_t flushConfiguration;
	uint8_t qosConfiguration;
	uint8_t retransmissionConfiguration;
	BtL2capQos qos;
	BtL2capRetransmission retransmission;
	uint8_t reserved[2];
	uint16_t psm;
	uint16_t localCid;
	uint16_t remoteCid;
	uint8_t identifier;
	const BtChannelOps *ops;
	BtChannelProtocol protocol;
};

_Static_assert(sizeof(BtL2capQos) == 22, "L2CAP QoS size");
_Static_assert(offsetof(BtL2capQos, flags) == 0, "QoS flags offset");
_Static_assert(offsetof(BtL2capQos, serviceType) == 1, "QoS service offset");
_Static_assert(offsetof(BtL2capQos, tokenRate) == 2, "QoS rate offset");
_Static_assert(offsetof(BtL2capQos, tokenBucketSize) == 6, "QoS bucket offset");
_Static_assert(offsetof(BtL2capQos, peakBandwidth) == 10, "QoS bandwidth offset");
_Static_assert(offsetof(BtL2capQos, latency) == 14, "QoS latency offset");
_Static_assert(offsetof(BtL2capQos, delayVariation) == 18, "QoS variation offset");
_Static_assert(sizeof(BtL2capRetransmission) == 9, "L2CAP retransmission size");
_Static_assert(offsetof(BtL2capRetransmission, mode) == 0, "retransmission mode");
_Static_assert(offsetof(BtL2capRetransmission, txWindowSize) == 1, "TX window");
_Static_assert(offsetof(BtL2capRetransmission, maxTransmit) == 2, "max transmit");
_Static_assert(offsetof(BtL2capRetransmission, retransmissionTimeout) == 3,
	       "retransmission timeout");
_Static_assert(offsetof(BtL2capRetransmission, monitorTimeout) == 5,
	       "monitor timeout");
_Static_assert(offsetof(BtL2capRetransmission, maxPduSize) == 7, "max PDU");
_Static_assert(sizeof(BtRfcommChannelState) == 552, "RFCOMM channel size");
_Static_assert(offsetof(BtRfcommChannelState, transmit) == 0, "RFCOMM ring");
_Static_assert(offsetof(BtRfcommChannelState, data) == 20, "RFCOMM data");
_Static_assert(offsetof(BtRfcommChannelState, transmitCredits) == 532,
	       "RFCOMM transmit credits");
_Static_assert(offsetof(BtRfcommChannelState, pendingCredits) == 533,
	       "RFCOMM pending credits");
_Static_assert(offsetof(BtRfcommChannelState, reserved) == 534, "RFCOMM unused");
_Static_assert(offsetof(BtRfcommChannelState, dlci) == 540, "RFCOMM DLCI");
_Static_assert(offsetof(BtRfcommChannelState, flags) == 544, "RFCOMM flags");
_Static_assert(offsetof(BtRfcommChannelState, profile) == 548, "RFCOMM profile");
_Static_assert(sizeof(BtAvrcpChannelState) == 56, "AVRCP channel size");
_Static_assert(offsetof(BtAvrcpChannelState, profiles) == 0, "AVRCP profiles");
_Static_assert(offsetof(BtAvrcpChannelState, continuationPosition) == 4,
	       "AVRCP continuation");
_Static_assert(offsetof(BtAvrcpChannelState, transactionLabel) == 8, "AVRCP label");
_Static_assert(offsetof(BtAvrcpChannelState, receivedButton) == 12, "RX button");
_Static_assert(offsetof(BtAvrcpChannelState, buttonReleaseTimer) == 16,
	       "button release timer");
_Static_assert(offsetof(BtAvrcpChannelState, sentButton) == 20, "TX button");
_Static_assert(offsetof(BtAvrcpChannelState, buttonRepeatTimer) == 24,
	       "button repeat timer");
_Static_assert(offsetof(BtAvrcpChannelState, translatedButton) == 28,
	       "translated button");
_Static_assert(offsetof(BtAvrcpChannelState, translatedButtonTimer) == 32,
	       "translated button timer");
_Static_assert(offsetof(BtAvrcpChannelState, playStatusRegistered) == 36,
	       "play-status registration");
_Static_assert(offsetof(BtAvrcpChannelState, trackRegistered) == 37,
	       "track registration");
_Static_assert(offsetof(BtAvrcpChannelState, playStatusLabel) == 38,
	       "play-status label");
_Static_assert(offsetof(BtAvrcpChannelState, trackLabel) == 39, "track label");
_Static_assert(offsetof(BtAvrcpChannelState, requestTimer) == 40, "request timer");
_Static_assert(offsetof(BtAvrcpChannelState, volume) == 44, "volume");
_Static_assert(offsetof(BtAvrcpChannelState, queuedButton) == 48, "queued button");
_Static_assert(offsetof(BtAvrcpChannelState, translatePlayPause) == 52,
	       "play-pause translation");
_Static_assert(sizeof(BtBnepChannelState) == 20, "BNEP channel size");
_Static_assert(offsetof(BtBnepChannelState, profile) == 0, "BNEP profile");
_Static_assert(offsetof(BtBnepChannelState, remoteService) == 4, "BNEP remote UUID");
_Static_assert(offsetof(BtBnepChannelState, localService) == 8, "BNEP local UUID");
_Static_assert(offsetof(BtBnepChannelState, retryTimer) == 12, "BNEP retry timer");
_Static_assert(offsetof(BtBnepChannelState, connected) == 16, "BNEP connected");
_Static_assert(sizeof(BtChannelProtocol) == 552, "channel protocol union size");
_Static_assert(sizeof(BtChannelOps) == 16, "channel callback table size");
_Static_assert(offsetof(BtChannelOps, opened) == 0, "channel opened callback");
_Static_assert(offsetof(BtChannelOps, closed) == 4, "channel closed callback");
_Static_assert(offsetof(BtChannelOps, receive) == 8, "channel receive callback");
_Static_assert(offsetof(BtChannelOps, requestDisconnect) == 12,
	       "channel disconnect callback");
_Static_assert(sizeof(BtChannel) == 616, "channel size");
_Static_assert(offsetof(BtChannel, connection) == 0, "channel connection");
_Static_assert(offsetof(BtChannel, flags) == 4, "channel flags");
_Static_assert(offsetof(BtChannel, mtu) == 8, "channel MTU");
_Static_assert(offsetof(BtChannel, flushTimeout) == 12, "channel flush timeout");
_Static_assert(offsetof(BtChannel, flushConfiguration) == 16, "flush status");
_Static_assert(offsetof(BtChannel, qosConfiguration) == 17, "QoS status");
_Static_assert(offsetof(BtChannel, retransmissionConfiguration) == 18,
	       "retransmission status");
_Static_assert(offsetof(BtChannel, qos) == 19, "channel QoS");
_Static_assert(offsetof(BtChannel, retransmission) == 41, "channel retransmission");
_Static_assert(offsetof(BtChannel, reserved) == 50, "channel unused");
_Static_assert(offsetof(BtChannel, psm) == 52, "channel PSM");
_Static_assert(offsetof(BtChannel, localCid) == 54, "local CID");
_Static_assert(offsetof(BtChannel, remoteCid) == 56, "remote CID");
_Static_assert(offsetof(BtChannel, identifier) == 58, "channel identifier");
_Static_assert(offsetof(BtChannel, ops) == 60, "channel callbacks");
_Static_assert(offsetof(BtChannel, protocol) == 64, "channel protocol state");

typedef struct BtSignal {
	uint32_t events;
	uint32_t waiters;
	SceUID eventFlag;
} BtSignal;

typedef struct BtL2capReceiveBuffer {
	uint16_t expectedLength;
	uint16_t channelId;
	uint16_t used;
	uint8_t data[2050];
} BtL2capReceiveBuffer;

typedef struct BtSdpReceiveBuffer {
	uint8_t data[2048];
	int length;
} BtSdpReceiveBuffer;

typedef struct BtSdpHeader {
	uint8_t pduId;
	uint8_t transactionIdBe[2];
	uint8_t parameterLengthBe[2];
} BtSdpHeader;

typedef struct BtSdpPacket {
	BtSdpHeader header;
	uint8_t parameters[];
} BtSdpPacket;

typedef struct BtSdpContinuation {
	uint8_t length;
	uint8_t data[];
} BtSdpContinuation;

typedef struct BtSdpSearchResponse {
	uint8_t totalCountBe[2];
	uint8_t currentCountBe[2];
	uint8_t handlesAndContinuation[];
} BtSdpSearchResponse;

typedef struct BtSdpAttributeResponse {
	uint8_t byteCountBe[2];
	uint8_t attributesAndContinuation[];
} BtSdpAttributeResponse;

_Static_assert(sizeof(BtSdpReceiveBuffer) == 2052, "SDP accumulated response");
_Static_assert(offsetof(BtSdpReceiveBuffer, length) == 2048, "SDP response length");
_Static_assert(sizeof(BtSdpHeader) == 5, "SDP packet header");
_Static_assert(offsetof(BtSdpHeader, transactionIdBe) == 1, "SDP transaction");
_Static_assert(offsetof(BtSdpHeader, parameterLengthBe) == 3, "SDP parameter length");
_Static_assert(sizeof(BtSdpPacket) == 5, "SDP variable packet");
_Static_assert(offsetof(BtSdpPacket, parameters) == 5, "SDP parameters");
_Static_assert(sizeof(BtSdpContinuation) == 1, "SDP continuation header");
_Static_assert(offsetof(BtSdpContinuation, data) == 1, "SDP continuation data");
_Static_assert(sizeof(BtSdpSearchResponse) == 4, "SDP search response");
_Static_assert(offsetof(BtSdpSearchResponse, currentCountBe) == 2,
	       "SDP current record count");
_Static_assert(offsetof(BtSdpSearchResponse, handlesAndContinuation) == 4,
	       "SDP handles and continuation");
_Static_assert(sizeof(BtSdpAttributeResponse) == 2, "SDP attribute response");
_Static_assert(offsetof(BtSdpAttributeResponse, attributesAndContinuation) == 2,
	       "SDP attributes and continuation");

typedef struct BtPacketCompletion {
	int packetCount;
	uint32_t profile;
	uintptr_t argument;
} BtPacketCompletion;

typedef struct BtAvdtpEndpoint {
	uint8_t seidFlags;
	uint8_t mediaFlags;
} BtAvdtpEndpoint;

/* AVDTP signaling and service-capability wire records. */
typedef struct BtAvdtpHeader {
	uint8_t header;
	uint8_t signalId;
} BtAvdtpHeader;

typedef struct BtAvdtpSignalPacket {
	BtAvdtpHeader header;
	uint8_t payload[];
} BtAvdtpSignalPacket;

typedef struct BtAvdtpSeidPacket {
	BtAvdtpHeader header;
	uint8_t seid;
} BtAvdtpSeidPacket;

typedef struct BtAvdtpSetConfiguration {
	BtAvdtpHeader header;
	uint8_t acceptorSeid;
	uint8_t initiatorSeid;
	uint8_t capabilities[];
} BtAvdtpSetConfiguration;

typedef struct BtAvdtpCapabilityHeader {
	uint8_t category;
	uint8_t length;
} BtAvdtpCapabilityHeader;

typedef struct BtAvdtpServiceCapability {
	BtAvdtpCapabilityHeader header;
	uint8_t body[];
} BtAvdtpServiceCapability;

typedef struct BtAvdtpSbcCapability {
	BtAvdtpCapabilityHeader header;
	uint8_t mediaType;
	uint8_t codecType;
	uint8_t frequencyChannels;
	uint8_t blocksSubbandsAllocation;
	uint8_t minimumBitpool;
	uint8_t maximumBitpool;
} BtAvdtpSbcCapability;

typedef struct BtAvdtpContentProtection {
	BtAvdtpCapabilityHeader header;
	uint8_t typeLe[2];
} BtAvdtpContentProtection;

typedef struct BtAvdtpCapabilities {
	BtAvdtpCapabilityHeader transport;
	BtAvdtpSbcCapability sbc;
	BtAvdtpContentProtection contentProtection;
} BtAvdtpCapabilities;

_Static_assert(sizeof(BtAvdtpHeader) == 2, "AVDTP signaling header");
_Static_assert(offsetof(BtAvdtpHeader, signalId) == 1, "AVDTP signal ID");
_Static_assert(sizeof(BtAvdtpSignalPacket) == 2, "AVDTP variable signaling");
_Static_assert(offsetof(BtAvdtpSignalPacket, payload) == 2, "AVDTP payload");
_Static_assert(sizeof(BtAvdtpSeidPacket) == 3, "AVDTP endpoint request");
_Static_assert(offsetof(BtAvdtpSeidPacket, seid) == 2, "AVDTP endpoint ID");
_Static_assert(sizeof(BtAvdtpSetConfiguration) == 4, "AVDTP configuration");
_Static_assert(offsetof(BtAvdtpSetConfiguration, acceptorSeid) == 2, "AVDTP acceptor");
_Static_assert(offsetof(BtAvdtpSetConfiguration, initiatorSeid) == 3, "AVDTP initiator");
_Static_assert(offsetof(BtAvdtpSetConfiguration, capabilities) == 4, "AVDTP configuration capabilities");
_Static_assert(sizeof(BtAvdtpCapabilityHeader) == 2, "AVDTP capability header");
_Static_assert(offsetof(BtAvdtpCapabilityHeader, length) == 1, "AVDTP capability length");
_Static_assert(sizeof(BtAvdtpServiceCapability) == 2, "AVDTP variable capability");
_Static_assert(offsetof(BtAvdtpServiceCapability, body) == 2, "AVDTP capability body");
_Static_assert(sizeof(BtAvdtpSbcCapability) == 8, "AVDTP SBC capability");
_Static_assert(offsetof(BtAvdtpSbcCapability, mediaType) == 2, "AVDTP media type");
_Static_assert(offsetof(BtAvdtpSbcCapability, codecType) == 3, "AVDTP codec type");
_Static_assert(offsetof(BtAvdtpSbcCapability, frequencyChannels) == 4, "AVDTP SBC frequency and channels");
_Static_assert(offsetof(BtAvdtpSbcCapability, blocksSubbandsAllocation) == 5, "AVDTP SBC blocks and allocation");
_Static_assert(offsetof(BtAvdtpSbcCapability, minimumBitpool) == 6, "AVDTP minimum bitpool");
_Static_assert(offsetof(BtAvdtpSbcCapability, maximumBitpool) == 7, "AVDTP maximum bitpool");
_Static_assert(sizeof(BtAvdtpContentProtection) == 4, "AVDTP content protection");
_Static_assert(offsetof(BtAvdtpContentProtection, typeLe) == 2, "AVDTP protection type");
_Static_assert(sizeof(BtAvdtpCapabilities) == 14, "AVDTP service capabilities");
_Static_assert(offsetof(BtAvdtpCapabilities, sbc) == 2, "AVDTP SBC service");
_Static_assert(offsetof(BtAvdtpCapabilities, contentProtection) == 10, "AVDTP protection service");

extern const BtAvdtpCapabilities gBtAvdtpReceiveCapabilities;
extern const BtAvdtpEndpoint gBtAvdtpLocalEndpoints[2];
extern const BtAvdtpCapabilities gBtAvdtpTransmitCapabilities;

typedef struct BtAvdtpStream {
	int state;
	uint8_t localSeid;
	uint8_t remoteSeid;
	uint8_t capabilities[16];
	int capabilityLength;
	BtChannel *channel;
	uint8_t transactionLabel;
	uint8_t startDeferred;
} BtAvdtpStream;

typedef struct BtAvdtpState {
	uint32_t requestedProfile;
	uint8_t transactionLabel;
	BtAvdtpEndpoint remoteEndpoints[8];
	uint8_t remainingEndpointBytes;
	uint8_t endpointPosition;
	uint8_t pendingLocalEndpoint;
	BtChannel *signalingChannel;
	BtAvdtpStream streams[3];
	int minimumBitpool;
	int maximumBitpool;
	int blockCount;
	int channelMode;
	int transmitContentProtection;
	int receiveContentProtection;
	int receiveSampleRate;
	uint8_t controllerSleepState;
} BtAvdtpState;

typedef struct BtHidQueue {
	SceBtHidRequest *sendHead;
	SceBtHidRequest *sendTail;
	SceBtHidRequest *readHead;
	SceBtHidRequest *readTail;
	int active;
	int sendPosition;
	int lastPacketLength;
	int receivePosition;
} BtHidQueue;

typedef struct BtHidAuthenticationMessage {
	uint8_t prefix[32];
	uint8_t challenges[2][16];
	uint8_t nonce[20];
} BtHidAuthenticationMessage;

typedef struct BtHidAuthenticationBuffer {
	BtHidAuthenticationMessage message;
	uint8_t digest[20];
} BtHidAuthenticationBuffer;

typedef struct BtHidAuthenticationResponse {
	uint8_t reportId;
	uint8_t sequence;
	uint8_t status;
	uint8_t digest[20];
	uint8_t nonce[20];
	uint8_t checksum[4];
} BtHidAuthenticationResponse;

_Static_assert(sizeof(BtHidAuthenticationMessage) == 84, "HID authentication message");
_Static_assert(offsetof(BtHidAuthenticationMessage, prefix) == 0, "authentication prefix");
_Static_assert(offsetof(BtHidAuthenticationMessage, challenges) == 32, "challenges");
_Static_assert(offsetof(BtHidAuthenticationMessage, nonce) == 64, "authentication nonce");
_Static_assert(sizeof(BtHidAuthenticationBuffer) == 104, "HID authentication buffer");
_Static_assert(offsetof(BtHidAuthenticationBuffer, message) == 0, "authentication input");
_Static_assert(offsetof(BtHidAuthenticationBuffer, digest) == 84, "authentication digest");
_Static_assert(sizeof(BtHidAuthenticationResponse) == 47, "HID authentication response");
_Static_assert(offsetof(BtHidAuthenticationResponse, reportId) == 0, "response report ID");
_Static_assert(offsetof(BtHidAuthenticationResponse, sequence) == 1, "response sequence");
_Static_assert(offsetof(BtHidAuthenticationResponse, status) == 2, "response status");
_Static_assert(offsetof(BtHidAuthenticationResponse, digest) == 3, "response digest");
_Static_assert(offsetof(BtHidAuthenticationResponse, nonce) == 23, "response nonce");
_Static_assert(offsetof(BtHidAuthenticationResponse, checksum) == 43, "response checksum");

typedef struct BtHidState {
	BtHidQueue control;
	BtHidQueue interrupt;
	SceBtHidRequest featureRequest;
	unsigned int featureTimer;
	uint8_t challenges[2][16];
	union {
		uint8_t featureResponse[47];
		BtHidAuthenticationResponse authenticationResponse;
	};
	uint8_t sequence;
	uint8_t retryCount;
	uint8_t fallback;
	uint64_t challengeDeadline;
	unsigned int keepaliveTimer;
	unsigned int statusTimer;
	unsigned int requestTimer;
	uint8_t challengeReport[38];
	SceBtHidRequest statusRequest;
	uint8_t statusResponse[53];
	/* Only the whole-connection reset touches these bytes. */
	uint8_t reserved[239];
} BtHidState;

typedef struct BtHspState {
	uint64_t connectedTime;
	unsigned int buttonCount;
} BtHspState;

typedef struct BtLinkStatistics {
	uint64_t queryTime;
	union {
		struct {
			uint64_t l2capReceivePackets;
			uint64_t l2capReceiveBytes;
			uint64_t l2capTransmitPackets;
			uint64_t l2capTransmitBytes;
		};
		uint64_t scoPacketStatusCounts[4];
	};
	uint64_t aclTransmitPackets;
	uint64_t aclTransmitBytes;
	uint64_t aclReceivePackets;
	uint64_t aclReceiveBytes;
} BtLinkStatistics;

typedef struct BtAudioStatistics {
	int queuedPcmBytes;
	int targetBufferedBytes;
	int prebuffering;
	int skipBytes;
	int silenceBytes;
	int pendingPcmBytes;
	unsigned int underruns;
	unsigned int overruns;
	int ringUsedBytes;
	unsigned int skippedBytes;
	unsigned int concealedBytes;
	unsigned int startupSkippedBytes;
	unsigned int adjustmentSkippedBytes;
	unsigned int adjustmentSilenceBytes;
	uint64_t contiguousPackets;
	uint64_t intervalUs;
	uint8_t unwritten[8];
} BtAudioStatistics;

_Static_assert(sizeof(BtAudioStatistics) == 80, "audio statistics copyout");
_Static_assert(offsetof(BtAudioStatistics, contiguousPackets) == 56,
	       "audio statistics packet count");
_Static_assert(offsetof(BtAudioStatistics, intervalUs) == 64,
	       "audio statistics interval");
_Static_assert(offsetof(BtAudioStatistics, unwritten) == 72,
	       "audio statistics untouched copyout bytes");

typedef struct BtAudioQueueStatistics {
	int underruns;
	int overruns;
	int pendingPackets;
} BtAudioQueueStatistics;

_Static_assert(sizeof(BtAudioQueueStatistics) == 12, "audio queue statistics");
_Static_assert(offsetof(BtAudioQueueStatistics, underruns) == 0, "underruns");
_Static_assert(offsetof(BtAudioQueueStatistics, overruns) == 4, "overruns");
_Static_assert(offsetof(BtAudioQueueStatistics, pendingPackets) == 8, "pending packets");

typedef struct BtHidRawPacket {
	uint8_t deviceAddress[6];
	uint8_t psm;
	uint8_t payload[4089];
} BtHidRawPacket;

_Static_assert(sizeof(BtHidRawPacket) == 4096, "raw HID packet buffer");
_Static_assert(offsetof(BtHidRawPacket, deviceAddress) == 0, "raw HID address");
_Static_assert(offsetof(BtHidRawPacket, psm) == 6, "raw HID PSM");
_Static_assert(offsetof(BtHidRawPacket, payload) == 7, "raw HID payload");

typedef struct BtVendorConnectionSetting {
	uint8_t deviceAddress[8];
	uint8_t value;
} BtVendorConnectionSetting;

typedef struct BtInquiryScanActivity {
	uint16_t interval;
	uint16_t window;
} BtInquiryScanActivity;

_Static_assert(sizeof(BtVendorConnectionSetting) == 9, "vendor setting wire size");
_Static_assert(offsetof(BtVendorConnectionSetting, value) == 8, "vendor setting byte");
_Static_assert(sizeof(BtInquiryScanActivity) == 4, "inquiry scan activity");
_Static_assert(offsetof(BtInquiryScanActivity, interval) == 0, "scan interval");
_Static_assert(offsetof(BtInquiryScanActivity, window) == 2, "scan window");

typedef struct BtLinkInfoForTest {
	uint64_t l2capReceivePackets;
	uint64_t l2capReceiveBytes;
	uint64_t l2capTransmitPackets;
	uint64_t l2capTransmitBytes;
	uint64_t aclTransmitPackets;
	uint64_t aclTransmitBytes;
	uint64_t aclReceivePackets;
	uint64_t aclReceiveBytes;
	uint16_t interval;
	uint8_t linkQuality;
	int8_t rssi;
	uint8_t afhMode;
	uint8_t afhMap[10];
	uint8_t linkState;
} BtLinkInfoForTest;

_Static_assert(sizeof(BtLinkInfoForTest) == 80, "test link info size");
_Static_assert(offsetof(BtLinkInfoForTest, l2capReceivePackets) == 0, "RX packets");
_Static_assert(offsetof(BtLinkInfoForTest, l2capReceiveBytes) == 8, "RX bytes");
_Static_assert(offsetof(BtLinkInfoForTest, l2capTransmitPackets) == 16, "TX packets");
_Static_assert(offsetof(BtLinkInfoForTest, l2capTransmitBytes) == 24, "TX bytes");
_Static_assert(offsetof(BtLinkInfoForTest, aclTransmitPackets) == 32, "ACL TX packets");
_Static_assert(offsetof(BtLinkInfoForTest, aclTransmitBytes) == 40, "ACL TX bytes");
_Static_assert(offsetof(BtLinkInfoForTest, aclReceivePackets) == 48, "ACL RX packets");
_Static_assert(offsetof(BtLinkInfoForTest, aclReceiveBytes) == 56, "ACL RX bytes");
_Static_assert(offsetof(BtLinkInfoForTest, interval) == 64, "link interval");
_Static_assert(offsetof(BtLinkInfoForTest, linkQuality) == 66, "link quality");
_Static_assert(offsetof(BtLinkInfoForTest, rssi) == 67, "link RSSI");
_Static_assert(offsetof(BtLinkInfoForTest, afhMode) == 68, "link AFH mode");
_Static_assert(offsetof(BtLinkInfoForTest, afhMap) == 69, "link AFH map");
_Static_assert(offsetof(BtLinkInfoForTest, linkState) == 79, "link state");

typedef struct BtInquiryResultForTest {
	uint64_t deviceAddress;
	uint32_t deviceClass;
	uint16_t clockOffset;
	uint8_t pageScanRepetitionMode;
	int8_t rssi;
	uint8_t extendedInquiryResponse[240];
} BtInquiryResultForTest;

_Static_assert(sizeof(BtInquiryResultForTest) == 256, "test inquiry result size");
_Static_assert(offsetof(BtInquiryResultForTest, deviceAddress) == 0, "inquiry MAC");
_Static_assert(offsetof(BtInquiryResultForTest, deviceClass) == 8, "inquiry class");
_Static_assert(offsetof(BtInquiryResultForTest, clockOffset) == 12, "inquiry clock");
_Static_assert(offsetof(BtInquiryResultForTest, pageScanRepetitionMode) == 14,
	       "inquiry page scan");
_Static_assert(offsetof(BtInquiryResultForTest, rssi) == 15, "inquiry RSSI");
_Static_assert(offsetof(BtInquiryResultForTest, extendedInquiryResponse) == 16,
	       "extended inquiry response");

extern BtInquiryResultForTest gBtTestInquiryResult;

typedef struct BtAvrcpFragment {
	uint8_t data[256];
	int length;
} BtAvrcpFragment;

typedef struct BtRfcommCommandHeader {
	uint8_t type;
	uint8_t length;
} BtRfcommCommandHeader;

typedef struct BtRfcommCommandPacket {
	BtRfcommCommandHeader header;
	uint8_t payload[];
} BtRfcommCommandPacket;

typedef struct BtRfcommParameterNegotiation {
	BtRfcommCommandHeader header;
	uint8_t dlci;
	uint8_t flowControl;
	uint8_t priority;
	uint8_t acknowledgementTimer;
	uint8_t frameSizeLe[2];
	uint8_t maxRetransmissions;
	uint8_t credits;
} BtRfcommParameterNegotiation;

typedef struct BtRfcommModemStatus {
	BtRfcommCommandHeader header;
	uint8_t dlci;
	uint8_t signals;
} BtRfcommModemStatus;

typedef struct BtRfcommRemotePortSettings {
	BtRfcommCommandHeader header;
	uint8_t dlci;
	uint8_t baudRate;
	uint8_t dataFormat;
	uint8_t flowControl;
	uint8_t xon;
	uint8_t xoff;
	uint8_t parameterMask[2];
} BtRfcommRemotePortSettings;

uintptr_t BtCompleteRfcommConnection(BtConnection *connection, BtChannel *channel);

_Static_assert(sizeof(BtRfcommCommandHeader) == 2, "RFCOMM command header");
_Static_assert(offsetof(BtRfcommCommandHeader, length) == 1, "RFCOMM command length");
_Static_assert(sizeof(BtRfcommCommandPacket) == 2, "RFCOMM variable command header");
_Static_assert(offsetof(BtRfcommCommandPacket, payload) == 2, "RFCOMM command payload");
_Static_assert(sizeof(BtRfcommParameterNegotiation) == 10, "RFCOMM parameter negotiation");
_Static_assert(offsetof(BtRfcommParameterNegotiation, dlci) == 2, "RFCOMM PN DLCI");
_Static_assert(offsetof(BtRfcommParameterNegotiation, flowControl) == 3, "RFCOMM PN flow control");
_Static_assert(offsetof(BtRfcommParameterNegotiation, priority) == 4, "RFCOMM PN priority");
_Static_assert(offsetof(BtRfcommParameterNegotiation, acknowledgementTimer) == 5, "RFCOMM PN acknowledgement timer");
_Static_assert(offsetof(BtRfcommParameterNegotiation, frameSizeLe) == 6, "RFCOMM PN frame size");
_Static_assert(offsetof(BtRfcommParameterNegotiation, maxRetransmissions) == 8, "RFCOMM PN retransmissions");
_Static_assert(offsetof(BtRfcommParameterNegotiation, credits) == 9, "RFCOMM PN credits");
_Static_assert(sizeof(BtRfcommModemStatus) == 4, "RFCOMM modem status");
_Static_assert(offsetof(BtRfcommModemStatus, dlci) == 2, "RFCOMM modem DLCI");
_Static_assert(offsetof(BtRfcommModemStatus, signals) == 3, "RFCOMM modem signals");
_Static_assert(sizeof(BtRfcommRemotePortSettings) == 10, "RFCOMM remote port settings");
_Static_assert(offsetof(BtRfcommRemotePortSettings, dlci) == 2, "RFCOMM RPN DLCI");
_Static_assert(offsetof(BtRfcommRemotePortSettings, baudRate) == 3, "RFCOMM RPN baud rate");
_Static_assert(offsetof(BtRfcommRemotePortSettings, dataFormat) == 4, "RFCOMM RPN data format");
_Static_assert(offsetof(BtRfcommRemotePortSettings, flowControl) == 5, "RFCOMM RPN flow control");
_Static_assert(offsetof(BtRfcommRemotePortSettings, xon) == 6, "RFCOMM RPN XON");
_Static_assert(offsetof(BtRfcommRemotePortSettings, xoff) == 7, "RFCOMM RPN XOFF");
_Static_assert(offsetof(BtRfcommRemotePortSettings, parameterMask) == 8, "RFCOMM RPN mask");

typedef struct BtAvctpHeader {
	uint8_t header;
	uint8_t profileIdBe[2];
} BtAvctpHeader;

typedef struct BtAvcHeader {
	BtAvctpHeader transport;
	uint8_t ctype;
	uint8_t subunit;
	uint8_t opcode;
} BtAvcHeader;

typedef struct BtAvrcpPassthroughPacket {
	BtAvcHeader header;
	uint8_t operationId;
	uint8_t operandLength;
	uint8_t operands[];
} BtAvrcpPassthroughPacket;

typedef struct BtAvrcpVendorPacket {
	BtAvcHeader header;
	uint8_t companyId[3];
	uint8_t pduId;
	uint8_t packetType;
	uint8_t parameterLengthBe[2];
	uint8_t parameters[];
} BtAvrcpVendorPacket;

_Static_assert(sizeof(BtAvctpHeader) == 3, "AVCTP header");
_Static_assert(offsetof(BtAvctpHeader, profileIdBe) == 1, "AVCTP profile ID");
_Static_assert(sizeof(BtAvcHeader) == 6, "AVC header");
_Static_assert(offsetof(BtAvcHeader, ctype) == 3, "AVC command type");
_Static_assert(offsetof(BtAvcHeader, subunit) == 4, "AVC subunit");
_Static_assert(offsetof(BtAvcHeader, opcode) == 5, "AVC opcode");
_Static_assert(sizeof(BtAvrcpPassthroughPacket) == 8, "AVRCP passthrough header");
_Static_assert(offsetof(BtAvrcpPassthroughPacket, operationId) == 6, "AVRCP operation");
_Static_assert(offsetof(BtAvrcpPassthroughPacket, operandLength) == 7, "AVRCP operand length");
_Static_assert(offsetof(BtAvrcpPassthroughPacket, operands) == 8, "AVRCP operands");
_Static_assert(sizeof(BtAvrcpVendorPacket) == 13, "AVRCP vendor header");
_Static_assert(offsetof(BtAvrcpVendorPacket, companyId) == 6, "AVRCP company");
_Static_assert(offsetof(BtAvrcpVendorPacket, pduId) == 9, "AVRCP PDU");
_Static_assert(offsetof(BtAvrcpVendorPacket, packetType) == 10, "AVRCP packet type");
_Static_assert(offsetof(BtAvrcpVendorPacket, parameterLengthBe) == 11, "AVRCP parameter length");
_Static_assert(offsetof(BtAvrcpVendorPacket, parameters) == 13, "AVRCP parameters");

extern uint32_t gBtAvrcpTitleLength;
extern int gBtAvrcpPlayStatusKnown;
extern int gBtAvrcpPlayStatus;
extern char gBtAvrcpTitle[1025];
extern const uint8_t gBtAvrcpGroupNavigation[5];

struct BtConnection {
	BtStack *stack;
	uint32_t flags;
	int linkMode;
	int interval;
	int authenticationRetries;
	unsigned int authenticationTimer;
	uint64_t deviceAddress;
	uint64_t remoteFeatures;
	uint64_t remoteExtendedFeatures;
	uint16_t aclHandle;
	uint16_t scoHandle;
	int outstandingAclPackets;
	uint8_t reservedPrefix[8];
	BtL2capReceiveBuffer l2capReceive;
	BtPacketCompletion completions[256];
	int completionCount;
	int completionWritePosition;
	int completionReadPosition;
	uint32_t currentServiceClass;
	uint8_t reservedSdp[72];
	uint16_t sdpTransactionId;
	BtDevice *device;
	uint8_t sspIoCapability;
	uint8_t hfpHandsfreeChannel;
	uint8_t hfpAudioGatewayChannel;
	uint8_t hspHeadsetChannel;
	uint8_t hspAudioGatewayChannel;
	uint8_t objectPushChannel;
	uint8_t imagingChannel;
	uint8_t serialPortChannel;
	BtChannel channels[16];
	BtAvdtpState avdtp;
	BtHidState hid;
	BtHspState hsp;
	uint32_t connectedProfiles;
	uint32_t connectingProfile;
	uint32_t startingProfiles;
	uint32_t activeProfiles;
	uint32_t stoppingProfiles;
	uint32_t pendingStartProfiles;
	uint32_t excludedProfiles;
	uint8_t linkQuality;
	int8_t rssi;
	uint8_t afhMode;
	uint8_t afhMap[10];
	BtLinkStatistics statistics;
	BtAvrcpFragment avrcpCommand;
	BtAvrcpFragment avrcpResponse;
	unsigned int profileTimer;
	unsigned int disconnectTimer;
	uint8_t *echoResponseBuffer;
	int echoResponseCapacity;
	SceUID echoResponseProcessId;
	uint32_t reservedEcho;
	BtRingBuffer serialReceive;
	uint8_t serialReceiveData[256];
	uint8_t hidVuCommand;
	int hidVuReportCapacity;
	uint32_t reservedHidVu;
	int linkState;
	unsigned int packetType;
};

_Static_assert(sizeof(BtL2capReceiveBuffer) == 2056, "L2CAP receive buffer");
_Static_assert(offsetof(BtL2capReceiveBuffer, data) == 6, "L2CAP receive data");
_Static_assert(sizeof(BtPacketCompletion) == 12, "packet completion");
_Static_assert(sizeof(BtAvdtpEndpoint) == 2, "AVDTP endpoint");
_Static_assert(sizeof(BtAvdtpStream) == 36, "AVDTP stream");
_Static_assert(offsetof(BtAvdtpStream, capabilities) == 6, "AVDTP capabilities");
_Static_assert(offsetof(BtAvdtpStream, capabilityLength) == 24,
	       "AVDTP capability length");
_Static_assert(offsetof(BtAvdtpStream, channel) == 28, "AVDTP channel");
_Static_assert(offsetof(BtAvdtpStream, transactionLabel) == 32,
	       "AVDTP stream transaction");
_Static_assert(sizeof(BtAvdtpState) == 168, "AVDTP state");
_Static_assert(offsetof(BtAvdtpState, signalingChannel) == 24,
	       "AVDTP signaling channel");
_Static_assert(offsetof(BtAvdtpState, streams) == 28, "AVDTP streams");
_Static_assert(offsetof(BtAvdtpState, minimumBitpool) == 136, "minimum bitpool");
_Static_assert(offsetof(BtAvdtpState, controllerSleepState) == 164,
	       "AVDTP controller sleep state");
_Static_assert(sizeof(BtHidQueue) == 32, "HID queue");
_Static_assert(sizeof(BtHidState) == 552, "HID connection state");
_Static_assert(offsetof(BtHidState, featureRequest) == 64, "HID feature request");
_Static_assert(offsetof(BtHidState, featureTimer) == 88, "HID feature timer");
_Static_assert(offsetof(BtHidState, challenges) == 92, "HID challenges");
_Static_assert(offsetof(BtHidState, featureResponse) == 124, "HID feature response");
_Static_assert(offsetof(BtHidState, authenticationResponse) == 124, "HID authentication response");
_Static_assert(offsetof(BtHidState, challengeDeadline) == 176,
	       "HID challenge deadline");
_Static_assert(offsetof(BtHidState, challengeReport) == 196, "HID challenge report");
_Static_assert(offsetof(BtHidState, statusRequest) == 236, "HID status request");
_Static_assert(offsetof(BtHidState, statusResponse) == 260, "HID status response");
_Static_assert(sizeof(BtHspState) == 16, "HSP state");
_Static_assert(sizeof(BtLinkStatistics) == 72, "Bluetooth link statistics");
_Static_assert(sizeof(BtAvrcpFragment) == 260, "AVRCP reassembly");
_Static_assert(offsetof(BtAvrcpFragment, length) == 256, "AVRCP assembled length");
_Static_assert(sizeof(BtConnection) == 16848, "Bluetooth connection");
_Static_assert(offsetof(BtConnection, deviceAddress) == 24, "connection address");
_Static_assert(offsetof(BtConnection, aclHandle) == 48, "connection ACL handle");
_Static_assert(offsetof(BtConnection, l2capReceive) == 64, "connection L2CAP RX");
_Static_assert(offsetof(BtConnection, completions) == 2120, "connection completions");
_Static_assert(offsetof(BtConnection, completionCount) == 5192, "completion count");
_Static_assert(offsetof(BtConnection, currentServiceClass) == 5204,
	       "current SDP service class");
_Static_assert(offsetof(BtConnection, sdpTransactionId) == 5280, "SDP transaction");
_Static_assert(offsetof(BtConnection, device) == 5284, "connection device");
_Static_assert(offsetof(BtConnection, channels) == 5296, "connection channels");
_Static_assert(offsetof(BtConnection, avdtp) == 15152, "connection AVDTP");
_Static_assert(offsetof(BtConnection, hid) == 15320, "connection HID");
_Static_assert(offsetof(BtConnection, hsp) == 15872, "connection HSP");
_Static_assert(offsetof(BtConnection, connectedProfiles) == 15888,
	       "connection connected profiles");
_Static_assert(offsetof(BtConnection, linkQuality) == 15916, "link quality");
_Static_assert(offsetof(BtConnection, statistics) == 15936, "link statistics");
_Static_assert(offsetof(BtConnection, avrcpCommand) == 16008, "AVRCP command");
_Static_assert(offsetof(BtConnection, avrcpResponse) == 16268, "AVRCP response");
_Static_assert(offsetof(BtConnection, profileTimer) == 16528, "profile timer");
_Static_assert(offsetof(BtConnection, disconnectTimer) == 16532, "disconnect timer");
_Static_assert(offsetof(BtConnection, echoResponseBuffer) == 16536, "echo buffer");
_Static_assert(offsetof(BtConnection, serialReceive) == 16552, "SPP receive ring");
_Static_assert(offsetof(BtConnection, serialReceiveData) == 16572, "SPP receive data");
_Static_assert(offsetof(BtConnection, hidVuCommand) == 16828, "HIDVU command");
_Static_assert(offsetof(BtConnection, hidVuReportCapacity) == 16832,
	       "HIDVU report capacity");
_Static_assert(offsetof(BtConnection, linkState) == 16840, "connection link state");
_Static_assert(offsetof(BtConnection, packetType) == 16844, "connection packet type");

_Static_assert(sizeof(BtSignal) == 12, "BtSignal size");
_Static_assert(offsetof(BtSignal, events) == 0, "signal events offset");
_Static_assert(offsetof(BtSignal, waiters) == 4, "signal waiters offset");
_Static_assert(offsetof(BtSignal, eventFlag) == 8, "signal eventFlag offset");

typedef struct BtObexVariableHeader {
	uint8_t id;
	uint8_t lengthBe[2];
	uint8_t data[];
} BtObexVariableHeader;

typedef struct BtObexByteHeader {
	uint8_t id;
	uint8_t value;
} BtObexByteHeader;

typedef struct BtObexWordHeader {
	uint8_t id;
	uint8_t valueBe[4];
} BtObexWordHeader;

_Static_assert(sizeof(BtObexVariableHeader) == 3, "OBEX variable header");
_Static_assert(offsetof(BtObexVariableHeader, data) == 3, "OBEX header data");
_Static_assert(sizeof(BtObexByteHeader) == 2, "OBEX byte header");
_Static_assert(sizeof(BtObexWordHeader) == 5, "OBEX word header");
_Static_assert(offsetof(BtObexWordHeader, valueBe) == 1, "OBEX header word");

typedef struct BtAudioSendState {
	BtConnection *connection;
	SbcContext codec;
	int resetEncoder;
	uint64_t timestamp;
	uint32_t synchronizationSource;
	uint32_t sequence;
	uint8_t packet[1038];
	uint8_t contentProtection;
	uint8_t framesPerPacket;
	int startupDropPackets;
} BtAudioSendState;

typedef struct BtAudioReceiveState {
	BtConnection *connection;
	SbcContext codec;
	uint64_t concealmentUntil;
	uint64_t previousPacketTime;
	int arrivalIntervalAverage16;
	int receivedSamples;
	uint32_t sbcFrameHeader;
	int targetBufferedBytes;
	int queuedPcmBytes;
	int prebuffering;
	int skipBytes;
	int silenceBytes;
	int bufferedBytesAverage32;
	uint32_t skippedBytes;
	uint32_t concealedBytes;
	uint32_t startupSkippedBytes;
	uint32_t adjustmentSkippedBytes;
	uint32_t adjustmentSilenceBytes;
	uint32_t underruns;
	uint32_t overruns;
	BtRingBuffer ring;
	uint8_t ringData[6720];
	int16_t pcm[1024];
	int pendingPcmBytes;
	int pcmOffset;
} BtAudioReceiveState;

_Static_assert(sizeof(BtAudioSendState) == 6648, "audio send state");
_Static_assert(offsetof(BtAudioSendState, codec) == 4, "audio encoder");
_Static_assert(offsetof(BtAudioSendState, resetEncoder) == 5580, "audio encoder reset");
_Static_assert(offsetof(BtAudioSendState, timestamp) == 5584, "audio timestamp");
_Static_assert(offsetof(BtAudioSendState, synchronizationSource) == 5592, "audio RTP source");
_Static_assert(offsetof(BtAudioSendState, sequence) == 5596, "audio RTP sequence");
_Static_assert(offsetof(BtAudioSendState, packet) == 5600, "audio encoded packet");
_Static_assert(offsetof(BtAudioSendState, contentProtection) == 6638, "audio protection");
_Static_assert(offsetof(BtAudioSendState, framesPerPacket) == 6639, "audio frame count");
_Static_assert(offsetof(BtAudioSendState, startupDropPackets) == 6640, "audio startup drops");
_Static_assert(sizeof(BtAudioReceiveState) == 14464, "audio receive state");
_Static_assert(offsetof(BtAudioReceiveState, codec) == 4, "audio decoder");
_Static_assert(offsetof(BtAudioReceiveState, concealmentUntil) == 5584, "audio concealment deadline");
_Static_assert(offsetof(BtAudioReceiveState, previousPacketTime) == 5592, "audio previous packet time");
_Static_assert(offsetof(BtAudioReceiveState, arrivalIntervalAverage16) == 5600, "audio arrival interval");
_Static_assert(offsetof(BtAudioReceiveState, receivedSamples) == 5604, "audio received samples");
_Static_assert(offsetof(BtAudioReceiveState, sbcFrameHeader) == 5608, "audio frame header");
_Static_assert(offsetof(BtAudioReceiveState, targetBufferedBytes) == 5612, "audio target buffering");
_Static_assert(offsetof(BtAudioReceiveState, queuedPcmBytes) == 5616, "audio queued PCM");
_Static_assert(offsetof(BtAudioReceiveState, prebuffering) == 5620, "audio prebuffering");
_Static_assert(offsetof(BtAudioReceiveState, skipBytes) == 5624, "audio skip count");
_Static_assert(offsetof(BtAudioReceiveState, silenceBytes) == 5628, "audio silence count");
_Static_assert(offsetof(BtAudioReceiveState, bufferedBytesAverage32) == 5632, "audio buffered average");
_Static_assert(offsetof(BtAudioReceiveState, skippedBytes) == 5636, "audio skipped bytes");
_Static_assert(offsetof(BtAudioReceiveState, concealedBytes) == 5640, "audio concealed bytes");
_Static_assert(offsetof(BtAudioReceiveState, startupSkippedBytes) == 5644, "audio startup skip count");
_Static_assert(offsetof(BtAudioReceiveState, adjustmentSkippedBytes) == 5648, "audio adjustment skips");
_Static_assert(offsetof(BtAudioReceiveState, adjustmentSilenceBytes) == 5652, "audio adjustment silence");
_Static_assert(offsetof(BtAudioReceiveState, underruns) == 5656, "audio underruns");
_Static_assert(offsetof(BtAudioReceiveState, overruns) == 5660, "audio overruns");
_Static_assert(offsetof(BtAudioReceiveState, ring) == 5664, "audio receive ring");
_Static_assert(offsetof(BtAudioReceiveState, ringData) == 5684, "audio receive ring data");
_Static_assert(offsetof(BtAudioReceiveState, pcm) == 12404, "audio PCM buffer");
_Static_assert(offsetof(BtAudioReceiveState, pendingPcmBytes) == 14452, "audio pending PCM");
_Static_assert(offsetof(BtAudioReceiveState, pcmOffset) == 14456, "audio PCM offset");

extern uint8_t gBtSbcSilenceFrame[21];
extern BtAudioSendState gBtAudioSend;
extern BtAudioReceiveState gBtAudioReceive;

typedef struct BtImageThumbnail {
	int offset;
	int length;
	int width;
	int height;
} BtImageThumbnail;

typedef struct BtImageSendState {
	uint64_t deviceAddress;
	int state;
	SceUID file;
	int error;
	char name[128];
	uint8_t packet[1024];
	int responseLength;
	int receivedLength;
	int imageSize;
	int width;
	int height;
	BtImageThumbnail thumbnail;
	int useThumbnail;
	int mtu;
	uint32_t connectionId;
	uint8_t imageHandle[16];
	int packetOffset;
	int remainingImageBytes;
	int command;
} BtImageSendState;

typedef struct BtImageReceiveState {
	uint64_t deviceAddress;
	SceUID file;
	char path[128];
	char name[128];
	uint8_t packet[1024];
	int packetLength;
	int receivedLength;
	int mtu;
	uint32_t connectionId;
	int nextImageHandle;
	int capabilitiesRequested;
	int remainingImageBytes;
} BtImageReceiveState;

_Static_assert(sizeof(BtImageThumbnail) == 16, "image thumbnail");
_Static_assert(sizeof(BtImageSendState) == 1248, "image send state");
_Static_assert(offsetof(BtImageSendState, state) == 8, "image send state flag");
_Static_assert(offsetof(BtImageSendState, file) == 12, "image send file");
_Static_assert(offsetof(BtImageSendState, error) == 16, "image send error");
_Static_assert(offsetof(BtImageSendState, name) == 20, "image send name");
_Static_assert(offsetof(BtImageSendState, packet) == 148, "image send packet");
_Static_assert(offsetof(BtImageSendState, responseLength) == 1172, "image response length");
_Static_assert(offsetof(BtImageSendState, receivedLength) == 1176, "image received length");
_Static_assert(offsetof(BtImageSendState, imageSize) == 1180, "image size");
_Static_assert(offsetof(BtImageSendState, width) == 1184, "image width");
_Static_assert(offsetof(BtImageSendState, height) == 1188, "image height");
_Static_assert(offsetof(BtImageSendState, thumbnail) == 1192, "image thumbnail state");
_Static_assert(offsetof(BtImageSendState, useThumbnail) == 1208, "image thumbnail selection");
_Static_assert(offsetof(BtImageSendState, mtu) == 1212, "image send MTU");
_Static_assert(offsetof(BtImageSendState, connectionId) == 1216, "image send connection");
_Static_assert(offsetof(BtImageSendState, imageHandle) == 1220, "image handle");
_Static_assert(offsetof(BtImageSendState, packetOffset) == 1236, "image packet offset");
_Static_assert(offsetof(BtImageSendState, remainingImageBytes) == 1240, "image remaining bytes");
_Static_assert(offsetof(BtImageSendState, command) == 1244, "image command");
_Static_assert(sizeof(BtImageReceiveState) == 1320, "image receive state");
_Static_assert(offsetof(BtImageReceiveState, file) == 8, "image receive file");
_Static_assert(offsetof(BtImageReceiveState, path) == 12, "image receive path");
_Static_assert(offsetof(BtImageReceiveState, name) == 140, "image receive name");
_Static_assert(offsetof(BtImageReceiveState, packet) == 268, "image receive packet");
_Static_assert(offsetof(BtImageReceiveState, packetLength) == 1292, "image receive packet length");
_Static_assert(offsetof(BtImageReceiveState, receivedLength) == 1296, "image received count");
_Static_assert(offsetof(BtImageReceiveState, mtu) == 1300, "image receive MTU");
_Static_assert(offsetof(BtImageReceiveState, connectionId) == 1304, "image receive connection");
_Static_assert(offsetof(BtImageReceiveState, nextImageHandle) == 1308, "image next handle");
_Static_assert(offsetof(BtImageReceiveState, capabilitiesRequested) == 1312, "image capability request");
_Static_assert(offsetof(BtImageReceiveState, remainingImageBytes) == 1316, "image receive remaining");

extern BtImageSendState gBtImageSend;
extern BtImageReceiveState gBtImageReceive;
extern const uint8_t gBtImagingTargetUuid[16];

typedef struct BtDeviceRegistryEntry {
	uint8_t serializedDevice[768];
	uint8_t reserved[196];
} BtDeviceRegistryEntry;

typedef struct BtDeviceRegistryPair {
	BtDeviceRegistryEntry entries[2];
} BtDeviceRegistryPair;

_Static_assert(sizeof(BtDeviceRegistryEntry) == 964, "registry entry");
_Static_assert(offsetof(BtDeviceRegistryEntry, reserved) == 768,
	       "registry reserved bytes");
_Static_assert(sizeof(BtDeviceRegistryPair) == 1928, "registry device pair");

struct BtStack {
	int state;
	int previousState;
	uint32_t mode;
	int autoAcceptDeviceCount;
	uint64_t deviceAddress;
	uint64_t localFeatures;
	int aclPacketLength;
	int aclPacketCount;
	int scoPacketLength;
	int scoPacketCount;
	uint32_t localAclHandle;
	uint8_t controllerType;
	uint32_t firmwareVersion;
	int lastError;
	uint64_t inquiryAddress;
	uint8_t pageScanRepetitionMode;
	uint16_t clockOffset;
	int inquiryRssi;
	uint32_t inquiryDeviceClass;
	unsigned int nameRequestTimer;
	uint8_t commandData[1024];
	BtRingBuffer commandQueue;
	int commandLength;
	int commandBusy;
	uint8_t aclData[65536];
	BtRingBuffer aclQueue;
	int aclLength;
	int outstandingAclPackets;
	uint8_t *pendingAclPacket;
	uint8_t transactionId;
	uint16_t nextChannelId;
	BtConnection connections[7];
	union {
		BtL2capReceiveBuffer l2capReceive;
		BtSdpReceiveBuffer sdpReceive;
	};
	BtDevice devices[32];
	BtDevice *deviceHead;
	BtDevice *deviceTail;
	uint32_t registrationSequence;
	SceInt64 inquiryProgressTime;
	int deviceRegistryState;
	union {
		uint8_t scratch[4096];
		BtHidRawPacket hidRawPacket;
		BtHidAuthenticationBuffer hidAuthentication;
		BtDeviceRegistryPair deviceRegistry;
		BtHciScoPacket scoPacket;
	};
	BtTimer *freeTimers;
	BtTimer *activeTimerHead;
	BtTimer *activeTimerTail;
	BtTimer *pendingTimerHead;
	BtTimer *pendingTimerTail;
	BtTimer timers[64];
	BtSignal timerSignal;
	SceUID timerThreadId;
	int timersInitialized;
	uint64_t connectingAddress;
	uint32_t scanEnable;
	BtEventSubscriber subscribers[8];
	BtHidVuSubscriber hidVuSubscriber;
	unsigned int shutdownTimer;
	unsigned int resetTimer;
	unsigned int powerOffTimer;
	uint8_t contentProtection;
	SceUID inquiryProcessId;
	SceUID inquiryCompletionProcessId;
	uint8_t exclusiveControllerCount;
	uint8_t limitedConnectionCount;
	uint8_t audioConnectionCount;
	uint8_t genericHidConnectionCount;
	uint8_t suppressInquiryResult;
	uint16_t vendorConnectionSetting;
	SceUID procEventHandlerId;
};

_Static_assert(sizeof(BtStack) == 221936, "Bluetooth stack size");
_Static_assert(_Alignof(BtStack) == 8, "Bluetooth stack alignment");
_Static_assert(offsetof(BtStack, state) == 0, "stack state");
_Static_assert(offsetof(BtStack, previousState) == 4, "stack previous state");
_Static_assert(offsetof(BtStack, mode) == 8, "stack mode");
_Static_assert(offsetof(BtStack, autoAcceptDeviceCount) == 12, "auto-accept devices");
_Static_assert(offsetof(BtStack, deviceAddress) == 16, "local device address");
_Static_assert(offsetof(BtStack, localFeatures) == 24, "controller features");
_Static_assert(offsetof(BtStack, aclPacketLength) == 32, "controller ACL length");
_Static_assert(offsetof(BtStack, aclPacketCount) == 36, "controller ACL count");
_Static_assert(offsetof(BtStack, scoPacketLength) == 40, "controller SCO length");
_Static_assert(offsetof(BtStack, scoPacketCount) == 44, "controller SCO count");
_Static_assert(offsetof(BtStack, localAclHandle) == 48, "local ACL handle");
_Static_assert(offsetof(BtStack, controllerType) == 52, "controller type");
_Static_assert(offsetof(BtStack, firmwareVersion) == 56, "controller firmware");
_Static_assert(offsetof(BtStack, lastError) == 60, "stack last error");
_Static_assert(offsetof(BtStack, inquiryAddress) == 64, "inquiry device address");
_Static_assert(offsetof(BtStack, pageScanRepetitionMode) == 72, "page repetition");
_Static_assert(offsetof(BtStack, clockOffset) == 74, "inquiry clock offset");
_Static_assert(offsetof(BtStack, inquiryRssi) == 76, "inquiry RSSI");
_Static_assert(offsetof(BtStack, inquiryDeviceClass) == 80, "inquiry device class");
_Static_assert(offsetof(BtStack, nameRequestTimer) == 84, "name request timer");
_Static_assert(offsetof(BtStack, commandData) == 88, "HCI command storage");
_Static_assert(offsetof(BtStack, commandQueue) == 1112, "HCI command queue");
_Static_assert(offsetof(BtStack, commandLength) == 1132, "HCI command length");
_Static_assert(offsetof(BtStack, commandBusy) == 1136, "HCI command busy");
_Static_assert(offsetof(BtStack, aclData) == 1140, "ACL packet storage");
_Static_assert(offsetof(BtStack, aclQueue) == 66676, "ACL packet queue");
_Static_assert(offsetof(BtStack, aclLength) == 66696, "ACL packet length");
_Static_assert(offsetof(BtStack, outstandingAclPackets) == 66700, "ACL outstanding");
_Static_assert(offsetof(BtStack, pendingAclPacket) == 66704, "ACL pending packet");
_Static_assert(offsetof(BtStack, transactionId) == 66708, "L2CAP transaction");
_Static_assert(offsetof(BtStack, nextChannelId) == 66710, "next L2CAP channel");
_Static_assert(offsetof(BtStack, connections) == 66712, "stack connections");
_Static_assert(offsetof(BtStack, l2capReceive) == 184648, "stack L2CAP receive");
_Static_assert(offsetof(BtStack, devices) == 186704, "stack device catalog");
_Static_assert(offsetof(BtStack, deviceHead) == 212048, "registered device head");
_Static_assert(offsetof(BtStack, deviceTail) == 212052, "registered device tail");
_Static_assert(offsetof(BtStack, registrationSequence) == 212056, "registration sequence");
_Static_assert(offsetof(BtStack, inquiryProgressTime) == 212064, "inquiry progress time");
_Static_assert(offsetof(BtStack, deviceRegistryState) == 212072, "registry state");
_Static_assert(offsetof(BtStack, sdpReceive) == 184648, "SDP accumulation storage");
_Static_assert(offsetof(BtStack, deviceRegistry) == 212076, "registry pair storage");
_Static_assert(offsetof(BtStack, scratch) == 212076, "stack scratch storage");
_Static_assert(offsetof(BtStack, scoPacket) == 212076, "SCO packet storage");
_Static_assert(offsetof(BtStack, hidAuthentication) == 212076, "stack HID authentication storage");
_Static_assert(offsetof(BtStack, freeTimers) == 216172, "free timer head");
_Static_assert(offsetof(BtStack, activeTimerHead) == 216176, "active timer head");
_Static_assert(offsetof(BtStack, activeTimerTail) == 216180, "active timer tail");
_Static_assert(offsetof(BtStack, pendingTimerHead) == 216184, "pending timer head");
_Static_assert(offsetof(BtStack, pendingTimerTail) == 216188, "pending timer tail");
_Static_assert(offsetof(BtStack, timers) == 216192, "timer pool");
_Static_assert(offsetof(BtStack, timerSignal) == 218240, "timer signal");
_Static_assert(offsetof(BtStack, timerThreadId) == 218252, "timer thread");
_Static_assert(offsetof(BtStack, timersInitialized) == 218256, "timer initialization");
_Static_assert(offsetof(BtStack, connectingAddress) == 218264, "connecting address");
_Static_assert(offsetof(BtStack, scanEnable) == 218272, "scan enable");
_Static_assert(offsetof(BtStack, subscribers) == 218280, "event subscribers");
_Static_assert(offsetof(BtStack, hidVuSubscriber) == 220712, "HIDVU subscriber");
_Static_assert(offsetof(BtStack, shutdownTimer) == 221896, "shutdown timer");
_Static_assert(offsetof(BtStack, resetTimer) == 221900, "controller reset timer");
_Static_assert(offsetof(BtStack, powerOffTimer) == 221904, "power-off timer");
_Static_assert(offsetof(BtStack, contentProtection) == 221908, "audio protection");
_Static_assert(offsetof(BtStack, inquiryProcessId) == 221912, "inquiry process");
_Static_assert(offsetof(BtStack, inquiryCompletionProcessId) == 221916, "inquiry completion process");
_Static_assert(offsetof(BtStack, exclusiveControllerCount) == 221920, "exclusive controllers");
_Static_assert(offsetof(BtStack, limitedConnectionCount) == 221921, "limited connections");
_Static_assert(offsetof(BtStack, audioConnectionCount) == 221922, "audio connections");
_Static_assert(offsetof(BtStack, genericHidConnectionCount) == 221923, "generic HID connections");
_Static_assert(offsetof(BtStack, suppressInquiryResult) == 221924, "suppress inquiry results");
_Static_assert(offsetof(BtStack, vendorConnectionSetting) == 221926, "vendor connection setting");
_Static_assert(offsetof(BtStack, procEventHandlerId) == 221928, "process event handler");

extern BtStack gBtStack;
extern const char gBtEmptyHciFormat[1];
extern const char gBtControllerEmptyFormat[1];
extern const char gBtQueueInitializationFormat[1];
extern const uint32_t gBtHidAuthenticationPrefix[8];
extern const char gBtDeviceAddressHciFormat[2];

typedef union BtRegisteredOutput {
	SceBtRegisteredInfo info;
	uint64_t deviceAddress;
} BtRegisteredOutput;

_Static_assert(sizeof(BtRegisteredOutput) == 256, "registered output size");
_Static_assert(offsetof(BtRegisteredOutput, info) == 0, "registered output record");
_Static_assert(offsetof(BtRegisteredOutput, deviceAddress) == 0,
	       "registered output address");

int BtCopyIn(const void *source, void *destination, unsigned int length,
	     int userMode);
int BtCopyOut(const void *source, void *destination, unsigned int length,
	      int userMode);
int BtCopyOutProcess(const void *source, void *destination, unsigned int length,
		     SceUID processId);
SceInt64 BtGetSystemTimeWide(void);
int BtInitializeServices(const char *logName, const char *timerThreadName);
void BtReceiveHciEvent(BtStack *stack, uint8_t *packet, int length);
void BtReceiveAclPacket(BtStack *stack, uint8_t *packet, int length);
void BtReceiveScoPacket(BtStack *stack, uint8_t *packet, int length);
int BtExitThread(void);
int BtDelayThread(SceUInt32 microseconds);

void BtRingReset(BtRingBuffer *buffer);
int BtRingWrite(BtRingBuffer *buffer, const uint8_t *source, int length,
		int userMode);
int BtRingRead(BtRingBuffer *buffer, uint8_t *destination, int length,
	       int userMode);
void BtRingRewind(BtRingBuffer *buffer, int length);
int BtRingConsume(BtRingBuffer *buffer, int length);

int BtUpdateUtf8String(char *destination, const uint8_t *source, int remaining);
int BtUtf8ToUtf16Be(uint8_t *destination, const uint8_t *source, int capacity);
uint8_t *BtUtf16BeToUtf8(uint8_t *destination, const uint8_t *source, int capacity);
const char *BtSkipSpaces(const char *cursor);
const char *BtParseDecimalList(const char *cursor, int32_t *values, int count);
int BtGenerateRandomBlock(uint8_t *destination);
uint32_t BtUpdateCrc32(uint32_t checksum, const uint8_t *source, int length);
uint32_t BtSha1Compress(uint32_t block[16], uint32_t state[5]);
uint32_t BtSha1Digest(const uint8_t *source, int length, uint8_t *digest);

uintptr_t BtClearConnectedProfiles(BtConnection *connection,
				   BtChannel *channel, unsigned int profiles);
uintptr_t BtAvrcpOpened(BtConnection *connection, BtChannel *channel);
void BtAvrcpClosed(BtConnection *connection, BtChannel *channel);
void BtReceiveAvrcp(BtConnection *connection, BtChannel *channel,
			uint8_t *packet, int length);
uintptr_t BtRequestAvrcpDisconnect(BtConnection *connection, BtChannel *channel);
extern const BtChannelOps gBtAvrcpOps;
void BtAvdtpClosed(BtConnection *connection, BtChannel *channel);
uintptr_t BtAvdtpSignalingOpened(BtConnection *connection, BtChannel *channel);
uintptr_t BtAvdtpTransmitOpened(BtConnection *connection, BtChannel *channel);
uintptr_t BtAvdtpReceiveOpened(BtConnection *connection, BtChannel *channel);
uintptr_t BtRequestAvdtpTransmitDisconnect(BtConnection *connection,
					 BtChannel *channel);
uintptr_t BtRequestAvdtpReceiveDisconnect(BtConnection *connection,
					BtChannel *channel);
void BtReceiveAvdtpSignaling(BtConnection *connection, BtChannel *channel,
				uint8_t *packet, int length);
extern const BtChannelOps gBtAvdtpReceiveOps;
extern const BtChannelOps gBtAvdtpTransmitOps;
extern const BtChannelOps gBtAvdtpSignalingOps;

uintptr_t BtOpenAvrcpBrowsing(BtConnection *connection, BtChannel *channel);
void BtReceiveAvrcpBrowsing(BtConnection *connection, BtChannel *channel,
				uint8_t *packet, int length);
extern const BtChannelOps gBtAvrcpBrowsingOps;

uintptr_t BtHidControlOpened(BtConnection *connection, BtChannel *channel);
uintptr_t BtHidInterruptOpened(BtConnection *connection, BtChannel *channel);
void BtHidInterruptClosed(BtConnection *connection, BtChannel *channel);
void BtReceiveHidInterrupt(BtConnection *connection, BtChannel *channel,
			      uint8_t *packet, int length);
void BtReceiveHidControl(BtConnection *connection, BtChannel *channel,
			    uint8_t *packet, int length);
extern const BtChannelOps gBtHidInterruptOps;
extern const BtChannelOps gBtHidControlOps;

uintptr_t BtBnepOpened(BtConnection *connection, BtChannel *channel);
void BtBnepClosed(BtConnection *connection, BtChannel *channel);
void BtReceiveBnep(BtConnection *connection, BtChannel *channel,
		      uint8_t *packet, int length);
extern const BtChannelOps gBtBnepOps;

uintptr_t BtRfcommOpened(BtConnection *connection, BtChannel *channel);
void BtRfcommClosed(BtConnection *connection, BtChannel *channel);
void BtReceiveRfcomm(BtConnection *connection, BtChannel *channel,
			 uint8_t *packet, int length);
uintptr_t BtRequestRfcommDisconnect(BtConnection *connection,
				  BtChannel *channel);
extern const BtChannelOps gBtRfcommOps;

uintptr_t BtSdpOpened(BtConnection *connection, BtChannel *channel);
void BtSdpClosed(BtConnection *connection, BtChannel *channel);
void BtReceiveSdp(BtConnection *connection, BtChannel *channel,
		  uint8_t *packet, int length);
extern const BtChannelOps gBtSdpOps;




/* GetFolderItems response with one media player, in network byte order. */
typedef struct BtAvrcpPlayerListResponse {
	uint8_t parameterLengthBe[2];
	uint8_t status;
	uint8_t uidCounterBe[2];
	uint8_t itemCountBe[2];
	uint8_t itemType;
	uint8_t itemLengthBe[2];
	uint8_t playerIdBe[2];
	uint8_t majorType;
	uint8_t subTypeBe[4];
	uint8_t playStatus;
	uint8_t features[16];
	uint8_t characterSetBe[2];
	uint8_t nameLengthBe[2];
	char name[6];
} BtAvrcpPlayerListResponse;

_Static_assert(sizeof(BtAvrcpPlayerListResponse) == 44,
	       "AVRCP player list response size");
_Static_assert(offsetof(BtAvrcpPlayerListResponse, features) == 18,
	       "AVRCP player features offset");
_Static_assert(offsetof(BtAvrcpPlayerListResponse, name) == 38,
	       "AVRCP player name offset");
extern const BtAvrcpPlayerListResponse gBtAvrcpPlayerListResponse;

#endif
