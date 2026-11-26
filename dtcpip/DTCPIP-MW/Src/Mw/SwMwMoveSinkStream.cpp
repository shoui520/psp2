/*
 * The move sink's stream (see SwMwMoveSinkStream.h): the window copy, the bookkeeping
 * lock and the "finished" flag its commands share, the open state, the name, the poll
 * and the gate its commands serialize on.
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
#include "SwMwMoveSinkStream.h"

#define SWMW_MOVESTREAM_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMoveSinkStream.cpp"

extern "C" {
int swMwTrace(const char *format, ...);

/* the move stream's event base (the SWS support library, 0x8107b768/0x8107b840) */
void SwMwMoveStreamEventConstruct(struct SwMwStreamEvent *event);
void SwMwMoveStreamEventDestruct(struct SwMwStreamEvent *event);

/* the SWS core's content manager behind a device (0x81002ce8) */
int SwMwCoreContentManager(int device);

/* the stream's bookkeeping mutex (SwMwSync.cpp's shared primitive, 0x8100c200/c4b6) */
int SwMwSinkStreamMutexDelete(SceKernelLwMutexWork *mutex);

/* the move session's record list and the device label (SwMwMoveSink.cpp's calls) */
int SwMwMoveRecordBegin(int manager, int id, int mode);
int SwMwMoveRecordAppend(int manager, struct SwMwMoveRecord *record);
int SwMwMoveListRelease(int manager, int id);
int SwMwCoreNotifyLabel(int device, int label, int value);

/* the shared status mapper (SwMwContent.h) */
int SwMwStatus(int status);
}

/* build the move stream object (0x8100c36e) */
extern "C" struct SwMwSinkStream *SwMwMoveSinkStreamConstruct(struct SwMwSinkStream *stream)
{
	stream->state = 0;
	stream->sink = 0;
	stream->contentId = -1;
	stream->downloader = NULL;
	stream->reader = NULL;
	SwMwMoveStreamEventConstruct(&stream->event);
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
	stream->eventArgument = 0;
	SwMwGateInit(&stream->gate);
	return stream;
}

/* the stream's window, as the transfer commands report it (0x8100a190) */
extern "C" int SwMwMoveSinkStreamGetWindow(struct SwMwSinkStream *stream, int *window)
{
	window[0] = stream->window.value[0];
	window[1] = stream->window.value[1];
	window[2] = stream->window.value[2];
	window[3] = stream->window.value[3];
	window[4] = stream->window.value[4];
	window[5] = stream->window.value[5];
	return stream->window.value[5];
}

/* lock and unlock the transfer bookkeeping (0x8100a348/0x8100a36a) */
extern "C" int SwMwMoveSinkStreamMutexLock(SceKernelLwMutexWork *mutex)
{
	return sceKernelLockLwMutex(mutex, 1, NULL);
}

extern "C" int SwMwMoveSinkStreamMutexUnlock(SceKernelLwMutexWork *mutex)
{
	return sceKernelUnlockLwMutex(mutex, 1);
}

/* mark the stream finished with a status, once (0x8100a388) */
extern "C" int SwMwMoveSinkStreamSetFinished(struct SwMwSinkStream *stream, int status)
{
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->finished == 0) {
		stream->finished = 1;
		stream->status = status;
	}
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return 0;
}

/* the stream's open state: it finishes the transfer (0x8100a3c6) */
extern "C" int SwMwMoveSinkStreamIsOpen(struct SwMwSinkStream *stream)
{
	SwMwMoveSinkStreamSetFinished(stream, 12015);
	return 0;
}

/* the stream's name (0x8100a3e2) */
extern "C" int SwMwMoveSinkStreamSetName(struct SwMwSinkStream *stream, const char *name)
{
	strncpy(stream->name, name, 0xFF);
	return 0;
}

/* a hook that has nothing to do (0x8100a406) */
extern "C" int SwMwMoveSinkStreamNoop(void)
{
	return 0;
}

/* the transfer's state while a command is outstanding (0x8100acb6) */
extern "C" int SwMwMoveSinkStreamPoll(struct SwMwSinkStream *stream)
{
	int status;

	status = 0;
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->finished == 0 && stream->empty == 0)
		status = 12007;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* end the transfer: release its pending bytes and reset it (0x8100a936) */
extern "C" int SwMwMoveSinkStreamAbort(struct SwMwSinkStream *stream)
{
	int status;

	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->endOfStream != 0) {
		status = 0;
	} else if (stream->sending != 0 || stream->sendingMessage != 0) {
		status = 12007;
	} else if (stream->downloader == NULL) {
		status = 12004;
	} else {
		stream->endOfStream = 1;
		if (stream->sendHave != 0) {
			stream->sendHave = 0;
			stream->downloader->release(stream->sendConsumed);
			stream->sendTotal += stream->sendConsumed;
		}
		swMwTrace("sw mw:MoveSinkStreamSetEOS: %d bytes written\n", stream->sendTotal);
		((struct SwMwTransfer *)((unsigned char *)stream->downloader +
					  (*(int **)(void *)stream->downloader)[-10]))->reset();
		status = 0;
	}
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* destroy the move stream: release its transfer, flush the sink's move and free it (0x8100a252) */
extern "C" int SwMwMoveSinkStreamDestroy(struct SwMwSinkStream *stream)
{
	struct SwMwMoveSink *sink;
	int status;

	if (stream == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 225);
	if (stream != NULL) {
		sink = (struct SwMwMoveSink *)stream->sink;
		if (stream->downloader != NULL)
			stream->manager->releaseTransfer(
				(struct SwMwTransfer *)((unsigned char *)stream->downloader +
					(*(int **)(void *)stream->downloader)[-13]));
		if (sink->result != 0) {
			status = sink->content->operation(&sink->uri, &sink->result);
			sink->result = 0;
			if (status != 0)
				swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 239);
		}
		::operator delete(SwMwMoveSinkStreamDestruct(stream));
	}
	return 0;
}

/* the move stream's two-phase MV_COMPLETE over the sink (0x8100ba4e) */
extern "C" int SwMwMoveSinkStreamComplete(struct SwMwSinkStream *stream)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	int value;
	int status;

	if (sink == NULL || sink->content == NULL)
		return 12004;
	if (sink->finalized == 0)
		return 12004;
	SwMwMoveSinkStreamMutexLock(&sink->mutex);
	if (stream->finished != 0) {
		sink->finalized = 0;
		sink->sessionState[0] = 0;
		sink->sessionState[1] = 0;
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return 12015;
	}
	if (sink->sessionState[0] == 0) {
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return 12007;
	}
	value = sink->sessionState[1];
	if (value != 0) {
		if (value == 1033) {
			sink->finalized = 0;
			sink->sessionState[0] = 0;
			sink->sessionState[1] = 0;
		} else if (value == 1019) {
			sink->content->abort(&sink->uri, (unsigned char)sink->moveState);
			sink->moveState = -1;
			sink->content->operation(&sink->uri, &sink->result);
			sink->result = 0;
			SwMwCoreNotifyLabel(sink->device, sink->sessionState[6], 41);
			SwMwMoveListRelease(sink->manager, sink->sessionState[5]);
		}
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return value;
	}
	SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
	if (sink->sessionState[2] == 0) {
		status = SwMwMoveRecordBegin(sink->manager, sink->sessionState[5], 2);
		if (status != 0)
			return status;
		swMwTrace("sw mw:Send MV_COMPLETE: kxm_label=0x%x contentID=%d\n",
			  sink->moveState, sink->sessionState[6]);
		status = sink->content->complete(&sink->uri, (unsigned char)sink->moveState);
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1263);
			return SwMwStatus(status);
		}
		sink->sessionState[2] = 1;
	}
	SwMwMoveSinkStreamMutexLock(&sink->mutex);
	if (sink->sessionState[3] == 0) {
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return 12007;
	}
	value = sink->sessionState[4];
	if (value != 0) {
		if (value == 1033) {
			sink->sessionState[2] = 0;
			sink->sessionState[3] = 0;
			sink->sessionState[4] = 0;
		}
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return value;
	}
	SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
	sink->content->abort(&sink->uri, (unsigned char)sink->moveState);
	sink->moveState = -1;
	status = sink->content->operation(&sink->uri, &sink->result);
	sink->result = 0;
	if (status != 0)
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1296);
	status = SwMwMoveListRelease(sink->manager, sink->sessionState[5]);
	if (status != 0)
		return status;
	sink->sessionState[5] = -1;
	return 0;
}

/* wait for the move to close: finalize once, then complete (0x8100b7fc) */
extern "C" int SwMwMoveSinkStreamWaitClosed(struct SwMwSinkStream *stream)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	struct SwMwMoveMessage message;
	struct SwMwMoveRecord record;
	int status;
	int i;

	SwMwMoveMessageInit(&message);
	memset(&record, 0, sizeof(record));
	if (sink->moveState == -1) {
		SwMwMoveMessageDone(&message);
		return 12003;
	}
	if (sink->moveResult != 0)
		stream->finished = 1;
	if (stream->finished != 0) {
		SwMwMoveMessageDone(&message);
		return 12015;
	}
	status = sink->content->request(&sink->uri, (unsigned char)sink->moveState, &message,
					sink->moveStatus);
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1141);
		SwMwMoveMessageDone(&message);
		return SwMwStatus(status);
	}
	record.label = message.label;
	memcpy(record.key, message.key, 8);
	memcpy(record.first, message.first, 10);
	memcpy(record.second, message.second, 10);
	record.kind = 1;
	record.requested = sink->sessionState[6];
	record.size = sink->sessionState[7];
	if (record.size != 0)
		memcpy(record.payload, sink->session, record.size);
	status = SwMwMoveRecordAppend(sink->manager, &record);
	if (status != 0) {
		SwMwMoveMessageDone(&message);
		return status;
	}
	sink->sessionState[5] = record.id;
	for (i = 0; i < 2; ++i) {
		swMwTrace("sw mw:Send MV_FINALIZE: kxm_label=0x%x contentID=%d\n",
			  record.label, record.requested);
		status = sink->content->finalize(&sink->uri, &message);
		if (status == 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1174);
			break;
		}
	}
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1176);
		SwMwMoveMessageDone(&message);
		return SwMwStatus(status);
	}
	sink->finalized = 1;
	status = SwMwMoveSinkStreamComplete(stream);
	SwMwMoveMessageDone(&message);
	return status;
}

/* send the move's MV_FINALIZE for the labelled session (0x8100bd10) */
extern "C" int SwMwMoveSinkStreamFinalize(struct SwMwSinkStream *stream, int value)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	struct SwMwMoveMessage message;
	struct SwMwMoveRecord record;
	int status;

	SwMwMoveMessageInit(&message);
	memset(&record, 0, sizeof(record));
	status = sink->content->request(&sink->uri, (unsigned char)sink->moveState, &message, 0);
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1334);
		SwMwMoveMessageDone(&message);
		return SwMwStatus(status);
	}
	record.label = message.label;
	memcpy(record.key, message.key, 8);
	memcpy(record.first, message.first, 10);
	memcpy(record.second, message.second, 10);
	record.kind = 1;
	record.requested = sink->sessionState[6];
	record.size = sink->sessionState[7];
	if (record.size != 0)
		memcpy(record.payload, sink->session, record.size);
	status = SwMwMoveRecordAppend(sink->manager, &record);
	if (status != 0) {
		SwMwMoveMessageDone(&message);
		return status;
	}
	sink->sessionState[5] = record.id;
	swMwTrace("sw mw:Send MV_FINALIZE: kxm_label=0x%x contentID=%d\n",
		  record.label, record.requested);
	status = sink->content->finalize(&sink->uri, &message);
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1365);
		SwMwMoveMessageDone(&message);
		return SwMwStatus(status);
	}
	sink->finalized = 1;
	status = SwMwMoveSinkStreamCompleteLabel(stream);
	SwMwMoveMessageDone(&message);
	return status;
}

/* send the move's MV_COMPLETE for the labelled session and finish it (0x8100bee0) */
extern "C" int SwMwMoveSinkStreamCompleteLabel(struct SwMwSinkStream *stream)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	int value;
	int status;

	if (sink == NULL || sink->content == NULL)
		return 12004;
	if (sink->finalized == 0)
		return 12004;
	SwMwMoveSinkStreamMutexLock(&sink->mutex);
	if (sink->sessionState[0] == 0) {
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return 12007;
	}
	value = sink->sessionState[1];
	if (value != 0) {
		if (value == 1033) {
			sink->finalized = 0;
			sink->sessionState[0] = 0;
			sink->sessionState[1] = 0;
		} else if (value == 1019) {
			sink->content->abort(&sink->uri, (unsigned char)sink->moveState);
			sink->moveState = -1;
			sink->content->operation(&sink->uri, &sink->result);
			sink->result = 0;
			SwMwCoreNotifyLabel(sink->device, sink->sessionState[6], 41);
			SwMwMoveListRelease(sink->manager, sink->sessionState[5]);
		}
		SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
		return value;
	}
	SwMwMoveSinkStreamMutexUnlock(&sink->mutex);
	if (sink->sessionState[2] == 0) {
		status = SwMwMoveRecordBegin(sink->manager, sink->sessionState[5], 2);
		if (status != 0)
			return status;
		swMwTrace("sw mw:Send MV_COMPLETE: kxm_label=0x%x contentID=%d\n",
			  sink->moveState, sink->sessionState[6]);
		status = sink->content->complete(&sink->uri, (unsigned char)sink->moveState);
		if (status != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 1443);
			return SwMwStatus(status);
		}
		sink->sessionState[2] = 1;
	}
	return 0;
}

/* hand the sink one of the reader's chunks (0x8100acf6) */
extern "C" int SwMwMoveSinkStreamReceive(struct SwMwSinkStream *stream, int *offset,
					 int *size, unsigned char *type)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	struct SwMwStreamChunk *chunks;
	int chunkSize;
	int finished;
	int status;
	int done;
	int failed;

	*type = 0xFF;
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->receiving = 1;
	if (sink->moveResult != 0)
		stream->finished = 1;
	finished = stream->finished;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	failed = 0;
	done = 0;
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MOVESTREAM_SOURCE, 661);
		*size = 0;
		*offset = 0;
		failed = 1;
	}
	if (failed == 0 && stream->reader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 669);
		*size = 0;
		*offset = 0;
		failed = 1;
	}
	if (failed == 0) {
		if (stream->receiveHave == 0) {
			stream->receiveCount = stream->reader->getReadChunkList(
								(void **)&stream->receiveChunks);
			if (stream->receiveCount == -1) {
				*size = 0;
				stream->reader->consume(0);
				stream->empty = 1;
				status = stream->emptyStatus;
				if (status != 0)
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESTREAM_SOURCE, 684);
				failed = 1;
			} else if (stream->receiveCount == 0) {
				stream->reader->consume(0);
				*size = 0;
				*offset = 0;
				done = 1;
			} else {
				stream->receiveIndex = 0;
				stream->receiveConsumed = 0;
				chunkSize = 0;
				while (stream->receiveIndex < stream->receiveCount) {
					chunkSize = stream->receiveChunks[stream->receiveIndex].size;
					if (chunkSize != 0)
						break;
					++stream->receiveIndex;
				}
				if (chunkSize == 0) {
					stream->reader->consume(0);
					status = 12014;
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESTREAM_SOURCE, 711);
					failed = 1;
				} else {
					stream->receiveHave = 1;
				}
			}
		}
		if (failed == 0) {
			if (done == 0) {
				chunks = stream->receiveChunks;
				chunkSize = chunks[stream->receiveIndex].size;
				*offset = chunks[stream->receiveIndex].offset +
					  stream->receiveConsumed;
				*size = chunkSize - stream->receiveConsumed;
				if (*size == 0)
					swMwTrace("[SWMW] ERROR in %s:%d\n",
						  SWMW_MOVESTREAM_SOURCE, 721);
			}
			*type = SwMwStreamEventType(&stream->event);
			SwMwMoveSinkStreamMutexLock(&stream->mutex);
			stream->receiving = 0;
			SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
			return 0;
		}
	}
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->receiving = 0;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* release the bytes the sink consumed from its chunk (0x8100aff8) */
extern "C" int SwMwMoveSinkStreamRelease(struct SwMwSinkStream *stream, int size)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	struct SwMwStreamChunk *chunks;
	int chunkSize;
	int finished;
	int status;
	int failed;
	int total;
	unsigned int i;

	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->releasing = 1;
	if (sink->moveResult != 0)
		stream->finished = 1;
	finished = stream->finished;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	failed = 0;
	status = 0;
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MOVESTREAM_SOURCE, 763);
		failed = 1;
	}
	if (failed == 0 && stream->reader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 769);
		failed = 1;
	}
	if (failed == 0) {
		if (stream->receiveHave != 0) {
			chunks = stream->receiveChunks;
			chunkSize = chunks[stream->receiveIndex].size;
			stream->receiveConsumed += size;
			if (stream->receiveConsumed >= chunkSize) {
				++stream->receiveIndex;
				while (stream->receiveIndex < stream->receiveCount &&
				       chunks[stream->receiveIndex].size == 0)
					++stream->receiveIndex;
				if (stream->receiveIndex >= stream->receiveCount) {
					stream->receiveHave = 0;
					total = 0;
					for (i = 0; i < (unsigned int)stream->receiveCount; ++i)
						total += chunks[i].size;
					if (stream->reader->consume(total) == 0) {
						swMwTrace("[SWMW] ERROR in %s:%d\n",
							  SWMW_MOVESTREAM_SOURCE, 796);
						status = 12003;
						failed = 1;
					}
				} else {
					stream->receiveConsumed = 0;
				}
			}
		}
	}
	if (failed == 0) {
		SwMwMoveSinkStreamMutexLock(&stream->mutex);
		stream->releasing = 0;
		SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
		return 0;
	}
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->releasing = 0;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* hand the sink the next send chunk (0x8100b22e) */
extern "C" int SwMwMoveSinkStreamSend(struct SwMwSinkStream *stream, int *offset, int *size)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	struct SwMwStreamChunk *chunks;
	int chunkSize;
	int finished;
	int endOfStream;
	int status;
	int failed;
	int done;

	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->sending = 1;
	if (sink->moveResult != 0)
		stream->finished = 1;
	finished = stream->finished;
	endOfStream = stream->endOfStream;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	failed = 0;
	done = 0;
	status = 0;
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MOVESTREAM_SOURCE, 843);
		*size = 0;
		*offset = 0;
		failed = 1;
	} else if (endOfStream != 0) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 851);
		*size = 0;
		*offset = 0;
		failed = 1;
	} else if (stream->downloader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 859);
		*size = 0;
		*offset = 0;
		failed = 1;
	} else {
		if (stream->sendHave == 0) {
			status = stream->downloader->getChunkList((void **)&stream->sendChunks,
							   &stream->sendCount);
			if (status == 0) {
				if (stream->sendCount == 0) {
					stream->downloader->release(0);
					*size = 0;
					*offset = 0;
					done = 1;
				} else {
					stream->sendIndex = 0;
					stream->sendConsumed = 0;
					chunkSize = 0;
					while (stream->sendIndex < stream->sendCount) {
						chunkSize = stream->sendChunks[stream->sendIndex].size;
						if (chunkSize != 0)
							break;
						++stream->sendIndex;
					}
					if (chunkSize == 0) {
						stream->downloader->release(0);
						status = 12014;
						swMwTrace("[SWMW] Error %d in %s:%d\n", status,
							  SWMW_MOVESTREAM_SOURCE, 902);
						failed = 1;
					} else {
						stream->sendHave = 1;
					}
				}
			} else {
				*size = 0;
				if (status == 102)
					status = 12401;
				else
					status = SwMwStatus(status);
				if (status != 0)
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESTREAM_SOURCE, 877);
				failed = 1;
			}
		}
		if (failed == 0 && done == 0) {
			chunks = stream->sendChunks;
			chunkSize = chunks[stream->sendIndex].size;
			*offset = chunks[stream->sendIndex].offset + stream->sendConsumed;
			*size = chunkSize - stream->sendConsumed;
			if (*size == 0)
				swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 912);
		}
	}
	if (failed == 0) {
		SwMwMoveSinkStreamMutexLock(&stream->mutex);
		stream->sending = 0;
		SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
		return 0;
	}
	if ((status > 11007 && status <= 11012) ||
	    (status <= 11007 && status > 1005 && status == 1016) ||
	    (status <= 1005 && status > 1004))
		stream->emptyStatus = status;
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->sending = 0;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* release the pending send bytes when the message is sent (0x8100b59e) */
extern "C" int SwMwMoveSinkStreamSendMessage(struct SwMwSinkStream *stream, int size)
{
	struct SwMwMoveSink *sink = (struct SwMwMoveSink *)stream->sink;
	int finished;
	int endOfStream;
	int status;
	int failed;

	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->sendingMessage = 1;
	if (sink->moveResult != 0)
		stream->finished = 1;
	finished = stream->finished;
	endOfStream = stream->endOfStream;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	status = 0;
	failed = 0;
	if (finished != 0) {
		status = stream->status;
		if (status != 12015 && status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MOVESTREAM_SOURCE, 965);
		failed = 1;
	} else if (endOfStream != 0) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 971);
		failed = 1;
	} else if (stream->downloader == NULL) {
		status = 12004;
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 977);
		failed = 1;
	} else if (stream->sendHave != 0) {
		int result;

		stream->sendConsumed += size;
		stream->sendHave = 0;
		result = stream->downloader->release(stream->sendConsumed);
		stream->sendTotal += stream->sendConsumed;
		if (result != 0) {
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 987);
			status = 12003;
			failed = 1;
		}
	}
	if (failed == 0) {
		SwMwMoveSinkStreamMutexLock(&stream->mutex);
		stream->sendingMessage = 0;
		SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
		return 0;
	}
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	stream->sendingMessage = 0;
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* stop the move stream: drain the reader when asked, then release the transfer and the
 * reader and notify the sink (0x8100aa1e) */
extern "C" int SwMwMoveSinkStreamStop(struct SwMwSinkStream *stream, int mode)
{
	struct SwMwStreamChunk *chunks;
	struct SwMwContentManager *manager;
	struct SwMwTransfer *interface;
	int list = 0;
	unsigned int count = 0;
	int total;
	int status;
	int drained;
	unsigned int i;

	if (stream->empty == 0) {
		status = SwMwMoveSinkStreamSetFinished(stream, 12015);
		if (status != 0)
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_MOVESTREAM_SOURCE, 529);
	}
	status = SwMwMoveSinkStreamAbort(stream);
	if (status == 12007)
		return status;
	SwMwMoveSinkStreamMutexLock(&stream->mutex);
	if (stream->downloader != NULL) {
		if (stream->receiving != 0 || stream->releasing != 0) {
			status = 12007;
		} else {
			drained = 0;
			if (mode == 0) {
				if (stream->receiveHave != 0) {
					stream->receiveHave = 0;
					total = 0;
					for (i = 0; i < (unsigned int)stream->receiveCount; ++i)
						total += stream->receiveChunks[i].size;
					stream->reader->consume(total);
				}
				if (stream->reader->isEndOfStream() == 0 ||
				    stream->reader->available() != 0) {
					count = stream->reader->getReadChunkList((void **)&list);
					if (count != 0xFFFFFFFFu) {
						chunks = (struct SwMwStreamChunk *)list;
						total = 0;
						for (i = 0; i < count; ++i)
							total += chunks[i].size;
						stream->reader->consume(total);
						status = 12007;
						drained = 1;
					}
				}
				if (drained == 0) {
					if (stream->downloader->getSessionData(
							(int *)&stream->connectData[0]) == 0)
						stream->resume = 1;
				}
			}
			if (drained == 0) {
				manager = stream->manager;
				interface = (struct SwMwTransfer *)((unsigned char *)stream->downloader +
						(*(int **)(void *)stream->downloader)[-13]);
				status = manager->releaseTransfer(interface);
				if (status != 0)
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESTREAM_SOURCE, 584);
				stream->downloader = NULL;
				stream->reader = NULL;
				stream->connected = 0;
				if (stream->empty == 0)
					status = SwMwCoreNotifyLabel(*(int *)stream->sink,
								      stream->contentId, 41);
			}
		}
	}
	SwMwMoveSinkStreamMutexUnlock(&stream->mutex);
	return status;
}

/* the set-parameter command is the stop body with its mode (0x8100ac9a) */
extern "C" int SwMwMoveSinkStreamSetParameter(struct SwMwSinkStream *stream, int parameter)
{
	return SwMwMoveSinkStreamStop(stream, parameter);
}

/* the stream's gate (0x8100b7ea) */
extern "C" struct SwMwGate *SwMwMoveSinkStreamGate(struct SwMwSinkStream *stream)
{
	return &stream->gate;
}

/* register the stream's event handler (0x8100a326) */
extern "C" int SwMwMoveSinkStreamEventSet(struct SwMwSinkStream *stream, void *handler,
					  void *argument)
{
	return SwMwStreamEventSet(&stream->event, handler, stream, argument);
}

/* initialise a constructed move stream on its sink and window (0x8100c23c) */
extern "C" int SwMwMoveSinkStreamInit(struct SwMwSinkStream *stream, void *sink,
				      const struct SwMwStreamWindow *window)
{
	int status;
	int failed;

	stream->sink = (int)sink;
	stream->window = *window;
	stream->manager = (struct SwMwContentManager *)SwMwCoreContentManager(*(int *)stream->sink);
	memset(stream->connectData, 0, 8);
	status = SwMwMutexCreate(&stream->mutex);
	failed = 0;
	if (status != 0) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 141);
		failed = 1;
	}
	if (failed != 0)
		return 12009;
	SwMwMoveSinkStreamEventSet(stream, (void *)SwMwMoveSinkStreamOnEvent, NULL);
	stream->state = 1;
	return 0;
}

/* tear the move stream down (0x8100c4ee) */
extern "C" struct SwMwSinkStream *SwMwMoveSinkStreamDestruct(struct SwMwSinkStream *stream)
{
	if (stream->state != 0) {
		stream->state = 0;
		if (SwMwSinkStreamMutexDelete(&stream->mutex) != 0)
			swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 157);
	}
	SwMwGateDone(&stream->gate);
	SwMwMoveStreamEventDestruct(&stream->event);
	return stream;
}

/* create the channel's move stream on its sink and window (0x8100a1be) */
extern "C" int SwMwMoveSinkStreamCreate(struct SwMwSinkStream **stream, void *sink,
					const struct SwMwStreamWindow *window)
{
	struct SwMwSinkStream *object = NULL;
	int status;
	int failed;

	object = (struct SwMwSinkStream *)::operator new(sizeof(struct SwMwSinkStream),
							 std::nothrow);
	if (object != NULL)
		object = SwMwMoveSinkStreamConstruct(object);
	failed = 0;
	if (object == NULL) {
		swMwTrace("[SWMW] ERROR in %s:%d\n", SWMW_MOVESTREAM_SOURCE, 203);
		failed = 1;
	}
	if (failed != 0)
		return 12008;
	status = SwMwMoveSinkStreamInit(object, sink, window);
	if (status != 0) {
		if (object != NULL)
			::operator delete(SwMwMoveSinkStreamDestruct(object));
		return status;
	}
	*stream = object;
	return 0;
}

/* the move stream's event: 0 asks for a re-authentication check, 1 finishes (0x8100c558) */
extern "C" int SwMwMoveSinkStreamOnEvent(struct SwMwSinkStream *stream, int *message)
{
	int result;

	result = message[0];
	if (result > 0) {
		if (result <= 1) {
			result = SwMwMoveSinkStreamIsOpen(stream);
			if (result != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", result,
					  SWMW_MOVESTREAM_SOURCE, 187);
				return 1;
			}
		}
	} else if (result >= 0) {
		result = SwMwMoveSinkStreamKxExpiring();
		if (result != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", result,
				  SWMW_MOVESTREAM_SOURCE, 183);
			return 1;
		}
	}
	return result;
}

/* the periodic connect: check the transfer's key when it is expiring (0x8100b786) */
extern "C" int SwMwMoveSinkStreamConnect(struct SwMwSinkStream *stream)
{
	int status;

	if (stream->connected == 0 || stream->downloader == NULL)
		return 0;
	if ((stream->downloader->isKeyExpiring() & 1) == 0)
		return 0;
	status = SwMwMoveSinkStreamKxExpiring();
	if (status != 0)
		swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESTREAM_SOURCE, 1034);
	return 0;
}

/* the move's key is about to expire (0x8100b76c) */
extern "C" int SwMwMoveSinkStreamKxExpiring(void)
{
	swMwTrace("sw mw:KX will be expired within 60 sec\n");
	return 0;
}
