/*
 * What the game plugin's PlayStation Mobile promotions and installers read an application's
 * app.info with: the app.info made, its promotion source built, its information and content ID
 * set into the record, and its LiveArea written (0x81094CF8..0x8109535E). The file's name and
 * the functions' are ours (no record names them).
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

GamePsmAppReader::GamePsmAppReader(string *errorTitleId)
	: m_appInfo(NULL), m_errorTitleId(errorTitleId)
{
}

GamePsmAppReader::GamePsmAppReader(const string &name, string *errorTitleId)
	: m_appInfo(NULL), m_name(name), m_errorTitleId(errorTitleId)
{
}

GamePsmAppReader::~GamePsmAppReader()
{
	if (m_appInfo != NULL)
		delete m_appInfo;
}

int GamePsmAppReader::Prepare(bool flag, LsdbAppInfoPtr *info, bool *installed)
{
	int ret;

	m_appInfo = new GamePsmAppInfo;
	if (m_appInfo == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;
	if (info->get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(m_name.c_str(), info);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if ((*info)->size() == 0) {
		*installed = false;
		return 0;
	}

	*installed = true;
	ret = gameAppCheckNotRunning(m_name, info);
	if (ret < 0)
		return ret;
	return 0;
}

int GamePsmAppReader::CreateSource(const string &appInfoPath, const string &dir,
				   const string &path, GamePromoteSource **source)
{
	m_path = appInfoPath;

	GameAppSourceParam param(dir.c_str(), path.c_str(), m_name.c_str(), m_path);

	*source = new GamePsmAppInfoSource(param);
	if (*source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;
	return 0;
}

int GamePsmAppReader::Cleanup(GamePromoteSource **source)
{
	if (m_appInfo != NULL)
		delete m_appInfo;
	m_appInfo = NULL;

	if (*source != NULL) {
		delete *source;
		*source = NULL;
	}
	return 0;
}

int GamePsmAppReader::SetInfo(bool registerApp, const LsdbAppInfoPtr *info,
			      GameAppRegistration *reg)
{
	int ret = m_appInfo->Load(m_path.c_str(), info, &reg->info);

	if (ret < 0)
		return ret;
	return 0;
}

int GamePsmAppReader::ReadContentId(const string &path, LsdbAppInfoPtr *info)
{
	char contentId[48];
	int ret = gameFileRead(path, contentId, sizeof(contentId), m_errorTitleId);

	if (ret < 0)
		return ret;

	LsdbValue &value = (**info)[LSDB_KEY_CONTENT_ID];

	if (value.type != LSDB_VALUE_TEXT || value.text == NULL)
		value.text = new string(contentId);
	else
		*value.text = contentId;
	value.type = LSDB_VALUE_TEXT;
	return 0;
}

int GamePsmAppReader::Finish(GameAppRegistration *reg, const GameJobProgress *progress,
			     GameCancelFunc cancel, void *arg, float scale)
{
	GamePsmLiveAreaWriter *writer = new GamePsmLiveAreaWriter(cancel, arg, progress, NULL,
								 scale);

	if (writer == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	writer->m_appInfo = m_appInfo;

	string dir;
	int ret = writer->Run(reg, &dir);

	delete writer;
	if (ret < 0)
		return ret;
	return 0;
}
