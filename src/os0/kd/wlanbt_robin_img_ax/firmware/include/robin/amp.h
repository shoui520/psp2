/*
 * Bluetooth 3.0+HS AMP (802.11 PAL) controller as implemented in RAM: the
 * HCI command layer, the physical and logical link tables, and the events
 * sent to the host. Role names follow the Bluetooth Core Specification
 * (HCI commands, AMP_ASSOC, Command Complete); the source file names are
 * inferred, none survives in the image.
 */
#ifndef ROBIN_AMP_H
#define ROBIN_AMP_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include <sce/wlanbt_protocol.h>

#define AMP_PHYSICAL_LINKS	2
#define AMP_LOGICAL_LINKS	2
#define AMP_LINK_FREE		0xff
#define AMP_LOGICAL_FREE	0xfff

/* HCI error codes used by the command layer. */
#define AMP_HCI_SUCCESS			0x00
#define AMP_HCI_UNKNOWN_COMMAND		0x01
#define AMP_HCI_UNKNOWN_CONNECTION	0x02
#define AMP_HCI_MEMORY_CAPACITY		0x07
#define AMP_HCI_CONNECTION_LIMIT	0x09
#define AMP_HCI_COMMAND_DISALLOWED	0x0c
#define AMP_HCI_LIMITED_RESOURCES	0x0d
#define AMP_HCI_INVALID_PARAMETERS	0x12
#define AMP_HCI_QOS_UNACCEPTABLE	0x2c
#define AMP_HCI_QOS_REJECTED		0x2d
#define AMP_HCI_NO_USABLE_CHANNEL	0x39

/* HCI event codes. */
#define AMP_EVENT_COMMAND_COMPLETE	0x0e
#define AMP_EVENT_COMMAND_STATUS	0x0f

/* A Bluetooth AMP flow specification (16 bytes). */
struct AmpFlowSpec {
	uint8_t id;
	uint8_t serviceType;
	uint16_t maxSdu;
	uint32_t sduInterArrivalTime;
	uint32_t accessLatency;
	uint32_t flushTimeout;
};
_Static_assert(sizeof(struct AmpFlowSpec) == 16, "AMP flow spec");

/* One logical link of a physical link (44 bytes). */
struct AmpLogicalLink {
	uint16_t handle;		/* AMP_LOGICAL_FREE when unused */
	uint8_t origin;			/* 0 created locally, 1 accepted */
	uint8_t pending;
	uint16_t failedContactCounter;
	uint8_t reserved[6];
	struct AmpFlowSpec txFlowSpec;
	struct AmpFlowSpec rxFlowSpec;
};
_Static_assert(sizeof(struct AmpLogicalLink) == 44, "AMP logical link record");

/* A physical link and its AMP_ASSOC (260 bytes). */
struct AmpPhysicalLink {
	uint8_t state;
	uint8_t handle;			/* AMP_LINK_FREE when unused */
	uint8_t channel;
	uint8_t shortRangeMode;
	uint16_t keyLength;
	uint8_t keyType;
	uint8_t logicalLinkCount;
	uint8_t key[32];
	struct AmpLogicalLink logicalLinks[AMP_LOGICAL_LINKS];
	uint8_t peerAddress[6];
	uint16_t connectedChannelsLength;
	uint8_t connectedChannels[10];
	uint16_t preferredChannelsLength;
	uint8_t preferredChannels[64];
	uint16_t palCapabilitiesLength;
	uint8_t palCapabilities[4];
	uint16_t palVersionLength;
	uint8_t palVersion[5];
	uint8_t reserved;
	uint16_t vendorLength;
	uint8_t vendor[32];
};
_Static_assert(sizeof(struct AmpPhysicalLink) == 260, "AMP physical link record");
_Static_assert(offsetof(struct AmpPhysicalLink, logicalLinks) == 40, "AMP logical links");
_Static_assert(offsetof(struct AmpPhysicalLink, peerAddress) == 128, "AMP peer address");
_Static_assert(offsetof(struct AmpPhysicalLink, vendor) == 228, "AMP vendor TLV");

/* The AMP_ASSOC the controller advertises for itself (29 bytes). */
struct AmpLocalAssoc {
	uint8_t macAddress[6];
	uint16_t connectedChannelsLength;
	uint8_t connectedChannels[9];
	uint8_t reserved;
	uint16_t preferredChannelsLength;
	uint8_t preferredChannels[9];
};
_Static_assert(sizeof(struct AmpLocalAssoc) == 30, "local AMP_ASSOC (29 bytes plus tail padding)");
_Static_assert(offsetof(struct AmpLocalAssoc, preferredChannels) == 20, "preferred channel list");

/* ROM message queue object of the command, event and data paths. */
struct AmpQueue {
	uint32_t words[5];
};

/* State machine object of one physical link (80 bytes), driven through the ROM. */
struct AmpLinkMachine {
	uint32_t words[19];
	uint8_t state;
	uint8_t reserved[3];
};
_Static_assert(sizeof(struct AmpLinkMachine) == 80, "AMP link state machine");

/* Event record handed to a link state machine (12 bytes). */
struct AmpLinkEvent {
	uint16_t opcode;
	uint8_t link;
	uint8_t state;
	uint32_t word1;
	uint32_t word2;
};
_Static_assert(sizeof(struct AmpLinkEvent) == 12, "AMP link event record");

extern struct AmpPhysicalLink ampPhysicalLinks[AMP_PHYSICAL_LINKS];
extern struct AmpLinkMachine ampLinkMachines[AMP_PHYSICAL_LINKS];
extern struct AmpLocalAssoc ampLocalAssoc;
extern int8_t ampHciCommandCount;
extern void *ampSdioDev;
extern void *ampHciDev;
extern void *ampPalDevice;
extern struct OsaBufferPool ampHciPool;

/* PAL protocol identifiers carried after the SNAP prefix. */
enum {
	AMP_PROTOCOL_L2CAP = 1,
	AMP_PROTOCOL_ACTIVITY_REPORT = 2,
	AMP_PROTOCOL_SECURITY = 3,
	AMP_PROTOCOL_LINK_SUPERVISION_REQUEST = 4,
	AMP_PROTOCOL_LINK_SUPERVISION_REPLY = 5,
};

/* amp/pal.c and amp/wlanif.c */
int ampPalNotify(int value, int code);
int ampPalNotifyCode10(int first, int second);
int ampPalNotifyCode11(int first, int second);
int ampPalNotifyCode12(int first, int second);
int ampPalTransmit(OsaBuffer *buffer, uint32_t length, int priority, int kind);
OsaBuffer *ampPalSendEmptyFrame(int kind, struct WlanConnection *context);
int ampPalLinkAlive(struct WlanConnection *context);
int ampPalDataToHost(OsaBuffer *buffer, unsigned int physicalIndex);
int ampPalDropPacket(OsaBuffer *buffer);
int ampPalDataLoop(void);
char *ampPalFormatSsid(char *out, const uint8_t *address);
int ampPalPrepareTxHeader(struct WlanConnection *context);
int ampPalSendActivityReport(struct WlanConnection *context);
int ampPalSetSupervisionInterval(void);
void ampPalWindowOpen(void);
void ampPalWindowClose(void);

/* amp/hci.c */
int ampHciAdjustCommandCount(int8_t delta);
int ampHciIsIdle(void);
int ampHciEventEnabled(const uint8_t *mask, uint32_t selector);
OsaBuffer *ampHciAllocBuffer(struct OsaBufferPool *pool, uint32_t size, uint32_t flags);
int ampHciSendEvent(uint8_t eventCode, uint32_t maskSelector, const char *format, ...);
int ampHciSendHostEvent(uint16_t id, int unused, const char *format, ...);
int ampHciRejectUnknownCommand(uint16_t opcode);
int ampHciDisallowCommand(uint16_t opcode, int status, int finished);
int ampHciSendCommandComplete(uint16_t opcode, int status, int finished);
int ampHciSendCommandStatus(uint16_t opcode, int status);
int ampHciSendNopCommandStatus(void);
int ampHciProcessCommands(void);
void ampPalNotifyDisconnect(unsigned int index);

extern uint8_t ampFlowControlMode;
extern uint8_t ampBufferCount;
extern uint8_t ampDisconnectReason;

unsigned int ampPhysicalLinkAllocate(uint32_t handle);
unsigned int ampPhysicalLinkFind(uint32_t handle);
unsigned int ampPhysicalLinkFindByLogical(uint32_t logicalHandle);
int ampPhysicalLinkReset(unsigned int index);
unsigned int ampLogicalLinkAllocate(uint32_t physicalHandle);
unsigned int ampLogicalLinkFind(uint32_t logicalHandle);
unsigned int ampLogicalLinkFindByFlowSpec(uint32_t flowSpecId);
int ampLogicalLinkServiceType(uint32_t logicalHandle);
int ampLogicalLinkRemove(int logicalHandle, int reason);
int ampLinkPostEvent(unsigned int index, uint32_t event, const void *record);
void ampPhysicalLinksInit(void);
int ampHciResetState(void);
unsigned int ampAssocBuild(uint8_t *out, unsigned int physicalIndex);
int ampAssocParse(const uint8_t *data, unsigned int total, int physicalIndex);
unsigned int ampAssocSelectChannel(unsigned int physicalIndex);
int ampAssocResetConnectedChannels(struct AmpLocalAssoc *assoc);

int ampHciLogicalLinkComplete(int status, int logicalHandle, int physicalHandle, int flowSpecId);
int ampHciDisconnectionLogicalLinkComplete(int status, int logicalHandle, int reason);
int ampHciFlowSpecModifyComplete(int status, int logicalHandle);
int ampHciNumberOfCompletedDataBlocks(int logicalHandle, int completedPackets, int completedBlocks);
int ampHciNumberOfCompletedPackets(int logicalHandle, int completedPackets);
int ampHciFlushOccurred(uint16_t opcode, OsaBuffer *command);
int ampHciEnhancedFlushComplete(uint16_t opcode, OsaBuffer *command);

char ampHexDigit(uint32_t nibble);

/* Link controller (bt/lmp, not converted). */
void btLmpLcAcquireContext(int a1);

#endif /* ROBIN_AMP_H */
