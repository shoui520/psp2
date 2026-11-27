/*
 * The sink's stream object (see SwMwSinkStream.h): its construction, the lwmutex and
 * event primitives its reader uses, the transfer bookkeeping the receive/send commands
 * share, its gate and its reader callbacks. The transfer bodies themselves (getInfo,
 * setParameter, receive, release, send, sendMessage, abort) are the file's next part.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* the SDK's <new> asks std::size_t of the freestanding headers */
namespace std {
using ::size_t;
}
#include <new>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "SwMwSinkStream.h"
#include "SwMwSync.h"

#define SWMW_STREAM_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwSinkStream.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the SWS core's content manager behind a device (the linked core's ) */
int SwMwCoreContentManager(int device);

/* the reader's event base object (the SWS support library, /) */
void SwMwStreamEventConstruct(struct SwMwStreamEvent *event);
void SwMwStreamEventDestruct(struct SwMwStreamEvent *event);

/* the sink's re-authentication and certificate helpers (SwMwSink.cpp) */
int SwMwSinkReauthenticate(int sink);
void SwMwContentGetInfo(void *certificate, void *block);
int SwMwContentGetStatus(void *certificate);
int SwMwCoreNotifyLabel(int device, int label, int value);

/* the transfer bodies the dispatcher's forwarders call */
int SwMwSinkStreamSetParameterBody(struct SwMwSinkStream *stream, int parameter);
int SwMwSinkStreamReceiveBody(struct SwMwSinkStream *stream, int *offset, int *size,
			      unsigned char *type);
int SwMwSinkStreamReleaseBody(struct SwMwSinkStream *stream, int size);
int SwMwSinkStreamSendBody(struct SwMwSinkStream *stream, int *offset, int *size);
int SwMwSinkStreamSendMessageBody(struct SwMwSinkStream *stream, int message);
int SwMwSinkStreamAbortBody(struct SwMwSinkStream *stream);
}

/* the stream's gate */
extern "C" struct SwMwGate *SwMwSinkStreamGate(struct SwMwSinkStream *stream)
{
	return &stream->gate;
}

/* the lwmutex that guards the transfer bookkeeping */
extern "C" int SwMwSinkStreamMutexCreate(SceKernelLwMutexWork *mutex)
{
	int result;

	result = sceKernelCreateLwMutex(mutex, "sce_sws_mutex", 0x2000, 0, NULL);
	if (result >= 0)
		return 0;
	return result;
}

/* delete the transfer bookkeeping's lwmutex */
extern "C" int SwMwSinkStreamMutexDelete(SceKernelLwMutexWork *mutex)
{
	return sceKernelDeleteLwMutex(mutex);
}

/* the reader's event: its flag, condition variable and mutex */
extern "C" int SwMwSinkStreamEventCreate(struct SwMwSignal *signal)
{
	signal->signalled = 0;
	if (sceKernelCreateLwMutex(&signal->mutex, "sce_sws_event_mutex", 0x2000, 0, NULL) < 0)
		return 11;
	if (sceKernelCreateLwCond(&signal->cond, "sce_sws_event_cond", 0x2000, &signal->mutex,
				  NULL) >= 0)
		return 0;
	sceKernelDeleteLwMutex(&signal->mutex);
	return 11;
}

/* delete the reader's event */
extern "C" int SwMwSinkStreamEventDelete(struct SwMwSignal *signal)
{
	sceKernelDeleteLwCond(&signal->cond);
	sceKernelDeleteLwMutex(&signal->mutex);
	return 0;
}

/* lock and unlock a lwmutex */
extern "C" int SwMwSinkStreamMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* wait for a signal's flag and clear it */
extern "C" int SwMwSinkStreamWait(struct SwMwSignal *signal)
{
	if (sceKernelLockLwMutex(&signal->mutex, 1, NULL) < 0)
		return 22;
	while (signal->signalled == 0) {
		if (sceKernelWaitLwCond(&signal->cond, NULL) < 0) {
			sceKernelUnlockLwMutex(&signal->mutex, 1);
			return 22;
		}
	}
	signal->signalled = 0;
	if (sceKernelUnlockLwMutex(&signal->mutex, 1) != 0)
		return 22;
	return 0;
}

/* set a signal's flag and wake its waiter */
extern "C" int SwMwSinkStreamSignal(struct SwMwSignal *signal)
{
	sceKernelLockLwMutex(&signal->mutex, 1, NULL);
	signal->signalled = 1;
	sceKernelSignalLwCond(&signal->cond);
	sceKernelUnlockLwMutex(&signal->mutex, 1);
	return 0;
}

/* lock and unlock the transfer bookkeeping's mutex */
extern "C" int SwMwSinkStreamStateLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwSinkStreamStateUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* the stream's window, as the transfer commands report it */
extern "C" void SwMwSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window)
{
	window[0] = (int)stream->window.receiveBase;
	window[1] = stream->window.sendSize;
	window[2] = (int)stream->window.sendBase;
	window[3] = stream->window.receiveSize;
	window[4] = stream->window.reserved;
	window[5] = stream->window.blockSize;
}

/* register the reader's callback with its context */
extern "C" int SwMwStreamEventSet(struct SwMwStreamEvent *event, void *handler, void *context,
				  void *argument)
{
	event->context = context;
	event->handler = handler;
	event->argument = argument;
	return (int)argument;
}

/* register a handler with the stream's reader */
extern "C" int SwMwSinkStreamSetHandler(struct SwMwSinkStream *stream, void *handler,
					void *argument)
{
	return SwMwStreamEventSet(&stream->event, handler, stream, argument);
}

/* mark the stream finished with a status, once */
extern "C" int SwMwSinkStreamSetFinished(struct SwMwSinkStream *stream, int status)
{
	SwMwSinkStreamStateLock(&stream->mutex);
	if (stream->finished == 0) {
		stream->finished = 1;
		stream->status = status;
	}
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return 0;
}

/* the stream's open state */
extern "C" int SwMwSinkStreamIsOpen(struct SwMwSinkStream *stream)
{
	SwMwSinkStreamSetFinished(stream, 12015);
	return 0;
}

/* the stream's name */
extern "C" int SwMwSinkStreamSetName(struct SwMwSinkStream *stream, const char *name)
{
	strncpy(stream->name, name, 0xFF);
	return 0;
}

/* a reader hook that has nothing to do */
extern "C" int SwMwSinkStreamNoop(void)
{
	return 0;
}

/* the stream's status while a transfer is outstanding */
extern "C" int SwMwSinkStreamPoll(struct SwMwSinkStream *stream)
{
	int status;

	status = 0;
	SwMwSinkStreamStateLock(&stream->mutex);
	if (stream->finished == 0 && stream->empty == 0)
		status = 12007;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return status;
}

/* build the stream object */
extern "C" struct SwMwSinkStream *SwMwSinkStreamConstruct(struct SwMwSinkStream *stream)
{
	stream->state = 0;
	stream->sink = 0;
	stream->contentId = -1;
	stream->downloader = NULL;
	stream->reader = NULL;
	SwMwStreamEventConstruct(&stream->event);
	stream->manager = NULL;
	stream->receiveChunks = NULL;
	stream->receiveCount = 0;
	stream->receiveIndex = 0;
	stream->receiveConsumed = 0;
	stream->receiveHave = 0;
	stream->sendChunks = NULL;
	stream->sendCount = 0;
	stream->sendIndex = 0;
	stream->sendConsumed = 0;
	stream->sendHave = 0;
	stream->sendTotal = 0;
	stream->sending = 0;
	stream->sendingMessage = 0;
	stream->receiving = 0;
	stream->releasing = 0;
	stream->endOfStream = 0;
	stream->empty = 0;
	stream->finished = 0;
	stream->connected = 0;
	stream->status = 0;
	stream->emptyStatus = 12401;
	stream->resume = 0;
	SwMwGateInit(&stream->gate);
	return stream;
}

/* the constructor's copy form */
extern "C" struct SwMwSinkStream *SwMwSinkStreamConstructCopy(struct SwMwSinkStream *stream)
{
	return SwMwSinkStreamConstruct(stream);
}

/* tear the stream object down */
extern "C" struct SwMwSinkStream *SwMwSinkStreamDestruct(struct SwMwSinkStream *stream)
{
	if (stream->state != 0) {
		stream->state = 0;
		if (SwMwSinkStreamMutexDelete(&stream->mutex) != 0)
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 156);
	}
	SwMwGateDone(&stream->gate);
	SwMwStreamEventDestruct(&stream->event);
	return stream;
}

/* the destructor's copy form */
extern "C" struct SwMwSinkStream *SwMwSinkStreamDestructCopy(struct SwMwSinkStream *stream)
{
	return SwMwSinkStreamDestruct(stream);
}

/* initialise a constructed stream on its sink and window */
extern "C" int SwMwSinkStreamInit(struct SwMwSinkStream *stream, void *sink,
				  const struct SwMwStreamWindow *window)
{
	int status;
	int failed;

	stream->sink = (int)sink;
	stream->window = *window;
	stream->manager = (struct SwMwContentManager *)SwMwCoreContentManager(*(int *)stream->sink);
	memset(stream->connectData, 0, 8);
	status = SwMwSinkStreamMutexCreate(&stream->mutex);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 140);
		failed = 1;
	}
	if (failed != 0)
		return 12009;
	SwMwSinkStreamSetHandler(stream, (void *)SwMwSinkStreamOnEvent, NULL);
	stream->state = 1;
	return 0;
}

/* create the channel's stream */
extern "C" int SwMwSinkStreamCreate(struct SwMwSinkStream **stream, void *sink,
				    const struct SwMwStreamWindow *window)
{
	struct SwMwSinkStream *object = NULL;
	int status;
	int failed;

	object = (struct SwMwSinkStream *)::operator new(sizeof(struct SwMwSinkStream),
							 std::nothrow);
	if (object != NULL)
		object = SwMwSinkStreamConstruct(object);
	failed = 0;
	if (object == NULL) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 205);
		failed = 1;
	}
	if (failed != 0)
		return 12008;
	status = SwMwSinkStreamInit(object, sink, window);
	if (status != 0) {
		if (object != NULL)
			::operator delete(SwMwSinkStreamDestruct(object));
		return status;
	}
	*stream = object;
	return 0;
}

/* the reader's event: 0 asks for re-authentication, 1 reports a finished move */
extern "C" int SwMwSinkStreamOnEvent(struct SwMwSinkStream *stream, int *message)
{
	int result;

	result = message[0];
	if (result > 0) {
		if (result <= 1) {
			result = SwMwSinkStreamSetFinished(stream, 12402);
			if (result != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_STREAM_SOURCE, 187);
				return 1;
			}
		}
	} else if (result >= 0) {
		result = SwMwSinkStreamReauthenticate(stream);
		if (result != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_STREAM_SOURCE, 183);
			return 1;
		}
	}
	return result;
}

/* the reader reported that its key is about to expire */
extern "C" void SwMwSinkStreamOnKeyExpired(struct SwMwStreamEvent *event)
{
	int message = 0;

	swMwTrace("sw mw:KeyBeingExpired\n");
	if (event->handler != NULL)
		((void (*)(void *, int *, void *))event->handler)(event->context, &message,
								  event->argument);
}

/* the reader asked for the stream to be discarded */
extern "C" void SwMwSinkStreamOnDiscard(struct SwMwStreamEvent *event, int reason)
{
	unsigned char byte;
	int message[3];

	message[2] = reason;
	byte = (unsigned char)reason;
	swMwTrace("sw mw:TransitToDiscardStream\n");
	message[0] = 1;
	message[1] = byte;
	if (event->handler != NULL)
		((void (*)(void *, int *, void *))event->handler)(event->context, message,
								  event->argument);
}

/* the reader reported the content's EEMI type */
extern "C" void SwMwSinkStreamOnEemi(struct SwMwStreamEvent *event, int type, int value)
{
	int message[3];

	message[2] = value;
	event->eemi = (unsigned char)type;
	if (type == 6) {
		swMwTrace("sw mw:detected EEMI is C1(Move)\n");
		swMwTrace("sw mw:=> discard stream\n");
		if (event->handler != NULL) {
			message[0] = 1;
			message[1] = 1;
			((void (*)(void *, int *, void *))event->handler)(event->context, message,
									  event->argument);
		}
	}
}

/* re-authenticate the stream's sink */
extern "C" int SwMwSinkStreamReauthenticate(struct SwMwSinkStream *stream)
{
	int status;

	swMwTrace("sw mw:KX will be expired within 60 sec\n");
	swMwTrace("sw mw:try to authenticate again\n");
	status = SwMwSinkReauthenticate(stream->sink);
	return status;
}

/* the periodic connect: re-authenticate when the reader's key is expiring */
extern "C" int SwMwSinkStreamConnect(struct SwMwSinkStream *stream)
{
	int status;

	if (stream->connected == 0 || stream->downloader == NULL)
		return 0;
	if ((stream->downloader->isKeyExpiring() & 1) == 0)
		return 0;
	status = SwMwSinkStreamReauthenticate(stream);
	if (status != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_STREAM_SOURCE, 1023);
	return 0;
}

/* the transfer forwarders the dispatcher calls: the file's bodies do the work */
extern "C" int SwMwSinkStreamSetParameter(struct SwMwSinkStream *stream, int parameter)
{
	return SwMwSinkStreamSetParameterBody(stream, parameter);
}

extern "C" int SwMwSinkStreamReceive(struct SwMwSinkStream *stream, int *offset, int *size,
				     unsigned char *type)
{
	return SwMwSinkStreamReceiveBody(stream, offset, size, type);
}

extern "C" int SwMwSinkStreamRelease(struct SwMwSinkStream *stream, int size)
{
	return SwMwSinkStreamReleaseBody(stream, size);
}

extern "C" int SwMwSinkStreamSend(struct SwMwSinkStream *stream, int *offset, int *size)
{
	return SwMwSinkStreamSendBody(stream, offset, size);
}

extern "C" int SwMwSinkStreamSendMessage(struct SwMwSinkStream *stream, int message)
{
	return SwMwSinkStreamSendMessageBody(stream, message);
}

extern "C" int SwMwSinkStreamAbort(struct SwMwSinkStream *stream)
{
	return SwMwSinkStreamAbortBody(stream);
}

/* stream event vtable and lifecycle helpers */
extern "C" void swMwStreamEventNoop(void) {}

static void swMwStreamEventDelete(struct SwMwStreamEvent *event)
{
	SwMwStreamEventDestruct(event);
	::operator delete(event);
}

static void *g_swMwStreamEventVtable[8] = {
	(void *)SwMwStreamEventDestruct,
	(void *)swMwStreamEventDelete,
	(void *)swMwStreamEventNoop,
	(void *)swMwStreamEventNoop,
	(void *)swMwStreamEventNoop,
	(void *)SwMwSinkStreamOnEemi,
	(void *)SwMwSinkStreamOnDiscard,
	(void *)SwMwSinkStreamOnKeyExpired,
};

extern "C" void SwMwStreamEventConstruct(struct SwMwStreamEvent *event)
{
	event->vtable = g_swMwStreamEventVtable;
	event->context = NULL;
	event->handler = NULL;
	event->argument = NULL;
	event->eemi = 0xFF;
}

extern "C" void SwMwStreamEventDestruct(struct SwMwStreamEvent *event)
{
	event->vtable = NULL;
}

extern "C" int SwMwStreamEventType(const struct SwMwStreamEvent *event)
{
	return (int)event->eemi;
}

/* destroy the sink stream and release its downloader */
extern "C" int SwMwSinkStreamDestroy(struct SwMwSinkStream *stream)
{
	if (stream == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_STREAM_SOURCE, 225);
	if (stream != NULL) {
		if (stream->downloader != NULL) {
			struct SwMwTransfer *iface =
				(struct SwMwTransfer *)((unsigned char *)stream->downloader +
				(*(int **)(void *)stream->downloader)[-13]);
			stream->manager->releaseTransfer(iface);
		}
		::operator delete(SwMwSinkStreamDestruct(stream));
	}
	return 0;
}

/* start the sink stream on the given content info descriptor */
struct SwMwSinkStreamInfo {
	unsigned int contentId;		/* 0x00 */
	unsigned int resume;		/* 0x04 */
	unsigned int modeByte;		/* 0x08 */
	unsigned int hasDirectBlock;	/* 0x0C */
	unsigned char sender[16];	/* 0x10 */
	unsigned char receiver[16];	/* 0x20 */
	unsigned int length;		/* 0x30 */
	unsigned int certificate;	/* 0x34 */
};

struct SwMwSinkCertBlock {
	unsigned char sender[16];	/* 0x00 */
	unsigned char receiver[16];	/* 0x10 */
	unsigned int length;		/* 0x20 */
	unsigned char certificate[32];	/* 0x24 */
};

extern "C" int SwMwSinkStreamGetInfo(struct SwMwSinkStream *stream, void *infoPtr)
{
	struct SwMwSinkStreamInfo *info = (struct SwMwSinkStreamInfo *)infoPtr;
	struct SwMwSinkCertBlock block;
	int status;
	int started;
	int stage;

	if (SwMwCoreNotifyLabel(*(int *)stream->sink, (int)info->contentId, 33) != 0)
		return 12007;

	stream->contentId = (int)info->contentId;
	stream->receiveChunks = NULL;
	stream->receiveCount = 0;
	stream->receiveIndex = 0;
	stream->receiveConsumed = 0;
	stream->receiveHave = 0;
	stream->sendChunks = NULL;
	stream->sendCount = 0;
	stream->sendIndex = 0;
	stream->sendConsumed = 0;
	stream->sendHave = 0;
	stream->sendTotal = 0;
	stream->finished = 0;
	stream->status = 0;
	stream->emptyStatus = 12401;
	stream->endOfStream = 0;
	stream->empty = 0;
	memset(&stream->work, 0, sizeof(stream->work));
	stream->work.tag = 0x90020001u;
	stream->work.eof = (stream->window.reserved == 0) ? 1 : 0;
	if (info->certificate != 0) {
		SwMwContentGetInfo((void *)(uintptr_t)info->certificate, &block);
		memcpy(stream->work.sender, block.sender, 16);
		memcpy(stream->work.receiver, block.receiver, 16);
		stream->work.length = block.length >> 4;
		memcpy(stream->work.certificate, block.certificate, 32);
		stream->work.reserved[0] = (unsigned char)info->modeByte;
		stream->work.keyId = SwMwContentGetStatus((void *)(uintptr_t)info->certificate);
	} else {
		if (info->hasDirectBlock != 0) {
			memcpy(stream->work.sender, info->sender, 16);
			memcpy(stream->work.receiver, info->receiver, 16);
			stream->work.length = info->length >> 4;
		} else {
			stream->work.length = 0;
		}
		stream->work.certificate[3] = (unsigned char)info->modeByte;
	}
	stream->work.base = stream->window.receiveBase;
	stream->work.sendSize = stream->window.sendSize;
	swMwTrace("sw mw:SinkStreamStart bufSyncLen=%d\n", stream->window.blockSize);
	stream->work.blockSize = stream->window.blockSize;
	swMwTrace("sw mw:SinkStreamStart contentID=%d\n", stream->contentId);

	started = stream->manager->createTransfer(
		&stream->downloader,
		(struct SwMwUri *)((unsigned char *)(uintptr_t)stream->sink + 20),
		0, &stream->event, stream->window.receiveSize, &stream->work,
		(int)(uintptr_t)stream->window.sendBase);
	stage = 0;
	if (started != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 392);
		stage = 1;
	}
	if (stage == 0) {
		struct SwMwTransfer *base =
			(struct SwMwTransfer *)((unsigned char *)stream->downloader +
			(*(int **)(void *)stream->downloader)[-10]);
		if (base->setParameter(20) != 0)
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 398);
		if (info->resume != 0 && stream->resume != 0) {
			stream->downloader->setSessionData(&stream->connectData[0]);
			stream->resume = 0;
		}
		started = base->start();
		if (started != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 407);
			stage = 1;
		}
	}
	status = 0;
	if (stage == 0) {
		stream->reader = stream->work.reader;
		if (stream->reader == NULL) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 412);
			status = 12004;
			stage = 1;
		}
	}
	if (stage == 0) {
		if (stream->reader->noop(20) != 0)
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 420);
		stream->connected = 1;
		return 0;
	}
	if (stream->downloader != NULL) {
		struct SwMwTransfer *iface =
			(struct SwMwTransfer *)((unsigned char *)stream->downloader +
			(*(int **)(void *)stream->downloader)[-13]);
		stream->manager->releaseTransfer(iface);
		stream->downloader = NULL;
	}
	stream->reader = NULL;
	if (status != 0)
		return status;
	return SwMwStatus(started);
}

/* signal end-of-stream on the sink stream */
extern "C" int SwMwSinkStreamAbortBody(struct SwMwSinkStream *stream)
{
	SwMwSinkStreamStateLock(&stream->mutex);
	if (stream->endOfStream != 0) {
		SwMwSinkStreamStateUnlock(&stream->mutex);
		return 0;
	}
	if (stream->sending != 0 || stream->sendingMessage != 0) {
		SwMwSinkStreamStateUnlock(&stream->mutex);
		return 12007;
	}
	if (stream->downloader == NULL) {
		SwMwSinkStreamStateUnlock(&stream->mutex);
		return 12004;
	}
	stream->endOfStream = 1;
	if (stream->sendHave != 0) {
		stream->sendHave = 0;
		stream->downloader->release(stream->sendConsumed);
		stream->sendTotal += stream->sendConsumed;
	}
	swMwTrace("sw mw:SinkStreamSetEOS: %d bytes written\n", stream->sendTotal);
	((struct SwMwTransfer *)((unsigned char *)stream->downloader +
	 (*(int **)(void *)stream->downloader)[-10]))->reset();
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return 0;
}

/* stop and tear down the active sink stream transfer */
extern "C" int SwMwSinkStreamSetParameterBody(struct SwMwSinkStream *stream, int parameter)
{
	int status;

	if (stream->empty == 0) {
		int finStatus = SwMwSinkStreamSetFinished(stream, 12015);
		if (finStatus != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", finStatus, SWMW_STREAM_SOURCE, 500);
	}
	status = SwMwSinkStreamAbortBody(stream);
	if (status == 12007)
		return status;

	SwMwSinkStreamStateLock(&stream->mutex);
	if (stream->downloader != NULL) {
		if (stream->receiving != 0 || stream->releasing != 0) {
			status = 12007;
		} else {
			int relStatus;
			struct SwMwTransfer *iface;

			if (parameter == 0) {
				if (stream->receiveHave != 0) {
					int total = 0;
					int i;
					stream->receiveHave = 0;
					for (i = 0; i < stream->receiveCount; ++i)
						total += stream->receiveChunks[i].size;
					stream->reader->consume((unsigned int)total);
				}
				if (stream->reader->isEndOfStream() == 0 ||
				    stream->reader->available() != 0) {
					struct SwMwStreamChunk *chunks = NULL;
					int count = stream->reader->getReadChunkList((void **)&chunks);
					if (count != -1) {
						int total = 0;
						int i;
						for (i = 0; i < count; ++i)
							total += chunks[i].size;
						stream->reader->consume((unsigned int)total);
						SwMwSinkStreamStateUnlock(&stream->mutex);
						return 12007;
					}
				}
				if (stream->downloader->getSessionData((int *)(void *)&stream->connectData[0]) == 0)
					stream->resume = 1;
			}
			iface = (struct SwMwTransfer *)((unsigned char *)stream->downloader +
				(*(int **)(void *)stream->downloader)[-13]);
			relStatus = stream->manager->releaseTransfer(iface);
			if (relStatus != 0)
				swMwTrace("[SWMW] Error %d in %s:%d\n", relStatus,
					  SWMW_STREAM_SOURCE, 555);
			stream->downloader = NULL;
			stream->reader = NULL;
			stream->connected = 0;
			status = SwMwCoreNotifyLabel(*(int *)stream->sink, stream->contentId,
						     stream->empty != 0 ? 35 : 34);
		}
	}
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return status;
}

/* obtain the next readable chunk from the sink stream */
extern "C" int SwMwSinkStreamReceiveBody(struct SwMwSinkStream *stream, int *offset, int *size,
					 unsigned char *type)
{
	int finished;
	int status;

	*type = 0xFF;
	SwMwSinkStreamStateLock(&stream->mutex);
	stream->receiving = 1;
	finished = stream->finished;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_STREAM_SOURCE, 627);
		*size = 0;
		*offset = 0;
		SwMwSinkStreamStateLock(&stream->mutex);
		stream->receiving = 0;
		SwMwSinkStreamStateUnlock(&stream->mutex);
		return status;
	}
	if (stream->reader == NULL) {
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 635);
		*size = 0;
		*offset = 0;
		SwMwSinkStreamStateLock(&stream->mutex);
		stream->receiving = 0;
		SwMwSinkStreamStateUnlock(&stream->mutex);
		return 12004;
	}
	if (stream->receiveHave == 0) {
		int chunkSize = 0;

		stream->receiveCount = stream->reader->getReadChunkList(
			(void **)&stream->receiveChunks);
		if (stream->receiveCount == -1) {
			*size = 0;
			stream->reader->consume(0);
			stream->empty = 1;
			status = stream->emptyStatus;
			SwMwSinkStreamStateLock(&stream->mutex);
			stream->receiving = 0;
			SwMwSinkStreamStateUnlock(&stream->mutex);
			return status;
		}
		if (stream->receiveCount == 0) {
			stream->reader->consume(0);
			*size = 0;
			*offset = 0;
			*type = (unsigned char)SwMwStreamEventType(&stream->event);
			SwMwSinkStreamStateLock(&stream->mutex);
			stream->receiving = 0;
			SwMwSinkStreamStateUnlock(&stream->mutex);
			return 0;
		}
		stream->receiveIndex = 0;
		stream->receiveConsumed = 0;
		while (stream->receiveIndex < stream->receiveCount) {
			chunkSize = stream->receiveChunks[stream->receiveIndex].size;
			if (chunkSize != 0)
				break;
			++stream->receiveIndex;
		}
		if (chunkSize == 0) {
			stream->reader->consume(0);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12014, SWMW_STREAM_SOURCE, 676);
			SwMwSinkStreamStateLock(&stream->mutex);
			stream->receiving = 0;
			SwMwSinkStreamStateUnlock(&stream->mutex);
			return 12014;
		}
		stream->receiveHave = 1;
	}
	*offset = stream->receiveChunks[stream->receiveIndex].offset + stream->receiveConsumed;
	*size = stream->receiveChunks[stream->receiveIndex].size - stream->receiveConsumed;
	if (*size == 0)
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 686);
	*type = (unsigned char)SwMwStreamEventType(&stream->event);
	SwMwSinkStreamStateLock(&stream->mutex);
	stream->receiving = 0;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return 0;
}

/* release consumed bytes back to the sink stream reader */
extern "C" int SwMwSinkStreamReleaseBody(struct SwMwSinkStream *stream, int size)
{
	int finished;
	int status = 0;

	SwMwSinkStreamStateLock(&stream->mutex);
	stream->releasing = 1;
	finished = stream->finished;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_STREAM_SOURCE, 720);
	} else if (stream->reader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 726);
	} else if (stream->receiveHave != 0) {
		unsigned int chunkLen = (unsigned int)stream->receiveChunks[stream->receiveIndex].size;
		stream->receiveConsumed += size;
		if ((unsigned int)stream->receiveConsumed >= chunkLen) {
			++stream->receiveIndex;
			while (stream->receiveIndex < stream->receiveCount &&
			       stream->receiveChunks[stream->receiveIndex].size == 0)
				++stream->receiveIndex;
			if (stream->receiveIndex >= stream->receiveCount) {
				int total = 0;
				int i;
				stream->receiveHave = 0;
				for (i = 0; i < stream->receiveCount; ++i)
					total += stream->receiveChunks[i].size;
				if ((stream->reader->consume((unsigned int)total) & 0xFF) == 0) {
					swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE,
						  753);
					status = 12003;
				}
			} else {
				stream->receiveConsumed = 0;
			}
		}
	}
	SwMwSinkStreamStateLock(&stream->mutex);
	stream->releasing = 0;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return status;
}

/* obtain a writable buffer chunk for incoming encrypted stream data */
extern "C" int SwMwSinkStreamSendBody(struct SwMwSinkStream *stream, int *offset, int *size)
{
	int finished;
	int eos;
	int status = 0;

	SwMwSinkStreamStateLock(&stream->mutex);
	stream->sending = 1;
	finished = stream->finished;
	eos = stream->endOfStream;
	SwMwSinkStreamStateUnlock(&stream->mutex);

	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_STREAM_SOURCE, 794);
		*size = 0;
		*offset = 0;
	} else if (eos != 0) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 802);
		*size = 0;
		*offset = 0;
	} else if (stream->downloader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 810);
		*size = 0;
		*offset = 0;
	} else {
		if (stream->sendHave == 0) {
			int ret = stream->downloader->getChunkList(
				(void **)&stream->sendChunks, &stream->sendCount);
			if (ret != 0) {
				*size = 0;
				status = (ret == 102) ? 12401 : SwMwStatus(ret);
				if (status != 0)
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_STREAM_SOURCE, 827);
			} else if (stream->sendCount == 0) {
				stream->downloader->release(0);
				*size = 0;
				*offset = 0;
				SwMwSinkStreamStateLock(&stream->mutex);
				stream->sending = 0;
				SwMwSinkStreamStateUnlock(&stream->mutex);
				return 0;
			} else {
				int chunkSize = 0;
				stream->sendIndex = 0;
				stream->sendConsumed = 0;
				while (stream->sendIndex < stream->sendCount) {
					chunkSize = stream->sendChunks[stream->sendIndex].size;
					if (chunkSize != 0)
						break;
					++stream->sendIndex;
				}
				if (chunkSize == 0) {
					stream->downloader->release(0);
					status = 12014;
					swMwTrace("[SWMW] Error %d in %s:%d\n", 12014,
						  SWMW_STREAM_SOURCE, 852);
				} else {
					stream->sendHave = 1;
				}
			}
		}
		if (status == 0) {
			*offset = stream->sendChunks[stream->sendIndex].offset +
				  stream->sendConsumed;
			*size = stream->sendChunks[stream->sendIndex].size - stream->sendConsumed;
			if (*size == 0)
				swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 862);
			SwMwSinkStreamStateLock(&stream->mutex);
			stream->sending = 0;
			SwMwSinkStreamStateUnlock(&stream->mutex);
			return 0;
		}
	}

	if ((status >= 11008 && status <= 11012) || status == 1005 || status == 1016)
		stream->emptyStatus = status;
	SwMwSinkStreamStateLock(&stream->mutex);
	stream->sending = 0;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return status;
}

/* commit written bytes to the downloader */
extern "C" int SwMwSinkStreamSendMessageBody(struct SwMwSinkStream *stream, int size)
{
	int eos;
	int finished;
	int status = 0;

	SwMwSinkStreamStateLock(&stream->mutex);
	stream->sendingMessage = 1;
	eos = stream->endOfStream;
	finished = stream->finished;
	SwMwSinkStreamStateUnlock(&stream->mutex);

	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_STREAM_SOURCE, 909);
	} else if (eos != 0) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 915);
	} else if (stream->downloader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_STREAM_SOURCE, 921);
	} else if (stream->sendHave != 0) {
		int ret;
		stream->sendConsumed += size;
		stream->sendHave = 0;
		ret = stream->downloader->release(stream->sendConsumed);
		stream->sendTotal += stream->sendConsumed;
		if (ret != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_STREAM_SOURCE, 931);
			status = 12003;
		}
	}
	SwMwSinkStreamStateLock(&stream->mutex);
	stream->sendingMessage = 0;
	SwMwSinkStreamStateUnlock(&stream->mutex);
	return status;
}
