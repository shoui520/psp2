/*
 * SceLibMp4Recorder AVC video encoder wrapper over SceVideoencUser / SceAvcencUser.
 */
#include "mp4rec_internal.h"

static uint32_t WriteBe32(uint32_t v)
{
	return ((v & 0xFFU) << 24) | ((v & 0xFF00U) << 8) |
		((v & 0xFF0000U) >> 8) | ((v >> 24) & 0xFFU);
}

int32_t AvcEncoder::BuildParams(const AvrecConfig *cfg, SceVideoencInitParam *videoInit,
	SceAvcencQueryParam *queryParam, uint32_t *outLibMemSize, SceAvcencMemSizeInfo *outEncMem)
{
	videoInit->size = 20;
	videoInit->width = cfg->width;
	videoInit->height = cfg->height;
	videoInit->maxFrameRateNum = 1;
	videoInit->maxFrameRateDen = 1;
	videoInit->reserved[0] = 0;
	videoInit->reserved[1] = 0;
	videoInit->reserved[2] = 0;

	int32_t ret = sceVideoencQueryMemSize(0x1002U, 0, videoInit, outLibMemSize);
	if (ret < 0)
		return ret;

	memset(queryParam, 0, sizeof(*queryParam));
	queryParam->framePitch = cfg->pitch;
	queryParam->frameWidth = cfg->width;
	queryParam->frameHeight = cfg->height;
	queryParam->displayWidth = cfg->width;

	if (cfg->height == 368U)
		queryParam->displayHeight = 368U;
	if (cfg->height == 272U)
		queryParam->displayHeight = 272U;
	if (cfg->height == 208U)
		queryParam->displayHeight = 208U;

	queryParam->profile = 1;
	queryParam->targetBitrate = cfg->targetBitrate;
	queryParam->maxBitrate = cfg->maxBitrate;
	queryParam->flags = 0x89U;

	if ((cfg->modeOption & SCE_MP4REC_MODE_OPTION_ITU601) != 0)
		queryParam->colorMatrix = 1;
	else if ((cfg->modeOption & SCE_MP4REC_MODE_OPTION_ITU709) != 0)
		queryParam->colorMatrix = 2;
	else if (cfg->width > 720U || cfg->height > 576U)
		queryParam->colorMatrix = 2;
	else
		queryParam->colorMatrix = 1;

	queryParam->level = (cfg->width <= 480U) ? 2 : 4;

	uint32_t rateNum;
	if (cfg->frameRateCode > 1U)
		rateNum = (cfg->frameRateCode <= 2U) ? 4800U : 4004U;
	else
		rateNum = (cfg->frameRateCode != 0) ? 8008U : 9600U;

	queryParam->frameRateNum = rateNum;
	queryParam->frameRateDen = 240000U;
	queryParam->rcMode = 1;
	queryParam->reserved0 = 0;
	queryParam->reserved1 = 0;

	return sceAvcencQueryEncoderMemSize(0x1002U, queryParam, outEncMem);
}

int32_t AvcEncoder::QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req)
{
	uint32_t libMemSize = 0;
	SceAvcencMemSizeInfo encMem;
	SceAvcencQueryParam queryParam;
	SceVideoencInitParam videoInit;

	int32_t ret = BuildParams(cfg, &videoInit, &queryParam, &libMemSize, &encMem);
	req->phyMemSize += ((libMemSize + 0x3FFFFU) & ~0x3FFFFU) +
		((encMem.workMemSize + 0x3FFFFU) & ~0x3FFFFU) + encMem.codecMemSize + 0x100000U;
	req->heapSize += encMem.maxAuSize + 1024U;
	return ret;
}

AvcEncoder::AvcEncoder()
{
	m_initFlags = 0;
	m_width = 0;
	m_pitch = 0;
	m_height = 0;
	m_unmapUid = 0;
	m_libMemAddr = 0;
	m_codecMemAddr = 0;
	m_workMemAddr = 0;
	m_contextMem = NULL;
	m_framesSinceIdr = 28;
}

AvcEncoder::~AvcEncoder()
{
}

int32_t AvcEncoder::Term(MemAllocator *allocator)
{
	if ((m_initFlags & 4U) != 0)
		sceAvcencDeleteEncoder(&m_encHandle);
	if ((m_initFlags & 2U) != 0)
		sceVideoencTermLibrary(0x1002U);

	if (m_contextMem != NULL) {
		allocator->Free(m_contextMem);
		m_contextMem = NULL;
	}
	if (m_workMemAddr != 0) {
		sceCodecEngineFreeMemoryFromUnmapMemBlock(m_unmapUid, m_workMemAddr);
		m_workMemAddr = 0;
	}
	if (m_codecMemAddr != 0) {
		sceCodecEngineFreeMemoryFromUnmapMemBlock(m_unmapUid, m_codecMemAddr);
		m_codecMemAddr = 0;
	}
	if (m_libMemAddr != 0) {
		sceCodecEngineFreeMemoryFromUnmapMemBlock(m_unmapUid, m_libMemAddr);
		m_libMemAddr = 0;
	}
	if (m_unmapUid != 0) {
		sceCodecEngineCloseUnmapMemBlock(m_unmapUid);
		m_unmapUid = 0;
	}
	m_initFlags = 0;
	return 0;
}

int32_t AvcEncoder::Init(const AvrecConfig *cfg, MemAllocator *allocator, uint32_t *outMaxAuSize)
{
	uint32_t libMemSize = 0;
	SceAvcencMemSizeInfo encMem;
	SceAvcencQueryParam queryParam;
	SceVideoencInitParam videoInit;

	m_width = cfg->width;
	m_pitch = cfg->pitch;
	m_height = cfg->height;
	m_flag96 = (uint8_t)cfg->reserved;
	m_framesSinceIdr = 28;

	int32_t ret = BuildParams(cfg, &videoInit, &queryParam, &libMemSize, &encMem);
	if (ret < 0) {
		Term(allocator);
		return ret;
	}

	*outMaxAuSize = encMem.maxAuSize;
	SceUID uid = sceCodecEngineOpenUnmapMemBlock(allocator->m_phyMemBase, allocator->m_phyMemSize);
	if (uid < 0) {
		Term(allocator);
		return uid;
	}
	m_unmapUid = uid;

	m_libMemAddr = sceCodecEngineAllocMemoryFromUnmapMemBlock(m_unmapUid, libMemSize, 0x40000U);
	if (m_libMemAddr == 0) {
		Term(allocator);
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	}
	m_codecMemAddr = sceCodecEngineAllocMemoryFromUnmapMemBlock(m_unmapUid, encMem.codecMemSize, 0x40000U);
	if (m_codecMemAddr == 0) {
		Term(allocator);
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	}
	m_workMemAddr = sceCodecEngineAllocMemoryFromUnmapMemBlock(m_unmapUid, encMem.workMemSize, 0x40000U);
	if (m_workMemAddr == 0) {
		Term(allocator);
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	}

	m_contextMem = allocator->Malloc(1024U);
	if (m_contextMem == NULL) {
		Term(allocator);
		return -1;
	}

	SceVideoencUnmapMem unmapMem;
	memset(&unmapMem, 0, sizeof(unmapMem));
	unmapMem.addr = m_libMemAddr;
	unmapMem.size = libMemSize;

	ret = sceVideoencInitLibraryWithUnmapMem(0x1002U, &unmapMem, 0, &videoInit);
	if (ret < 0) {
		Term(allocator);
		return ret;
	}
	m_initFlags |= 2U;

	memset(&m_encHandle, 0, sizeof(m_encHandle));
	m_encHandle.codecMemAddr = m_codecMemAddr;
	m_encHandle.codecMemSize = encMem.codecMemSize;
	m_encHandle.workMemAddr = m_workMemAddr;
	m_encHandle.workMemSize = encMem.workMemSize;
	m_encHandle.contextMem = m_contextMem;
	m_encHandle.contextMemSize = 1024U;

	ret = sceAvcencCreateEncoder(0x1002U, &m_encHandle, &queryParam);
	if (ret < 0) {
		Term(allocator);
		return ret;
	}

	SceAvcencEncodeParam encParam;
	memset(&encParam, 0, sizeof(encParam));
	encParam.idrInterval = 28;
	ret = sceAvcencSetEncoderParameter(&m_encHandle, &encParam);
	if (ret < 0) {
		Term(allocator);
		return ret;
	}

	m_encodedCount = 0;
	m_initFlags |= 4U;
	return ret;
}

int32_t AvcEncoder::Encode(const void *yuv, uint32_t yuvSize, void *outBuf, uint32_t *inOutSize,
	AvrecVideoFrameMeta *outMeta, int32_t *inOutMode)
{
	(void)yuvSize;
	uint32_t maxSize = *inOutSize;
	SceAvcencAuInfo auInfo;
	memset(&auInfo, 0, sizeof(auInfo));
	auInfo.size = sizeof(auInfo);
	auInfo.esAddr = (uint32_t)(uintptr_t)outBuf;
	auInfo.esSize = maxSize;

	SceAvcencAuInfo *auPtr = &auInfo;
	SceAvcencOutput output;
	output.numOutput = 0;
	output.maxOutput = 1;
	output.auList = &auPtr;

	int32_t warnErr = 0;
	int32_t ret;
	if (*inOutMode != 0 && *inOutMode != 1) {
		ret = sceAvcencEncodeStop(&m_encHandle, &output);
		if (ret < 0)
			return ret;
	} else {
		SceAvcencPicture pic;
		memset(&pic, 0, sizeof(pic));
		pic.size = sizeof(pic);
		pic.pixelType = SCE_MP4REC_PIXEL_YUV420_PACKED_RASTER;
		pic.framePitch = m_pitch;
		pic.frameWidth = m_width;
		pic.frameHeight = m_height;

		if (*inOutMode == 1 && (m_framesSinceIdr + 1U) < 28U) {
			pic.pPicture[0] = NULL;
			pic.flags = 128U;
			*inOutMode = 1;
		} else {
			pic.pPicture[0] = (void *)yuv;
			pic.flags = 1U;
			*inOutMode = 0;
		}

		ret = sceAvcencEncode(&m_encHandle, &pic, &output);
		m_encodedCount++;
		if (ret < 0) {
			if (ret != (int32_t)0x8062100A)
				return ret;
			warnErr = MP4REC_AVREC_ERR_VENC_BUFFER_FULL;
		}
	}

	if (output.numOutput == 0) {
		outMeta->frameType = 0;
		*inOutSize = 0;
		return (ret == 0 && warnErr != 0) ? warnErr : ret;
	}

	*inOutSize = auInfo.esSize;
	outMeta->frameType = (auInfo.frameType == 4) ? 1 : 2;
	outMeta->dts = (uint32_t)((240000ULL * auInfo.dtsSubNum) / auInfo.dtsSubDen) + 240000U * auInfo.dtsSec;
	outMeta->pts = (uint32_t)((240000ULL * auInfo.ptsSubNum) / auInfo.ptsSubDen) + 240000U * auInfo.ptsSec;

	uint32_t baseAddr = (uint32_t)(uintptr_t)outBuf;
	uint32_t endAddr = baseAddr + maxSize;
	int32_t sawIdr = 0;
	for (int32_t i = 0; i < (int32_t)auInfo.nalCount; i++) {
		SceAvcencNalUnit nal;
		ret = sceAvcencGetNalUnit(&m_encHandle, i, &nal);
		if (ret < 0)
			return ret;
		if (nal.nalType == 5)
			sawIdr = 1;
		if (nal.addr < baseAddr || (nal.addr + nal.size) > endAddr)
			return MP4REC_AVREC_ERR_VENC_INVALID_POINTER;
		*(uint32_t *)(uintptr_t)(nal.addr - 4U) = WriteBe32(nal.size);
	}

	if (sawIdr != 0)
		m_framesSinceIdr = 0;
	else
		m_framesSinceIdr++;

	return (ret == 0 && warnErr != 0) ? warnErr : ret;
}

int32_t AvcEncoder::GetParameterSets(void **outSps, uint32_t *outSpsSize,
	void **outPps, uint32_t *outPpsSize)
{
	*outSps = m_encHandle.spsData;
	*outSpsSize = m_encHandle.spsSize;
	*outPps = m_encHandle.ppsData;
	*outPpsSize = m_encHandle.ppsSize;
	return 0;
}

int32_t AvcEncoder::GetMaxGopFrames()
{
	return 28;
}

uint32_t AvcEncoder::GetMaxFrameBytes()
{
	return (3U * m_pitch * m_height) >> 3;
}
