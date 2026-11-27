/*
 * The game plugin's installer of a PSP or PlayStation game's package folder without PBOOT.PBP:
 * its record read and its game data checked not in use, its folder moved into
 * ux0:pspemu/PSP/GAME, its appmeta written, its EBOOT.PBP signed and a PlayStation game's saved
 * data installed (0x81071890..0x810729E4). The file's name and the functions' are ours (no
 * record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GamePspInstaller::GamePspInstaller(const char *path, const SceNpDrmInstallInfo *info,
				   GameJobProgress *progress, string *errorTitleId, float scale)
	: GamePackageInstaller(path, info, progress, errorTitleId, scale),
	  m_appInfo(new LsdbAppInfo)
{
	m_prepareFlag = false;
	m_weights[0] = 0.05f;
	m_weights[1] = 0.2f;
	m_weights[4] = 0.05f;
	m_weights[2] = 0.4f;
	m_weights[3] = 0.25f;
	m_weights[5] = 0.05f;
}

GamePspInstaller::~GamePspInstaller()
{
	gameRemoveDirectory("ux0:pspemu/temp/promote", 0, 0, 0, true);
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ur0:temp/promote", 0, 0, 0, true);
}

/*
 * The content ID checked when the install asks for it; the game's record read, and its game
 * data checked not in use, from the record or from the EBOOT.PBP's param.sfo; an update
 * whose game data another title shares is taken as installed (0x81071A80).
 */
int GamePspInstaller::Prepare(bool flag)
{
	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	int ret;

	if (param->checkContentId) {
		ret = gamePspCheckContentId(m_info->info.contentId);
		if (ret < 0)
			return ret;
	}

	if (m_appInfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(m_titleId.c_str(), &m_appInfo);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (m_appInfo->size() != 0) {
		ret = gamePspCheckGameDataNotRunning(m_titleId, &m_appInfo);
	} else {
		LsdbAppInfoPtr sfo(new LsdbAppInfo);

		if (sfo.get() == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		string ebootPath = string(m_path) + string("/USRDIR/CONTENT/EBOOT.PBP");

		ret = gamePbpLoadSfo(&ebootPath, gameSfoGetPspCheckParams(),
				     gameSfoGetCheckParamCount(), &sfo);
		if (ret == 0)
			ret = gamePspCheckGameDataNotRunning(m_titleId, &sfo);
		else if (ret == GAME_PAF_ERROR_NOT_FOUND)
			ret = gameAppCheckNotForeground(m_titleId);
		else
			gamePathGetMountedDrive(ebootPath, m_errorTitleId);
	}

	if (ret < 0)
		return ret;

	if (m_appInfo->size() != 0) {
		m_flag = true;
		return 0;
	}

	m_flag = false;

	if (!(m_info->info.flags & GAME_PACKAGE_FLAG_UPDATE))
		return 0;

	vector<string> titles;

	ret = gameFindTitlesByText(&titles, LSDB_KEY_GAMEDATA_ID, m_titleId.c_str());
	if (ret < 0)
		return ret;

	if (titles.size() != 0)
		m_flag = true;

	return 0;
}

GameInstallSource *GamePspInstaller::CreateSource(int *ret)
{
	*ret = 0;
	return new GamePspInstallSource(m_info, m_titleId, m_errorTitleId);
}

/*
 * The installed folder's paths, and its source made, unless the package is the PocketStation's
 * (0x81071E36).
 */
int GamePspInstaller::Installed(string *dir, string *path)
{
	if (gameIsPocketStation(string(m_info->info.contentId)))
		return 0;

	m_gameDir = *dir + string("/") + m_titleId;
	m_appDir = *dir + string("/") + m_titleId;
	m_dir = string("ur0:appmeta/") + m_titleId;

	GameAppSourceParam param(dir->c_str(), "ux0:pspemu/PSP/GAME", m_titleId.c_str(), m_dir);

	m_infoSource = new GamePspSource(param);
	if (m_infoSource == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

/* the source deleted (0x8107243E) */
int GamePspInstaller::Cleanup(void)
{
	delete m_infoSource;
	m_infoSource = NULL;
	return 0;
}

/* the record built, the install time kept from the old record, the change time now (0x81072452) */
int GamePspInstaller::SetInfo(bool registerApp, GameAppRegistration *reg)
{
	int ret = GamePackageInstaller::SetInfo(registerApp, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_appInfo, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	m_paramPath = ((GamePspSource *)m_infoSource)->m_paramPath;
	return 0;
}

/*
 * The appmeta written, with less of the progress when the saved data follows; the installed
 * EBOOT.PBP signed when the source asks for it, and a PlayStation game's saved data installed
 * (0x8107249E).
 */
int GamePspInstaller::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	const GameInstallExParam *param = (const GameInstallExParam *)m_path;
	float scale;

	if (m_info->info.contentType == GAME_CONTENT_TYPE_PS1 && param->copySavedata == true)
		scale = 0.8f;
	else
		scale = 1.0f;

	string ebootPath;

	gameAppInfoUpdateEbootParam(ebootPath, &reg->info, NULL);

	GamePspAppmetaWriter *writer =
		new GamePspAppmetaWriter(cancel, arg, &m_progress, NULL, scale * 0.25f);

	if (writer == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = writer->Run(reg, &m_paramPath);

	if (ret < 0) {
		delete writer;
		return ret;
	}

	delete writer;

#if defined(FW_TYPE_CEX)
	if (gameTypeIsSpecial(m_info->info.contentType) &&
	    ((GamePspInstallSource *)m_source)->m_ebootSig) {
		LocalFileStat stat;
		string pbpPath = m_gameDir + string("/EBOOT.PBP");

		if (LocalFile::Getstat(pbpPath.c_str(), &stat) == 0) {
			gameDebugDump("sceNpDrmInstallInfo.param_digest : hash dump",
				      m_info->param_digest, sizeof(m_info->param_digest));

			string sigPath = m_gameDir + string("/__sce_ebootpbp");

			ret = gameEbootSigGen(m_info, pbpPath, m_info->param_digest, sigPath,
					      m_errorTitleId);
			if (ret < 0)
				return ret;
		}
	}
#endif

	if (m_info->info.contentType == GAME_CONTENT_TYPE_PS1 && param->copySavedata == true) {
		GamePs1SavedataCopier *copier =
			new GamePs1SavedataCopier(cancel, arg, &m_progress, m_errorTitleId, 0.05f);

		if (copier == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = copier->Run(&m_paramPath, reg);
		if (ret < 0) {
			delete copier;
			return ret;
		}

		delete copier;

		m_progress.Set(gameSumPercent(3, m_weights, GAME_INSTALL_STAGES) + 5.0f);
	}

	return 0;
}
