/* Directory adapters and directory-type predicate. */
#include <stdint.h>
#include <kernel/iofilemgr.h>
#include "../std/string.h"

typedef struct PafDirectoryEntry {
	int32_t kind;
	PafString name;
	uint64_t size;
} PafDirectoryEntry;

int32_t ScePafMisc_A4F553B0(const int32_t *directory,
			    PafDirectoryEntry *entry)
{
	SceIoDirent native;
	int32_t fd;
	int32_t result;

	if (!entry)
		return -2135992063;
	fd = *directory;
	if (fd == -1)
		return -2135992063;
	native.d_stat.st_mode = 0;
	ScePafStdc_75ECC54E(&native.d_stat.st_attr, 0, 0x54);
	ScePafStdc_75ECC54E(native.d_name, 0, 0x108);
	result = sceIoDread(fd, &native);
	if (result < 0)
		return result;
	if (result == 0)
		return -2135992062;
	if (native.d_stat.st_mode & 0x1000)
		entry->kind = 0;
	else if (native.d_stat.st_mode & 0x2000)
		entry->kind = 1;
	else if (native.d_stat.st_mode & 0x4000)
		entry->kind = 2;
	else
		entry->kind = 3;
	PafStringAssign(&entry->name, native.d_name,
			sce_paf_strlen(native.d_name));
	entry->size = native.d_stat.st_size;
	return 0;
}

extern int32_t sceIoMkdir(const char *path, int32_t mode);
extern int32_t sceIoDclose(int32_t fd);
extern int32_t sceIoDopen(const char *path);
extern int32_t sceIoGetstat(const char *path, SceIoStat *stat);

int32_t ScePafMisc_40573275(const char *path)
{
	SceIoStat second;
	SceIoStat first;

	if (!path)
		return -2135992063;
	if (sceIoGetstat(path, &first) != 0)
		return 0;
	if (sceIoGetstat(path, &second) != 0)
		return sceIoRemove(path);
	if ((second.st_mode & SCE_STM_FDIR) != 0)
		return sceIoRmdir(path);
	return sceIoRemove(path);
}

int32_t ScePafMisc_53EAF163(const char *path, int32_t mode)
{
	return sceIoMkdir(path, mode);
}

int32_t ScePafMisc_198A4C81(const char *path)
{
	return sceIoMkdir(path, 6);
}

void ScePafMisc_2E8E9CBB(int32_t *directory)
{
	if (*directory == -1)
		return;
	sceIoDclose(*directory);
	*directory = -1;
}

int32_t ScePafMisc_95F2C107(int32_t *directory, const char *path)
{
	int32_t result = sceIoDopen(path);

	if (result >= 0) {
		*directory = result;
		return 0;
	}
	return result;
}

int32_t ScePafMisc_B002E56C(const char *path)
{
	SceIoStat stat;

	if (sceIoGetstat(path, &stat) != 0)
		return 0;
	return (stat.st_mode & SCE_STM_FDIR) != 0;
}
