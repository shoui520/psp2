/*
 * The game plugin's source of an application's folder: what the application promoter reads an
 * application's data folders and paths from (0x810475B2..0x81048414). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
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

GameAppSource::GameAppSource(const GameAppSourceParam &param) : GameFolderSource(param)
{
	m_appDir = param.dir;
}

GameAppSource::~GameAppSource()
{
}

/*
 * The application's save data, additional content and cache folders under its title ID, and its
 * #LIBLOCATION, ATTRIBUTE and #appconf from the record it had (0x81047612).
 */
int GameAppSource::SetDirs(GameAppRegistration *reg)
{
	gameAppInfoSetSaveDataDir(&reg->info, m_titleId);
	gameAppInfoSetAddcontDir(&reg->info, m_titleId);

	int ret = gameAppInfoSetPatchAttribute(&m_oldInfo, &reg->info);

	if (ret < 0)
		return ret;

	gameAppInfoSetCacheDir(&reg->info, m_titleId);
	gameAppInfoClearImportSaveDataInfo(&reg->info);

	return 0;
}

/*
 * The application's paths: its title ID and folder, its eboot.bin (or the one its
 * BOOT_INSTALL_DIR names), its kind, the splash image and whether it is on ux0: or has a manual
 * (0x81047650).
 */
int GameAppSource::SetPaths(GameAppRegistration *reg)
{
	LsdbAppInfo::iterator end = reg->info->end();
	string category;
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	category = LsdbValueText(it->second);
	bool isGp = category == "gp";

	if (isGp) {
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID], m_titleId.c_str());
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH], m_path.c_str());
	} else {
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID], m_titleId.c_str());
		gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH], m_path.c_str());
	}

	string bootPath = m_dir + string("/eboot.bin");
	string selfPath = m_appPath + string("/eboot.bin");

	int ret = gameAppInfoUseBootSelfPath(&reg->info, &bootPath, &selfPath);

	if (ret < 0)
		return ret;

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SELF_PATH], selfPath);

	int kind = 1;

	gameAppInfoGetGameKind(&reg->info, m_titleId, &kind);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APPTYPE], (SceUInt32)kind);

	string splashPath;
	LocalFileStat stat;

	if (gameDriveIsNotData(category)) {
		splashPath = m_metaDir + string("/pic0.png");
	} else {
		string bootDir;
		LsdbAppInfo::iterator boot = reg->info->find(LSDB_KEY_BOOT_INSTALL_DIR);

		if (boot == end)
			bootDir = m_name;
		else
			bootDir = LsdbValueText(boot->second);

		string pic = m_sysDir + string("/pic0.png");

		if (LocalFile::Getstat(pic.c_str(), &stat) == GAME_PAF_ERROR_NOT_FOUND &&
		    m_name != bootDir) {
			pic = m_metaDir;
			pic.replace(pic.rfind("/", string::npos, sce_paf_strlen("/")) + 1,
				    m_name.length(), bootDir);
			splashPath = pic + string("/sce_sys/pic0.png");
		} else {
			splashPath = m_metaDir + string("/sce_sys/pic0.png");
		}
	}

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SPLASH_PATH], splashPath);
	gameAppRegistrationSetBgTitleId(reg);

	bool onUx0 = false;

	ret = gameAppIsOnUx0(&reg->info, &onUx0);
	if (ret < 0)
		return ret;

	if (onUx0)
		m_flags |= GAME_SOURCE_FLAG_UX0;

	string manual = m_dir + string("/sce_sys/manual/001.png");

	if (LocalFile::Getstat(manual.c_str(), &stat) == 0)
		m_flags |= GAME_SOURCE_FLAG_MANUAL;

	bool installed = m_installed;

	it = reg->info->find(LSDB_KEY_ATTRIBUTE);
	if (it != end && (gameLsdbValueInteger(it->second) & GAME_ATTRIBUTE_MANUAL) != 0)
		m_flags |= GAME_SOURCE_FLAG_MANUAL;

	if (installed)
		m_flags |= GAME_SOURCE_FLAG_INSTALLED;

	return 0;
}
