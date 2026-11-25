/*
 * SceSulpha agent management, schema serialization, node and event handling.
 */

#include "sulpha_internal.h"

static inline __attribute__((always_inline)) SceUInt16 sulphaFieldEncodedSize(SceUInt8 type, SceUInt16 count)
{
	SceUInt32 bytes;

	switch (type) {
	case SULPHA_FIELD_UINT8:
	case SULPHA_FIELD_INT8:
	case SULPHA_FIELD_STRING:
	case SULPHA_FIELD_BYTES:
		bytes = (SceUInt32)count + 2;
		break;
	case SULPHA_FIELD_UINT16:
	case SULPHA_FIELD_INT16:
		bytes = 2U * count + 2;
		break;
	case SULPHA_FIELD_UINT32:
	case SULPHA_FIELD_FLOAT32:
	case SULPHA_FIELD_INT32:
	case SULPHA_FIELD_NODE_REF:
		bytes = 4U * count + 2;
		break;
	case SULPHA_FIELD_UINT64:
	case SULPHA_FIELD_FLOAT64:
	case SULPHA_FIELD_INT64:
	case SULPHA_FIELD_TIMESTAMP:
		bytes = 8U * count + 2;
		break;
	default:
		return 2;
	}

	return (bytes > 0xFFFEU) ? 0xFFFFU : (SceUInt16)bytes;
}

static inline __attribute__((always_inline)) void sulphaQueueEnqueue(SulphaQueue *queue, SulphaQueueNode *node)
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

SceBool sulphaEncodeFieldList(SulphaBerStream *stream, const SulphaEventDef *eventDef)
{
	SulphaBerMarker listMarker;
	SulphaBerMarker itemMarker;
	const SulphaFieldDef *fields = (const SulphaFieldDef *)eventDef->fields;
	SceUInt32 i;

	if (!sulphaBerBeginContainer(stream, 17, 0, &listMarker))
		return false;

	for (i = 0; i < eventDef->fieldCount; i++) {
		const SulphaFieldDef *field = &fields[i];

		if (!sulphaBerBeginContainer(stream, 16, 0, &itemMarker))
			return false;

		sulphaBerWriteTaggedUInt8(stream, 1, field->id);
		sulphaBerWriteTaggedUInt8(stream, 3, field->type);
		sulphaBerWriteTaggedBytes(stream, 15, field->name, sulphaStrlen(field->name));
		sulphaBerWriteTaggedBytes(stream, 16, field->desc, sulphaStrlen(field->desc));
		if (field->unit != NULL)
			sulphaBerWriteTaggedBytes(stream, 8, field->unit, sulphaStrlen(field->unit));

		sulphaBerEndContainer(stream, &itemMarker);
	}

	return sulphaBerEndContainer(stream, &listMarker);
}

SceInt32 sulphaAgentInit(SulphaAgent *agent, SulphaServer *server,
			 const SulphaAgentConfig *cfg, SulphaAllocator *alloc)
{
	const SulphaAgentCallbacks *cbs = cfg->callbacks;
	SulphaAgentMemReq memReq;
	SulphaAllocator defAlloc;
	SceUInt32 poolSize;
	SceInt32 res;

	if (cbs == NULL || cbs->getDef == NULL || cbs->getMemReq == NULL)
		return SCE_SULPHA_ERROR_INVALID_AGENT_DEF;

	agent->server = server;
	agent->callbacks = cbs;
	agent->agentTypeId = cfg->agentTypeId;
	agent->disabled = 0;
	agent->userData = NULL;

	sulphaMemset(&memReq, 0, sizeof(memReq));
	res = cbs->getMemReq(cfg, &memReq);

	sulphaPoolGetNeededMemory(16, sizeof(SulphaEventEncoder), 4, &poolSize);
	sulphaPoolInit(&agent->encoderPool, alloc->alloc(alloc, poolSize, 4),
		       16, sizeof(SulphaEventEncoder), 4);

	if (memReq.heapSize != 0) {
		sulphaHeapGetNeededMemory(memReq.heapSize, &poolSize);
		sulphaHeapInit(&agent->heap, alloc->alloc(alloc, poolSize, 4), memReq.heapSize);
	}

	if (memReq.eventCount != 0) {
		SceUInt32 evCount = (memReq.eventCount < 128) ? 128 : memReq.eventCount;
		sulphaPoolGetNeededMemory((SceInt32)evCount, sizeof(SulphaEvent), 8, &poolSize);
		sulphaPoolInit(&agent->eventPool, alloc->alloc(alloc, poolSize, 8),
			       (SceInt32)evCount, sizeof(SulphaEvent), 8);
	}

	if (memReq.nodeCount != 0) {
		sulphaPoolGetNeededMemory((SceInt32)memReq.nodeCount, sizeof(SulphaNode), 4, &poolSize);
		sulphaPoolInit(&agent->nodePool, alloc->alloc(alloc, poolSize, 4),
			       (SceInt32)memReq.nodeCount, sizeof(SulphaNode), 4);
	}

	if (memReq.userDataSize != 0) {
		void *userBuf = alloc->alloc(alloc, memReq.userDataSize, 4);
		agent->userData = userBuf;
		if (cbs->init != NULL && userBuf != NULL) {
			sulphaMemset(userBuf, 0, memReq.userDataSize);
			res = cbs->init(agent, cfg, agent->userData, memReq.userDataSize);
		}
	}

	if (res < 0)
		return res;

	if (memReq.defCopySize < sizeof(SulphaAgentDef))
		memReq.defCopySize = sizeof(SulphaAgentDef);

	void *defBuf = alloc->alloc(alloc, memReq.defCopySize, 4);
	sulphaBumpAllocInit(&defAlloc, defBuf, memReq.defCopySize);
	SulphaAgentDef *defCopy = (SulphaAgentDef *)defAlloc.alloc(&defAlloc, sizeof(SulphaAgentDef), 4);
	agent->def = defCopy;
	if (defCopy == NULL)
		return res;

	const SulphaAgentDef *srcDef = NULL;
	SceInt32 defRes = cbs->getDef(agent->userData, &srcDef);
	if (srcDef == NULL || defRes < 0)
		return defRes;

	sulphaMemcpy(defCopy, srcDef, sizeof(SulphaAgentDef));
	defCopy->eventDefs = (SulphaEventDef *)defAlloc.alloc(
		&defAlloc, sizeof(SulphaEventDef) * srcDef->eventDefCount, 4);

	if (defCopy->eventDefs != NULL) {
		for (SceUInt32 i = 0; i < srcDef->eventDefCount; i++) {
			sulphaMemcpy(&defCopy->eventDefs[i], &srcDef->eventDefs[i], sizeof(SulphaEventDef));
			defCopy->eventDefs[i].fields = NULL;
		}

		for (SceUInt32 i = 0; i < srcDef->eventDefCount; i++) {
			const SulphaEventDef *srcEv = &srcDef->eventDefs[i];
			SulphaEventDef *dstEv = &defCopy->eventDefs[i];
			SceUInt16 totalBytes = 64;
			SceUInt16 blockCount = 1;

			if (srcEv->fieldCount == 0)
				continue;

			if ((srcEv->flagsAndBlocks & 8) != 0) {
				SulphaExtFieldDef *dstFields = (SulphaExtFieldDef *)defAlloc.alloc(
					&defAlloc, sizeof(SulphaExtFieldDef) * srcEv->fieldCount, 4);
				const SulphaExtFieldDef *srcFields = (const SulphaExtFieldDef *)srcEv->fields;
				dstEv->fields = dstFields;
				for (SceUInt32 j = 0; j < srcEv->fieldCount; j++) {
					sulphaMemcpy(&dstFields[j], &srcFields[j], sizeof(SulphaExtFieldDef));
					totalBytes = (SceUInt16)(totalBytes +
						sulphaFieldEncodedSize(dstFields[j].type, dstFields[j].count));
				}
				blockCount = (SceUInt16)((totalBytes + 63U) >> 6);
			} else {
				SulphaFieldDef *dstFields = (SulphaFieldDef *)defAlloc.alloc(
					&defAlloc, sizeof(SulphaFieldDef) * srcEv->fieldCount, 4);
				const SulphaFieldDef *srcFields = (const SulphaFieldDef *)srcEv->fields;
				dstEv->fields = dstFields;
				for (SceUInt32 j = 0; j < srcEv->fieldCount; j++) {
					sulphaMemcpy(&dstFields[j], &srcFields[j], sizeof(SulphaFieldDef));
					totalBytes = (SceUInt16)(totalBytes +
						sulphaFieldEncodedSize(dstFields[j].type, dstFields[j].count));
				}
				blockCount = (SceUInt16)((totalBytes + 63U) >> 6);
			}

			dstEv->flagsAndBlocks = (SceUInt16)((dstEv->flagsAndBlocks & 0xC00FU) |
				((blockCount & 0x3FFU) << 4));
		}

		for (SceUInt32 i = 0; i < defCopy->eventDefCount; i++) {
			if (defCopy->eventDefs[i].name != NULL)
				sulphaStrlen(defCopy->eventDefs[i].name);
		}
	}

	for (SceUInt32 i = 0; i < srcDef->subEventDefCount; i++) {
		if (srcDef->subEventDefs[i].name != NULL)
			sulphaStrlen(srcDef->subEventDefs[i].name);
	}

	return defRes;
}

SceInt32 sulphaAgentShutdown(SulphaAgent *agent)
{
	SceInt32 res = 0;

	if (agent->callbacks->shutdown != NULL)
		res = agent->callbacks->shutdown(agent, agent->userData);

	agent->server = NULL;
	agent->slotId = 0;
	return res;
}

static inline __attribute__((always_inline)) void sulphaEncodeTypedFieldVal(SulphaBerStream *stream, SceUInt8 fieldType,
				      SceUInt8 tag, SceUInt32 rawVal)
{
	switch (fieldType) {
	case SULPHA_FIELD_UINT8:
		sulphaBerWriteTaggedUInt8(stream, tag, (SceUInt8)rawVal);
		break;
	case SULPHA_FIELD_UINT16:
		sulphaBerWriteTaggedUInt16(stream, tag, (SceUInt16)rawVal);
		break;
	case SULPHA_FIELD_UINT32:
		sulphaBerWriteTaggedUInt32(stream, tag, rawVal);
		break;
	case SULPHA_FIELD_FLOAT32: {
		union { SceUInt32 u; float f; } conv = { rawVal };
		sulphaBerWriteTaggedFloat32(stream, tag, conv.f);
		break;
	}
	case SULPHA_FIELD_INT8:
		sulphaBerWriteTaggedInt8(stream, tag, (SceInt8)rawVal);
		break;
	case SULPHA_FIELD_INT16:
		sulphaBerWriteTaggedInt16(stream, tag, (SceInt16)rawVal);
		break;
	case SULPHA_FIELD_INT32:
		sulphaBerWriteTaggedInt32(stream, tag, (SceInt32)rawVal);
		break;
	default:
		break;
	}
}

SceInt32 sulphaAgentHandleMessage(SulphaAgent *agent, SulphaBerStream *stream, SceUInt8 msgType)
{
	switch (msgType) {
	case 3: {
		SceUInt8 seqNum = 0;
		SceUInt8 includeSchema = 1;
		SulphaBerReader *reader = NULL;
		SulphaBerStream *writer = NULL;
		SulphaBerMarker respMarker, schemaMarker, verMarker, listMarker, subListMarker, itemMarker;
		SceUInt32 uuidBe[4];

		sulphaBerGetReader(stream, &reader);
		do {
			sulphaBerReadUInt8(reader, 2, &seqNum);
			sulphaBerReadUInt8(reader, 3, &includeSchema);
		} while (reader->state != SULPHA_BER_EOF &&
			 reader->pos < reader->end &&
			 reader->state != SULPHA_BER_ERROR);

		sulphaBerResetStream(stream);
		sulphaBerGetWriter(stream, &writer);
		sulphaWriteMsgHeader(writer, agent->server, 0, NULL, NULL);
		sulphaBerBeginContainer(stream, 4, 64, &respMarker);

		const SulphaAgentDef *def = agent->def;
		uuidBe[0] = __builtin_bswap32(def->uuid[0]);
		uuidBe[1] = ((SceUInt32)__builtin_bswap16((SceUInt16)def->uuid[1])) |
			    ((SceUInt32)__builtin_bswap16((SceUInt16)(def->uuid[1] >> 16)) << 16);
		uuidBe[2] = def->uuid[2];
		uuidBe[3] = def->uuid[3];

		if (respMarker.prevOffset != 0xFFFFU && respMarker.prevLen != 0xFFFFU) {
			sulphaBerWriteTaggedUInt8(writer, 2, seqNum);
			sulphaBerWriteTaggedBytes(writer, 3, uuidBe, sizeof(uuidBe));

			if (includeSchema == 1 &&
			    sulphaBerBeginContainer(writer, 27, 64, &schemaMarker)) {
				sulphaBerWriteTaggedUInt32(writer, 3, def->typeId);
				if (sulphaBerBeginContainer(writer, 4, 128, &verMarker)) {
					sulphaBerWriteUInt16(writer, def->versionMajor);
					sulphaBerWriteUInt16(writer, def->versionMinor);
					sulphaBerWriteUInt32(writer, def->versionBuild);
					sulphaBerEndContainer(writer, &verMarker);

					if (def->name != NULL) {
						sulphaBerWriteTaggedBytes(writer, 15, def->name, sulphaStrlen(def->name));
						if (def->desc != NULL)
							sulphaBerWriteTaggedBytes(writer, 16, def->desc, sulphaStrlen(def->desc));

						if (def->eventDefCount != 0 &&
						    sulphaBerBeginContainer(writer, 17, 128, &listMarker)) {
							for (SceUInt32 i = 0; i < def->eventDefCount; i++) {
								const SulphaEventDef *evDef = &def->eventDefs[i];
								SulphaBerMarker evMarker;

								sulphaBerBeginContainer(writer, 16, 0, &evMarker);
								if (evMarker.prevOffset == 0xFFFFU || evMarker.prevLen == 0xFFFFU)
									break;

								sulphaBerWriteTaggedUInt8(writer, 1, evDef->id);
								sulphaBerWriteTaggedUInt8(writer, 6, (SceUInt8)(evDef->flagsAndBlocks & 1));
								sulphaBerWriteTaggedUInt8(writer, 12, (SceUInt8)((evDef->flagsAndBlocks & 4) != 0));
								SceBool enabled = (evDef->flagsAndBlocks & 2) != 0;
								if ((SceBool)(evDef->flagsAndBlocks & 1) != enabled)
									sulphaBerWriteTaggedUInt8(writer, 7, (SceUInt8)enabled);
								sulphaBerWriteTaggedBytes(writer, 15, evDef->name, sulphaStrlen(evDef->name));
								sulphaBerWriteTaggedBytes(writer, 16, evDef->desc, sulphaStrlen(evDef->desc));

								if ((evDef->flagsAndBlocks & 8) != 0) {
									const SulphaExtFieldDef *extFields = (const SulphaExtFieldDef *)evDef->fields;
									if (!sulphaBerBeginContainer(writer, 17, 0, &subListMarker))
										break;
									SceBool ok = true;
									for (SceUInt32 j = 0; j < evDef->fieldCount; j++) {
										const SulphaExtFieldDef *f = &extFields[j];
										if (!sulphaBerBeginContainer(writer, 16, 0, &itemMarker)) {
											ok = false;
											break;
										}
										sulphaBerWriteTaggedUInt8(writer, 1, f->id);
										sulphaBerWriteTaggedUInt8(writer, 3, f->type);
										sulphaBerWriteTaggedBytes(writer, 15, f->name, sulphaStrlen(f->name));
										sulphaBerWriteTaggedBytes(writer, 16, f->desc, sulphaStrlen(f->desc));
										sulphaEncodeTypedFieldVal(writer, f->type, 18, f->minVal);
										sulphaEncodeTypedFieldVal(writer, f->type, 19, f->maxVal);
										sulphaEncodeTypedFieldVal(writer, f->type, 20, f->defaultVal);
										if (f->unit != NULL)
											sulphaBerWriteTaggedBytes(writer, 8, f->unit, sulphaStrlen(f->unit));
										sulphaBerEndContainer(writer, &itemMarker);
									}
									if (!ok || !sulphaBerEndContainer(writer, &subListMarker))
										break;
								} else {
									if (!sulphaEncodeFieldList(writer, evDef))
										break;
								}

								sulphaBerEndContainer(writer, &evMarker);
							}
							sulphaBerEndContainer(writer, &listMarker);
						}

						if (def->subEventDefCount != 0) {
							if (sulphaBerBeginContainer(writer, 22, 128, &subListMarker)) {
								for (SceUInt32 i = 0; i < def->subEventDefCount; i++) {
									const SulphaEventDef *subDef = &def->subEventDefs[i];
									sulphaBerBeginContainer(writer, 16, 0, &itemMarker);
									if (itemMarker.prevOffset == 0xFFFFU || itemMarker.prevLen == 0xFFFFU)
										break;
									sulphaBerWriteTaggedUInt8(writer, 1, subDef->id);
									sulphaBerWriteTaggedBytes(writer, 15, subDef->name, sulphaStrlen(subDef->name));
									sulphaBerWriteTaggedBytes(writer, 16, subDef->desc, sulphaStrlen(subDef->desc));
									if (!sulphaEncodeFieldList(writer, subDef))
										break;
									sulphaBerEndContainer(writer, &itemMarker);
								}
								sulphaBerEndContainer(writer, &subListMarker);
							} else {
								sulphaBerEndContainer(writer, &respMarker);
								return 0;
							}
						}

						if (def->enumDefCount != 0) {
							if (sulphaBerBeginContainer(writer, 23, 128, &listMarker)) {
								for (SceUInt32 i = 0; i < def->enumDefCount; i++) {
									const SulphaEnumDef *enumDef = &def->enumDefs[i];
									SulphaBerMarker evMarker;

									sulphaBerBeginContainer(writer, 16, 0, &evMarker);
									if (evMarker.prevOffset == 0xFFFFU || evMarker.prevLen == 0xFFFFU)
										break;
									sulphaBerWriteTaggedUInt8(writer, 1, enumDef->id);
									sulphaBerWriteTaggedBytes(writer, 15, enumDef->name, sulphaStrlen(enumDef->name));
									sulphaBerWriteTaggedBytes(writer, 16, enumDef->desc, sulphaStrlen(enumDef->desc));
									if (!sulphaBerBeginContainer(writer, 17, 0, &subListMarker))
										break;
									SceBool enumOk = true;
									for (SceInt32 j = 0; j < enumDef->valueCount; j++) {
										const SulphaEnumValueDef *val = &enumDef->values[j];
										if (!sulphaBerBeginContainer(writer, 16, 0, &itemMarker)) {
											enumOk = false;
											break;
										}
										sulphaBerWriteTaggedInt32(writer, 1, val->value);
										sulphaBerWriteTaggedBytes(writer, 15, val->name, sulphaStrlen(val->name));
										sulphaBerWriteTaggedBytes(writer, 16, val->desc, sulphaStrlen(val->desc));
										sulphaBerEndContainer(writer, &itemMarker);
									}
									if (!enumOk || !sulphaBerEndContainer(writer, &subListMarker))
										break;
									sulphaBerEndContainer(writer, &evMarker);
								}
								sulphaBerEndContainer(writer, &listMarker);
							} else {
								sulphaBerEndContainer(writer, &respMarker);
								return 0;
							}
						}
						sulphaBerEndContainer(writer, &schemaMarker);
					}
				}
			}
		}

		sulphaBerEndContainer(writer, &respMarker);
		return 0;
	}

	case 5: {
		SulphaBerReader *reader = NULL;
		SulphaBerStream *writer = NULL;
		SulphaAgentDef *def = agent->def;
		SceUInt8 evId = 0;

		sulphaBerGetReader(stream, &reader);

		for (SceUInt32 i = 0; i < def->eventDefCount; i++) {
			if ((def->eventDefs[i].flagsAndBlocks & 1) == 0)
				def->eventDefs[i].flagsAndBlocks &= (SceUInt16)~2U;
		}

		if ((reader->data[reader->pos] & 0x1F) != 0x11 || sulphaBerReaderEnter(reader) == 0)
			return SULPHA_ERROR_INTERNAL;

		SceUInt32 errCode = 0;
		while (sulphaBerReaderNext(reader) &&
		       reader->state != SULPHA_BER_EOF &&
		       reader->pos < reader->end) {
			if ((reader->data[reader->pos] & 0x1F) != 0x10)
				continue;

			SulphaBerReader elemReader;
			sulphaBerReaderCopy(&elemReader, reader);
			if ((SceInt32)sulphaBerReaderEnter(&elemReader) == -1 ||
			    !sulphaBerReaderNext(&elemReader) ||
			    !sulphaBerReadUInt8(&elemReader, 1, &evId))
				continue;

			SceBool found = false;
			for (SceUInt32 i = 0; i < def->eventDefCount; i++) {
				if (def->eventDefs[i].id == evId) {
					def->eventDefs[i].flagsAndBlocks |= 2U;
					found = true;
					break;
				}
			}
			if (!found)
				errCode = 6;
		}

		sulphaBerResetStream(stream);
		sulphaBerGetWriter(stream, &writer);
		sulphaWriteSimpleResponse(writer, agent->server, errCode, 6);
		return 0;
	}

	case 11: {
		SulphaBerStream *writer = NULL;

		sulphaBerResetStream(stream);
		sulphaBerGetWriter(stream, &writer);
		sulphaWriteSimpleResponse(writer, agent->server, 0, 12);

		if (agent->callbacks->onStart != NULL)
			agent->callbacks->onStart(agent, agent->userData);
		agent->capturing = 1;
		if (agent->callbacks->onStarted != NULL)
			agent->callbacks->onStarted(agent, agent->userData);
		return 0;
	}

	case 15: {
		SulphaBerStream *writer = NULL;

		agent->capturing = 0;
		sulphaBerResetStream(stream);
		sulphaBerGetWriter(stream, &writer);
		sulphaWriteSimpleResponse(writer, agent->server, 0, 16);
		return 0;
	}

	default: {
		if (agent->callbacks != NULL && agent->callbacks->onCommand != NULL) {
			SulphaBerReader *reader = NULL;
			sulphaBerGetReader(stream, &reader);
			if (sulphaBerReaderEnter(reader) != 0)
				agent->callbacks->onCommand(agent, agent->userData, reader);
			sulphaBerResetStream(stream);
		}
		return 0;
	}
	}
}

SceInt32 sulphaAgentNodeNew(SulphaNode **outNode, SulphaAgent *agent, const SulphaNode *parent)
{
	SulphaNode *node = (SulphaNode *)sulphaPoolAlloc(&agent->nodePool);
	if (node == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	*outNode = node;
	node->agent = agent;

	SceUInt32 nodeId;
	do {
		SceUInt32 cur = g_sulphaNextNodeId;
		nodeId = cur + 1;
		if (sulphaAtomicLoad32(&g_sulphaNextNodeId) != cur)
			continue;
	} while (sulphaAtomicStore32(nodeId, &g_sulphaNextNodeId) != 0);

	node->nodeId = nodeId;
	node->parentId = (parent != NULL) ? parent->nodeId : 0;
	return 0;
}

SceInt32 sulphaAgentNodeDelete(SulphaAgent *agent, SulphaNode *node, SceUInt8 eventId)
{
	SulphaServer *server = agent->server;

	if (sulphaServerIsStreaming(server) && sulphaPoolIsValid(&agent->eventPool) >= 0) {
		SulphaEvent *ev = (SulphaEvent *)sulphaPoolAlloc(&agent->eventPool);
		if (ev != NULL) {
			SulphaBerStream stream;
			SulphaBerMarker msgMarker, bodyMarker, nodeMarker;
			SceUInt8 emptyPayload;

			ev->queueNode.next = NULL;
			ev->queueNode.reserved = 0;
			ev->agent = agent;
			ev->dataSize = 0;
			ev->payload = &emptyPayload;

			sulphaSpinLockAcquire(&server->heapLock);
			void *buf = sulphaHeapAlloc(&agent->heap, 64);
			sulphaSpinLockRelease(&server->heapLock);

			if (buf != NULL) {
				sulphaBerInitStream(&stream, buf, 64);
				sulphaBerSetHeader(&stream, 0, agent->slotId);
				sulphaWriteMsgHeader(&stream, agent->server, 0, NULL, NULL);
				sulphaBerBeginContainer(&stream, 24, 64, &msgMarker);
				sulphaBerBeginContainer(&stream, 28, 64, &bodyMarker);
				sulphaBerWriteTaggedUInt8(&stream, 1, eventId);
				sulphaBerEndContainer(&stream, &bodyMarker);
				sulphaBerBeginContainer(&stream, 30, 64, &nodeMarker);
				sulphaBerWriteTaggedUInt32(&stream, 10, node->nodeId);
				sulphaBerEndContainer(&stream, &nodeMarker);
				sulphaBerEndContainer(&stream, &msgMarker);

				ev->dataSize = sulphaBerFinalize(&stream);
				ev->payload = sulphaBerGetBuffer(&stream);
				sulphaQueueEnqueue(&server->eventQueue, &ev->queueNode);
			} else {
				sulphaPoolFree(&agent->eventPool, ev);
			}
		}
	}

	sulphaPoolFree(&agent->nodePool, node);
	return 0;
}

SceInt32 sulphaAgentEventNew(SulphaAgent *agent, SulphaEvent **outEvent, SceUInt8 eventId)
{
	const SulphaAgentDef *def = agent->def;
	const SulphaEventDef *evDef = NULL;

	for (SceUInt32 i = 0; i < def->eventDefCount; i++) {
		if (def->eventDefs[i].id == eventId) {
			evDef = &def->eventDefs[i];
			break;
		}
	}
	if (evDef == NULL) {
		*outEvent = NULL;
		return SCE_SULPHA_ERROR_INVALID_EVENT_ID;
	}

	if (!sulphaServerIsStreaming(agent->server) ||
	    !agent->capturing ||
	    (evDef->flagsAndBlocks & 2) == 0) {
		*outEvent = NULL;
		return SCE_SULPHA_ERROR_NOT_CONNECTED;
	}

	SulphaEvent *ev = (SulphaEvent *)sulphaPoolAlloc(&agent->eventPool);
	if (ev == NULL) {
		*outEvent = NULL;
		return SCE_SULPHA_ERROR_NOT_CONNECTED;
	}

	SulphaServer *server = agent->server;
	SulphaEventEncoder *enc = (SulphaEventEncoder *)sulphaPoolAlloc(&agent->encoderPool);
	ev->queueNode.next = NULL;
	ev->queueNode.reserved = 0;
	ev->agent = agent;
	ev->dataSize = 0;
	ev->payload = enc;

	if (enc == NULL) {
		if (agent->server != NULL)
			sulphaPoolFree(&agent->eventPool, ev);
		*outEvent = NULL;
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;
	}

	enc->eventDef = evDef;
	enc->stream.buffer = NULL;
	enc->fieldsMarker.prevOffset = 0xFFFFU;
	enc->fieldCount = 0;

	SceUInt32 bufSize = ((SceUInt32)(evDef->flagsAndBlocks >> 4) & 0x3FFU) << 6;
	if (bufSize != 0) {
		sulphaSpinLockAcquire(&server->heapLock);
		void *buf = sulphaHeapAlloc(&agent->heap, bufSize);
		sulphaSpinLockRelease(&server->heapLock);

		if (buf != NULL) {
			sulphaBerInitStream(&enc->stream, buf, bufSize);
			sulphaBerSetHeader(&enc->stream, 0, agent->slotId);
			sulphaWriteMsgHeader(&enc->stream, agent->server, 0, NULL, NULL);
			sulphaBerBeginContainer(&enc->stream, 24, 64, &enc->msgMarker);
			sulphaBerBeginContainer(&enc->stream, 28, 64, &enc->bodyMarker);
			sulphaBerWriteTaggedUInt8(&enc->stream, 1, evDef->id);
			*outEvent = ev;
			return 0;
		}
	}

	if (agent->server != NULL) {
		sulphaSpinLockAcquire(&agent->server->heapLock);
		if (ev->dataSize != 0) {
			sulphaHeapFree(&agent->heap, ev->payload);
		} else if (enc->stream.buffer != NULL) {
			sulphaHeapFree(&agent->heap, enc->stream.buffer);
		}
		sulphaSpinLockRelease(&agent->server->heapLock);
		if (ev->dataSize == 0)
			sulphaPoolFree(&agent->encoderPool, enc);
		sulphaPoolFree(&agent->eventPool, ev);
	}

	*outEvent = NULL;
	return SULPHA_ERROR_INTERNAL;
}

SceInt32 sulphaAgentEventAdd(SulphaEvent *event, SceUInt8 fieldId, const void *data, SceUInt32 size)
{
	SulphaEventEncoder *enc = (SulphaEventEncoder *)event->payload;
	const SulphaEventDef *evDef = enc->eventDef;
	const SulphaFieldDef *field = NULL;
	SceUInt32 swapped[2];

	if (evDef == NULL)
		return 0;
	if ((evDef->flagsAndBlocks & 2) == 0)
		return 0;

	if ((evDef->flagsAndBlocks & 8) != 0) {
		const SulphaExtFieldDef *extFields = (const SulphaExtFieldDef *)evDef->fields;
		for (SceUInt32 i = 0; i < evDef->fieldCount; i++) {
			if (extFields[i].id == fieldId) {
				field = (const SulphaFieldDef *)&extFields[i];
				break;
			}
		}
	} else {
		const SulphaFieldDef *fields = (const SulphaFieldDef *)evDef->fields;
		for (SceUInt32 i = 0; i < evDef->fieldCount; i++) {
			if (fields[i].id == fieldId) {
				field = &fields[i];
				break;
			}
		}
	}

	if (field == NULL || data == NULL)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	if (enc->fieldCount == 0)
		sulphaBerBeginContainer(&enc->stream, 17, 0, &enc->fieldsMarker);

	switch (field->type) {
	case SULPHA_FIELD_UINT8:
	case SULPHA_FIELD_INT8:
		if (size != 1)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		sulphaBerWriteRaw(&enc->stream, fieldId, data, size);
		break;

	case SULPHA_FIELD_UINT16:
	case SULPHA_FIELD_INT16: {
		if (size != 2)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		SceUInt16 beVal = __builtin_bswap16(*(const SceUInt16 *)data);
		sulphaBerWriteRaw(&enc->stream, fieldId, &beVal, 2);
		break;
	}

	case SULPHA_FIELD_UINT32:
	case SULPHA_FIELD_FLOAT32:
	case SULPHA_FIELD_INT32:
		if (size != 4)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		swapped[0] = __builtin_bswap32(*(const SceUInt32 *)data);
		sulphaBerWriteRaw(&enc->stream, fieldId, swapped, 4);
		break;

	case SULPHA_FIELD_UINT64:
	case SULPHA_FIELD_FLOAT64:
	case SULPHA_FIELD_INT64:
	case SULPHA_FIELD_TIMESTAMP: {
		if (size != 8)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		const SceUInt32 *words = (const SceUInt32 *)data;
		swapped[0] = __builtin_bswap32(words[1]);
		swapped[1] = __builtin_bswap32(words[0]);
		sulphaBerWriteRaw(&enc->stream, fieldId, swapped, 8);
		break;
	}

	case SULPHA_FIELD_NODE_REF: {
		if (size != 4)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		const SulphaNode *node = *(const SulphaNode *const *)data;
		swapped[0] = (node != NULL) ? __builtin_bswap32(node->nodeId) : 0;
		sulphaBerWriteRaw(&enc->stream, fieldId, swapped, 4);
		break;
	}

	case SULPHA_FIELD_STRING:
	case SULPHA_FIELD_BYTES:
		if (size > field->count)
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		sulphaBerWriteRaw(&enc->stream, fieldId, data, size);
		break;

	default:
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	}

	enc->fieldCount++;
	return 0;
}

SceInt32 sulphaAgentEventDelete(SulphaAgent *agent, SulphaEvent *event, SceBool force)
{
	if (!force && event->queueNode.next != NULL)
		return 0;

	SulphaServer *server = agent->server;
	if (server == NULL)
		return 0;

	if (event->payload != NULL) {
		sulphaSpinLockAcquire(&server->heapLock);
		if (event->dataSize != 0) {
			sulphaHeapFree(&agent->heap, event->payload);
		} else {
			SulphaEventEncoder *enc = (SulphaEventEncoder *)event->payload;
			if (enc->stream.buffer != NULL)
				sulphaHeapFree(&agent->heap, enc->stream.buffer);
		}
		sulphaSpinLockRelease(&server->heapLock);

		if (event->dataSize == 0)
			sulphaPoolFree(&agent->encoderPool, event->payload);
	}

	sulphaPoolFree(&agent->eventPool, event);
	return 0;
}

SceInt32 sulphaAgentEventReport(SulphaAgent *agent, SulphaEvent *event, const SulphaNode *node)
{
	SulphaServer *server = agent->server;
	SulphaEventEncoder *enc = (SulphaEventEncoder *)event->payload;

	if (event->queueNode.next != NULL)
		return SCE_SULPHA_ERROR_EVENT_ALREADY_SENT;

	if (sulphaServerIsStreaming(server) &&
	    enc->eventDef != NULL &&
	    (enc->eventDef->flagsAndBlocks & 2) != 0 &&
	    event->agent != NULL &&
	    event->agent->capturing) {
		if (enc->fieldCount != 0 &&
		    enc->fieldsMarker.prevOffset != 0xFFFFU &&
		    enc->fieldsMarker.prevLen != 0xFFFFU) {
			sulphaBerEndContainer(&enc->stream, &enc->fieldsMarker);
			enc->fieldCount = 0;
		}

		sulphaBerEndContainer(&enc->stream, &enc->bodyMarker);

		if (node != NULL) {
			SulphaBerMarker nodeMarker;
			sulphaBerBeginContainer(&enc->stream, 29, 64, &nodeMarker);
			sulphaBerWriteTaggedUInt32(&enc->stream, 10, node->nodeId);
			sulphaBerWriteTaggedUInt32(&enc->stream, 11, node->parentId);
			sulphaBerEndContainer(&enc->stream, &nodeMarker);
		}

		sulphaBerEndContainer(&enc->stream, &enc->msgMarker);
		event->dataSize = sulphaBerFinalize(&enc->stream);
		event->payload = sulphaBerGetBuffer(&enc->stream);
		sulphaPoolFree(&agent->encoderPool, enc);

		sulphaQueueEnqueue(&server->eventQueue, &event->queueNode);
		return 0;
	}

	if (agent->server == NULL)
		return SCE_SULPHA_ERROR_INVALID_AGENT_DEF;

	if (enc != NULL) {
		sulphaSpinLockAcquire(&agent->server->heapLock);
		if (event->dataSize != 0) {
			sulphaHeapFree(&agent->heap, event->payload);
		} else if (enc->stream.buffer != NULL) {
			sulphaHeapFree(&agent->heap, enc->stream.buffer);
		}
		sulphaSpinLockRelease(&agent->server->heapLock);

		if (event->dataSize == 0)
			sulphaPoolFree(&agent->encoderPool, enc);
	}

	sulphaPoolFree(&agent->eventPool, event);
	return 0;
}

void *sulphaBumpAlloc(SulphaAllocator *alloc, SceUInt32 size, SceUInt32 align)
{
	SceUIntPtr cur = alloc->current;
	SceUIntPtr aligned;
	SceUInt32 step;

	if (align != 0) {
		step = (size + align - 1U) & ~(align - 1U);
		aligned = (cur + align - 1U) & ~(SceUIntPtr)(align - 1U);
	} else {
		step = size;
		aligned = cur;
	}

	alloc->current = cur + step;
	if (alloc->base == 0 || size == 0)
		return NULL;
	return (void *)aligned;
}

SulphaAllocator *sulphaBumpAllocInit(SulphaAllocator *alloc, void *buf, SceUInt32 size)
{
	alloc->magic = SULPHA_ALLOC_MAGIC_LIVE;
	alloc->totalSize = size;
	alloc->current = (SceUIntPtr)buf;
	alloc->end = (SceUIntPtr)buf + size;
	alloc->base = (SceUIntPtr)buf;
	alloc->alloc = sulphaBumpAlloc;
	return alloc;
}
