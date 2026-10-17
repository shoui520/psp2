/*
 * regmgr_wrapreg.c: the registry manager's wrapper of the registry files. Every
 * call is skipped in the volatile mode, and an error of the registry files
 * (0x800cXXXX) is given as the registry manager's (0x800dXXXX), and kept as the
 * last error, with the line it was met at.
 */

#include <sysmem/debugled.h>
#include "regmgr_internal.h"

#define REGMGR_REG_ERROR_FACILITY	0x800c0000	/* names unknown */
#define REGMGR_ERROR_FACILITY		0x800d0000

/* a key whose value is also kept in the NVS, or set to the GPI (names unknown) */
typedef struct RegMgrNvsKey {
	int categoryNo;
	int nameNo;
	int areaNo;
	int gpi;		/* the value is the GPI, not in the NVS */
} RegMgrNvsKey;

/* an area of the NVS (names unknown) */
typedef struct RegMgrNvsArea {
	SceUInt32 offset;
	SceSize size;
} RegMgrNvsArea;

RegMgrRegistryParam g_regMgrRegistryParam = {	/* names unknown */
	.mode = 1,
	.size = 4,
	.blockSize = 1,
};
int g_regMgrRegistryHandle = -1;
int g_regMgrLastError;
int g_regMgrLastErrorLine;

static const char g_regMgrNvsCategory[][0x100] = {
	"/CONFIG/SYSTEM",
	"/DEVENV/TOOL",
};
static const char g_regMgrNvsName[][0x1c] = {
	"language",
	"gpi_switch",
};
static const RegMgrNvsKey g_regMgrNvsKey[] = {
	{ 0, 0, 0, 0 },
	{ 1, 1, 0, 1 },
};
static const RegMgrNvsArea g_regMgrNvsArea[] = {
	{ 0x4a4, 4 },
	{ 0x500, 1 },
	{ 0x482, 1 },
	{ 0x4e0, 0x20 },
	{ 0x483, 1 },
	{ 0x486, 1 },
};

/* the registry manager's error for an error of the registry files */
static inline __attribute__((always_inline)) int regMgrWrapRegConvError(int ret)
{
	if (ret != 0 && (ret & 0xffff0000) == REGMGR_REG_ERROR_FACILITY)
		ret = (ret & 0xffff) + REGMGR_ERROR_FACILITY;
	return ret;
}

int regMgrWrapRegGetRegistryHandle(void)	/* name unknown */
{
	return g_regMgrRegistryHandle;
}

int regMgrWrapRegInit(void)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || (ret = regInit()) == 0)
		ret = 0;
	else
		ret = regMgrWrapRegConvError(ret);
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 244;	/* __LINE__ of Sony's regmgr_wrapreg.c */
	return ret;
}

static inline __attribute__((always_inline)) int regMgrWrapRegOpen(int mode, int openMode)
{
	int ret;

	if (regMgrGetVolatileMode() == 1)
		return 0;
	if (g_regMgrRegistryHandle != -1)
		return SCE_REGMGR_ERROR_ALREADY_OPENED;
	switch (openMode) {
	case 2:
		g_regMgrRegistryParam.mode = 0x81;
		break;
	case 3:
		g_regMgrRegistryParam.mode = 2;
		break;
	default:
		g_regMgrRegistryParam.mode = 1;
		break;
	}
	ret = regOpenRegistry(&g_regMgrRegistryParam, mode, &g_regMgrRegistryHandle);
	if (ret == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 310;
	g_regMgrRegistryHandle = -1;
	return ret;
}

int regMgrWrapRegOpenRegistry(int mode, int openMode)
{
	return regMgrWrapRegOpen(mode, openMode);
}

static inline __attribute__((always_inline)) int regMgrWrapRegClose(void)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || g_regMgrRegistryHandle == -1)
		return 0;
	ret = regMgrWrapRegConvError(regCloseRegistry(g_regMgrRegistryHandle));
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 344;
	g_regMgrRegistryHandle = -1;
	return ret;
}

int regMgrWrapRegCloseRegistry(void)
{
	return regMgrWrapRegClose();
}

int regMgrWrapRegReopenRegistry(int openMode)
{
	regMgrWrapRegClose();
	return regMgrWrapRegOpen(2, openMode);
}

int regMgrWrapRegRemoveRegistry(void)
{
	int ret;

	if (regMgrGetVolatileMode() == 1)
		return 0;
	ret = regRemoveRegistry(&g_regMgrRegistryParam);
	if (ret == 0) {
		g_regMgrRegistryHandle = -1;
		return 0;
	}
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 401;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegBackupRegistry(void)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || (ret = regBackupRegistry()) == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 429;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegRestoreRegistry(void)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || (ret = regRestoreRegistry()) == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 456;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegExportRegistry(const char *path)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || (ret = regExportRegistry(path)) == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 483;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegImportRegistry(const char *path)
{
	int ret;

	if (regMgrGetVolatileMode() == 1 || (ret = regImportRegistry(path)) == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 510;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegOpenCategory(const char *category, int mode, int *handle)
{
	char path[REGMGR_WORD_SIZE];
	SceSize len;
	int ret;

	if (regMgrGetVolatileMode() == 1) {
		ret = 0;
		goto out;
	}
	if (category == NULL)
		goto bad_name;
	len = strnlen(category, sizeof(path)) - 1;
	if (len >= 0xe5)
		goto bad_name;
	if (category[len] == '/') {
		if (len != 0)
			category = memcpy(path, category, len);
		else
			category = path;
		path[len] = '\0';
	}
	ret = regMgrWrapRegConvError(regOpenCategory(g_regMgrRegistryHandle, category, mode, handle));
	goto out;
bad_name:
	ret = SCE_REGMGR_ERROR_PATH_TOO_LONG;
out:
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 572;
	return ret;
}

int regMgrWrapRegCloseCategory(int *handle)
{
	int ret;

	if (handle == NULL || *handle == 0 || regMgrGetVolatileMode() == 1)
		return 0;
	ret = regCloseCategory(*handle);
	*handle = 0;
	if (ret == 0)
		return 0;
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastErrorLine = 610;
	g_regMgrLastError = ret;
	return ret;
}

int regMgrWrapRegRemoveCategory(const char *category)
{
	int ret;

	if (regMgrGetVolatileMode() == 1)
		return 0;
	ret = regMgrWrapRegConvError(regRemoveCategory(g_regMgrRegistryHandle, category, 0));
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 633;
	return ret;
}

int regMgrWrapRegDeleteKey(int handle, const char *category, const char *name)
{
	int ret;

	(void)category;
	if (regMgrGetVolatileMode() == 1)
		return 0;
	ret = regMgrWrapRegConvError(regDeleteKey(handle, name));
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 662;
	return ret;
}

int regMgrWrapRegCreateKey(int handle, const char *category, const char *name, int type, int size)
{
	int regType, ret;

	(void)category;
	switch (type) {
	case REGMGR_TYPE_INT:
		regType = 2;	/* the type in the registry files (names unknown) */
		break;
	case REGMGR_TYPE_STR:
		regType = 3;
		break;
	case REGMGR_TYPE_BIN:
		regType = 4;
		break;
	default:
		return SCE_REGMGR_ERROR_BAD_TYPE;
	}
	if (regMgrGetVolatileMode() == 1)
		return 0;
	ret = regMgrWrapRegConvError(regCreateKey(handle, name, regType, size));
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 710;
	return ret;
}

/* whether a key of a category is the NVS key */
static inline __attribute__((always_inline)) int regMgrIsNvsKey(const RegMgrNvsKey *key, const char *category,
					  const char *name)
{
	SceSize len, last;

	if (strncmp(name, g_regMgrNvsName[key->nameNo], sizeof(g_regMgrNvsName[0])) != 0)
		return 0;
	len = strnlen(category, sizeof(g_regMgrNvsCategory[0]));
	last = len - 1;
	if (((const SceChar8 *)category)[last] == '/')
		len = last;
	return ((const SceChar8 *)g_regMgrNvsCategory[key->categoryNo])[len] == '\0' &&
	       memcmp(category, g_regMgrNvsCategory[key->categoryNo], len) == 0;
}

int regMgrWrapRegSetKey(int handle, const char *category, const char *name, const void *buf,
			SceSize size)
{
	const RegMgrNvsKey *key;
	SceSize areaSize;
	int ret = 0;
	int i, n, err;

	n = regMgrIsTool() ? 1 : 2;
	for (i = 0, key = g_regMgrNvsKey; i < n; ) {
		i++;
		if (regMgrIsNvsKey(key, category, name)) {
			areaSize = g_regMgrNvsArea[key->areaNo].size;
			ret++;
			if (size > areaSize) {
				ret = SCE_REGMGR_ERROR_NVS_SIZE;
				goto out;
			}
			if (key->gpi) {
				key++;
				SceDebugLedForDriver_51C5325A(*(const int *)buf);
				continue;
			}
			err = ksceSblSsSetNvsData(key->areaNo, buf, areaSize);
			if (err != 0) {
				ret = err;
				goto out;
			}
		}
		key++;
	}
out:
	if (regMgrGetVolatileMode() != 1)
		ret = regSetKey(handle, name, buf, size);
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 755;
	return ret;
}

int regMgrWrapRegGetKey(int handle, const char *category, const char *name, void *buf, SceSize size)
{
	int ret;

	(void)category;
	if (regMgrGetVolatileMode() == 1) {
		if (buf != NULL && (int)size > 0)
			memset(buf, 0, size);
		ret = 0;
	} else {
		ret = regMgrWrapRegConvError(regGetKey(handle, name, buf, size));
	}
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = 790;
	return ret;
}

/* get the value of a key kept in the NVS; gives how many keys it found */
int regMgrWrapGetNVSData(const char *category, const char *name, void *buf, SceSize size)
{
	const RegMgrNvsKey *key;
	SceSize areaSize;
	int ret = 0;
	int i, n, areaNo, err;

	n = regMgrIsTool() ? 1 : 2;
	for (i = 0, key = g_regMgrNvsKey; i < n; i++, key++) {
		areaNo = key->areaNo;
		if (!regMgrIsNvsKey(key, category, name))
			continue;
		ret++;
		areaSize = g_regMgrNvsArea[areaNo].size;
		if (size > areaSize)
			return SCE_REGMGR_ERROR_NVS_SIZE;
		if (key->gpi)
			continue;
		err = ksceSblSsGetNvsData(areaNo, buf, areaSize);
		if (err != 0)
			return err;
	}
	return ret;
}

/* keep an error met at a line, as the registry manager's (name unknown) */
int regMgrWrapRegSetError(int ret, int line)
{
	ret = regMgrWrapRegConvError(ret);
	g_regMgrLastError = ret;
	g_regMgrLastErrorLine = line;
	return ret;
}

void regMgrSetLastError(int error)
{
	g_regMgrLastError = error;
}

int regMgrGetLastError(void)
{
	return g_regMgrLastError;
}
