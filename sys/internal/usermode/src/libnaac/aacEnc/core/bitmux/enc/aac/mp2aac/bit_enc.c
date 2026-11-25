#include "../../../../../../naac_internal.h"

int32_t __f98CreateBitstreamEncoder(NaacBitstreamEncoder *enc, const NaacChannelMap *map,
	int32_t sampleRate, int32_t bitRate, int32_t writeAdts)
{
	if (enc == NULL || map == NULL)
		return NAAC_ERR_INVALID_PARAM;

	sceClibMemset(enc, 0, sizeof(NaacBitstreamEncoder));
	sceClibMemcpy_safe(&enc->channelMap, map, sizeof(NaacChannelMap));
	return __f98InitFrameInfo(&enc->frameInfo, sampleRate, map->numChannels, bitRate, writeAdts);
}

int32_t __f98WriteBitstream(NaacBitstreamEncoder *enc, NaacBitBuffer *bb, const NaacCodChannel *channels,
	const NaacMsChannel *msChannels, const NaacPsyParam *param)
{
	if (enc == NULL || bb == NULL || channels == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t cpeIndex = 0;
	for (int32_t e = 0; e < enc->channelMap.numElements; e++) {
		const NaacChannelElement *elem = &enc->channelMap.elements[e];
		int32_t err = NAAC_OK;
		if (elem->elementType == NAAC_ELEM_SCE) {
			err = naacWriteSingleChannelElement(bb, &channels[elem->firstChannel], param, elem->instanceTag);
		} else if (elem->elementType == NAAC_ELEM_CPE) {
			const NaacMsChannel *ms = (msChannels != NULL) ? &msChannels[cpeIndex++] : NULL;
			err = naacWriteChannelPairElement(bb, &channels[elem->firstChannel],
				&channels[elem->secondChannel], ms, param, elem->instanceTag);
		} else if (elem->elementType == NAAC_ELEM_LFE) {
			err = naacWriteLfeElement(bb, &channels[elem->firstChannel], param, elem->instanceTag);
		} else if (elem->elementType == NAAC_ELEM_CCE) {
			err = writeCouplingChannelElement(bb, &channels[elem->firstChannel]);
		} else {
			return NAAC_ERR_INTERNAL;
		}
		if (err != NAAC_OK)
			return err;
	}

	naacBitBufWrite(bb, (uint32_t)NAAC_ELEM_END, 3);
	byteAlignment(bb);
	enc->totalBitsWritten = naacBitBufGetBitCount(bb);
	return NAAC_OK;
}

int32_t __f98AdvanceBitstreamEncoder(NaacBitstreamEncoder *enc, const NaacCodChannel *channels,
	const NaacMsChannel *msChannels, const NaacPsyParam *param, uint8_t *outBuf, int32_t maxOutBytes, int32_t *outBytes)
{
	if (enc == NULL || channels == NULL || outBuf == NULL || outBytes == NULL)
		return NAAC_ERR_INVALID_PARAM;

	uint8_t rawBlock[8192];
	NaacBitBuffer rawBb;
	naacBitBufInit(&rawBb, rawBlock, (int32_t)sizeof(rawBlock));

	int32_t err = __f98WriteBitstream(enc, &rawBb, channels, msChannels, param);
	if (err != NAAC_OK)
		return err;

	int32_t rawBytes = (naacBitBufGetBitCount(&rawBb) + 7) >> 3;
	int32_t headerBytes = (enc->frameInfo.writeAdts != 0) ? 7 : 0;
	if (rawBytes + headerBytes > maxOutBytes)
		return NAAC_ERR_INTERNAL;

	if (enc->frameInfo.writeAdts != 0) {
		NaacBitBuffer hdrBb;
		naacBitBufInit(&hdrBb, outBuf, 7);
		err = __f98encodeADTSHeader(&hdrBb, &enc->frameInfo, rawBytes);
		if (err != NAAC_OK)
			return err;
		sceClibMemcpy_safe(outBuf + 7, rawBlock, (size_t)rawBytes);
		*outBytes = rawBytes + 7;
	} else {
		sceClibMemcpy_safe(outBuf, rawBlock, (size_t)rawBytes);
		*outBytes = rawBytes;
	}

	enc->headerBits = headerBytes * 8;
	return NAAC_OK;
}
