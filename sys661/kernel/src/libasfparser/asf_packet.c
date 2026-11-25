/*
 * PS Vita ePSP 6.61 libasfparser.prx (sceASFParser)
 * ASF I/O wrappers, packet/payload demuxer, seek, and DRM decryption.
 */
#include "asf_int.h"

s32 asfIoSeekAbs(SceAsfParser *parser, u64 pos)
{
	if (parser == NULL)
		return SCE_ASF_ERROR_INVALID_PARAM;
	s64 res = asfIoSeek(&parser->io, (s64)pos, ASF_SEEK_SET);
	if (res < 0)
		return (s32)res;
	return 0;
}

s32 asfIoReadU64(SceAsfIoContext *io, u64 *out)
{
	u8 buf[8];
	s32 ret = io->readCb(io->userData, buf, 8);
	if (ret >= 0) {
		u32 lo = (u32)buf[0] | ((u32)buf[1] << 8) | ((u32)buf[2] << 16) | ((u32)buf[3] << 24);
		u32 hi = (u32)buf[4] | ((u32)buf[5] << 8) | ((u32)buf[6] << 16) | ((u32)buf[7] << 24);
		*out = ((u64)hi << 32) | lo;
	}
	return ret;
}

s32 asfIoInit(SceAsfParser *parser, void *userData, SceAsfReadCallback readCb, SceAsfSeekCallback seekCb)
{
	parser->io.userData = userData;
	parser->io.readCb = readCb;
	parser->io.seekCb = seekCb;
	s64 res = seekCb(userData, userData, 0, ASF_SEEK_SET);
	if (res < 0)
		return (s32)res;
	return 0;
}

s64 asfIoTell(SceAsfIoContext *io)
{
	return io->seekCb(io->userData, io->userData, 0, ASF_SEEK_CUR);
}

s32 asfIoReadU8(SceAsfIoContext *io, u8 *out)
{
	return io->readCb(io->userData, out, 1);
}

s32 asfIoReadU16(SceAsfIoContext *io, u16 *out)
{
	u8 buf[2];
	s32 ret = io->readCb(io->userData, buf, 2);
	if (ret >= 0)
		*out = (u16)((u32)buf[0] | ((u32)buf[1] << 8));
	return ret;
}

s32 asfIoReadU32(SceAsfIoContext *io, u32 *out)
{
	u8 buf[4];
	s32 ret = io->readCb(io->userData, buf, 4);
	if (ret >= 0)
		*out = (u32)buf[0] | ((u32)buf[1] << 8) | ((u32)buf[2] << 16) | ((u32)buf[3] << 24);
	return ret;
}

s32 asfIoReadBytes(SceAsfIoContext *io, void *buf, u32 size)
{
	return io->readCb(io->userData, buf, size);
}

s32 asfIoSkip(SceAsfIoContext *io, s64 offset)
{
	s64 res = io->seekCb(io->userData, io->userData, offset, ASF_SEEK_CUR);
	if (res < 0)
		return (s32)res;
	return 0;
}

s64 asfIoSeek(SceAsfIoContext *io, s64 offset, s32 whence)
{
	return io->seekCb(io->userData, io->userData, offset, whence);
}

/* Packet bitfield accessors */
static s32 pktHasErrorCorrection(const SceAsfPacketState *pkt)
{
	return pkt->errCorrFlags & 0x80;
}

static u32 pktErrorCorrType(const SceAsfPacketState *pkt)
{
	return pkt->errCorrFlags & 0x60;
}

static u32 pktErrorCorrDataLen(const SceAsfPacketState *pkt)
{
	return pkt->errCorrFlags & 0x0F;
}

static u32 pktLenType(const SceAsfPacketState *pkt)
{
	return (pkt->lengthTypeFlags >> 5) & 3;
}

static u32 pktSeqType(const SceAsfPacketState *pkt)
{
	return (pkt->lengthTypeFlags >> 1) & 3;
}

static u32 pktPadLenType(const SceAsfPacketState *pkt)
{
	return (pkt->lengthTypeFlags >> 3) & 3;
}

static s32 pktIsMultiPayload(const SceAsfPacketState *pkt)
{
	return (pkt->lengthTypeFlags & 1) != 0;
}

static u32 pktMediaObjNumLenType(const SceAsfPacketState *pkt)
{
	return (pkt->propertyFlags >> 4) & 3;
}

static u32 pktOffsetLenType(const SceAsfPacketState *pkt)
{
	return (pkt->propertyFlags >> 2) & 3;
}

static u32 pktRepDataLenType(const SceAsfPacketState *pkt)
{
	return pkt->propertyFlags & 3;
}

static u32 pktNumPayloads(const SceAsfPacketState *pkt)
{
	return pkt->payloadCountFlags & 0x3F;
}

static u32 pktPayloadLenType(const SceAsfPacketState *pkt)
{
	return pkt->payloadCountFlags >> 6;
}

static s32 pktIsAtPacketEnd(const SceAsfPacketState *pkt)
{
	return (pkt->packetOffset + pkt->paddingLength) >= pkt->packetLength;
}

static s32 pktIsAtPayloadEnd(const SceAsfPacketState *pkt)
{
	return pkt->packetOffset >= pkt->payloadEnd;
}

static s32 pktIsAtSubPayloadEnd(const SceAsfPacketState *pkt)
{
	return pkt->packetOffset >= pkt->subPayloadEnd;
}

static s32 pktHasMorePayloads(const SceAsfPacketState *pkt)
{
	return !pktIsAtPacketEnd(pkt);
}

static s32 pktHasMoreSubPayloads(const SceAsfPacketState *pkt)
{
	if ((pkt->payloadFlags & 1) != 0)
		return !pktIsAtPayloadEnd(pkt);
	return !pktIsAtPacketEnd(pkt);
}

static s32 pktIsEof(const SceAsfPacketState *pkt)
{
	return pkt->packetNumber >= pkt->headerInfo->dataPacketsCount;
}

static s32 pktReadU32(SceAsfPacketState *pkt, u32 *out)
{
	s32 ret = asfIoReadU32(pkt->io, out);
	pkt->packetOffset += 4;
	return ret;
}

static s32 pktReadU16(SceAsfPacketState *pkt, u16 *out)
{
	s32 ret = asfIoReadU16(pkt->io, out);
	pkt->packetOffset += 2;
	return ret;
}

static s32 pktReadU8(SceAsfPacketState *pkt, u8 *out)
{
	s32 ret = asfIoReadU8(pkt->io, out);
	pkt->packetOffset += 1;
	return ret;
}

static s32 pktSkip(SceAsfPacketState *pkt, u32 count)
{
	pkt->packetOffset += count;
	return asfIoSkip(pkt->io, (s64)count);
}

static s32 pktReadVarField(SceAsfPacketState *pkt, u32 lenType, u32 *out, u32 defaultVal)
{
	u8 b;
	u16 w;
	s32 ret = 0;

	switch (lenType) {
	case 0:
		*out = defaultVal;
		return 0;
	case 1:
		ret = pktReadU8(pkt, &b);
		*out = b;
		return ret;
	case 2:
		ret = pktReadU16(pkt, &w);
		*out = w;
		return ret;
	case 3:
		return pktReadU32(pkt, out);
	default:
		*out = defaultVal;
		return 0;
	}
}

static s32 pktReadSubPayloadLen(SceAsfPacketState *pkt)
{
	u8 len;
	s32 ret = pktReadU8(pkt, &len);
	if (ret >= 0) {
		pkt->subPayloadIndex++;
		pkt->subPayloadEnd = pkt->packetOffset + len;
	}
	return ret;
}

static s32 pktReadDrmIvSubPayload(SceAsfPacketState *pkt, u32 size)
{
	u8 tmp[16];
	u32 toRead = (size >= 16) ? 15 : size;

	pkt->packetOffset += toRead;
	s32 ret = asfIoReadBytes(pkt->io, tmp, toRead);
	if (ret < 0)
		return ret;

	SceAsfDrmContext *drm = pkt->drm;
	if (drm->ivSize != 0)
		return ret;

	sceKernelMemset(drm->ivBuf, 0, 15);
	if (toRead == 9) {
		drm->ivSize = 8;
		sceKernelMemcpy(&drm->ivBuf[7], &tmp[1], 8);
	} else if (toRead == 8) {
		drm->ivSize = 8;
		sceKernelMemcpy(&drm->ivBuf[7], &tmp[0], 8);
	}
	return ret;
}

static s32 pktPrepareDrmPayload(SceAsfPacketState *pkt)
{
	SceAsfDrmContext *drm = pkt->drm;
	SceAsfIoContext *io = pkt->io;
	u32 payloadSize;
	s32 ret = 0;

	if (drm->initialized == 0)
		return SCE_ASF_ERROR_DRM_NOT_INITIALIZED;

	if ((pkt->payloadFlags & 1) != 0)
		payloadSize = pkt->payloadEnd - pkt->packetOffset;
	else
		payloadSize = pkt->packetLength - (pkt->packetOffset + pkt->paddingLength);

	drm->decryptedBytes = 0;
	drm->payloadTotalSize = payloadSize;
	drm->payloadFileOffset = 0;

	if (drm->decryptBufSize < payloadSize)
		return SCE_ASF_ERROR_DRM_PAYLOAD_TOO_LARGE;

	s64 curPos = asfIoTell(io);
	if (curPos < 0)
		return (s32)curPos;

	if (drm->ivSize != 0) {
		u32 rem = payloadSize & 7;
		if (rem != 0 && drm->ivSize == 8) {
			asfMemmove(&drm->ivBuf[7 - rem], &drm->ivBuf[7], 8);
			sceKernelMemset(&drm->ivBuf[15 - rem], 0, rem);
		}
	} else {
		s64 seekRes = asfIoSeek(io, (s64)(payloadSize - 15), ASF_SEEK_CUR);
		if (seekRes < 0)
			return (s32)seekRes;
		ret = asfIoReadBytes(io, drm->ivBuf, 15);
		if (ret < 0)
			return ret;
	}

	if (drm->setIvCb != NULL) {
		ret = drm->setIvCb(&drm->session, drm->ivBuf, payloadSize);
		if (ret >= 0) {
			drm->ivSize = 0;
			s64 restoreRes = asfIoSeek(io, curPos, ASF_SEEK_SET);
			if (restoreRes < 0)
				return (s32)restoreRes;
		}
	}
	return ret;
}

static s32 pktReadDrmData(SceAsfPacketState *pkt, void *dst, u32 size, s64 filePos)
{
	SceAsfDrmContext *drm = pkt->drm;
	SceAsfIoContext *io = pkt->io;
	u32 copied = 0;
	s32 ret = 0;

	if (drm->initialized == 0)
		return SCE_ASF_ERROR_DRM_NOT_INITIALIZED;

	if (drm->payloadFileOffset == 0) {
		s64 pos = asfIoTell(io);
		drm->payloadFileOffset = pos;
		if (pos < 0)
			return (s32)pos;
	}

	s64 payloadStart = drm->payloadFileOffset;
	s64 payloadEnd = payloadStart + (s64)drm->payloadTotalSize;
	if (filePos < payloadStart || filePos + (s64)size > payloadEnd)
		return SCE_ASF_ERROR_DRM_READ_RANGE;

	while (copied < size) {
		s64 decEnd = drm->payloadFileOffset + (s64)drm->decryptedBytes;
		s64 curPos = filePos + (s64)copied;
		if (curPos < decEnd) {
			s64 avail = decEnd - curPos;
			u32 chunk = (avail >= (s64)size) ? size : (u32)avail;
			if (chunk > size - copied)
				chunk = size - copied;
			sceKernelMemcpy((u8 *)dst + copied,
			                drm->decryptBuf + (u32)(curPos - drm->payloadFileOffset),
			                chunk);
			copied += chunk;
		} else {
			u32 decOff = drm->decryptedBytes;
			u32 blk = drm->blockAlign;
			drm->decryptedBytes = decOff + blk;
			ret = asfIoReadBytes(io, drm->decryptBuf + decOff, blk);
			if (ret < 0)
				return ret;
			if (drm->decryptCb == NULL)
				return SCE_ASF_ERROR_DRM_NO_DECRYPT_CB;
			ret = drm->decryptCb(&drm->session, drm->decryptBuf + decOff, blk);
			if (ret < 0)
				return ret;
		}
	}
	return ret;
}

static s32 pktSkipRestOfPacket(SceAsfPacketState *pkt)
{
	s32 ret = 0;
	if (pkt->packetOffset < pkt->packetLength)
		ret = pktSkip(pkt, pkt->packetLength - pkt->packetOffset);
	if (pkt->packetOffset < pkt->headerInfo->maxPacketSize)
		return pktSkip(pkt, pkt->headerInfo->maxPacketSize - pkt->packetOffset);
	return ret;
}

static s32 pktParseErrorCorrection(SceAsfPacketState *pkt)
{
	u32 type = pktErrorCorrType(pkt);
	if (type == 0x20 || type == 0x40 || type == 0x60)
		return SCE_ASF_ERROR_ERR_CORR_UNSUPPORTED;
	if (type != 0)
		return 0;
	if (pktErrorCorrDataLen(pkt) != 2)
		return SCE_ASF_ERROR_ERR_CORR_UNSUPPORTED;
	return pktSkip(pkt, 2);
}

static s32 pktParsePayloadHeader(SceAsfPacketState *pkt)
{
	u32 seq;
	s32 ret;

	ret = pktReadU8(pkt, &pkt->propertyFlags);
	if (ret < 0)
		return ret;
	ret = pktReadVarField(pkt, pktLenType(pkt), &pkt->packetLength, pkt->headerInfo->maxPacketSize);
	if (ret < 0)
		return ret;
	ret = pktReadVarField(pkt, pktSeqType(pkt), &seq, 0);
	if (ret < 0)
		return ret;
	ret = pktReadVarField(pkt, pktPadLenType(pkt), &pkt->paddingLength, 0);
	if (ret < 0)
		return ret;
	ret = pktReadU32(pkt, &pkt->sendTime);
	if (ret < 0)
		return ret;
	return pktReadU16(pkt, &pkt->duration);
}

static s32 pktParseOnePayload(SceAsfPacketState *pkt, s32 *foundTarget)
{
	u8 streamByte;
	u32 fieldVal;
	u32 payloadLen;
	u32 skipLen = 0;
	s32 ret;

	*foundTarget = 0;
	ret = pktReadU8(pkt, &streamByte);
	if (ret < 0)
		return ret;

	u32 streamNum = streamByte & 0x7F;
	if (streamNum == 0)
		return SCE_ASF_ERROR_INVALID_STREAM_ID;

	if (streamNum == pkt->streamId)
		*foundTarget = 1;

	ret = pktReadVarField(pkt, pktMediaObjNumLenType(pkt), &fieldVal, 0);
	if (ret < 0)
		return ret;
	ret = pktReadVarField(pkt, pktOffsetLenType(pkt), &fieldVal, 0);
	if (ret < 0)
		return ret;
	ret = pktReadVarField(pkt, pktRepDataLenType(pkt), &fieldVal, 0);
	if (ret < 0)
		return ret;

	if (fieldVal == 1) {
		pkt->subPayloadIndex = 0;
		pkt->payloadFlags |= 2;
		ret = pktReadU8(pkt, &pkt->subPayloadDelta);
		if (ret < 0)
			return ret;
	} else {
		ret = pktSkip(pkt, fieldVal);
		if (ret < 0)
			return ret;
	}

	if ((pkt->payloadFlags & 1) != 0) {
		ret = pktReadVarField(pkt, pktPayloadLenType(pkt), &payloadLen, 0);
		if (ret < 0)
			return ret;
		if (*foundTarget != 0)
			pkt->payloadEnd = pkt->packetOffset + payloadLen;
		else
			skipLen = payloadLen;
	}

	if ((pkt->payloadFlags & 2) != 0) {
		ret = pktReadSubPayloadLen(pkt);
		if (ret < 0)
			return ret;
	}

	if (*foundTarget == 1 && pkt->drm->initialized != 0) {
		ret = pktPrepareDrmPayload(pkt);
		if (ret < 0)
			return ret;
	}

	if (streamNum == pkt->drm->drmStreamId) {
		u32 ivSize;
		if ((pkt->payloadFlags & 2) != 0)
			ivSize = pkt->subPayloadEnd - pkt->packetOffset;
		else if (skipLen != 0)
			ivSize = skipLen;
		else
			ivSize = pkt->packetLength - (pkt->packetOffset + pkt->paddingLength);

		ret = pktReadDrmIvSubPayload(pkt, ivSize);
		if (ret < 0 || skipLen == 0)
			return ret;
		skipLen -= ivSize;
	}

	if (skipLen != 0)
		return pktSkip(pkt, skipLen);
	return ret;
}

static s32 pktParsePayloads(SceAsfPacketState *pkt, s32 *foundTarget)
{
	*foundTarget = 0;
	if (!pktIsMultiPayload(pkt)) {
		pkt->payloadFlags = 0;
		return pktParseOnePayload(pkt, foundTarget);
	}

	pkt->payloadFlags = 1;
	s32 ret = pktReadU8(pkt, &pkt->payloadCountFlags);
	if (ret < 0)
		return ret;

	pkt->payloadIndex = 0;
	u32 count = pktNumPayloads(pkt);
	while (pkt->payloadIndex < count && *foundTarget == 0) {
		ret = pktParseOnePayload(pkt, foundTarget);
		if (ret < 0)
			return ret;
		pkt->payloadIndex++;
	}
	return ret;
}

static s32 pktAdvanceToNextPacket(SceAsfPacketState *pkt)
{
	s32 found = 0;
	s32 ret;

	do {
		if (pktIsEof(pkt))
			return SCE_ASF_ERROR_END_OF_STREAM;

		pkt->packetOffset = 0;
		pkt->packetNumber++;

		ret = pktReadU8(pkt, &pkt->errCorrFlags);
		if (ret < 0)
			return ret;

		if (pktHasErrorCorrection(pkt)) {
			ret = pktParseErrorCorrection(pkt);
			if (ret < 0)
				return ret;
			ret = pktReadU8(pkt, &pkt->lengthTypeFlags);
			if (ret < 0)
				return ret;
		} else {
			pkt->lengthTypeFlags = pkt->errCorrFlags;
		}

		ret = pktParsePayloadHeader(pkt);
		if (ret < 0)
			return ret;

		if (pkt->packetLength != 0) {
			ret = pktParsePayloads(pkt, &found);
			if (ret < 0)
				return ret;
		}
		if (found != 0)
			break;

		ret = pktSkipRestOfPacket(pkt);
		if (ret < 0)
			return ret;
	} while (found == 0);

	return ret;
}

static s32 pktInitStream(SceAsfPacketState *pkt, u8 streamId, SceAsfHeaderInfo *hdr, SceAsfIoContext *io)
{
	pkt->streamId = streamId;
	pkt->packetNumber = 0;
	pkt->headerInfo = hdr;
	pkt->io = io;

	s64 seekRes = asfIoSeek(io, (s64)(hdr->headerSize + 50), ASF_SEEK_SET);
	if (seekRes < 0)
		return (s32)seekRes;

	s32 ret = pktAdvanceToNextPacket(pkt);
	if (ret >= 0)
		pkt->savedFilePos = asfIoTell(io);
	return ret;
}

s32 asfInitAllPacketStreams(SceAsfParser *parser)
{
	s32 ret = 0;
	u32 i;

	for (i = 0; i < parser->streamCount; i++) {
		parser->streams[i].pkt.drm = &parser->drm;
		ret = pktInitStream(&parser->streams[i].pkt, (u8)parser->streams[i].streamNumber,
		                    &parser->header, &parser->io);
		if (ret < 0)
			break;
	}
	return ret;
}

s32 asfPacketPeekPayload(void *dst, SceAsfPacketState *pkt, u32 size)
{
	SceAsfIoContext *io = pkt->io;
	SceAsfDrmContext *drm = pkt->drm;

	s64 pos = asfIoTell(io);
	if (pos < 0)
		return (s32)pos;

	s32 ret;
	if (drm->initialized != 0)
		ret = pktReadDrmData(pkt, dst, size, pos);
	else
		ret = asfIoReadBytes(io, dst, size);

	if (ret >= 0) {
		s64 seekRes = asfIoSeek(io, pos, ASF_SEEK_SET);
		if (seekRes < 0)
			return (s32)seekRes;
	}
	return ret;
}

s32 asfPacketAdvancePayload(SceAsfPacketState *pkt)
{
	if ((pkt->payloadFlags & 2) != 0) {
		if (pktIsAtSubPayloadEnd(pkt) && pktHasMoreSubPayloads(pkt))
			return pktReadSubPayloadLen(pkt);
	}
	if ((pkt->payloadFlags & 1) != 0 && pktIsAtPayloadEnd(pkt) && pktHasMorePayloads(pkt)) {
		s32 found = 0;
		s32 ret = pktParseOnePayload(pkt, &found);
		if (ret >= 0 && found == 0)
			return asfPacketAdvancePayload(pkt);
		return ret;
	}
	s32 ret = pktSkipRestOfPacket(pkt);
	if (ret >= 0)
		return pktAdvanceToNextPacket(pkt);
	return ret;
}

s32 asfPacketIsPayloadDone(SceAsfPacketState *pkt)
{
	if ((pkt->payloadFlags & 2) != 0) {
		if (pktIsAtSubPayloadEnd(pkt))
			return 1;
	}
	if ((pkt->payloadFlags & 1) != 0) {
		if (pktIsAtPayloadEnd(pkt))
			return 1;
	}
	return pktIsAtPacketEnd(pkt);
}

s32 asfPacketTimeDelta(SceAsfPacketState *pkt, u32 targetTimeMs)
{
	u32 start = pkt->sendTime;
	u32 end = start + pkt->duration;
	if (targetTimeMs >= start && targetTimeMs < end)
		return 0;
	if (targetTimeMs >= start)
		return (s32)(targetTimeMs - end);
	return (s32)(targetTimeMs - start);
}

s32 asfPacketSeekToPacket(SceAsfPacketState *pkt, u64 packetNum)
{
	u64 idx = packetNum - 1;
	pkt->packetNumber = idx;
	s64 filePos = (s64)(pkt->headerInfo->headerSize + 50 + idx * (u64)pkt->headerInfo->maxPacketSize);
	s64 seekRes = asfIoSeek(pkt->io, filePos, ASF_SEEK_SET);
	if (seekRes < 0)
		return (s32)seekRes;
	return pktAdvanceToNextPacket(pkt);
}

s32 asfPacketSeekToTime(SceAsfPacketState *pkt, u32 targetTimeMs)
{
	SceAsfHeaderInfo *hdr = pkt->headerInfo;
	s64 step = 0;
	u64 curPkt = pkt->packetNumber;
	u64 prevPkt = curPkt;
	u32 maxPktSize = hdr->maxPacketSize;
	u32 maxBitrate = hdr->maxBitrate;
	s64 delta = (s64)asfPacketTimeDelta(pkt, targetTimeMs);
	s64 prevDelta = delta;
	s32 ret = 0;

	while (delta != 0) {
		s64 move;
		if ((prevDelta < 0 && delta > 0) || (prevDelta > 0 && delta < 0)) {
			if ((u64)(step + 1) < 3)
				return asfPacketSeekToPacket(pkt, prevPkt);
			move = -step / 2;
		} else {
			move = (delta * (s64)maxBitrate) / (s64)(8000U * maxPktSize);
		}

		if (move == 0)
			move = (delta > 0) ? 1 : -1;

		prevPkt = pkt->packetNumber;
		s64 nextPkt = (s64)curPkt + move;
		u64 clamped;
		if (nextPkt <= 0)
			clamped = 1;
		else if ((u64)nextPkt > hdr->dataPacketsCount)
			clamped = hdr->dataPacketsCount;
		else
			clamped = (u64)nextPkt;

		ret = asfPacketSeekToPacket(pkt, clamped);
		if (ret < 0)
			return ret;

		curPkt = pkt->packetNumber;
		step = (s64)(curPkt - prevPkt);
		if (curPkt == prevPkt)
			return ret;

		prevDelta = delta;
		delta = (s64)asfPacketTimeDelta(pkt, targetTimeMs);
	}
	return ret;
}

s32 asfPacketSkipBytes(SceAsfPacketState *pkt, u32 count)
{
	return pktSkip(pkt, count);
}

u32 asfPacketGetSendTime(SceAsfPacketState *pkt)
{
	return pkt->sendTime;
}
