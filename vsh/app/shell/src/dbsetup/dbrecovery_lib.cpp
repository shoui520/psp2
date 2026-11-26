/*
 * dbrecovery_lib.cpp: database recovery markers, memory-card integrity check,
 * and tbl_config helpers (0x813603D8..0x813613E3).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <kernel/iofilemgr.h>
#include <file/localfile.h>
#include <thread/thread.h>
#include <sqlite3.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include "../../db_check.h"
#include "../livespace_db/lsdb_keys.h"
#include "dbsetup_internal.h"

#define DBRECOVERY_ERR_INVALID_ARG	-2146411007	/* 0x80105E01 */
#define DBRECOVERY_ERR_PATH_TOO_LONG	-2146411006	/* 0x80105E02 */
#define DBRECOVERY_ERR_BAD_ERRFILE	-2146411002	/* 0x80105E06 */

#define DBRECOVERY_ERRFILE_PATH		"ur0:shell/db/dbr.db-err"
#define DBRECOVERY_ERRFILE_MAGIC_MASK	0xFFFF0000U
#define DBRECOVERY_ERRFILE_MAGIC	0xCAFE0000U
#define DBRECOVERY_FLAG_RESET		0xCAFE0100U
#define DBRECOVERY_FLAG_CORRUPT		0xCAFE0200U

#define DBRECOVERY_CORRUPT_MAGIC	0xFEE10001U
#define DBRECOVERY_RESET_MAGIC		0xFEE10002U

#define DBRECOVERY_LSDB_MAX_ENTRIES	500
#define DBRECOVERY_LSDB_BASE_PAGE_ID	-100000000

#define MC_BIND_ERR_UNBOUND_SYSTEM	0x00000001
#define MC_BIND_ERR_ACCOUNT_MISMATCH	0x00000002
#define MC_BIND_ERR_DEVICE_MISMATCH	0x00000004
#define MC_BIND_ERR_FW_TOO_OLD		0x00000008
#define MC_BIND_ERR_DIGEST_INVALID	0x00000010
#define MC_BIND_ERR_CARD_CHANGED	0x00080000

#define SCE_VSH_MOUNT_UX0		0x800

struct SceKernelFwInfo {
	SceSize size;
	char versionString[28];
	SceUInt32 version;
	SceUInt32 unk;
};

struct LsdbCursorHandle {
	void *opaque;
};

extern "C" {
extern int sceKernelGetSystemSwVersion(SceKernelFwInfo *info);
extern int sceRegMgrGetKeyBin(const char *category, const char *name,
			      void *buf, SceSize size);
extern int vshIoMount(int id, const char *path, int permission,
		      int a4_unused, int a5_unused, int a6_unused);
extern int vshIoUmount(int id, int a2_unused, int a3_unused, int a4_unused);
extern int SceNpManager_E45E4003(char *outOnlineId);
extern int SceLsdb_B0E42B67(void);
extern void SceLsdb_D322A870(LsdbCursorHandle *cursor);
extern int SceLsdb_CF4B3684(LsdbCursorHandle *cursor, int pageId, int mode);
extern int SceLsdb_4DB9BCB4(LsdbCursorHandle *cursor);
extern void SceLsdb_56D8DCF0(LsdbCursorHandle *cursor);
extern int SceLsdb_D4B1C54E(paf::string *outMemoryCardId);
}

/* 0x815C64B8: static mutex constructed at startup (0x81360854) */
static paf::thread::Mutex s_dbRecoveryIntegrityMutex("SceDbRecoveryCheckIntegrity", 0);

static int dbRecoveryReadErrorFile(unsigned int *outFlags);
static int dbRecoveryCheckIntegrity(void);
static int dbRecoveryClassifyMemoryCardState(void);

/* 0x813603D8 */
int DbCheckNeedsRecovery(int *needsRecovery)
{
	if (needsRecovery == NULL)
		return DBRECOVERY_ERR_INVALID_ARG;

	*needsRecovery = 0;
	if (dbRecoveryCheckIntegrity() != 0) {
		*needsRecovery = 1;
		return 0;
	}

	unsigned int errorFlags = 0;
	int err = dbRecoveryReadErrorFile(&errorFlags);
	if (err == 0 && errorFlags != 0) {
		*needsRecovery = 1;
		return 0;
	}
	return err;
}

/* 0x81360432 */
int DbCheckGetMemoryCardStatus(void)
{
	return dbRecoveryClassifyMemoryCardState();
}

/* 0x8136043C */
static int dbRecoveryReadErrorFile(unsigned int *outFlags)
{
	*outFlags = 0;
	if (!paf::LocalFile::Exists(DBRECOVERY_ERRFILE_PATH))
		return 0;

	paf::LocalFile file;
	paf::LocalFile::OpenArg openArg;
	openArg.filename = DBRECOVERY_ERRFILE_PATH;
	openArg.flag = SCE_O_RDONLY;
	int err = file.Open(&openArg);
	if (err != 0)
		return err;

	unsigned int storedFlags = 0;
	int32_t bytesRead = file.Read(&storedFlags, sizeof(storedFlags));
	file.Close();
	if (bytesRead >= (int32_t)sizeof(storedFlags) &&
	    (storedFlags & DBRECOVERY_ERRFILE_MAGIC_MASK) == DBRECOVERY_ERRFILE_MAGIC) {
		*outFlags = storedFlags;
		return 0;
	}

	paf::LocalFile::RemoveFile(DBRECOVERY_ERRFILE_PATH);
	return DBRECOVERY_ERR_BAD_ERRFILE;
}

/* 0x813604FE */
static int dbRecoverySetErrorFlags(unsigned int flags)
{
	unsigned int currentFlags = 0;
	paf::LocalFile file;
	paf::LocalFile::OpenArg openArg;
	openArg.filename = DBRECOVERY_ERRFILE_PATH;
	openArg.flag = SCE_O_RDWR | SCE_O_CREAT;
	openArg.mode = 262;

	int err = file.Open(&openArg);
	if (err != 0)
		return err;

	file.Read(&currentFlags, sizeof(currentFlags));
	currentFlags |= flags;
	file.Seek(0, SCE_SEEK_SET);
	int32_t written = file.Write(&currentFlags, sizeof(currentFlags));
	file.Close();
	return written;
}

/* 0x8136059A */
static int dbRecoveryWriteCorruptMarker(const char *dbPath, int writeMagic)
{
	paf::LocalFile file;
	if (dbPath == NULL)
		return DBRECOVERY_ERR_INVALID_ARG;
	if (sce_paf_strlen(dbPath) > 0x80U)
		return DBRECOVERY_ERR_PATH_TOO_LONG;

	char markerPath[138];
	sce_paf_memset(markerPath, 0, sizeof(markerPath));
	sce_paf_strncpy(markerPath, dbPath, sizeof(markerPath) - 1);
	size_t curLen = sce_paf_strlen(markerPath);
	sce_paf_strncat(markerPath, "-corrupt", (sizeof(markerPath) - 1) - curLen);

	paf::LocalFile::OpenArg openArg;
	openArg.filename = markerPath;
	openArg.flag = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC;
	openArg.mode = 262;
	int err = file.Open(&openArg);
	if (err != 0)
		return err;

	if (writeMagic != 0) {
		unsigned int magic = DBRECOVERY_CORRUPT_MAGIC;
		int32_t written = file.Write(&magic, sizeof(magic));
		if (written < 0)
			return written;
	}

	file.Close();
	return 0;
}

/* 0x81360682 */
static int dbRecoveryWriteResetMarker(const char *dbPath)
{
	paf::LocalFile file;
	if (dbPath == NULL)
		return DBRECOVERY_ERR_INVALID_ARG;
	if (sce_paf_strlen(dbPath) > 0x80U)
		return DBRECOVERY_ERR_PATH_TOO_LONG;

	char markerPath[138];
	sce_paf_memset(markerPath, 0, sizeof(markerPath));
	sce_paf_strncpy(markerPath, dbPath, sizeof(markerPath) - 1);
	size_t curLen = sce_paf_strlen(markerPath);
	sce_paf_strncat(markerPath, "-corrupt", (sizeof(markerPath) - 1) - curLen);

	paf::LocalFile::OpenArg openArg;
	openArg.filename = markerPath;
	openArg.flag = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC;
	openArg.mode = 262;
	int err = file.Open(&openArg);
	if (err != 0)
		return err;

	unsigned int magic = DBRECOVERY_RESET_MAGIC;
	int32_t written = file.Write(&magic, sizeof(magic));
	if (written < 0)
		return written;

	file.Close();
	return 0;
}

/* 0x81360766 */
int DbCheckReset(int id)
{
	char dbPath[132];
	sce_paf_memset(dbPath, 0, 0x81U);
	int err = dbSetupResolvePath(id, dbPath, 0x81U);
	if (err != 0)
		return err;

	err = dbRecoveryWriteResetMarker(dbPath);
	if (err >= 0) {
		dbRecoverySetErrorFlags(DBRECOVERY_FLAG_RESET);
		return 0;
	}
	return err;
}

/* 0x813607BE */
int DbCheckSetCorruptByPath(const char *path, int writeMagic)
{
	int err = dbRecoveryWriteCorruptMarker(path, writeMagic);
	if (err >= 0) {
		dbRecoverySetErrorFlags(DBRECOVERY_FLAG_CORRUPT);
		return 0;
	}
	return err;
}

/* 0x813607DA */
int DbCheckSetCorrupt(int id, int corrupt)
{
	char dbPath[132];
	sce_paf_memset(dbPath, 0, 0x81U);
	int err = dbSetupResolvePath(id, dbPath, 0x81U);
	if (err != 0)
		return err;

	err = dbRecoveryWriteCorruptMarker(dbPath, corrupt);
	if (err >= 0) {
		dbRecoverySetErrorFlags(DBRECOVERY_FLAG_CORRUPT);
		return 0;
	}
	return err;
}

/* 0x81360836 */
int DbCheckClearCorruptByPath(const char *path)
{
	int err = dbRecoveryWriteCorruptMarker(path, 0);
	if (err >= 0) {
		dbRecoverySetErrorFlags(DBRECOVERY_FLAG_CORRUPT);
		return 0;
	}
	return err;
}

/* 0x81360A48: verify LSDB does not exceed maximum 500 home-screen entries */
static int dbRecoveryCheckLsdbOverflow(void)
{
	int err = SceLsdb_B0E42B67();
	if (err != 0)
		return err;

	LsdbCursorHandle cursor;
	SceLsdb_D322A870(&cursor);
	int pendingCount = SceLsdb_CF4B3684(&cursor, DBRECOVERY_LSDB_BASE_PAGE_ID, 1);
	if (pendingCount > 0) {
		int existingCount = SceLsdb_4DB9BCB4(&cursor);
		if (existingCount >= 0) {
			int remaining = DBRECOVERY_LSDB_MAX_ENTRIES - existingCount;
			if (remaining < pendingCount)
				pendingCount = remaining;
			SceLsdb_56D8DCF0(&cursor);
			if (pendingCount != 0)
				return -1;
			return 0;
		}
	}
	SceLsdb_56D8DCF0(&cursor);
	return 0;
}

/*
 * Memory-card id.dat vs. system state comparator singleton (0x81360B38..0x81360F20,
 * 108 bytes at 0x815C6558).
 */
class IdDatStateManager {
public:
	static IdDatStateManager *Instance();

	int IsMemoryCardChanged() const;
	int CheckAccountBinding() const;
	void PrepareUpdateFields(SceUInt32 *ioFieldMask);
	int CheckAndSyncMemoryCard();

	bool HasStoredMid() const { return !m_storedMid.empty(); }
	bool HasCardMid() const { return !m_cardInfo.mid.empty(); }

private:
	IdDatStateManager();
	~IdDatStateManager();

	int LoadConsoleIdentity();

	ShellActivationInfo m_cardInfo;		/* +0x00..+0x34 (from ux0:id.dat) */
	paf::string m_storedMid;		/* +0x38 (from SceLsdb) */
	SceUInt64 m_consoleAccountId;		/* +0x48 (from /CONFIG/NP account_id) */
	paf::string m_consoleDid;		/* +0x50 (SHA-1 of OpenPSID) */
	paf::string m_consoleOnlineId;		/* +0x5C */
	SceUInt32 m_consoleSwVersion;		/* +0x68 (from sceKernelGetSystemSwVersion) */
};

/* 0x81360B38 */
int IdDatStateManager::LoadConsoleIdentity()
{
	SceLsdb_D4B1C54E(&m_storedMid);
	sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &m_consoleAccountId, sizeof(m_consoleAccountId));
	shellIdDatGenerateDid(&m_consoleDid);

	SceKernelFwInfo fwInfo;
	sce_paf_memset(&fwInfo, 0, sizeof(fwInfo));
	fwInfo.size = sizeof(fwInfo);
	if (sceKernelGetSystemSwVersion(&fwInfo) == 0)
		m_consoleSwVersion = fwInfo.version;
	return 0;
}

/* 0x81360BB0 */
IdDatStateManager::IdDatStateManager()
	: m_consoleAccountId(0), m_consoleSwVersion(0)
{
	LoadConsoleIdentity();
	shellIdDatReadFromPath(&m_cardInfo, NULL);
}

/* 0x81360F20 */
IdDatStateManager::~IdDatStateManager()
{
}

/* 0x81360BFC */
IdDatStateManager *IdDatStateManager::Instance()
{
	static IdDatStateManager s_instance;
	return &s_instance;
}

/* 0x81360C58 */
int IdDatStateManager::IsMemoryCardChanged() const
{
	if (m_storedMid.empty() && m_cardInfo.mid.empty())
		return 0;
	if (m_storedMid != m_cardInfo.mid)
		return 1;
	if (m_consoleDid != m_cardInfo.did)
		return 1;
	return 0;
}

/* 0x81360CA0 */
int IdDatStateManager::CheckAccountBinding() const
{
	if (!shellIdDatIsAccountBoundMode())
		return 0;
	if (m_cardInfo.aid != 0 && m_consoleAccountId == 0)
		return MC_BIND_ERR_UNBOUND_SYSTEM;
	if (m_cardInfo.aid != 0 && m_consoleAccountId != 0 &&
	    m_cardInfo.aid != m_consoleAccountId)
		return MC_BIND_ERR_ACCOUNT_MISMATCH;
	if (m_cardInfo.aid == 0 && m_consoleAccountId != 0 &&
	    !m_cardInfo.did.empty() && m_cardInfo.did != m_consoleDid)
		return MC_BIND_ERR_DEVICE_MISMATCH;
	if (m_consoleSwVersion < (SceUInt32)m_cardInfo.svr)
		return MC_BIND_ERR_FW_TOO_OLD;
	return 0;
}

/* 0x81360D38 */
void IdDatStateManager::PrepareUpdateFields(SceUInt32 *ioFieldMask)
{
	if (m_cardInfo.mid.empty()) {
		shellIdDatGenerateMid(&m_cardInfo.mid);
		*ioFieldMask |= 1U;
	}
	if (shellIdDatIsAccountBoundMode()) {
		if (m_consoleDid == m_cardInfo.did &&
		    m_cardInfo.aid == 0 && m_consoleAccountId != 0) {
			m_cardInfo.aid = m_consoleAccountId;
			*ioFieldMask |= 4U;
		}
	}
}

/* 0x81360D9E */
int IdDatStateManager::CheckAndSyncMemoryCard()
{
	SceUInt32 updateMask = 0;
	int missingCardResult = m_storedMid.empty() ? 0 : MC_BIND_ERR_CARD_CHANGED;

	SceIoStat statBuf;
	int statErr = sceIoGetstat("ux0:", &statBuf);
	if (statErr != 0) {
		if (statErr == SCE_ERROR_ERRNO_ENODEV) {
			SceUID fd = sceIoOpen("sdstor0:xmc-lp-ign-entire", SCE_O_RDONLY, 0);
			if (fd >= 0) {
				sceIoClose(fd);
				if (vshIoMount(SCE_VSH_MOUNT_UX0, NULL, 0, 0, 0, 0) != 0)
					missingCardResult |= MC_BIND_ERR_DIGEST_INVALID;
				else
					vshIoUmount(SCE_VSH_MOUNT_UX0, 0, 0, 0);
			}
		}
		m_cardInfo.mid.clear();
		return missingCardResult;
	}

	PrepareUpdateFields(&updateMask);

	int bindFlags = m_cardInfo.hasDigest ? MC_BIND_ERR_DIGEST_INVALID : 0;
	bindFlags |= CheckAccountBinding();
	if (bindFlags == 0 && IsMemoryCardChanged() != 0)
		bindFlags = MC_BIND_ERR_CARD_CHANGED;

	if (shellIdDatIsAccountBoundMode() && bindFlags == 0) {
		if ((SceInt32)m_cardInfo.svr < (SceInt32)m_consoleSwVersion) {
			m_cardInfo.svr = (int)m_consoleSwVersion;
			updateMask |= 0x10U;
		}
		if (m_cardInfo.oid.empty()) {
			char onlineIdBuf[328];
			if (SceNpManager_E45E4003(onlineIdBuf) == 0)
				m_cardInfo.oid = onlineIdBuf;
			if (!m_cardInfo.oid.empty())
				updateMask |= 8U;
		}
	}

	if (updateMask != 0) {
		NpIdDatUpdateInfo writeInfo;
		writeInfo.memoryCardId = m_cardInfo.mid;
		writeInfo.deviceId = m_cardInfo.did;
		writeInfo.accountId = m_cardInfo.aid;
		writeInfo.onlineId = m_cardInfo.oid;
		writeInfo.sysVersion = (SceUInt32)m_cardInfo.svr;
		writeInfo.hasDigest = m_cardInfo.hasDigest ? 1 : 0;
		if (shellIdDatWrite(&writeInfo, updateMask, 1, 0) != 0) {
			m_cardInfo.mid.clear();
			return missingCardResult;
		}
	}

	return bindFlags;
}

/* 0x81360888 */
static int dbRecoveryCheckIntegrity(void)
{
	if (IdDatStateManager::Instance()->CheckAndSyncMemoryCard() != 0)
		return -1;
	if (dbRecoveryCheckLsdbOverflow() != 0)
		return -2;
	return 0;
}

/* 0x813608B0 */
static int dbRecoveryClassifyMemoryCardState(void)
{
	IdDatStateManager *mgr = IdDatStateManager::Instance();
	if (!mgr->HasStoredMid() && !mgr->HasCardMid())
		return 0;
	if (mgr->IsMemoryCardChanged() == 0)
		return 1;
	if (!mgr->HasStoredMid() && mgr->HasCardMid())
		return 2;
	if (mgr->HasStoredMid() && !mgr->HasCardMid())
		return 3;
	return 4;
}

/* 0x81361196: REPLACE INTO tbl_config VALUES(?, ?) */
int dbConfigSetValue(dbutil::Connection *conn, int key, const dbutil::Value *value)
{
	dbutil::RefPtr<dbutil::Statement> stmt;
	paf::string sql("REPLACE INTO tbl_config VALUES(?, ?)");
	int err = conn->Prepare(sql, &stmt);
	int result;
	if (err != 0) {
		result = DBCONFIG_ERR_INTERNAL;
	} else {
		dbutil::Value keyVal;
		sce_paf_memset(&keyVal, 0, sizeof(keyVal));
		keyVal.type = SQLITE_INTEGER;
		keyVal.data.integer = key;
		result = stmt->Bind(1, &keyVal, false);
		if (result == 0) {
			if (stmt->Bind(2, value, false) == 0) {
				result = stmt->Step(1);
				if (result == 1)
					result = 0;
			} else {
				result = DBCONFIG_ERR_INTERNAL;
			}
		}
	}
	return result;
}

/* 0x8136127C */
int dbConfigSetInt(dbutil::Connection *conn, int key, int value)
{
	dbutil::Value intVal;
	sce_paf_memset(&intVal, 0, sizeof(intVal));
	intVal.type = SQLITE_INTEGER;
	intVal.data.integer = value;
	return dbConfigSetValue(conn, key, &intVal);
}

/* 0x813612BC: SELECT val FROM tbl_config WHERE ikey=? */
int dbConfigGetValue(dbutil::Connection *conn, int key, dbutil::Value *outValue)
{
	dbutil::RefPtr<dbutil::Statement> stmt;
	paf::string sql("SELECT val FROM tbl_config WHERE ikey=?");
	int err = conn->Prepare(sql, &stmt);
	int result;
	if (err != 0) {
		result = DBCONFIG_ERR_INTERNAL;
	} else {
		dbutil::Value keyVal;
		sce_paf_memset(&keyVal, 0, sizeof(keyVal));
		keyVal.type = SQLITE_INTEGER;
		keyVal.data.integer = key;
		result = stmt->Bind(1, &keyVal, false);
		if (result == 0) {
			result = stmt->Step(1);
			if (result == 0)
				result = stmt->GetValue(0, outValue);
			else if (result == 1)
				result = DBCONFIG_ERR_NOT_FOUND;
		}
	}
	return result;
}

/* 0x8136139E */
int dbConfigGetInt(dbutil::Connection *conn, int key, int *outValue)
{
	dbutil::Value val;
	int err = dbConfigGetValue(conn, key, &val);
	if (err == 0) {
		if (val.type == SQLITE_INTEGER)
			*outValue = (int)val.data.integer;
		else
			return DBCONFIG_ERR_INTERNAL;
	}
	return err;
}
