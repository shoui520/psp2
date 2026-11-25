/*
 * SceSulpha server, protocol state machine, lock-free queue, and agent registry.
 */

#include "sulpha_internal.h"

static const SulphaProtoStep g_stepSendResp = {
	sulphaProtoSendBuf,
	sulphaProtoResetRecvHeader,
};

static const SulphaProtoStep g_stepDrainQueue = {
	sulphaProtoDrainQueue,
	sulphaProtoCheckTxReady,
};

static const SulphaProtoStep g_stepRecvHeader = {
	sulphaProtoRecvBuf,
	sulphaProtoParseHeader,
};

static const SulphaProtoStep g_stepFlushTx = {
	sulphaProtoFlushTx,
	sulphaProtoFinishTx,
};

static const SulphaProtoStep g_stepRecvBody = {
	sulphaProtoRecvBuf,
	sulphaProtoHandleMessage,
};

static inline __attribute__((always_inline)) void sulphaQueuePushNode(SulphaQueue *queue, SulphaQueueNode *node)
{
	node->next = (SulphaQueueNode *)queue;

	for (;;) {
		SceUInt32 tailTag = queue->tail.tag;
		SulphaQueueNode *tailPtr = (SulphaQueueNode *)queue->tail.ptr;
		SceUInt64 expectedTail = sulphaPackTaggedPtr(tailPtr, tailTag);

		while ((SulphaQueueNode *)sulphaAtomicLoad32(&tailPtr->next) == (SulphaQueueNode *)queue) {
			if (sulphaAtomicStore32((SceUInt32)(SceUIntPtr)node, &tailPtr->next) == 0) {
				SceUInt64 newTail = sulphaPackTaggedPtr(node, tailTag + 1);
				while (sulphaAtomicLoad64(&queue->tail) == expectedTail &&
				       sulphaAtomicStore64(newTail, &queue->tail) != 0)
					;
				sulphaAtomicInc32(&queue->count);
				return;
			}
		}

		SulphaQueueNode *nextPtr = tailPtr->next;
		SceUInt64 advTail = sulphaPackTaggedPtr(nextPtr, tailTag + 1);
		while (sulphaAtomicLoad64(&queue->tail) == expectedTail &&
		       sulphaAtomicStore64(advTail, &queue->tail) != 0)
			;
	}
}

static inline __attribute__((always_inline)) SulphaQueueNode *sulphaQueuePopInternal(SulphaQueue *queue)
{
	for (;;) {
		SceUInt32 headTag = queue->head.tag;
		SulphaQueueNode *headPtr = (SulphaQueueNode *)queue->head.ptr;
		SulphaQueueNode *nextPtr = headPtr->next;

		if (queue->tail.ptr == headPtr) {
			if (nextPtr == (SulphaQueueNode *)queue)
				return NULL;
			SceUInt32 tailTag = queue->tail.tag;
			SceUInt64 expectedTail = sulphaPackTaggedPtr(headPtr, tailTag);
			SceUInt64 advTail = sulphaPackTaggedPtr(nextPtr, tailTag + 1);
			while (sulphaAtomicLoad64(&queue->tail) == expectedTail &&
			       sulphaAtomicStore64(advTail, &queue->tail) != 0)
				;
			continue;
		}

		if (nextPtr == (SulphaQueueNode *)queue)
			continue;

		SceUInt64 expectedHead = sulphaPackTaggedPtr(headPtr, headTag);
		SceUInt64 newHead = sulphaPackTaggedPtr(nextPtr, headTag + 1);
		SceUInt64 observed;
		do {
			observed = sulphaAtomicLoad64(&queue->head);
		} while (observed == expectedHead && sulphaAtomicStore64(newHead, &queue->head) != 0);

		if (observed == expectedHead) {
			sulphaAtomicDec32(&queue->count);
			if (headPtr == &queue->sentinel) {
				sulphaQueuePushNode(queue, headPtr);
				continue;
			}
			return headPtr;
		}
	}
}

const SulphaProtoStep *sulphaProtoCheckTxReady(SulphaProtocol *proto)
{
	if (proto->txBuf == NULL)
		return NULL;
	if (proto->txRemaining != 0)
		return &g_stepFlushTx;
	return NULL;
}

const SulphaProtoStep *sulphaProtoFinishTx(SulphaProtocol *proto)
{
	if (proto->txBuf != NULL) {
		proto->txBuf = NULL;
		proto->txOffset = 0;
	}
	return (proto->state == SULPHA_PROTO_ERROR) ? NULL : &g_stepDrainQueue;
}

const SulphaProtoStep *sulphaProtoResetRecvHeader(SulphaProtocol *proto)
{
	proto->rxOffset = 0;
	proto->rxRemaining = 4;
	sulphaBerResetStream(&proto->rxStream);
	return (proto->state == SULPHA_PROTO_ERROR) ? NULL : &g_stepRecvHeader;
}

SceUInt32 sulphaProtoSetState(SulphaProtocol *proto, SceUInt32 newState)
{
	SceUInt32 cur = proto->state;
	if (cur == newState)
		return cur;
	if (cur == SULPHA_PROTO_STOPPING) {
		if (newState != 0 && newState != SULPHA_PROTO_STOPPED)
			return cur;
	} else if (cur == SULPHA_PROTO_STOPPED && newState > SULPHA_PROTO_IDLE) {
		return cur;
	}
	proto->state = newState;
	return newState;
}

SulphaQueueNode *sulphaQueuePopA(SulphaQueue *queue)
{
	return sulphaQueuePopInternal(queue);
}

SulphaQueueNode *sulphaQueuePopB(SulphaQueue *queue)
{
	return sulphaQueuePopInternal(queue);
}

const SulphaProtoStep *sulphaProtoParseHeader(SulphaProtocol *proto)
{
	SceUInt16 len = sulphaBerGetPayloadLength(&proto->rxStream);
	proto->rxRemaining = len;

	if ((SceUInt32)(len - 1U) > 0xFFFDU) {
		if (proto->state == SULPHA_PROTO_ERROR)
			return NULL;
		if (proto->state != SULPHA_PROTO_STOPPING && proto->state != SULPHA_PROTO_STOPPED) {
			proto->state = SULPHA_PROTO_ERROR;
			return NULL;
		}
	}

	SceUInt32 rel = proto->state - SULPHA_PROTO_CONNECTED;
	if (rel > 4 || ((1U << rel) & 0x17U) == 0 || len == 0)
		return NULL;

	return &g_stepRecvBody;
}

SceBool sulphaProtoFlushTx(SulphaProtocol *proto)
{
	SceInt32 res = 0;

	if (proto->fileCapture != NULL) {
		res = sulphaFileCaptureWrite(proto->fileCapture, proto->txBuf + proto->txOffset, proto->txRemaining);
		proto->txOffset = 0;
		proto->txRemaining = 0;
	} else {
		SceInt32 sent = 0;
		if (proto->txRemaining != 0) {
			if (sulphaSocketIsOpen(&proto->socket)) {
				sent = sulphaSocketSend(&proto->socket, proto->txBuf + proto->txOffset, proto->txRemaining);
				if (sent <= 0) {
					if (sent == 0) {
						sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
						res = SULPHA_ERROR_IO;
					} else {
						SceInt32 err = sulphaSocketGetError(&proto->socket);
						if (err == 1 || (err == 0 && sent == -1)) {
							sent = 0;
							res = 0;
						} else {
							sent = 0;
							sulphaProtoSetState(proto, SULPHA_PROTO_ERROR);
							res = SULPHA_ERROR_TRANSPORT;
						}
					}
				}
			} else {
				sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
				res = SCE_SULPHA_ERROR_INVALID_POINTER;
			}
		}
		proto->txOffset += (SceUInt32)sent;
		proto->txRemaining -= (SceUInt32)sent;
	}

	return res < 0 || proto->state == SULPHA_PROTO_ERROR || proto->txRemaining == 0;
}

SceBool sulphaProtoSendBuf(SulphaProtocol *proto)
{
	SceUInt8 *buf = sulphaBerGetBuffer(&proto->rxStream);

	if (proto->rxRemaining != 0) {
		if (!sulphaSocketIsOpen(&proto->socket)) {
			sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
			return true;
		}

		SceInt32 sent = sulphaSocketSend(&proto->socket, buf + proto->rxOffset, proto->rxRemaining);
		if (sent <= 0) {
			if (sent == 0) {
				sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
				return true;
			}
			SceInt32 err = sulphaSocketGetError(&proto->socket);
			if (err != 1 && !(err == 0 && sent == -1)) {
				sulphaProtoSetState(proto, SULPHA_PROTO_ERROR);
				return true;
			}
			sent = 0;
		}
		proto->rxOffset += (SceUInt32)sent;
		proto->rxRemaining -= (SceUInt32)sent;
	}

	return proto->state == SULPHA_PROTO_ERROR || proto->rxRemaining == 0;
}

SceBool sulphaProtoRecvBuf(SulphaProtocol *proto)
{
	SceUInt8 *buf = sulphaBerGetBuffer(&proto->rxStream);

	if (proto->rxRemaining != 0) {
		if (!sulphaSocketIsOpen(&proto->socket)) {
			sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
			return true;
		}

		SceInt32 recvd = sulphaSocketRecv(&proto->socket, buf + proto->rxOffset, (SceInt32)proto->rxRemaining);
		if (recvd <= 0) {
			if (recvd == 0) {
				sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
				return true;
			}
			SceInt32 err = sulphaSocketGetError(&proto->socket);
			if (err != 1 && !(err == 0 && recvd == -1)) {
				sulphaProtoSetState(proto, SULPHA_PROTO_ERROR);
				return true;
			}
			recvd = 0;
		}
		proto->rxOffset += (SceUInt32)recvd;
		proto->rxRemaining -= (SceUInt32)recvd;
	}

	return proto->state == SULPHA_PROTO_ERROR || proto->rxRemaining == 0;
}

SceBool sulphaProtoDrainQueue(SulphaProtocol *proto)
{
	SulphaServer *server = proto->server;
	SceUInt32 total = proto->txOffset;
	SceUInt8 *dst = sulphaBerGetBuffer(&proto->txStream) + total;
	SulphaEvent *ev = proto->pendingEvent;

	if (ev == NULL) {
		ev = (SulphaEvent *)sulphaQueuePopA(&server->eventQueue);
		proto->pendingEvent = ev;
	}

	while (ev != NULL && total + ev->dataSize <= SULPHA_STREAM_BUF_SIZE) {
		sulphaMemcpy(dst, ev->payload, ev->dataSize);
		dst += ev->dataSize;
		total += ev->dataSize;

		if (ev->agent != NULL)
			sulphaAgentEventDelete(ev->agent, ev, true);
		else
			sulphaServerFreeBookmark(server, ev);

		ev = (SulphaEvent *)sulphaQueuePopA(&server->eventQueue);
		proto->pendingEvent = ev;
	}

	if (proto->txMaxBatch < total)
		proto->txMaxBatch = total;

	proto->txBuf = sulphaBerGetBuffer(&proto->txStream);
	proto->txOffset = 0;
	proto->txRemaining = total;

	if (proto->state == SULPHA_PROTO_ERROR)
		return false;
	return proto->txBuf != NULL && total != 0;
}

void sulphaProtoInit(SulphaProtocol *proto, SulphaServer *server, SulphaAllocator *alloc)
{
	proto->state = SULPHA_PROTO_IDLE;
	proto->server = server;
	proto->rxStep = NULL;
	proto->txStep = NULL;
	proto->pendingEvent = NULL;
	proto->txOffset = 0;
	proto->txRemaining = 0;
	proto->txBuf = NULL;
	proto->txMaxBatch = 0;
	proto->fileCapture = NULL;

	sulphaSocketInit(&proto->socket);
	sulphaBerAllocStream(&proto->rxStream, alloc, SULPHA_STREAM_BUF_SIZE);
	sulphaBerAllocStream(&proto->txStream, alloc, SULPHA_STREAM_BUF_SIZE);
}

void sulphaProtoCloseSocket(SulphaProtocol *proto)
{
	sulphaSocketShutdown(&proto->socket);
	sulphaProtoSetState(proto, SULPHA_PROTO_CLOSING);
}

void sulphaProtoReset(SulphaProtocol *proto, SceBool flush)
{
	if (flush && proto->state == SULPHA_PROTO_STREAMING) {
		const SulphaProtoStep *step = proto->txStep;
		proto->state = SULPHA_PROTO_CLOSING;
		while (step != NULL) {
			if (!step->run(proto))
				break;
			step = proto->txStep->next(proto);
			proto->txStep = step;
		}
	}

	proto->rxStep = NULL;
	proto->txStep = NULL;
	proto->txOffset = 0;
	proto->txRemaining = 0;
	proto->txBuf = NULL;
	proto->txMaxBatch = 0;

	if (proto->state != SULPHA_PROTO_IDLE && proto->state != SULPHA_PROTO_STOPPING)
		proto->state = SULPHA_PROTO_IDLE;
}

SceInt32 sulphaProtoUpdate(SulphaProtocol *proto)
{
	const SulphaProtoStep *step = proto->rxStep;
	while (step != NULL) {
		if (!step->run(proto))
			break;
		step = proto->rxStep->next(proto);
		proto->rxStep = step;
	}

	if (proto->state == SULPHA_PROTO_STREAMING) {
		step = proto->txStep;
		while (step != NULL) {
			if (!step->run(proto))
				break;
			step = proto->txStep->next(proto);
			proto->txStep = step;
		}
	}

	if (proto->state <= SULPHA_PROTO_STREAMING)
		return 0;

	proto->rxStep = NULL;
	proto->txStep = NULL;
	return SCE_SULPHA_ERROR_CONNECTION;
}

SceInt32 sulphaProtoAccept(SulphaProtocol *proto, SulphaListener *listener)
{
	SceInt32 res = sulphaListenerAccept(listener, &proto->socket);
	if (res < 0)
		return res;

	sulphaSocketSetNonBlocking(&proto->socket, 0);
	sulphaProtoSetState(proto, SULPHA_PROTO_CONNECTED);
	proto->txStep = &g_stepDrainQueue;
	proto->rxStep = &g_stepSendResp;
	proto->rxOffset = 0;
	proto->rxRemaining = 4;
	sulphaBerResetStream(&proto->rxStream);
	proto->rxStep = (proto->state == SULPHA_PROTO_ERROR) ? NULL : &g_stepRecvHeader;
	return sulphaProtoUpdate(proto);
}

SceInt32 sulphaProtoStartFileCapture(SulphaProtocol *proto, SulphaFileCapture *fc,
				     const SceUInt8 *agentIds, SceUInt8 agentCount)
{
	SulphaBerStream *tx = &proto->txStream;
	SulphaBerStream *writer = NULL;
	SulphaBerMarker marker;
	SceUInt8 msgType;

	for (SceUInt32 i = 0; i < agentCount; i++) {
		sulphaBerResetStream(tx);
		sulphaBerGetWriter(tx, &writer);
		sulphaBerSetHeader(tx, 2, agentIds[i]);
		sulphaBerBeginContainer(writer, 3, 64, &marker);
		sulphaBerWriteTaggedUInt8(writer, 2, 0);
		sulphaBerWriteTaggedUInt8(writer, 3, 1);
		sulphaBerEndContainer(writer, &marker);
		sulphaBerFinalize(tx);
		sulphaBerBeginRead(tx, &msgType);
		sulphaServerDispatchMessage(proto->server, tx, msgType);
		sulphaBerFinalize(tx);
		sulphaFileCaptureWrite(fc, sulphaBerGetBuffer(tx), (SceUInt32)sulphaBerGetPayloadLength(tx) + 4U);
	}

	sulphaProtoSetState(proto, SULPHA_PROTO_STREAMING);

	for (SceUInt32 i = 0; i < agentCount; i++) {
		sulphaBerSetAgentId(tx, agentIds[i]);
		sulphaServerDispatchMessage(proto->server, tx, 11);
		sulphaBerFinalize(tx);
		sulphaFileCaptureWrite(fc, sulphaBerGetBuffer(tx), (SceUInt32)sulphaBerGetPayloadLength(tx) + 4U);
	}

	proto->fileCapture = fc;
	proto->txStep = &g_stepDrainQueue;
	return sulphaProtoUpdate(proto);
}

SceBool sulphaWriteMsgHeader(SulphaBerStream *stream, SulphaServer *server,
			     SceUInt32 errCode, const char *errMsg, SceUInt64 *outTimestamp)
{
	SulphaBerMarker marker;
	const char *msg = errMsg;
	SceSize msgLen = 0;

	if (msg == NULL)
		msg = sulphaProtocolErrorString(errCode);
	if (msg != NULL)
		msgLen = sulphaStrlen(msg);

	if (!sulphaBerBeginContainer(stream, 0, 64, &marker))
		return false;

	SceUInt64 ts = sulphaTimerGetTimestamp(&server->timer);
	SceBool ok = sulphaBerWriteTaggedUInt64(stream, 0, ts);
	if (ok && errCode != 0)
		ok = sulphaBerWriteTaggedInt32(stream, 1, (SceInt32)errCode);
	if (!ok)
		return false;

	if (msgLen != 0 && errMsg != NULL) {
		if (!sulphaBerWriteRaw(stream, 12, msg, msgLen))
			return false;
	}

	ok = sulphaBerEndContainer(stream, &marker);
	if (ok && outTimestamp != NULL)
		*outTimestamp = ts;
	return ok;
}

SceBool sulphaWriteSimpleResponse(SulphaBerStream *stream, SulphaServer *server,
				  SceUInt32 errCode, SceUInt8 respTag)
{
	SulphaBerMarker marker;
	const char *errStr = sulphaProtocolErrorString(errCode);
	if (errStr != NULL)
		sulphaStrlen(errStr);

	if (!sulphaBerBeginContainer(stream, 0, 64, &marker))
		return false;

	SceUInt64 ts = sulphaTimerGetTimestamp(&server->timer);
	SceBool ok = sulphaBerWriteTaggedUInt64(stream, 0, ts);
	if (ok && errCode != 0)
		ok = sulphaBerWriteTaggedInt32(stream, 1, (SceInt32)errCode);
	if (!ok)
		return false;

	if (!sulphaBerEndContainer(stream, &marker))
		return false;
	if (!sulphaBerBeginContainer(stream, respTag, 64, &marker))
		return false;
	if (!sulphaBerEndContainer(stream, &marker))
		return false;

	return stream->state != SULPHA_BER_OVERFLOW && stream->state != SULPHA_BER_ERROR;
}

const SulphaProtoStep *sulphaProtoHandleMessage(SulphaProtocol *proto)
{
	SulphaBerStream *rx = &proto->rxStream;
	SceUInt8 msgType = 0;
	SceUInt8 agentId = sulphaBerGetAgentId(rx);
	SceInt32 dispatchRes = SULPHA_ERROR_INTERNAL;

	sulphaBerBeginRead(rx, &msgType);
	if (agentId != 0)
		dispatchRes = sulphaServerDispatchMessage(proto->server, rx, msgType);

	switch (msgType) {
	case 1: {
		if (proto->state != SULPHA_PROTO_CONNECTED)
			break;

		proto->state = SULPHA_PROTO_HELLO;
		const SceUInt8 *magicPtr = NULL;
		SceUInt32 magicLen = 0;
		SceUInt8 seqNum = 0;
		SceUInt8 protoVer = 1;
		SulphaBerReader *reader = NULL;
		SulphaBerStream *writer = NULL;
		SulphaBerMarker respMarker, verMarker;
		SceUInt8 agentIds[SULPHA_MAX_AGENTS];

		sulphaBerGetReader(rx, &reader);
		for (;;) {
			SceBool gotMagic = sulphaBerReadBytes(reader, 1, &magicPtr, &magicLen);
			SceBool gotSeq = sulphaBerReadUInt8(reader, 2, &seqNum);
			SceBool gotVer = sulphaBerReadUInt8(reader, 3, &protoVer);
			if (!gotMagic && !gotSeq && !gotVer)
				break;
			if (reader->state == SULPHA_BER_EOF ||
			    reader->pos >= reader->end ||
			    reader->state == SULPHA_BER_ERROR)
				break;
		}

		if (magicPtr == NULL || magicLen != 4 ||
		    magicPtr[0] != 'H' || magicPtr[1] != 'E' ||
		    magicPtr[2] != 'L' || magicPtr[3] != 'O' ||
		    protoVer != 1)
			return NULL;

		sulphaBerResetStream(rx);
		sulphaBerGetWriter(rx, &writer);
		if (!sulphaWriteMsgHeader(writer, proto->server, 0, NULL, NULL) ||
		    !sulphaBerBeginContainer(writer, 2, 64, &respMarker))
			return NULL;

		SceUInt32 count = sulphaServerGetActiveAgentIds(proto->server, agentIds, sizeof(agentIds));
		sulphaBerWriteTaggedUInt8(writer, 2, seqNum);
		sulphaBerWriteTaggedUInt8(writer, 3, 1);
		if (sulphaBerBeginContainer(writer, 4, 128, &verMarker)) {
			sulphaBerWriteUInt16(writer, 1);
			sulphaBerWriteUInt16(writer, 0);
			sulphaBerWriteUInt32(writer, 1);
			sulphaBerEndContainer(writer, &verMarker);
		}
		sulphaBerWriteRaw(writer, 4, agentIds, count);
		sulphaBerEndContainer(writer, &respMarker);
		dispatchRes = 0;
		break;
	}

	case 7: {
		if (proto->state != SULPHA_PROTO_HELLO)
			break;

		proto->state = SULPHA_PROTO_STREAMING;
		sulphaBerResetBuffer(rx, rx->buffer, rx->capacity);
		sulphaWriteSimpleResponse(rx, proto->server, 0, 8);
		if (proto->server->notifyCallback != NULL)
			proto->server->notifyCallback(NULL);
		dispatchRes = 0;
		break;
	}

	case 15:
		if (proto->state != SULPHA_PROTO_STOPPING)
			sulphaProtoSetState(proto, SULPHA_PROTO_STOPPING);
		break;

	case 19:
		proto->state = SULPHA_PROTO_STOPPED;
		break;

	default:
		break;
	}

	if (dispatchRes < 0)
		return NULL;

	proto->rxOffset = 0;
	proto->rxRemaining = sulphaBerFinalize(rx);
	return (proto->state == SULPHA_PROTO_ERROR) ? NULL : &g_stepSendResp;
}

SceBool sulphaWriteBookmarkMsg(SulphaBerStream *stream, SulphaServer *server,
			       const SceSulphaUTF8 *name, SceInt32 id)
{
	SulphaBerMarker marker;
	SceSize nameLen = 0;

	if (name != NULL) {
		nameLen = sulphaStrlen((const char *)name);
		if (nameLen > 100)
			nameLen = 100;
	}

	const char *errStr = sulphaProtocolErrorString(0);
	if (errStr != NULL)
		sulphaStrlen(errStr);

	if (!sulphaBerBeginContainer(stream, 0, 64, &marker))
		return false;
	if (!sulphaBerWriteTaggedUInt64(stream, 0, sulphaTimerGetTimestamp(&server->timer)) ||
	    !sulphaBerEndContainer(stream, &marker))
		return false;

	SceBool ok = sulphaBerBeginContainer(stream, 26, 64, &marker);
	if (ok && nameLen != 0)
		ok = sulphaBerWriteRaw(stream, 12, name, nameLen);
	if (ok && id >= 0)
		ok = sulphaBerWriteTaggedInt32(stream, 10, id);
	if (!ok || !sulphaBerEndContainer(stream, &marker))
		return false;

	return stream->state != SULPHA_BER_OVERFLOW && stream->state != SULPHA_BER_ERROR;
}

void *sulphaMeasureAlloc(SulphaAllocator *alloc, SceUInt32 size, SceUInt32 align)
{
	if (align != 0)
		size = (size + align - 1U) & ~(align - 1U);
	alloc->current += size;
	return NULL;
}

SceInt32 sulphaServerGetNeededMemory(const SceSulphaConfig *config, SceUInt32 *outSize)
{
	SulphaAllocator alloc;
	SulphaServer tmpServer;
	SceUInt32 poolSize = 0;

	*outSize = 0;
	alloc.magic = SULPHA_ALLOC_MAGIC_MEASURE;
	alloc.totalSize = 0;
	alloc.current = 0;
	alloc.end = 0;
	alloc.base = 0;
	alloc.alloc = sulphaMeasureAlloc;

	sulphaMemset(&tmpServer, 0, sizeof(tmpServer));
	sulphaFileCaptureInit(&tmpServer.fileCapture, &alloc);
	sulphaProtoInit(&tmpServer.protocol, &tmpServer, &alloc);
	sulphaPoolGetNeededMemory((SceInt32)config->bookmarkCount, 152, 4, &poolSize);
	sulphaPoolInit(&tmpServer.bookmarkPool, alloc.alloc(&alloc, poolSize, 4),
		       (SceInt32)config->bookmarkCount, 152, 4);

	*outSize = (SceUInt32)(((alloc.current + 3U) & ~3U) + sizeof(SulphaServer));
	return 0;
}

SceInt32 sulphaServerInit(SulphaServer *server, const SceSulphaConfig *config, SulphaAllocator *alloc)
{
	SceUInt32 poolSize = 0;

	sulphaMemset(server, 0, sizeof(*server));
	sulphaFileCaptureInit(&server->fileCapture, alloc);
	sulphaProtoInit(&server->protocol, server, alloc);
	sulphaPoolGetNeededMemory((SceInt32)config->bookmarkCount, 152, 4, &poolSize);
	sulphaPoolInit(&server->bookmarkPool, alloc->alloc(alloc, poolSize, 4),
		       (SceInt32)config->bookmarkCount, 152, 4);

	if (alloc->magic == SULPHA_ALLOC_MAGIC_MEASURE)
		return 0;

	server->notifyCallback = config->notifyCallback;
	sulphaSpinLockInit(&server->heapLock, "sulpha");

	server->eventQueue.head.ptr = &server->eventQueue.sentinel;
	server->eventQueue.head.tag = 0;
	server->eventQueue.tail.ptr = &server->eventQueue.sentinel;
	server->eventQueue.tail.tag = 0;
	server->eventQueue.count = 0;
	server->eventQueue.sentinel.next = (SulphaQueueNode *)&server->eventQueue;

	sulphaTimerInit(&server->timer);
	sulphaListenerInit(&server->listener, config, server);
	SceInt32 res = sulphaPlatformInit();
	if (res >= 0)
		res = sulphaTimerStart(&server->timer);
	sulphaListenerStart(&server->listener);
	return res;
}

SceInt32 sulphaServerShutdown(SulphaServer *server)
{
	sulphaProtoCloseSocket(&server->protocol);
	sulphaListenerClose(&server->listener);
	sulphaPoolShutdown(&server->bookmarkPool);

	for (SceUInt32 i = 0; i < SULPHA_MAX_AGENTS; i++) {
		if (server->agents[i].slotId != 0)
			sulphaAgentShutdown(&server->agents[i]);
	}

	sulphaSpinLockDestroy(&server->heapLock);
	sulphaTimerStop(&server->timer);
	sulphaTimerReset(&server->timer);
	sulphaFileCaptureShutdown(&server->fileCapture);
	sulphaPlatformShutdown();
	return 0;
}

static inline __attribute__((always_inline)) void sulphaDrainAndDiscardEvents(SulphaServer *server)
{
	for (SceUInt32 i = 0; i < SULPHA_MAX_AGENTS; i++) {
		if (server->agents[i].slotId != 0)
			server->agents[i].capturing = 0;
	}

	while (server->eventQueue.count != 0) {
		SulphaEvent *ev = (SulphaEvent *)sulphaQueuePopB(&server->eventQueue);
		if (ev == NULL)
			break;
		if (ev->agent != NULL)
			sulphaAgentEventDelete(ev->agent, ev, true);
		else
			sulphaPoolFree(&server->bookmarkPool, ev);
	}
}

SceInt32 sulphaServerUpdate(SulphaServer *server)
{
	if (server->protocol.rxStep == NULL && server->protocol.txStep == NULL) {
		if (server->fileCapture.enabled != 0) {
			SceUInt8 agentIds[SULPHA_MAX_AGENTS];
			SceUInt8 count = 0;
			for (SceUInt32 i = 0; i < SULPHA_MAX_AGENTS; i++) {
				if (server->agents[i].slotId != 0 && server->agents[i].disabled == 0)
					agentIds[count++] = server->agents[i].slotId;
			}
			sulphaProtoStartFileCapture(&server->protocol, &server->fileCapture, agentIds, count);
		} else if (sulphaListenerHasPending(&server->listener)) {
			sulphaProtoAccept(&server->protocol, &server->listener);
		} else if (server->protocol.state != SULPHA_PROTO_IDLE) {
			sulphaProtoReset(&server->protocol, false);
			sulphaDrainAndDiscardEvents(server);
		}
	}

	sulphaProtoUpdate(&server->protocol);
	return 0;
}

SceInt32 sulphaServerFileConnect(SulphaServer *server, const char *filename)
{
	if (server->protocol.rxStep != NULL || server->protocol.txStep != NULL)
		return SCE_SULPHA_ERROR_CAPTURE_STARTED;

	SceInt32 res = sulphaFileCaptureOpen(&server->fileCapture, filename);
	if (res >= 0)
		sulphaServerUpdate(server);
	return res;
}

SceInt32 sulphaServerFileDisconnect(SulphaServer *server)
{
	if (server->fileCapture.enabled == 0)
		return SCE_SULPHA_ERROR_NOT_CONNECTED;

	sulphaProtoReset(&server->protocol, true);
	sulphaDrainAndDiscardEvents(server);
	return sulphaFileCaptureClose(&server->fileCapture);
}

SceInt32 sulphaServerIsFileConnected(const SulphaServer *server, SceBool *outConnected)
{
	*outConnected = (server->fileCapture.enabled != 0);
	return 0;
}

SceInt32 sulphaServerAgentsGetNeededMemory(const SulphaServer *server,
					   const SulphaAgentsConfig *cfg,
					   SceUInt32 *outSize)
{
	SulphaAllocator alloc;
	SulphaServer tmpServer;

	*outSize = 0;
	alloc.magic = SULPHA_ALLOC_MAGIC_MEASURE;
	alloc.totalSize = 0;
	alloc.current = 0;
	alloc.end = 0;
	alloc.base = 0;
	alloc.alloc = sulphaMeasureAlloc;

	sulphaMemset(&tmpServer, 0, sizeof(tmpServer));
	sulphaMemcpy(tmpServer.agents, server->agents, sizeof(tmpServer.agents));

	if (cfg->count == 0 || cfg->agents == NULL) {
		*outSize = (SceUInt32)((alloc.current + 3U) & ~3U);
		return 0;
	}

	if (cfg->count + tmpServer.agentCount > SULPHA_MAX_AGENTS)
		return SCE_SULPHA_ERROR_AGENT_LIMIT_REACHED;

	for (SceUInt32 i = 0; i < cfg->count; i++) {
		const SulphaAgentConfig *agentCfg = cfg->agents[i];
		if (agentCfg == NULL || agentCfg->version != SULPHA_AGENT_VERSION)
			return SCE_SULPHA_ERROR_INVALID_AGENT_VERSION;

		SceUInt32 slot = 0;
		while (slot < SULPHA_MAX_AGENTS && tmpServer.agents[slot].slotId != 0)
			slot++;
		if (slot == SULPHA_MAX_AGENTS)
			return SCE_SULPHA_ERROR_AGENT_LIMIT_REACHED;

		tmpServer.agents[slot].slotId = (SceUInt8)(slot + 1);
		SceInt32 res = sulphaAgentInit(&tmpServer.agents[slot], &tmpServer, agentCfg, &alloc);
		if (res < 0)
			return res;
		tmpServer.agentCount++;
	}

	*outSize = (SceUInt32)((alloc.current + 3U) & ~3U);
	return 0;
}

SceInt32 sulphaServerAgentsRegister(SulphaServer *server, const SulphaAgentsConfig *cfg,
				    SulphaAllocator *alloc, SceUInt64 *outHandles)
{
	if (cfg->count == 0 || cfg->agents == NULL)
		return 0;

	if (cfg->count + server->agentCount > SULPHA_MAX_AGENTS)
		return SCE_SULPHA_ERROR_AGENT_LIMIT_REACHED;

	SceInt32 res = 0;
	for (SceUInt32 i = 0; i < cfg->count; i++) {
		const SulphaAgentConfig *agentCfg = cfg->agents[i];
		if (agentCfg == NULL || agentCfg->version != SULPHA_AGENT_VERSION)
			return SCE_SULPHA_ERROR_INVALID_AGENT_VERSION;

		SceUInt32 slot = 0;
		while (slot < SULPHA_MAX_AGENTS && server->agents[slot].slotId != 0)
			slot++;
		if (slot == SULPHA_MAX_AGENTS)
			return SCE_SULPHA_ERROR_AGENT_LIMIT_REACHED;

		server->agents[slot].slotId = (SceUInt8)(slot + 1);
		res = sulphaAgentInit(&server->agents[slot], server, agentCfg, alloc);
		if (res < 0) {
			server->agents[slot].slotId = 0;
			return res;
		}

		server->agentCount++;
		if (outHandles != NULL) {
			outHandles[i] = ((SceUInt64)server->agents[slot].agentTypeId << 8) |
					(SceUInt64)server->agents[slot].slotId;
		}
	}

	return res;
}

SceInt32 sulphaServerAgentsUnregister(SulphaServer *server, const SceUInt64 *handles, SceUInt32 count)
{
	for (SceUInt32 i = 0; i < count; i++) {
		SceUInt8 slotId = (SceUInt8)handles[i];
		SceUInt8 slotIndex = (SceUInt8)(slotId - 1U);
		if (slotIndex >= SULPHA_MAX_AGENTS - 1 || server->agents[slotIndex].slotId != slotId)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		server->agents[slotIndex].capturing = 0;
	}

	sulphaProtoReset(&server->protocol, true);
	sulphaDrainAndDiscardEvents(server);

	for (SceUInt32 i = 0; i < count; i++) {
		SceUInt8 slotId = (SceUInt8)handles[i];
		SceUInt8 slotIndex = (SceUInt8)(slotId - 1U);
		if (slotIndex >= SULPHA_MAX_AGENTS - 1 || server->agents[slotIndex].slotId != slotId)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		sulphaAgentShutdown(&server->agents[slotIndex]);
		server->agentCount--;
	}

	return 0;
}

SceBool sulphaServerIsStreaming(const SulphaServer *server)
{
	return server->protocol.state == SULPHA_PROTO_STREAMING;
}

SceUInt32 sulphaServerGetActiveAgentIds(const SulphaServer *server, SceUInt8 *outIds, SceUInt32 maxCount)
{
	if (maxCount == 0)
		return 0;

	SceUInt32 count = 0;
	for (SceUInt32 i = 0; i < SULPHA_MAX_AGENTS - 1 && count < maxCount; i++) {
		if (server->agents[i].slotId != 0 && server->agents[i].disabled == 0)
			outIds[count++] = server->agents[i].slotId;
	}
	return count;
}

SceInt32 sulphaServerDispatchMessage(SulphaServer *server, SulphaBerStream *stream, SceUInt8 msgType)
{
	SceUInt8 agentId = sulphaBerGetAgentId(stream);

	if (agentId == 0xFFU) {
		if (server->agentCount == 0)
			return 0;

		SceUInt32 activeCount = 0;
		for (SceUInt32 i = 0; i < SULPHA_MAX_AGENTS && activeCount < server->agentCount; i++) {
			if (server->agents[i].slotId != 0 && server->agents[i].disabled == 0)
				activeCount++;
		}
		return (activeCount != 0) ? SULPHA_ERROR_INTERNAL : 0;
	}

	SceUInt8 slotIndex = (SceUInt8)(agentId - 1U);
	if (server->agents[slotIndex].server != server)
		return SULPHA_ERROR_UNKNOWN_AGENT;
	if (server->agents[slotIndex].disabled != 0)
		return 0;

	return sulphaAgentHandleMessage(&server->agents[slotIndex], stream, msgType);
}

SceInt32 sulphaServerSetBookmark(SulphaServer *server, const SceSulphaUTF8 *name, SceInt32 id)
{
	if (server->protocol.state != SULPHA_PROTO_STREAMING)
		return SCE_SULPHA_ERROR_NOT_CONNECTED;

	SulphaEvent *ev = (SulphaEvent *)sulphaPoolAlloc(&server->bookmarkPool);
	if (ev == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_BOOKMARKS;

	SceUInt8 *payloadBuf = (SceUInt8 *)(ev + 1);
	SulphaBerStream stream;

	ev->queueNode.next = NULL;
	ev->queueNode.reserved = 0;
	ev->agent = NULL;
	ev->dataSize = 0;
	ev->payload = payloadBuf;

	sulphaBerInitStream(&stream, payloadBuf, 128);
	sulphaBerSetHeader(&stream, 0, 0);

	if (!sulphaWriteBookmarkMsg(&stream, server, name, id)) {
		sulphaPoolFree(&server->bookmarkPool, ev);
		return SCE_SULPHA_ERROR_CONNECTION;
	}

	ev->dataSize = sulphaBerFinalize(&stream);
	ev->payload = sulphaBerGetBuffer(&stream);
	sulphaQueuePushNode(&server->eventQueue, &ev->queueNode);
	return 0;
}

SceInt32 sulphaServerFreeBookmark(SulphaServer *server, SulphaEvent *event)
{
	sulphaPoolFree(&server->bookmarkPool, event);
	return 0;
}

const char *sulphaProtocolErrorString(SceUInt32 errCode)
{
	switch (errCode) {
	case 0:
		return "NoError";
	case 1:
		return "Ignore";
	case 2:
		return "AlreadyInProgress";
	case 3:
		return "NotNormalOperatingMode";
	case 4:
		return "InternalError";
	case 5:
		return "InvalidCommand";
	case 6:
		return "InvalidParameter";
	case 128:
		return "ExtendedError";
	default:
		return "Unknown";
	}
}

SceBool sulphaVformatString(char *buf, SceInt32 size, const char *fmt, va_list args)
{
	SceInt32 written = sulphaClibVsnprintf(buf, (SceSize)size, fmt, args);
	if (written < 0 || written >= size) {
		buf[size - 1] = '\0';
		return false;
	}
	return true;
}

SceBool sulphaFormatString(char *buf, SceInt32 size, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SceInt32 written = sulphaClibVsnprintf(buf, (SceSize)size, fmt, args);
	va_end(args);

	if (written < 0 || written >= size) {
		buf[size - 1] = '\0';
		return false;
	}
	return true;
}
