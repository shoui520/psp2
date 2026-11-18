/*
 * The game plugin's promoter of an application from its appmeta: the game card's (gro0:app) or
 * ux0:app's application registered again from its data's param.sfo, the card's gc_param.sfo kept
 * in ux0:appmeta, and the record's #_up_status and eboot parameters set (0x8106AC10..0x8106D0B0).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

GameAppmetaPromoter::GameAppmetaPromoter(const char *path, const GameJobProgress *progress,
					 int userData, float scale)
	: GamePromoter(path, progress, userData, scale), m_hasUpStatus(false), m_upStatus(0)
{
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	m_checkVersion = true;
}

GameAppmetaPromoter::~GameAppmetaPromoter()
{
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ur0:appmeta/new", 0, 0, 0, true);
	gameAppDataUnmount(&m_dataMountPoint);
	gameAppUnmount(&m_appMountPoint);
}

/*
 * The installed record read, the application's data mounted and its param.sfo checked, the card's
 * gc_param.sfo read from ux0:appmeta; an installed application is checked against the new
 * information, and a card's application whose license decides it keeps its #_up_status
 * (0x8106AD6C).
 */
int GameAppmetaPromoter::Prepare(bool flag)
{
	int ret = 0;
	LsdbAppInfoPtr installed(new LsdbAppInfo);

	if (installed.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int err = gameAppGetInfo(m_name.c_str(), &installed);

	if (err < 0 && err != (int)LSDB_ERROR_NOT_FOUND)
		return err;

	err = gameAppDataMount(m_name, &m_dataMountPoint, true);
	if (err < 0)
		return err;

	LsdbAppInfoPtr sfo(new LsdbAppInfo);

	if (sfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string path = m_dataMountPoint + string("/param.sfo");

	err = gameSfoLoad(string(path.c_str()), gameSfoGetCheckParams(),
			  gameSfoGetCheckParamCount(), &sfo);
	if (err < 0) {
		gamePathGetMountedDrive(path, (string *)m_userData);
		return err;
	}

	m_gcSfo = LsdbAppInfoPtr(new LsdbAppInfo);
	if (m_gcSfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	path = string("ux0:appmeta/") + m_name + string("/gc_param.sfo");

	GameSfoReader *reader = new GameSfoReader(&m_gcSfo);

	if (reader == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	reader->Load(path);
	delete reader;

	if (installed->size() == 0) {
		m_install = false;
	} else {
		m_install = true;

		GameUpdateChecker *checker = new GameUpdateChecker(m_path, false, true, NULL, NULL);

		if (checker == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		if (m_gcSfo.get() != NULL && m_gcSfo->size() != 0) {
			err = gameAppInfoCopyParental(&m_gcSfo, &sfo);
			if (err < 0) {
				delete checker;
				return err;
			}
		}

		err = checker->Check(&installed, &sfo);
		if (err < 0) {
			delete checker;
			return err;
		}

		delete checker;

		int storage = 0;

		gameAppGetStorage(&installed, &storage);

		string gro0Path = string("gro0:app/") + m_name;

		if (storage == 2 && !LocalFile::Exists(gro0Path.c_str())) {
			LsdbAppInfo::iterator end = installed->end();
			LsdbAppInfo::iterator it = installed->find(LSDB_KEY_CATEGORY);
			string category;

			if (it != end)
				category = LsdbValueText(it->second);

			SceUInt16 attribute = 0;

			it = installed->find(LSDB_KEY_ATTRIBUTE);
			if (it != end)
				attribute = (SceUInt16)gameLsdbValueInteger(it->second);

			if (gameCategoryIsGame(category) &&
			    (attribute & GAME_ATTRIBUTE_UP_STATUS_LICENSE)) {
				it = installed->find(LSDB_KEY_UP_STATUS);
				if (it != end) {
					m_hasUpStatus = true;
					m_upStatus = (SceUInt32)gameLsdbValueInteger(it->second);
				}
			}
		}
	}

	return ret;
}

/*
 * The source of the application: ux0:app's, its game data mounted with its license (a free
 * trial's kept without one), else the game card's, with the LiveArea in ur0:appmeta
 * (0x8106BD20).
 */
int GameAppmetaPromoter::CreateSource(GamePromoteSource **source)
{
	m_dir = string("ur0:appmeta/") + m_name;
	m_appDir = m_dir;

	string appDir = string("ux0:app/") + m_name;
	string appPath;
	string drive;

	if (Dir::IsExist(appDir.c_str())) {
		m_licensePath = string("ux0:license/app/") + m_name;
		appPath = string("ux0:app/") + m_name;
		drive = string("ux0:");

		string patchDir = string("ux0:patch/") + m_name;

		if (gameGameDataMount(appDir.c_str(), NULL, NULL, &m_appMountPoint) ==
			    GAME_LICENSE_ERROR_NOT_FOUND &&
		    gameAppHasFttLicense(m_name))
			m_param.noLicense = true;

		if (m_appMountPoint.length() != 0)
			m_licensePath = string("ux0:license/app/") + m_name;
	} else {
		appPath = string("gro0:app/") + m_name;
		drive = string("gro0:");
	}

	string device;
	int ret = gamePathGetDevice(m_path, &device);

	if (ret < 0)
		return ret;

	GameFolderSourceParam param(device.c_str(), drive.c_str(), m_name.c_str(),
				    m_dataMountPoint.c_str(), m_dir.c_str(), appPath.c_str());
	GameAppmetaSource *created = new GameAppmetaSource(param);

	if (created == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_param.noLicense)
		created->m_installed = true;

	*source = created;
	return 0;
}

/*
 * The record built, the #_up_status kept from the installed record, and an application with no
 * LiveArea template marked in its appconf (0x8106C8EC).
 */
int GameAppmetaPromoter::SetInfo(GamePromoteSource *source, bool registerApp,
				 GameAppRegistration *reg)
{
	int ret = GamePromoter::SetInfo(source, registerApp, reg);

	if (ret < 0)
		return ret;

	LsdbAppInfo::iterator end = reg->info->end();

	if (m_hasUpStatus)
		gameLsdbValueSetInteger((*reg->info)[LSDB_KEY_UP_STATUS], m_upStatus);

	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_LAREA_TYPE);

	if (it == end || (int)gameLsdbValueInteger(it->second) == 0) {
		int upStatus = 0;

		it = reg->info->find(LSDB_KEY_UP_STATUS);
		if (it != end)
			upStatus = (int)gameLsdbValueInteger(it->second);

		string path;

		if (upStatus == 3) {
			path = m_path + string("/retail/livearea/contents/template.xml");
			if (!LocalFile::Exists(path.c_str()))
				path = m_path + string("/livearea/contents/template.xml");
		} else {
			path = m_path + string("/livearea/contents/template.xml");
		}

		if (!LocalFile::Exists(path.c_str()))
			gameAppInfoSetAppconfBit12(&reg->info, true);
	}

	return 0;
}

/*
 * The card's parental level given to the record, the progress moved on, and the eboot
 * parameters of the application and of its first related one read from its data (0x8106CD54).
 */
int GameAppmetaPromoter::Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
{
	if (m_gcSfo.get() != NULL && m_gcSfo->size() != 0) {
		int ret = gameAppInfoCopyParental(&m_gcSfo, &reg->info);

		if (ret < 0)
			return ret;
	}

	float percent = gameSumPercent(1, m_weights, GAME_PROMOTE_STAGES) + 32.0f;

	if (m_progress.percent != NULL && !(percent < *m_progress.percent) && !(percent > 100.0f)) {
		*m_progress.percent = percent;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, *m_progress.percent);
		m_progress.added = 0;
	}

	string path = m_dataMountPoint + string("/ebootparam.bin");
	int ret = gameAppInfoLoadEbootParam(path, &reg->info, NULL);

	if (ret < 0) {
		gamePathGetMountedDrive(path, (string *)m_userData);
		return ret;
	}

	LocalFileStat stat;

	for (unsigned int i = 0; i < reg->related.size() && i < 1; i++) {
		char name[32];

		sce_paf_snprintf(name, sizeof(name), "/ebootparam%u.bin", i + 2);
		path = m_dataMountPoint + string(name);
		if (LocalFile::Getstat(path.c_str(), &stat) == 0)
			gameAppInfoLoadEbootParam(path, &reg->related[i], NULL);
	}

	return 0;
}
