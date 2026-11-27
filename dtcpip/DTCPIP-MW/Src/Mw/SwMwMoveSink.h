/*
 * The move sink object (SwMwMoveSink.cpp): the DTCP "move" session behind the system
 * engine's 0x2801 init. It owns the move URI, its own lock, and the move status the
 * content object's setUri/start drive.
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwContent.h"

struct SwMwMoveSink {
	int device;			/* 0x00: the SCE device */
	int dataLabel;			/* 0x04: the device's data label */
	int manager;			/* 0x08: the device's move manager */
	struct SwMwContent *content;	/* 0x0C: the content object */
	struct SwMwUri uri;		/* 0x10: the move URI */
	int moveStatus;			/* 0x24: the content's move status */
	int moveState;			/* 0x28: the kxm label byte the move carries */
	int moveResult;			/* 0x2C */
	SceKernelLwMutexWork mutex;	/* 0x30: "sce_sws_mutex" */
	int closed;			/* 0x50 */
	int status;			/* 0x54 */
	int result;			/* 0x58: the pending move result */
	int finalized;			/* 0x5C: the MV_FINALIZE was sent */
	int sessionState[8];		/* 0x60: the move session words: phase, result, sent, phase two, second result, label
					 * (-1), requested label, reserved */
	unsigned char session[0x200];	/* 0x80: the move session's own area */
};

/* the information block the 0x2801 start command carries: the content id, the resume
 * flag, the direct block, the certificate pointer and the payload */
struct SwMwMoveSinkStreamInfoHeader {
	unsigned int word[4];		/* 0x00: content id, resume, zero, block present */
};

struct SwMwMoveSinkStreamInfo {
	struct SwMwMoveSinkStreamInfoHeader header;	/* 0x00 */
	unsigned char second[16];	/* 0x10: the sender id */
	unsigned char third[16];	/* 0x20: the receiver id */
	int value0;			/* 0x30: the direct block's length */
	int value1;			/* 0x34: the certificate object */
	unsigned char byte;		/* 0x38 */
	unsigned char reserved[3];	/* 0x39 */
	int size;			/* 0x3C: the payload size */
	unsigned char payload[0x200];	/* 0x40 */
};

/* one move record (0x230 bytes): the session id, its kind, the kxm label and key blocks,
 * the requested label, the session-data size and the payload */
struct SwMwMoveRecord {
	int id;				/* 0x000 */
	int kind;			/* 0x004 */
	unsigned char label;		/* 0x008 */
	unsigned char key[8];		/* 0x00C */
	unsigned char first[10];	/* 0x014 */
	unsigned char second[10];	/* 0x01E */
	int requested;			/* 0x028 */
	int size;			/* 0x02C */
	unsigned char payload[0x200];	/* 0x030 */
};

extern "C" {
int SwMwMoveSinkCreate(int *sink, int device);
int SwMwMoveSinkDestroy(int sink);
int SwMwMoveSinkSetUri(int sink, void *uri, int size);
int SwMwMoveSinkGetUriResult(int sink, unsigned char *out, int value);
int SwMwMoveSinkClose(int sink, unsigned char *out, int mode);
int SwMwMoveSinkFinalize(int sink, void *payload, int size, int label);
int SwMwMoveSinkComplete(int sink);
int SwMwMoveSinkMutexCreate(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexDelete(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexLock(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexUnlock(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkOnAuthResult(struct SwMwMoveSink *sink, int status, unsigned char kxmLabel);
int SwMwMoveSinkNotifyDevice(struct SwMwMoveSink *sink, int event);
int SwMwMoveSinkOnCancel(struct SwMwMoveSink *sink);
int SwMwMoveSinkOnFinalizeResult(struct SwMwMoveSink *sink, int status);
int SwMwMoveSinkOnCompleteResult(struct SwMwMoveSink *sink, int status);
int SwMwMoveSinkGetAuthId(struct SwMwMoveSink *sink);
int SwMwMoveSinkGetKxmLabel(struct SwMwMoveSink *sink);
}
