/*
 * SceLibMp4Recorder MP4 muxer, buffered I/O callbacks, and metadata helpers.
 */
#include "mp4rec_internal.h"
#include "ff4_api.h"
extern "C" {
#include "../ces/libces/src/UCS/ces_ucs_context.h"
}

static uint32_t s_muxFlushCount = 0;

static const uint32_t s_rotMatrix90[9] = {
	0x00000000U, 0x00010000U, 0x00000000U,
	0xFFFF0000U, 0x00000000U, 0x00000000U,
	0x00000000U, 0x00000000U, 0x40000000U,
};

static const uint32_t s_rotMatrix180[9] = {
	0xFFFF0000U, 0x00000000U, 0x00000000U,
	0x00000000U, 0xFFFF0000U, 0x00000000U,
	0x00000000U, 0x00000000U, 0x40000000U,
};

static const uint32_t s_rotMatrix270[9] = {
	0x00000000U, 0xFFFF0000U, 0x00000000U,
	0x00010000U, 0x00000000U, 0x00000000U,
	0x00000000U, 0x00000000U, 0x40000000U,
};

static const uint8_t s_aacDecoderSpecificInfo[2] = { 0x11, 0x90 };

extern "C" {

int32_t Mp4RecFormatDateUtf16Be(SceUInt64 tickVal, char *outBuf, size_t outSize)
{
	if (outBuf == NULL || outSize < 40U)
		return (int32_t)SCE_MP4REC_ERROR_INVALID_ARG;

	SceRtcTick tick;
	tick.tick = tickVal;
	SceDateTime dt;
	int32_t ret = sceRtcSetTick(&dt, &tick);
	if (ret < 0)
		return ret;

	memset(outBuf, 0, outSize);
	snprintf(outBuf, 20U, "%04d/%02d/%02d %02d:%02d:%02d",
		dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
	outBuf[19] = '\0';

	for (int32_t i = 19; i >= 0; i--) {
		outBuf[i * 2 + 1] = outBuf[i];
		outBuf[i * 2] = '\0';
	}
	return 40;
}

uint32_t Mp4RecRescaleDuration(uint32_t ticks, uint32_t srcScale, uint32_t dstScale, uint32_t *outVal)
{
	uint32_t val = (uint32_t)(((double)dstScale / (double)srcScale) * (double)ticks);
	*outVal = val;
	return val;
}

static uint32_t SwapBe32(uint32_t v)
{
	return ((v & 0xFFU) << 24) | ((v & 0xFF00U) << 8) |
		((v & 0xFF0000U) >> 8) | ((v >> 24) & 0xFFU);
}

int32_t Mp4RecBuildChapterData(uint8_t *outBuf, uint32_t outSize, const uint32_t *chapters,
	uint32_t chapterCount, uint32_t totalDuration90k)
{
	(void)outSize;
	outBuf[0] = 0;
	*(uint16_t *)(outBuf + 2) = 0x0100U;
	*(uint32_t *)(outBuf + 4) = 0;
	*(uint32_t *)(outBuf + 12) = SwapBe32(totalDuration90k);

	uint32_t count = 0;
	uint32_t *dst = (uint32_t *)(outBuf + 20);
	while (count < chapterCount && count < SCE_MP4REC_CHAPTER_NUM_MAX) {
		uint32_t pts90k = (uint32_t)(90.0 * (double)chapters[count]);
		dst[count] = SwapBe32(pts90k);
		count++;
	}
	outBuf[16] = (uint8_t)((count >> 8) & 0xFFU);
	outBuf[17] = (uint8_t)(count & 0xFFU);
	return 0;
}

int32_t Mp4RecConvertUtf8ToUtf16Be(const char *src, uint32_t srcLen, uint16_t *dst, uint32_t dstChars)
{
	if (src == NULL || dst == NULL)
		return 0;

	uint32_t outChars = 0;
	uint32_t inBytes = (uint32_t)strlen(src) + 1U;
	SceCesUcsContext ctx;
	if (sceCesUcsContextInit(&ctx) == 0) {
		sceCesUcsSetPolicyOutputBom(&ctx, 0);
		if (sceCesUcsSetUtf16StrEndian(&ctx, CES_ENDIAN_BE, CES_ENDIAN_BE) == 0)
			sceCesUtf8StrToUtf16Str(&ctx, (const uint8_t *)src, srcLen, &inBytes, dst, dstChars, &outChars);
	}
	return (int32_t)outChars;
}

int32_t Mp4RecIoRead(SceUID fd, void *buf, SceSize size, int32_t unused, uint64_t *outRead)
{
	(void)unused;
	SceSSize ret = sceIoRead(fd, buf, size);
	*outRead = (uint64_t)(int64_t)ret;
	return (ret >= 0) ? 0 : (int32_t)ret;
}

int32_t Mp4RecIoWrite(SceUID fd, const void *buf, SceSize size, int32_t unused, uint64_t *outWritten)
{
	(void)unused;
	SceSSize ret = sceIoWrite(fd, buf, size);
	*outWritten = (uint64_t)(int64_t)ret;
	return (ret >= 0) ? 0 : (int32_t)ret;
}

int32_t Mp4RecIoSeek(SceUID fd, int64_t offset, int32_t whence)
{
	SceOff ret = sceIoLseek(fd, offset, whence);
	return (ret >= 0) ? 0 : (int32_t)ret;
}

int32_t Mp4RecIoTell(SceUID fd, SceOff *outPos)
{
	SceOff pos = sceIoLseek(fd, 0, SCE_SEEK_CUR);
	*outPos = pos;
	return (pos >= 0) ? 0 : (int32_t)pos;
}

int32_t Mp4RecIoGetSize(SceUID fd, uint64_t *outSize)
{
	SceOff cur = sceIoLseek(fd, 0, SCE_SEEK_CUR);
	SceOff end = sceIoLseek(fd, 0, SCE_SEEK_END);
	*outSize = (uint64_t)end;
	SceOff back = sceIoLseek(fd, cur, SCE_SEEK_SET);
	return (back >= 0) ? 0 : (int32_t)back;
}

} /* extern "C" */

int32_t BufferedIoRead(void *ctx, void *buf, uint64_t size, uint64_t *outRead)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	SceSSize ret = sceIoRead(w->m_fd, buf, (SceSize)size);
	*outRead = (uint64_t)(int64_t)ret;
	return (ret >= 0) ? 0 : (int32_t)ret;
}

static int32_t BufferedIoReadAlt(void *ctx, void *buf, uint64_t size, uint64_t *outRead)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	SceSSize ret = sceIoRead(w->m_fd, buf, (SceSize)size);
	*outRead = (uint64_t)(int64_t)ret;
	return (ret >= 0) ? 0 : (int32_t)ret;
}

static int32_t BufferedIoWrite(void *ctx, const void *buf, uint64_t size, uint64_t *outWritten)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	const uint8_t *src = (const uint8_t *)buf;
	uint32_t remain = (uint32_t)size;
	uint8_t *dst = (uint8_t *)w->m_curBuf + w->m_curPos;

	if ((uint64_t)w->m_curPos + size > MP4REC_WRITE_BLOCK_SIZE) {
		uint32_t head = MP4REC_WRITE_BLOCK_SIZE - w->m_curPos;
		memcpy(dst, src, head);

		uint32_t submitted = 0;
		int32_t ret = ThrWriteFileBufSubmit(&w->m_thrWriter, w->m_curBuf, MP4REC_WRITE_BLOCK_SIZE, &submitted);
		if (ret < 0 && ret != -2)
			return ret;

		void *nextBuf = NULL;
		uint32_t nextSize = 0;
		ret = ThrWriteFileBufAcquire(&w->m_thrWriter, &nextBuf, &nextSize);
		if (ret < 0 && ret != -2)
			return ret;

		w->m_curBuf = nextBuf;
		w->m_fileOffset += MP4REC_WRITE_BLOCK_SIZE;
		if (w->m_fileOffset > w->m_fileSize)
			w->m_fileSize = w->m_fileOffset;
		s_muxFlushCount++;
		if (ret < 0)
			return ret;

		src += head;
		remain -= head;
		w->m_curPos = 0;
		dst = (uint8_t *)w->m_curBuf;
	}

	memcpy(dst, src, remain);
	w->m_curPos += remain;
	*outWritten = size;
	return 0;
}

int32_t BufferedIoGetSize(void *ctx, uint64_t *outSize)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	SceOff cur = sceIoLseek(w->m_fd, 0, SCE_SEEK_CUR);
	SceOff end = sceIoLseek(w->m_fd, 0, SCE_SEEK_END);
	sceIoLseek(w->m_fd, cur, SCE_SEEK_SET);
	*outSize = (uint64_t)end + w->m_curPos;
	return 0;
}

static int32_t BufferedIoSeek(void *ctx, int64_t offset, int32_t whence)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	uint64_t target;

	if (whence == 0) {
		target = (uint64_t)offset;
	} else if (whence == 1) {
		target = w->m_fileOffset + (uint64_t)offset;
	} else {
		SceOff cur = sceIoLseek(w->m_fd, 0, SCE_SEEK_CUR);
		SceOff end = sceIoLseek(w->m_fd, 0, SCE_SEEK_END);
		sceIoLseek(w->m_fd, cur, SCE_SEEK_SET);
		uint64_t total = (uint64_t)end + w->m_curPos;
		target = total + (uint64_t)offset;
		if (target > total)
			target = total;
	}

	if (target >= w->m_fileOffset && target < w->m_fileOffset + MP4REC_WRITE_BLOCK_SIZE) {
		w->m_curPos = (uint32_t)(target - w->m_fileOffset);
		return 0;
	}

	if (w->m_curPos != 0) {
		uint32_t submitted = 0;
		int32_t ret = ThrWriteFileBufSubmit(&w->m_thrWriter, w->m_curBuf, w->m_curPos, &submitted);
		if (ret < 0 && ret != -2)
			return ret;
		ret = ThrWriteFileBufFlush(&w->m_thrWriter);
		if (ret < 0 && ret != -2)
			return ret;
		void *nextBuf = NULL;
		uint32_t nextSize = 0;
		ret = ThrWriteFileBufAcquire(&w->m_thrWriter, &nextBuf, &nextSize);
		if (ret < 0 && ret != -2)
			return ret;
		w->m_curBuf = nextBuf;
		uint64_t endPos = w->m_fileOffset + w->m_curPos;
		if (endPos > w->m_fileSize)
			w->m_fileSize = endPos;
		w->m_curPos = 0;
	}

	uint64_t aligned = target & ~(uint64_t)(MP4REC_WRITE_BLOCK_SIZE - 1U);
	w->m_fileOffset = aligned;
	SceOff seekRes = sceIoLseek(w->m_fd, (SceOff)aligned, SCE_SEEK_SET);
	if (seekRes < 0)
		return (int32_t)seekRes;

	if (target <= w->m_fileSize) {
		sceIoRead(w->m_fd, w->m_curBuf, MP4REC_WRITE_BLOCK_SIZE);
		seekRes = sceIoLseek(w->m_fd, (SceOff)w->m_fileOffset, SCE_SEEK_SET);
		if (seekRes < 0)
			return (int32_t)seekRes;
	}

	w->m_curPos = (uint32_t)(target & (MP4REC_WRITE_BLOCK_SIZE - 1U));
	return 0;
}

int32_t BufferedIoTell(void *ctx, uint64_t *outPos)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	*outPos = w->m_fileOffset + w->m_curPos;
	return 0;
}

static int32_t BufferedIoTellAlt(void *ctx, uint64_t *outPos)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	*outPos = w->m_fileOffset + w->m_curPos;
	return 0;
}

static int32_t BufferedIoGetSizeAlt(void *ctx, uint64_t *outSize)
{
	BufferedFileWriter *w = (BufferedFileWriter *)ctx;
	SceOff cur = sceIoLseek(w->m_fd, 0, SCE_SEEK_CUR);
	SceOff end = sceIoLseek(w->m_fd, 0, SCE_SEEK_END);
	sceIoLseek(w->m_fd, cur, SCE_SEEK_SET);
	*outSize = (uint64_t)end + w->m_curPos;
	return 0;
}

static void *Ff4AllocCb(void *allocUser, uint64_t size)
{
	return ((MemAllocator *)allocUser)->Malloc((size_t)size);
}

static void Ff4FreeCb(void *allocUser, void *ptr)
{
	((MemAllocator *)allocUser)->Free(ptr);
}

static char *Ff4StrncpyCb(void *allocUser, char *dst, uint64_t dstSize, const char *src, uint64_t n)
{
	(void)allocUser;
	size_t count = (dstSize > n + 1U) ? (size_t)n : (size_t)dstSize;
	return strncpy(dst, src, count);
}

static int32_t Ff4StrncmpCb(void *allocUser, const char *s1, const char *s2, uint64_t n)
{
	(void)allocUser;
	return strncmp(s1, s2, (size_t)n);
}

static int32_t Ff4MemcmpCb(void *allocUser, const void *p1, const void *p2, uint64_t n)
{
	(void)allocUser;
	return memcmp(p1, p2, (size_t)n);
}

static void *Ff4MemcpyCb(void *allocUser, void *dst, uint64_t dstSize, const void *src, uint64_t n)
{
	(void)allocUser;
	if (dstSize >= n)
		return memcpy(dst, src, (size_t)n);
	return NULL;
}

static void *Ff4MemmoveCb(void *allocUser, void *dst, uint64_t dstSize, const void *src, uint64_t n)
{
	(void)allocUser;
	if (dstSize >= n)
		return memmove(dst, src, (size_t)n);
	return NULL;
}

static void *Ff4MemsetCb(void *allocUser, void *dst, int32_t val, uint64_t n)
{
	(void)allocUser;
	return memset(dst, val, (size_t)n);
}

static int32_t BufferedIoWriteWrapper(void *ctx, const void *buf, uint64_t size, uint64_t *outWritten)
{
	return BufferedIoWrite(ctx, buf, size, outWritten);
}

static int32_t BufferedIoSeekWrapper(void *ctx, int64_t offset, int32_t whence)
{
	return BufferedIoSeek(ctx, offset, whence);
}

BufferedFileWriter::BufferedFileWriter()
{
	m_curBuf = NULL;
	m_curPos = 0;
	m_fd = 0;
	m_fileOffset = 0;
	m_fileSize = 0;
	m_directLowSpaceCb = NULL;
	m_allocator = NULL;
}

BufferedFileWriter::~BufferedFileWriter()
{
	ThrWriteFileBufTerm(&m_thrWriter);
	m_curBuf = NULL;
}

int32_t BufferedFileWriter::Term()
{
	ThrWriteFileBufTerm(&m_thrWriter);
	m_curBuf = NULL;
	return 0;
}

int32_t BufferedFileWriter::Init(SceUID fd, MemAllocator *allocator, int32_t affinity, int32_t priority)
{
	m_allocator = allocator;
	m_fd = fd;
	m_curPos = 0;

	int32_t ret = ThrWriteFileBufInit(&m_thrWriter, allocator, fd, priority, affinity);
	if (ret >= 0) {
		void *buf = NULL;
		uint32_t sz = 0;
		ret = ThrWriteFileBufAcquire(&m_thrWriter, &buf, &sz);
		if (ret >= 0) {
			m_curBuf = buf;
			return ret;
		}
		ThrWriteFileBufTerm(&m_thrWriter);
	}
	ThrWriteFileBufTerm(&m_thrWriter);
	m_curBuf = NULL;
	return ret;
}

int32_t BufferedFileWriter::Flush()
{
	uint32_t pending = m_curPos;
	if (pending != 0) {
		uint32_t submitted = 0;
		int32_t ret = ThrWriteFileBufSubmit(&m_thrWriter, m_curBuf, pending, &submitted);
		if (ret < 0 && ret != -2)
			return ret;
		ret = ThrWriteFileBufFlush(&m_thrWriter);
		if (ret < 0 && ret != -2)
			return ret;
		void *nextBuf = NULL;
		uint32_t nextSize = 0;
		ret = ThrWriteFileBufAcquire(&m_thrWriter, &nextBuf, &nextSize);
		if (ret < 0 && ret != -2)
			return ret;
		m_curBuf = nextBuf;
		uint64_t endPos = m_fileOffset + pending;
		if (endPos > m_fileSize)
			m_fileSize = endPos;
		m_curPos = 0;
	}
	m_fileOffset += pending;
	return 0;
}

int32_t BufferedFileWriter::SetLowSpaceCallback(void *unused, uint64_t threshold, LowSpaceCallbackFn cb)
{
	return ThrWriteFileBufSetLowSpaceCallback(&m_thrWriter, unused, threshold, cb);
}

int32_t BufferedFileWriter::DirectWriteWithSpaceCheck(SceUID fd, const void *buf, uint32_t unused,
	uint32_t size, uint32_t unused2, uint64_t *outWritten)
{
	(void)unused;
	(void)unused2;
	uint64_t maxSize = 0;
	uint64_t freeSize = 0;
	*outWritten = (uint64_t)(int64_t)sceIoWrite(fd, buf, size);
	int32_t ret = sceAppMgrGetDevInfo("ux0:", &maxSize, &freeSize);
	if (ret >= 0 && freeSize <= m_directLowSpaceThreshold && m_directLowSpaceCb != NULL)
		m_directLowSpaceCb(freeSize);
	return ret;
}

int32_t Mp4Muxer::QueryMemSize(const AvrecConfig *cfg, AvrecMemReq *req)
{
	req->heapSize += 2654208U;
	req->heapSize += SampleRingBuffer::CalcWorkMemSize((3U * cfg->audioBitrate) >> 3, 100U);
	req->heapSize += SampleRingBuffer::CalcWorkMemSize((3U * cfg->maxBitrate) >> 3, 100U);
	return 0;
}

Mp4Muxer::Mp4Muxer()
	: m_audioBuf(),
	  m_videoBuf(),
	  m_fileWriter()
{
	m_fileWriterPtr = NULL;
	m_ff4Writer = NULL;
	m_ff4Session = NULL;
	m_currentOffset = 0;
	m_mdatStartOffset = 0;
	m_videoTrackId = 0;
	m_audioTrackId = 0;
	m_videoSampleDescId = 0;
	m_audioSampleDescId = 0;
	m_lastWrittenStream = 0;
	m_mutexCreated = 0;
	m_videoChunkScale = 0;
	m_audioChunkScale = 0;
	m_fd = 0;
	m_avcProfileLevel = 0;
	m_reserved552 = 0;
}

Mp4Muxer::~Mp4Muxer()
{
}

int32_t Mp4Muxer::MapFf4Error(int32_t ff4Err)
{
	switch (ff4Err) {
	case 0:
		return 0;
	case 1:
		return MP4REC_FF4_ERR_ARGS;
	case 2:
		return MP4REC_FF4_ERR_STATE;
	case 0x100:
		return MP4REC_FF4_ERR_NOMEM;
	case 0x101:
		return MP4REC_FF4_ERR_FILE;
	case 0x200:
		return MP4REC_FF4_ERR_NOTFOUND;
	case 0x201:
		return MP4REC_FF4_ERR_OVERFLOW;
	case 0x1000:
		return MP4REC_FF4_ERR_FORMAT;
	case 0x10000000:
		return MP4REC_FF4_ERR_INTERNAL;
	default:
		return MP4REC_FF4_ERR_FATAL;
	}
}

static uint32_t FrameRateCodeToTimescale(uint32_t code)
{
	if (code > 1U)
		return (code <= 2U) ? 25000U : 30000U;
	return (code != 0) ? 30000U : 12500U;
}

static uint32_t FrameRateCodeToChunkScale(uint32_t code)
{
	if (code > 1U)
		return (code <= 2U) ? 9600U : 8008U;
	return (code != 0) ? 16016U : 19200U;
}

int32_t Mp4Muxer::Init(const AvrecConfig *cfg, const char *filePath, MemAllocator *allocator)
{
	uint32_t timescale = FrameRateCodeToTimescale(cfg->frameRateCode);

	m_allocator = allocator;
	m_lastWrittenStream = 0;

	int32_t ret = sceKernelCreateLwMutex(&m_mutex, "SceMp4RecAvrecMux", 0, 0, NULL);
	if (ret != 0)
		return ret;
	m_mutexCreated = 1;

	ret = m_audioBuf.Init(49152U, 100U, allocator);
	if (ret != 0)
		return ret;
	ret = m_videoBuf.Init(943718U, 100U, allocator);
	if (ret != 0)
		return ret;

	m_videoChunkScale = FrameRateCodeToChunkScale(cfg->frameRateCode);
	m_audioChunkScale = 5120U;

	Ff4Callbacks *cb = (Ff4Callbacks *)m_ff4Callbacks;
	cb->allocUser = m_allocator;
	cb->ioRead = BufferedIoReadAlt;
	cb->ioWrite = BufferedIoWriteWrapper;
	cb->ioSeek = BufferedIoSeekWrapper;
	cb->ioTell = BufferedIoTellAlt;
	cb->ioGetSize = BufferedIoGetSizeAlt;
	cb->memAlloc = Ff4AllocCb;
	cb->memFree = Ff4FreeCb;
	cb->strCmp = Ff4StrncmpCb;
	cb->strCpy = Ff4StrncpyCb;
	cb->memCmp = Ff4MemcmpCb;
	cb->memCpy = Ff4MemcpyCb;
	cb->memMove = Ff4MemmoveCb;
	cb->memSet = Ff4MemsetCb;

	int32_t ff4Res = Ff4CreateSession(cb, &m_ff4Session);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4WriterInitParam wParam;
	memset(&wParam, 0, sizeof(wParam));
	wParam.maxMemorySize = 2621440U;
	wParam.modeFlags = 1U;
	ff4Res = Ff4CreateWriter(m_ff4Session, &wParam, &m_ff4Writer);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	SceUID fd = sceIoOpen(filePath, 0x0603, 6);
	if (fd < 0)
		return fd;
	m_fd = fd;

	ret = m_fileWriter.Init(fd, m_allocator, cfg->writeThreadAffinity, cfg->writeThreadPriority);
	if (ret < 0)
		return ret;
	m_fileWriterPtr = &m_fileWriter;

	Ff4FtypParam ftyp;
	ftyp.majorBrand = 0x4D534E56U; /* 'MSNV' */
	ftyp.minorVersion = 0x01642005U;
	uint64_t written = 0;

	ff4Res = Ff4SetFtyp(m_ff4Writer, &ftyp);
	if (ff4Res == 0) ff4Res = Ff4AddCompatibleBrand(m_ff4Writer, ftyp.majorBrand);
	if (ff4Res == 0) ff4Res = Ff4AddCompatibleBrand(m_ff4Writer, 0x69736F6DU); /* 'isom' */
	if (ff4Res == 0) ff4Res = Ff4AddCompatibleBrand(m_ff4Writer, 0x6D703432U); /* 'mp42' */
	if (ff4Res == 0) ff4Res = Ff4WriteFtyp(m_ff4Writer, m_fileWriterPtr, m_currentOffset, &written);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);
	m_currentOffset += written;

	uint32_t profId = 0;
	ff4Res = Ff4CreateProfileBox(m_ff4Writer, &profId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4ProfAudioParam aProf;
	memset(&aProf, 0, sizeof(aProf));
	aProf.trackIndex = 2;
	aProf.codecFourcc = 0x6D703461U; /* 'mp4a' */
	aProf.profileLevel = 553;
	aProf.avgBitrateKbps = cfg->audioBitrate / 1000U;
	aProf.maxBitrateKbps = cfg->audioBitrate / 1000U;
	aProf.sampleRate = 48000U;
	aProf.channels = 2;
	ff4Res = Ff4AddAudioProfile(m_ff4Writer, &aProf);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	uint32_t profLevel = (cfg->width > 480U) ? 0x0042E01EU : 0x0042E015U;
	m_avcProfileLevel = profLevel;

	Ff4ProfVideoParam vProf;
	memset(&vProf, 0, sizeof(vProf));
	vProf.trackIndex = 1;
	vProf.codecFourcc = 0x61766331U; /* 'avc1' */
	vProf.profileLevel = profLevel;
	vProf.avgBitrateKbps = cfg->targetBitrate / 1000U;
	vProf.maxBitrateKbps = cfg->maxBitrate / 1000U;
	vProf.fpsInt = (uint16_t)(timescale / 1001U);
	vProf.fpsFrac = (uint16_t)(((timescale % 1001U) << 16) / 1001U);
	vProf.fpsInt2 = vProf.fpsInt;
	vProf.fpsFrac2 = vProf.fpsFrac;
	vProf.width = (uint16_t)cfg->width;
	vProf.height = (uint16_t)cfg->height;
	vProf.aspectRatio = 0x00010001U;
	ff4Res = Ff4AddVideoProfile(m_ff4Writer, &vProf);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	ff4Res = Ff4WriteProfileBox(m_ff4Writer, m_fileWriterPtr, m_currentOffset, &written);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);
	m_currentOffset += written;
	m_mdatStartOffset = m_currentOffset;

	ff4Res = Ff4WriteMdatHeader(m_ff4Writer, NULL, 0, 0x6D646174U, 0, &written);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);
	m_currentOffset += written;
	m_mdatDataOffset = m_currentOffset;

	ret = BufferedIoSeek(m_fileWriterPtr, (int64_t)m_currentOffset, 0);
	if (ret != 0)
		return ret;

	ff4Res = Ff4SetMovieTimescale(m_ff4Writer, 90000U);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4VideoTrackParam vTrack;
	memset(&vTrack, 0, sizeof(vTrack));
	vTrack.trackId = vProf.trackIndex;
	vTrack.width = vProf.width;
	vTrack.height = vProf.height;
	vTrack.timescale = FrameRateCodeToTimescale(cfg->frameRateCode);
	vTrack.language = SCE_MP4REC_LANG_ENGLISH;
	vTrack.flags = 1;
	ff4Res = Ff4AddVideoTrack(m_ff4Writer, &vTrack, &m_videoTrackId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4TrackAttrParam trackAttr;
	memset(&trackAttr, 0, sizeof(trackAttr));
	if (cfg->rotation == 1U) {
		memcpy(trackAttr.matrix, s_rotMatrix90, sizeof(trackAttr.matrix));
		trackAttr.matrix[6] = cfg->height << 16;
		ff4Res = Ff4SetTrackAttribute(m_ff4Writer, m_videoTrackId, 512U, &trackAttr);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	} else if (cfg->rotation == 2U) {
		memcpy(trackAttr.matrix, s_rotMatrix180, sizeof(trackAttr.matrix));
		trackAttr.matrix[6] = cfg->width << 16;
		trackAttr.matrix[7] = cfg->height << 16;
		ff4Res = Ff4SetTrackAttribute(m_ff4Writer, m_videoTrackId, 512U, &trackAttr);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	} else if (cfg->rotation == 3U) {
		memcpy(trackAttr.matrix, s_rotMatrix270, sizeof(trackAttr.matrix));
		trackAttr.matrix[7] = cfg->width << 16;
		ff4Res = Ff4SetTrackAttribute(m_ff4Writer, m_videoTrackId, 512U, &trackAttr);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	}

	Ff4DataRefParam dref = { 0, 0 };
	uint32_t vDrefIdx = 0;
	ff4Res = Ff4AddTrackDataRef(m_ff4Writer, m_videoTrackId, &dref, &vDrefIdx);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4VideoSampleDescParam vDesc;
	memset(&vDesc, 0, sizeof(vDesc));
	vDesc.codecType = 2;
	vDesc.dataRefIndex = vDrefIdx;
	vDesc.width = vTrack.width;
	vDesc.height = vTrack.height;
	vDesc.horizResInt = 72;
	vDesc.vertResInt = 72;
	ff4Res = Ff4AddVideoSampleDesc(m_ff4Writer, m_videoTrackId, &vDesc, &m_videoSampleDescId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4AudioTrackParam aTrack;
	memset(&aTrack, 0, sizeof(aTrack));
	aTrack.trackId = aProf.trackIndex;
	aTrack.timescale = 48000U;
	aTrack.language = SCE_MP4REC_LANG_ENGLISH;
	ff4Res = Ff4AddAudioTrack(m_ff4Writer, &aTrack, &m_audioTrackId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	uint32_t aDrefIdx = 0;
	ff4Res = Ff4AddTrackDataRef(m_ff4Writer, m_audioTrackId, &dref, &aDrefIdx);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4AudioSampleDescParam aDesc;
	aDesc.dataRefIndex = aDrefIdx;
	aDesc.channelCount = 2;
	aDesc.sampleSize = 16;
	ff4Res = Ff4AddAudioSampleDesc(m_ff4Writer, m_audioTrackId, &aDesc, &m_audioSampleDescId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4EsdsParam esds;
	esds.objectType = 1344;
	esds.bufferSizeDb = 1536;
	esds.maxBitrate = cfg->audioBitrate;
	esds.avgBitrate = cfg->audioBitrate;
	esds.decoderSpecificInfo = s_aacDecoderSpecificInfo;
	esds.decoderSpecificInfoSize = sizeof(s_aacDecoderSpecificInfo);
	ff4Res = Ff4SetAudioEsds(m_ff4Writer, m_audioTrackId, m_audioSampleDescId, &esds);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	return 0;
}

int32_t Mp4Muxer::WriteVideoSamples(SampleIterator *it)
{
	Ff4SampleEntry entry;
	memset(&entry, 0, sizeof(entry));
	entry.duration = SCE_MP4REC_VIDEO_SAMPLE_DURATION;
	entry.isSync = 1;
	entry.sampleDescIndex = m_videoSampleDescId;

	int32_t wroteAny = 0;
	SampleSpan *span;
	while ((span = it->Next()) != NULL) {
		entry.size = span->size;
		entry.offset = m_currentOffset;
		int32_t ff4Res = Ff4AddTrackSample(m_ff4Writer, m_videoTrackId, &entry, 0);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
		entry.isSync = 0;

		uint64_t written = 0;
		int32_t ioRes = BufferedIoWrite(m_fileWriterPtr, span->data, span->size, &written);
		if (ioRes != 0)
			return ioRes;
		m_currentOffset += written;
		wroteAny = 1;
	}

	if (wroteAny != 0) {
		int32_t ff4Res = Ff4FlushTrackChunk(m_ff4Writer, m_videoTrackId);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	}
	m_lastWrittenStream = 2;
	return 0;
}

int32_t Mp4Muxer::WriteAudioSamples(SampleIterator *it)
{
	Ff4SampleEntry entry;
	memset(&entry, 0, sizeof(entry));
	entry.duration = SCE_MP4REC_AUDIO_SAMPLE_DURATION;
	entry.isSync = 1;
	entry.sampleDescIndex = m_audioSampleDescId;

	int32_t wroteAny = 0;
	SampleSpan *span;
	while ((span = it->Next()) != NULL) {
		entry.size = span->size;
		entry.offset = m_currentOffset;
		int32_t ff4Res = Ff4AddTrackSample(m_ff4Writer, m_audioTrackId, &entry, 0);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);

		uint64_t written = 0;
		int32_t ioRes = BufferedIoWrite(m_fileWriterPtr, span->data, span->size, &written);
		if (ioRes != 0)
			return ioRes;
		m_currentOffset += written;
		wroteAny = 1;
	}

	if (wroteAny != 0) {
		int32_t ff4Res = Ff4FlushTrackChunk(m_ff4Writer, m_audioTrackId);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	}
	m_lastWrittenStream = 1;
	return 0;
}

int32_t Mp4Muxer::Finalize(const uint16_t *creationDate)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);

	int32_t ret = 0;
	if (m_lastWrittenStream == 2U) {
		SampleIterator aIt = m_audioBuf.GetAllSamplesIterator();
		ret = WriteAudioSamples(&aIt);
		if (ret == 0) {
			SampleIterator vIt = m_videoBuf.GetAllSamplesIterator();
			ret = WriteVideoSamples(&vIt);
		}
	} else {
		SampleIterator vIt = m_videoBuf.GetAllSamplesIterator();
		ret = WriteVideoSamples(&vIt);
		if (ret == 0) {
			SampleIterator aIt = m_audioBuf.GetAllSamplesIterator();
			ret = WriteAudioSamples(&aIt);
		}
	}

	if (ret == 0) {
		m_fileWriter.Flush();
		m_audioBuf.Term(m_allocator);
		m_videoBuf.Term(m_allocator);

		uint64_t hdrSize = 0;
		uint64_t mdatPayload = m_currentOffset - m_mdatDataOffset;
		int32_t ff4Res = Ff4WriteMdatHeader(m_ff4Writer, m_fileWriterPtr,
			m_mdatStartOffset, 0x6D646174U, mdatPayload, &hdrSize);
		if (ff4Res != 0) {
			ret = MapFf4Error(ff4Res);
		} else {
			uint32_t finParam[2] = { 0, 0 };
			ff4Res = Ff4FinalizeTrack(m_ff4Writer, m_videoTrackId, finParam);
			if (ff4Res == 0)
				ff4Res = Ff4FinalizeTrack(m_ff4Writer, m_audioTrackId, finParam);
			if (ff4Res == 0 && creationDate != NULL) {
				uint16_t dateCopy[6];
				memcpy(dateCopy, creationDate, sizeof(dateCopy));
				uint64_t mp4Time = 0;
				ff4Res = Ff4ConvertDateToMp4Time(m_ff4Session, dateCopy, &mp4Time);
				if (ff4Res == 0)
					ff4Res = Ff4SetCreationTime(m_ff4Writer, 0, 0xFFU, mp4Time, mp4Time);
			}
			if (ff4Res == 0) {
				uint64_t moovSize = 0;
				ff4Res = Ff4WriteMoov(m_ff4Writer, m_fileWriterPtr, m_currentOffset, &moovSize);
				if (ff4Res == 0) {
					m_currentOffset += moovSize;
					m_fileWriter.Flush();
				}
			}
			if (ff4Res != 0)
				ret = MapFf4Error(ff4Res);
		}
	}

	sceKernelUnlockLwMutex(&m_mutex, 1);
	return ret;
}

int32_t Mp4Muxer::Term()
{
	Ff4DestroyWriter(m_ff4Writer);
	Ff4DestroySession(m_ff4Session);
	m_fileWriter.Term();
	if (m_fd > 0) {
		sceIoClose(m_fd);
		m_fd = 0;
	}
	int32_t ret = 0;
	if (m_mutexCreated != 0) {
		ret = sceKernelDeleteLwMutex(&m_mutex);
		m_mutexCreated = 0;
	}
	return ret;
}

int32_t Mp4Muxer::FlushInterleaved()
{
	int32_t ret = 0;
	sceKernelGetProcessTimeWide();

	for (;;) {
		if (m_videoBuf.HasReadyChunk() && m_lastWrittenStream != 2U) {
			SampleIterator vIt = m_videoBuf.GetReadyChunkIterator();
			ret = WriteVideoSamples(&vIt);
			if (ret != 0)
				break;
			ret = m_videoBuf.ConsumeReadyChunk();
			if (ret != 0)
				break;
		}
		if (!m_audioBuf.HasReadyChunk() || m_lastWrittenStream != 2U) {
			sceKernelGetProcessTimeWide();
			break;
		}
		SampleIterator aIt = m_audioBuf.GetReadyChunkIterator();
		ret = WriteAudioSamples(&aIt);
		if (ret != 0)
			break;
		ret = m_audioBuf.ConsumeReadyChunk();
		if (ret != 0)
			break;
	}
	return ret;
}

int32_t Mp4Muxer::AddVideoAu(const void *data, size_t size, const AvrecVideoFrameMeta *meta)
{
	if (data == NULL || size == 0 || meta->frameType == 0)
		return 0;

	int32_t ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		return ret;

	int32_t err = 0;
	if (meta->frameType == 1) {
		err = m_videoBuf.MarkChunkNow();
		if (err == 0) {
			uint32_t vCount = m_videoBuf.GetTotalPushed();
			uint32_t audioTarget = (uint32_t)(((double)(vCount * m_videoChunkScale) / (double)m_audioChunkScale) + 0.5);
			m_audioBuf.MarkChunkAt(audioTarget);
			err = FlushInterleaved();
		}
	}

	if (err == 0) {
		err = m_videoBuf.Push(data, size);
		if (err == MP4REC_AVREC_ERR_BUFFER_FULL)
			err = MP4REC_AVREC_ERR_VIDEO_BUFFER_FULL;
		else if (err == MP4REC_AVREC_ERR_SAMPLE_NUM_FULL)
			err = MP4REC_AVREC_ERR_VIDEO_SAMPLE_NUM_FULL;
	}

	sceKernelUnlockLwMutex(&m_mutex, 1);
	return err;
}

int32_t Mp4Muxer::AddAudioAu(const void *data, size_t size)
{
	if (data == NULL || size == 0)
		return 0;

	int32_t ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		return ret;

	int32_t err = m_audioBuf.Push(data, size);
	if (err == MP4REC_AVREC_ERR_BUFFER_FULL)
		err = MP4REC_AVREC_ERR_AUDIO_BUFFER_FULL;
	else if (err == MP4REC_AVREC_ERR_SAMPLE_NUM_FULL)
		err = MP4REC_AVREC_ERR_AUDIO_SAMPLE_NUM_FULL;
	else if (err == 0)
		err = FlushInterleaved();

	sceKernelUnlockLwMutex(&m_mutex, 1);
	return err;
}

int32_t Mp4Muxer::SetAvcConfig(const void *sps, uint32_t spsSize, const void *pps, uint32_t ppsSize)
{
	Ff4AvcConfigParam avcCfg;
	memset(&avcCfg, 0, sizeof(avcCfg));
	avcCfg.configurationVersion = 1;
	avcCfg.profileIndication = 66;
	avcCfg.profileCompatibility = 0xE0;
	avcCfg.levelIndication = (uint8_t)(m_avcProfileLevel & 0xFFU);
	avcCfg.lengthSizeMinusOne = 3;

	int32_t ff4Res = Ff4SetAvcConfigHeader(m_ff4Writer, m_videoTrackId, m_videoSampleDescId, &avcCfg);
	if (ff4Res == 0) {
		Ff4NalDataParam spsParam = { sps, spsSize };
		ff4Res = Ff4AddAvcSps(m_ff4Writer, m_videoTrackId, m_videoSampleDescId, &spsParam);
	}
	if (ff4Res == 0) {
		Ff4NalDataParam ppsParam = { pps, ppsSize };
		ff4Res = Ff4AddAvcPps(m_ff4Writer, m_videoTrackId, m_videoSampleDescId, &ppsParam);
	}
	return (ff4Res != 0) ? MapFf4Error(ff4Res) : 0;
}

int32_t Mp4Muxer::AddMetadata(const AvrecMetadataItem *item)
{
	int32_t ff4Res = 0;
	if (item->extraData != NULL && item->extraSize != 0) {
		Ff4MetadataExtraParam extra;
		extra.type = item->type;
		extra.lang = SCE_MP4REC_LANG_ENGLISH;
		extra.encoding = 1;
		extra.data = item->extraData;
		extra.size = item->extraSize;
		ff4Res = Ff4AddUsmtExtraMetadata(m_ff4Writer, 0, &extra);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	}

	Ff4MetadataEntryParam entry;
	entry.type = item->type;
	entry.flag = item->flag;
	entry.pad0 = 0;
	entry.lang = item->lang;
	entry.encoding = item->encoding;
	entry.pad1 = 0;
	entry.data = item->data;
	entry.size = item->dataSize;
	ff4Res = Ff4AddUsmtMetadata(m_ff4Writer, 0, &entry);
	return (ff4Res != 0) ? MapFf4Error(ff4Res) : 0;
}

int32_t Mp4Muxer::PrepareThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count, int32_t flag)
{
	if (thumbs == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	uint32_t boxId = 0;
	uint32_t trackId = 0;
	uint32_t drefId = 0;
	uint32_t groupId = 0;

	int32_t ff4Res = Ff4CreateMtsdBox(m_ff4Writer, &boxId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);
	ff4Res = Ff4AddMtsdTrack(m_ff4Writer, boxId, &trackId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4DataRefParam dref = { 0, 0 };
	ff4Res = Ff4AddMtsdDataRef(m_ff4Writer, trackId, &dref, &drefId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	Ff4ThumbGroupParam grp;
	memset(&grp, 0, sizeof(grp));
	grp.type = 2564;
	grp.flag = 1;
	grp.lang = 0x55C4U;
	grp.groupIndex = (uint32_t)flag;
	ff4Res = Ff4AddMtsdGroup(m_ff4Writer, &grp, &groupId);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	uint64_t hdrSize = 0;
	ff4Res = Ff4WriteMtsdHeader(m_ff4Writer, NULL, 0, 0, &hdrSize);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);

	uint32_t curOffset = (uint32_t)hdrSize;
	for (uint32_t i = 0; i < count; i++) {
		Ff4ThumbSampleDescParam sdesc;
		memset(&sdesc, 0, sizeof(sdesc));
		sdesc.type = 1;
		sdesc.dataRefIndex = drefId;
		sdesc.width = thumbs[i].width;
		sdesc.height = thumbs[i].height;
		sdesc.horizResInt = 72;
		sdesc.vertResInt = 72;

		uint32_t descId = 0;
		ff4Res = Ff4AddMtsdSampleDesc(m_ff4Writer, trackId, &sdesc, &descId);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);

		uint32_t attrBuf[14];
		memset(attrBuf, 0, sizeof(attrBuf));
		uint32_t attrId = 1024;
		if (thumbs[i].format == 1) {
			attrBuf[0] = 0x504E4720U; /* 'PNG ' */
			attrId = 1025;
		}
		((uint16_t *)attrBuf)[27] = thumbs[i].depth;
		ff4Res = Ff4SetMtsdSampleDescAttr(m_ff4Writer, trackId, descId, attrId, attrBuf);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);

		Ff4ThumbItemParam item;
		item.sampleDescIndex = descId;
		item.offset = curOffset;
		item.size = thumbs[i].size;
		uint32_t sampleId = 0;
		ff4Res = Ff4AddMtsdSample(m_ff4Writer, trackId, &item, &sampleId);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);

		curOffset += thumbs[i].size;
		ff4Res = Ff4LinkMtsdSample(m_ff4Writer, groupId, trackId, sampleId);
		if (ff4Res != 0)
			return MapFf4Error(ff4Res);
	}
	return 0;
}

int32_t Mp4Muxer::WriteThumbnails(const AvrecThumbnailDesc *thumbs, uint32_t count)
{
	if (thumbs == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	uint64_t totalPayload = 0;
	for (uint32_t i = 0; i < count; i++)
		totalPayload += thumbs[i].size;

	uint64_t hdrSize = 0;
	int32_t ff4Res = Ff4WriteMtsdHeader(m_ff4Writer, m_fileWriterPtr, m_currentOffset, totalPayload, &hdrSize);
	if (ff4Res != 0)
		return MapFf4Error(ff4Res);
	m_currentOffset += hdrSize;

	for (uint32_t i = 0; i < count; i++) {
		uint64_t written = 0;
		int32_t ioRes = BufferedIoWrite(m_fileWriterPtr, thumbs[i].data, thumbs[i].size, &written);
		if (ioRes != 0)
			return ioRes;
	}
	m_fileWriter.Flush();
	return 0;
}

void Mp4MuxerGetLocalTime(SceDateTime *outTime)
{
	SceDateTime dt;
	sceRtcGetCurrentClockLocalTime(&dt);
	*outTime = dt;
}

int32_t Mp4MuxerDiffTimeUs(const SceDateTime *start, const SceDateTime *end)
{
	int32_t t1 = (int32_t)start->hour * 3600000000 + (int32_t)start->minute * 60000000 +
		(int32_t)start->second * 1000000 + (int32_t)start->microsecond;
	int32_t t2 = (int32_t)end->hour * 3600000000 + (int32_t)end->minute * 60000000 +
		(int32_t)end->second * 1000000 + (int32_t)end->microsecond;
	return t2 - t1;
}
