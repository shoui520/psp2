/*
 * The game plugin's source of an application promoted from its appmeta folder: the
 * application's own folder looked for under it, its paths and its splash from its appmeta, and
 * whether it has a manual in its app or patch folder (0x8107BEF0..0x8107DA08). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the folder with the application's pic0.png; whether its app folder is there (0x8107BEF0) */
__attribute__((noinline)) void GameAppmetaSource::Init(const GameFolderSourceParam &param)
{
	m_dataDir = param.sysDir;

	LocalFileStat stat;

	m_dir.append(string("app/") + m_titleId);
	if (LocalFile::Getstat(m_dir.c_str(), &stat) == 0)
		m_hasAppDir = true;

	m_path = param.path;
}

GameAppmetaSource::GameAppmetaSource(const GameFolderSourceParam &param)
	: GameFolderSource(param), m_hasAppDir(false), m_reserved(false)
{
	Init(param);
}

GameAppmetaSource::~GameAppmetaSource()
{
}

/*
 * The application's save data, additional content and cache folders under its title ID and its
 * patch's attributes; an application on ux0: whose app folder is not there has that marked in
 * its ATTRIBUTE (0x8107C192).
 */
int GameAppmetaSource::SetDirs(GameAppRegistration *reg)
{
	gameAppInfoSetSaveDataDir(&reg->info, m_titleId);
	gameAppInfoSetAddcontDir(&reg->info, m_titleId);

	int ret = gameAppInfoSetPatchAttribute(&m_oldInfo, &reg->info);

	if (ret < 0)
		return ret;

	gameAppInfoSetCacheDir(&reg->info, m_titleId);
	gameAppInfoClearImportSaveDataInfo(&reg->info);

	if (m_dir.find("ux0:", 0, sce_paf_strlen("ux0:")) != string::npos) {
		LsdbAppInfo::iterator end = reg->info->end();
		LocalFileStat stat;
		SceUInt64 attribute = 0;
		LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_ATTRIBUTE);

		if (it != end)
			attribute = gameLsdbValueInteger(it->second);

		if (LocalFile::Getstat(m_dir.c_str(), &stat) == GAME_PAF_ERROR_NOT_FOUND)
			attribute |= GAME_ATTRIBUTE_NO_APP_DIR;

		gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_ATTRIBUTE], attribute);
	}

	return 0;
}

/*
 * The application's paths: its title ID, its original folder (its app/<title ID> under the path
 * when the path does not name it), its eboot.bin, up status and kind, the splash from its
 * appmeta when there is one; whether it is on ux0:, has a manual in its app or patch folder, and
 * was installed (0x8107C38C).
 */
int GameAppmetaSource::SetPaths(GameAppRegistration *reg)
{
	LocalFileStat stat;

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID], m_titleId);

	if (m_path.find(m_titleId) == string::npos)
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH],
				     m_path + string("app/") + m_titleId);
	else
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH], m_path);

	string selfPath = m_appPath + string("/eboot.bin");

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SELF_PATH], selfPath);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_UP_STATUS], m_upStatus);

	int kind = 1;

	gameAppInfoGetGameKind(&reg->info, m_titleId, &kind);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APPTYPE], (SceUInt32)kind);

	string splashPath = m_dataDir + string("/pic0.png");

	if (LocalFile::Getstat(splashPath.c_str(), &stat) == 0) {
		splashPath = m_metaDir + string("/pic0.png");
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_SPLASH_PATH], splashPath);
	}

	gameAppRegistrationSetBgTitleId(reg);

	bool onUx0 = false;
	int ret = gameAppIsOnUx0(&reg->info, &onUx0);

	if (ret < 0)
		return ret;

	if (onUx0)
		m_flags |= GAME_SOURCE_FLAG_UX0;

	int storage;

	ret = gameAppGetStorage(&reg->info, &storage);
	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = reg->info->end();

	if (storage == GAME_STORAGE_GRO0 || storage == GAME_STORAGE_GRO0_OTHER) {
		LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_ORG_PATH);

		if (it != end && LocalFile::Getstat(LsdbValueText(it->second), &stat) == 0) {
			string manual = string(LsdbValueText(it->second)) +
					string("/sce_sys/manual/001.png");

			if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
				m_flags |= GAME_SOURCE_FLAG_MANUAL;

			string dir(m_dir);
			string patchDir(dir.replace(dir.find("app/", 0, sce_paf_strlen("app/")), 4, "patch/", 0, 6));

			manual = patchDir + string("/sce_sys/manual/001.png");
			if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
				m_flags |= GAME_SOURCE_FLAG_MANUAL;

			it = reg->info->find(LSDB_KEY_ATTRIBUTE);
			if (it != end && (gameLsdbValueInteger(it->second) & GAME_ATTRIBUTE_MANUAL) != 0)
				m_flags |= GAME_SOURCE_FLAG_MANUAL;
		}
	} else if (storage == GAME_STORAGE_UX0) {
		string manual = m_dir + string("/sce_sys/manual/001.png");

		if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
			m_flags |= GAME_SOURCE_FLAG_MANUAL;

		string dir(m_dir);
		string patchDir(dir.replace(dir.find("app/", 0, sce_paf_strlen("app/")), 4, "patch/", 0, 6));

		manual = patchDir + string("/sce_sys/manual/001.png");
		if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
			m_flags |= GAME_SOURCE_FLAG_MANUAL;

		LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_ATTRIBUTE);

		if (it != end && (gameLsdbValueInteger(it->second) & GAME_ATTRIBUTE_MANUAL) != 0)
			m_flags |= GAME_SOURCE_FLAG_MANUAL;
	}

	if (m_installed)
		m_flags |= GAME_SOURCE_FLAG_INSTALLED;

	return 0;
}

/*
 * A related application's title ID, its original folder as the application's, its eboot<n>.bin,
 * kind and promotion state (0x8107D35C).
 */
int GameAppmetaSource::SetRelatedPaths(unsigned int index, LsdbAppInfoPtr *related)
{
	LsdbAppInfo::iterator it = (*related)->find(LSDB_KEY_TITLE_ID_SFO);

	if (it == (*related)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));

	gameLsdbValueSetText((**related)[LSDB_KEY_TITLE_ID], titleId.c_str());

	if (m_path.find(m_titleId) == string::npos)
		gameLsdbValueSetText((**related)[LSDB_KEY_ORG_PATH],
				     m_path + string("app/") + m_titleId);
	else
		gameLsdbValueSetText((**related)[LSDB_KEY_ORG_PATH], m_path);

	char name[32];

	sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", index);

	string selfPath = m_appPath + string(name);

	gameLsdbValueSetText((**related)[LSDB_KEY_SELF_PATH], selfPath);

	int kind = 1;

	gameAppInfoGetGameKind(related, titleId, &kind);
	gameLsdbValueSetInteger((**related)[LSDB_KEY_APPTYPE], (SceUInt32)kind);

	bool onUx0 = false;
	int ret = gameAppIsOnUx0(related, &onUx0);

	if (ret < 0)
		return ret;

	ret = gameAppInfoSetPromoteAttr(onUx0 ? GAME_SOURCE_FLAG_UX0 : 0, related);
	if (ret < 0)
		return ret;

	return 0;
}
