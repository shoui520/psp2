/*
 * The MPEG-4 decoder's parser engine (0x841730-0x8471ac): the first of the
 * decoder's two engine threads, as the AVC decoder's (avcdecparser.h). It
 * takes the access units, maps their bytes, and parses them with VThread
 * jobs (the VOL, VOP, GOB and video packet headers), the helper running
 * their macroblocks, then hands each picture to the decoder engine's thread.
 */
#ifndef M4VDEC_PARSER_H
#define M4VDEC_PARSER_H

#include "service.h"

/* the engine's memory: its header, its info, the stream, the job, the unit, the VOP and the packet */
#define PARSER_HEADER_SIZE		64
#define PARSER_INFO_SIZE		64
#define PARSER_STREAM_SIZE		64
#define PARSER_JOB_SIZE			1408
#define PARSER_UNIT_SIZE		256
#define PARSER_VOP_SIZE			64
#define PARSER_PACKET_SIZE		256
#define PARSER_FIXED_SIZE		2176	/* all of them */
#define PARSER_FRAMES_SIZE		2048	/* the pictures' records */
#define PARSER_TABLE_SIZE		64	/* a VideoM4vTable */
#define PARSER_TABLES			2	/* the tables alternate */
#define PARSER_FRAME_SIZE		128	/* a VideoM4vFrame */

/* the size of the frames when no context gives it */
#define PARSER_DEFAULT_WIDTH		1280
#define PARSER_DEFAULT_HEIGHT		720

/* the engine thread's states */
#define PARSER_STOPPED			0
#define PARSER_IDLE			2	/* waiting for an access unit */
#define PARSER_PARSING			3	/* the unit's jobs run */
#define PARSER_STARTING			4	/* a unit taken, its first job running */
#define PARSER_HELPING			5	/* the helper runs the unit's macroblocks */

/* the engine thread's flags */
#define PARSER_FLAG_AU			0x00000001	/* the access unit queue */
#define PARSER_FLAG_WAKE		0x00000004	/* the wake queue */
#define PARSER_FLAG_CONTEXT		0x00000010	/* the context was set: the unit is done */
#define PARSER_FLAG_DRAIN		0x00000040	/* the stream is drained */
#define PARSER_FLAG_OUTPUT		0x00000080	/* a picture not decoded: output only */
#define PARSER_FLAG_FAILED		0x00000100	/* the unit failed */
#define PARSER_FLAG_EXIT		0x00008000
#define PARSER_FLAG_JOB_DONE		0x00010000	/* the helper or the output job is done */
#define PARSER_FLAG_DECODE		0x00020000	/* a picture to decode: the helper runs it */
#define PARSER_FLAG_SEQUENCE		0x00040000	/* only the sequence was asked: the unit is done */
#define PARSER_FLAGS			0x000781d5

/* the engine's info, read by its jobs (64 bytes) */
typedef struct M4vdecParserInfo {
	VkThread *thread;		/* the parser engine's */
	VkThread *decoderThread;	/* the decoder engine's */
	int32_t id;			/* the decoder's */
	int32_t index;			/* the decoder's slot */
	uint32_t reserved;
	uint32_t frames;		/* VideoM4vFrame, the pictures' records */
	uint32_t tables;		/* VideoM4vTable, PARSER_TABLES of them */
	uint16_t commandSize;		/* the service's */
	uint16_t horizontal;
	uint16_t contextHorizontal;	/* the context's, valid when set */
	uint16_t contextVertical;
	uint8_t contextSet;
	uint8_t contextMacroblock;	/* VIDEO_ENGINE_CONTEXT_MACROBLOCK */
	uint8_t reserved2[2];
	VideoTime option5;		/* the decoder's options, doubled */
	VideoTime option4;
	uint32_t reserved3[2];
} M4vdecParserInfo;

/* what the engine tells the decoder engine of a picture parsed (a message's word) */
typedef struct M4vdecParsed {
	uint8_t valid;
	uint8_t flush;			/* the stream was flushed */
	uint8_t pictureInfo[2];		/* the unit's, swapped */
} M4vdecParsed;

/* the engine's header (64 bytes) */
typedef struct M4vdecParser {
	int32_t state;			/* PARSER_* */
	M4vdecParserInfo *info;
	uint32_t stream;		/* a VideoM4vStream */
	uint32_t job;			/* a VideoM4vJob */
	uint32_t unit;			/* a VideoM4vUnit */
	uint32_t request;		/* the access unit's VideoEngineRequest */
	union {
		M4vdecParsed parsed;
		uint32_t parsedWord;
	};
	uint32_t frames;		/* the pictures' memory */
	uint8_t prepared;		/* the pictures were laid out */
	uint8_t flush;			/* the pictures are flushed out */
	uint8_t output;			/* the unit puts pictures out (no run for the helper) */
	uint8_t keepError;		/* the output job keeps the unit's error */
	uint8_t option;
	uint8_t reserved[3];
	VkMessageQueue *auQueue;
	VkMessageQueue *serviceQueue;	/* the service's, for the units done */
	VkMessageQueue *helperQueue;
	VkMessageQueue *decoderQueue;	/* the decoder engine's */
	VkMessageQueue *wakeQueue;
	VkVThreadContext *context;
} M4vdecParser;

#endif
