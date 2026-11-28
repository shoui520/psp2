/*
 * nptrophy_runtime.cpp: core runtime primitives for SceNpTrophy
 * (0x812E9978..0x812EB9FF, 0x812F0208..0x812F050F).
 */
#include "nptrophy_runtime.h"
#include "nptrophy_io.h"

#include <sce_atomic.h>
#include <kernel/threadmgr.h>

extern "C" {
extern int sceAppMgrGetUserDirPath(int partitionId, char *path, SceSize size);
extern int sceKernelPowerLock(int type);
extern int sceKernelPowerUnlock(int type);
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *buf);
extern int sceRegMgrSystemParamGetInt(int id, int *buf);
extern int sceNpCommonGetNpEnviroment(char *environment);
extern int sceNpManagerGetNpId(SceNpId *npId);
extern int SceNpManager_DDF30B0E(void *loginId, void *onlineId, uint64_t *outAccountId);
extern int SceNpCommon_A1CF53F3(uint32_t lo, uint32_t hi, uint64_t otherAccountId);
extern int SceNpManager_8D7981D4(void);
extern int SceNpManager_80036A44(int ticketCtxId);
extern int SceNpManager_7CE94530(int ticketCtxId, void *outBuf, SceSize *ioSize);
extern int SceNpManager_82DA7CFD(int ticketCtxId, SceNpId *outNpId);
extern int SceNpManager_29C9B8A5(int ticketCtxId);
}

/* Global heap state (0x815460B8..0x815460D4, 0x815C383C) */
static int s_heapInitCount = 0;
int g_npTrophyDebugMode = 0;
static int s_mspaceRefCount = 0;
static TrophyMspace s_trophyMspace;

static TrophyHeapAllocator s_trophyHeapAllocator = {
	(void *(*)(SceSize, void *))npTrophyHeapAlloc,
	(void *(*)(void *, SceSize, void *))npTrophyHeapRealloc,
	(void (*)(void *, void *))npTrophyHeapFree,
	NULL
};

static const char *const s_validNpEnvironments[4] = {
	"rc",
	"e1-np",
	"e2-np",
	"np"
};

/* --- TrophyMspace (0x812F0208..0x812F025A) --- */

int TrophyMspace::Init(void *memblock, SceSize size)
{
	void *space = sceClibMspaceCreate(memblock, size);
	if (space == NULL)
		return NP_TROPHY_ERR_GENERIC;
	m_mspace = space;
	return 0;
}

void TrophyMspace::Term()
{
	if (m_mspace != NULL) {
		sceClibMspaceDestroy(m_mspace);
		m_mspace = NULL;
	}
}

void *TrophyMspace::Malloc(SceSize size)
{
	return sceClibMspaceMalloc(m_mspace, size);
}

void *TrophyMspace::Realloc(void *ptr, SceSize size)
{
	return sceClibMspaceRealloc(m_mspace, ptr, size);
}

void TrophyMspace::Free(void *ptr)
{
	sceClibMspaceFree(m_mspace, ptr);
}

/* --- Heap API (0x812E9978..0x812E9AEB) --- */

int npTrophyMspaceInit(void *memblock, SceSize size)
{
	if (s_mspaceRefCount == 0) {
		int err = s_trophyMspace.Init(memblock, size);
		if (err < 0)
			return err;
	}
	++s_mspaceRefCount;
	return 0;
}

void npTrophyMspaceTerm(void)
{
	if (s_mspaceRefCount != 0) {
		--s_mspaceRefCount;
		if (s_mspaceRefCount == 0)
			s_trophyMspace.Term();
	}
}

int npTrophyHeapInit(void *baseAddr, SceSize size)
{
	if (s_heapInitCount != 0)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;

	int err = npTrophyMspaceInit(baseAddr, size);
	if (err < 0)
		return err;

	if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "debug_mode", &g_npTrophyDebugMode) != 0)
		g_npTrophyDebugMode = 0;

	++s_heapInitCount;
	return 0;
}

int npTrophyHeapTerm(void)
{
	if (s_heapInitCount == 0)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	--s_heapInitCount;
	npTrophyMspaceTerm();
	return 0;
}

void *npTrophyHeapGetContext(void)
{
	return &s_trophyHeapAllocator;
}

void *npTrophyHeapAlloc(SceSize size)
{
	return s_trophyMspace.Malloc(size);
}

void *npTrophyHeapRealloc(void *ptr, SceSize size)
{
	return s_trophyMspace.Realloc(ptr, size);
}

void npTrophyHeapFree(void *ptr)
{
	s_trophyMspace.Free(ptr);
}

struct TrophyAllocHeader {
	TrophyHeapAllocator *allocator;
	uint32_t alignPadA;
	uint32_t alignPadB;
	uint32_t alignPadC;
};

void *npTrophyHeapAllocObj(SceSize size, void *heapCtx)
{
	TrophyHeapAllocator *allocator = static_cast<TrophyHeapAllocator *>(heapCtx);
	TrophyAllocHeader *header = static_cast<TrophyAllocHeader *>(
		allocator->alloc(size + sizeof(TrophyAllocHeader), allocator->userCtx));
	if (header == NULL)
		return NULL;
	header->allocator = allocator;
	return header + 1;
}

void npTrophyHeapFreeObj(void *ptr)
{
	if (ptr == NULL)
		return;
	TrophyAllocHeader *header = static_cast<TrophyAllocHeader *>(ptr) - 1;
	TrophyHeapAllocator *allocator = header->allocator;
	allocator->free(header, allocator->userCtx);
}

void *TrophyHeapObject::operator new(size_t size)
{
	return npTrophyHeapAllocObj(size, npTrophyHeapGetContext());
}

void TrophyHeapObject::operator delete(void *ptr)
{
	npTrophyHeapFreeObj(ptr);
}

/* --- TrophyCommId (0x812E9AEC..0x812E9D63) --- */

TrophyCommId::TrophyCommId()
{
	Clear();
}

void TrophyCommId::Clear()
{
	sceClibMemset(data, 0, sizeof(data));
}

bool TrophyCommId::Equals(const TrophyCommId &other) const
{
	if (sceClibMemcmp(data, other.data, 9) == 0)
		return static_cast<uint8_t>(data[10]) == static_cast<uint8_t>(other.data[10]);
	return false;
}

bool TrophyCommId::IsEmpty() const
{
	TrophyCommId emptyId;
	return emptyId.Equals(*this);
}

int TrophyCommId::ParseFromString(const char *str)
{
	SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
	Clear();
	TrophyCommId parsed;
	int err = npTrophyParseCommIdN(str, len, &parsed);
	if (err < 0)
		return err;
	sceClibMemcpy_safe(data, parsed.data, sizeof(data));
	return 0;
}

bool TrophyCommId::EqualsString(const char *str) const
{
	TrophyCommId candidate;
	if (candidate.ParseFromString(str) < 0)
		return false;
	return Equals(candidate);
}

int TrophyCommId::FormatToString(char *outBuf, SceSize outSize) const
{
	sceClibMemset(outBuf, 0, outSize);
	int err = npTrophyFormatCommId(this, outBuf, outSize);
	if (err < 0)
		return err;
	return 0;
}

void TrophyCommId::CopyTo(void *dst) const
{
	sceClibMemcpy_safe(dst, data, sizeof(data));
}

/* --- NP Environment & Ticket Client (0x812E9D64..0x812EA0E5) --- */

int npTrophyGetNpEnvironment(char *outEnv)
{
	int err = sceNpCommonGetNpEnviroment(outEnv);
	if (err < 0)
		return err;
	outEnv[16] = '\0';
	return 0;
}

bool npTrophyIsValidNpEnvironment(const char *env)
{
	for (unsigned int index = 0; index < 4; ++index) {
		const char *knownEnv = s_validNpEnvironments[index];
		SceSize len = sceClibStrnlen(knownEnv, 0x7FFFFFFFu);
		if (sceClibStrncmp(env, knownEnv, len + 1) == 0)
			return true;
	}
	return false;
}

class TrophyTicketCancelToken : public TrophyScopedCancelToken {
public:
	explicit TrophyTicketCancelToken(int ticketCtxId)
		: m_ticketCtxId(ticketCtxId)
	{
	}

	virtual ~TrophyTicketCancelToken() {}

protected:
	virtual void OnCancel(int errorCode)
	{
		(void)errorCode;
		SceNpManager_29C9B8A5(m_ticketCtxId);
	}

private:
	int m_ticketCtxId;
};

TrophyTicketClient::TrophyTicketClient()
	: m_ticketCtxId(-1)
{
}

TrophyTicketClient::~TrophyTicketClient()
{
	if (m_ticketCtxId != -1) {
		SceNpManager_80036A44(m_ticketCtxId);
		m_ticketCtxId = -1;
	}
}

int TrophyTicketClient::Init()
{
	int ctxId = SceNpManager_8D7981D4();
	if (ctxId < 0)
		return ctxId;
	m_ticketCtxId = ctxId;
	return 0;
}

int TrophyTicketClient::GetTicket(TrophyHandle *handle, void *outBuf, SceSize *ioSize)
{
	TrophyTicketCancelToken cancelToken(m_ticketCtxId);
	int err = cancelToken.Attach(handle);
	if (err < 0)
		return err;

	int ticketErr = SceNpManager_7CE94530(m_ticketCtxId, outBuf, ioSize);
	err = cancelToken.Detach(ticketErr);
	if (err < 0)
		return err;
	return 0;
}

int TrophyTicketClient::GetNpId(TrophyHandle *handle, SceNpId *outNpId)
{
	TrophyTicketCancelToken cancelToken(m_ticketCtxId);
	int err = cancelToken.Attach(handle);
	if (err < 0)
		return err;

	int npIdErr = SceNpManager_82DA7CFD(m_ticketCtxId, outNpId);
	err = cancelToken.Detach(npIdErr);
	if (err < 0)
		return err;
	return 0;
}

int TrophyTicketClient::FetchTicketAlloc(TrophyHandle *handle, void **outBuf, SceSize *outSize)
{
	TrophyTicketClient client;
	void *ticketBuf = npTrophyHeapAlloc(1536);
	if (ticketBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	SceSize ticketSize = 1536;
	int err = client.Init();
	if (err >= 0)
		err = client.GetTicket(handle, ticketBuf, &ticketSize);

	if (err < 0) {
		npTrophyHeapFree(ticketBuf);
		return err;
	}

	*outBuf = ticketBuf;
	*outSize = ticketSize;
	return 0;
}

void TrophyTicketClient::FreeTicketBuf(void *buf)
{
	npTrophyHeapFree(buf);
}

int TrophyTicketClient::FetchNpId(TrophyHandle *handle, SceNpId *outNpId)
{
	TrophyTicketClient client;
	int err = client.Init();
	if (err < 0)
		return err;
	return client.GetNpId(handle, outNpId);
}

/* --- System & Account Utilities (0x812EA0E6..0x812EA75F) --- */

int npTrophyGetLocalNpId(SceNpId *outNpId)
{
	int err = sceNpManagerGetNpId(outNpId);
	return (err < 0) ? err : 0;
}

int npTrophyFormatNpIdJid(const SceNpId *npId, char *outBuf, SceSize outSize, bool stripOpt)
{
	SceNpId copy;
	sceClibMemcpy_safe(&copy, npId, sizeof(copy));
	if (stripOpt) {
		copy.opt[4] = 0;
		copy.opt[5] = 0;
		copy.opt[6] = 0;
		copy.opt[7] = 0;
	}
	int err = npTrophyNpIdToJidString(&copy, outBuf, outSize);
	return (err < 0) ? err : 0;
}

int npTrophyCopyNonEmptyNpId(const SceNpId *src, SceNpId *dst)
{
	SceNpId zeroNpId;
	sceClibMemset(&zeroNpId, 0, sizeof(zeroNpId));
	if (sceClibMemcmp(src, &zeroNpId, sizeof(zeroNpId)) == 0)
		return NP_TROPHY_ERR_NPID_EMPTY;
	sceClibMemcpy_safe(dst, src, sizeof(SceNpId));
	return 0;
}

int npTrophyFormatRfc3339(char *outStr, SceSize outSize, const SceRtcTick *utc, int tzMinutes)
{
	if (outSize < 128)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	int err = sceRtcFormatRFC3339(outStr, utc, tzMinutes);
	if (err < 0)
		return err;
	outStr[outSize - 1] = '\0';
	return 0;
}

int npTrophyGetSystemLanguage(int *outLang)
{
	int regLang = 0;
	int err = sceRegMgrSystemParamGetInt(226562, &regLang);
	if (err < 0)
		return err;
	if (regLang < 0 || regLang > 19)
		return NP_TROPHY_ERR_GENERIC;
	*outLang = regLang;
	return 0;
}

int npTrophyGetOpenPsId(SceKernelOpenPsId *outPsId)
{
	int err = sceKernelGetOpenPsId(outPsId);
	return (err < 0) ? err : 0;
}

int npTrophyFormatOpenPsIdHex(const SceKernelOpenPsId *psId, char *outHex, SceSize outSize)
{
	if (outSize < 33)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	SceSize remaining = outSize;
	char *cursor = outHex;
	for (int index = 0; index < 16; ++index) {
		sceClibSnprintf(cursor, remaining, "%02x", psId->id[index]);
		cursor += 2;
		remaining -= 2;
	}
	return 0;
}

int npTrophyCopyOpenPsIdBytes(const SceKernelOpenPsId *psId, void *outBuf, SceSize outSize)
{
	if (outSize < 16)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	sceClibMemcpy_safe(outBuf, psId->id, 16);
	return 0;
}

int npTrophyGetCurrentTick(SceRtcTick *outTick)
{
	int err = sceRtcGetCurrentTick(outTick);
	return (err < 0) ? err : 0;
}

int TrophyAccountCache::GetAccountId(uint64_t *outAccountId)
{
	if (loaded == 0) {
		int err = SceNpManager_DDF30B0E(NULL, NULL, &accountId);
		if (err < 0)
			return err;
		loaded = 1;
	}
	*outAccountId = accountId;
	return 0;
}

int TrophyAccountCache::HasAccount(bool *outHasAccount)
{
	if (loaded == 0) {
		int err = SceNpManager_DDF30B0E(NULL, NULL, &accountId);
		if (err < 0)
			return err;
		loaded = 1;
	}
	*outHasAccount = (accountId != 0);
	return 0;
}

int TrophyAccountCache::VerifyMatchesAccount(uint64_t expectedAccountId)
{
	if (loaded == 0) {
		int err = SceNpManager_DDF30B0E(NULL, NULL, &accountId);
		if (err < 0)
			return err;
		loaded = 1;
	}
	if (accountId != 0) {
		if (expectedAccountId == 0)
			return 0;
		uint32_t lo = static_cast<uint32_t>(accountId & 0xFFFFFFFFu);
		uint32_t hi = static_cast<uint32_t>(accountId >> 32);
		if (SceNpCommon_A1CF53F3(lo, hi, expectedAccountId) == 0)
			return 0;
	} else if (expectedAccountId == 0) {
		return 0;
	}
	return NP_TROPHY_ERR_ACCOUNT_MISMATCH;
}

int npTrophyGetUr0UserDir(int userId, char *outPath, SceSize outSize)
{
	(void)userId;
	int err = sceAppMgrGetUserDirPath(1, outPath, outSize);
	return (err < 0) ? err : 0;
}

int npTrophyGetUx0UserDir(int userId, char *outPath, SceSize outSize)
{
	(void)userId;
	int err = sceAppMgrGetUserDirPath(2, outPath, outSize);
	return (err < 0) ? err : 0;
}

/* --- TrophyPowerLock (0x812EA340..0x812EA38B) --- */

TrophyPowerLock::~TrophyPowerLock()
{
	if (m_locked != 0)
		Unlock();
}

int TrophyPowerLock::Lock()
{
	int err = sceKernelPowerLock(0);
	if (err < 0)
		return err;
	m_locked = 1;
	return 1;
}

int TrophyPowerLock::Unlock()
{
	int err = sceKernelPowerUnlock(0);
	if (err < 0)
		return err;
	m_locked = 0;
	return 0;
}

/* --- String & Numeric Utilities (0x812EA38C..0x812EA633) --- */

SceSize npTrophyStrlcpy(char *dst, const char *src, SceSize dstSize)
{
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFu);
	if (dstSize != 0) {
		SceSize copyLen = (srcLen < dstSize) ? srcLen : (dstSize - 1);
		sceClibMemcpy_safe(dst, src, copyLen);
		dst[copyLen] = '\0';
	}
	return srcLen;
}

SceSize npTrophyStrlcat(char *dst, const char *src, SceSize dstSize)
{
	SceSize dstLen = sceClibStrnlen(dst, 0x7FFFFFFFu);
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFu);
	SceSize totalLen = dstLen + srcLen;
	if (dstSize != 0) {
		SceSize copyLen = (totalLen < dstSize) ? srcLen : (dstSize - dstLen - 1);
		sceClibMemcpy_safe(dst + dstLen, src, copyLen);
		dst[dstLen + copyLen] = '\0';
	}
	return totalLen;
}

int npTrophyStrndup(char **outStr, const char *src, SceSize maxLen)
{
	SceSize len = sceClibStrnlen(src, maxLen);
	char *copy = static_cast<char *>(npTrophyHeapAlloc(len + 1));
	if (copy == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	sceClibStrncpy(copy, src, len);
	copy[len] = '\0';
	*outStr = copy;
	return 0;
}

int npTrophyFormatUint(unsigned int value, char *outBuf, SceSize outSize, SceSize *outLen)
{
	SceSize digitCount = 0;
	unsigned int probe = value;
	do {
		++digitCount;
		probe /= 10u;
	} while (probe != 0);

	char *tail = outBuf;
	SceSize remSize = outSize;
	if (outSize > digitCount) {
		char *writePos = outBuf + digitCount - 1;
		do {
			*writePos-- = static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0);
		remSize = outSize - digitCount;
		tail = outBuf + digitCount;
	}
	if (remSize != 0)
		*tail = '\0';
	if (outLen != NULL)
		*outLen = digitCount;
	return (outSize <= digitCount) ? NP_TROPHY_ERR_INSUFFICIENT_BUFFER : 0;
}

int npTrophyParseUintN(const char *str, SceSize maxLen, unsigned int *outValue)
{
	unsigned int acc = 0;
	for (SceSize index = 0; index < maxLen; ++index) {
		char ch = str[index];
		if (ch == '\0')
			break;
		if (ch < '0' || ch > '9')
			return NP_TROPHY_ERR_PARSE_ERROR;
		unsigned int digit = static_cast<unsigned int>(ch - '0');
		if (acc > 429496729u || (acc == 429496729u && digit > 5u))
			return NP_TROPHY_ERR_PARSE_ERROR;
		acc = acc * 10u + digit;
	}
	*outValue = acc;
	return 0;
}

int npTrophyParseUint(const char *str, unsigned int *outValue)
{
	SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
	return npTrophyParseUintN(str, len, outValue);
}

int npTrophyHexDigitValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

int npTrophyBytesToHex(const uint8_t *src, SceSize srcLen, char *outHex, SceSize outSize,
		       bool uppercase)
{
	if (outSize < srcLen * 2 + 1)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	char alphaBase = uppercase ? ('A' - 10) : ('a' - 10);
	char *dst = outHex;
	for (SceSize index = 0; index < srcLen; ++index) {
		uint8_t hi = src[index] >> 4;
		uint8_t lo = src[index] & 0xFu;
		dst[0] = static_cast<char>((hi > 9) ? (hi + alphaBase) : (hi + '0'));
		dst[1] = static_cast<char>((lo > 9) ? (lo + alphaBase) : (lo + '0'));
		dst += 2;
	}
	*dst = '\0';
	return 0;
}

/* --- TrophyGrowBuffer (0x812EA760..0x812EA8DB) --- */

void TrophyGrowBuffer::Clear()
{
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_buffer = NULL;
		m_length = 0;
		m_streamOffset = 0;
	}
}

int TrophyGrowBuffer::Append(const void *bytes, SceSize length)
{
	if (bytes == NULL) {
		if (m_buffer != NULL)
			return NP_TROPHY_ERR_BUFFER_ALREADY_SET;
		m_length += length;
		return 0;
	}

	if (m_buffer != NULL) {
		char *grown = static_cast<char *>(npTrophyHeapRealloc(m_buffer, m_length + length + 1));
		if (grown == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		sceClibMemcpy_safe(grown + m_length, bytes, length);
		grown[m_length + length] = '\0';
		m_buffer = grown;
		m_length += length;
	} else {
		char *fresh = static_cast<char *>(npTrophyHeapAlloc(length + 1));
		if (fresh == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		sceClibMemcpy_safe(fresh, bytes, length);
		fresh[length] = '\0';
		m_buffer = fresh;
		m_length = length;
	}
	return 0;
}

int TrophyGrowBuffer::AppendBounded(const void *bytes, SceSize length, SceSize maxLength)
{
	if (m_length + length > maxLength)
		return NP_TROPHY_ERR_PARSE_ERROR;
	int err = Append(bytes, length);
	return (err < 0) ? err : 0;
}

int TrophyGrowBuffer::MoveFrom(TrophyGrowBuffer *source)
{
	if (m_buffer != NULL)
		return NP_TROPHY_ERR_BUFFER_ALREADY_SET;
	m_buffer = source->m_buffer;
	m_length = source->m_length;
	m_streamOffset = source->m_streamOffset;
	source->m_buffer = NULL;
	source->m_length = 0;
	source->m_streamOffset = 0;
	return 0;
}

int TrophyGrowBuffer::CopyToString(char *outStr, SceSize outSize) const
{
	if (m_buffer == NULL)
		return NP_TROPHY_ERR_BUFFER_NOT_SET;
	if (outSize < m_length + 1)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	sceClibMemcpy_safe(outStr, m_buffer, m_length);
	outStr[m_length] = '\0';
	return 0;
}

void TrophyGrowBuffer::GetRange(uint64_t *outOffset, SceSize *outLength) const
{
	*outOffset = m_streamOffset;
	*outLength = m_length;
}

/* --- TrophyReadBuffer (0x812EA8DC..0x812EAE33) --- */

TrophyReadBuffer::TrophyReadBuffer()
	: m_capacity(0), m_buffer(NULL), m_available(0), m_cursor(NULL), m_eof(0), m_totalRead(0)
{
}

TrophyReadBuffer::~TrophyReadBuffer()
{
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_buffer = NULL;
	}
}

int TrophyReadBuffer::Init(SceSize capacity)
{
	uint8_t *buf = static_cast<uint8_t *>(npTrophyHeapAlloc(capacity));
	if (buf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	m_capacity = capacity;
	m_buffer = buf;
	m_available = 0;
	m_cursor = buf;
	m_totalRead = 0;
	return 0;
}

void TrophyReadBuffer::Term()
{
	if (m_buffer != NULL) {
		npTrophyHeapFree(m_buffer);
		m_capacity = 0;
		m_buffer = NULL;
	}
	m_available = 0;
	m_cursor = NULL;
}

int TrophyReadBuffer::Refill(TrophyHandle *handle, TrophyInputStream *stream)
{
	if (m_available != 0)
		sceClibMemmove(m_buffer, m_cursor, m_available);
	m_cursor = m_buffer;
	while (m_available < m_capacity) {
		SceSize got = 0;
		int err = stream->Read(handle, m_buffer + m_available, m_capacity - m_available, &got);
		if (err < 0)
			return err;
		m_totalRead += got;
		m_available += got;
		if (got == 0) {
			m_eof = 1;
			return 0;
		}
	}
	return 0;
}

int TrophyReadBuffer::RefillBounded(TrophyHandle *handle, TrophyInputStream *stream,
				    SceSize maxBytes, SceSize *outBytesRead)
{
	if (m_available != 0)
		sceClibMemmove(m_buffer, m_cursor, m_available);
	m_cursor = m_buffer;

	SceSize space = m_capacity - m_available;
	if (maxBytes != 0 && maxBytes < space)
		space = maxBytes;

	SceSize totalGot = 0;
	while (space != 0) {
		SceSize got = 0;
		int err = stream->Read(handle, m_buffer + m_available, space, &got);
		if (err < 0)
			return err;
		space -= got;
		m_available += got;
		m_totalRead += got;
		totalGot += got;
		if (got == 0) {
			m_eof = 1;
			break;
		}
	}

	if (outBytesRead != NULL)
		*outBytesRead = totalGot;
	return 0;
}

/* --- Stream Helpers & Tee Stream (0x812EAA1C..0x812EAF37) --- */

int npTrophyStreamReadHashed(TrophyInputStream *input, TrophyHandle *handle,
			     TrophyHashSink *hashSink, void *outBuf, SceSize size,
			     SceSize *outRead)
{
	int err = input->Read(handle, outBuf, size, outRead);
	if (err < 0)
		return err;
	if (hashSink != NULL) {
		err = hashSink->Update(outBuf, *outRead);
		if (err < 0)
			return err;
	}
	return 0;
}

int npTrophyStreamReadExactHashed(TrophyInputStream *input, TrophyHandle *handle,
				  TrophyHashSink *hashSink, void *outBuf, SceSize size,
				  SceSize *outRead)
{
	uint8_t *dst = static_cast<uint8_t *>(outBuf);
	SceSize total = 0;
	int err = 0;

	while (total < size) {
		SceSize got = 0;
		err = npTrophyStreamReadHashed(input, handle, hashSink, dst + total,
					       size - total, &got);
		if (err < 0)
			break;
		total += got;
		if (got == 0) {
			if (total != size)
				err = NP_TROPHY_ERR_UNEXPECTED_EOF;
			break;
		}
	}

	if (outRead != NULL)
		*outRead = total;
	return err;
}

int npTrophyStreamReadExact(TrophyInputStream *input, TrophyHandle *handle, void *outBuf,
			    SceSize size, SceSize *outRead)
{
	return npTrophyStreamReadExactHashed(input, handle, NULL, outBuf, size, outRead);
}

int npTrophyStreamSkipHashed(TrophyInputStream *input, TrophyHandle *handle,
			     TrophyHashSink *hashSink, int64_t byteCount, uint64_t *outSkipped)
{
	uint8_t scratch[256];
	int64_t remaining = byteCount;
	while (remaining > 0) {
		SceSize chunk = (remaining >= 256) ? 256u : static_cast<SceSize>(remaining);
		SceSize got = 0;
		int err = npTrophyStreamReadExactHashed(input, handle, hashSink, scratch,
							chunk, &got);
		if (outSkipped != NULL)
			*outSkipped += got;
		if (err < 0)
			return err;
		remaining -= got;
	}
	return 0;
}

int npTrophyStreamWriteExactHashed(TrophyOutputStream *output, TrophyHandle *handle,
				   TrophyHashSink *hashSink, const void *srcBuf, SceSize size,
				   SceSize *outWritten)
{
	const uint8_t *src = static_cast<const uint8_t *>(srcBuf);
	SceSize total = 0;
	int err = 0;

	while (total < size) {
		SceSize wrote = 0;
		err = output->Write(handle, src + total, size - total, &wrote);
		if (err >= 0 && hashSink != NULL)
			err = hashSink->Update(src + total, wrote);
		if (err < 0)
			break;
		total += wrote;
	}

	if (outWritten != NULL)
		*outWritten = total;
	return err;
}

int npTrophyStreamWriteExact(TrophyOutputStream *output, TrophyHandle *handle,
			     const void *srcBuf, SceSize size, SceSize *outWritten)
{
	return npTrophyStreamWriteExactHashed(output, handle, NULL, srcBuf, size, outWritten);
}

int npTrophyStreamFillHashed(TrophyOutputStream *output, TrophyHandle *handle,
			     TrophyHashSink *hashSink, uint8_t fillByte, int64_t byteCount)
{
	uint8_t fillBuf[256];
	sceClibMemset(fillBuf, fillByte, sizeof(fillBuf));
	int64_t remaining = byteCount;
	while (remaining > 0) {
		SceSize chunk = (remaining <= 256) ? static_cast<SceSize>(remaining) : 256u;
		SceSize wrote = 0;
		int err = output->Write(handle, fillBuf, chunk, &wrote);
		if (err < 0)
			return err;
		if (hashSink != NULL) {
			err = hashSink->Update(fillBuf, wrote);
			if (err < 0)
				return err;
		}
		remaining -= wrote;
	}
	return 0;
}

int npTrophyStreamFill(TrophyOutputStream *output, TrophyHandle *handle, uint8_t fillByte,
		       int64_t byteCount)
{
	return npTrophyStreamFillHashed(output, handle, NULL, fillByte, byteCount);
}

TrophyTeeStream::TrophyTeeStream()
	: m_input(NULL), m_output(NULL), m_totalBytes(0), m_hashSink(NULL)
{
}

void TrophyTeeStream::Init(TrophyInputStream *input, TrophyOutputStream *output,
			   TrophyHashSink *hashSink)
{
	m_input = input;
	m_output = output;
	m_totalBytes = 0;
	m_hashSink = hashSink;
}

int TrophyTeeStream::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	int err = m_input->Read(handle, outBuf, size, outRead);
	if (err < 0)
		return err;
	m_totalBytes += *outRead;
	if (m_hashSink != NULL) {
		err = m_hashSink->Update(outBuf, *outRead);
		if (err < 0)
			return err;
	}
	if (m_output != NULL) {
		err = npTrophyStreamWriteExact(m_output, handle, outBuf, *outRead, NULL);
		if (err < 0)
			return err;
	}
	return 0;
}

int TrophyTeeStream::DrainAll(TrophyHandle *handle)
{
	void *chunkBuf = npTrophyHeapAlloc(0x10000);
	if (chunkBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int err = 0;
	for (;;) {
		SceSize got = 0;
		err = Read(handle, chunkBuf, 0x10000, &got);
		if (err < 0)
			break;
		if (got == 0) {
			err = 0;
			break;
		}
	}
	npTrophyHeapFree(chunkBuf);
	return err;
}

/* --- Atomic & Sync Primitives (0x812EAF38..0x812EB10B, 0x812F027C..0x812F050F) --- */

unsigned int npTrophyAtomicInc(volatile unsigned int *counter)
{
	return static_cast<unsigned int>(
		sceAtomicIncrement32AcqRel(reinterpret_cast<volatile int32_t *>(counter)));
}

unsigned int npTrophyAtomicDec(volatile unsigned int *counter)
{
	return static_cast<unsigned int>(
		sceAtomicDecrement32AcqRel(reinterpret_cast<volatile int32_t *>(counter)));
}

TrophyLwMutex::TrophyLwMutex()
{
	sceClibMemset(&m_work, 0, sizeof(m_work));
}

TrophyLwMutex::~TrophyLwMutex()
{
	Term();
}

int TrophyLwMutex::Init(const char *name, bool recursive)
{
	SceKernelLwMutexWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&m_work, &zeroWork, sizeof(m_work)) != 0)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;
	SceUInt32 attr = recursive ? (0x2000u | 0x0002u) : 0x2000u;
	int err = sceKernelCreateLwMutex(&m_work, name, attr, 0, NULL);
	return (err < 0) ? err : 0;
}

void TrophyLwMutex::Term()
{
	SceKernelLwMutexWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&m_work, &zeroWork, sizeof(m_work)) != 0) {
		sceKernelDeleteLwMutex(&m_work);
		sceClibMemset(&m_work, 0, sizeof(m_work));
	}
}

void TrophyLwMutex::Lock()
{
	if (sceKernelLockLwMutex(&m_work, 1, NULL) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(&m_work, &info);
	}
}

void TrophyLwMutex::Unlock()
{
	if (sceKernelUnlockLwMutex(&m_work, 1) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(&m_work, &info);
	}
}

TrophyLwCond::TrophyLwCond()
{
	sceClibMemset(&m_work, 0, sizeof(m_work));
}

TrophyLwCond::~TrophyLwCond()
{
	Term();
}

int TrophyLwCond::Init(const char *name, TrophyLwMutex *mutex)
{
	SceKernelLwCondWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&m_work, &zeroWork, sizeof(m_work)) != 0)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;
	int err = sceKernelCreateLwCond(&m_work, name, 0x2000u, mutex->GetWork(), NULL);
	return (err < 0) ? err : 0;
}

void TrophyLwCond::Term()
{
	SceKernelLwCondWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&m_work, &zeroWork, sizeof(m_work)) != 0) {
		sceKernelDeleteLwCond(&m_work);
		sceClibMemset(&m_work, 0, sizeof(m_work));
	}
}

int TrophyLwCond::Wait(unsigned int *timeout)
{
	int err = sceKernelWaitLwCond(&m_work, timeout);
	return (err < 0) ? err : 0;
}

int TrophyLwCond::Signal()
{
	int err = sceKernelSignalLwCond(&m_work);
	return (err < 0) ? err : 0;
}

/* --- TrophyThread (0x812F03B8..0x812F050F) --- */

TrophyThread::TrophyThread()
	: m_threadId(-1), m_started(0), m_exitCode(NP_TROPHY_ERR_GENERIC)
{
}

TrophyThread::~TrophyThread()
{
	Delete();
}

void TrophyThread::Delete()
{
	if (m_threadId != -1) {
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
}

int TrophyThread::ThreadEntry(SceSize args, void *argp)
{
	(void)args;
	TrophyThread *self = *static_cast<TrophyThread **>(argp);
	self->m_exitCode = self->Run();
	return 0;
}

int TrophyThread::Create(const char *name, int priority, SceSize stackSize)
{
	if (m_threadId != -1)
		return NP_TROPHY_ERR_ALREADY_INITIALIZED;
	SceUID tid = sceKernelCreateThread(name, ThreadEntry, priority, stackSize, 0, 0, NULL);
	if (tid < 0)
		return tid;
	m_threadId = tid;
	m_started = 0;
	return 0;
}

int TrophyThread::Start()
{
	TrophyThread *self = this;
	int err = sceKernelStartThread(m_threadId, sizeof(self), &self);
	if (err < 0)
		return err;
	m_started = 1;
	return 0;
}

int TrophyThread::Join(int *outExitCode)
{
	int err = sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	if (err < 0)
		return err;
	if (outExitCode != NULL)
		*outExitCode = m_exitCode;
	return 0;
}

/* --- NpTrophyRuntimeJob & NpTrophyQueue (0x812EB10C..0x812EB7BB) --- */

NpTrophyRuntimeJob::NpTrophyRuntimeJob(const char *name)
	: m_requestId(-1), m_name(name), m_state(TROPHY_JOB_STATE_CREATED),
	  m_next(NULL), m_prev(NULL), m_queue(NULL), m_cancelError(0)
{
}

NpTrophyRuntimeJob::~NpTrophyRuntimeJob()
{
}

int NpTrophyRuntimeJob::Cancel(int cancelError)
{
	if (m_state == TROPHY_JOB_STATE_QUEUED && m_queue != NULL)
		m_queue->MoveJobToFinished(this, cancelError);
	return m_state;
}

NpTrophyQueueWorker::NpTrophyQueueWorker(NpTrophyQueue *queue)
	: m_queue(queue), m_unused(0), m_exitRequested(0)
{
}

NpTrophyQueueWorker::~NpTrophyQueueWorker()
{
}

int NpTrophyQueueWorker::Run()
{
	while (m_exitRequested == 0) {
		NpTrophyRuntimeJob *job = NULL;
		if (m_queue->Dequeue(&job) < 0)
			break;
		int result = job->Run();
		m_queue->CompleteJob(job, result);
	}
	return 0;
}

NpTrophyQueue::NpTrophyQueue()
	: m_pendingHead(NULL), m_pendingTail(NULL),
	  m_runningHead(NULL), m_runningTail(NULL),
	  m_finishedHead(NULL), m_finishedTail(NULL),
	  m_worker(NULL), m_stopping(0)
{
}

NpTrophyQueue::~NpTrophyQueue()
{
	Shutdown();
}

void NpTrophyQueue::UnlinkJob(NpTrophyRuntimeJob *job)
{
	NpTrophyRuntimeJob **headPtr = NULL;
	NpTrophyRuntimeJob **tailPtr = NULL;

	if (job->m_state == TROPHY_JOB_STATE_QUEUED) {
		headPtr = &m_pendingHead;
		tailPtr = &m_pendingTail;
	} else if (job->m_state == TROPHY_JOB_STATE_RUNNING) {
		headPtr = &m_runningHead;
		tailPtr = &m_runningTail;
	} else if (job->m_state == TROPHY_JOB_STATE_CANCELLED_PENDING ||
		   job->m_state == TROPHY_JOB_STATE_FINISHED) {
		headPtr = &m_finishedHead;
		tailPtr = &m_finishedTail;
	} else {
		return;
	}

	if (job->m_next != NULL)
		job->m_next->m_prev = job->m_prev;
	else
		*tailPtr = job->m_prev;

	if (job->m_prev != NULL)
		job->m_prev->m_next = job->m_next;
	else
		*headPtr = job->m_next;

	job->m_next = NULL;
	job->m_prev = NULL;
}

void NpTrophyQueue::MoveJobToFinished(NpTrophyRuntimeJob *job, int result)
{
	UnlinkJob(job);
	if (job->m_state == TROPHY_JOB_STATE_RUNNING)
		job->m_state = TROPHY_JOB_STATE_FINISHED;
	else if (job->m_state == TROPHY_JOB_STATE_QUEUED)
		job->m_state = TROPHY_JOB_STATE_CANCELLED_PENDING;

	job->m_next = NULL;
	job->m_prev = m_finishedTail;
	if (m_finishedTail != NULL)
		m_finishedTail->m_next = job;
	if (m_finishedHead == NULL)
		m_finishedHead = job;
	m_finishedTail = job;
	job->m_cancelError = result;
}

NpTrophyRuntimeJob *NpTrophyQueue::FindJobLocked(int requestId)
{
	for (NpTrophyRuntimeJob *job = m_pendingHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	for (NpTrophyRuntimeJob *job = m_runningHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	for (NpTrophyRuntimeJob *job = m_finishedHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	return NULL;
}

void NpTrophyQueue::DrainFinishedState(int targetState)
{
	for (;;) {
		NpTrophyRuntimeJob *found = NULL;
		for (NpTrophyRuntimeJob *job = m_finishedHead; job != NULL; job = job->m_next) {
			if (job->m_state == targetState) {
				found = job;
				break;
			}
		}
		if (found == NULL)
			return;
		UnlinkJob(found);
		found->Finish(found->m_cancelError);
		delete found;
	}
}

int NpTrophyQueue::Init(const char *name, int priority, SceSize stackSize)
{
	int err = m_mutex.Init("SceNpTrophyWorkQueue", false);
	if (err < 0)
		return err;

	err = m_cond.Init("SceNpTrophyWorkQueue", &m_mutex);
	if (err >= 0) {
		m_worker = new NpTrophyQueueWorker(this);
		if (m_worker != NULL) {
			err = m_worker->Create(name, priority, stackSize);
			if (err >= 0)
				return 0;
			delete m_worker;
			m_worker = NULL;
		} else {
			err = NP_TROPHY_ERR_OUT_OF_MEMORY;
		}
		m_cond.Term();
	}
	m_mutex.Term();
	return err;
}

int NpTrophyQueue::Start()
{
	if (m_worker == NULL)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (m_worker->IsStarted())
		return NP_TROPHY_ERR_QUEUE_ALREADY_RUNNING;
	m_stopping = 0;
	int err = m_worker->Start();
	return (err < 0) ? err : 0;
}

int NpTrophyQueue::Stop()
{
	if (m_worker == NULL)
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	if (!m_worker->IsStarted())
		return NP_TROPHY_ERR_QUEUE_STOPPED;
	m_stopping = 1;
	m_cond.Signal();
	int err = m_worker->Join(NULL);
	return (err < 0) ? err : 0;
}

int NpTrophyQueue::CancelAll(int cancelError)
{
	m_mutex.Lock();
	for (NpTrophyRuntimeJob *job = m_pendingHead; job != NULL; ) {
		NpTrophyRuntimeJob *next = job->m_next;
		job->Cancel(cancelError);
		job = next;
	}
	for (NpTrophyRuntimeJob *job = m_runningHead; job != NULL; ) {
		NpTrophyRuntimeJob *next = job->m_next;
		job->Cancel(cancelError);
		job = next;
	}
	m_mutex.Unlock();

	DrainFinishedState(TROPHY_JOB_STATE_CANCELLED_PENDING);
	return 0;
}

void NpTrophyQueue::Shutdown()
{
	CancelAll(NP_TROPHY_ERR_GENERIC);
	if (m_worker != NULL) {
		if (m_worker->IsStarted()) {
			m_stopping = 1;
			m_cond.Signal();
			m_worker->Join(NULL);
		}
		m_worker->Delete();
		delete m_worker;
		m_worker = NULL;
	}
	m_cond.Term();
	m_mutex.Term();
}

int NpTrophyQueue::Enqueue(int requestId, NpTrophyRuntimeJob *job)
{
	if (m_stopping != 0)
		return NP_TROPHY_ERR_QUEUE_STOPPED;

	m_mutex.Lock();
	if (FindJobLocked(requestId) != NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_JOB_DUPLICATE;
	}

	job->m_queue = this;
	job->m_requestId = requestId;
	job->m_next = NULL;
	job->m_prev = m_pendingTail;
	job->m_state = TROPHY_JOB_STATE_QUEUED;
	if (m_pendingTail != NULL)
		m_pendingTail->m_next = job;
	if (m_pendingHead == NULL)
		m_pendingHead = job;
	m_pendingTail = job;
	m_mutex.Unlock();

	int err = m_cond.Signal();
	return (err < 0) ? err : 0;
}

int NpTrophyQueue::CancelJob(int requestId, int cancelError)
{
	m_mutex.Lock();
	NpTrophyRuntimeJob *job = FindJobLocked(requestId);
	if (job == NULL) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_JOB_NOT_FOUND;
	}
	if (job->m_state != TROPHY_JOB_STATE_FINISHED)
		job->Cancel(cancelError);
	m_mutex.Unlock();

	DrainFinishedState(TROPHY_JOB_STATE_CANCELLED_PENDING);
	return 0;
}

int NpTrophyQueue::Dequeue(NpTrophyRuntimeJob **outJob)
{
	m_mutex.Lock();
	NpTrophyRuntimeJob *job = NULL;
	while (m_stopping == 0) {
		job = m_pendingHead;
		if (job != NULL)
			break;
		int err = m_cond.Wait(NULL);
		if (err < 0) {
			m_mutex.Unlock();
			return err;
		}
	}
	if (m_stopping != 0) {
		m_mutex.Unlock();
		return NP_TROPHY_ERR_QUEUE_STOPPED;
	}

	UnlinkJob(job);
	job->m_state = TROPHY_JOB_STATE_RUNNING;
	job->m_next = NULL;
	job->m_prev = m_runningTail;
	if (m_runningTail != NULL)
		m_runningTail->m_next = job;
	if (m_runningHead == NULL)
		m_runningHead = job;
	m_runningTail = job;
	*outJob = job;
	m_mutex.Unlock();
	return 0;
}

void NpTrophyQueue::CompleteJob(NpTrophyRuntimeJob *job, int result)
{
	m_mutex.Lock();
	MoveJobToFinished(job, result);
	m_mutex.Unlock();

	DrainFinishedState(TROPHY_JOB_STATE_FINISHED);
}

/* C-style wrapper exports declared in nptrophy_internal.h */
NpTrophyQueue *npTrophyQueueConstruct(void *mem)
{
	return new (mem) NpTrophyQueue();
}

int npTrophyQueueTerm(NpTrophyQueue *queue)
{
	return queue->Stop();
}

int npTrophyQueueCancelAll(NpTrophyQueue *queue, int errorCode)
{
	return queue->CancelAll(errorCode);
}

int npTrophyQueueInit(NpTrophyQueue *queue, const char *name, SceUInt32 attr, SceSize stackSize)
{
	return queue->Init(name, static_cast<int>(attr), stackSize);
}

int npTrophyQueueStart(NpTrophyQueue *queue)
{
	return queue->Start();
}

int npTrophyQueueEnqueue(NpTrophyQueue *queue, int handleId, SceNpTrophyJobBase *job)
{
	return queue->Enqueue(handleId, job);
}

int npTrophyQueueCancelHandle(NpTrophyQueue *queue, int handleId, int cancelError)
{
	return queue->CancelJob(handleId, cancelError);
}

/* --- Bitset, Grade Weight & Platform Mask Helpers (0x812EB7BC..0x812EBA17) --- */

int npTrophyBitsetAnd(const NpTrophyFlagArray *left, const NpTrophyFlagArray *right,
		      NpTrophyFlagArray *outResult)
{
	int count = 0;
	for (int bit = 0; bit < 128; ++bit) {
		int wordIdx = bit >> 5;
		uint32_t mask = 1u << (bit & 31);
		if ((left->flagBits[wordIdx] & mask) != 0 &&
		    (right->flagBits[wordIdx] & mask) != 0) {
			if (outResult != NULL)
				outResult->flagBits[wordIdx] |= mask;
			++count;
		}
	}
	return count;
}

int npTrophyBitsetOr(const NpTrophyFlagArray *left, const NpTrophyFlagArray *right,
		     NpTrophyFlagArray *outResult)
{
	int count = 0;
	for (int bit = 0; bit < 128; ++bit) {
		int wordIdx = bit >> 5;
		uint32_t mask = 1u << (bit & 31);
		if ((left->flagBits[wordIdx] & mask) != 0 ||
		    (right->flagBits[wordIdx] & mask) != 0) {
			if (outResult != NULL)
				outResult->flagBits[wordIdx] |= mask;
			++count;
		}
	}
	return count;
}

int npTrophyBitsetCount(const NpTrophyFlagArray *bits)
{
	int count = 0;
	for (int bit = 0; bit < 128; ++bit) {
		if ((bits->flagBits[bit >> 5] & (1u << (bit & 31))) != 0)
			++count;
	}
	return count;
}

uint32_t npTrophyGradeCountTotal(const TrophyGradeCounts *counts)
{
	return counts->platinum + counts->gold + counts->silver + counts->bronze;
}

uint32_t npTrophyGradeWeightPoints(const TrophyGradeCounts *counts)
{
	return 90u * counts->gold + 30u * counts->silver + 15u * counts->bronze;
}

int npTrophyParsePlatformMask(const char *str, SceSize length, uint32_t *outMask)
{
	*outMask = 0;
	SceSize pos = 0;
	while (pos < length && str[pos] != '\0') {
		while (pos < length && str[pos] == ' ')
			++pos;
		if (pos >= length || str[pos] == '\0')
			break;

		const char *token = str + pos;
		if (sceClibMemcmp(token, "psp2", 4) == 0)
			*outMask |= TROPHY_PLATFORM_PSP2;
		else if (sceClibMemcmp(token, "ps3", 3) == 0)
			*outMask |= TROPHY_PLATFORM_PS3;
		else if (sceClibMemcmp(token, "ps4", 3) == 0)
			*outMask |= TROPHY_PLATFORM_PS4;

		while (pos < length && str[pos] != '\0') {
			char ch = str[pos++];
			if (ch == ',')
				break;
		}
	}
	return 0;
}

int npTrophyCombineGradeBitsets(const TrophyGradeBitsets *gradeBits, NpTrophyFlagArray *outAll)
{
	sceClibMemset(outAll, 0, sizeof(*outAll));
	npTrophyBitsetOr(&gradeBits->platinum, &gradeBits->gold, outAll);
	npTrophyBitsetOr(outAll, &gradeBits->silver, outAll);
	return npTrophyBitsetOr(outAll, &gradeBits->bronze, outAll);
}

int npTrophyComputeProgressPercent(const TrophyGradeBitsets *gradeBits,
				   const TrophyGradeCounts *unlockedCounts)
{
	TrophyGradeCounts totalCounts;
	totalCounts.platinum = static_cast<uint32_t>(npTrophyBitsetCount(&gradeBits->platinum));
	totalCounts.gold = static_cast<uint32_t>(npTrophyBitsetCount(&gradeBits->gold));
	totalCounts.silver = static_cast<uint32_t>(npTrophyBitsetCount(&gradeBits->silver));
	totalCounts.bronze = static_cast<uint32_t>(npTrophyBitsetCount(&gradeBits->bronze));

	uint32_t totalPoints = npTrophyGradeWeightPoints(&totalCounts);
	uint32_t earnedPoints = npTrophyGradeWeightPoints(unlockedCounts);
	if (totalPoints == 0 || earnedPoints == 0)
		return 0;
	if (totalPoints == earnedPoints)
		return 100;
	return static_cast<int>((100ULL * earnedPoints) / totalPoints);
}
