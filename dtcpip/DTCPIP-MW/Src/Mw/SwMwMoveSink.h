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
	int moveLabel;			/* 0x04: the device's move label */
	int contentId;			/* 0x08: the device's content id */
	struct SwMwContent *content;	/* 0x0C: the content object */
	struct SwMwUri uri;		/* 0x10: the move URI */
	int moveStatus;			/* 0x24: the content's move status */
	int moveState;			/* 0x28: -1 until the first move */
	int moveResult;			/* 0x2C */
	SceKernelLwMutexWork mutex;	/* 0x30: "sce_sws_mutex" */
	int notified;			/* 0x50 */
	int finished;			/* 0x54 */
	int result;			/* 0x58 */
	int padding;			/* 0x5C: not written by the create */
	int sessionState[8];		/* 0x60: the move session words, [5] = -1 */
	unsigned char session[0x200];	/* 0x80: the move session area */
};

extern "C" {
int SwMwMoveSinkCreate(int *sink, int device);
int SwMwMoveSinkDestroy(int sink);
int SwMwMoveSinkSetUri(int sink, void *uri, int size);
int SwMwMoveSinkGetUriResult(int sink, void *out, int value);
int SwMwMoveSinkClose(int sink, void *out, int value);
int SwMwMoveSinkFinalize(int sink, void *payload, int size, int label, int size2);
int SwMwMoveSinkComplete(int sink);
int SwMwMoveSinkMutexCreate(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexDelete(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexLock(SceKernelLwMutexWork *mutex);
int SwMwMoveSinkMutexUnlock(SceKernelLwMutexWork *mutex);
}
