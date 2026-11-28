/*
 * nptrophy_trp.cpp: SceNpTrophy commSign verification, AppMgr TRP mounting,
 * TROPHY.TRP archive reader & stream, and TRP conf/asset source
 * (0x812CB694..0x812CC5EF, 0x812CC774..0x812CDB8B).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "nptrophy_service.h"

#define TROPHY_TRP_MAGIC		0xDCA24D00u
#define TROPHY_TRP_HEADER_SIZE		64u
#define TROPHY_TRP_ENTRY_SIZE		64u
#define TROPHY_TRP_SUPPORTED_VERSION	2u
#define TROPHY_COMMSIGN_MAGIC		0xB9DDA1BBu
#define TROPHY_COMMSIGN_DEV_KEY_ID	0x0000BEEFu

namespace {

/* 0x81545C28: SceNpTrophy commSign verification key (RSA-1024 + HMAC-SHA1) */
static const TrophySfmVerifyKeyEntry s_commSignKeyEntry = {
	0x00000100u,
	{
		0xE7, 0xF5, 0x9C, 0xF2, 0xAC, 0x00, 0x1C, 0x3C,
		0x57, 0x17, 0x1C, 0x82, 0x4D, 0x80, 0x0B, 0x57,
		0x5C, 0xBF, 0x4F, 0x37, 0x6A, 0xA9, 0x00, 0x5E,
		0x7E, 0x15, 0x61, 0xBB, 0xEA, 0xC3, 0x7E, 0x30,
		0x84, 0xD0, 0x86, 0xB6, 0x50, 0x0A, 0xB2, 0x53,
		0x5B, 0xBA, 0xAD, 0xEF, 0x28, 0xE5, 0xC5, 0xD4,
		0xC4, 0xAA, 0x02, 0x97, 0xE9, 0x2C, 0x69, 0xDE,
		0xA9, 0xA1, 0x94, 0x2D, 0xF4, 0x1E, 0xE5, 0x5F,
		0x34, 0xA1, 0xF4, 0x10, 0xDF, 0x8A, 0xEE, 0xC8,
		0x46, 0x4C, 0x44, 0x4E, 0x01, 0x18, 0x0D, 0x66,
		0xF9, 0x72, 0x75, 0x4E, 0x58, 0xBA, 0x7B, 0x34,
		0x5C, 0xA0, 0xED, 0x51, 0x04, 0x0E, 0xDE, 0x7F,
		0xE8, 0xC3, 0xAD, 0x04, 0x97, 0x31, 0x44, 0xD9,
		0x25, 0x1D, 0x2E, 0x94, 0x52, 0x4E, 0x57, 0x14,
		0xF9, 0x95, 0xEA, 0xD5, 0x1C, 0x2C, 0xD1, 0xEA,
		0x50, 0xEC, 0x08, 0x1F, 0x56, 0x7F, 0x8A, 0xCE
	},
	{
		0x01, 0x00, 0x01
	},
	{
		0xA8, 0xAB, 0xD9, 0xBC, 0x89, 0x46, 0x46, 0x2B,
		0xB9, 0xC8, 0x8D, 0x4C, 0x07, 0xED, 0x22, 0x8B,
		0x27, 0xA4, 0xE3, 0x67, 0x15, 0xB0, 0x42, 0x89,
		0x8F, 0xBC, 0xCD, 0xC4, 0x1A, 0x02, 0xE5, 0xB0,
		0xA5, 0xF5, 0xAC, 0xF7, 0x99, 0x12, 0x45, 0x58,
		0x97, 0x19, 0x36, 0x00, 0xBC, 0x66, 0x5F, 0x95,
		0xA5, 0xAA, 0x47, 0x84, 0x94, 0x4C, 0x48, 0x6A,
		0xAA, 0x75, 0x7D, 0xD0, 0x73, 0x1D, 0x28, 0xD7
	}
};

struct TrpDiskHeader {
	uint32_t magic;
	uint32_t version;
	uint64_t fileSize;
	uint32_t entryCount;
	uint32_t entrySize;
	uint32_t flags;
	uint8_t sha1[20];
	uint8_t reserved[16];
};

struct TrpDiskEntry {
	char name[32];
	uint64_t offset;
	uint64_t size;
	uint32_t entryKind;
	uint8_t reserved[12];
};

/* Owned stream wrapper exposing a TrophyTrpFileStream as TrophyConfSourceStream */
class TrophyTrpOwnedAssetStream : public TrophyConfSourceStream {
public:
	TrophyTrpOwnedAssetStream();
	virtual ~TrophyTrpOwnedAssetStream();

	TrophyTrpFileStream *GetTrpStream() { return &m_stream; }
	int GetSize(SceOff *outSize);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	virtual int CloseAndRelease();

private:
	TrophyTrpFileStream m_stream;
};

TrophyTrpOwnedAssetStream::TrophyTrpOwnedAssetStream()
{
}

TrophyTrpOwnedAssetStream::~TrophyTrpOwnedAssetStream()
{
	m_stream.Close();
}

/* 0x812CC3A4 */
int TrophyTrpOwnedAssetStream::GetSize(SceOff *outSize)
{
	int ret = m_stream.GetSize(outSize);
	return (ret >= 0) ? 0 : ret;
}

int TrophyTrpOwnedAssetStream::Read(TrophyHandle *handle, void *outBuf, SceSize size,
				    SceSize *outRead)
{
	return m_stream.Read(handle, outBuf, size, outRead);
}

int TrophyTrpOwnedAssetStream::CloseAndRelease()
{
	int ret = m_stream.Close();
	delete this;
	return ret;
}

/* Result holder for TRP archive extraction (0x812CCFC0, 0x812CD778) */
struct TrophyTrpExtractResult {
	TrophyConfHandle *defaultConfHandle;
	TrophyConfHandle *localizedConfHandle;

	TrophyTrpExtractResult();
	~TrophyTrpExtractResult();
	int TakeDefaultConf(TrophyConfData **outConf);
};

TrophyTrpExtractResult::TrophyTrpExtractResult()
	: defaultConfHandle(NULL), localizedConfHandle(NULL)
{
}

/* 0x812CCFC0 */
TrophyTrpExtractResult::~TrophyTrpExtractResult()
{
	if (defaultConfHandle != NULL)
		delete defaultConfHandle;
	if (localizedConfHandle != NULL)
		delete localizedConfHandle;
}

/* 0x812CD778 */
int TrophyTrpExtractResult::TakeDefaultConf(TrophyConfData **outConf)
{
	if (defaultConfHandle == NULL)
		return NP_TROPHY_ERR_GENERIC;
	*outConf = defaultConfHandle->DetachConfData();
	delete defaultConfHandle;
	defaultConfHandle = NULL;
	return 0;
}

/* 0x812CCFE0 */
static int shouldExtractTrpEntry(TrophyHandle *handle, TrophyTrpArchive *archive,
				 const TrophyExtractOptions *options, uint32_t entryIndex,
				 bool *outShouldExtract, TrophyAssetDesc *outDesc)
{
	TrophyTrpEntry entry;
	int ret = archive->GetEntryByIndex(entryIndex, &entry);
	if (ret < 0)
		return ret;

	ret = npTrophyParseAssetFilename(entry.name, outDesc);
	if (ret < 0)
		return ret;

	if (outDesc->IsDefaultLang()) {
		TrophyAssetDesc localizedDesc = *outDesc;
		localizedDesc.SetLang(options->lang);
		char localizedName[32];
		ret = npTrophyFormatAssetFilename(localizedName, &localizedDesc);
		if (ret < 0)
			return ret;

		TrophyTrpEntry foundEntry;
		int findRet = archive->FindEntryByName(localizedName, &foundEntry);
		if (findRet == NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND)
			*outShouldExtract = TrophyAssetExtractor::ShouldExtractAsset(entry.name,
										     options);
		else if (findRet < 0)
			return findRet;
		else
			*outShouldExtract = false;
	} else if (outDesc->HasSpecificLang()) {
		if (outDesc->IsValidLang() && outDesc->GetLang() == options->lang)
			*outShouldExtract = TrophyAssetExtractor::ShouldExtractAsset(entry.name,
										     options);
		else
			*outShouldExtract = false;
	} else {
		*outShouldExtract = TrophyAssetExtractor::ShouldExtractAsset(entry.name, options);
	}
	(void)handle;
	return 0;
}

/* 0x812CD2EA */
static int hashTrpEntryBytes(TrophyHandle *handle, TrophyTrpArchive *archive, uint32_t entryIndex,
			     TrophyHashSink *hashSink, uint64_t *ioTotalBytes)
{
	TrophyTrpFileStream entryStream;
	int ret = archive->OpenEntryStreamByIndex(entryIndex, &entryStream);
	if (ret >= 0) {
		TrophyTeeStream teeStream;
		teeStream.Init(&entryStream, NULL, hashSink);
		ret = teeStream.DrainAll(handle);
		if (ret >= 0)
			*ioTotalBytes += teeStream.GetTotalBytes();
	}
	if (entryStream.IsOpen())
		entryStream.Close();
	return ret;
}

/* 0x812CD394 */
static int extractTrpEntry(TrophyHandle *handle, TrophyTrpArchive *archive, uint32_t entryIndex,
			   const TrophyExtractOptions *options, TrophyHashSink *hashSink,
			   uint64_t *ioTotalBytes, TrophyAssetDesc *outDesc,
			   TrophyConfHandle **outConfHandle)
{
	TrophyTrpEntry entry;
	int ret = archive->GetEntryByIndex(entryIndex, &entry);
	if (ret < 0)
		return ret;

	TrophyTrpFileStream entryStream;
	TrophyAssetExtractor extractor;

	ret = archive->OpenEntryStreamByIndex(entryIndex, &entryStream);
	if (ret >= 0) {
		ret = extractor.Begin(entry.name, &entryStream, options, hashSink);
		if (ret >= 0) {
			ret = extractor.Process(handle, outDesc);
			if (ret >= 0) {
				*ioTotalBytes += extractor.GetBytesProcessed();
				if (outConfHandle != NULL)
					ret = extractor.TakeConfHandle(outConfHandle);
			}
		}
	}

	if (extractor.IsActive())
		extractor.End();
	if (entryStream.IsOpen())
		entryStream.Close();
	return ret;
}

static bool isAllZeroDigest(const TrophySha1Digest &digest)
{
	for (SceSize i = 0; i < sizeof(digest.bytes); ++i) {
		if (digest.bytes[i] != 0)
			return false;
	}
	return true;
}

static bool isValidTrpEntryName(const char *name)
{
	if (name == NULL || name[0] == '\0')
		return false;
	SceSize len = sceClibStrnlen(name, 32);
	return len > 0 && len < 32;
}

} /* namespace */

/* =========================================================================
 * CommSign verification (0x812CC4CC)
 * ========================================================================= */

int npTrophyVerifyCommSign(const TrophyCommId *commId, const void *commSignBuf)
{
	if (commId == NULL || commSignBuf == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophySfmSignature sig;
	sceClibMemcpy_safe(sig.bytes, commSignBuf, sizeof(sig.bytes));
	sig.length = sizeof(sig.bytes);

	uint32_t magic = 0;
	uint32_t keyId = 0;
	int ret = TrophySignatureVerifierCore::ParseHeader(&sig, &magic, &keyId);
	if (ret < 0)
		return NP_TROPHY_ERR_INVALID_COMM_SIGN_FORMAT;
	if (magic != TROPHY_COMMSIGN_MAGIC)
		return NP_TROPHY_ERR_INVALID_COMM_SIGN_FORMAT;

	if (keyId == TROPHY_COMMSIGN_DEV_KEY_ID) {
		bool requireSig = true;
		ret = npTrophyRequireSfmSignature(&requireSig);
		if (ret < 0)
			return ret;
		if (!requireSig)
			return 0;
		return NP_TROPHY_ERR_INVALID_COMM_SIGN;
	}

	if (keyId != s_commSignKeyEntry.keyId)
		return NP_TROPHY_ERR_INVALID_COMM_SIGN;

	char commStr[16];
	ret = npTrophyFormatCommId(commId, commStr, sizeof(commStr));
	if (ret < 0)
		return ret;

	TrophySignatureVerifierCore verifier;
	ret = verifier.Init(&s_commSignKeyEntry);
	if (ret < 0)
		return ret;

	ret = verifier.Update(commStr, 12);
	if (ret < 0)
		return ret;

	ret = verifier.Verify(&s_commSignKeyEntry, &sig);
	if (ret == NP_TROPHY_ERR_SIG_VERIFIER_MISSING)
		return NP_TROPHY_ERR_INVALID_COMM_SIGN_FORMAT;
	if (ret == NP_TROPHY_ERR_SIG_VERIFIER_FAILED || ret == NP_TROPHY_ERR_SIG_VERIFY_FAILED)
		return NP_TROPHY_ERR_INVALID_COMM_SIGN;
	return ret;
}

/* =========================================================================
 * TrophyMount (0x812CC774..0x812CC815)
 * ========================================================================= */

TrophyMount::TrophyMount()
	: m_mounted(0)
{
	sceClibMemset(m_mountPoint, 0, sizeof(m_mountPoint));
}

/* 0x812CC468 */
TrophyMount::~TrophyMount()
{
	Unmount();
}

/* 0x812CC4C0 */
const char *TrophyMount::GetMountPoint() const
{
	if (m_mounted != 0)
		return m_mountPoint;
	return NULL;
}

int TrophyMount::MountForPid(SceUID pid)
{
	if (IsMounted())
		return 0;

	char mountBuf[16];
	sceClibMemset(mountBuf, 0, sizeof(mountBuf));
	int ret = sceAppMgrTrophyMount(TROPHY_MOUNT_ID_GAME_TRP, pid, mountBuf);
	if (ret < 0)
		return ret;

	npTrophyStrlcpy(m_mountPoint, mountBuf, sizeof(m_mountPoint));
	m_mounted = 1;
	return 0;
}

void TrophyMount::Unmount()
{
	if (!IsMounted())
		return;
	if (sceAppMgrUmount(m_mountPoint) >= 0) {
		sceClibMemset(m_mountPoint, 0, sizeof(m_mountPoint));
		m_mounted = 0;
	}
}

/* =========================================================================
 * TrophyTrpFileStream (0x812CD778..0x812CDB8B)
 * ========================================================================= */

TrophyTrpFileStream::TrophyTrpFileStream()
	: m_archive(NULL),
	  m_entryOffset(0),
	  m_entrySize(0),
	  m_cursor(0)
{
}

TrophyTrpFileStream::~TrophyTrpFileStream()
{
	Close();
}

int TrophyTrpFileStream::OpenEntry(TrophyTrpArchive *archive, const TrophyTrpEntry *entry)
{
	if (archive == NULL || entry == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (IsOpen())
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;
	if (!archive->IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;
	if (archive->m_openStreamCount != 0)
		return NP_TROPHY_ERR_TRP_STREAM_BUSY;

	m_archive = archive;
	m_entryOffset = entry->offset;
	m_entrySize = entry->size;
	m_cursor = 0;
	archive->m_openStreamCount++;
	return 0;
}

/* 0x812CD8DA */
int TrophyTrpFileStream::OpenByName(TrophyHandle *handle, TrophyTrpArchive *archive,
				    const char *name)
{
	(void)handle;
	TrophyTrpEntry entry;
	int ret = archive->FindEntryByName(name, &entry);
	if (ret < 0) {
		if (ret == NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND)
			return NP_TROPHY_ERR_TRP_NOT_OPEN;
		return ret;
	}
	return OpenEntry(archive, &entry);
}

/* 0x812CDB8C */
bool TrophyTrpFileStream::IsOpen() const
{
	return m_archive != NULL;
}

int TrophyTrpFileStream::Close()
{
	if (m_archive == NULL)
		return 0;
	TrophyTrpArchive *archive = m_archive;
	m_archive = NULL;
	m_entryOffset = 0;
	m_entrySize = 0;
	m_cursor = 0;
	archive->NotifyStreamClosed();
	return 0;
}

int TrophyTrpFileStream::GetSize(SceOff *outSize)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (outSize != NULL)
		*outSize = static_cast<SceOff>(m_entrySize);
	return 0;
}

int TrophyTrpFileStream::Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (handle != NULL) {
		int cancelErr = handle->CheckCancel();
		if (cancelErr < 0)
			return cancelErr;
	}

	int64_t nextPos = 0;
	if (whence == SCE_SEEK_SET) {
		nextPos = offset;
	} else if (whence == SCE_SEEK_CUR) {
		nextPos = static_cast<int64_t>(m_cursor) + offset;
	} else if (whence == SCE_SEEK_END) {
		nextPos = static_cast<int64_t>(m_entrySize) + offset;
	} else {
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	}

	if (nextPos < 0 || static_cast<uint64_t>(nextPos) > m_entrySize)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	m_cursor = static_cast<uint64_t>(nextPos);
	if (outPos != NULL)
		*outPos = static_cast<SceOff>(m_cursor);
	return 0;
}

int TrophyTrpFileStream::Truncate(TrophyHandle *handle, SceOff length)
{
	(void)handle;
	(void)length;
	return NP_TROPHY_ERR_GENERIC;
}

int TrophyTrpFileStream::Sync(TrophyHandle *handle)
{
	(void)handle;
	return IsOpen() ? 0 : NP_TROPHY_ERR_FILE_NOT_OPEN;
}

int TrophyTrpFileStream::Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	if (outRead != NULL)
		*outRead = 0;
	if (size == 0)
		return 0;

	if (m_cursor >= m_entrySize)
		return 0;

	uint64_t remaining = m_entrySize - m_cursor;
	SceSize toRead = (remaining < static_cast<uint64_t>(size))
				 ? static_cast<SceSize>(remaining)
				 : size;

	SceOff targetOffset = static_cast<SceOff>(m_entryOffset + m_cursor);
	int ret = m_archive->m_file.Seek(handle, targetOffset, SCE_SEEK_SET, NULL);
	if (ret < 0)
		return ret;

	SceSize bytesRead = 0;
	ret = m_archive->m_file.Read(handle, outBuf, toRead, &bytesRead);
	if (ret < 0)
		return ret;

	m_cursor += bytesRead;
	if (outRead != NULL)
		*outRead = bytesRead;
	return 0;
}

int TrophyTrpFileStream::Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			       SceSize *outWritten)
{
	(void)handle;
	(void)srcBuf;
	(void)size;
	(void)outWritten;
	return NP_TROPHY_ERR_GENERIC;
}

/* =========================================================================
 * TrophyTrpArchive (0x812CC816..0x812CD777)
 * ========================================================================= */

TrophyTrpArchive::TrophyTrpArchive()
	: m_entries(NULL),
	  m_openStreamCount(0),
	  m_sha1Verified(0)
{
	sceClibMemset(&m_header, 0, sizeof(m_header));
}

TrophyTrpArchive::~TrophyTrpArchive()
{
	Close();
}

void TrophyTrpArchive::NotifyStreamClosed()
{
	if (m_openStreamCount > 0)
		m_openStreamCount--;
}

/* 0x812CC80E */
bool TrophyTrpArchive::IsOpen() const
{
	return m_file.IsOpen();
}

/* 0x812CCF18 */
int TrophyTrpArchive::Close()
{
	if (m_openStreamCount != 0)
		return NP_TROPHY_ERR_TRP_STREAM_BUSY;

	if (m_entries != NULL) {
		npTrophyHeapFree(m_entries);
		m_entries = NULL;
	}
	int ret = m_file.Close();
	sceClibMemset(&m_header, 0, sizeof(m_header));
	m_sha1Verified = 0;
	return ret;
}

int TrophyTrpArchive::ReadAndParseHeader(TrophyHandle *handle)
{
	SceOff actualSize = 0;
	int ret = m_file.GetSize(&actualSize);
	if (ret < 0)
		return ret;
	if (actualSize < static_cast<SceOff>(TROPHY_TRP_HEADER_SIZE))
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;

	ret = m_file.Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (ret < 0)
		return ret;

	TrpDiskHeader disk;
	SceSize bytesRead = 0;
	ret = npTrophyStreamReadExact(&m_file, handle, &disk, sizeof(disk), &bytesRead);
	if (ret < 0)
		return ret;
	if (bytesRead != sizeof(disk))
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;

	m_header.magic = sceNetNtohl(disk.magic);
	m_header.version = sceNetNtohl(disk.version);
	m_header.fileSize = sceNetNtohll(disk.fileSize);
	m_header.entryCount = sceNetNtohl(disk.entryCount);
	m_header.entrySize = sceNetNtohl(disk.entrySize);
	m_header.flags = sceNetNtohl(disk.flags);
	m_header.sha1.Set(disk.sha1, sizeof(disk.sha1));

	if (m_header.magic != TROPHY_TRP_MAGIC)
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;
	if (m_header.version != TROPHY_TRP_SUPPORTED_VERSION)
		return NP_TROPHY_ERR_TRP_UNSUPPORTED_VERSION;
	if (m_header.entrySize != TROPHY_TRP_ENTRY_SIZE)
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;
	if ((m_header.flags & ~1u) != 0)
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;
	if (m_header.fileSize != static_cast<uint64_t>(actualSize))
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;

	uint64_t tableEnd = TROPHY_TRP_HEADER_SIZE +
			    static_cast<uint64_t>(m_header.entryCount) * TROPHY_TRP_ENTRY_SIZE;
	if (tableEnd > m_header.fileSize)
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;

	return 0;
}

int TrophyTrpArchive::LoadEntryTable(TrophyHandle *handle)
{
	if (m_header.entryCount == 0)
		return 0;

	SceSize allocSize = static_cast<SceSize>(m_header.entryCount) * sizeof(TrophyTrpEntry);
	m_entries = static_cast<TrophyTrpEntry *>(npTrophyHeapAlloc(allocSize));
	if (m_entries == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	sceClibMemset(m_entries, 0, allocSize);

	int ret = m_file.Seek(handle, static_cast<SceOff>(TROPHY_TRP_HEADER_SIZE), SCE_SEEK_SET,
			      NULL);
	if (ret < 0)
		return ret;

	uint64_t tableEnd = TROPHY_TRP_HEADER_SIZE +
			    static_cast<uint64_t>(m_header.entryCount) * TROPHY_TRP_ENTRY_SIZE;

	for (uint32_t i = 0; i < m_header.entryCount; ++i) {
		TrpDiskEntry disk;
		SceSize bytesRead = 0;
		ret = npTrophyStreamReadExact(&m_file, handle, &disk, sizeof(disk), &bytesRead);
		if (ret < 0)
			return ret;
		if (bytesRead != sizeof(disk))
			return NP_TROPHY_ERR_TRP_INVALID_HEADER;

		TrophyTrpEntry &entry = m_entries[i];
		sceClibMemcpy_safe(entry.name, disk.name, sizeof(entry.name));
		entry.name[sizeof(entry.name) - 1] = '\0';
		entry.offset = sceNetNtohll(disk.offset);
		entry.size = sceNetNtohll(disk.size);
		entry.entryKind = sceNetNtohl(disk.entryKind);

		if (!isValidTrpEntryName(entry.name))
			return NP_TROPHY_ERR_TRP_INVALID_HEADER;
		if (entry.offset < tableEnd || entry.offset > m_header.fileSize)
			return NP_TROPHY_ERR_TRP_INVALID_HEADER;
		if (entry.size > m_header.fileSize - entry.offset)
			return NP_TROPHY_ERR_TRP_INVALID_HEADER;
	}

	return 0;
}

int TrophyTrpArchive::VerifyArchiveSha1(TrophyHandle *handle)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;
	if (m_sha1Verified != 0)
		return 0;
	if (m_openStreamCount != 0)
		return NP_TROPHY_ERR_TRP_STREAM_BUSY;

	bool requireSig = true;
	int ret = npTrophyRequireSfmSignature(&requireSig);
	if (ret < 0)
		return ret;

	if (isAllZeroDigest(m_header.sha1)) {
		if (!requireSig && (m_header.flags & 1u) != 0) {
			m_sha1Verified = 1;
			return 0;
		}
		return NP_TROPHY_ERR_TRP_HASH_MISMATCH;
	}

	TrophySha1Hasher hasher;
	ret = hasher.Init();
	if (ret < 0)
		return ret;

	ret = m_file.Seek(handle, 0, SCE_SEEK_SET, NULL);
	if (ret < 0)
		return ret;

	TrpDiskHeader hdrCopy;
	SceSize bytesRead = 0;
	ret = npTrophyStreamReadExact(&m_file, handle, &hdrCopy, sizeof(hdrCopy), &bytesRead);
	if (ret < 0)
		return ret;
	if (bytesRead != sizeof(hdrCopy))
		return NP_TROPHY_ERR_TRP_INVALID_HEADER;

	sceClibMemset(hdrCopy.sha1, 0, sizeof(hdrCopy.sha1));
	ret = hasher.Update(&hdrCopy, sizeof(hdrCopy));
	if (ret < 0)
		return ret;

	uint64_t remaining = m_header.fileSize - TROPHY_TRP_HEADER_SIZE;
	uint8_t *chunkBuf = static_cast<uint8_t *>(npTrophyHeapAlloc(4096));
	if (chunkBuf == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	while (remaining > 0) {
		SceSize chunk = (remaining < 4096u) ? static_cast<SceSize>(remaining) : 4096u;
		SceSize got = 0;
		ret = npTrophyStreamReadExactHashed(&m_file, handle, &hasher, chunkBuf, chunk,
						    &got);
		if (ret < 0) {
			npTrophyHeapFree(chunkBuf);
			return ret;
		}
		if (got != chunk) {
			npTrophyHeapFree(chunkBuf);
			return NP_TROPHY_ERR_TRP_INVALID_HEADER;
		}
		remaining -= got;
	}

	npTrophyHeapFree(chunkBuf);

	TrophySha1Digest computed;
	ret = hasher.Final(&computed);
	if (ret < 0)
		return ret;

	if (!computed.Equals(m_header.sha1))
		return NP_TROPHY_ERR_TRP_HASH_MISMATCH;

	m_sha1Verified = 1;
	return 0;
}

int TrophyTrpArchive::Open(TrophyHandle *handle, const char *trpPath, bool verifySha1)
{
	if (trpPath == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	if (IsOpen())
		return NP_TROPHY_ERR_TRP_ALREADY_OPEN;

	int ret = m_file.Open(trpPath, SCE_O_RDONLY, 0);
	if (ret < 0) {
		if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
			return NP_TROPHY_ERR_TRP_NOT_FOUND;
		return ret;
	}

	ret = ReadAndParseHeader(handle);
	if (ret >= 0)
		ret = LoadEntryTable(handle);
	if (ret >= 0 && verifySha1)
		ret = VerifyArchiveSha1(handle);

	if (ret < 0) {
		Close();
		return ret;
	}
	return 0;
}

int TrophyTrpArchive::GetEntryByIndex(uint32_t index, TrophyTrpEntry *outEntry) const
{
	if (!IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;
	if (index >= m_header.entryCount || outEntry == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outEntry = m_entries[index];
	return 0;
}

/* 0x812CCF78 */
int TrophyTrpArchive::GetEntryPtr(uint32_t index, const TrophyTrpEntry **outEntry) const
{
	if (index >= m_header.entryCount)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	*outEntry = &m_entries[index];
	return 0;
}

/* 0x812CCBB6 */
int TrophyTrpArchive::EnsureHeaderAndGetDevFlag(TrophyHandle *handle, bool *outIsDev)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;
	(void)handle;
	*outIsDev = (m_header.flags & 1u) != 0;
	return 0;
}

/* 0x812CCD36 */
int TrophyTrpArchive::ReadDefaultConfOrExtract(TrophyHandle *handle, const TrophyCommId *commId,
					       TrophyConfMeta *outMeta, TrophyConfData **outConf)
{
	if (!IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;

	if (isAllZeroDigest(m_header.sha1)) {
		TrophyExtractOptions opts;
		opts.userId = 0;
		opts.commId = commId;
		opts.parseOptions = 1u;
		opts.lang = -2;
		opts.assetMask = 1u;
		opts.flags = 0x10000u;

		TrophyTrpExtractResult result;
		uint64_t totalBytes = 0;
		uint32_t count = m_header.entryCount;
		for (uint32_t i = 0; i < count; ++i) {
			bool shouldExtract = false;
			TrophyAssetDesc desc;
			int ret = shouldExtractTrpEntry(handle, this, &opts, i, &shouldExtract,
							&desc);
			if (ret < 0)
				return ret;
			if (shouldExtract) {
				ret = extractTrpEntry(handle, this, i, &opts, NULL, &totalBytes,
						      &desc, &result.defaultConfHandle);
				if (ret < 0)
					return ret;
			} else {
				ret = hashTrpEntryBytes(handle, this, i, NULL, &totalBytes);
				if (ret < 0)
					return ret;
			}
		}
		if (outMeta != NULL) {
			outMeta->sourceType = 1u;
			outMeta->confVersion = m_header.flags;
			outMeta->sha1 = m_header.sha1;
		}
		return result.TakeDefaultConf(outConf);
	}

	if (outMeta != NULL) {
		outMeta->confVersion = m_header.flags;
		outMeta->sha1 = m_header.sha1;
	}

	TrophyTrpFileStream sfmStream;
	int ret = sfmStream.OpenByName(handle, this, "TROPCONF.SFM");
	if (ret < 0)
		return ret;

	TrophySignedSfmReader reader;
	ret = reader.Init(0);
	if (ret >= 0)
		ret = reader.Parse(handle, &sfmStream);
	if (ret >= 0)
		ret = reader.TakeConfData(commId, outConf);
	sfmStream.Close();
	return ret;
}

int TrophyTrpArchive::FindEntryByName(const char *name, TrophyTrpEntry *outEntry) const
{
	if (!IsOpen())
		return NP_TROPHY_ERR_TRP_NOT_OPEN;
	if (name == NULL || outEntry == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	for (uint32_t i = 0; i < m_header.entryCount; ++i) {
		if (sceClibStrncmp(m_entries[i].name, name, sizeof(m_entries[i].name)) == 0) {
			*outEntry = m_entries[i];
			return 0;
		}
	}
	return NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND;
}

int TrophyTrpArchive::OpenEntryStream(const char *name, TrophyTrpFileStream *outStream)
{
	TrophyTrpEntry entry;
	int ret = FindEntryByName(name, &entry);
	if (ret < 0)
		return ret;
	return outStream->OpenEntry(this, &entry);
}

int TrophyTrpArchive::OpenEntryStreamByIndex(uint32_t index, TrophyTrpFileStream *outStream)
{
	TrophyTrpEntry entry;
	int ret = GetEntryByIndex(index, &entry);
	if (ret < 0)
		return ret;
	return outStream->OpenEntry(this, &entry);
}

/* =========================================================================
 * TrophyTrpConfSource (0x812CB694..0x812CC4C8)
 * ========================================================================= */

TrophyTrpConfSource::TrophyTrpConfSource()
	: TrophyConfSourceBase(1)
{
}

TrophyTrpConfSource::~TrophyTrpConfSource()
{
	Close();
}

int TrophyTrpConfSource::Open(TrophyHandle *handle, const TrophyMount *mount,
			      const TrophyCommId *commId, bool verifySha1)
{
	if (mount == NULL || commId == NULL || !mount->IsMounted())
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	char commDir[32];
	int ret = npTrophyFormatCommDirNamePlain(commDir, commId);
	if (ret < 0)
		return ret;

	char trpPath[256];
	ret = npTrophyJoinBasePath(trpPath, mount->GetMountPoint(), 4, "sce_sys", "trophy",
				   commDir, "TROPHY.TRP");
	if (ret < 0)
		return ret;

	ret = m_archive.Open(handle, trpPath, verifySha1);
	if (ret < 0)
		return ret;

	SetCommId(commId);
	return 0;
}

int TrophyTrpConfSource::Close()
{
	return m_archive.Close();
}

int TrophyTrpConfSource::IsDevFlagSet(bool *outIsDev) const
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (outIsDev != NULL)
		*outIsDev = (m_archive.GetHeader().flags & 1u) != 0;
	return 0;
}

int TrophyTrpConfSource::EstimateSpace(TrophyHandle *handle, const TrophyExtractOptions *options,
				       uint64_t *outUr0Bytes, uint64_t *outUx0Bytes)
{
	(void)handle;
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (options == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	uint64_t ur0Bytes = 0;
	uint64_t ux0Bytes = 0;
	uint32_t count = m_archive.GetEntryCount();

	for (uint32_t i = 0; i < count; ++i) {
		TrophyTrpEntry entry;
		int ret = m_archive.GetEntryByIndex(i, &entry);
		if (ret < 0)
			return ret;

		if (!TrophyAssetExtractor::ShouldExtractAsset(entry.name, options))
			continue;

		TrophyAssetDesc desc;
		if (npTrophyParseAssetFilename(entry.name, &desc) < 0)
			continue;

		uint64_t alignedSize = (entry.size + 4095u) & ~static_cast<uint64_t>(4095u);
		if (desc.kind == TROPHY_ASSET_SFM || desc.kind == TROPHY_ASSET_TITLE_ICON)
			ur0Bytes += alignedSize;
		else
			ux0Bytes += alignedSize;
	}

	if (outUr0Bytes != NULL)
		*outUr0Bytes = ur0Bytes;
	if (outUx0Bytes != NULL)
		*outUx0Bytes = ux0Bytes;
	return 0;
}

int TrophyTrpConfSource::OpenOwnedAssetStream(TrophyHandle *handle, const char *assetName,
					      TrophyConfSourceStream **outStream)
{
	(void)handle;
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (assetName == NULL || outStream == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outStream = NULL;

	TrophyTrpOwnedAssetStream *stream = new TrophyTrpOwnedAssetStream();
	if (stream == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = m_archive.OpenEntryStream(assetName, stream->GetTrpStream());
	if (ret < 0) {
		delete stream;
		if (ret == NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND)
			return NP_TROPHY_ERR_ASSET_NOT_FOUND;
		return ret;
	}

	*outStream = stream;
	return 0;
}

int TrophyTrpConfSource::ExtractAssets(TrophyHandle *handle, const TrophyExtractOptions *options,
				       TrophyConfMeta *outMeta,
				       TrophyExtractedAssetReport *outReport,
				       TrophyConfData **outDefaultConf,
				       TrophyConfHandle **outLocalizedConf)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;

	int ret = m_archive.VerifyArchiveSha1(handle);
	if (ret < 0)
		return ret;

	ret = TrophyConfSourceBase::ExtractAssets(handle, options, outMeta, outReport,
						  outDefaultConf, outLocalizedConf);
	if (ret < 0)
		return ret;

	if (outMeta != NULL) {
		outMeta->sourceType = 1u;
		outMeta->sha1 = m_archive.GetHeader().sha1;
	}
	return 0;
}

int TrophyTrpConfSource::OpenAssetWithLangFallback(const TrophyAssetDesc *requested,
						   bool fallbackDefault,
						   TrophyTrpFileStream *outStream)
{
	char filename[32];
	int ret = npTrophyFormatAssetFilename(filename, requested);
	if (ret < 0)
		return ret;

	ret = m_archive.OpenEntryStream(filename, outStream);
	if (ret >= 0)
		return 0;
	if (ret != NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND || !fallbackDefault ||
	    requested->IsDefaultLang()) {
		return (ret == NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND)
			       ? NP_TROPHY_ERR_ASSET_NOT_FOUND
			       : ret;
	}

	TrophyAssetDesc fallbackDesc = *requested;
	fallbackDesc.SetLang(-1);
	ret = npTrophyFormatAssetFilename(filename, &fallbackDesc);
	if (ret < 0)
		return ret;

	ret = m_archive.OpenEntryStream(filename, outStream);
	if (ret == NP_TROPHY_ERR_TRP_ENTRY_NOT_FOUND)
		return NP_TROPHY_ERR_ASSET_NOT_FOUND;
	return ret;
}

int TrophyTrpConfSource::OpenSfmStream(TrophyHandle *handle, int lang, bool fallbackDefault,
				       TrophyTrpFileStream *outStream, SceSize *outHeaderSize)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;

	TrophyAssetDesc desc;
	desc.kind = TROPHY_ASSET_SFM;
	desc.langOrTrophyId = lang;
	desc.groupId = 0;

	int ret = OpenAssetWithLangFallback(&desc, fallbackDefault, outStream);
	if (ret < 0) {
		if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND)
			return NP_TROPHY_ERR_SFM_NOT_FOUND;
		return ret;
	}

	if (outHeaderSize != NULL) {
		TrophySfmSignatureStripper stripper;
		ret = stripper.Init(outStream);
		if (ret >= 0)
			ret = stripper.StripHeader(handle);
		if (ret >= 0)
			ret = stripper.GetStrippedHeaderSize(outHeaderSize);
		stripper.Term();
		if (ret < 0) {
			outStream->Close();
			return ret;
		}
		ret = outStream->Seek(handle, 0, SCE_SEEK_SET, NULL);
		if (ret < 0) {
			outStream->Close();
			return ret;
		}
	}
	return 0;
}

int TrophyTrpConfSource::ReadHeaderAndConf(TrophyHandle *handle, uint32_t parseOptions,
					   TrophyConfData **outConfData)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (outConfData == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outConfData = NULL;

	TrophyTrpFileStream trpStream;
	int ret = OpenSfmStream(handle, -1, false, &trpStream, NULL);
	if (ret < 0)
		return ret;

	TrophySignedSfmReader reader;
	ret = reader.Init(parseOptions);
	if (ret >= 0)
		ret = reader.Parse(handle, &trpStream);
	if (ret >= 0)
		ret = reader.TakeConfData(&GetCommId(), outConfData);

	trpStream.Close();
	return ret;
}

int TrophyTrpConfSource::ReadDefaultConf(TrophyHandle *handle, TrophyConfData **outConfData)
{
	return ReadHeaderAndConf(handle, 0, outConfData);
}

int TrophyTrpConfSource::ReadConfHandle(TrophyHandle *handle, int lang,
					TrophyConfHandle **outHandle)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (outHandle == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outHandle = NULL;

	TrophyTrpFileStream trpStream;
	int actualLang = lang;
	int ret = OpenSfmStream(handle, lang, false, &trpStream, NULL);
	if (ret == NP_TROPHY_ERR_SFM_NOT_FOUND && lang >= 0) {
		actualLang = -1;
		ret = OpenSfmStream(handle, -1, false, &trpStream, NULL);
	}
	if (ret < 0)
		return ret;

	TrophySignedSfmReader reader;
	ret = reader.Init(1u);
	if (ret >= 0)
		ret = reader.Parse(handle, &trpStream);
	if (ret >= 0)
		ret = reader.TakeConfHandle(&GetCommId(), actualLang, outHandle);

	trpStream.Close();
	return ret;
}

/* 0x812CB9EE */
int TrophyTrpConfSource::ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					  void *outTitleDetails)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (confHandle == NULL || outTitleDetails == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyTrpFileStream trpStream;
	int ret = OpenSfmStream(handle, confHandle->GetLang(), false, &trpStream, NULL);
	if (ret < 0)
		return ret;

	ret = npTrophyExtractTitleTextInfo(handle, &trpStream, confHandle, outTitleDetails);
	trpStream.Close();
	return ret;
}

/* 0x812CBA98 */
int TrophyTrpConfSource::ExtractGroupText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
					  int groupId, void *outGroupDetails)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (confHandle == NULL || outGroupDetails == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyTrpFileStream trpStream;
	int ret = OpenSfmStream(handle, confHandle->GetLang(), false, &trpStream, NULL);
	if (ret < 0)
		return ret;

	ret = npTrophyExtractGroupTextInfo(handle, &trpStream, confHandle, groupId,
					   outGroupDetails);
	trpStream.Close();
	return ret;
}

/* 0x812CBB42 */
int TrophyTrpConfSource::ExtractTrophyText(TrophyHandle *handle,
					   const TrophyConfHandle *confHandle, int trophyId,
					   bool revealDetails, void *outTrophyDetails)
{
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;
	if (confHandle == NULL || outTrophyDetails == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyTrpFileStream trpStream;
	int ret = OpenSfmStream(handle, confHandle->GetLang(), false, &trpStream, NULL);
	if (ret < 0)
		return ret;

	ret = npTrophyExtractTrophyTextInfo(handle, &trpStream, confHandle, trophyId,
					    revealDetails, outTrophyDetails);
	trpStream.Close();
	return ret;
}

/* 0x812CBBFC */
int TrophyTrpConfSource::OpenTitleIconStream(TrophyHandle *handle, int lang, bool fallbackDefault,
					     TrophyTrpFileStream *outStream)
{
	(void)handle;
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;

	TrophyAssetDesc desc;
	desc.kind = TROPHY_ASSET_TITLE_ICON;
	desc.langOrTrophyId = lang;
	desc.groupId = 0;

	int ret = OpenAssetWithLangFallback(&desc, fallbackDefault, outStream);
	if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND)
		return NP_TROPHY_ERR_ICON_NOT_FOUND;
	return ret;
}

/* 0x812CBCBE */
int TrophyTrpConfSource::OpenOwnedTitleIconStream(TrophyHandle *handle, int lang, int smallFlag,
						  bool fallbackDefault,
						  TrophyConfSourceStream **outStream)
{
	(void)smallFlag;
	TrophyTrpOwnedAssetStream *stream = new TrophyTrpOwnedAssetStream();
	if (stream == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = OpenTitleIconStream(handle, lang, fallbackDefault, stream->GetTrpStream());
	if (ret >= 0) {
		*outStream = stream;
		return 0;
	}

	delete stream;
	*outStream = NULL;
	if (ret == NP_TROPHY_ERR_TRP_NOT_OPEN)
		return NP_TROPHY_ERR_ASSET_NOT_FOUND;
	return ret;
}

/* 0x812CBCC8 */
int TrophyTrpConfSource::OpenGroupIconStream(TrophyHandle *handle, int groupId, int lang,
					     bool fallbackDefault, TrophyTrpFileStream *outStream)
{
	(void)handle;
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;

	TrophyAssetDesc desc;
	desc.kind = TROPHY_ASSET_GROUP_ICON;
	desc.langOrTrophyId = lang;
	desc.groupId = groupId;

	int ret = OpenAssetWithLangFallback(&desc, fallbackDefault, outStream);
	if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND)
		return NP_TROPHY_ERR_ICON_NOT_FOUND;
	return ret;
}

/* 0x812CBECE */
int TrophyTrpConfSource::OpenOwnedGroupIconStream(TrophyHandle *handle, int groupId, int lang,
						  int smallFlag, bool fallbackDefault,
						  TrophyConfSourceStream **outStream)
{
	(void)smallFlag;
	TrophyTrpOwnedAssetStream *stream = new TrophyTrpOwnedAssetStream();
	if (stream == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	int ret = OpenGroupIconStream(handle, groupId, lang, fallbackDefault,
				      stream->GetTrpStream());
	if (ret >= 0) {
		*outStream = stream;
		return 0;
	}

	delete stream;
	*outStream = NULL;
	if (ret == NP_TROPHY_ERR_TRP_NOT_OPEN)
		return NP_TROPHY_ERR_ASSET_NOT_FOUND;
	return ret;
}

/* 0x812CBD98 */
int TrophyTrpConfSource::OpenTrophyIconStream(TrophyHandle *handle, int trophyId,
					      TrophyTrpFileStream *outStream)
{
	(void)handle;
	if (!m_archive.IsOpen())
		return NP_TROPHY_ERR_CONF_SOURCE_NOT_OPEN;

	TrophyAssetDesc desc;
	desc.kind = TROPHY_ASSET_TROPHY_ICON;
	desc.langOrTrophyId = trophyId;
	desc.groupId = 0;

	int ret = OpenAssetWithLangFallback(&desc, false, outStream);
	if (ret == NP_TROPHY_ERR_ASSET_NOT_FOUND)
		return NP_TROPHY_ERR_ICON_NOT_FOUND;
	return ret;
}
