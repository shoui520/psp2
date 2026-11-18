/*
 * PlayStation Plus's automatic save data upload: the applications' entries in each user's
 * list (savedata_plus/auto_upload/up_app_list.dat), whose status the system sets. Sony's
 * gamedata_plus_util (its path in gamedata_plugin: vsh/app/gamedata_plus_util/src), linked
 * into the shell. The file's and every name here are ours (no record names them; 3.20's
 * gamedata_plugin names the family IoUpdatePlusListFile); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <std/string.h>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <rtc/rtc.h>
#include <file/localfile.h>

#include "plus_list.h"

using namespace paf;

/* the folder of every user's lists, and the list's place in it */
#define PLUS_LIST_DEVICE	"ux0:"
#define PLUS_LIST_DIR		"savedata_plus"
#define PLUS_LIST_AUTO_UPLOAD	"auto_upload"
#define PLUS_APP_LIST_FILE	"up_app_list.dat"

/* the partition sceAppMgrGetUserDirPath gives the user's folder of, and the users there are */
#define PLUS_USER_DIR_PARTITION	2
#define PLUS_USERS_MAX		64

static thread::RMutex s_appListMutex("SceGameDataAPPList", 0);

/* whether titleId is a title ID that fits an entry: 0 or PLUS_ERROR_INVALID_ARG */
static inline int plusCheckTitleId(const char *titleId)
{
	if (titleId == NULL)
		return PLUS_ERROR_INVALID_ARG;

	if (titleId[0] == '\0')
		return PLUS_ERROR_INVALID_ARG;

	for (int i = 1; i < PLUS_TITLE_ID_SIZE; i++)
		if (titleId[i] == '\0')
			return 0;

	return PLUS_ERROR_INVALID_ARG;
}

/*
 * The entry of titleId in the list at path given status and the time: 0, PLUS_ERROR_NO_ENTRY
 * without one, or an error (0x8102A56C).
 */
static __attribute__((noinline)) int plusListFileSetStatus(const char *titleId, SceUInt8 status,
							    const char *path)
{
	common::SharedPtr<File> file;
	LocalFileStat stat;
	int ret = LocalFile::Getstat(path, &stat);

	if (ret == 0) {
		file = LocalFile::Open(path, SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

		if (ret != 0) {
			ret = PLUS_ERROR_IO;
		} else {
			PlusListHeader header;
			PlusListEntry entry;

			sce_paf_memset(&header, 0, sizeof(header));
			sce_paf_memset(&entry, 0, sizeof(entry));

			int size = file->Read(&header, sizeof(header));

			if (size < 0) {
				ret = size;
			} else if ((SceUInt32)size < sizeof(header)) {
				ret = PLUS_ERROR_IO;
			} else {
				SceInt64 offset = size;

				if (sce_paf_strncmp(header.magic, PLUS_LIST_MAGIC, 4) != 0) {
					ret = PLUS_ERROR_BAD_LIST;
				} else {
					SceUInt32 i;

					for (i = 0; i < header.count; i++) {
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
						if (sce_paf_strncmp(entry.titleId, titleId,
								    sizeof(entry.titleId) - 1) == 0)
							break;
						offset += size;
					}

					if (i >= header.count) {
						ret = PLUS_ERROR_NO_ENTRY;
					} else {
						rtc::Tick tick;

						ret = rtc::GetCurrentTick(&tick);
						if (ret == 0) {
							sce_paf_memset(&entry, 0, sizeof(entry));
							sce_paf_strncpy(entry.titleId, titleId,
									sizeof(entry.titleId) - 1);
							entry.status = status;
							entry.time = tick;

							if (file->Seek(offset, SCE_SEEK_SET) >= 0) {
								size = file->Write(&entry, sizeof(entry));
								if (size < 0)
									ret = size;
								else if ((SceUInt32)size < sizeof(entry))
									ret = PLUS_ERROR_IO;
							}
						}
					}
				}
			}
		}
close:
		if (file.get() != NULL)
			file->Close();
	}

	return ret;
}

/* The status of titleId in the current user's list set (0x8102A860). */
int plusAppListSetStatus(const char *titleId, SceUInt8 status)
{
	s_appListMutex.Lock();

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
			sce_paf_strncat(path, PLUS_APP_LIST_FILE, sce_paf_strlen(PLUS_APP_LIST_FILE));
			ret = plusListFileSetStatus(titleId, status, path);
		}
	}

	s_appListMutex.Unlock();

	return ret;
}

/* The status of titleId set in every user's list: the last user's result (0x8102A9AC). */
int plusAppListSetStatusAllUsers(const char *titleId, SceUInt8 status)
{
	s_appListMutex.Lock();

	if (plusCheckTitleId(titleId) == 0) {
		int ret;
		char path[128];

		for (int user = 0; user < PLUS_USERS_MAX; user++) {
			sce_paf_memset(path, 0, sizeof(path));
			sce_paf_snprintf(path, sizeof(path), "%suser/%02d/%s/%s/%s", PLUS_LIST_DEVICE,
					 user, PLUS_LIST_DIR, PLUS_LIST_AUTO_UPLOAD,
					 PLUS_APP_LIST_FILE);
			ret = plusListFileSetStatus(titleId, status, path);
		}

		s_appListMutex.Unlock();

		return ret;
	}

	s_appListMutex.Unlock();

	return PLUS_ERROR_INVALID_ARG;
}
