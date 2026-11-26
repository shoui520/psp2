#ifndef SCE_BEMP2SYS_INTERNAL_H
#define SCE_BEMP2SYS_INTERNAL_H

#include <stdarg.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <libkernel/libkernel.h>

/* --------------------------------------------------------------------------
 * Error codes
 * -------------------------------------------------------------------------- */

#define BE_MP2SYS_OK                              0
#define BE_MP2SYS_ERR_NOT_INITIALIZED             ((int32_t)0x80100001)
#define BE_MP2SYS_ERR_UNSUPPORTED_CMD             ((int32_t)0x80100002)
#define BE_MP2SYS_ERR_INVALID_STATE               ((int32_t)0x80100003)
#define BE_MP2SYS_ERR_OPEN_CALLBACK               ((int32_t)0x80100004)
#define BE_MP2SYS_ERR_CLOSE_CALLBACK              ((int32_t)0x80100005)
#define BE_MP2SYS_ERR_WRITER_MISMATCH             ((int32_t)0x80100006)
#define BE_MP2SYS_ERR_SECTION_RANGE               ((int32_t)0x80100007)
#define BE_MP2SYS_ERR_CORRUPT_DESCRIPTOR          ((int32_t)0x80100008)
#define BE_MP2SYS_ERR_TOO_LARGE                   ((int32_t)0x80100009)

#define BE_MP2SYS_ERR_BUFFER_END                  ((int32_t)0x81000001)
#define BE_MP2SYS_ERR_BITSTREAM_UNDERFLOW         ((int32_t)0x81000001)
#define BE_MP2SYS_ERR_BITSTREAM_SHORT             ((int32_t)0x81000002)
#define BE_MP2SYS_ERR_INVALID_ARG                 ((int32_t)0x81001001)
#define BE_MP2SYS_ERR_NOT_IMPLEMENTED             ((int32_t)0x81001002)
#define BE_MP2SYS_ERR_UNKNOWN_PARSER_STATE        ((int32_t)0x81002001)
#define BE_MP2SYS_ERR_MEMPOOL_EMPTY               ((int32_t)0x81003001)
#define BE_MP2SYS_ERR_INIT_FAILED                 ((int32_t)0x81004001)
#define BE_MP2SYS_ERR_ALLOC_FAILED                ((int32_t)0x81004001)

#define BE_MP2SYS_ERR_NO_SYNC_BYTE                ((int32_t)0x82000001)
#define BE_MP2SYS_ERR_TRANSPORT_ERROR             ((int32_t)0x82000002)
#define BE_MP2SYS_ERR_PARSER_INACTIVE             ((int32_t)0x82000003)

#define BE_MP2SYS_ERR_CRC_MISMATCH                ((int32_t)0x84000001)
#define BE_MP2SYS_ERR_PMT_POOL_FULL               ((int32_t)0x84000002)
#define BE_MP2SYS_ERR_PROGRAM_MISMATCH            ((int32_t)0x84000003)
#define BE_MP2SYS_ERR_PES_POOL_FULL               ((int32_t)0x84000004)
#define BE_MP2SYS_ERR_BAD_CA_DESCRIPTOR           ((int32_t)0x84000005)
#define BE_MP2SYS_ERR_NO_SECTION_BUF              ((int32_t)0x84000007)
#define BE_MP2SYS_ERR_TABLE_ID_MISMATCH           ((int32_t)0x84000008)

#define BE_MP2SYS_ERR_BAD_PES_START_CODE          ((int32_t)0x88000001)

/* Packetizer type identifiers */
#define BE_PACKETIZER_TYPE_PES                    0x10000001u
#define BE_PACKETIZER_TYPE_PAT                    0x10000002u
#define BE_PACKETIZER_TYPE_CAT                    0x10000004u
#define BE_PACKETIZER_TYPE_PMT                    0x10000008u
#define BE_PACKETIZER_TYPE_SI                     0x10000010u
#define BE_PACKETIZER_TYPE_TS                     0x10000020u

/* Payload parser type identifiers */
#define BE_PARSER_TYPE_NONE                       0
#define BE_PARSER_TYPE_PAT                        2
#define BE_PARSER_TYPE_PMT                        3
#define BE_PARSER_TYPE_CAT                        4
#define BE_PARSER_TYPE_PRIVATE                    5
#define BE_PARSER_TYPE_PES                        6

/* AV stream type constants */
#define BE_AV_STREAM_INVALID                      (-1)
#define BE_AV_STREAM_VIDEO_ANY                    0
#define BE_AV_STREAM_MPEG1_VIDEO                  1
#define BE_AV_STREAM_MPEG2_VIDEO                  2
#define BE_AV_STREAM_MPEG4_VIDEO                  3
#define BE_AV_STREAM_AVC_VIDEO                    4
#define BE_AV_STREAM_AUDIO_ANY                    10000
#define BE_AV_STREAM_AC3_AUDIO                    10001
#define BE_AV_STREAM_LPCM_AUDIO                   10002
#define BE_AV_STREAM_ATRAC3PLUS_AUDIO             10003
#define BE_AV_STREAM_ATRAC9_AUDIO                 10004
#define BE_AV_STREAM_MPEG1_AUDIO                  10005
#define BE_AV_STREAM_MPEG2_AUDIO                  10006
#define BE_AV_STREAM_MP3_AUDIO                    10007
#define BE_AV_STREAM_AAC_AUDIO                    10008
#define BE_AV_STREAM_DTS_AUDIO                    10009
#define BE_AV_STREAM_DDP_AUDIO                    10010
#define BE_AV_STREAM_CELP_AUDIO                   10011
#define BE_AV_STREAM_DTS_HD_LBR                   10012
#define BE_AV_STREAM_PRIVATE_SECTION              20000
#define BE_AV_STREAM_PRIVATE_DATA                 20001
#define BE_AV_STREAM_PES_DATA                     20002
#define BE_AV_STREAM_MPEG4_SUBTITLE               30000
#define BE_AV_STREAM_CLOSED_CAPTION               30001

/* --------------------------------------------------------------------------
 * C library / logging / allocator wrappers
 * -------------------------------------------------------------------------- */

typedef void *(*BeAllocFunc)(SceSize size);
typedef void *(*BeReallocFunc)(void *ptr, SceSize size);
typedef void (*BeFreeFunc)(void *ptr);
typedef int (*BeLogFunc)(const char *message);
typedef int (*BeStreamTypePredicate)(int32_t avStreamType);

void *beMemcpy(void *dst, const void *src, SceSize size);
void *beMemset(void *dst, int value, SceSize size);
int beMemcmp(const void *lhs, const void *rhs, SceSize size);
int beVsnprintf(char *buf, SceSize size, const char *fmt, va_list ap);

void *beMalloc(SceSize size);
void beFree(void *ptr);
void *beRealloc(void *ptr, SceSize size);
void beSetAllocator(BeAllocFunc allocFn, BeReallocFunc reallocFn, BeFreeFunc freeFn);

int beLogPrintf(int level, const char *fmt, ...);

extern BeLogFunc g_logFunc;
extern int32_t g_logLevel;
extern BeStreamTypePredicate g_isVideoStreamFunc;
extern BeStreamTypePredicate g_isAudioStreamFunc;
extern void *g_streamPredicateUserData;

extern int32_t g_defaultMaxPmtCount;
extern int32_t g_defaultMaxPesCount;
extern int32_t g_defaultMaxPrivateCount;

struct AvStreamTypeNameEntry {
	int32_t avStreamType;
	char name[20];
};

extern AvStreamTypeNameEntry g_avStreamTypeNames[22];

extern "C" {
int SceBemp2sys_CF2FD310(void);
}

uint32_t beCalcMpegCrc32(uint32_t initCrc, const uint8_t *data, int32_t length);

/* --------------------------------------------------------------------------
 * Base memory object
 * -------------------------------------------------------------------------- */

class BeMemObject {
public:
	BeMemObject();
	virtual ~BeMemObject();

	static void *operator new(SceSize size);
	static void *operator new[](SceSize size);
	static void operator delete(void *ptr);
	static void operator delete[](void *ptr);
};

/* --------------------------------------------------------------------------
 * BitReader
 * -------------------------------------------------------------------------- */

class BitReader {
public:
	const uint8_t *m_buffer;
	int32_t m_size;
	int32_t m_pos;

	BitReader();
	~BitReader();

	inline void setBuffer(const uint8_t *buf, int32_t size) {
		m_buffer = buf;
		m_size = size;
		m_pos = 0;
	}

	inline const uint8_t *currentPtr() const {
		if (m_pos < m_size)
			return m_buffer + m_pos;
		return 0;
	}

	inline int32_t advance(int32_t count) {
		if (m_pos + count > m_size)
			return BE_MP2SYS_ERR_BUFFER_END;
		m_pos += count;
		return BE_MP2SYS_OK;
	}

	inline int32_t advanceUnderflow(int32_t count) {
		if (m_pos + count > m_size)
			return BE_MP2SYS_ERR_BITSTREAM_UNDERFLOW;
		m_pos += count;
		return BE_MP2SYS_OK;
	}
};

/* --------------------------------------------------------------------------
 * Mutex and lightweight mutex wrappers
 * -------------------------------------------------------------------------- */

class BePrxMutex {
public:
	SceUID m_mutexId;
	SceUID *m_condIds;
	uint32_t m_condCount;
	uint8_t m_initialized;
	uint8_t m_locked;

	BePrxMutex();
	~BePrxMutex();

	int Initialize(uint32_t condCount);
	int Release();
	int Lock(uint32_t timeout);
	int TryLock();
	int Unlock();
	int Wait(uint32_t condIndex, uint32_t timeout);
	int Signal(uint32_t condIndex);
};

class BePrxMutexLight {
public:
	void *m_mutexRawMem;
	SceKernelLwMutexWork *m_lwMutex;
	SceKernelLwCondWork *m_lwConds;
	uint32_t m_condCount;
	uint8_t m_initialized;
	uint8_t m_locked;

	BePrxMutexLight();
	~BePrxMutexLight();

	int Initialize(uint32_t condCount);
	int Release();
	int Lock(uint32_t timeout);
	int TryLock();
	int Unlock();
	int Wait(uint32_t condIndex, uint32_t timeout);
	int Signal(uint32_t condIndex);
};

/* --------------------------------------------------------------------------
 * MemBlockPool
 * -------------------------------------------------------------------------- */

struct MemBlockNode {
	MemBlockNode *next;
	uint8_t *buffer;
};

class MemBlockPool : public BeMemObject {
public:
	MemBlockNode *m_freeList;
	MemBlockNode *m_usedList;
	int32_t m_totalCount;
	int32_t m_usedCount;
	int32_t m_growCount;
	int32_t m_blockSize;
	int32_t m_initialCount;
	int32_t m_maxFreeKeep;
	BePrxMutexLight m_mutex;

	MemBlockPool();
	virtual ~MemBlockPool();

	int Destroy();
	int Create(int32_t blockSize, int32_t initialCount, int32_t growCount, int32_t maxFreeKeep);
	int DestroyIfEmpty();
	MemBlockNode *GetBlock();
	int ReturnBlock(MemBlockNode *block);
	int IsValidBlock(MemBlockNode *block);
	uint8_t *GetBlockData(MemBlockNode *block);

private:
	int freeListNodes(MemBlockNode *head);
	__attribute__((noinline)) MemBlockNode *allocateNodes(int32_t count, int32_t blockSize);
	int trimExcessFreeBlocks();
};

/* --------------------------------------------------------------------------
 * Packetizer interfaces and concrete packetizers
 * -------------------------------------------------------------------------- */

typedef int (*PacketizerOpenCallback)(void *userData, uint32_t type);
typedef int (*PacketizerCloseCallback)(void *userData, uint32_t type);
typedef int (*PacketizerWriteCallback)(void *userData, uint32_t type, const uint8_t *buf, uint32_t size);
typedef int (*PacketizerOverwriteCallback)(void *userData, uint32_t type, uint32_t offset, const uint8_t *buf, uint32_t size);
typedef int (*SectionCheckCallback)(void *userData, uint32_t type, uint8_t sectionNum, uint8_t lastSectionNum,
	int32_t remainBytes, uint32_t pendingBytes, int32_t allowFlush);
typedef void (*SectionDoneCallback)(void *userData, uint32_t type, uint8_t sectionNum, uint8_t lastSectionNum,
	uint32_t sectionBytes);
typedef int (*TsPacketWrittenCallback)(void *userData, uint32_t type, uint16_t pid, uint8_t continuityCounter,
	uint32_t consumedBytes, uint32_t totalBytes);

struct PacketizerInitParam {
	void *userData;
	uint32_t type;
	PacketizerOpenCallback onOpen;
	PacketizerCloseCallback onClose;
	PacketizerWriteCallback onWrite;
	PacketizerOverwriteCallback onOverwrite;
};

struct SectionCallbacks {
	SectionCheckCallback onCheckSection;
	SectionDoneCallback onSectionDone;
};

struct SectionPacketizerInitParam {
	PacketizerInitParam base;
	SectionCallbacks sectionCallbacks;
};

struct PesPacketizerInitParam {
	PacketizerInitParam base;
	uint8_t streamId;
};

struct TsPacketizerInitParam {
	PacketizerInitParam base;
	TsPacketWrittenCallback onPacketWritten;
	uint16_t pid;
	uint8_t continuityCounter;
};

struct PacketizerPayloadParam {
	const uint8_t *buffer;
	uint32_t size;
};

struct CatHeaderParam {
	uint8_t versionNumber : 5;
	uint8_t currentNext : 1;
	uint32_t totalDescriptorSize;
};

struct PatHeaderParam {
	uint16_t transportStreamId;
	uint16_t networkPid;
	uint8_t versionNumber : 5;
	uint8_t currentNext : 1;
	uint32_t programCount;
};

struct PatProgramParam {
	uint16_t programNumber;
	uint16_t pmtPid;
};

struct PmtHeaderParam {
	uint8_t versionNumber : 5;
	uint8_t currentNext : 1;
	uint16_t programNumber;
	uint16_t pcrPid;
	uint32_t totalDataSize;
};

struct PmtDescriptorParam {
	const uint8_t *buffer;
	uint16_t size;
};

struct PmtElementaryParam {
	uint8_t streamType;
	uint16_t elementaryPid;
	const uint8_t *descriptors;
	uint16_t descriptorLength;
};

struct SiHeaderParam {
	uint8_t tableId;
	uint8_t sectionSyntaxIndicator : 1;
	uint8_t privateIndicator : 1;
	uint16_t tableIdExtension;
	uint8_t versionNumber : 5;
	uint8_t currentNext : 1;
	uint32_t totalDataSize;
};

struct PesHeaderParam {
	uint8_t flags;
	int32_t dtsHi;
	uint32_t dtsLo;
	int32_t ptsHi;
	uint32_t ptsLo;
};

struct TsHeaderParam {
	uint8_t payloadUnitStart : 1;
	uint8_t scramblingControl : 2;
	uint8_t discontinuityIndicator : 1;
	uint8_t randomAccessIndicator : 1;
	uint8_t pcrFlag : 1;
	uint16_t pcrExt;
	int32_t pcrBaseHi;
	uint32_t pcrBaseLo;
	uint8_t hasPointerField;
};

class BePacketizer : public BeMemObject {
public:
	void *m_userData;
	uint32_t m_type;
	PacketizerOpenCallback m_onOpen;
	PacketizerCloseCallback m_onClose;
	PacketizerWriteCallback m_onWrite;
	PacketizerOverwriteCallback m_onOverwrite;

	BePacketizer() {}
	virtual ~BePacketizer() {}
	virtual int Init(const void *param) = 0;
	virtual int Release() = 0;
	virtual int Process(int command, void *arg) = 0;
};

class BeSectionPacketizer : public BePacketizer {
public:
	SectionCheckCallback m_onCheckSection;
	SectionDoneCallback m_onSectionDone;
	uint32_t m_state;
	int32_t m_curPos;
	uint8_t m_sectionNumber;
	uint8_t m_lastSectionNumber;

	BeSectionPacketizer() {}
	virtual ~BeSectionPacketizer() {}

	inline void setSectionCallbacks(const SectionCallbacks *cb) {
		if (cb != 0) {
			m_onSectionDone = cb->onSectionDone;
			m_onCheckSection = cb->onCheckSection;
		}
	}
};

class CatPacketizer : public BeSectionPacketizer {
public:
	uint8_t m_versionNumber;
	uint8_t m_currentNext;
	uint8_t m_initialized;
	uint8_t m_sectionBuf[1024];

	CatPacketizer();
	virtual ~CatPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void MakeSectionHeader();
	int UpdateSectionLengthAndCRC();
	int ProcessCATAddDescriptors(const PacketizerPayloadParam *param);
	int ProcessCATHeader(const CatHeaderParam *param);
	int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
};

class PatPacketizer : public BeSectionPacketizer {
public:
	uint8_t m_versionNumber;
	uint16_t m_transportStreamId;
	uint16_t m_networkPid;
	uint8_t m_currentNext;
	uint8_t m_initialized;
	uint8_t m_sectionBuf[1024];

	PatPacketizer();
	virtual ~PatPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void MakeSectionHeader();
	int UpdateSectionLengthAndCRC();
	int ProcessPATAddProgram(const PatProgramParam *param);
	int ProcessPATHeader(const PatHeaderParam *param);
	int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
};

class PmtPacketizer : public BeSectionPacketizer {
public:
	uint8_t m_versionNumber;
	uint8_t m_currentNext;
	uint16_t m_programNumber;
	uint16_t m_pcrPid;
	uint8_t m_initialized;
	uint8_t m_sectionBuf[1024];

	PmtPacketizer();
	virtual ~PmtPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void MakeSectionHeader();
	int updateProgramInfoLength();
	int UpdateSectionLengthAndCRC();
	int ProcessPMTAddElementary(const PmtElementaryParam *param);
	int ProcessPMTAddDescriptors(const PmtDescriptorParam *param);
	int ProcessPMTHeader(const PmtHeaderParam *param);
	int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
};

class SiPacketizer : public BeSectionPacketizer {
public:
	uint8_t m_tableId;
	uint8_t m_sectionSyntaxIndicator;
	uint8_t m_privateIndicator;
	uint16_t m_tableIdExtension;
	uint8_t m_versionNumber;
	uint8_t m_currentNext;
	uint8_t m_initialized;
	uint8_t m_sectionBuf[4096];

	SiPacketizer();
	virtual ~SiPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void MakeSectionHeader();
	int UpdateSectionLengthAndCRC();
	int ProcessSIPayload(const PacketizerPayloadParam *param);
	int ProcessSIHeader(const SiHeaderParam *param);
	int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
};

class PesPacketizer : public BePacketizer {
public:
	uint32_t m_state;
	uint8_t m_streamId;
	int32_t m_writtenBytes;
	uint8_t m_initialized;

	PesPacketizer();
	virtual ~PesPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void UpdatePESLength();
	int ProcessPESHeader(const PesHeaderParam *param);
	__attribute__((noinline)) int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
	__attribute__((noinline)) int ProcessPayloadData(const PacketizerPayloadParam *param);
};

class TsPacketizer : public BePacketizer {
public:
	uint32_t m_state;
	int32_t m_packetPos;
	uint16_t m_pid;
	uint8_t m_continuityCounter;
	uint8_t m_payloadUnitStart;
	uint8_t m_scramblingControl;
	uint8_t m_discontinuityIndicator;
	uint8_t m_randomAccessIndicator;
	uint8_t m_pcrFlag;
	uint16_t m_pcrExt;
	int32_t m_pcrBaseHi;
	uint32_t m_pcrBaseLo;
	uint8_t m_hasPointerField;
	uint8_t m_initialized;
	TsPacketWrittenCallback m_onPacketWritten;

	TsPacketizer();
	virtual ~TsPacketizer();
	virtual int Init(const void *param);
	virtual int Release();
	virtual int Process(int command, void *arg);

	void clearPacketCallback();
	void setPacketCallback(const TsPacketWrittenCallback *cb);
	int notifyPacketWritten(uint32_t consumedBytes, uint32_t totalBytes);
	int ConstructTSHeader(uint8_t isFirstPacket, int32_t remainBytes);
	int ProcessPayloadData(const PacketizerPayloadParam *param);
	int ProcessTSHeader(const TsHeaderParam *param);
	int ProcessPacketClose(int32_t *outBytes);
	int ProcessPacketOpen(int32_t *outBytes);
};

BePacketizer *createPacketizer(uint32_t type);

/* --------------------------------------------------------------------------
 * SectionStorage
 * -------------------------------------------------------------------------- */

class TsDemuxer;

struct SectionSlot {
	int32_t length;
	MemBlockNode *block;
	uint8_t hasCrc;
	uint8_t valid;
};

struct SectionTableVersion {
	int32_t version;
	uint32_t sectionCount;
	uint32_t capacity;
	SectionSlot *sections;
};

class ISectionStorage {
public:
	virtual ~ISectionStorage() {}
	virtual int32_t GetCurrentVersion() = 0;
	virtual uint32_t GetCurrentVersionAndCount(int32_t *outVersion) = 0;
	virtual uint8_t *GetSectionBufOnCurVersion(uint32_t sectionNum, int32_t *outLength) = 0;
};

class SectionStorage : public BeMemObject, public ISectionStorage {
public:
	SectionTableVersion m_tables[2];
	int32_t m_currentIndex;
	TsDemuxer *m_demuxer;
	MemBlockPool *m_memPool;

	SectionStorage();
	virtual ~SectionStorage();

	virtual int32_t GetCurrentVersion();
	virtual uint32_t GetCurrentVersionAndCount(int32_t *outVersion);
	virtual uint8_t *GetSectionBufOnCurVersion(uint32_t sectionNum, int32_t *outLength);

	int Clear();
	int Init(TsDemuxer *demuxer);
	int SetSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
		const void *buf, int32_t length, uint8_t hasCrc);
	int SetSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
		MemBlockNode *block, int32_t length, uint8_t hasCrc);
	int AccumulateSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
		const void *buf, int32_t length, uint8_t hasCrc);
	int AccumulateSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
		MemBlockNode *block, int32_t length, uint8_t hasCrc);
	uint8_t *GetSectionBuf(int32_t version, uint32_t sectionNum, int32_t *outLength);
	uint8_t *GetSectionBufAccumulated(int32_t *outVersion, uint32_t sectionNum, int32_t *outLength);
	int IsAccumulatedSameAsCurrent();
	int ChangeCurrentVersion(int32_t version);
	int IsAccumulatedComplete();
	int DiscardAccumulated();

private:
	int releaseTableBlocks(SectionTableVersion *table, uint8_t keepArray);
	int MakeSectionArray(SectionTableVersion *table, int32_t count);
};

/* --------------------------------------------------------------------------
 * PID info, payload parser hierarchy, PES parser, TS parser, StreamInfo
 * -------------------------------------------------------------------------- */

class ITsPayloadParser;
class TsPayloadParser;
struct PidInfo;

class TsDemuxerHandle;
class StreamInfoHandle;

class IPesHostCallback {
public:
	virtual ~IPesHostCallback() {}
	virtual int reservedCheckA() = 0;
	virtual int reservedCheckB() = 0;
	virtual int reservedCheckC() = 0;
	virtual void *getUserData() = 0;
	virtual int isDemuxerHost() = 0;
};

class PesDemuxerHost : public IPesHostCallback {
public:
	TsDemuxerHandle *m_demuxerHandle;
};

struct StreamInfoHost {
	void *m_vtableOrContext;
	StreamInfoHandle *m_handle;
};

class IPesParserCallback {
public:
	virtual ~IPesParserCallback() {}
	virtual void onPesHeader(void *userData, void *pesInfo) = 0;
	virtual void onPesPayload(void *userData, void *pesInfo, const uint8_t *data, int32_t size) = 0;
};

class ISectionParserCallback {
public:
	virtual ~ISectionParserCallback() {}
	virtual void onSectionParsed(void *userData, int16_t pid, const uint8_t *buf, int32_t length,
		int32_t status, ISectionStorage *storage) = 0;
};

class ITsDemuxerEventCallback {
public:
	virtual ~ITsDemuxerEventCallback() {}
	virtual void onDemuxerEvent(int32_t eventType, PidInfo *pidInfo) = 0;
	virtual void onResolveStreamType(uint8_t streamType, int32_t *avStreamType) = 0;
	virtual void onResolveStreamTypeWithDesc(uint8_t streamType, int32_t *avStreamType,
		PidInfo *pidInfo, const uint8_t *descriptors, int32_t descLength, int32_t version) = 0;
};

class IStreamInfoSink {
public:
	virtual ~IStreamInfoSink() {}
	virtual int onPesHeader(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo) = 0;
	virtual int onPesPayload(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo,
		const uint8_t *data, int32_t size) = 0;
	virtual int onSection(uint16_t pid, const uint8_t *buf, int32_t length, int32_t status, ISectionStorage *storage) = 0;
	virtual int onTsPayload(PidInfo *pidInfo, const uint8_t *data, int32_t size) = 0;
	virtual void onTsPacket(PidInfo *pidInfo, const uint8_t *packet, int32_t packetSize,
		const uint8_t *payload, int32_t payloadSize);
};

class StreamTypeMap {
public:
	int32_t m_entries[256];

	StreamTypeMap() {
		for (int i = 0; i < 256; ++i)
			m_entries[i] = BE_AV_STREAM_INVALID;

		m_entries[0x01] = BE_AV_STREAM_MPEG1_VIDEO;
		m_entries[0x02] = BE_AV_STREAM_MPEG2_VIDEO;
		m_entries[0x03] = BE_AV_STREAM_MPEG1_AUDIO;
		m_entries[0x04] = BE_AV_STREAM_MPEG2_AUDIO;
		m_entries[0x05] = BE_AV_STREAM_PRIVATE_SECTION;
		m_entries[0x06] = BE_AV_STREAM_PRIVATE_DATA;
		m_entries[0x0F] = BE_AV_STREAM_AAC_AUDIO;
		m_entries[0x10] = BE_AV_STREAM_MPEG4_VIDEO;
		m_entries[0x11] = BE_AV_STREAM_AAC_AUDIO;
		m_entries[0x12] = BE_AV_STREAM_PES_DATA;
		m_entries[0x15] = BE_AV_STREAM_PES_DATA;
		m_entries[0x1B] = BE_AV_STREAM_AVC_VIDEO;
		m_entries[0x80] = BE_AV_STREAM_MPEG2_VIDEO;
		m_entries[0x81] = BE_AV_STREAM_AC3_AUDIO;
		m_entries[0x82] = BE_AV_STREAM_DTS_AUDIO;
		m_entries[0x86] = BE_AV_STREAM_DTS_AUDIO;
		m_entries[0x87] = BE_AV_STREAM_DDP_AUDIO;
		m_entries[0x8A] = BE_AV_STREAM_DTS_AUDIO;
	}
	virtual ~StreamTypeMap() {}
	inline int32_t &operator[](int idx) { return m_entries[idx]; }
	inline const int32_t &operator[](int idx) const { return m_entries[idx]; }
};

class ITsPayloadParser {
public:
	int32_t m_parserType;
	int32_t m_state;

	ITsPayloadParser() : m_parserType(0) {}
	virtual ~ITsPayloadParser() {}
	virtual int Init() = 0;
	virtual int Release() = 0;
	virtual int IsActive() = 0;
	virtual int32_t GetParserType();
	virtual void SetState(int32_t state);
};

class TsPayloadParser : public ITsPayloadParser, public BeMemObject {
public:
	TsPayloadParser *m_nextFree;
	BitReader *m_bitReader;
	TsDemuxer *m_demuxer;
	uint8_t m_active;

	TsPayloadParser() : m_nextFree(0), m_active(0) {
		m_parserType = 0;
		m_state = 0;
	}
	virtual ~TsPayloadParser() {}
	virtual int Init() {
		m_nextFree = 0;
		m_state = 0;
		m_active = 1;
		return 1;
	}
	virtual int Release() {
		m_active = 0;
		m_nextFree = 0;
		return 0;
	}
	virtual int IsActive() {
		return m_active;
	}
	virtual int Parse(BitReader *reader, TsDemuxer *demuxer, int32_t payloadLen) {
		(void)demuxer;
		if (reader->m_pos + payloadLen <= reader->m_size)
			reader->m_pos += payloadLen;
		return BE_MP2SYS_ERR_NOT_IMPLEMENTED;
	}
};

struct PidInfo {
	int32_t valid;
	int32_t programNumber;
	int16_t pid;
	int16_t pendingPid;
	int8_t continuityCounter;
	uint8_t payloadUnitStart;
	uint8_t scramblingControl;
	uint8_t hasAdaptationField;
	int16_t adaptationFieldLength;
	uint8_t randomAccessIndicator;
	uint64_t userTimestamp;
	const uint8_t *descriptorPtr;
	int32_t descriptorLength;
	uint8_t needsUpdate;
	TsPayloadParser *parser;
	PidInfo *childList;
	PidInfo *nextSibling;

	PidInfo() {
		reset();
	}

	void reset() {
		valid = 1;
		programNumber = 70000;
		pid = -1;
		pendingPid = -1;
		continuityCounter = -1;
		hasAdaptationField = 0;
		needsUpdate = 1;
		parser = 0;
		childList = 0;
		nextSibling = 0;
		descriptorPtr = 0;
		descriptorLength = 0;
		adaptationFieldLength = -1;
		randomAccessIndicator = 0;
	}
};

class SectionParser : public TsPayloadParser {
public:
	int32_t m_accumulatedBytes;
	int32_t m_sectionSize;
	uint8_t m_pointerField;
	uint8_t m_pointerSkipped;
	const uint8_t *m_sectionHeader;
	const uint8_t *m_sectionExtHeader;
	ISectionParserCallback *m_callback;
	SectionStorage m_storage;
	MemBlockNode *m_memBlock;

	SectionParser() {
		m_callback = 0;
		m_active = 0;
	}
	virtual ~SectionParser() {}
	virtual int Init() {
		TsPayloadParser::Init();
		m_storage.Clear();
		m_callback = 0;
		m_active = 1;
		return 1;
	}
	virtual int Release() {
		m_storage.Clear();
		m_active = 0;
		m_callback = 0;
		m_nextFree = 0;
		return 0;
	}
	virtual int Parse(BitReader *reader, TsDemuxer *demuxer, int32_t payloadLen);
	virtual int ParseSection() {
		return BE_MP2SYS_ERR_NOT_IMPLEMENTED;
	}

	__attribute__((noinline)) void logSectionHeader();
	int parseSectionHeader(BitReader *reader);
	void logSectionExtHeader();
	__attribute__((noinline)) int parseSectionExtHeader(BitReader *reader);
	int notifySectionAndReleaseBlock(int32_t status, const uint8_t *buf);
};

class PatSectionParser : public SectionParser {
public:
	int32_t m_programCount;

	PatSectionParser() {
		m_parserType = BE_PARSER_TYPE_PAT;
		m_programCount = 0;
	}
	virtual ~PatSectionParser() {}
	virtual int Init() {
		SectionParser::Init();
		m_programCount = 0;
		return 1;
	}
	virtual int Release() {
		SectionParser::Release();
		m_programCount = 0;
		return 0;
	}
	virtual int ParseSection();

	void logProgramEntry(const uint8_t *entry, int32_t index);
	int ParseTable();
};

class PmtSectionParser : public SectionParser {
public:
	int32_t m_esCount;
	int16_t m_pcrPid;

	PmtSectionParser() {
		m_parserType = BE_PARSER_TYPE_PMT;
	}
	virtual ~PmtSectionParser() {}
	virtual int Init() {
		SectionParser::Init();
		m_esCount = 0;
		m_pcrPid = -1;
		return 1;
	}
	virtual int Release() {
		SectionParser::Release();
		m_esCount = 0;
		m_pcrPid = -1;
		return 0;
	}
	virtual int ParseSection();

	void logElementaryStream(const uint8_t *entry, int32_t index);
	void logProgramInfo(const uint8_t *header);
	int ParseTable();
};

class CatSectionParser : public SectionParser {
public:
	CatSectionParser() {
		m_parserType = BE_PARSER_TYPE_CAT;
	}
	virtual ~CatSectionParser() {}
	virtual int Init() {
		SectionParser::Init();
		return 1;
	}
	virtual int Release() {
		SectionParser::Release();
		return 0;
	}
	virtual int ParseSection();

	int ParseTable();
};

class PrivateSectionParser : public SectionParser {
public:
	PrivateSectionParser() {
		m_parserType = BE_PARSER_TYPE_PRIVATE;
	}
	virtual ~PrivateSectionParser() {}
	virtual int Init() {
		SectionParser::Init();
		return 1;
	}
	virtual int Release() {
		m_callback = 0;
		int r = m_storage.Clear();
		m_callback = 0;
		m_nextFree = 0;
		m_active = 0;
		return r;
	}
	virtual int ParseSection();

	int commitPrivateVersion();
};

struct PesPacketInfo {
	uint8_t streamType;
	uint8_t streamId;
	int32_t avStreamType;
	int32_t ptsHi;
	uint32_t ptsLo;
	int32_t dtsHi;
	uint32_t dtsLo;
	uint32_t packetLength;
	uint32_t bytesRead;
	IPesParserCallback *callback;
};

class PesParser : public BeMemObject {
public:
	int32_t m_parserType;
	int32_t m_state;
	BitReader *m_bitReader;
	IPesHostCallback *m_host;
	uint8_t m_active;
	PesPacketInfo m_packetInfo;
	uint8_t m_headerBuf[264];
	int32_t m_headerPos;
	int32_t m_headerSize;
	uint8_t m_hasOptionalHeader;

	PesParser() {
		m_packetInfo.streamId = 0;
		m_packetInfo.callback = 0;
		m_packetInfo.streamType = 0xFF;
		m_state = 0;
		m_active = 0;
		m_packetInfo.avStreamType = BE_AV_STREAM_INVALID;
		m_parserType = BE_PARSER_TYPE_PES;
	}
	virtual ~PesParser() {}

	int Init();
	void Release();
	void logProgramPacketSeqCounter(const uint8_t *bytes);
	void logPesExtensionFlags(const uint8_t *flags);
	int parsePesExtension(BitReader *reader);
	int parseAdditionalCopyInfo(BitReader *reader);
	void logTrickModeFields(const uint8_t *bytePtr);
	int parseTrickMode(BitReader *reader);
	int parseEsRate(BitReader *reader);
	void logEscr(const uint8_t *bytes);
	int parseEscr(BitReader *reader);
	void storePtsOrDts(const uint8_t *bytes, uint8_t isPts);
	__attribute__((noinline)) void logPtsOrDts(const uint8_t *bytes, uint8_t isPts);
	int parsePtsDts(BitReader *reader, uint32_t ptsDtsFlags);
	void logPesOptionalHeader(const uint8_t *bytes);
	void logPesStartHeader(const uint8_t *bytes);
	int parseHeader();
	void updateOptionalHeaderFlag(int32_t streamId);
	int Parse(BitReader *reader, IPesHostCallback *host, int32_t payloadLen);
};

class PesPayloadParser : public TsPayloadParser {
public:
	PesParser m_pesParser;

	PesPayloadParser();
	virtual ~PesPayloadParser();
	virtual int Init();
	virtual int Release();
	virtual int IsActive();
	virtual int32_t GetParserType();
	virtual void SetState(int32_t state);
	virtual int Parse(BitReader *reader, TsDemuxer *demuxer, int32_t payloadLen);
	PesPacketInfo *getPesPacketInfo();
};

/* --------------------------------------------------------------------------
 * StreamInfo (Program/ES metadata & filter registry)
 * -------------------------------------------------------------------------- */

struct StreamFilterEntry {
	uint32_t mask;
	uint16_t pid;
	void *callback;

	StreamFilterEntry() : mask(0), pid(0xFFFF), callback(0) {}
};

struct EsStreamEntry {
	uint8_t streamType;
	uint16_t pid;
	int32_t avStreamType;

	EsStreamEntry() : pid(0xFFFF), avStreamType(BE_AV_STREAM_INVALID) {}
};

struct ProgramEntry {
	uint16_t programNumber;
	uint16_t pmtPid;
	int32_t esCapacity;
	int32_t esCount;
	uint8_t pmtComplete;
	EsStreamEntry *esList;

	ProgramEntry() : pmtPid(0xFFFF), esCapacity(0), esCount(0), pmtComplete(0), esList(0) {}
};

class StreamInfo : public BeMemObject, public IStreamInfoSink {
public:
	TsDemuxer *m_demuxer;
	StreamFilterEntry *m_filters;
	int32_t m_filterCapacity;
	int32_t m_filterCount;
	BePrxMutexLight m_mutex;
	ProgramEntry *m_programs;
	int32_t m_programCapacity;
	int32_t m_programCount;
	uint8_t m_patComplete;
	IStreamInfoSink *m_sink;

	StreamInfo();
	virtual ~StreamInfo();

	virtual int onPesHeader(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo);
	virtual int onPesPayload(uint16_t pid, int32_t ptsHi, uint32_t ptsLo, int32_t dtsHi, uint32_t dtsLo,
		const uint8_t *data, int32_t size);
	virtual int onSection(uint16_t pid, const uint8_t *buf, int32_t length, int32_t status, ISectionStorage *storage);
	virtual int onTsPayload(PidInfo *pidInfo, const uint8_t *data, int32_t size);
	virtual void onTsPacket(PidInfo *pidInfo, const uint8_t *packet, int32_t packetSize,
		const uint8_t *payload, int32_t payloadSize);

	void freePrograms();
	int Release();
	int Init(IStreamInfoSink *sink);

	int IsPatComplete();
	int GetProgramCount();
	int GetProgramList(int32_t maxCount, uint16_t *outPairs);
	int GetPmtPidByProgramNumber(uint16_t programNumber, uint16_t *outPmtPid);
	int IsPmtComplete(uint16_t pmtPid);
	int GetProgramNumberByPmtPid(uint16_t pmtPid, uint16_t *outProgramNumber);
	int GetEsCount(uint16_t pmtPid);
	int GetEsList(uint16_t pmtPid, int32_t maxCount, EsStreamEntry *outEntries);
	int GetEsAvStreamType(uint16_t pmtPid, uint16_t esPid, int32_t *outAvStreamType);
	const uint8_t *GetPidDescriptors(uint16_t pmtPid, uint16_t targetPid, int32_t *outDescLength);

	__attribute__((noinline)) int expandFilterTable();
	int SetPidFilter(uint16_t pid, uint32_t mask, void *callback);
	int AddPidFilterMask(uint16_t pid, uint32_t mask);
	int RemovePidFilterMask(uint16_t pid, uint32_t mask);
	int RemovePidFilter(uint16_t pid);
	void *FindPidFilter(uint16_t pid, uint32_t requiredMask);

	int StartNewPATParse(int32_t estimatedProgramCount);
	int AddProgram(uint16_t pmtPid, uint16_t programNumber);
	int FinishPATParse();
	int StartNewPMTParse(uint16_t pmtPid, int32_t estimatedEsCount);
	int AddElementaryStreamInfo(uint16_t pmtPid, uint16_t esPid, uint8_t streamType, int32_t avStreamType);
	int FinishPMTParse(uint16_t pmtPid);
};

/* --------------------------------------------------------------------------
 * TsParser & TsDemuxer
 * -------------------------------------------------------------------------- */

class TsParser : public BeMemObject {
public:
	BitReader *m_bitReader;
	TsDemuxer *m_demuxer;
	int32_t m_packetBytesRead;
	PidInfo *m_currentPidInfo;
	uint8_t m_hasTwoPcrs;
	uint64_t m_prevPcr;
	uint64_t m_prevPcrStreamPos;
	uint64_t m_curPcr;
	uint64_t m_curPcrStreamPos;
	uint64_t m_totalBytesParsed;
	int32_t m_bitrate;
	int16_t m_pcrPid;
	int32_t m_parseStartOffset;
	uint8_t m_hasExtraHeader;
	uint8_t m_extraHeaderBeforeSync;
	int32_t m_extraHeaderSize;

	TsParser();
	virtual ~TsParser();

	virtual int Init(TsDemuxer *demuxer);
	virtual void Release();
	virtual int IsInitialized();

	void resetPcrTracking();
	int parsePayload();
	void logSeamlessSplice(const uint8_t *bytes);
	void logLtw(const uint8_t *bytes);
	void logAdaptationExtHeader(const uint8_t *bytes);
	__attribute__((noinline)) void logPcrValue(const uint8_t *bytes, uint8_t isPcr);
	__attribute__((noinline)) void updateBitrate();
	void logAdaptationFlags(const uint8_t *flags);
	int parseAdaptationField();
	void logTsHeader(const uint8_t *header);
	int ParseHeader(uint64_t userTimestamp);
	int Parse(BitReader *reader, uint64_t userTimestamp);
};

class StreamInfoHandle;

class TsDemuxer : public BeMemObject {
public:
	StreamTypeMap m_streamTypeMap;
	MemBlockPool m_memBlockPool;
	uint8_t m_reservedState;
	ITsDemuxerEventCallback *m_eventCallback;
	void *m_userData;
	BitReader m_bitReader;
	TsParser m_tsParser;
	uint32_t m_packetCount;
	PidInfo m_patPidInfo;
	PidInfo m_catPidInfo;
	PidInfo *m_privatePidInfos;
	PidInfo *m_pmtPidInfos;
	PidInfo *m_pesPidInfos;
	PatSectionParser m_patParser;
	CatSectionParser m_catParser;
	PmtSectionParser *m_pmtParserPool;
	PrivateSectionParser *m_privateParserPool;
	PesPayloadParser *m_pesParserPool;
	TsPayloadParser *m_freePmtList;
	TsPayloadParser *m_freePrivateList;
	TsPayloadParser *m_freePesList;
	StreamInfo *m_streamInfo;
	int64_t m_ptsOffset;
	int32_t m_maxPmtCount;
	int32_t m_maxPesCount;
	int32_t m_maxPrivateCount;
	IPesHostCallback *m_pesHost;
	uint32_t m_flags;

	TsDemuxer();
	virtual ~TsDemuxer();

	int Release();
	int Init(IPesHostCallback *pesHost, void *userData, ITsDemuxerEventCallback *eventCallback,
		int32_t initialBlocks, int32_t growBlocks, int32_t maxFreeKeep);
	void resetHandles();
	int Parse(const uint8_t *buffer, int32_t size, int32_t *outConsumed, uint64_t userTimestamp);
	void cleanupRemovedPrograms();
	void cleanupRemovedStreams(PidInfo *pmtPidInfo);
	void attachStreamInfo(StreamInfoHost *host);

	inline PidInfo *findPidInfo(int16_t pid) {
		switch (pid) {
		case 0:
			return &m_patPidInfo;
		case 1:
			return &m_catPidInfo;
		default:
			break;
		}

		if (m_pmtPidInfos != 0) {
			for (int32_t i = 0; i < m_maxPmtCount; ++i) {
				int16_t entryPid = m_pmtPidInfos[i].needsUpdate ? m_pmtPidInfos[i].pendingPid : m_pmtPidInfos[i].pid;
				if (entryPid == pid)
					return &m_pmtPidInfos[i];
			}
		}

		if (m_pesPidInfos != 0) {
			for (int32_t i = 0; i < m_maxPesCount; ++i) {
				if (m_pesPidInfos[i].pid == pid)
					return &m_pesPidInfos[i];
			}
		}

		if (m_privatePidInfos != 0) {
			for (int32_t i = 0; i < m_maxPrivateCount; ++i) {
				if (m_privatePidInfos[i].pid == pid)
					return &m_privatePidInfos[i];
			}
		}
		return 0;
	}
};

/* --------------------------------------------------------------------------
 * Handle wrappers exposed through C API
 * -------------------------------------------------------------------------- */

class PesParserHandle : public BeMemObject {
public:
	PesParser *m_parser;

	PesParserHandle() {
		m_parser = new PesParser();
	}
	virtual ~PesParserHandle() {
		if (m_parser != 0)
			delete m_parser;
	}
};

class TsDemuxerHandle : public BeMemObject {
public:
	TsDemuxer *m_demuxer;

	TsDemuxerHandle() {
		m_demuxer = new TsDemuxer();
	}
	virtual ~TsDemuxerHandle() {
		if (m_demuxer != 0)
			delete m_demuxer;
	}
};

class StreamInfoHandle : public BeMemObject {
public:
	StreamInfo *m_streamInfo;

	StreamInfoHandle() {
		m_streamInfo = new StreamInfo();
	}
	virtual ~StreamInfoHandle() {
		if (m_streamInfo != 0)
			delete m_streamInfo;
	}
};

static inline int isVideoAvStreamType(int32_t avStreamType) {
	switch (avStreamType) {
	case BE_AV_STREAM_MPEG1_VIDEO:
	case BE_AV_STREAM_MPEG2_VIDEO:
	case BE_AV_STREAM_MPEG4_VIDEO:
	case BE_AV_STREAM_AVC_VIDEO:
		return 1;
	default:
		if (g_isVideoStreamFunc != 0)
			return g_isVideoStreamFunc(avStreamType);
		return 0;
	}
}

static inline int isAudioAvStreamType(int32_t avStreamType) {
	switch (avStreamType) {
	case BE_AV_STREAM_AC3_AUDIO:
	case BE_AV_STREAM_LPCM_AUDIO:
	case BE_AV_STREAM_ATRAC3PLUS_AUDIO:
	case BE_AV_STREAM_ATRAC9_AUDIO:
	case BE_AV_STREAM_MPEG1_AUDIO:
	case BE_AV_STREAM_MPEG2_AUDIO:
	case BE_AV_STREAM_MP3_AUDIO:
	case BE_AV_STREAM_AAC_AUDIO:
	case BE_AV_STREAM_DTS_AUDIO:
	case BE_AV_STREAM_DDP_AUDIO:
	case BE_AV_STREAM_CELP_AUDIO:
	case BE_AV_STREAM_DTS_HD_LBR:
		return 1;
	default:
		if (g_isAudioStreamFunc != 0)
			return g_isAudioStreamFunc(avStreamType);
		return 0;
	}
}

static inline PesPacketInfo *getParserPesInfo(TsPayloadParser *parser) {
	return static_cast<PesPayloadParser *>(parser)->getPesPacketInfo();
}

static inline PesParser *getParserPesObject(TsPayloadParser *parser) {
	return &static_cast<PesPayloadParser *>(parser)->m_pesParser;
}

#endif /* SCE_BEMP2SYS_INTERNAL_H */
