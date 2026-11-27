/*
 * SceLibMp4Recorder AAC encoder wrapper over SceAacencInternal (libnaac).
 */
#include "mp4rec_internal.h"

static int32_t MapNaacError(int32_t err)
{
	switch (err) {
	case 0:
		return 0;
	case -1:
		return MP4REC_NAAC_ERR_INVALIDARG;
	case -2:
		return MP4REC_NAAC_ERR_INVALIDHANDLE;
	case -3:
		return MP4REC_NAAC_ERR_OUTOFMEMORY;
	case -4:
		return MP4REC_NAAC_ERR_INTERNAL;
	case -5:
		return MP4REC_NAAC_ERR_FRAME_LIMIT;
	case -6:
		return MP4REC_NAAC_ERR_NOTIMPL;
	default:
		return MP4REC_NAAC_ERR_FATAL;
	}
}

int32_t AacEncoder::QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req)
{
	(void)cfg;
	req->heapSize += (uint32_t)SceAacencInternal_118F588C() + 2048U;
	return 0;
}

AacEncoder::AacEncoder()
{
	m_workBuf = NULL;
	m_handle = NULL;
}

AacEncoder::~AacEncoder()
{
}

int32_t AacEncoder::Term(MemAllocator *allocator)
{
	if (m_handle != NULL) {
		SceAacencInternal_B3004C1B(m_handle);
		m_handle = NULL;
	}
	if (m_workBuf != NULL)
		allocator->Free(m_workBuf);
	return 0;
}

int32_t AacEncoder::Init(const AvrecConfig *cfg, MemAllocator *allocator, uint32_t *outMaxAuSize)
{
	int32_t channelOrder[4] = { 0, 1, 1, 0 };
	int32_t channelStride[2] = { 2, 2 };
	Mp4RecNaacConfig naacCfg;
	memset(&naacCfg, 0, sizeof(naacCfg));

	naacCfg.numChannels = 2;
	naacCfg.sampleRate = 48000;
	naacCfg.bitRate = (int32_t)cfg->audioBitrate;
	naacCfg.audioObjectType = 0;
	naacCfg.streamFormat = 0;
	naacCfg.enableTns = 1;
	naacCfg.enableMs = 0;
	naacCfg.disablePns = 1;
	naacCfg.channelOrder = channelOrder;
	naacCfg.channelStride = channelStride;
	naacCfg.reserved[8] = 1;
	naacCfg.reserved[9] = 2;
	naacCfg.reserved[10] = 1;

	int32_t workSize = SceAacencInternal_118F588C();
	m_workBuf = allocator->Malloc((size_t)workSize);
	if (m_workBuf == NULL) {
		Term(allocator);
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	}

	int32_t ret = SceAacencInternal_71BDD121(&m_handle, m_workBuf);
	if (ret < 0) {
		Term(allocator);
		return MapNaacError(ret);
	}

	ret = SceAacencInternal_C867CE87(m_handle, &naacCfg);
	if (ret < 0) {
		Term(allocator);
		return MapNaacError(ret);
	}

	if (outMaxAuSize != NULL)
		*outMaxAuSize = 2048U;
	return 0;
}

int32_t AacEncoder::Encode(const void *pcm, uint32_t pcmBytes, void *outBuf,
	uint32_t *inOutSize, int32_t isFlush)
{
	int32_t ret;
	int32_t outBytes = (int32_t)*inOutSize;

	if (isFlush != 0)
		ret = SceAacencInternal_30D75275(m_handle, (uint8_t *)outBuf, &outBytes, 0);
	else
		ret = SceAacencInternal_B8BEEDE6(m_handle, pcm, pcmBytes, (uint8_t *)outBuf, &outBytes, outBytes);

	if (ret < 0) {
		*inOutSize = 0;
		return MapNaacError(ret);
	}

	*inOutSize = (uint32_t)outBytes;
	return 0;
}

int32_t AacEncoder::Flush()
{
	return 0;
}
