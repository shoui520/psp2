/* Complete module-private Bluetooth SDIO transport owners. */
#ifndef BT_TRANSPORT_H
#define BT_TRANSPORT_H

#include <sce/sdif.h>
#include "bt_internal.h"

typedef struct BtSdioPacket {
	uint8_t length[3];
	uint8_t packetType;
	uint8_t payload[2044];
} BtSdioPacket;

typedef union BtSdioPacketBuffer {
	BtSdioPacket packet;
	uint8_t bytes[2048];
} BtSdioPacketBuffer;

typedef struct BtTxRequest {
	struct BtTxRequest *next;
	struct BtTxRequest *prev;
	unsigned int packetType;
	const uint8_t *data;
	int length;
	void (*completion)(void *argument);
	void *argument;
} BtTxRequest;

typedef struct BtTransport {
	SdifSdioContext *sdio;
	SdifSdioFunction *function;
	int subInterrupt;
	BtSignal interruptSignal;
	SceUID interruptThread;
	uint32_t ioPort;
	uint8_t receiveUnitShift;
	uint8_t cardRevision;
	uint8_t asleep;
	uint8_t wakeupPending;
	uint8_t interruptEnabled;
	BtSignal lockSignal;
	SceUID lockOwner;
	unsigned int lockDepth;
	unsigned int transmitActive;
	BtTxRequest requests[32];
	BtTxRequest *transmitHead;
	BtTxRequest *transmitTail;
	BtTxRequest *freeRequests;
	BtStack *stack;
	unsigned int wakeupTimer;
} BtTransport;

_Static_assert(sizeof(BtSdioPacket) == 2048, "BtSdioPacket size");
_Static_assert(offsetof(BtSdioPacket, length) == 0, "packet length offset");
_Static_assert(offsetof(BtSdioPacket, packetType) == 3, "packet type offset");
_Static_assert(offsetof(BtSdioPacket, payload) == 4, "packet payload offset");
_Static_assert(sizeof(BtSdioPacketBuffer) == 2048, "BtSdioPacketBuffer size");
_Static_assert(offsetof(BtSdioPacketBuffer, packet) == 0, "packet view offset");
_Static_assert(offsetof(BtSdioPacketBuffer, bytes) == 0, "byte view offset");
_Static_assert(sizeof(BtTxRequest) == 28, "BtTxRequest size");
_Static_assert(offsetof(BtTxRequest, next) == 0, "request next offset");
_Static_assert(offsetof(BtTxRequest, prev) == 4, "request prev offset");
_Static_assert(offsetof(BtTxRequest, packetType) == 8, "request type offset");
_Static_assert(offsetof(BtTxRequest, data) == 12, "request data offset");
_Static_assert(offsetof(BtTxRequest, length) == 16, "request length offset");
_Static_assert(offsetof(BtTxRequest, completion) == 20, "request completion offset");
_Static_assert(offsetof(BtTxRequest, argument) == 24, "request argument offset");
_Static_assert(sizeof(BtTransport) == 980, "BtTransport size");
_Static_assert(offsetof(BtTransport, sdio) == 0, "SDIO context offset");
_Static_assert(offsetof(BtTransport, function) == 4, "SDIO function offset");
_Static_assert(offsetof(BtTransport, subInterrupt) == 8, "subinterrupt offset");
_Static_assert(offsetof(BtTransport, interruptSignal) == 12, "interrupt signal offset");
_Static_assert(offsetof(BtTransport, interruptThread) == 24, "interrupt thread offset");
_Static_assert(offsetof(BtTransport, ioPort) == 28, "I/O port offset");
_Static_assert(offsetof(BtTransport, receiveUnitShift) == 32, "receive unit offset");
_Static_assert(offsetof(BtTransport, cardRevision) == 33, "card revision offset");
_Static_assert(offsetof(BtTransport, asleep) == 34, "sleep state offset");
_Static_assert(offsetof(BtTransport, wakeupPending) == 35, "wakeup state offset");
_Static_assert(offsetof(BtTransport, interruptEnabled) == 36, "interrupt state offset");
_Static_assert(offsetof(BtTransport, lockSignal) == 40, "lock signal offset");
_Static_assert(offsetof(BtTransport, lockOwner) == 52, "lock owner offset");
_Static_assert(offsetof(BtTransport, lockDepth) == 56, "lock depth offset");
_Static_assert(offsetof(BtTransport, transmitActive) == 60, "transmit state offset");
_Static_assert(offsetof(BtTransport, requests) == 64, "request pool offset");
_Static_assert(offsetof(BtTransport, transmitHead) == 960, "transmit head offset");
_Static_assert(offsetof(BtTransport, transmitTail) == 964, "transmit tail offset");
_Static_assert(offsetof(BtTransport, freeRequests) == 968, "free requests offset");
_Static_assert(offsetof(BtTransport, stack) == 972, "stack offset");
_Static_assert(offsetof(BtTransport, wakeupTimer) == 976, "wakeup timer offset");

extern BtSdioPacketBuffer gBtTransmitPacket;
extern BtTransport gBtTransport;
extern BtSdioPacketBuffer gBtReceivePacket;

#endif
