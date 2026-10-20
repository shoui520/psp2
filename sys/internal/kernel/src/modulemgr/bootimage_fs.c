/*
 * bootimage_fs.c - the files of the boot image module (bootimage.skprx)
 */

#include "modulemgr_internal.h"

BootimageFS *g_bootimage;

/* name unknown */
SceSize BootimageGetSize(void)
{
	return g_bootimage->table.entry[g_bootimage->current].size;
}

/* name unknown */
void *BootimageGetData(void)
{
	return g_bootimage->table.entry[g_bootimage->current].data;
}

/* name unknown */
SceUID BootimageOpen(const char *path)
{
	BootimageFS *fs = g_bootimage;
	BootimageEntry *ent;
	int i, n;

	if (fs == NULL || (n = fs->table.count) <= 0)
		return 0x80010002;	/* ENOENT */
	ent = fs->table.entry;
	for (i = 0; i < n; i++) {
		if (strncmp(path, ent[i].name, 0xff) == 0) {
			fs->current = i;
			return BOOTIMAGE_FD;
		}
	}
	return 0x80010002;
}

int sceLoadcoreIoClose(SceUID fd)
{
	BootimageFS *fs = g_bootimage;

	SCE_DBG_ASSERT_LEVEL(0, fs != NULL, 74);
	fs->current = -1;
	return 0;
}

int sceLoadcoreIoReadFrom(void *buf, SceSize size, SceOff offset)
{
	BootimageFS *fs = g_bootimage;
	BootimageEntry *ent;
	char *data;
	SceSize entSize;

	SCE_DBG_ASSERT_LEVEL(0, fs != NULL, 84);
	ent = &fs->table.entry[fs->current];
	data = ent->data;
	entSize = ent->size;
	if (entSize < (SceUInt32)offset + size)
		return 0;
	memcpy(buf, data + (SceUInt32)offset, size);
	return size;
}

int sceKernelMountBootimageFS(const char *path)
{
	BootimageFS *fs;
	SceUID modid;
#if FW_DEBUG
	int i;
#endif

	if (g_bootimage != NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_ALREADY_LOADED;
	modid = SceModulemgrForDriver_189BFBBB(path, 0, NULL, 0x100, NULL, NULL);
	if (modid < 0)
		return modid;
	g_bootimage = ModulemgrKernelAlloc(sizeof(*fs));
	SCE_DBG_ASSERT_LEVEL(0, g_bootimage != NULL, 125);
	fs = g_bootimage;
	fs->modid = modid;
	fs->current = -1;
	fs->table.count = sceKernelBootimageEntries.count;
	fs->table.entry = sceKernelBootimageEntries.entry;
	ModulemgrReferModule(modid);
#if FW_DEBUG
	for (i = 0; i < fs->table.count; i++)
		SCE_DBG_PRINTF_LEVEL(2, 0, 136, " %3d: 0x%08x (%8d byte) %s\n", i,
				     fs->table.entry[i].data, fs->table.entry[i].size,
				     fs->table.entry[i].name);
#endif
	ModulemgrReleaseModule(fs->modid);
	return 0;
}

int sceKernelUmountBootimageFS(void)
{
	int ret;

	if (g_bootimage == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_STOP_FAIL;
	ret = SceModulemgrForDriver_03B30B7E(g_bootimage->modid, 0, NULL, 0, NULL, NULL);
	SCE_DBG_ASSERT_LEVEL(0, ret == 0, 154);
	(void)ret;
	SceSysmemForKernel_4233C16D(g_bootimage);
	g_bootimage = NULL;
	return 0;
}
