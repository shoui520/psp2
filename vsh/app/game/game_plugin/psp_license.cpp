/*
 * The game plugin's PSP licenses: the name of a PSP application's license (its content ID and
 * ".rif"), read from its EBOOT.PBP or from an EDAT file, and the license removed (0x81059D68..). The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* a removal asked to stop (0x81544108) */
static bool s_stopRequested;

/* where a PSP EBOOT's data.psp keeps its content ID, and where a license file keeps its own */
#define GAME_PSP_DATA_CONTENT_ID_OFFSET	0x560
#define GAME_PSP_EDAT_CONTENT_ID_OFFSET	16
#define GAME_PSP_EDAT_MIN_SIZE		64

/* a file name as a removal step lists it */
#define GAME_PSP_NAME_SIZE		256

/* what a removal step reports: go on, the tree is removed, it failed */
#define GAME_PSP_REMOVE_CONTINUE	0
#define GAME_PSP_REMOVE_DONE		1
#define GAME_PSP_REMOVE_FAILED		2

/* where the PSP emulator keeps the licenses */
#define GAME_PSP_LICENSE_DIR		"ux0:pspemu/PSP/LICENSE"

/* a content ID as it is read, and the license name made of it */
#define GAME_PSP_CONTENT_ID_SIZE	48
#define GAME_PSP_RIF_NAME_SIZE		52

/*
 * The name of a PSP application's license, from the content ID its EBOOT.PBP's data.psp holds
 * (0x81059D68).
 */
int gamePspEbootGetRifName(const char *path, char *rifName)
{
	int ret = 0;
	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	SceInt64 size = file->GetFileSize();

	if (size < 0)
		return (int)size;

	GamePbpHeader header;
	int read = file->Read(&header, sizeof(header));

	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(header)) {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	if (header.magic[0] != '\0' || header.magic[1] != 'P' || header.magic[2] != 'B' ||
	    header.magic[3] != 'P') {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	SceUInt32 offset = header.dataPspOffset + GAME_PSP_DATA_CONTENT_ID_OFFSET;

	if (size < offset) {
		file->Close();
		return GAME_ERROR_BAD_PBP;
	}

	SceInt64 pos = file->Seek(offset, SCE_SEEK_SET);

	if (pos < 0) {
		file->Close();
		return (int)pos;
	}

	char contentId[GAME_PSP_CONTENT_ID_SIZE];

	read = file->Read(contentId, sizeof(contentId));
	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(contentId))
		ret = GAME_ERROR_BAD_PBP;

	file->Close();
	sce_paf_snprintf(rifName, GAME_PSP_RIF_NAME_SIZE, "%s.rif", contentId);

	return ret;
}

/* The name of a PSP license, from the content ID the EDAT file at path holds (0x8105A142). */
int gamePspEdatGetRifName(const char *path, char *rifName)
{
	LocalFileStat stat;
	int ret = LocalFile::Getstat(path, &stat);

	if (ret < 0)
		return ret;

	if (stat.st_size < GAME_PSP_EDAT_MIN_SIZE)
		return GAME_ERROR_BAD_PBP;

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	file->Seek(GAME_PSP_EDAT_CONTENT_ID_OFFSET, SCE_SEEK_SET);

	char contentId[GAME_PSP_CONTENT_ID_SIZE];
	int read = file->Read(contentId, sizeof(contentId));

	if (read < 0) {
		file->Close();
		return read;
	}

	if (read != sizeof(contentId))
		ret = GAME_ERROR_BAD_PBP;

	file->Close();
	sce_paf_snprintf(rifName, GAME_PSP_RIF_NAME_SIZE, "%s.rif", contentId);

	return ret;
}

/*
 * The license of the PSP application or content at path (an EBOOT.PBP or an EDAT file) removed
 * from the PSP emulator's licenses, when it is there (0x8105A346).
 */
int gamePspLicenseRemove(const string &path)
{
	LocalFileStat stat;
	char rifName[64];
	int ret;

	if (path.find("EBOOT.PBP") != string::npos)
		ret = gamePspEbootGetRifName(path.c_str(), rifName);
	else if (path.find(".EDAT") != string::npos || path.find(".edat") != string::npos)
		ret = gamePspEdatGetRifName(path.c_str(), rifName);
	else
		return 0;

	if (ret != 0)
		return 0;

	string licensePath = string(GAME_PSP_LICENSE_DIR) + "/" + string(rifName);

	if (LocalFile::Getstat(licensePath.c_str(), &stat) != 0)
		return 0;

	ret = LocalFile::RemoveFile(licensePath.c_str());
	if (ret < 0)
		return ret;

	return ret;
}

/*
 * The names of at most maxNames files (not folders) of the folder at path, each in GAME_PSP_NAME_SIZE
 * bytes of names; count is what was read (inlined into gamePspTreeRemoveStep).
 */
static int gamePspDirReadFileNames(const char *path, char *names, unsigned int maxNames,
				   unsigned int *count)
{
	Dir dir;
	DirEnt entry;
	unsigned int n = 0;
	int ret;

	*count = 0;

	ret = dir.Open(path);
	if (ret >= 0) {
		for (;;) {
			ret = dir.Read(&entry);
			if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY) {
				ret = 0;
				*count = n;
				break;
			}

			if (ret < 0)
				break;

			if (entry.type != DirEnt::Type_Dir) {
				sce_paf_strncpy(names, entry.name.c_str(), GAME_PSP_NAME_SIZE - 1);
				names[GAME_PSP_NAME_SIZE - 1] = '\0';
				n++;
				names += GAME_PSP_NAME_SIZE;
				if (n == maxNames) {
					ret = 0;
					*count = n;
					break;
				}
			}
		}
	}

	dir.Close();

	return ret;
}

/*
 * One step of a PSP folder tree's removal (0x8105A760): the files of the folder at path removed,
 * maxNames at a time with their licenses, then the step goes down into its first folder (depth
 * one more) or, once the folder is empty, removes it and goes back up. path holds pathLen
 * characters; names is the room for maxNames file names. GAME_PSP_REMOVE_DONE once the tree is
 * removed, GAME_PSP_REMOVE_FAILED with the error in result.
 */
int gamePspTreeRemoveStep(char *path, SceSize *pathLen, char *names, unsigned int maxNames,
			  int *depth, int *result)
{
	LocalFileStat stat;
	Dir dir;
	DirEnt entry;
	LocalFileStat entryStat;
	char filePath[SCE_IO_MAX_PATH_BUFFER_SIZE + 1];
	int ret;

	if (result != NULL)
		*result = 0;

	SceSize baseLen = sce_paf_strlen(path);

	for (;;) {
		if (s_stopRequested) {
			ret = GAME_ERROR_ABORTED;
			break;
		}

		unsigned int count;

		ret = gamePspDirReadFileNames(path, names, maxNames, &count);
		if (ret < 0)
			break;

		if (count == 0) {
			ret = 0;
			break;
		}

		sce_paf_strncpy(filePath, path, SCE_IO_MAX_PATH_BUFFER_SIZE);

		const char *name = names;

		for (unsigned int i = 0; i < count; i++) {
			if (baseLen + sce_paf_strlen(name) + 1 > SCE_IO_MAX_PATH_BUFFER_SIZE) {
				ret = GAME_ERROR_TOO_DEEP;
				goto removed;
			}

			if (sce_paf_strlen(filePath) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(filePath, "/",
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(filePath));
			if (sce_paf_strlen(filePath) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(filePath, name,
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(filePath));

			ret = gamePspLicenseRemove(string(filePath));
			if (ret < 0)
				goto removed;

			ret = LocalFile::RemoveFile(filePath);
			if (ret < 0)
				goto removed;

			filePath[baseLen] = '\0';
			name += GAME_PSP_NAME_SIZE;
		}
	}

removed:
	if (ret < 0)
		goto failed;

	if (dir.Open(path) < 0) {
		ret = LocalFile::Getstat(path, &stat);
		if (ret < 0)
			goto failed;

		if (stat.st_mode & SCE_STM_FREG) {
			ret = LocalFile::RemoveFile(path);
			if (ret < 0)
				goto failed;
		}

		*result = 0;
		return GAME_PSP_REMOVE_DONE;
	}

	for (;;) {
		if (s_stopRequested) {
			ret = GAME_ERROR_ABORTED;
			goto failed;
		}

		ret = dir.Read(&entry);
		if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
			break;

		if (ret < 0)
			goto failed;

		if (entry.type != DirEnt::Type_Dir ||
		    sce_paf_strncmp(entry.name.c_str(), ".", 2) == 0 ||
		    sce_paf_strncmp(entry.name.c_str(), "..", 3) == 0)
			continue;

		dir.Close();

		SceSize size = entry.name.size();
		const char *dirName = entry.name.c_str();

		if (*pathLen + size + 1 <= SCE_IO_MAX_PATH_BUFFER_SIZE) {
			if (sce_paf_strlen(path) != SCE_IO_MAX_PATH_BUFFER_SIZE)
				sce_paf_strncat(path, "/",
						SCE_IO_MAX_PATH_BUFFER_SIZE - sce_paf_strlen(path));
			sce_paf_memcpy(&path[*pathLen + 1], dirName, size);
			ret = 0;
			path[*pathLen + 1 + size] = '\0';
			*pathLen += size + 1;
		} else {
			ret = GAME_ERROR_TOO_DEEP;
		}

		if (ret < 0)
			goto failed;

		(*depth)++;
		*result = 0;
		return GAME_PSP_REMOVE_CONTINUE;
	}

	dir.Close();

	if (*depth > 0) {
		ret = LocalFile::Getstat(path, &stat);
		if (ret < 0)
			goto failed;

		ret = Dir::Remove(path);
		if (ret < 0)
			goto failed;
	} else {
		ret = 0;
	}

	(*depth)--;

	{
		char *slash = sce_paf_strrchr(path, '/');

		if (slash != NULL) {
			*slash = '\0';
			*pathLen = slash - path;
		}
	}

	if (*depth >= 0) {
		*result = 0;
		return GAME_PSP_REMOVE_CONTINUE;
	}

	*result = ret;
	return GAME_PSP_REMOVE_DONE;

failed:
	*result = ret;
	return GAME_PSP_REMOVE_FAILED;
}
