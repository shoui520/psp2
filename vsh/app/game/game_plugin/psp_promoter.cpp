/*
 * The game plugin's promoter of a PSP game: its folder in ux0:pspemu/PSP/GAME promoted from its
 * EBOOT.PBP's param.sfo, an EG or ME game's EBOOT signature checked, and its appmeta step run
 * (0x8106A1E8..0x8106AC10). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
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

GamePspPromoter::GamePspPromoter(const char *path, const char *installPath, bool writeAppmeta,
				 const GameJobProgress *progress, int userData, float scale)
	: GamePromoter(path, progress, userData, scale), m_installPath(installPath),
	  m_writeAppmeta(writeAppmeta)
{
	m_checkVersion = false;
}

GamePspPromoter::~GamePspPromoter()
{
}

/* the title ID checked, and the record the game had read (0x8106A2FA) */
int GamePspPromoter::Prepare(bool flag)
{
	int ret = gameCheckTitleId(m_name.c_str());

	if (ret < 0)
		return ret;

	if (m_info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameAppGetInfo(m_name.c_str(), &m_info);
	if (ret < 0 && ret != (int)LSDB_ERROR_NOT_FOUND)
		return ret;

	if (m_info->size() == 0)
		m_install = false;
	else
		m_install = true;

	return 0;
}

/*
 * The source of the game: the parameters checked from the EBOOT.PBP's param.sfo when the game
 * has no record, an EG or ME game's EBOOT checked against its __sce_ebootpbp, and the LiveArea in
 * ur0:appmeta (0x8106A346).
 */
int GamePspPromoter::CreateSource(GamePromoteSource **source)
{
	string ebootPath = m_path + string("/EBOOT.PBP");
	bool loaded = false;

	if (m_info->size() == 0) {
		loaded = true;

		int ret = gamePbpLoadSfo(&ebootPath, gameSfoGetPspCheckParams(),
					 gameSfoGetCheckParamCount(), &m_info);

		if (ret < 0) {
			if (ret == GAME_PAF_ERROR_NOT_FOUND)
				return GAME_ERROR_GC_QUIET;

			gamePathGetMountedDrive(ebootPath, (string *)m_userData);
			return ret;
		}
	}

	LsdbAppInfo::iterator it = m_info->find(LSDB_KEY_CATEGORY);

	if (it == m_info->end())
		return GAME_ERROR_BAD_DATA;

	if (gameCategoryIsEgMe(string(LsdbValueText(it->second)))) {
		LocalFileStat stat;

		if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0) {
			string sigPath = m_path + string("/__sce_ebootpbp");

			if (gameEbootSigCheck(m_name, ebootPath, sigPath, true,
					      (string *)m_userData) < 0)
				return GAME_ERROR_EBOOT;
		}
	}

	if (loaded)
		m_info->clear();

	string dir = m_path.substr(0, m_path.length() - m_name.length() - 1);
	string path;

	if (m_installPath.length() != 0)
		path = m_installPath.substr(0, m_installPath.length() - m_name.length() - 1);
	else
		path = dir;

	m_dir = string("ur0:appmeta/") + m_name;
	m_licensePath.assign(NULL, 0);

	GameAppSourceParam param(dir.c_str(), path.c_str(), m_name.c_str(), m_dir);

	*source = new GamePspSource(param);
	if (*source == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	return 0;
}

/*
 * The record built, the install time kept from the old record, the change time now; an update of
 * an installed game whose version differs keeps the old record's kept values (0x8106AAAE).
 */
int GamePspPromoter::SetInfo(GamePromoteSource *source, bool registerApp,
			     GameAppRegistration *reg)
{
	int ret = GamePromoter::SetInfo(source, registerApp, reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_info, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	m_paramPath = ((GamePspSource *)source)->m_paramPath;

	if (!m_install)
		return 0;

	if (!registerApp)
		return 0;

	if (!gameAppVersionDiffers(&m_info, &reg->info))
		return 0;

	m_param.updateData = true;

	ret = gameAppInfoCopyKept(&m_info, &reg->info);
	if (ret < 0)
		return ret;

	return 0;
}

/* the appmeta step, when it is asked for, with the last stage's share of the progress (0x8106AB2E) */
int GamePspPromoter::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	if (m_writeAppmeta) {
		GamePspAppmetaWriter *writer =
			new GamePspAppmetaWriter(cancel, arg, &m_progress, NULL, 0.4f);

		if (writer == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		int ret = writer->Run(reg, &m_paramPath);

		if (ret < 0) {
			delete writer;
			return ret;
		}

		delete writer;
	}

	return 0;
}
