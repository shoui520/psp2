/*
 * SceSulpha BER/TLV stream encoder and decoder.
 */

#include "sulpha_internal.h"

static inline SceUInt32 sulphaBerHeaderSize(const SceUInt8 *data, SceUInt32 pos)
{
	SceUInt8 lenByte = data[pos + 1];
	return ((lenByte & 0x80U) != 0) ? (SceUInt32)(lenByte & 0x7FU) + 2U : 2U;
}

static inline __attribute__((always_inline)) SceBool sulphaBerValidateCurrent(const SulphaBerReader *reader)
{
	if (reader->data == NULL)
		return false;

	SceUInt32 hdrEnd = reader->pos + sulphaBerHeaderSize(reader->data, reader->pos);
	if (hdrEnd > reader->end)
		return false;

	return (hdrEnd + sulphaBerReadLength(reader)) <= reader->end;
}

SceUInt32 sulphaBerReadLength(const SulphaBerReader *reader)
{
	const SceUInt8 *p = &reader->data[reader->pos];
	SceUInt8 lenByte = p[1];

	if ((lenByte & 0x80U) == 0)
		return lenByte;

	switch (lenByte & 0x7FU) {
	case 0:
		return 0;
	case 1:
		return p[2];
	case 2:
		return ((SceUInt32)p[2] << 8) | p[3];
	case 3:
		return ((SceUInt32)p[2] << 16) | ((SceUInt32)p[3] << 8) | p[4];
	case 4:
		return ((SceUInt32)p[2] << 24) | ((SceUInt32)p[3] << 16) |
		       ((SceUInt32)p[4] << 8) | p[5];
	default:
		return 0xFFFFU;
	}
}

SceBool sulphaBerWriteTagValue(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len)
{
	SceUInt8 lenBuf[4];
	SceUInt8 lenPrefix = 0;
	SceUInt32 lenExtra = 0;
	SceUInt32 lenFieldBytes = 0;

	if (len != 0) {
		if (len < 0x80U) {
			lenPrefix = (SceUInt8)len;
			lenFieldBytes = 1;
			lenExtra = 0;
		} else if (len < 0x100U) {
			lenPrefix = 0x81U;
			lenBuf[0] = (SceUInt8)len;
			lenFieldBytes = 2;
			lenExtra = 1;
		} else if (len < 0x10000U) {
			lenPrefix = 0x82U;
			lenBuf[0] = (SceUInt8)(len >> 8);
			lenBuf[1] = (SceUInt8)len;
			lenFieldBytes = 3;
			lenExtra = 2;
		} else {
			lenPrefix = 0x84U;
			lenBuf[0] = (SceUInt8)(len >> 24);
			lenBuf[1] = (SceUInt8)(len >> 16);
			lenBuf[2] = (SceUInt8)(len >> 8);
			lenBuf[3] = (SceUInt8)len;
			lenFieldBytes = 5;
			lenExtra = 4;
		}
	}

	if (stream->state > SULPHA_BER_OK)
		return false;

	if (stream->capacity < stream->writePos + 1U + lenFieldBytes + len) {
		stream->state = SULPHA_BER_ERROR;
		return false;
	}

	stream->state = SULPHA_BER_OK;
	stream->buffer[stream->writePos++] = tag;
	stream->containerLen++;
	stream->buffer[stream->writePos++] = lenPrefix;
	stream->containerLen++;

	if (lenFieldBytes > 1 && lenExtra != 0) {
		for (SceUInt32 i = 0; i < lenExtra; i++) {
			stream->buffer[stream->writePos++] = lenBuf[i];
			stream->containerLen++;
		}
	}

	if (len != 0 && data != NULL) {
		if (len > 0x40U) {
			sulphaMemcpy(&stream->buffer[stream->writePos], data, len);
			stream->writePos += len;
			stream->containerLen += len;
		} else {
			const SceUInt8 *src = (const SceUInt8 *)data;
			for (SceUInt32 i = 0; i < len; i++) {
				stream->buffer[stream->writePos++] = src[i];
				stream->containerLen++;
			}
		}
	}

	return true;
}

SulphaBerReader *sulphaBerReaderCopy(SulphaBerReader *dst, const SulphaBerReader *src)
{
	*dst = *src;
	return dst;
}

SceBool sulphaBerReaderNext(SulphaBerReader *reader)
{
	if (reader->state == SULPHA_BER_INIT) {
		if (reader->pos >= reader->end) {
			reader->state = SULPHA_BER_EOF;
			return false;
		}
		if (sulphaBerValidateCurrent(reader)) {
			reader->state = SULPHA_BER_OK;
			return true;
		}
		reader->state = SULPHA_BER_ERROR;
		return false;
	}

	if (reader->state != SULPHA_BER_OK)
		return false;

	SceUInt32 hdrSize = sulphaBerHeaderSize(reader->data, reader->pos);
	SceUInt32 payloadLen = sulphaBerReadLength(reader);
	reader->pos += hdrSize + payloadLen;

	if (reader->pos >= reader->end) {
		reader->state = SULPHA_BER_EOF;
		return false;
	}

	if (!sulphaBerValidateCurrent(reader)) {
		reader->state = SULPHA_BER_ERROR;
		return false;
	}

	return reader->state == SULPHA_BER_OK;
}

SceUInt32 sulphaBerReaderEnter(SulphaBerReader *reader)
{
	SceUInt32 origPos = reader->pos;
	SceUInt8 tag = reader->data[origPos];

	if ((tag & 0xC0U) != 0x40U && (SceUInt32)((tag & 0x1FU) - 16U) > 1U)
		return 0xFFFFU;

	SceUInt32 payloadLen = sulphaBerReadLength(reader);
	SceUInt32 hdrSize = sulphaBerHeaderSize(reader->data, origPos);
	reader->pos = origPos + hdrSize;

	if (payloadLen != 0 &&
	    (reader->data == NULL ||
	     reader->state == SULPHA_BER_EOF ||
	     reader->pos >= reader->end ||
	     !sulphaBerValidateCurrent(reader))) {
		reader->state = SULPHA_BER_ERROR;
		return 0xFFFFU;
	}

	reader->state = SULPHA_BER_INIT;
	return origPos;
}

SceBool sulphaBerReadUInt8(SulphaBerReader *reader, SceUInt8 expectedTag, SceUInt8 *outVal)
{
	SceUInt8 tag = reader->data[reader->pos];
	if ((tag & 0xC0U) != 0x80U || (tag & 0x1FU) != expectedTag)
		return false;

	SceUInt32 len = sulphaBerReadLength(reader);
	if (len == 1) {
		SceUInt32 hdrSize = sulphaBerHeaderSize(reader->data, reader->pos);
		*outVal = reader->data[reader->pos + hdrSize];
	}

	sulphaBerReaderNext(reader);
	return true;
}

SceBool sulphaBerReadBytes(SulphaBerReader *reader, SceUInt8 expectedTag,
			   const SceUInt8 **outPtr, SceUInt32 *outLen)
{
	SceUInt8 tag = reader->data[reader->pos];
	if ((tag & 0xC0U) != 0x80U || (tag & 0x1FU) != expectedTag || reader->state != SULPHA_BER_OK)
		return false;

	SceUInt32 len = sulphaBerReadLength(reader);
	if (len != 0) {
		SceUInt32 hdrSize = sulphaBerHeaderSize(reader->data, reader->pos);
		*outPtr = &reader->data[reader->pos + hdrSize];
	}

	*outLen = len;
	if (len == 0xFFFFU) {
		reader->state = SULPHA_BER_ERROR;
		return false;
	}

	sulphaBerReaderNext(reader);
	return true;
}

SulphaBerStream *sulphaBerResetBuffer(SulphaBerStream *stream, void *buf, SceUInt32 capacity)
{
	stream->writePos = 4;
	stream->bodyStart = 4;
	stream->containerOffset = 4;
	stream->containerLen = 0;
	stream->state = SULPHA_BER_OK;
	stream->capacity = capacity;
	stream->buffer = (SceUInt8 *)buf;
	return stream;
}

SceBool sulphaBerWriteRaw(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len)
{
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, data, len);
}

SceBool sulphaBerWriteUInt16(SulphaBerStream *stream, SceUInt16 val)
{
	SceUInt8 buf[2];
	buf[0] = (SceUInt8)(val >> 8);
	buf[1] = (SceUInt8)val;

	if (stream->state != SULPHA_BER_OK)
		return false;

	if ((val >> 8) != 0)
		return sulphaBerWriteTagValue(stream, 2, buf, 2);
	return sulphaBerWriteTagValue(stream, 2, &buf[1], 1);
}

SceBool sulphaBerWriteUInt32(SulphaBerStream *stream, SceUInt32 val)
{
	SceUInt32 be = __builtin_bswap32(val);
	const SceUInt8 *bytes = (const SceUInt8 *)&be;
	SceUInt32 len = 1;
	SceUInt32 tmp = val >> 8;

	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, 2, bytes + (4U - len), len);
}

SceBool sulphaBerWriteTaggedUInt16(SulphaBerStream *stream, SceUInt8 tag, SceUInt16 val)
{
	SceUInt8 buf[2];
	buf[0] = (SceUInt8)(val >> 8);
	buf[1] = (SceUInt8)val;

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	if ((val >> 8) != 0)
		return sulphaBerWriteTagValue(stream, tag, buf, 2);
	return sulphaBerWriteTagValue(stream, tag, &buf[1], 1);
}

SceBool sulphaBerWriteTaggedInt16(SulphaBerStream *stream, SceUInt8 tag, SceInt16 val)
{
	SceUInt8 buf[2];
	buf[0] = (SceUInt8)((SceUInt16)val >> 8);
	buf[1] = (SceUInt8)val;

	SceUInt32 len = 1;
	SceUInt64 tmp = (SceUInt64)(SceInt64)val >> 8;
	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, buf + (2U - len), len);
}

SceBool sulphaBerWriteTaggedUInt32(SulphaBerStream *stream, SceUInt8 tag, SceUInt32 val)
{
	SceUInt32 be = __builtin_bswap32(val);
	const SceUInt8 *bytes = (const SceUInt8 *)&be;
	SceUInt32 len = 1;
	SceUInt32 tmp = val >> 8;

	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, bytes + (4U - len), len);
}

SceBool sulphaBerWriteTaggedInt32(SulphaBerStream *stream, SceUInt8 tag, SceInt32 val)
{
	SceUInt32 be = __builtin_bswap32((SceUInt32)val);
	const SceUInt8 *bytes = (const SceUInt8 *)&be;
	SceUInt32 len = 1;
	SceUInt64 tmp = (SceUInt64)(SceInt64)val >> 8;

	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, bytes + (4U - len), len);
}

SceBool sulphaBerWriteTaggedUInt64(SulphaBerStream *stream, SceUInt8 tag, SceUInt64 val)
{
	SceUInt32 be[2];
	be[0] = __builtin_bswap32((SceUInt32)(val >> 32));
	be[1] = __builtin_bswap32((SceUInt32)val);

	SceUInt32 len = 1;
	SceUInt64 tmp = val >> 8;
	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, (const SceUInt8 *)be + (8U - len), len);
}

SceBool sulphaBerWriteTaggedUInt8(SulphaBerStream *stream, SceUInt8 tag, SceUInt8 val)
{
	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, &val, 1);
}

SceBool sulphaBerWriteTaggedInt8(SulphaBerStream *stream, SceUInt8 tag, SceInt8 val)
{
	SceUInt8 b = (SceUInt8)val;

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, &b, 1);
}

SceBool sulphaBerWriteTaggedFloat32(SulphaBerStream *stream, SceUInt8 tag, float val)
{
	union { float f; SceUInt32 u; } conv = { val };
	SceUInt32 be = __builtin_bswap32(conv.u);
	const SceUInt8 *bytes = (const SceUInt8 *)&be;
	SceUInt32 len = 1;
	SceUInt32 tmp = conv.u >> 8;

	while (tmp != 0) {
		len++;
		tmp >>= 8;
	}

	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, bytes + (4U - len), len);
}

SceBool sulphaBerWriteTaggedBytes(SulphaBerStream *stream, SceUInt8 tag, const void *data, SceUInt32 len)
{
	if (len == 0 || data == NULL)
		return true;
	if ((tag & 0xC0U) == 0)
		tag |= 0x80U;
	if (stream->state != SULPHA_BER_OK)
		return false;

	return sulphaBerWriteTagValue(stream, tag, data, len);
}

SceBool sulphaBerBeginContainer(SulphaBerStream *stream, SceUInt8 tag, SceUInt8 flags,
				SulphaBerMarker *marker)
{
	marker->prevOffset = 0xFFFFU;
	marker->prevLen = 0xFFFFU;

	if (stream->state != SULPHA_BER_OK)
		return false;
	if (stream->capacity < stream->writePos + 4U) {
		stream->state = SULPHA_BER_ERROR;
		return false;
	}

	marker->prevOffset = (SceUInt16)stream->containerOffset;
	marker->prevLen = (SceUInt16)stream->containerLen;

	stream->containerOffset = stream->writePos;
	stream->containerLen = 0;

	stream->buffer[stream->writePos++] = (SceUInt8)((tag & 0x3FU) | flags | 0x20U);
	stream->containerLen++;
	stream->buffer[stream->writePos++] = 0;
	stream->containerLen++;
	stream->buffer[stream->writePos++] = 0;
	stream->containerLen++;
	stream->buffer[stream->writePos++] = 0;
	stream->containerLen++;

	return marker->prevOffset != 0xFFFFU && marker->prevLen != 0xFFFFU;
}

SceBool sulphaBerEndContainer(SulphaBerStream *stream, const SulphaBerMarker *marker)
{
	if (stream->state != SULPHA_BER_OK || marker->prevOffset == 0xFFFFU)
		return false;

	SceUInt32 bodyLen = stream->containerLen - 4U;
	SceUInt8 *hdr = &stream->buffer[stream->containerOffset];
	hdr[1] = 0x82U;
	hdr[2] = (SceUInt8)(bodyLen >> 8);
	hdr[3] = (SceUInt8)bodyLen;

	stream->containerOffset = marker->prevOffset;
	stream->containerLen += marker->prevLen;
	return true;
}

void *sulphaBerAllocStream(SulphaBerStream *stream, SulphaAllocator *alloc, SceUInt32 capacity)
{
	void *buf = alloc->alloc(alloc, capacity, 0);
	sulphaBerInitStream(stream, buf, capacity);
	return buf;
}

SulphaBerStream *sulphaBerInitStream(SulphaBerStream *stream, void *buf, SceUInt32 capacity)
{
	stream->writePos = 0;
	stream->bodyStart = 0;
	stream->containerOffset = 0;
	stream->containerLen = 0;
	stream->state = SULPHA_BER_OK;
	stream->capacity = capacity;
	stream->buffer = (SceUInt8 *)buf;

	if (buf != NULL && capacity > 3) {
		stream->header = (SceUInt8 *)buf;
		stream->header[2] = 0;
		stream->header[3] = 0;
		stream->writePos = 4;
		stream->bodyStart = 4;
		stream->containerOffset = 4;
	} else {
		stream->header = NULL;
	}
	return stream;
}

SulphaBerStream *sulphaBerResetStream(SulphaBerStream *stream)
{
	stream->writePos = 4;
	stream->bodyStart = 4;
	stream->containerOffset = 4;
	stream->containerLen = 0;
	stream->state = SULPHA_BER_OK;
	return stream;
}

SceUInt8 *sulphaBerGetBuffer(const SulphaBerStream *stream)
{
	return stream->buffer;
}

SceUInt16 sulphaBerGetPayloadLength(const SulphaBerStream *stream)
{
	return (SceUInt16)(((SceUInt16)stream->header[2] << 8) | stream->header[3]);
}

SceUInt8 sulphaBerGetAgentId(const SulphaBerStream *stream)
{
	return stream->header[0];
}

SulphaBerStream *sulphaBerSetAgentId(SulphaBerStream *stream, SceUInt8 agentId)
{
	stream->header[0] = agentId;
	return stream;
}

SulphaBerStream *sulphaBerSetHeader(SulphaBerStream *stream, SceUInt8 flags, SceUInt8 agentId)
{
	stream->header[0] = agentId;
	stream->header[1] = flags;
	return stream;
}

SceUInt32 sulphaBerFinalize(SulphaBerStream *stream)
{
	SceUInt16 payloadLen = (SceUInt16)(stream->writePos - stream->bodyStart);
	stream->header[2] = (SceUInt8)(payloadLen >> 8);
	stream->header[3] = (SceUInt8)payloadLen;
	if (payloadLen != 0)
		return (SceUInt32)payloadLen + 4U;
	return 0;
}

SulphaBerStream *sulphaBerGetReader(SulphaBerStream *stream, SulphaBerReader **outReader)
{
	*outReader = &stream->reader;
	return stream;
}

SulphaBerStream *sulphaBerGetWriter(SulphaBerStream *stream, SulphaBerStream **outWriter)
{
	*outWriter = stream;
	return stream;
}

SceInt32 sulphaBerBeginRead(SulphaBerStream *stream, SceUInt8 *outMsgType)
{
	SulphaBerReader *reader = &stream->reader;
	SceUInt16 payloadLen = sulphaBerGetPayloadLength(stream);

	reader->end = (SceUInt32)payloadLen + 4U;
	reader->pos = 4;
	reader->state = SULPHA_BER_INIT;
	reader->data = stream->buffer;

	if (outMsgType == NULL)
		return 0;

	if (reader->end == 4) {
		reader->state = SULPHA_BER_EOF;
		return SULPHA_ERROR_INTERNAL;
	}

	if (!sulphaBerValidateCurrent(reader)) {
		reader->state = SULPHA_BER_ERROR;
		return SULPHA_ERROR_INTERNAL;
	}

	reader->state = SULPHA_BER_OK;
	*outMsgType = reader->data[reader->pos] & 0x1FU;

	sulphaBerReaderEnter(reader);
	if (!sulphaBerReaderNext(reader))
		return SULPHA_ERROR_INTERNAL;

	return 0;
}
