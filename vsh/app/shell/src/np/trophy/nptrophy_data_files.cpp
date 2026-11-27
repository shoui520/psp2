/*
 * nptrophy_data_files.cpp: journaled binary record files for SceNpTrophy
 * (TrophyJournaledRecordFile, TRPTRANS.DAT, TRPTITLE.DAT, TRPPARAM.INI,
 * TRPUSER.DAT; 0x812D8764..0x812E1113).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include "nptrophy_storage.h"

extern "C" {
extern uint32_t sceNetHtonl(uint32_t host32);
extern uint32_t sceNetNtohl(uint32_t net32);
extern uint64_t sceNetHtonll(uint64_t host64);
extern uint64_t sceNetNtohll(uint64_t net64);
}

namespace {

/* On-disk header record for section 1 of every journaled record file (48 bytes) */
struct JournaledHeaderDisk {
	uint32_t formatVersionBe;
	uint32_t formatFlagsBe;
	uint8_t reservedTail[40];
};

/* Host representation of section 2 (journal & account binding, 96 bytes) */
struct JournalSectionHost {
	uint32_t state;
	uint32_t flags;
	uint64_t sequence;
	uint64_t commitSecureTick;
	uint64_t commitLocalTick;
	uint64_t accountId;
	uint8_t reservedMid[8];
	uint8_t openPsId[16];
	uint8_t reservedTail[32];
};

/* On-disk representation of section 2 (96 bytes) */
struct JournalSectionDisk {
	uint32_t stateBe;
	uint32_t flagsBe;
	uint64_t sequenceBe;
	uint64_t commitSecureTickBe;
	uint64_t commitLocalTickBe;
	uint64_t accountIdBe;
	uint8_t reservedMid[8];
	uint8_t openPsId[16];
	uint8_t reservedTail[32];
};

static void encodeJournalSection(const JournalSectionHost *host, JournalSectionDisk *disk)
{
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->stateBe = sceNetHtonl(host->state);
	disk->flagsBe = sceNetHtonl(host->flags);
	disk->sequenceBe = sceNetHtonll(host->sequence);
	disk->commitSecureTickBe = sceNetHtonll(host->commitSecureTick);
	disk->commitLocalTickBe = sceNetHtonll(host->commitLocalTick);
	disk->accountIdBe = sceNetHtonll(host->accountId);
	sceClibMemcpy_safe(disk->openPsId, host->openPsId, sizeof(disk->openPsId));
}

static void decodeJournalSection(const JournalSectionDisk *disk, JournalSectionHost *host)
{
	sceClibMemset(host, 0, sizeof(*host));
	host->state = sceNetNtohl(disk->stateBe);
	host->flags = sceNetNtohl(disk->flagsBe);
	host->sequence = sceNetNtohll(disk->sequenceBe);
	host->commitSecureTick = sceNetNtohll(disk->commitSecureTickBe);
	host->commitLocalTick = sceNetNtohll(disk->commitLocalTickBe);
	host->accountId = sceNetNtohll(disk->accountIdBe);
	sceClibMemcpy_safe(host->openPsId, disk->openPsId, sizeof(host->openPsId));
}

/* =========================================================================
 * TRPTRANS.DAT on-disk records (section 0x100: 320B, section 0x200: 160B)
 * ========================================================================= */
struct TransHeaderRecord {
	char commIdText[16];
	uint32_t capacity;
	uint32_t usedCount;
	uint32_t syncedCount;
	uint32_t deferredCount;
	uint8_t serverTag[256];
	uint8_t reservedTail[32];
};

struct TransSlotRecord {
	uint32_t slotIndex;
	uint32_t transType;
	uint32_t flags;
	uint32_t reservedHeader[5];
	union {
		struct {
			uint64_t secureTick;
			uint64_t localTick;
			uint32_t registerFlags;
		} reg;
		struct {
			uint32_t trophyId;
			uint32_t grade;
			uint32_t unlockFlags;
			uint32_t reservedMid;
			uint64_t secureTick;
			uint64_t localTick;
		} unlock;
		uint8_t raw[128];
	} body;
};

static void encodeTransHeader(const TransHeaderRecord *host, TransHeaderRecord *disk)
{
	sceClibMemset(disk, 0, sizeof(*disk));
	sceClibMemcpy_safe(disk->commIdText, host->commIdText, sizeof(disk->commIdText));
	disk->capacity = sceNetHtonl(host->capacity);
	disk->usedCount = sceNetHtonl(host->usedCount);
	disk->syncedCount = sceNetHtonl(host->syncedCount);
	disk->deferredCount = sceNetHtonl(host->deferredCount);
	sceClibMemcpy_safe(disk->serverTag, host->serverTag, sizeof(disk->serverTag));
}

static void decodeTransHeader(const TransHeaderRecord *disk, TransHeaderRecord *host)
{
	sceClibMemset(host, 0, sizeof(*host));
	sceClibMemcpy_safe(host->commIdText, disk->commIdText, sizeof(host->commIdText));
	host->capacity = sceNetNtohl(disk->capacity);
	host->usedCount = sceNetNtohl(disk->usedCount);
	host->syncedCount = sceNetNtohl(disk->syncedCount);
	host->deferredCount = sceNetNtohl(disk->deferredCount);
	sceClibMemcpy_safe(host->serverTag, disk->serverTag, sizeof(host->serverTag));
}

/* 0x812D9AA4 */
static void encodeTransSlot(const TransSlotRecord *host, TransSlotRecord *disk)
{
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->slotIndex = sceNetHtonl(host->slotIndex);
	disk->transType = sceNetHtonl(host->transType);
	disk->flags = sceNetHtonl(host->flags);
	if (host->transType == 1) {
		disk->body.reg.secureTick = sceNetHtonll(host->body.reg.secureTick);
		disk->body.reg.localTick = sceNetHtonll(host->body.reg.localTick);
		disk->body.reg.registerFlags = sceNetHtonl(host->body.reg.registerFlags);
	} else if (host->transType == 2) {
		disk->body.unlock.trophyId = sceNetHtonl(host->body.unlock.trophyId);
		disk->body.unlock.grade = sceNetHtonl(host->body.unlock.grade);
		disk->body.unlock.unlockFlags = sceNetHtonl(host->body.unlock.unlockFlags);
		disk->body.unlock.secureTick = sceNetHtonll(host->body.unlock.secureTick);
		disk->body.unlock.localTick = sceNetHtonll(host->body.unlock.localTick);
	}
}

/* 0x812D9FD6 */
static void decodeTransSlot(const TransSlotRecord *disk, TransSlotRecord *host)
{
	sceClibMemset(host, 0, sizeof(*host));
	host->slotIndex = sceNetNtohl(disk->slotIndex);
	host->transType = sceNetNtohl(disk->transType);
	host->flags = sceNetNtohl(disk->flags);
	if (host->transType == 1) {
		host->body.reg.secureTick = sceNetNtohll(disk->body.reg.secureTick);
		host->body.reg.localTick = sceNetNtohll(disk->body.reg.localTick);
		host->body.reg.registerFlags = sceNetNtohl(disk->body.reg.registerFlags);
	} else if (host->transType == 2) {
		host->body.unlock.trophyId = sceNetNtohl(disk->body.unlock.trophyId);
		host->body.unlock.grade = sceNetNtohl(disk->body.unlock.grade);
		host->body.unlock.unlockFlags = sceNetNtohl(disk->body.unlock.unlockFlags);
		host->body.unlock.secureTick = sceNetNtohll(disk->body.unlock.secureTick);
		host->body.unlock.localTick = sceNetNtohll(disk->body.unlock.localTick);
	}
}

/* =========================================================================
 * TRPTITLE.DAT on-disk records (sections 0x100..0x800)
 * ========================================================================= */
struct TitleConfMetaRecord {
	uint32_t sourceType;
	uint32_t confKind;
	uint32_t reservedHead[2];
	uint8_t sha1[20];
	uint32_t confVersion;
	uint8_t reservedTail[40];
};

struct TitleIdRecord {
	char titleId[16];
	uint8_t reservedTail[32];
};

struct TitleStaticMetaRecord {
	char commIdText[16];
	uint64_t trophySetVersion;
	uint8_t reservedHead[8];
	uint32_t confVersion;
	uint32_t numTrophies;
	uint32_t titleFlags;
	uint32_t reservedMid;
	uint8_t allMaskPacked[32];
	uint8_t hiddenMaskPacked[32];
	uint8_t platinumMaskPacked[32];
	uint8_t goldMaskPacked[32];
	uint8_t silverMaskPacked[32];
	uint8_t bronzeMaskPacked[32];
	uint8_t groupMaskPacked[16];
	uint8_t reservedTail[32];
};

struct TitleGroupMetaRecord {
	uint32_t groupId;
	uint32_t reservedHead[3];
	uint8_t trophyMaskPacked[32];
	uint8_t reservedTail[32];
};

struct TitleTrophyMetaRecord {
	uint32_t trophyId;
	uint32_t grade;
	uint32_t hidden;
	uint32_t reservedHead;
	uint32_t platinumLinkId;
	uint32_t groupId;
	uint32_t reservedMid[2];
	uint8_t platinumReqPacked[32];
	uint8_t reservedTail[32];
};

struct TitleProgressRecord {
	uint64_t revision;
	uint64_t reservedHead;
	uint64_t installedSecureTick;
	uint64_t installedLocalTick;
	uint64_t modifiedSecureTick;
	uint64_t modifiedLocalTick;
	uint64_t unlockedSecureTick;
	uint64_t unlockedLocalTick;
	uint64_t syncedSecureTick;
	uint64_t syncedLocalTick;
	uint64_t serverTimestamp;
	uint32_t deferredCount;
	uint32_t reservedMid;
	uint8_t unlockedMaskPacked[32];
	uint8_t reservedTail[32];
};

struct TitleGroupProgressRecord {
	uint32_t groupId;
	uint32_t reservedHead[3];
	uint64_t unlockedSecureTick;
	uint64_t unlockedLocalTick;
	uint8_t unlockedMaskPacked[32];
	uint8_t reservedTail[32];
};

struct TitleTrophyUnlockRecord {
	uint32_t trophyId;
	uint32_t state;
	uint32_t flags;
	uint32_t reservedHead;
	uint64_t unlockedSecureTick;
	uint64_t unlockedLocalTick;
	uint8_t reservedTail[48];
};

/* 0x812DBFC6 */
static void encodeTitleProgress(const TitleProgressRecord *host, TitleProgressRecord *disk)
{
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->revision = sceNetHtonll(host->revision);
	disk->installedSecureTick = sceNetHtonll(host->installedSecureTick);
	disk->installedLocalTick = sceNetHtonll(host->installedLocalTick);
	disk->modifiedSecureTick = sceNetHtonll(host->modifiedSecureTick);
	disk->modifiedLocalTick = sceNetHtonll(host->modifiedLocalTick);
	disk->unlockedSecureTick = sceNetHtonll(host->unlockedSecureTick);
	disk->unlockedLocalTick = sceNetHtonll(host->unlockedLocalTick);
	disk->syncedSecureTick = sceNetHtonll(host->syncedSecureTick);
	disk->syncedLocalTick = sceNetHtonll(host->syncedLocalTick);
	disk->serverTimestamp = sceNetHtonll(host->serverTimestamp);
	disk->deferredCount = sceNetHtonl(host->deferredCount);
	sceClibMemcpy_safe(disk->unlockedMaskPacked, host->unlockedMaskPacked,
		      sizeof(disk->unlockedMaskPacked));
}

static void decodeTitleProgress(const TitleProgressRecord *disk, TitleProgressRecord *host)
{
	sceClibMemset(host, 0, sizeof(*host));
	host->revision = sceNetNtohll(disk->revision);
	host->installedSecureTick = sceNetNtohll(disk->installedSecureTick);
	host->installedLocalTick = sceNetNtohll(disk->installedLocalTick);
	host->modifiedSecureTick = sceNetNtohll(disk->modifiedSecureTick);
	host->modifiedLocalTick = sceNetNtohll(disk->modifiedLocalTick);
	host->unlockedSecureTick = sceNetNtohll(disk->unlockedSecureTick);
	host->unlockedLocalTick = sceNetNtohll(disk->unlockedLocalTick);
	host->syncedSecureTick = sceNetNtohll(disk->syncedSecureTick);
	host->syncedLocalTick = sceNetNtohll(disk->syncedLocalTick);
	host->serverTimestamp = sceNetNtohll(disk->serverTimestamp);
	host->deferredCount = sceNetNtohl(disk->deferredCount);
	sceClibMemcpy_safe(host->unlockedMaskPacked, disk->unlockedMaskPacked,
		      sizeof(host->unlockedMaskPacked));
}

static uint32_t sanitizeTitleFlags(uint32_t rawFlags)
{
	uint32_t out = 0;
	if ((rawFlags & 0x0001u) != 0)
		out |= 0x0001u;
	if ((rawFlags & 0x0002u) != 0)
		out |= 0x0002u;
	if ((rawFlags & 0x0010u) != 0)
		out |= 0x0010u;
	if ((rawFlags & 0x0020u) != 0)
		out |= 0x0020u;
	if ((rawFlags & 0x0100u) != 0)
		out |= 0x0100u;
	if ((rawFlags & 0x0200u) != 0)
		out |= 0x0200u;
	return out;
}

static uint32_t encodeGroupWireId(int groupId)
{
	if (groupId == -1)
		return 0xFFFFFFFFu;
	if (groupId < 0)
		return 0xFFFFFFFEu;
	return (uint32_t)groupId;
}

/* =========================================================================
 * TRPUSER.DAT on-disk records (section 0x100: 48B, 0x200: 96B, 0x300: 224B)
 * ========================================================================= */
struct UserIndexFlagsRecord {
	uint32_t flags;
	uint8_t reservedTail[44];
};

struct UserIndexSummaryRecord {
	uint64_t lastAttemptSecureTick;
	uint64_t lastAttemptLocalTick;
	uint64_t lastSuccessSecureTick;
	uint64_t lastSuccessLocalTick;
	uint32_t totalTitles;
	uint32_t installedTitles;
	uint32_t reservedMid[2];
	uint32_t pendingUploadTitles;
	uint32_t deferredUploadTitles;
	uint8_t reservedTail[24];
};

struct UserIndexEntryRecord {
	char commIdText[16];
	uint32_t activeFlag;
	uint32_t reservedHead[3];
	uint32_t confSourceType;
	uint32_t reservedConf[3];
	uint8_t confSha1[20];
	uint32_t confVersion;
	uint32_t reservedMeta[2];
	uint32_t numTrophies;
	uint32_t titleFlags;
	uint32_t reservedSetVer[2];
	uint64_t trophySetVersion;
	uint32_t reservedJournal[2];
	uint32_t titleJournalState;
	uint32_t titleJournalFlags;
	uint64_t titleJournalSeq;
	uint64_t progressRevision;
	uint64_t reservedRev;
	uint64_t lastUnlockedSecureTick;
	uint64_t lastUnlockedLocalTick;
	uint32_t transJournalState;
	uint32_t transJournalFlags;
	uint64_t transJournalSeq;
	uint32_t pendingUploadCount;
	uint32_t deferredUploadCount;
	uint8_t reservedTail[40];
};

/* 0x812E00E0 */
static void encodeUserIndexEntry(const UserIndexEntryRecord *host, UserIndexEntryRecord *disk)
{
	sceClibMemset(disk, 0, sizeof(*disk));
	sceClibMemcpy_safe(disk->commIdText, host->commIdText, sizeof(disk->commIdText));
	disk->activeFlag = sceNetHtonl(host->activeFlag);
	disk->confSourceType = sceNetHtonl(host->confSourceType);
	sceClibMemcpy_safe(disk->confSha1, host->confSha1, sizeof(disk->confSha1));
	disk->confVersion = sceNetHtonl(host->confVersion);
	disk->numTrophies = sceNetHtonl(host->numTrophies);
	disk->titleFlags = sceNetHtonl(host->titleFlags);
	disk->trophySetVersion = sceNetHtonll(host->trophySetVersion);
	disk->titleJournalState = sceNetHtonl(host->titleJournalState);
	disk->titleJournalFlags = sceNetHtonl(host->titleJournalFlags);
	disk->titleJournalSeq = sceNetHtonll(host->titleJournalSeq);
	disk->progressRevision = sceNetHtonll(host->progressRevision);
	disk->lastUnlockedSecureTick = sceNetHtonll(host->lastUnlockedSecureTick);
	disk->lastUnlockedLocalTick = sceNetHtonll(host->lastUnlockedLocalTick);
	disk->transJournalState = sceNetHtonl(host->transJournalState);
	disk->transJournalFlags = sceNetHtonl(host->transJournalFlags);
	disk->transJournalSeq = sceNetHtonll(host->transJournalSeq);
	disk->pendingUploadCount = sceNetHtonl(host->pendingUploadCount);
	disk->deferredUploadCount = sceNetHtonl(host->deferredUploadCount);
}

static void decodeUserIndexEntry(const UserIndexEntryRecord *disk, UserIndexEntryRecord *host)
{
	sceClibMemset(host, 0, sizeof(*host));
	sceClibMemcpy_safe(host->commIdText, disk->commIdText, sizeof(host->commIdText));
	host->activeFlag = sceNetNtohl(disk->activeFlag);
	host->confSourceType = sceNetNtohl(disk->confSourceType);
	sceClibMemcpy_safe(host->confSha1, disk->confSha1, sizeof(host->confSha1));
	host->confVersion = sceNetNtohl(disk->confVersion);
	host->numTrophies = sceNetNtohl(disk->numTrophies);
	host->titleFlags = sceNetNtohl(disk->titleFlags);
	host->trophySetVersion = sceNetNtohll(disk->trophySetVersion);
	host->titleJournalState = sceNetNtohl(disk->titleJournalState);
	host->titleJournalFlags = sceNetNtohl(disk->titleJournalFlags);
	host->titleJournalSeq = sceNetNtohll(disk->titleJournalSeq);
	host->progressRevision = sceNetNtohll(disk->progressRevision);
	host->lastUnlockedSecureTick = sceNetNtohll(disk->lastUnlockedSecureTick);
	host->lastUnlockedLocalTick = sceNetNtohll(disk->lastUnlockedLocalTick);
	host->transJournalState = sceNetNtohl(disk->transJournalState);
	host->transJournalFlags = sceNetNtohl(disk->transJournalFlags);
	host->transJournalSeq = sceNetNtohll(disk->transJournalSeq);
	host->pendingUploadCount = sceNetNtohl(disk->pendingUploadCount);
	host->deferredUploadCount = sceNetNtohl(disk->deferredUploadCount);
}

static int unpackUserIndexEntry(uint32_t slotIndex, const UserIndexEntryRecord *host,
				TrophyTitleRecord *outRecord)
{
	outRecord->Clear();
	outRecord->slotIndex = (int)slotIndex;
	int ret = outRecord->commId.ParseFromString(host->commIdText);
	if (ret < 0)
		return ret;
	outRecord->titleSnapshot.confMeta.sourceType = host->confSourceType;
	ret = outRecord->titleSnapshot.confMeta.sha1.Set(host->confSha1, sizeof(host->confSha1));
	if (ret < 0)
		return ret;
	outRecord->titleSnapshot.confMeta.confVersion = host->confVersion;
	outRecord->titleSnapshot.summary.numTrophies = host->numTrophies;
	outRecord->titleSnapshot.summary.titleFlags = host->titleFlags;
	outRecord->titleSnapshot.summary.trophySetVersion = host->trophySetVersion;
	outRecord->titleSnapshot.journal.state = host->titleJournalState;
	outRecord->titleSnapshot.journal.flags = host->titleJournalFlags;
	outRecord->titleSnapshot.journal.sequence = host->titleJournalSeq;
	outRecord->titleSnapshot.progressRevision = host->progressRevision;
	outRecord->titleSnapshot.lastUnlockedTick.secureTick.tick = host->lastUnlockedSecureTick;
	outRecord->titleSnapshot.lastUnlockedTick.localTick.tick = host->lastUnlockedLocalTick;
	outRecord->transJournal.state = host->transJournalState;
	outRecord->transJournal.flags = host->transJournalFlags;
	outRecord->transJournal.sequence = host->transJournalSeq;
	outRecord->transCounts.pendingCount = host->pendingUploadCount;
	outRecord->transCounts.deferredCount = host->deferredUploadCount;
	return 0;
}

static int packUserIndexEntry(const TrophyTitleRecord *record, UserIndexEntryRecord *outHost)
{
	sceClibMemset(outHost, 0, sizeof(*outHost));
	int ret = record->commId.FormatToString(outHost->commIdText, sizeof(outHost->commIdText));
	if (ret < 0)
		return ret;
	outHost->activeFlag = 1;
	outHost->confSourceType = record->titleSnapshot.confMeta.sourceType;
	ret = record->titleSnapshot.confMeta.sha1.CopyTo(outHost->confSha1,
							 sizeof(outHost->confSha1));
	if (ret < 0)
		return ret;
	outHost->confVersion = record->titleSnapshot.confMeta.confVersion;
	outHost->numTrophies = record->titleSnapshot.summary.numTrophies;
	outHost->titleFlags = record->titleSnapshot.summary.titleFlags;
	outHost->trophySetVersion = record->titleSnapshot.summary.trophySetVersion;
	outHost->titleJournalState = record->titleSnapshot.journal.state;
	outHost->titleJournalFlags = record->titleSnapshot.journal.flags;
	outHost->titleJournalSeq = record->titleSnapshot.journal.sequence;
	outHost->progressRevision = record->titleSnapshot.progressRevision;
	outHost->lastUnlockedSecureTick = record->titleSnapshot.lastUnlockedTick.secureTick.tick;
	outHost->lastUnlockedLocalTick = record->titleSnapshot.lastUnlockedTick.localTick.tick;
	outHost->transJournalState = record->transJournal.state;
	outHost->transJournalFlags = record->transJournal.flags;
	outHost->transJournalSeq = record->transJournal.sequence;
	outHost->pendingUploadCount = record->transCounts.pendingCount;
	outHost->deferredUploadCount = record->transCounts.deferredCount;
	return 0;
}

static bool isTickEarlier(const TrophyDualTick &lhs, const TrophyDualTick &rhs)
{
	if (lhs.secureTick.tick != rhs.secureTick.tick)
		return lhs.secureTick.tick < rhs.secureTick.tick;
	return lhs.localTick.tick < rhs.localTick.tick;
}

} /* namespace */

/* =========================================================================
 * Bitset packing & sequence helpers (0x812D935C..0x812D952F)
 * ========================================================================= */

/* 0x812D935C */
void TrophyJournalState::Clear()
{
	sceClibMemset(this, 0, sizeof(*this));
}

/* 0x812D9368 */
bool TrophyJournalState::Equals(const TrophyJournalState &other) const
{
	return state == other.state && flags == other.flags && sequence == other.sequence;
}

/* 0x812D9392 */
bool TrophyJournalState::IsDirty() const
{
	return state != 0 || (flags & 1u) != 0;
}

/* 0x812D93A6 */
void TrophyJournalState::MarkDirty()
{
	flags |= 1u;
}

/* 0x812CB658 */
void TrophyConfMeta::Clear()
{
	sourceType = 0;
	confVersion = 0;
	sha1.Clear();
}

/* 0x812CB66E */
bool TrophyConfMeta::Equals(const TrophyConfMeta &other) const
{
	return sourceType == other.sourceType && confVersion == other.confVersion &&
	       sha1.Equals(other.sha1);
}

/* 0x812DF6A0 */
void TrophyTitleDataSnapshot::Clear()
{
	journal.Clear();
	confMeta.Clear();
	sceClibMemset(&summary, 0, sizeof(summary));
	lastUnlockedTick.Clear();
	progressRevision = 0;
}

/* 0x812DF6D6 */
bool TrophyTitleDataSnapshot::Equals(const TrophyTitleDataSnapshot &other) const
{
	return journal.Equals(other.journal) && confMeta.Equals(other.confMeta) &&
	       summary.numTrophies == other.summary.numTrophies &&
	       summary.trophySetVersion == other.summary.trophySetVersion &&
	       summary.titleFlags == other.summary.titleFlags &&
	       lastUnlockedTick.secureTick.tick == other.lastUnlockedTick.secureTick.tick &&
	       lastUnlockedTick.localTick.tick == other.lastUnlockedTick.localTick.tick &&
	       progressRevision == other.progressRevision;
}

/* 0x812DB9B2 */
void TrophyTransCounts::Clear()
{
	pendingCount = 0;
	deferredCount = 0;
}

/* 0x812CB4C8 */
void TrophyTitleRecord::Clear()
{
	sceClibMemset(this, 0, sizeof(*this));
	slotIndex = -1;
}

/* 0x812CB4EA */
bool TrophyTitleRecord::IsBroken() const
{
	return titleSnapshot.journal.IsDirty() || transJournal.IsDirty();
}

/* 0x812CB50E */
void TrophyTitleRecord::MarkBroken(int scope)
{
	if (scope == 0 || scope == 1)
		titleSnapshot.journal.MarkDirty();
	if (scope == 0 || scope == 2)
		transJournal.MarkDirty();
}

/* 0x812CB56C */
bool TrophyTitleRecord::HasPendingUpload() const
{
	return transCounts.pendingCount != 0;
}

/* 0x812CB576 */
bool TrophyTitleRecord::HasDeferredUploadOnly() const
{
	return transCounts.pendingCount == 0 && transCounts.deferredCount != 0;
}

/* 0x812CB588 */
bool TrophyTitleRecord::HasInstalledFlag() const
{
	return (titleSnapshot.summary.titleFlags & 0x100u) != 0;
}

/* 0x812CB598 */
void TrophyTitleRecord::ExportTitleInfo(void *outInfo) const
{
	struct TitleInfoLayout {
		TrophyCommId commId;
		uint32_t broken;
		uint32_t confSourceType;
		uint32_t confVersion;
		uint8_t confSha1[SCE_SHA1_DIGEST_SIZE];
		uint32_t numTrophies;
		uint32_t titleFlags;
		uint64_t trophySetVersion;
		uint64_t progressRevision;
		SceRtcTick lastUnlockedSecureTick;
		SceRtcTick lastUnlockedLocalTick;
	};

	TitleInfoLayout *dst = static_cast<TitleInfoLayout *>(outInfo);
	sceClibMemset(dst, 0, sizeof(*dst));
	commId.CopyTo(&dst->commId);
	dst->broken = IsBroken() ? 1u : 0u;
	dst->confSourceType = titleSnapshot.confMeta.sourceType;
	dst->confVersion = titleSnapshot.confMeta.confVersion;
	titleSnapshot.confMeta.sha1.CopyTo(dst->confSha1, sizeof(dst->confSha1));
	dst->numTrophies = titleSnapshot.summary.numTrophies;
	dst->titleFlags = titleSnapshot.summary.titleFlags;
	dst->trophySetVersion = titleSnapshot.summary.trophySetVersion;
	dst->progressRevision = titleSnapshot.progressRevision;
	dst->lastUnlockedSecureTick = titleSnapshot.lastUnlockedTick.secureTick;
	dst->lastUnlockedLocalTick = titleSnapshot.lastUnlockedTick.localTick;
}

/* 0x812CB610 */
void TrophyTitleRecord::ExportSyncInfo(void *outInfo) const
{
	struct SyncInfoLayout {
		uint32_t pendingCount;
		uint32_t deferredCount;
	};

	SyncInfoLayout *dst = static_cast<SyncInfoLayout *>(outInfo);
	dst->pendingCount = transCounts.pendingCount;
	dst->deferredCount = transCounts.deferredCount;
}

/* 0x812E1AFA */
void TrophyTransRegisterEntry::Init()
{
	slotIndex = -1;
	transType = 1;
	synced = 0;
	deferred = 0;
	secureTick.tick = 0;
	localTick.tick = 0;
	flags = 0;
}

/* 0x812E1B1A */
void TrophyTransRegisterEntry::ResetPayload()
{
	slotIndex = -1;
	synced = 0;
	deferred = 0;
	secureTick.tick = 0;
	localTick.tick = 0;
}

/* 0x812E1B34 */
void TrophyTransUnlockEntry::Init()
{
	slotIndex = -1;
	transType = 2;
	synced = 0;
	deferred = 0;
	trophyId = -1;
	grade = 0;
	secureTick.tick = 0;
	localTick.tick = 0;
	flags = 0;
}

/* 0x812E1B5C */
void TrophyTransUnlockEntry::ResetPayload()
{
	slotIndex = -1;
	synced = 0;
	deferred = 0;
	grade = 0;
	secureTick.tick = 0;
	localTick.tick = 0;
	flags = 0;
}

/* 0x812D93B0 */
int npTrophyGenerateInitialSequence(uint64_t *outSequence)
{
	SceRtcTick tick;
	tick.tick = 0;
	int ret = npTrophyGetCurrentTick(&tick);
	if (ret < 0)
		return ret;
	*outSequence = tick.tick;
	return 0;
}

/* 0x812D9412 */
void npTrophyUnpackBitset128(const uint8_t *packed32, NpTrophyFlagArray *outBits)
{
	outBits->flagBits[0] = 0;
	outBits->flagBits[1] = 0;
	outBits->flagBits[2] = 0;
	outBits->flagBits[3] = 0;
	for (int bit = 0; bit < 128; ++bit) {
		if ((packed32[bit / 8] & (1u << (bit % 8))) != 0)
			outBits->flagBits[bit >> 5] |= (1u << (bit & 31));
	}
}

/* 0x812D945A */
void npTrophyPackBitset128(uint8_t *outPacked32, const NpTrophyFlagArray *bits)
{
	sceClibMemset(outPacked32, 0, 32);
	for (int bit = 0; bit < 128; ++bit) {
		if ((bits->flagBits[bit >> 5] & (1u << (bit & 31))) != 0)
			outPacked32[bit / 8] |= (uint8_t)(1u << (bit % 8));
	}
}

/* 0x812D94A4 */
void npTrophyUnpackBitset32(const uint8_t *packed16, uint32_t *outMask)
{
	*outMask = 0;
	for (int bit = 0; bit < 32; ++bit) {
		if ((packed16[bit / 8] & (1u << (bit % 8))) != 0)
			*outMask |= (1u << bit);
	}
}

/* 0x812D94E6 */
void npTrophyPackBitset32(uint8_t *outPacked16, uint32_t mask)
{
	sceClibMemset(outPacked16, 0, 16);
	for (int bit = 0; bit < 32; ++bit) {
		if ((mask & (1u << bit)) != 0)
			outPacked16[bit / 8] |= (uint8_t)(1u << (bit % 8));
	}
}

/* =========================================================================
 * TrophyJournaledRecordFile (0x812D8764..0x812D9539)
 * ========================================================================= */

/* 0x812D8C74 */
TrophyJournaledRecordFile::TrophyJournaledRecordFile(uint32_t magic)
	: m_recordFile(magic),
	  m_formatVersion(0),
	  m_formatFlags(0),
	  m_txnHandle(NULL),
	  m_workBuf(NULL),
	  m_workBufSize(0),
	  m_headerSectionIdx(0),
	  m_journalSectionIdx(0)
{
	m_journal.Clear();
}

/* 0x812D8C98 / 0x812D8CCC */
TrophyJournaledRecordFile::~TrophyJournaledRecordFile()
{
	if (m_txnHandle != NULL) {
		m_txnHandle->GetCancelToken()->PopNoCancel();
		m_txnHandle = NULL;
	}
	if (m_workBuf != NULL) {
		TrophyHeapObject::operator delete(m_workBuf);
		m_workBuf = NULL;
		m_workBufSize = 0;
	}
}

/* 0x812D9256 */
int TrophyJournaledRecordFile::InitHeaderCallback(void *recordBuf, SceSize recordSize,
						  uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyJournaledCreateParams *params =
		static_cast<const TrophyJournaledCreateParams *>(userCtx);
	JournaledHeaderDisk *disk = static_cast<JournaledHeaderDisk *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->formatVersionBe = sceNetHtonl(params->formatVersion);
	disk->formatFlagsBe = sceNetHtonl(params->formatFlags);
	return 0;
}

/* 0x812D92AA */
int TrophyJournaledRecordFile::InitJournalCallback(void *recordBuf, SceSize recordSize,
						   uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyJournaledCreateParams *params =
		static_cast<const TrophyJournaledCreateParams *>(userCtx);
	JournalSectionHost host;
	sceClibMemset(&host, 0, sizeof(host));
	host.state = params->initialJournalState;
	host.flags = 0;
	host.sequence = params->initialSequence;
	int ret = params->accountCache->GetAccountId(&host.accountId);
	if (ret < 0)
		return ret;
	npTrophyCopyOpenPsIdBytes(params->openPsId, host.openPsId, sizeof(host.openPsId));
	encodeJournalSection(&host, static_cast<JournalSectionDisk *>(recordBuf));
	return 0;
}

/* 0x812D87AE */
int TrophyJournaledRecordFile::WriteJournalRecord(TrophyHandle *handle, const void *journalHost)
{
	if (m_txnHandle == NULL)
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	JournalSectionDisk disk;
	encodeJournalSection(static_cast<const JournalSectionHost *>(journalHost), &disk);
	return m_recordFile.WriteRecordRange(handle, m_journalSectionIdx, 0, 0, &disk,
					     sizeof(disk));
}

/* 0x812D877A */
int TrophyJournaledRecordFile::WriteRecord(TrophyHandle *handle, uint32_t sectionIndex,
					   uint32_t recordIndex, SceSize byteOffset,
					   const void *srcBuf, SceSize byteCount)
{
	if (m_txnHandle == NULL)
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	return m_recordFile.WriteRecordRange(handle, sectionIndex, recordIndex, byteOffset, srcBuf,
					     byteCount);
}

/* 0x812D8878 */
int TrophyJournaledRecordFile::ReadRecord(TrophyHandle *handle, uint32_t sectionIndex,
					  uint32_t recordIndex, SceSize byteOffset, void *outBuf,
					  SceSize byteCount)
{
	return m_recordFile.ReadRecordRange(handle, sectionIndex, recordIndex, byteOffset, outBuf,
					    byteCount);
}

/* 0x812D909A */
int TrophyJournaledRecordFile::AppendRecord(TrophyHandle *handle, uint32_t sectionIndex,
					    const void *recordBuf, SceSize recordSize,
					    uint32_t *outRecordIndex)
{
	if (m_txnHandle == NULL)
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	return m_recordFile.AppendRecord(handle, sectionIndex, recordBuf, recordSize,
					 outRecordIndex);
}

/* 0x812D90CA */
int TrophyJournaledRecordFile::MoveRecord(TrophyHandle *handle, uint32_t sectionIndex,
					  uint32_t fromIndex, uint32_t toIndex)
{
	if (m_txnHandle == NULL)
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	return m_recordFile.MoveRecord(handle, sectionIndex, fromIndex, toIndex, m_workBuf,
				       m_workBufSize);
}

/* 0x812D8D06 */
int TrophyJournaledRecordFile::LoadMetadata(TrophyHandle *handle,
					    TrophyAccountCache *accountCache, bool expectInTxn,
					    SceSize workBufSize)
{
	int ret = m_recordFile.FindSection(1, &m_headerSectionIdx);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(2, &m_journalSectionIdx);
	if (ret < 0)
		return ret;

	JournaledHeaderDisk headerDisk;
	ret = m_recordFile.ReadRecordRange(handle, m_headerSectionIdx, 0, 0, &headerDisk,
					   sizeof(headerDisk));
	if (ret < 0)
		return ret;

	JournalSectionDisk journalDisk;
	ret = m_recordFile.ReadRecordRange(handle, m_journalSectionIdx, 0, 0, &journalDisk,
					   sizeof(journalDisk));
	if (ret < 0)
		return ret;

	JournalSectionHost journalHost;
	decodeJournalSection(&journalDisk, &journalHost);

	if (expectInTxn) {
		if (journalHost.state == 0)
			return NP_TROPHY_ERR_RECORD_TRUNCATED;
	} else {
		if (journalHost.state != 0)
			return NP_TROPHY_ERR_RECORD_TRUNCATED;
	}

	ret = accountCache->VerifyMatchesAccount(journalHost.accountId);
	if (ret < 0)
		return ret;

	m_journal.state = journalHost.state;
	m_journal.flags = journalHost.flags;
	m_journal.sequence = journalHost.sequence;

	if (expectInTxn) {
		m_workBuf = TrophyHeapObject::operator new(workBufSize);
		if (m_workBuf == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;
		m_workBufSize = workBufSize;
		m_txnHandle = handle;
	}

	m_formatVersion = sceNetNtohl(headerDisk.formatVersionBe);
	m_formatFlags = sceNetNtohl(headerDisk.formatFlagsBe);
	return 0;
}

/* 0x812D8EBC */
int TrophyJournaledRecordFile::Create(TrophyHandle *handle, const char *path, int flags,
				      SceMode mode, const TrophyRecordSectionSpec *userSections,
				      int userSectionCount,
				      const TrophyJournaledCreateParams *params,
				      SceSize workBufSize)
{
	handle->GetCancelToken()->PushNoCancel();
	int totalSections = userSectionCount + 2;
	TrophyRecordSectionSpec *allSpecs = static_cast<TrophyRecordSectionSpec *>(
		TrophyHeapObject::operator new(sizeof(TrophyRecordSectionSpec) *
					       (SceSize)totalSections));
	if (allSpecs == NULL) {
		handle->GetCancelToken()->PopNoCancel();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	JournaledHeaderDisk headerBuf;
	JournalSectionDisk journalBuf;

	allSpecs[0].sectionId = 1;
	allSpecs[0].recordSize = sizeof(JournaledHeaderDisk);
	allSpecs[0].sectionKind = 1;
	allSpecs[0].recordCount = 1;
	allSpecs[0].initialRecordBuf = &headerBuf;
	allSpecs[0].initCallback = InitHeaderCallback;
	allSpecs[0].initUserCtx = const_cast<TrophyJournaledCreateParams *>(params);

	allSpecs[1].sectionId = 2;
	allSpecs[1].recordSize = sizeof(JournalSectionDisk);
	allSpecs[1].sectionKind = 1;
	allSpecs[1].recordCount = 1;
	allSpecs[1].initialRecordBuf = &journalBuf;
	allSpecs[1].initCallback = InitJournalCallback;
	allSpecs[1].initUserCtx = const_cast<TrophyJournaledCreateParams *>(params);

	int ret = 0;
	for (int i = 0; i < userSectionCount; ++i) {
		if (userSections[i].sectionId <= 255u) {
			ret = NP_TROPHY_ERR_INVALID_ARGUMENT;
			break;
		}
		allSpecs[i + 2] = userSections[i];
	}

	if (ret >= 0) {
		ret = m_recordFile.Create(handle, path, flags, mode, allSpecs, totalSections, true);
		if (ret >= 0) {
			ret = LoadMetadata(handle, params->accountCache, true, workBufSize);
			if (ret >= 0) {
				TrophyHeapObject::operator delete(allSpecs);
				return 0;
			}
		}
	}

	handle->GetCancelToken()->PopNoCancel();
	TrophyHeapObject::operator delete(allSpecs);
	return ret;
}

/* 0x812D9044 */
int TrophyJournaledRecordFile::Open(TrophyHandle *handle, const char *path, int flags,
				    SceMode mode, TrophyAccountCache *accountCache)
{
	int ret = m_recordFile.Open(handle, path, flags, mode, true);
	if (ret < 0)
		return ret;
	ret = LoadMetadata(handle, accountCache, false, 0);
	if (ret < 0) {
		if (m_recordFile.IsOpen())
			m_recordFile.Close();
		return ret;
	}
	return 0;
}

/* 0x812D9104 / 0x812D9530 */
int TrophyJournaledRecordFile::Close()
{
	if (!m_recordFile.IsOpen())
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (m_txnHandle != NULL) {
		m_txnHandle->GetCancelToken()->PopNoCancel();
		m_txnHandle = NULL;
		if (m_workBuf != NULL) {
			TrophyHeapObject::operator delete(m_workBuf);
			m_workBuf = NULL;
			m_workBufSize = 0;
		}
	}
	return m_recordFile.Close();
}

/* 0x812D889C */
int TrophyJournaledRecordFile::BeginTransaction(TrophyHandle *handle, uint32_t journalState,
						SceSize workBufSize)
{
	handle->GetCancelToken()->PushNoCancel();
	int ret = 0;
	if (m_txnHandle != NULL) {
		ret = NP_TROPHY_ERR_TXN_ALREADY_OPEN;
	} else {
		JournalSectionDisk disk;
		ret = m_recordFile.ReadRecordRange(handle, m_journalSectionIdx, 0, 0, &disk,
						   sizeof(disk));
		if (ret >= 0) {
			JournalSectionHost host;
			decodeJournalSection(&disk, &host);
			host.state = journalState;
			host.sequence += 1;
			m_workBuf = TrophyHeapObject::operator new(workBufSize);
			if (m_workBuf == NULL) {
				ret = NP_TROPHY_ERR_OUT_OF_MEMORY;
			} else {
				m_workBufSize = workBufSize;
				m_txnHandle = handle;
				ret = WriteJournalRecord(handle, &host);
				if (ret >= 0) {
					m_journal.state = host.state;
					m_journal.sequence = host.sequence;
					return 0;
				}
			}
		}
	}

	handle->GetCancelToken()->PopNoCancel();
	if (m_txnHandle != NULL) {
		m_txnHandle->GetCancelToken()->PopNoCancel();
		m_txnHandle = NULL;
	}
	if (m_workBuf != NULL) {
		TrophyHeapObject::operator delete(m_workBuf);
		m_workBuf = NULL;
		m_workBufSize = 0;
	}
	return ret;
}

/* 0x812D89F8 */
int TrophyJournaledRecordFile::CommitTransaction(TrophyHandle *handle,
						 const TrophyDualTick *commitTicks)
{
	if (m_txnHandle == NULL)
		return NP_TROPHY_ERR_TXN_NOT_OPEN;

	JournalSectionDisk disk;
	int ret = m_recordFile.ReadRecordRange(handle, m_journalSectionIdx, 0, 0, &disk,
					       sizeof(disk));
	if (ret < 0)
		return ret;

	JournalSectionHost host;
	decodeJournalSection(&disk, &host);
	host.sequence += 1;
	host.state = 0;
	host.commitSecureTick = commitTicks->secureTick.tick;
	host.commitLocalTick = commitTicks->localTick.tick;

	ret = WriteJournalRecord(handle, &host);
	if (ret < 0)
		return ret;

	if (m_txnHandle != NULL) {
		m_txnHandle->GetCancelToken()->PopNoCancel();
		m_txnHandle = NULL;
	}
	if (m_workBuf != NULL) {
		TrophyHeapObject::operator delete(m_workBuf);
		m_workBuf = NULL;
		m_workBufSize = 0;
	}
	m_journal.state = host.state;
	m_journal.sequence = host.sequence;
	return 0;
}

/* 0x812D8B24 */
int TrophyJournaledRecordFile::BindAccountIfUnbound(TrophyHandle *handle,
						    TrophyAccountCache *accountCache,
						    bool *outBound, uint64_t *outAccountId)
{
	JournalSectionDisk disk;
	int ret = m_recordFile.ReadRecordRange(handle, m_journalSectionIdx, 0, 0, &disk,
					       sizeof(disk));
	if (ret < 0)
		return ret;

	JournalSectionHost host;
	decodeJournalSection(&disk, &host);

	ret = accountCache->VerifyMatchesAccount(host.accountId);
	if (ret < 0)
		return ret;

	bool hasAccount = false;
	ret = accountCache->HasAccount(&hasAccount);
	if (ret < 0)
		return ret;

	if (hasAccount && host.accountId == 0) {
		ret = accountCache->GetAccountId(&host.accountId);
		if (ret < 0)
			return ret;
		*outBound = true;
		ret = WriteJournalRecord(handle, &host);
		if (ret < 0)
			return ret;
	} else {
		*outBound = false;
	}

	if (outAccountId != NULL)
		*outAccountId = host.accountId;
	return 0;
}

/* 0x812D9154 */
int TrophyJournaledRecordFile::GetFormatInfo(uint32_t *outVersion, uint32_t *outFlags) const
{
	if (!m_recordFile.IsOpen())
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	if (outVersion != NULL)
		*outVersion = m_formatVersion;
	if (outFlags != NULL)
		*outFlags = m_formatFlags;
	return 0;
}

/* 0x812D8C52 */
int TrophyJournaledRecordFile::GetFormatFlags(uint32_t *outFlags) const
{
	if (!m_recordFile.IsOpen())
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	*outFlags = m_formatFlags;
	return 0;
}

/* 0x812D917A */
int TrophyJournaledRecordFile::GetJournalState(TrophyJournalState *outState) const
{
	if (!m_recordFile.IsOpen())
		return NP_TROPHY_ERR_RECORD_NOT_OPEN;
	*outState = m_journal;
	return 0;
}

/* 0x812D91AA */
int TrophyJournaledRecordFile::ReadCommitTicks(TrophyHandle *handle, TrophyDualTick *outTicks)
{
	JournalSectionDisk disk;
	int ret = m_recordFile.ReadRecordRange(handle, m_journalSectionIdx, 0, 0, &disk,
					       sizeof(disk));
	if (ret < 0)
		return ret;

	JournalSectionHost host;
	decodeJournalSection(&disk, &host);
	outTicks->secureTick.tick = host.commitSecureTick;
	outTicks->localTick.tick = host.commitLocalTick;
	return 0;
}

/* =========================================================================
 * TrophyTransDataFile (TRPTRANS.DAT, 0x812D953A..0x812DB9B2)
 * ========================================================================= */

TrophyTransDataFile::TrophyTransDataFile()
	: TrophyJournaledRecordFile(TROPHY_MAGIC_TRPTRANS),
	  m_headerSection(0),
	  m_slotsSection(0),
	  m_capacity(0)
{
}

/* 0x812DB866 */
int TrophyTransDataFile::InitHeaderSectionCallback(void *recordBuf, SceSize recordSize,
						   uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyTransCreateParams *params =
		static_cast<const TrophyTransCreateParams *>(userCtx);
	TransHeaderRecord host;
	sceClibMemset(&host, 0, sizeof(host));
	int ret = params->commId->FormatToString(host.commIdText, sizeof(host.commIdText));
	if (ret < 0)
		return ret;
	host.capacity = params->capacity;
	if (params->serverTag != NULL)
		sceClibMemcpy_safe(host.serverTag, params->serverTag, 160);
	encodeTransHeader(&host, static_cast<TransHeaderRecord *>(recordBuf));
	return 0;
}

/* 0x812DB956 */
int TrophyTransDataFile::InitSlotSectionCallback(void *recordBuf, SceSize recordSize,
						 uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)userCtx;
	TransSlotRecord host;
	sceClibMemset(&host, 0, sizeof(host));
	host.slotIndex = recordIndex;
	host.transType = 0;
	encodeTransSlot(&host, static_cast<TransSlotRecord *>(recordBuf));
	return 0;
}

/* 0x812D953A */
int TrophyTransDataFile::LoadSectionsAndVerifyCommId(TrophyHandle *handle,
						     const TrophyCommId *expectedCommId)
{
	uint32_t formatVer = 0;
	int ret = GetFormatInfo(&formatVer, NULL);
	if (ret < 0)
		return ret;
	if (formatVer != TROPHY_FORMAT_VERSION_V1 && formatVer != TROPHY_FORMAT_VERSION_V1_1)
		return NP_TROPHY_ERR_RECORD_BAD_VERSION;

	ret = m_recordFile.FindSection(0x100u, &m_headerSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x200u, &m_slotsSection);
	if (ret < 0)
		return ret;

	TransHeaderRecord disk;
	ret = ReadRecord(handle, m_headerSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TransHeaderRecord host;
	decodeTransHeader(&disk, &host);

	TrophyCommId fileCommId;
	ret = fileCommId.ParseFromString(host.commIdText);
	if (ret < 0)
		return ret;
	if (!expectedCommId->Equals(fileCommId))
		return NP_TROPHY_ERR_COMM_ID_MISMATCH;

	m_capacity = host.capacity;
	return 0;
}

/* 0x812D96F4 */
int TrophyTransDataFile::Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
				const TrophyTransCreateParams *params)
{
	TransHeaderRecord headerBuf;
	TransSlotRecord slotBuf;

	TrophyRecordSectionSpec sections[2];
	sections[0].sectionId = 0x100u;
	sections[0].recordSize = sizeof(TransHeaderRecord);
	sections[0].sectionKind = 1;
	sections[0].recordCount = 1;
	sections[0].initialRecordBuf = &headerBuf;
	sections[0].initCallback = InitHeaderSectionCallback;
	sections[0].initUserCtx = const_cast<TrophyTransCreateParams *>(params);

	sections[1].sectionId = 0x200u;
	sections[1].recordSize = sizeof(TransSlotRecord);
	sections[1].sectionKind = 1;
	sections[1].recordCount = params->capacity;
	sections[1].initialRecordBuf = &slotBuf;
	sections[1].initCallback = InitSlotSectionCallback;
	sections[1].initUserCtx = const_cast<TrophyTransCreateParams *>(params);

	TrophyJournaledCreateParams baseParams;
	baseParams.initialJournalState = params->initialJournalState;
	baseParams.formatVersion = TROPHY_FORMAT_VERSION_V1_1;
	baseParams.formatFlags = params->formatFlags;
	baseParams.openPsId = params->openPsId;
	baseParams.accountCache = params->accountCache;
	baseParams.initialSequence = params->initialSequence;

	int ret = TrophyJournaledRecordFile::Create(handle, path, flags, mode, sections, 2,
						    &baseParams, 1024);
	if (ret < 0)
		return ret;

	ret = LoadSectionsAndVerifyCommId(handle, params->commId);
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812D97EC */
int TrophyTransDataFile::CreateSimple(TrophyHandle *handle, const char *path, int flags,
				      SceMode mode, const TrophyTransCreateParams *params)
{
	if (params->serverTag == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	TrophyTransCreateParams copy = *params;
	copy.initialJournalState = 1;
	return Create(handle, path, flags, mode, &copy);
}

/* 0x812DA5A8 */
int TrophyTransDataFile::CreateMigrated(TrophyHandle *handle, const char *path, int flags,
					SceMode mode, TrophyTransDataFile *oldFile,
					const TrophyTransMigrateParams *params)
{
	TrophyJournalState oldJournal;
	int ret = oldFile->GetJournalState(&oldJournal);
	if (ret < 0)
		return ret;
	if (oldJournal.state != 0 || params->serverTag == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyTransCreateParams createParams;
	createParams.formatFlags = params->formatFlags;
	createParams.openPsId = params->openPsId;
	createParams.accountCache = params->accountCache;
	createParams.commId = params->commId;
	createParams.serverTag = params->serverTag;
	createParams.initialSequence = oldJournal.sequence;
	createParams.initialJournalState = 2;
	createParams.capacity = params->capacity;

	ret = Create(handle, path, flags, mode, &createParams);
	if (ret < 0)
		return ret;

	for (uint32_t slot = 0; slot < oldFile->m_capacity; ++slot) {
		TrophyTransSlotHeader hdr;
		ret = oldFile->ReadSlotHeader(handle, slot, &hdr);
		if (ret < 0)
			break;
		if (hdr.transType == 1) {
			TrophyTransRegisterEntry regEntry;
			regEntry.Init();
			ret = oldFile->ReadRegisterEntry(handle, slot, &regEntry);
			if (ret < 0)
				break;
			ret = WriteRegisterEntry(handle, &regEntry);
			if (ret < 0)
				break;
		} else if (hdr.transType == 2) {
			TrophyTransUnlockEntry unlockEntry;
			unlockEntry.Init();
			ret = oldFile->ReadUnlockEntry(handle, slot, &unlockEntry);
			if (ret < 0)
				break;
			ret = WriteUnlockEntry(handle, &unlockEntry);
			if (ret < 0)
				break;
		}
	}

	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DA74C */
int TrophyTransDataFile::Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			      TrophyAccountCache *accountCache, const TrophyCommId *expectedCommId)
{
	int ret = TrophyJournaledRecordFile::Open(handle, path, flags, mode, accountCache);
	if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
		return NP_TROPHY_ERR_TRANS_DAT_MISSING;
	if (ret < 0)
		return ret;

	ret = LoadSectionsAndVerifyCommId(handle, expectedCommId);
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DA596 */
int TrophyTransDataFile::GetJournal(TrophyHandle *handle, TrophyJournalState *outJournal) const
{
	(void)handle;
	return GetJournalState(outJournal);
}

/* 0x812DA7D6 */
int TrophyTransDataFile::VerifyJournal(TrophyHandle *handle,
				       const TrophyJournalState *expected) const
{
	(void)handle;
	TrophyJournalState actual;
	int ret = GetJournalState(&actual);
	if (ret < 0)
		return ret;
	return actual.Equals(*expected) ? 0 : NP_TROPHY_ERR_JOURNAL_MISMATCH;
}

int TrophyTransDataFile::ReadHeaderCounts(TrophyHandle *handle, uint32_t *outHeaderFields)
{
	TransHeaderRecord disk;
	int ret = ReadRecord(handle, m_headerSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TransHeaderRecord host;
	decodeTransHeader(&disk, &host);
	outHeaderFields[0] = host.capacity;
	outHeaderFields[1] = host.usedCount;
	outHeaderFields[2] = host.syncedCount;
	outHeaderFields[3] = host.deferredCount;
	return 0;
}

/* 0x812D986E */
int TrophyTransDataFile::WriteHeaderCounts(TrophyHandle *handle, const uint32_t *headerFields)
{
	TransHeaderRecord disk;
	int ret = ReadRecord(handle, m_headerSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TransHeaderRecord host;
	decodeTransHeader(&disk, &host);
	if (host.capacity == headerFields[0] && host.usedCount == headerFields[1] &&
	    host.syncedCount == headerFields[2] && host.deferredCount == headerFields[3]) {
		return 0;
	}
	host.capacity = headerFields[0];
	host.usedCount = headerFields[1];
	host.syncedCount = headerFields[2];
	host.deferredCount = headerFields[3];
	encodeTransHeader(&host, &disk);
	return WriteRecord(handle, m_headerSection, 0, 0, &disk, sizeof(disk));
}

/* 0x812DA822 */
int TrophyTransDataFile::GetCounts(TrophyHandle *handle, TrophyTransCounts *outCounts)
{
	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;
	outCounts->pendingCount = fields[1] - fields[2] - fields[3];
	outCounts->deferredCount = fields[3];
	return 0;
}

/* 0x812DAD8A */
int TrophyTransDataFile::GetServerTag(TrophyHandle *handle, bool *outHasTag, void *outTagBuf)
{
	TransHeaderRecord disk;
	int ret = ReadRecord(handle, m_headerSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TransHeaderRecord host;
	decodeTransHeader(&disk, &host);
	sceClibMemcpy_safe(outTagBuf, host.serverTag, 160);
	const uint8_t *tagBytes = static_cast<const uint8_t *>(outTagBuf);
	*outHasTag = false;
	for (SceSize i = 0; i < 160; ++i) {
		if (tagBytes[i] != 0) {
			*outHasTag = true;
			break;
		}
	}
	return 0;
}

/* 0x812D9B3C */
int TrophyTransDataFile::FindFreeSlot(TrophyHandle *handle, uint32_t *outStartIdx,
				      uint32_t *outFreeIdx, void *outSlotHost)
{
	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	uint32_t startIdx = fields[1];
	TransSlotRecord *slotHost = static_cast<TransSlotRecord *>(outSlotHost);

	if (startIdx < m_capacity) {
		TransSlotRecord disk;
		ret = ReadRecord(handle, m_slotsSection, startIdx, 0, &disk, 32);
		if (ret < 0)
			return ret;
		sceClibMemset(slotHost, 0, 32);
		slotHost->slotIndex = sceNetNtohl(disk.slotIndex);
		slotHost->transType = sceNetNtohl(disk.transType);
		slotHost->flags = sceNetNtohl(disk.flags);
		if (slotHost->slotIndex != startIdx)
			return NP_TROPHY_ERR_RECORD_TRUNCATED;
		if (slotHost->transType == 0) {
			*outStartIdx = startIdx;
			*outFreeIdx = startIdx;
			return 0;
		}
	}

	for (uint32_t idx = 0; idx < m_capacity; ++idx) {
		TransSlotRecord disk;
		ret = ReadRecord(handle, m_slotsSection, idx, 0, &disk, 32);
		if (ret < 0)
			return ret;
		sceClibMemset(slotHost, 0, 32);
		slotHost->slotIndex = sceNetNtohl(disk.slotIndex);
		slotHost->transType = sceNetNtohl(disk.transType);
		slotHost->flags = sceNetNtohl(disk.flags);
		if (slotHost->slotIndex != idx)
			return NP_TROPHY_ERR_RECORD_TRUNCATED;
		if (slotHost->transType == 0) {
			*outStartIdx = startIdx;
			*outFreeIdx = idx;
			return 0;
		}
	}

	return NP_TROPHY_ERR_TRANS_FULL;
}

/* 0x812D9D6C */
int TrophyTransDataFile::WriteUnlockEntry(TrophyHandle *handle,
					  const TrophyTransUnlockEntry *entry)
{
	TransSlotRecord slotHost;
	sceClibMemset(&slotHost, 0, sizeof(slotHost));
	uint32_t startIdx = 0;
	uint32_t freeIdx = 0;
	int ret = FindFreeSlot(handle, &startIdx, &freeIdx, &slotHost);
	if (ret < 0)
		return ret;

	uint32_t fields[4];
	ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	fields[1] += 1;
	slotHost.transType = 2;
	slotHost.flags = 0;
	if (entry->deferred != 0) {
		slotHost.flags = 2;
		fields[3] += 1;
	} else if (entry->synced != 0) {
		slotHost.flags = 1;
		fields[2] += 1;
	}

	if (entry->grade < 1 || entry->grade > 4)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	slotHost.body.unlock.trophyId = (uint32_t)entry->trophyId;
	slotHost.body.unlock.grade = (uint32_t)entry->grade;
	uint32_t wireFlags = 0;
	if ((entry->flags & 0x1000u) != 0)
		wireFlags |= 0x1000u;
	if ((entry->flags & 0x2000u) != 0)
		wireFlags |= 0x2000u;
	if ((entry->flags & 0x8000u) != 0)
		wireFlags |= 0x8000u;
	slotHost.body.unlock.unlockFlags = wireFlags;
	slotHost.body.unlock.secureTick = entry->secureTick.tick;
	slotHost.body.unlock.localTick = entry->localTick.tick;

	if (slotHost.slotIndex != freeIdx)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TransSlotRecord disk;
	encodeTransSlot(&slotHost, &disk);
	ret = WriteRecord(handle, m_slotsSection, freeIdx, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	return WriteHeaderCounts(handle, fields);
}

/* 0x812DA1DA */
int TrophyTransDataFile::WriteRegisterEntry(TrophyHandle *handle,
					    const TrophyTransRegisterEntry *entry)
{
	TransSlotRecord slotHost;
	sceClibMemset(&slotHost, 0, sizeof(slotHost));
	uint32_t startIdx = 0;
	uint32_t freeIdx = 0;
	int ret = FindFreeSlot(handle, &startIdx, &freeIdx, &slotHost);
	if (ret < 0)
		return ret;

	uint32_t fields[4];
	ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	fields[1] += 1;
	slotHost.transType = 1;
	slotHost.flags = 0;
	if (entry->deferred != 0) {
		slotHost.flags = 2;
		fields[3] += 1;
	} else if (entry->synced != 0) {
		slotHost.flags = 1;
		fields[2] += 1;
	}

	slotHost.body.reg.secureTick = entry->secureTick.tick;
	slotHost.body.reg.localTick = entry->localTick.tick;
	if ((entry->flags & 0x8000u) != 0)
		slotHost.body.reg.registerFlags |= 0x8000u;

	if (slotHost.slotIndex != freeIdx)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TransSlotRecord disk;
	encodeTransSlot(&slotHost, &disk);
	ret = WriteRecord(handle, m_slotsSection, freeIdx, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	return WriteHeaderCounts(handle, fields);
}

/* 0x812DA4C8 */
int TrophyTransDataFile::ReadSlotHeader(TrophyHandle *handle, uint32_t slotIndex,
					TrophyTransSlotHeader *outHeader)
{
	TransSlotRecord disk;
	int ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, 32);
	if (ret < 0)
		return ret;

	uint32_t idx = sceNetNtohl(disk.slotIndex);
	uint32_t kind = sceNetNtohl(disk.transType);
	uint32_t slotFlags = sceNetNtohl(disk.flags);
	if (idx != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	outHeader->slotIndex = (int)idx;
	outHeader->synced = (slotFlags & 1u) != 0 ? 1 : 0;
	outHeader->deferred = (slotFlags & 2u) != 0 ? 1 : 0;
	if (kind == 0)
		outHeader->transType = 0;
	else if (kind == 1)
		outHeader->transType = 1;
	else if (kind == 2)
		outHeader->transType = 2;
	else
		outHeader->transType = -1;
	return 0;
}

/* 0x812DA3C8 */
int TrophyTransDataFile::ReadRegisterEntry(TrophyHandle *handle, uint32_t slotIndex,
					   TrophyTransRegisterEntry *outEntry)
{
	TransSlotRecord disk;
	int ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	TransSlotRecord host;
	decodeTransSlot(&disk, &host);
	if (host.slotIndex != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (host.transType != 1)
		return NP_TROPHY_ERR_TRANS_TYPE_MISMATCH;

	uint32_t formatVer = 0;
	ret = GetFormatInfo(&formatVer, NULL);
	if (ret < 0)
		return ret;
	if (formatVer == TROPHY_FORMAT_VERSION_V1)
		host.body.reg.registerFlags &= 0x8000u;

	outEntry->ResetPayload();
	outEntry->slotIndex = (int)host.slotIndex;
	outEntry->transType = 1;
	outEntry->synced = (host.flags & 1u) != 0 ? 1 : 0;
	outEntry->deferred = (host.flags & 2u) != 0 ? 1 : 0;
	outEntry->secureTick.tick = host.body.reg.secureTick;
	outEntry->localTick.tick = host.body.reg.localTick;
	outEntry->flags = (host.body.reg.registerFlags & 0x8000u) != 0 ? 0x8000u : 0u;
	return 0;
}

/* 0x812DA06E */
int TrophyTransDataFile::ReadUnlockEntry(TrophyHandle *handle, uint32_t slotIndex,
					 TrophyTransUnlockEntry *outEntry)
{
	TransSlotRecord disk;
	int ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	TransSlotRecord host;
	decodeTransSlot(&disk, &host);
	if (host.slotIndex != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (host.transType != 2)
		return NP_TROPHY_ERR_TRANS_TYPE_MISMATCH;

	uint32_t formatVer = 0;
	ret = GetFormatInfo(&formatVer, NULL);
	if (ret < 0)
		return ret;

	if (host.body.unlock.grade < 1 || host.body.unlock.grade > 4)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	outEntry->ResetPayload();
	outEntry->slotIndex = (int)host.slotIndex;
	outEntry->transType = 2;
	outEntry->synced = (host.flags & 1u) != 0 ? 1 : 0;
	outEntry->deferred = (host.flags & 2u) != 0 ? 1 : 0;
	outEntry->trophyId = (int)host.body.unlock.trophyId;
	outEntry->grade = (int)host.body.unlock.grade;
	outEntry->secureTick.tick = host.body.unlock.secureTick;
	outEntry->localTick.tick = host.body.unlock.localTick;
	uint32_t outFlags = 0;
	if ((host.body.unlock.unlockFlags & 0x1000u) != 0)
		outFlags |= 0x1000u;
	if ((host.body.unlock.unlockFlags & 0x2000u) != 0)
		outFlags |= 0x2000u;
	if ((host.body.unlock.unlockFlags & 0x8000u) != 0)
		outFlags |= 0x8000u;
	outEntry->flags = outFlags;
	return 0;
}

/* 0x812DA93C */
int TrophyTransDataFile::AddRegisterEntry(TrophyHandle *handle,
					  const TrophyTransRegisterInput *input)
{
	TrophyTransRegisterEntry entry;
	entry.Init();
	entry.ResetPayload();
	entry.secureTick = input->ticks->secureTick;
	entry.localTick = input->ticks->localTick;
	if (input->deferredFlag != 0) {
		entry.deferred = 1;
		entry.flags |= 0x8000u;
	}
	return WriteRegisterEntry(handle, &entry);
}

/* 0x812DA9B0 */
int TrophyTransDataFile::AddUnlockEntry(TrophyHandle *handle, const TrophyTransUnlockInput *input)
{
	bool hasAccount = false;
	int ret = input->accountCache->HasAccount(&hasAccount);
	if (ret < 0)
		return ret;

	TrophyTransUnlockEntry entry;
	entry.Init();
	entry.ResetPayload();
	entry.trophyId = input->trophyId;
	entry.grade = input->grade;
	entry.secureTick = input->ticks->secureTick;
	entry.localTick = input->ticks->localTick;
	entry.flags |= hasAccount ? 0x2000u : 0x1000u;
	if (input->deferredFlag != 0) {
		entry.deferred = 1;
		entry.flags |= 0x8000u;
	}
	return WriteUnlockEntry(handle, &entry);
}

/* 0x812DAA4C */
int TrophyTransDataFile::DeleteSlot(TrophyHandle *handle, uint32_t slotIndex,
				    uint32_t expectedType)
{
	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	TransSlotRecord disk;
	ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	TransSlotRecord host;
	decodeTransSlot(&disk, &host);
	if (host.slotIndex != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (host.transType != expectedType)
		return NP_TROPHY_ERR_TRANS_TYPE_MISMATCH;

	if (host.transType != 0) {
		fields[1] -= 1;
		if ((host.flags & 1u) != 0)
			fields[2] -= 1;
		if ((host.flags & 2u) != 0)
			fields[3] -= 1;
	}

	host.transType = 0;
	host.flags = 0;
	sceClibMemset(host.body.raw, 0, sizeof(host.body.raw));
	encodeTransSlot(&host, &disk);
	ret = WriteRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	return WriteHeaderCounts(handle, fields);
}

/* 0x812DAC7E */
int TrophyTransDataFile::FindUnlockSlotByTrophyId(TrophyHandle *handle, int trophyId,
						  int *outSlotIndex)
{
	for (uint32_t slot = 0; slot < m_capacity; ++slot) {
		TrophyTransSlotHeader hdr;
		int ret = ReadSlotHeader(handle, slot, &hdr);
		if (ret < 0)
			return ret;
		if (hdr.transType == 2) {
			TrophyTransUnlockEntry entry;
			entry.Init();
			ret = ReadUnlockEntry(handle, slot, &entry);
			if (ret < 0)
				return ret;
			if (entry.trophyId == trophyId) {
				*outSlotIndex = (int)slot;
				return 0;
			}
		}
	}
	*outSlotIndex = -1;
	return 0;
}

/* 0x812DAD40 */
int TrophyTransDataFile::RemoveUnlockByTrophyId(TrophyHandle *handle, int trophyId)
{
	int foundSlot = -1;
	int ret = FindUnlockSlotByTrophyId(handle, trophyId, &foundSlot);
	if (ret < 0)
		return ret;
	if (foundSlot == -1)
		return 0;
	return DeleteSlot(handle, (uint32_t)foundSlot, 2);
}

/* 0x812DAEBA */
int TrophyTransDataFile::PopulateUploadBatch(TrophyHandle *handle,
					     TrophySyncTitleRequestBase *outReq)
{
	npTrophySyncRequestReset(outReq);
	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;
	if (fields[1] - fields[2] == fields[3]) {
		npTrophySyncRequestReset(outReq);
		return 0;
	}

	bool hasTag = false;
	uint8_t tagBuf[160];
	ret = GetServerTag(handle, &hasTag, tagBuf);
	if (ret < 0) {
		npTrophySyncRequestReset(outReq);
		return ret;
	}
	if (!hasTag) {
		npTrophySyncRequestReset(outReq);
		return 0;
	}

	ret = npTrophySyncRequestSetServerTag(outReq, tagBuf);
	if (ret < 0) {
		npTrophySyncRequestReset(outReq);
		return ret;
	}

	for (uint32_t slot = 0; slot < m_capacity; ++slot) {
		TrophyTransSlotHeader hdr;
		ret = ReadSlotHeader(handle, slot, &hdr);
		if (ret < 0) {
			npTrophySyncRequestReset(outReq);
			return ret;
		}
		if (hdr.synced != 0 || hdr.deferred != 0)
			continue;

		if (hdr.transType == 1) {
			TrophyTransRegisterEntry regEntry;
			regEntry.Init();
			ret = ReadRegisterEntry(handle, slot, &regEntry);
			if (ret < 0 || (ret = npTrophySyncRequestAddRegister(outReq, &regEntry)) < 0) {
				npTrophySyncRequestReset(outReq);
				return ret;
			}
		} else if (hdr.transType == 2) {
			TrophyTransUnlockEntry unlockEntry;
			unlockEntry.Init();
			ret = ReadUnlockEntry(handle, slot, &unlockEntry);
			if (ret < 0 || (ret = npTrophySyncRequestAddUnlock(outReq, &unlockEntry)) < 0) {
				npTrophySyncRequestReset(outReq);
				return ret;
			}
		}
	}
	return 0;
}

/* 0x812DB110 */
int TrophyTransDataFile::MarkSlotSynced(TrophyHandle *handle, uint32_t slotIndex,
					uint32_t expectedType, uint32_t *ioHeaderFields,
					bool *outMarked)
{
	*outMarked = false;
	TransSlotRecord disk;
	int ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	TransSlotRecord host;
	decodeTransSlot(&disk, &host);
	if (host.slotIndex != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (host.transType != expectedType)
		return NP_TROPHY_ERR_TRANS_TYPE_MISMATCH;

	if ((host.flags & 2u) != 0)
		return NP_TROPHY_ERR_TRANS_DEFERRED_CONFLICT;

	if ((host.flags & 1u) == 0) {
		host.flags |= 1u;
		ioHeaderFields[2] += 1;
		if (host.transType == 1 || host.transType == 2)
			*outMarked = true;
	}

	encodeTransSlot(&host, &disk);
	return WriteRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
}

/* 0x812DB24C */
int TrophyTransDataFile::ApplySyncResults(TrophyHandle *handle,
					  TrophySyncTitleResultBase *result,
					  TrophyTransCounts *outRemaining,
					  NpTrophyFlagArray *outSyncedTrophies)
{
	sceClibMemset(outSyncedTrophies, 0, sizeof(*outSyncedTrophies));
	int totalCount = npTrophySyncResultGetTransCount(result);

	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	for (int i = 0; i < totalCount; ++i) {
		TrophyTransBatchEntry entry;
		bool accepted = false;
		ret = npTrophySyncResultGetTransStatus(result, (uint32_t)i, &entry, &accepted);
		if (ret < 0)
			return ret;
		bool marked = false;
		if ((entry.transType == 1 || entry.transType == 2) && accepted) {
			ret = MarkSlotSynced(handle, (uint32_t)entry.slotIndex,
					     (uint32_t)entry.transType, fields, &marked);
			if (ret < 0)
				return ret;
		}
		if (marked && entry.transType == 2 && entry.trophyId >= 0 && entry.trophyId < 128)
			outSyncedTrophies->flagBits[entry.trophyId >> 5] |=
				(1u << (entry.trophyId & 31));
	}

	ret = WriteHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;

	outRemaining->pendingCount = fields[1] - fields[2] - fields[3];
	outRemaining->deferredCount = fields[3];
	return 0;
}

/* 0x812DB42A / 0x812DB556 */
int TrophyTransDataFile::ClearDeferredForSlot(TrophyHandle *handle, uint32_t slotIndex,
					      uint32_t expectedType, uint32_t *ioHeaderFields)
{
	TransSlotRecord disk;
	int ret = ReadRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	TransSlotRecord host;
	decodeTransSlot(&disk, &host);
	if (host.slotIndex != slotIndex)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;
	if (host.transType != expectedType)
		return NP_TROPHY_ERR_TRANS_TYPE_MISMATCH;

	uint32_t deferredBit = (expectedType == 1) ? host.body.reg.registerFlags
						   : host.body.unlock.unlockFlags;
	if ((deferredBit & 0x8000u) == 0 || (host.flags & 2u) == 0)
		return 0;
	if ((host.flags & 1u) != 0)
		return NP_TROPHY_ERR_TRANS_DEFERRED_CONFLICT;

	host.flags &= ~2u;
	encodeTransSlot(&host, &disk);
	ret = WriteRecord(handle, m_slotsSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	ioHeaderFields[3] -= 1;
	return 0;
}

/* 0x812DB682 */
int TrophyTransDataFile::ClearAllDeferred(TrophyHandle *handle)
{
	uint32_t fields[4];
	int ret = ReadHeaderCounts(handle, fields);
	if (ret < 0)
		return ret;
	if (fields[3] == 0)
		return 0;

	for (uint32_t slot = 0; slot < m_capacity; ++slot) {
		TrophyTransSlotHeader hdr;
		ret = ReadSlotHeader(handle, slot, &hdr);
		if (ret < 0)
			return ret;
		if (hdr.transType == 1 || hdr.transType == 2) {
			ret = ClearDeferredForSlot(handle, slot, (uint32_t)hdr.transType, fields);
			if (ret < 0)
				return ret;
		}
	}

	return WriteHeaderCounts(handle, fields);
}

/* =========================================================================
 * TrophyProgressDirtyCache (0x812DC3EE, 0x812DC99E, 0x812DD1D4)
 * ========================================================================= */

/* 0x812DD1D4 */
void TrophyProgressDirtyCache::Init()
{
	dirty = 0;
	bumpRevision = 0;
	deferredCount = 0;
	sceClibMemset(&unlockedMask, 0, sizeof(unlockedMask));
	lastUnlockedTick.Clear();
	lastSyncedTick.Clear();
	serverTimestamp = 0;
	for (int i = 0; i < 33; ++i) {
		groups[i].groupId = -2;
		sceClibMemset(&groups[i].unlockedMask, 0, sizeof(groups[i].unlockedMask));
		groups[i].lastUnlockedTick.Clear();
	}
}

/* 0x812DC99E */
void TrophyProgressDirtyCache::ResetUnlocked()
{
	deferredCount = 0;
	sceClibMemset(&unlockedMask, 0, sizeof(unlockedMask));
	lastUnlockedTick.Clear();
	for (int i = 0; i < 33; ++i) {
		sceClibMemset(&groups[i].unlockedMask, 0, sizeof(groups[i].unlockedMask));
		groups[i].lastUnlockedTick.Clear();
	}
	bumpRevision = 1;
}

/* 0x812DC3EE */
void TrophyProgressDirtyCache::ApplyTrophy(const TrophyEntryStaticMeta *meta,
					   const TrophyUnlockRecord *unlock)
{
	int wordIdx = meta->trophyId >> 5;
	uint32_t bit = 1u << (meta->trophyId & 31);
	bool wasUnlocked = (unlockedMask.flagBits[wordIdx] & bit) != 0;

	if (unlock->unlocked != 0) {
		if (!wasUnlocked)
			unlockedMask.flagBits[wordIdx] |= bit;
		bumpRevision = 1;
		TrophyDualTick unlockTick;
		unlockTick.secureTick = unlock->unlockedSecureTick;
		unlockTick.localTick = unlock->unlockedLocalTick;
		if (isTickEarlier(lastUnlockedTick, unlockTick))
			lastUnlockedTick = unlockTick;
		if ((unlock->flags & 0x8000u) != 0)
			deferredCount += 1;
	} else if (wasUnlocked) {
		bumpRevision = 1;
		unlockedMask.flagBits[wordIdx] &= ~bit;
	}

	if (meta->groupId != -2) {
		for (int i = 0; i < 33; ++i) {
			if (groups[i].groupId != meta->groupId)
				continue;
			bool groupHad = (groups[i].unlockedMask.flagBits[wordIdx] & bit) != 0;
			if (unlock->unlocked != 0) {
				if (!groupHad)
					groups[i].unlockedMask.flagBits[wordIdx] |= bit;
				TrophyDualTick unlockTick;
				unlockTick.secureTick = unlock->unlockedSecureTick;
				unlockTick.localTick = unlock->unlockedLocalTick;
				if (isTickEarlier(groups[i].lastUnlockedTick, unlockTick))
					groups[i].lastUnlockedTick = unlockTick;
			} else if (groupHad) {
				groups[i].unlockedMask.flagBits[wordIdx] &= ~bit;
			}
			break;
		}
	}
	dirty = 1;
}

/* =========================================================================
 * TrophyTitleDataFile (TRPTITLE.DAT, 0x812DB9C0..0x812DF74B)
 * ========================================================================= */

TrophyTitleDataFile::TrophyTitleDataFile()
	: TrophyJournaledRecordFile(TROPHY_MAGIC_TRPTITLE),
	  m_confMetaSection(0),
	  m_titleIdSection(0),
	  m_titleMetaSection(0),
	  m_groupMetaSection(0),
	  m_trophyMetaSection(0),
	  m_progressSection(0),
	  m_groupProgressSection(0),
	  m_trophyUnlockSection(0)
{
}

/* 0x812DEDA6 */
int TrophyTitleDataFile::InitConfMetaCallback(void *recordBuf, SceSize recordSize,
					      uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyTitleCreateParams *params =
		static_cast<const TrophyTitleCreateParams *>(userCtx);
	const TrophyConfMeta *meta = params->confMeta;
	if (meta->sourceType != 1 && meta->sourceType != 2)
		return NP_TROPHY_ERR_GENERIC;

	TitleConfMetaRecord *disk = static_cast<TitleConfMetaRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->sourceType = sceNetHtonl(meta->sourceType);
	disk->confKind = sceNetHtonl(meta->sourceType);
	int ret = meta->sha1.CopyTo(disk->sha1, sizeof(disk->sha1));
	if (ret < 0)
		return ret;
	disk->confVersion = sceNetHtonl(meta->confVersion);
	return 0;
}

/* 0x812DEE68 */
int TrophyTitleDataFile::InitTitleIdCallback(void *recordBuf, SceSize recordSize,
					     uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	(void)userCtx;
	sceClibMemset(recordBuf, 0, sizeof(TitleIdRecord));
	return 0;
}

/* 0x812DEEAE */
int TrophyTitleDataFile::InitTitleMetaCallback(void *recordBuf, SceSize recordSize,
					       uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyTitleCreateParams *params =
		static_cast<const TrophyTitleCreateParams *>(userCtx);
	const TrophyConfData *conf = params->confData;

	TitleStaticMetaRecord host;
	sceClibMemset(&host, 0, sizeof(host));
	int ret = params->commId->FormatToString(host.commIdText, sizeof(host.commIdText));
	if (ret < 0)
		return ret;
	host.trophySetVersion = conf->GetTrophySetVersion();
	host.confVersion = conf->GetConfVersion();

	NpTrophyFlagArray bits;
	uint32_t totalCount = 0;
	ret = conf->GetAllTrophyBitset(&bits, &totalCount);
	if (ret < 0)
		return ret;
	host.numTrophies = totalCount;
	host.titleFlags = sanitizeTitleFlags(params->titleFlags);
	npTrophyPackBitset128(host.allMaskPacked, &bits);

	ret = conf->GetHiddenTrophyBitset(&bits, NULL);
	if (ret < 0)
		return ret;
	npTrophyPackBitset128(host.hiddenMaskPacked, &bits);

	ret = conf->GetGradeTrophyBitset(1, &bits, NULL);
	if (ret < 0)
		return ret;
	npTrophyPackBitset128(host.platinumMaskPacked, &bits);

	ret = conf->GetGradeTrophyBitset(2, &bits, NULL);
	if (ret < 0)
		return ret;
	npTrophyPackBitset128(host.goldMaskPacked, &bits);

	ret = conf->GetGradeTrophyBitset(3, &bits, NULL);
	if (ret < 0)
		return ret;
	npTrophyPackBitset128(host.silverMaskPacked, &bits);

	ret = conf->GetGradeTrophyBitset(4, &bits, NULL);
	if (ret < 0)
		return ret;
	npTrophyPackBitset128(host.bronzeMaskPacked, &bits);

	if (conf->SupportsGroups()) {
		uint32_t groupMask = 0;
		ret = conf->GetGroupBitset(&groupMask, NULL);
		if (ret < 0)
			return ret;
		npTrophyPackBitset32(host.groupMaskPacked, groupMask);
	}

	TitleStaticMetaRecord *disk = static_cast<TitleStaticMetaRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	sceClibMemcpy_safe(disk->commIdText, host.commIdText, sizeof(disk->commIdText));
	disk->trophySetVersion = sceNetHtonll(host.trophySetVersion);
	disk->confVersion = sceNetHtonl(host.confVersion);
	disk->numTrophies = sceNetHtonl(host.numTrophies);
	disk->titleFlags = sceNetHtonl(host.titleFlags);
	sceClibMemcpy_safe(disk->allMaskPacked, host.allMaskPacked, sizeof(disk->allMaskPacked));
	sceClibMemcpy_safe(disk->hiddenMaskPacked, host.hiddenMaskPacked,
		      sizeof(disk->hiddenMaskPacked));
	sceClibMemcpy_safe(disk->platinumMaskPacked, host.platinumMaskPacked,
		      sizeof(disk->platinumMaskPacked));
	sceClibMemcpy_safe(disk->goldMaskPacked, host.goldMaskPacked, sizeof(disk->goldMaskPacked));
	sceClibMemcpy_safe(disk->silverMaskPacked, host.silverMaskPacked,
		      sizeof(disk->silverMaskPacked));
	sceClibMemcpy_safe(disk->bronzeMaskPacked, host.bronzeMaskPacked,
		      sizeof(disk->bronzeMaskPacked));
	sceClibMemcpy_safe(disk->groupMaskPacked, host.groupMaskPacked, sizeof(disk->groupMaskPacked));
	return 0;
}

/* 0x812DF30C */
int TrophyTitleDataFile::InitGroupMetaCallback(void *recordBuf, SceSize recordSize,
					       uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	const TrophyTitleCreateParams *params =
		static_cast<const TrophyTitleCreateParams *>(userCtx);
	int groupId = (recordIndex == 0) ? -1 : (int)(recordIndex - 1);

	NpTrophyFlagArray groupBits;
	int ret = params->confData->GetGroupTrophyBitset(groupId, &groupBits, NULL);
	if (ret < 0)
		return ret;

	TitleGroupMetaRecord *disk = static_cast<TitleGroupMetaRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->groupId = sceNetHtonl(encodeGroupWireId(groupId));
	npTrophyPackBitset128(disk->trophyMaskPacked, &groupBits);
	return 0;
}

/* 0x812DF3D0 */
int TrophyTitleDataFile::InitTrophyMetaCallback(void *recordBuf, SceSize recordSize,
						uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	const TrophyTitleCreateParams *params =
		static_cast<const TrophyTitleCreateParams *>(userCtx);
	TrophyConfEntry *entry = NULL;
	int ret = params->confData->GetTrophy(recordIndex, &entry);
	if (ret < 0)
		return ret;

	TitleTrophyMetaRecord *disk = static_cast<TitleTrophyMetaRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->trophyId = sceNetHtonl(recordIndex);
	disk->grade = sceNetHtonl((uint32_t)entry->GetGrade());
	disk->hidden = sceNetHtonl(entry->IsHidden() ? 1u : 0u);
	int platId = entry->GetPlatinumId();
	disk->platinumLinkId = sceNetHtonl(platId < 0 ? 0xFFFFFFFFu : (uint32_t)platId);
	uint32_t wireGroup = 0xFFFFFFFFu;
	if (params->confData->SupportsGroups())
		wireGroup = encodeGroupWireId(entry->GetGroupId());
	disk->groupId = sceNetHtonl(wireGroup);

	if (entry->GetGrade() == 1) {
		NpTrophyFlagArray reqBits;
		ret = params->confData->GetPlatinumTargetBitset(recordIndex, &reqBits, NULL);
		if (ret < 0)
			return ret;
		npTrophyPackBitset128(disk->platinumReqPacked, &reqBits);
	}
	return 0;
}

/* 0x812DF4FC */
int TrophyTitleDataFile::InitProgressCallback(void *recordBuf, SceSize recordSize,
					      uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)recordIndex;
	const TrophyTitleCreateParams *params =
		static_cast<const TrophyTitleCreateParams *>(userCtx);
	TitleProgressRecord host;
	sceClibMemset(&host, 0, sizeof(host));
	host.revision = params->initialRevision;
	host.installedSecureTick = params->installedTick->secureTick.tick;
	host.installedLocalTick = params->installedTick->localTick.tick;
	host.modifiedSecureTick = params->modifiedTick->secureTick.tick;
	host.modifiedLocalTick = params->modifiedTick->localTick.tick;
	encodeTitleProgress(&host, static_cast<TitleProgressRecord *>(recordBuf));
	return 0;
}

/* 0x812DF56C */
int TrophyTitleDataFile::InitGroupProgressCallback(void *recordBuf, SceSize recordSize,
						   uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)userCtx;
	int groupId = (recordIndex == 0) ? -1 : (int)(recordIndex - 1);
	TitleGroupProgressRecord *disk = static_cast<TitleGroupProgressRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->groupId = sceNetHtonl(encodeGroupWireId(groupId));
	return 0;
}

/* 0x812DF624 */
int TrophyTitleDataFile::InitTrophyUnlockCallback(void *recordBuf, SceSize recordSize,
						  uint32_t recordIndex, void *userCtx)
{
	(void)recordSize;
	(void)userCtx;
	TitleTrophyUnlockRecord *disk = static_cast<TitleTrophyUnlockRecord *>(recordBuf);
	sceClibMemset(disk, 0, sizeof(*disk));
	disk->trophyId = sceNetHtonl(recordIndex);
	return 0;
}

/* 0x812DBBF4 */
int TrophyTitleDataFile::LoadSectionIndices()
{
	int ret = m_recordFile.FindSection(0x100u, &m_confMetaSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x200u, &m_titleIdSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x300u, &m_titleMetaSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x400u, &m_groupMetaSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x500u, &m_trophyMetaSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x600u, &m_progressSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x700u, &m_groupProgressSection);
	if (ret < 0)
		return ret;
	return m_recordFile.FindSection(0x800u, &m_trophyUnlockSection);
}

/* 0x812DBC90 */
int TrophyTitleDataFile::LoadSections(TrophyHandle *handle, const TrophyCommId *expectedCommId,
				      TrophyTitleStaticMeta *outMeta)
{
	uint32_t formatVer = 0;
	int ret = GetFormatInfo(&formatVer, NULL);
	if (ret < 0)
		return ret;
	if (formatVer != TROPHY_FORMAT_VERSION_V1)
		return NP_TROPHY_ERR_RECORD_BAD_VERSION;

	ret = LoadSectionIndices();
	if (ret < 0)
		return ret;

	TrophyTitleStaticMeta meta;
	ret = ReadTitleStaticMeta(handle, &meta);
	if (ret < 0)
		return ret;
	if (!expectedCommId->Equals(meta.commId))
		return NP_TROPHY_ERR_COMM_ID_MISMATCH;

	if (outMeta != NULL)
		*outMeta = meta;
	return 0;
}

/* 0x812DBDD6 */
int TrophyTitleDataFile::Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
				const TrophyTitleCreateParams *params)
{
	uint32_t trophyCount = 0;
	int ret = params->confData->GetAllTrophyBitset(NULL, &trophyCount);
	if (ret < 0)
		return ret;

	uint32_t groupRecordCount = 0;
	if (params->confData->SupportsGroups()) {
		uint32_t groupMask = 0;
		uint32_t groupCount = 0;
		ret = params->confData->GetGroupBitset(&groupMask, &groupCount);
		if (ret < 0)
			return ret;
		groupRecordCount = ((groupMask & 1u) != 0) ? (groupCount + 1) : (groupCount + 2);
	}

	TitleConfMetaRecord confBuf;
	TitleIdRecord titleIdBuf;
	TitleStaticMetaRecord titleBuf;
	TitleGroupMetaRecord groupBuf;
	TitleTrophyMetaRecord trophyBuf;
	TitleProgressRecord progBuf;
	TitleGroupProgressRecord groupProgBuf;
	TitleTrophyUnlockRecord unlockBuf;

	TrophyRecordSectionSpec sections[8];
	sections[0].sectionId = 0x100u;
	sections[0].recordSize = sizeof(TitleConfMetaRecord);
	sections[0].sectionKind = 1;
	sections[0].recordCount = 1;
	sections[0].initialRecordBuf = &confBuf;
	sections[0].initCallback = InitConfMetaCallback;
	sections[0].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[1].sectionId = 0x200u;
	sections[1].recordSize = sizeof(TitleIdRecord);
	sections[1].sectionKind = 1;
	sections[1].recordCount = 16;
	sections[1].initialRecordBuf = &titleIdBuf;
	sections[1].initCallback = InitTitleIdCallback;
	sections[1].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[2].sectionId = 0x300u;
	sections[2].recordSize = sizeof(TitleStaticMetaRecord);
	sections[2].sectionKind = 1;
	sections[2].recordCount = 1;
	sections[2].initialRecordBuf = &titleBuf;
	sections[2].initCallback = InitTitleMetaCallback;
	sections[2].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[3].sectionId = 0x400u;
	sections[3].recordSize = sizeof(TitleGroupMetaRecord);
	sections[3].sectionKind = 1;
	sections[3].recordCount = groupRecordCount;
	sections[3].initialRecordBuf = &groupBuf;
	sections[3].initCallback = InitGroupMetaCallback;
	sections[3].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[4].sectionId = 0x500u;
	sections[4].recordSize = sizeof(TitleTrophyMetaRecord);
	sections[4].sectionKind = 1;
	sections[4].recordCount = trophyCount;
	sections[4].initialRecordBuf = &trophyBuf;
	sections[4].initCallback = InitTrophyMetaCallback;
	sections[4].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[5].sectionId = 0x600u;
	sections[5].recordSize = sizeof(TitleProgressRecord);
	sections[5].sectionKind = 1;
	sections[5].recordCount = 1;
	sections[5].initialRecordBuf = &progBuf;
	sections[5].initCallback = InitProgressCallback;
	sections[5].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[6].sectionId = 0x700u;
	sections[6].recordSize = sizeof(TitleGroupProgressRecord);
	sections[6].sectionKind = 1;
	sections[6].recordCount = groupRecordCount;
	sections[6].initialRecordBuf = &groupProgBuf;
	sections[6].initCallback = InitGroupProgressCallback;
	sections[6].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	sections[7].sectionId = 0x800u;
	sections[7].recordSize = sizeof(TitleTrophyUnlockRecord);
	sections[7].sectionKind = 1;
	sections[7].recordCount = trophyCount;
	sections[7].initialRecordBuf = &unlockBuf;
	sections[7].initCallback = InitTrophyUnlockCallback;
	sections[7].initUserCtx = const_cast<TrophyTitleCreateParams *>(params);

	TrophyJournaledCreateParams baseParams;
	baseParams.initialJournalState = params->initialJournalState;
	baseParams.formatVersion = TROPHY_FORMAT_VERSION_V1;
	baseParams.formatFlags = params->formatFlags;
	baseParams.openPsId = params->openPsId;
	baseParams.accountCache = params->accountCache;
	baseParams.initialSequence = params->initialSequence;

	ret = TrophyJournaledRecordFile::Create(handle, path, flags, mode, sections, 8,
						&baseParams, 1024);
	if (ret < 0)
		return ret;

	ret = LoadSections(handle, params->commId, NULL);
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DBF3E */
int TrophyTitleDataFile::CreateFresh(TrophyHandle *handle, const char *path, int flags,
				     SceMode mode, const TrophyTitleMigrateParams *params)
{
	uint64_t seq = 0;
	uint64_t rev = 0;
	int ret = npTrophyGenerateInitialSequence(&seq);
	if (ret < 0)
		return ret;
	ret = npTrophyGenerateInitialSequence(&rev);
	if (ret < 0)
		return ret;

	TrophyTitleCreateParams createParams;
	createParams.formatFlags = params->formatFlags;
	createParams.openPsId = params->openPsId;
	createParams.accountCache = params->accountCache;
	createParams.commId = params->commId;
	createParams.initialSequence = seq;
	createParams.initialRevision = rev;
	createParams.initialJournalState = 1;
	createParams.confMeta = params->confMeta;
	createParams.titleFlags = params->titleFlags;
	createParams.confData = params->confData;
	createParams.installedTick = params->modifiedTick;
	createParams.modifiedTick = params->modifiedTick;
	return Create(handle, path, flags, mode, &createParams);
}

/* 0x812DD238 */
int TrophyTitleDataFile::CreateMigrated(TrophyHandle *handle, const char *path, int flags,
					SceMode mode, TrophyTitleDataFile *oldFile,
					const TrophyTitleMigrateParams *params)
{
	TrophyTitleDataSnapshot oldSnap;
	oldSnap.Clear();
	int ret = oldFile->ReadSnapshot(handle, &oldSnap);
	if (ret < 0)
		return ret;
	if (oldSnap.journal.state != 0)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TitleProgressRecord oldProgDisk;
	ret = oldFile->ReadRecord(handle, oldFile->m_progressSection, 0, 0, &oldProgDisk,
				  sizeof(oldProgDisk));
	if (ret < 0)
		return ret;
	TitleProgressRecord oldProgHost;
	decodeTitleProgress(&oldProgDisk, &oldProgHost);

	TrophyDualTick installedTick;
	installedTick.secureTick.tick = oldProgHost.installedSecureTick;
	installedTick.localTick.tick = oldProgHost.installedLocalTick;

	TrophyTitleCreateParams createParams;
	createParams.formatFlags = params->formatFlags;
	createParams.openPsId = params->openPsId;
	createParams.accountCache = params->accountCache;
	createParams.commId = params->commId;
	createParams.initialSequence = oldSnap.journal.sequence;
	createParams.initialRevision = oldSnap.progressRevision;
	createParams.initialJournalState = 2;
	createParams.confMeta = params->confMeta;
	createParams.titleFlags = params->titleFlags;
	createParams.confData = params->confData;
	createParams.installedTick = &installedTick;
	createParams.modifiedTick = params->modifiedTick;

	ret = Create(handle, path, flags, mode, &createParams);
	if (ret < 0)
		return ret;

	TrophyProgressDirtyCache cache;
	cache.Init();
	if (params->confData->SupportsGroups()) {
		uint32_t groupMask = 0;
		ret = params->confData->GetGroupBitset(&groupMask, NULL);
		if (ret < 0)
			return ret;
		ret = LoadDirtyCache(handle, &groupMask, &cache);
	} else {
		ret = LoadDirtyCache(handle, NULL, &cache);
	}
	if (ret < 0)
		return ret;

	TrophyTitleStaticMeta oldMeta;
	ret = oldFile->ReadTitleStaticMeta(handle, &oldMeta);
	if (ret < 0)
		return ret;

	for (int trophyId = 0; trophyId < 128; ++trophyId) {
		if ((oldMeta.allMask.flagBits[trophyId >> 5] & (1u << (trophyId & 31))) == 0)
			continue;
		TrophyUnlockRecord unlockRec;
		ret = oldFile->ReadTrophyUnlockRecord(handle, trophyId, &unlockRec);
		if (ret < 0)
			return ret;
		TrophyEntryStaticMeta newTrophyMeta;
		ret = ReadTrophyStaticMeta(handle, trophyId, &newTrophyMeta);
		if (ret < 0)
			return ret;
		ret = WriteTrophyUnlock(handle, &newTrophyMeta, &unlockRec, 0, &cache);
		if (ret < 0)
			return ret;
	}

	return FlushDirtyCache(handle, &cache);
}

/* 0x812DD632 */
int TrophyTitleDataFile::Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			      TrophyAccountCache *accountCache, const TrophyCommId *expectedCommId,
			      TrophyTitleStaticMeta *outMeta)
{
	int ret = TrophyJournaledRecordFile::Open(handle, path, flags, mode, accountCache);
	if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
		return NP_TROPHY_ERR_TITLE_DAT_MISSING;
	if (ret < 0)
		return ret;

	ret = LoadSections(handle, expectedCommId, outMeta);
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DB9C0 */
int TrophyTitleDataFile::ReadTitleStaticMeta(TrophyHandle *handle, TrophyTitleStaticMeta *outMeta)
{
	TitleStaticMetaRecord disk;
	int ret = ReadRecord(handle, m_titleMetaSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;

	sceClibMemset(outMeta, 0, sizeof(*outMeta));
	ret = outMeta->commId.ParseFromString(disk.commIdText);
	if (ret < 0)
		return ret;

	uint32_t groupRecordCount = 0;
	ret = m_recordFile.GetRecordCount(m_groupProgressSection, &groupRecordCount);
	if (ret < 0)
		return ret;
	outMeta->hasGroups = (groupRecordCount != 0) ? 1 : 0;
	outMeta->numTrophies = sceNetNtohl(disk.numTrophies);
	outMeta->titleFlags = sanitizeTitleFlags(sceNetNtohl(disk.titleFlags));
	npTrophyUnpackBitset32(disk.groupMaskPacked, &outMeta->groupMask);
	npTrophyUnpackBitset128(disk.hiddenMaskPacked, &outMeta->hiddenMask);
	npTrophyUnpackBitset128(disk.allMaskPacked, &outMeta->allMask);
	npTrophyUnpackBitset128(disk.platinumMaskPacked, &outMeta->platinumMask);
	npTrophyUnpackBitset128(disk.goldMaskPacked, &outMeta->goldMask);
	npTrophyUnpackBitset128(disk.silverMaskPacked, &outMeta->silverMask);
	npTrophyUnpackBitset128(disk.bronzeMaskPacked, &outMeta->bronzeMask);
	return 0;
}

/* 0x812DC66A */
int TrophyTitleDataFile::ReadAllGradeMasks(TrophyHandle *handle,
					   NpTrophyFlagArray *outAllAndGrades)
{
	TitleStaticMetaRecord disk;
	int ret = ReadRecord(handle, m_titleMetaSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	npTrophyUnpackBitset128(disk.allMaskPacked, &outAllAndGrades[0]);
	npTrophyUnpackBitset128(disk.platinumMaskPacked, &outAllAndGrades[1]);
	npTrophyUnpackBitset128(disk.goldMaskPacked, &outAllAndGrades[2]);
	npTrophyUnpackBitset128(disk.silverMaskPacked, &outAllAndGrades[3]);
	npTrophyUnpackBitset128(disk.bronzeMaskPacked, &outAllAndGrades[4]);
	return 0;
}

/* 0x812DC7FA */
int TrophyTitleDataFile::ReadTrophyStaticMeta(TrophyHandle *handle, int trophyId,
					      TrophyEntryStaticMeta *outMeta)
{
	NpTrophyFlagArray masks[5];
	int ret = ReadAllGradeMasks(handle, masks);
	if (ret < 0)
		return ret;
	if (trophyId < 0 || trophyId >= 128 ||
	    (masks[0].flagBits[trophyId >> 5] & (1u << (trophyId & 31))) == 0)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;

	TitleTrophyMetaRecord disk;
	ret = ReadRecord(handle, m_trophyMetaSection, (uint32_t)trophyId, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	if (sceNetNtohl(disk.trophyId) != (uint32_t)trophyId)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	outMeta->trophyId = trophyId;
	outMeta->grade = (int)sceNetNtohl(disk.grade);
	outMeta->hidden = (sceNetNtohl(disk.hidden) & 1u) != 0 ? 1 : 0;
	outMeta->platinumLinkId = (int)sceNetNtohl(disk.platinumLinkId);
	npTrophyUnpackBitset128(disk.platinumReqPacked, &outMeta->platinumReqMask);

	uint32_t groupRecordCount = 0;
	ret = m_recordFile.GetRecordCount(m_groupProgressSection, &groupRecordCount);
	if (ret < 0)
		return ret;
	outMeta->groupId = (groupRecordCount != 0) ? (int)sceNetNtohl(disk.groupId) : -2;
	return 0;
}

/* 0x812DC546 */
int TrophyTitleDataFile::ReadTrophyUnlockRecord(TrophyHandle *handle, int trophyId,
						TrophyUnlockRecord *outRecord)
{
	TitleTrophyUnlockRecord disk;
	int ret = ReadRecord(handle, m_trophyUnlockSection, (uint32_t)trophyId, 0, &disk,
			     sizeof(disk));
	if (ret < 0)
		return ret;

	uint32_t wireId = sceNetNtohl(disk.trophyId);
	uint32_t expectedWire = (trophyId < 0) ? 0xFFFFFFFFu : (uint32_t)trophyId;
	if (wireId != expectedWire)
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	uint32_t state = sceNetNtohl(disk.state);
	uint32_t rawFlags = sceNetNtohl(disk.flags);
	sceClibMemset(outRecord, 0, sizeof(*outRecord));
	outRecord->unlocked = (state != 0) ? 1 : 0;
	outRecord->syncedFlag = ((state & 0x10u) != 0) ? 1 : 0;
	if ((rawFlags & 0x1000u) != 0)
		outRecord->flags |= 0x1000u;
	if ((rawFlags & 0x2000u) != 0)
		outRecord->flags |= 0x2000u;
	if ((rawFlags & 0x4000u) != 0)
		outRecord->flags |= 0x4000u;
	if ((rawFlags & 0x8000u) != 0)
		outRecord->flags |= 0x8000u;
	outRecord->unlockedSecureTick.tick = sceNetNtohll(disk.unlockedSecureTick);
	outRecord->unlockedLocalTick.tick = sceNetNtohll(disk.unlockedLocalTick);
	return 0;
}

/* 0x812DDB9C */
int TrophyTitleDataFile::ReadTrophyUnlockState(TrophyHandle *handle, int trophyId,
					       TrophyUnlockStateInfo *outState)
{
	NpTrophyFlagArray masks[5];
	int ret = ReadAllGradeMasks(handle, masks);
	if (ret < 0)
		return ret;
	if (trophyId < 0 || trophyId >= 128 ||
	    (masks[0].flagBits[trophyId >> 5] & (1u << (trophyId & 31))) == 0)
		return NP_TROPHY_ERR_INVALID_TROPHY_ID;

	TrophyUnlockRecord rec;
	ret = ReadTrophyUnlockRecord(handle, trophyId, &rec);
	if (ret < 0)
		return ret;

	sceClibMemset(outState, 0, sizeof(*outState));
	outState->trophyId = trophyId;
	outState->unlocked = rec.unlocked;
	outState->unlockedOffline = (rec.flags & 0x4000u) != 0 ? 1 : 0;
	outState->unlockedDeferred = (rec.flags & 0x8000u) != 0 ? 1 : 0;
	outState->secureTick = rec.unlockedSecureTick;
	outState->localTick = rec.unlockedLocalTick;
	return 0;
}

/* 0x812DD8AE */
int TrophyTitleDataFile::ReadGroupStaticMeta(TrophyHandle *handle, int groupId,
					     TrophyGroupStaticMeta *outMeta)
{
	uint32_t groupRecordCount = 0;
	int ret = m_recordFile.GetRecordCount(m_groupProgressSection, &groupRecordCount);
	if (ret < 0)
		return ret;
	if (groupRecordCount == 0)
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;

	TrophyTitleStaticMeta titleMeta;
	ret = ReadTitleStaticMeta(handle, &titleMeta);
	if (ret < 0)
		return ret;
	if (groupId != -1) {
		if (groupId < 0 || groupId >= 32 || (titleMeta.groupMask & (1u << groupId)) == 0)
			return NP_TROPHY_ERR_INVALID_GROUP_ID;
	}

	uint32_t recordIdx = (groupId == -1) ? 0u : (uint32_t)(groupId + 1);
	TitleGroupMetaRecord disk;
	ret = ReadRecord(handle, m_groupMetaSection, recordIdx, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	if (sceNetNtohl(disk.groupId) != encodeGroupWireId(groupId))
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	sceClibMemset(outMeta, 0, sizeof(*outMeta));
	outMeta->groupId = groupId;
	npTrophyUnpackBitset128(disk.trophyMaskPacked, &outMeta->allMask);
	npTrophyBitsetAnd(&outMeta->allMask, &titleMeta.platinumMask,
			  &outMeta->gradeBits.platinum);
	npTrophyBitsetAnd(&outMeta->allMask, &titleMeta.goldMask, &outMeta->gradeBits.gold);
	npTrophyBitsetAnd(&outMeta->allMask, &titleMeta.silverMask, &outMeta->gradeBits.silver);
	npTrophyBitsetAnd(&outMeta->allMask, &titleMeta.bronzeMask, &outMeta->gradeBits.bronze);
	return 0;
}

/* 0x812DDA76 */
int TrophyTitleDataFile::ReadGroupProgress(TrophyHandle *handle, int groupId,
					   TrophyGroupProgressSnapshot *outProgress)
{
	uint32_t groupRecordCount = 0;
	int ret = m_recordFile.GetRecordCount(m_groupProgressSection, &groupRecordCount);
	if (ret < 0)
		return ret;
	if (groupRecordCount == 0)
		return NP_TROPHY_ERR_GROUP_UNSUPPORTED;

	uint32_t recordIdx = (groupId == -1) ? 0u : (uint32_t)(groupId + 1);
	TitleGroupProgressRecord disk;
	ret = ReadRecord(handle, m_groupProgressSection, recordIdx, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	if (sceNetNtohl(disk.groupId) != encodeGroupWireId(groupId))
		return NP_TROPHY_ERR_RECORD_TRUNCATED;

	outProgress->lastUnlockedTick.secureTick.tick = sceNetNtohll(disk.unlockedSecureTick);
	outProgress->lastUnlockedTick.localTick.tick = sceNetNtohll(disk.unlockedLocalTick);
	outProgress->displayUpdatedTick = outProgress->lastUnlockedTick;
	npTrophyUnpackBitset128(disk.unlockedMaskPacked, &outProgress->unlockedMask);
	return 0;
}

/* 0x812DD6C4 */
int TrophyTitleDataFile::ReadTitleProgress(TrophyHandle *handle,
					   TrophyTitleProgressSnapshot *outProgress)
{
	int ret = ReadCommitTicks(handle, &outProgress->lastCommitTick);
	if (ret < 0)
		return ret;

	TitleProgressRecord disk;
	ret = ReadRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TitleProgressRecord host;
	decodeTitleProgress(&disk, &host);

	outProgress->installedTick.secureTick.tick = host.installedSecureTick;
	outProgress->installedTick.localTick.tick = host.installedLocalTick;
	outProgress->lastModifiedTick.secureTick.tick = host.modifiedSecureTick;
	outProgress->lastModifiedTick.localTick.tick = host.modifiedLocalTick;
	outProgress->lastUnlockedTick.secureTick.tick = host.unlockedSecureTick;
	outProgress->lastUnlockedTick.localTick.tick = host.unlockedLocalTick;
	outProgress->lastSyncedTick.secureTick.tick = host.syncedSecureTick;
	outProgress->lastSyncedTick.localTick.tick = host.syncedLocalTick;
	outProgress->serverTimestamp = host.serverTimestamp;

	uint64_t bestSecure = host.modifiedSecureTick;
	if (bestSecure < host.unlockedSecureTick)
		bestSecure = host.unlockedSecureTick;
	if (bestSecure == 0)
		bestSecure = host.syncedSecureTick;

	uint64_t bestLocal = host.modifiedLocalTick;
	if (bestLocal < host.unlockedLocalTick)
		bestLocal = host.unlockedLocalTick;
	if (bestLocal == 0)
		bestLocal = host.syncedLocalTick;

	outProgress->displayUpdatedTick.secureTick.tick = bestSecure;
	outProgress->displayUpdatedTick.localTick.tick = bestLocal;
	npTrophyUnpackBitset128(host.unlockedMaskPacked, &outProgress->unlockedMask);
	return 0;
}

/* 0x812DD020 */
int TrophyTitleDataFile::ReadSummaryMeta(TrophyHandle *handle, TrophyTitleSummaryMeta *outSummary)
{
	TitleStaticMetaRecord disk;
	int ret = ReadRecord(handle, m_titleMetaSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	outSummary->numTrophies = sceNetNtohl(disk.numTrophies);
	outSummary->trophySetVersion = sceNetNtohll(disk.trophySetVersion);
	outSummary->titleFlags = sanitizeTitleFlags(sceNetNtohl(disk.titleFlags));
	return 0;
}

/* 0x812DCE8C */
int TrophyTitleDataFile::ReadProgressTicksAndRevision(TrophyHandle *handle,
						      TrophyDualTick *outLastUnlocked,
						      uint64_t *outRevision)
{
	TitleProgressRecord disk;
	int ret = ReadRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TitleProgressRecord host;
	decodeTitleProgress(&disk, &host);

	if (outLastUnlocked != NULL) {
		uint64_t bestSecure = host.modifiedSecureTick;
		if (bestSecure < host.unlockedSecureTick)
			bestSecure = host.unlockedSecureTick;
		if (bestSecure == 0)
			bestSecure = host.syncedSecureTick;

		uint64_t bestLocal = host.modifiedLocalTick;
		if (bestLocal < host.unlockedLocalTick)
			bestLocal = host.unlockedLocalTick;
		if (bestLocal == 0)
			bestLocal = host.syncedLocalTick;

		outLastUnlocked->secureTick.tick = bestSecure;
		outLastUnlocked->localTick.tick = bestLocal;
	}
	if (outRevision != NULL)
		*outRevision = host.revision;
	return 0;
}

/* 0x812DD102 */
int TrophyTitleDataFile::ReadSnapshot(TrophyHandle *handle, TrophyTitleDataSnapshot *outSnapshot)
{
	int ret = GetJournalState(&outSnapshot->journal);
	if (ret < 0)
		return ret;

	TitleConfMetaRecord confDisk;
	ret = ReadRecord(handle, m_confMetaSection, 0, 0, &confDisk, sizeof(confDisk));
	if (ret < 0)
		return ret;
	outSnapshot->confMeta.sourceType = sceNetNtohl(confDisk.sourceType);
	outSnapshot->confMeta.confVersion = sceNetNtohl(confDisk.confVersion);
	ret = outSnapshot->confMeta.sha1.Set(confDisk.sha1, sizeof(confDisk.sha1));
	if (ret < 0)
		return ret;

	ret = ReadSummaryMeta(handle, &outSnapshot->summary);
	if (ret < 0)
		return ret;

	return ReadProgressTicksAndRevision(handle, &outSnapshot->lastUnlockedTick,
					    &outSnapshot->progressRevision);
}

/* 0x812DEA28 */
int TrophyTitleDataFile::VerifySnapshot(TrophyHandle *handle,
					const TrophyTitleDataSnapshot *expected)
{
	TrophyTitleDataSnapshot actual;
	actual.Clear();
	int ret = ReadSnapshot(handle, &actual);
	if (ret < 0)
		return ret;
	return actual.Equals(*expected) ? 0 : NP_TROPHY_ERR_JOURNAL_MISMATCH;
}

/* 0x812DCC16 */
int TrophyTitleDataFile::LoadDirtyCache(TrophyHandle *handle, const uint32_t *groupMask,
					TrophyProgressDirtyCache *outCache)
{
	TitleProgressRecord disk;
	int ret = ReadRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TitleProgressRecord host;
	decodeTitleProgress(&disk, &host);

	outCache->deferredCount = host.deferredCount;
	outCache->lastUnlockedTick.secureTick.tick = host.unlockedSecureTick;
	outCache->lastUnlockedTick.localTick.tick = host.unlockedLocalTick;
	outCache->lastSyncedTick.secureTick.tick = host.syncedSecureTick;
	outCache->lastSyncedTick.localTick.tick = host.syncedLocalTick;
	outCache->serverTimestamp = host.serverTimestamp;
	npTrophyUnpackBitset128(host.unlockedMaskPacked, &outCache->unlockedMask);

	for (int idx = 0; idx < 33; ++idx) {
		int groupId = idx - 1;
		bool active = (groupMask != NULL) &&
			      (groupId == -1 || ((*groupMask & (1u << groupId)) != 0));
		if (!active) {
			outCache->groups[idx].groupId = -2;
			continue;
		}
		TitleGroupProgressRecord groupDisk;
		ret = ReadRecord(handle, m_groupProgressSection, (uint32_t)idx, 0, &groupDisk,
				 sizeof(groupDisk));
		if (ret < 0)
			return ret;
		if (sceNetNtohl(groupDisk.groupId) != encodeGroupWireId(groupId))
			return NP_TROPHY_ERR_RECORD_TRUNCATED;

		outCache->groups[idx].groupId = groupId;
		npTrophyUnpackBitset128(groupDisk.unlockedMaskPacked,
					&outCache->groups[idx].unlockedMask);
		outCache->groups[idx].lastUnlockedTick.secureTick.tick =
			sceNetNtohll(groupDisk.unlockedSecureTick);
		outCache->groups[idx].lastUnlockedTick.localTick.tick =
			sceNetNtohll(groupDisk.unlockedLocalTick);
	}
	return 0;
}

/* 0x812DC08A */
int TrophyTitleDataFile::FlushDirtyCache(TrophyHandle *handle,
					 const TrophyProgressDirtyCache *cache)
{
	if (cache->dirty == 0)
		return 0;

	for (int idx = 0; idx < 33; ++idx) {
		if (cache->groups[idx].groupId == -2)
			continue;
		int groupId = idx - 1;
		TitleGroupProgressRecord groupDisk;
		int ret = ReadRecord(handle, m_groupProgressSection, (uint32_t)idx, 0, &groupDisk,
				     sizeof(groupDisk));
		if (ret < 0)
			return ret;
		if (sceNetNtohl(groupDisk.groupId) != encodeGroupWireId(groupId))
			return NP_TROPHY_ERR_RECORD_TRUNCATED;

		groupDisk.unlockedSecureTick =
			sceNetHtonll(cache->groups[idx].lastUnlockedTick.secureTick.tick);
		groupDisk.unlockedLocalTick =
			sceNetHtonll(cache->groups[idx].lastUnlockedTick.localTick.tick);
		npTrophyPackBitset128(groupDisk.unlockedMaskPacked,
				      &cache->groups[idx].unlockedMask);
		ret = WriteRecord(handle, m_groupProgressSection, (uint32_t)idx, 0, &groupDisk,
				  sizeof(groupDisk));
		if (ret < 0)
			return ret;
	}

	TitleProgressRecord disk;
	int ret = ReadRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TitleProgressRecord host;
	decodeTitleProgress(&disk, &host);

	if (cache->bumpRevision != 0)
		host.revision += 1;
	host.deferredCount = cache->deferredCount;
	host.unlockedSecureTick = cache->lastUnlockedTick.secureTick.tick;
	host.unlockedLocalTick = cache->lastUnlockedTick.localTick.tick;
	npTrophyPackBitset128(host.unlockedMaskPacked, &cache->unlockedMask);
	if (host.syncedSecureTick != cache->lastSyncedTick.secureTick.tick) {
		host.syncedSecureTick = cache->lastSyncedTick.secureTick.tick;
		host.syncedLocalTick = cache->lastSyncedTick.localTick.tick;
	}
	if (host.serverTimestamp != cache->serverTimestamp)
		host.serverTimestamp = cache->serverTimestamp;

	encodeTitleProgress(&host, &disk);
	return WriteRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
}

/* 0x812DC9F4 */
int TrophyTitleDataFile::WriteTrophyUnlock(TrophyHandle *handle,
					   const TrophyEntryStaticMeta *meta,
					   const TrophyUnlockRecord *unlock, uint32_t options,
					   TrophyProgressDirtyCache *cache)
{
	TitleTrophyUnlockRecord disk;
	sceClibMemset(&disk, 0, sizeof(disk));
	disk.trophyId = sceNetHtonl((uint32_t)meta->trophyId);

	if (unlock->unlocked != 0) {
		if ((unlock->flags & 0xFFFF0FFFu) != 0)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;
		uint32_t state = (unlock->syncedFlag != 0) ? 17u : 1u;
		uint32_t wireFlags = unlock->flags & 0xF000u;
		disk.state = sceNetHtonl(state);
		disk.flags = sceNetHtonl(wireFlags);
		disk.unlockedSecureTick = sceNetHtonll(unlock->unlockedSecureTick.tick);
		disk.unlockedLocalTick = sceNetHtonll(unlock->unlockedLocalTick.tick);
		cache->ApplyTrophy(meta, unlock);
	} else {
		if (unlock->flags != 0 || unlock->unlockedSecureTick.tick != 0 ||
		    unlock->unlockedLocalTick.tick != 0)
			return NP_TROPHY_ERR_INVALID_ARGUMENT;

		NpTrophyFlagArray masks[5];
		int ret = ReadAllGradeMasks(handle, masks);
		if (ret < 0)
			return ret;
		cache->ResetUnlocked();
		for (int id = 0; id < 128; ++id) {
			if (id == meta->trophyId)
				continue;
			if ((masks[0].flagBits[id >> 5] & (1u << (id & 31))) == 0)
				continue;
			TrophyEntryStaticMeta otherMeta;
			ret = ReadTrophyStaticMeta(handle, id, &otherMeta);
			if (ret < 0)
				return ret;
			TrophyUnlockRecord otherUnlock;
			ret = ReadTrophyUnlockRecord(handle, id, &otherUnlock);
			if (ret < 0)
				return ret;
			cache->ApplyTrophy(&otherMeta, &otherUnlock);
		}
	}

	if ((options & 1u) != 0)
		return 0;

	return WriteRecord(handle, m_trophyUnlockSection, (uint32_t)meta->trophyId, 0, &disk,
			   sizeof(disk));
}

/* 0x812DDC66 */
int TrophyTitleDataFile::UpdateModifiedTicks(TrophyHandle *handle, const TrophyDualTick *ticks)
{
	TitleProgressRecord disk;
	int ret = ReadRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	TitleProgressRecord host;
	decodeTitleProgress(&disk, &host);
	host.modifiedSecureTick = ticks->secureTick.tick;
	host.modifiedLocalTick = ticks->localTick.tick;
	encodeTitleProgress(&host, &disk);
	return WriteRecord(handle, m_progressSection, 0, 0, &disk, sizeof(disk));
}

/* 0x812DE386 */
int TrophyTitleDataFile::RecordTitleId(TrophyHandle *handle, const char *titleId)
{
	char zeroTitleId[16];
	sceClibMemset(zeroTitleId, 0, sizeof(zeroTitleId));
	if (sceClibMemcmp(zeroTitleId, titleId, sizeof(zeroTitleId)) == 0)
		return 0;

	int freeSlot = -1;
	for (uint32_t slot = 0; slot < 16; ++slot) {
		TitleIdRecord disk;
		int ret = ReadRecord(handle, m_titleIdSection, slot, 0, &disk, sizeof(disk));
		if (ret < 0)
			return ret;
		if (sceClibMemcmp(titleId, disk.titleId, 16) == 0)
			return MoveRecord(handle, m_titleIdSection, slot, 0);
		if (sceClibMemcmp(zeroTitleId, disk.titleId, 16) == 0) {
			freeSlot = (int)slot;
			break;
		}
	}

	uint32_t writeSlot = (freeSlot >= 0) ? (uint32_t)freeSlot : 15u;
	TitleIdRecord disk;
	sceClibMemset(&disk, 0, sizeof(disk));
	sceClibMemcpy_safe(disk.titleId, titleId, 16);
	int ret = WriteRecord(handle, m_titleIdSection, writeSlot, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	return MoveRecord(handle, m_titleIdSection, writeSlot, 0);
}

/* 0x812DDDC8 */
int TrophyTitleDataFile::UnlockTrophyInternal(TrophyHandle *handle, const TrophyUnlockRequest *req,
					      const TrophyEntryStaticMeta *meta, uint32_t options,
					      TrophyProgressDirtyCache *cache,
					      TrophyUnlockDelta *outDelta)
{
	TrophyUnlockRecord unlockRec;
	int ret = ReadTrophyUnlockRecord(handle, meta->trophyId, &unlockRec);
	if (ret < 0)
		return ret;
	bool hasAccount = false;
	ret = req->accountCache->HasAccount(&hasAccount);
	if (ret < 0)
		return ret;
	if (unlockRec.unlocked != 0)
		return NP_TROPHY_ERR_ALREADY_UNLOCKED;

	unlockRec.unlocked = 1;
	unlockRec.flags |= hasAccount ? 0x2000u : 0x1000u;
	if (req->offlineFlag != 0)
		unlockRec.flags |= 0x4000u;
	if (req->deferredFlag != 0)
		unlockRec.flags |= 0x8000u;
	unlockRec.unlockedSecureTick = req->ticks->secureTick;
	unlockRec.unlockedLocalTick = req->ticks->localTick;

	ret = WriteTrophyUnlock(handle, meta, &unlockRec, options, cache);
	if (ret < 0)
		return ret;

	if (outDelta != NULL) {
		sceClibMemset(outDelta, 0, sizeof(*outDelta));
		outDelta->trophyId = meta->trophyId;
		outDelta->unlocked = 1;
		outDelta->unlockedOffline = (unlockRec.flags & 0x4000u) != 0 ? 1 : 0;
		outDelta->unlockedDeferred = (unlockRec.flags & 0x8000u) != 0 ? 1 : 0;
		outDelta->secureTick = unlockRec.unlockedSecureTick;
		outDelta->localTick = unlockRec.unlockedLocalTick;
		outDelta->grade = meta->grade;
	}
	return 0;
}

/* 0x812DDF02 */
int TrophyTitleDataFile::UnlockTrophy(TrophyHandle *handle, const TrophyUnlockRequest *req,
				      uint32_t options, TrophyUnlockDelta *outTrophyDelta,
				      TrophyUnlockDelta *outPlatinumDelta)
{
	TrophyProgressDirtyCache cache;
	cache.Init();

	TrophyEntryStaticMeta meta;
	int ret = ReadTrophyStaticMeta(handle, req->trophyId, &meta);
	if (ret < 0)
		return ret;
	if (meta.grade == 1)
		return NP_TROPHY_ERR_PLATINUM_CANNOT_UNLOCK;

	if (meta.groupId == -2) {
		ret = LoadDirtyCache(handle, NULL, &cache);
	} else {
		uint32_t groupMask = (meta.groupId >= 0) ? (1u << meta.groupId) : 0u;
		ret = LoadDirtyCache(handle, &groupMask, &cache);
	}
	if (ret < 0)
		return ret;

	ret = UnlockTrophyInternal(handle, req, &meta, options, &cache, outTrophyDelta);
	if (ret < 0)
		return ret;

	sceClibMemset(outPlatinumDelta, 0, sizeof(*outPlatinumDelta));
	outPlatinumDelta->trophyId = -1;

	if (meta.platinumLinkId != -1) {
		if ((options & 1u) != 0)
			return 0;

		TrophyEntryStaticMeta platMeta;
		ret = ReadTrophyStaticMeta(handle, meta.platinumLinkId, &platMeta);
		if (ret < 0)
			return ret;

		bool hasAnyReq = false;
		bool allUnlocked = true;
		for (int id = 0; id < 128; ++id) {
			uint32_t bit = 1u << (id & 31);
			if ((platMeta.platinumReqMask.flagBits[id >> 5] & bit) != 0) {
				hasAnyReq = true;
				if ((cache.unlockedMask.flagBits[id >> 5] & bit) == 0) {
					allUnlocked = false;
					break;
				}
			}
		}

		bool platAlreadyUnlocked =
			(cache.unlockedMask.flagBits[meta.platinumLinkId >> 5] &
			 (1u << (meta.platinumLinkId & 31))) != 0;

		if (hasAnyReq && allUnlocked && !platAlreadyUnlocked) {
			uint32_t combinedFlags = 0;
			for (int id = 0; id < 128; ++id) {
				uint32_t bit = 1u << (id & 31);
				if ((platMeta.platinumReqMask.flagBits[id >> 5] & bit) == 0)
					continue;
				TrophyUnlockRecord reqRec;
				ret = ReadTrophyUnlockRecord(handle, id, &reqRec);
				if (ret < 0)
					return ret;
				if (reqRec.unlocked == 0)
					return NP_TROPHY_ERR_GENERIC;
				combinedFlags |= reqRec.flags;
			}
			TrophyUnlockRequest platReq;
			platReq.trophyId = meta.platinumLinkId;
			platReq.accountCache = req->accountCache;
			platReq.ticks = req->ticks;
			platReq.offlineFlag = (combinedFlags & 0x4000u) != 0 ? 1 : 0;
			platReq.deferredFlag = (combinedFlags & 0x8000u) != 0 ? 1 : 0;
			ret = UnlockTrophyInternal(handle, &platReq, &platMeta, options, &cache,
						   outPlatinumDelta);
			if (ret < 0)
				return ret;
		}
	}

	if ((options & 1u) == 0) {
		ret = FlushDirtyCache(handle, &cache);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812DE16E */
int TrophyTitleDataFile::LockTrophyInternal(TrophyHandle *handle,
					    const TrophyEntryStaticMeta *meta, uint32_t options,
					    TrophyProgressDirtyCache *cache,
					    TrophyUnlockDelta *outDelta)
{
	TrophyUnlockRecord unlockRec;
	int ret = ReadTrophyUnlockRecord(handle, meta->trophyId, &unlockRec);
	if (ret < 0)
		return ret;
	if (unlockRec.unlocked == 0)
		return NP_TROPHY_ERR_NOT_UNLOCKED;

	sceClibMemset(&unlockRec, 0, sizeof(unlockRec));
	ret = WriteTrophyUnlock(handle, meta, &unlockRec, options, cache);
	if (ret < 0)
		return ret;

	if (outDelta != NULL) {
		sceClibMemset(outDelta, 0, sizeof(*outDelta));
		outDelta->trophyId = meta->trophyId;
		outDelta->grade = meta->grade;
	}
	return 0;
}

/* 0x812DE23E */
int TrophyTitleDataFile::LockTrophy(TrophyHandle *handle, int trophyId, uint32_t options,
				    TrophyUnlockDelta *outTrophyDelta,
				    TrophyUnlockDelta *outPlatinumDelta)
{
	TrophyProgressDirtyCache cache;
	cache.Init();

	TrophyEntryStaticMeta meta;
	int ret = ReadTrophyStaticMeta(handle, trophyId, &meta);
	if (ret < 0)
		return ret;
	if (meta.grade == 1)
		return NP_TROPHY_ERR_PLATINUM_CANNOT_LOCK;

	if (meta.groupId == -2) {
		ret = LoadDirtyCache(handle, NULL, &cache);
	} else {
		uint32_t groupMask = (meta.groupId >= 0) ? (1u << meta.groupId) : 0u;
		ret = LoadDirtyCache(handle, &groupMask, &cache);
	}
	if (ret < 0)
		return ret;

	ret = LockTrophyInternal(handle, &meta, options, &cache, outTrophyDelta);
	if (ret < 0)
		return ret;

	sceClibMemset(outPlatinumDelta, 0, sizeof(*outPlatinumDelta));
	outPlatinumDelta->trophyId = -1;

	if (meta.platinumLinkId != -1) {
		if ((options & 1u) != 0)
			return 0;
		bool platUnlocked = (cache.unlockedMask.flagBits[meta.platinumLinkId >> 5] &
				     (1u << (meta.platinumLinkId & 31))) != 0;
		if (platUnlocked) {
			TrophyEntryStaticMeta platMeta;
			ret = ReadTrophyStaticMeta(handle, meta.platinumLinkId, &platMeta);
			if (ret < 0)
				return ret;
			ret = LockTrophyInternal(handle, &platMeta, options, &cache,
						 outPlatinumDelta);
			if (ret < 0)
				return ret;
		}
	}

	if ((options & 1u) == 0) {
		ret = FlushDirtyCache(handle, &cache);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812DE4C6 */
int TrophyTitleDataFile::ReconcilePlatinum(TrophyHandle *handle, TrophyAccountCache *accountCache,
					   const TrophyDualTick *ticks, int platinumId,
					   uint32_t options, TrophyProgressDirtyCache *cache,
					   TrophyUnlockDelta *outDelta)
{
	sceClibMemset(outDelta, 0, sizeof(*outDelta));
	outDelta->trophyId = -1;

	TrophyEntryStaticMeta platMeta;
	int ret = ReadTrophyStaticMeta(handle, platinumId, &platMeta);
	if (ret < 0)
		return ret;

	bool platUnlocked = (cache->unlockedMask.flagBits[platinumId >> 5] &
			     (1u << (platinumId & 31))) != 0;
	bool hasAnyReq = false;
	bool allUnlocked = true;
	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((platMeta.platinumReqMask.flagBits[id >> 5] & bit) != 0) {
			hasAnyReq = true;
			if ((cache->unlockedMask.flagBits[id >> 5] & bit) == 0) {
				allUnlocked = false;
				break;
			}
		}
	}

	if (hasAnyReq && allUnlocked) {
		if (platUnlocked)
			return 0;
		uint32_t combinedFlags = 0;
		for (int id = 0; id < 128; ++id) {
			uint32_t bit = 1u << (id & 31);
			if ((platMeta.platinumReqMask.flagBits[id >> 5] & bit) == 0)
				continue;
			TrophyUnlockRecord reqRec;
			ret = ReadTrophyUnlockRecord(handle, id, &reqRec);
			if (ret < 0)
				return ret;
			if (reqRec.unlocked == 0)
				return NP_TROPHY_ERR_GENERIC;
			combinedFlags |= reqRec.flags;
		}
		TrophyUnlockRequest platReq;
		platReq.trophyId = platinumId;
		platReq.accountCache = accountCache;
		platReq.ticks = ticks;
		platReq.offlineFlag = (combinedFlags & 0x4000u) != 0 ? 1 : 0;
		platReq.deferredFlag = (combinedFlags & 0x8000u) != 0 ? 1 : 0;
		return UnlockTrophyInternal(handle, &platReq, &platMeta, options, cache, outDelta);
	}

	if (!platUnlocked)
		return 0;
	return LockTrophyInternal(handle, &platMeta, options, cache, outDelta);
}

/* 0x812DE658 */
int TrophyTitleDataFile::MergeSyncResult(TrophyHandle *handle, TrophyAccountCache *accountCache,
					 TrophySyncTitleResultBase *syncResult,
					 const TrophyDualTick *syncTicks,
					 uint32_t *outUnrecognizedTrophyCount,
					 TrophyUnlockDelta *outPlatinumDelta)
{
	TrophyProgressDirtyCache cache;
	cache.Init();
	*outUnrecognizedTrophyCount = 0;

	TrophyTitleStaticMeta titleMeta;
	int ret = ReadTitleStaticMeta(handle, &titleMeta);
	if (ret < 0)
		return ret;

	if (titleMeta.hasGroups != 0)
		ret = LoadDirtyCache(handle, &titleMeta.groupMask, &cache);
	else
		ret = LoadDirtyCache(handle, NULL, &cache);
	if (ret < 0)
		return ret;

	NpTrophyFlagArray masks[5];
	ret = ReadAllGradeMasks(handle, masks);
	if (ret < 0)
		return ret;

	int platinumId = -1;
	for (int id = 0; id < 128; ++id) {
		uint32_t bit = 1u << (id & 31);
		if ((masks[1].flagBits[id >> 5] & bit) != 0) {
			if (platinumId != -1)
				return NP_TROPHY_ERR_BROKEN_CONF;
			platinumId = id;
		}

		uint64_t serverTick = 0;
		int serverUnlocked = 0;
		ret = npTrophySyncResultGetTrophyState(syncResult, id, &serverTick,
						       &serverUnlocked);
		if (ret < 0)
			return ret;
		if (serverUnlocked == 0)
			continue;

		if ((masks[0].flagBits[id >> 5] & bit) == 0) {
			*outUnrecognizedTrophyCount += 1;
			continue;
		}

		TrophyUnlockRecord localRec;
		ret = ReadTrophyUnlockRecord(handle, id, &localRec);
		if (ret < 0)
			return ret;

		if (localRec.unlocked != 0) {
			bool serverOlder = (localRec.unlockedSecureTick.tick > serverTick) ||
					   (localRec.unlockedSecureTick.tick == serverTick &&
					    localRec.unlockedLocalTick.tick != 0);
			if (serverOlder) {
				localRec.unlockedSecureTick.tick = serverTick;
				localRec.unlockedLocalTick.tick = 0;
				TrophyEntryStaticMeta trophyMeta;
				ret = ReadTrophyStaticMeta(handle, id, &trophyMeta);
				if (ret < 0)
					return ret;
				ret = WriteTrophyUnlock(handle, &trophyMeta, &localRec, 0, &cache);
				if (ret < 0)
					return ret;
			}
		} else {
			TrophyEntryStaticMeta trophyMeta;
			ret = ReadTrophyStaticMeta(handle, id, &trophyMeta);
			if (ret < 0)
				return ret;
			localRec.unlocked = 1;
			localRec.flags |= 0x2000u;
			localRec.unlockedSecureTick.tick = serverTick;
			localRec.unlockedLocalTick.tick = 0;
			ret = WriteTrophyUnlock(handle, &trophyMeta, &localRec, 0, &cache);
			if (ret < 0)
				return ret;
		}
	}

	uint64_t serverTimestamp = 0;
	ret = npTrophySyncResultGetServerTimestamp(syncResult, &serverTimestamp);
	if (ret < 0)
		return ret;

	cache.lastSyncedTick = *syncTicks;
	cache.serverTimestamp = serverTimestamp;
	cache.dirty = 1;

	if (platinumId != -1 && outPlatinumDelta != NULL) {
		TrophyDualTick nowTicks;
		nowTicks.Clear();
		ret = npTrophyGetDualTick(&nowTicks);
		if (ret < 0)
			return ret;
		ret = ReconcilePlatinum(handle, accountCache, &nowTicks, platinumId, 0, &cache,
					outPlatinumDelta);
		if (ret < 0)
			return ret;
	}

	return FlushDirtyCache(handle, &cache);
}

/* 0x812DEAA2 */
int TrophyTitleDataFile::ClearAllDeferredFlags(TrophyHandle *handle)
{
	TrophyProgressDirtyCache cache;
	cache.Init();

	TrophyTitleStaticMeta titleMeta;
	int ret = ReadTitleStaticMeta(handle, &titleMeta);
	if (ret < 0)
		return ret;

	if (titleMeta.hasGroups != 0)
		ret = LoadDirtyCache(handle, &titleMeta.groupMask, &cache);
	else
		ret = LoadDirtyCache(handle, NULL, &cache);
	if (ret < 0)
		return ret;

	if (cache.deferredCount == 0)
		return 0;

	cache.ResetUnlocked();
	for (int id = 0; id < 128; ++id) {
		if ((titleMeta.allMask.flagBits[id >> 5] & (1u << (id & 31))) == 0)
			continue;
		TrophyUnlockRecord unlockRec;
		ret = ReadTrophyUnlockRecord(handle, id, &unlockRec);
		if (ret < 0)
			return ret;
		TrophyEntryStaticMeta trophyMeta;
		ret = ReadTrophyStaticMeta(handle, id, &trophyMeta);
		if (ret < 0)
			return ret;
		if ((unlockRec.flags & 0x8000u) != 0)
			unlockRec.flags &= ~0x8000u;
		ret = WriteTrophyUnlock(handle, &trophyMeta, &unlockRec, 0, &cache);
		if (ret < 0)
			return ret;
	}

	return FlushDirtyCache(handle, &cache);
}

/* =========================================================================
 * Static info export helpers (0x812DEC28..0x812DED9F)
 * ========================================================================= */

/* 0x812DEC28 */
int npTrophyComputeProgressFromBitsets(const TrophyGradeBitsets *gradeBits,
				       const TrophyGradeCounts *unlockedCounts)
{
	return npTrophyComputeProgressPercent(gradeBits, unlockedCounts);
}

/* 0x812DEC90 */
void npTrophyExportTitleStaticInfo(const TrophyTitleStaticMeta *src, void *dst)
{
	struct ExportedTitleStaticInfo {
		TrophyCommId commId;
		uint32_t numTrophies;
		uint32_t titleFlags;
		uint32_t hasGroups;
		uint32_t groupMask;
		NpTrophyFlagArray hiddenMask;
		TrophyGradeBitsets gradeBits;
	};

	ExportedTitleStaticInfo *out = static_cast<ExportedTitleStaticInfo *>(dst);
	sceClibMemset(out, 0, sizeof(*out));
	src->commId.CopyTo(&out->commId);
	out->numTrophies = src->numTrophies;
	out->titleFlags = src->titleFlags;
	out->hasGroups = (src->hasGroups != 0) ? 1u : 0u;
	out->groupMask = src->groupMask;
	out->hiddenMask = src->hiddenMask;
	out->gradeBits.platinum = src->platinumMask;
	out->gradeBits.gold = src->goldMask;
	out->gradeBits.silver = src->silverMask;
	out->gradeBits.bronze = src->bronzeMask;
}

/* 0x812DED10 */
void npTrophyExportGroupStaticInfo(const TrophyGroupStaticMeta *src, void *dst)
{
	struct ExportedGroupStaticInfo {
		int groupId;
		TrophyGradeBitsets gradeBits;
	};

	ExportedGroupStaticInfo *out = static_cast<ExportedGroupStaticInfo *>(dst);
	sceClibMemset(out, 0, sizeof(*out));
	out->groupId = src->groupId;
	out->gradeBits = src->gradeBits;
}

/* 0x812DED66 */
void npTrophyExportTrophyStaticInfo(const TrophyEntryStaticMeta *src, void *dst)
{
	struct ExportedTrophyStaticInfo {
		int trophyId;
		int grade;
		int platinumLinkId;
		NpTrophyFlagArray platinumReqMask;
		int groupId;
		uint32_t hidden;
	};

	ExportedTrophyStaticInfo *out = static_cast<ExportedTrophyStaticInfo *>(dst);
	sceClibMemset(out, 0, sizeof(*out));
	out->trophyId = src->trophyId;
	out->grade = src->grade;
	out->platinumLinkId = src->platinumLinkId;
	out->platinumReqMask = src->platinumReqMask;
	out->groupId = src->groupId;
	out->hidden = (src->hidden != 0) ? 1u : 0u;
}

/* =========================================================================
 * TrophyParamIniFile (TRPPARAM.INI, 0x812DF74C..0x812DFAC3)
 * ========================================================================= */

/* 0x812DF74C */
TrophyParamIniFile::TrophyParamIniFile()
	: m_isOpen(0)
{
}

/* 0x812DF760 */
TrophyParamIniFile::~TrophyParamIniFile()
{
}

/* 0x812DF774 */
int TrophyParamIniFile::Close()
{
	if (m_isOpen == 0)
		return NP_TROPHY_ERR_FILE_NOT_OPEN;
	m_processor.close();
	m_isOpen = 0;
	return 0;
}

/* 0x812DF792 */
int TrophyParamIniFile::Create(const char *path, const TrophyParamIniValues *values)
{
	if (m_isOpen != 0)
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;

	IniConfig config;
	config.poolSize = 1024;
	int ret = m_processor.initialize(&config);
	if (ret >= 0) {
		ret = m_processor.create(path, "rw", 0x100000);
		if (ret >= 0) {
			m_isOpen = 1;
			char buf[64];
			ret = npTrophyFormatVersion(values->sysVersion, buf, sizeof(buf));
			if (ret >= 0)
				ret = m_processor.set("TROPSYSVER", buf);
			if (ret >= 0)
				ret = values->commId->FormatToString(buf, sizeof(buf));
			if (ret >= 0)
				ret = m_processor.set("NPCOMMID", buf);
			if (ret >= 0)
				ret = npTrophyFormatVersion(values->appVersion, buf, sizeof(buf));
			if (ret >= 0)
				ret = m_processor.set("TROPAPPVER", buf);
			if (ret >= 0)
				ret = npTrophyFormatUint((unsigned int)values->lang, buf,
							 sizeof(buf), NULL);
			if (ret >= 0)
				ret = m_processor.set("LANG", buf);
			if (ret >= 0)
				return 0;
		}
	}

	if (m_isOpen != 0) {
		m_processor.close();
		m_isOpen = 0;
	}
	return ret;
}

/* 0x812DF8DA */
int TrophyParamIniFile::ReadAppVersion(uint32_t *outVersion)
{
	char buf[32];
	int ret = m_processor.get("TROPAPPVER", buf, sizeof(buf), 0);
	if (ret < 0)
		return ret;
	return npTrophyParseVersion(buf, outVersion);
}

/* 0x812DF928 */
int TrophyParamIniFile::Open(const char *path, const TrophyCommId *expectedCommId)
{
	if (m_isOpen != 0)
		return NP_TROPHY_ERR_FILE_ALREADY_OPEN;

	IniConfig config;
	config.poolSize = 1024;
	int ret = m_processor.initialize(&config);
	if (ret < 0)
		return ret;

	bool exists = false;
	ret = npTrophyPathExists(path, &exists);
	if (ret < 0)
		return ret;
	if (!exists)
		return NP_TROPHY_ERR_PARAM_INI_MISSING;

	if (m_processor.open(path, "r", 0x100000) < 0) {
		m_processor.terminate();
		return NP_TROPHY_ERR_PARAM_INI_CORRUPT;
	}

	m_isOpen = 1;
	char commIdText[32];
	char sysVerText[32];
	char appVerText[32];
	TrophyCommId parsedCommId;
	uint32_t parsedVer = 0;

	if (m_processor.get("NPCOMMID", commIdText, sizeof(commIdText), 0) >= 0 &&
	    npTrophyParseCommId(commIdText, &parsedCommId) >= 0 &&
	    expectedCommId->Equals(parsedCommId) &&
	    m_processor.get("TROPSYSVER", sysVerText, sizeof(sysVerText), 0) >= 0 &&
	    npTrophyParseVersion(sysVerText, &parsedVer) >= 0 &&
	    m_processor.get("TROPAPPVER", appVerText, sizeof(appVerText), 0) >= 0 &&
	    npTrophyParseVersion(appVerText, &parsedVer) >= 0) {
		return 0;
	}

	if (m_isOpen != 0) {
		m_processor.close();
		m_isOpen = 0;
	}
	return NP_TROPHY_ERR_PARAM_INI_CORRUPT;
}

/* 0x812DFAB8 */
int TrophyParamIniFile::Flush()
{
	return m_processor.flush();
}

/* =========================================================================
 * TrophyUserIndexFile (TRPUSER.DAT, 0x812DFAC4..0x812E1113)
 * ========================================================================= */

/* 0x812DFAC4 */
TrophyUserIndexFile::TrophyUserIndexFile()
	: TrophyJournaledRecordFile(TROPHY_MAGIC_TRPUSER),
	  m_flagsSection(0),
	  m_summarySection(0),
	  m_entriesSection(0)
{
}

/* 0x812DFB22 */
int TrophyUserIndexFile::LoadSections()
{
	uint32_t formatVer = 0;
	int ret = GetFormatInfo(&formatVer, NULL);
	if (ret < 0)
		return ret;
	if (formatVer != TROPHY_FORMAT_VERSION_V1)
		return NP_TROPHY_ERR_RECORD_BAD_VERSION;

	ret = m_recordFile.FindSection(0x100u, &m_flagsSection);
	if (ret < 0)
		return ret;
	ret = m_recordFile.FindSection(0x200u, &m_summarySection);
	if (ret < 0)
		return ret;
	return m_recordFile.FindSection(0x300u, &m_entriesSection);
}

/* 0x812DFBB2 */
int TrophyUserIndexFile::Create(TrophyHandle *handle, const char *path, int flags, SceMode mode,
				const TrophyUserIndexCreateParams *params)
{
	TrophyRecordSectionSpec sections[3];
	sceClibMemset(sections, 0, sizeof(sections));
	sections[0].sectionId = 0x200u;
	sections[0].recordSize = sizeof(UserIndexSummaryRecord);
	sections[0].sectionKind = 1;
	sections[0].recordCount = 1;

	sections[1].sectionId = 0x100u;
	sections[1].recordSize = sizeof(UserIndexFlagsRecord);
	sections[1].sectionKind = 1;
	sections[1].recordCount = 1;

	sections[2].sectionId = 0x300u;
	sections[2].recordSize = sizeof(UserIndexEntryRecord);
	sections[2].sectionKind = 1;
	sections[2].recordCount = 0;

	TrophyJournaledCreateParams baseParams;
	baseParams.initialJournalState = 5;
	baseParams.formatVersion = TROPHY_FORMAT_VERSION_V1;
	baseParams.formatFlags = params->formatFlags;
	baseParams.openPsId = params->openPsId;
	baseParams.accountCache = params->accountCache;
	baseParams.initialSequence = params->initialSequence;

	int ret = TrophyJournaledRecordFile::Create(handle, path, flags, mode, sections, 3,
						    &baseParams, 1024);
	if (ret < 0)
		return ret;

	ret = LoadSections();
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DFC86 */
int TrophyUserIndexFile::Open(TrophyHandle *handle, const char *path, int flags, SceMode mode,
			      TrophyAccountCache *accountCache)
{
	int ret = TrophyJournaledRecordFile::Open(handle, path, flags, mode, accountCache);
	if (ret == NP_TROPHY_ERR_FILE_NOT_FOUND)
		return NP_TROPHY_ERR_USER_DAT_MISSING;
	if (ret < 0)
		return ret;

	ret = LoadSections();
	if (ret < 0) {
		if (IsOpen())
			Close();
		return ret;
	}
	return 0;
}

/* 0x812DFD10 */
int TrophyUserIndexFile::SetNeedsRecoveryFlag(TrophyHandle *handle)
{
	UserIndexFlagsRecord disk;
	int ret = ReadRecord(handle, m_flagsSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	uint32_t value = sceNetNtohl(disk.flags) | 1u;
	sceClibMemset(&disk, 0, sizeof(disk));
	disk.flags = sceNetHtonl(value);
	return WriteRecord(handle, m_flagsSection, 0, 0, &disk, sizeof(disk));
}

/* 0x812DFDAE */
int TrophyUserIndexFile::ClearNeedsRecoveryFlag(TrophyHandle *handle)
{
	UserIndexFlagsRecord disk;
	int ret = ReadRecord(handle, m_flagsSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	uint32_t value = sceNetNtohl(disk.flags) & ~1u;
	sceClibMemset(&disk, 0, sizeof(disk));
	disk.flags = sceNetHtonl(value);
	return WriteRecord(handle, m_flagsSection, 0, 0, &disk, sizeof(disk));
}

/* 0x812DFE4C */
int TrophyUserIndexFile::GetNeedsRecoveryFlag(TrophyHandle *handle, bool *outNeedsRecovery)
{
	UserIndexFlagsRecord disk;
	int ret = ReadRecord(handle, m_flagsSection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	*outNeedsRecovery = (sceNetNtohl(disk.flags) & 1u) != 0;
	return 0;
}

/* 0x812E0222 */
int TrophyUserIndexFile::ReadSummary(TrophyHandle *handle, TrophyUserIndexSummary *outSummary)
{
	UserIndexSummaryRecord disk;
	int ret = ReadRecord(handle, m_summarySection, 0, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	outSummary->lastSyncAttemptTick.secureTick.tick =
		sceNetNtohll(disk.lastAttemptSecureTick);
	outSummary->lastSyncAttemptTick.localTick.tick = sceNetNtohll(disk.lastAttemptLocalTick);
	outSummary->lastSyncSuccessTick.secureTick.tick =
		sceNetNtohll(disk.lastSuccessSecureTick);
	outSummary->lastSyncSuccessTick.localTick.tick = sceNetNtohll(disk.lastSuccessLocalTick);
	outSummary->totalTitles = sceNetNtohl(disk.totalTitles);
	outSummary->installedTitles = sceNetNtohl(disk.installedTitles);
	outSummary->pendingUploadTitles = sceNetNtohl(disk.pendingUploadTitles);
	outSummary->deferredUploadTitles = sceNetNtohl(disk.deferredUploadTitles);
	return 0;
}

/* 0x812DFEB8 */
int TrophyUserIndexFile::WriteSummary(TrophyHandle *handle,
				      const TrophyUserIndexSummary *summary)
{
	TrophyUserIndexSummary current;
	int ret = ReadSummary(handle, &current);
	if (ret < 0)
		return ret;
	if (current.lastSyncAttemptTick.secureTick.tick ==
		    summary->lastSyncAttemptTick.secureTick.tick &&
	    current.lastSyncAttemptTick.localTick.tick ==
		    summary->lastSyncAttemptTick.localTick.tick &&
	    current.lastSyncSuccessTick.secureTick.tick ==
		    summary->lastSyncSuccessTick.secureTick.tick &&
	    current.lastSyncSuccessTick.localTick.tick ==
		    summary->lastSyncSuccessTick.localTick.tick &&
	    current.totalTitles == summary->totalTitles &&
	    current.installedTitles == summary->installedTitles &&
	    current.pendingUploadTitles == summary->pendingUploadTitles &&
	    current.deferredUploadTitles == summary->deferredUploadTitles) {
		return 0;
	}

	UserIndexSummaryRecord disk;
	sceClibMemset(&disk, 0, sizeof(disk));
	disk.lastAttemptSecureTick = sceNetHtonll(summary->lastSyncAttemptTick.secureTick.tick);
	disk.lastAttemptLocalTick = sceNetHtonll(summary->lastSyncAttemptTick.localTick.tick);
	disk.lastSuccessSecureTick = sceNetHtonll(summary->lastSyncSuccessTick.secureTick.tick);
	disk.lastSuccessLocalTick = sceNetHtonll(summary->lastSyncSuccessTick.localTick.tick);
	disk.totalTitles = sceNetHtonl(summary->totalTitles);
	disk.installedTitles = sceNetHtonl(summary->installedTitles);
	disk.pendingUploadTitles = sceNetHtonl(summary->pendingUploadTitles);
	disk.deferredUploadTitles = sceNetHtonl(summary->deferredUploadTitles);
	return WriteRecord(handle, m_summarySection, 0, 0, &disk, sizeof(disk));
}

/* 0x812E0310 */
int TrophyUserIndexFile::ReadEntryRaw(TrophyHandle *handle, uint32_t slotIndex,
				      void *outHostEntry)
{
	UserIndexEntryRecord disk;
	int ret = ReadRecord(handle, m_entriesSection, slotIndex, 0, &disk, sizeof(disk));
	if (ret < 0)
		return ret;
	decodeUserIndexEntry(&disk, static_cast<UserIndexEntryRecord *>(outHostEntry));
	return 0;
}

int TrophyUserIndexFile::WriteEntryRaw(TrophyHandle *handle, uint32_t slotIndex,
				       const void *hostEntry)
{
	UserIndexEntryRecord disk;
	encodeUserIndexEntry(static_cast<const UserIndexEntryRecord *>(hostEntry), &disk);
	return WriteRecord(handle, m_entriesSection, slotIndex, 0, &disk, sizeof(disk));
}

int TrophyUserIndexFile::AppendEntryRaw(TrophyHandle *handle, const void *hostEntry,
					uint32_t *outSlotIndex)
{
	UserIndexEntryRecord disk;
	encodeUserIndexEntry(static_cast<const UserIndexEntryRecord *>(hostEntry), &disk);
	return AppendRecord(handle, m_entriesSection, &disk, sizeof(disk), outSlotIndex);
}

/* 0x812E049A */
int TrophyUserIndexFile::FindTitle(TrophyHandle *handle, const TrophyCommId *commId,
				   int *outSlotIndex, TrophyTitleRecord *outRecord)
{
	uint32_t count = 0;
	int ret = m_recordFile.GetRecordCount(m_entriesSection, &count);
	if (ret < 0)
		return ret;

	for (uint32_t idx = 0; idx < count; ++idx) {
		UserIndexEntryRecord host;
		ret = ReadEntryRaw(handle, idx, &host);
		if (ret < 0)
			return ret;
		if (host.activeFlag == 1 && commId->EqualsString(host.commIdText)) {
			*outSlotIndex = (int)idx;
			if (outRecord != NULL) {
				ret = unpackUserIndexEntry(idx, &host, outRecord);
				if (ret < 0)
					return ret;
			}
			return 0;
		}
	}

	*outSlotIndex = -1;
	if (outRecord != NULL)
		outRecord->Clear();
	return 0;
}

/* 0x812E0762 */
int TrophyUserIndexFile::HasTitle(TrophyHandle *handle, const TrophyCommId *commId,
				  bool *outExists, TrophyTitleRecord *outRecord)
{
	int slot = -1;
	int ret = FindTitle(handle, commId, &slot, outRecord);
	if (ret < 0)
		return ret;
	*outExists = (slot != -1);
	return 0;
}

/* 0x812E0AFC */
int TrophyUserIndexFile::GetTitle(TrophyHandle *handle, const TrophyCommId *commId,
				  TrophyTitleRecord *outRecord)
{
	int slot = -1;
	int ret = FindTitle(handle, commId, &slot, outRecord);
	if (ret < 0)
		return ret;
	if (slot == -1)
		return NP_TROPHY_ERR_TITLE_NOT_FOUND;
	return 0;
}

/* 0x812E07AA */
int TrophyUserIndexFile::ReadTitleAt(TrophyHandle *handle, uint32_t slotIndex,
				     TrophyTitleRecord *outRecord)
{
	UserIndexEntryRecord host;
	int ret = ReadEntryRaw(handle, slotIndex, &host);
	if (ret < 0)
		return ret;
	if (host.activeFlag != 1)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;
	return unpackUserIndexEntry(slotIndex, &host, outRecord);
}

/* 0x812E087A */
int TrophyUserIndexFile::FindNextTitle(TrophyHandle *handle, uint32_t startIndex,
				       uint32_t *outFoundIndex, TrophyTitleRecord *outRecord)
{
	if (startIndex == 0xFFFFFFFFu)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	uint32_t count = 0;
	int ret = m_recordFile.GetRecordCount(m_entriesSection, &count);
	if (ret < 0)
		return ret;

	for (uint32_t idx = startIndex; idx < count; ++idx) {
		UserIndexEntryRecord host;
		ret = ReadEntryRaw(handle, idx, &host);
		if (ret < 0)
			return ret;
		if (host.activeFlag == 1) {
			ret = unpackUserIndexEntry(idx, &host, outRecord);
			if (ret < 0)
				return ret;
			*outFoundIndex = idx;
			return 0;
		}
	}

	*outFoundIndex = 0xFFFFFFFFu;
	outRecord->Clear();
	return 0;
}

/* 0x812E09A2 */
int TrophyUserIndexFile::FindOldestOrBrokenTitle(TrophyHandle *handle,
						 TrophyTitleRecord *outRecord)
{
	int oldestSlot = -1;
	TrophyDualTick oldestTick;
	oldestTick.Clear();

	uint32_t curIndex = 0;
	for (;;) {
		uint32_t foundIndex = 0xFFFFFFFFu;
		TrophyTitleRecord candidate;
		candidate.Clear();
		int ret = FindNextTitle(handle, curIndex, &foundIndex, &candidate);
		if (ret < 0)
			return ret;
		if (foundIndex == 0xFFFFFFFFu)
			break;
		if (candidate.IsBroken())
			return ReadTitleAt(handle, foundIndex, outRecord);
		if (oldestSlot == -1 ||
		    isTickEarlier(candidate.titleSnapshot.lastUnlockedTick, oldestTick)) {
			oldestSlot = (int)foundIndex;
			oldestTick = candidate.titleSnapshot.lastUnlockedTick;
		}
		curIndex = foundIndex + 1;
	}

	if (oldestSlot == -1) {
		outRecord->Clear();
		return 0;
	}
	return ReadTitleAt(handle, (uint32_t)oldestSlot, outRecord);
}

/* 0x812E05CC */
int TrophyUserIndexFile::RemoveTitle(TrophyHandle *handle, const TrophyCommId *commId,
				     int *outRemovedSlot)
{
	if (!InTransaction())
		return NP_TROPHY_ERR_TXN_NOT_OPEN;

	int foundSlot = -1;
	TrophyTitleRecord existing;
	existing.Clear();
	int ret = FindTitle(handle, commId, &foundSlot, &existing);
	if (ret < 0)
		return ret;
	if (foundSlot == -1) {
		if (outRemovedSlot != NULL)
			*outRemovedSlot = -1;
		return 0;
	}

	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	ret = ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;

	summary.totalTitles -= 1;
	if (existing.HasPendingUpload())
		summary.pendingUploadTitles -= 1;
	if (existing.HasDeferredUploadOnly())
		summary.deferredUploadTitles -= 1;
	if (existing.HasInstalledFlag())
		summary.installedTitles -= 1;

	UserIndexEntryRecord emptyHost;
	sceClibMemset(&emptyHost, 0, sizeof(emptyHost));
	ret = WriteEntryRaw(handle, (uint32_t)foundSlot, &emptyHost);
	if (ret < 0)
		return ret;

	ret = WriteSummary(handle, &summary);
	if (ret < 0)
		return ret;

	if (outRemovedSlot != NULL)
		*outRemovedSlot = foundSlot;
	return 0;
}

/* 0x812E0B40 */
int TrophyUserIndexFile::AddTitle(TrophyHandle *handle, TrophyTitleRecord *ioRecord)
{
	if (!InTransaction())
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	if (ioRecord->slotIndex != -1 || ioRecord->commId.IsEmpty())
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	int existingSlot = -1;
	int ret = FindTitle(handle, &ioRecord->commId, &existingSlot, NULL);
	if (ret < 0)
		return ret;
	if (existingSlot != -1)
		return NP_TROPHY_ERR_TITLE_ALREADY_EXISTS;

	uint32_t count = 0;
	ret = m_recordFile.GetRecordCount(m_entriesSection, &count);
	if (ret < 0)
		return ret;

	int reuseSlot = -1;
	for (uint32_t idx = 0; idx < count; ++idx) {
		UserIndexEntryRecord probe;
		ret = ReadEntryRaw(handle, idx, &probe);
		if (ret < 0)
			return ret;
		if (probe.activeFlag == 0) {
			reuseSlot = (int)idx;
			break;
		}
	}

	UserIndexEntryRecord host;
	ret = packUserIndexEntry(ioRecord, &host);
	if (ret < 0)
		return ret;

	uint32_t assignedSlot = 0;
	if (reuseSlot == -1) {
		ret = AppendEntryRaw(handle, &host, &assignedSlot);
		if (ret < 0)
			return ret;
	} else {
		assignedSlot = (uint32_t)reuseSlot;
		ret = WriteEntryRaw(handle, assignedSlot, &host);
		if (ret < 0)
			return ret;
	}

	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	ret = ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;

	summary.totalTitles += 1;
	if (ioRecord->HasPendingUpload())
		summary.pendingUploadTitles += 1;
	if (ioRecord->HasDeferredUploadOnly())
		summary.deferredUploadTitles += 1;
	if (ioRecord->HasInstalledFlag())
		summary.installedTitles += 1;

	ret = WriteSummary(handle, &summary);
	if (ret < 0)
		return ret;

	ioRecord->slotIndex = (int)assignedSlot;
	return 0;
}

/* 0x812E0D98 */
int TrophyUserIndexFile::UpdateTitle(TrophyHandle *handle, const TrophyTitleRecord *record)
{
	if (!InTransaction())
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	if (record->slotIndex == -1)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;

	TrophyTitleRecord oldRecord;
	oldRecord.Clear();
	ret = ReadTitleAt(handle, (uint32_t)record->slotIndex, &oldRecord);
	if (ret < 0)
		return ret;
	if (!record->commId.Equals(oldRecord.commId))
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	if (oldRecord.HasPendingUpload())
		summary.pendingUploadTitles -= 1;
	if (oldRecord.HasDeferredUploadOnly())
		summary.deferredUploadTitles -= 1;
	if (oldRecord.HasInstalledFlag())
		summary.installedTitles -= 1;

	if (record->HasPendingUpload())
		summary.pendingUploadTitles += 1;
	if (record->HasDeferredUploadOnly())
		summary.deferredUploadTitles += 1;
	if (record->HasInstalledFlag())
		summary.installedTitles += 1;

	UserIndexEntryRecord host;
	ret = packUserIndexEntry(record, &host);
	if (ret < 0)
		return ret;

	ret = WriteEntryRaw(handle, (uint32_t)record->slotIndex, &host);
	if (ret < 0)
		return ret;

	return WriteSummary(handle, &summary);
}

/* 0x812E0FE6 */
int TrophyUserIndexFile::ReadJournal(TrophyJournalState *outJournal) const
{
	return GetJournalState(outJournal);
}

/* 0x812E0FF6 */
int TrophyUserIndexFile::UpdateSyncAttemptTicks(TrophyHandle *handle,
						const SceRtcTick *attemptTick,
						const SceRtcTick *successTick)
{
	if (!InTransaction())
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	summary.lastSyncAttemptTick.secureTick = *attemptTick;
	if (successTick != NULL)
		summary.lastSyncAttemptTick.localTick = *successTick;
	return WriteSummary(handle, &summary);
}

/* 0x812E1084 */
int TrophyUserIndexFile::UpdateSyncSuccessTicks(TrophyHandle *handle,
						const SceRtcTick *attemptTick,
						const SceRtcTick *successTick)
{
	if (!InTransaction())
		return NP_TROPHY_ERR_TXN_NOT_OPEN;
	TrophyUserIndexSummary summary;
	sceClibMemset(&summary, 0, sizeof(summary));
	int ret = ReadSummary(handle, &summary);
	if (ret < 0)
		return ret;
	summary.lastSyncSuccessTick.secureTick = *attemptTick;
	if (successTick != NULL)
		summary.lastSyncSuccessTick.localTick = *successTick;
	return WriteSummary(handle, &summary);
}
