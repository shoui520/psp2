/*
 * TarArchive (in updater_core, 0x8100da60..0x8100de64): parses a POSIX ustar / GNU LongLink
 * TAR archive from memory and indexes its directory and file entries.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "updater_memory.h"
#include "tar_archive.h"

#define TAR_BLOCK_SIZE		512
#define TAR_ENTRY_ALIGN		512

struct TarHeader {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeFlag;
	char linkName[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devMajor[8];
	char devMinor[8];
	char prefix[155];
	char padding[12];
};

TarArchive::TarArchive()
	: m_directories(4), m_files(4)
{
	m_remainingFiles = 0;
}

TarArchive::~TarArchive()
{
}

int TarArchive::Reset(void)
{
	m_directories.Clear();
	m_files.Clear();
	return 0;
}

int TarArchive::Clear(void)
{
	int dirCount = (int)m_directories.GetCount();
	int fileCount;
	int i;

	for (i = 0; i < dirCount; ++i)
		g_updaterMemoryInfo.updaterCoreFree(m_directories.Get(i));
	fileCount = (int)m_files.GetCount();
	for (i = 0; i < fileCount; ++i)
		g_updaterMemoryInfo.updaterCoreFree(m_files.Get(i));
	m_directories.Clear();
	m_files.Clear();
	return 0;
}

int TarArchive::Parse(const void *archiveBuffer, SceSize archiveSize)
{
	const SceUInt8 *base = static_cast<const SceUInt8 *>(archiveBuffer);
	const TarHeader *header = reinterpret_cast<const TarHeader *>(base);
	char fullPath[TAR_PATH_MAX];
	SceInt8 longNameState = -1;
	int blockIndex = 0;
	SceUInt64 nextOffset = TAR_BLOCK_SIZE;

	while (header->name[0] != '\0') {
		SceUInt64 currentEnd = nextOffset;

		blockIndex++;
		if (longNameState == 'L') {
			if (header->prefix[0] == '/')
				sceClibStrncpy(fullPath, header->name, sizeof(fullPath));
			else
				sceClibSnprintf(fullPath, sizeof(fullPath), "/%s", header->name);
			longNameState = 0;
		} else {
			char typeFlag = header->typeFlag;

			if (typeFlag == '0' || typeFlag == '\0' || typeFlag == '5') {
				if (longNameState != 0) {
					if (header->prefix[0] == '/')
						sceClibSnprintf(fullPath, sizeof(fullPath),
								"%s%s", header->prefix,
								header->name);
					else
						sceClibSnprintf(fullPath, sizeof(fullPath),
								"/%s%s", header->prefix,
								header->name);
				} else {
					longNameState = -1;
				}
				SceUInt32 mode = (SceUInt32)(sceClibStrtoll(header->mode, NULL, 8) &
							     0x1ff);
				if (header->typeFlag == '5') {
					TarDirectoryEntry *dirEntry =
						static_cast<TarDirectoryEntry *>(
							g_updaterMemoryInfo.updaterCoreMemalign(
								TAR_ENTRY_ALIGN,
								sizeof(TarDirectoryEntry)));
					sceClibStrncpy(dirEntry->path, fullPath,
						       sizeof(dirEntry->path));
					dirEntry->mode = mode;
					m_directories.Push(dirEntry);
				} else {
					TarFileEntry *fileEntry = static_cast<TarFileEntry *>(
						g_updaterMemoryInfo.updaterCoreMemalign(
							TAR_ENTRY_ALIGN, sizeof(TarFileEntry)));
					SceInt32 fileSize;
					int dataBlocks;

					sceClibStrncpy(fileEntry->path, fullPath,
						       sizeof(fileEntry->path));
					fileSize = (SceInt32)sceClibStrtoll(header->size, NULL, 8);
					fileEntry->mode = mode;
					fileEntry->size = (SceUInt32)fileSize;
					fileEntry->offset = (SceUInt32)(blockIndex * TAR_BLOCK_SIZE);
					m_files.Push(fileEntry);
					dataBlocks = fileSize / TAR_BLOCK_SIZE;
					if (dataBlocks * TAR_BLOCK_SIZE < fileSize)
						dataBlocks++;
					blockIndex += dataBlocks;
					currentEnd += (SceUInt64)(dataBlocks * TAR_BLOCK_SIZE);
				}
			} else if (typeFlag == 'L') {
				longNameState = 'L';
			}
			if (currentEnd > (SceUInt64)archiveSize)
				break;
		}
		header = reinterpret_cast<const TarHeader *>(base + (SceUInt32)currentEnd);
		nextOffset = currentEnd + TAR_BLOCK_SIZE;
	}
	m_remainingFiles = (int)m_files.GetCount();
	return 0;
}

int TarArchive::GetFileCount(int *count)
{
	*count = (int)m_files.GetCount();
	return 0;
}

int TarArchive::FindFile(const char *path, SceUInt32 *size, SceUInt32 *offset)
{
	int count = (int)m_files.GetCount();
	SceSize pathLen = sceClibStrnlen(path, 0x100);
	int i;

	for (i = 0; i < count; ++i) {
		TarFileEntry *entry = static_cast<TarFileEntry *>(m_files.Get(i));

		if (sceClibStrncmp(path, entry->path, pathLen) == 0) {
			entry = static_cast<TarFileEntry *>(m_files.Get(i));
			*size = entry->size;
			entry = static_cast<TarFileEntry *>(m_files.Get(i));
			*offset = entry->offset;
			m_remainingFiles--;
			return 0;
		}
	}
	return SCE_UPDATER_ERROR_TAR_NOT_FOUND;
}

int TarArchive::GetRemainingFileCount(void)
{
	return m_remainingFiles;
}
