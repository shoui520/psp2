/*
 * The game plugin's tree remover: a file or a folder tree removed, each file shrunk first, each
 * folder once it is empty (0x810595CC..0x81059A44). The file's name and the functions' are ours
 * (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <std/string.h>
#include <std/string>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "copier.h"

using namespace paf;

GameTreeRemover::GameTreeRemover(GameJobProgress *progress, GameCancelFunc cancel, void *arg,
				 bool mapErrors, unsigned int maxDepth)
	: m_cancel(cancel), m_cancelArg(arg), m_maxDepth(maxDepth), m_depth(0), m_count(0),
	  m_removed(0), m_progress(progress), m_mapErrors(mapErrors)
{
	sce_paf_memset(&m_dirent, 0, sizeof(m_dirent));
	sce_paf_memset(m_path, 0, sizeof(m_path));
}

GameTreeRemover::~GameTreeRemover()
{
}

/* the progress set to the share of the tree removed (0x81059648) */
void GameTreeRemover::UpdateProgress(void)
{
	if (m_count == 0)
		m_progress->Update(0);
	else
		m_progress->Update((float)(m_removed * 100 / m_count));
}

/*
 * The folder at m_path emptied, file by file and folder by folder, and removed (0x810596D8). A
 * folder that cannot be opened is left, without an error.
 */
int GameTreeRemover::RemoveDir(void)
{
	int ret = 0;

	if (m_depth > m_maxDepth)
		return GAME_ERROR_TOO_DEEP;

	m_depth++;

	SceUID fd = sceIoDopen(m_path);

	if (fd < 0) {
		if (m_mapErrors)
			return gameFileToPafError(ret);
		return ret;
	}

	for (;;) {
		ret = sceIoDread(fd, &m_dirent);

		if (ret == 0)
			break;

		if (ret < 0) {
			sceIoDclose(fd);
			if (m_mapErrors)
				return gameFileToPafError(ret);
			return ret;
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

			ret = gamePathAppend(m_path, sizeof(m_path), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = RemoveDir();
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			*sce_paf_strrchr(m_path, '/') = '\0';
		} else {
			ret = gamePathAppend(m_path, sizeof(m_path), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = gameFileShrink(string(m_path), m_dirent.d_stat.st_size);
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = sceIoRemove(m_path);
			if (ret < 0) {
				sceIoDclose(fd);
				if (m_mapErrors)
					return gameFileToPafError(ret);
				return ret;
			}

			*sce_paf_strrchr(m_path, '/') = '\0';

			m_removed++;
			if (m_progress != NULL)
				UpdateProgress();
		}
	}

	sceIoDclose(fd);

	ret = sceIoRmdir(m_path);
	if (ret < 0) {
		if (m_mapErrors)
			return gameFileToPafError(ret);
		return ret;
	}

	m_removed++;
	if (m_progress != NULL)
		UpdateProgress();

	m_depth--;

	return 0;
}

/*
 * The file or folder tree at path removed, its files and folders counted first when there is a
 * progress to update (0x81059914).
 */
int GameTreeRemover::Remove(const string &path)
{
	SceIoStat stat;
	int ret;

	if (m_progress != NULL) {
		ret = gameTreeGetCount(path.c_str(), m_cancel, m_cancelArg, &m_count);
		if (ret < 0)
			return ret;
	}

	sce_paf_strncpy(m_path, path.c_str(), sizeof(m_path) - 1);
	m_path[sizeof(m_path) - 1] = '\0';

	ret = sceIoGetstat(m_path, &stat);
	if (ret < 0) {
		if (m_mapErrors)
			return gameFileToPafError(ret);
		return ret;
	}

	if (stat.st_mode & SCE_STM_FREG) {
		ret = sceIoRemove(m_path);
		if (ret < 0) {
			if (m_mapErrors)
				return gameFileToPafError(ret);
			return ret;
		}

		m_removed++;
		if (m_progress != NULL)
			UpdateProgress();

		return 0;
	}

	ret = RemoveDir();
	if (ret < 0)
		return ret;

	return 0;
}
