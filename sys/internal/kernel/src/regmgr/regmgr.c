/*
 * regmgr.c: SceRegistryMgr's keys. registry.db0 defines them (their
 * categories, types, sizes, attributes and initial values); their values
 * live in the registry files, in the NVS, or in the caches here.
 */

#include <sysmem/sysroot.h>
#include <psp2kern/io/fcntl.h>
#include <acmgr/acmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/debugled.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <excpmgr/excpmgr_cpu.h>
#include "regmgr_internal.h"

#define REGMGR_DB_PATH		"os0:/kd/registry.db0"	/* names unknown */
#define REGMGR_REGION_TOOL	0xb

/* the sections of registry.db0 (names unknown) */
#define REGMGR_SECTION_SKIP	0
#define REGMGR_SECTION_BASE	1	/* [BASE]: the category names, "<n>=<name>" */
#define REGMGR_SECTION_KEY	2	/* [REG-BAS]: the keys, "/<n>/<n>=<definition>" */
#define REGMGR_SECTION_UPDATE	3	/* [REG-<region>]: definitions replaced for a region */

/* the region codes of [REG-<region>] sections (name unknown) */
const char g_regMgrRegionName[][4] = {
	"BAS", "J1", "UC2", "AU3", "CEK", "CEL", "KR2", "E12", "TW1", "RU3", "MX2", "TOL", "BR2",
	"CN9",
};

/* registry.db0 after its leading comment lines is masked with this (name unknown) */
static const SceUInt8 g_regMgrDbMask[16] = {
	0x89, 0xfa, 0x95, 0x48, 0xcb, 0x6d, 0x77, 0x9d, 0xa2, 0x25, 0x34, 0xfd, 0xa9, 0x35, 0x59, 0x6e,
};

int g_regMgrLevelLimit = 30;
SceUID g_regMgrEventFlag = -1;
SceUID g_regMgrMutex = -1;
SceUInt32 g_regMgrConfig = 2;
int s_rgMnKeyCnt;
int g_regMgrReadPos;
char g_regMgrReadBuf[REGMGR_READ_SIZE];
int g_regMgrRegion;
SceUInt32 g_regMgrPermission;
int g_regMgrVolatileMode;
char g_regMgrOpenCategory[REGMGR_WORD_SIZE];
int g_regMgrNumCacheCnt;
int g_regMgrWriteVersion;
SceUInt32 g_regMgrVersion;
SceUInt32 g_regMgrStatus;
int g_regMgrCategoryCnt;
SceUInt8 g_regMgrBinCache[REGMGR_BIN_CACHE_SIZE];
SceUInt32 g_regMgrResetLevel;
RegMgrNotifyInfo *g_regMgrNotifyInfo;
SceUInt8 g_regMgrNumCacheValid[REGMGR_NUM_CACHE_MAX / 8 + 1];
int g_regMgrLineLen;
SceUInt8 g_regMgrInitData[REGMGR_INIT_DATA_SIZE];
int g_regMgrIsQaf;
RegMgrKey s_rgMnKey[REGMGR_BASE_MAX];
char g_regMgrWord[REGMGR_WORD_SIZE];
char g_regMgrCategoryName[REGMGR_CATEGORY_MAX][REGMGR_CATEGORY_NAME];
int g_regMgrNumCache[REGMGR_NUM_CACHE_MAX];
int g_regMgrIsDolce;
RegMgrCallback g_regMgrCallback[REGMGR_CALLBACK_MAX];
int g_regMgrReadEnd;
int (*g_regMgrNotifyFunc)(RegMgrNotifyInfo *info);
int g_regMgrInitDataCnt;
int g_regMgrInitResult;
SceUInt32 g_regMgrErrorMasquerade;
int g_regMgrErrorCountdown;
int g_regMgrApiNest;
SceUInt32 g_regMgrSavedTpidruro;
SceUID g_regMgrDrvErrCallback;
SceUInt32 g_regMgrUiSerial;
SceUInt32 g_regMgrEvSerial;
int g_regMgrRegionError;
SceUInt32 g_regMgrInitNotice;
SceUInt32 g_regMgrErrEnable;
int s_rgMnBinCrt;
char g_regMgrLine[REGMGR_LINE_SIZE];

/* a decimal (optionally negative) or 0x-prefixed hexadecimal number of at most len
 * characters after leading spaces; *used gets the characters read (name unknown) */
static int regMgrStrToInt(const char *text, SceSize len, SceSize *used)
{
	const SceChar8 *str = (const SceChar8 *)text;
	SceSize i = 0;
	int value = 0, sign;
	SceChar8 c;

	if (len == 0)
		goto out;
	while (str[i] == ' ') {
		if (++i == len)
			goto out;
	}
	if (str[i] == '0' && (str[i + 1] & ~0x20) == 'X') {
		for (i += 2; i < len; i++) {
			c = str[i];
			if (REGMGR_ISDIGIT(c))
				value = value * 16 + c - '0';
			else if ((SceUInt32)((SceUInt8)c - 'a') < 6)
				value = value * 16 + c - 'a' + 10;
			else if ((SceUInt32)((SceUInt8)c - 'A') < 6)
				value = value * 16 + c - 'A' + 10;
			else
				break;
		}
		goto out;
	}
	sign = 1;
	if (str[i] == '-') {
		if (++i >= len)
			goto out;
		sign = -1;
	}
	if (!REGMGR_ISDIGIT(str[i]))
		goto out;
	do {
		value = value * 10 + str[i] - '0';
		if (++i >= len)
			break;
	} while (REGMGR_ISDIGIT(str[i]));
	value *= sign;
out:
	if (used != NULL)
		*used = i;
	return value;
}

/* forget the cached values (name unknown) */
void regMgrClearCache(void)
{
	int i;

	for (i = 0; i < REGMGR_BASE_MAX; i++) {
		s_rgMnKey[i].attr.flags &= ~REGMGR_KEY_CACHED;
		if (s_rgMnKey[i].attr.cacheType == REGMGR_CACHE_INT)
			s_rgMnKey[i].cache.value = 0;
	}
	memset(g_regMgrNumCache, 0, sizeof(g_regMgrNumCache));
	memset(g_regMgrNumCacheValid, 0, sizeof(g_regMgrNumCacheValid));
	memset(g_regMgrBinCache, 0, sizeof(g_regMgrBinCache));
}

/* the initial value of a key (name unknown) */
int regMgrGetKeyInitValue(int keyNo, RegMgrValue *value)
{
	RegMgrValue computed;
	SceUInt32 initType;
	SceSize size;
	int ret;

	if (keyNo < 0 || keyNo >= s_rgMnKeyCnt)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if (s_rgMnKey[keyNo].type > REGMGR_TYPE_BIN)
		return SCE_REGMGR_ERROR_BAD_TYPE;
	initType = s_rgMnKey[keyNo].initType;
	if (initType & REGMGR_INIT_COMPUTED) {
		computed.data = g_regMgrLine;
		computed.size = s_rgMnKey[keyNo].size;
		ret = regMgrComputeInitValue(initType, &computed);
		value->data = g_regMgrLine;
		value->size = computed.size;
		if (ret != 0)
			goto fill;
	}
	if (initType == 0) {
fill:
		size = s_rgMnKey[keyNo].size;
		if (size > sizeof(g_regMgrLine))
			return SCE_REGMGR_ERROR_BAD_SIZE;
		if (s_rgMnKey[keyNo].init.value >= 0x100)
			return SCE_REGMGR_ERROR_BAD_VALUE;
		if (size != 0)
			memset(g_regMgrLine, s_rgMnKey[keyNo].init.value, size);
		value->size = size;
		value->data = g_regMgrLine;
		return 0;
	}
	if (initType == 1) {
		if (s_rgMnKey[keyNo].type != REGMGR_TYPE_INT)
			value->data = s_rgMnKey[keyNo].init.data;
		else
			value->data = &s_rgMnKey[keyNo].init.value;
		value->size = s_rgMnKey[keyNo].initSize;
		return 0;
	}
	return 0;
}

/* set a key's value in the registry and notify the callbacks registered for it (name unknown) */
int regMgrSetKey(int handle, const char *category, const char *name, const void *buf,
		 SceSize size, int keyNo)
{
	SceUInt32 no, level;
	int ret, cb, i;

	if (regMgrWrapRegGetKey(handle, category, name, g_regMgrReadBuf, size) == 0 &&
	    memcmp(g_regMgrReadBuf, buf, size) == 0)
		return 0;
	ret = regMgrWrapRegSetKey(handle, category, name, buf, size);
	if (ret != 0)
		return ret;
	for (cb = 0; cb < REGMGR_CALLBACK_MAX; cb++) {
		if (g_regMgrCallback[cb].cbid == -1)
			continue;
		if (g_regMgrCallback[cb].mode != REGMGR_CALLBACK_NOTIFY) {
			if (SceThreadmgrForDriver_0892D8DF(g_regMgrCallback[cb].cbid) < 0)
				memset(&g_regMgrCallback[cb], 0xff, sizeof(g_regMgrCallback[cb]));
			continue;
		}
		for (i = 0; i < REGMGR_CATEGORY_DEPTH; i++) {
			no = g_regMgrCallback[cb].keyNo[i];
			if (no == REGMGR_KEYNO_NONE)
				break;
			if (no == REGMGR_KEYNO_ANY || no == keyNo)
				goto notify;
			if (no & REGMGR_KEYNO_CATEGORY) {
				level = (no >> 12) & 7;
				if (level < 4 && s_rgMnKey[keyNo].categoryNo[level] ==
						 s_rgMnKey[no & 0xfff].categoryNo[level])
					goto notify;
			}
		}
		continue;
notify:
		if (sceKernelNotifyCallback(g_regMgrCallback[cb].cbid, i) != 0)
			memset(&g_regMgrCallback[cb], 0xff, sizeof(g_regMgrCallback[cb]));
	}
	return 0;
}

/* open a category, creating it and up to three missing parents (name unknown) */
int regMgrCreateCategory(RegMgrKeyAccess *access)
{
	char path[REGMGR_WORD_SIZE];
	int cut[3];
	int len, depth, created, ret, i;

	strncpy(path, access->category, sizeof(path));
	len = strnlen(path, sizeof(path));
	if (len < 1)
		return SCE_REGMGR_ERROR_PATH_TOO_LONG;
	if (path[len - 1] == '/')
		path[len - 1] = '\0';
	created = 0;
	depth = 0;
	for (;;) {
		while ((ret = regMgrWrapRegOpenCategory(path, 1, &access->handle)) == 0) {
			created++;
			if (depth == 0)
				return 0;
			ret = regMgrWrapRegCloseCategory(&access->handle);
			g_regMgrOpenCategory[0] = '\0';
			if (ret != 0)
				return ret;
			path[cut[--depth]] = '/';
		}
		access->handle = 0;
		if (created != 0)
			return ret;
		for (i = strnlen(path, sizeof(path)) - 1; i >= 0 && path[i] != '/'; i--)
			;
		if (i <= 0)
			return SCE_REGMGR_ERROR_PATH_TOO_LONG;
		if (depth > 2)
			return SCE_REGMGR_ERROR_BAD_PATH;
		cut[depth++] = i;
		path[i] = '\0';
	}
}

/* copy the next word of a path into g_regMgrWord: up to and with its '/', or all
 * that is left (names unknown) */
static inline __attribute__((always_inline)) SceSize regMgrCutWord(const char **path, SceSize *rest)
{
	const char *p = *path;
	SceSize i, len;

	if ((int)*rest > 0 && *p != '/' && *p != '\0') {
		for (i = 0; i + 1 != *rest; i++) {
			if (p[i + 1] == '/') {
				if (i >= 0xff)
					break;
				memcpy(g_regMgrWord, p, i + 1);
				g_regMgrWord[i + 1] = '/';
				g_regMgrWord[i + 2] = '\0';
				*path = p + i + 2;
				*rest -= i + 2;
				return i + 2;
			}
			if (p[i + 1] == '\0')
				break;
		}
	}
	if (strnlen(p, *rest) == 0)
		return 0;
	len = *rest;
	if (len != 0)
		memcpy(g_regMgrWord, p, len);
	g_regMgrWord[len] = '\0';
	*rest = 0;
	return len;
}

/* the number of the numbered category ("/CONFIG/NET/01/") in a path (name unknown) */
int regMgrGetPathNumber(const char *path)
{
	const char *p;
	SceSize len, rest;
	int depth;

	len = strnlen(path, REGMGR_WORD_SIZE);
	if (len >= REGMGR_WORD_SIZE)
		return SCE_REGMGR_ERROR_PATH_TOO_LONG;
	if (path[0] != '/')
		return SCE_REGMGR_ERROR_BAD_PATH;
	rest = len - 1;
	p = path + 1;
	for (depth = REGMGR_CATEGORY_DEPTH; depth != 0; depth--) {
		if (regMgrCutWord(&p, &rest) == 0)
			return SCE_REGMGR_ERROR_BAD_CATEGORY;
		if (REGMGR_ISDIGIT(g_regMgrWord[0]) && REGMGR_ISDIGIT(g_regMgrWord[1]))
			return regMgrStrToInt(g_regMgrWord, 2, NULL);
		if (rest == 0)
			return SCE_REGMGR_ERROR_BAD_CATEGORY;
	}
	return 0;
}

/* get a key's value: from its cache, the NVS or the registry (name unknown) */
int regMgrGetKey(int keyNo, int handle, const char *category, const char *name,
		  SceInt16 *size, void *buf)
{
	const RegMgrKey *key = &s_rgMnKey[keyNo];
	RegMgrValue value;
	int ret, no;

	if (*size < key->size)
		return SCE_REGMGR_ERROR_BAD_SIZE;
	if ((key->attribute & REGMGR_ATTR_MASK) == REGMGR_ATTR_VOLATILE) {
		ret = regMgrWrapGetNVSData(category, name, buf, *size);
		if (ret > 0)
			return ret;
		if ((key->attr.flags & REGMGR_KEY_CACHED) && key->attr.cacheType == REGMGR_CACHE_BIN) {
			if (*size != 0)
				memcpy(buf, &g_regMgrBinCache[key->cache.offset], *size);
		} else {
			value.data = NULL;
			value.size = 0;
			ret = regMgrGetKeyInitValue(keyNo, &value);
			if (ret == 0) {
				if (value.size != 0)
					memcpy(buf, value.data, value.size);
				*size = value.size;
			}
		}
		return ret;
	}
	if (!(key->attr.flags & REGMGR_KEY_CACHED)) {
		if (key->attr.cacheType == REGMGR_CACHE_NUM) {
			no = regMgrGetPathNumber(category);
			if (no < 0)
				return no;
			no = key->cache.num.index + (no - key->cache.num.first);
			if (no < 0)
				return no;
			if ((g_regMgrNumCacheValid[no >> 3] >> (no & 7)) & 1) {
				*(int *)buf = g_regMgrNumCache[no];
				return 0;
			}
		}
		return regMgrWrapRegGetKey(handle, category, name, buf, *size);
	}
	if (key->attr.cacheType == REGMGR_CACHE_INT) {
		*(int *)buf = key->cache.value;
		return 0;
	}
	if (*size != 0)
		memcpy(buf, &g_regMgrBinCache[key->cache.offset], *size);
	return 0;
}

/* get a key's value, opening its category unless it is open already (name unknown) */
int regMgrGetKeyAccess(RegMgrKeyAccess *access)
{
	int ret;

	if (strncmp(g_regMgrOpenCategory, access->category, sizeof(g_regMgrOpenCategory)) != 0) {
		regMgrWrapRegCloseCategory(&access->handle);
		g_regMgrOpenCategory[0] = '\0';
		ret = regMgrWrapRegOpenCategory(access->category, 2, &access->handle);
		if (ret != 0) {
			access->handle = 0;
			return ret;
		}
		strncpy(g_regMgrOpenCategory, access->category, sizeof(g_regMgrOpenCategory));
	}
	return regMgrGetKey(access->keyNo, access->handle, access->category, access->name,
			    (SceInt16 *)&access->valueSize, access->buf);
}

/* the category of a word of a path, from start: a name, or a number in a range
 * ("01-10") (name unknown) */
int regMgrFindCategory(const char *word, SceSize len, int start)
{
	int cnt = g_regMgrCategoryCnt;
	const char *name;
	int no;

	if (word == NULL || len > 0x19 || cnt > REGMGR_CATEGORY_MAX)
		return SCE_REGMGR_ERROR_PATH_TOO_LONG;
	if (REGMGR_ISDIGIT(word[0]) && REGMGR_ISDIGIT(word[1]) &&
	    (word[2] == '\0' || word[2] == '/')) {
		for (; start <= g_regMgrCategoryCnt; start++) {
			name = g_regMgrCategoryName[start];
			if (name[2] != '-')
				continue;
			if (strnlen(name, 0x1a) < 5)
				return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			no = regMgrStrToInt(word, 2, NULL);
			if (no < regMgrStrToInt(name, 2, NULL) || no > regMgrStrToInt(name + 3, 2, NULL))
				return SCE_REGMGR_ERROR_OUT_OF_RANGE;
			goto found;
		}
	} else {
		for (; start <= cnt; start++) {
			name = g_regMgrCategoryName[start];
			if (name[0] == word[0] && (name[len] == '/' || name[len] == '\0') &&
			    memcmp(word, name, len) == 0)
				goto found;
		}
	}
	return SCE_REGMGR_ERROR_BAD_CATEGORY;
found:
	if (start > cnt)
		return SCE_REGMGR_ERROR_BAD_CATEGORY;
	return start;
}

/* the category numbers of a path; gives the depth (name unknown) */
int regMgrGetCategoryNo(const char *path, SceSize len, SceUInt16 *categoryNo)
{
	const char *p;
	SceSize rest, wordLen;
	SceUInt16 depth;
	int no;

	if (len >= 0xe6)
		return SCE_REGMGR_ERROR_PATH_TOO_LONG;
	if (path[0] != '/')
		return SCE_REGMGR_ERROR_BAD_PATH;
	p = path + 1;
	rest = len - 1;
	no = 0;
	depth = 0;
	do {
		wordLen = regMgrCutWord(&p, &rest);
		if (wordLen == 0)
			break;
		no = regMgrFindCategory(g_regMgrWord, wordLen, no);
		if (no < 1)
			break;
		categoryNo[depth] = no;
		if (rest == 0)
			return depth;
		depth++;
	} while (depth != REGMGR_CATEGORY_DEPTH);
	return SCE_REGMGR_ERROR_BAD_CATEGORY;
}

/* whether a category number is in a range ("01-10"), given as a word or a number (name unknown) */
int regMgrCheckNumberRange(const char *range, const char *word, int no)
{
	if (range == NULL || strnlen(range, 0x1a) < 5)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if (word != NULL)
		no = regMgrStrToInt(word, 2, NULL);
	if (no < regMgrStrToInt(range, 2, NULL))
		return SCE_REGMGR_ERROR_OUT_OF_RANGE;
	if (no > regMgrStrToInt(range + 3, 2, NULL))
		return SCE_REGMGR_ERROR_OUT_OF_RANGE;
	return 0;
}

/* read the next ':'-terminated field of a key definition into g_regMgrWord (name unknown) */
static inline __attribute__((always_inline)) int regMgrNextField(const char **def, SceSize *rest)
{
	const char *p = *def;
	SceSize i;

	if ((int)*rest < 1 || *p == ':' || *p == '\0')
		return -1;
	for (i = 0; ; i++) {
		if (i + 1 == *rest)
			return -1;
		if (p[i + 1] == ':')
			break;
		if (p[i + 1] == '\0')
			return -1;
	}
	if (i > 0xfe)
		return -1;
	memcpy(g_regMgrWord, p, i + 1);
	g_regMgrWord[i + 1] = '\0';
	*def = p + i + 2;
	*rest -= i + 2;
	return 0;
}

static inline __attribute__((always_inline)) int regMgrFieldToInt(void)
{
	return regMgrStrToInt(g_regMgrWord, sizeof(g_regMgrWord), NULL);
}

/* a flag of a key definition: 0 or 1 */
static inline __attribute__((always_inline)) int regMgrSetKeyFlag(int keyNo, SceUInt8 flag)
{
	int value = regMgrFieldToInt();

	if (value == 1)
		s_rgMnKey[keyNo].attr.flags |= flag;
	else if (value != 0)
		return -1;
	return 0;
}

/* a category name of a numbered category range: "01-10/" */
static inline __attribute__((always_inline)) int regMgrIsNumberRange(const char *name)
{
	return REGMGR_ISDIGIT(name[0]) && REGMGR_ISDIGIT(name[1]) && name[2] == '-' &&
	       REGMGR_ISDIGIT(name[3]) && REGMGR_ISDIGIT(name[4]) && name[5] == '/';
}

/* a key of registry.db0, after its category path:
 * type:size:cached:attribute:permission:flag:level:level:flag:flag:flag:flag:option:option:initType:value
 * (name unknown) */
int regMgrParseKey(int keyNo, const char *def, SceSize rest)
{
	const char *name, *value;
	SceUInt32 attribute, initType, permission, level, c;
	SceSize len, size, i, out;
	SceUInt8 mul, digit;
	SceUInt8 *data;
	int cached, first, sel;

	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_TYPE;
	s_rgMnKey[keyNo].type = regMgrFieldToInt();
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_SIZE;
	s_rgMnKey[keyNo].size = regMgrFieldToInt();

	/* cached in memory: an int, a binary, or a numbered category's ints */
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_CACHE;
	cached = regMgrFieldToInt();
	if (cached == 1) {
		for (i = 0; i < REGMGR_CATEGORY_DEPTH && s_rgMnKey[keyNo].categoryNo[i] != 0; i++) {
			name = g_regMgrCategoryName[s_rgMnKey[keyNo].categoryNo[i]];
			if (!regMgrIsNumberRange(name))
				continue;
			first = regMgrStrToInt(name, 2, NULL);
			c = regMgrStrToInt(name + 3, 2, NULL);
			if (s_rgMnKey[keyNo].type != REGMGR_TYPE_INT)
				return SCE_REGMGR_ERROR_BAD_CACHE;
			s_rgMnKey[keyNo].cache.num.first = first;
			s_rgMnKey[keyNo].attr.cacheType = REGMGR_CACHE_NUM;
			s_rgMnKey[keyNo].cache.num.index = g_regMgrNumCacheCnt;
			g_regMgrNumCacheCnt += c - first + 1;
			if (g_regMgrNumCacheCnt >= REGMGR_NUM_CACHE_MAX)
				return SCE_REGMGR_ERROR_BAD_CACHE;
			goto attribute;
		}
		if (s_rgMnKey[keyNo].type == REGMGR_TYPE_INT) {
			s_rgMnKey[keyNo].attr.cacheType = REGMGR_CACHE_INT;
		} else {
			s_rgMnKey[keyNo].attr.cacheType = REGMGR_CACHE_BIN;
			s_rgMnKey[keyNo].cache.offset = s_rgMnBinCrt;
			s_rgMnBinCrt += s_rgMnKey[keyNo].size;
			if (s_rgMnBinCrt >= REGMGR_BIN_CACHE_SIZE)
				return SCE_REGMGR_ERROR_BAD_CACHE;
		}
	} else if (cached != 0) {
		return SCE_REGMGR_ERROR_BAD_CACHE;
	}

attribute:
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_ATTRIBUTE;
	attribute = regMgrFieldToInt();
	s_rgMnKey[keyNo].attribute = attribute;
	switch (attribute & REGMGR_ATTR_MASK) {
	case 0:
	case 0x10:
	case 0x20:
	case 0x30:
	case 0x40:
	case 0x100:
	case REGMGR_ATTR_VOLATILE:
		break;
	case REGMGR_ATTR_NVS:
		s_rgMnKey[keyNo].attr.flags |= REGMGR_KEY_NVS;
		s_rgMnKey[keyNo].attribute = (s_rgMnKey[keyNo].attribute & ~REGMGR_ATTR_NVS) |
					     REGMGR_ATTR_VOLATILE;
		break;
	default:
		return SCE_REGMGR_ERROR_BAD_ATTRIBUTE;
	}
	if (g_regMgrVolatileMode == 1)
		s_rgMnKey[keyNo].attribute = REGMGR_ATTR_VOLATILE;

	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_ATTRIBUTE;
	permission = (SceUInt8)regMgrFieldToInt();
	s_rgMnKey[keyNo].attr.permission = permission;
	if (permission != 0 && !(permission & 3) && !(permission & 0xc) && !(permission & 0x30))
		return SCE_REGMGR_ERROR_BAD_PERMISSION;

	if (regMgrNextField(&def, &rest) < 0 || regMgrSetKeyFlag(keyNo, 1) < 0)
		return SCE_REGMGR_ERROR_BAD_FLAG;

	/* the level: one for tool machines, one for the others */
	sel = (g_regMgrConfig & 2) == 0;
	for (i = 0; i < 2; i++) {
		if (regMgrNextField(&def, &rest) < 0)
			return SCE_REGMGR_ERROR_BAD_LEVEL;
		if (i != sel)
			continue;
		level = (SceUInt8)regMgrFieldToInt();
		s_rgMnKey[keyNo].attr.level = level;
		switch (level) {
		case 0:
		case 5:
		case 10:
		case 20:
		case 30:
		case 40:
		case 99:
			break;
		default:
			return SCE_REGMGR_ERROR_BAD_LEVEL;
		}
	}

	if (regMgrNextField(&def, &rest) < 0 || regMgrSetKeyFlag(keyNo, 2) < 0)
		return SCE_REGMGR_ERROR_BAD_FLAG2;
	if (regMgrNextField(&def, &rest) < 0 || regMgrSetKeyFlag(keyNo, 4) < 0)
		return SCE_REGMGR_ERROR_BAD_FLAG2;
	if (regMgrNextField(&def, &rest) < 0 || regMgrSetKeyFlag(keyNo, 8) < 0)
		return SCE_REGMGR_ERROR_BAD_FLAG3;
	if (regMgrNextField(&def, &rest) < 0 || regMgrSetKeyFlag(keyNo, 0x10) < 0)
		return SCE_REGMGR_ERROR_BAD_FLAG3;

	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_OPTION;
	s_rgMnKey[keyNo].attr.option0 = regMgrFieldToInt();
	if (s_rgMnKey[keyNo].attr.option0 > 1)
		return SCE_REGMGR_ERROR_BAD_OPTION;
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_OPTION;
	s_rgMnKey[keyNo].attr.option1 = regMgrFieldToInt();
	if (s_rgMnKey[keyNo].attr.option1 > 1)
		return SCE_REGMGR_ERROR_BAD_OPTION;

	/* the initial value */
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_INIT;
	initType = (SceUInt16)regMgrFieldToInt();
	s_rgMnKey[keyNo].initType = initType;
	value = def;
	if (regMgrNextField(&def, &rest) < 0)
		return SCE_REGMGR_ERROR_BAD_INIT;
	if (s_rgMnKey[keyNo].type == REGMGR_TYPE_INT) {
		if (s_rgMnKey[keyNo].size != sizeof(int))
			return SCE_REGMGR_ERROR_BAD_SIZE;
		if (initType != 1)
			return SCE_REGMGR_ERROR_BAD_INIT;
		s_rgMnKey[keyNo].size = sizeof(int);
		s_rgMnKey[keyNo].initSize = sizeof(int);
		s_rgMnKey[keyNo].init.value = regMgrFieldToInt();
		return 0;
	}
	if (s_rgMnKey[keyNo].type > REGMGR_TYPE_BIN)
		return SCE_REGMGR_ERROR_BAD_TYPE;
	if ((SceUInt32)(s_rgMnKey[keyNo].size - 1) >= 0x7c0)
		return SCE_REGMGR_ERROR_BAD_SIZE;
	if (initType == 0 || (initType & REGMGR_INIT_COMPUTED)) {
		s_rgMnKey[keyNo].initSize = s_rgMnKey[keyNo].size;
		s_rgMnKey[keyNo].init.value = regMgrFieldToInt();
		return 0;
	}
	if (initType != 1)
		return SCE_REGMGR_ERROR_BAD_VALUE;

	size = strnlen(g_regMgrWord, 0xff);
	len = size + 1;
	if (*def == '\0') {
		/* the last field: a string, or b'<hex>' */
		if (g_regMgrWord[0] == 'b' && g_regMgrWord[1] == '\'') {
			if (s_rgMnKey[keyNo].type != REGMGR_TYPE_BIN)
				return SCE_REGMGR_ERROR_BAD_TYPE;
			if (size & 1)
				return SCE_REGMGR_ERROR_BAD_VALUE;
			if (len > 2)
				memset(g_regMgrWord, 0, (int)size / 2);
			len = 0;
			if ((int)size > 2) {
				mul = 16;
				out = 0;
				for (i = 2; i != size; i++) {
					c = (SceUInt8)value[i];
					if (c - 'A' < 6)
						digit = c - 'A' + 10;
					else if (c - 'a' < 6)
						digit = c - 'a' + 10;
					else if ((SceUInt8)(c - '0') < 10)
						digit = c - '0';
					else
						return SCE_REGMGR_ERROR_BAD_VALUE;
					g_regMgrWord[out] += digit * mul;
					if (mul == 16) {
						mul = 1;
					} else {
						mul = 16;
						out++;
					}
				}
				len = out;
			}
		}
	} else {
		/* a string with ':' in it: all the rest of the line */
		size = strnlen(value, REGMGR_WORD_SIZE);
		if (value[size - 1] == ':') {
			if (size != 0)
				memcpy(g_regMgrWord, value, size);
			g_regMgrWord[size - 1] = '\0';
			len = size;
		}
	}
	if (g_regMgrInitDataCnt + len > REGMGR_INIT_DATA_SIZE)
		return SCE_REGMGR_ERROR_BAD_VALUE;
	data = &g_regMgrInitData[g_regMgrInitDataCnt];
	if (len != 0)
		memcpy(data, g_regMgrWord, len);
	g_regMgrInitDataCnt += len;
	s_rgMnKey[keyNo].initSize = len;
	s_rgMnKey[keyNo].init.data = data;
	return 0;
}

/* read a line of g_regMgrReadBuf at pos into g_regMgrLine; gives the length of its end of
 * line (1 or 2), 0 when the buffer ends first, or < 0 (names unknown) */
static inline __attribute__((always_inline)) int regMgrReadLine(int pos, int end)
{
	const char *start = &g_regMgrReadBuf[pos];
	const char *p = start;
	char c;

	g_regMgrLineLen = 0;
	for (;;) {
		c = *p++;
		if (c == '\n') {
			g_regMgrLine[g_regMgrLineLen] = '\0';
			return 1;
		}
		if (c == '\r' && start[g_regMgrLineLen + 1] == '\n') {
			g_regMgrLine[g_regMgrLineLen] = '\0';
			return 2;
		}
		g_regMgrLine[g_regMgrLineLen++] = c;
		if (p >= &g_regMgrReadBuf[end]) {
			g_regMgrReadPos = pos + (p - start);
			return 0;
		}
		if (g_regMgrLineLen == REGMGR_LINE_SIZE)
			return -1;
	}
}

/* the section a [REG-<region>] line starts (name unknown) */
static inline __attribute__((always_inline)) int regMgrRegionSection(const char *code)
{
	char name[6];
	int tool = g_regMgrRegion == REGMGR_REGION_TOOL && (g_regMgrConfig & 1);

	if (strncmp("BAS", code, 4) == 0)
		return REGMGR_SECTION_KEY;
	if (strncmp(g_regMgrRegionName[g_regMgrRegion], code, 4) == 0)
		return tool ? REGMGR_SECTION_SKIP : REGMGR_SECTION_UPDATE;
	if (strncmp("DEX", code, 4) == 0)
		return tool ? REGMGR_SECTION_UPDATE : REGMGR_SECTION_SKIP;
	if (g_regMgrIsDolce != 1)
		return REGMGR_SECTION_SKIP;
	snprintf(name, sizeof(name), "%s%s", g_regMgrRegionName[g_regMgrRegion], "_D");
	if (strncmp("BAS_D", code, 6) == 0)
		return REGMGR_SECTION_UPDATE;
	if (strncmp(name, code, 6) == 0)
		return tool ? REGMGR_SECTION_SKIP : REGMGR_SECTION_UPDATE;
	if (strncmp("DEX_D", code, 6) == 0 && tool)
		return REGMGR_SECTION_UPDATE;
	return REGMGR_SECTION_SKIP;
}

/* a line of registry.db0 in a section (name unknown) */
static inline __attribute__((always_inline)) int regMgrParseLine(int *section)
{
	const char *p;
	SceSize used;
	int keyNo, no, level, n, i;
	char c;

	if (g_regMgrLine[0] == '[') {
		if (memcmp("[BASE", g_regMgrLine, 5) == 0) {
			*section = REGMGR_SECTION_BASE;
			return 0;
		}
		if (memcmp("[REG-", g_regMgrLine, 5) == 0) {
			*section = regMgrRegionSection(&g_regMgrLine[5]);
			return 0;
		}
	}
	switch (*section) {
	case REGMGR_SECTION_KEY:
		keyNo = s_rgMnKeyCnt;
		used = 0;
		p = g_regMgrLine;
		c = *p;
		for (level = 0; level < REGMGR_CATEGORY_DEPTH && c == '/'; level++) {
			c = *++p;
			if (REGMGR_ISDIGIT(c)) {
				s_rgMnKey[keyNo].categoryNo[level] = regMgrStrToInt(p, REGMGR_LINE_SIZE, &used);
				p += used;
				c = *p;
			}
		}
		if (c != '=')
			return SCE_REGMGR_ERROR_BAD_CATEGORY;
		n = regMgrParseKey(keyNo, p + 1, &g_regMgrLine[REGMGR_LINE_SIZE] - (p + 1));
		if (n != 0)
			return n;
		s_rgMnKeyCnt = keyNo + 1;
		if (s_rgMnKeyCnt >= REGMGR_BASE_MAX)
			return SCE_REGMGR_ERROR_TOO_MANY_KEYS;
		return 0;
	case REGMGR_SECTION_UPDATE:
		used = 0;
		keyNo = 0;
		no = 0;
		p = g_regMgrLine;
		for (level = 0; *p == '/'; ) {
			p++;
			if (REGMGR_ISDIGIT(*p)) {
				no = regMgrStrToInt(p, REGMGR_LINE_SIZE, &used);
				p += used;
			}
			if (keyNo >= s_rgMnKeyCnt)
				return SCE_REGMGR_ERROR_BAD_CATEGORY;
			while (s_rgMnKey[keyNo].categoryNo[level] != no) {
				if (++keyNo == s_rgMnKeyCnt)
					return SCE_REGMGR_ERROR_BAD_CATEGORY;
			}
			if (++level == REGMGR_CATEGORY_DEPTH)
				break;
		}
		if (*p != '=')
			return SCE_REGMGR_ERROR_BAD_CATEGORY;
		return regMgrParseKey(keyNo, p + 1, &g_regMgrLine[REGMGR_LINE_SIZE] - (p + 1));
	case REGMGR_SECTION_BASE:
		n = ++g_regMgrCategoryCnt;
		if (n >= REGMGR_CATEGORY_MAX)
			return SCE_REGMGR_ERROR_TOO_MANY_KEYS;
		for (i = 0; g_regMgrLine[i] != '='; ) {
			if (g_regMgrLine[i] == '\0' || ++i == REGMGR_LINE_SIZE)
				return SCE_REGMGR_ERROR_PATH_TOO_LONG;
		}
		g_regMgrLine[i] = '\0';
		if (n != regMgrStrToInt(g_regMgrLine, i, NULL))
			return SCE_REGMGR_ERROR_BAD_PATH;
		strncpy(g_regMgrCategoryName[n], &g_regMgrLine[i + 1], REGMGR_CATEGORY_NAME);
		return 0;
	}
	return 0;
}

/* load the key definitions of registry.db0 (name unknown) */
int regMgrLoadDb(void)
{
	SceUID fd;
	SceSize mask;
	int readOff, readSize, start, end, pos, section, rest, eol, n, ret;
	int started = 0;

	memset(g_regMgrCategoryName, 0, sizeof(g_regMgrCategoryName));
	memset(s_rgMnKey, 0, sizeof(s_rgMnKey));
	memset(g_regMgrInitData, 0, sizeof(g_regMgrInitData));
	memset(g_regMgrNumCache, 0, sizeof(g_regMgrNumCache));
	memset(g_regMgrNumCacheValid, 0, sizeof(g_regMgrNumCacheValid));
	memset(g_regMgrBinCache, 0, sizeof(g_regMgrBinCache));
	g_regMgrNumCacheCnt = 0;
	g_regMgrCategoryCnt = 0;
	g_regMgrInitDataCnt = 0;
	s_rgMnBinCrt = 0;
	s_rgMnKeyCnt = 0;

	fd = ksceIoOpen(REGMGR_DB_PATH, SCE_O_RDONLY, 0);
	if (fd < 0)
		return SCE_REGMGR_ERROR_NO_FILE;
	mask = 0;
	readSize = REGMGR_READ_SIZE;
	readOff = 0;
	section = REGMGR_SECTION_SKIP;
	g_regMgrReadPos = 0;
	for (;;) {
		n = ksceIoRead(fd, &g_regMgrReadBuf[readOff], readSize);
		if (n <= 0)
			break;
		end = readOff + n;
		start = readOff;
		g_regMgrReadEnd = end;
		if (!started) {
			/* the comment lines at the top are plain */
			for (pos = g_regMgrReadPos; g_regMgrReadBuf[pos] == ';'; pos += g_regMgrLineLen + eol) {
				g_regMgrReadPos = pos;
				eol = regMgrReadLine(pos, end);
				if (eol <= 0) {
					ret = SCE_REGMGR_ERROR_LINE_TOO_LONG;
					goto out;
				}
			}
			g_regMgrReadPos = pos;
			start = pos;
		}
		for (; start < end; start++)
			g_regMgrReadBuf[start] ^= g_regMgrDbMask[mask++ & 0xf];

		readOff = 0;
		for (pos = g_regMgrReadPos; pos != end; pos = g_regMgrReadPos) {
			eol = regMgrReadLine(pos, end);
			if (eol < 0) {
				ret = SCE_REGMGR_ERROR_LINE_TOO_LONG;
				goto out;
			}
			if (eol == 0) {
				/* carry the start of a line over to the next read */
				readSize = REGMGR_READ_SIZE;
				if (g_regMgrLine[0] != '\0') {
					rest = g_regMgrLineLen & 0x1f;
					if (rest != 0) {
						g_regMgrReadPos = 0x20 - rest;
						strncpy(&g_regMgrReadBuf[g_regMgrReadPos], g_regMgrLine, rest + 0x7e0);
					} else {
						g_regMgrReadPos = 0;
						strncpy(g_regMgrReadBuf, g_regMgrLine, REGMGR_LINE_SIZE);
					}
					readOff = g_regMgrReadPos + g_regMgrLineLen;
					readSize = REGMGR_READ_SIZE - readOff;
				}
				goto next;
			}
			g_regMgrReadPos = pos + g_regMgrLineLen + eol;
			if (g_regMgrLineLen == 0) {
				ret = 0;
				goto out;
			}
			if (g_regMgrLine[0] == ';')
				continue;
			ret = regMgrParseLine(&section);
			if (ret < 0)
				goto out;
		}
		readSize = REGMGR_READ_SIZE;
		g_regMgrReadPos = 0;
		g_regMgrReadEnd = 0;
next:
		started = 1;
	}
	ret = n != 0 ? SCE_REGMGR_ERROR_NO_FILE : 0;
out:
	ksceIoClose(fd);
	return ret;
}

int regMgrSetKeyAccess(RegMgrKeyAccess *access);

/* put the two digits of a category number into a path */
static inline __attribute__((always_inline)) void regMgrPutNumber(char *path, int pos, int no)
{
	if (pos == 0)
		return;
	if (no < 100) {
		path[pos] = '0' + no / 10;
		path[pos + 1] = '0' + no % 10;
	} else {
		path[pos] = '\0';
	}
}

/* a key a reset of the level resets, that the current permission may write (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsResetKey(int keyNo, SceUInt32 level)
{
	SceUInt32 attribute = s_rgMnKey[keyNo].attribute;
	SceUInt32 kind = attribute & REGMGR_ATTR_MASK;
	SceUInt32 permission;

	if (kind == REGMGR_ATTR_VOLATILE)
		return 0;
	if (level == 0x40) {
		if (!(attribute & REGMGR_ATTR_RESET_40))
			return 0;
	} else if (level == 0x100) {
		if (!(attribute & REGMGR_ATTR_RESET_100))
			return 0;
	} else {
		if (level != 0x10 && (int)level > (int)kind)
			return 0;
		if (level == 5 && s_rgMnKey[keyNo].attr.level >= g_regMgrLevelLimit)
			return 0;
	}
	permission = s_rgMnKey[keyNo].attr.permission;
	return permission == 0 || (permission & g_regMgrPermission) == g_regMgrPermission;
}

/* write the version of the registry: category_version (name unknown) */
static inline __attribute__((always_inline)) int regMgrWriteVersion(RegMgrKeyAccess *access, int *version)
{
	int ret;

	access->category = "/REGISTRY";
	access->name = "category_version";
	g_regMgrWriteVersion = 1;
	access->size = sizeof(int);
	access->valueSize = sizeof(int);
	access->buf = version;
	access->handle = 0;
	access->keyNo = 0;
	access->type = REGMGR_TYPE_INT;
	ret = regMgrSetKeyAccess(access);
	g_regMgrWriteVersion = 0;
	return ret;
}

/* reset the keys of a level to their initial values: all of them, or those of a category
 * (name unknown) */
int regMgrInitRegistry(const char *category, SceUInt32 level)
{
	RegMgrKeyAccess access;
	RegMgrValue value;
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];
	int number[4] = { -1, -1, -1, -1 };
	int lo[4], hi[4], pos[4];
	char path[REGMGR_WORD_SIZE];
	const char *p, *name;
	int version, lastVersion;
	int start, end, first, last, key, k, depth, lv, mode, no, i, n, ret;
	int n0, n1, n2, n3;
	const void *cached;

	end = s_rgMnKeyCnt;
	memset(&access, 0, sizeof(access));
	value.data = NULL;
	value.size = 0;
	switch (level) {
	case 5:
		if (category != NULL)
			return SCE_REGMGR_ERROR_BAD_ATTRIBUTE;
		g_regMgrResetLevel = level;
		regMgrWrapRegCloseRegistry();
		mode = 3;
		if (regMgrWrapRegOpenRegistry(1, 3) != 0)
			goto fail;
		goto version;
	case 0:
	case 0x10:
	case 0x20:
	case 0x30:
	case 0x40:
	case 0x100:
	case 0x1000:
		break;
	default:
		ret = SCE_REGMGR_ERROR_BAD_ATTRIBUTE;
		goto out;
	}
	if (category == NULL) {
		g_regMgrResetLevel = level;
		regMgrWrapRegCloseRegistry();
		if (level == 0x10) {
			ret = regMgrWrapRegRemoveRegistry();
			if (ret != 0) {
				g_regMgrStatus &= ~REGMGR_STATUS_INIT;
				regMgrWrapRegCloseCategory(&access.handle);
				g_regMgrOpenCategory[0] = '\0';
				return ret;
			}
			regMgrClearCache();
		}
		mode = 1;
		if (regMgrWrapRegOpenRegistry(1, 1) != 0)
			goto fail;
		if (level == 0) {
			/* move the values of an older registry */
			n = regMgrDeleteObsoleteValues(g_regMgrVersion, s_rgMnKey[0].init.value);
			regMgrConvertLanguage(g_regMgrVersion, s_rgMnKey[0].init.value);
			n += regMgrMoveNpAccountNames(g_regMgrVersion, s_rgMnKey[0].init.value);
			n += regMgrMoveTelValues(g_regMgrVersion, s_rgMnKey[0].init.value);
			n += regMgrMoveNpCurrentAccount(g_regMgrVersion, s_rgMnKey[0].init.value);
			if (n > 0) {
				regMgrClearCache();
				ret = regMgrLoadCache();
				if (ret != 0)
					goto fail;
			}
		}
version:
		version = g_regMgrVersion | REGMGR_VERSION_DIRTY;
		g_regMgrStatus |= REGMGR_STATUS_INIT;
		g_regMgrOpenCategory[0] = '\0';
		start = 1;
		regMgrWriteVersion(&access, &version);
	} else {
		memset(categoryNo, 0, sizeof(categoryNo));
		g_regMgrResetLevel = level;
		depth = regMgrGetCategoryNo(category, strnlen(category, REGMGR_WORD_SIZE), categoryNo);
		if (depth < 0) {
			ret = depth;
			goto out;
		}
		if (end < 2) {
			ret = SCE_REGMGR_ERROR_OUT_OF_RANGE;
			goto out;
		}
		/* the keys of the category: a run of keys */
		first = -1;
		last = -1;
		for (k = 1; k != end; k++) {
			for (lv = 0; lv <= depth; lv++) {
				if (s_rgMnKey[k].categoryNo[lv] != categoryNo[lv])
					break;
			}
			if (lv > depth) {
				last = k;
				if (first == -1)
					first = k;
			} else if (first != -1) {
				break;
			}
		}
		if (first == -1) {
			ret = SCE_REGMGR_ERROR_OUT_OF_RANGE;
			goto out;
		}
		ret = last - first + 1;
		if (ret < 0)
			goto out;
		/* the numbers of its numbered categories */
		p = category + 1;
		for (lv = 0; *p != '\0'; ) {
			if (REGMGR_ISDIGIT(p[0]) && REGMGR_ISDIGIT(p[1]) && (p[2] == '/' || p[2] == '\0'))
				number[lv] = regMgrStrToInt(p, 2, NULL);
			for (i = 0; p[i] != '/'; ) {
				if (p[i] == '\0' || ++i == REGMGR_WORD_SIZE)
					goto numbered;
			}
			if (i == 0)
				break;
			p += i + 1;
			if (++lv == 4)
				break;
		}
numbered:
		start = first;
		end = last + 1;
		mode = 2;
	}

	while (start < end) {
		key = start;
		path[0] = '/';
		path[1] = '\0';
		memset(lo, 0, sizeof(lo));
		memset(hi, 0, sizeof(hi));
		memset(pos, 0, sizeof(pos));
		depth = 0;
		/* the path of its category: a numbered category is every number of its range,
		 * unless the category asked gives it */
		for (i = 0; s_rgMnKey[key].categoryNo[i + 1] != 0; ) {
			name = g_regMgrCategoryName[s_rgMnKey[key].categoryNo[i]];
			if (regMgrIsNumberRange(name)) {
				if (number[i] == -1) {
					n = strnlen(path, sizeof(path));
					lo[i] = regMgrStrToInt(name, 2, NULL);
					hi[i] = regMgrStrToInt(name + 3, 2, NULL);
					path[n] = name[0];
					path[n + 1] = name[1];
					pos[i] = n;
					path[n + 3] = '\0';
					path[n + 2] = '/';
				} else {
					ret = regMgrCheckNumberRange(name, NULL, number[i]);
					if (ret != 0)
						goto fail;
					snprintf(path, sizeof(path), "%s%02d/", path, number[i]);
				}
			} else {
				strncat(path, name, 0xff - strnlen(path, sizeof(path)));
			}
			depth = ++i;
			if (i == 4)
				break;
		}
		for (i = 0; i < 4; i++) {
			if (lo[i] >= 100 || hi[i] >= 100) {
				ret = SCE_REGMGR_ERROR_BAD_CATEGORY;
				goto fail;
			}
		}

		for (n0 = lo[0]; n0 <= hi[0]; n0++) {
		regMgrPutNumber(path, pos[0], n0);
		for (n1 = lo[1]; n1 <= hi[1]; n1++) {
		regMgrPutNumber(path, pos[1], n1);
		for (n2 = lo[2]; n2 <= hi[2]; n2++) {
		regMgrPutNumber(path, pos[2], n2);
		for (n3 = lo[3]; n3 <= hi[3]; n3++) {
			regMgrPutNumber(path, pos[3], n3);
			start = key;
			for (k = key; k < end; ) {
				if (regMgrIsResetKey(k, level)) {
					ret = regMgrGetKeyInitValue(k, &value);
					if (ret != 0)
						goto fail;
					if (!(s_rgMnKey[k].attr.flags & REGMGR_KEY_CACHED)) {
						if (s_rgMnKey[k].attr.cacheType != REGMGR_CACHE_NUM)
							goto write;
						no = regMgrGetPathNumber(path);
						if (no < 0) {
							ret = no;
							goto fail;
						}
						no = s_rgMnKey[k].cache.num.index + (no - s_rgMnKey[k].cache.num.first);
						if (no < 0) {
							ret = no;
							goto fail;
						}
						if (!((g_regMgrNumCacheValid[no >> 3] >> (no & 7)) & 1))
							goto write;
						cached = &g_regMgrNumCache[no];
					} else if (s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_INT) {
						cached = &s_rgMnKey[k].cache.value;
					} else {
						cached = &g_regMgrBinCache[s_rgMnKey[k].cache.offset];
					}
					if (memcmp(value.data, cached, value.size) == 0)
						goto next;
write:
					access.category = path;
					access.type = s_rgMnKey[k].type;
					access.size = s_rgMnKey[k].size;
					access.name = g_regMgrCategoryName[s_rgMnKey[k].categoryNo[depth]];
					access.keyNo = k;
					access.buf = (void *)value.data;
					access.valueSize = value.size;
					ret = regMgrSetKeyAccess(&access);
					if (ret != 0)
						goto fail;
				}
next:
				start = k + 1;
				if (depth == 0)
					break;
				start = ++k;
				/* the next key, while in the same category */
				if (s_rgMnKey[k].categoryNo[depth - 1] !=
				    s_rgMnKey[key].categoryNo[depth - 1] ||
				    (depth != 4 && s_rgMnKey[k].categoryNo[depth + 1] != 0) || k == end)
					break;
			}
		}
		}
		}
		}
	}
	ret = 0;
	if (category == NULL) {
		if (level == 0) {
			regMgrWrapRegCloseCategory(&access.handle);
			g_regMgrOpenCategory[0] = '\0';
			regMgrMoveUpdateValues(g_regMgrVersion, s_rgMnKey[0].init.value);
		}
		lastVersion = s_rgMnKey[0].init.value;
		ret = regMgrWriteVersion(&access, &lastVersion);
	}
fail:
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	if (mode == 2)
		return ret;
	if (mode == 3) {
		regMgrClearCache();
		ret = regMgrWrapRegReopenRegistry(1);
		if (ret != 0)
			return ret;
		ret = regMgrLoadCache();
		if (ret != 0)
			return ret;
	}
	return regMgrWrapRegReopenRegistry(2);

out:
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	return ret;
}

/* tell the notify function of a change; clearing a network configuration's conf_flag
 * also resets its keys (name unknown) */
int regMgrNotifyChange(const char *category, const RegMgrKeyInfo *param)
{
	RegMgrNotifyInfo *info = g_regMgrNotifyInfo;
	char net[16];
	int len, ret;

	if (g_regMgrNotifyFunc == NULL)
		return 0;
	info->type = param->type;
	info->valueSize = param->valueSize;
	if (param->type == REGMGR_TYPE_STR) {
		info->valueSize = strnlen(param->data, REGMGR_WORD_SIZE);
		if (info->valueSize != 0)
			memcpy(info->data, param->data, info->valueSize);
	} else if (param->type == REGMGR_TYPE_BIN) {
		if (param->valueSize != 0)
			memcpy(info->data, param->data, param->valueSize);
	} else {
		info->value = param->value;
	}
	len = strnlen(category, REGMGR_WORD_SIZE);
	if (len == 0 || (memcpy(info->path, category, len), category[len - 1] != '/'))
		info->path[len++] = '/';
	strncpy(&info->path[len], param->name, REGMGR_WORD_SIZE - len);
	if (g_regMgrNotifyFunc == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	ret = g_regMgrNotifyFunc(info);
	if (ret == 0 && g_regMgrVolatileMode != 1 &&
	    strnlen(info->path, REGMGR_WORD_SIZE) == 0x1f &&
	    strncmp("conf_flag", &info->path[0x16], 10) == 0 && info->value == 0) {
		/* "/CONFIG/NET/<n>/COMMON/conf_flag" */
		memcpy(net, info->path, 15);
		net[15] = '\0';
		g_regMgrVolatileMode = 1;
		ret = regMgrInitRegistry(net, 0x20);
		g_regMgrVolatileMode = 0;
		if (ret == 0)
			g_regMgrOpenCategory[0] = '\0';
	}
	return ret;
}

/* set a key's value: create it, or change it, and cache it (name unknown) */
int regMgrSetKeyAccess(RegMgrKeyAccess *access)
{
	RegMgrKeyInfo param;
	RegMgrKey *key;
	int ret, no;

	if (strncmp(g_regMgrOpenCategory, access->category, sizeof(g_regMgrOpenCategory)) != 0) {
		regMgrWrapRegCloseCategory(&access->handle);
		g_regMgrOpenCategory[0] = '\0';
		ret = regMgrCreateCategory(access);
		if (ret != 0) {
			access->handle = 0;
			return ret;
		}
		strncpy(g_regMgrOpenCategory, access->category, sizeof(g_regMgrOpenCategory));
	}
	ret = regMgrWrapRegCreateKey(access->handle, access->category, access->name, access->type,
				     access->size);
	if (ret == 0) {
		ret = regMgrWrapRegSetKey(access->handle, access->category, access->name, access->buf,
					  access->valueSize);
	} else {
		if (ret != SCE_REGMGR_ERROR_KEY_EXISTS)
			return ret;
		if (g_regMgrResetLevel == 0 && g_regMgrWriteVersion == 0)
			return 0;
		ret = regMgrSetKey(access->handle, access->category, access->name, access->buf,
				   access->valueSize, access->keyNo);
	}
	if (ret != 0)
		return ret;

	if (g_regMgrVolatileMode != 1) {
		key = &s_rgMnKey[access->keyNo];
		switch (key->attr.cacheType) {
		case REGMGR_CACHE_INT:
			key->cache.value = *(int *)access->buf;
			key->attr.flags |= REGMGR_KEY_CACHED;
			break;
		case REGMGR_CACHE_BIN:
			if (access->valueSize != 0)
				memcpy(&g_regMgrBinCache[key->cache.offset], access->buf, access->valueSize);
			key->attr.flags |= REGMGR_KEY_CACHED;
			break;
		case REGMGR_CACHE_NUM:
			no = regMgrGetPathNumber(access->category);
			if (no < 0)
				return no;
			no = key->cache.num.index + (no - key->cache.num.first);
			if (no < 0)
				return no;
			g_regMgrNumCacheValid[no >> 3] |= 1 << (no & 7);
			g_regMgrNumCache[no] = *(int *)access->buf;
			break;
		}
	}
	if (!(g_regMgrConfig & 1) && g_regMgrNotifyFunc != NULL &&
	    ((g_regMgrIsQaf == 1 && s_rgMnKey[access->keyNo].attr.option0 == 1) ||
	     s_rgMnKey[access->keyNo].attr.option1 == 1)) {
		param.size = sizeof(param);
		param.name = access->name;
		param.type = access->type;
		param.valueSize = access->valueSize;
		if (param.type == REGMGR_TYPE_INT)
			param.value = *(int *)access->buf;
		else
			param.data = access->buf;
		return regMgrNotifyChange(access->category, &param);
	}
	return 0;
}

/* read the values of the cached keys into the caches (name unknown) */
int regMgrLoadCache(void)
{
	RegMgrKeyAccess access;
	char path[REGMGR_WORD_SIZE];
	int lo[4], hi[4], pos[4];
	const char *name;
	int end, key, k, depth, len, no, i, n, ret;
	int n0, n1, n2, n3;

	end = s_rgMnKeyCnt;
	memset(&access, 0, sizeof(access));
	no = 0;
	for (key = 0; key < end; key = k) {
		path[0] = '/';
		path[1] = '\0';
		memset(lo, 0, sizeof(lo));
		memset(hi, 0, sizeof(hi));
		memset(pos, 0, sizeof(pos));
		depth = 0;
		for (i = 0; s_rgMnKey[key].categoryNo[i + 1] != 0; ) {
			name = g_regMgrCategoryName[s_rgMnKey[key].categoryNo[i]];
			if (regMgrIsNumberRange(name)) {
				n = strnlen(path, sizeof(path));
				lo[i] = regMgrStrToInt(name, 2, NULL);
				hi[i] = regMgrStrToInt(name + 3, 2, NULL);
				path[n] = name[0];
				pos[i] = n;
				path[n + 1] = name[1];
				path[n + 3] = '\0';
				path[n + 2] = '/';
			} else {
				strncat(path, name, 0xff - strnlen(path, sizeof(path)));
			}
			depth = ++i;
			if (i == 4)
				break;
		}
		len = strnlen(path, sizeof(path));
		for (i = 0; i < 4; i++) {
			if (lo[i] >= 100 || hi[i] >= 100) {
				ret = SCE_REGMGR_ERROR_BAD_CATEGORY;
				goto out;
			}
		}

		for (n0 = lo[0]; n0 <= hi[0]; n0++) {
		regMgrPutNumber(path, pos[0], n0);
		for (n1 = lo[1]; n1 <= hi[1]; n1++) {
		regMgrPutNumber(path, pos[1], n1);
		for (n2 = lo[2]; n2 <= hi[2]; n2++) {
		regMgrPutNumber(path, pos[2], n2);
		for (n3 = lo[3]; n3 <= hi[3]; n3++) {
			regMgrPutNumber(path, pos[3], n3);
			for (k = key; k < end; ) {
				if (s_rgMnKey[k].attr.cacheType == 0 ||
				    (s_rgMnKey[k].attr.flags & REGMGR_KEY_CACHED))
					goto next;
				if (s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_NUM) {
					no = regMgrGetPathNumber(path);
					if (no < 0) {
						ret = no;
						goto out;
					}
					no = s_rgMnKey[k].cache.num.index + (no - s_rgMnKey[k].cache.num.first);
					if (no < 0) {
						ret = no;
						goto out;
					}
					if ((g_regMgrNumCacheValid[no >> 3] >> (no & 7)) & 1)
						goto next;
				}
				if (s_rgMnKey[k].attr.permission != 0 &&
				    (s_rgMnKey[k].attr.permission & g_regMgrPermission) != g_regMgrPermission)
					goto next;
				access.category = path;
				access.size = s_rgMnKey[k].size;
				access.type = s_rgMnKey[k].type;
				name = g_regMgrCategoryName[s_rgMnKey[k].categoryNo[depth]];
				if (s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_INT)
					access.buf = &s_rgMnKey[k].cache.value;
				else if (s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_BIN)
					access.buf = &g_regMgrBinCache[s_rgMnKey[k].cache.offset];
				else
					access.buf = &g_regMgrNumCache[no];
				if (path[len - 1] == '/')
					path[len - 1] = '\0';
				access.keyNo = k;
				access.name = name;
				access.valueSize = access.size;
				if (regMgrGetKeyAccess(&access) != 0)
					goto next;
				if (s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_INT ||
				    s_rgMnKey[k].attr.cacheType == REGMGR_CACHE_BIN)
					s_rgMnKey[k].attr.flags |= REGMGR_KEY_CACHED;
				else
					g_regMgrNumCacheValid[no >> 3] |= 1 << (no & 7);
next:
				k++;
				if (depth == 0)
					break;
				if (s_rgMnKey[k].categoryNo[depth - 1] !=
				    s_rgMnKey[key].categoryNo[depth - 1] ||
				    (depth != 4 && s_rgMnKey[k].categoryNo[depth + 1] != 0) || k == end)
					break;
			}
		}
		}
		}
		}
	}
	ret = 0;
out:
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	return ret;
}

/* an error of the registry is reported without its 0x8000 bit */
static inline __attribute__((always_inline)) int regMgrMaskError(int error)
{
	if (((SceUInt32)error & 0xffff0000) == 0x800d0000)
		error &= ~0x8000;
	return error;
}

/* the errors that stop the system with a blue screen (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsBlueScreenError(int error)
{
	return (SceUInt32)(error - 0x800d2711) <= 0x271a;
}

int sceRegMgrIsBlueScreen(int error)
{
	int lock, ret, blue;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = error;
	blue = regMgrIsBlueScreenError(regMgrMaskError(ret));
	regMgrComApiEpilogue(lock);
	return blue;
}

int sceRegMgrGetRegVersion(void)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = s_rgMnKey[0].cache.value;
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the key of a category: gives the depth of the category and its numbers (name unknown) */
static inline __attribute__((always_inline)) int regMgrFindCategoryKey(const char *category, SceUInt16 *categoryNo)
{
	int depth, k, i;

	depth = regMgrGetCategoryNo(category, strnlen(category, REGMGR_WORD_SIZE), categoryNo);
	if (depth < 0)
		return depth;
	if (depth >= 4)
		return SCE_REGMGR_ERROR_BAD_CATEGORY;
	for (k = 1; k < s_rgMnKeyCnt; k++) {
		for (i = 0; i <= depth; i++) {
			if (s_rgMnKey[k].categoryNo[i] != categoryNo[i])
				break;
		}
		if (i > depth)
			return depth;
	}
	return SCE_REGMGR_ERROR_OUT_OF_RANGE;
}

/* the key of a category path (name unknown) */
static inline __attribute__((always_inline)) int regMgrSearchKey(const SceUInt16 *categoryNo)
{
	int k, i;

	for (k = 0; k < s_rgMnKeyCnt; k++) {
		for (i = 0; i < REGMGR_CATEGORY_DEPTH; i++) {
			if (s_rgMnKey[k].categoryNo[i] != categoryNo[i])
				break;
		}
		if (i == REGMGR_CATEGORY_DEPTH)
			break;
	}
	return k;
}

/* the key of a name in a category (name unknown) */
static inline __attribute__((always_inline)) int regMgrFindKey(const char *name, int depth, SceUInt16 *categoryNo)
{
	SceSize len;
	int no, k;

	len = strnlen(name, 0x1a);
	if (len > 0x19)
		return SCE_REGMGR_ERROR_BAD_NAME;
	no = regMgrFindCategory(name, len, categoryNo[depth] + 1);
	if (no < 1)
		return SCE_REGMGR_ERROR_BAD_NAME;
	categoryNo[depth + 1] = no;
	k = regMgrSearchKey(categoryNo);
	if (k == s_rgMnKeyCnt)
		return SCE_REGMGR_ERROR_OUT_OF_RANGE;
	return k;
}

/* whether the keys a notify function is told of include this one (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsNotifyKey(int keyNo)
{
	return (g_regMgrIsQaf == 1 && s_rgMnKey[keyNo].attr.option0 == 1) ||
	       s_rgMnKey[keyNo].attr.option1 == 1;
}

/* whether the current mode may read or write a key (name unknown) */
static inline __attribute__((always_inline)) int regMgrCheckAccess(int keyNo, int read, int write)
{
	const RegMgrKey *key = &s_rgMnKey[keyNo];
	int allowed;

	if (g_regMgrStatus & REGMGR_STATUS_INIT)
		goto permission;
	if (g_regMgrStatus & REGMGR_STATUS_FLAGS) {
		if (g_regMgrStatus & REGMGR_STATUS_FLAGS_ALT)
			allowed = (key->attr.flags & (write << 1)) != 0;
		else
			allowed = (key->attr.flags & read) != 0;
	} else if (!(g_regMgrStatus & REGMGR_STATUS_NOTIFY)) {
		allowed = 1;
	} else if (!(g_regMgrConfig & 1)) {
		allowed = regMgrIsNotifyKey(keyNo);
	} else {
		return SCE_REGMGR_ERROR_NO_ACCESS;
	}
	if ((key->attribute & REGMGR_ATTR_MASK) <= 0xf || !allowed)
		return SCE_REGMGR_ERROR_NO_ACCESS;
permission:
	if (key->attr.permission != 0 &&
	    (key->attr.permission & g_regMgrPermission) != g_regMgrPermission)
		return SCE_REGMGR_ERROR_BAD_PERMISSION;
	return 0;
}

/* the index of a numbered category's key in g_regMgrNumCache (name unknown) */
static inline __attribute__((always_inline)) int regMgrNumCacheIndex(const RegMgrKey *key, const char *category)
{
	int no;

	no = regMgrGetPathNumber(category);
	if (no < 0)
		return no;
	return key->cache.num.index + (no - key->cache.num.first);
}

/* set the values of keys of a category (name unknown) */
int regMgrSetKeys(const char *category, RegMgrKeyInfo *info, int *count)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH] = { 0 };
	RegMgrKey *key;
	const void *buf;
	SceSize size;
	int handle = 0;
	int depth, k, no, done = 0, ret;

	if (info == NULL || count == NULL || *count < 1) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	depth = regMgrFindCategoryKey(category, categoryNo);
	if (depth < 0) {
		ret = depth;
		goto out;
	}
	for (; done < *count; done++, info++) {
		if (info->size != sizeof(*info)) {
			ret = SCE_REGMGR_ERROR_BAD_KEYS_INFO;
			goto out;
		}
		k = regMgrFindKey(info->name, depth, categoryNo);
		if (k < 0) {
			ret = k;
			goto out;
		}
		ret = regMgrCheckAccess(k, REGMGR_KEY_WRITE, REGMGR_KEY_WRITE_ALT >> 1);
		if (ret != 0)
			goto out;
		key = &s_rgMnKey[k];
		if (key->type != info->type) {
			ret = SCE_REGMGR_ERROR_BAD_TYPE;
			goto out;
		}
		size = info->valueSize;
		if (size > key->size || size == 0) {
			ret = SCE_REGMGR_ERROR_BAD_SIZE;
			goto out;
		}
		if (info->type == REGMGR_TYPE_STR) {
			buf = info->data;
			if ((int)size < (int)(strnlen(buf, 0x7bf) + 1)) {
				ret = SCE_REGMGR_ERROR_STR_TOO_LONG;
				goto out;
			}
			size = strnlen(buf, 0x7bf) + 1;
		} else if (info->type == REGMGR_TYPE_INT) {
			if (size != sizeof(int)) {
				ret = SCE_REGMGR_ERROR_BAD_SIZE;
				goto out;
			}
			buf = &info->value;
		} else if (info->type == REGMGR_TYPE_BIN) {
			buf = info->data;
		} else {
			ret = SCE_REGMGR_ERROR_BAD_TYPE;
			goto out;
		}

		if ((key->attribute & REGMGR_ATTR_MASK) == REGMGR_ATTR_VOLATILE) {
			/* not in the registry: only kept here */
			if (info->type == REGMGR_TYPE_INT) {
				key->init.value = info->value;
			} else if (key->attr.cacheType == REGMGR_CACHE_BIN) {
				if (size != 0)
					memcpy(&g_regMgrBinCache[key->cache.offset], buf, size);
				key->attr.flags |= REGMGR_KEY_CACHED;
			} else if (!(key->attr.flags & REGMGR_KEY_NVS)) {
				continue;
			}
		} else {
			if (key->attr.flags & REGMGR_KEY_CACHED) {
				if (key->attr.cacheType == REGMGR_CACHE_INT) {
					if (info->value == key->cache.value)
						goto notify;
				} else if (memcmp(buf, &g_regMgrBinCache[key->cache.offset], size) == 0) {
					goto notify;
				}
			} else if (key->attr.cacheType == REGMGR_CACHE_NUM) {
				no = regMgrNumCacheIndex(key, category);
				if (no < 0) {
					ret = no;
					goto out;
				}
				if (((g_regMgrNumCacheValid[no >> 3] >> (no & 7)) & 1) &&
				    info->value == g_regMgrNumCache[no])
					goto notify;
			}
			if (handle == 0) {
				ret = regMgrWrapRegOpenCategory(category, 2, &handle);
				if (ret != 0)
					goto out;
			}
			ret = regMgrSetKey(handle, category, info->name, buf, size, k);
			if (ret != 0)
				goto out;
			switch (key->attr.cacheType) {
			case REGMGR_CACHE_INT:
				key->cache.value = info->value;
				key->attr.flags |= REGMGR_KEY_CACHED;
				break;
			case REGMGR_CACHE_BIN:
				if (size != 0)
					memcpy(&g_regMgrBinCache[key->cache.offset], buf, size);
				key->attr.flags |= REGMGR_KEY_CACHED;
				break;
			case REGMGR_CACHE_NUM:
				no = regMgrNumCacheIndex(key, category);
				if (no < 0) {
					ret = no;
					goto out;
				}
				g_regMgrNumCache[no] = info->value;
				g_regMgrNumCacheValid[no >> 3] |= 1 << (no & 7);
				break;
			}
		}
notify:
		if (!(g_regMgrConfig & 1) && regMgrIsNotifyKey(k)) {
			ret = regMgrNotifyChange(category, info);
			if (ret != 0)
				goto out;
		}
	}
	ret = 0;
out:
	*count = done;
	regMgrWrapRegCloseCategory(&handle);
	return ret;
}

int regMgrSetKeyInt(const char *category, const char *name, int value)
{
	RegMgrKeyInfo info;
	int count = 1;

	info.size = sizeof(info);
	info.name = name;
	info.type = REGMGR_TYPE_INT;
	info.valueSize = sizeof(int);
	info.value = value;
	return regMgrSetKeys(category, &info, &count);
}

/* get the values of keys of a category; in the volatile mode, from the NVS if the
 * registry fails (name unknown) */
int regMgrGetKeys(const char *category, RegMgrKeyInfo *info, int *count)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH] = { 0 };
	RegMgrKeyInfo *first = info;
	const RegMgrKey *key;
	void *buf;
	SceSize size;
	int handle = 0;
	int depth, k, no, done = 0, ret;

	if (info == NULL || count == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	ret = regMgrFindCategoryKey(category, categoryNo);
	if (ret < 0)
		goto fail;
	depth = ret;
	for (; done < *count; done++, info++) {
		if (info->size != sizeof(*info)) {
			ret = SCE_REGMGR_ERROR_BAD_KEYS_INFO;
			goto fail;
		}
		k = regMgrFindKey(info->name, depth, categoryNo);
		if (k < 0) {
			ret = k;
			goto fail;
		}
		ret = regMgrCheckAccess(k, REGMGR_KEY_READ, REGMGR_KEY_READ_ALT >> 1);
		if (ret != 0)
			goto fail;
		key = &s_rgMnKey[k];
		if (key->type != info->type) {
			ret = SCE_REGMGR_ERROR_BAD_TYPE;
			goto fail;
		}
		if (info->valueSize < key->size) {
			ret = SCE_REGMGR_ERROR_BAD_SIZE;
			goto fail;
		}
		if (key->type == REGMGR_TYPE_INT) {
			if (info->valueSize != sizeof(int)) {
				ret = SCE_REGMGR_ERROR_BAD_SIZE;
				goto fail;
			}
			buf = &info->value;
		} else {
			buf = (void *)info->data;
		}
		if (!(key->attr.flags & REGMGR_KEY_CACHED)) {
			if (key->attr.cacheType == REGMGR_CACHE_NUM) {
				no = regMgrNumCacheIndex(key, category);
				if (no < 0) {
					ret = no;
					goto fail;
				}
				if ((g_regMgrNumCacheValid[no >> 3] >> (no & 7)) & 1)
					goto get;
			}
			if (handle == 0) {
				ret = regMgrWrapRegOpenCategory(category, 2, &handle);
				if (ret != 0)
					goto fail;
			}
		}
get:
		ret = regMgrGetKey(k, handle, category, info->name, (SceInt16 *)&info->valueSize, buf);
		if (ret != 0)
			goto fail;
	}
	regMgrWrapRegCloseCategory(&handle);
	if (ret == 0)
		goto out;
	goto volatile_;
fail:
	regMgrWrapRegCloseCategory(&handle);
volatile_:
	if (g_regMgrVolatileMode != 1)
		goto out;
	for (done = 0, info = first; done < *count; done++, info++) {
		if (info->size != sizeof(*info)) {
			ret = SCE_REGMGR_ERROR_BAD_KEYS_INFO;
			goto out;
		}
		size = info->valueSize;
		if (info->type == REGMGR_TYPE_INT) {
			if (size != sizeof(int)) {
				ret = SCE_REGMGR_ERROR_BAD_SIZE;
				goto out;
			}
			buf = &info->value;
		} else {
			if (size > 0x7c0) {
				ret = SCE_REGMGR_ERROR_BAD_SIZE;
				goto out;
			}
			buf = (void *)info->data;
			if (buf == NULL) {
				ret = SCE_REGMGR_ERROR_BAD_VALUE;
				goto out;
			}
		}
		if (regMgrWrapGetNVSData(category, info->name, buf, size) < 1 && size != 0)
			memset(buf, 0, size);
	}
	ret = 0;
out:
	*count = done;
	return ret;
}

int regMgrGetKeyInt(const char *category, const char *name, int *value)
{
	RegMgrKeyInfo info;
	int count = 1;
	int ret;

	info.size = sizeof(info);
	info.name = name;
	info.type = REGMGR_TYPE_INT;
	info.valueSize = sizeof(int);
	info.value = 0;
	ret = regMgrGetKeys(category, &info, &count);
	if (ret == 0)
		*value = info.value;
	return ret;
}

/* the tool's settings and event log definitions, for licensees and for internal developers */
static const char *const g_regMgrToolDefLicensee[] = {	/* names unknown */
	"vs0:/tool/setting_def_for_licensee.xml",
	"vs0:/tool/eventlog_def_for_licensee.xml",
	"vs0:/tool/setting_def_for_licensee_dolce.xml",
	"vs0:/tool/eventlog_def_for_licensee_dolce.xml",
};
static const char *const g_regMgrToolDefInternal[] = {
	"vs0:/tool/setting_def_for_internal_developer.xml",
	"vs0:/tool/eventlog_def_for_internal_developer.xml",
	"vs0:/tool/setting_def_for_internal_developer_dolce.xml",
	"vs0:/tool/eventlog_def_for_internal_developer_dolce.xml",
};

/* whether the "[serial:<n>]" of a tool definition file differs from the one in
 * /DEVENV/HOST (name unknown) */
int regMgrCheckToolDefSerial(SceUInt32 eventLog)
{
	const char *const *defs;
	const char *name;
	SceUInt32 *serial;
	char *p, *q;
	SceUID fd;
	int n, i, value;

	i = eventLog > 1 ? 0 : eventLog;
	if (g_regMgrIsDolce == 1)
		i += 2;
	defs = g_regMgrIsQaf == 1 ? g_regMgrToolDefInternal : g_regMgrToolDefLicensee;
	if (eventLog == 0) {
		name = "ui_serial";
		serial = &g_regMgrUiSerial;
	} else {
		name = "ev_serial";
		serial = &g_regMgrEvSerial;
	}
	fd = ksceIoOpen(defs[i], SCE_O_RDONLY, 0);
	if (fd < 0)
		return 1;
	n = ksceIoRead(fd, g_regMgrWord, sizeof(g_regMgrWord));
	ksceIoClose(fd);
	if (n <= 0)
		return 1;
	p = g_regMgrWord;
	do {
		for (i = 0; p[i] != '['; ) {
			if (p[i] == '\0' || ++i >= n)
				return 1;
		}
		n -= i + 1;
		p += i + 1;
		if (n <= 0)
			return 1;
	} while (memcmp("serial:", p, 7) != 0);
	for (i = 0, q = p + 7; *q != ']'; q++) {
		if (*q == '\0' || ++i >= n)
			return 1;
	}
	if (i > 10 || n - i < 1)
		return 1;
	*q = '\0';
	value = regMgrStrToInt(p + 7, i, NULL);
	if (value == 0)
		return 1;
	n = regMgrGetKeyInt("/DEVENV/HOST", name, &i);
	if (n == 0) {
		if (value == i)
			return 0;
	} else if (n < 0) {
		return 1;
	}
	*serial = value;
	return 1;
}

/* the region of the product; sets the configuration and the permission (name unknown) */
int regMgrGetRegion(int masquerade)
{
	int code, region, value;

	code = SceSblAIMgrForDriver_14345161();
	if (code < 0)
		return code;
	switch (code) {
	case 0x100:	/* the tools */
	case 0x101:
	case 0x102:
		region = REGMGR_REGION_TOOL;
		if (masquerade == 1 &&
		    regMgrGetKeyInt("/DEVENV/TOOL", "region_masquerade", &value) == 0 &&
		    (SceUInt32)(value - 1) <= 0xc)
			region = value;
		if (SceSblACMgrForDriver_E87D1777() == 1 && code != 0x102) {
			g_regMgrConfig = 2;
			g_regMgrPermission = g_regMgrIsQaf == 1 ? 8 : 2;
			return region;
		}
		code = masquerade == 1 ? 2 : REGMGR_REGION_TOOL;
		if (region != REGMGR_REGION_TOOL)
			code = region;
		g_regMgrConfig = 3;
		g_regMgrPermission = g_regMgrIsQaf == 1 ? 0x20 : 0x10;
		return code;
	case 0x103: region = 1; break;
	case 0x104: region = 2; break;
	case 0x105: region = 5; break;
	case 0x106: region = 6; break;
	case 0x107: region = 4; break;
	case 0x108: region = 10; break;
	case 0x109: region = 3; break;
	case 0x10a: region = 7; break;
	case 0x10b: region = 8; break;
	case 0x10c: region = 9; break;
	case 0x10d: region = 13; break;
	default:
		return SCE_REGMGR_ERROR_BAD_PRODUCT;
	}
	g_regMgrConfig = 1;
	g_regMgrPermission = g_regMgrIsQaf == 1 ? 4 : 1;
	return region;
}

/* enter an API: lock the registry and give its state; the debug tools can make the
 * calls fail after a count down (/DEVENV/TOOL/error_masquerade) */
int regMgrComApiPrologue(int *lock)
{
	SceUInt32 tpidruro;
	int ret;

	if (lock == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (g_regMgrMutex == -1) {
		ret = g_regMgrInitResult;
		*lock = ret;
		if (ret < 0)
			goto out;
	} else {
		ret = sceKernelLockMutex(g_regMgrMutex, 1, NULL);
		*lock = ret;
		if (ret < 0)
			goto out;
		if (g_regMgrInitResult != 0 && g_regMgrVolatileMode != 1) {
			ret = g_regMgrInitResult;
			if (((SceUInt32)ret & 0xffff0000) == 0x800d0000) {
				ret |= 0x800d8000;
				goto out;
			}
			if (ret < 0)
				goto out;
		}
	}
	if (g_regMgrErrorMasquerade << 16) {
		if (g_regMgrErrorCountdown > 0) {
			g_regMgrErrorCountdown--;
			SCE_DBG_PRINTF(0, 1919, "[REGMGR] Count Down %d\n", g_regMgrErrorCountdown);
			if (g_regMgrErrorCountdown > 0)
				goto out;
		}
		ret = g_regMgrErrorMasquerade;
		regMgrSetLastError(ret);
	}
out:
	if (g_regMgrApiNest == 0) {
		tpidruro = readTpidruro();
		writeTpidruro(tpidruro << 16);
		g_regMgrSavedTpidruro = tpidruro;
	}
	g_regMgrApiNest++;
	return ret;
}

/* leave an API: tell the driver error callback of a blue screen error, unlock
 * (name unknown) */
int regMgrComApiEpilogue(int lock)
{
	int error;

	if (g_regMgrDrvErrCallback != -1) {
		if (SceThreadmgrForDriver_0892D8DF(g_regMgrDrvErrCallback) < 0) {
			g_regMgrDrvErrCallback = -1;
		} else {
			error = regMgrGetLastError();
			if (regMgrIsBlueScreenError(regMgrMaskError(error)) &&
			    sceKernelNotifyCallback(g_regMgrDrvErrCallback, error) != 0)
				g_regMgrDrvErrCallback = -1;
		}
	}
	if (--g_regMgrApiNest != 0) {
		if (g_regMgrApiNest < 0)
			g_regMgrApiNest = 0;
	} else {
		writeTpidruro(g_regMgrSavedTpidruro);
	}
	if (lock >= 0)
		return sceKernelUnlockMutex(g_regMgrMutex, 1);
	return 0;
}

/* the work that initializes the registry, queued by module_start: reads registry.db0
 * and the caches, and resets the registry to the level the last shutdown asked for
 * (/CONFIG/SYSTEM/init_settings) or after an update (name unknown) */
int regMgrInitWork(void *arg)
{
	RegMgrKeyAccess access;
	int version = 0, machineType = 0, value;
	int lock, ret, region, level, found;
	int noRegistry, toolCheck, keep;
	SceUInt32 machine;

	(void)arg;
	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	ret = sceKernelSetEventFlag(g_regMgrEventFlag, 1);
	if (ret < 0)
		goto out;
	memset(g_regMgrCategoryName, 0, sizeof(g_regMgrCategoryName));
	memset(s_rgMnKey, 0, sizeof(s_rgMnKey));
	memset(g_regMgrInitData, 0, sizeof(g_regMgrInitData));
	memset(g_regMgrNumCache, 0, sizeof(g_regMgrNumCache));
	memset(g_regMgrNumCacheValid, 0, sizeof(g_regMgrNumCacheValid));
	memset(g_regMgrBinCache, 0, sizeof(g_regMgrBinCache));
	memset(g_regMgrCallback, 0xff, sizeof(g_regMgrCallback));
	g_regMgrUiSerial = 0;
	g_regMgrResetLevel = 0;
	g_regMgrEvSerial = 0;
	g_regMgrVersion = 0;
	g_regMgrApiNest = 0;
	g_regMgrWriteVersion = 0;
	g_regMgrInitNotice = 0;
	g_regMgrSavedTpidruro = 0;
	g_regMgrInitDataCnt = 0;
	g_regMgrRegionError = 0;
	g_regMgrCategoryCnt = 0;
	s_rgMnKeyCnt = 0;
	g_regMgrNotifyInfo = NULL;
	g_regMgrNotifyFunc = NULL;
	g_regMgrNumCacheCnt = 0;
	g_regMgrPermission = 0;
	g_regMgrLevelLimit = 30;
	g_regMgrInitResult = 0;
	s_rgMnBinCrt = 0;
	g_regMgrErrorCountdown = 0;
	g_regMgrRegion = 0;
	g_regMgrConfig = 1;
	g_regMgrErrorMasquerade = 0;
	g_regMgrVolatileMode = 0;
	g_regMgrDrvErrCallback = -1;
	if (SceSysrootForDriver_834439A7() != 0)
		g_regMgrVolatileMode = 1;
	if (SceSysrootForDriver_B0E1FC67() != 0)
		g_regMgrVolatileMode = 1;
	g_regMgrIsQaf = SceQafMgrForDriver_0E588747() == 1;
	g_regMgrIsDolce = SceSblAIMgrForDriver_71608CA3() == 1;

	region = regMgrGetRegion(0);
	if (region < 0) {
		g_regMgrRegionError = region;
		ret = region;
		goto out;
	}
	g_regMgrStatus = 0;
	g_regMgrRegion = region;
	ret = regMgrLoadDb();
	if (ret != 0)
		goto out;
	ret = regMgrWrapRegInit();
	if (ret != 0)
		goto out;

	level = 0;
	keep = 0;
	toolCheck = 0;
	noRegistry = 0;
	if (regMgrWrapRegOpenRegistry(2, 1) != 0) {
		noRegistry = 1;
		goto check;
	}
	access.handle = 0;
	access.keyNo = 0;
	access.category = "/REGISTRY";
	access.name = "category_version";
	access.type = REGMGR_TYPE_INT;
	access.size = sizeof(int);
	access.buf = &version;
	access.valueSize = sizeof(int);
	found = regMgrGetKeyAccess(&access);
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	ret = regMgrLoadCache();
	if (ret != 0)
		goto out;
	if (found != 0)
		goto check;
	g_regMgrVersion = version;
	if (version != s_rgMnKey[0].init.value) {
		/* another version: convert the registry */
		toolCheck = 1;
		goto check;
	}

	/* the reset the last shutdown asked for */
	keep = 1;
	toolCheck = 1;
	if (regMgrGetKeyInt("/CONFIG/SYSTEM", "init_settings", &value) == 0) {
		level = value;
		switch (value) {
		case 0x10:
			keep = 0;
			toolCheck = 0;
			break;
		case 0x20:
		case 0x30:
		case 0x40:
		case 0x100:
			keep = 0;
			break;
		case 0x80:
			level = 0;
			g_regMgrInitNotice |= 3;
			break;
		default:
			level = 0;
			break;
		}
	}
	if ((g_regMgrConfig & 2) &&
	    regMgrGetKeyInt("/DEVENV/TOOL", "error_masquerade", &value) == 0 && value != 0 &&
	    ((SceUInt32)value & 0xffff0000) == 0x800d0000) {
		regMgrGetKeyInt("/DEVENV/TOOL", "error_countdown", &g_regMgrErrorCountdown);
		if (g_regMgrConfig & 1) {
			snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/DEVENV/TOOL", "error_masquerade");
			regMgrInitRegistry(g_regMgrLine, 0x20);
			snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/DEVENV/TOOL", "error_countdown");
			regMgrInitRegistry(g_regMgrLine, 0x20);
		} else {
			ret = regMgrSetKeyInt("/DEVENV/TOOL", "error_masquerade", 0x800d0000);
			if (ret != 0)
				goto out;
		}
		if (((SceUInt32)value & 0x800d8000) == 0x800d8000) {
			ret = value;
			goto out;
		}
		g_regMgrErrorMasquerade = value;
	}

check:
	if (g_regMgrIsQaf == 1 && regMgrGetKeyInt("/REGISTRY", "check_qaf", &value) != 0)
		keep = 0;
	machine = 0;
	if (g_regMgrConfig & 2) {
		if (toolCheck) {
			/* a tool: the region may be masqueraded, and the machine type may
			 * have changed */
			g_regMgrConfig = 2;
			region = regMgrGetRegion(1);
			if (region < 0) {
				ret = region;
				goto out;
			}
			if (region != g_regMgrRegion) {
				g_regMgrRegion = region;
				ret = regMgrLoadDb();
				if (ret != 0)
					goto out;
				ret = regMgrLoadCache();
				if (ret != 0)
					goto out;
			}
			if (regMgrGetKeyInt("/DEVENV/TOOL", "machine_type", &machineType) != 0)
				goto reset;
			machine = g_regMgrConfig;
			if (g_regMgrIsQaf == 1)
				machine |= 0x80;
			if (((SceUInt32)machineType & ~0x40) != machine)
				goto reset;
		}
	}
	if (keep)
		goto settings;
reset:
	ret = regMgrInitRegistry(NULL, level);
	if (ret != 0)
		goto out;
	if (g_regMgrVolatileMode != 1)
		g_regMgrInitNotice |= 3;
	regMgrInitRegistry("/REGISTRY/ERROR", 0x20);
	g_regMgrResetLevel = level;
	if (noRegistry && SceSysrootForDriver_55392965() == 1) {
		ret = regMgrSetKeyInt("/CONFIG/TEL", "enable_wwan", 0);
		if (ret != 0)
			goto out;
	}

settings:
	if (g_regMgrInitNotice & 1) {
		ret = regMgrSetKeyInt("/CONFIG/SYSTEM", "init_settings", 0);
		if (ret != 0)
			goto out;
	}
	if (!(g_regMgrConfig & 1) && g_regMgrVolatileMode != 1) {
		if (regMgrCheckToolDefSerial(0) == 1)
			g_regMgrInitNotice |= 3;
		if (regMgrCheckToolDefSerial(1) == 1)
			g_regMgrInitNotice |= 4;
	}
	if (g_regMgrConfig & 2) {
		if (g_regMgrVolatileMode != 1 &&
		    regMgrGetKeyInt("/DEVENV/TOOL", "gpi_switch", &value) == 0)
			SceDebugLedForDriver_51C5325A(value);
		if ((g_regMgrConfig & 1) && g_regMgrRegion == REGMGR_REGION_TOOL)
			g_regMgrRegion = 2;
		if (g_regMgrIsDolce == 1)
			machine |= 0x40;
		if (machineType != machine) {
			if ((machineType ^ machine) & 0x40) {
				regMgrInitRegistry(NULL, 0x40);
				g_regMgrResetLevel = level;
			}
			regMgrSetKeyInt("/DEVENV/TOOL", "machine_type", machine);
		}
	}
	if (regMgrGetKeyInt("/REGISTRY/ERROR", "err_enable", &value) == 0)
		g_regMgrErrEnable = value;
	else
		g_regMgrErrEnable = 0;
	regMgrWrapRegReopenRegistry(2);
	ret = g_regMgrInitResult;
out:
	g_regMgrInitResult = ret;
	regMgrComApiEpilogue(lock);
	return 0;
}

int regMgrGetVolatileMode(void)	/* names unknown */
{
	return g_regMgrVolatileMode;
}

void regMgrClearStatus(void)
{
	g_regMgrStatus = 0;
}

void regMgrSetStatus(SceUInt32 status)
{
	g_regMgrStatus |= status;
}

/* the region: the error of finding it, if there was one */
int regMgrGetRegionNo(void)
{
	if (g_regMgrRegionError != 0)
		return g_regMgrRegionError;
	return g_regMgrRegion;
}

/* the code of a region ("J1", ...) */
const char *regMgrGetRegionName(int region)
{
	if ((SceUInt32)(region - 1) < 0xd)
		return g_regMgrRegionName[region];
	return NULL;
}

int regMgrIsQaf(void)
{
	return g_regMgrIsQaf;
}

SceUInt32 regMgrGetErrEnable(void)
{
	return g_regMgrErrEnable;
}

/* the bytes of the OpenPsId that make each of the four kinds of token: 0 for a filler
 * byte, else the index of the OpenPsId byte plus 1 (names unknown) */
static const SceUInt8 g_regMgrPsIdMap[4][32] = {
	{ 1, 0, 2, 0, 3, 0, 4, 0, 0, 5, 0, 6, 0, 7, 0, 8,
	  0, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 15, 0, 16 },
	{ 0, 0, 0, 3, 0, 0, 6, 7, 0, 0, 0, 0, 16, 0, 0, 0,
	  5, 0, 1, 14, 4, 10, 11, 2, 8, 13, 0, 9, 12, 0, 15, 0 },
	{ 0, 0, 16, 0, 1, 2, 3, 4, 0, 0, 0, 5, 6, 7, 8, 9,
	  10, 0, 0, 11, 0, 0, 0, 12, 0, 13, 0, 14, 0, 15, 0, 0 },
	{ 16, 0, 14, 1, 12, 0, 0, 2, 0, 0, 11, 3, 8, 4, 6, 0,
	  15, 0, 0, 0, 10, 5, 0, 9, 0, 0, 7, 0, 0, 0, 0, 13 },
};

/* make a token of the console's OpenPsId, its bytes scattered among filler bytes
 * (name unknown) */
int regMgrMakePsIdToken(SceUInt8 *token)
{
	SceOpenPsId psid;
	SceUInt32 time;
	int ret, i;

	ret = ksceSblAimgrGetOpenPsId(&psid);
	if (ret != 0)
		return ret;
	time = SceProcessmgrForDriver_02179E12();
	for (i = 0; i < 32; i++) {
		if (i == 8)
			token[i] = time & 3;
		else if (g_regMgrPsIdMap[time & 3][i] != 0)
			token[i] = psid.open_psid[g_regMgrPsIdMap[time & 3][i] - 1];
		else
			token[i] = time * i * i;
	}
	return 0;
}

/* whether a token is of this console's OpenPsId (name unknown) */
int regMgrCheckPsIdToken(const SceUInt8 *token)
{
	SceOpenPsId psid;
	int ret, i;

	ret = ksceSblAimgrGetOpenPsId(&psid);
	if (ret != 0)
		return ret;
	for (i = 0; i < 32; i++) {
		if (g_regMgrPsIdMap[token[8]][i] != 0 &&
		    psid.open_psid[g_regMgrPsIdMap[token[8]][i] - 1] != token[i])
			return 0;
	}
	return 1;
}

int module_start(SceSize args, void *argp)
{
	SceUInt timeout = 5000000;
	unsigned int bits = 0;
	int ret;

	(void)args;
	(void)argp;
	if (g_regMgrMutex != -1) {
		ret = 0x800d0005;	/* name unknown */
	} else {
		ret = sceKernelCreateEventFlag("SceRegMgr", 0, 0, NULL);
		if (ret < 0)
			goto out;
		g_regMgrEventFlag = ret;
		ret = sceKernelCreateMutex("SceRegMgr", 2, 0, NULL);
		if (ret < 0)
			goto out;
		g_regMgrMutex = ret;
		ret = sceKernelScheduleWorkTask(0x10023, "SceRegMgrInit", regMgrInitWork, NULL);
		if (ret < 0)
			goto out;
		ret = sceKernelWaitEventFlag(g_regMgrEventFlag, 1, 0, &bits, &timeout);
	}
out:
	if (g_regMgrEventFlag != -1) {
		ret = sceKernelDeleteEventFlag(g_regMgrEventFlag);
		g_regMgrEventFlag = -1;
	}
	if (ret < 0)
		g_regMgrInitResult = ret;
	return 0;
}

SceUInt32 regMgrIsRelease(void)	/* names unknown */
{
	return g_regMgrConfig & REGMGR_CONFIG_RELEASE;
}

SceUInt32 regMgrIsTool(void)
{
	return g_regMgrConfig & REGMGR_CONFIG_TOOL;
}

/* the path of a key's category, with the numbers of its numbered categories left to
 * fill in; gives the depth of the key (name unknown) */
static inline __attribute__((always_inline)) int regMgrKeyPath(int key, char *path, int *lo, int *hi, int *pos)
{
	const char *name;
	int depth, n;

	path[0] = '/';
	path[1] = '\0';
	memset(lo, 0, 4 * sizeof(int));
	memset(hi, 0, 4 * sizeof(int));
	memset(pos, 0, 4 * sizeof(int));
	for (depth = 0; depth < 4; depth++) {
		if (s_rgMnKey[key].categoryNo[depth + 1] == 0)
			break;
		name = g_regMgrCategoryName[s_rgMnKey[key].categoryNo[depth]];
		if (regMgrIsNumberRange(name)) {
			n = strnlen(path, REGMGR_WORD_SIZE);
			lo[depth] = regMgrStrToInt(name, 2, NULL);
			hi[depth] = regMgrStrToInt(name + 3, 2, NULL);
			path[n] = name[0];
			path[n + 1] = name[1];
			pos[depth] = n;
			path[n + 3] = '\0';
			path[n + 2] = '/';
		} else {
			strncat(path, name, 0xff - strnlen(path, REGMGR_WORD_SIZE));
		}
	}
	return depth;
}

/* whether the key after one is in the same category (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsNextKeyOf(int n, int key, int depth)
{
	return depth != 0 &&
	       s_rgMnKey[n + 1].categoryNo[depth - 1] == s_rgMnKey[key].categoryNo[depth - 1] &&
	       (depth == 4 || s_rgMnKey[n + 1].categoryNo[depth + 1] == 0);
}

/* whether a reset of the level resets a key (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsLevelKey(const RegMgrKey *key, int level)
{
	SceUInt32 attribute = key->attribute;

	if (level == 0x40)
		return (attribute & REGMGR_ATTR_RESET_40) != 0;
	if (level == 0x100)
		return (attribute & REGMGR_ATTR_RESET_100) != 0;
	if (level == 0x1000)
		return (attribute & REGMGR_ATTR_VOLATILE) && !(key->attr.flags & REGMGR_KEY_NVS);
	return (int)(attribute & ~(REGMGR_ATTR_RESET_40 | REGMGR_ATTR_RESET_100)) >= level;
}

/* tell the notify function the values of the keys a reset of the level resets; then
 * reset the network configurations that are off (name unknown) */
int regMgrNotifyLevel(int level)
{
	RegMgrNotifyInfo *info = g_regMgrNotifyInfo;
	int cnt = s_rgMnKeyCnt;
	RegMgrKeyAccess access = { 0 };
	int lo[4], hi[4], pos[4];
	char net[0x17];
	const RegMgrKey *k;
	char *path;
	int key, n, i, depth, len, n0, n1, n2, n3, flag, ret;

	if (g_regMgrNotifyFunc == NULL || info == NULL) {
		ret = 0x800d0016;	/* name unknown */
		goto out;
	}
	ret = regMgrWrapRegReopenRegistry(1);
	if (ret != 0)
		goto out;
	path = info->path;
	for (key = 1; key < cnt; ) {
		depth = regMgrKeyPath(key, path, lo, hi, pos);
		len = strnlen(path, REGMGR_WORD_SIZE);
		for (i = 0; i < 4; i++) {
			if (lo[i] >= 100 || hi[i] >= 100) {
				ret = SCE_REGMGR_ERROR_BAD_CATEGORY;
				goto out;
			}
		}

		for (n0 = lo[0]; n0 <= hi[0]; n0++) {
		regMgrPutNumber(path, pos[0], n0);
		for (n1 = lo[1]; n1 <= hi[1]; n1++) {
		regMgrPutNumber(path, pos[1], n1);
		for (n2 = lo[2]; n2 <= hi[2]; n2++) {
		regMgrPutNumber(path, pos[2], n2);
		for (n3 = lo[3]; n3 <= hi[3]; n3++) {
			regMgrPutNumber(path, pos[3], n3);
			for (n = key; ; ) {
				k = &s_rgMnKey[n];
				if (regMgrIsLevelKey(k, level) &&
				    (k->attr.permission == 0 ||
				     (k->attr.permission & g_regMgrPermission) == g_regMgrPermission) &&
				    regMgrIsNotifyKey(n)) {
					access.valueSize = k->size;
					access.type = k->type;
					access.name = g_regMgrCategoryName[k->categoryNo[depth]];
					if (path[len - 1] == '/')
						path[len - 1] = '\0';
					if (access.type == REGMGR_TYPE_INT)
						access.buf = &info->value;
					else
						access.buf = info->data;
					access.keyNo = n;
					access.category = path;
					access.size = access.valueSize;
					ret = regMgrGetKeyAccess(&access);
					if (ret != 0)
						goto out;
					path[len - 1] = '/';
					strncpy(&path[len], access.name, REGMGR_WORD_SIZE - len);
					info->type = access.type;
					if (access.type == REGMGR_TYPE_STR)
						info->valueSize = strnlen(info->data, access.size);
					else
						info->valueSize = access.size;
					ret = g_regMgrNotifyFunc(info);
				}
				/* the next key, while in the same category */
				if (!regMgrIsNextKeyOf(n, key, depth))
					break;
				if (++n >= cnt)
					break;
			}
		}
		}
		}
		}
		key = n + 1;
	}
out:
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	if ((g_regMgrInitNotice & 1) && g_regMgrVolatileMode != 1) {
		for (i = 1; i != 0x1f; i++) {
			snprintf(net, sizeof(net), "/CONFIG/NET/%02d/COMMON", i);
			if (regMgrGetKeyInt(net, "conf_flag", &flag) != 0)
				break;
			if (flag == 0) {
				snprintf(net, sizeof(net), "/CONFIG/NET/%02d/", i);
				g_regMgrVolatileMode = 1;
				n = regMgrInitRegistry(net, 0x20);
				g_regMgrVolatileMode = flag;
				if (n != 0)
					break;
			}
		}
	}
	n = regMgrWrapRegReopenRegistry(2);
	if (n != 0 && ret == 0)
		ret = n;
	return ret;
}

/* whether the settings are to be initialized (names unknown) */
SceUInt32 regMgrIsInitSettings(void)
{
	return g_regMgrInitNotice & 1;
}

/* whether the tool's settings (or event log) definitions changed */
SceUInt32 regMgrIsToolDefChanged(int eventLog)
{
	if (eventLog != 0)
		return (g_regMgrInitNotice >> 2) & 1;
	return (g_regMgrInitNotice >> 1) & 1;
}

/* the serial of the tool's settings (or event log) definitions, if it changed */
SceUInt32 regMgrGetToolDefSerial(int eventLog)
{
	if (eventLog != 0)
		return g_regMgrEvSerial;
	return g_regMgrUiSerial;
}

/* the path of the tool's settings (or event log) definitions */
const char *regMgrGetToolDefPath(SceUInt32 eventLog)
{
	const char *const *defs;

	if (eventLog > 1)
		eventLog = 0;
	if (g_regMgrIsDolce == 1)
		eventLog += 2;
	defs = g_regMgrIsQaf == 1 ? g_regMgrToolDefInternal : g_regMgrToolDefLicensee;
	return defs[eventLog];
}

/* the lock alone, without the error masquerade (name unknown) */
int regMgrLock(int *lock)
{
	int ret = g_regMgrInitResult;

	if (g_regMgrMutex == -1) {
		*lock = ret;
		return ret;
	}
	ret = sceKernelLockMutex(g_regMgrMutex, 1, NULL);
	*lock = ret;
	if (ret >= 0 && g_regMgrInitResult != 0 && g_regMgrVolatileMode != 1) {
		ret = g_regMgrInitResult;
		if (((SceUInt32)ret & 0xffff0000) == 0x800d0000)
			return ret | 0x800d8000;
	}
	return ret;
}

int regMgrUnlock(int lock)
{
	if (lock >= 0)
		return sceKernelUnlockMutex(g_regMgrMutex, 1);
	return 0;
}

/* an error, as /REGISTRY/ERROR/<nn> keeps it: the last ten errors (names unknown) */
typedef struct RegMgrErrorRecord {
	SceUInt64 authId;				/* of the process that called */
	int error;
	SceSize size;
	SceUInt16 count;				/* the number of the error */
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];
	SceUInt8 number;				/* of a numbered category */
	SceUInt8 api;					/* REGMGR_API_* */
	SceUInt8 type;
	SceUInt8 reserved;
} RegMgrErrorRecord;

#define REGMGR_ERROR_RECORD_MAX	10
#define REGMGR_ERROR_PATH_MAX	0x60

/* the APIs an error is recorded for (names unknown) */
#define REGMGR_API_RESET_REGISTRY_LV	0
#define REGMGR_API_SET_KEY_INT		1
#define REGMGR_API_GET_KEY_INT		2
#define REGMGR_API_SET_KEY_STR		3
#define REGMGR_API_GET_KEY_STR		4
#define REGMGR_API_SET_KEY_BIN		5
#define REGMGR_API_GET_KEY_BIN		6
#define REGMGR_API_GET_KEYS_INFO	7
#define REGMGR_API_SET_KEYS		8
#define REGMGR_API_GET_KEYS		9
#define REGMGR_API_GET_INIT_VALS	10

/* the errors /REGISTRY/ERROR/err_enable leaves out (names unknown) */
#define REGMGR_ERR_ENABLE_NO_PERMISSION	0x10000
#define REGMGR_ERR_ENABLE_NO_FLAG	0x20000

/* record an error of an API in /REGISTRY/ERROR, unless the same one is there
 * (name unknown) */
void regMgrRecordError(int error, int api, const char *category, const char *name,
		       const void *data, SceSize size)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH] = { 0 };
	RegMgrErrorRecord record = { 0 };
	char errPath[0x20];
	char path[REGMGR_WORD_SIZE];
	const RegMgrKeyInfo *info;
	RegMgrKeyInfo keyInfo;
	SceUInt64 authId = 0;
	SceUInt8 number, type;
	int count, len, bad, max, i, n;

	if (g_regMgrVolatileMode == 1 || g_regMgrErrEnable == 0)
		return;
	if (!(g_regMgrErrEnable & ~(REGMGR_ERR_ENABLE_NO_PERMISSION | REGMGR_ERR_ENABLE_NO_FLAG | 1)) &&
	    (((g_regMgrErrEnable & REGMGR_ERR_ENABLE_NO_PERMISSION) &&
	      error == SCE_REGMGR_ERROR_BAD_PERMISSION) ||
	     ((g_regMgrErrEnable & REGMGR_ERR_ENABLE_NO_FLAG) && error == SCE_REGMGR_ERROR_BAD_FLAG)))
		return;
	memset(path, 0, sizeof(path));
	g_regMgrStatus |= REGMGR_STATUS_INIT;
	type = 0;
	if ((SceUInt32)(api - REGMGR_API_SET_KEYS) < 3) {
		info = data;
		name = NULL;
		if (info == NULL) {
			size = sizeof(int);
			data = &size;
		} else {
			size = info->valueSize;
			type = info->type;
			name = info->name;
			if (type == REGMGR_TYPE_INT)
				data = &info->value;
			else
				data = info->data;
		}
	}
	if (category == NULL) {
		strncpy(path, "(NULL)/", sizeof(path));
		len = 0;
	} else {
		strncpy(path, category, sizeof(path) - 1);
		len = strnlen(path, sizeof(path) - 1);
	}
	if (name != NULL) {
		if (path[len - 1] != '/') {
			path[len] = '/';
			path[len + 1] = '\0';
		}
		strncat(path, name, sizeof(path) - 1 - strnlen(path, sizeof(path)));
		len = strnlen(path, sizeof(path));
	}
	if (len == 0) {
		number = 0;
		bad = 0;
	} else {
		bad = regMgrGetCategoryNo(path, len, categoryNo);
		if (bad < 0)
			categoryNo[0] = 0xffff;
		else
			bad = 0;
		n = regMgrGetPathNumber(path);
		number = n < 0 ? 0 : n;
	}

	max = 0;
	for (i = 0; i < REGMGR_ERROR_RECORD_MAX; i++) {
		snprintf(errPath, sizeof(errPath), "/REGISTRY/ERROR/%02d", i);
		keyInfo.size = sizeof(keyInfo);
		keyInfo.name = "info";
		keyInfo.type = REGMGR_TYPE_BIN;
		keyInfo.valueSize = sizeof(record);
		keyInfo.data = &record;
		count = 1;
		if (regMgrGetKeys(errPath, &keyInfo, &count) != 0)
			goto out;
		if (max < record.count)
			max = record.count;
		if (record.count == 0)
			break;
		/* the same error is there already */
		if (record.error == error &&
		    (((SceUInt32)error & 0xffff0000) != 0x800d0000 ||
		     ((SceUInt32)error & 0xffff8000) == 0x800d8000 || (error & 0x7fff) > 10000 ||
		     memcmp(categoryNo, record.categoryNo, sizeof(categoryNo)) == 0))
			goto out;
	}
	record.count = max + 1;
	memcpy(record.categoryNo, categoryNo, sizeof(categoryNo));
	record.authId = 0;
	record.size = size;
	record.number = number;
	record.api = api;
	record.type = type;
	record.error = error;
	if (sceSblACMgrGetPaid(0, &authId) != 0)
		authId = 0;
	record.authId = authId;
	snprintf(errPath, sizeof(errPath), "/REGISTRY/ERROR/%02d", max % REGMGR_ERROR_RECORD_MAX);
	keyInfo.size = sizeof(keyInfo);
	keyInfo.name = "info";
	keyInfo.type = REGMGR_TYPE_BIN;
	keyInfo.valueSize = sizeof(record);
	keyInfo.data = &record;
	count = 1;
	if (regMgrSetKeys(errPath, &keyInfo, &count) != 0)
		goto out;
	/* and the value, or the path that was wrong */
	if (bad != 0 && category != NULL) {
		if (len < REGMGR_ERROR_PATH_MAX) {
			data = path;
			size = strnlen(path, REGMGR_ERROR_PATH_MAX);
		} else {
			data = &path[len - REGMGR_ERROR_PATH_MAX];
			size = REGMGR_ERROR_PATH_MAX;
		}
	}
	if (data == NULL) {
		data = "(NULL)";
		size = 4;
	} else if (size > REGMGR_ERROR_PATH_MAX) {
		size = REGMGR_ERROR_PATH_MAX;
	}
	keyInfo.size = sizeof(keyInfo);
	keyInfo.name = "value";
	keyInfo.type = REGMGR_TYPE_BIN;
	keyInfo.valueSize = size;
	keyInfo.data = data;
	count = 1;
	regMgrSetKeys(errPath, &keyInfo, &count);
out:
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
}

static inline __attribute__((always_inline)) void regMgrApiError(int error, int api, const char *category,
					   const char *name, const void *data, SceSize size)
{
	if (g_regMgrErrEnable != 0)
		regMgrRecordError(error, api, category, name, data, size);
}

int sceRegMgrResetRegistryLv(const char *category, int level)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrInitRegistry(category, level);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_RESET_REGISTRY_LV, category, NULL, &level, sizeof(level));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeyInt(const char *category, const char *name, int value)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrSetKeyInt(category, name, value);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_SET_KEY_INT, category, name, &value, sizeof(value));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeyInt(const char *category, const char *name, int *value)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrGetKeyInt(category, name, value);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_GET_KEY_INT, category, name, value, sizeof(*value));
	regMgrComApiEpilogue(lock);
	return ret;
}

/* a key of a category, given as a buffer of the type (name unknown) */
static inline __attribute__((always_inline)) int regMgrAccessKey(int set, const char *category, const char *name,
					   int type, void *buf, SceSize size)
{
	RegMgrKeyInfo info;
	int count = 1;

	info.size = sizeof(info);
	info.name = name;
	info.type = type;
	info.valueSize = size;
	info.data = buf;
	if (set)
		return regMgrSetKeys(category, &info, &count);
	return regMgrGetKeys(category, &info, &count);
}

int sceRegMgrSetKeyStr(const char *category, const char *name, const char *buf, SceSize size)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrAccessKey(1, category, name, REGMGR_TYPE_STR, (void *)buf, size);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_SET_KEY_STR, category, name, buf, size);
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeyStr(const char *category, const char *name, char *buf, SceSize size)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrAccessKey(0, category, name, REGMGR_TYPE_STR, buf, size);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_GET_KEY_STR, category, name, buf, size);
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeyBin(const char *category, const char *name, const void *buf, SceSize size)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrAccessKey(1, category, name, REGMGR_TYPE_BIN, (void *)buf, size);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_SET_KEY_BIN, category, name, buf, size);
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeyBin(const char *category, const char *name, void *buf, SceSize size)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrAccessKey(0, category, name, REGMGR_TYPE_BIN, buf, size);
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_GET_KEY_BIN, category, name, buf, size);
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the types and sizes of keys of a category */
int sceRegMgrGetKeysInfo(const char *category, RegMgrKeyInfo *info, int count)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];
	const RegMgrKey *key;
	SceSize len;
	int lock, ret, depth, i, k, no;

	lock = 0;
	ret = regMgrComApiPrologue(&lock);
	i = 0;
	if (ret < 0)
		goto fail;
	if (info == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	if (count < 1) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	memset(categoryNo, 0, sizeof(categoryNo));
	depth = regMgrFindCategoryKey(category, categoryNo);
	if (depth < 0) {
		ret = depth;
		goto fail;
	}
	for (; i < count; i++) {
		if (info[i].size != sizeof(*info)) {
			ret = SCE_REGMGR_ERROR_BAD_KEYS_INFO;
			goto fail;
		}
		len = strnlen(info[i].name, 0x1c);
		if (len >= 0x1a ||
		    (no = regMgrFindCategory(info[i].name, len, categoryNo[depth] + 1)) < 1) {
			if (g_regMgrStatus != REGMGR_STATUS_INIT) {
				ret = SCE_REGMGR_ERROR_BAD_NAME;
				goto fail;
			}
			goto none;
		}
		categoryNo[depth + 1] = no;
		k = regMgrSearchKey(categoryNo);
		if (k == s_rgMnKeyCnt) {
			if (g_regMgrStatus != REGMGR_STATUS_INIT) {
				ret = SCE_REGMGR_ERROR_OUT_OF_RANGE;
				goto fail;
			}
			goto none;
		}
		ret = regMgrCheckAccess(k, REGMGR_KEY_READ, REGMGR_KEY_READ_ALT >> 1);
		if (ret == SCE_REGMGR_ERROR_NO_ACCESS)
			goto fail;
		if (ret != 0) {
			if (g_regMgrStatus != REGMGR_STATUS_INIT)
				goto fail;
none:
			info[i].type = 0;
			info[i].valueSize = 0;
			continue;
		}
		key = &s_rgMnKey[k];
		info[i].type = key->type;
		info[i].valueSize = key->size;
	}
	ret = 0;
	goto out;
fail:
	regMgrApiError(ret, REGMGR_API_GET_KEYS_INFO, category,
		       info != NULL ? info[i].name : NULL, &count, sizeof(count));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeys(const char *category, RegMgrKeyInfo *info, int count)
{
	int lock, ret, done = count;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (info != NULL && count > 0)
			ret = regMgrSetKeys(category, info, &done);
		else
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_SET_KEYS, category, NULL,
			       info != NULL ? &info[done] : NULL, count);
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeys(const char *category, RegMgrKeyInfo *info, int count)
{
	int lock, ret, done = count;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (info != NULL && count > 0)
			ret = regMgrGetKeys(category, info, &done);
		else
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_GET_KEYS, category, NULL,
			       info != NULL ? &info[done] : NULL, count);
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the initial values (registry.db0's) of keys of a category */
int sceRegMgrGetInitVals(const char *category, RegMgrKeyInfo *info, int count)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];
	RegMgrValue value;
	void *buf;
	int lock, ret, depth, i, k;

	lock = 0;
	ret = regMgrComApiPrologue(&lock);
	i = 0;
	if (ret < 0)
		goto fail;
	if (info == NULL || category == NULL || count < 1) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	memset(categoryNo, 0, sizeof(categoryNo));
	value.data = NULL;
	value.size = 0;
	depth = regMgrFindCategoryKey(category, categoryNo);
	if (depth < 0) {
		ret = depth;
		goto fail;
	}
	do {
		if (info[i].size != sizeof(*info)) {
			ret = SCE_REGMGR_ERROR_BAD_KEYS_INFO;
			goto fail;
		}
		if (info[i].name == NULL) {
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			goto fail;
		}
		k = regMgrFindKey(info[i].name, depth, categoryNo);
		if (k < 0) {
			ret = k;
			goto fail;
		}
		ret = regMgrCheckAccess(k, REGMGR_KEY_READ, REGMGR_KEY_READ_ALT >> 1);
		if (ret != 0)
			goto fail;
		if (s_rgMnKey[k].type != info[i].type) {
			ret = SCE_REGMGR_ERROR_BAD_TYPE;
			goto fail;
		}
		ret = regMgrGetKeyInitValue(k, &value);
		if (ret != 0) {
			if (ret >= 0)
				goto out;
			goto fail;
		}
		if (info[i].valueSize < value.size) {
			ret = SCE_REGMGR_ERROR_BAD_SIZE;
			goto fail;
		}
		if (info[i].type == REGMGR_TYPE_INT) {
			buf = &info[i].value;
		} else {
			buf = (void *)info[i].data;
			if (buf == NULL) {
				ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
		}
		if (value.size != 0)
			memcpy(buf, value.data, value.size);
	} while (++i != count);
	ret = 0;
	goto out;
fail:
	regMgrApiError(ret, REGMGR_API_GET_INIT_VALS, category, NULL,
		       info != NULL ? &info[i] : NULL, count);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

#define REGMGR_API_REGISTER_CALLBACK		11	/* names unknown */
#define REGMGR_API_ADD_REGISTRY_CALLBACK	12
#define REGMGR_API_START_CALLBACK		13
#define REGMGR_API_STOP_CALLBACK		14
#define REGMGR_API_UNREGISTER_CALLBACK		15
#define REGMGR_API_REGISTER_DRV_ERR_CALLBACK	16
#define REGMGR_API_UNREGISTER_DRV_ERR_CALLBACK	17

#define SCE_REGMGR_ERROR_CALLBACK_EXISTS	0x800d0025	/* names unknown */
#define SCE_REGMGR_ERROR_CALLBACK_FULL		0x800d0026
#define SCE_REGMGR_ERROR_NO_CALLBACK		0x800d0027

/* the slot of a callback (name unknown) */
static inline __attribute__((always_inline)) int regMgrFindCallback(SceUID cbid)
{
	int i;

	for (i = 0; i < REGMGR_CALLBACK_MAX; i++) {
		if (g_regMgrCallback[i].cbid == cbid)
			return i;
	}
	return SCE_REGMGR_ERROR_NO_CALLBACK;
}

int sceRegMgrRegisterCallback(SceUID cbid)
{
	int lock, ret, i, slot;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto fail;
	slot = -1;
	for (i = 0; i < REGMGR_CALLBACK_MAX; i++) {
		if (g_regMgrCallback[i].cbid == cbid) {
			ret = SCE_REGMGR_ERROR_CALLBACK_EXISTS;
			goto fail;
		}
		if (slot == -1 && g_regMgrCallback[i].cbid == -1)
			slot = i;
	}
	if (slot == -1) {
		ret = SCE_REGMGR_ERROR_CALLBACK_FULL;
		goto fail;
	}
	g_regMgrCallback[slot].cbid = cbid;
	regMgrComApiEpilogue(lock);
	return slot;
fail:
	regMgrApiError(ret, REGMGR_API_REGISTER_CALLBACK, NULL, NULL, &cbid, sizeof(cbid));
	regMgrComApiEpilogue(lock);
	return ret;
}

/* whether a key is in a category, up to the depth (name unknown) */
static inline __attribute__((always_inline)) int regMgrIsKeyOf(int k, const SceUInt16 *categoryNo, int depth)
{
	int i;

	for (i = 0; i <= depth; i++) {
		if (s_rgMnKey[k].categoryNo[i] != categoryNo[i])
			return 0;
	}
	return 1;
}

/* have a callback notified of the changes of a key, or of the keys of a category */
int sceRegMgrAddRegistryCallback(SceUID cbid, const char *path)
{
	SceUInt16 categoryNo[REGMGR_CATEGORY_DEPTH];
	RegMgrCallback *callback;
	int lock, ret, slot, depth, first, last, k, i;

	lock = 0;
	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto fail;
	if (path == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	i = regMgrFindCallback(cbid);
	if (i < 0) {
		ret = i;
		goto fail;
	}
	callback = &g_regMgrCallback[i];
	for (slot = 0; slot < REGMGR_CATEGORY_DEPTH; slot++) {
		if (callback->keyNo[slot] == REGMGR_KEYNO_NONE)
			break;
	}
	if (slot == REGMGR_CATEGORY_DEPTH) {
		ret = SCE_REGMGR_ERROR_CALLBACK_FULL;
		goto fail;
	}
	memset(categoryNo, 0, sizeof(categoryNo));
	depth = regMgrGetCategoryNo(path, strnlen(path, REGMGR_WORD_SIZE), categoryNo);
	if (depth < 0 || s_rgMnKeyCnt < 2)
		goto invalid;
	/* the keys of the path */
	first = -1;
	last = -1;
	for (k = 1; k < s_rgMnKeyCnt; k++) {
		if (regMgrIsKeyOf(k, categoryNo, depth)) {
			last = k;
			if (first == -1)
				first = k;
		} else if (first != -1) {
			break;
		}
	}
	if (first == -1 || last - first < 0)
		goto invalid;
	if (last == first) {
		callback->keyNo[slot] = first;
		ret = slot;
		goto out;
	}
	/* a category */
	depth = regMgrGetCategoryNo(path, strnlen(path, REGMGR_WORD_SIZE), categoryNo);
	if ((SceUInt32)depth >= 4)
		goto invalid;
	for (k = 1; k < s_rgMnKeyCnt; k++) {
		if (regMgrIsKeyOf(k, categoryNo, depth)) {
			callback->keyNo[slot] = REGMGR_KEYNO_CATEGORY | depth << 12 | first;
			ret = slot;
			goto out;
		}
	}
invalid:
	ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
fail:
	regMgrApiError(ret, REGMGR_API_ADD_REGISTRY_CALLBACK, path, NULL, &cbid, sizeof(cbid));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrStartCallback(SceUID cbid)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrFindCallback(cbid);
		if (ret >= 0) {
			if (g_regMgrCallback[ret].keyNo[0] == REGMGR_KEYNO_NONE)
				g_regMgrCallback[ret].keyNo[0] = REGMGR_KEYNO_ANY;
			g_regMgrCallback[ret].mode = REGMGR_CALLBACK_NOTIFY;
		}
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_START_CALLBACK, NULL, NULL, &cbid, sizeof(cbid));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrStopCallback(SceUID cbid)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrFindCallback(cbid);
		if (ret >= 0)
			g_regMgrCallback[ret].mode = 0;
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_STOP_CALLBACK, NULL, NULL, &cbid, sizeof(cbid));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrUnregisterCallback(SceUID cbid)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrFindCallback(cbid);
		if (ret >= 0)
			memset(&g_regMgrCallback[ret], 0xff, sizeof(g_regMgrCallback[ret]));
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_UNREGISTER_CALLBACK, NULL, NULL, &cbid, sizeof(cbid));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrRegisterDrvErrCallback(SceUID cbid)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (g_regMgrDrvErrCallback != -1) {
			ret = SCE_REGMGR_ERROR_CALLBACK_FULL;
		} else {
			ret = 0;
			g_regMgrDrvErrCallback = cbid;
		}
	}
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_REGISTER_DRV_ERR_CALLBACK, NULL, NULL, &cbid,
			       sizeof(cbid));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrUnregisterDrvErrCallback(SceUID cbid)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (cbid == g_regMgrDrvErrCallback) {
			g_regMgrDrvErrCallback = -1;
			ret = 0;
			goto out;
		}
		ret = SCE_REGMGR_ERROR_NO_CALLBACK;
	}
	regMgrApiError(ret, REGMGR_API_UNREGISTER_DRV_ERR_CALLBACK, NULL, NULL, &cbid,
		       sizeof(cbid));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

#define REGMGR_API_DB_BACKUP		18	/* names unknown */
#define REGMGR_API_DB_RESTORE		19
#define REGMGR_API_DEBUGGER_NOTIFY	20

#define SCE_REGMGR_ERROR_RELEASE	0x800d001e	/* names unknown */
#define SCE_REGMGR_ERROR_VOLATILE	0x800d001f

#define REGMGR_NOTIFY_INFO_SIZE		0x8cc

int sceRegMgrDbBackup(const char *path, int level)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto fail;
	if (path == NULL || strnlen(path, REGMGR_WORD_SIZE) >= REGMGR_WORD_SIZE) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	switch (level) {
	case 0:
	case 10:
	case 20:
	case 30:
	case 40:
	case 99:
		break;
	default:
		ret = SCE_REGMGR_ERROR_BAD_LEVEL;
		goto fail;
	}
	g_regMgrLevelLimit = level;
	ret = regMgrWrapRegBackupRegistry();
	if (ret == 0 && (ret = regMgrInitRegistry(NULL, 5)) == 0)
		ret = regMgrWrapRegExportRegistry(path);
	if (ret >= 0)
		goto out;
fail:
	regMgrApiError(ret, REGMGR_API_DB_BACKUP, NULL, NULL, &level, sizeof(level));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* read the values of the cached keys again, from a restored registry (name unknown) */
static inline __attribute__((always_inline)) int regMgrReloadCache(void)
{
	RegMgrKeyAccess access = { 0 };
	int cnt = s_rgMnKeyCnt;
	int lo[4], hi[4], pos[4];
	char path[REGMGR_WORD_SIZE];
	RegMgrKey *k;
	int key, n, i, depth, len, n0, n1, n2, n3, ret = 0;

	for (key = 0; key < cnt; ) {
		depth = regMgrKeyPath(key, path, lo, hi, pos);
		len = strnlen(path, sizeof(path));
		for (i = 0; i < 4; i++) {
			if (lo[i] >= 100 || hi[i] >= 100)
				return SCE_REGMGR_ERROR_BAD_CATEGORY;
		}
		n = key;
		for (n0 = lo[0]; n0 <= hi[0]; n0++) {
		regMgrPutNumber(path, pos[0], n0);
		for (n1 = lo[1]; n1 <= hi[1]; n1++) {
		regMgrPutNumber(path, pos[1], n1);
		for (n2 = lo[2]; n2 <= hi[2]; n2++) {
		regMgrPutNumber(path, pos[2], n2);
		for (n3 = lo[3]; n3 <= hi[3]; n3++) {
			regMgrPutNumber(path, pos[3], n3);
			for (n = key; n < cnt; n++) {
				k = &s_rgMnKey[n];
				if (k->attr.cacheType != 0 &&
				    (k->attr.permission == 0 ||
				     (k->attr.permission & g_regMgrPermission) == g_regMgrPermission)) {
					access.type = k->type;
					access.size = k->size;
					access.category = path;
					access.name = g_regMgrCategoryName[k->categoryNo[depth]];
					if (k->attr.cacheType == REGMGR_CACHE_INT)
						access.buf = &k->cache.value;
					else if (k->attr.cacheType == REGMGR_CACHE_BIN)
						access.buf = &g_regMgrBinCache[k->cache.offset];
					else
						access.buf = g_regMgrNumCache;
					if (path[len - 1] == '/')
						path[len - 1] = '\0';
					access.keyNo = n;
					access.valueSize = access.size;
					if (k->attr.level == 5) {
						/* kept by the backup: write it back */
						if (access.type == REGMGR_TYPE_STR)
							access.valueSize = strnlen(access.buf, access.size) + 1;
						g_regMgrWriteVersion = 1;
						regMgrSetKeyAccess(&access);
						g_regMgrWriteVersion = 0;
					} else {
						if ((SceUInt32)(k->attr.cacheType - 1) < 2)
							k->attr.flags &= ~REGMGR_KEY_CACHED;
						else
							g_regMgrNumCacheValid[0] &= ~1;
						if (regMgrGetKeyAccess(&access) == 0) {
							if (k->attr.cacheType == REGMGR_CACHE_INT ||
							    k->attr.cacheType == REGMGR_CACHE_BIN)
								k->attr.flags |= REGMGR_KEY_CACHED;
							else
								g_regMgrNumCacheValid[0] |= 1;
						}
					}
				}
				if (!regMgrIsNextKeyOf(n, key, depth)) {
					n++;
					break;
				}
			}
		}
		}
		}
		}
		key = n;
	}
	regMgrWrapRegCloseCategory(&access.handle);
	g_regMgrOpenCategory[0] = '\0';
	return ret;
}

int sceRegMgrDbRestore(const char *path)
{
	RegMgrKeyInfo info;
	int lock, ret, step, masquerade, region, serial, count;

	lock = 0;
	step = 0;
	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto fail;
	if (path == NULL) {
		step = 1;
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	if (strnlen(path, REGMGR_WORD_SIZE) >= REGMGR_WORD_SIZE) {
		step = 2;
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	masquerade = 0;
	if ((g_regMgrConfig & REGMGR_CONFIG_TOOL) &&
	    regMgrGetKeyInt("/DEVENV/TOOL", "region_masquerade", &region) == 0)
		masquerade = region;
	ret = regMgrWrapRegImportRegistry(path);
	if (ret != 0) {
		step = 3;
		goto fail;
	}
	ret = regMgrWrapRegRestoreRegistry();
	if (ret != 0) {
		step = 4;
		goto fail;
	}
	ret = regMgrWrapRegReopenRegistry(1);
	if (ret != 0) {
		step = 5;
		goto fail;
	}
	ret = regMgrReloadCache();
	count = regMgrWrapRegReopenRegistry(2);
	if (ret != 0 || count != 0) {
		step = 5;
		goto fail;
	}

	/* the registry restored is of another console, or of another region */
	g_regMgrStatus |= REGMGR_STATUS_INIT;
	g_regMgrVersion = s_rgMnKey[0].cache.value;
	info.size = sizeof(info);
	info.name = "serial_id";
	info.type = REGMGR_TYPE_BIN;
	info.valueSize = 0x20;
	info.data = g_regMgrLine;
	count = 1;
	serial = regMgrGetKeys("/REGISTRY", &info, &count);
	ret = regMgrGetKeyInt("/REGISTRY", "region_no", &region);
	if (ret != 0)
		region = ret;
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	ret = regMgrGetRegion(0);
	if (ret < 0) {
		step = 6;
		goto fail;
	}
	g_regMgrRegion = ret;
	ret = regMgrLoadDb();
	if (ret != 0) {
		step = 7;
		goto fail;
	}
	ret = regMgrInitRegistry(NULL, 0);
	if (ret != 0) {
		step = 8;
		goto fail;
	}
	if (region < 0 || g_regMgrRegion != region || region == REGMGR_REGION_TOOL) {
		snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/REGISTRY", "region_no");
		g_regMgrStatus |= REGMGR_STATUS_INIT;
		g_regMgrWriteVersion = 1;
		ret = regMgrInitRegistry(g_regMgrLine, 0);
		g_regMgrWriteVersion = 0;
		g_regMgrStatus &= ~REGMGR_STATUS_INIT;
		if (ret != 0) {
			step = 9;
			goto fail;
		}
		ret = regMgrGetRegion(1);
		if (ret < 0) {
			step = 10;
			goto fail;
		}
		if (ret == g_regMgrRegion) {
			if (ret != region)
				goto reset;
		} else {
			g_regMgrRegion = ret;
			ret = regMgrLoadDb();
			if (ret != 0) {
				step = 11;
				goto fail;
			}
			if (g_regMgrRegion != masquerade) {
reset:
				ret = regMgrInitRegistry(NULL, 0x40);
				if (ret != 0) {
					step = 12;
					goto fail;
				}
			}
		}
	}
	if (serial == 0) {
		info.size = sizeof(info);
		info.name = "serial_id";
		info.type = REGMGR_TYPE_BIN;
		info.valueSize = 0x20;
		info.data = g_regMgrLine;
		count = 1;
		g_regMgrStatus |= REGMGR_STATUS_INIT;
		serial = regMgrGetKeys("/REGISTRY", &info, &count);
		g_regMgrStatus &= ~REGMGR_STATUS_INIT;
		if (serial == 0 && regMgrCheckPsIdToken((SceUInt8 *)g_regMgrLine) == 1)
			goto done;
	}
	/* another console's: a new serial_id, and the console's own settings reset */
	snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/REGISTRY", "serial_id");
	g_regMgrStatus |= REGMGR_STATUS_INIT;
	g_regMgrWriteVersion = 1;
	ret = regMgrInitRegistry(g_regMgrLine, 0);
	g_regMgrWriteVersion = 0;
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	if (ret != 0) {
		step = 13;
		goto fail;
	}
	ret = regMgrInitRegistry(NULL, 0x100);
	if (ret != 0) {
		step = 14;
		goto fail;
	}
done:
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	ret = 0;
	goto out;
fail:
	g_regMgrStatus &= ~REGMGR_STATUS_INIT;
	if (ret < 0)
		regMgrApiError(ret, REGMGR_API_DB_RESTORE, NULL, NULL, &step, sizeof(step));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* have the debugger's notify function told the values of the keys of the reset
 * level, or of the volatile keys (name unknown) */
int SceRegMgrForDebugger_53970232(int (*func)(RegMgrNotifyInfo *info), RegMgrNotifyInfo *info)
{
	int lock, ret, level;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto fail;
	if (g_regMgrConfig & REGMGR_CONFIG_RELEASE) {
		ret = SCE_REGMGR_ERROR_RELEASE;
		goto fail;
	}
	if (g_regMgrVolatileMode == 1) {
		ret = SCE_REGMGR_ERROR_VOLATILE;
		goto fail;
	}
	if (func == NULL || info == NULL || info->size != REGMGR_NOTIFY_INFO_SIZE) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	level = (g_regMgrInitNotice & 1) ? (int)g_regMgrResetLevel : REGMGR_ATTR_VOLATILE;
	g_regMgrStatus |= REGMGR_STATUS_NOTIFY;
	g_regMgrNotifyInfo = info;
	g_regMgrNotifyFunc = func;
	ret = regMgrNotifyLevel(level);
	if (ret == 0) {
		g_regMgrInitNotice &= ~1;
		if (g_regMgrErrorMasquerade != 0) {
			if (g_regMgrErrorMasquerade == 0x800d0000)
				g_regMgrErrorMasquerade = 0;
			snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/DEVENV/TOOL", "error_masquerade");
			regMgrInitRegistry(g_regMgrLine, 0x20);
			snprintf(g_regMgrLine, REGMGR_WORD_SIZE, "%s/%s", "/DEVENV/TOOL", "error_countdown");
			regMgrInitRegistry(g_regMgrLine, 0x20);
		}
		g_regMgrStatus = 0;
		goto out;
	}
	g_regMgrStatus = 0;
	if (ret >= 0)
		goto out;
fail:
	g_regMgrStatus = 0;
	regMgrApiError(ret, REGMGR_API_DEBUGGER_NOTIFY, NULL, NULL, &g_regMgrNotifyFunc,
		       sizeof(g_regMgrNotifyFunc));
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the tool: a region and configuration to behave as (name unknown) */
int SceRegMgrForTool_0CFA929B(int region, SceUInt32 config)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (g_regMgrConfig & REGMGR_CONFIG_RELEASE) {
		ret = SCE_REGMGR_ERROR_RELEASE;
	} else if ((SceUInt32)(region - 1) < 0xd && config - 1 < 3) {
		g_regMgrConfig = config;
		g_regMgrRegion = region;
		ret = regMgrLoadDb();
		if (ret == 0 && (ret = regMgrInitRegistry(NULL, 0x40)) == 0)
			ret = regMgrSetKeyInt("/DEVENV/TOOL", "region_masquerade", g_regMgrRegion);
	} else {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the tool: reset the registry; a reset below 0x40 also leaves a masqueraded region
 * (name unknown) */
int SceRegMgrForTool_E6E81558(int level)
{
	int lock, ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (g_regMgrConfig & REGMGR_CONFIG_RELEASE) {
		ret = SCE_REGMGR_ERROR_RELEASE;
		goto out;
	}
	if (level < 0x40 && g_regMgrRegion != REGMGR_REGION_TOOL) {
		ret = SceRegMgrForTool_0CFA929B(REGMGR_REGION_TOOL, REGMGR_CONFIG_TOOL);
		if (ret < 0)
			goto out;
	}
	ret = regMgrInitRegistry(NULL, level);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}
