/*
 * SceLibMp4Recorder high-level Recorder class.
 */
#include "mp4rec_internal.h"

static const Mp4RecModeSpec s_recorderModes[6] = {
	{ 640, 640, 368, 353280, 2000000 },
	{ 640, 640, 368, 353280, 1000000 },
	{ 480, 480, 272, 195840, 2000000 },
	{ 480, 480, 272, 195840, 1000000 },
	{ 368, 368, 208, 114816, 2000000 },
	{ 368, 368, 208, 114816, 1000000 },
};

static const uint32_t s_supportedLangs[17] = {
	SCE_MP4REC_LANG_JAPANESE,
	SCE_MP4REC_LANG_ENGLISH,
	SCE_MP4REC_LANG_FRENCH,
	SCE_MP4REC_LANG_SPANISH,
	SCE_MP4REC_LANG_GERMAN,
	SCE_MP4REC_LANG_ITALIAN,
	SCE_MP4REC_LANG_DUTCH,
	SCE_MP4REC_LANG_PORTUGUESE,
	SCE_MP4REC_LANG_RUSSIAN,
	SCE_MP4REC_LANG_KOREAN,
	SCE_MP4REC_LANG_CHINESE,
	SCE_MP4REC_LANG_FINNISH,
	SCE_MP4REC_LANG_SWEDISH,
	SCE_MP4REC_LANG_DANISH,
	SCE_MP4REC_LANG_NORWEGIAN,
	SCE_MP4REC_LANG_POLISH,
	SCE_MP4REC_LANG_TURKISH,
};

static const uint16_t s_vitaVideoCreator[14] = {
	0x5000, 0x5300, 0x2000, 0x5600, 0x6900, 0x7400, 0x6100,
	0x2000, 0x5600, 0x6900, 0x6400, 0x6500, 0x6F00, 0x0000
};

typedef struct ParentalRatingEntry {
	uint32_t level;
	const uint8_t *utf16beAge;
} ParentalRatingEntry;

static const uint8_t s_age20[6] = { 0x00, '2', 0x00, '0', 0x00, 0x00 };
static const uint8_t s_age18[6] = { 0x00, '1', 0x00, '8', 0x00, 0x00 };
static const uint8_t s_age17[6] = { 0x00, '1', 0x00, '7', 0x00, 0x00 };
static const uint8_t s_age16[6] = { 0x00, '1', 0x00, '6', 0x00, 0x00 };
static const uint8_t s_age15[6] = { 0x00, '1', 0x00, '5', 0x00, 0x00 };
static const uint8_t s_age13[6] = { 0x00, '1', 0x00, '3', 0x00, 0x00 };
static const uint8_t s_age12[6] = { 0x00, '1', 0x00, '2', 0x00, 0x00 };
static const uint8_t s_age10[6] = { 0x00, '1', 0x00, '0', 0x00, 0x00 };
static const uint8_t s_age06[6] = { 0x00, '0', 0x00, '6', 0x00, 0x00 };
static const uint8_t s_age03[6] = { 0x00, '0', 0x00, '3', 0x00, 0x00 };
static const uint8_t s_age01[6] = { 0x00, '0', 0x00, '1', 0x00, 0x00 };

static const ParentalRatingEntry s_parentalRatings[11] = {
	{ 11, s_age20 },
	{ 10, s_age18 },
	{  9, s_age17 },
	{  8, s_age16 },
	{  7, s_age15 },
	{  6, s_age13 },
	{  5, s_age12 },
	{  4, s_age10 },
	{  3, s_age06 },
	{  2, s_age03 },
	{  1, s_age01 },
};

Recorder::Recorder(uint32_t ctrlSize)
	: m_avrec(),
	  m_allocator(),
	  m_stateMutex(&m_stateMutexErr, "SceMp4RecRecorder", 0),
	  m_infoMutex(&m_infoMutexErr, "SceMp4RecRecorder2", 0)
{
	m_startTime = 0;
	m_recordedMs = 0;
	m_asyncError = 0;
	m_reserved208 = 0;
	m_reserved212 = 0;
	m_videoDurationTicks = 0;
	m_audioDurationTicks = 0;
	m_state = 0;
	m_ctrlSize = ctrlSize;
	m_lastDevCheckErr = 0;
}

Recorder::~Recorder()
{
}

int32_t Recorder::CheckFreeSpace(uint32_t extraBytes)
{
	uint64_t maxSize = 0;
	uint64_t freeSize = 0;
	uint32_t required = extraBytes + MP4REC_MIN_FREE_SPACE;

	int32_t ret = sceAppMgrGetDevInfo("ux0:", &maxSize, &freeSize);
	if (ret >= 0 && freeSize <= (uint64_t)required)
		ret = (int32_t)SCE_MP4REC_ERROR_NOSPACE;
	return ret;
}

int32_t Recorder::Prepare()
{
	if (m_stateMutexErr < 0)
		return m_stateMutexErr;
	if (m_infoMutexErr < 0)
		return m_infoMutexErr;

	int32_t ret = m_stateMutex.Lock();
	if (ret < 0)
		return ret;

	if (m_state != 0) {
		m_stateMutex.Unlock();
		return (int32_t)SCE_MP4REC_ERROR_STATE;
	}

	m_state = 1;
	int32_t lockRes = m_infoMutex.Lock();
	if (lockRes < 0) {
		m_stateMutex.Unlock();
		return lockRes;
	}

	m_startTime = 0;
	m_recordedMs = 0;
	m_asyncError = 0;
	m_reserved208 = 0;
	m_reserved212 = 0;
	m_infoMutex.Unlock();

	uint64_t maxSize = 0;
	uint64_t freeSize = 0;
	int32_t devRes = sceAppMgrGetDevInfo("ux0:", &maxSize, &freeSize);
	if (devRes >= 0 && freeSize <= MP4REC_MIN_FREE_SPACE)
		devRes = (int32_t)SCE_MP4REC_ERROR_NOSPACE;
	m_lastDevCheckErr = devRes;

	m_stateMutex.Unlock();
	return m_lastDevCheckErr;
}

int32_t Recorder::BuildConfig(uint32_t modeIndex, uint32_t modeOption, AvrecConfig *outCfg)
{
	if (modeIndex >= 6U)
		return (int32_t)SCE_MP4REC_ERROR_INVALID_RECMODE;

	memset(outCfg, 0, sizeof(*outCfg));
	outCfg->pitch = s_recorderModes[modeIndex].width;
	outCfg->width = s_recorderModes[modeIndex].pitch;
	outCfg->height = s_recorderModes[modeIndex].height;
	uint32_t bitrate = s_recorderModes[modeIndex].bitrate;
	outCfg->targetBitrate = bitrate;
	outCfg->maxBitrate = (uint32_t)((double)bitrate * 1.2);
	outCfg->frameRateCode = 3;
	outCfg->rotation = 0;
	outCfg->audioCodec = 0;
	outCfg->audioSampleRateCode = 0;
	outCfg->audioBitrate = 128000U;
	outCfg->modeOption = modeOption;
	return 0;
}

int32_t Recorder::Start(const RecorderStartParam *param)
{
	if (param->filePath == NULL || param->allocIf == NULL ||
		param->encoderPhyPtr == NULL || param->encoderPhySize == 0) {
		return (int32_t)SCE_MP4REC_ERROR_INVALID_ARG;
	}

	int32_t ret = m_stateMutex.Lock();
	if (ret < 0)
		return ret;

	if (m_state != 1) {
		m_stateMutex.Unlock();
		return (int32_t)SCE_MP4REC_ERROR_STATE;
	}

	m_state = 2;
	AvrecConfig cfg;
	ret = BuildConfig(param->modeIndex, param->modeOption, &cfg);
	if (ret >= 0) {
		cfg.writeThreadAffinity = param->affinity;
		cfg.writeThreadPriority = param->priority;
		AvrecMemReq req;
		ret = Avrec::QueryMemSize(&cfg, &req);
		if (ret >= 0) {
			ret = m_allocator.Init((AllocatorInterface *)param->allocIf,
				param->encoderPhySize, param->encoderPhyPtr);
			if (ret >= 0) {
				ret = m_avrec.Init(param->filePath, &cfg, &m_allocator);
				if (ret >= 0) {
					int32_t infoRes = m_infoMutex.Lock();
					if (infoRes >= 0) {
						m_startTime = sceKernelGetProcessTimeWide();
						m_recordedMs = 0;
						m_infoMutex.Unlock();
						m_stateMutex.Unlock();
						return 0;
					}
					m_stateMutex.Unlock();
					return infoRes;
				}
			}
		}
	}
	return ret;
}

int32_t Recorder::Pause()
{
	return 0;
}

int32_t Recorder::Stop()
{
	int32_t ret = m_stateMutex.Lock();
	if (ret < 0)
		return ret;

	if (m_state != 2) {
		m_stateMutex.Unlock();
		return (int32_t)SCE_MP4REC_ERROR_STATE;
	}

	m_state = 3;
	int32_t infoRes = m_infoMutex.Lock();
	if (infoRes < 0) {
		m_stateMutex.Unlock();
		return infoRes;
	}

	uint32_t videoMs = 0;
	uint32_t audioMs = 0;
	Mp4RecRescaleDuration(m_videoDurationTicks, SCE_MP4REC_VIDEO_TIMESCALE, 1000U, &videoMs);
	Mp4RecRescaleDuration(m_audioDurationTicks, SCE_MP4REC_AUDIO_TIMESCALE, 1000U, &audioMs);
	m_recordedMs = (audioMs > videoMs) ? audioMs : videoMs;
	m_infoMutex.Unlock();

	m_startTime = 0;
	int32_t stopRes = m_avrec.StopEncoders();
	m_stateMutex.Unlock();
	return (stopRes < 0) ? stopRes : 0;
}

int32_t Recorder::Finalize(const RecorderTermInfo *info, AllocatorInterface *allocIf)
{
	if (allocIf == NULL)
		return (int32_t)SCE_MP4REC_ERROR_INVALID_ARG;

	int32_t ret = m_stateMutex.Lock();
	if (ret < 0)
		return ret;

	if (m_state != 3) {
		m_stateMutex.Unlock();
		return (int32_t)SCE_MP4REC_ERROR_STATE;
	}

	m_state = 1;
	int32_t infoRes = m_infoMutex.Lock();
	if (infoRes < 0) {
		m_stateMutex.Unlock();
		return infoRes;
	}

	m_recordedMs = 0;
	m_startTime = 0;
	m_infoMutex.Unlock();

	uint16_t creatorBuf[14];
	memcpy(creatorBuf, s_vitaVideoCreator, sizeof(creatorBuf));

	AvrecMetadataItem creatorItem;
	memset(&creatorItem, 0, sizeof(creatorItem));
	creatorItem.type = 5;
	creatorItem.flag = 1;
	creatorItem.lang = SCE_MP4REC_LANG_ENGLISH;
	creatorItem.encoding = 1;
	creatorItem.data = creatorBuf;
	creatorItem.dataSize = sizeof(creatorBuf);

	int32_t err = m_avrec.AddMetadata(&creatorItem);
	if (err < 0) {
		m_avrec.FinalizeFile(NULL);
		m_avrec.Cleanup();
		m_stateMutex.Unlock();
		return err;
	}

	uint16_t titleIdUtf16[12];
	if (info->titleId != NULL) {
		int32_t convChars = Mp4RecConvertUtf8ToUtf16Be(info->titleId, 10, titleIdUtf16, 12);
		if (convChars != 0) {
			AvrecMetadataItem titleIdItem;
			memset(&titleIdItem, 0, sizeof(titleIdItem));
			titleIdItem.type = 4;
			titleIdItem.flag = 1;
			titleIdItem.lang = SCE_MP4REC_LANG_ENGLISH;
			titleIdItem.encoding = 1;
			titleIdItem.data = titleIdUtf16;
			titleIdItem.dataSize = (uint32_t)(convChars * 2 + 2);
			err = m_avrec.AddMetadata(&titleIdItem);
			if (err < 0) {
				m_avrec.FinalizeFile(NULL);
				m_avrec.Cleanup();
				m_stateMutex.Unlock();
				return err;
			}
		}
	}

	uint32_t thumbCount = 0;
	int32_t hasThumbs = 0;
	AvrecThumbnailDesc thumbDescs[SCE_MP4REC_THUMBNAIL_NUM_MAX];
	memset(thumbDescs, 0, sizeof(thumbDescs));

	SceMp4RecMetadata *meta = info->metadata;
	if (meta != NULL && meta->size == sizeof(SceMp4RecMetadata)) {
		uint32_t matchedLang = 0;
		for (uint32_t i = 0; i < 17U; i++) {
			if (meta->lang == s_supportedLangs[i])
				matchedLang = s_supportedLangs[i];
		}

		if (meta->pTitle != NULL) {
			uint16_t *titlePtr = (uint16_t *)meta->pTitle;
			uint32_t titleBytes = meta->titleSize;
			uint16_t *lastChar = (titleBytes >= 2U && (titleBytes & 1U) == 0)
				? (uint16_t *)((uint8_t *)titlePtr + titleBytes - 2U) : NULL;
			if (titleBytes != 0 && matchedLang != 0 && titleBytes <= SCE_MP4REC_TITLE_SIZE_MAX &&
				(titleBytes & 1U) == 0 && lastChar != NULL && *lastChar == 0) {
				AvrecMetadataItem titleItem;
				memset(&titleItem, 0, sizeof(titleItem));
				titleItem.type = 1;
				titleItem.flag = 1;
				titleItem.lang = (uint16_t)matchedLang;
				titleItem.encoding = 1;
				titleItem.data = titlePtr;
				titleItem.dataSize = titleBytes;

				char *trimmed = NULL;
				for (uint32_t off = 0; off < titleBytes; off += 2U) {
					if (titlePtr[off / 2U] == 0x0A00U) {
						trimmed = (char *)allocIf->Malloc(off + 2U);
						if (trimmed != NULL) {
							memcpy(trimmed, titlePtr, off + 2U);
							*(uint16_t *)(trimmed + off) = 0;
							titleItem.data = trimmed;
							titleItem.dataSize = off + 2U;
						} else {
							titleItem.data = NULL;
							titleItem.dataSize = 0;
						}
						break;
					}
				}

				err = m_avrec.AddMetadata(&titleItem);
				if (err < 0) {
					m_avrec.FinalizeFile(NULL);
					m_avrec.Cleanup();
					m_stateMutex.Unlock();
					return err;
				}
				if (trimmed != NULL)
					allocIf->Free(trimmed);
			}
		}

		if (meta->pDescription != NULL && meta->descriptionSize != 0 &&
			matchedLang != 0 && meta->descriptionSize <= SCE_MP4REC_DESCRIPTION_SIZE_MAX) {
			AvrecMetadataItem descItem;
			memset(&descItem, 0, sizeof(descItem));
			descItem.type = 6;
			descItem.flag = 1;
			descItem.lang = (uint16_t)matchedLang;
			descItem.encoding = 1;
			descItem.data = meta->pDescription;
			descItem.dataSize = meta->descriptionSize;
			err = m_avrec.AddMetadata(&descItem);
			if (err < 0) {
				m_avrec.FinalizeFile(NULL);
				m_avrec.Cleanup();
				m_stateMutex.Unlock();
				return err;
			}
		}

		if (meta->pCopyrightNotice != NULL && meta->copyrightNoticeSize != 0 &&
			matchedLang != 0 && meta->copyrightNoticeSize <= SCE_MP4REC_COPYRIGHT_NOTICE_SIZE_MAX) {
			AvrecMetadataItem copyItem;
			memset(&copyItem, 0, sizeof(copyItem));
			copyItem.type = 2561;
			copyItem.flag = 1;
			copyItem.lang = (uint16_t)matchedLang;
			copyItem.encoding = 1;
			copyItem.data = meta->pCopyrightNotice;
			copyItem.dataSize = meta->copyrightNoticeSize;
			err = m_avrec.AddMetadata(&copyItem);
			if (err < 0) {
				m_avrec.FinalizeFile(NULL);
				m_avrec.Cleanup();
				m_stateMutex.Unlock();
				return err;
			}
		}

		if (meta->parentalLevel != 0) {
			uint32_t ratingIdx = 0;
			for (uint32_t i = 0; i < 11U; i++) {
				if (meta->parentalLevel == s_parentalRatings[i].level) {
					ratingIdx = i;
					break;
				}
			}
			AvrecMetadataItem parentalItem;
			memset(&parentalItem, 0, sizeof(parentalItem));
			parentalItem.type = 2562;
			parentalItem.flag = 1;
			parentalItem.lang = SCE_MP4REC_LANG_ENGLISH;
			parentalItem.encoding = 1;
			parentalItem.data = (void *)s_parentalRatings[ratingIdx].utf16beAge;
			parentalItem.dataSize = 6;
			err = m_avrec.AddMetadata(&parentalItem);
			if (err < 0) {
				m_avrec.FinalizeFile(NULL);
				m_avrec.Cleanup();
				m_stateMutex.Unlock();
				return err;
			}
		}

		if (meta->numOfChapter != 0) {
			uint32_t chCount = (meta->numOfChapter <= SCE_MP4REC_CHAPTER_NUM_MAX)
				? meta->numOfChapter : SCE_MP4REC_CHAPTER_NUM_MAX;
			uint32_t chBufSize = chCount * 4U + 20U;
			uint8_t *chBuf = (uint8_t *)allocIf->Malloc(chBufSize);
			if (chBuf != NULL) {
				memset(chBuf, 0, chBufSize);
				uint32_t v90k = 0;
				uint32_t a90k = 0;
				Mp4RecRescaleDuration(m_videoDurationTicks, SCE_MP4REC_VIDEO_TIMESCALE, 90000U, &v90k);
				Mp4RecRescaleDuration(m_audioDurationTicks, SCE_MP4REC_AUDIO_TIMESCALE, 90000U, &a90k);
				uint32_t max90k = (a90k < v90k) ? v90k : a90k;
				Mp4RecBuildChapterData(chBuf, chBufSize, meta->pTopOfChapterTbl, chCount, max90k);

				AvrecMetadataItem chItem;
				memset(&chItem, 0, sizeof(chItem));
				chItem.type = 13;
				chItem.flag = 1;
				chItem.lang = 0x55C4U;
				chItem.encoding = 0;
				chItem.data = chBuf;
				chItem.dataSize = chBufSize;
				err = m_avrec.AddMetadata(&chItem);
				if (err < 0) {
					m_avrec.FinalizeFile(NULL);
					m_avrec.Cleanup();
					m_stateMutex.Unlock();
					return err;
				}
				allocIf->Free(chBuf);
			}
		}

		if (meta->numOfThumbnail != 0 && meta->numOfThumbnail <= SCE_MP4REC_THUMBNAIL_NUM_MAX) {
			int32_t hasRequired = 0;
			for (uint32_t i = 0; i < meta->numOfThumbnail && i < SCE_MP4REC_THUMBNAIL_NUM_MAX; i++) {
				int32_t w = 0, h = 0, outFmt = 0, strFmt = 0;
				if (scePngGetOutputInfo(meta->thumbnailInfo[i].pPng, meta->thumbnailInfo[i].pngSize,
						&w, &h, &outFmt, &strFmt) >= 0) {
					if (w <= SCE_MP4REC_THUMBNAIL_WIDTH_MAX && h <= SCE_MP4REC_THUMBNAIL_HEIGHT_MAX) {
						thumbDescs[thumbCount].width = (uint16_t)w;
						thumbDescs[thumbCount].height = (uint16_t)h;
						thumbDescs[thumbCount].depth = 24;
						thumbDescs[thumbCount].format = 1;
						thumbDescs[thumbCount].data = meta->thumbnailInfo[i].pPng;
						thumbDescs[thumbCount].size = meta->thumbnailInfo[i].pngSize;
						thumbCount++;
						hasThumbs = 1;
						if (w == SCE_MP4REC_THUMBNAIL_WIDTH_REQUIRED &&
							h == SCE_MP4REC_THUMBNAIL_HEIGHT_REQUIRED) {
							hasRequired = 1;
						}
					}
				}
			}

			if (hasThumbs != 0 && hasRequired != 0) {
				err = m_avrec.PrepareThumbnails(thumbDescs, thumbCount, 1);
				if (err < 0) {
					m_avrec.FinalizeFile(NULL);
					m_avrec.Cleanup();
					m_stateMutex.Unlock();
					return err;
				}
			} else {
				thumbCount = 0;
				hasThumbs = 0;
			}
		}
	}

	uint16_t dateFields[6];
	uint16_t *datePtr = NULL;
	if (info->startUtcTick.tick != 0) {
		SceDateTime dt;
		if (sceRtcSetTick(&dt, &info->startUtcTick) == 0) {
			dateFields[0] = dt.year;
			dateFields[1] = dt.month;
			dateFields[2] = dt.day;
			dateFields[3] = dt.hour;
			dateFields[4] = dt.minute;
			dateFields[5] = dt.second;
			datePtr = dateFields;
		}
	}

	err = m_avrec.FinalizeFile(datePtr);
	if (err >= 0 && hasThumbs != 0)
		err = m_avrec.WriteThumbnails(thumbDescs, thumbCount);

	int32_t cleanErr = m_avrec.Cleanup();
	if (err >= 0)
		err = cleanErr;

	m_stateMutex.Unlock();
	return err;
}

int32_t Recorder::Unprepare()
{
	int32_t ret = m_stateMutex.Lock();
	if (ret < 0)
		return ret;

	if (m_state != 1) {
		m_stateMutex.Unlock();
		return (int32_t)SCE_MP4REC_ERROR_STATE;
	}

	m_state = 0;
	int32_t infoRes = m_infoMutex.Lock();
	if (infoRes >= 0) {
		m_startTime = 0;
		m_recordedMs = 0;
		m_asyncError = 0;
		m_reserved208 = 0;
		m_reserved212 = 0;
		m_infoMutex.Unlock();
	}
	m_stateMutex.Unlock();
	return infoRes;
}

int32_t Recorder::WaitIdle()
{
	int32_t ret = m_stateMutex.Lock();
	if (ret >= 0)
		ret = m_stateMutex.Unlock();
	return ret;
}

uint32_t Recorder::GetDurationMs()
{
	if (m_infoMutex.Lock() < 0)
		return 0;

	uint32_t videoMs = 0;
	uint32_t audioMs = 0;
	Mp4RecRescaleDuration(m_videoDurationTicks, SCE_MP4REC_VIDEO_TIMESCALE, 1000U, &videoMs);
	Mp4RecRescaleDuration(m_audioDurationTicks, SCE_MP4REC_AUDIO_TIMESCALE, 1000U, &audioMs);
	uint32_t result = (audioMs > videoMs) ? audioMs : videoMs;
	m_infoMutex.Unlock();
	return result;
}

int32_t Recorder::TakeAsyncError()
{
	int32_t ret = m_infoMutex.Lock();
	if (ret >= 0) {
		int32_t err = m_asyncError;
		m_asyncError = 0;
		m_infoMutex.Unlock();
		return err;
	}
	return ret;
}

int32_t Recorder::AddAudioSample(const void *pcm, uint32_t size)
{
	if (m_lastDevCheckErr != 0)
		return m_lastDevCheckErr;

	int32_t ret = m_avrec.AddAudioFrame(pcm, size);
	if (ret >= 0) {
		m_audioDurationTicks += SCE_MP4REC_AUDIO_SAMPLE_DURATION;
		return 0;
	}

	m_infoMutex.Lock();
	if (m_asyncError == 0)
		m_asyncError = ret;
	m_infoMutex.Unlock();
	return ret;
}

void Recorder::AddAudioSampleNoReturn(const void *pcm, uint32_t size)
{
	if (m_lastDevCheckErr != 0)
		return;

	int32_t ret = m_avrec.AddAudioFrame(pcm, size);
	if (ret >= 0) {
		m_audioDurationTicks += SCE_MP4REC_AUDIO_SAMPLE_DURATION;
		return;
	}

	m_infoMutex.Lock();
	if (m_asyncError == 0)
		m_asyncError = ret;
	m_infoMutex.Unlock();
}

int32_t Recorder::AddVideoSample(const void *yuv, uint32_t size, uint8_t *outDropped)
{
	if (m_lastDevCheckErr != 0)
		return m_lastDevCheckErr;

	int32_t ret = m_avrec.AddVideoFrame(yuv, size, outDropped);
	if (ret >= 0) {
		m_videoDurationTicks += SCE_MP4REC_VIDEO_SAMPLE_DURATION;
		return 0;
	}

	m_infoMutex.Lock();
	if (m_asyncError == 0)
		m_asyncError = ret;
	m_infoMutex.Unlock();
	return ret;
}

void Recorder::AddVideoSampleNoReturn(const void *yuv, uint32_t size, uint8_t *outDropped)
{
	if (m_lastDevCheckErr != 0)
		return;

	int32_t ret = m_avrec.AddVideoFrame(yuv, size, outDropped);
	if (ret >= 0) {
		m_videoDurationTicks += SCE_MP4REC_VIDEO_SAMPLE_DURATION;
		return;
	}

	m_infoMutex.Lock();
	if (m_asyncError == 0)
		m_asyncError = ret;
	m_infoMutex.Unlock();
}

int32_t Recorder::QueryMemSize(uint32_t modeIndex, uint32_t modeOption, AvrecMemReq *outReq)
{
	AvrecConfig cfg;
	int32_t ret = BuildConfig(modeIndex, modeOption, &cfg);
	if (ret >= 0)
		ret = Avrec::QueryMemSize(&cfg, outReq);
	return ret;
}
