/*
 * FW2's stream parsers: the library both parser programs build in (decoder.c,
 * run from DRAM, and in part from the program's code overlay). It is fw1's PU0
 * decoder front end (fw1/pu0/decoder.c) made to serve one context per command:
 * what fw1 kept per channel is the context's.
 *
 * The parser reads a record per NAL unit (VipRecord, VIP_RECORD_AVC_*) and the
 * elementary stream the records point into, has the bitstream unit parse each
 * unit, and writes the results and the slice data to the parsed stream with a
 * record for each (VIP_PARSED_*), for the codec on core 1. A command runs it
 * until a picture's input ends (VIP_RECORD_PICTURE_END), the stream ends
 * (VIP_RECORD_STREAM_END), the input runs dry or the output is full. Between
 * commands its state lives in the context: the saved local RAM data
 * (VIP_*_SAVED) and the $gp globals (VipParserGlobals).
 *
 * The program around the library gives it its input: parser0 scans an
 * elementary stream for start codes and writes the records itself. The program
 * loads each NAL unit into the read stream (g_loadNal), routes the units'
 * interrupts and runs the commands.
 */
#pragma once

#include "../common/runtime.h"
#include "../common/ring.h"
#include "../../fw1/common/dma.h"
#include "../../fw1/pu0/bitstream.h"

/* where the parsers' code and data go (fw2/parser.ld) */
#define VIP_LIBRARY_SAVED	__attribute__((section(".saved.library")))
#define VIP_PROGRAM_SAVED	__attribute__((section(".saved.program")))
#define VIP_LIBRARY_LOCAL	__attribute__((section(".local.library")))
#define VIP_PROGRAM_LOCAL	__attribute__((section(".local.program")))
#define VIP_LIBRARY_OVERLAY	__attribute__((section(".overlay.library"), noinline))
#define VIP_PROGRAM_OVERLAY	__attribute__((section(".overlay.program"), noinline))
/*
 * Out of the overlay, in DRAM: what the overlay calls, and what the firmware runs
 * from the overlay but ours cannot, since our code is larger and local RAM holds
 * 0xed0 bytes of overlay and loader (the setup of a command, a scan or a PES
 * header, not the interrupts or the per-unit steps)
 */
#define VIP_DRAM		__attribute__((noinline))

/* g_decoderState: fw1's VIP_DECODER_*, and */
#define VIP_DECODER_PICTURE_DONE	9	/* a picture's input is parsed */
#define VIP_DECODER_HELD		0xb	/* an output ring is full */

/* the interrupts of the bitstream unit and the streams */
#define VIP_PARSER_INTERRUPTS		0xfe0	/* 5 to 11 */

/* how long to wait for a bitstream command */
#define VIP_BITSTREAM_POLLS		2666

/* a stream buffer's DRAM side: the segments its halves are copied from or to */
typedef struct VipParserStream {
	uint32_t cursor;		/* the next DRAM address to transfer */
	VipRingSegment segments[VIP_STREAM_SEGMENTS];	/* in stream order */
	uint32_t segmentCount;
	uint32_t half;			/* the half to transfer next, by its low bit */
} VipParserStream;

/* the context's globals ($gp): the ARM side reads the statistics */
typedef struct VipParserGlobals {
	uint32_t mode;			/* the program's: the commands it takes */
	uint32_t pictures;		/* g_pictures, ... as the last command left them */
	uint32_t reserved0[3];
	uint32_t starved;
	uint32_t overflows;
	union {				/* the program's */
		struct {
			uint32_t truncatedUnits;	/* NAL units cut for being too long */
			uint8_t options;
		} parser0;
		struct {
			uint32_t endRecords;	/* picture and stream ends recorded */
			uint32_t units;		/* NAL units recorded */
			uint8_t options;
		} parser1;
	};
	int32_t state;			/* vipSetState's, the program's */
	VipSps sps[VIP_SPS_COUNT];
	VipPps pps[VIP_PPS_COUNT];
} VipParserGlobals;

extern VipParserGlobals g_parser VIP_GLOBAL;

/* the parsers' commands (VipCommand.kind) */
#define VIP_PARSER_SET_OPTION		4
#define VIP_PARSER_OPEN			0x10000
#define VIP_PARSER_RUN			0x20000
#define VIP_PARSER_CLOSE		0x40000

/* VipParserGlobals.mode: the commands a parser takes */
#define VIP_PARSER_CLOSED		0	/* VIP_PARSER_OPEN, VIP_PARSER_SET_OPTION */
#define VIP_PARSER_OPENED		1	/* VIP_PARSER_RUN */
#define VIP_PARSER_ENDED		2	/* VIP_PARSER_CLOSE: the stream has ended */

/* VipParserGlobals.state: the mode in its low half, and what holds the parser */
#define VIP_PARSER_MODE_MASK		0xffff
#define VIP_PARSER_INPUT_HELD		0x10000	/* the NAL record ring is full */
#define VIP_PARSER_OUTPUT_HELD		0x20000	/* the parsed stream is full */
#define VIP_PARSER_RECORDS_HELD		0x40000	/* its record ring is full */

/* VipParserGlobals' options */
#define VIP_PARSER_OPTION_NEED_INPUT	4	/* refuse a run with nothing to parse */

/* a command's results besides VIP_RESULT_OK and VIP_PROGRAM_CONTINUE */
#define VIP_PARSER_FAILED		-2	/* the open failed: response[4] says why */
#define VIP_PARSER_WRONG_MODE		-3
#define VIP_PARSER_NO_COMMAND		-5	/* none, or one the parser does not know */
#define VIP_PARSER_BAD_MODE		-8
#define VIP_PARSER_HELD			-11	/* an output ring is full */
#define VIP_PARSER_NO_INPUT		-12

/* the events the parsers raise (vipNotifyEvents) */
#define VIP_PARSER_EVENT_FAILED		0x800000	/* the decoder stopped on an error */
#define VIP_PARSER_EVENT_PICTURE_END	0x1000000

/* decoder.c: the context's parser state the programs see */
extern volatile uint32_t g_decoderState;
extern int8_t g_decoderOpen;		/* until the stream ends */
extern uint8_t g_keepInput;		/* the program's: never drop a stuck input */
extern void (*g_loadNal)(void (*callback)(void));	/* the program's */
extern VipParserStream g_readStream;
extern VipRingHandle g_outputRing;		/* the parsed stream */
extern VipRingHandle g_outputRecordRing;

/* what the DMA reads from local RAM besides the streams */
typedef union VipParserDmaSource {
	VipParsedRecord record;		/* an empty record for the output */
	struct {			/* the clearing of the CABAC state */
		VipDmaDescriptor descriptor;
		uint32_t zeros[2];
	} clear;
	VipDmaDescriptor readLists[2][VIP_STREAM_SEGMENTS];	/* parser1's read stream */
} VipParserDmaSource;

extern VipParserDmaSource g_dmaSource;
extern uint32_t g_errorCode;		/* VIP_DECODER_ERROR_* */
extern uint32_t g_overflows;
extern uint32_t g_starved;
extern uint32_t g_pictures;

void resetScanner(void);
void resetBitstream(void);
int decodeRecords(void);
void startRecord(void);
void initializeDecoder(void);
void stopDecoder(void);
int openDecoder(VipRing *input, int32_t inputCursor, VipRing *records,
	int32_t recordsCursor, VipRing *output, VipRing *outputRecords);
void holdWriteStream(void);

/* decoder.c, in the program's code overlay */
uint32_t readBits(uint32_t count);
void setupInterrupts(void);
void decoderInterrupt(void);
void interrupt8(void) __attribute__((interrupt));
void interrupt11(void) __attribute__((interrupt));
void readStreamHalf0Ready(void);
void readStreamHalf1Ready(void);
void writeStreamInterrupt(void);
void parserInterrupt(void);		/* vectors.S's entry calls it */

/* the program's, which parserInterrupt runs */
void readStreamInterrupt(void);
void scannerInterrupt(void);

/* the scanner's interrupt */
#define VIP_INTERRUPT_SCANNER		12

/* the NAL unit types the scanners tell apart */
#define VIP_NAL_SLICE			1
#define VIP_NAL_IDR_SLICE		5
#define VIP_NAL_SEI			6	/* SEI to filler data have records by type */
#define VIP_NAL_ACCESS_UNIT_LAST	9	/* SEI to the delimiter start an access unit */

static inline int vipNalIsSlice(uint32_t header)
{
	uint32_t type = header & 0x1f;

	return type == VIP_NAL_SLICE || type == VIP_NAL_IDR_SLICE;
}

/*
 * A NAL unit (its header, and the byte after it) starts the next access unit
 * after a picture's slices: an SEI, SPS, PPS or delimiter, or a slice with
 * first_mb_in_slice 0.
 */
static inline int vipNalEndsPicture(uint32_t header, uint32_t next, uint32_t inPicture)
{
	uint32_t type = header & 0x1f;

	if (!inPicture)
		return 0;
	if (type >= VIP_NAL_SEI && type <= VIP_NAL_ACCESS_UNIT_LAST)
		return 1;
	return vipNalIsSlice(header) && (next & 0x80);
}

/* the record type of a NAL unit */
static inline uint32_t vipNalRecordType(uint32_t header, uint32_t next)
{
	int32_t type = header & 0x1f;

	if (vipNalIsSlice(header))
		return next & 0x80 ? VIP_RECORD_AVC_PICTURE : VIP_RECORD_AVC_SLICE;
	type -= VIP_NAL_SEI;
	if (type < 0 || type > VIP_RECORD_AVC_OTHER - VIP_RECORD_AVC_SEI)
		type = VIP_RECORD_AVC_OTHER - VIP_RECORD_AVC_SEI;
	return VIP_RECORD_AVC_SEI + type;
}

/* scannerStep's results */
#define VIP_STEP_STARTED		0
#define VIP_STEP_NOTHING		-1
#define VIP_STEP_BUSY			-2
#define VIP_STEP_ENDED			-3
#define VIP_STEP_PICTURE_END		-4

/* how often a parser goes round without sleeping before it waits */
#define VIP_PARSER_IDLE_ROUNDS		4
#define VIP_PARSER_IDLE_WAIT		0x2000	/* timer ticks */

/* the program's: the scanner's state, which resetScanner clears */
extern uint32_t g_scannerState;
