/*
 * The game plugin's file copier: a file or a folder tree copied with two buffers, the next part
 * of a file read into one while the other is written (0x81057D74..0x81058C20). The file's name and
 * the functions' are ours (no record names them).
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>
#include <memory/heap_allocator.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "copier.h"

using namespace paf;

GameFileCopier::GameFileCopier(const GameCopyParam *param)
	: m_cancel(param->cancel), m_cancelArg(param->cancelArg), m_replace(param->replace),
	  m_reserved(param->unusedFlag), m_maxDepth(param->maxDepth), m_depth(0),
	  m_bufSize(GAME_COPIER_BUF_SIZE), m_progress(param->progress), m_totalSize(0),
	  m_doneSize(0), m_detail(param->detail), m_heap(param->heap), m_cur(0), m_remain(0),
	  m_read(0)
{
	sce_paf_memset(&m_dirent, 0, sizeof(m_dirent));
	sce_paf_memset(m_srcPath, 0, sizeof(m_srcPath));
	sce_paf_memset(m_dstPath, 0, sizeof(m_dstPath));
	sce_paf_memset(m_buf, 0, sizeof(m_buf));
	sce_paf_memset(m_chunk, 0, sizeof(m_chunk));

	if (m_heap != NULL) {
		m_buf[0] = m_heap->sce_paf_malloc(m_bufSize);
		m_buf[1] = m_heap->sce_paf_malloc(m_bufSize);
	} else if (param->buf != NULL) {
		m_buf[0] = param->buf;
		m_bufSize = param->bufSize / 2;
		m_buf[1] = (char *)param->buf + m_bufSize;
	}
}

GameFileCopier::~GameFileCopier()
{
	if (m_heap != NULL) {
		if (m_buf[0] != NULL)
			m_heap->sce_paf_free(m_buf[0]);
		if (m_buf[1] != NULL)
			m_heap->sce_paf_free(m_buf[1]);
	} else {
		sce_paf_memset(m_buf, 0, sizeof(m_buf));
	}
}

/* the progress set to the share of the tree copied (0x81057EFC) */
void GameFileCopier::UpdateProgress(void)
{
	if (m_progress == NULL)
		return;

	if (m_totalSize == 0)
		m_progress->Update(0);
	else
		m_progress->Update((float)(m_doneSize * 100 / m_totalSize));
}

/*
 * The file at m_srcPath, of the size stat gives, copied to m_dstPath: each part read into one
 * buffer, the other written while the next part is read (0x81057FD6).
 */
int GameFileCopier::CopyFile(const SceIoStat *stat)
{
	int ret = 0;
	common::SharedPtr<File> src = LocalFile::Open(m_srcPath, SCE_O_RDONLY, 0, &ret);

	if (src.get() == NULL || ret < 0) {
		gamePathGetMountedDrive(string(m_srcPath), m_detail);
		return ret;
	}

	common::SharedPtr<File> dst =
		LocalFile::Open(m_dstPath, SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (dst.get() == NULL || ret < 0) {
		src->Close();
		gamePathGetMountedDrive(string(m_dstPath), m_detail);
		return ret;
	}

	m_cur = 0;
	m_remain = stat->st_size;
	m_read = 0;
	sce_paf_memset(m_chunk, 0, sizeof(m_chunk));
	m_read = 0;

	m_chunk[m_cur] = m_bufSize;
	if (m_remain < m_chunk[m_cur])
		m_chunk[m_cur] = m_remain;

	do {
		ret = src->Read(m_buf[m_cur], m_chunk[m_cur]);
		if (ret < 0) {
			gamePathGetMountedDrive(string(m_srcPath), m_detail);
			break;
		}

		m_doneSize += m_chunk[m_cur];
		UpdateProgress();

		{
			SceOff size = m_chunk[m_cur];

			m_cur ^= 1;
			m_read += size;
			m_remain -= size;
			m_chunk[m_cur] = m_bufSize;
		}

		bool loopFailed = false;
		while (m_remain > 0) {
			if (m_remain < m_chunk[m_cur])
				m_chunk[m_cur] = m_remain;

			int readRet = src->ReadAsync(m_buf[m_cur], m_chunk[m_cur]);

			m_cur ^= 1;

			if (m_cancel != NULL && m_cancel(m_cancelArg) != 0) {
				ret = GAME_ERROR_ABORTED;
				loopFailed = true;
				break;
			}

			int writeRet = dst->Write(m_buf[m_cur], m_chunk[m_cur]);

			if (writeRet < 0) {
				ret = writeRet;
				gamePathGetMountedDrive(string(m_dstPath), m_detail);
				loopFailed = true;
				break;
			}

			if (m_cancel != NULL && m_cancel(m_cancelArg) != 0) {
				ret = GAME_ERROR_ABORTED;
				loopFailed = true;
				break;
			}

			m_doneSize += m_chunk[m_cur];
			UpdateProgress();

			if (readRet != 0) {
				ret = readRet;
				gamePathGetMountedDrive(string(m_srcPath), m_detail);
				loopFailed = true;
				break;
			}

			SceInt64 result;

			ret = src->WaitAsync(&result);
			if (ret < 0 || result < 0) {
				if (result < 0)
					ret = (int)result;
				gamePathGetMountedDrive(string(m_srcPath), m_detail);
				loopFailed = true;
				break;
			}

			m_doneSize += m_chunk[m_cur ^ 1];
			UpdateProgress();

			m_read += m_chunk[m_cur ^ 1];
			m_remain -= m_chunk[m_cur];
		}
		if (loopFailed)
			break;

		m_cur ^= 1;

		ret = dst->Write(m_buf[m_cur], m_chunk[m_cur]);
		if (ret < 0) {
			gamePathGetMountedDrive(string(m_dstPath), m_detail);
			break;
		}

		m_doneSize += m_chunk[m_cur];

		src->Close();
		src.reset();
		dst->Close();
		dst.reset();

		return 0;
	} while (0);

	if (src.get() != NULL) {
		SceInt64 result = 0;

		src->WaitAsync(&result);
		src->Close();
	}

	if (dst.get() != NULL)
		dst->Close();

	return ret;
}

/* The folder at m_srcPath copied into m_dstPath, file by file and folder by folder (0x81058872). */
int GameFileCopier::CopyDir(void)
{
	if (m_depth > m_maxDepth)
		return GAME_ERROR_TOO_DEEP;

	m_depth++;

	SceUID fd = sceIoDopen(m_srcPath);

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
				sceIoDclose(fd);
				return gameFileToPafError(ret);
			}

			ret = CopyDir();
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

			ret = CopyFile(&m_dirent.d_stat);
			if (ret < 0) {
				sceIoDclose(fd);
				return gameFileToPafError(ret);
			}

			*sce_paf_strrchr(m_srcPath, '/') = '\0';
			*sce_paf_strrchr(m_dstPath, '/') = '\0';
		}
	}

	sceIoDclose(fd);
	m_depth--;

	return 0;
}

/*
 * The file or folder tree at src copied to dst; with m_replace, what dst holds is removed first,
 * else it must be of src's kind (0x81058A50).
 */
int GameFileCopier::Copy(const string *src, const string *dst, SceUInt64 *copied)
{
	SceIoStat srcStat;
	SceIoStat dstStat;

	sce_paf_strncpy(m_srcPath, src->c_str(), sizeof(m_srcPath) - 1);
	m_srcPath[sizeof(m_srcPath) - 1] = '\0';
	sce_paf_strncpy(m_dstPath, dst->c_str(), sizeof(m_dstPath) - 1);
	m_dstPath[sizeof(m_dstPath) - 1] = '\0';

	if (m_buf[0] == NULL || m_buf[1] == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = sceIoGetstat(m_srcPath, &srcStat);

	if (ret < 0) {
		gamePathGetMountedDrive(string(m_srcPath), m_detail);
		return gameFileToPafError(ret);
	}

	ret = sceIoGetstat(m_dstPath, &dstStat);

	if (m_replace) {
		if (dstStat.st_mode & SCE_STM_FREG)
			sceIoRemove(dst->c_str());
		else
			gameRemoveDirectory(dst->c_str(), 0, 0, 0, true);
	} else if (ret == 0 && ((srcStat.st_mode ^ dstStat.st_mode) & SCE_STM_FMT) != 0) {
		return GAME_ERROR_KIND_MISMATCH;
	}

	if (m_progress != NULL) {
		ret = gameTreeGetSize(m_srcPath, m_cancel, m_cancelArg, &m_totalSize);
		if (ret < 0) {
			gamePathGetMountedDrive(string(m_srcPath), m_detail);
			return ret;
		}

		m_totalSize *= 2;
	}

	if (srcStat.st_mode & SCE_STM_FREG) {
		ret = CopyFile(&srcStat);
		if (ret < 0)
			return gameFileToPafError(ret);
	} else {
		if (!Dir::IsExist(dst->c_str())) {
			ret = Dir::CreateRecursive(dst->c_str(), 6);
			if (ret < 0)
				return ret;
		}

		ret = CopyDir();
		if (ret < 0)
			return ret;
	}

	return 0;
}
