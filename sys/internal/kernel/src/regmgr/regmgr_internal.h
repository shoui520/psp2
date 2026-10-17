/*
 * SceRegistryMgr: the system registry. The keys are defined by
 * os0:kd/registry.db0 and kept in vd0:/registry; the values of some are
 * cached in memory, some live in the NVS.
 */

#ifndef REGMGR_INTERNAL_H
#define REGMGR_INTERNAL_H

#include <psp2kern/types.h>
#include <psp2kern/kernel/sysclib.h>
#include <string.h>
#include <sysmem/debug.h>
#include "reg.h"
#include <psp2kern/kernel/threadmgr/misc.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/qafmgr.h>
#include <processmgr/processmgr.h>

int regMgrLoadCache(void);

/* 4.00 is built with _FORTIFY_SOURCE */
#if FW_VERSION >= 0x04000000
#define memcpy(dst, src, len)	__builtin___memcpy_chk(dst, src, len, __builtin_object_size(dst, 0))
#define memset(dst, c, len)	__builtin___memset_chk(dst, c, len, __builtin_object_size(dst, 0))
#define strncpy(dst, src, len)	__builtin___strncpy_chk(dst, src, len, __builtin_object_size(dst, 1))
#define strncat(dst, src, len)	__builtin___strncat_chk(dst, src, len, __builtin_object_size(dst, 1))
#endif

#define SCE_REGMGR_ERROR_INVALID_ARGUMENT	0x800d0004	/* names unknown */
#define SCE_REGMGR_ERROR_LINE_TOO_LONG		0x800d0006
#define SCE_REGMGR_ERROR_TOO_MANY_KEYS		0x800d0007
#define SCE_REGMGR_ERROR_PATH_TOO_LONG		0x800d0008
#define SCE_REGMGR_ERROR_BAD_PATH		0x800d0009
#define SCE_REGMGR_ERROR_BAD_CATEGORY		0x800d000a
#define SCE_REGMGR_ERROR_BAD_TYPE		0x800d000b
#define SCE_REGMGR_ERROR_BAD_SIZE		0x800d000c
#define SCE_REGMGR_ERROR_BAD_ATTRIBUTE		0x800d000d
#define SCE_REGMGR_ERROR_BAD_PERMISSION		0x800d000e
#define SCE_REGMGR_ERROR_BAD_FLAG		0x800d000f
#define SCE_REGMGR_ERROR_BAD_OPTION		0x800d0010
#define SCE_REGMGR_ERROR_BAD_INIT		0x800d0012
#define SCE_REGMGR_ERROR_BAD_VALUE		0x800d0013
#define SCE_REGMGR_ERROR_BAD_FLAG2		0x800d001a
#define SCE_REGMGR_ERROR_BAD_FLAG3		0x800d001b
#define SCE_REGMGR_ERROR_BAD_LEVEL		0x800d0020
#define SCE_REGMGR_ERROR_BAD_CACHE		0x800d0021
#define SCE_REGMGR_ERROR_OUT_OF_RANGE		0x800d0017
#define SCE_REGMGR_ERROR_NO_FILE		0x800d0022
#define SCE_REGMGR_ERROR_BAD_NAME		0x800d0014
#define SCE_REGMGR_ERROR_NO_ACCESS		0x800d0015
#define SCE_REGMGR_ERROR_BAD_PRODUCT		0x800d0018
#define SCE_REGMGR_ERROR_BAD_KEYS_INFO		0x800d001c
#define SCE_REGMGR_ERROR_STR_TOO_LONG		0x800d001d
#define SCE_REGMGR_ERROR_NVS_SIZE		0x800d0019
#define SCE_REGMGR_ERROR_ALREADY_OPENED		0x800d0030

#define REGMGR_BASE_MAX		0x400	/* the most keys */
#define REGMGR_CATEGORY_MAX	0x400	/* names unknown */
#define REGMGR_CATEGORY_NAME	0x1c
#define REGMGR_CATEGORY_DEPTH	5
#define REGMGR_WORD_SIZE	0x100
#define REGMGR_LINE_SIZE	0x800
#define REGMGR_READ_SIZE	0x1400
#define REGMGR_BIN_CACHE_SIZE	0x800
#define REGMGR_NUM_CACHE_MAX	0x100
#define REGMGR_INIT_DATA_SIZE	0x500
#define REGMGR_CALLBACK_MAX	0x10

#define REGMGR_ISDIGIT(c)	((SceUInt32)((SceUInt8)(c) - '0') < 10)

/* a key's type in registry.db0 */
#define REGMGR_TYPE_INT		0	/* names unknown */
#define REGMGR_TYPE_STR		1
#define REGMGR_TYPE_BIN		2

/* how a key's value is cached (RegMgrKey.cacheType, names unknown) */
#define REGMGR_CACHE_INT	1
#define REGMGR_CACHE_BIN	2
#define REGMGR_CACHE_NUM	0x10	/* a "NUM CATEGORY": one int per numbered category */

/* RegMgrKey.flags (names unknown): who may read and write the key */
#define REGMGR_KEY_READ		0x02
#define REGMGR_KEY_WRITE	0x04
#define REGMGR_KEY_READ_ALT	0x08
#define REGMGR_KEY_WRITE_ALT	0x10
#define REGMGR_KEY_CACHED	0x20
#define REGMGR_KEY_NVS		0x40

/* RegMgrKey.attribute (names unknown): a volatile key is not kept in the registry
 * files (its value is in the NVS, or its initial value) */
#define REGMGR_ATTR_NVS		0x80	/* in registry.db0: becomes REGMGR_ATTR_VOLATILE */
#define REGMGR_ATTR_VOLATILE	0x1000
#define REGMGR_ATTR_MASK	0x9fff
#define REGMGR_ATTR_RESET_40	0x2000	/* reset by a reset of level 0x40 */
#define REGMGR_ATTR_RESET_100	0x4000	/* reset by a reset of level 0x100 */

/* RegMgrKey.initType: the initial value is computed (name unknown) */
#define REGMGR_INIT_COMPUTED	0x80

/* an entry of s_rgMnKey: a key of registry.db0 (field names unknown) */
typedef struct RegMgrKey {
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];	/* its category path */
	SceUInt16 type;
	SceUInt16 size;
	SceUInt16 attribute;
	struct {
		SceUInt8 level;
		SceUInt8 permission;
		SceUInt8 flags;
		SceUInt8 option0;
		SceUInt8 option1;
		SceUInt8 cacheType;
		SceUInt16 reserved;
	} attr;
	SceUInt16 initType;
	SceUInt16 initSize;
	union {
		int value;
		const void *data;
	} init;
	union {
		int value;		/* REGMGR_CACHE_INT */
		SceUInt32 offset;	/* REGMGR_CACHE_BIN: into g_regMgrBinCache */
		struct {		/* REGMGR_CACHE_NUM */
			SceUInt16 index;	/* into g_regMgrNumCache */
			SceUInt16 first;	/* the number of the first category */
		} num;
	} cache;
} RegMgrKey;

/* a callback registered for changes of up to five keys (names unknown) */
#define REGMGR_CALLBACK_NOTIFY	1
#define REGMGR_KEYNO_NONE	0xffff
#define REGMGR_KEYNO_ANY	0xfffe
#define REGMGR_KEYNO_CATEGORY	0x8000	/* the keys of a category: level in bits 12-14 */

typedef struct RegMgrCallback {
	SceUID cbid;
	SceUInt16 mode;
	SceUInt16 keyNo[REGMGR_CATEGORY_DEPTH];
} RegMgrCallback;

/* a key access (names unknown) */
typedef struct RegMgrKeyAccess {
	int handle;
	int keyNo;
	const char *category;
	const char *name;
	int type;
	int size;
	void *buf;
	int valueSize;
} RegMgrKeyAccess;

/* a change, as the notify function gets it (names unknown) */
typedef struct RegMgrKeyInfo {
	SceSize size;
	const char *name;
	SceUInt16 type;
	SceUInt16 valueSize;
	union {
		int value;
		const void *data;
	};
} RegMgrKeyInfo;

typedef struct RegMgrNotifyInfo {
	SceSize size;			/* REGMGR_NOTIFY_INFO_SIZE */
	char path[REGMGR_WORD_SIZE];
	SceUInt16 type;
	SceUInt16 valueSize;
	int value;
	char data[];
} RegMgrNotifyInfo;

typedef struct RegMgrValue {	/* name unknown */
	void *data;
	SceSize size;
} RegMgrValue;

extern int s_rgMnKeyCnt;
extern RegMgrKey s_rgMnKey[REGMGR_BASE_MAX];
extern int g_regMgrCategoryCnt;				/* names unknown */
extern char g_regMgrCategoryName[REGMGR_CATEGORY_MAX][REGMGR_CATEGORY_NAME];
extern char g_regMgrWord[REGMGR_WORD_SIZE];
extern char g_regMgrLine[REGMGR_LINE_SIZE];
extern char g_regMgrReadBuf[REGMGR_READ_SIZE];
extern char g_regMgrOpenCategory[REGMGR_WORD_SIZE];
extern SceUInt8 g_regMgrBinCache[REGMGR_BIN_CACHE_SIZE];
extern int g_regMgrNumCache[REGMGR_NUM_CACHE_MAX];
extern SceUInt8 g_regMgrNumCacheValid[REGMGR_NUM_CACHE_MAX / 8 + 1];
extern RegMgrCallback g_regMgrCallback[REGMGR_CALLBACK_MAX];
extern int g_regMgrNumCacheCnt;
extern int s_rgMnBinCrt;
extern int g_regMgrInitDataCnt;
extern SceUInt8 g_regMgrInitData[REGMGR_INIT_DATA_SIZE];
extern int g_regMgrVolatileMode;
extern SceUInt32 g_regMgrConfig;
extern int g_regMgrReadPos;
extern int g_regMgrReadEnd;
extern int g_regMgrLineLen;
extern int g_regMgrRegion;
extern int g_regMgrIsDolce;
extern const char g_regMgrRegionName[][4];
extern SceUInt32 g_regMgrResetLevel;
extern int g_regMgrLevelLimit;
extern int g_regMgrWriteVersion;
extern SceUInt32 g_regMgrVersion;
extern SceUInt32 g_regMgrStatus;
extern SceUInt32 g_regMgrPermission;
extern RegMgrNotifyInfo *g_regMgrNotifyInfo;
extern int (*g_regMgrNotifyFunc)(RegMgrNotifyInfo *info);
extern int g_regMgrIsQaf;
extern SceUID g_regMgrMutex;
extern int g_regMgrInitResult;
extern SceUInt32 g_regMgrErrorMasquerade;
extern int g_regMgrErrorCountdown;
extern int g_regMgrApiNest;
extern SceUInt32 g_regMgrSavedTpidruro;
extern SceUID g_regMgrDrvErrCallback;
extern SceUInt32 g_regMgrUiSerial;
extern SceUInt32 g_regMgrEvSerial;
extern SceUID g_regMgrEventFlag;
extern int g_regMgrRegionError;
extern SceUInt32 g_regMgrInitNotice;
extern SceUInt32 g_regMgrErrEnable;

#define REGMGR_CONFIG_RELEASE	0x1		/* names unknown: a retail console, or a tool not in development mode */
#define REGMGR_CONFIG_TOOL	0x2

#define REGMGR_STATUS_INIT	0x10000000	/* names unknown */
#define REGMGR_STATUS_NOTIFY	0x00000002	/* only the notified keys are accessible */
#define REGMGR_STATUS_FLAGS	0x00000004	/* the keys' read/write flags apply */
#define REGMGR_STATUS_FLAGS_ALT	0x00000008	/* ... the alternative ones */
#define REGMGR_VERSION_DIRTY	0x80000000

#define SCE_REGMGR_ERROR_KEY_EXISTS	0x800d2728	/* names unknown */

/* the registry files (reg library, names unknown) */
int regInit(void);
int regOpenRegistry(void *param, int mode, int *handle);
int regCloseRegistry(int handle);
int regRemoveRegistry(void *param);
int regBackupRegistry(void);
int regRestoreRegistry(void);
int regExportRegistry(const char *path);
int regImportRegistry(const char *path);
int regOpenCategory(int registry, const char *category, int mode, int *handle);
int regCloseCategory(int handle);
int regRemoveCategory(int registry, const char *path, int flags);
int regDeleteKey(int handle, const char *name);
int regCreateKey(int handle, const char *name, int type, int size);
int regSetKey(int handle, const char *name, const void *buf, SceSize size);
int regGetKey(int handle, const char *name, void *buf, SceSize size);

int ksceSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size);
int ksceSblSsSetNvsData(SceUInt32 offset, const void *buf, SceSize size);

/* regmgr_wrapreg.c */
extern int g_regMgrRegistryHandle;
extern int g_regMgrLastError;
extern int g_regMgrLastErrorLine;
int regMgrWrapRegGetRegistryHandle(void);
int regMgrWrapRegSetError(int ret, int line);
int regMgrWrapRegInit(void);
int regMgrWrapRegOpenCategory(const char *category, int mode, int *handle);
int regMgrWrapRegCloseCategory(int *handle);
int regMgrWrapRegSetKey(int handle, const char *category, const char *name, const void *buf, SceSize size);
int regMgrWrapRegGetKey(int handle, const char *category, const char *name, void *buf, SceSize size);
int regMgrWrapGetNVSData(const char *category, const char *name, void *buf, SceSize size);
void regMgrSetLastError(int error);
int regMgrGetLastError(void);
int regMgrComputeInitValue(int initType, RegMgrValue *value);
int regMgrAddinPsIdToken(RegMgrValue *value);
int regMgrAddinUserName(RegMgrValue *value);
int regMgrWrapRegRemoveCategory(const char *category);
int regMgrWrapRegDeleteKey(int handle, const char *category, const char *name);
int regMgrWrapRegCreateKey(int handle, const char *category, const char *name, int type, int size);
int regMgrWrapRegOpenRegistry(int mode, int openMode);
int regMgrWrapRegCloseRegistry(void);
int regMgrWrapRegReopenRegistry(int openMode);
int regMgrWrapRegRemoveRegistry(void);
int regMgrWrapRegBackupRegistry(void);
int regMgrWrapRegRestoreRegistry(void);
int regMgrWrapRegExportRegistry(const char *path);
int regMgrWrapRegImportRegistry(const char *path);

/* regmgr_movevalue.c, regmgr_convertvalue.c: values of an older registry version (names unknown) */
int regMgrDeleteObsoleteValues(int version, int newVersion);
void regMgrMoveUpdateValues(int version, int newVersion);
int regMgrMoveNpAccountNames(int version, int newVersion);
int regMgrMoveTelValues(int version, int newVersion);
int regMgrMoveNpCurrentAccount(int version, int newVersion);
void regMgrConvertLanguage(int version, int newVersion);

#define REGMGR_STATUS_SYSCALL	0x00000001	/* called by a system call (name unknown) */

/* the kernel APIs */
int sceRegMgrIsBlueScreen(int error);
int sceRegMgrGetRegVersion(void);
int sceRegMgrResetRegistryLv(const char *category, int level);
int sceRegMgrSetKeyInt(const char *category, const char *name, int value);
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);
int sceRegMgrSetKeyStr(const char *category, const char *name, const char *buf, SceSize size);
int sceRegMgrGetKeyStr(const char *category, const char *name, char *buf, SceSize size);
int sceRegMgrSetKeyBin(const char *category, const char *name, const void *buf, SceSize size);
int sceRegMgrGetKeyBin(const char *category, const char *name, void *buf, SceSize size);
int sceRegMgrGetKeysInfo(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrSetKeys(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrGetKeys(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrGetInitVals(const char *category, RegMgrKeyInfo *info, int count);
int sceRegMgrRegisterCallback(SceUID cbid);
int sceRegMgrAddRegistryCallback(SceUID cbid, const char *path);
int sceRegMgrStartCallback(SceUID cbid);
int sceRegMgrStopCallback(SceUID cbid);
int sceRegMgrUnregisterCallback(SceUID cbid);
int sceRegMgrRegisterDrvErrCallback(SceUID cbid);
int sceRegMgrUnregisterDrvErrCallback(SceUID cbid);
int sceRegMgrDbBackup(const char *path, int level);
int sceRegMgrDbRestore(const char *path);
int sceRegMgrSrvGetRegion(void);
int sceRegMgrSrvGetRegionStr(char *buf, SceSize size);
int sceRegMgrSrvCnvRegionStr(int region, char *buf, SceSize size);
int sceRegMgrSrvCnvRegionInt(const char *name);
int sceRegMgrSrvCnvRegionPsCode(int region);

/* regmgr.c */
int regMgrComApiPrologue(int *lock);
int regMgrComApiEpilogue(int lock);
int regMgrGetRegionNo(void);
const char *regMgrGetRegionName(int region);
SceUInt32 regMgrGetErrEnable(void);
void regMgrRecordError(int error, int api, const char *category, const char *name,
		       const void *data, SceSize size);
int regMgrGetVolatileMode(void);
int regMgrMakePsIdToken(SceUInt8 *token);
SceUInt32 regMgrIsTool(void);
SceUInt32 regMgrIsRelease(void);
int regMgrIsQaf(void);
int regMgrLock(int *lock);
int regMgrUnlock(int lock);
int regMgrNotifyLevel(int level);
void regMgrClearStatus(void);
void regMgrSetStatus(SceUInt32 status);
SceUInt32 regMgrIsInitSettings(void);
SceUInt32 regMgrIsToolDefChanged(int eventLog);
SceUInt32 regMgrGetToolDefSerial(int eventLog);
const char *regMgrGetToolDefPath(SceUInt32 eventLog);
int regMgrSetKeys(const char *category, RegMgrKeyInfo *info, int *count);
int regMgrSetKeyInt(const char *category, const char *name, int value);
int regMgrGetKeyInt(const char *category, const char *name, int *value);

#endif
