/*
 * Sony FF4 (File Format 4) MP4 muxer library public C API.
 */
#ifndef FF4_API_H
#define FF4_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *Ff4Session;
typedef void *Ff4Writer;

typedef struct Ff4Callbacks {
	void *allocUser;
	int32_t (*ioRead)(void *fileCtx, void *buf, uint64_t size, uint64_t *outRead);
	int32_t (*ioWrite)(void *fileCtx, const void *buf, uint64_t size, uint64_t *outWritten);
	int32_t (*ioSeek)(void *fileCtx, int64_t offset, int32_t whence);
	int32_t (*ioTell)(void *fileCtx, uint64_t *outPos);
	int32_t (*ioGetSize)(void *fileCtx, uint64_t *outSize);
	void *(*memAlloc)(void *allocUser, uint64_t size);
	void (*memFree)(void *allocUser, void *ptr);
	int32_t (*strCmp)(void *allocUser, const char *s1, const char *s2, uint64_t n);
	char *(*strCpy)(void *allocUser, char *dst, uint64_t dstSize, const char *src, uint64_t n);
	int32_t (*memCmp)(void *allocUser, const void *p1, const void *p2, uint64_t n);
	void *(*memCpy)(void *allocUser, void *dst, uint64_t dstSize, const void *src, uint64_t n);
	void *(*memMove)(void *allocUser, void *dst, uint64_t dstSize, const void *src, uint64_t n);
	void *(*memSet)(void *allocUser, void *dst, int32_t val, uint64_t n);
} Ff4Callbacks;

typedef struct Ff4WriterInitParam {
	uint32_t maxMemorySize;
	uint32_t reserved[5];
	uint32_t modeFlags;
	uint32_t reserved2;
} Ff4WriterInitParam;

typedef struct Ff4FtypParam {
	uint32_t majorBrand;
	uint32_t minorVersion;
} Ff4FtypParam;

typedef struct Ff4ProfVideoParam {
	uint32_t trackIndex;
	uint32_t codecFourcc;
	uint32_t profileLevel;
	uint32_t reserved0;
	uint32_t avgBitrateKbps;
	uint32_t maxBitrateKbps;
	uint16_t fpsInt;
	uint16_t fpsFrac;
	uint16_t fpsInt2;
	uint16_t fpsFrac2;
	uint16_t width;
	uint16_t height;
	uint32_t aspectRatio;
} Ff4ProfVideoParam;

typedef struct Ff4ProfAudioParam {
	uint32_t trackIndex;
	uint32_t codecFourcc;
	uint32_t profileLevel;
	uint32_t reserved0;
	uint32_t avgBitrateKbps;
	uint32_t maxBitrateKbps;
	uint32_t sampleRate;
	uint32_t channels;
} Ff4ProfAudioParam;

typedef struct Ff4VideoTrackParam {
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t trackId;
	uint16_t width;
	uint16_t widthFrac;
	uint16_t height;
	uint16_t heightFrac;
	uint32_t timescale;
	uint16_t language;
	uint16_t pad;
	uint32_t flags;
} Ff4VideoTrackParam;

typedef struct Ff4AudioTrackParam {
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t trackId;
	uint32_t timescale;
	uint16_t language;
	uint16_t pad;
} Ff4AudioTrackParam;

typedef struct Ff4TrackAttrParam {
	uint32_t reserved[12];
	uint32_t matrix[9];
	uint32_t reserved2[3];
} Ff4TrackAttrParam;

typedef struct Ff4DataRefParam {
	uint32_t type;
	uint32_t flags;
} Ff4DataRefParam;

typedef struct Ff4VideoSampleDescParam {
	uint32_t codecType;
	uint32_t dataRefIndex;
	uint16_t width;
	uint16_t height;
	uint16_t horizResInt;
	uint16_t horizResFrac;
	uint16_t vertResInt;
	uint16_t vertResFrac;
} Ff4VideoSampleDescParam;

typedef struct Ff4AudioSampleDescParam {
	uint32_t dataRefIndex;
	uint16_t channelCount;
	uint16_t sampleSize;
} Ff4AudioSampleDescParam;

typedef struct Ff4EsdsParam {
	uint32_t objectType;
	uint32_t bufferSizeDb;
	uint32_t maxBitrate;
	uint32_t avgBitrate;
	const void *decoderSpecificInfo;
	uint32_t decoderSpecificInfoSize;
} Ff4EsdsParam;

typedef struct Ff4AvcConfigParam {
	uint8_t configurationVersion;
	uint8_t profileIndication;
	uint8_t profileCompatibility;
	uint8_t levelIndication;
	uint8_t lengthSizeMinusOne;
	uint8_t pad[3];
} Ff4AvcConfigParam;

typedef struct Ff4NalDataParam {
	const void *data;
	uint32_t size;
} Ff4NalDataParam;

typedef struct Ff4MetadataEntryParam {
	uint32_t type;
	uint8_t flag;
	uint8_t pad0;
	uint16_t lang;
	uint16_t encoding;
	uint16_t pad1;
	const void *data;
	uint32_t size;
} Ff4MetadataEntryParam;

typedef struct Ff4MetadataExtraParam {
	uint32_t type;
	uint16_t lang;
	uint16_t encoding;
	const void *data;
	uint32_t size;
} Ff4MetadataExtraParam;

typedef struct Ff4SampleEntry {
	uint32_t size;
	uint32_t reserved0;
	uint64_t offset;
	uint32_t duration;
	uint32_t isSync;
	uint32_t compOffset;
	uint32_t sampleDescIndex;
} Ff4SampleEntry;

typedef struct Ff4ThumbSampleDescParam {
	uint32_t type;
	uint32_t dataRefIndex;
	uint16_t width;
	uint16_t height;
	uint16_t horizResInt;
	uint16_t horizResFrac;
	uint16_t vertResInt;
	uint16_t vertResFrac;
} Ff4ThumbSampleDescParam;

typedef struct Ff4ThumbItemParam {
	uint32_t sampleDescIndex;
	uint32_t offset;
	uint32_t size;
} Ff4ThumbItemParam;

typedef struct Ff4ThumbGroupParam {
	uint32_t type;
	uint8_t flag;
	uint8_t pad0;
	uint16_t lang;
	uint32_t groupIndex;
} Ff4ThumbGroupParam;

/* FF4 API functions called by Mp4Muxer */
int32_t Ff4CreateSession(const Ff4Callbacks *cb, Ff4Session *outSession);
int32_t Ff4DestroySession(Ff4Session session);
int32_t Ff4ConvertDateToMp4Time(Ff4Session session, const uint16_t *dateFields, uint64_t *outMp4Time);
int32_t Ff4DestroyWriter(Ff4Writer writer);
int32_t Ff4CreateWriter(Ff4Session session, const Ff4WriterInitParam *param, Ff4Writer *outWriter);
int32_t Ff4SetFtyp(Ff4Writer writer, const Ff4FtypParam *param);
int32_t Ff4AddCompatibleBrand(Ff4Writer writer, uint32_t brand);
int32_t Ff4WriteFtyp(Ff4Writer writer, void *fileCtx, uint64_t offset, uint64_t *outWritten);
int32_t Ff4CreateProfileBox(Ff4Writer writer, uint32_t *outProfileId);
int32_t Ff4AddVideoProfile(Ff4Writer writer, const Ff4ProfVideoParam *param);
int32_t Ff4AddAudioProfile(Ff4Writer writer, const Ff4ProfAudioParam *param);
int32_t Ff4WriteProfileBox(Ff4Writer writer, void *fileCtx, uint64_t offset, uint64_t *outWritten);
int32_t Ff4SetMovieTimescale(Ff4Writer writer, uint32_t timescale);
int32_t Ff4WriteMoov(Ff4Writer writer, void *fileCtx, uint64_t offset, uint64_t *outWritten);
int32_t Ff4AddVideoTrack(Ff4Writer writer, const Ff4VideoTrackParam *param, uint32_t *outTrackHandle);
int32_t Ff4AddAudioTrack(Ff4Writer writer, const Ff4AudioTrackParam *param, uint32_t *outTrackHandle);
int32_t Ff4SetTrackAttribute(Ff4Writer writer, uint32_t trackHandle, uint32_t attrFlags, const void *param);
int32_t Ff4FinalizeTrack(Ff4Writer writer, uint32_t trackHandle, const uint32_t *param);
int32_t Ff4AddTrackDataRef(Ff4Writer writer, uint32_t trackHandle, const Ff4DataRefParam *param, uint32_t *outRefIndex);
int32_t Ff4AddVideoSampleDesc(Ff4Writer writer, uint32_t trackHandle, const Ff4VideoSampleDescParam *param, uint32_t *outDescIndex);
int32_t Ff4AddAudioSampleDesc(Ff4Writer writer, uint32_t trackHandle, const Ff4AudioSampleDescParam *param, uint32_t *outDescIndex);
int32_t Ff4SetAudioEsds(Ff4Writer writer, uint32_t trackHandle, uint32_t descIndex, const Ff4EsdsParam *param);
int32_t Ff4SetAvcConfigHeader(Ff4Writer writer, uint32_t trackHandle, uint32_t descIndex, const Ff4AvcConfigParam *param);
int32_t Ff4AddAvcSps(Ff4Writer writer, uint32_t trackHandle, uint32_t descIndex, const Ff4NalDataParam *param);
int32_t Ff4AddAvcPps(Ff4Writer writer, uint32_t trackHandle, uint32_t descIndex, const Ff4NalDataParam *param);
int32_t Ff4AddUsmtMetadata(Ff4Writer writer, uint32_t flags, const Ff4MetadataEntryParam *param);
int32_t Ff4AddUsmtExtraMetadata(Ff4Writer writer, uint32_t flags, const Ff4MetadataExtraParam *param);
int32_t Ff4CreateMtsdBox(Ff4Writer writer, uint32_t *outBoxId);
int32_t Ff4AddMtsdTrack(Ff4Writer writer, uint32_t boxId, uint32_t *outTrackId);
int32_t Ff4AddMtsdDataRef(Ff4Writer writer, uint32_t trackId, const Ff4DataRefParam *param, uint32_t *outRefIndex);
int32_t Ff4AddMtsdSampleDesc(Ff4Writer writer, uint32_t trackId, const Ff4ThumbSampleDescParam *param, uint32_t *outDescIndex);
int32_t Ff4SetMtsdSampleDescAttr(Ff4Writer writer, uint32_t trackId, uint32_t descIndex, uint32_t attrId, const void *attrData);
int32_t Ff4AddMtsdSample(Ff4Writer writer, uint32_t trackId, const Ff4ThumbItemParam *param, uint32_t *outSampleId);
int32_t Ff4AddMtsdGroup(Ff4Writer writer, const Ff4ThumbGroupParam *param, uint32_t *outGroupId);
int32_t Ff4LinkMtsdSample(Ff4Writer writer, uint32_t groupId, uint32_t trackId, uint32_t sampleId);
int32_t Ff4WriteMtsdHeader(Ff4Writer writer, void *fileCtx, uint64_t offset, uint64_t payloadSize, uint64_t *outHeaderSize);
int32_t Ff4SetCreationTime(Ff4Writer writer, uint32_t trackIndex, uint8_t flags,
			   uint64_t creationTime, uint64_t modTime);
int32_t Ff4WriteMdatHeader(Ff4Writer writer, void *fileCtx, uint64_t offset, uint32_t fourcc,
			   uint64_t payloadSize, uint64_t *outHeaderSize);
int32_t Ff4AddTrackSample(Ff4Writer writer, uint32_t trackHandle, const Ff4SampleEntry *sample, uint32_t flags);
int32_t Ff4FlushTrackChunk(Ff4Writer writer, uint32_t trackHandle);

#ifdef __cplusplus
}
#endif

#endif /* FF4_API_H */
