#include "../../../../../naac_internal.h"

void naacBitBufInit(NaacBitBuffer *bb, uint8_t *storage, int32_t capacityBytes)
{
	bb->buffer = storage;
	bb->capacityBits = capacityBytes * 8;
	bb->bitPos = 0;
	bb->totalBits = 0;
	if (storage != NULL && capacityBytes > 0)
		sceClibMemset(storage, 0, (size_t)capacityBytes);
}

void naacBitBufWrite(NaacBitBuffer *bb, uint32_t value, int32_t numBits)
{
	if (bb == NULL || bb->buffer == NULL || numBits <= 0)
		return;
	for (int32_t i = numBits - 1; i >= 0; i--) {
		if (bb->bitPos >= bb->capacityBits)
			break;
		uint32_t bit = (value >> i) & 1U;
		int32_t bytePos = bb->bitPos >> 3;
		int32_t bitOffset = 7 - (bb->bitPos & 7);
		if (bit != 0U)
			bb->buffer[bytePos] |= (uint8_t)(1U << bitOffset);
		bb->bitPos++;
		bb->totalBits++;
	}
}

uint32_t naacBitBufRead(NaacBitBuffer *bb, int32_t numBits)
{
	if (bb == NULL || bb->buffer == NULL || numBits <= 0)
		return 0U;
	uint32_t result = 0U;
	for (int32_t i = 0; i < numBits; i++) {
		result <<= 1;
		if (bb->bitPos < bb->capacityBits) {
			int32_t bytePos = bb->bitPos >> 3;
			int32_t bitOffset = 7 - (bb->bitPos & 7);
			result |= (uint32_t)((bb->buffer[bytePos] >> bitOffset) & 1U);
			bb->bitPos++;
		}
	}
	return result;
}

int32_t naacBitBufGetBitCount(const NaacBitBuffer *bb)
{
	return (bb != NULL) ? bb->totalBits : 0;
}

int32_t byteAlignment(NaacBitBuffer *bb)
{
	if (bb == NULL)
		return 0;
	int32_t rem = bb->bitPos & 7;
	if (rem != 0) {
		int32_t padBits = 8 - rem;
		naacBitBufWrite(bb, 0U, padBits);
		return padBits;
	}
	return 0;
}

int32_t __f98encodeADTSHeader(NaacBitBuffer *bb, const NaacFrameInfo *frameInfo, int32_t rawDataBytes)
{
	if (bb == NULL || frameInfo == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t frameLength = rawDataBytes + 7;
	if (frameLength > 8191)
		return NAAC_ERR_INTERNAL;

	naacBitBufWrite(bb, 0xFFFU, 12);                         /* syncword */
	naacBitBufWrite(bb, 0U, 1);                              /* ID (MPEG-4) */
	naacBitBufWrite(bb, 0U, 2);                              /* layer */
	naacBitBufWrite(bb, (uint32_t)frameInfo->protectionAbsent, 1);
	naacBitBufWrite(bb, (uint32_t)(frameInfo->audioObjectType - 1), 2);
	naacBitBufWrite(bb, (uint32_t)frameInfo->sampleRateIndex, 4);
	naacBitBufWrite(bb, 0U, 1);                              /* private_bit */
	naacBitBufWrite(bb, (uint32_t)frameInfo->channelConfig, 3);
	naacBitBufWrite(bb, 0U, 1);                              /* original_copy */
	naacBitBufWrite(bb, 0U, 1);                              /* home */
	naacBitBufWrite(bb, 0U, 1);                              /* copyright_id_bit */
	naacBitBufWrite(bb, 0U, 1);                              /* copyright_id_start */
	naacBitBufWrite(bb, (uint32_t)frameLength, 13);          /* aac_frame_length */
	naacBitBufWrite(bb, 0x7FFU, 11);                         /* adts_buffer_fullness (VBR) */
	naacBitBufWrite(bb, 0U, 2);                              /* number_of_raw_data_blocks_in_frame */
	return NAAC_OK;
}

int32_t naacWriteAudioSpecificConfig(NaacBitBuffer *bb, const NaacCoreInfo *info, int32_t *outBits)
{
	if (bb == NULL || info == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t startBits = naacBitBufGetBitCount(bb);
	int32_t chanConfig = (info->numChannels == 8) ? 7 : info->numChannels;
	naacBitBufWrite(bb, (uint32_t)info->audioObjectType, 5);
	naacBitBufWrite(bb, (uint32_t)info->sampleRateIndex, 4);
	naacBitBufWrite(bb, (uint32_t)chanConfig, 4);
	/* GASpecificConfig: frameLengthFlag=0, dependsOnCoreCoder=0, extensionFlag=0 */
	naacBitBufWrite(bb, 0U, 3);
	if (outBits != NULL)
		*outBits = naacBitBufGetBitCount(bb) - startBits;
	return NAAC_OK;
}

int32_t naacWriteStreamMuxConfig(NaacBitBuffer *bb, const NaacCoreInfo *info, int32_t *outBits)
{
	if (bb == NULL || info == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t startBits = naacBitBufGetBitCount(bb);
	naacBitBufWrite(bb, 0U, 1); /* audioMuxVersion = 0 */
	naacBitBufWrite(bb, 1U, 1); /* allStreamsSameTimeFraming = 1 */
	naacBitBufWrite(bb, 0U, 6); /* numSubFrames = 0 */
	naacBitBufWrite(bb, 0U, 4); /* numProgram = 0 */
	naacBitBufWrite(bb, 0U, 3); /* numLayer = 0 */
	naacWriteAudioSpecificConfig(bb, info, NULL);
	naacBitBufWrite(bb, 0U, 3); /* frameLengthType = 0 */
	naacBitBufWrite(bb, 0xFFU, 8); /* latmBufferFullness */
	naacBitBufWrite(bb, 0U, 1); /* otherDataPresent = 0 */
	naacBitBufWrite(bb, 0U, 1); /* crcCheckPresent = 0 */
	if (outBits != NULL)
		*outBits = naacBitBufGetBitCount(bb) - startBits;
	return NAAC_OK;
}

static void naacWriteIcsInfo(NaacBitBuffer *bb, const NaacCodChannel *chan)
{
	naacBitBufWrite(bb, 0U, 1); /* ics_reserved_bit */
	naacBitBufWrite(bb, (uint32_t)(chan->blockType & 3), 2);
	naacBitBufWrite(bb, (uint32_t)(chan->windowShape & 1), 1);
	if (chan->blockType == NAAC_SHORT_WINDOW) {
		naacBitBufWrite(bb, (uint32_t)(chan->maxSfb & 0xF), 4);
		naacBitBufWrite(bb, 0U, 7); /* scale_factor_grouping */
	} else {
		naacBitBufWrite(bb, (uint32_t)(chan->maxSfb & 0x3F), 6);
		naacBitBufWrite(bb, 0U, 1); /* predictor_data_present */
	}
}

static void naacWriteIndividualChannelStream(NaacBitBuffer *bb, const NaacCodChannel *chan,
	const NaacPsyParam *param, int32_t commonWindow)
{
	(void)param;
	naacBitBufWrite(bb, (uint32_t)(chan->globalGain & 0xFF), 8);
	if (commonWindow == 0)
		naacWriteIcsInfo(bb, chan);

	/* Section data: single zero codebook run across maxSfb */
	if (chan->maxSfb > 0) {
		naacBitBufWrite(bb, 0U, 4); /* ZERO_HCB */
		int32_t sectBits = (chan->blockType == NAAC_SHORT_WINDOW) ? 3 : 5;
		int32_t escVal = (1 << sectBits) - 1;
		int32_t remaining = chan->maxSfb;
		while (remaining >= escVal) {
			naacBitBufWrite(bb, (uint32_t)escVal, sectBits);
			remaining -= escVal;
		}
		naacBitBufWrite(bb, (uint32_t)remaining, sectBits);
	}

	naacBitBufWrite(bb, 0U, 1); /* pulse_data_present */
	naacBitBufWrite(bb, 0U, 1); /* tns_data_present */
	naacBitBufWrite(bb, 0U, 1); /* gain_control_data_present */
}

int32_t naacWriteSingleChannelElement(NaacBitBuffer *bb, const NaacCodChannel *chan,
	const NaacPsyParam *param, int32_t tag)
{
	if (bb == NULL || chan == NULL)
		return NAAC_ERR_INVALID_PARAM;
	naacBitBufWrite(bb, (uint32_t)NAAC_ELEM_SCE, 3);
	naacBitBufWrite(bb, (uint32_t)(tag & 0xF), 4);
	naacWriteIndividualChannelStream(bb, chan, param, 0);
	return NAAC_OK;
}

int32_t naacWriteChannelPairElement(NaacBitBuffer *bb, const NaacCodChannel *left, const NaacCodChannel *right,
	const NaacMsChannel *ms, const NaacPsyParam *param, int32_t tag)
{
	if (bb == NULL || left == NULL || right == NULL)
		return NAAC_ERR_INVALID_PARAM;
	naacBitBufWrite(bb, (uint32_t)NAAC_ELEM_CPE, 3);
	naacBitBufWrite(bb, (uint32_t)(tag & 0xF), 4);

	int32_t commonWindow = (left->blockType == right->blockType) ? 1 : 0;
	naacBitBufWrite(bb, (uint32_t)commonWindow, 1);
	if (commonWindow != 0) {
		naacWriteIcsInfo(bb, left);
		uint32_t msMask = (ms != NULL) ? (uint32_t)(ms->msMaskPresent & 3) : 0U;
		naacBitBufWrite(bb, msMask, 2);
		if (msMask == 1U && ms != NULL) {
			for (int32_t sfb = 0; sfb < left->maxSfb; sfb++)
				naacBitBufWrite(bb, ms->msUsed[sfb] ? 1U : 0U, 1);
		}
	}
	naacWriteIndividualChannelStream(bb, left, param, commonWindow);
	naacWriteIndividualChannelStream(bb, right, param, commonWindow);
	return NAAC_OK;
}

int32_t naacWriteLfeElement(NaacBitBuffer *bb, const NaacCodChannel *chan,
	const NaacPsyParam *param, int32_t tag)
{
	if (bb == NULL || chan == NULL)
		return NAAC_ERR_INVALID_PARAM;
	naacBitBufWrite(bb, (uint32_t)NAAC_ELEM_LFE, 3);
	naacBitBufWrite(bb, (uint32_t)(tag & 0xF), 4);
	naacWriteIndividualChannelStream(bb, chan, param, 0);
	return NAAC_OK;
}

int32_t writeCouplingChannelElement(NaacBitBuffer *bb, const NaacCodChannel *chan)
{
	if (bb == NULL || chan == NULL)
		return NAAC_ERR_INVALID_PARAM;
	naacBitBufWrite(bb, (uint32_t)NAAC_ELEM_CCE, 3);
	naacBitBufWrite(bb, 0U, 4);
	return NAAC_OK;
}
