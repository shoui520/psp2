/*
 * An application's NpDrm license (its rif): found in its license folder, by the account's name
 * or the fixed one, read and its information taken; and the PocketStation's license checked
 * (0x810349D4..0x81035AA8). The file's name and every name here are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <npdrm/npdrm.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the SDK's glue over SceVshBridge's import (0x81417C70) */
extern "C" int vshRtcGetCurrentSecureTick(SceRtcTick *tick);

/* the SDK's glue over SceNpDrm's imports (0x81449964, 0x8144996C) */
extern "C" int sceNpDrmGetRifName(char *rifName, SceUInt64 accountId);
extern "C" int sceNpDrmGetFixedRifName(char *rifName, SceUInt64 accountId);

/* what sceNpDrmGetRifInfo returns for a license outside its period, and without an account */
#define GAME_NPDRM_ERROR_EXPIRED	-2138636282	/* 0x80870006 */
#define GAME_NPDRM_ERROR_NOT_STARTED	-2138636281	/* 0x80870007 */
#define GAME_NPDRM_ERROR_NO_ACCOUNT	-2138636280	/* 0x80870008 */

/* the end of a license without one */
#define GAME_LICENSE_NO_EXPIRY		0x7FFFFFFFFFFFFFFFLL

/* the game card's drive: its licenses are under the fixed name */
#define GAME_GAMECARD_DRIVE		"gro0:"

/* the PocketStation's license */
#define GAME_POCKETSTATION_CONTENT_ID	"JA0003-PCSC80018_00-POCKETSTATION001"

/* the length of a rif's name */
#define GAME_RIF_NAME_SIZE		48

/* 0x810349D4 */
GameRifInfo::GameRifInfo(int userData)
	: m_accountId(0), m_licenseType(GAME_RIF_LICENSE_PERMANENT), m_loaded(false), m_result(0),
	  m_userData(userData)
{
	sce_paf_memset(&m_startTime, 0, sizeof(m_startTime));
	sce_paf_memset(&m_expireTime, 0, sizeof(m_expireTime));
}

/* 0x81034A18, deleting 0x81034A26 */
GameRifInfo::~GameRifInfo()
{
}

/*
 * A license's information taken, the license padded to its full size; outside its period it
 * is taken as valid where allowPeriod is set (0x81034A3C).
 */
int GameRifInfo::GetInfo(SceNpDrmLicense *license, SceSize size, SceUInt64 *accountId,
			 SceUInt64 *startTime, SceUInt64 *expireTime, SceUInt32 *skuFlags,
			 bool allowPeriod)
{
	if (size < sizeof(*license))
		sce_paf_memset((char *)license + size, 0, sizeof(*license) - size);

	gameDebugDump("rif data", license, size);

	int drmType = 0;
	int ret = sceNpDrmGetRifInfo(license, size, 1, NULL, accountId, NULL, &drmType, NULL,
				     skuFlags, startTime, expireTime, NULL);

	if (ret >= 0)
		return 0;

	if (allowPeriod && (ret == GAME_NPDRM_ERROR_EXPIRED || ret == GAME_NPDRM_ERROR_NOT_STARTED))
		return 0;

	return ret;
}

/* The license at path read and its information taken (0x81034AEA). */
int GameRifInfo::ReadFile(const string &path, SceUInt64 *accountId, SceUInt64 *startTime,
			  SceUInt64 *expireTime, SceUInt32 *skuFlags, bool allowPeriod)
{
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0)
		return ret;

	int size = (int)file->GetFileSize();

	if (size < 0) {
		file->Close();
		return size;
	}

	SceNpDrmLicense *license = new SceNpDrmLicense;

	if (license == NULL) {
		file->Close();
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	if ((SceSize)size > sizeof(*license)) {
		file->Close();
		delete license;
		return GAME_ERROR_BAD_LICENSE_SIZE;
	}

	ret = file->Read(license, size);
	if (ret < 0) {
		file->Close();
		delete license;
		return ret;
	}

	file->Close();

	ret = GetInfo(license, size, accountId, startTime, expireTime, skuFlags, allowPeriod);
	if (ret >= 0) {
		delete license;
		return 0;
	}

	delete license;

	return ret;
}

/* The path of the license in dir under the fixed name for accountId (0x81034E7A). */
static __attribute__((noinline)) int gameRifGetFixedPath(const string &dir, SceUInt64 accountId,
							  string *path)
{
	char name[GAME_RIF_NAME_SIZE];
	int ret = sceNpDrmGetFixedRifName(name, accountId);

	if (ret < 0)
		return ret;

	*path = dir + string("/") + string(name);

	return 0;
}

/* The path of the license in dir under the account's name (0x81035084). */
static __attribute__((noinline)) int gameRifGetAccountPath(const string &dir,
							    const SceUInt64 *accountId,
							    string *path)
{
	char name[GAME_RIF_NAME_SIZE];
	int ret = sceNpDrmGetRifName(name, *accountId);

	if (ret < 0)
		return ret;

	*path = dir + string("/") + string(name);

	return 0;
}

/* the kind of license its SKU flags make */
static inline void gameRifSetLicenseType(int *licenseType, SceUInt32 skuFlags)
{
	if (skuFlags == 0)
		*licenseType = GAME_RIF_LICENSE_NONE;
	else if (skuFlags <= 1)
		*licenseType = GAME_RIF_LICENSE_PERMANENT;
	else
		*licenseType = GAME_RIF_LICENSE_TRIAL;
}

/*
 * The license in dir read: a game card's under the fixed name of the account 1, another's under
 * the account's name, else under the fixed name (0x81035282). What reading it returned is left
 * in m_result.
 */
int GameRifInfo::Load(const string &dir, const SceUInt64 *accountId, bool allowPeriod)
{
	string path;
	SceUInt32 skuFlags = 0;
	int ret;

	if (dir.find(GAME_GAMECARD_DRIVE, 0, sce_paf_strlen(GAME_GAMECARD_DRIVE)) !=
	    string::npos) {
		ret = gameRifGetFixedPath(dir, 1, &path);
		if (ret < 0)
			return ret;

		ret = ReadFile(path, &m_accountId, &m_startTime, &m_expireTime, &skuFlags,
			       allowPeriod);
		if (ret == 0) {
			gameRifSetLicenseType(&m_licenseType, skuFlags);
			goto done;
		}
	} else if (*accountId != 0) {
		ret = gameRifGetAccountPath(dir, accountId, &path);
		if (ret < 0)
			return ret;

		ret = ReadFile(path, &m_accountId, &m_startTime, &m_expireTime, &skuFlags,
			       allowPeriod);
		if (ret == 0) {
			gameRifSetLicenseType(&m_licenseType, skuFlags);
			goto done;
		}
	}

	ret = gameRifGetFixedPath(dir, 0, &path);
	if (ret < 0)
		return ret;

	ret = ReadFile(path, &m_accountId, &m_startTime, &m_expireTime, &skuFlags, allowPeriod);
	if (ret == 0)
		gameRifSetLicenseType(&m_licenseType, skuFlags);

done:
	m_result = ret;
	m_loaded = true;
	if (ret == GAME_NPDRM_ERROR_NO_ACCOUNT)
		m_loaded = false;

	return 0;
}

/* A license's content ID and period taken, the license padded to its full size (0x810354C8). */
int GameRifInfo::GetPeriod(SceNpDrmLicense *license, SceSize size, char *contentId,
			   SceUInt64 *startTime, SceUInt64 *expireTime)
{
	if (size < sizeof(*license))
		sce_paf_memset((char *)license + size, 0, sizeof(*license) - size);

	gameDebugDump("rif data", license, size);

	int ret = sceNpDrmGetRifInfo(license, size, 1, contentId, NULL, NULL, NULL, NULL, NULL,
				     startTime, expireTime, NULL);

	if (ret >= 0)
		return 0;

	return ret;
}

/*
 * The license at path read into a new buffer left in *license, padded to its full size, and
 * its size (0x81035528).
 */
__attribute__((noinline)) int gameRifRead(const string &path, SceNpDrmLicense **license, int *size)
{
	int ret = 0;

	*size = (int)LocalFile::GetFileSize(path.c_str());
	if (*size < 0)
		return *size;

	if ((SceSize)*size > sizeof(**license))
		return GAME_ERROR_BAD_LICENSE_SIZE;

	*license = new SceNpDrmLicense;
	if (*license == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	{
		common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0,
							       &ret);

		if (ret < 0 || (sce_paf_memset(*license, 0, sizeof(**license)),
				(ret = file->Read(*license, *size)) < 0)) {
			file->Close();
			delete *license;
			*license = NULL;
			return ret;
		}

		file->Close();
	}

	if ((SceSize)*size < sizeof(**license))
		sce_paf_memset((char *)*license + *size, 0, sizeof(**license) - *size);

	return 0;
}

/*
 * Whether a license is the PocketStation's and within its period; outside the period it was
 * issued for, sceNpDrmGetRifInfo's own check, it is taken as valid (0x810356EC).
 */
static __attribute__((noinline)) int gamePocketStationLicenseCheck(SceNpDrmLicense *license, SceSize size)
{
	if (size < sizeof(*license))
		sce_paf_memset((char *)license + size, 0, sizeof(*license) - size);

	char contentId[GAME_RIF_NAME_SIZE];

	sce_paf_memset(contentId, 0, sizeof(contentId));

	SceUInt64 startTime = 0;
	SceUInt64 expireTime = 0;
	int ret = sceNpDrmGetRifInfo(license, size, 1, contentId, NULL, NULL, NULL, NULL, NULL,
				     &startTime, &expireTime, NULL);

	switch (ret) {
	case 0:
		if (sce_paf_strncmp(contentId, GAME_POCKETSTATION_CONTENT_ID,
				    sizeof(contentId)) != 0)
			return GAME_ERROR_FTT_MISMATCH;

		if (expireTime != GAME_LICENSE_NO_EXPIRY) {
			SceRtcTick now;

			now.tick = 0;
			if (vshRtcGetCurrentSecureTick(&now) == 0 &&
			    (startTime > now.tick || now.tick > expireTime))
				return GAME_ERROR_LICENSE_PERIOD;
		}
		break;
	case GAME_NPDRM_ERROR_EXPIRED:
	case GAME_NPDRM_ERROR_NOT_STARTED:
		return 0;
	}

	return ret;
}

/* The PocketStation's license at rifPath checked (0x810357F4). */
int gamePspRifCheck(const string &rifPath)
{
	SceNpDrmLicense *license = new SceNpDrmLicense;

	if (license == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int size = 0;
	int ret = gameRifRead(rifPath, &license, &size);

	if (ret < 0) {
		delete license;
		return ret;
	}

	ret = gamePocketStationLicenseCheck(license, size);
	delete license;

	return ret;
}

/*
 * The path of the license in dir: a game card's under the fixed name of the account 1, another's
 * under the account's name, when it is there (its drive left in detail); else under the fixed
 * name (0x81035860).
 */
int gameRifGetPath(const string &dir, const SceUInt64 *accountId, string *path, string *detail)
{
	LocalFileStat stat;
	int ret;

	if (dir.find(GAME_GAMECARD_DRIVE, 0, sce_paf_strlen(GAME_GAMECARD_DRIVE)) !=
	    string::npos) {
		ret = gameRifGetFixedPath(dir, 1, path);
		if (ret < 0)
			return ret;

		if (LocalFile::Getstat(path->c_str(), &stat) == 0) {
			gamePathGetMountedDrive(*path, detail);
			return 0;
		}
	} else if (*accountId != 0) {
		ret = gameRifGetAccountPath(dir, accountId, path);
		if (ret < 0)
			return ret;

		if (LocalFile::Getstat(path->c_str(), &stat) == 0) {
			gamePathGetMountedDrive(*path, detail);
			return 0;
		}
	}

	ret = gameRifGetFixedPath(dir, 0, path);
	if (ret < 0)
		return ret;

	return 0;
}
