/* Directory adapters and directory-type predicate. */
#include <stdint.h>
#include <kernel/iofilemgr.h>

extern int32_t sceIoMkdir(const char *path, int32_t mode);
extern int32_t sceIoClose(int32_t fd);
extern int32_t sceIoDopen(const char *path);
extern int32_t sceIoGetstat(const char *path, SceIoStat *stat);

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
	sceIoClose(*directory);
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
