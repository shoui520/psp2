/*
 * regmgr_debugger.c: the debugger's access to the registry: keys by their full
 * path, and the tool definition files.
 */

#include <iofilemgr/iofilemgr.h>
#include "regmgr_internal.h"

#define SCE_REGMGR_ERROR_RELEASE_CHECK_MODE	0x800d001e	/* names unknown */
#define SCE_REGMGR_ERROR_VOLATILE_MODE		0x800d001f
#define SCE_REGMGR_ERROR_FILE			0x800d0023

#define REGMGR_DBG_COPY_SIZE	0x1400	/* names unknown */

/* how a tool definition file is read: in pieces, each given to a function
 * (names unknown) */
typedef struct RegMgrToolDefReader {
	SceSize size;			/* sizeof(RegMgrToolDefReader) */
	int (*func)(void);		/* for each piece read */
	void *buf;
	SceSize bufSize;
	SceSize readSize;		/* of the piece in buf */
	int reserved[3];
} RegMgrToolDefReader;

/* a file to copy a tool definition file to (names unknown) */
typedef struct RegMgrToolDefCopy {
	SceSize size;			/* sizeof(RegMgrToolDefCopy) */
	const char *path;		/* on host0: */
	int mode;
	int reserved;
} RegMgrToolDefCopy;

RegMgrKeyInfo g_regMgrDbgKey;			/* names unknown */
char g_regMgrDbgBuf[REGMGR_DBG_COPY_SIZE];
char g_regMgrDbgCategory[REGMGR_WORD_SIZE];
char g_regMgrDbgName[REGMGR_CATEGORY_NAME];

/* read a tool definition file; at its end, keep its serial (name unknown) */
int regMgrDbgReadToolDef(int eventLog, RegMgrToolDefReader *reader)
{
	SceUID fd;
	int ret, n, serial;

	if (reader == NULL || reader->size != sizeof(*reader) || reader->func == NULL ||
	    reader->buf == NULL || reader->bufSize == 0)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	fd = sceIoOpen(regMgrGetToolDefPath(eventLog), SCE_O_RDONLY, 0);
	if (fd < 0)
		return SCE_REGMGR_ERROR_FILE;
	for (;;) {
		n = sceIoRead(fd, reader->buf, reader->bufSize);
		if (n <= 0)
			break;
		reader->readSize = n;
		ret = reader->func();
		if (ret != 0)
			goto out;
	}
	if (n != 0) {
		ret = SCE_REGMGR_ERROR_FILE;
		goto out;
	}
	serial = regMgrGetToolDefSerial(eventLog);
	ret = 0;
	if (serial != 0) {
		regMgrClearStatus();
		ret = regMgrSetKeyInt("/DEVENV/HOST", eventLog ? "ev_serial" : "ui_serial", serial);
		if (ret < 0)
			ret = 0;
		regMgrSetStatus(REGMGR_STATUS_NOTIFY);
	}
out:
	sceIoClose(fd);
	return ret;
}

/* take a key given by its full path: its category and name (name unknown) */
int regMgrDbgSplitKey(const RegMgrKeyInfo *info)
{
	const char *path;
	int len, i;

	if (info == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_REGMGR_ERROR_BAD_KEYS_INFO;
	g_regMgrDbgKey = *info;
	path = info->name;
	if (path == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	len = strnlen(path, sizeof(g_regMgrDbgCategory) - 1);
	if ((SceUInt32)(len - 1) > sizeof(g_regMgrDbgCategory) - 3)
		return SCE_REGMGR_ERROR_BAD_CATEGORY;
	memcpy(g_regMgrDbgCategory, path, len + 1);
	for (i = len; g_regMgrDbgCategory[i] != '/'; i--) {
		if (i == 0)
			return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
	g_regMgrDbgCategory[i] = '\0';
	if ((SceUInt32)(len - i - 2) >= REGMGR_CATEGORY_NAME - 3)
		return SCE_REGMGR_ERROR_BAD_NAME;
	strncpy(g_regMgrDbgName, &g_regMgrDbgCategory[i + 1], len - i);
	g_regMgrDbgKey.name = g_regMgrDbgName;
	return 0;
}

/* whether the debugger may use the registry */
static inline __attribute__((always_inline)) int regMgrDbgCheck(void)
{
	if (regMgrIsTool() != 0)
		return SCE_REGMGR_ERROR_RELEASE_CHECK_MODE;
	if (regMgrGetVolatileMode() == 1)
		return SCE_REGMGR_ERROR_VOLATILE_MODE;
	return 0;
}

/* set a key given by its full path */
int SceRegMgrForDebugger_810D2F09(const RegMgrKeyInfo *info)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrDbgCheck();
		if (ret == 0) {
			ret = regMgrDbgSplitKey(info);
			if (ret == 0) {
				regMgrSetStatus(REGMGR_STATUS_NOTIFY);
				ret = sceRegMgrSetKeys(g_regMgrDbgCategory, &g_regMgrDbgKey, 1);
				regMgrClearStatus();
			}
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* get a key given by its full path */
int SceRegMgrForDebugger_C64C67A8(RegMgrKeyInfo *info)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrDbgCheck();
		if (ret == 0) {
			ret = regMgrDbgSplitKey(info);
			if (ret == 0) {
				regMgrSetStatus(REGMGR_STATUS_NOTIFY);
				ret = sceRegMgrGetKeys(g_regMgrDbgCategory, &g_regMgrDbgKey, 1);
				regMgrClearStatus();
				if (ret == 0 && info->type == REGMGR_TYPE_INT)
					info->value = g_regMgrDbgKey.value;
			}
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForDebugger_8E798E47(void)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0 && regMgrIsTool() == 0) {
		if (regMgrGetVolatileMode() == 1)
			ret = SCE_REGMGR_ERROR_VOLATILE_MODE;
		else
			ret = regMgrIsInitSettings();
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForDebugger_B9A0BB77(void)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0 && regMgrIsTool() == 0) {
		if (regMgrGetVolatileMode() == 1)
			ret = SCE_REGMGR_ERROR_VOLATILE_MODE;
		else
			ret = regMgrIsToolDefChanged(0);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the size of a tool definition file */
static inline __attribute__((always_inline)) int regMgrDbgGetToolDefSize(int eventLog, SceSize *uisize)
{
	const char *path;
	SceIoStat stat;
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrDbgCheck();
		if (ret == 0) {
			if (uisize == NULL) {
				ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			} else {
				path = regMgrGetToolDefPath(eventLog);
				memset(&stat, 0, sizeof(stat));
				ret = sceIoGetstat(path, &stat);
				if (ret < 0)
					ret = SCE_REGMGR_ERROR_FILE;
				else
					*uisize = stat.st_size;
			}
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForDebugger_D668ABD2(SceSize *uisize)
{
	return regMgrDbgGetToolDefSize(0, uisize);
}

int SceRegMgrForDebugger_C588D3E7(void)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0 && regMgrIsTool() == 0) {
		if (regMgrGetVolatileMode() == 1)
			ret = SCE_REGMGR_ERROR_VOLATILE_MODE;
		else
			ret = regMgrIsToolDefChanged(1);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForDebugger_8AE5757E(SceSize *uisize)
{
	return regMgrDbgGetToolDefSize(1, uisize);
}

/* copy the tool definition file to host0: */
static inline __attribute__((always_inline)) int regMgrDbgCopyToolDef(const RegMgrToolDefCopy *copy)
{
	const char *path;
	SceUID dst, src;
	int n, written;

	if (copy == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if (copy->size != sizeof(*copy))
		return SCE_REGMGR_ERROR_BAD_KEYS_INFO;
	path = copy->path;
	if (path == NULL || copy->mode == 0 || memcmp(path, "host0:", 6) != 0)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	dst = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0664);
	if (dst < 0)
		return SCE_REGMGR_ERROR_FILE;
	src = sceIoOpen(regMgrGetToolDefPath(0), SCE_O_RDONLY, 0);
	if (src < 0) {
		sceIoClose(dst);
		return SCE_REGMGR_ERROR_FILE;
	}
	do {
		n = sceIoRead(src, g_regMgrDbgBuf, sizeof(g_regMgrDbgBuf));
		if (n <= 0) {
			written = n == 0 ? 0 : SCE_REGMGR_ERROR_FILE;
			goto out;
		}
		written = sceIoWrite(dst, g_regMgrDbgBuf, n);
		if (written <= 0) {
			if (written != 0) {
				written = SCE_REGMGR_ERROR_FILE;
				goto out;
			}
			break;
		}
	} while (n == written);
	written = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
out:
	sceIoClose(dst);
	sceIoClose(src);
	return written;
}

int SceRegMgrForDebugger_46AC0CCB(int cmd, void *arg)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	ret = regMgrDbgCheck();
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_NOTIFY);
	switch (cmd) {
	case 1:
		ret = regMgrDbgCopyToolDef(arg);
		break;
	case 2:
		ret = regMgrComSyncKeyValue(0);
		break;
	case 4:
		ret = sceRegMgrResetRegistryLv(arg, 0x20);
		break;
	case 8:
		ret = regMgrDbgReadToolDef(0, arg);
		break;
	case 0x10:
		ret = regMgrDbgReadToolDef(1, arg);
		break;
	default:
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		break;
	}
	regMgrClearStatus();
out:
	regMgrComApiEpilogue(lock);
	return ret;
}
