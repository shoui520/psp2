/*
 * The promotion source of a PlayStation Mobile application: its record built from its name and
 * folders, and from its runtime's param.sfo when it has one (0x81096D8C..0x8109860E). The file's
 * name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePsmAppInfoSource::GamePsmAppInfoSource(const GameAppSourceParam &param)
	: GamePromoteSource(param), m_appInfoPath(param.sysDir)
{
}

GamePsmAppInfoSource::~GamePsmAppInfoSource()
{
}

/*
 * What the runtime's param.sfo gives the record: the title ID, the save data's size and folder,
 * the versions and the runtime's eboot.bin; an application whose param.sfo lacks one of them is
 * bad data (0x81096E6A).
 */
__attribute__((noinline)) int GamePsmAppInfoSource::LoadRuntime(const LsdbAppInfoPtr *sfo,
								  GameAppRegistration *reg)
{
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID_SFO], m_name);

	LsdbAppInfo::iterator it = (*sfo)->find(LSDB_KEY_SAVEDATA_MAX_SIZE);
	if (it == (*sfo)->end())
		return GAME_ERROR_BAD_DATA;
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_SAVEDATA_MAX_SIZE],
				(SceUInt32)gameLsdbValueInteger(it->second));

	it = (*sfo)->find(LSDB_KEY_DISP_VER);
	if (it == (*sfo)->end())
		return GAME_ERROR_BAD_DATA;
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_DISP_VER], LsdbValueText(it->second));

	it = (*sfo)->find(LSDB_KEY_SYSTEM_VER);
	if (it == (*sfo)->end())
		return GAME_ERROR_BAD_DATA;
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_SYSTEM_VER],
				(SceUInt32)gameLsdbValueInteger(it->second));

	string selfPath = m_path + string("/runtime") + string("/eboot.bin");

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SELF_PATH], selfPath);

	string saveData;

	it = (*sfo)->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);
	if (it == (*sfo)->end())
		saveData = m_name;
	else
		saveData = LsdbValueText(it->second);
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_INSTALL_DIR_SAVEDATA], saveData);

	it = (*sfo)->find(LSDB_KEY_APP_VER);
	if (it == (*sfo)->end())
		return GAME_ERROR_BAD_DATA;
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_PSM_RUNTIME_APP_VER], LsdbValueText(it->second));

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_CATEGORY], "mba");
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_REGION_DENY], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APPCONF], 0);
	return 0;
}

/*
 * The record of the application: its title ID, folder, app.exe, LiveArea, splash and the
 * attributes of a PlayStation Mobile application, and its entry (0x8109769E).
 */
__attribute__((noinline)) int GamePsmAppInfoSource::LoadApp(GameAppRegistration *reg)
{
	string path;
	LsdbAppInfo::iterator end = reg->info->end();

	gameLsdbValueSetText((*reg->info)[LSDB_KEY_TITLE_ID], m_name);
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_ORG_PATH], m_path);

	path = m_path + string("/RO/Application/app.exe");
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SELF_PATH], path);

	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APPTYPE], 1);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_PROMOTE_ATTR], 3);

	path = string("ur0:appmeta/") + m_name + string("/livearea/contents");
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_LIVEAREA_PATH], path);

	path = string("ur0:appmeta/") + m_name + string("/pic0.png");
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_SPLASH_PATH], path);

	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_LAREA_TYPE], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_UP_STATUS], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_EBOOT_ATTR], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_APP_MEMSIZE], 0);
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_PHY_MEMSIZE], 0);
	gameLsdbValueSetText((*reg->info)[LSDB_KEY_CATEGORY], "mb");
	gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_ATTRIBUTE], 0x209000);

	gameAppInfoGetStitle(&reg->info, &reg->entry->otherText);

	reg->entry->iconPath = string("ur0:appmeta/") + m_name + string("/icon0.dds");
	reg->entry->storage = 0;
	reg->entry->titleId = m_name;
	reg->entry->kind = 6;

	int parentalLevel = 0;
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_PARENTAL_LEVEL);

	if (it != end)
		parentalLevel = (int)gameLsdbValueInteger(it->second);
	reg->entry->parentalLevel = parentalLevel;
	return 0;
}

/* the record read: the application's, then its runtime's param.sfo when there is one (0x810982BC) */
int GamePsmAppInfoSource::Load(GameAppRegistration *reg)
{
	int ret = LoadApp(reg);

	if (ret < 0)
		return ret;

	string sfoPath = m_dir + string("/runtime") + string("/sce_sys/param.sfo");

	if (LocalFile::Exists(sfoPath.c_str())) {
		LsdbAppInfoPtr sfo(new LsdbAppInfo);

		gameSfoLoad(sfoPath, gameSfoGetSaveDataParams(), gameSfoGetSaveDataParamCount(),
			    &sfo);
		ret = LoadRuntime(&sfo, reg);
		if (ret < 0)
			return ret;
	}
	return 0;
}
