/*
 * The sink object (SwMwSink.cpp): the per-sink content session the library's 0x2201
 * connect creates, its URI, its lock and the close/status handshake.
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwContent.h"

struct SwMwSink {
	int device;			/* 0x00: the SCE device */
	int reserved;			/* 0x04: the support base's word */
	int connected;			/* 0x08: the device was connected */
	int uriSet;			/* 0x0C: the content has the URI */
	struct SwMwContent *content;	/* 0x10: the content object */
	struct SwMwUri uri;		/* 0x14: the URI value */
	SceKernelLwMutexWork mutex;	/* 0x28: "sce_sws_mutex" */
	int closed;			/* 0x48: close was asked */
	int status;			/* 0x4C: the close status */
	int notified;			/* 0x50: the content was told */
	int padding;			/* 0x54 */
};

extern "C" {
int SwMwSinkCreate(int *sink, int device);
int SwMwSinkDestroy(int sink);
int SwMwSinkClose(int sink, int mode);
int SwMwSinkSetUri(int sink, void *uri, int size);
int SwMwSinkGetUriResult(int sink, int value);
int SwMwSinkMutexCreate(SceKernelLwMutexWork *mutex);
int SwMwSinkMutexDelete(SceKernelLwMutexWork *mutex);
int SwMwSinkMutexLock(SceKernelLwMutexWork *mutex);
int SwMwSinkMutexUnlock(SceKernelLwMutexWork *mutex);
int SwMwSinkSetStatus(int sink, int status);
int SwMwSinkGetUriState(int sink);
int SwMwSinkDeviceNotify(int sink, int value);
void SwMwContentGetInfo(void *content, void *info);
int SwMwContentGetStatus(void *content);
}
