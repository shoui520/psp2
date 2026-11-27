/*
 * SceLibMp4Recorder public C API and VideoExport integration.
 */
#include "mp4rec_internal.h"

static const Mp4RecModeSpec s_apiModes[6] = {
	{ 640, 640, 368, 353280, 2000000 },
	{ 640, 640, 368, 353280, 1000000 },
	{ 480, 480, 272, 195840, 2000000 },
	{ 480, 480, 272, 195840, 1000000 },
	{ 368, 368, 208, 114816, 2000000 },
	{ 368, 368, 208, 114816, 1000000 },
};

static void SendTrcNotification(const char *msg)
{
	struct {
		uint32_t header;
		char text[60];
	} notif;
	memset(&notif, 0, sizeof(notif));
	sceClibStrncpy(notif.text, msg, sizeof(notif.text));
	notif.text[sizeof(notif.text) - 1] = '\0';
	sceAppMgrSendNotificationRequest(&notif, 1);
}

static bool IsValidRecMode(uint32_t recMode)
{
	uint32_t base = recMode & 0xFFFFU;
	uint32_t opt = recMode & 0xFFFF0000U;
	if (base > 5U)
		return false;
	if ((opt & SCE_MP4REC_MODE_OPTION_ITU601) != 0 &&
		(opt & SCE_MP4REC_MODE_OPTION_ITU709) != 0)
		return false;
	if ((opt & 0xFFFC0000U) != 0)
		return false;
	return true;
}

extern "C" {

uint32_t Mp4RecTranslateError(uint32_t err)
{
	uint32_t group = err & 0xFFFFFFF0U;
	if (group == 0x80621000U)
		return (err & 0xFFU) | 0x80108350U;
	switch (group) {
	case 0x80105400U:
		return (err & 0xFFU) | 0x801083E0U;
	case 0x80105410U:
		return (err & 0xFFU) | 0x801083F0U;
	case 0x80106F00U:
		return (err & 0xFFU) | 0x801083A0U;
	case 0x80106F40U:
		return (err & 0xFFU) | 0x801083B0U;
	case 0x80106F80U:
		return (err & 0xFFU) | 0x801083C0U;
	case 0x80621040U:
		return (err & 0xFFU) | 0x80108360U;
	case 0x80621800U:
		return (err & 0xFFU) | 0x80108340U;
	case 0x806B0000U:
		return (err & 0xFFU) | 0x80108380U;
	default:
		return err;
	}
}

SceInt32 sceMp4RecRecorderInit(void)
{
	return 0;
}

SceInt32 sceMp4RecRecorderEnd(void)
{
	return 0;
}

uint32_t PadMinimumDuration(SceMp4RecCtrl *pCtrl, uint32_t padMs)
{
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	uint32_t modeIdx = h->startParam.modeIndex;
	if (modeIdx >= 6U)
		return SCE_MP4REC_ERROR_INVALID_RECMODE;

	uint8_t *yuvBuf = (uint8_t *)h->avRecBuf->ptr;
	uint32_t frameBytes = s_apiModes[modeIdx].frameBytes;
	void *pcmBuf = h->allocIf->Malloc(4096U);
	if (pcmBuf == NULL)
		return SCE_MP4REC_ERROR_OUT_OF_MEMORY;

	memset(pcmBuf, 0, 4096U);
	uint32_t lumaBytes = s_apiModes[modeIdx].width * s_apiModes[modeIdx].height;
	memset(yuvBuf, 16, lumaBytes);
	memset(yuvBuf + lumaBytes, 128, lumaBytes >> 1);

	uint32_t videoTicks = 0;
	uint32_t audioTicks = 0;
	uint32_t videoMs = 0;
	uint32_t audioMs = 0;
	int32_t err = 0;

	if (padMs != 0) {
		for (;;) {
			if (audioTicks > videoTicks) {
				uint8_t dropped = 0;
				videoTicks += SCE_MP4REC_VIDEO_SAMPLE_DURATION;
				err = h->recorder->AddVideoSample(yuvBuf, frameBytes, &dropped);
				Mp4RecRescaleDuration(videoTicks, SCE_MP4REC_VIDEO_TIMESCALE, 1000U, &videoMs);
			} else {
				audioTicks += SCE_MP4REC_AUDIO_SAMPLE_DURATION;
				err = h->recorder->AddAudioSample(pcmBuf, 4096U);
				Mp4RecRescaleDuration(audioTicks, SCE_MP4REC_AUDIO_TIMESCALE, 1000U, &audioMs);
			}
			if (err < 0) {
				h->allocIf->Free(pcmBuf);
				return Mp4RecTranslateError((uint32_t)err);
			}
			uint32_t curMs = (audioTicks > videoTicks) ? videoMs : audioMs;
			if (curMs >= padMs)
				break;
		}
	}

	h->allocIf->Free(pcmBuf);
	return 0;
}

SceInt32 sceMp4RecCreateRecorder(SceMp4RecCtrl *pCtrl)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pCtrl->size != sizeof(SceMp4RecCtrl) ||
		pCtrl->pHeapBase == NULL || pCtrl->heapSize == 0) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	if (pCtrl->heapSize < SCE_MP4REC_MIN_HEAP_SIZE)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_BUFSIZE;

	SendTrcNotification("Mp4Rec:\nRequire TRC R3146 test\nsceMp4RecCreateRecorder()");

	int32_t regVal = 0;
	if (sceRegMgrSystemParamGetInt(9839568, &regVal) == 0 && regVal == 1)
		return (SceInt32)0x80010013U;

	SceLibcMspace msp = mspace_create(pCtrl->pHeapBase, pCtrl->heapSize);
	if (msp == NULL)
		return (SceInt32)SCE_MP4REC_ERROR_OUT_OF_MEMORY;

	int32_t err = 0;
	Mp4RecHandle *h = (Mp4RecHandle *)mspace_malloc(msp, sizeof(Mp4RecHandle));
	MspaceAllocatorInterface *allocIf = NULL;
	void *allocMem = NULL;
	Recorder *recorder = NULL;
	void *recMem = NULL;
	Mp4RecAvRecBufInfo *avBuf = NULL;
	void *mtxMem = NULL;

	if (h != NULL) {
		memset(h, 0, sizeof(Mp4RecHandle));
		allocMem = mspace_malloc(msp, sizeof(MspaceAllocatorInterface));
		if (allocMem != NULL) {
			allocIf = new (allocMem) MspaceAllocatorInterface(msp);

			recMem = mspace_malloc(msp, sizeof(Recorder));
			if (recMem != NULL) {
				recorder = new (recMem) Recorder(pCtrl->size);

				err = recorder->CheckFreeSpace(0);
				if (err == 0) {
					avBuf = (Mp4RecAvRecBufInfo *)mspace_malloc(msp, sizeof(Mp4RecAvRecBufInfo));
					if (avBuf != NULL) {
						avBuf->ptr = NULL;
						avBuf->size = 0;
						mtxMem = mspace_malloc(msp, sizeof(Mp4RecMutex));
						if (mtxMem != NULL) {
							h->apiMutex = new (mtxMem) Mp4RecMutex(&err, "SceMp4RecAPIMutex", 0);
							if (err >= 0) {
								h->recorder = recorder;
								h->allocIf = allocIf;
								h->mspace = msp;
								h->avRecBuf = avBuf;
								h->magic = MP4REC_HANDLE_MAGIC;
								pCtrl->handle = (SceInt32)(uintptr_t)h;
								return 0;
							}
						} else {
							err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
						}
					} else {
						err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
					}
				}
			} else {
				err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
			}

			if (recMem != NULL) {
				if (recorder != NULL)
					recorder->~Recorder();
				mspace_free(msp, recMem);
			}
		} else {
			err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		}

		if (allocMem != NULL) {
			if (allocIf != NULL)
				allocIf->~MspaceAllocatorInterface();
			mspace_free(msp, allocMem);
		}
		if (mtxMem != NULL) {
			if (h->apiMutex != NULL)
				h->apiMutex->~Mp4RecMutex();
			mspace_free(msp, mtxMem);
		}
		if (avBuf != NULL)
			mspace_free(msp, avBuf);
		mspace_free(msp, h);
	} else {
		err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
	}

	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecDeleteRecorder(SceMp4RecCtrl *pCtrl)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL || h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;

	SceLibcMspace msp = h->mspace;
	if (h->apiMutex != NULL)
		h->apiMutex->Lock();

	if (h->state == 1) {
		SceMp4RecTermParam termParam;
		memset(&termParam, 0, sizeof(termParam));
		termParam.size = sizeof(termParam);
		termParam.isCancel = SCE_TRUE;
		sceMp4RecTerm(pCtrl, &termParam);
	}

	if (msp != NULL) {
		if (h->recorder != NULL) {
			h->recorder->~Recorder();
			mspace_free(msp, h->recorder);
		}
		if (h->allocIf != NULL) {
			h->allocIf->~MspaceAllocatorInterface();
			mspace_free(msp, h->allocIf);
		}
		if (h->apiMutex != NULL) {
			h->apiMutex->Unlock();
			h->apiMutex->~Mp4RecMutex();
			mspace_free(msp, h->apiMutex);
			h->apiMutex = NULL;
		}
		mspace_free(msp, h);
		mspace_destroy(msp);
	}

	pCtrl->handle = 0;
	return 0;
}

SceInt32 sceMp4RecQueryPhysicalMemSize(SceMp4RecCtrl *pCtrl, SceUInt32 recMode,
	SceUInt32 *pEncodeWorkMemSize, SceUInt32 *pAvRecMemSize)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pEncodeWorkMemSize == NULL || pAvRecMemSize == NULL ||
		((uintptr_t)pAvRecMemSize & 3U) != 0 ||
		pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	if (!IsValidRecMode(recMode))
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_RECMODE;
	if (h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;

	int32_t lockErr = h->apiMutex->Lock();
	if (lockErr < 0)
		return (SceInt32)Mp4RecTranslateError((uint32_t)lockErr);

	uint32_t modeIdx = recMode & 0xFFFFU;
	uint32_t modeOpt = recMode & 0xFFFF0000U;
	AvrecMemReq req;
	int32_t err = h->recorder->QueryMemSize(modeIdx, modeOpt, &req);
	if (err == 0)
		*pEncodeWorkMemSize = req.phyMemSize + 0x100000U;
	*pAvRecMemSize = s_apiModes[modeIdx].frameBytes;
	h->apiMutex->Unlock();
	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecInit(SceMp4RecCtrl *pCtrl, SceMp4RecInitParam *pParam)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 || pParam == NULL ||
		pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	if (pParam->size != sizeof(SceMp4RecInitParam) ||
		pParam->pEncoderPhyContPtr == NULL || pParam->encoderPhyContSize == 0 ||
		pParam->pAvRecPhyContPtr == NULL || pParam->avRecPhyContSize == 0) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	if (!IsValidRecMode(pParam->recMode))
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_RECMODE;
	if (h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;

	SendTrcNotification("Mp4Rec:\nRequire TRC R3146 test\nsceMp4RecInit()");

	int32_t regVal = 0;
	if (sceRegMgrSystemParamGetInt(1395244, &regVal) == 0 && regVal == 1)
		return (SceInt32)SCE_MP4REC_ERROR_NOSPACE;
	if (sceRegMgrSystemParamGetInt(8196804, &regVal) == 0 && regVal == 1)
		return (SceInt32)SCE_MP4REC_ERROR_EXPORT_CONTENT_FULL;

	int32_t lockErr = h->apiMutex->Lock();
	if (lockErr < 0)
		return (SceInt32)Mp4RecTranslateError((uint32_t)lockErr);

	void *expWork = h->allocIf->Malloc(0x10000U);
	if (expWork == NULL)
		return (SceInt32)SCE_MP4REC_ERROR_OUT_OF_MEMORY;

	VideoExportInputParam expIn;
	VideoExportOutputParam expOut;
	memset(&expIn, 0, sizeof(expIn));
	int32_t expErr = sceVideoExportFromFile(&expIn, 1, expWork, NULL, NULL, NULL, 0, &expOut);
	h->allocIf->Free(expWork);
	if (expErr == (int32_t)0x80105403U) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_EXPORT_CONTENT_FULL;
	}

	SceUInt32 reqEncSize = 0;
	SceUInt32 reqAvSize = 0;
	sceMp4RecQueryPhysicalMemSize(pCtrl, pParam->recMode, &reqEncSize, &reqAvSize);
	if (reqEncSize > pParam->encoderPhyContSize || reqAvSize > pParam->avRecPhyContSize) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_BUFSIZE;
	}

	sceRtcGetCurrentTick(&h->startUtcTick);
	int32_t err = h->recorder->CheckFreeSpace(0);
	if (err != 0) {
		h->apiMutex->Unlock();
		return (SceInt32)Mp4RecTranslateError((uint32_t)err);
	}

	SceUID pid = sceKernelGetProcessId();
	err = sceAppMgrAppParamGetString(pid, 12, h->titleId, 10U);
	if (err < 0) {
		h->apiMutex->Unlock();
		return (SceInt32)Mp4RecTranslateError((uint32_t)err);
	}

	err = sceAppMgrWorkDirMountById(207, h->titleId, h->mountPoint);
	if (err < 0 && err != (int32_t)0x80800003U) {
		h->apiMutex->Unlock();
		return (SceInt32)Mp4RecTranslateError((uint32_t)err);
	}

	snprintf(h->tmpFilePath, sizeof(h->tmpFilePath), "%s/%s", h->mountPoint, "mp4rectmp.mp4");
	sceIoRemove(h->tmpFilePath);

	char *oldNameBuf = (char *)h->allocIf->Malloc(SCE_MP4REC_PATH_MAX);
	if (oldNameBuf != NULL) {
		char *oldPathBuf = (char *)h->allocIf->Malloc(SCE_MP4REC_PATH_MAX);
		if (oldPathBuf != NULL) {
			char datPath[SCE_MP4REC_PATH_MAX];
			snprintf(datPath, sizeof(datPath), "%s/%s", h->mountPoint, "mp4recexp.dat");
			SceUID fd = sceIoOpen(datPath, 1, 0);
			if (fd >= 0) {
				SceSSize nread = sceIoRead(fd, oldNameBuf, SCE_MP4REC_PATH_MAX);
				if (nread >= 0) {
					if (nread < SCE_MP4REC_PATH_MAX)
						oldNameBuf[nread] = '\0';
					sceIoClose(fd);
					snprintf(oldPathBuf, SCE_MP4REC_PATH_MAX, "%s/%s", h->mountPoint, oldNameBuf);
					sceIoRemove(oldPathBuf);
					char datPath2[SCE_MP4REC_PATH_MAX];
					snprintf(datPath2, sizeof(datPath2), "%s/%s", h->mountPoint, "mp4recexp.dat");
					sceIoRemove(datPath2);
				}
			}

			RecorderStartParam startParam;
			startParam.filePath = h->tmpFilePath;
			startParam.allocIf = h->allocIf;
			startParam.encoderPhyPtr = pParam->pEncoderPhyContPtr;
			startParam.encoderPhySize = pParam->encoderPhyContSize;
			startParam.modeIndex = pParam->recMode & 0xFFFFU;
			startParam.affinity = pParam->affinityForWriteThread;
			startParam.priority = pParam->priorityForWriteThread;
			startParam.modeOption = pParam->recMode & 0xFFFF0000U;
			memcpy(&h->startParam, &startParam, sizeof(startParam));

			err = h->recorder->Prepare();
			if (err >= 0) {
				err = h->recorder->Start(&startParam);
				if (err >= 0) {
					h->state = 1;
					h->avRecBuf->ptr = pParam->pAvRecPhyContPtr;
					h->avRecBuf->size = pParam->avRecPhyContSize;
					h->allocIf->Free(oldNameBuf);
					h->allocIf->Free(oldPathBuf);
					h->apiMutex->Unlock();
					return (SceInt32)Mp4RecTranslateError((uint32_t)err);
				}
				h->recorder->Unprepare();
				h->recorder->WaitIdle();
			}
			h->allocIf->Free(oldPathBuf);
		} else {
			if (err == 0)
				err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		}
		h->allocIf->Free(oldNameBuf);
	} else {
		if (err == 0)
			err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
	}

	sceAppMgrUmount(h->mountPoint);
	h->apiMutex->Unlock();
	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecTerm(SceMp4RecCtrl *pCtrl, SceMp4RecTermParam *pParam)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pParam == NULL || ((uintptr_t)pParam & 3U) != 0 ||
		pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL || pParam->size != sizeof(SceMp4RecTermParam) ||
		h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	if (h->state != 1)
		return (SceInt32)SCE_MP4REC_ERROR_NOT_INITIALIZED;

	int32_t err = h->apiMutex->Lock();
	if (err < 0)
		return (SceInt32)Mp4RecTranslateError((uint32_t)err);

	SendTrcNotification("Mp4Rec:\nRequire TRC R3146 test\nsceMp4RecTerm()");

	int32_t regVal = 0;
	sceRegMgrSystemParamGetInt(8196804, &regVal);

	uint32_t durMs = h->recorder->GetDurationMs();
	if (err == 0 && pParam->isCancel == SCE_FALSE) {
		if (durMs < SCE_MP4REC_DURATION_MIN)
			err = (int32_t)PadMinimumDuration(pCtrl, SCE_MP4REC_DURATION_MIN - durMs);
		if (err != 0)
			err = (int32_t)SCE_MP4REC_ERROR_NOSPACE;
	}

	char *dstMountPath = (char *)h->allocIf->Malloc(SCE_MP4REC_PATH_MAX);
	if (dstMountPath == NULL && err == 0)
		err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
	char *exportSrcPath = (char *)h->allocIf->Malloc(SCE_MP4REC_PATH_MAX);
	if (exportSrcPath == NULL && err == 0)
		err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
	VideoExportInputParam *inParam = (VideoExportInputParam *)h->allocIf->Malloc(sizeof(VideoExportInputParam));
	if (inParam == NULL && err == 0)
		err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
	VideoExportOutputParam *outParam = (VideoExportOutputParam *)h->allocIf->Malloc(sizeof(VideoExportOutputParam));
	if (outParam == NULL && err == 0)
		err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;

	h->recorder->Stop();

	RecorderTermInfo termInfo;
	memset(&termInfo, 0, sizeof(termInfo));
	termInfo.startUtcTick = h->startUtcTick;
	termInfo.titleId = h->titleId;
	if (pParam->pMetadata != NULL && pParam->pMetadata->size == sizeof(SceMp4RecMetadata))
		termInfo.metadata = pParam->pMetadata;

	h->recorder->Finalize(&termInfo, h->allocIf);
	h->recorder->Unprepare();
	h->recorder->WaitIdle();
	h->state = 2;

	if (pParam->isCancel != SCE_FALSE || err != 0) {
		sceIoRemove(h->tmpFilePath);
	} else {
		void *expWork = h->allocIf->Malloc(0x10000U);
		if (expWork == NULL) {
			err = (int32_t)SCE_MP4REC_ERROR_OUT_OF_MEMORY;
		} else {
			SceRtcTick localTick;
			SceDateTime dt;
			if (sceRtcConvertUtcToLocalTime(&h->startUtcTick, &localTick) < 0 ||
				sceRtcSetTick(&dt, &localTick) < 0) {
				err = (int32_t)SCE_MP4REC_ERROR_INTERNAL;
			} else {
				char fmtPath[SCE_MP4REC_PATH_MAX];
				snprintf(fmtPath, sizeof(fmtPath), "%s/%04d-%02d-%02d_%02d%02d%02d%s",
					h->mountPoint, dt.year, dt.month, dt.day,
					dt.hour, dt.minute, dt.second, ".mp4");
				size_t len = strlen(fmtPath);
				if (len + 1U <= SCE_MP4REC_PATH_MAX) {
					memcpy(dstMountPath, fmtPath, len + 1U);
					err = 0;
				} else {
					err = (int32_t)SCE_MP4REC_ERROR_INVALID_BUFSIZE;
				}
			}

			if (err >= 0) {
				char *slash = strrchr(dstMountPath, '/');
				if (slash == NULL) {
					err = (int32_t)SCE_MP4REC_ERROR_INTERNAL;
				} else {
					const char *fileName = slash + 1;
					snprintf(exportSrcPath, SCE_MP4REC_PATH_MAX, "%s%s%s%s",
						"ux0:temp/app_work/", h->titleId, "/rec/", fileName);
					memset(inParam, 0, sizeof(VideoExportInputParam));
					memset(outParam, 0, sizeof(VideoExportOutputParam));
					memcpy(inParam->path, exportSrcPath, strlen(exportSrcPath) + 1U);

					char datPath[SCE_MP4REC_PATH_MAX];
					snprintf(datPath, sizeof(datPath), "%s/%s", h->mountPoint, "mp4recexp.dat");
					SceUID fd = sceIoOpen(datPath, 0x0603, 0x180);
					if (fd >= 0) {
						SceSSize wr = sceIoWrite(fd, fileName, strlen(fileName));
						if (wr >= 0) {
							sceIoClose(fd);
							err = 0;
						} else {
							err = (int32_t)wr;
						}
					} else {
						err = (int32_t)SCE_MP4REC_ERROR_INTERNAL;
					}

					if (err >= 0) {
						err = sceIoRename(h->tmpFilePath, dstMountPath);
						if (err >= 0) {
							err = sceAppMgrUmount(h->mountPoint);
							if (err >= 0) {
								err = sceVideoExportFromFile(inParam, 1, expWork,
									NULL, NULL, NULL, 0, outParam);
								sceAppMgrWorkDirMountById(207, h->titleId, h->mountPoint);
								if (err < 0) {
									sceIoRemove(dstMountPath);
								} else {
									char datPath2[SCE_MP4REC_PATH_MAX];
									snprintf(datPath2, sizeof(datPath2), "%s/%s",
										h->mountPoint, "mp4recexp.dat");
									sceIoRemove(datPath2);
								}
							}
						}
					}
				}
			}

			h->allocIf->Free(expWork);
		}
	}

	if (dstMountPath != NULL)
		h->allocIf->Free(dstMountPath);
	if (exportSrcPath != NULL)
		h->allocIf->Free(exportSrcPath);
	if (inParam != NULL)
		h->allocIf->Free(inParam);
	if (outParam != NULL)
		h->allocIf->Free(outParam);

	sceAppMgrUmount(h->mountPoint);
	h->apiMutex->Unlock();
	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecAddVideoSample(SceMp4RecCtrl *pCtrl, void *pBuffer, SceInt32 bufsize)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pBuffer == NULL || pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL || h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;

	SendTrcNotification("Mp4Rec:\nRequire TRC R3146 test\nsceMp4RecAddVideoSample()");

	int32_t regVal = 0;
	if (sceRegMgrSystemParamGetInt(1395244, &regVal) == 0 && regVal == 1)
		return (SceInt32)SCE_MP4REC_ERROR_NOSPACE;

	int32_t lockErr = h->apiMutex->Lock();
	if (lockErr < 0)
		return (SceInt32)Mp4RecTranslateError((uint32_t)lockErr);

	if (h->state != 1) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_NOT_INITIALIZED;
	}

	uint32_t modeIdx = h->startParam.modeIndex;
	if (modeIdx >= 6U) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_RECMODE;
	}

	uint32_t expectedBytes = s_apiModes[modeIdx].frameBytes;
	if ((uint32_t)bufsize != expectedBytes) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_BUFSIZE;
	}

	if (h->recorder->GetDurationMs() > SCE_MP4REC_DURATION_MAX) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_DURATION_LIMIT;
	}

	uint8_t dropped = 0;
	int32_t err = h->recorder->AddVideoSample(pBuffer, expectedBytes, &dropped);
	h->apiMutex->Unlock();
	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecAddAudioSample(SceMp4RecCtrl *pCtrl, void *pBuffer, SceInt32 bufsize)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0 ||
		pBuffer == NULL || pCtrl->size != sizeof(SceMp4RecCtrl)) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}
	Mp4RecHandle *h = (Mp4RecHandle *)(uintptr_t)pCtrl->handle;
	if (h == NULL || h->avRecBuf == NULL || h->magic != MP4REC_HANDLE_MAGIC)
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;

	SendTrcNotification("Mp4Rec:\nRequire TRC R3146 test\nsceMp4RecAddAudioSample()");

	int32_t regVal = 0;
	if (sceRegMgrSystemParamGetInt(1395244, &regVal) == 0 && regVal == 1)
		return (SceInt32)SCE_MP4REC_ERROR_NOSPACE;

	int32_t lockErr = h->apiMutex->Lock();
	if (lockErr < 0)
		return (SceInt32)Mp4RecTranslateError((uint32_t)lockErr);

	if (h->state != 1) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_NOT_INITIALIZED;
	}

	if (bufsize != 4096) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_BUFSIZE;
	}

	if (h->recorder->GetDurationMs() > SCE_MP4REC_DURATION_MAX) {
		h->apiMutex->Unlock();
		return (SceInt32)SCE_MP4REC_ERROR_DURATION_LIMIT;
	}

	int32_t err = h->recorder->AddAudioSample(pBuffer, 4096U);
	h->apiMutex->Unlock();
	return (SceInt32)Mp4RecTranslateError((uint32_t)err);
}

SceInt32 sceMp4RecCsc(SceMp4RecPicture *pDst, const SceMp4RecPicture *pSrc)
{
	if (pDst == NULL || ((uintptr_t)pDst & 3U) != 0 ||
		pSrc == NULL || ((uintptr_t)pSrc & 3U) != 0) {
		return (SceInt32)SCE_MP4REC_ERROR_INVALID_ARG;
	}

	SceAvcencPicture avcDst;
	SceAvcencPicture avcSrc;
	memset(&avcSrc, 0, sizeof(avcSrc));
	memset(&avcDst, 0, sizeof(avcDst));
	avcSrc.size = sizeof(avcSrc);
	avcDst.size = sizeof(avcDst);
	memcpy(&avcSrc.pixelType, &pSrc->frame, sizeof(SceMp4RecFrame));
	memcpy(&avcDst.pixelType, &pDst->frame, sizeof(SceMp4RecFrame));

	uint32_t err = (uint32_t)sceAvcencCsc(&avcDst, &avcSrc);
	return (SceInt32)Mp4RecTranslateError(err);
}

} /* extern "C" */
