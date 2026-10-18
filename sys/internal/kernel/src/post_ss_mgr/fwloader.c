/*
 * fwloader.c: SceSblFwLoader, loading the firmware images of other
 * processors. A path is set (and the loader locked) first; the image is
 * then loaded from external storage or host0: when there is one, else from
 * the path itself.
 */

#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysroot.h>
#include "post_ss_mgr_internal.h"
#include <sysmem/qafmgr.h>

#define SCE_SBL_ERROR_FWLOADER_INVALID_ARGUMENT	0x800f0e16	/* names unknown */
#define SCE_SBL_ERROR_FWLOADER_NOT_LOCKED	0x800f0e29

#define FWLOADER_HEAP		0x1000b		/* name unknown: the kernel heap */
#define FWLOADER_PATH_MAX	0x100		/* names unknown */
#define FWLOADER_BUF_SIZE	0x200
#define FWLOADER_DIPSW_HOST0	0xfc

#define FWLOADER_PREFIX_EXTERNAL	"sd0:"
#define FWLOADER_PREFIX_HOST0		"host0:module/"

/* sceSblFwLoaderSetPath to sceSblFwLoaderUnlock (names unknown) */
#define FWLOADER_STATE_LOCKED	1
#define FWLOADER_STATE_UNLOCKED	2

/* 4.00 is built with _FORTIFY_SOURCE */
#if FW_VERSION >= 0x04000000
#define fwLoaderStrncpyChk(dst, src, n, size)	__builtin___strncpy_chk(dst, src, n, size)
#else
#define fwLoaderStrncpyChk(dst, src, n, size)	strncpy(dst, src, n)
#endif

char *g_fwLoaderBuf;			/* names unknown */
char *g_fwLoaderOverridePath;
SceSize g_fwLoaderOverridePathSize;
char *g_fwLoaderPath;
SceSize g_fwLoaderPathSize;
SceKernelFastMutex g_fwLoaderMutex;
int g_fwLoaderState;

int fwLoaderClearPath(void)	/* name unknown */
{
	if (g_fwLoaderBuf != NULL)
		sceKernelFreeHeapMemory(FWLOADER_HEAP, g_fwLoaderBuf);
	g_fwLoaderBuf = NULL;
	g_fwLoaderOverridePath = NULL;
	g_fwLoaderOverridePathSize = 0;
	g_fwLoaderPath = NULL;
	g_fwLoaderPathSize = 0;
	return 0;
}

int sceSblFwLoaderInit(void)
{
	sceKernelInitializeFastMutex(&g_fwLoaderMutex, "SceSblFwLoader", 0, NULL);
	fwLoaderClearPath();
	return 0;
}

/* set the path to load from and lock the loader until sceSblFwLoaderUnlock */
int sceSblFwLoaderSetPath(const char *path, int flags)
{
	const char *prefix, *p, *device, *dir, *name;
	SceSize len, prefixLen, nameLen;
	char *buf;
	int ret;

	if (path == NULL || flags != 0)
		return SCE_SBL_ERROR_FWLOADER_INVALID_ARGUMENT;
	ret = lockFastMutex(&g_fwLoaderMutex);
	if (ret < 0)
		return ret;
	fwLoaderClearPath();
	len = strnlen(path, FWLOADER_PATH_MAX);
	if (len >= FWLOADER_PATH_MAX)
		goto fail;
	g_fwLoaderPathSize = len + 1;

	prefix = FWLOADER_PREFIX_EXTERNAL;
	if (!SceSysrootForKernel_55392965()) {
		prefix = NULL;
		if (SceQafMgrForDriver_082A4FC2()) {
			prefix = FWLOADER_PREFIX_HOST0;
			if (!SceDipswForDriver_A98FC2FD(FWLOADER_DIPSW_HOST0))
				prefix = NULL;
		}
	}

	/* the device ("os0:") and the directory of the path */
	device = NULL;
	dir = NULL;
	for (p = path; *p != '\0'; p++) {
		if (*p == ':')
			device = p;
		else if (*p == '/')
			dir = p;
	}
	if (device == NULL || dir == NULL)
		goto fail;
	name = device + 1;

	if (prefix == NULL) {
		g_fwLoaderOverridePathSize = 0;
		prefixLen = 0;
		nameLen = 0;
		g_fwLoaderOverridePath = NULL;
	} else {
		prefixLen = strnlen(prefix, FWLOADER_PATH_MAX);
		if (prefixLen >= FWLOADER_PATH_MAX)
			goto fail;
		if (strncmp(prefix, FWLOADER_PREFIX_EXTERNAL, 5) == 0) {
			nameLen = strnlen(name, FWLOADER_PATH_MAX);
			if (nameLen >= FWLOADER_PATH_MAX)
				goto fail;
		} else if (strncmp(prefix, FWLOADER_PREFIX_HOST0, 0xe) == 0) {
			name = dir + 1;
			nameLen = strnlen(name, FWLOADER_PATH_MAX);
			if (nameLen >= FWLOADER_PATH_MAX)
				goto fail;
		} else {
			SCE_DBG_ASSERT(0, 410);
			nameLen = 0;
		}
		g_fwLoaderOverridePathSize = prefixLen + 1 + nameLen;
	}
	if (g_fwLoaderPathSize + g_fwLoaderOverridePathSize > FWLOADER_BUF_SIZE)
		goto fail;

	buf = sceKernelAllocHeapMemory(FWLOADER_HEAP, FWLOADER_BUF_SIZE);
	g_fwLoaderBuf = buf;
	if (prefix != NULL) {
		g_fwLoaderOverridePath = buf;
		fwLoaderStrncpyChk(buf, prefix, prefixLen, FWLOADER_BUF_SIZE);
		strncpy(buf + prefixLen, name, nameLen);
		buf[prefixLen + nameLen] = '\0';
	}
	buf += g_fwLoaderOverridePathSize;
	g_fwLoaderPath = buf;
	strncpy(buf, path, len);
	buf[len] = '\0';
	g_fwLoaderState = FWLOADER_STATE_LOCKED;
	return 0;

fail:
	sceKernelUnlockFastMutex(&g_fwLoaderMutex);
	return SCE_SBL_ERROR_FWLOADER_INVALID_ARGUMENT;
}

int sceSblFwLoaderClose(void)
{
	if (g_fwLoaderState != FWLOADER_STATE_LOCKED)
		return SCE_SBL_ERROR_FWLOADER_NOT_LOCKED;
	fwLoaderClearPath();
	g_fwLoaderState = FWLOADER_STATE_UNLOCKED;
	sceKernelUnlockFastMutex(&g_fwLoaderMutex);
	return 0;
}

/* load the image at the path set into dst */
int sceSblFwLoaderLoad(int type, void *dst, SceSize size, SceSize *loaded)
{
	char *override = g_fwLoaderOverridePath;
	char *path;
	int ret, clear;

	if (type != 1 || dst == NULL || loaded == NULL) {
		ret = SCE_SBL_ERROR_FWLOADER_INVALID_ARGUMENT;
	} else if (g_fwLoaderState != FWLOADER_STATE_LOCKED) {
		ret = SCE_SBL_ERROR_FWLOADER_NOT_LOCKED;
	} else {
		if (override != NULL) {
			ret = sceKernelLoadPtLoadSegForFwloader(override, 1, dst, size, 0, loaded);
			if (ret >= 0)
				goto out;
			SCE_DBG_PRINTF(0, 296, "load %s : failed (0x%08x) (ignored)\n", override, ret);
		}
		path = g_fwLoaderPath;
		ret = sceKernelLoadPtLoadSegForFwloader(path, 1, dst, size, 0, loaded);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 308, "sceKernelLoadStartModule (%s) fail (ret=0x%08x)\n",
				       g_fwLoaderPath, ret);
		else
			SCE_DBG_PRINTF(0, 305, "Loading %s (dst=%p, size=0x%08x): OK\n", path, dst,
				       *loaded);
	}
out:
	clear = fwLoaderClearPath();
	if (clear < 0) {
		SCE_DBG_ASSERT(0, 319);
		return clear;
	}
	return ret;
}
