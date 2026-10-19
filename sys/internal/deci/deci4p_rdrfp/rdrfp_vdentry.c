/*
 * SceDeci4pRDrfp virtual directory entries (Sony's rdrfp_vdentry.c): the
 * directories the host sees above the titles' data, e.g. "Memory Card/game
 * titles/<title>/app0", each a name with a list of children.
 */
#include <iofilemgr/iofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include "rdrfp.h"

#define RDRFP_VDENTRY_NAME_MAX		0xff
#define RDRFP_MOUNT_POINT_MAX		0x10

/* the directory flags of a title's app0 and savedata0 */
#define RDRFP_VDENTRY_APP		3
#define RDRFP_VDENTRY_SAVEDATA		5

static int compareVDEntryName(void *data, const void *name)
{
	return strncmp(((RdrfpVDEntry *)data)->name, name, RDRFP_VDENTRY_NAME_MAX);
}

static inline RdrfpVDEntry *newVDEntry(const char *name)
{
	RdrfpVDEntry *entry = rdrfpAlloc(sizeof(*entry));

	if (entry == NULL)
		return NULL;
	memset(entry, 0, sizeof(*entry));
	entry->nameSize = strnlen(name, RDRFP_VDENTRY_NAME_MAX) + 1;
	entry->name = rdrfpAlloc(entry->nameSize);
	if (entry->name == NULL) {
		rdrfpFree(entry);
		return NULL;
	}
	memcpy(entry->name, name, entry->nameSize);
	rdrfpInitList(&entry->children);
	return entry;
}

RdrfpVDEntry *rdrfpCreateVDEntry(const char *name)
{
	return newVDEntry(name);
}

void rdrfpDeleteVDEntry(RdrfpVDEntry *entry)
{
	SCE_DBG_ASSERT_LEVEL(0, entry != NULL, 81);
	rdrfpDeleteList(&entry->children);
	if (entry->name) {
		rdrfpFree(entry->name);
		entry->name = NULL;
	}
	if (entry->mountPoint) {
		rdrfpFree(entry->mountPoint);
		entry->mountPoint = NULL;
	}
	rdrfpFree(entry);
}

RdrfpVDEntry *rdrfpAddVDEntry(RdrfpVDEntry *parent, const char *name)
{
	RdrfpVDEntry *entry = newVDEntry(name);

	if (entry == NULL)
		return NULL;
	rdrfpAddToList(&parent->children, entry);
	entry->parent = parent;
	return entry;
}

RdrfpVDEntry *rdrfpFindVDEntry(RdrfpVDEntry *parent, const char *name)
{
	RdrfpListElem *elem = rdrfpFindListElem(compareVDEntryName, &parent->children, name);

	return elem ? elem->data : NULL;
}

int rdrfpVDEntryDopen(RdrfpVDEntry *entry)
{
	(void)entry;
	return 0;
}

int rdrfpVDEntryDclose(RdrfpVDEntry *entry)
{
	(void)entry;
	return 0;
}

/* a virtual directory: readable and writable, dated 2011-01-01 */
static inline void setVDEntryStat(SceIoStat *stat)
{
	stat->st_ctime.year = 2011;
	stat->st_ctime.month = 1;
	stat->st_ctime.day = 1;
	stat->st_ctime.hour = 0;
	stat->st_ctime.minute = 0;
	stat->st_ctime.second = 0;
	stat->st_ctime.microsecond = 0;
	stat->st_mode = SCE_STM_FDIR | SCE_STM_RWU;
	stat->st_attr = 0;
	stat->st_size = 0;
	stat->st_atime = stat->st_ctime;
	stat->st_mtime = stat->st_ctime;
}

void rdrfpVDEntryGetstat(RdrfpVDEntry *entry, SceIoStat *stat)
{
	(void)entry;
	setVDEntryStat(stat);
}

/* the index'th child; 0 past the last */
int rdrfpVDEntryDread(RdrfpVDEntry *entry, int index, SceIoDirent *dirent)
{
	RdrfpListElem *elem = rdrfpGetListElem(&entry->children, index);
	RdrfpVDEntry *child;
	SceSize len;

	if (elem == NULL)
		return 0;
	child = elem->data;
	setVDEntryStat(&dirent->d_stat);
	len = child->nameSize <= 0x100 ? child->nameSize - 1 : 0xff;
	memcpy(dirent->d_name, child->name, len);
	dirent->d_name[len] = '\0';
	dirent->d_private = NULL;
	return 1;
}

/* a title of ux0:app has its app0 */
int rdrfpUpdateVDEntryOfApp(RdrfpVDEntry *title)
{
	RdrfpVDEntry *entry = rdrfpFindVDEntry(title, "app0");

	if (entry == NULL) {
		entry = rdrfpAddVDEntry(title, "app0");
		if (entry == NULL) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 253, "Couldn't create child entry \"%s\"\n", "app0");
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		entry->flags |= RDRFP_VDENTRY_APP;
	}
	entry->visited = 1;
	return 0;
}

/* a title with save data has its savedata0 */
int rdrfpUpdateVDEntryOfSavedata(RdrfpVDEntry *title, const char *path)
{
	RdrfpVDEntry *entry = rdrfpFindVDEntry(title, "savedata0");
	SceIoStat stat;
	int ret;

	memset(&stat, 0, sizeof(stat));
	ret = sceIoGetstat(path, &stat);
	if (entry == NULL) {
		if (ret == (int)SCE_ERROR_ERRNO_ENOENT)
			return 0;
		if (ret != 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 307, "unexpected error 0x%x\n", ret);
			return 0;
		}
		entry = rdrfpAddVDEntry(title, "savedata0");
		if (entry == NULL) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 291, "Couldn't create child entry \"%s\"\n", "savedata0");
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		entry->visited = 1;
		entry->flags |= RDRFP_VDENTRY_SAVEDATA;
		return 0;
	}
	if (ret == (int)SCE_ERROR_ERRNO_ENOENT)
		return 0;
	if (ret != 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 318, "unexpected error 0x%x\n", ret);
		return 0;
	}
	entry->visited = 1;
	return 0;
}

/* the real path of a path under a mounted entry */
int rdrfpVDEntryMakePath(RdrfpVDEntry *entry, const char *rest, char **path)
{
	*path = rdrfpAlloc(0x400);
	if (*path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	snprintf(*path, 0x400, "%s%s", entry->mountPoint, rest);
	return 0;
}

int rdrfpVDEntrySetMountPoint(RdrfpVDEntry *entry, const char *mountPoint)
{
	SceSize len = strnlen(mountPoint, RDRFP_MOUNT_POINT_MAX);

	entry->mountPoint = rdrfpAlloc(len + 1);
	if (entry->mountPoint == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memcpy(entry->mountPoint, mountPoint, len + 1);
	return 0;
}

void rdrfpVDEntryClearMountPoint(RdrfpVDEntry *entry)
{
	rdrfpFree(entry->mountPoint);
	entry->mountPoint = NULL;
}

int rdrfpVDEntryIsMounted(RdrfpVDEntry *entry)
{
	return entry->mountPoint != NULL;
}
