/*
 * The sink's stream (SwMwSinkStream.cpp): the per-stream object the middleware opens
 * behind a sink's openStream — the reader/downloader pair it hands the content to, the
 * transfer bookkeeping its receive/send/release commands walk, and the gate its commands
 * serialize on (the same gate the dispatcher takes at +0x660).
 */

#pragma once

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwSync.h"

/* the windows the stream shares with the library: the receive and send areas of the
 * shared memory, their sizes and the block size */
struct SwMwStreamWindow {
	union {
		struct {
			unsigned char *receiveBase;	/* 0x00: base + receive size */
			int sendSize;			/* 0x04 */
			unsigned char *sendBase;	/* 0x08: the shared memory's base */
			int receiveSize;		/* 0x0C */
			int reserved;			/* 0x10: the library's config[3] */
			int blockSize;			/* 0x14 */
		};
		int value[6];				/* the same words, as the commands copy them */
	};
};

/* one chunk of a transfer: where it starts in its window and how many bytes it holds */
struct SwMwStreamChunk {
	int offset;	/* 0x00 */
	int size;	/* 0x04 */
};

/* the event object the stream registers with its reader: the handler the reader calls,
 * its argument and the EEMI type the reader last reported */
struct SwMwStreamEvent {
	void *vtable;			/* 0x00 */
	void *context;			/* 0x04 */
	void *handler;			/* 0x08 */
	void *argument;			/* 0x0C */
	unsigned char eemi;		/* 0x10 */
	unsigned char reserved[3];	/* 0x11 */
};

/* the stream object (0x700 bytes): the sink it belongs to, the two objects that move its
 * data, the window it shares with the library and the per-transfer state */
struct SwMwSinkStream {
	int state;			/* 0x000: 1 once init succeeded */
	int sink;			/* 0x004: the sink that created the stream */
	int contentId;			/* 0x008: the content the stream plays */
	void *downloader;		/* 0x00C: the download object */
	void *reader;			/* 0x010: the read object */
	struct SwMwStreamWindow window;	/* 0x014: the shared-memory windows */
	struct SwMwStreamEvent event;	/* 0x02C: the reader's event object */
	unsigned char work[0x498];	/* 0x040: the reader's work area */
	void *content;			/* 0x4D8: the content manager */
	struct SwMwStreamChunk *receiveChunks;	/* 0x4DC */
	int receiveCount;		/* 0x4E0 */
	int receiveIndex;		/* 0x4E4 */
	int receiveConsumed;		/* 0x4E8 */
	int receiveHave;		/* 0x4EC */
	struct SwMwStreamChunk *sendChunks;	/* 0x4F0 */
	int sendCount;			/* 0x4F4 */
	int sendIndex;			/* 0x4F8 */
	int sendConsumed;		/* 0x4FC */
	int sendHave;			/* 0x500 */
	int sendTotal;			/* 0x504 */
	SceKernelLwMutexWork mutex;	/* 0x508: the bookkeeping's lwmutex */
	int sending;			/* 0x528: a send is in flight */
	int sendingMessage;		/* 0x52C: a message send is in flight */
	int receiving;			/* 0x530: a receive is in flight */
	int parameterBusy;		/* 0x534: a parameter set is in flight */
	int aborted;			/* 0x538 */
	int empty;			/* 0x53C */
	int finished;			/* 0x540 */
	int connected;			/* 0x544 */
	int status;			/* 0x548 */
	int emptyStatus;		/* 0x54C: 12401 */
	char name[0x100];		/* 0x550: the stream's name */
	int resume;			/* 0x650 */
	unsigned char connectData[12];	/* 0x654 */
	struct SwMwGate gate;		/* 0x660 */
};

extern "C" {
/* construct and tear the object down (SwMwSinkStream.cpp's class methods) */
struct SwMwSinkStream *SwMwSinkStreamConstruct(struct SwMwSinkStream *stream);
struct SwMwSinkStream *SwMwSinkStreamConstructCopy(struct SwMwSinkStream *stream);
struct SwMwSinkStream *SwMwSinkStreamDestruct(struct SwMwSinkStream *stream);
struct SwMwSinkStream *SwMwSinkStreamDestructCopy(struct SwMwSinkStream *stream);

/* the object the dispatcher holds: create/init/destroy and the gate the commands take */
int SwMwSinkStreamCreate(struct SwMwSinkStream **stream, void *sink,
			 const struct SwMwStreamWindow *window);
int SwMwSinkStreamInit(struct SwMwSinkStream *stream, void *sink,
		       const struct SwMwStreamWindow *window);
int SwMwSinkStreamDestroy(struct SwMwSinkStream *stream);
struct SwMwGate *SwMwSinkStreamGate(struct SwMwSinkStream *stream);

/* the stream's operations (SwMwSinkStream.cpp); the bodies the dispatcher's forwarders
 * call are the file's own implementations */
int SwMwSinkStreamGetInfo(struct SwMwSinkStream *stream, void *info);
int SwMwSinkStreamSetParameter(struct SwMwSinkStream *stream, int parameter);
int SwMwSinkStreamPoll(struct SwMwSinkStream *stream);
int SwMwSinkStreamReceive(struct SwMwSinkStream *stream, int *offset, int *size,
			  unsigned char *type);
int SwMwSinkStreamRelease(struct SwMwSinkStream *stream, int size);
int SwMwSinkStreamSend(struct SwMwSinkStream *stream, int *offset, int *size);
int SwMwSinkStreamSendMessage(struct SwMwSinkStream *stream, int message);
int SwMwSinkStreamAbort(struct SwMwSinkStream *stream);
int SwMwSinkStreamIsOpen(struct SwMwSinkStream *stream);
int SwMwSinkStreamConnect(struct SwMwSinkStream *stream);
int SwMwSinkStreamReauthenticate(struct SwMwSinkStream *stream);
void SwMwSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window);

/* the state the stream keeps for its reader and downloader */
int SwMwSinkStreamMutexCreate(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamMutexDelete(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamEventCreate(struct SwMwSignal *signal);
int SwMwSinkStreamEventDelete(struct SwMwSignal *signal);
int SwMwSinkStreamMutexLock(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamWait(struct SwMwSignal *signal);
int SwMwSinkStreamSignal(struct SwMwSignal *signal);
int SwMwSinkStreamStateLock(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamStateUnlock(SceKernelLwMutexWork *mutex);
int SwMwSinkStreamSetFinished(struct SwMwSinkStream *stream, int status);
int SwMwSinkStreamSetHandler(struct SwMwSinkStream *stream, void *handler, void *argument);
int SwMwStreamEventSet(struct SwMwStreamEvent *event, void *handler, void *context,
		       void *argument);
int SwMwSinkStreamSetName(struct SwMwSinkStream *stream, const char *name);
int SwMwSinkStreamNoop(void);

/* the stream's event callbacks (the reader's virtual handlers) */
void SwMwSinkStreamOnKeyExpired(struct SwMwStreamEvent *event);
void SwMwSinkStreamOnDiscard(struct SwMwStreamEvent *event, int reason);
void SwMwSinkStreamOnEemi(struct SwMwStreamEvent *event, int type, int value);
int SwMwSinkStreamOnEvent(struct SwMwSinkStream *stream, int *message);
}
