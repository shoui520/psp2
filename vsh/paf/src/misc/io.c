/* Directory adapters and directory-type predicate. */
#include <stdint.h>
#include <kernel/iofilemgr.h>

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
