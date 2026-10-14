#include "atrac_internal.h"

AtracDecoderGroup g_at9DecoderGroup;

/* ---------------------------------------------------------------------- */
/* module                                                                 */

int module_start(SceSize argc, const void *args)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	SceInt32 ret, err;
	int i;

	ret = sceKernelCreateLwMutex(&group->mutex, "sceAtracAt9DecoderGroup",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO,
				     0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	group->atracType = SCE_ATRAC_TYPE_AT9;
	group->totalCh = 0;
	group->vacantCh = 0;
	group->wordLength = 0;
	for (i = 0; i < ATRAC_MAX_DECODERS; i++)
		atracStreamReset(&group->decoder[i].stream);
	err = sceKernelUnlockLwMutex(&group->mutex, 1);
	if (err < 0 && ret == 0)
		ret = err;
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 atracDeleteDecoderGroup(SceInt32 termAudiodecFlag, SceInt32 lock)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	SceInt32 ret = 0, err;
	int i;

	if (lock == 1) {
		ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	if (group->totalCh == 0) {
		ret = SCE_ATRAC_ERROR_NOT_CREATED;
	} else if (group->totalCh > group->vacantCh) {
		ret = SCE_ATRAC_ERROR_REMAIN_VALID_HANDLE;
	} else if (termAudiodecFlag == 0 ||
		   (ret = terminateAtracCodecLibrary(group->atracType)) >= 0) {
		group->totalCh = 0;
		group->vacantCh = 0;
		group->wordLength = 0;
		for (i = 0; i < ATRAC_MAX_DECODERS; i++) {
			group->decoder[i].usedCh = 0;
			group->decoder[i].pcmBuffer = NULL;
		}
	}
	if (lock == 1) {
		err = sceKernelUnlockLwMutex(&group->mutex, 1);
		if (err < 0 && ret == 0)
			ret = err;
	}
	return ret;
}

/* returns the channels of decoder index to the group */
static inline SceInt32 atracReleaseChannels(AtracDecoderGroup *group, SceUInt16 index)
{
	AtracDecoder *decoder = &group->decoder[index];

	if (decoder->usedCh == 0)
		return SCE_ATRAC_ERROR_INVALID_HANDLE;
	group->vacantCh += decoder->usedCh;
	decoder->usedCh = 0;
	return 0;
}

int module_stop(SceSize argc, const void *args)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	AtracDecoder *decoder;
	SceInt32 ret, err;
	SceUInt32 i;

	/* the mutex is deleted while held */
	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (group->totalCh != 0) {
		for (i = 0; i < group->totalCh; i++) {
			decoder = &group->decoder[i];
			if (decoder->stream.channels > 0) {
				err = deleteAtracCodecDecoder(decoder);
				atracStreamReset(&decoder->stream);
				if (err < 0 && ret == 0)
					ret = err;
				err = atracReleaseChannels(group, i);
				if (err < 0 && ret == 0)
					ret = err;
			}
		}
		err = atracDeleteDecoderGroup(0, 0);
		if (err < 0 && ret == 0)
			ret = err;
	}
	err = sceKernelDeleteLwMutex(&group->mutex);
	if (err < 0 && ret == 0)
		ret = err;
	return ret;
}

/* ---------------------------------------------------------------------- */
/* decoder group                                                          */

static inline SceInt32 atracCheckDecoderGroup(const SceAtracDecoderGroup *pDecoderGroup)
{
	SceInt32 ret = 0;

	if (pDecoderGroup == NULL)
		ret = SCE_ATRAC_ERROR_INVALID_POINTER;
	else if (pDecoderGroup->size != sizeof(*pDecoderGroup))
		ret = SCE_ATRAC_ERROR_INVALID_SIZE;
	else if (pDecoderGroup->wordLength != SCE_ATRAC_WORD_LENGTH_16BITS)
		ret = SCE_ATRAC_ERROR_INVALID_WORD_LENGTH;
	return ret;
}

static inline SceInt32 atracCheckBuffer(const void *buffer)
{
	SceInt32 ret = 0;

	if (buffer == NULL)
		ret = SCE_ATRAC_ERROR_INVALID_POINTER;
	else if (((SceUInt32)buffer & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)
		ret = SCE_ATRAC_ERROR_INVALID_ALIGNMENT;
	return ret;
}

int sceAtracQueryDecoderGroupMemSize(SceUInt32 atracType, const SceAtracDecoderGroup *pDecoderGroup)
{
	SceInt32 ret = atracCheckDecoderGroup(pDecoderGroup);

	if (ret >= 0) {
		if (atracType == SCE_ATRAC_TYPE_AT9) {
			if (pDecoderGroup->totalCh == 0 ||
			    pDecoderGroup->totalCh > SCE_ATRAC_AT9_MAX_TOTAL_CH)
				ret = SCE_ATRAC_ERROR_INVALID_TOTAL_CH;
			else
				ret = pDecoderGroup->totalCh * ATRAC_PCM_BUFFER_SIZE;
		} else {
			ret = SCE_ATRAC_ERROR_INVALID_TYPE;
		}
	}
	return ret;
}

int sceAtracCreateDecoderGroup(SceUInt32 atracType, const SceAtracDecoderGroup *pDecoderGroup,
			       void *pvWorkMem, int initAudiodecFlag)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	SceUChar8 *work = pvWorkMem;
	SceInt32 ret, err;
	SceUInt32 i;

	ret = atracCheckDecoderGroup(pDecoderGroup);
	if (ret < 0)
		return ret;
	ret = atracCheckBuffer(pvWorkMem);
	if (ret < 0)
		return ret;
	if (atracType != SCE_ATRAC_TYPE_AT9)
		return SCE_ATRAC_ERROR_INVALID_TYPE;
	if (pDecoderGroup->totalCh == 0 || pDecoderGroup->totalCh > SCE_ATRAC_AT9_MAX_TOTAL_CH)
		return SCE_ATRAC_ERROR_INVALID_TOTAL_CH;
	if (group->totalCh != 0)
		return SCE_ATRAC_ERROR_ALREADY_CREATED;
	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (initAudiodecFlag == 0 ||
	    (ret = initializeAtracCodecLibrary(group->atracType, pDecoderGroup->totalCh)) >= 0) {
		group->totalCh = pDecoderGroup->totalCh;
		group->vacantCh = pDecoderGroup->totalCh;
		group->wordLength = pDecoderGroup->wordLength;
		for (i = 0; i < pDecoderGroup->totalCh; i++) {
			group->decoder[i].usedCh = 0;
			group->decoder[i].pcmBuffer = work;
			work += ATRAC_PCM_BUFFER_SIZE;
		}
		for (; i < ATRAC_MAX_DECODERS; i++) {
			group->decoder[i].usedCh = 0;
			group->decoder[i].pcmBuffer = NULL;
		}
	}
	err = sceKernelUnlockLwMutex(&group->mutex, 1);
	if (err < 0 && ret == 0)
		ret = err;
	return ret;
}

int sceAtracDeleteDecoderGroup(SceUInt32 atracType, int termAudiodecFlag)
{
	SceInt32 ret = SCE_ATRAC_ERROR_INVALID_TYPE;

	if (atracType == SCE_ATRAC_TYPE_AT9)
		ret = atracDeleteDecoderGroup(termAudiodecFlag, 1);
	return ret;
}

int sceAtracGetDecoderGroupInfo(SceUInt32 atracType, SceAtracDecoderGroup *pCreatedDecoder,
				SceAtracDecoderGroup *pAvailableDecoder)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	SceInt32 ret, err;

	if (pCreatedDecoder == NULL || pAvailableDecoder == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	if (pCreatedDecoder->size != sizeof(*pCreatedDecoder) ||
	    pAvailableDecoder->size != sizeof(*pAvailableDecoder))
		return SCE_ATRAC_ERROR_INVALID_SIZE;
	if (atracType != SCE_ATRAC_TYPE_AT9)
		return SCE_ATRAC_ERROR_INVALID_TYPE;
	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (group->totalCh == 0) {
		ret = SCE_ATRAC_ERROR_NOT_CREATED;
	} else {
		pCreatedDecoder->wordLength = group->wordLength;
		pCreatedDecoder->totalCh = group->totalCh;
		pAvailableDecoder->wordLength = group->wordLength;
		pAvailableDecoder->totalCh = group->vacantCh;
	}
	err = sceKernelUnlockLwMutex(&group->mutex, 1);
	if (err < 0 && ret == 0)
		ret = err;
	return ret;
}

/* ---------------------------------------------------------------------- */
/* handles                                                                */


/* takes a free decoder with channels channels: its index, or an error */
static inline SceInt32 atracAcquireDecoder(AtracDecoderGroup *group, SceUInt32 channels)
{
	SceInt32 ret, err;
	SceUInt32 i;

	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (group->vacantCh < channels) {
		ret = SCE_ATRAC_ERROR_SHORTAGE_OF_CH;
	} else {
		/* no bound check (as in Sony's code): totalCh vacant channels imply a free slot */
		for (i = 0; i < group->totalCh; i++) {
			if (group->decoder[i].usedCh == 0)
				break;
		}
		group->vacantCh -= channels;
		group->decoder[i].usedCh = channels;
		ret = i;
	}
	err = sceKernelUnlockLwMutex(&group->mutex, 1);
	if (err < 0 && ret == 0)
		ret = err;
	return ret;
}

int sceAtracSetDataAndAcquireHandle(SceUChar8 *pucBuffer, SceUInt32 uiReadSize,
				    SceUInt32 uiBufferSize)
{
	AtracDecoderGroup *group = &g_at9DecoderGroup;
	AtracDecoder *decoder;
	AtracHeader header;
	SceInt32 index, ret;

	ret = atracCheckBuffer(pucBuffer);
	if (ret < 0)
		return ret;
	if ((uiBufferSize & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)
		return SCE_ATRAC_ERROR_INVALID_ALIGNMENT;
	if (uiBufferSize < uiReadSize)
		return SCE_ATRAC_ERROR_READ_SIZE_OVER_BUFFER;
	ret = atracParseHeader(&header, pucBuffer, uiReadSize);
	if (ret < 0)
		return ret;
	if (header.atracType != SCE_ATRAC_TYPE_AT9)
		return SCE_ATRAC_ERROR_UNSUPPORTED_DATA;
	if (group->totalCh == 0)
		return SCE_ATRAC_ERROR_NOT_CREATED;

	index = atracAcquireDecoder(group, header.channels);
	if (index < 0)
		return index;
	decoder = &group->decoder[index];
	ret = atracStreamInit(&decoder->stream, &header, pucBuffer, uiReadSize, uiBufferSize);
	if (ret >= 0) {
		ret = createAtracCodecDecoder(decoder, &header);
		if (ret < 0) {
			atracStreamReset(&decoder->stream);
		} else {
			ret = atracSkipSamples(decoder);
			if (ret < 0) {
				deleteAtracCodecDecoder(decoder);
				atracStreamReset(&decoder->stream);
			}
		}
	}
	if (ret < 0) {
		if (sceKernelLockLwMutex(&group->mutex, 1, NULL) >= 0) {
			atracReleaseChannels(group, index);
			sceKernelUnlockLwMutex(&group->mutex, 1);
		}
		return ret;
	}
	return (header.atracType << 16) | (SceUInt16)index;
}

/* the group of a handle's ATRAC type */
static inline SceInt32 atracHandleGroup(SceInt32 atracHandle, AtracDecoderGroup **pGroup)
{
	SceInt32 ret = 0;

	if ((atracHandle >> 16) == SCE_ATRAC_TYPE_AT9)
		*pGroup = &g_at9DecoderGroup;
	else
		ret = SCE_ATRAC_ERROR_INVALID_HANDLE;
	return ret;
}

int sceAtracReleaseHandle(int atracHandle)
{
	AtracDecoderGroup *group = NULL;
	AtracDecoder *decoder;
	SceUInt16 index;
	SceInt32 ret, err;

	ret = atracHandleGroup(atracHandle, &group);
	if (ret < 0)
		return ret;
	decoder = NULL;
	ret = 0;
	index = atracHandle;
	if (index >= (SceInt32)group->totalCh)
		ret = SCE_ATRAC_ERROR_INVALID_HANDLE;
	else if (group->decoder[index].usedCh == 0)
		ret = SCE_ATRAC_ERROR_INVALID_HANDLE;
	else
		decoder = &group->decoder[index];
	if (ret < 0)
		return ret;
	ret = deleteAtracCodecDecoder(decoder);
	atracStreamReset(&decoder->stream);
	if (ret < 0)
		return ret;
	ret = sceKernelLockLwMutex(&group->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = atracReleaseChannels(group, index);
	err = sceKernelUnlockLwMutex(&group->mutex, 1);
	if (err < 0 && ret == 0)
		ret = err;
	return ret;
}

SceInt32 atracResolveHandle(SceInt32 atracHandle, AtracDecoder **pDecoder)
{
	AtracDecoderGroup *group = NULL;
	AtracDecoder *decoder;
	SceUInt16 index;
	SceInt32 ret;

	ret = atracHandleGroup(atracHandle, &group);
	if (ret < 0)
		return ret;
	ret = 0;
	index = atracHandle;
	if (index >= (SceInt32)group->totalCh) {
		ret = SCE_ATRAC_ERROR_INVALID_HANDLE;
	} else {
		decoder = &group->decoder[index];
		if (decoder->usedCh == 0)
			ret = SCE_ATRAC_ERROR_INVALID_HANDLE;
		else
			*pDecoder = decoder;
	}
	return ret;
}

int sceAtracDecode(int atracHandle, void *pOutputBuffer, SceUInt32 *pOutputSamples,
		   SceUInt32 *pDecoderStatus)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pOutputBuffer == NULL || pOutputSamples == NULL || pDecoderStatus == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	if (((SceUInt32)pOutputBuffer & 1) != 0)
		return SCE_ATRAC_ERROR_INVALID_ALIGNMENT;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracDecode(decoder, pOutputBuffer, pOutputSamples, pDecoderStatus);
	return ret;
}

int sceAtracGetStreamInfo(int atracHandle, SceAtracStreamInfo *pStreamInfo)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pStreamInfo == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	if (pStreamInfo->size != sizeof(*pStreamInfo))
		return SCE_ATRAC_ERROR_INVALID_SIZE;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetStreamInfo(&decoder->stream, pStreamInfo);
	return ret;
}

int sceAtracAddStreamData(int atracHandle, SceUInt32 addSize)
{
	AtracDecoder *decoder = NULL;
	SceInt32 ret;

	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0) {
		ret = atracStreamAddStreamData(&decoder->stream, addSize);
		if (ret >= 0)
			ret = atracSkipSamples(decoder);
	}
	return ret;
}

int sceAtracIsSubBufferNeeded(int atracHandle)
{
	AtracDecoder *decoder = NULL;
	SceInt32 ret;

	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamIsSubBufferNeeded(&decoder->stream);
	return ret;
}

int sceAtracGetSubBufferInfo(int atracHandle, SceUInt32 *pReadPosition,
			     SceUInt32 *pMinSubBufferSize, SceUInt32 *pDataSize)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pReadPosition == NULL || pMinSubBufferSize == NULL || pDataSize == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetSubBufferInfo(&decoder->stream, pReadPosition,
						  pMinSubBufferSize, pDataSize);
	return ret;
}

int sceAtracSetSubBuffer(int atracHandle, SceUChar8 *pSubBuffer, SceUInt32 subBufferSize)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (((SceUInt32)pSubBuffer & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)
		return SCE_ATRAC_ERROR_INVALID_ALIGNMENT;
	if ((subBufferSize & (SCE_ATRAC_ALIGNMENT_SIZE - 1)) != 0)
		return SCE_ATRAC_ERROR_INVALID_ALIGNMENT;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamSetSubBuffer(&decoder->stream, pSubBuffer, subBufferSize);
	return ret;
}

int sceAtracSetLoopNum(int atracHandle, int loopNum)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (loopNum < SCE_ATRAC_INFINITE_LOOP_NUM)
		return SCE_ATRAC_ERROR_INVALID_LOOP_NUM;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamSetLoopNum(&decoder->stream, loopNum);
	return ret;
}

int sceAtracSetOutputSamples(int atracHandle, SceUInt32 outputSamples)
{
	AtracDecoder *decoder = NULL;
	SceInt32 ret;

	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamSetOutputSamples(&decoder->stream, outputSamples);
	return ret;
}

int sceAtracResetNextOutputPosition(int atracHandle, SceUInt32 resetSample)
{
	AtracDecoder *decoder = NULL;
	SceInt32 ret;

	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0) {
		ret = atracStreamResetNextOutputPosition(&decoder->stream, resetSample);
		if (ret >= 0)
			ret = atracSkipSamples(decoder);
	}
	return ret;
}

int sceAtracGetContentInfo(int atracHandle, SceAtracContentInfo *pContentInfo)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pContentInfo == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	if (pContentInfo->size != sizeof(*pContentInfo))
		return SCE_ATRAC_ERROR_INVALID_SIZE;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetContentInfo(&decoder->stream, pContentInfo);
	return ret;
}

int sceAtracGetLoopInfo(int atracHandle, int *pLoopNum, SceUInt32 *pLoopStatus)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pLoopNum == NULL || pLoopStatus == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetLoopInfo(&decoder->stream, pLoopNum, pLoopStatus);
	return ret;
}

int sceAtracGetOutputSamples(int atracHandle, SceUInt32 *pOutputSamples)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pOutputSamples == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0) {
		ret = atracStreamGetOutputSamples(&decoder->stream);
		if (ret >= 0) {
			*pOutputSamples = ret;
			ret = 0;
		}
	}
	return ret;
}

int sceAtracGetNextOutputPosition(int atracHandle, SceUInt32 *pNextOutputSample)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pNextOutputSample == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetNextOutputPosition(&decoder->stream, pNextOutputSample);
	return ret;
}

int sceAtracGetRemainSamples(int atracHandle, SceLong64 *pRemainSamples)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pRemainSamples == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if ((SceInt64)ret >= 0)
		*pRemainSamples = atracStreamRemainSamples(&decoder->stream);
	return ret;
}

int sceAtracGetOutputableSamples(int atracHandle, SceLong64 *pOutputableSamples)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pOutputableSamples == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if ((SceInt64)ret >= 0)
		*pOutputableSamples = atracStreamOutputableSamples(&decoder->stream);
	return ret;
}

int sceAtracGetDecoderStatus(int atracHandle, SceUInt32 *pDecoderStatus)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pDecoderStatus == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		*pDecoderStatus = atracStreamDecoderStatus(&decoder->stream);
	return ret;
}

int sceAtracGetVacantSize(int atracHandle, SceUInt32 *pVacantSize)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pVacantSize == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracStreamGetVacantSize(&decoder->stream, pVacantSize);
	return ret;
}

int sceAtracGetInternalError(int atracHandle, int *pInternalError)
{
	AtracDecoder *decoder;
	SceInt32 ret;

	if (pInternalError == NULL)
		return SCE_ATRAC_ERROR_INVALID_POINTER;
	decoder = NULL;
	ret = atracResolveHandle(atracHandle, &decoder);
	if (ret >= 0)
		ret = atracGetInternalError(decoder, pInternalError);
	return ret;
}
