/* SceEnumWakeUp: Files kept in memory for MTP (the device icon). */
#include "ew_internal.h"

/* a miss returns the entry one past the end, as Sony does (EwGlobals.cacheMiss) */
static inline __attribute__((always_inline)) MtpCacheEntry *mtpCacheFind(const char *path)
{
	int i;

	for (i = 0; i < MTP_CACHE_ENTRIES; i++)
		if (strncmp(path, ewg.mtp.cache[i].path, sizeof(ewg.mtp.cache[i].path)) == 0)
			break;
	return &ewg.mtp.cache[i];
}

int mtpCacheLoad(const char *path, SceSize size)
{
	MtpCacheEntry *e = mtpCacheFind(path);
	SceUID fd;
	int ret;

	if (e->base == NULL) {
		e->block = sceKernelAllocMemBlockForKernel("MTPDataCacheBuffer", EW_MEMTYPE_FB,
						   (size + 0xfff) & ~0xfff, NULL);
		if (e->block < 0)
			return e->block;
		sceKernelGetMemBlockBase(e->block, &e->base);
		strncpy(e->path, path, strnlen(path, 0xff));
	}
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	ret = sceIoRead(fd, e->base, size);
	sceIoClose(fd);
	e->size = ret;
	return ret;
}

int mtpCacheRead(const char *path, void *dst, SceSize max)
{
	MtpCacheEntry *e = mtpCacheFind(path);

	if (e->base == NULL)
		return MTP_ERROR_NOT_CACHED;
	memcpy(dst, e->base, e->size > max ? max : e->size);
	return e->size > max ? max : e->size;
}
