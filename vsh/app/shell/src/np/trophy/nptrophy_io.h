/*
 * nptrophy_io.h: version/filename/path utilities, file/memfile/record-file IO,
 * cancellation handles, progress tracking, and SHA-1 / HMAC-SHA1 sinks
 * (0x812E7868..0x812E9977, 0x812EDAA4..0x812F0207).
 */
#ifndef SHELL_NP_TROPHY_IO_H
#define SHELL_NP_TROPHY_IO_H

#include <stdarg.h>
#include <stdint.h>
#include <scetypes.h>
#include <libsha1.h>
#include <libhmac.h>
#include <kernel/iofilemgr.h>
#include "nptrophy_runtime.h"

/* Version / commId / JID formatting utilities (0x812E7868..0x812E7B2D) */
int npTrophyParseVersionN(const char *str, SceSize maxLen, uint32_t *outVersion);
int npTrophyParseVersion(const char *str, uint32_t *outVersion);
int npTrophyFormatVersion(uint32_t version, char *outBuf, SceSize outSize);
int npTrophyParseCommId(const char *str, TrophyCommId *outCommId);
int npTrophyParseCommIdN(const char *str, SceSize length, TrophyCommId *outCommId);
int npTrophyFormatCommId(const TrophyCommId *commId, char *outBuf, SceSize outSize);
int npTrophyNpIdToJidString(const SceNpId *npId, char *outBuf, SceSize outSize);

/* Directory & filesystem utilities (0x812E7B2E..0x812E838F) */
enum TrophyDirEntryKind {
	TROPHY_DIR_ENTRY_DIR		= 0,
	TROPHY_DIR_ENTRY_LINK		= 1,
	TROPHY_DIR_ENTRY_FILE		= 2,
	TROPHY_DIR_ENTRY_OTHER		= 3
};

struct TrophyDirEntry {
	int kind;
	char name[32];
};

class TrophyDir {
public:
	TrophyDir() : m_dirFd(-1) {}
	~TrophyDir();

	int Open(const char *dirPath, bool enableIoctl);
	int Close();
	int Read(TrophyDirEntry *outEntry, bool *outHasEntry);

private:
	SceUID m_dirFd;
};

int npTrophyPathExists(const char *path, bool *outExists);
int npTrophyEnsureDir(const char *path, SceMode mode);
int npTrophyRemoveFile(const char *path);
int npTrophyRenamePath(const char *oldPath, const char *newPath);
int npTrophyRemoveTree(const char *path);

/* Trophy conf/icon filename parsing & formatting (0x812E8010..0x812E8C87) */
enum TrophyAssetKind {
	TROPHY_ASSET_SFM	= 1,
	TROPHY_ASSET_TITLE_ICON	= 2,
	TROPHY_ASSET_GROUP_ICON	= 3,
	TROPHY_ASSET_TROPHY_ICON = 4
};

struct TrophyAssetDesc {
	int kind;
	int langOrTrophyId;
	int groupId;

	bool IsDefaultLang() const;
	bool HasSpecificLang() const;
	bool IsValidLang() const;
	void SetLang(int lang);
	int GetLang() const;
};

int npTrophyFormatCommDirName(char *outName, const TrophyCommId *commId, const char *suffix);
int npTrophyFormatCommDirNamePlain(char *outName, const TrophyCommId *commId);
bool npTrophyIsSceTropDirName(const char *name);
bool npTrophyParseCommDirName(const char *name, TrophyCommId *outCommId);
int npTrophyParseAssetFilename(const char *filename, TrophyAssetDesc *outDesc);
int npTrophyFormatSfmFilename(char *outName, int lang);
int npTrophyFormatTitleIconFilename(char *outName, int lang, int smallFlag);
int npTrophyFormatGroupIconFilename(char *outName, int groupId, int lang, int smallFlag);
int npTrophyFormatTrophyIconFilename(char *outName, int trophyId, int smallFlag);
int npTrophyFormatAssetFilename(char *outName, const TrophyAssetDesc *desc);

/* Path join & directory helpers (0x812E8C88..0x812E8E8B, 0x812E9268..0x812E9313, 0x812EDAA4..0x812EE493) */
int npTrophyAppendPathSep(char *path, SceSize pathSize, char sep);
int npTrophyAppendPathComponentsV(char *path, SceSize pathSize, char sep, int count, va_list ap);
int npTrophyAppendPathComponents(char *path, SceSize pathSize, char sep, int count, ...);
int npTrophyBuildPathV(char *outPath, SceSize pathSize, char sep, int count, va_list ap);
int npTrophyPopPathComponent(char *path, SceSize pathSize, char sep);
int npTrophyPathInit(char *outPath, int count, ...);
int npTrophyPathAppend(char *path, int count, ...);
int npTrophyPathPop(char *path);

int npTrophyUr0Path(char *outPath, int userId, int count, ...);
int npTrophyUr0ConfDirPath(char *outPath, int userId, const TrophyCommId *commId,
			   const char *suffix);
int npTrophyUr0ConfFilePath(char *outPath, int userId, const TrophyCommId *commId,
			    const char *filename, const char *suffix);
int npTrophyUr0DataRootPath(char *outPath, int userId);
int npTrophyUr0DataDirPath(char *outPath, int userId, const TrophyCommId *commId,
			   const char *suffix);
int npTrophyEnsureUr0ConfRoot(int userId);
int npTrophyEnsureUr0ConfDir(int userId, const TrophyCommId *commId, const char *suffix);
int npTrophyRemoveUr0ConfRoot(int userId);
int npTrophyRemoveUr0ConfDir(int userId, const TrophyCommId *commId, const char *suffix);
int npTrophyRemoveUr0DataRoot(int userId);
int npTrophyRemoveUr0SceTropDir(int userId);
int npTrophyRemoveUr0DataDir(int userId, const TrophyCommId *commId, const char *suffix);
int npTrophyRenameUr0ConfDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix);
int npTrophyRenameUr0DataDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix);

int npTrophyJoinBasePath(char *outPath, const char *basePath, int count, ...);
int npTrophyJoinBaseFile(char *outPath, const char *basePath, const char *filename);

int npTrophyUx0Path(char *outPath, int userId, int count, ...);
int npTrophyUx0ConfFilePath(char *outPath, int userId, const TrophyCommId *commId,
			    const char *filename, const char *suffix);
int npTrophyUx0RootExists(int userId, bool *outExists);
int npTrophyUx0ConfDirExists(int userId, const TrophyCommId *commId, bool *outExists,
			     const char *suffix);
int npTrophyEnsureUx0ConfRoot(int userId);
int npTrophyEnsureUx0ConfDir(int userId, const TrophyCommId *commId, const char *suffix);
int npTrophyRemoveUx0ConfRoot(int userId);
int npTrophyRemoveUx0ConfDir(int userId, const TrophyCommId *commId, const char *suffix);
int npTrophyRenameUx0ConfDir(int userId, const TrophyCommId *commId, const char *oldSuffix,
			     const char *newSuffix);

/* Cancellation token, progress tracker & operation handle (0x812E9314..0x812E985B) */
class TrophyCancelToken {
public:
	TrophyCancelToken();
	virtual ~TrophyCancelToken();

	int Init();
	void Term();
	void PushNoCancel();
	void PopNoCancel();
	int GetCancelError();
	void Cancel(int errorCode);
	void ClearSuspended();
	int SetChild(TrophyCancelToken *child);
	void ClearChild(TrophyCancelToken *child);

protected:
	virtual void OnCancel(int errorCode) = 0;

	TrophyLwMutex m_mutex;
	int m_cancelError;
	uint8_t m_cancelEnabled;
	int m_noCancelDepth;
	TrophyCancelToken *m_child;
};

struct TrophyProgress {
	int totalSteps;
	int completedSteps;

	TrophyProgress() : totalSteps(0), completedSteps(0) {}
	void Reset()
	{
		totalSteps = 0;
		completedSteps = 0;
	}
	void AddTotal(int delta) { totalSteps += delta; }
	void Advance(int delta);
	void Complete() { completedSteps = totalSteps; }
	int GetPercent() const;
};

class TrophyHandle : public TrophyHeapObject {
public:
	TrophyHandle();
	virtual ~TrophyHandle();

	int Init();
	void Term();
	void AddRef();
	void ReleaseRef();
	bool HasRefs() const { return m_refCount != 0; }

	TrophyCancelToken *GetCancelToken() { return &m_cancelToken; }
	int CheckCancel() { return m_cancelToken.GetCancelError(); }
	void Cancel(int errorCode) { m_cancelToken.Cancel(errorCode); }
	void ClearSuspended() { m_cancelToken.ClearSuspended(); }

	void SetProgress(const TrophyProgress *progress);
	int AdvanceProgress(int delta);
	int CompleteProgress();
	int GetProgressPercent();

private:
	class HandleCancelToken : public TrophyCancelToken {
	protected:
		virtual void OnCancel(int errorCode) { (void)errorCode; }
	};

	HandleCancelToken m_cancelToken;
	volatile unsigned int m_refCount;
	TrophyProgress m_progress;
};

class TrophyNoCancelGuard {
public:
	explicit TrophyNoCancelGuard(TrophyHandle *handle);
	~TrophyNoCancelGuard();

private:
	TrophyHandle *m_handle;
};

class TrophyScopedCancelToken : public TrophyCancelToken {
public:
	TrophyScopedCancelToken();
	virtual ~TrophyScopedCancelToken();

	int Attach(TrophyHandle *handle);
	int Detach(int operationResult);

private:
	TrophyHandle *m_parentHandle;
};

/* File abstraction, disk file, memory file & icon file (0x812E8E8C..0x812E925A, 0x812EF7B4..0x812F0207) */
class TrophyFileBase : public TrophyInputStream, public TrophyOutputStream {
public:
	virtual ~TrophyFileBase() {}
	virtual int Close() = 0;
	virtual bool IsOpen() const = 0;
	virtual int GetSize(SceOff *outSize) = 0;
	virtual int Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos) = 0;
	virtual int Truncate(TrophyHandle *handle, SceOff length) = 0;
	virtual int Sync(TrophyHandle *handle) = 0;

	int MoveRange(TrophyHandle *handle, SceOff srcOffset, SceOff dstOffset, SceOff byteCount,
		      void *workBuf, SceSize workBufSize);
};

class TrophyFile : public TrophyFileBase {
public:
	TrophyFile();
	virtual ~TrophyFile();

	int Open(const char *path, int flags, SceMode mode);
	virtual int Close();
	virtual bool IsOpen() const;
	virtual int GetSize(SceOff *outSize);
	virtual int Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos);
	virtual int Truncate(TrophyHandle *handle, SceOff length);
	virtual int Sync(TrophyHandle *handle);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	virtual int Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			  SceSize *outWritten);

private:
	SceUID m_fileFd;
};

struct TrophyMemFileConfig {
	SceSize maxSize;
	SceSize initialSize;
	SceSize growStep;
};

class TrophyMemFile : public TrophyFileBase {
public:
	TrophyMemFile();
	virtual ~TrophyMemFile();

	int Open(const TrophyMemFileConfig *config);
	virtual int Close();
	virtual bool IsOpen() const { return m_isOpen != 0; }
	virtual int GetSize(SceOff *outSize);
	virtual int Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos);
	virtual int Truncate(TrophyHandle *handle, SceOff length);
	virtual int Sync(TrophyHandle *handle);
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	virtual int Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			  SceSize *outWritten);

	int LoadFromFile(TrophyHandle *handle, TrophyFileBase *source);
	int FlushToFile(TrophyHandle *handle, TrophyFileBase *dest);
	bool IsDirty() const { return m_dirty != 0; }

private:
	uint8_t m_isOpen;
	SceSize m_maxSize;
	SceSize m_growStep;
	SceSize m_capacity;
	uint8_t *m_buffer;
	SceSize m_size;
	SceSize m_position;
	uint8_t m_dirty;
};

/* Structured section/record binary file (TRPTITLE.DAT, TRPUSER.DAT, TRPSTAT.DAT, TRPTRANS.DAT; 0x812EE494..0x812EF7B3) */
typedef int (*TrophyRecordInitCallback)(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
					void *userCtx);

struct TrophyRecordSectionSpec {
	uint32_t sectionId;
	uint32_t recordSize;
	uint32_t sectionKind;
	uint32_t recordCount;
	void *initialRecordBuf;
	TrophyRecordInitCallback initCallback;
	void *initUserCtx;
};

struct TrophyRecordSectionMeta {
	uint32_t sectionId;
	uint32_t recordSize;
	uint32_t sectionKind;
	uint32_t recordCount;
	uint64_t fileOffset;
	uint64_t reserved;
};

class TrophyRecordFile {
public:
	explicit TrophyRecordFile(uint32_t magic);
	~TrophyRecordFile();

	bool IsOpen() const { return m_activeFile != NULL; }
	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophyRecordSectionSpec *sections, int sectionCount, bool useMemCache);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode, bool useMemCache);
	int Close();
	int FindSection(uint32_t sectionId, uint32_t *outSectionIndex) const;
	int GetRecordCount(uint32_t sectionIndex, uint32_t *outRecordCount) const;
	int AppendRecord(TrophyHandle *handle, uint32_t sectionIndex, const void *recordBuf,
			 SceSize recordSize, uint32_t *outRecordIndex);
	int MoveRecord(TrophyHandle *handle, uint32_t sectionIndex, uint32_t fromIndex,
		       uint32_t toIndex, void *workBuf, SceSize workBufSize);
	int WriteRecordRange(TrophyHandle *handle, uint32_t sectionIndex, uint32_t recordIndex,
			     SceSize byteOffset, const void *srcBuf, SceSize byteCount);
	int ReadRecordRange(TrophyHandle *handle, uint32_t sectionIndex, uint32_t recordIndex,
			    SceSize byteOffset, void *outBuf, SceSize byteCount);

private:
	int WriteInitialLayout(TrophyHandle *handle, const TrophyRecordSectionSpec *sections,
			       int sectionCount);
	int ReadHeaderAndDirectory(TrophyHandle *handle);
	void CleanupFiles();

	TrophyFile *m_diskFile;
	TrophyMemFile *m_memFile;
	TrophyFileBase *m_activeFile;
	uint32_t m_magic;
	uint32_t m_sectionCount;
	TrophyRecordSectionMeta *m_sections;
};

struct TrophySimpleHeader {
	uint32_t formatVersion;
	uint32_t flags;
};

class TrophySimpleRecordFile {
public:
	explicit TrophySimpleRecordFile(uint32_t magic);
	~TrophySimpleRecordFile();

	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophySimpleHeader *initialHeader);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode);
	int Close();
	int ReadHeader(TrophyHandle *handle, TrophySimpleHeader *outHeader);

private:
	static int SerializeHeaderCallback(void *recordBuf, SceSize recordSize,
					   uint32_t recordIndex, void *userCtx);

	TrophyRecordFile m_recordFile;
	uint32_t m_sectionIndex;
};

/* Conf / icon disk stream with handle refcounting (0x812EF7B4..0x812EFD3F) */
class TrophyConfFile : public TrophyFileBase {
public:
	TrophyConfFile();
	virtual ~TrophyConfFile();

	int OpenConfFile(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			 const char *filename, int storageLocation);
	virtual int Close();
	virtual bool IsOpen() const;
	virtual int GetSize(SceOff *outSize) { return m_file.GetSize(outSize); }
	virtual int Seek(TrophyHandle *handle, SceOff offset, int whence, SceOff *outPos)
	{
		return m_file.Seek(handle, offset, whence, outPos);
	}
	virtual int Truncate(TrophyHandle *handle, SceOff length)
	{
		return m_file.Truncate(handle, length);
	}
	virtual int Sync(TrophyHandle *handle) { return m_file.Sync(handle); }
	virtual int Read(TrophyHandle *handle, void *outBuf, SceSize size, SceSize *outRead);
	virtual int Write(TrophyHandle *handle, const void *srcBuf, SceSize size,
			  SceSize *outWritten)
	{
		return m_file.Write(handle, srcBuf, size, outWritten);
	}

private:
	TrophyFile m_file;
	TrophyHandle *m_ownerHandle;
};

class TrophyConfIconFile : public TrophyConfFile {
public:
	TrophyConfIconFile();
	virtual ~TrophyConfIconFile();

	int OpenTitleIcon(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			  int lang, bool fallbackDefault);
	int OpenGroupIcon(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			  int groupId, int lang, bool fallbackDefault);
	int OpenTrophyIcon(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			   int trophyId);
};

/* SHA-1 and HMAC-SHA1 wrappers (0x812E985C..0x812E9977) */
struct TrophySha1Digest {
	uint8_t bytes[SCE_SHA1_DIGEST_SIZE];

	void Clear();
	int Set(const void *src, SceSize length);
	int CopyTo(void *dst, SceSize dstSize) const;
	bool Equals(const TrophySha1Digest &other) const;
	int FormatHexUpper(char *outHex, SceSize outSize) const;
};

class TrophySha1Hasher : public TrophyHashSink {
public:
	TrophySha1Hasher() {}
	virtual ~TrophySha1Hasher() {}

	int Init();
	virtual int Update(const void *bytes, SceSize size);
	int Final(TrophySha1Digest *outDigest);

private:
	SceSha1Context m_sha1Ctx;
};

class TrophyHmacSha1Hasher : public TrophyHashSink {
public:
	TrophyHmacSha1Hasher() {}
	virtual ~TrophyHmacSha1Hasher() {}

	int Init(const void *key, SceSize keyLen);
	virtual int Update(const void *bytes, SceSize size);
	int Final(TrophySha1Digest *outDigest);

	static int ComputeDigest(const void *key, SceSize keyLen, const void *bytes,
				 SceSize size, TrophySha1Digest *outDigest);

private:
	SceHmacSha1Context m_hmacCtx;
};

#endif /* SHELL_NP_TROPHY_IO_H */
