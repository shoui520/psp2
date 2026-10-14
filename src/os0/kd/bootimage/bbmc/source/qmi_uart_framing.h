#ifndef BBMC_QMI_UART_FRAMING_H
#define BBMC_QMI_UART_FRAMING_H

#include <stddef.h>
#include <psp2/types.h>

typedef struct QmiUartDataHeader {
	SceUInt8 startMarkerHigh;
	SceUInt8 startMarkerLow;
	SceUInt8 lengthHigh;
	SceUInt8 lengthLow;
	SceUInt8 sequence;
	SceUInt8 fragmentIndex;
	SceUInt8 fragmentCount;
} QmiUartDataHeader;

typedef struct QmiUartEncodedFrame {
	QmiUartDataHeader header;
	SceUInt8 encoded[1033];
} QmiUartEncodedFrame;

typedef struct QmiUartAckHeader {
	SceUInt8 markerHigh;
	SceUInt8 markerLow;
	SceUInt8 sequence;
} QmiUartAckHeader;

typedef union QmiUartFrameBuffer {
	SceUInt8 bytes[1040];
	QmiUartEncodedFrame data;
	QmiUartAckHeader acknowledgment;
} QmiUartFrameBuffer;

typedef struct QmiUartFrameEntry {
	QmiUartFrameBuffer buffer;
	SceUInt32 length;
	SceUInt32 kind;
	SceUInt32 sequence;
	SceUInt32 fragmentIndex;
	SceUInt32 fragmentCount;
	SceUInt32 attempts;
	SceUInt32 state;
	SceInt32 acknowledgmentTicks;
	struct QmiUartFrameEntry *prev;
	struct QmiUartFrameEntry *next;
} QmiUartFrameEntry;

typedef struct QmiUartFramingControl {
	QmiUartFrameEntry entries[16];
	QmiUartFrameEntry *head;
	QmiUartFrameEntry *tail;
	QmiUartFrameEntry *producer;
	SceUInt32 freeCount;
	SceUInt32 previousReceiveSequence;
	SceUInt32 lastReceiveSequence;
	SceUInt8 transmitPending;
	SceUInt8 waitingAcknowledgment;
	SceUInt8 multipartPending;
	SceUInt8 enabled;
	int (*lock)(void *context);
	int (*unlock)(void *context);
	int (*send)(const void *data, unsigned int length);
	unsigned int (*isTransmitPending)(void);
	void *context;
	SceUInt16 transmitStartMarker;
	SceUInt16 transmitEndMarker;
	SceUInt8 transmitSequence;
	SceUInt8 bufferedInput;
	SceUInt16 receiveStartMarker;
	SceUInt16 receiveEndMarker;
	SceUInt32 receiveProducer;
	SceUInt32 receiveConsumer;
	SceUInt32 receiveCount;
	SceUInt8 receiveData[2048];
	SceUInt32 decodedLength;
	SceUInt8 decodedData[768];
} QmiUartFramingControl;

_Static_assert(sizeof(QmiUartDataHeader) == 7, "UART data frame header");
_Static_assert(sizeof(QmiUartAckHeader) == 3, "UART acknowledgment header");
_Static_assert(sizeof(QmiUartFrameEntry) == 1080, "UART frame entry");
_Static_assert(offsetof(QmiUartFrameEntry, prev) == 1072, "UART frame links");
_Static_assert(sizeof(QmiUartFramingControl) == 20172, "UART framing controller");
_Static_assert(offsetof(QmiUartFramingControl, head) == 17280, "UART frame queue");
_Static_assert(offsetof(QmiUartFramingControl, receiveData) == 17352, "UART receive ring");
_Static_assert(offsetof(QmiUartFramingControl, decodedData) == 19404, "UART decoded packet");

#endif
