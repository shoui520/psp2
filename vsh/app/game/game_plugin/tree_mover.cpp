/*
 * The game plugin's tree mover: a file or a folder tree moved, its folders made at the destination
 * and its files renamed into them (0x81058F38..0x810595CC). The file's name and the functions'
 * are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "copier.h"

using namespace paf;

GameTreeMover::GameTreeMover(const GameTreeMoveParam *param)
	: m_cancel(param->cancel), m_cancelArg(param->cancelArg), m_maxDepth(param->maxDepth),
	  m_depth(0), m_count(0), m_moved(0), m_progress(param->progress), m_detail(param->detail),
	  m_reserved(param->reserved), m_unusedFlag(param->unusedFlag), m_moveFiles(param->moveFiles)
{
	sce_paf_memset(&m_dirent, 0, sizeof(m_dirent));
	sce_paf_memset(m_srcPath, 0, sizeof(m_srcPath));
	sce_paf_memset(m_dstPath, 0, sizeof(m_dstPath));
}

GameTreeMover::~GameTreeMover()
{
}

/* The file at m_srcPath renamed to m_dstPath, what was there removed first (0x81058FDA). */
int GameTreeMover::MoveFile(void)
{
	if (!m_moveFiles)
		return 0;

	sceIoRemove(m_dstPath);

	int ret = sceIoRename(m_srcPath, m_dstPath);

	if (ret < 0) {
		gamePathGetMountedDrive(string(m_dstPath), m_detail);
		return ret;
	}

	return 0;
}

/*
 * The folder at m_srcPath moved into m_dstPath, file by file and folder by folder, and then
 * removed (0x8105906C).
 */
int GameTreeMover::MoveDir(void)
{
	if (m_depth > m_maxDepth)
		return GAME_ERROR_TOO_DEEP;

	m_depth++;

	SceUID fd = sceIoDopen(m_srcPath);

	if (fd < 0) {
		gamePathGetMountedDrive(string(m_srcPath), m_detail);
		return gameDirToPafError(fd);
	}

	for (;;) {
		int ret = sceIoDread(fd, &m_dirent);

		if (ret == 0)
			break;

		if (ret < 0) {
			sceIoDclose(fd);
			gamePathGetMountedDrive(string(m_srcPath), m_detail);
			return gameDirToPafError(ret);
		}

		if (m_cancel != NULL && m_cancel(m_cancelArg) != 0) {
			sceIoDclose(fd);
			return GAME_ERROR_ABORTED;
		}

		const char *name = m_dirent.d_name;

		if (m_dirent.d_stat.st_mode & SCE_STM_FDIR) {
			if (name[0] == '.' && name[1] == '\0')
				continue;
			if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
				continue;

			ret = gamePathAppend(m_srcPath, sizeof(m_srcPath), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = gamePathAppend(m_dstPath, sizeof(m_dstPath), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = sceIoMkdir(m_dstPath, 6);
			if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
				gamePathGetMountedDrive(string(m_dstPath), m_detail);
				sceIoDclose(fd);
				return gameFileToPafError(ret);
			}

			m_moved++;

			ret = MoveDir();
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			*sce_paf_strrchr(m_srcPath, '/') = '\0';
			*sce_paf_strrchr(m_dstPath, '/') = '\0';
		} else {
			ret = gamePathAppend(m_srcPath, sizeof(m_srcPath), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = gamePathAppend(m_dstPath, sizeof(m_dstPath), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = MoveFile();
			if (ret < 0) {
				sceIoDclose(fd);
				return gameFileToPafError(ret);
			}

			m_moved++;

			*sce_paf_strrchr(m_srcPath, '/') = '\0';
			*sce_paf_strrchr(m_dstPath, '/') = '\0';
		}
	}

	sceIoDclose(fd);

	int ret = gameRemoveDirectory(m_srcPath, 0, 0, 0, true);

	if (ret < 0)
		return ret;

	m_depth--;

	return 0;
}

/*
 * The file or folder tree at src moved to dst: dst made when it is missing, a file moved into
 * it, a folder's contents moved into it and the folder removed (0x8105936E).
 */
int GameTreeMover::Move(const string &src, const string &dst)
{
	SceIoStat stat;

	sce_paf_strncpy(m_srcPath, src.c_str(), sizeof(m_srcPath) - 1);
	m_srcPath[sizeof(m_srcPath) - 1] = '\0';
	sce_paf_strncpy(m_dstPath, dst.c_str(), sizeof(m_dstPath) - 1);
	m_dstPath[sizeof(m_dstPath) - 1] = '\0';

	int ret = sceIoGetstat(m_dstPath, &stat);

	if (ret == 0) {
		if (stat.st_mode & SCE_STM_FREG) {
			gamePathGetMountedDrive(string(m_dstPath), m_detail);
			return GAME_ERROR_KIND_MISMATCH;
		}
	} else if (ret == SCE_ERROR_ERRNO_ENOENT) {
		ret = Dir::CreateRecursive(m_dstPath, 6);
		if (ret < 0) {
			gamePathGetMountedDrive(string(m_dstPath), m_detail);
			return ret;
		}
	}

	if (sceIoGetstat(m_srcPath, &stat) == 0 && (stat.st_mode & SCE_STM_FREG)) {
		char *name = sce_paf_strrchr(m_srcPath, '/');

		if (name == NULL)
			return GAME_ERROR_NO_DRIVE;

		if (gamePathAppend(m_dstPath, sizeof(m_dstPath), name + 1, sizeof(m_dstPath)) < 0)
			return GAME_ERROR_NO_DRIVE;

		ret = MoveFile();
		if (ret < 0)
			return gameFileToPafError(ret);

		return 0;
	}

	if (m_progress != NULL) {
		ret = gameTreeGetCount(m_srcPath, m_cancel, m_cancelArg, &m_count);
		if (ret < 0) {
			gamePathGetMountedDrive(string(m_srcPath), m_detail);
			return ret;
		}
	}

	ret = MoveDir();
	if (ret < 0)
		return ret;

	gameRemoveDirectory(m_srcPath, 0, 0, 0, true);

	return 0;
}
