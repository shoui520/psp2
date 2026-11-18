/*
 * The game plugin's installer of a PSP game's package folder with PBOOT.PBP: the package
 * installed by its source, then every game that shares its game data promoted again from its
 * folder in ux0:pspemu/PSP/GAME and its record updated (0x810729E4..0x81073D34). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePspPbootInstaller::GamePspPbootInstaller(const char *path, const SceNpDrmInstallInfo *info,
					     GameJobProgress *progress, string *errorTitleId,
					     float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale)
{
	m_prepareFlag = false;
	m_weights[0] = 0.1f;
	m_weights[1] = 0.2f;
	m_weights[2] = 0.55f;
	m_weights[3] = 0.0f;
	m_weights[4] = 0.1f;
	m_weights[5] = 0.05f;
}

GamePspPbootInstaller::~GamePspPbootInstaller()
{
}

/*
 * One game's folder at gameDir promoted into reg as installed to installPath, with the install's
 * progress; the promoter's replace and LiveArea flags returned for its record's update
 * (0x81072AEA).
 */
__attribute__((noinline)) int
GamePspPbootInstaller::PromoteTitle(const string *gameDir, const string *installPath,
				    GameAppRegistration *reg, bool registerApp, bool *replace,
				    bool *notifyTopmenu, GameCancelFunc cancel, void *arg,
				    float scale)
{
	GamePspPromoter *promoter = new GamePspPromoter(gameDir->c_str(), installPath->c_str(), true,
							&m_progress, (int)m_errorTitleId, scale);

	if (promoter == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	promoter->m_notifyTopmenu = !((const GameInstallExParam *)m_path)->imported;

	int ret = promoter->Run(reg, registerApp, cancel, arg);

	if (ret < 0) {
		delete promoter;
		return ret;
	}

	*replace = promoter->m_param.replace;
	*notifyTopmenu = promoter->m_notifyTopmenu;
	delete promoter;
	return 0;
}

/*
 * The install: the package made ready and installed by its source, each game sharing the game
 * data promoted from its folder, the source's install completed, then the games' records
 * updated; a failed completion ends the registrations it started (0x81072BA8).
 */
int GamePspPbootInstaller::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			       void *arg)
{
	int ret = 0;

	m_titleId = m_info->info.titleId;

	ret = Prepare(m_prepareFlag);
	if (ret < 0)
		return ret;

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	m_progress.Set(gameSumPercent(0, m_weights, GAME_INSTALL_STAGES));

	m_source = CreateSource(&ret);
	if (ret < 0)
		return ret;

	if (m_source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir;
	string installPath;

	{
		string path(m_path);

		ret = m_source->Install(&path, &m_titleId, &dir, &installPath, &m_progress, 0, 0,
					m_weights[1]);
		if (ret < 0) {
			if (ret != GAME_ERROR_GC_QUIET)
				return ret;

			ret = m_source->Complete(&dir, &installPath, 0, 0, 0, 1.0f);
			if (ret < 0)
				return ret;

			return 0;
		}
	}

	m_progress.Set(gameSumPercent(1, m_weights, GAME_INSTALL_STAGES));

	vector<GamePspPbootTitle> titles;
	float scale = m_weights[2] / m_titleIds.size();

	for (unsigned int i = 0; i < m_titleIds.size(); i++) {
		string gameDir = dir + string("/") + m_titleIds[i];

		if (!Dir::IsExist(gameDir.c_str()))
			continue;

		string titlePath = string("ux0:pspemu/PSP/GAME/") + m_titleIds[i];
		GameAppRegistration titleReg;

		if (reg->info.get() == NULL || reg->entry.get() == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		bool replace = false;
		bool notifyTopmenu = false;

		ret = PromoteTitle(&gameDir, &titlePath, &titleReg, registerApp, &replace,
				   &notifyTopmenu, cancel, arg, scale);

		GamePspPbootTitle title(m_titleIds[i]);

		title.info = titleReg.info;
		title.replace = replace;
		title.notifyTopmenu = notifyTopmenu;
		title.ret = ret;
		titles.push_back(title);

		if (ret < 0)
			continue;
	}

	m_progress.Set(gameSumPercent(2, m_weights, GAME_INSTALL_STAGES));

	if (m_source != NULL) {
		ret = m_source->Complete(&dir, &installPath, 0, 0, 0, 1.0f);
		if (ret < 0) {
			for (unsigned int i = 0; i < titles.size(); i++) {
				if (titles[i].ret == 0 && titles[i].notifyTopmenu)
					gameAppRegisterEnd(&titles[i].info);
			}

			return ret;
		}
	}

	m_progress.Set(gameSumPercent(4, m_weights, GAME_INSTALL_STAGES));

	for (unsigned int i = 0; i < titles.size(); i++) {
		if (titles[i].ret != 0)
			continue;

		ret = gameAppUpdate(&titles[i].info, titles[i].replace, titles[i].notifyTopmenu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * The games whose GAMEDATA_ID is titleId, with titleId itself when its folder is installed and
 * no record names it; NPJH90038's game data is also ULJS00167's, ULJS00168's and ULJS00169's
 * (0x8107381C).
 */
__attribute__((noinline)) int
GamePspPbootInstaller::FindTitles(const string &titleId, vector<string> *titleIds)
{
	int ret = gameFindTitlesByText(titleIds, LSDB_KEY_GAMEDATA_ID, titleId.c_str());

	if (ret < 0)
		return ret;

	bool found = false;

	for (unsigned int i = 0; i < titleIds->size(); i++) {
		if (titleId == (*titleIds)[i])
			found = true;
	}

	if (!found) {
		LocalFileStat stat;
		string gameDir = string("ux0:pspemu/PSP/GAME/") + titleId;

		if (Dir::IsExist(gameDir.c_str()))
			titleIds->push_back(titleId);
	}

	if (sce_paf_strncmp(titleId.c_str(), "NPJH90038", 10) == 0) {
		titleIds->push_back(string("ULJS00167"));
		titleIds->push_back(string("ULJS00168"));
		titleIds->push_back(string("ULJS00169"));
	}

	return 0;
}

/*
 * The content ID checked when the install asks for it; the games sharing the game data found,
 * and none of them running (0x81073BD4).
 */
int GamePspPbootInstaller::Prepare(bool flag)
{
	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	int ret;

	if (param->checkContentId) {
		ret = gamePspCheckContentId(m_info->info.contentId);
		if (ret < 0)
			return ret;
	}

	ret = FindTitles(string(m_info->info.titleId), &m_titleIds);
	if (ret < 0)
		return ret;

	for (unsigned int i = 0; i < m_titleIds.size(); i++) {
		ret = gameAppCheckNotForeground(m_titleIds[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

GameInstallSource *GamePspPbootInstaller::CreateSource(int *ret)
{
	*ret = 0;
	return new GamePspPbootInstallSource(m_info, m_titleId, m_titleIds, m_errorTitleId);
}

/*
 * Run calls the titles' vector's push_back, which SNC inlines here: the copy Sony's build has out
 * of line (0x8141013C)
 */
template int vector<GamePspPbootTitle>::push_back(const GamePspPbootTitle &val);
