/*
 * The game plugin's tree scanner: the files and folders under a path counted and the files' sizes
 * added up (0x81058C90..0x81058EC6). The file's name and the functions' are ours (no record names
 * them).
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

GameTreeScanner::GameTreeScanner(GameCancelFunc cancel, void *arg, unsigned int maxDepth)
	: m_cancel(cancel), m_cancelArg(arg), m_maxDepth(maxDepth), m_depth(0), m_fileCount(0),
	  m_dirCount(0), m_totalSize(0)
{
	sce_paf_memset(&m_dirent, 0, sizeof(m_dirent));
	sce_paf_memset(m_path, 0, sizeof(m_path));
}

GameTreeScanner::~GameTreeScanner()
{
}

/* The folder at m_path walked, its files counted and added up, its folders walked (0x81058D10). */
int GameTreeScanner::Walk(void)
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
			m_fileCount++;
			m_totalSize += m_dirent.d_stat.st_size;
		}
	}

	sceIoDclose(fd);
	m_dirCount++;
	m_depth--;

	return 0;
}

/* The file or the folder tree at path counted (0x81058E4E). */
int GameTreeScanner::Scan(const string &path)
{
	SceIoStat stat;

	sce_paf_strncpy(m_path, path.c_str(), sizeof(m_path) - 1);
	m_path[sizeof(m_path) - 1] = '\0';

	int ret = sceIoGetstat(m_path, &stat);

	if (ret < 0)
		return gameFileToPafError(ret);

	if (stat.st_mode & SCE_STM_FREG) {
		m_fileCount = 1;
		m_totalSize = stat.st_size;
		return 0;
	}

	ret = Walk();
	if (ret < 0)
		return ret;

	return 0;
}
