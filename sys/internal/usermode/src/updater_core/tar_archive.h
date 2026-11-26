/*
 * TarArchive (in updater_core, 0x8100da60..0x8100de64): parses a POSIX ustar / GNU LongLink
 * TAR stream extracted from a differential update SPKG and indexes its directories and files.
 * The class and method names are ours.
 */
#ifndef UPDATER_CORE_TAR_ARCHIVE_H
#define UPDATER_CORE_TAR_ARCHIVE_H

#include <scetypes.h>
#include "updater_array.h"
#include "updater_object.h"

#define TAR_PATH_MAX	512

struct TarDirectoryEntry {
	char path[TAR_PATH_MAX];
	SceUInt32 mode;
};

struct TarFileEntry {
	char path[TAR_PATH_MAX];
	SceUInt32 size;
	SceUInt32 offset;
	SceUInt32 mode;
};

/* 40 bytes (0x28) */
class TarArchive : public UpdaterObject {
public:
	TarArchive();
	virtual ~TarArchive();

	int Reset(void);
	int Clear(void);
	int Parse(const void *archiveBuffer, SceSize archiveSize);
	int GetFileCount(int *count);
	int FindFile(const char *path, SceUInt32 *size, SceUInt32 *offset);
	int GetRemainingFileCount(void);

private:
	UpdaterArray m_directories;
	UpdaterArray m_files;
	int m_remainingFiles;
};

#endif
