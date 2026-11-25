#include "avplayer_internal.h"

void avLogInfo(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...)
{
	(void)funcName;
	(void)line;
	if (level == SCE_AVPLAYER_DBG_INFO || level == SCE_AVPLAYER_DBG_ALL) {
		char buffer[256];
		va_list args;
		va_start(args, fmt);
		sceClibVsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		sceClibPrintf("%s\n", buffer);
	}
}

/* Base AvSourceFilter out-of-line virtual definitions (single vtable TU) */
AvSourceFilter::~AvSourceFilter()
{
}

int32_t AvSourceFilter::init(const char *path, SceAvPlayerFileReplacement *ops, int32_t dbgLevel)
{
	(void)path;
	(void)ops;
	(void)dbgLevel;
	return -1;
}

int32_t AvSourceFilter::open()
{
	return -1;
}

int32_t AvSourceFilter::close()
{
	return -1;
}

int32_t AvSourceFilter::getStreamCount()
{
	return -1;
}

int32_t AvSourceFilter::getStreamInfo(uint32_t trackIndex, AvStreamDescriptor *outDesc)
{
	(void)trackIndex;
	(void)outDesc;
	return -1;
}

int32_t AvSourceFilter::enableStream(uint32_t trackIndex, int32_t enable)
{
	(void)trackIndex;
	(void)enable;
	return -1;
}

int32_t AvSourceFilter::readAccessUnit(uint32_t trackIndex, AvAccessUnit *outAu)
{
	(void)trackIndex;
	(void)outAu;
	return -1;
}

int32_t AvSourceFilter::seekToTime(uint32_t trackIndex, uint64_t timeMs, uint32_t mode)
{
	(void)trackIndex;
	(void)timeMs;
	(void)mode;
	return -1;
}

int32_t AvSourceFilter::getTrackOffset(uint32_t trackIndex, uint32_t *outOffset)
{
	(void)trackIndex;
	(void)outOffset;
	return -1;
}

int32_t AvSourceFilter::getCodecConfig(uint32_t trackIndex, void *outConfig)
{
	(void)trackIndex;
	(void)outConfig;
	return -1;
}

int32_t AvSourceFilter::getExtraData(uint32_t trackIndex, void *outExtra)
{
	(void)trackIndex;
	(void)outExtra;
	return -1;
}

int64_t AvSourceFilter::getNextSyncPts(uint32_t trackIndex)
{
	(void)trackIndex;
	return -1;
}

int64_t AvSourceFilter::getDuration()
{
	return -1;
}

void AvSourceFilter::setDebugLevel(int32_t level)
{
	(void)level;
}

void AvSourceFilter::resetState()
{
}

void AvSourceFilter::clearFlags()
{
}

int32_t AvSourceFilter::getSourceType()
{
	return -1;
}

int32_t AvSourceFilter::isSeekable()
{
	return -1;
}

int32_t AvSourceFilter::getEncryptionInfo()
{
	return -1;
}

int32_t AvSourceFilter::shutdown()
{
	return 0;
}

/* Default sceIo file callbacks for AvMp4SourceFilter */
static int avDefaultFileOpen(void *argP, const char *filename)
{
	AvMp4SourceFilter *filter = (AvMp4SourceFilter *)argP;
	if (filter == NULL || filename == NULL)
		return -1;
	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	filter->defaultFileFd = fd;
	SceOff fileLen = sceIoLseek(fd, 0, SCE_SEEK_END);
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	filter->defaultFileSize = (fileLen > 0) ? (uint64_t)fileLen : 0;
	return 0;
}

static int avDefaultFileClose(void *argP)
{
	AvMp4SourceFilter *filter = (AvMp4SourceFilter *)argP;
	if (filter == NULL || filter->defaultFileFd < 0)
		return -1;
	int32_t ret = sceIoClose(filter->defaultFileFd);
	filter->defaultFileFd = -1;
	return ret;
}

static int avDefaultFileReadOffset(void *argP, uint8_t *buffer, uint64_t position, uint32_t length)
{
	AvMp4SourceFilter *filter = (AvMp4SourceFilter *)argP;
	if (filter == NULL || filter->defaultFileFd < 0 || buffer == NULL)
		return -1;
	return sceIoPread(filter->defaultFileFd, buffer, length, (SceOff)position);
}

static uint64_t avDefaultFileSize(void *argP)
{
	AvMp4SourceFilter *filter = (AvMp4SourceFilter *)argP;
	if (filter == NULL)
		return 0;
	return filter->defaultFileSize;
}

/* AvMp4SourceFilter implementation */
AvMp4SourceFilter::AvMp4SourceFilter()
{
	mp4Demux = NULL;
	debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
	sceClibMemset(&fileOps, 0, sizeof(fileOps));
	defaultFileFd = -1;
	defaultFileSize = 0;
	sceClibMemset(sourcePath, 0, sizeof(sourcePath));
}

AvMp4SourceFilter::~AvMp4SourceFilter()
{
	shutdown();
}

int32_t AvMp4SourceFilter::init(const char *path, SceAvPlayerFileReplacement *ops, int32_t dbgLevel)
{
	debugLevel = dbgLevel;
	if (path != NULL)
		sceClibSnprintf(sourcePath, sizeof(sourcePath), "%s", path);

	if (ops != NULL && ops->open != NULL && ops->close != NULL && ops->readOffset != NULL && ops->size != NULL) {
		fileOps = *ops;
	} else {
		fileOps.objectPointer = this;
		fileOps.open = avDefaultFileOpen;
		fileOps.close = avDefaultFileClose;
		fileOps.readOffset = avDefaultFileReadOffset;
		fileOps.size = avDefaultFileSize;
	}

	mp4Demux = SceMp4_920623C8();
	if (mp4Demux == NULL) {
		avLogError(debugLevel, "Init", 120, "Cannot create SceMp4 demux instance");
		return -1;
	}

	SceMp4_05474AF0(mp4Demux, debugLevel);
	return SceMp4_30E49E4D(mp4Demux, &fileOps, (void *)sourcePath);
}

int32_t AvMp4SourceFilter::open()
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_C05DFF01(mp4Demux);
}

int32_t AvMp4SourceFilter::close()
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_D2871F1C(mp4Demux);
}

int32_t AvMp4SourceFilter::getStreamCount()
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_A870587F(mp4Demux);
}

int32_t AvMp4SourceFilter::getStreamInfo(uint32_t trackIndex, AvStreamDescriptor *outDesc)
{
	if (mp4Demux == NULL || outDesc == NULL)
		return -1;
	return SceMp4_6B69F900(mp4Demux, trackIndex, outDesc);
}

int32_t AvMp4SourceFilter::enableStream(uint32_t trackIndex, int32_t enable)
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_40C64DC0(mp4Demux, trackIndex, enable);
}

int32_t AvMp4SourceFilter::readAccessUnit(uint32_t trackIndex, AvAccessUnit *outAu)
{
	if (mp4Demux == NULL || outAu == NULL)
		return -1;
	return SceMp4_20A01112(mp4Demux, trackIndex, outAu);
}

int32_t AvMp4SourceFilter::seekToTime(uint32_t trackIndex, uint64_t timeMs, uint32_t mode)
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_7B4832FE(mp4Demux, trackIndex, timeMs, mode);
}

int32_t AvMp4SourceFilter::getTrackOffset(uint32_t trackIndex, uint32_t *outOffset)
{
	if (mp4Demux == NULL || outOffset == NULL)
		return -1;
	return SceMp4_32A15788(mp4Demux, trackIndex, outOffset);
}

int32_t AvMp4SourceFilter::getCodecConfig(uint32_t trackIndex, void *outConfig)
{
	if (mp4Demux == NULL || outConfig == NULL)
		return -1;
	return SceMp4_D5B26179(mp4Demux, trackIndex, outConfig);
}

int32_t AvMp4SourceFilter::getExtraData(uint32_t trackIndex, void *outExtra)
{
	if (mp4Demux == NULL || outExtra == NULL)
		return -1;
	return SceMp4_609E57AD(mp4Demux, trackIndex, outExtra);
}

int64_t AvMp4SourceFilter::getNextSyncPts(uint32_t trackIndex)
{
	if (mp4Demux == NULL)
		return -1;
	uint64_t pts = 0;
	if (SceMp4_A7C09746(mp4Demux, trackIndex, &pts) < 0)
		return -1;
	return (int64_t)pts;
}

int64_t AvMp4SourceFilter::getDuration()
{
	if (mp4Demux == NULL)
		return -1;
	return (int64_t)SceMp4_40351E1A(mp4Demux);
}

void AvMp4SourceFilter::setDebugLevel(int32_t level)
{
	debugLevel = level;
	if (mp4Demux != NULL)
		SceMp4_05474AF0(mp4Demux, level);
}

void AvMp4SourceFilter::resetState()
{
	if (mp4Demux != NULL)
		SceMp4_96676BA0(mp4Demux);
}

void AvMp4SourceFilter::clearFlags()
{
	if (mp4Demux != NULL)
		SceMp4_CB91CEBC(mp4Demux);
}

int32_t AvMp4SourceFilter::getSourceType()
{
	if (mp4Demux == NULL)
		return -1;
	return SceMp4_D092A066(mp4Demux);
}

int32_t AvMp4SourceFilter::isSeekable()
{
	if (mp4Demux == NULL)
		return 0;
	return SceMp4_EFE299C9(mp4Demux);
}

int32_t AvMp4SourceFilter::getEncryptionInfo()
{
	return 0;
}

int32_t AvMp4SourceFilter::shutdown()
{
	if (mp4Demux != NULL) {
		SceMp4_D2871F1C(mp4Demux);
		SceMp4_94E1305D(mp4Demux);
		mp4Demux = NULL;
	}
	if (defaultFileFd >= 0) {
		sceIoClose(defaultFileFd);
		defaultFileFd = -1;
	}
	return 0;
}
