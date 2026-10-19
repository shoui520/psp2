/*
 * PU0 task 1: the demultiplexer. It reads an MPEG program stream of H.264 video
 * (PES packets, stream id 0xe0, or 0xfd for MVC) from the input ring, drops the
 * PES headers, and writes the elementary stream to the output ring and a record
 * for each NAL unit (its type, its position in the output ring, the PES
 * timestamps) to the record ring.
 *
 * The input moves by DMA from the input ring to a staging buffer in bank 1, and
 * from there to the input buffer. A start code scanner (control bus 0x9000-0x9011)
 * searches the input buffer; in a PES payload it also copies the input to the
 * output buffer, which drains to the output ring. The PES parser is an event
 * driven state machine (pes.c); the task (demux.c) runs it once a frame while the
 * ARM side keeps the rings going.
 *
 * The task's state (VipDemux) lives in local RAM while the task runs and is kept
 * in DRAM (g_demuxHome) between frames.
 */
#pragma once

#include "../common/ring.h"
#include "../common/dma.h"
#include "../common/debug.h"
#include "../common/parsed.h"
#include "scanner.h"

/* task 1's code and data overlays (tasks.c) */
#define VIP_TASK1_TEXT		__attribute__((section(".task1.text")))
#define VIP_TASK1_DATA		__attribute__((section(".task1.data")))
#define VIP_TASK1_BSS		__attribute__((section(".task1.bss")))

/* where the scanner is: its registers, saved while the task is switched out */
typedef struct VipScannerState {
	uint32_t mode;
	uint32_t read;			/* local addresses */
	uint32_t end;
	uint32_t pattern;		/* the stream id to look for, in its low byte */
	uint32_t config;
	uint32_t write;
	uint32_t status;		/* VIP_SCANNER_FOUND_* */
} VipScannerState;

/*
 * A circular buffer in local RAM: the parser's view of a part of the stream. Data
 * is added at in and taken at out; count is the data between them.
 */
typedef struct VipStreamBuffer {
	uint16_t size;
	uint16_t count;
	uint32_t start;
	uint32_t end;
	uint32_t in;
	uint32_t out;
	uint32_t peek;			/* peekBuffer's position */
	uint16_t peeked;
	uint16_t reserved;
} VipStreamBuffer;

/* a record of the record ring: a NAL unit and the timestamps before it */
typedef VipRecord VipDemuxRecord;

/* the record types of MVC; the others are by the NAL header byte (pesFlushMvc) */
#define VIP_RECORD_MVC_START		0x3400
#define VIP_RECORD_MVC_ZEROS		0x3401
#define VIP_RECORD_MVC_RESERVED		0x3407	/* NAL header bytes 0x1b-0x1f */
#define VIP_RECORD_MVC_OTHER		0x3409

/*
 * The queue of the output's NAL unit starts: their positions in the output buffer,
 * and the records[] whose timestamps go with the NAL units after them.
 */
#define VIP_PES_RECORD_NEXT		1	/* the timestamps for the next record */
#define VIP_PES_RECORD_PACKET		2
#define VIP_PES_RECORD_TIMESTAMPS	3
#define VIP_PES_QUEUE_RECORDS		4
#define VIP_PES_QUEUE_LENGTH		32

/* VipPesParser.mode */
#define VIP_PES_ACTIVE			0x001
#define VIP_PES_STOPPED_TIMEOUT		0x301	/* the frame's time was used up */
#define VIP_PES_STOPPED_EMPTY		0x401	/* the input stayed empty */
#define VIP_PES_STOPPED_BLOCKED		0x501	/* the output or record ring was full */

/* VipPesParser.result: what the last call to the parser ended with */
#define VIP_PES_RUNNING			0
#define VIP_PES_WAITING			1
#define VIP_PES_STOPPED			2
#define VIP_PES_FAILED			3

/* VipPesParser.state */
#define VIP_PES_CLOSED			0
#define VIP_PES_IDLE			1
#define VIP_PES_OPEN			2
#define VIP_PES_SCANNING		3
#define VIP_PES_PACKET_LENGTH		4
#define VIP_PES_HEADER_FLAGS		5
#define VIP_PES_HEADER			6
#define VIP_PES_STATES			7

/* VipPesParser.events: bit (event - 1) */
#define VIP_PES_EVENT_NONE		0
#define VIP_PES_EVENT_START		1
#define VIP_PES_EVENT_OPEN		2
#define VIP_PES_EVENT_RUN		3
#define VIP_PES_EVENT_CLOSE		4
#define VIP_PES_EVENT_DRAIN		5
#define VIP_PES_EVENT_BLOCKED		6	/* the output or record ring is full */
#define VIP_PES_EVENT_DATA		7	/* new input is in the input buffer */
#define VIP_PES_EVENT_IDLE		8	/* the input stayed empty */
#define VIP_PES_EVENT_TIMEOUT		9	/* the frame's time is used up */
#define VIP_PES_EVENT_PAYLOAD_END	10	/* the PES packet's payload is done */
#define VIP_PES_EVENT_PACKET		11	/* a start code with the stream id */
#define VIP_PES_EVENT_START_CODE	12	/* 00 00 01 */
#define VIP_PES_EVENT_ZERO_CODE		13	/* 00 00 00 */
#define VIP_PES_EVENT_OUTPUT_FULL	14	/* the output buffer is full */
#define VIP_PES_EVENT_SCAN_END		15	/* the scanner reached the input's end */
#define VIP_PES_EVENTS			16

#define VIP_PES_EVENT_BIT(event)	(1u << ((event) - 1))
#define VIP_PES_CONTROL_EVENTS		0x01ff
#define VIP_PES_SCANNER_EVENTS		0xfe00

/* VipPesParser.rescan: scan the input buffer before the next event */
#define VIP_PES_RESCAN			0xe

/* VipPesParser.streamType */
#define VIP_PES_STREAM_AVC		0	/* stream id 0xe0-0xef */
#define VIP_PES_STREAM_MVC		1	/* stream id 0xfd */

#define VIP_PES_STREAM_ID_AVC		0xe0
#define VIP_PES_STREAM_ID_MVC		0xfd

typedef struct VipPesParser VipPesParser;

/* moves length bytes at a local address on: the bytes moved, or -1 */
typedef int (*VipPesTransfer)(uint32_t local, uint32_t length);
typedef int (*VipPesConsumer)(VipPesParser *parser, uint32_t local, uint32_t length);

/* the lengths of a PES packet, from its header */
typedef struct VipPesPacket {
	uint16_t length;		/* PES_packet_length; then the payload's */
	uint16_t headerSize;		/* the optional fields' bytes */
	uint16_t headerLength;		/* from the start code to the payload */
	uint16_t reserved;
} VipPesPacket;

struct VipPesParser {
	uint16_t mode;			/* VIP_PES_ACTIVE, VIP_PES_STOPPED_* */
	int16_t result;			/* VIP_PES_RUNNING, ... */
	uint16_t state;			/* VIP_PES_CLOSED, ... */
	uint16_t events;		/* VIP_PES_EVENT_BIT()s pending */
	int16_t rescan;			/* VIP_PES_RESCAN */
	uint16_t streamType;		/* VIP_PES_STREAM_* */
	uint32_t deadline;		/* readTime() when the frame's time is up */
	uint32_t reserved0;
	int16_t lengthKnown;		/* the PES packet has a length */
	int16_t inPayload;		/* the scanner is in a PES payload */
	VipPesPacket packet;		/* the header being read */
	VipPesPacket payload;		/* the payload being scanned: length is left */
	uint8_t header[4];		/* the PES header's flags and length bytes */
	VipScannerState scanner;	/* saved while the scanner is idle */
	uint16_t startCodeLength;
	uint16_t scannedOutput;		/* bytes the scanner wrote to the output */
	uint16_t scannedInput;		/* bytes the scanner moved past */
	int (*start)(VipPesParser *parser);
	int (*open)(VipPesParser *parser);
	int (*run)(VipPesParser *parser);
	int (*close)(VipPesParser *parser);
	int (*drain)(VipPesParser *parser);
	int (*flush)(VipPesParser *parser);
	VipPesTransfer readInput;	/* input ring to the staging buffer */
	VipPesTransfer readStaged;	/* staging buffer to the input buffer */
	VipPesTransfer writeOutput;	/* output buffer to the output ring */
	VipPesTransfer writeRecord;	/* a VipDemuxRecord to the record ring */
	VipStreamBuffer input;
	VipStreamBuffer staging;	/* in bank 1 */
	uint32_t stagedBytes;		/* moved to the input buffer this frame */
	uint32_t idleScans;		/* polls that found little new input */
	VipStreamBuffer output;
	uint32_t resumeEvents;		/* VIP_PES_EVENT_BIT()s to retry, or 1 */
	int16_t timestampRecord;	/* VIP_PES_RECORD_PACKET or _TIMESTAMPS */
	uint8_t zeroRun;		/* 0 while the output ends in zero bytes */
	uint8_t endPending;		/* the stream end record waits for room */
	VipDemuxRecord *records;	/* VIP_PES_QUEUE_RECORDS */
	uint16_t queueHead;
	uint16_t queueTail;
	uint32_t queue[VIP_PES_QUEUE_LENGTH];
	uint32_t packets;		/* statistics, copied to DRAM each frame */
	uint32_t badHeaders;
	uint32_t badTimestamps;
	uint32_t errors;		/* VIP_PES_ERROR_* seen */
	uint32_t reserved1;
};

/* VipPesParser.errors: from readHeaderFlags */
#define VIP_PES_ERROR_MARKER		0x01	/* the header's '10' bits */
#define VIP_PES_ERROR_SCRAMBLED		0x02
#define VIP_PES_ERROR_TIMESTAMP_FLAGS	0x04

/* from readTimestamps */
#define VIP_PES_ERROR_PTS		0x01
#define VIP_PES_ERROR_PTS_MARKER	0x02
#define VIP_PES_ERROR_PTS_PREFIX	0x04
#define VIP_PES_ERROR_DTS		0x08
#define VIP_PES_ERROR_DTS_MARKER	0x10

/* a ring the task uses, and its local copy while the task runs */
typedef struct VipDemuxStream {
	VipRingHandle handle;		/* handle.ring: shared, or &local */
	VipRing *shared;		/* the ring in DRAM, while handle.ring is &local */
	VipRing local;
	int32_t available;		/* input: bytes to read; output: room */
} VipDemuxStream;

/* the task's cursor in each ring: the input's first reader, the others' writer */
#define VIP_DEMUX_READER		1
#define VIP_DEMUX_WRITER		0

/* VipDemux.state */
#define VIP_DEMUX_CLOSED		0
#define VIP_DEMUX_OPENING		1
#define VIP_DEMUX_CLOSING		2
#define VIP_DEMUX_OPEN			4

/* VipDemux.mode, from the debugger */
#define VIP_DEMUX_MODE_STREAM		0xf	/* 3: MVC */
#define VIP_DEMUX_MODE_MVC		3
#define VIP_DEMUX_MODE_LEVELS_SHIFT	14	/* report the input level ... */
#define VIP_DEMUX_MODE_LEVELS_MASK	3
#define VIP_DEMUX_MODE_LEVELS		2	/* ... when the field is this */

/* the done flag of a DMA list before it is queued */
#define VIP_DEMUX_LIST_IDLE		((uint32_t)-2)

#define VIP_DEMUX_INPUT_LIST		3
#define VIP_DEMUX_STAGING_LIST		3
#define VIP_DEMUX_OUTPUT_LIST		10

/* the debugger events for the input level */
#define VIP_DEMUX_EVENT_EMPTY		0x1000000
#define VIP_DEMUX_EVENT_FULL		0x2000000

/* the demultiplexer's parameters from the ARM side (debug area 0) */
typedef struct VipDemuxParameters {
	uint32_t reserved0[2];
	uint32_t holdWhenFull;		/* tell the input's writer to hold */
	uint32_t notifyFull;
	uint32_t notifyEmpty;
	volatile uint32_t full;		/* set when the input runs full */
	volatile uint32_t empty;
	uint32_t reserved1[3];
	uint32_t fullSpace;		/* the input is full at or below this room */
	uint32_t emptySpace;		/* and empty at or above this */
} VipDemuxParameters;

#define VIP_DEMUX_PARAMETERS		((VipDemuxParameters *)VIP_DEBUG_AREA_0)
#define VIP_DEMUX_MODE_REGISTER		0x10c	/* readDebugRegister */

/* the debug point that opens and closes the demultiplexer, and its values */
#define VIP_DEMUX_POINT			0
#define VIP_DEMUX_POINT_INPUT		0	/* the rings */
#define VIP_DEMUX_POINT_OUTPUT		1
#define VIP_DEMUX_POINT_RECORDS		2
#define VIP_DEMUX_ACTION_OPEN		1
#define VIP_DEMUX_ACTION_CLOSE		2
#define VIP_DEMUX_OPEN_COUNT		20
#define VIP_DEMUX_CLOSE_COUNT		8

/* the parser's statistics in DRAM */
#define VIP_DEMUX_STATISTICS		(VIP_STATISTICS_ADDRESS + 0x200)

/* the buffers: input and output in the scanner's window, staging in bank 1 */
#define VIP_DEMUX_BUFFER_SIZE		0x200
#define VIP_DEMUX_STAGING		(VIP_LOCAL_BANK1 | VIP_LOCAL_RAM | 0x1600)

/* the core and the DMA engine's external side see bank 1 this much higher */
#define VIP_DEMUX_STAGING_BUS		0x400

typedef struct VipDemux {
	uint32_t mode;			/* VIP_DEMUX_MODE_* */
	uint32_t holdWhenFull;		/* VipDemuxParameters, each frame */
	uint32_t notifyFull;
	uint32_t notifyEmpty;
	uint32_t fullSpace;		/* VipDemuxParameters, when opened */
	uint32_t emptySpace;
	uint32_t reserved0;
	uint32_t emptyReported;
	uint32_t fullReported;
	VipDemuxStream input;
	VipDemuxStream output;
	uint32_t outputWritten;		/* bytes written to the output this frame */
	VipDemuxStream recordStream;
	uint32_t state;			/* VIP_DEMUX_* */
	uint32_t inputListCount;	/* the DMA lists being built */
	volatile uint32_t inputListBusy;
	uint32_t stagingListCount;
	volatile uint32_t stagingListBusy;
	uint32_t outputListCount;
	volatile uint32_t outputListBusy;
	VipDemuxRecord records[VIP_PES_QUEUE_RECORDS];
	VipPesParser parser;
	VipPesParser *copyParser;	/* the parser of the last copy callback */
	uint8_t lastByte;		/* skipBytes: the first byte skipped */
	uint8_t reserved1[3];
	uint32_t copySource;		/* copyFromSource's next address */
	VipDmaDescriptor inputList[VIP_DEMUX_INPUT_LIST];
	VipDmaDescriptor stagingList[VIP_DEMUX_STAGING_LIST];
	VipDmaDescriptor outputList[VIP_DEMUX_OUTPUT_LIST];
} VipDemux;

extern VipDemux g_demux;

/* demux.c */
int readInputByCopy(uint32_t local, uint32_t length);
int readStagedByCopy(uint32_t local, uint32_t length);
int writeOutputByCopy(uint32_t local, uint32_t length);
int readInputByDma(uint32_t local, uint32_t length);
int readStagedByDma(uint32_t local, uint32_t length);
int writeOutputByDma(uint32_t local, uint32_t length);
int writeRecord(uint32_t local, uint32_t length);
void resetBuffers(VipPesParser *parser);
uint32_t copyByScanner(uint32_t source, uint32_t destination, uint32_t length);

/* pes.c */
int pesStart(VipPesParser *parser);
int pesOpen(VipPesParser *parser);
int pesRun(VipPesParser *parser);
int pesClose(VipPesParser *parser);
int pesDrain(VipPesParser *parser);
int pesFlushAvc(VipPesParser *parser);
int pesFlushMvc(VipPesParser *parser);
uint32_t moveInBuffer(VipStreamBuffer *buffer, uint32_t inData, uint32_t position,
	int32_t distance);
int fillBuffer(VipPesParser *parser, VipStreamBuffer *buffer, uint32_t to,
	VipPesConsumer producer);
int scanCallback(VipPesParser *parser, uint32_t local, uint32_t length);

/* the overlay (overlay.c) */
void runParser(VipPesParser *parser);
void reportInputLevel(void);
int flushInputList(void);
int flushStagingList(void);
int flushOutputList(void);
int drainBuffer(VipPesParser *parser, VipStreamBuffer *buffer, uint32_t to,
	VipPesConsumer consumer);
int skipBytes(VipPesParser *parser, uint32_t local, uint32_t length);
void saveScanner(VipScannerState *state);
void restoreScanner(const VipScannerState *state);
void setScannerRange(uint32_t read, uint32_t end);
uint32_t runScanner(uint32_t inPayload);
int queueStartCodes(VipPesParser *parser, uint32_t position, int32_t distance,
	uint32_t count);

/* clock.c */
void enableDemuxClocks(void);
void gateDemuxClocks(void);

/* demux.c */
void demuxTask(uint32_t argument);
