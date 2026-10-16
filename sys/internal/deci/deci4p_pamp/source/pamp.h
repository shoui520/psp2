/* PAMP wire and private state layouts. Internal field names are inferred. */
#pragma once

#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include "sce/deci4p.h"
#include "sce/pamgr.h"
#include "sce/processmgr.h"
#include "sce/sysmem.h"
#include "sce/zlib.h"
#include <stddef.h>

/* names unknown: the protocol number, and the status codes it reports */
#define PAMP_PROTOCOL			0x28000

#define PAMP_STATUS_OK			0
#define PAMP_STATUS_NO_SPACE		1	/* trace memory left is too small */
#define PAMP_STATUS_NOT_AVAILABLE	4	/* SCE_KERNEL_ERROR_PA_NOT_AVAILABLE */
#define PAMP_STATUS_INVALID_ARGUMENT	7
#define PAMP_STATUS_UNSUPPORTED		8	/* command or destination */
#define PAMP_STATUS_SHORT_REQUEST	9
#define PAMP_STATUS_ALREADY_OPEN	(PAMP_PROTOCOL | 0x00)
#define PAMP_STATUS_NOT_OPEN		(PAMP_PROTOCOL | 0x01)
#define PAMP_STATUS_ALREADY_TRACING	(PAMP_PROTOCOL | 0x02)
#define PAMP_STATUS_NOT_TRACING		(PAMP_PROTOCOL | 0x03)
#define PAMP_STATUS_WRONG_STATE		(PAMP_PROTOCOL | 0x04)
#define PAMP_STATUS_INVALID_UNIT	(PAMP_PROTOCOL | 0x05)
#define PAMP_STATUS_INVALID_MODE	(PAMP_PROTOCOL | 0x06)
#define PAMP_STATUS_INVALID_DESTINATION	(PAMP_PROTOCOL | 0x07)
#define PAMP_STATUS_OUT_OF_RANGE	(PAMP_PROTOCOL | 0x08)
#define PAMP_STATUS_NO_TRACE_BUFFER	(PAMP_PROTOCOL | 0x09)
#define PAMP_STATUS_INVALID_ALIGNMENT	(PAMP_PROTOCOL | 0x0a)
#define PAMP_STATUS_STREAMING		(PAMP_PROTOCOL | 0x0b)
#define PAMP_STATUS_INVALID_PROCESS	(PAMP_PROTOCOL | 0x0c)
#define PAMP_STATUS_INTERNAL_ERROR	(PAMP_PROTOCOL | 0xfe)	/* lock or deflate failure */
#define PAMP_STATUS_ERROR		(PAMP_PROTOCOL | 0xff)

/* names unknown: request commands */
enum PampCommand {
	PAMP_CMD_OPEN			= 0,
	PAMP_CMD_CLOSE			= 1,
	PAMP_CMD_GET_FREE_SIZE		= 2,
	PAMP_CMD_ALLOCATE_TRACE		= 3,
	PAMP_CMD_CONFIGURE		= 4,
	PAMP_CMD_CONTROL		= 5,
	PAMP_CMD_START			= 6,
	PAMP_CMD_STOP			= 7,
	PAMP_CMD_TRACE_STATUS		= 8,
	PAMP_CMD_STREAM_TRACE		= 9,
	PAMP_CMD_SET_PMU		= 12,
	PAMP_CMD_GET_PMU		= 13,
	PAMP_CMD_GET_PA_INFO		= 16,
	PAMP_CMD_SET_COUNTER		= 17,
	PAMP_CMD_GET_PA_VERSION		= 18,
};

/* names unknown: notifications the target sends by itself */
#define PAMP_NOTIFY_STARTED		0x80000000
#define PAMP_NOTIFY_STOPPED		0x80000001

/* names unknown: the session state */
enum PampState {
	PAMP_STATE_CLOSED,
	PAMP_STATE_OPEN,
	PAMP_STATE_TRACING,
	PAMP_STATE_STOPPED,
};

/* the trace bytes a PAMP_CMD_STREAM_TRACE packet carries */
#define PAMP_CHUNK_SIZE			0x10000

typedef struct PampPacket {
	SceDeci4pHeader hdr;
	SceUInt32 command;
	SceUInt32 sequence;
	SceUInt32 length;
	SceUInt32 status;
	union {
		SceUInt8 data[0x10fe0];
		SceUInt32 words[0x10fe0 / 4];
		SceUInt16 halves[0x10fe0 / 2];
		struct {
			SceUInt32 more;		/* another chunk follows */
			SceUInt8 bytes[PAMP_CHUNK_SIZE];
		} chunk;			/* PAMP_CMD_STREAM_TRACE */
	};
} PampPacket;

typedef struct PampBuffer {
	SceUInt32 allocated;
	SceUInt8 *base;
	SceUInt32 physical;
	SceUInt32 circular;
	SceUInt32 size;
	SceUInt32 available;
} PampBuffer;

typedef struct PampContext {
	PampPacket request;
	PampPacket buffers[2];
	SceKernelFastMutex controlLock;
	SceKernelFastMutex commandLock;
	SceUID mainThread;
	SceUID sendThread;
	SceUID bufferSema;
	SceUID sendSema;
	SceUID event;
	SceUInt32 used;
	PampPacket *reply;
	PampPacket *notification;
	PampPacket *workerPacket;
	PampPacket *sendPacket;
	SceUInt32 receiving;
	union {
		SceUInt32 traceFlags;
		struct {
			SceUInt8 traceUnit;
			SceUInt8 traceCompression;
			SceUInt8 reservedTraceFlags[2];
		};
	};
	SceUInt32 traceOffset;
	SceUInt32 traceRemaining;
	SceUInt32 streaming;
	SceUInt32 kickWorker;
	SceUInt32 finalPacket;
	SceUInt32 traceSequence;
	int handle;
	SceUInt32 state;
	SceUInt32 received;
	SceUInt32 paConfig[5];
	SceUInt32 pmuAddress;
	SceUInt32 pmuSize;
	SceUInt16 pmuConfig[2];
	SceUInt32 sampleConfig[11];
	SceUInt32 flags;
	SceUInt32 traced;
	SceUInt32 sequence;
	SceUInt8 *traceBase;
	SceUInt32 traceSize;
	SceUInt32 reserved0;
	PampBuffer trace[2];
	SceUID heap;
	SceZlibStream zstream;
	SceUInt8 scratch[0x10000];
	int zfinished;
	int key;
	SceUInt8 reserved1[0x14];
} PampContext;

_Static_assert(sizeof(PampPacket) == 0x11000, "packet slot");
_Static_assert(offsetof(PampContext, zstream) == 0x3316c, "zstream");
_Static_assert(sizeof(PampContext) == 0x431c0, "context");

/* (unnamed imports: sce/pamgr.h, sce/processmgr.h, sce/sysmem.h) */
