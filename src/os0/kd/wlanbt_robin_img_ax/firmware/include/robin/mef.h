/*
 * Packet-filter expressions the host installs for host sleep (the Marvell
 * "MEF", as in mwifiex' mef_entry) and the offload helpers that answer ARP
 * requests and ping while the host sleeps. A filter is a postfix expression:
 * tokens are pushed on a byte stack and an operator pops its operands.
 */
#ifndef ROBIN_MEF_H
#define ROBIN_MEF_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include <sce/wlanbt_protocol.h>

/* The stack: bytes are pushed after the length. */
struct WlanMefStack {
	uint16_t length;
	uint8_t data[];
};

/* The frame being matched. */
struct WlanMefPacket {
	uint16_t length;
	uint16_t reserved;
	const uint8_t *data;
};

/* An operand or an operator; the number or the sequence follows the type. */
struct WlanMefToken {
	uint8_t type;		/* 1: number, 2: byte sequence, above 0x40: operator */
	uint8_t reserved[3];
	union {
		uint32_t number;
		struct {
			uint8_t length;
			uint8_t bytes[7];
		} sequence;
	};
};

enum {
	WLAN_MEF_NUMBER = 1,
	WLAN_MEF_SEQUENCE = 2,
	WLAN_MEF_OPERATOR_MIN = 0x41,
	WLAN_MEF_COMPARE_SEQUENCE = 'A',	/* offset, sequence, repeat count */
	WLAN_MEF_COMPARE_NUMBER = 'B',		/* size, offset, expected value */
	WLAN_MEF_COMPARE_MASKED = 'C',		/* mask, offset, expected bytes */
	WLAN_MEF_AND = 'D',
	WLAN_MEF_OR = 'E',
};

/* 802.2 SNAP header followed by an ARP packet for IPv4 over Ethernet. */
struct WlanSnapArp {
	uint8_t snap[8];
	uint8_t hardwareType[2];
	uint8_t protocolType[2];
	uint8_t hardwareSize;
	uint8_t protocolSize;
	uint8_t operation[2];
	uint8_t senderHardware[6];
	uint8_t senderProtocol[4];
	uint8_t targetHardware[6];
	uint8_t targetProtocol[4];
};

_Static_assert(sizeof(struct WlanSnapArp) == 36, "SNAP ARP");

/* One configured filter: the modes it applies in, flags, and its expression (stack). */
struct WlanMefEntry {
	uint8_t modes;
	uint8_t flags;
	struct WlanMefStack stack;	/* 2 length bytes and up to 100 of data */
	uint8_t data[100];
};

_Static_assert(sizeof(struct WlanMefEntry) == 104, "MEF entry");

#define WLAN_MEF_ENTRIES	3
#define WLAN_MEF_WORKING	2	/* the entry evaluated in place */

/* An address rule of the host-sleep wake filter. */
struct WlanMefAddressRule {
	uint16_t addressType;	/* 0 ends the table, 0xffff any */
	uint16_t protocol;	/* ethertype, 0xffff any */
	uint32_t target;	/* IPv4 address an ARP must ask for, 0xffffffff any */
};

struct WlanMefConfig {
	uint8_t header[20];
	struct WlanMefAddressRule rules[8];
};

_Static_assert(sizeof(struct WlanMefConfig) == 84, "MEF configuration");

#define WLAN_TLV_MEF_ADDRESS_RULES	0x0115

extern struct WlanMefEntry wlanMefEntries[WLAN_MEF_ENTRIES];
extern struct WlanMefConfig wlanMefConfig;
extern uint32_t wlanWakeClassMask;
int wlanMefMatchFrame(OsaBuffer *buffer);
void wlanMefConfigClear(void);
void wlanMefConfigParse(const uint8_t *tlvs, int length);
int wlanMefAcceptFrame(OsaBuffer *buffer, uint8_t *securityDrop);
#define WLAN_MEF_FLAG_WAKE	0x01	/* the host sees the frame too */
#define WLAN_MEF_FLAG_KEEP	0x02	/* the frame is not discarded */
#define WLAN_MEF_ACTION_SHIFT	4
enum {
	WLAN_MEF_ACTION_ARP_REPLY = 1,
	WLAN_MEF_ACTION_ECHO_REPLY = 2,
};
#define WLAN_MEF_MODE_PATTERN	4

/* A frame the offload sends on its own (one buffer, header at dataOffset). */
struct WlanReplyFrame {
	WlanTxDescriptor descriptor;
	uint8_t reserved16[80];
	uint8_t *frame;			/* points at frameLength */
	uint8_t reserved100[8];
	uint8_t queue;
	uint8_t kind;
	uint8_t reserved110[6];
	uint8_t frameLength[2];
	uint8_t header[30];		/* frame control, duration, three addresses */
	uint8_t body[];
};

#define WLAN_REPLY_FRAME_BODY	148	/* offset of body from the start of the data */

extern struct OsaBufferPool wlanReplyFramePool;
extern struct WlanConnection *wlanMefConnection;
extern uint32_t wlanMefEntryCount;
OsaBuffer *wlanReplyFrameAlloc(struct WlanConnection *connection);
uint8_t *wlanReplyFramePayload(OsaBuffer *buffer);
int wlanReplyFrameSend(OsaBuffer *buffer, const uint8_t *destination, int subtype, int length);
OsaBuffer *wlanReplyFrameFree(OsaBuffer *buffer);
int wlanMefProcessFrame(OsaBuffer *buffer, uint32_t *wake, uint32_t mode);
int wlanMefReplyPattern(struct WlanConnection *connection);
int wlanMefDisable(void);
unsigned int wlanTidToAccessCategory(int tid);
unsigned int wlanAccessCategoryToQueue(unsigned int category);
extern uint16_t wlanMefStackLimit;
extern uint16_t wlanMefDepthLimit;
extern uint8_t wlanMefDepth;
extern uint32_t wlanOffloadHeadroom;

int wlanMefStackPush(struct WlanMefStack *stack, unsigned int count, const void *source);
int wlanMefStackPop(struct WlanMefStack *stack, unsigned int count, void *destination);
int wlanMefTokenPush(struct WlanMefStack *stack, const struct WlanMefToken *token);
int wlanMefTokenPop(struct WlanMefStack *stack, struct WlanMefToken *token);
int wlanMefIsOperator(const struct WlanMefToken *token);
int wlanMefCompareSequence(const struct WlanMefPacket *packet, struct WlanMefToken *offset,
			   const struct WlanMefToken *sequence, const struct WlanMefToken *repeat);
int wlanMefCompareNumber(const struct WlanMefPacket *packet, struct WlanMefToken *size,
			 const struct WlanMefToken *offset, const struct WlanMefToken *expected);
int wlanMefCompareMasked(const struct WlanMefPacket *packet, struct WlanMefToken *mask,
			 const struct WlanMefToken *offset, const struct WlanMefToken *expected);
void wlanMefAnd(struct WlanMefToken *first, const struct WlanMefToken *second);
void wlanMefOr(struct WlanMefToken *first, const struct WlanMefToken *second);
int wlanMefEvaluate(struct WlanMefStack *stack, const struct WlanMefPacket *packet);
int wlanMefResetDepth(void);
int wlanMefInit(void);
unsigned int wlanInternetChecksum(const uint16_t *data, unsigned int length);
void wlanFrameGetEthernetAddresses(void *destination, void *source, const uint8_t *frame);
int wlanBuildEthernetHeader(OsaBuffer *buffer, unsigned int classMask, struct WlanMefPacket *packet);
void wlanMefLoad(void *destination, const uint8_t *source);
int wlanMefClassEnabled(uint32_t mask);
extern uint32_t wlanMefClassMask;
unsigned int wlanAddressClass(const uint16_t *address);
struct WlanSnapArp *wlanBuildArpReply(OsaBuffer *buffer, struct WlanSnapArp *reply,
				      const uint8_t *localAddress);
int wlanBuildEchoReply(OsaBuffer *buffer, uint8_t *reply, uint16_t *replyLength, unsigned int maxLength);

/* Typed parameter slots with a validity bit per index. */
int wlanParam8Set(uint8_t *slot, unsigned int index, const uint8_t *value);
int wlanParam8Get(const uint8_t *slot, unsigned int index, uint8_t *value);
int wlanParam16Set(uint8_t *slot, unsigned int index, const uint16_t *value);
int wlanParam16Get(const uint8_t *slot, unsigned int index, uint16_t *value);
int wlanParam32Set(uint8_t *slot, unsigned int index, const uint32_t *value);
int wlanParam32Get(const uint8_t *slot, unsigned int index, uint32_t *value);
int wlanParam64Set(uint8_t *slot, unsigned int index, const void *value);
int wlanParam64Get(const uint8_t *slot, unsigned int index, void *value);

#endif /* ROBIN_MEF_H */
