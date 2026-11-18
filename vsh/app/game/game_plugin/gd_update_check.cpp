/*
 * The game plugin's check of an application's update package against the installed
 * application: none for an application not installed; ux0: must be there, a game card's
 * application takes no update here, and a downloaded one is moved to ux0: and must have a newer
 * APP_VER than 1.00 or no patch already (0x8107DA08..0x8107DF50). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
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

GameGdUpdateCheck::GameGdUpdateCheck(const string &titleId, const string &path, bool notImported,
				     string *errorTitleId)
	: m_titleId(titleId), m_path(path), m_notImported(notImported),
	  m_errorTitleId(errorTitleId)
{
}

GameGdUpdateCheck::~GameGdUpdateCheck()
{
}

/*
 * By the application's storage: a game card's application (its gro0:app folder there or not)
 * takes no update, a downloaded one on gro0: is moved to ux0: and must not have a patch yet or
 * be past APP_VER 1.00, one on ux0: is fine (0x8107DAF4).
 */
__attribute__((noinline)) int GameGdUpdateCheck::CheckStorage(int storage,
							      const LsdbAppInfoPtr *info)
{
	string path = string("gro0:app/") + m_titleId;

	if (Dir::IsExist(path.c_str())) {
		if (storage == GAME_STORAGE_GRO0)
			return GAME_ERROR_GC_UPDATE;
		if (storage == GAME_STORAGE_GRO0_OTHER)
			return 0;
		return GAME_ERROR_NO_STORAGE;
	}

	if (storage == GAME_STORAGE_GRO0)
		return GAME_ERROR_GC_UPDATE;

	if (storage == GAME_STORAGE_GRO0_OTHER) {
		int ret = gameAppInfoMoveToUx0(const_cast<LsdbAppInfoPtr *>(info), true,
					       m_notImported, &m_path);

		if (ret < 0)
			return ret;

		if (!gamePatchExists(m_titleId)) {
			SceUInt32 version = 0;
			LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_APP_VER);

			if (it != (*info)->end() &&
			    gameVersionToNumber(LsdbValueText(it->second), 8, &version) == 0 &&
			    version > 100)
				return 0;
		}

		return GAME_ERROR_GC_QUIET;
	}

	if (storage == GAME_STORAGE_UX0)
		return 0;

	return GAME_ERROR_NO_STORAGE;
}

int GameGdUpdateCheck::Check(const LsdbAppInfoPtr *info)
{
	if ((*info)->size() == 0)
		return 0;

	if (m_titleId.length() == 0)
		return GAME_ERROR_INVALID_ARGUMENT;

	LocalFileStat stat;

	if (LocalFile::Getstat("ux0:", &stat) < 0) {
		gamePathGetMountedDrive(string("ux0:"), m_errorTitleId);
		return GAME_ERROR_NO_UX0;
	}

	int storage = 0;
	int ret = gameAppGetStorage(info, &storage);

	if (ret < 0)
		return ret;

	ret = CheckStorage(storage, info);
	if (ret < 0)
		return ret;

	return 0;
}
