/*
 * The game plugin's copiers and walkers of folder trees: a file or a folder tree copied with two
 * buffers, one read while the other is written (file_copier.cpp), and a tree's files and folders
 * counted (tree_scanner.cpp), a folder tree moved, file by file (tree_mover.cpp), removed
 * (tree_remover.cpp), and the room it takes on its device counted (dir_size_counter.cpp). The names
 * are ours (no record names them).
 */
#ifndef _GAME_PLUGIN_COPIER_H
#define _GAME_PLUGIN_COPIER_H

#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <std/string>
#include <memory/heap_allocator.h>

#include "game_plugin.h"
#include "game_appinfo.h"

/* the copier's two buffers, when it allocates them */
#define GAME_COPIER_BUF_SIZE	4096

/* what a file copier is made with */
struct GameCopyParam {
	paf::memory::HeapAllocator *heap;	/* the buffers' heap, or NULL to share buf */
	GameCancelFunc cancel;
	void *cancelArg;
	unsigned int maxDepth;			/* the folders a copy goes down at most */
	bool reserved;
	bool replace;				/* what is at the destination removed first */
	bool unusedFlag;			/* kept by the copier, read by nothing */
	void *buf;				/* split in two, when there is no heap */
	SceSize bufSize;
	GameJobProgress *progress;		/* the copy's progress, or NULL */
	paf::string *detail;			/* where a copy that failed was */
};

/*
 * A file or a folder tree copied (constructor 0x81057D74, copy 0x81058A50): each file read into
 * one buffer while the other is written.
 */
class GameFileCopier {
public:
	GameFileCopier(const GameCopyParam *param);
	virtual ~GameFileCopier();

	int Copy(const paf::string *src, const paf::string *dst, SceUInt64 *copied);

private:
	void UpdateProgress(void);
	int CopyFile(const SceIoStat *stat);
	int CopyDir(void);

	SceIoDirent m_dirent;			/* the entry of the folder being copied */
	char m_srcPath[SCE_IO_MAX_PATH_BUFFER_SIZE];
	char m_dstPath[SCE_IO_MAX_PATH_BUFFER_SIZE];
	GameCancelFunc m_cancel;
	void *m_cancelArg;
	bool m_replace;
	bool m_reserved;
	unsigned int m_maxDepth;
	unsigned int m_depth;
	void *m_buf[2];
	SceSize m_bufSize;
	GameJobProgress *m_progress;
	SceUInt64 m_totalSize;			/* twice the tree's size: each byte read and written */
	SceUInt64 m_doneSize;
	paf::string *m_detail;
	paf::memory::HeapAllocator *m_heap;
	unsigned int m_cur;			/* the buffer read into next */
	SceOff m_remain;			/* what is left of the file to read */
	SceOff m_read;				/* what has been read of the file */
	SceOff m_chunk[2];			/* what each buffer holds */
};

typedef char GameFileCopierSize[sizeof(GameFileCopier) == 2512 ? 1 : -1];

/*
 * A file or folder tree walked (constructor 0x81058C90, scan 0x81058E4E): the files and folders
 * under a path counted, at most maxDepth folders deep, and the files' sizes added up.
 */
class GameTreeScanner {
public:
	GameTreeScanner(GameCancelFunc cancel, void *arg, unsigned int maxDepth);
	virtual ~GameTreeScanner();

	int Scan(const paf::string &path);

private:
	int Walk(void);

	SceIoDirent m_dirent;			/* the entry of the folder being walked */
	char m_path[SCE_IO_MAX_PATH_BUFFER_SIZE];
	GameCancelFunc m_cancel;
	void *m_cancelArg;
	unsigned int m_maxDepth;
	unsigned int m_depth;

public:
	unsigned int m_fileCount;
	unsigned int m_dirCount;
	SceUInt64 m_totalSize;
};

typedef char GameTreeScannerSize[sizeof(GameTreeScanner) == 1416 ? 1 : -1];

/* what a tree mover is made with */
struct GameTreeMoveParam {
	GameCancelFunc cancel;
	void *cancelArg;
	unsigned int maxDepth;			/* the folders a move goes down at most */
	bool reserved;				/* kept by the mover, read by nothing */
	bool unusedFlag;			/* kept by the mover, read by nothing */
	bool moveFiles;				/* the files renamed into place, else left */
	GameJobProgress *progress;		/* the files counted first when not NULL */
	paf::string *detail;			/* where a move that failed was */
};

/*
 * A file or a folder tree moved (constructor 0x81058F38, move 0x8105936E): its folders made at
 * the destination, its files renamed into them, and the emptied folders removed.
 */
class GameTreeMover {
public:
	GameTreeMover(const GameTreeMoveParam *param);
	virtual ~GameTreeMover();

	virtual int Move(const paf::string &src, const paf::string &dst);

private:
	int MoveFile(void);
	int MoveDir(void);

	SceIoDirent m_dirent;			/* the entry of the folder being moved */
	char m_srcPath[SCE_IO_MAX_PATH_BUFFER_SIZE];
	char m_dstPath[SCE_IO_MAX_PATH_BUFFER_SIZE];
	GameCancelFunc m_cancel;
	void *m_cancelArg;
	unsigned int m_maxDepth;
	unsigned int m_depth;
	SceUInt32 m_count;			/* the files and folders under the source */
	unsigned int m_moved;			/* the files and folders moved so far */
	GameJobProgress *m_progress;
	paf::string *m_detail;
	bool m_reserved;
	bool m_unusedFlag;
	bool m_moveFiles;
};

typedef char GameTreeMoverSize[sizeof(GameTreeMover) == 2448 ? 1 : -1];

/*
 * A file or a folder tree removed (constructor 0x810595CC, remove 0x81059914): each file shrunk and
 * removed, each folder removed once it is empty, at most maxDepth folders deep.
 */
class GameTreeRemover {
public:
	GameTreeRemover(GameJobProgress *progress, GameCancelFunc cancel, void *arg, bool mapErrors,
			unsigned int maxDepth);
	virtual ~GameTreeRemover();

	int Remove(const paf::string &path);

private:
	void UpdateProgress(void);
	int RemoveDir(void);

	SceIoDirent m_dirent;			/* the entry of the folder being removed */
	char m_path[SCE_IO_MAX_PATH_BUFFER_SIZE];
	GameCancelFunc m_cancel;
	void *m_cancelArg;
	unsigned int m_maxDepth;
	unsigned int m_depth;
	SceUInt32 m_count;			/* the files and folders under the path */
	unsigned int m_removed;			/* the files and folders removed so far */
	GameJobProgress *m_progress;
	bool m_mapErrors;			/* errors translated by gameFileToPafError */
};

typedef char GameTreeRemoverSize[sizeof(GameTreeRemover) == 1416 ? 1 : -1];

/*
 * The room a file or a folder tree takes on its device (constructor 0x81059A44, count 0x81059C22):
 * each file's size rounded up to whole clusters, and one cluster for the folder counted.
 */
class GameDirSizeCounter {
public:
	GameDirSizeCounter(GameCancelFunc cancel, void *arg, unsigned int maxDepth);
	virtual ~GameDirSizeCounter();

	int Count(const paf::string *path, SceUInt64 clusterSize, SceUInt64 *size);

private:
	int Walk(void);

	SceIoDirent m_dirent;			/* the entry of the folder being walked */
	char m_path[SCE_IO_MAX_PATH_BUFFER_SIZE];
	GameCancelFunc m_cancel;
	void *m_cancelArg;
	SceUInt64 m_clusterSize;
	SceUInt64 m_totalSize;			/* the files' clusters, in bytes */
	unsigned int m_maxDepth;
	unsigned int m_depth;
};

typedef char GameDirSizeCounterSize[sizeof(GameDirSizeCounter) == 1416 ? 1 : -1];

#endif /* _GAME_PLUGIN_COPIER_H */
