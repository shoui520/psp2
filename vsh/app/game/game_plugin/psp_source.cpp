/*
 * The game plugin's source of a PSP application's folder: its PBP's param.sfo read and checked,
 * the newest of its updates chosen, and its record filled from them (0x81048414..0x8104B81C).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

void GamePspSource::Init(const GameAppSourceParam &param)
{
	m_baseDir = m_dir;
	m_basePath = m_path;
	m_dir.append(string("/") + m_titleId);
	m_path.append(string("/") + m_titleId);
	m_sysDir = param.sysDir;
	m_drive = string("ux0:");
	m_flags |= GAME_SOURCE_FLAG_UX0;

	string document = m_dir + string("/DOCUMENT.DAT");
	LocalFileStat stat;

	if (LocalFile::Getstat(document.c_str(), &stat) == 0)
		m_flags |= GAME_SOURCE_FLAG_MANUAL;
}

GamePspSource::GamePspSource(const GameAppSourceParam &param)
	: GamePromoteSource(param), m_reserved(0), m_sfo(new LsdbAppInfo)
{
	Init(param);
}

GamePspSource::~GamePspSource()
{
}

int gameSfoCheck(const LsdbAppInfoPtr *sfo)
{
	LsdbAppInfo::iterator end = (*sfo)->end();
	string category("");
	LsdbAppInfo::iterator it = (*sfo)->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_BAD_DATA;

	category = LsdbValueText(it->second);
	if (category.length() == 0)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_PARENTAL_LEVEL);
	if (it != end && (SceUInt32)gameLsdbValueInteger(it->second) > 11)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_DISC_ID);
	if (it != end) {
		const char *discId = LsdbValueText(it->second);

		if (discId == NULL || discId[0] == '\0')
			return GAME_ERROR_BAD_DATA;
	}

	it = (*sfo)->find(LSDB_KEY_DISC_TOTAL);
	if (it != end) {
		SceUInt32 total = (SceUInt32)gameLsdbValueInteger(it->second);

		if (total < 1 || total > 99)
			return GAME_ERROR_BAD_DATA;
	}

	it = (*sfo)->find(LSDB_KEY_DISC_NUMBER);
	if (it != end) {
		SceUInt32 number = (SceUInt32)gameLsdbValueInteger(it->second);

		if (number < 1 || number > 99)
			return GAME_ERROR_BAD_DATA;
	}

	it = (*sfo)->find(LSDB_KEY_BOOTABLE);
	if (it == end) {
		if (category == "UG" || category == "ME" || category == "EG" || category == "MG" ||
		    category == "PG" || category == "WG" || category == "SG")
			return GAME_ERROR_BAD_DATA;
	} else if ((SceUInt32)gameLsdbValueInteger(it->second) > 2) {
		return GAME_ERROR_BAD_DATA;
	}

	it = (*sfo)->find(LSDB_KEY_PSP_SYSTEM_VER);
	if (it == end) {
		if (category == "EG")
			return GAME_ERROR_BAD_DATA;
	} else if (gameVersionCheckShort(LsdbValueText(it->second)) < 0) {
		return GAME_ERROR_BAD_DATA;
	}

	it = (*sfo)->find(LSDB_KEY_UPDATER_VER);
	if (it != end && gameVersionCheckShort(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_USE_USB);
	if (it != end && (SceUInt32)gameLsdbValueInteger(it->second) > 1)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_TARGET_APP_VER);
	if (it != end && gameVersionCheck(LsdbValueText(it->second)) < 0)
		return GAME_ERROR_BAD_DATA;

	it = (*sfo)->find(LSDB_KEY_GAMEDATA_ID);
	if (it != end) {
		int ret = gameCheckTitleId(LsdbValueText(it->second));

		if (ret < 0)
			return ret;
	}

	return 0;
}

int gamePbpLoadSfo(const string *path, const GameSfoParam *params, unsigned int count,
		   LsdbAppInfoPtr *info)
{
	void *buf = NULL;
	SfoHandle *handle = NULL;
	GamePbpHeader header;
	int ret = gamePbpReadHeader(path->c_str(), &header, NULL);

	if (ret >= 0) {
		SceSize size = header.icon0Offset - header.paramSfoOffset;

		ret = gameFileReadAlloc(path->c_str(), header.paramSfoOffset, &buf, size);
		if (ret >= 0) {
			ret = gameSfoLoadParams(buf, size, params, count, info, true);
			if (ret >= 0)
				ret = 0;
		}
	}

	sfoClose(handle);

	if (buf != NULL)
		sce_paf_free(buf);

	return ret;
}

int gamePbpReadSfo(const string *path, LsdbAppInfoPtr *sfo)
{
	string category;
	void *buf = NULL;
	GamePbpHeader header;
	int ret = gamePbpReadHeader(path->c_str(), &header, NULL);

	if (ret < 0)
		goto out;

	{
		SceSize size = header.icon0Offset - header.paramSfoOffset;

		ret = gameFileReadAlloc(path->c_str(), header.paramSfoOffset, &buf, size);
		if (ret < 0)
			goto out;

		ret = gameSfoLoadParams(buf, size, gameSfoGetPspCheckParams(),
					gameSfoGetPspCheckParamCount(), sfo, true);
		if (ret < 0)
			goto out;

		LsdbAppInfo::iterator end = (*sfo)->end();
		LsdbAppInfo::iterator it = (*sfo)->find(LSDB_KEY_CATEGORY);

		if (it == end) {
			ret = GAME_ERROR_BAD_DATA;
			goto out;
		}

		category = LsdbValueText(it->second);

		const GameSfoParam *params = NULL;
		unsigned int count;

		if ((*sfo)->find(LSDB_KEY_UPDATER_VER) == end) {
			if (category == "MG") {
				it = (*sfo)->find(LSDB_KEY_BOOTABLE);
				if (it == end) {
					params = gameSfoGetPspGameParams();
					count = gameSfoGetPspGameParamCount();
				} else {
					if (gameLsdbValueInteger(it->second) != 0)
						params = gameSfoGetPspGameParams();
					else
						params = gameSfoGetPspGameBootParams();
					count = gameSfoGetPspGameParamCount();
				}
			} else if (category == "MA") {
				params = gameSfoGetPspAppParams();
				count = gameSfoGetPspAppParamCount();
			} else if (category == "ME") {
				params = gameSfoGetPspPs1Params();
				count = gameSfoGetPspPs1ParamCount();
			} else if (category == "EG") {
				params = gameSfoGetPspDiscGameParams();
				count = gameSfoGetPspDiscGameParamCount();
			} else if (category == "PG") {
				params = gameSfoGetPspPatchParams();
				count = gameSfoGetPspPatchParamCount();
			} else if (category == "WG") {
				params = gameSfoGetPspWgParams();
				count = gameSfoGetPspWgParamCount();
			}
		} else {
			if (category != "MG") {
				ret = GAME_ERROR_BAD_DATA;
				goto out;
			}

			params = gameSfoGetPspUpdaterParams();
			count = gameSfoGetPspUpdaterParamCount();
		}

		if (params == NULL) {
			ret = GAME_ERROR_NOT_APP_PACKAGE;
			goto out;
		}

		ret = gameSfoLoadParams(buf, size, params, count, sfo, true);
		if (ret >= 0)
			ret = 0;
	}

out:
	if (buf != NULL) {
		sce_paf_free(buf);
		buf = NULL;
	}

	return ret;
}

int GamePspSource::SetPaths(const string *pbpPath, GameAppRegistration *reg)
{
	LsdbAppInfo::iterator end = reg->info->end();

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID], m_titleId.c_str());
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH], m_path.c_str());
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SELF_PATH], m_selfPath.c_str());
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_LAREA_TYPE], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APPTYPE], 1);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APP_MEMSIZE], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_PHY_MEMSIZE], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_EBOOT_ATTR], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_UP_STATUS], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_LIBLOCATION], 1);

	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_GAMEDATA_ID);

	if (it != end && sce_paf_strncmp(LsdbValueText(it->second), m_titleId.c_str(),
					 m_titleId.length()) == 0)
		reg->info->erase(it);

	return 0;
}

int GamePspSource::FixGamedataId(LsdbAppInfoPtr *info, const string *titleId)
{
	if (sce_paf_strncmp(titleId->c_str(), "ULJS00167", 10) == 0 ||
	    sce_paf_strncmp(titleId->c_str(), "ULJS00168", 10) == 0 ||
	    sce_paf_strncmp(titleId->c_str(), "ULJS00169", 10) == 0)
		gameLsdbValueSetText((**info)[LSDB_KEY_GAMEDATA_ID], "NPJH90038");

	return 0;
}

int GamePspSource::SetTitle(GameAppRegistration *reg)
{
	LsdbAppInfo::iterator end = reg->info->end();
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_TITLE);
	int ret;

	if (it != end) {
		string title;

		{
			string utf8(LsdbValueText(it->second));
			int length = gameUtf8Length(utf8.c_str());
			SceWChar16 *utf16 = new SceWChar16[length + 1];

			if (utf16 != NULL) {
				ret = gameUtf8ToUtf16(utf16, length * sizeof(SceWChar16) +
						      sizeof(SceWChar16), utf8.c_str());
				if (ret >= 0) {
					utf16[length] = 0;
					ret = common::Utf16ToUtf8((const wchar_t *)utf16, &title);
					delete[] utf16;
					if (ret >= 0)
						ret = 0;
				} else {
					delete[] utf16;
				}
			} else {
				ret = GAME_ERROR_NO_INFO_MEMORY;
			}
		}

		if (ret < 0)
			return ret;

		gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE], title);
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_STITLE], title);
	}

	it = reg->info->find(LSDB_KEY_PSP_SYSTEM_VER);
	if (it != end) {
		SceUInt32 version = 0;

		ret = gameVersionToNumber(LsdbValueText(it->second), 5, &version);
		if (ret < 0)
			return ret;

		gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_SYSTEM_VER], version);
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_DISP_VER], LsdbValueText(it->second));
		reg->info->erase(it);
	}

	ret = FixGamedataId(&reg->info, &m_titleId);
	if (ret < 0)
		return ret;

	if (reg->info->find(LSDB_KEY_APP_VER) == end)
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_APP_VER], "00.00");

	return 0;
}

int GamePspSource::CopyInfo(const LsdbAppInfoPtr *src, LsdbAppInfoPtr *dst)
{
	LsdbAppInfo::iterator end = (*src)->end();

	for (LsdbAppInfo::iterator it = (*src)->begin(); it != end; ++it) {
		if (it->first == LSDB_KEY_BOOTABLE || it->first == LSDB_KEY_CATEGORY)
			continue;

		if (it->second.type == LSDB_VALUE_INTEGER)
			gameLsdbValueSetInteger((**dst)[it->first], gameLsdbValueInteger(it->second));
		else
			gameLsdbValueSetText((**dst)[it->first], LsdbValueText(it->second));
	}

	return 0;
}

/* an update's APP_VER as a number, 0 without one */
static inline void gameSfoGetAppVer(const LsdbAppInfoPtr &sfo, int *version)
{
	LsdbAppInfo::iterator end = sfo->end();
	LsdbAppInfo::iterator it = sfo->find(LSDB_KEY_APP_VER);

	if (it == end)
		*version = 0;
	else if (gameVersionToNumber(LsdbValueText(it->second), 6, (SceUInt32 *)version) < 0)
		*version = 0;
}

int GamePspSource::LoadSfo(const string &titleId, string *pbpPath, LsdbAppInfoPtr *info,
			   int unused1, int unused2)
{
	LocalFileStat stat;

	if (LocalFile::Getstat(pbpPath->c_str(), &stat) < 0)
		return GAME_ERROR_GC_QUIET;

	int appVer = -1;
	LsdbAppInfo::iterator end = (*info)->end();
	SceInt64 attribute = 0;
	int ret = gamePbpReadSfo(pbpPath, info);

	if (ret < 0) {
		gamePathGetMountedDrive(*pbpPath, m_detail);
		return ret;
	}

	ret = gameSfoCheck(info);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator it = (*info)->find(LsdbKeyHash("ATTRIBUTE"));

	if (it != end)
		attribute = gameLsdbValueInteger(it->second);

	it = (*info)->find(LsdbKeyHash("APP_VER"));
	if (it == end)
		appVer = 0;
	else if (gameVersionToNumber(LsdbValueText(it->second), 6, (SceUInt32 *)&appVer) < 0)
		appVer = 0;

	string gameDataPath;
	string gameDataId;

	ret = gamePspGetGameDataId(info, &gameDataId);
	if (ret < 0)
		return ret;

	if (gameDataId.length() != 0) {
		gameDataPath = m_baseDir + "/" + gameDataId + string("/PBOOT.PBP");
		if (LocalFile::Getstat(gameDataPath.c_str(), &stat) < 0)
			gameDataPath.clear();

		if (gameDataPath.length() != 0 && gameDataId == "NPJH90038") {
			m_paramPath = *pbpPath;
			*pbpPath = gameDataPath;
			return 0;
		}
	}

	string pbootPath;
	string paramPath;

	pbootPath = m_baseDir + string("/") + titleId + string("/PBOOT.PBP");
	if (LocalFile::Getstat(pbootPath.c_str(), &stat) < 0)
		pbootPath.clear();

	paramPath = m_baseDir + string("/") + titleId + string("/PARAM.PBP");
	if (LocalFile::Getstat(paramPath.c_str(), &stat) < 0)
		paramPath.clear();

	LsdbAppInfoPtr pbootSfo(new LsdbAppInfo);
	LsdbAppInfoPtr gameDataSfo(new LsdbAppInfo);
	LsdbAppInfoPtr paramSfo(new LsdbAppInfo);
	int pbootVer = -1;
	int gameDataVer = -1;

	if (pbootPath.length() != 0 && gamePbpReadSfo(&pbootPath, &pbootSfo) == 0) {
		ret = gameSfoCheck(info);
		if (ret < 0)
			return ret;

		gameSfoGetAppVer(pbootSfo, &pbootVer);
	}

	if (gameDataPath.length() != 0 && gamePbpReadSfo(&gameDataPath, &gameDataSfo) == 0) {
		ret = gameSfoCheck(&gameDataSfo);
		if (ret < 0)
			return ret;

		gameSfoGetAppVer(gameDataSfo, &gameDataVer);
	}

	if (paramPath.length() != 0) {
		ret = gamePbpReadSfo(&paramPath, &paramSfo);
		if (ret < 0) {
			gamePathGetMountedDrive(paramPath, m_detail);
			return ret;
		}

		ret = gameSfoCheck(&paramSfo);
		if (ret < 0)
			return ret;
	}

	LsdbAppInfo none;

	if (gameDataVer < pbootVer || appVer > gameDataVer) {
		if (gameDataVer < pbootVer && appVer <= pbootVer) {
			gameDataPath.clear();
			gameDataSfo->clear();
			*pbpPath = pbootPath;
			pbootSfo = pbootSfo;
		}
	} else {
		pbootPath.clear();
		pbootSfo->clear();
		*pbpPath = gameDataPath;
		pbootSfo = gameDataSfo;
	}

	string sfoPath(*pbpPath);

	if ((attribute & 1) != 0) {
		if (paramPath.length() != 0) {
			sfoPath = paramPath;

			if (pbootSfo->size() != 0) {
				ret = CopyInfo(&pbootSfo, info);
				if (ret < 0)
					return ret;
			}

			ret = CopyInfo(&paramSfo, info);
			if (ret < 0)
				return ret;
		} else if (pbootSfo->size() != 0) {
			ret = CopyInfo(&pbootSfo, info);
			if (ret < 0)
				return ret;
		}
	} else if (pbootSfo->size() != 0) {
		ret = CopyInfo(&pbootSfo, info);
		if (ret < 0)
			return ret;
	}

	m_paramPath = sfoPath;

	return 0;
}

int GamePspSource::Load(GameAppRegistration *reg)
{
	string pbpPath = m_dir + string("/EBOOT.PBP");
	int ret = LoadSfo(m_titleId, &pbpPath, &reg->info, -1, -1);

	if (ret < 0)
		return ret;

	string sub = pbpPath.substr(m_baseDir.length(), pbpPath.length() - m_baseDir.length());

	m_selfPath = m_basePath + sub;
	SetTitle(reg);
	SetPaths(&pbpPath, reg);

	ret = gameAppInfoSetPromoteAttr(m_flags, &reg->info);
	if (ret < 0)
		return ret;

	return 0;
}
