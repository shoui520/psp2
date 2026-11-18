/*
 * The game plugin's folder size counter: the room a file or a folder tree takes on its device, in
 * whole clusters (0x81059A44..0x81059D68). The file's name and the functions' are ours (no record
 * names them).
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

GameDirSizeCounter::GameDirSizeCounter(GameCancelFunc cancel, void *arg, unsigned int maxDepth)
	: m_cancel(cancel), m_cancelArg(arg), m_clusterSize(0), m_totalSize(0),
	  m_maxDepth(maxDepth), m_depth(0)
{
	sce_paf_memset(&m_dirent, 0, sizeof(m_dirent));
	sce_paf_memset(m_path, 0, sizeof(m_path));
}

GameDirSizeCounter::~GameDirSizeCounter()
{
}

/* The folder at m_path walked, its files' clusters added up, its folders walked (0x81059AC4). */
int GameDirSizeCounter::Walk(void)
{
	if (m_depth > m_maxDepth)
		return GAME_ERROR_TOO_DEEP;

	m_depth++;

	SceUID fd = sceIoDopen(m_path);

	if (fd < 0)
		return gameDirToPafError(fd);

	for (;;) {
		int ret = sceIoDread(fd, &m_dirent);

		if (ret == 0)
			break;

		if (ret < 0) {
			sceIoDclose(fd);
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

			ret = gamePathAppend(m_path, sizeof(m_path), name, sizeof(m_dirent.d_name));
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			ret = Walk();
			if (ret < 0) {
				sceIoDclose(fd);
				return ret;
			}

			*sce_paf_strrchr(m_path, '/') = '\0';
		} else {
			m_totalSize += (m_dirent.d_stat.st_size + (m_clusterSize - 1)) /
				       m_clusterSize * m_clusterSize;
		}
	}

	sceIoDclose(fd);
	m_depth--;

	return 0;
}

/*
 * The room the file or folder tree at path takes in clusters of clusterSize bytes: a file's size
 * rounded up, a folder's files' and one cluster for the folder (0x81059C22).
 */
int GameDirSizeCounter::Count(const string *path, SceUInt64 clusterSize, SceUInt64 *size)
{
	SceIoStat stat;

	sce_paf_strncpy(m_path, path->c_str(), sizeof(m_path) - 1);
	m_clusterSize = clusterSize;
	m_path[sizeof(m_path) - 1] = '\0';
	*size = 0;

	int ret = sceIoGetstat(m_path, &stat);

	if (ret < 0)
		return gameFileToPafError(ret);

	if (stat.st_mode & SCE_STM_FREG) {
		*size = (stat.st_size + (m_clusterSize - 1)) / m_clusterSize * m_clusterSize;
		return 0;
	}

	ret = Walk();
	if (ret < 0)
		return ret;

	*size = m_totalSize + m_clusterSize;

	return 0;
}
