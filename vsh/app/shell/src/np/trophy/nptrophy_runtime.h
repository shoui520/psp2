/*
 * nptrophy_runtime.h: core runtime primitives for SceNpTrophy
 * (heap, mspace, sync, thread, work queue, commId, ticket, buffers, streams,
 * bitsets, grade weights, platform masks; 0x812E9978..0x812EB9FF, 0x812F0208..0x812F050F).
 */
#ifndef SHELL_NP_TROPHY_RUNTIME_H
#define SHELL_NP_TROPHY_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <kernel/openpsid.h>
#ifdef __cplusplus
}
#endif
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <rtc.h>
#include "../ipc/nptrophy/nptrophy_internal.h"

/* SceNpTrophy error codes */
#define NP_TROPHY_ERR_GENERIC			-2141907456	/* 0x80551600 */
#define NP_TROPHY_ERR_NOT_INITIALIZED		-2141907455	/* 0x80551601 */
#define NP_TROPHY_ERR_ALREADY_INITIALIZED	-2141907454	/* 0x80551602 */
#define NP_TROPHY_ERR_OUT_OF_MEMORY		-2141907453	/* 0x80551603 */
#define NP_TROPHY_ERR_INVALID_ARGUMENT		-2141907452	/* 0x80551604 */
#define NP_TROPHY_ERR_INSUFFICIENT_BUFFER	-2141907451	/* 0x80551605 */
#define NP_TROPHY_ERR_BUSY			-2141907448	/* 0x80551608 */
#define NP_TROPHY_ERR_INVALID_COMM_ID		-2141907446	/* 0x8055160A */
#define NP_TROPHY_ERR_INVALID_GROUP_ID		-2141907443	/* 0x8055160D */
#define NP_TROPHY_ERR_INVALID_TROPHY_ID		-2141907442	/* 0x8055160E */
#define NP_TROPHY_ERR_ACCOUNT_MISMATCH		-2141907439	/* 0x80551611 */
#define NP_TROPHY_ERR_ICON_NOT_FOUND		-2141907432	/* 0x80551618 */
#define NP_TROPHY_ERR_UNSUPPORTED_VERSION	-2141907427	/* 0x8055161D */
#define NP_TROPHY_ERR_BROKEN_CONF		-2141907428	/* 0x8055161C */
#define NP_TROPHY_ERR_QUEUE_STOPPED		-2141907327	/* 0x80551681 */
#define NP_TROPHY_ERR_QUEUE_ALREADY_RUNNING	-2141907328	/* 0x80551680 */
#define NP_TROPHY_ERR_JOB_NOT_FOUND		-2141907326	/* 0x80551682 */
#define NP_TROPHY_ERR_JOB_DUPLICATE		-2141907325	/* 0x80551683 */
#define NP_TROPHY_ERR_FILE_NOT_FOUND		-2141907324	/* 0x80551684 */
#define NP_TROPHY_ERR_FILE_NOT_OPEN		-2141907323	/* 0x80551685 */
#define NP_TROPHY_ERR_FILE_ALREADY_OPEN		-2141907322	/* 0x80551686 */
#define NP_TROPHY_ERR_UNEXPECTED_EOF		-2141907321	/* 0x80551687 */
#define NP_TROPHY_ERR_RECORD_BAD_MAGIC		-2141907320	/* 0x80551688 */
#define NP_TROPHY_ERR_RECORD_BAD_VERSION	-2141907319	/* 0x80551689 */
#define NP_TROPHY_ERR_RECORD_SECTION_NOT_FOUND	-2141907318	/* 0x8055168A */
#define NP_TROPHY_ERR_RECORD_NOT_OPEN		-2141907317	/* 0x8055168B */
#define NP_TROPHY_ERR_RECORD_TRUNCATED		-2141907316	/* 0x8055168C */
#define NP_TROPHY_ERR_INVALID_GRADE		-2141907299	/* 0x8055169D */
#define NP_TROPHY_ERR_PARSE_ERROR		-2141907284	/* 0x805516AC */
#define NP_TROPHY_ERR_BUFFER_NOT_SET		-2141907282	/* 0x805516AE */
#define NP_TROPHY_ERR_BUFFER_ALREADY_SET	-2141907281	/* 0x805516AF */
#define NP_TROPHY_ERR_STREAM_NOT_INIT		-2141907280	/* 0x805516B0 */
#define NP_TROPHY_ERR_NPID_EMPTY		-2141907277	/* 0x805516B3 */
#define NP_TROPHY_ERR_INVALID_FILENAME		-2141907271	/* 0x805516B9 */
#define NP_TROPHY_ERR_SUSPENDED			-2141907265	/* 0x805516BF */
#define NP_TROPHY_ERR_GROUP_UNSUPPORTED		-2141907262	/* 0x805516C2 */
#define NP_TROPHY_ERR_NOT_PLATINUM		-2141907261	/* 0x805516C3 */
#define NP_TROPHY_ERR_DIR_NOT_OPEN		-2141907248	/* 0x805516D0 */
#define NP_TROPHY_ERR_MEMFILE_LIMIT		-2141907247	/* 0x805516D1 */
#define NP_TROPHY_ERR_RECORD_ALREADY_OPEN	-2141907245	/* 0x805516D3 */
#define NP_TROPHY_ERR_URL_TOO_LONG		-2141907196	/* 0x80551704 */
#define NP_TROPHY_ERR_HTTP_POOL_FULL		-2141907175	/* 0x80551719 */
#define NP_TROPHY_ERR_HTTP_CONN_NOT_FOUND	-2141907174	/* 0x8055171A */

/* Platform bitmask values from TROPCONF.SFM platform attribute */
enum TrophyPlatformBits {
	TROPHY_PLATFORM_PS3	= 0x02,
	TROPHY_PLATFORM_PSP2	= 0x04,
	TROPHY_PLATFORM_PS4	= 0x08
};

/* Heap allocator callback table (0x815460C4) */
struct TrophyHeapAllocator {
	void *(*alloc)(SceSize size, void *userCtx);
	void *(*realloc)(void *ptr, SceSize size, void *userCtx);
	void (*free)(void *ptr, void *userCtx);
	void *userCtx;
};

/* Heap base class providing tagged heap-allocated new/delete */
class TrophyHeapObject {
public:
	virtual ~TrophyHeapObject() {}
	static void *operator new(size_t size);
	static void *operator new(size_t size, void *mem) { (void)size; return mem; }
	static void operator delete(void *ptr);
};

/* Mspace wrapper (0x812F0208..0x812F025A) */
class TrophyMspace {
public:
	TrophyMspace() : m_mspace(NULL) {}
	int Init(void *memblock, SceSize size);
	void Term();
	void *Malloc(SceSize size);
	void *Realloc(void *ptr, SceSize size);
	void Free(void *ptr);

private:
	void *m_mspace;
};

/* Global trophy heap helpers */
extern int g_npTrophyDebugMode;
int npTrophyMspaceInit(void *memblock, SceSize size);
void npTrophyMspaceTerm(void);
void *npTrophyHeapRealloc(void *ptr, SceSize size);

/* Atomic counter helpers (0x812EAF38..0x812EAF6F) */
unsigned int npTrophyAtomicInc(volatile unsigned int *counter);
unsigned int npTrophyAtomicDec(volatile unsigned int *counter);

/* Lightweight mutex wrapper (0x812EAF70..0x812EB10B) */
class TrophyLwMutex {
public:
	TrophyLwMutex();
	~TrophyLwMutex();
	int Init(const char *name, bool recursive);
	void Term();
	void Lock();
	void Unlock();
	SceKernelLwMutexWork *GetWork() { return &m_work; }

private:
	SceKernelLwMutexWork m_work;
};

/* Lightweight condition variable wrapper (0x812F027C..0x812F03B7) */
class TrophyLwCond {
public:
	TrophyLwCond();
	~TrophyLwCond();
	int Init(const char *name, TrophyLwMutex *mutex);
	void Term();
	int Wait(unsigned int *timeout);
	int Signal();

private:
	SceKernelLwCondWork m_work;
};

/* Thread base class (0x812F03B8..0x812F050F) */
class TrophyThread : public TrophyHeapObject {
public:
	TrophyThread();
	virtual ~TrophyThread();
	virtual int Run() = 0;

	int Create(const char *name, int priority, SceSize stackSize);
	int Start();
	int Join(int *outExitCode);
	void Delete();
	bool IsStarted() const { return m_started != 0; }

private:
	static int ThreadEntry(SceSize args, void *argp);

	SceUID m_threadId;
	uint8_t m_started;
	int m_exitCode;
};

/* Work queue state machine (0x812EB10C..0x812EB7BB) */
enum TrophyJobState {
	TROPHY_JOB_STATE_CREATED		= 1,
	TROPHY_JOB_STATE_QUEUED			= 3,
	TROPHY_JOB_STATE_RUNNING		= 4,
	TROPHY_JOB_STATE_CANCELLED_PENDING	= 5,
	TROPHY_JOB_STATE_FINISHED		= 6
};

class NpTrophyQueueWorker;

class NpTrophyQueue : public TrophyHeapObject {
public:
	NpTrophyQueue();
	virtual ~NpTrophyQueue();

	int Init(const char *name, int priority, SceSize stackSize);
	int Start();
	int Stop();
	void Shutdown();
	int Enqueue(int requestId, NpTrophyRuntimeJob *job);
	int CancelJob(int requestId, int cancelError);
	int CancelAll(int cancelError);

	int Dequeue(NpTrophyRuntimeJob **outJob);
	void CompleteJob(NpTrophyRuntimeJob *job, int result);

	void UnlinkJob(NpTrophyRuntimeJob *job);
	void MoveJobToFinished(NpTrophyRuntimeJob *job, int result);

private:
	NpTrophyRuntimeJob *FindJobLocked(int requestId);
	void DrainFinishedState(int targetState);

	TrophyLwMutex m_mutex;			/* +0x08 */
	TrophyLwCond m_cond;			/* +0x28 */
	NpTrophyRuntimeJob *m_pendingHead;	/* +0x38 */
	NpTrophyRuntimeJob *m_pendingTail;	/* +0x3C */
	NpTrophyRuntimeJob *m_runningHead;	/* +0x40 */
	NpTrophyRuntimeJob *m_runningTail;	/* +0x44 */
	NpTrophyRuntimeJob *m_finishedHead;	/* +0x48 */
	NpTrophyRuntimeJob *m_finishedTail;	/* +0x4C */
	NpTrophyQueueWorker *m_worker;		/* +0x50 */
	uint8_t m_stopping;			/* +0x54 */
};

class NpTrophyQueueWorker : public TrophyThread {
public:
	explicit NpTrophyQueueWorker(NpTrophyQueue *queue);
	virtual ~NpTrophyQueueWorker();
	virtual int Run();

private:
	NpTrophyQueue *m_queue;
	int m_unused;
	uint8_t m_exitRequested;
};

/* Communication ID helpers (0x812E9AEC..0x812E9D63) */
struct TrophyCommId {
	char data[12];

	TrophyCommId();
	void Clear();
	bool Equals(const TrophyCommId &other) const;
	bool IsEmpty() const;
	int ParseFromString(const char *str);
	bool EqualsString(const char *str) const;
	int FormatToString(char *outBuf, SceSize outSize) const;
	void CopyTo(void *dst) const;
};

/* NP environment & account / user helpers (0x812E9D64..0x812EA75F) */
int npTrophyGetNpEnvironment(char *outEnv);
bool npTrophyIsValidNpEnvironment(const char *env);

class TrophyHandle;

class TrophyTicketClient : public TrophyHeapObject {
public:
	TrophyTicketClient();
	virtual ~TrophyTicketClient();

	int Init();
	int GetTicket(TrophyHandle *handle, void *outBuf, SceSize *ioSize);
	int GetNpId(TrophyHandle *handle, SceNpId *outNpId);

	static int FetchTicketAlloc(TrophyHandle *handle, void **outBuf, SceSize *outSize);
	static void FreeTicketBuf(void *buf);
	static int FetchNpId(TrophyHandle *handle, SceNpId *outNpId);

private:
	int m_ticketCtxId;
};

int npTrophyGetLocalNpId(SceNpId *outNpId);
int npTrophyFormatNpIdJid(const SceNpId *npId, char *outBuf, SceSize outSize, bool stripOpt);
int npTrophyCopyNonEmptyNpId(const SceNpId *src, SceNpId *dst);
int npTrophyFormatRfc3339(char *outStr, SceSize outSize, const SceRtcTick *utc, int tzMinutes);
int npTrophyGetSystemLanguage(int *outLang);
int npTrophyGetOpenPsId(SceKernelOpenPsId *outPsId);
int npTrophyFormatOpenPsIdHex(const SceKernelOpenPsId *psId, char *outHex, SceSize outSize);
int npTrophyCopyOpenPsIdBytes(const SceKernelOpenPsId *psId, void *outBuf, SceSize outSize);
int npTrophyGetCurrentTick(SceRtcTick *outTick);

struct TrophyAccountCache {
	uint8_t loaded;
	uint64_t accountId;

	TrophyAccountCache() : loaded(0), accountId(0) {}
	int GetAccountId(uint64_t *outAccountId);
	int HasAccount(bool *outHasAccount);
	int VerifyMatchesAccount(uint64_t expectedAccountId);
};

int npTrophyGetUr0UserDir(int userId, char *outPath, SceSize outSize);
int npTrophyGetUx0UserDir(int userId, char *outPath, SceSize outSize);

/* Power lock RAII guard (0x812EA340..0x812EA38B) */
class TrophyPowerLock {
public:
	TrophyPowerLock() : m_locked(0) {}
	~TrophyPowerLock();
	int Lock();
	int Unlock();

private:
	uint8_t m_locked;
};

/* String & numeric formatting/parsing utilities (0x812EA38C..0x812EA633) */
SceSize npTrophyStrlcpy(char *dst, const char *src, SceSize dstSize);
SceSize npTrophyStrlcat(char *dst, const char *src, SceSize dstSize);
int npTrophyStrndup(char **outStr, const char *src, SceSize maxLen);
int npTrophyFormatUint(unsigned int value, char *outBuf, SceSize outSize, SceSize *outLen);
int npTrophyParseUintN(const char *str, SceSize maxLen, unsigned int *outValue);
int npTrophyParseUint(const char *str, unsigned int *outValue);
int npTrophyHexDigitValue(char ch);
int npTrophyBytesToHex(const uint8_t *src, SceSize srcLen, char *outHex, SceSize outSize,
		       bool uppercase);

/* Growable text/byte buffer (0x812EA760..0x812EA8DB) */
class TrophyGrowBuffer : public TrophyHeapObject {
public:
	TrophyGrowBuffer() : m_buffer(NULL), m_length(0), m_streamOffset(0) {}
	virtual ~TrophyGrowBuffer() { Clear(); }

	void Clear();
	int Append(const void *bytes, SceSize length);
	int AppendBounded(const void *bytes, SceSize length, SceSize maxLength);
	int MoveFrom(TrophyGrowBuffer *source);
	int CopyToString(char *outStr, SceSize outSize) const;
	void GetRange(uint64_t *outOffset, SceSize *outLength) const;

	const char *GetData() const { return m_buffer; }
	SceSize GetLength() const { return m_length; }
	void SetStreamOffset(uint64_t offset) { m_streamOffset = offset; }

private:
	char *m_buffer;
	SceSize m_length;
	uint64_t m_streamOffset;
};

/* Abstract byte stream interfaces */
class TrophyInputStream : public TrophyHeapObject {
public:
	virtual ~TrophyInputStream() {}
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead) = 0;
};

class TrophyOutputStream : public TrophyHeapObject {
public:
	virtual ~TrophyOutputStream() {}
	virtual int Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			  SceSize *outWritten) = 0;
};

class TrophyHashSink : public TrophyHeapObject {
public:
	virtual ~TrophyHashSink() {}
	virtual int Update(const void *bytes, SceSize size) = 0;
};

/* Buffered reader over a TrophyInputStream (0x812EA8DC..0x812EAE33) */
class TrophyReadBuffer {
public:
	TrophyReadBuffer();
	~TrophyReadBuffer();

	int Init(SceSize capacity);
	void Term();
	bool IsEmpty() const { return m_available == 0; }
	int Refill(TrophyHandle *handle, TrophyInputStream *stream);
	int RefillBounded(TrophyHandle *handle, TrophyInputStream *stream, SceSize maxBytes,
			  SceSize *outBytesRead);
	uint8_t PeekByte(SceSize offset) const { return m_cursor[offset]; }

	const uint8_t *GetCursor() const { return m_cursor; }
	SceSize GetAvailable() const { return m_available; }
	bool IsEof() const { return m_eof != 0; }
	uint64_t GetTotalRead() const { return m_totalRead; }
	void Consume(SceSize count)
	{
		m_cursor += count;
		m_available -= count;
	}

private:
	SceSize m_capacity;
	uint8_t *m_buffer;
	SceSize m_available;
	uint8_t *m_cursor;
	uint8_t m_eof;
	uint64_t m_totalRead;
};

/* Tee stream: reads from input, hashes, and optionally mirrors to output (0x812EAE34..0x812EAF37) */
class TrophyTeeStream : public TrophyInputStream {
public:
	TrophyTeeStream();
	virtual ~TrophyTeeStream() {}

	void Init(TrophyInputStream *input, TrophyOutputStream *output, TrophyHashSink *hashSink);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	int DrainAll(TrophyHandle *handle);
	uint64_t GetTotalBytes() const { return m_totalBytes; }

private:
	TrophyInputStream *m_input;
	TrophyOutputStream *m_output;
	uint64_t m_totalBytes;
	TrophyHashSink *m_hashSink;
};

/* Stream transfer utilities (0x812EAA1C..0x812EAD59) */
int npTrophyStreamReadHashed(TrophyInputStream *input, TrophyHandle *handle,
			     TrophyHashSink *hashSink, void *outBuf, SceSize size,
			     SceSize *outRead);
int npTrophyStreamReadExactHashed(TrophyInputStream *input, TrophyHandle *handle,
				  TrophyHashSink *hashSink, void *outBuf, SceSize size,
				  SceSize *outRead);
int npTrophyStreamReadExact(TrophyInputStream *input, TrophyHandle *handle, void *outBuf,
			    SceSize size, SceSize *outRead);
int npTrophyStreamSkipHashed(TrophyInputStream *input, TrophyHandle *handle,
			     TrophyHashSink *hashSink, int64_t byteCount, uint64_t *outSkipped);
int npTrophyStreamWriteExactHashed(TrophyOutputStream *output, TrophyHandle *handle,
				   TrophyHashSink *hashSink, const void *srcBuf, SceSize size,
				   SceSize *outWritten);
int npTrophyStreamWriteExact(TrophyOutputStream *output, TrophyHandle *handle,
			     const void *srcBuf, SceSize size, SceSize *outWritten);
int npTrophyStreamFillHashed(TrophyOutputStream *output, TrophyHandle *handle,
			     TrophyHashSink *hashSink, uint8_t fillByte, int64_t byteCount);
int npTrophyStreamFill(TrophyOutputStream *output, TrophyHandle *handle, uint8_t fillByte,
		       int64_t byteCount);

/* Trophy bitset, grade count, weight, and platform utilities (0x812EB7BC..0x812EBA17) */
struct TrophyGradeCounts {
	uint32_t platinum;
	uint32_t gold;
	uint32_t silver;
	uint32_t bronze;
};

struct TrophyGradeBitsets {
	NpTrophyFlagArray platinum;
	NpTrophyFlagArray gold;
	NpTrophyFlagArray silver;
	NpTrophyFlagArray bronze;
};

int npTrophyBitsetAnd(const NpTrophyFlagArray *left, const NpTrophyFlagArray *right,
		      NpTrophyFlagArray *outResult);
int npTrophyBitsetOr(const NpTrophyFlagArray *left, const NpTrophyFlagArray *right,
		     NpTrophyFlagArray *outResult);
int npTrophyBitsetCount(const NpTrophyFlagArray *bits);
uint32_t npTrophyGradeCountTotal(const TrophyGradeCounts *counts);
uint32_t npTrophyGradeWeightPoints(const TrophyGradeCounts *counts);
int npTrophyParsePlatformMask(const char *str, SceSize length, uint32_t *outMask);
int npTrophyCombineGradeBitsets(const TrophyGradeBitsets *gradeBits, NpTrophyFlagArray *outAll);
int npTrophyComputeProgressPercent(const TrophyGradeBitsets *gradeBits,
				   const TrophyGradeCounts *unlockedCounts);

#endif /* SHELL_NP_TROPHY_RUNTIME_H */
