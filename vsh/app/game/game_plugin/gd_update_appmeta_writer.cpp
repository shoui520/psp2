/*
 * The game plugin's appmeta of a game's update package: for additional content (gp, gpc), its
 * parameters, its EBOOTs' parameters and its images made as "new" files in its application data
 * and renamed over the old ones once all are there (0x8107DF50..0x8107FBF0). The file's name and
 * the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the steps of the writer's progress: its checks, its copy and its commit */
#define GAME_GD_APPMETA_STEPS	3

/* the images and the parameters made as "new" files, each with the name it is renamed to */
static const char *s_newImageNames[][2] = {
	{ "/new_icon0.png", "/icon0.png" },
	{ "/new_pic0.png", "/pic0.png" },
};

static const char *s_newParamNames[][2] = {
	{ "/new_param.sfo", "/param.sfo" },
	{ "/new_ebootparam.bin", "/ebootparam.bin" },
};

GameGdUpdateAppmetaWriter::GameGdUpdateAppmetaWriter(GameCancelFunc cancel, void *arg,
						     const GameJobProgress *progress,
						     string *detail, float scale)
	: m_cancel(cancel), m_arg(arg), m_progress(*progress, scale), m_detail(detail)
{
	m_weights[0] = GAME_APPMETA_WEIGHT_CHECK;
	m_weights[1] = GAME_APPMETA_WEIGHT_COPY;
	m_weights[2] = GAME_APPMETA_WEIGHT_COMMIT;
}

GameGdUpdateAppmetaWriter::~GameGdUpdateAppmetaWriter()
{
	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);
}

/*
 * Each of the count "new" files in dir renamed to its name, over the old file when there is one
 * (0x8107E03E).
 */
static int gameAppmetaRenameNew(const string *dir, const char *names[][2], unsigned int count)
{
	LocalFileStat stat;
	string from;
	string to;

	for (unsigned int i = 0; i < count; i++) {
		from = *dir + string(names[i][0]);
		to = *dir + string(names[i][1]);

		int fromRet = LocalFile::Getstat(from.c_str(), &stat);
		int toRet = LocalFile::Getstat(to.c_str(), &stat);

		if ((fromRet | toRet) == 0 || (fromRet == 0 && toRet < 0)) {
			LocalFile::RemoveFile(to.c_str());

			int ret = LocalFile::RenameFile(from.c_str(), to.c_str());

			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

/*
 * The "new" images in dir renamed over the old ones, and with params the "new" parameters and
 * EBOOT parameters, the related application's too (0x8107E2F4).
 */
static int gameAppmetaCommitFiles(const string *dir, bool params)
{
	int ret = gameAppmetaRenameNew(dir, s_newImageNames, 2);

	if (ret < 0)
		return ret;

	if (!params)
		return 0;

	ret = gameAppmetaRenameNew(dir, s_newParamNames, 2);
	if (ret < 0)
		return ret;

	LocalFileStat stat;
	string from;
	string to;

	for (unsigned int i = 0; i < 1; i++) {
		char name[32];

		sce_paf_snprintf(name, sizeof(name), "/new_param%u.sfo", i + 2);
		from = *dir + string(name);
		sce_paf_snprintf(name, sizeof(name), "/param%u.sfo", i + 2);
		to = *dir + string(name);

		int fromRet = LocalFile::Getstat(from.c_str(), &stat);
		int toRet = LocalFile::Getstat(to.c_str(), &stat);

		if ((fromRet | toRet) == 0 || (fromRet == 0 && toRet < 0)) {
			LocalFile::RemoveFile(to.c_str());
			ret = LocalFile::RenameFile(from.c_str(), to.c_str());
			if (ret < 0)
				return ret;

			sce_paf_snprintf(name, sizeof(name), "/new_ebootparam%u.bin", i + 2);
			from = *dir + string(name);
			sce_paf_snprintf(name, sizeof(name), "/ebootparam%u.bin", i + 2);
			to = *dir + string(name);

			fromRet = LocalFile::Getstat(from.c_str(), &stat);
			toRet = LocalFile::Getstat(to.c_str(), &stat);

			if ((fromRet | toRet) == 0 || (fromRet == 0 && toRet < 0)) {
				LocalFile::RemoveFile(to.c_str());
				ret = LocalFile::RenameFile(from.c_str(), to.c_str());
				if (ret < 0)
					return ret;
			}
		}
	}

	return 0;
}

/*
 * The application's param.sfo files (with params) and its images copied from src into dst as
 * "new" files, the copy stopped when cancel says so (0x8107E7E6).
 */
static int gameAppmetaCopyNew(const string *src, const string *dst, bool params,
			      GameCancelFunc cancel, void *arg, string *detail)
{
	string from;
	string to;
	int ret;

	if (params) {
		from = *src + string("/sce_sys/param.sfo");
		to = *dst + string("/new_param.sfo");

		LocalFileStat stat;

		ret = LocalFile::Getstat(from.c_str(), &stat);
		if (ret < 0)
			return ret;

		if (stat.st_size > GAME_PARAM_SFO_MAX_SIZE)
			return GAME_ERROR_BAD_PARAM_SIZE;

		LocalFile::RemoveFile(to.c_str());

		ret = gameCopyFileWithPool(from.c_str(), to.c_str(), (int)cancel, (int)arg, 0,
					   (int)detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
			return ret;

		for (unsigned int i = 0; i < 1; i++) {
			char name[32];

			sce_paf_snprintf(name, sizeof(name), "/sce_sys/param%u.sfo", i + 2);
			from = *src + string(name);

			if (LocalFile::Getstat(from.c_str(), &stat) != 0)
				continue;

			if (stat.st_size > GAME_PARAM_SFO_MAX_SIZE)
				return GAME_ERROR_BAD_PARAM_SIZE;

			sce_paf_snprintf(name, sizeof(name), "/new_param%u.sfo", i + 2);
			to = *dst + string(name);

			ret = gameCopyFileWithPool(from.c_str(), to.c_str(), 0, 0, 0, (int)detail);
			if (ret < 0)
				return ret;
		}
	}

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	from = *src + string("/sce_sys/icon0.png");
	to = *dst + string("/new_icon0.png");
	LocalFile::RemoveFile(to.c_str());

	ret = gameImageConvert(&from, &to, gameIcon0Format(), detail);
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
		return ret;

	from = *src + string("/sce_sys/pic0.png");
	to = *dst + string("/new_pic0.png");
	LocalFile::RemoveFile(to.c_str());

	ret = gameImageConvert(&from, &to, gamePic0Format(), detail);
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
		return ret;

	return 0;
}

/*
 * The appmeta of the application in dir made in its application data when onUx0: its files
 * copied as "new" ones, its EBOOTs' parameters written, then all renamed over the old files
 * (0x8107F13A).
 */
__attribute__((noinline)) int
GameGdUpdateAppmetaWriter::CopyToNew(const string *dir, const string *titleId, unsigned int kind,
				     bool gcParam, bool onUx0)
{
	int ret = gameAppmetaCheckSpace(onUx0, GAME_APPMETA_SIZE, m_detail);

	if (ret < 0)
		return ret;

	gameRemoveDirectory("ux0:appmeta/new", 0, 0, 0, true);

	if (onUx0) {
		string mountPoint;

		ret = gameAppDataMount(string(titleId->c_str()), &mountPoint, false);
		if (ret < 0)
			return ret;

		ret = gameAppmetaCopyNew(dir, &mountPoint, true, m_cancel, m_arg, NULL);
		if (ret < 0)
			return ret;

		if (m_cancel != NULL && m_cancel(m_arg) != 0)
			return GAME_ERROR_ABORTED;

		string ebootPath = *dir + string("/eboot.bin");
		string paramPath = mountPoint + string("/new_ebootparam.bin");

		ret = gameAppmetaWriteEbootParam(&ebootPath, &paramPath, m_detail);
		if (ret < 0)
			return ret;

		LocalFileStat stat;

		for (unsigned int i = 0; i < 1; i++) {
			char name[32];

			sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
			ebootPath = *dir + string(name);

			if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0) {
				sce_paf_snprintf(name, sizeof(name), "/new_ebootparam%u.bin", i + 2);
				paramPath = mountPoint + string(name);

				ret = gameAppmetaWriteEbootParam(&ebootPath, &paramPath, m_detail);
				if (ret < 0)
					return ret;
			}
		}

		if (m_cancel != NULL && m_cancel(m_arg) != 0) {
			gameAppDataUnmount(&mountPoint);
			return GAME_ERROR_ABORTED;
		}

		m_progress.Set(gameSumPercent(0, m_weights, GAME_GD_APPMETA_STEPS));

		ret = gameAppmetaCommitFiles(&mountPoint, true);
		if (ret < 0) {
			gameAppDataUnmount(&mountPoint);
			return ret;
		}

		gameAppDataUnmount(&mountPoint);
	}

	m_progress.Set(gameSumPercent(1, m_weights, GAME_GD_APPMETA_STEPS));
	return 0;
}

/* the appmeta of info's application made by where it is installed (0x8107F7C6) */
__attribute__((noinline)) int GameGdUpdateAppmetaWriter::Copy(const LsdbAppInfoPtr *info,
							      const string *dir)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int storage = 0;
	int ret = gameAppGetStorage(info, &storage);

	if (ret < 0)
		return ret;

	unsigned int kind = 0;

	it = (*info)->find(LSDB_KEY_UP_STATUS);
	if (it != end)
		kind = (unsigned int)gameLsdbValueInteger(it->second);

	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);
	Dir::CreateRecursive("ur0:appmeta", 6);
	m_progress.Set(gameSumPercent(0, m_weights, GAME_GD_APPMETA_STEPS));

	switch (storage) {
	case GAME_STORAGE_GRO0:
		ret = CopyToNew(dir, &titleId, kind, true, false);
		break;
	case GAME_STORAGE_GRO0_OTHER:
		Dir::CreateRecursive("ux0:appmeta", 6);
		ret = CopyToNew(dir, &titleId, kind, true, true);
		break;
	case GAME_STORAGE_UX0:
		Dir::CreateRecursive("ux0:appmeta", 6);
		ret = CopyToNew(dir, &titleId, kind, false, true);
		break;
	default:
		return GAME_ERROR_NO_STORAGE;
	}

	if (ret < 0)
		return ret;

	return 0;
}

/* an additional content's appmeta (a game's that is not game data) from its folder dir */
int GameGdUpdateAppmetaWriter::Run(GameAppRegistration *reg, const string *dir)
{
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

	if (it == reg->info->end())
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));

	if (!gameDriveIsNotData(category))
		return 0;

	if (category == "gp" || category == "gpc") {
		int ret = Copy(&reg->info, dir);

		if (ret < 0)
			return ret;
	}

	return 0;
}
