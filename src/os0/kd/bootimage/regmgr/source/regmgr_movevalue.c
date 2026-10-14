/*
 * regmgr_movevalue.c: when the registry of an older version is converted, the
 * values that are obsolete are deleted, and those that moved are moved.
 */

#include "regmgr_internal.h"

/* a value deleted from a version on: a key, or a category (names unknown) */
typedef struct RegMgrObsoleteValue {
	int version;
	SceInt16 first;		/* the numbered categories: from first, count of them */
	SceUInt16 count;
	const char *category;
	const char *sub;	/* in each numbered category */
	const char *name;	/* NULL: the category */
} RegMgrObsoleteValue;

static const RegMgrObsoleteValue g_regMgrObsoleteValue[] = {	/* name unknown */
	{ 0x9103, 1, 30, "/CONFIG/NET", "IP", "lease_expiration_time" },
	{ 0x9103, 0, 0, "/CONFIG/NP", NULL, "env" },
	{ 0x9201, 0, 0, "/DEVENV/HOST", NULL, "software_unique_id" },
	{ 0x9216, 0, 0, "/DEVENV/HOST", NULL, "init_settings" },
	{ 0x9216, 0, 0, "/CONFIG/NP/GUEST", NULL, NULL },
	{ 0x9300, 1, 30, "/CONFIG/NET", "AOSS", NULL },
	{ 0x9300, 0, 0, "/CONFIG/LOCATION", NULL, "xps_config" },
	{ 0x9406, 0, 0, "/CONFIG/IME/TRADITIONAL_CHINESE", NULL, NULL },
	{ 0x9410, 0, 0, "/CONFIG/SYSTEM", NULL, "nickname" },
	{ 0x9925, 0, 0, "/CONFIG/NP", NULL, "lang" },
	{ 0x9928, 1, 30, "/CONFIG/NET", "COMMON", "emulation_type" },
	{ 0x9930, 0, 0, "/CONFIG/TEL", NULL, "max_temp" },
	{ 0x9930, 0, 0, "/CONFIG/TEL", NULL, "recover_temp" },
	{ 0x9932, 0, 0, "/CONFIG/TEL", NULL, "apn_name" },
	{ 0x9973, 0, 0, "/CONFIG/TEL", NULL, "use_debug_settings" },
	{ 0x9943, 0, 0, "/DATA/FONT/PGF", NULL, NULL },
	{ 0x9943, 0, 0, "/DATA/FONT/PVF", NULL, NULL },
	{ 0x9943, 0, 0, "/DATA/FONT", NULL, NULL },
	{ 0x9943, 0, 0, "/DATA", NULL, NULL },
	{ 0x9964, 1, 30, "/CONFIG/NET", "IP", "lease_ip_address" },
	{ 0x9964, 1, 30, "/CONFIG/NET", "IP", "lease_expiration_tick" },
	{ 0x9965, 0, 0, "/CONFIG/NP", NULL, "privacy_confirmation" },
	{ 0x9965, 0, 0, "/CONFIG/NP", NULL, "test_patch" },
	{ 0x9982, 0, 0, "/CONFIG/TEL", NULL, "imsi" },
	{ 0x9988, 0, 5, "/CONFIG/MAIL", NULL, "fetch_interval" },
	{ 0x9991, 0, 0, "/CONFIG/MOTION/MAGN", NULL, NULL },
	{ 0x9991, 0, 0, "/CONFIG/MOTION", NULL, NULL },
	{ 0x99602, 0, 5, "/CONFIG/MAIL", "RECV", "mail_address" },
	{ 0x99602, 0, 5, "/CONFIG/MAIL", NULL, "debug" },
	{ 0x150016, 0, 0, "/CONFIG/TEL", NULL, "apn_user_id" },
	{ 0x150050, 0, 0, "/REGISTRY/ERROR", NULL, "err_enable" },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", "RECV", NULL },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", "SEND", NULL },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", "DRAFT", NULL },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", "SENT", NULL },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", "TRASH", NULL },
	{ 0x150055, 0, 5, "/CONFIG/MAIL", NULL, NULL },
	{ 0x150055, 0, 0, "/CONFIG/MAIL", NULL, NULL },
	{ 0x150060, 0, 0, "/CONFIG/NP2/PSNMSG", NULL, "fetch_interval" },
	{ 0x180020, 0, 0, "/CONFIG/SHELL", NULL, "show_mode" },
	{ 0x180045, 0, 0, "/CONFIG/NET", NULL, "fake_http_contentlength" },
	{ 0x180050, 0, 0, "/CONFIG/PSPEMU/SETTINGS/", NULL, "camera_direction" },
	{ 0x200002, 0, 0, "/CONFIG/COREDUMP/", NULL, "enable_coredump" },
	{ 0x200002, 0, 0, "/CONFIG/COREDUMP/", NULL, "behave_cex_on_devkit" },
	{ 0x250040, 0, 0, "/REGISTRY/ERROR", NULL, "err_enable" },
	{ 0x310010, 0, 0, "/CONFIG/SYSTEM/UPDATE", NULL, "enable_pup_bgdl" },
	{ 0x310010, 0, 0, "/CONFIG/SYSTEM/UPDATE", NULL, "pup_bgdl_cycle" },
	{ 0x357002, 0, 0, "/CONFIG/NP2/FACEBOOK", NULL, NULL },
};

#define REGMGR_OBSOLETE_VALUE_CNT	(sizeof(g_regMgrObsoleteValue) / sizeof(g_regMgrObsoleteValue[0]))

/* delete a key, or a category (name unknown) */
static inline __attribute__((always_inline)) void regMgrDeleteValue(const char *path, const char *name)
{
	int handle;

	if (name == NULL) {
		regMgrWrapRegRemoveCategory(path);
	} else if (regMgrWrapRegOpenCategory(path, 2, &handle) == 0) {
		regMgrWrapRegDeleteKey(handle, path, name);
		regMgrWrapRegCloseCategory(&handle);
	}
}

/* delete the values that became obsolete after the version; gives how many */
int regMgrDeleteObsoleteValues(int version, int newVersion)
{
	const RegMgrObsoleteValue *value;
	char path[REGMGR_WORD_SIZE];
	int n, i, end;

	if (version == 0)
		return 0;
	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (regMgrGetVolatileMode() == 1)
		return 0;
	n = 0;
	for (value = g_regMgrObsoleteValue;
	     value < &g_regMgrObsoleteValue[REGMGR_OBSOLETE_VALUE_CNT]; value++) {
		if (value->version > newVersion || version >= value->version)
			continue;
		if (value->count == 0) {
			strncpy(path, value->category, sizeof(path));
			regMgrDeleteValue(path, value->name);
			n++;
			continue;
		}
		end = value->first + value->count;
		if (value->first >= end)
			continue;
		for (i = value->first; i < end; i++) {
			if (value->sub == NULL)
				snprintf(path, sizeof(path), "%s/%02d", value->category, i);
			else
				snprintf(path, sizeof(path), "%s/%02d/%s", value->category, i, value->sub);
			regMgrDeleteValue(path, value->name);
		}
		n += end - value->first;
	}
	return n;
}

/* 2.00 (0x9915): the update settings moved from /CONFIG/SYSTEM into
 * /CONFIG/SYSTEM/UPDATE */
void regMgrMoveUpdateValues(int version, int newVersion)
{
	RegMgrKeyInfo info[3];
	char url[0x200];
	int handle = 0, count;

	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (version < 0x9104 || regMgrGetVolatileMode() == 1)
		return;
	if (newVersion <= 0x9914 || version >= 0x9915)
		goto out;
	if (regMgrWrapRegOpenCategory("/CONFIG/SYSTEM", 2, &handle) != 0)
		return;
	info[0].size = sizeof(info[0]);
	info[0].name = "update_server_url";
	info[0].type = REGMGR_TYPE_STR;
	info[0].valueSize = sizeof(url);
	info[0].data = url;
	info[1].size = sizeof(info[1]);
	info[1].name = "update_pup_list_flag";
	info[1].type = REGMGR_TYPE_INT;
	info[1].valueSize = sizeof(int);
	info[1].value = 0;
	info[2].size = sizeof(info[2]);
	info[2].name = "update_enable_host0";
	info[2].type = REGMGR_TYPE_INT;
	info[2].valueSize = sizeof(int);
	info[2].value = 0;
	if (regMgrWrapRegGetKey(handle, "/CONFIG/SYSTEM", "update_server_url", url, sizeof(url)) != 0 ||
	    regMgrWrapRegGetKey(handle, "/CONFIG/SYSTEM", info[1].name, &info[1].value,
				info[1].valueSize) != 0 ||
	    regMgrWrapRegGetKey(handle, "/CONFIG/SYSTEM", info[2].name, &info[2].value,
				info[2].valueSize) != 0)
		goto out;
	if (regMgrWrapRegCloseCategory(&handle) != 0)
		return;
	info[0].name = "server_url";
	info[1].name = "pup_list_flag";
	info[2].name = "enable_host0";
	count = 3;
	if (regMgrSetKeys("/CONFIG/SYSTEM/UPDATE", info, &count) != 0 ||
	    regMgrWrapRegOpenCategory("/CONFIG/SYSTEM", 2, &handle) != 0)
		return;
	if (regMgrWrapRegDeleteKey(handle, "/CONFIG/SYSTEM", "update_server_url") == 0 &&
	    regMgrWrapRegDeleteKey(handle, "/CONFIG/SYSTEM", "update_pup_list_flag") == 0)
		regMgrWrapRegDeleteKey(handle, "/CONFIG/SYSTEM", "update_enable_host0");
out:
	regMgrWrapRegCloseCategory(&handle);
}

/* the NP account names moved from /CONFIG/NP into /CONFIG/NP_MULTI; gives how many */
int regMgrMoveNpAccountNames(int version, int newVersion)
{
	char name[0x1c];
	char account[0x20];
	int handle, ret, n;

	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (version < 0x9909 || regMgrGetVolatileMode() == 1 ||
	    version > 0x995f || newVersion < 0x9960)
		return 0;
	for (n = 0; ; ) {
		snprintf(name, sizeof(name), "accountname_%02d", n);
		memset(account, 0, sizeof(account));
		handle = 0;
		if (regMgrWrapRegOpenCategory("/CONFIG/NP", 2, &handle) != 0)
			break;
		ret = regMgrWrapRegGetKey(handle, "/CONFIG/NP", name, account, sizeof(account));
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0)
			break;
		handle = ret;
		if (regMgrWrapRegOpenCategory("/CONFIG/NP_MULTI", 1, &handle) != 0)
			break;
		ret = regMgrWrapRegCreateKey(handle, "/CONFIG/NP_MULTI", name, REGMGR_TYPE_STR,
					     sizeof(account));
		if (ret != SCE_REGMGR_ERROR_KEY_EXISTS && ret != 0) {
			regMgrWrapRegCloseCategory(&handle);
			break;
		}
		ret = regMgrWrapRegSetKey(handle, "/CONFIG/NP_MULTI", name, account,
					  strnlen(account, sizeof(account) - 1) + 1);
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0)
			break;
		n++;
		handle = ret;
		if (regMgrWrapRegOpenCategory("/CONFIG/NP", 2, &handle) != 0)
			break;
		ret = regMgrWrapRegDeleteKey(handle, "/CONFIG/NP", name);
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0 || n == 16)
			break;
	}
	return n;
}

/* the APN user and password moved within /CONFIG/TEL, and became of their own type;
 * gives how many */
static const char *const g_regMgrTelValueName[] = { "apn_user_id", "apn_password" };	/* names unknown */
static const int g_regMgrTelValueType[] = { REGMGR_TYPE_STR, REGMGR_TYPE_STR };

int regMgrMoveTelValues(int version, int newVersion)
{
	char value[0x80];
	const char *name;
	int handle, type, ret, n;

	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (version < 0x9416 || regMgrGetVolatileMode() == 1 ||
	    version > 0x150034 || newVersion < 0x150035)
		return 0;
	for (n = 0; ; ) {
		memset(value, 0, sizeof(value));
		handle = 0;
		name = g_regMgrTelValueName[n];
		if (regMgrWrapRegOpenCategory("/CONFIG/TEL", 2, &handle) != 0)
			break;
		ret = regMgrWrapRegGetKey(handle, "/CONFIG/TEL", name, value, sizeof(value));
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0)
			break;
		handle = ret;
		if (regMgrWrapRegOpenCategory("/CONFIG/TEL", 2, &handle) != 0)
			break;
		ret = regMgrWrapRegDeleteKey(handle, "/CONFIG/TEL", name);
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0)
			break;
		type = g_regMgrTelValueType[n];
		handle = ret;
		if (regMgrWrapRegOpenCategory("/CONFIG/TEL", 1, &handle) != 0)
			break;
		ret = regMgrWrapRegCreateKey(handle, "/CONFIG/TEL", name, type, sizeof(value));
		if (ret != SCE_REGMGR_ERROR_KEY_EXISTS && ret != 0) {
			regMgrWrapRegCloseCategory(&handle);
			break;
		}
		ret = regMgrWrapRegSetKey(handle, "/CONFIG/TEL", name, value,
					  type == REGMGR_TYPE_STR ?
					  strnlen(value, sizeof(value) - 1) + 1 : sizeof(value));
		regMgrWrapRegCloseCategory(&handle);
		if (ret != 0 || ++n == 2)
			break;
	}
	return n;
}

/* the current NP account moved into /CONFIG/NP2; gives whether it did */
int regMgrMoveNpCurrentAccount(int version, int newVersion)
{
	int account = 0;
	int handle, ret;

	if (version < 0)
		version &= ~REGMGR_VERSION_DIRTY;
	if (version <= 0x9905 || regMgrGetVolatileMode() == 1)
		return 0;
	if (version > 0x170004 || newVersion <= 0x170004)
		return 0;
	handle = 0;
	if (regMgrWrapRegOpenCategory("/CONFIG/NP", 2, &handle) != 0)
		return 0;
	ret = regMgrWrapRegGetKey(handle, "/CONFIG/NP", "current_account", &account, sizeof(account));
	regMgrWrapRegCloseCategory(&handle);
	if (ret != 0)
		return 0;
	handle = ret;
	if (regMgrWrapRegOpenCategory("/CONFIG/NP2", 1, &handle) != 0)
		return 0;
	ret = regMgrWrapRegCreateKey(handle, "/CONFIG/NP2", "current_account2", REGMGR_TYPE_INT,
				     sizeof(account));
	if (ret == SCE_REGMGR_ERROR_KEY_EXISTS || ret == 0)
		ret = regMgrWrapRegSetKey(handle, "/CONFIG/NP2", "current_account2", &account,
					  sizeof(account));
	regMgrWrapRegCloseCategory(&handle);
	return ret == 0;
}
