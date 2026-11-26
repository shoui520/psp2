/*
 * dbsetup_lib.cpp: content database setup and schema validation library
 * (SceDbsl_, 0x8135F470..0x813603D7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <framework.h>
#include <file/localfile.h>
#include <dir/dir.h>
#include <buffer/buffer.h>
#include <datetime/datetime.h>
#include <sqlite3.h>
#include <dbutil/connection.h>
#include <dbutil/statement.h>
#include "../../db_check.h"
#include "dbsetup_internal.h"

#define DBSL_ERR_INVALID_DB_ID		-2146411263	/* 0x80105D01 */
#define DBSL_ERR_PATH_TOO_LONG		-2146411262	/* 0x80105D02 */
#define DBSL_ERR_INVALID_PATH		-2146411261	/* 0x80105D03 */
#define DBSL_ERR_MUTEX_FAILED		-2146411260	/* 0x80105D04 */
#define DBSL_ERR_VERSION_MISMATCH	-2146411259	/* 0x80105D05 */
#define DBSL_ERR_DB_BUSY		-2146411258	/* 0x80105D06 */
#define DBSL_ERR_VERSION_NEWER		-2146411024	/* 0x80105DF0 */
#define DBSL_ERR_DB_CORRUPT		-2146411023	/* 0x80105DF1 */
#define DBSL_ERR_SQLITE_OTHER		-2146411022	/* 0x80105DF2 */

#define DBSL_COPY_BUFFER_SIZE		0x4000U
#define DBSL_MUTEX_RETRY_COUNT		5
#define DBSL_MUTEX_TIMEOUT_USEC		1000000U
#define DBSL_MUTEX_ATTR			0x2080U
#define DBSL_BUSY_TIMEOUT_MSEC		20000
#define DBSL_DESCRIPTOR_COUNT		8

struct DbSetupDescriptor {
	int id;
	int userDirType;
	const char *dbPath;
	const char *templatePath;
	int fileMode;
	int dirMode;
	unsigned int expectedVersion;
	int rebuildNotifyError;
};

static const DbSetupDescriptor s_dbSetupTable[DBSL_DESCRIPTOR_COUNT] = {
	{ 1, 0, "ux0:/mms/music/AVContent.db",
	  "vs0:vsh/common/mms/db_template/AVContentMusic.db",
	  6, 262, 265U, DBSL_ERR_DB_CORRUPT },
	{ 2, 0, "ux0:/mms/photo/AVContent.db",
	  "vs0:vsh/common/mms/db_template/AVContentPhoto.db",
	  6, 262, 265U, DBSL_ERR_DB_CORRUPT },
	{ 3, 0, "ux0:/mms/video/AVContent.db",
	  "vs0:vsh/common/mms/db_template/AVContentVideo.db",
	  6, 262, 266U, DBSL_ERR_DB_CORRUPT },
	{ 4, 1, "/psnfriend/database/friends.db",
	  "vs0:vsh/common/mms/db_template/friends.db",
	  6, 262, 1U, 0 },
	{ 5, 0, "ur0:contacts/database/addressbook.db",
	  "vs0:vsh/common/mms/db_template/addressbook.db",
	  6, 262, 2U, 0 },
	{ 6, 1, "/psnmsg/messages.db",
	  "vs0:vsh/common/mms/db_template/messages.db",
	  6, 262, 32U, DBSL_ERR_DB_CORRUPT },
	{ 7, 1, "/grpmsg/database/grpmsgui.db",
	  "vs0:vsh/common/mms/db_template/grpmsgui.db",
	  6, 262, 1U, 0 },
	{ 8, 0, "ux0:/calendar/calendar.db",
	  "vs0:vsh/common/mms/db_template/calendar.db",
	  6, 262, 2U, 0 },
};

extern "C" {
extern SceUID sceKernelOpenMutex(const char *name);
extern int sceKernelCloseMutex(SceUID mutexId);
extern int sceAppMgrGetUserDirPath(int type, char *buf, SceSize size);
}

/* 0x8136014A: sqlite3_exec callback extracting PRAGMA user_version */
static int dbSetupUserVersionCallback(void *userArg, int colCount,
				      char **colValues, char **colNames)
{
	(void)colCount;
	unsigned int *outVersion = (unsigned int *)userArg;
	if (colNames[0] != NULL && colValues[0] != NULL &&
	    sce_paf_strcmp(colNames[0], "user_version") == 0) {
		*outVersion = (unsigned int)sce_paf_atoi(colValues[0]);
	}
	return 0;
}

/* 0x8135F470: read PRAGMA user_version from an SQLite database file */
static int dbSetupReadUserVersion(const char *dbPath, unsigned int *outVersion)
{
	sqlite3 *db = NULL;
	char *errMsg = NULL;
	int rc = sqlite3_open_v2(dbPath, &db, SQLITE_OPEN_READWRITE, NULL);
	if (rc == SQLITE_OK) {
		sqlite3_busy_timeout(db, DBSL_BUSY_TIMEOUT_MSEC);
		rc = sqlite3_exec(db, "PRAGMA user_version",
				  dbSetupUserVersionCallback, outVersion, &errMsg);
		if (rc != SQLITE_OK)
			sqlite3_free(errMsg);
	}
	sqlite3_close(db);

	switch (rc) {
	case SQLITE_OK:
		return 0;
	case SQLITE_BUSY:
		return DBSL_ERR_DB_BUSY;
	case SQLITE_CORRUPT:
	case SQLITE_CANTOPEN:
	case SQLITE_FORMAT:
	case SQLITE_NOTADB:
		return DBSL_ERR_DB_CORRUPT;
	default:
		return DBSL_ERR_SQLITE_OTHER;
	}
}

/* 0x8135F53E: copy initial template database into place via .tmp and seed tbl_config */
static int dbSetupCopyTemplateDb(const char *dstPath, const char *srcPath,
				 int dbId, int fileMode)
{
	paf::LocalFile srcFile;
	paf::LocalFile dstFile;
	paf::LocalFileStat statBuf;

	int err = paf::LocalFile::Getstat(srcPath, &statBuf);
	if (err != 0)
		return err;

	paf::common::SharedPtr<paf::MallocBuffer> copyBuf =
		paf::MallocBuffer::Allocate(DBSL_COPY_BUFFER_SIZE);
	if (copyBuf.get() == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	paf::LocalFile::OpenArg openArg;
	openArg.filename = srcPath;
	openArg.flag = SCE_O_RDONLY;
	openArg.mode = statBuf.st_mode;
	err = srcFile.Open(&openArg);
	if (err != 0)
		return err;

	paf::string tmpPath(dstPath != NULL ? dstPath : "");
	tmpPath += ".tmp";

	openArg.filename = tmpPath.c_str();
	openArg.flag = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC;
	openArg.mode = (uint32_t)fileMode;
	err = dstFile.Open(&openArg);
	if (err != 0)
		return err;

	while (!srcFile.IsEof()) {
		int32_t readBytes = srcFile.Read(copyBuf->GetAddr(), copyBuf->GetSize());
		if (readBytes < 0) {
			srcFile.Close();
			dstFile.Close();
			return readBytes;
		}
		int32_t writtenBytes = dstFile.Write(copyBuf->GetAddr(), (size_t)readBytes);
		if (writtenBytes < 0) {
			srcFile.Close();
			dstFile.Close();
			return writtenBytes;
		}
	}

	srcFile.Close();
	dstFile.Close();
	paf::LocalFile::RenameFile(tmpPath.c_str(), dstPath);

	if (dbId == 1 || dbId == 2 || dbId == 3) {
		int existingVal = 0;
		dbutil::Connection conn;
		paf::string dbPathStr(dstPath != NULL ? dstPath : "");
		if (conn.Open(dbPathStr, SQLITE_OPEN_READWRITE) == 0) {
			conn.Begin(DBUTIL_TRANSACTION_DEFERRED);
			if (dbConfigGetInt(&conn, 0, &existingVal) == DBCONFIG_ERR_NOT_FOUND) {
				int initVal = -1;
				if (dbId == 2) {
					initVal = 1;
				} else if (dbId == 1 || dbId == 3) {
					paf::Framework *fw = paf::Framework::Instance();
					if (fw != NULL)
						initVal = (int)fw->GetLocale();
				}
				dbConfigSetInt(&conn, 0, initVal);
			}
			conn.Commit();
		}
	}

	return 0;
}

/* 0x81360174: find descriptor entry by known database ID */
static const DbSetupDescriptor *dbSetupFindDescriptor(int dbId)
{
	for (unsigned int i = 0; i < DBSL_DESCRIPTOR_COUNT; ++i) {
		if (s_dbSetupTable[i].id == dbId)
			return &s_dbSetupTable[i];
	}
	return NULL;
}

/* 0x813601A8: resolve target filesystem path for a known database ID */
int dbSetupResolvePath(int dbId, char *outPath, SceSize outLen)
{
	const DbSetupDescriptor *desc = dbSetupFindDescriptor(dbId);
	if (desc == NULL)
		return DBSL_ERR_INVALID_DB_ID;

	if (desc->userDirType != 0) {
		char userDir[32];
		sceAppMgrGetUserDirPath(desc->userDirType, userDir, sizeof(userDir));
		sce_paf_snprintf(outPath, outLen, "%s%s", userDir, desc->dbPath);
	} else {
		sce_paf_strncpy(outPath, desc->dbPath, outLen);
	}
	return 0;
}

/* Named kernel mutex guard for SceDbsl_<basename> (0x8136024C..0x81360388) */
class DbSetupScopedMutex {
public:
	DbSetupScopedMutex(const char *dbPath, int *outError);
	~DbSetupScopedMutex();

private:
	int Acquire(const char *dbPath);

	SceUID m_mutexId;
	unsigned int m_flags;
};

/* 0x8136024C */
int DbSetupScopedMutex::Acquire(const char *dbPath)
{
	m_flags = 0;

	char mutexName[32];
	sce_paf_memset(mutexName, 0, sizeof(mutexName) - 1);
	sce_paf_strncpy(mutexName, "SceDbsl_", 30);

	const char *baseName = sce_paf_strrchr(dbPath, '/');
	const char *suffix = (baseName != NULL) ? (baseName + 1) : dbPath;
	sce_paf_strncat(mutexName, suffix, 30 - sce_paf_strlen("SceDbsl_"));

	int createRetries = DBSL_MUTEX_RETRY_COUNT;
	int lockRetries = DBSL_MUTEX_RETRY_COUNT;

	for (;;) {
		SceUID id = sceKernelOpenMutex(mutexName);
		m_mutexId = id;
		if (id >= 0)
			break;

		SceKernelMutexOptParam opt;
		opt.size = sizeof(opt);
		opt.ceilingPriority = 0;
		id = sceKernelCreateMutex(mutexName, DBSL_MUTEX_ATTR, 0, &opt);
		if (id >= 0) {
			m_mutexId = id;
			m_flags |= 1U;
			break;
		}

		if (createRetries <= 0)
			return DBSL_ERR_MUTEX_FAILED;
		--createRetries;
	}

	for (;;) {
		SceUInt32 timeout = DBSL_MUTEX_TIMEOUT_USEC;
		if (sceKernelLockMutex(m_mutexId, 1, &timeout) >= 0)
			return 0;
		if (lockRetries <= 0)
			return DBSL_ERR_MUTEX_FAILED;
		--lockRetries;
	}
}

/* 0x81360366 */
DbSetupScopedMutex::DbSetupScopedMutex(const char *dbPath, int *outError)
	: m_mutexId(-1), m_flags(0)
{
	*outError = Acquire(dbPath);
}

/* 0x81360388 */
DbSetupScopedMutex::~DbSetupScopedMutex()
{
	if (m_mutexId >= 0) {
		sceKernelUnlockMutex(m_mutexId, 1);
		if ((m_flags & 1U) != 0) {
			sceKernelDeleteMutex(m_mutexId);
			m_flags &= ~1U;
		} else {
			sceKernelCloseMutex(m_mutexId);
		}
		m_mutexId = -1;
	}
}

static void dbSetupFormatCorruptPath(char *outBuf, SceSize outSize, const char *dbPath)
{
	sce_paf_memset(outBuf, 0, outSize);
	sce_paf_strncpy(outBuf, dbPath, outSize - 1);
	size_t curLen = sce_paf_strlen(outBuf);
	if (curLen < outSize - 1)
		sce_paf_strncat(outBuf, "-corrupt", (outSize - 1) - curLen);
}

/* 0x8135FB5C: verify or recreate a managed content database */
int DbCheckOpen(const DbCheckParam *param, DbCheckResult *result)
{
	char dbPath[132];
	char templatePath[132];
	int fileMode = 0;
	int dirMode = 0;
	unsigned int expectedVersion = 0;
	int rebuildNotifyError = 0;
	SceUInt64 options = 0;

	if (result != NULL) {
		result->status = 0;
		result->version = 0;
	}

	if (param->id != 0) {
		const DbSetupDescriptor *desc = dbSetupFindDescriptor(param->id);
		if (desc == NULL)
			return DBSL_ERR_INVALID_DB_ID;
		fileMode = desc->fileMode;
		dirMode = desc->dirMode;
		dbSetupResolvePath(param->id, dbPath, 0x81U);
		sce_paf_strncpy(templatePath, desc->templatePath, 0x81U);
		expectedVersion = desc->expectedVersion;
		rebuildNotifyError = desc->rebuildNotifyError;
	} else {
		if (param->path == NULL || param->initialPath == NULL)
			return DBSL_ERR_INVALID_PATH;
		fileMode = param->copyFlags;
		dirMode = param->dirMode;
		sce_paf_strncpy(dbPath, param->path, 0x81U);
		sce_paf_strncpy(templatePath, param->initialPath, 0x81U);
		expectedVersion = 0;
		rebuildNotifyError = 0;
	}
	options = param->option;

	int mutexErr = 0;
	DbSetupScopedMutex guard(dbPath, &mutexErr);
	if (mutexErr != 0)
		return mutexErr;

	bool dbExists = paf::LocalFile::Exists(dbPath);
	if (dbExists && result != NULL)
		result->status |= 2U;

	bool needRecreate = false;
	unsigned int currentVersion = 0;

	if (dbExists && (options & DB_CHECK_OPTION_REMOVE) != 0) {
		needRecreate = true;
	} else {
		bool versionReadFailed = false;
		if (dbExists) {
			int verErr = dbSetupReadUserVersion(dbPath, &currentVersion);
			if (verErr == DBSL_ERR_DB_BUSY)
				return verErr;
			if (verErr != 0) {
				if ((options & (DB_CHECK_OPTION_RESET_ON_VERSION |
						DB_CHECK_OPTION_REMOVE)) == 0) {
					return verErr;
				}
				versionReadFailed = true;
			} else if (result != NULL) {
				result->version = currentVersion;
			}
		}

		if (expectedVersion == 0) {
			int tmplErr = dbSetupReadUserVersion(templatePath, &expectedVersion);
			if (tmplErr != 0)
				return tmplErr;
		}

		if (versionReadFailed) {
			needRecreate = true;
		} else {
			int checkErr = 0;
			if (!paf::LocalFile::Exists(dbPath)) {
				checkErr = SCE_ERROR_ERRNO_ENOENT;
			} else {
				char corruptPath[138];
				dbSetupFormatCorruptPath(corruptPath, sizeof(corruptPath), dbPath);
				if (paf::LocalFile::Exists(corruptPath)) {
					checkErr = DBSL_ERR_DB_CORRUPT;
				} else if (currentVersion < expectedVersion) {
					checkErr = DBSL_ERR_VERSION_MISMATCH;
				} else if (currentVersion > expectedVersion) {
					checkErr = DBSL_ERR_VERSION_NEWER;
				} else {
					checkErr = 0;
				}
			}

			if (checkErr == DBSL_ERR_VERSION_MISMATCH) {
				if (result != NULL)
					result->version = currentVersion;
				return checkErr;
			}
			if (checkErr == DBSL_ERR_VERSION_NEWER || checkErr == DBSL_ERR_DB_CORRUPT) {
				if ((options & (DB_CHECK_OPTION_RESET_ON_VERSION |
						DB_CHECK_OPTION_REMOVE)) == 0) {
					if (result != NULL)
						result->version = currentVersion;
					return checkErr;
				}
				needRecreate = true;
			} else if (checkErr != 0) {
				needRecreate = true;
			}
		}
	}

	if (needRecreate) {
		if (dbExists) {
			int rmErr = paf::LocalFile::RemoveFile(dbPath);
			if (rmErr != 0)
				return rmErr;
		}

		paf::string journalPath(dbPath);
		journalPath += "-journal";
		paf::LocalFile::RemoveFile(journalPath.c_str());

		if (rebuildNotifyError == 0) {
			char corruptPath[138];
			dbSetupFormatCorruptPath(corruptPath, sizeof(corruptPath), dbPath);
			if (paf::LocalFile::Exists(corruptPath))
				paf::LocalFile::RemoveFile(corruptPath);
		}

		if (sce_paf_strlen(dbPath) >= 0x80U)
			return DBSL_ERR_PATH_TOO_LONG;

		const char *lastSlash = sce_paf_strrchr(dbPath, '/');
		if (lastSlash != NULL) {
			char parentDir[132];
			size_t parentLen = (size_t)(lastSlash - dbPath);
			sce_paf_memcpy(parentDir, dbPath, parentLen);
			parentDir[parentLen] = '\0';
			if (!paf::Dir::IsExist(parentDir)) {
				int mkErr = paf::Dir::CreateRecursive(parentDir, (uint32_t)dirMode);
				if (mkErr != 0)
					return mkErr;
			}
		}

		int copyErr = dbSetupCopyTemplateDb(dbPath, templatePath, param->id, fileMode);
		if (copyErr != 0)
			return copyErr;

		if (result != NULL)
			result->status |= 1U;

		const char *syncEnd = sce_paf_strrchr(dbPath, '/');
		if (syncEnd == NULL)
			syncEnd = sce_paf_strchr(dbPath, ':');
		if (syncEnd != NULL) {
			size_t devLen = (size_t)(syncEnd - dbPath);
			if (devLen < 0x80U) {
				paf::string devicePath(dbPath, devLen);
				sceIoSync(devicePath.c_str(), 0);
			}
		}

		if (result != NULL) {
			int verErr = dbSetupReadUserVersion(dbPath, &result->version);
			if (verErr != 0)
				return verErr;
		}
	}

	if ((options & DB_CHECK_OPTION_REMOVE) == 0 &&
	    result != NULL && result->status == 3U) {
		return rebuildNotifyError;
	}
	return 0;
}
