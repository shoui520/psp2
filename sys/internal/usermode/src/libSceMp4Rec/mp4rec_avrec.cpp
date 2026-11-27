/*
 * SceLibMp4Recorder Avrec coordinator (AVC encoder, AAC encoder, and MP4 muxer).
 */
#include "mp4rec_internal.h"

static uint8_t s_avrecLowSpaceFlag = 0;
static Mp4RecMutex *s_avrecSpaceMutex = NULL;

Avrec::Avrec()
{
	m_avcEnc = NULL;
	m_aacEnc = NULL;
	m_muxer = NULL;
	m_videoAuBuf = NULL;
	m_videoAuBufSize = 0;
	s_avrecLowSpaceFlag = 0;
	s_avrecSpaceMutex = NULL;
}

Avrec::~Avrec()
{
}

int32_t Avrec::QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req)
{
	if (cfg == NULL || req == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	req->heapSize = 0;
	req->phyMemSize = 0;

	int32_t ret = AvcEncoder::QueryMemSize(cfg, req);
	if (ret >= 0) {
		ret = AacEncoder::QueryMemSize(cfg, req);
		if (ret >= 0)
			ret = Mp4Muxer::QueryMemSize(cfg, req);
	}
	return ret;
}

int32_t Avrec::Cleanup()
{
	int32_t ret = 0;
	if (m_muxer != NULL)
		ret = m_muxer->Term();

	if (m_avcEnc != NULL) {
		m_avcEnc->~AvcEncoder();
		m_allocator->Free(m_avcEnc);
		m_avcEnc = NULL;
	}
	if (m_aacEnc != NULL) {
		m_aacEnc->~AacEncoder();
		m_allocator->Free(m_aacEnc);
		m_aacEnc = NULL;
	}
	if (m_muxer != NULL) {
		m_muxer->~Mp4Muxer();
		m_allocator->Free(m_muxer);
		m_muxer = NULL;
	}
	if (s_avrecSpaceMutex != NULL) {
		s_avrecSpaceMutex->~Mp4RecMutex();
		m_allocator->Free(s_avrecSpaceMutex);
		s_avrecSpaceMutex = NULL;
	}
	return ret;
}

int32_t Avrec::ValidateConfig(const AvrecConfig *cfg)
{
	if (cfg->format != 0 || cfg->videoCodec != 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	if ((cfg->width & 0xFU) != 0 || (cfg->pitch & 0xFU) != 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	if (cfg->width > cfg->pitch || (cfg->height & 0xFU) != 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	if (cfg->frameRateCode > 3U || cfg->audioCodec != 0 || cfg->audioSampleRateCode != 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	return 0;
}

int32_t Avrec::Init(const char *filePath, const AvrecConfig *cfg, MemAllocator *allocator)
{
	if (cfg == NULL || allocator == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	m_config = *cfg;
	int32_t status = ValidateConfig(&m_config);
	if (status >= 0) {
		if (allocator->m_initialized == 0)
			return MP4REC_AVREC_ERR_INVALID_VALUE;

		m_allocator = allocator;
		void *avcMem = m_allocator->Malloc(sizeof(AvcEncoder));
		void *aacMem = m_allocator->Malloc(sizeof(AacEncoder));
		void *muxMem = m_allocator->Malloc(sizeof(Mp4Muxer));
		void *mtxMem = m_allocator->Malloc(sizeof(Mp4RecMutex));
		if (avcMem == NULL || aacMem == NULL || muxMem == NULL || mtxMem == NULL)
			status = MP4REC_AVREC_ERR_OUT_OF_MEMORY;

		m_avcEnc = (avcMem != NULL) ? new (avcMem) AvcEncoder() : NULL;
		m_aacEnc = (aacMem != NULL) ? new (aacMem) AacEncoder() : NULL;
		m_muxer = (muxMem != NULL) ? new (muxMem) Mp4Muxer() : NULL;
		s_avrecSpaceMutex = (mtxMem != NULL) ? new (mtxMem) Mp4RecMutex(&status, "SceMp4RecAvrecMutex", 3) : NULL;

		if (status >= 0) {
			status = m_avcEnc->Init(&m_config, m_allocator, &m_videoAuBufSize);
			if (status >= 0) {
				m_videoAuBuf = m_allocator->Malloc(m_videoAuBufSize);
				if (m_videoAuBuf == NULL) {
					status = MP4REC_AVREC_ERR_OUT_OF_MEMORY;
				} else {
					status = m_aacEnc->Init(&m_config, m_allocator, &m_audioAuBufSize);
					if (status >= 0) {
						m_audioAuBuf = m_allocator->Malloc(m_audioAuBufSize);
						if (m_audioAuBuf == NULL) {
							status = MP4REC_AVREC_ERR_OUT_OF_MEMORY;
						} else {
							status = m_muxer->Init(&m_config, filePath, m_allocator);
							if (status >= 0) {
								m_muxer->m_fileWriter.SetLowSpaceCallback(
									m_muxer, MP4REC_MIN_FREE_SPACE, OnLowFreeSpace);
							}
						}
					}
				}
			}
		}
	}

	if (status < 0) {
		if (m_avcEnc != NULL)
			m_avcEnc->Term(m_allocator);
		if (m_aacEnc != NULL)
			m_aacEnc->Term(m_allocator);
		Cleanup();
	}
	return status;
}

int32_t Avrec::FlushVideoOnce()
{
	uint32_t outSize = m_videoAuBufSize;
	int32_t mode = 2;
	AvrecVideoFrameMeta meta;
	int32_t ret = m_avcEnc->Encode(NULL, 0, m_videoAuBuf, &outSize, &meta, &mode);
	if (ret >= 0) {
		ret = m_muxer->AddVideoAu(m_videoAuBuf, outSize, &meta);
		if (ret >= 0)
			ret = (int32_t)outSize;
	}
	return ret;
}

int32_t Avrec::FlushAudioOnce()
{
	uint32_t outSize = m_audioAuBufSize;
	int32_t ret = m_aacEnc->Encode(NULL, 0, m_audioAuBuf, &outSize, 1);
	if (ret >= 0) {
		ret = m_muxer->AddAudioAu(m_audioAuBuf, outSize);
		if (ret >= 0)
			ret = (int32_t)outSize;
	}
	return ret;
}

int32_t Avrec::StopEncoders()
{
	void *sps = NULL;
	uint32_t spsSize = 0;
	void *pps = NULL;
	uint32_t ppsSize = 0;

	m_avcEnc->GetParameterSets(&sps, &spsSize, &pps, &ppsSize);
	m_muxer->SetAvcConfig(sps, spsSize, pps, ppsSize);

	int32_t ret;
	do {
		ret = FlushAudioOnce();
	} while (ret > 0);

	do {
		ret = FlushVideoOnce();
	} while (ret > 0);

	m_avcEnc->Term(m_allocator);
	m_aacEnc->Term(m_allocator);

	if (m_videoAuBuf != NULL) {
		m_allocator->Free(m_videoAuBuf);
		m_videoAuBuf = NULL;
	}
	if (m_audioAuBuf != NULL) {
		m_allocator->Free(m_audioAuBuf);
		m_audioAuBuf = NULL;
	}
	return 0;
}

int32_t Avrec::FinalizeFile(const uint16_t *creationDate)
{
	return m_muxer->Finalize(creationDate);
}

int32_t Avrec::AddVideoFrame(const void *yuv, uint32_t size, uint8_t *outDropped)
{
	if (yuv == NULL || size == 0 || outDropped == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	int32_t ret = s_avrecSpaceMutex->Lock();
	if (ret < 0)
		return ret;
	if (s_avrecLowSpaceFlag != 0) {
		s_avrecSpaceMutex->Unlock();
		return (int32_t)SCE_MP4REC_ERROR_NOSPACE;
	}
	s_avrecSpaceMutex->Unlock();

	uint32_t outSize = m_videoAuBufSize;
	uint32_t margin = 2U * (((uint32_t)m_avcEnc->GetMaxGopFrames() << 7) + 0x2000U + m_avcEnc->GetMaxFrameBytes());
	uint32_t freeSpace = m_muxer->m_videoBuf.GetFreeSpace();
	int32_t dropFlag = (freeSpace <= margin) ? 1 : 0;

	AvrecVideoFrameMeta meta;
	ret = m_avcEnc->Encode(yuv, size, m_videoAuBuf, &outSize, &meta, &dropFlag);
	if (ret >= 0) {
		ret = m_muxer->AddVideoAu(m_videoAuBuf, outSize, &meta);
		if (ret >= 0) {
			ret = (int32_t)outSize;
			*outDropped = (uint8_t)dropFlag;
		}
	}
	if (dropFlag != 0)
		ret = (int32_t)SCE_MP4REC_ERROR_AVREC_VIDEO_BUFFER_FULL;
	return ret;
}

int32_t Avrec::AddAudioFrame(const void *pcm, uint32_t size)
{
	if (pcm == NULL || size == 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	int32_t ret = s_avrecSpaceMutex->Lock();
	if (ret < 0)
		return ret;
	if (s_avrecLowSpaceFlag != 0) {
		s_avrecSpaceMutex->Unlock();
		return (int32_t)SCE_MP4REC_ERROR_NOSPACE;
	}
	s_avrecSpaceMutex->Unlock();

	uint32_t outSize = m_audioAuBufSize;
	ret = m_aacEnc->Encode(pcm, size, m_audioAuBuf, &outSize, 0);
	if (ret >= 0) {
		ret = m_muxer->AddAudioAu(m_audioAuBuf, outSize);
		if (ret >= 0)
			ret = (int32_t)outSize;
	}
	return ret;
}

int32_t Avrec::AddMetadata(const AvrecMetadataItem *item)
{
	return m_muxer->AddMetadata(item);
}

int32_t Avrec::PrepareThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count, int32_t flag)
{
	return m_muxer->PrepareThumbnails(thumbs, count, flag);
}

int32_t Avrec::WriteThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count)
{
	return m_muxer->WriteThumbnails(thumbs, count);
}

void Avrec::OnLowFreeSpace(uint64_t freeSpace)
{
	(void)freeSpace;
	if (s_avrecSpaceMutex != NULL) {
		s_avrecSpaceMutex->Lock();
		s_avrecLowSpaceFlag = 1;
		s_avrecSpaceMutex->Unlock();
	}
}
