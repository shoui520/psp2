/*
 * The game plugin's promoter of a PlayStation Mobile game: its folder in ux0:psm promoted from
 * its app.info, the record's times and content ID set, and its LiveArea written
 * (0x81069D54..0x8106A1E8). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePsmPromoter::GamePsmPromoter(const char *path, bool flag, const GameJobProgress *progress,
				 int userData, float scale)
	: GamePromoter(path, progress, userData, scale), m_reader((string *)userData), m_flag(flag)
{
	m_checkVersion = false;
}

GamePsmPromoter::~GamePsmPromoter()
{
	gameRemoveDirectory("grw0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ur0:temp/promote", 0, 0, 0, true);
	gameRemoveDirectory("ur0:appmeta/new", 0, 0, 0, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
}

/* the reader given the folder's name, and the app.info made (0x81069E54) */
int GamePsmPromoter::Prepare(bool flag)
{
	m_reader.m_name = m_name;

	int ret = m_reader.Prepare(flag, &m_info, &m_install);

	if (ret < 0)
		return ret;

	return 0;
}

/* the source of the folder, from its RO/Application/app.info (0x81069E82) */
int GamePsmPromoter::CreateSource(GamePromoteSource **source)
{
	string path = m_path + string("/RO/Application/app.info");
	int ret = m_reader.CreateSource(path, m_path, m_path, source);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The record built: the app.info's information, the install time kept from the old record, the
 * change time now, the content ID from RW/System/content_id, then the source's (0x81069FBE).
 */
int GamePsmPromoter::SetInfo(GamePromoteSource *source, bool registerApp,
			     GameAppRegistration *reg)
{
	int ret = m_reader.SetInfo(registerApp, &m_info, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_info, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	string path = m_path + string("/RW/System/content_id");

	m_reader.ReadContentId(path, &reg->info);

	ret = source->SetInfo(reg);
	if (ret < 0)
		return ret;

	return 0;
}

/* the LiveArea written, its share of the promotion's progress the last stage's (0x8106A148) */
int GamePsmPromoter::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	int ret = m_reader.Finish(reg, &m_progress, cancel, arg, 0.4f);

	if (ret < 0)
		return ret;

	return 0;
}
