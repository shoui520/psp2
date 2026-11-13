/*
 * The AVC decoder's parser engine (0x824ca0-0x82cd90): the first of the
 * decoder's two engine threads. It takes the access units, maps their
 * bytes, and parses them with VThread jobs (the slice headers, the
 * reference lists and the tables the decoder engine needs), then hands each
 * picture to the decoder engine's thread.
 */
#ifndef AVCDEC_PARSER_H
#define AVCDEC_PARSER_H

#include "service.h"

/* the engine's memory: its header, its info, the stream, the job and the unit */
#define PARSER_HEADER_SIZE		64
#define PARSER_INFO_SIZE		128
#define PARSER_STREAM_SIZE		128
#define PARSER_JOB_SIZE			192
#define PARSER_NAL_SIZE			1216
#define PARSER_FIXED_SIZE		1728	/* all of them */
#define PICTURE_SIZE			128	/* a VideoAvcPicture */
#define PICTURE_DATA_SIZE		256
#define PICTURE_EXTRA_SIZE		16
#define PARAMETER_SET_SIZE		1088
#define PARAMETER_SET2_SIZE		16
#define MAX_PICTURES			16	/* the frames a stream keeps, the two in work aside */

/* the engine thread's states */
#define PARSER_STOPPED			0
#define PARSER_IDLE			2	/* waiting for an access unit */
#define PARSER_PARSING			3	/* the unit's jobs run */
#define PARSER_STARTING			4	/* a unit taken, its first job running */
#define PARSER_HELPING			5	/* the helper parses the unit's slices */

/* the thread's flags */
#define PARSER_FLAG_AU			0x00000001	/* the access unit queue */
#define PARSER_FLAG_WAKE		0x00000004	/* the wake queue */
#define PARSER_FLAG_CONTEXT		0x00000010	/* the context was set: the unit is done */
#define PARSER_FLAG_DRAIN		0x00000040	/* the stream is drained */
#define PARSER_FLAG_OUTPUT		0x00000080	/* a picture of slices not decoded: output only */
#define PARSER_FLAG_FAILED		0x00000100	/* the unit failed */
#define PARSER_FLAG_EXIT		0x00008000
#define PARSER_FLAG_JOB_DONE		0x00010000	/* the helper or the output job is done */
#define PARSER_FLAG_DECODE		0x00020000	/* a picture to decode: the helper takes its slices */
#define PARSER_FLAG_SEQUENCE		0x00040000	/* only the sequence was asked: the unit is done */
#define PARSER_FLAGS			0x000781d5

/* the engine's info: what its jobs share (128 bytes) */
typedef struct AvcdecParserInfo {
	VkThread *thread;		/* the parser engine's */
	VkThread *decoderThread;	/* the decoder engine's */
	int32_t id;			/* the decoder's */
	int16_t index;			/* the decoder's slot */
	uint16_t cacheSize;
	uint32_t reserved;
	uint32_t parameterSets;		/* 1088 bytes each, maxStreams of them */
	uint32_t parameterSets2;	/* 16 bytes each, maxFrames of them */
	uint32_t parameterCache;
	uint32_t streamData;		/* 256 bytes a stream */
	uint32_t pictureData;		/* 256 bytes a picture */
	uint32_t decoderData;		/* 2048 bytes */
	uint32_t pictures;		/* VideoAvcPicture */
	uint32_t pictureExtras;		/* 16 bytes each */
	uint16_t commandSize;		/* the service's */
	uint16_t horizontal;
	uint16_t contextHorizontal;	/* the context's, valid when set */
	uint16_t contextVertical;
	uint8_t contextRefFrames;
	uint8_t fieldDecode;
	uint8_t contextOption;
	uint8_t contextSet;
	VideoTime option5;		/* the decoder's options, doubled */
	VideoTime option4;
	uint32_t reserved2[12];
} AvcdecParserInfo;

/* what the engine tells the decoder engine of a picture parsed (a message's word) */
typedef struct AvcdecParsed {
	uint8_t valid;
	uint8_t flush;			/* the stream was flushed */
	uint8_t pictureInfo[2];		/* the unit's, swapped */
} AvcdecParsed;

/* the engine's header (64 bytes) */
typedef struct AvcdecParser {
	int32_t state;			/* PARSER_* */
	AvcdecParserInfo *info;
	uint32_t stream;		/* a VideoStream */
	uint32_t job;			/* a VideoSliceJob */
	uint32_t nal;			/* a VideoNal */
	uint32_t request;		/* the access unit's VideoEngineRequest */
	union {
		AvcdecParsed parsed;
		uint32_t parsedWord;
	};
	uint32_t frames;		/* the pictures' memory */
	uint8_t prepared;		/* the pictures were laid out */
	uint8_t flush;			/* the pictures are flushed out */
	uint8_t output;			/* the unit puts pictures out (no slices for the helper) */
	uint8_t keepError;		/* the output job keeps the unit's error */
	uint8_t option;
	uint8_t reserved[3];
	VkMessageQueue *auQueue;
	VkMessageQueue *serviceQueue;	/* the service's, for the units done */
	VkMessageQueue *helperQueue;
	VkMessageQueue *decoderQueue;	/* the decoder engine's */
	VkMessageQueue *wakeQueue;
	VkVThreadContext *context;
} AvcdecParser;

#endif
