/*
 * nptrophy_storage.h: local storage layer for SceNpTrophy — journaled binary
 * record files (TRPTRANS.DAT, TRPTITLE.DAT, TRPUSER.DAT), TRPPARAM.INI wrapper,
 * signed SFM extraction/reading, asset extraction, SNS feed publishing, and
 * title/conf/user storage controllers (0x812D00C0..0x812E1113).
 */
#ifndef SHELL_NP_TROPHY_STORAGE_H
#define SHELL_NP_TROPHY_STORAGE_H

#include <stdint.h>
#include <scetypes.h>
#include <ini_file_processor/ini_file_processor.h>
#include "nptrophy_runtime.h"
#include "nptrophy_io.h"
#include "nptrophy_sfm.h"

/* Additional SceNpTrophy error codes used by local storage & sync */
#define NP_TROPHY_ERR_ALREADY_UNLOCKED		-2141907441	/* 0x8055160F */
#define NP_TROPHY_ERR_PLATINUM_CANNOT_UNLOCK	-2141907440	/* 0x80551610 */
#define NP_TROPHY_ERR_BROKEN_DATA		-2141907436	/* 0x80551614 */
#define NP_TROPHY_ERR_INSUFFICIENT_UR0_SPACE	-2141907435	/* 0x80551615 */
#define NP_TROPHY_ERR_NOT_UNLOCKED		-2141907426	/* 0x8055161E */
#define NP_TROPHY_ERR_UNLOCK_NOT_ALLOWED	-2141907425	/* 0x8055161F */
#define NP_TROPHY_ERR_INSUFFICIENT_UX0_SPACE	-2141907424	/* 0x80551620 */
#define NP_TROPHY_ERR_TXN_ALREADY_OPEN		-2141907315	/* 0x8055168D */
#define NP_TROPHY_ERR_TXN_NOT_OPEN		-2141907314	/* 0x8055168E */
#define NP_TROPHY_ERR_JOURNAL_MISMATCH		-2141907313	/* 0x8055168F */
#define NP_TROPHY_ERR_COMM_ID_MISMATCH		-2141907312	/* 0x80551690 */
#define NP_TROPHY_ERR_FLAGS_MISMATCH		-2141907311	/* 0x80551691 */
#define NP_TROPHY_ERR_TITLE_ALREADY_EXISTS	-2141907310	/* 0x80551692 */
#define NP_TROPHY_ERR_TITLE_NOT_FOUND		-2141907309	/* 0x80551693 */
#define NP_TROPHY_ERR_TRANS_TYPE_MISMATCH	-2141907308	/* 0x80551694 */
#define NP_TROPHY_ERR_TRANS_FULL		-2141907307	/* 0x80551695 */
#define NP_TROPHY_ERR_SFM_NOT_FOUND		-2141907306	/* 0x80551696 */
#define NP_TROPHY_ERR_STORAGE_NOT_OPEN		-2141907300	/* 0x8055169C */
#define NP_TROPHY_ERR_TRANS_DEFERRED_CONFLICT	-2141907295	/* 0x805516A1 */
#define NP_TROPHY_ERR_ASSET_NOT_FOUND		-2141907287	/* 0x805516A9 */
#define NP_TROPHY_ERR_USER_DAT_MISSING		-2141907276	/* 0x805516B4 */
#define NP_TROPHY_ERR_TITLE_DAT_MISSING		-2141907275	/* 0x805516B5 */
#define NP_TROPHY_ERR_TRANS_DAT_MISSING		-2141907274	/* 0x805516B6 */
#define NP_TROPHY_ERR_PARAM_INI_MISSING		-2141907273	/* 0x805516B7 */
#define NP_TROPHY_ERR_MAX_TITLES_REACHED	-2141907270	/* 0x805516BA */
#define NP_TROPHY_ERR_FORMAT_VERSION_MISMATCH	-2141907260	/* 0x805516C4 */
#define NP_TROPHY_ERR_PLATINUM_CANNOT_LOCK	-2141907259	/* 0x805516C5 */
#define NP_TROPHY_ERR_TITLE_NOT_REGISTERED	-2141907258	/* 0x805516C6 */
#define NP_TROPHY_ERR_SIG_VERIFIER_MISSING	-2141907256	/* 0x805516C8 */
#define NP_TROPHY_ERR_SIG_VERIFIER_FAILED	-2141907255	/* 0x805516C9 */
#define NP_TROPHY_ERR_SIG_FORMAT_INVALID	-2141907254	/* 0x805516CA */
#define NP_TROPHY_ERR_SIG_VERIFY_FAILED		-2141907253	/* 0x805516CB */
#define NP_TROPHY_ERR_PARAM_INI_CORRUPT		-2141907252	/* 0x805516CC */
#define NP_TROPHY_ERR_CONF_MARKED_BROKEN	-2141907250	/* 0x805516CE */
#define NP_TROPHY_ERR_DATA_MARKED_BROKEN	-2141907249	/* 0x805516CF */
#define NP_TROPHY_ERR_SNS_PUBLISH_FAILED	-2141907244	/* 0x805516D4 */
#define NP_TROPHY_ERR_XML_ENTITY_INVALID	-2141907243	/* 0x805516D5 */
#define NP_TROPHY_ERR_HTTP_NOT_FOUND		-2141907191	/* 0x80551709 */

/* On-disk binary record file magics */
#define TROPHY_MAGIC_CONF_STAT			0x15D316C7u	/* 366142663 */
#define TROPHY_MAGIC_DATA_STAT			0xCB05CF8Fu	/* -888807025 */
#define TROPHY_MAGIC_TRPTITLE			0x17E66372u	/* 400989042 */
#define TROPHY_MAGIC_TRPTRANS			0x3E3197BAu	/* 1043435450 */
#define TROPHY_MAGIC_TRPUSER			0x7F159936u	/* 2132121910 */

#define TROPHY_FORMAT_VERSION_V1		0x00010000u
#define TROPHY_FORMAT_VERSION_V1_1		0x00010001u
#define TROPHY_MAX_INSTALLED_TITLES		100u
#define TROPHY_DEFAULT_TRANS_CAPACITY		144u

/* Forward declarations for Batch 3 (network/sync) and Batch 4 (service/arbiter) */
class TrophyConfHandle;
class TrophySfmSignatureVerifier;
class TrophySyncTitleRequestBase;
class TrophySyncTitleResultBase;
class TrophySyncSessionBase;
class TrophyNetClient;
class TrophyStorageArbiter;
class TrophyUserSession;
class TrophyContext;
class TrophyMount;

/* Dual clock pair: network/secure tick (0 if uninitialized) + local RTC tick */
struct TrophyDualTick {
	SceRtcTick secureTick;
	SceRtcTick localTick;

	void Clear()
	{
		secureTick.tick = 0;
		localTick.tick = 0;
	}
};

/* System clock & signature requirement helpers (0x812D7EA8, 0x812D7EC0) */
int npTrophyRequireSfmSignature(bool *outRequired);
int npTrophyGetDualTick(TrophyDualTick *outTicks);

/* Helper functions for filesystem free space & NP status (in other shell units) */
int npTrophyGetDeviceFreeSpace(const char *device, uint64_t *outMaxSize, uint64_t *outFreeSize);

/* Journal header state tracked inside section 2 of journaled record files */
struct TrophyJournalState {
	uint32_t state;
	uint32_t flags;
	uint64_t sequence;

	void Clear();
	bool Equals(const TrophyJournalState &other) const;
	bool IsDirty() const;
	void MarkDirty();
};

/* Conf source metadata persisted in TRPTITLE.DAT section 0x100 & TRPUSER.DAT */
struct TrophyConfMeta {
	uint32_t sourceType;
	uint32_t confVersion;
	TrophySha1Digest sha1;

	void Clear();
	bool Equals(const TrophyConfMeta &other) const;
};

/* Summary of a title's trophy set in TRPTITLE.DAT section 0x300 */
struct TrophyTitleSummaryMeta {
	uint32_t numTrophies;
	uint32_t titleFlags;
	uint64_t trophySetVersion;
};

/* Snapshot of a single title's TRPTITLE.DAT state (0x812DD102, 88 bytes) */
struct TrophyTitleDataSnapshot {
	TrophyJournalState journal;		/* +0x00 */
	TrophyConfMeta confMeta;		/* +0x10 */
	TrophyTitleSummaryMeta summary;		/* +0x30 */
	TrophyDualTick lastUnlockedTick;	/* +0x40 */
	uint64_t progressRevision;		/* +0x50 */

	void Clear();
	bool Equals(const TrophyTitleDataSnapshot &other) const;
};

/* Transaction count summary from TRPTRANS.DAT (0x812DA822) */
struct TrophyTransCounts {
	uint32_t pendingCount;
	uint32_t deferredCount;

	void Clear();
};

/* Directory record stored in TRPUSER.DAT section 0x300 and cached in TrophyTitleStorage (128 bytes) */
struct TrophyTitleRecord {
	int slotIndex;				/* +0x00 */
	TrophyCommId commId;			/* +0x04 */
	TrophyTitleDataSnapshot titleSnapshot;	/* +0x10 */
	TrophyJournalState transJournal;	/* +0x68 */
	TrophyTransCounts transCounts;		/* +0x78 */

	void Clear();
	bool IsBroken() const;
	void MarkBroken(int scope);
	bool HasPendingUpload() const;
	bool HasDeferredUploadOnly() const;
	bool HasInstalledFlag() const;
	void ExportTitleInfo(void *outInfo) const;
	void ExportSyncInfo(void *outInfo) const;
};

/* Parsed title static metadata from TRPTITLE.DAT section 0x300 */
struct TrophyTitleStaticMeta {
	TrophyCommId commId;			/* +0x00 */
	uint32_t numTrophies;			/* +0x0C */
	uint32_t titleFlags;			/* +0x10 */
	uint8_t hasGroups;			/* +0x14 */
	uint32_t groupMask;			/* +0x18 */
	NpTrophyFlagArray hiddenMask;		/* +0x1C */
	NpTrophyFlagArray allMask;		/* +0x2C */
	NpTrophyFlagArray platinumMask;		/* +0x3C */
	NpTrophyFlagArray goldMask;		/* +0x4C */
	NpTrophyFlagArray silverMask;		/* +0x5C */
	NpTrophyFlagArray bronzeMask;		/* +0x6C */
};

/* Static group metadata from TRPTITLE.DAT section 0x400 */
struct TrophyGroupStaticMeta {
	int groupId;				/* +0x00 */
	NpTrophyFlagArray allMask;		/* +0x04 */
	TrophyGradeBitsets gradeBits;		/* +0x14 */
};

/* Static trophy metadata from TRPTITLE.DAT section 0x500 */
struct TrophyEntryStaticMeta {
	int trophyId;				/* +0x00 */
	int grade;				/* +0x04 */
	int platinumLinkId;			/* +0x08 */
	NpTrophyFlagArray platinumReqMask;	/* +0x0C */
	int groupId;				/* +0x1C */
	uint8_t hidden;				/* +0x20 */
};

/* Per-trophy unlock status from TRPTITLE.DAT section 0x800 */
struct TrophyUnlockRecord {
	uint8_t unlocked;			/* +0x00 */
	uint32_t flags;				/* +0x04 */
	SceRtcTick unlockedSecureTick;		/* +0x08 */
	SceRtcTick unlockedLocalTick;		/* +0x10 */
	uint8_t syncedFlag;			/* +0x18 */
};

/* Public-facing trophy unlock state returned by GetTrophyState (24 bytes) */
struct TrophyUnlockStateInfo {
	int trophyId;				/* +0x00 */
	uint8_t unlocked;			/* +0x04 */
	uint8_t unlockedOffline;		/* +0x05 */
	uint8_t unlockedDeferred;		/* +0x06 */
	SceRtcTick secureTick;			/* +0x08 */
	SceRtcTick localTick;			/* +0x10 */
};

/* Unlock result returned when unlocking/locking a trophy (32 bytes) */
struct TrophyUnlockDelta {
	int trophyId;				/* +0x00 */
	uint8_t unlocked;			/* +0x04 */
	uint8_t unlockedOffline;		/* +0x05 */
	uint8_t unlockedDeferred;		/* +0x06 */
	SceRtcTick secureTick;			/* +0x08 */
	SceRtcTick localTick;			/* +0x10 */
	int grade;				/* +0x18 */
};

/* Full title progress snapshot from TRPTITLE.DAT section 0x600 */
struct TrophyTitleProgressSnapshot {
	TrophyDualTick lastCommitTick;		/* +0x00 */
	TrophyDualTick installedTick;		/* +0x10 */
	TrophyDualTick lastModifiedTick;	/* +0x20 */
	TrophyDualTick lastUnlockedTick;	/* +0x30 */
	TrophyDualTick lastSyncedTick;		/* +0x40 */
	uint64_t serverTimestamp;		/* +0x50 */
	TrophyDualTick displayUpdatedTick;	/* +0x58 */
	NpTrophyFlagArray unlockedMask;		/* +0x68 */
};

/* Group progress snapshot from TRPTITLE.DAT section 0x700 */
struct TrophyGroupProgressSnapshot {
	TrophyDualTick lastUnlockedTick;	/* +0x00 */
	TrophyDualTick displayUpdatedTick;	/* +0x10 */
	NpTrophyFlagArray unlockedMask;		/* +0x20 */
};

/* User-visible title progress output (0x812D53C4) */
struct TrophyTitleProgressView {
	TrophyCommId commId;			/* +0x00 */
	TrophyGradeCounts unlockedCounts;	/* +0x0C */
	uint32_t progressPercent;		/* +0x1C */
	TrophyTitleProgressSnapshot snapshot;	/* +0x20 */
};

/* User-visible group progress output (0x812D553E) */
struct TrophyGroupProgressView {
	int groupId;				/* +0x00 */
	TrophyGradeCounts unlockedCounts;	/* +0x04 */
	uint32_t progressPercent;		/* +0x14 */
	TrophyGroupProgressSnapshot snapshot;	/* +0x18 */
};

/* User-visible single trophy state output (0x812D562C) */
struct TrophyEntryStateView {
	int trophyId;				/* +0x00 */
	uint32_t unlockFlags;			/* +0x04 */
	SceRtcTick unlockedSecureTick;		/* +0x10 */
	SceRtcTick unlockedLocalTick;		/* +0x18 */
	uint32_t unlocked;			/* +0x20 */
	uint32_t visible;			/* +0x24 */
};

/* Summary of TRPUSER.DAT section 0x200 */
struct TrophyUserIndexSummary {
	TrophyDualTick lastSyncAttemptTick;	/* +0x00 */
	TrophyDualTick lastSyncSuccessTick;	/* +0x10 */
	uint32_t totalTitles;			/* +0x20 */
	uint32_t installedTitles;		/* +0x24 */
	uint32_t pendingUploadTitles;		/* +0x28 */
	uint32_t deferredUploadTitles;		/* +0x2C */
};

/* Transaction entry structs for TRPTRANS.DAT */
struct TrophyTransSlotHeader {
	int slotIndex;				/* +0x00 */
	int transType;				/* +0x04: 1=register, 2=unlock */
	uint8_t synced;				/* +0x08 */
	uint8_t deferred;			/* +0x09 */
};

struct TrophyTransRegisterEntry {
	int slotIndex;				/* +0x00 */
	int transType;				/* +0x04 */
	uint8_t synced;				/* +0x08 */
	uint8_t deferred;			/* +0x09 */
	SceRtcTick secureTick;			/* +0x10 */
	SceRtcTick localTick;			/* +0x18 */
	uint32_t flags;				/* +0x20 */

	void Init();
	void ResetPayload();
};

struct TrophyTransUnlockEntry {
	int slotIndex;				/* +0x00 */
	int transType;				/* +0x04 */
	uint8_t synced;				/* +0x08 */
	uint8_t deferred;			/* +0x09 */
	int trophyId;				/* +0x0C */
	int grade;				/* +0x10 */
	SceRtcTick secureTick;			/* +0x18 */
	SceRtcTick localTick;			/* +0x20 */
	uint32_t flags;				/* +0x28 */

	void Init();
	void ResetPayload();
};

/* Unlock input parameters */
struct TrophyUnlockRequest {
	int trophyId;
	TrophyAccountCache *accountCache;
	const TrophyDualTick *ticks;
	uint8_t offlineFlag;
	uint8_t deferredFlag;
};

struct TrophyUnlockActionParams {
	TrophyDualTick ticks;
	uint8_t offlineFlag;
	uint8_t deferredFlag;
};

/* Conf extraction options passed to TrophyConfSource / TrophyAssetExtractor */
struct TrophyExtractOptions {
	int userId;
	const TrophyCommId *commId;
	uint32_t parseOptions;
	int lang;
	uint32_t assetMask;
	uint32_t flags;
};

/* Extracted language record for assets in a title */
struct TrophyExtractedAssetReport {
	TrophyCommId commId;
	int defaultLang;
	int sfmLang;
	int titleIconLang;
	int groupIconLangs[32];

	void Init();
	void ResetWithCommId(const TrophyCommId *id, int lang);
	void RecordAsset(const TrophyAssetDesc *desc);
};

/* Abstract configuration source (TRP archive or network download) */
class TrophyConfSource : public TrophyHeapObject {
public:
	virtual ~TrophyConfSource() {}
	virtual int IsOpen() const = 0;
	virtual int Close() = 0;
	virtual int EstimateSpace(TrophyHandle *handle, const TrophyExtractOptions *options,
				  uint64_t *outUr0Bytes, uint64_t *outUx0Bytes) = 0;
	virtual int OpenAssetStream(TrophyHandle *handle, const char *filename,
				    TrophyInputStream **outStream) = 0;
	virtual int ExtractAssets(TrophyHandle *handle, const TrophyExtractOptions *options,
				  TrophyConfMeta *outMeta, TrophyExtractedAssetReport *outReport,
				  TrophyConfData **outDefaultConf,
				  TrophyConfHandle **outLocalizedConf) = 0;
};

/* Helper: acquire/release a locked directory path under TrophyStorageArbiter */
class TrophyDataDirGuard {
public:
	TrophyDataDirGuard() : m_arbiter(NULL) { m_path[0] = '\0'; }
	~TrophyDataDirGuard();

	int AcquireUserRoot(TrophyStorageArbiter *arbiter, int userId);
	int AcquireTitleData(TrophyStorageArbiter *arbiter, int userId, const TrophyCommId *commId);
	int AcquireTitleBackup(TrophyStorageArbiter *arbiter, int userId,
			       const TrophyCommId *commId);
	int Release();
	bool IsAcquired() const { return m_arbiter != NULL; }
	const char *GetPath() const { return m_path; }

private:
	TrophyStorageArbiter *m_arbiter;
	char m_path[256];
};

/* =========================================================================
 * Base class for journaled binary record files (0x812D8764..0x812D9539)
 * ========================================================================= */
struct TrophyJournaledCreateParams {
	uint32_t initialJournalState;
	uint32_t formatVersion;
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
	uint64_t initialSequence;
};

class TrophyJournaledRecordFile {
public:
	explicit TrophyJournaledRecordFile(uint32_t magic);
	virtual ~TrophyJournaledRecordFile();

	bool IsOpen() const { return m_recordFile.IsOpen(); }
	bool InTransaction() const { return m_txnHandle != NULL; }

	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophyRecordSectionSpec *userSections, int userSectionCount,
		   const TrophyJournaledCreateParams *params, SceSize workBufSize);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		 TrophyAccountCache *accountCache);
	int Close();

	int BeginTransaction(TrophyHandle *handle, uint32_t journalState, SceSize workBufSize);
	int CommitTransaction(TrophyHandle *handle, const TrophyDualTick *commitTicks);
	int BindAccountIfUnbound(TrophyHandle *handle, TrophyAccountCache *accountCache,
				 bool *outBound, uint64_t *outAccountId);

	int GetFormatInfo(uint32_t *outVersion, uint32_t *outFlags) const;
	int GetFormatFlags(uint32_t *outFlags) const;
	int GetJournalState(TrophyJournalState *outState) const;
	int ReadCommitTicks(TrophyHandle *handle, TrophyDualTick *outTicks);

protected:
	int WriteRecord(TrophyHandle *handle, uint32_t sectionIndex, uint32_t recordIndex,
			SceSize byteOffset, const void *srcBuf, SceSize byteCount);
	int ReadRecord(TrophyHandle *handle, uint32_t sectionIndex, uint32_t recordIndex,
		       SceSize byteOffset, void *outBuf, SceSize byteCount);
	int AppendRecord(TrophyHandle *handle, uint32_t sectionIndex, const void *recordBuf,
			 SceSize recordSize, uint32_t *outRecordIndex);
	int MoveRecord(TrophyHandle *handle, uint32_t sectionIndex, uint32_t fromIndex,
		       uint32_t toIndex);

	TrophyRecordFile m_recordFile;		/* +0x04 */
	uint32_t m_formatVersion;		/* +0x1C */
	uint32_t m_formatFlags;			/* +0x20 */
	TrophyJournalState m_journal;		/* +0x28 */
	TrophyHandle *m_txnHandle;		/* +0x38 */
	void *m_workBuf;			/* +0x3C */
	SceSize m_workBufSize;			/* +0x40 */
	uint32_t m_headerSectionIdx;		/* +0x44 */
	uint32_t m_journalSectionIdx;		/* +0x48 */

private:
	int LoadMetadata(TrophyHandle *handle, TrophyAccountCache *accountCache,
			 bool expectInTxn, SceSize workBufSize);
	int WriteJournalRecord(TrophyHandle *handle, const void *journalHost);
	static int InitHeaderCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
				      void *userCtx);
	static int InitJournalCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
				       void *userCtx);
};

/* =========================================================================
 * TRPTRANS.DAT: per-title upload transaction log (0x812D953A..0x812DB9B2)
 * ========================================================================= */
struct TrophyTransCreateParams {
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
	const TrophyCommId *commId;
	const void *serverTag;
	uint64_t initialSequence;
	uint32_t initialJournalState;
	uint32_t capacity;
};

struct TrophyTransMigrateParams {
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
	const TrophyCommId *commId;
	const void *serverTag;
	uint32_t capacity;
};

struct TrophyTransRegisterInput {
	const TrophyDualTick *ticks;
	uint8_t deferredFlag;
};

struct TrophyTransUnlockInput {
	int trophyId;
	int grade;
	TrophyAccountCache *accountCache;
	const TrophyDualTick *ticks;
	uint8_t deferredFlag;
};

class TrophyTransDataFile : public TrophyJournaledRecordFile {
public:
	TrophyTransDataFile();
	virtual ~TrophyTransDataFile() {}

	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophyTransCreateParams *params);
	int CreateSimple(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			 const TrophyTransCreateParams *params);
	int CreateMigrated(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			   TrophyTransDataFile *oldFile, const TrophyTransMigrateParams *params);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		 TrophyAccountCache *accountCache, const TrophyCommId *expectedCommId);

	int GetJournal(TrophyHandle *handle, TrophyJournalState *outJournal) const;
	int VerifyJournal(TrophyHandle *handle, const TrophyJournalState *expected) const;
	int GetCounts(TrophyHandle *handle, TrophyTransCounts *outCounts);
	int GetServerTag(TrophyHandle *handle, bool *outHasTag, void *outTagBuf);

	int AddRegisterEntry(TrophyHandle *handle, const TrophyTransRegisterInput *input);
	int AddUnlockEntry(TrophyHandle *handle, const TrophyTransUnlockInput *input);
	int RemoveUnlockByTrophyId(TrophyHandle *handle, int trophyId);
	int ClearAllDeferred(TrophyHandle *handle);

	int PopulateUploadBatch(TrophyHandle *handle, TrophySyncTitleRequestBase *outReq);
	int ApplySyncResults(TrophyHandle *handle, TrophySyncTitleResultBase *result,
			     TrophyTransCounts *outRemaining, NpTrophyFlagArray *outSyncedTrophies);

private:
	int LoadSectionsAndVerifyCommId(TrophyHandle *handle, const TrophyCommId *expectedCommId);
	int ReadHeaderCounts(TrophyHandle *handle, uint32_t *outHeaderFields);
	int WriteHeaderCounts(TrophyHandle *handle, const uint32_t *headerFields);
	int FindFreeSlot(TrophyHandle *handle, uint32_t *outStartIdx, uint32_t *outFreeIdx,
			 void *outSlotHost);
	int WriteUnlockEntry(TrophyHandle *handle, const TrophyTransUnlockEntry *entry);
	int WriteRegisterEntry(TrophyHandle *handle, const TrophyTransRegisterEntry *entry);
	int ReadSlotHeader(TrophyHandle *handle, uint32_t slotIndex,
			   TrophyTransSlotHeader *outHeader);
	int ReadRegisterEntry(TrophyHandle *handle, uint32_t slotIndex,
			      TrophyTransRegisterEntry *outEntry);
	int ReadUnlockEntry(TrophyHandle *handle, uint32_t slotIndex,
			    TrophyTransUnlockEntry *outEntry);
	int DeleteSlot(TrophyHandle *handle, uint32_t slotIndex, uint32_t expectedType);
	int FindUnlockSlotByTrophyId(TrophyHandle *handle, int trophyId, int *outSlotIndex);
	int MarkSlotSynced(TrophyHandle *handle, uint32_t slotIndex, uint32_t expectedType,
			   uint32_t *ioHeaderFields, bool *outMarked);
	int ClearDeferredForSlot(TrophyHandle *handle, uint32_t slotIndex, uint32_t expectedType,
				 uint32_t *ioHeaderFields);

	static int InitHeaderSectionCallback(void *recordBuf, SceSize recordSize,
					     uint32_t recordIndex, void *userCtx);
	static int InitSlotSectionCallback(void *recordBuf, SceSize recordSize,
					   uint32_t recordIndex, void *userCtx);

	uint32_t m_headerSection;		/* +0x4C */
	uint32_t m_slotsSection;		/* +0x50 */
	uint32_t m_capacity;			/* +0x54 */
};

/* =========================================================================
 * TRPTITLE.DAT: per-title trophy definitions & unlock progress (0x812DB9C0..0x812DF74B)
 * ========================================================================= */
struct TrophyTitleCreateParams {
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
	const TrophyCommId *commId;
	uint64_t initialSequence;
	uint64_t initialRevision;
	uint32_t initialJournalState;
	const TrophyConfMeta *confMeta;
	uint32_t titleFlags;
	const TrophyConfData *confData;
	const TrophyDualTick *installedTick;
	const TrophyDualTick *modifiedTick;
};

struct TrophyTitleMigrateParams {
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
	const TrophyCommId *commId;
	const TrophyConfMeta *confMeta;
	uint32_t titleFlags;
	const TrophyConfData *confData;
	const TrophyDualTick *modifiedTick;
};

struct TrophyProgressDirtyCache {
	uint8_t dirty;
	uint8_t bumpRevision;
	uint32_t deferredCount;
	NpTrophyFlagArray unlockedMask;
	TrophyDualTick lastUnlockedTick;
	TrophyDualTick lastSyncedTick;
	uint64_t serverTimestamp;
	struct GroupCache {
		int groupId;
		NpTrophyFlagArray unlockedMask;
		TrophyDualTick lastUnlockedTick;
	} groups[33];

	void Init();
	void ResetUnlocked();
	void ApplyTrophy(const TrophyEntryStaticMeta *meta, const TrophyUnlockRecord *unlock);
};

class TrophyTitleDataFile : public TrophyJournaledRecordFile {
public:
	TrophyTitleDataFile();
	virtual ~TrophyTitleDataFile() {}

	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophyTitleCreateParams *params);
	int CreateFresh(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			const TrophyTitleMigrateParams *params);
	int CreateMigrated(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			   TrophyTitleDataFile *oldFile, const TrophyTitleMigrateParams *params);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		 TrophyAccountCache *accountCache, const TrophyCommId *expectedCommId,
		 TrophyTitleStaticMeta *outMeta);

	int ReadTitleStaticMeta(TrophyHandle *handle, TrophyTitleStaticMeta *outMeta);
	int ReadAllGradeMasks(TrophyHandle *handle, NpTrophyFlagArray *outAllAndGrades);
	int ReadTrophyStaticMeta(TrophyHandle *handle, int trophyId,
				 TrophyEntryStaticMeta *outMeta);
	int ReadTrophyUnlockRecord(TrophyHandle *handle, int trophyId,
				   TrophyUnlockRecord *outRecord);
	int ReadTrophyUnlockState(TrophyHandle *handle, int trophyId,
				  TrophyUnlockStateInfo *outState);
	int ReadGroupStaticMeta(TrophyHandle *handle, int groupId,
				TrophyGroupStaticMeta *outMeta);
	int ReadGroupProgress(TrophyHandle *handle, int groupId,
			      TrophyGroupProgressSnapshot *outProgress);
	int ReadTitleProgress(TrophyHandle *handle, TrophyTitleProgressSnapshot *outProgress);
	int ReadSnapshot(TrophyHandle *handle, TrophyTitleDataSnapshot *outSnapshot);
	int VerifySnapshot(TrophyHandle *handle, const TrophyTitleDataSnapshot *expected);

	int UpdateModifiedTicks(TrophyHandle *handle, const TrophyDualTick *ticks);
	int RecordTitleId(TrophyHandle *handle, const char *titleId);
	int UnlockTrophy(TrophyHandle *handle, const TrophyUnlockRequest *req, uint32_t options,
			 TrophyUnlockDelta *outTrophyDelta, TrophyUnlockDelta *outPlatinumDelta);
	int LockTrophy(TrophyHandle *handle, int trophyId, uint32_t options,
		       TrophyUnlockDelta *outTrophyDelta, TrophyUnlockDelta *outPlatinumDelta);
	int MergeSyncResult(TrophyHandle *handle, TrophyAccountCache *accountCache,
			    TrophySyncTitleResultBase *syncResult, const TrophyDualTick *syncTicks,
			    uint32_t *outUnrecognizedTrophyCount, TrophyUnlockDelta *outPlatinumDelta);
	int ClearAllDeferredFlags(TrophyHandle *handle);

private:
	int LoadSections(TrophyHandle *handle, const TrophyCommId *expectedCommId,
			 TrophyTitleStaticMeta *outMeta);
	int LoadSectionIndices();
	int ReadSummaryMeta(TrophyHandle *handle, TrophyTitleSummaryMeta *outSummary);
	int ReadProgressTicksAndRevision(TrophyHandle *handle, TrophyDualTick *outLastUnlocked,
					 uint64_t *outRevision);
	int LoadDirtyCache(TrophyHandle *handle, const uint32_t *groupMask,
			   TrophyProgressDirtyCache *outCache);
	int FlushDirtyCache(TrophyHandle *handle, const TrophyProgressDirtyCache *cache);
	int WriteTrophyUnlock(TrophyHandle *handle, const TrophyEntryStaticMeta *meta,
			      const TrophyUnlockRecord *unlock, uint32_t options,
			      TrophyProgressDirtyCache *cache);
	int UnlockTrophyInternal(TrophyHandle *handle, const TrophyUnlockRequest *req,
				 const TrophyEntryStaticMeta *meta, uint32_t options,
				 TrophyProgressDirtyCache *cache, TrophyUnlockDelta *outDelta);
	int LockTrophyInternal(TrophyHandle *handle, const TrophyEntryStaticMeta *meta,
			       uint32_t options, TrophyProgressDirtyCache *cache,
			       TrophyUnlockDelta *outDelta);
	int ReconcilePlatinum(TrophyHandle *handle, TrophyAccountCache *accountCache,
			      const TrophyDualTick *ticks, int platinumId, uint32_t options,
			      TrophyProgressDirtyCache *cache, TrophyUnlockDelta *outDelta);

	static int InitConfMetaCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
					void *userCtx);
	static int InitTitleIdCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
				       void *userCtx);
	static int InitTitleMetaCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
					 void *userCtx);
	static int InitGroupMetaCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
					 void *userCtx);
	static int InitTrophyMetaCallback(void *recordBuf, SceSize recordSize,
					  uint32_t recordIndex, void *userCtx);
	static int InitProgressCallback(void *recordBuf, SceSize recordSize, uint32_t recordIndex,
					void *userCtx);
	static int InitGroupProgressCallback(void *recordBuf, SceSize recordSize,
					     uint32_t recordIndex, void *userCtx);
	static int InitTrophyUnlockCallback(void *recordBuf, SceSize recordSize,
					    uint32_t recordIndex, void *userCtx);

	uint32_t m_confMetaSection;		/* +0x4C */
	uint32_t m_titleIdSection;		/* +0x50 */
	uint32_t m_titleMetaSection;		/* +0x54 */
	uint32_t m_groupMetaSection;		/* +0x58 */
	uint32_t m_trophyMetaSection;		/* +0x5C */
	uint32_t m_progressSection;		/* +0x60 */
	uint32_t m_groupProgressSection;	/* +0x64 */
	uint32_t m_trophyUnlockSection;		/* +0x68 */
};

/* =========================================================================
 * TRPPARAM.INI wrapper (0x812DF74C..0x812DFAC3)
 * ========================================================================= */
struct TrophyParamIniValues {
	uint32_t sysVersion;
	const TrophyCommId *commId;
	uint32_t appVersion;
	int lang;
};

class TrophyParamIniFile {
public:
	TrophyParamIniFile();
	~TrophyParamIniFile();

	bool IsOpen() const { return m_isOpen != 0; }
	int Create(const char *path, const TrophyParamIniValues *values);
	int Open(const char *path, const TrophyCommId *expectedCommId);
	int ReadAppVersion(uint32_t *outVersion);
	int Flush();
	int Close();

private:
	uint8_t m_isOpen;
	IniFileProcessor m_processor;
};

/* =========================================================================
 * TRPUSER.DAT: per-user title directory index (0x812DFAC4..0x812E1113)
 * ========================================================================= */
struct TrophyUserIndexCreateParams {
	uint64_t initialSequence;
	uint32_t formatFlags;
	const SceKernelOpenPsId *openPsId;
	TrophyAccountCache *accountCache;
};

class TrophyUserIndexFile : public TrophyJournaledRecordFile {
public:
	TrophyUserIndexFile();
	virtual ~TrophyUserIndexFile() {}

	int Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		   const TrophyUserIndexCreateParams *params);
	int Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
		 TrophyAccountCache *accountCache);

	int SetNeedsRecoveryFlag(TrophyHandle *handle);
	int ClearNeedsRecoveryFlag(TrophyHandle *handle);
	int GetNeedsRecoveryFlag(TrophyHandle *handle, bool *outNeedsRecovery);

	int ReadSummary(TrophyHandle *handle, TrophyUserIndexSummary *outSummary);
	int WriteSummary(TrophyHandle *handle, const TrophyUserIndexSummary *summary);
	int UpdateSyncAttemptTicks(TrophyHandle *handle, const SceRtcTick *attemptTick,
				   const SceRtcTick *successTick);
	int UpdateSyncSuccessTicks(TrophyHandle *handle, const SceRtcTick *attemptTick,
				   const SceRtcTick *successTick);

	int FindTitle(TrophyHandle *handle, const TrophyCommId *commId, int *outSlotIndex,
		      TrophyTitleRecord *outRecord);
	int HasTitle(TrophyHandle *handle, const TrophyCommId *commId, bool *outExists,
		     TrophyTitleRecord *outRecord);
	int GetTitle(TrophyHandle *handle, const TrophyCommId *commId,
		     TrophyTitleRecord *outRecord);
	int ReadTitleAt(TrophyHandle *handle, uint32_t slotIndex, TrophyTitleRecord *outRecord);
	int FindNextTitle(TrophyHandle *handle, uint32_t startIndex, uint32_t *outFoundIndex,
			  TrophyTitleRecord *outRecord);
	int FindOldestOrBrokenTitle(TrophyHandle *handle, TrophyTitleRecord *outRecord);

	int AddTitle(TrophyHandle *handle, TrophyTitleRecord *ioRecord);
	int UpdateTitle(TrophyHandle *handle, const TrophyTitleRecord *record);
	int RemoveTitle(TrophyHandle *handle, const TrophyCommId *commId, int *outRemovedSlot);

	int ReadJournal(TrophyJournalState *outJournal) const;

private:
	int LoadSections();
	int ReadEntryRaw(TrophyHandle *handle, uint32_t slotIndex, void *outHostEntry);
	int WriteEntryRaw(TrophyHandle *handle, uint32_t slotIndex, const void *hostEntry);
	int AppendEntryRaw(TrophyHandle *handle, const void *hostEntry, uint32_t *outSlotIndex);

	uint32_t m_flagsSection;		/* +0x4C */
	uint32_t m_summarySection;		/* +0x50 */
	uint32_t m_entriesSection;		/* +0x54 */
};

/* =========================================================================
 * Signed SFM parsing, text extraction & asset extraction (0x812D6780..0x812D7EA7)
 * ========================================================================= */
class TrophyConfStream : public TrophyConfFile {
public:
	TrophyConfStream();
	virtual ~TrophyConfStream();

	int OpenSfm(TrophyHandle *handle, int userId, const TrophyCommId *commId, int lang,
		    bool fallbackDefault);
	virtual int Close();
	int GetStrippedStream(TrophyHandle *handle, TrophyInputStream **outStream);

private:
	TrophySfmSignatureStripper m_stripper;
};

class TrophySignedSfmParser : public TrophySfmParser {
public:
	TrophySignedSfmParser();
	virtual ~TrophySignedSfmParser();

	int InitSigned(uint32_t parseOptions);
	int ParseSignedStream(TrophyHandle *handle, TrophySfmSignatureStripper *stripper);
	int TakeSignedConfData(const TrophyCommId *expectedCommId, TrophyConfData **outConfData);

private:
	TrophySfmSignatureVerifier *m_verifier;
	TrophySfmSignature *m_signatureCopy;
};

class TrophySignedSfmStream : public TrophyHeapObject {
public:
	TrophySignedSfmStream();
	virtual ~TrophySignedSfmStream();

	int Init(uint32_t parseOptions, TrophyInputStream *input, TrophyOutputStream *output,
		 TrophyHashSink *hashSink);
	int Parse(TrophyHandle *handle);
	int TakeConfHandle(const TrophyCommId *expectedCommId, int lang,
			   TrophyConfHandle **outHandle);
	uint64_t GetTotalBytes() const { return m_teeStream.GetTotalBytes(); }

private:
	TrophyTeeStream m_teeStream;
	TrophySfmSignatureStripper m_stripper;
	TrophySignedSfmParser m_parser;
};

class TrophySignedSfmReader : public TrophyHeapObject {
public:
	TrophySignedSfmReader();
	virtual ~TrophySignedSfmReader();

	int Init(uint32_t parseOptions);
	int Parse(TrophyHandle *handle, TrophyInputStream *input);
	int TakeConfHandle(const TrophyCommId *expectedCommId, int lang,
			   TrophyConfHandle **outHandle);
	int TakeConfData(const TrophyCommId *expectedCommId, TrophyConfData **outConfData);

private:
	TrophySfmSignatureStripper m_stripper;
	TrophySignedSfmParser m_parser;
};

/* Decodes XML character entities from an SFM byte range into a UTF-8 buffer */
int npTrophyReadDecodedSfmText(TrophyHandle *handle, TrophyFileBase *file, uint64_t offset,
			       SceSize byteLength, char *outText, SceSize outSize);
int npTrophyExtractTitleTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				 const TrophyConfHandle *confHandle, void *outTitleDetails);
int npTrophyExtractGroupTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				 const TrophyConfHandle *confHandle, int groupId,
				 void *outGroupDetails);
int npTrophyExtractTrophyTextInfo(TrophyHandle *handle, TrophyFileBase *file,
				  const TrophyConfHandle *confHandle, int trophyId,
				  bool revealDetails, void *outTrophyDetails);

/* Single-asset extractor used during TRP / network install */
class TrophyAssetExtractor : public TrophyHeapObject {
public:
	TrophyAssetExtractor();
	virtual ~TrophyAssetExtractor();

	int Begin(const char *filename, TrophyInputStream *input,
		  const TrophyExtractOptions *options, TrophyHashSink *hashSink);
	bool IsActive() const { return m_sfmStream != NULL || m_teeStream != NULL; }
	int Process(TrophyHandle *handle, TrophyAssetDesc *outDesc);
	int TakeConfHandle(TrophyConfHandle **outHandle);
	int End();
	uint64_t GetBytesProcessed() const;

	static bool ShouldExtractAsset(const char *filename, const TrophyExtractOptions *options);

private:
	TrophyCommId m_commId;
	TrophyAssetDesc m_assetDesc;
	char m_filename[32];
	char m_destPath[256];
	TrophyFile m_destFile;
	TrophySignedSfmStream *m_sfmStream;
	TrophyTeeStream *m_teeStream;
};

/* =========================================================================
 * Handle pool entry & cancel fan-out (0x812D7D04..0x812D7EA7)
 * ========================================================================= */
class TrophyHandlePoolEntry : public TrophyHeapObject {
public:
	TrophyHandlePoolEntry();
	virtual ~TrophyHandlePoolEntry();

	int InitHandle(int handleKind);
	bool IsServiceHandle() const { return m_handleKind == 1; }
	TrophyHandle *GetHandle() const { return m_handle; }

	int m_slotId;
	uint8_t m_inUse;
	uint8_t m_reservedState;
	uint8_t m_aborted;
	int m_ownerTag;
	uint8_t m_activeFlag;
	void *m_ownerData;
	void *m_extraData;
	void **m_backPointer;
	int m_handleKind;
	TrophyHandle *m_handle;
};

struct TrophyHandlePoolTable {
	TrophyLwMutex *mutex;
	TrophyHandlePoolEntry *entries[17];
	uint32_t capacity;

	void CancelAll(int errorCode);
	void ClearAllSuspended();
};

/* =========================================================================
 * SNS feed publisher & network title sync batch (0x812D7EF8..0x812D8763)
 * ========================================================================= */
bool npTrophyIsSnsFeedEnabled();

struct TrophySnsFeedItem {
	int hidden;
	int grade;
	const char *name;
	const char *detail;
};

class TrophySnsFeedPublisher : public TrophyHeapObject {
public:
	TrophySnsFeedPublisher() : m_snsContext(0) {}
	virtual ~TrophySnsFeedPublisher();

	int Publish(TrophyHandle *handle, const char *titleName, const char *iconUrl,
		    const TrophySnsFeedItem *items, int itemCount);
	static int PublishBatch(TrophyHandle *handle, const char *titleName, const char *iconUrl,
				const TrophySnsFeedItem *items, int itemCount);

private:
	int m_snsContext;
};

/* Per-title transaction upload record linking local TRPTRANS slot to server sync */
struct TrophyTransBatchEntry {
	int slotIndex;
	int transType;
	int trophyId;
};

/* =========================================================================
 * Conf, User, Index & Title storage controllers (0x812D00C0..0x812D677F)
 * ========================================================================= */
struct TrophyBrokenMarker {
	uint32_t reason;
	int errorCode;
};

class TrophyConfStorage : public TrophyHeapObject {
public:
	TrophyConfStorage(int userId, const TrophyCommId *commId);
	virtual ~TrophyConfStorage();

	static int DetermineAssetMask(TrophyHandle *handle, int userId, uint32_t *outMask);
	static int EstimateInstallSpace(TrophyHandle *handle, TrophyConfSource *source,
					int userId, const TrophyCommId *commId, int lang,
					uint32_t assetMask, uint64_t *outUr0, uint64_t *outUx0);

	int InstallFromSource(TrophyHandle *handle, TrophyConfSource *source, int lang,
			      uint32_t assetMask, TrophyConfMeta *outMeta,
			      TrophyConfData **outDefaultConf,
			      TrophyConfHandle **outLocalizedConf);
	int Open(TrophyHandle *handle);
	int Close();
	bool IsOpen() const { return m_ur0ParamIni.IsOpen(); }
	int NeedsUx0Refresh(TrophyHandle *handle, int userId, const TrophyCommId *commId,
			    bool *outNeedsRefresh);

	int CheckBrokenMarker(TrophyHandle *handle, bool *outBroken);
	int WriteBrokenMarker(TrophyHandle *handle, const TrophyBrokenMarker *marker);

	int OpenSfmStream(TrophyHandle *handle, TrophyConfStream *stream);
	int ReadConfData(TrophyHandle *handle, uint32_t parseOptions, TrophyConfData **outConf);
	int ReadConfHandle(TrophyHandle *handle, TrophyConfHandle **outHandle);
	int ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			     void *outTitleDetails);
	int ExtractTrophyText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			      int trophyId, bool revealDetails, void *outTrophyDetails);

	int OpenTitleIcon(TrophyHandle *handle, TrophyConfIconFile *iconFile);
	int OpenGroupIcon(TrophyHandle *handle, int groupId, TrophyConfIconFile *iconFile);
	int OpenTrophyIcon(TrophyHandle *handle, int trophyId, TrophyConfIconFile *iconFile);

	static int RemoveDirectories(int userId, const TrophyCommId *commId);
	static int BackupDirectories(int userId, const TrophyCommId *commId,
				     uint8_t *outBackupFlags);
	static int RestoreDirectories(int userId, const TrophyCommId *commId,
				      uint8_t *ioBackupFlags);

private:
	static bool IsConfCorruptionError(int err);

	int m_userId;
	const TrophyCommId *m_commId;
	TrophyParamIniFile m_ur0ParamIni;
	TrophyParamIniFile m_ux0ParamIni;
};

struct TrophyUserStorageInstallArgs {
	const TrophyDualTick *ticks;
	const TrophyConfMeta *confMeta;
	uint32_t titleFlags;
	const TrophyConfData *confData;
	const void *serverTag;
	uint8_t deferredFlag;
};

class TrophyUserStorage : public TrophyHeapObject {
public:
	TrophyUserStorage(TrophyStorageArbiter *arbiter, const SceKernelOpenPsId *openPsId,
			  int userId, const TrophyCommId *commId);
	virtual ~TrophyUserStorage();

	int Create(TrophyHandle *handle, const TrophyUserStorageInstallArgs *args,
		   uint32_t fileMask);
	int CreateMigrated(TrophyHandle *handle, const TrophyUserStorageInstallArgs *args,
			   uint32_t fileMask, TrophyUserStorage *oldStorage);
	int Open(TrophyHandle *handle, uint32_t openFlags, int accessMode);
	int Close();
	bool IsOpen() const;

	int ReadTitleRecord(TrophyHandle *handle, TrophyTitleRecord *outRecord);
	int VerifyTitleRecord(TrophyHandle *handle, const TrophyTitleRecord *expected);
	int BeginTransactions(TrophyHandle *handle, uint32_t journalState, uint32_t fileMask);
	int CommitTransactions(TrophyHandle *handle, const TrophyDualTick *ticks,
			       TrophyTitleRecord *outRecord);

	int ApplySyncSession(TrophyHandle *handle, const TrophyDualTick *ticks,
			     TrophySyncSessionBase *syncSession, uint32_t *outSyncFlags,
			     NpTrophyFlagArray *outSyncedTrophies, int *outUnlockedPlatinumId);
	int ApplyRegisterAndSync(TrophyHandle *handle, const TrophyDualTick *ticks,
				 const char *titleId, bool keepDeferred,
				 TrophySyncSessionBase *syncSession, uint32_t *outSyncFlags,
				 NpTrophyFlagArray *outSyncedTrophies, int *outUnlockedPlatinumId);

	int UnlockTrophy(TrophyHandle *handle, int trophyId,
			 const TrophyUnlockActionParams *params, uint32_t options,
			 int *outPlatinumId);
	int LockTrophy(TrophyHandle *handle, int trophyId, uint32_t options, int *outPlatinumId);
	int UnlockAllTrophies(TrophyHandle *handle, const TrophyUnlockActionParams *params,
			      uint32_t options, int *outPlatinumId);
	int LockAllTrophies(TrophyHandle *handle, uint32_t options, int *outPlatinumId);
	int UnlockGroupTrophies(TrophyHandle *handle, int groupId,
				const TrophyUnlockActionParams *params, uint32_t options,
				int *outPlatinumId);
	int LockGroupTrophies(TrophyHandle *handle, int groupId, uint32_t options,
			      int *outPlatinumId);

	int GetGroupMask(TrophyHandle *handle, uint32_t *outGroupMask);
	int GetUnlockedAndAllMasks(TrophyHandle *handle, NpTrophyFlagArray *outUnlocked,
				   NpTrophyFlagArray *outAll);
	int GetTitleProgressView(TrophyHandle *handle, void *outStaticInfo,
				 TrophyTitleProgressView *outProgress);
	int GetGroupProgressView(TrophyHandle *handle, int groupId, void *outStaticInfo,
				 TrophyGroupProgressView *outProgress);
	int GetTrophyStateView(TrophyHandle *handle, int trophyId, void *outStaticInfo,
			       TrophyEntryStateView *outState);
	int GetGameProgressInfo(TrophyHandle *handle, void *outGameDetails);
	int GetGroupProgressInfo(TrophyHandle *handle, int groupId, void *outGroupDetails);
	int GetTrophyUnlockInfo(TrophyHandle *handle, int trophyId, bool *outVisible,
				void *outTrophyData);

	int PrepareUploadSyncSession(TrophyHandle *handle, TrophySyncSessionBase *syncSession);
	int CheckBrokenMarker(TrophyHandle *handle, bool *outBroken);
	int WriteBrokenMarker(TrophyHandle *handle, const TrophyBrokenMarker *marker);

	static int RemoveDirectory(int userId, const TrophyCommId *commId);
	static int BackupDirectory(int userId, const TrophyCommId *commId, uint8_t *outBackedUp);
	static int RestoreDirectory(int userId, const TrophyCommId *commId, uint8_t *ioBackedUp);

private:
	int CreateTransFile(TrophyHandle *handle, uint32_t formatFlags, const void *serverTag);
	int CreateTitleFile(TrophyHandle *handle, uint32_t formatFlags,
			    const TrophyDualTick *ticks, const TrophyConfMeta *confMeta,
			    uint32_t titleFlags, const TrophyConfData *confData);
	int OpenTransFile(TrophyHandle *handle, int accessMode);
	int OpenTitleFile(TrophyHandle *handle, int accessMode);

	const SceKernelOpenPsId *m_openPsId;
	TrophyStorageArbiter *m_arbiter;
	int m_userId;
	const TrophyCommId *m_commId;
	int m_accessMode;
	uint32_t m_activeTxnMask;
	uint32_t m_formatFlags;
	TrophyTitleDataFile m_titleFile;
	TrophyTransDataFile m_transFile;
	TrophyDataDirGuard m_dirGuard;
};

class TrophyUserIndexStorage : public TrophyHeapObject {
public:
	TrophyUserIndexStorage(TrophyStorageArbiter *arbiter, const SceKernelOpenPsId *openPsId,
			       int userId);
	virtual ~TrophyUserIndexStorage();

	int Open(TrophyHandle *handle, int accessMode, const TrophyDualTick *commitTicks);
	int Close();
	bool IsOpen() const { return m_isOpen != 0; }
	int RemoveIndexFile();

	int ClearNeedsRecovery(TrophyHandle *handle);
	int GetNeedsRecovery(TrophyHandle *handle, bool *outNeedsRecovery);
	int HasTitle(TrophyHandle *handle, const TrophyCommId *commId, bool *outExists,
		     TrophyTitleRecord *outRecord);
	int RemoveTitle(TrophyHandle *handle, const TrophyCommId *commId);
	int AddTitle(TrophyHandle *handle, TrophyTitleRecord *ioRecord);
	int GetTitle(TrophyHandle *handle, const TrophyCommId *commId,
		     TrophyTitleRecord *outRecord);
	int UpdateTitle(TrophyHandle *handle, const TrophyTitleRecord *record,
			bool markNeedsRecovery);
	int FindOldestOrBrokenTitle(TrophyHandle *handle, TrophyTitleRecord *outRecord);
	int HasPendingUploads(TrophyHandle *handle, bool *outHasPending);
	int GetJournalStatus(TrophyHandle *handle, void *outStatus);
	int GetSyncTicks(TrophyHandle *handle, void *outTicks);
	int GetCounts(TrophyHandle *handle, void *outCounts);
	int EnumerateTitle(TrophyHandle *handle, uint32_t startIndex, uint32_t *outFoundIndex,
			   void *outTitleInfo, void *outSyncInfo);
	int RecordSyncAttempt(TrophyHandle *handle, const SceRtcTick *tick, int syncResult);
	int RecordSyncSuccess(TrophyHandle *handle, const SceRtcTick *tick, int syncResult);

private:
	int HasAnyInstalledTitleDir(TrophyHandle *handle, int userId, bool *outHasAny);
	int CreateFresh(TrophyHandle *handle, const char *path, const TrophyDualTick *commitTicks);

	const SceKernelOpenPsId *m_openPsId;
	TrophyStorageArbiter *m_arbiter;
	int m_userId;
	TrophyUserIndexFile m_indexFile;
	uint8_t m_isOpen;
	TrophyDataDirGuard m_dirGuard;
};

struct TrophyTitleBackupState {
	uint8_t hasIndexRecord;
	TrophyTitleRecord savedRecord;
	uint8_t confBackupFlags[2];
	uint8_t dataBackedUp;
};

struct TrophyDebugTrophyTarget {
	int scope;	/* 1=all, 2=group, 3=single */
	int targetId;
};

class TrophyTitleStorage : public TrophyHeapObject {
public:
	TrophyTitleStorage(TrophyUserSession *session, const TrophyCommId *commId);
	virtual ~TrophyTitleStorage();

	bool IsOpen() const { return m_titleRecord != NULL; }
	int EnsureNotInstalledOrRemove(TrophyHandle *handle, const TrophyCommId *commId);
	int Open(TrophyHandle *handle, uint32_t openFlags);
	int Close();
	int MarkBroken(TrophyHandle *handle, int brokenKind);
	int Uninstall(TrophyHandle *handle);

	int InstallFromContext(TrophyHandle *handle, TrophyContext *context,
			       TrophyConfSource *source, int lang, int *outPlatinumId);
	int UpgradeFromContext(TrophyHandle *handle, TrophyContext *context,
			       TrophyConfSource *source, int lang, int *outPlatinumId);
	int InstallFromNetwork(TrophyHandle *handle, TrophyConfSource *source, int lang);
	int RegisterExistingContext(TrophyHandle *handle, TrophyContext *context,
				    TrophyConfSource *source, int lang, int *outPlatinumId);

	int UnlockTrophy(TrophyHandle *handle, TrophyContext *context, int trophyId,
			 int *outPlatinumId);
	int DebugUnlock(TrophyHandle *handle, const TrophyDebugTrophyTarget *target,
			int *outPlatinumId);
	int DebugLock(TrophyHandle *handle, const TrophyDebugTrophyTarget *target,
		      int *outPlatinumId);
	int SyncUpload(TrophyHandle *handle, uint32_t *outSyncFlags, int *outPlatinumId);
	int SyncDownload(TrophyHandle *handle, uint32_t *outSyncFlags, int *outPlatinumId);

	int GetGroupMask(TrophyHandle *handle, uint32_t *outGroupMask);
	int GetUnlockedAndAllMasks(TrophyHandle *handle, NpTrophyFlagArray *outUnlocked,
				   NpTrophyFlagArray *outAll);
	int GetTitleInfo(TrophyHandle *handle, void *outTitleInfo);
	int GetSyncInfo(TrophyHandle *handle, void *outSyncInfo);
	int GetTitleProgressView(TrophyHandle *handle, void *outStaticInfo,
				 TrophyTitleProgressView *outProgress);
	int GetGroupProgressView(TrophyHandle *handle, int groupId, void *outStaticInfo,
				 TrophyGroupProgressView *outProgress);
	int GetTrophyStateView(TrophyHandle *handle, int trophyId, void *outStaticInfo,
			       TrophyEntryStateView *outState);
	int GetGameProgressInfo(TrophyHandle *handle, void *outGameDetails);
	int GetGroupProgressInfo(TrophyHandle *handle, int groupId, void *outGroupDetails);
	int GetTrophyUnlockInfo(TrophyHandle *handle, int trophyId, bool *outVisible,
				void *outTrophyData);

	int ReadConfHandle(TrophyHandle *handle, TrophyConfHandle **outHandle);
	int ExtractTitleText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			     void *outTitleDetails);
	int ExtractTrophyText(TrophyHandle *handle, const TrophyConfHandle *confHandle,
			      int trophyId, bool revealDetails, void *outTrophyDetails);
	int OpenSfmStream(TrophyHandle *handle, TrophyConfStream *stream);
	int OpenTitleIcon(TrophyHandle *handle, TrophyConfIconFile *iconFile);
	int OpenGroupIcon(TrophyHandle *handle, int groupId, TrophyConfIconFile *iconFile);
	int OpenTrophyIcon(TrophyHandle *handle, int trophyId, TrophyConfIconFile *iconFile);

private:
	int PrepareComponents();
	void DestroyComponents();
	int EnsureOpenAndSessionReady(TrophyHandle *handle);
	int PublishUnlockedToSns(TrophyHandle *handle, const NpTrophyFlagArray *unlockedMask);
	int BuildUploadSyncSession(TrophyHandle *handle, TrophySyncSessionBase **outSession);
	int BuildInitialSyncSession(TrophyHandle *handle, TrophySyncSessionBase **outSession);
	int BuildPeriodicSyncSession(TrophyHandle *handle, const TrophyDualTick *now,
				     TrophySyncSessionBase **outSession);
	int BuildDownloadSyncSession(TrophyHandle *handle, TrophySyncSessionBase **outSession);
	int BackupForUpgrade(TrophyHandle *handle, TrophyTitleBackupState *outBackup);
	int RestoreFromBackup(TrophyHandle *handle, TrophyTitleBackupState *backup);
	int BuildUnlockParams(TrophyContext *context, bool forceDebug,
			      TrophyUnlockActionParams *outParams, bool *outDisallowUnlock);
	int UnlockSingleInternal(TrophyHandle *handle, TrophyContext *context, int trophyId,
				 bool forceDebug, int *outPlatinumId);
	int UnlockGroupInternal(TrophyHandle *handle, TrophyContext *context, int groupId,
				bool forceDebug, int *outPlatinumId);
	int UnlockAllInternal(TrophyHandle *handle, TrophyContext *context, bool forceDebug,
			      int *outPlatinumId);
	int LockSingleInternal(TrophyHandle *handle, int trophyId, int *outPlatinumId);
	int LockGroupInternal(TrophyHandle *handle, int groupId, int *outPlatinumId);
	int LockAllInternal(TrophyHandle *handle, int *outPlatinumId);

	static bool IsDataCorruptionError(int err);
	static int CheckFreeSpace(uint64_t requiredUr0, uint64_t requiredUx0);

	TrophyUserSession *m_session;
	TrophyTitleRecord *m_titleRecord;
	TrophyUserStorage *m_userStorage;
	TrophyConfStorage *m_confStorage;
	TrophyCommId m_commId;
};

/* Serialization helpers shared across data files (0x812D935C..0x812D952F) */
void npTrophyUnpackBitset128(const uint8_t *packed32, NpTrophyFlagArray *outBits);
void npTrophyPackBitset128(uint8_t *outPacked32, const NpTrophyFlagArray *bits);
void npTrophyUnpackBitset32(const uint8_t *packed16, uint32_t *outMask);
void npTrophyPackBitset32(uint8_t *outPacked16, uint32_t mask);
int npTrophyGenerateInitialSequence(uint64_t *outSequence);

/* Static info export helpers (0x812DEC28..0x812DED9F) */
int npTrophyComputeProgressFromBitsets(const TrophyGradeBitsets *gradeBits,
				       const TrophyGradeCounts *unlockedCounts);
void npTrophyExportTitleStaticInfo(const TrophyTitleStaticMeta *src, void *dst);
void npTrophyExportGroupStaticInfo(const TrophyGroupStaticMeta *src, void *dst);
void npTrophyExportTrophyStaticInfo(const TrophyEntryStaticMeta *src, void *dst);

/* Batch 3 / Batch 4 bridge declarations used by Batch 2 storage layer */
extern "C" {
int vshSblSsIsDevelopmentMode(void);
int vshRtcGetCurrentSecureTick(SceRtcTick *tick);
int SceNpManager_A775A452(void);
}

/* Batch 4 service / arbiter / context bridge helpers */
bool npTrophyServiceIsSyncInGameDisabled();
int npTrophyUserSessionEnsureReady(TrophyUserSession *session, TrophyHandle *handle, int mode);
int npTrophyUserSessionGetUserId(const TrophyUserSession *session);
TrophyStorageArbiter *npTrophyUserSessionGetArbiter(const TrophyUserSession *session);
TrophyUserIndexStorage *npTrophyUserSessionGetIndexStorage(const TrophyUserSession *session);
TrophyNetClient *npTrophyUserSessionGetNetClient(const TrophyUserSession *session);
const SceKernelOpenPsId *npTrophyUserSessionGetOpenPsId(const TrophyUserSession *session);
int npTrophyUserSessionGetAccessMode(const TrophyUserSession *session);
bool npTrophyUserSessionIsWritable(const TrophyUserSession *session);
int npTrophyUserSessionPurgeTitle(TrophyUserSession *session, TrophyHandle *handle,
				  const TrophyCommId *commId);

int npTrophyArbiterLockUserRoot(TrophyStorageArbiter *arbiter, int userId, char *outPath,
				SceSize outSize);
int npTrophyArbiterLockTitleData(TrophyStorageArbiter *arbiter, int userId,
				 const TrophyCommId *commId, char *outPath, SceSize outSize);
int npTrophyArbiterLockTitleBackup(TrophyStorageArbiter *arbiter, int userId,
				   const TrophyCommId *commId, char *outPath, SceSize outSize);
int npTrophyArbiterUnlockPath(TrophyStorageArbiter *arbiter, const char *path);

int npTrophyContextGetServerTag(const TrophyContext *context, const void **outTag);
int npTrophyContextGetTrialFlag(const TrophyContext *context, uint8_t *outIsTrial);
int npTrophyContextGetUnlockAllowed(const TrophyContext *context, uint8_t *outAllowed);
int npTrophyContextGetOfflineFlag(const TrophyContext *context, uint8_t *outOffline);
const char *npTrophyContextGetTitleId(const TrophyContext *context);

/* SceNpSnsFb internal bridge helpers (0x81338C08, 0x81338C26, 0x81339D6A, 0x8133CC14) */
int npSnsFbIsEnabled(int *outEnabled);
int npSnsFbCreateContext();
int npSnsFbDestroyContext(int contextId);
int npSnsFbPostTrophyFeed(int contextId, const char *titleName, const char *iconUrl,
			  const TrophySnsFeedItem *items, int itemCount, int *outServerError);

#endif /* SHELL_NP_TROPHY_STORAGE_H */
