/*
 * The game plugin's installer of a PSP game's package: the package installed by its source, then
 * every game that shares its game data promoted again from its folder in ux0:pspemu/PSP/GAME and
 * its record updated; with no such game, its old appmeta moved aside and its record unregistered
 * (0x8107832C..0x8107A3B4). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
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

GamePspAppInstaller::GamePspAppInstaller(const GameInstallParam *param,
					 GameJobProgress *progress, string *errorTitleId,
					 float scale)
	: GameInstaller(param, progress, errorTitleId, scale)
{
	m_prepareFlag = false;
	m_weights[0] = 0.05f;
	m_weights[1] = 0.1f;
	m_weights[2] = 0.75f;
	m_weights[3] = 0.0f;
	m_weights[4] = 0.05f;
	m_weights[5] = 0.05f;
}

GamePspAppInstaller::~GamePspAppInstaller()
{
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
}

/*
 * One game's folder at gameDir promoted into reg as installed to installPath, with the install's
 * progress; the promoter's replace and LiveArea flags returned for its record's update
 * (0x81078442).
 */
__attribute__((noinline)) int
GamePspAppInstaller::PromoteTitle(const string *gameDir, const string *installPath,
				  GameAppRegistration *reg, bool registerApp, bool *replace,
				  bool *notifyTopmenu, GameCancelFunc cancel, void *arg,
				  float scale)
{
	GamePspPromoter *promoter = new GamePspPromoter(gameDir->c_str(), installPath->c_str(), true,
							&m_progress, (int)m_errorTitleId, scale);

	if (promoter == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

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
 * data promoted from its folder (or, with none, the title's appmeta moved aside and its record
 * unregistered), the source's install completed, then the games' records updated; a failed
 * completion ends the registrations it started (0x810784EC).
 */
int GamePspAppInstaller::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			     void *arg)
{
	int ret;

	m_titleId = m_param->titleId;

	ret = Prepare(m_prepareFlag);
	if (ret < 0)
		return ret;

	m_progress.Set(gameSumPercent(0, m_weights, GAME_INSTALL_STAGES));

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	m_source = CreateSource();
	if (m_source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string dir;
	string installPath;

	{
		string path(m_param->path);

		ret = m_source->Install(&path, &m_titleId, &dir, &installPath, &m_progress, 0, 0,
					m_weights[1]);
		if (ret < 0)
			return ret;
	}

	m_progress.Set(gameSumPercent(1, m_weights, GAME_INSTALL_STAGES));

	vector<GamePspPbootTitle> titles;

	if (m_titleIds.size() != 0) {
		float scale = m_weights[2] / m_titleIds.size();

		for (unsigned int i = 0; i < m_titleIds.size(); i++) {
			string gameDir = dir + string("/PSP/GAME/") + string(m_titleIds[i]);

			if (!Dir::IsExist(gameDir.c_str()))
				continue;

			string titlePath = string("ux0:pspemu/PSP/GAME/") + string(m_titleIds[i]);
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
	} else {
		string metaDir = string("ur0:appmeta/") + m_titleId;

		if (Dir::IsExist(metaDir.c_str())) {
			gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
			Dir::CreateRecursive("ur0:temp/appmeta", 6);

			string tempDir = string("ur0:temp/appmeta/") + m_titleId;

			ret = LocalFile::RenameFile(metaDir.c_str(), tempDir.c_str());
			if (ret < 0)
				return ret;
		}

		/* Sony's passes the string itself: its first word is its text */
		ret = gameAppUnregister((const char *const *)&m_titleId);
		if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
			return ret;
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

/* a PBOOT.PBP package: the games whose GAMEDATA_ID is the package's title ID (0x81079432) */
__attribute__((noinline)) int GamePspAppInstaller::PreparePboot(void)
{
	int ret = gameFindTitlesByText(&m_titleIds, LSDB_KEY_GAMEDATA_ID, m_param->titleId);

	if (ret < 0)
		return ret;

	if (m_titleIds.size() != 0)
		m_gameDataId = m_param->titleId;

	return 0;
}

/*
 * An EBOOT.PBP package: its game data ID read from its param.sfo, the games sharing it with the
 * package's own title, and an EG or ME game's EBOOT.PBP signature checked (0x81079474).
 */
__attribute__((noinline)) int GamePspAppInstaller::PrepareEboot(void)
{
	LsdbAppInfoPtr sfo(new LsdbAppInfo);

	if (sfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string ebootPath = string(m_param->path) + string("/PSP/GAME/") +
			   string(m_param->titleId) + string("/EBOOT.PBP");

	int ret = gamePbpLoadSfo(&ebootPath, gameSfoGetPspCheckParams(),
				 gameSfoGetCheckParamCount(), &sfo);

	if (ret < 0) {
		gamePathGetMountedDrive(ebootPath, m_errorTitleId);
		return ret;
	}

	ret = gamePspGetGameDataId(&sfo, &m_gameDataId);
	if (ret < 0)
		return ret;

	if (m_gameDataId.length() != 0) {
		ret = gameFindTitlesByText(&m_titleIds, LSDB_KEY_GAMEDATA_ID, m_gameDataId.c_str());
		if (ret < 0)
			return ret;
	}

	unsigned int i;

	for (i = 0; i < m_titleIds.size(); i++) {
		if (sce_paf_strncmp(m_titleIds[i].c_str(), m_param->titleId,
				    m_titleIds[i].length()) == 0)
			break;
	}

	if (i == m_titleIds.size())
		m_titleIds.push_back(string(m_param->titleId));

	LsdbAppInfo::iterator it = sfo->find(LSDB_KEY_CATEGORY);

	if (it == sfo->end())
		return GAME_ERROR_BAD_DATA;

	if (gameCategoryIsEgMe(string(LsdbValueText(it->second)))) {
		string sigPath = string(m_param->path) + string("/PSP/GAME/") +
				 string(m_param->titleId) + string("/__sce_ebootpbp");

		if (gameEbootSigCheck(m_titleId, ebootPath, sigPath, false, m_errorTitleId) < 0)
			return GAME_ERROR_EBOOT;
	}

	return 0;
}

/*
 * The title not the foreground application; the PSP emulator's work folder mounted while the
 * package's boot file decides how the games sharing its game data are found, and none of them
 * running (0x81079F66).
 */
int GamePspAppInstaller::Prepare(bool flag)
{
	int ret = gameAppCheckNotForeground(string(m_param->titleId));

	if (ret < 0)
		return ret;

	string workMount;

	ret = gamePspemuWorkMount(&workMount);
	if (ret < 0)
		return ret;

	string gameDir = string(m_param->path) + string("/PSP/GAME/") + string(m_param->titleId);
	string bootPath;

	ret = gamePspGetBootPath(gameDir, &bootPath);
	if (ret < 0) {
		gamePathGetMountedDrive(gameDir, m_errorTitleId);
	} else {
		if (bootPath.rfind("EBOOT.PBP", string::npos, sce_paf_strlen("EBOOT.PBP")) !=
		    string::npos)
			ret = PrepareEboot();
		else if (bootPath.rfind("PBOOT.PBP", string::npos, sce_paf_strlen("PBOOT.PBP")) !=
		    string::npos)
			ret = PreparePboot();

		if (ret < 0) {
			gamePathGetMountedDrive(bootPath, m_errorTitleId);
		} else {
			for (unsigned int i = 0; i < m_titleIds.size(); i++) {
				ret = gameAppCheckNotForeground(m_titleIds[i]);
				if (ret < 0)
					break;
			}

			if (ret >= 0)
				ret = 0;
		}
	}

	gamePspemuWorkUnmount(&workMount);
	return ret;
}

GameInstallSource *GamePspAppInstaller::CreateSource(void)
{
	return new GamePspAppInstallSource(m_param, &m_titleIds, &m_gameDataId, m_errorTitleId);
}
