/*
 * PlayStation Plus's automatic save data upload: the save data entries in each user's list
 * (savedata_plus/auto_upload/up_savedata_list.dat), which the system takes out when an
 * application goes. Sony's gamedata_plus_util, linked into the shell. The file's and every
 * name here are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <std/string.h>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/localfile.h>

#include "plus_list.h"

using namespace paf;

#define PLUS_LIST_DEVICE	"ux0:"
#define PLUS_LIST_DIR		"savedata_plus"
#define PLUS_LIST_AUTO_UPLOAD	"auto_upload"
#define PLUS_SD_LIST_FILE	"up_savedata_list.dat"

#define PLUS_USER_DIR_PARTITION	2
#define PLUS_USERS_MAX		64

static thread::RMutex s_sdListMutex("SceGameDataSDList", 0);

/* whether titleId is a title ID that fits an entry: 0 or PLUS_ERROR_INVALID_ARG */
static inline int plusCheckTitleId(const char *titleId)
{
	if (titleId == NULL)
		return PLUS_ERROR_INVALID_ARG;

	if (titleId[0] == '\0')
		return PLUS_ERROR_INVALID_ARG;

	for (unsigned int i = 1; i < PLUS_TITLE_ID_SIZE; i++)
		if (titleId[i] == '\0')
			return 0;

	return PLUS_ERROR_INVALID_ARG;
}

/*
 * The entry of titleId in the list at path cleared and the header's count lowered: 0,
 * PLUS_ERROR_NO_ENTRY without one, or an error (0x8102AAAC).
 */
static __attribute__((noinline)) int plusSdListFileDelete(const char *titleId, const char *path)
{
	common::SharedPtr<File> file;
	LocalFileStat stat;
	int ret = LocalFile::Getstat(path, &stat);

	if (ret == 0) {
		file = LocalFile::Open(path, SCE_O_RDWR, 6, &ret);

		if (ret == 0) {
			PlusSdListHeader header;
			PlusSdListEntry entry;

			sce_paf_memset(&header, 0, sizeof(header));

			int size = file->Read(&header, sizeof(header));

			if (size < 0) {
				ret = size;
				goto close;
			}

			if ((SceUInt32)size < sizeof(header)) {
				ret = PLUS_ERROR_IO;
				goto close;
			}

			SceInt64 offset = size;

			if (sce_paf_strncmp(header.magic, PLUS_SD_LIST_MAGIC, 4) != 0) {
				ret = PLUS_ERROR_BAD_LIST;
				goto close;
			}

			sce_paf_memset(&entry, 0, sizeof(entry));

			SceUInt32 i = 0;

			while (i < header.count) {
				SceInt64 entryOffset = offset;

				sce_paf_memset(&entry, 0, sizeof(entry));
				size = file->Read(&entry, sizeof(entry));
				if (size < 0) {
					ret = size;
					goto close;
				}
				if ((SceUInt32)size < sizeof(entry)) {
					ret = PLUS_ERROR_IO;
					goto close;
				}

				offset += size;

				if (entry.slot[0] == '\0')
					continue;

				if (sce_paf_strncmp(entry.titleId, titleId, sizeof(entry.titleId)) == 0) {
					if (file->Seek(entryOffset, SCE_SEEK_SET) < 0)
						goto close;

					sce_paf_memset(&entry, 0, sizeof(entry));
					size = file->Write(&entry, sizeof(entry));
					if (size < 0) {
						ret = size;
						goto close;
					}
					if ((SceUInt32)size < sizeof(entry)) {
						ret = PLUS_ERROR_IO;
						goto close;
					}

					header.count--;
					if (file->Seek(0, SCE_SEEK_SET) < 0)
						goto close;

					size = file->Write(&header, sizeof(header));
					if (size < 0)
						ret = size;
					else if ((SceUInt32)size < sizeof(header))
						ret = PLUS_ERROR_IO;
					goto close;
				}

				i++;
			}

			ret = PLUS_ERROR_NO_ENTRY;
		}
close:
		if (file.get() != NULL)
			file->Close();
	}

	return ret;
}

/* The save data of titleId taken out of the current user's list (0x8102ADBE). */
int plusSdListDelete(const char *titleId)
{
	s_sdListMutex.Lock();

	int ret = plusCheckTitleId(titleId);

	if (ret == 0) {
		char path[512];
		char dir[32];

		sce_paf_memset(path, 0, sizeof(path));
		sce_paf_memset(dir, 0, sizeof(dir));

		ret = sceAppMgrGetUserDirPath(PLUS_USER_DIR_PARTITION, dir, sizeof(dir));
		if (ret >= 0) {
			/* Sony measures the folder once without using it */
			sce_paf_strlen(dir);
			sce_paf_strncat(path, dir, sce_paf_strlen(dir));
			path[sce_paf_strlen(path)] = '/';
			sce_paf_strncat(path, PLUS_LIST_DIR, sce_paf_strlen(PLUS_LIST_DIR));
		}

		if (ret >= 0) {
			path[sce_paf_strlen(path)] = '/';
			sce_paf_strncat(path, PLUS_LIST_AUTO_UPLOAD,
					sce_paf_strlen(PLUS_LIST_AUTO_UPLOAD));
			path[sce_paf_strlen(path)] = '/';
			sce_paf_strncat(path, PLUS_SD_LIST_FILE, sce_paf_strlen(PLUS_SD_LIST_FILE));
			ret = plusSdListFileDelete(titleId, path);
		}
	}

	s_sdListMutex.Unlock();

	return ret;
}

/* The save data of titleId taken out of every user's list: the last user's result (0x8102AF02). */
int plusSdListDeleteAllUsers(const char *titleId)
{
	s_sdListMutex.Lock();

	if (plusCheckTitleId(titleId) == 0) {
		int ret;
		char path[128];

		for (int user = 0; user < PLUS_USERS_MAX; user++) {
			sce_paf_memset(path, 0, sizeof(path));
			sce_paf_snprintf(path, sizeof(path), "%suser/%02d/%s/%s/%s", PLUS_LIST_DEVICE,
					 user, PLUS_LIST_DIR, PLUS_LIST_AUTO_UPLOAD,
					 PLUS_SD_LIST_FILE);
			ret = plusSdListFileDelete(titleId, path);
		}

		s_sdListMutex.Unlock();

		return ret;
	}

	s_sdListMutex.Unlock();

	return PLUS_ERROR_INVALID_ARG;
}
