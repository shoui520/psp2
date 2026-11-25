/*
 * The sink's stream object (see SwMwSinkStream.h): its construction, the lwmutex and
 * event primitives its reader uses, the transfer bookkeeping the receive/send commands
 * share, its gate and its reader callbacks. The transfer bodies themselves (getInfo,
 * setParameter, receive, release, send, sendMessage, abort) are the file's next part.
 */

#include <stddef.h>
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

/* the SWS core's content manager behind a device (the linked core's 0x81002ce8) */
int SwMwCoreContentManager(int device);

/* the reader's event base object (the SWS support library, 0x8107ba54/0x8107ba88) */
void SwMwStreamEventConstruct(struct SwMwStreamEvent *event);
void SwMwStreamEventDestruct(struct SwMwStreamEvent *event);

/* the sink's re-authentication (SwMwSink.cpp, 0x8100ccaa) */
int SwMwSinkReauthenticate(int sink);

/* the transfer bodies the dispatcher's forwarders call */
int SwMwSinkStreamSetParameterBody(struct SwMwSinkStream *stream, int parameter);
int SwMwSinkStreamReceiveBody(struct SwMwSinkStream *stream, int *offset, int *size,
			      unsigned char *type);
int SwMwSinkStreamReleaseBody(struct SwMwSinkStream *stream, int size);
int SwMwSinkStreamSendBody(struct SwMwSinkStream *stream, int *offset, int *size);
int SwMwSinkStreamSendMessageBody(struct SwMwSinkStream *stream, int message);
int SwMwSinkStreamAbortBody(struct SwMwSinkStream *stream);
}

/* the stream's gate (0x8100fe54) */
extern "C" struct SwMwGate *SwMwSinkStreamGate(struct SwMwSinkStream *stream)
{
	return &stream->gate;
}

/* the lwmutex that guards the transfer bookkeeping (0x8100ffbe) */
extern "C" int SwMwSinkStreamMutexCreate(SceKernelLwMutexWork *mutex)
{
	int result;

	result = sceKernelCreateLwMutex(mutex, "sce_sws_mutex", 0x2000, 0, NULL);
	if (result >= 0)
		return 0;
	return result;
}

/* delete the transfer bookkeeping's lwmutex (0x8101026a) */
extern "C" int SwMwSinkStreamMutexDelete(SceKernelLwMutexWork *mutex)
{
	return sceKernelDeleteLwMutex(mutex);
}

/* the reader's event: its flag, condition variable and mutex (0x810100ae) */
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

/* delete the reader's event (0x81010284) */
extern "C" int SwMwSinkStreamEventDelete(struct SwMwSignal *signal)
{
	sceKernelDeleteLwCond(&signal->cond);
	sceKernelDeleteLwMutex(&signal->mutex);
	return 0;
}

/* lock and unlock a lwmutex (0x8100e7de/0x8100e800) */
extern "C" int SwMwSinkStreamMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* wait for a signal's flag and clear it (0x8100e81e) */
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

/* set a signal's flag and wake its waiter (0x8100e8a8) */
extern "C" int SwMwSinkStreamSignal(struct SwMwSignal *signal)
{
	sceKernelLockLwMutex(&signal->mutex, 1, NULL);
	signal->signalled = 1;
	sceKernelSignalLwCond(&signal->cond);
	sceKernelUnlockLwMutex(&signal->mutex, 1);
	return 0;
}

/* lock and unlock the transfer bookkeeping's mutex (0x8100ea46/0x8100ea68) */
extern "C" int SwMwSinkStreamStateLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwSinkStreamStateUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* the stream's window, as the transfer commands report it (0x8100e8e4) */
extern "C" void SwMwSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window)
{
	window[0] = (int)stream->window.receiveBase;
	window[1] = stream->window.sendSize;
	window[2] = (int)stream->window.sendBase;
	window[3] = stream->window.receiveSize;
	window[4] = stream->window.reserved;
	window[5] = stream->window.blockSize;
}

/* register the reader's callback with its context (0x8100fe66) */
extern "C" int SwMwStreamEventSet(struct SwMwStreamEvent *event, void *handler, void *context,
				  void *argument)
{
	event->context = context;
	event->handler = handler;
	event->argument = argument;
	return (int)argument;
}

/* register a handler with the stream's reader (0x8100ea24) */
extern "C" int SwMwSinkStreamSetHandler(struct SwMwSinkStream *stream, void *handler,
					void *argument)
{
	return SwMwStreamEventSet(&stream->event, handler, stream, argument);
}

/* mark the stream finished with a status, once (0x8100ea86) */
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

/* the stream's open state (0x8100eac4) */
extern "C" int SwMwSinkStreamIsOpen(struct SwMwSinkStream *stream)
{
	SwMwSinkStreamSetFinished(stream, 12015);
	return 0;
}

/* the stream's name (0x8100eae0) */
extern "C" int SwMwSinkStreamSetName(struct SwMwSinkStream *stream, const char *name)
{
	strncpy(stream->name, name, 0xFF);
	return 0;
}

/* a reader hook that has nothing to do (0x8100eb04) */
extern "C" int SwMwSinkStreamNoop(void)
{
	return 0;
}

/* the stream's status while a transfer is outstanding (0x8100f316) */
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

/* build the stream object (0x8101012c) */
extern "C" struct SwMwSinkStream *SwMwSinkStreamConstruct(struct SwMwSinkStream *stream)
{
	stream->state = 0;
	stream->sink = 0;
	stream->contentId = -1;
	stream->downloader = NULL;
	stream->reader = NULL;
	SwMwStreamEventConstruct(&stream->event);
	stream->content = NULL;
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
	stream->parameterBusy = 0;
	stream->aborted = 0;
	stream->empty = 0;
	stream->finished = 0;
	stream->connected = 0;
	stream->status = 0;
	stream->emptyStatus = 12401;
	stream->resume = 0;
	SwMwGateInit(&stream->gate);
	return stream;
}

/* the constructor's copy form (0x8101025a) */
extern "C" struct SwMwSinkStream *SwMwSinkStreamConstructCopy(struct SwMwSinkStream *stream)
{
	return SwMwSinkStreamConstruct(stream);
}

/* tear the stream object down (0x810102a2) */
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

/* the destructor's copy form (0x810102fc) */
extern "C" struct SwMwSinkStream *SwMwSinkStreamDestructCopy(struct SwMwSinkStream *stream)
{
	return SwMwSinkStreamDestruct(stream);
}

/* initialise a constructed stream on its sink and window (0x8100fffa) */
extern "C" int SwMwSinkStreamInit(struct SwMwSinkStream *stream, void *sink,
				  const struct SwMwStreamWindow *window)
{
	int status;
	int failed;

	stream->sink = (int)sink;
	stream->window = *window;
	stream->content = (void *)SwMwCoreContentManager(*(int *)stream->sink);
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

/* create the channel's stream (0x8100e912) */
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

/* the reader's event: 0 asks for re-authentication, 1 reports a finished move (0x8101030c) */
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

/* the reader reported that its key is about to expire (0x8100fe86) */
extern "C" void SwMwSinkStreamOnKeyExpired(struct SwMwStreamEvent *event)
{
	int message = 0;

	swMwTrace("sw mw:KeyBeingExpired\n");
	if (event->handler != NULL)
		((void (*)(void *, int *, void *))event->handler)(event->context, &message,
								  event->argument);
}

/* the reader asked for the stream to be discarded (0x8100feda) */
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

/* the reader reported the content's EEMI type (0x8100ff3c) */
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

/* re-authenticate the stream's sink (0x8100fdc6) */
extern "C" int SwMwSinkStreamReauthenticate(struct SwMwSinkStream *stream)
{
	int status;

	swMwTrace("sw mw:KX will be expired within 60 sec\n");
	swMwTrace("sw mw:try to authenticate again\n");
	status = SwMwSinkReauthenticate(stream->sink);
	return status;
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
