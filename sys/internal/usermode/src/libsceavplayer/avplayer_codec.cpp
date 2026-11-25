#include "avplayer_internal.h"

/* Base AvDecoder out-of-line virtual definitions (single vtable TU) */
AvDecoder::AvDecoder()
{
	streamType = SCE_AVPLAYER_UNKNOWN;
	decoderState = 0;
	codecFlags = 0;
	sceClibMemset(&descriptor, 0, sizeof(descriptor));
}

AvDecoder::~AvDecoder()
{
}

int32_t AvDecoder::init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t dbgLevel)
{
	(void)desc;
	(void)alloc;
	(void)dbgLevel;
	return -1;
}

int32_t AvDecoder::decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput)
{
	(void)au;
	(void)outFrame;
	(void)gotOutput;
	return -1;
}

int32_t AvDecoder::reset()
{
	return -1;
}

int32_t AvDecoder::shutdown()
{
	return 0;
}

int32_t AvDecoder::stop()
{
	return -1;
}

int32_t AvDecoder::flush(AvDecodedFrame *outFrame)
{
	(void)outFrame;
	return -1;
}

int32_t AvDecoder::getInfo(void *outInfo)
{
	(void)outInfo;
	return -1;
}

/* AvAacDecoder implementation */
AvAacDecoder::AvAacDecoder()
{
	streamType = SCE_AVPLAYER_AUDIO;
	sceClibMemset(&memAlloc, 0, sizeof(memAlloc));
	debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
	sceClibMemset(&audioCtrl, 0, sizeof(audioCtrl));
	sceClibMemset(&aacInfo, 0, sizeof(aacInfo));
	contextMem = NULL;
	unmapUid = -1;
	vaContext = 0;
	contextSize = 0;
	isInitialized = 0;
}

AvAacDecoder::~AvAacDecoder()
{
	shutdown();
}

int32_t AvAacDecoder::init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t dbgLevel)
{
	if (desc == NULL || alloc == NULL)
		return -1;

	descriptor = *desc;
	memAlloc = *alloc;
	debugLevel = dbgLevel;

	sceClibMemset(&audioCtrl, 0, sizeof(audioCtrl));
	sceClibMemset(&aacInfo, 0, sizeof(aacInfo));
	audioCtrl.size = sizeof(SceAudiodecCtrl);
	audioCtrl.pInfo = (SceAudiodecInfo *)&aacInfo;
	audioCtrl.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;

	aacInfo.size = sizeof(SceAudiodecInfoAac);
	aacInfo.isAdts = 0;
	aacInfo.ch = (desc->channelCount > 0) ? (SceUInt32)desc->channelCount : 2u;
	aacInfo.samplingRate = (desc->sampleRate > 0) ? desc->sampleRate : 48000u;
	aacInfo.isSbr = 0;

	int32_t ret = sceAudiodecGetContextSize(&audioCtrl, SCE_AUDIODEC_TYPE_AAC);
	if (ret < 0) {
		avLogError(debugLevel, "AvAacDecoder::Init", 112, "sceAudiodecGetContextSize failed: 0x%08x", ret);
		return -1;
	}

	contextSize = SCE_AUDIODEC_ROUND_UP((uint32_t)ret);
	if (memAlloc.allocate != NULL)
		contextMem = memAlloc.allocate(memAlloc.objectPointer, 256, contextSize);
	if (contextMem == NULL) {
		avLogError(debugLevel, "AvAacDecoder::Init", 120, "Cannot allocate AAC context memory");
		return -1;
	}

	unmapUid = sceCodecEngineOpenUnmapMemBlock(contextMem, contextSize);
	if (unmapUid < 0) {
		avLogError(debugLevel, "AvAacDecoder::Init", 128, "sceCodecEngineOpenUnmapMemBlock failed: 0x%08x", unmapUid);
		memAlloc.deallocate(memAlloc.objectPointer, contextMem);
		contextMem = NULL;
		return -1;
	}

	vaContext = sceCodecEngineAllocMemoryFromUnmapMemBlock(unmapUid, contextSize, 256);
	if (vaContext == 0) {
		sceCodecEngineCloseUnmapMemBlock(unmapUid);
		unmapUid = -1;
		memAlloc.deallocate(memAlloc.objectPointer, contextMem);
		contextMem = NULL;
		return -1;
	}

	ret = sceAudiodecCreateDecoderExternal(&audioCtrl, SCE_AUDIODEC_TYPE_AAC, vaContext, contextSize);
	if (ret < 0) {
		avLogError(debugLevel, "AvAacDecoder::Init", 142, "sceAudiodecCreateDecoderExternal failed: 0x%08x", ret);
		sceCodecEngineFreeMemoryFromUnmapMemBlock(unmapUid, vaContext);
		sceCodecEngineCloseUnmapMemBlock(unmapUid);
		unmapUid = -1;
		vaContext = 0;
		memAlloc.deallocate(memAlloc.objectPointer, contextMem);
		contextMem = NULL;
		return -1;
	}

	isInitialized = 1;
	return 0;
}

int32_t AvAacDecoder::decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput)
{
	if (gotOutput != NULL)
		*gotOutput = 0;
	if (!isInitialized || au == NULL || outFrame == NULL)
		return -1;

	audioCtrl.pEs = au->buffer;
	audioCtrl.inputEsSize = au->size;
	audioCtrl.maxEsSize = SCE_AUDIODEC_AAC_MAX_ES_SIZE;
	audioCtrl.pPcm = outFrame->buffer;
	audioCtrl.outputPcmSize = 0;
	audioCtrl.maxPcmSize = 0x8000u;

	int32_t ret = sceAudiodecDecode(&audioCtrl);
	if (ret < 0) {
		avLogWarn(debugLevel, "AvAacDecoder::Decode", 185, "sceAudiodecDecode error: 0x%08x", ret);
		return ret;
	}

	outFrame->pts = au->pts;
	outFrame->heightOrSize = audioCtrl.outputPcmSize;
	outFrame->cropLeftOrReadOffset = 0;
	if (gotOutput != NULL && audioCtrl.outputPcmSize > 0)
		*gotOutput = 1;
	return 0;
}

int32_t AvAacDecoder::reset()
{
	return 0;
}

int32_t AvAacDecoder::stop()
{
	return 0;
}

int32_t AvAacDecoder::shutdown()
{
	if (isInitialized) {
		SceUIntVAddr releasedVa = vaContext;
		sceAudiodecDeleteDecoderExternal(&audioCtrl, &releasedVa);
		isInitialized = 0;
	}
	if (unmapUid >= 0) {
		if (vaContext != 0) {
			sceCodecEngineFreeMemoryFromUnmapMemBlock(unmapUid, vaContext);
			vaContext = 0;
		}
		sceCodecEngineCloseUnmapMemBlock(unmapUid);
		unmapUid = -1;
	}
	if (contextMem != NULL && memAlloc.deallocate != NULL) {
		memAlloc.deallocate(memAlloc.objectPointer, contextMem);
		contextMem = NULL;
	}
	return 0;
}

/* AvAvcDecoder implementation */
AvAvcDecoder::AvAvcDecoder()
{
	streamType = SCE_AVPLAYER_VIDEO;
	sceClibMemset(&memAlloc, 0, sizeof(memAlloc));
	debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
	sceClibMemset(&videodecCtrl, 0, sizeof(videodecCtrl));
	sceClibMemset(&avcdecCtrl, 0, sizeof(avcdecCtrl));
	libMem = NULL;
	frameMem = NULL;
	unmapUid = -1;
	vaContext = 0;
	contextSize = 0;
	frameWidth = 0;
	frameHeight = 0;
	alignedWidth = 0;
	alignedHeight = 0;
	isInitialized = 0;
	libInitialized = 0;
}

AvAvcDecoder::~AvAvcDecoder()
{
	shutdown();
}

int32_t AvAvcDecoder::init(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t dbgLevel)
{
	if (desc == NULL || alloc == NULL)
		return -1;

	descriptor = *desc;
	memAlloc = *alloc;
	debugLevel = dbgLevel;

	frameWidth = desc->width;
	frameHeight = desc->height;
	alignedWidth = (frameWidth + 15u) & ~15u;
	alignedHeight = (frameHeight + 15u) & ~15u;

	SceVideodecQueryInitInfo initInfo;
	sceClibMemset(&initInfo, 0, sizeof(initInfo));
	initInfo.hwAvc.size = sizeof(SceVideodecQueryInitInfoHwAvcdec);
	initInfo.hwAvc.horizontal = alignedWidth;
	initInfo.hwAvc.vertical = alignedHeight;
	initInfo.hwAvc.numOfRefFrames = 4;
	initInfo.hwAvc.numOfStreams = 1;

	SceVideodecMemInfo memInfo;
	sceClibMemset(&memInfo, 0, sizeof(memInfo));
	int32_t ret = sceVideodecQueryMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &initInfo, &memInfo);
	if (ret < 0) {
		avLogError(debugLevel, "AvAvcDecoder::Init", 240, "sceVideodecQueryMemSize failed: 0x%08x", ret);
		return -1;
	}

	contextSize = (memInfo.memSize + 0xFFFu) & ~0xFFFu;
	if (memAlloc.allocate != NULL)
		libMem = memAlloc.allocate(memAlloc.objectPointer, 4096, contextSize);
	if (libMem == NULL) {
		avLogError(debugLevel, "AvAvcDecoder::Init", 248, "Cannot allocate videodec memory");
		return -1;
	}

	unmapUid = sceCodecEngineOpenUnmapMemBlock(libMem, contextSize);
	if (unmapUid < 0) {
		memAlloc.deallocate(memAlloc.objectPointer, libMem);
		libMem = NULL;
		return -1;
	}

	vaContext = sceCodecEngineAllocMemoryFromUnmapMemBlock(unmapUid, contextSize, 4096);
	if (vaContext == 0) {
		sceCodecEngineCloseUnmapMemBlock(unmapUid);
		unmapUid = -1;
		memAlloc.deallocate(memAlloc.objectPointer, libMem);
		libMem = NULL;
		return -1;
	}

	sceClibMemset(&videodecCtrl, 0, sizeof(videodecCtrl));
	videodecCtrl.memBuf.pBuf = libMem;
	videodecCtrl.memBuf.size = contextSize;
	videodecCtrl.memBufUid = unmapUid;
	videodecCtrl.vaContext = vaContext;
	videodecCtrl.contextSize = contextSize;

	ret = sceVideodecInitLibraryWithUnmapMem(SCE_VIDEODEC_TYPE_HW_AVCDEC, &videodecCtrl, &initInfo);
	if (ret < 0) {
		avLogError(debugLevel, "AvAvcDecoder::Init", 270, "sceVideodecInitLibraryWithUnmapMem failed: 0x%08x", ret);
		shutdown();
		return -1;
	}
	libInitialized = 1;

	SceAvcdecQueryDecoderInfo decQuery;
	decQuery.horizontal = alignedWidth;
	decQuery.vertical = alignedHeight;
	decQuery.numOfRefFrames = 4;

	SceAvcdecDecoderInfo decInfo;
	sceClibMemset(&decInfo, 0, sizeof(decInfo));
	ret = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &decQuery, &decInfo);
	if (ret < 0) {
		avLogError(debugLevel, "AvAvcDecoder::Init", 282, "sceAvcdecQueryDecoderMemSize failed: 0x%08x", ret);
		shutdown();
		return -1;
	}

	if (memAlloc.allocate != NULL)
		frameMem = memAlloc.allocate(memAlloc.objectPointer, 256, decInfo.frameMemSize);
	if (frameMem == NULL) {
		shutdown();
		return -1;
	}

	sceClibMemset(&avcdecCtrl, 0, sizeof(avcdecCtrl));
	avcdecCtrl.frameBuf.pBuf = frameMem;
	avcdecCtrl.frameBuf.size = decInfo.frameMemSize;

	ret = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, &avcdecCtrl, &decQuery);
	if (ret < 0) {
		avLogError(debugLevel, "AvAvcDecoder::Init", 298, "sceAvcdecCreateDecoder failed: 0x%08x", ret);
		shutdown();
		return -1;
	}

	isInitialized = 1;
	return 0;
}

int32_t AvAvcDecoder::decode(AvAccessUnit *au, AvDecodedFrame *outFrame, int32_t *gotOutput)
{
	if (gotOutput != NULL)
		*gotOutput = 0;
	if (!isInitialized || au == NULL || outFrame == NULL)
		return -1;

	int32_t avail = sceAvcdecDecodeAvailableSize(&avcdecCtrl);
	if (avail < 0 || (uint32_t)avail < au->size)
		return 0;

	SceAvcdecAu avcAu;
	sceClibMemset(&avcAu, 0, sizeof(avcAu));
	avcAu.pts.upper = (uint32_t)(au->pts >> 32);
	avcAu.pts.lower = (uint32_t)(au->pts & 0xFFFFFFFFu);
	avcAu.dts.upper = (uint32_t)(au->dts >> 32);
	avcAu.dts.lower = (uint32_t)(au->dts & 0xFFFFFFFFu);
	avcAu.es.pBuf = au->buffer;
	avcAu.es.size = au->size;

	SceAvcdecPicture pic;
	sceClibMemset(&pic, 0, sizeof(pic));
	pic.size = sizeof(SceAvcdecPicture);
	pic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_RASTER;
	pic.frame.framePitch = alignedWidth;
	pic.frame.frameWidth = alignedWidth;
	pic.frame.frameHeight = alignedHeight;
	pic.frame.pPicture[0] = outFrame->buffer;

	SceAvcdecPicture *picList[1];
	picList[0] = &pic;
	SceAvcdecArrayPicture arrayPic;
	sceClibMemset(&arrayPic, 0, sizeof(arrayPic));
	arrayPic.numOfElm = 1;
	arrayPic.pPicture = picList;

	int32_t ret = sceAvcdecDecode(&avcdecCtrl, &avcAu, &arrayPic);
	if (ret < 0) {
		avLogWarn(debugLevel, "AvAvcDecoder::Decode", 345, "sceAvcdecDecode error: 0x%08x", ret);
		return ret;
	}

	if (arrayPic.numOfOutput > 0) {
		uint64_t outPts = ((uint64_t)pic.info.outputPts.upper << 32) | (uint64_t)pic.info.outputPts.lower;
		outFrame->pts = (outPts != 0xFFFFFFFFFFFFFFFFULL) ? outPts : au->pts;
		outFrame->width = pic.frame.frameWidth;
		outFrame->heightOrSize = pic.frame.frameHeight;
		outFrame->cropLeftOrReadOffset = pic.frame.frameCropLeftOffset;
		outFrame->cropRightOffset = pic.frame.frameCropRightOffset;
		outFrame->cropTopOffset = pic.frame.frameCropTopOffset;
		outFrame->cropBottomOffset = pic.frame.frameCropBottomOffset;
		if (gotOutput != NULL)
			*gotOutput = 1;
	}
	return 0;
}

int32_t AvAvcDecoder::reset()
{
	if (!isInitialized)
		return -1;
	return sceAvcdecDecodeFlush(&avcdecCtrl);
}

int32_t AvAvcDecoder::stop()
{
	if (!isInitialized)
		return -1;
	SceAvcdecArrayPicture arrayPic;
	sceClibMemset(&arrayPic, 0, sizeof(arrayPic));
	return sceAvcdecDecodeStop(&avcdecCtrl, &arrayPic);
}

int32_t AvAvcDecoder::flush(AvDecodedFrame *outFrame)
{
	if (!isInitialized || outFrame == NULL)
		return -1;
	SceAvcdecPicture pic;
	sceClibMemset(&pic, 0, sizeof(pic));
	pic.size = sizeof(SceAvcdecPicture);
	pic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_RASTER;
	pic.frame.framePitch = alignedWidth;
	pic.frame.frameWidth = alignedWidth;
	pic.frame.frameHeight = alignedHeight;
	pic.frame.pPicture[0] = outFrame->buffer;

	SceAvcdecPicture *picList[1];
	picList[0] = &pic;
	SceAvcdecArrayPicture arrayPic;
	sceClibMemset(&arrayPic, 0, sizeof(arrayPic));
	arrayPic.numOfElm = 1;
	arrayPic.pPicture = picList;

	return sceAvcdecDecodeStop(&avcdecCtrl, &arrayPic);
}

int32_t AvAvcDecoder::shutdown()
{
	if (isInitialized) {
		sceAvcdecDeleteDecoder(&avcdecCtrl);
		isInitialized = 0;
	}
	if (frameMem != NULL && memAlloc.deallocate != NULL) {
		memAlloc.deallocate(memAlloc.objectPointer, frameMem);
		frameMem = NULL;
	}
	if (libInitialized) {
		sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
		libInitialized = 0;
	}
	if (unmapUid >= 0) {
		if (vaContext != 0) {
			sceCodecEngineFreeMemoryFromUnmapMemBlock(unmapUid, vaContext);
			vaContext = 0;
		}
		sceCodecEngineCloseUnmapMemBlock(unmapUid);
		unmapUid = -1;
	}
	if (libMem != NULL && memAlloc.deallocate != NULL) {
		memAlloc.deallocate(memAlloc.objectPointer, libMem);
		libMem = NULL;
	}
	return 0;
}
