/*
 * regmgr_tool.c: the tools' direct access to the registry files: the registry
 * reopened, its categories and keys listed. Refused in a console that is not in
 * the release check mode, unless QA flags allow it.
 */

#include "regmgr_internal.h"

#define SCE_REGMGR_ERROR_NOT_ALLOWED	0x800d001e	/* names unknown */

#define REGMGR_REG_NAME_SIZE	0x1b	/* a name in the registry files (name unknown) */

/* the types of the registry files (names unknown) */
#define REGMGR_REG_TYPE_CATEGORY	1
#define REGMGR_REG_TYPE_INT		2
#define REGMGR_REG_TYPE_STR		3
#define REGMGR_REG_TYPE_BIN		4
#define REGMGR_TYPE_CATEGORY		0x10	/* name unknown */

int g_regMgrToolLock;	/* held while the tool has the registry reopened (name unknown) */

int regGetCategoryNum(int registry, int *num);	/* the reg library (names unknown) */
int regGetCategoryNames(int registry, char (*names)[REGMGR_REG_NAME_SIZE], int num);
int regGetKeyInfo(int handle, const char *name, int *type, int *size);
int regGetKeyNum(int handle, int *num);
int regGetKeyNames(int handle, char (*names)[REGMGR_REG_NAME_SIZE], int num);

/* whether the tool may use the registry files */
static inline __attribute__((always_inline)) int regMgrToolCheck(void)
{
	return regMgrIsRelease() != 0 || regMgrIsQaf() == 1;
}

/* reopen the registry for the tool, and hold it */
int SceRegMgrForTool_6808B7AD(void)
{
	int lock = 0;
	int ret;

	ret = regMgrLock(&g_regMgrToolLock);
	if (ret >= 0 && (ret = regMgrComApiPrologue(&lock)) >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else
			ret = regMgrWrapRegReopenRegistry(1);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* reopen the registry for the system again, and release it */
int SceRegMgrForTool_0B623D0B(void)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else
			ret = regMgrWrapRegReopenRegistry(2);
	}
	regMgrComApiEpilogue(lock);
	regMgrUnlock(g_regMgrToolLock);
	return ret;
}

int SceRegMgrForTool_2A76051A(const char *category, int *handle)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else
			ret = regMgrWrapRegOpenCategory(category, 2, handle);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForTool_B441660B(int handle)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else
			ret = regMgrWrapRegCloseCategory(&handle);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the number of categories */
int SceRegMgrForTool_F55E6E4F(int *num)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck()) {
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		} else if (num == NULL) {
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else if (regMgrGetVolatileMode() == 1) {
			*num = 0;
		} else {
			ret = regMgrWrapRegSetError(regGetCategoryNum(regMgrWrapRegGetRegistryHandle(),
								      num), 245);
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the names of the categories */
int SceRegMgrForTool_3AAB71EF(char (*names)[REGMGR_REG_NAME_SIZE], int num)
{
	int lock = 0;
	int ret, i;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (!regMgrToolCheck()) {
		ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		goto out;
	}
	if (names == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	for (i = 0; i < num; i++)
		names[i][0] = '\0';
	if (regMgrGetVolatileMode() != 1)
		ret = regMgrWrapRegSetError(regGetCategoryNames(regMgrWrapRegGetRegistryHandle(),
								names, num), 306);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForTool_7B30AC2C(const char *category, RegMgrKeyInfo *info, int count)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck()) {
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		} else if (category == NULL || info == NULL || count < 1) {
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else if (regMgrGetVolatileMode() != 1) {
			regMgrSetStatus(REGMGR_STATUS_INIT);
			ret = regMgrWrapRegSetError(sceRegMgrGetKeysInfo(category, info, count), 361);
			regMgrClearStatus();
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the type and size of a key */
int SceRegMgrForTool_6276E7D8(int handle, const char *name, int *type, int *size)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (!regMgrToolCheck()) {
		ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		goto out;
	}
	if (name == NULL || type == NULL || size == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (regMgrGetVolatileMode() == 1) {
		*type = 0;
		*size = 0;
		goto out;
	}
	ret = regGetKeyInfo(handle, name, type, size);
	if (ret != 0) {
		ret = regMgrWrapRegSetError(ret, 442);
		goto out;
	}
	switch (*type) {
	case REGMGR_REG_TYPE_CATEGORY:
		*type = REGMGR_TYPE_CATEGORY;
		break;
	case REGMGR_REG_TYPE_INT:
		*type = REGMGR_TYPE_INT;
		break;
	case REGMGR_REG_TYPE_STR:
		*type = REGMGR_TYPE_STR;
		break;
	case REGMGR_REG_TYPE_BIN:
		*type = REGMGR_TYPE_BIN;
		break;
	default:
		ret = SCE_REGMGR_ERROR_BAD_TYPE;
		break;
	}
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int SceRegMgrForTool_C689E36C(int handle, const char *name, void *buf, int size)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else if (name == NULL || buf == NULL || size < 1)
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		else if (regMgrGetVolatileMode() == 1)
			memset(buf, 0, size);
		else
			ret = regMgrWrapRegSetError(regGetKey(handle, name, buf, size), 496);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the number of keys of a category */
int SceRegMgrForTool_2D415472(int handle, int *num)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (!regMgrToolCheck())
			ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		else if (num == NULL)
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		else if (regMgrGetVolatileMode() == 1)
			*num = 0;
		else
			ret = regMgrWrapRegSetError(regGetKeyNum(handle, num), 547);
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the names of the keys of a category */
int SceRegMgrForTool_4EDF87F9(int handle, char (*names)[REGMGR_REG_NAME_SIZE], int num)
{
	int lock = 0;
	int ret, i;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (!regMgrToolCheck()) {
		ret = SCE_REGMGR_ERROR_NOT_ALLOWED;
		goto out;
	}
	if (names == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	for (i = 0; i < num; i++)
		names[i][0] = '\0';
	if (regMgrGetVolatileMode() != 1)
		ret = regMgrWrapRegSetError(regGetKeyNames(handle, names, num), 608);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}
