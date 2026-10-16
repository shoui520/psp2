/*
 * regmgr_syscalls.c: the registry manager's system calls. The arguments are
 * copied in from user memory to the buffers here, the kernel API is called on
 * them, and the results are copied out.
 */

#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>
#include <sce/error.h>
#include "regmgr_internal.h"

#define REGMGR_SYS_KEYS_MAX	32	/* names unknown */
#define REGMGR_SYS_NAME_SIZE	0x1a
#define REGMGR_SYS_DATA_SIZE	0x800
#define REGMGR_SYS_STR_MAX	0x7c0
#define REGMGR_SYS_PARAM_CNT	63

#define REGMGR_COPY_KERNEL	1	/* the caller is the kernel: no copy to user (name unknown) */

/* the keys of the system parameters, by ID (names unknown) */
typedef struct RegMgrSysParam {
	SceInt8 write;		/* may be set */
	SceInt8 read;		/* may be got */
	SceInt8 categoryNo;
	SceInt8 nameNo;
} RegMgrSysParam;

const RegMgrSysParam g_regMgrSysParam[REGMGR_SYS_PARAM_CNT] = {	/* names unknown */
	{ 0, 1, 0, 0 },   { 0, 1, 0, 2 },   { 0, 1, 1, 3 },   { 0, 1, 1, 4 },
	{ 0, 1, 1, 5 },   { 0, 1, 1, 6 },   { 0, 1, 3, 10 },  { 0, 1, 0, 11 },
	{ 0, 1, 4, 12 },  { 1, 1, 6, 14 },  { 1, 1, 6, 15 },  { 1, 1, 6, 16 },
	{ 1, 1, 6, 17 },  { 0, 1, 4, 19 },  { 0, 1, 4, 20 },  { 0, 1, 8, 21 },
	{ 0, 1, 4, 22 },  { 0, 1, 4, 23 },  { 0, 1, 4, 24 },  { 0, 1, 4, 25 },
	{ 0, 1, 9, 26 },  { 0, 1, 9, 27 },  { 0, 1, 9, 28 },  { 0, 1, 9, 29 },
	{ 0, 1, 9, 30 },  { 0, 1, 9, 31 },  { 1, 1, 5, 13 },  { 0, 1, 9, 39 },
	{ 0, 1, 4, 40 },  { 0, 1, 12, 41 }, { 0, 1, 0, 1 },   { 0, 1, 14, 43 },
	{ 0, 1, 15, 44 }, { 0, 1, 15, 45 }, { 0, 1, 16, 46 }, { 0, 1, 15, 47 },
	{ 0, 1, 15, 48 }, { 1, 1, 10, 32 }, { 1, 1, 10, 33 }, { 0, 1, 17, 49 },
	{ 0, 1, 9, 50 },  { 0, 1, 18, 51 }, { 0, 1, 12, 52 }, { 1, 1, 11, 34 },
	{ 1, 1, 11, 35 }, { 1, 1, 11, 36 }, { 1, 1, 11, 37 }, { 0, 1, 11, 38 },
	{ 0, 1, 13, 42 }, { 1, 1, 19, 53 }, { 1, 1, 19, 54 }, { 0, 1, 15, 55 },
	{ 0, 1, 15, 56 }, { 0, 1, 3, 57 },  { 0, 1, 1, 58 },  { 0, 1, 20, 59 },
	{ 1, 1, 7, 18 },  { 0, 1, 9, 60 },  { 0, 1, 9, 61 },  { 0, 1, 2, 7 },
	{ 0, 1, 2, 8 },   { 0, 1, 2, 9 },   { 0, 1, 3, 62 },
};

const int g_regMgrSysParamId[REGMGR_SYS_PARAM_CNT] = {	/* names unknown */
	0x229142, 0x037502, 0x088776, 0x668503, 0x100591, 0x760538, 0x186122, 0x598438,
	0x450f32, 0x81649f, 0x068303, 0x1b2292, 0x872621, 0x156489, 0x033818, 0x3cb6a4,
	0x0b73cd, 0x0a0495, 0x0b6ecd, 0x0d18e5, 0x313905, 0x646a8e, 0x22b191, 0x8eb468,
	0x168b9b, 0x6ff829, 0x612b3e, 0x3317a1, 0x8f94f9, 0x134c03, 0x683dcd, 0x8e3939,
	0x146e23, 0x424500, 0x51f6ae, 0x154a2c, 0x25ce9a, 0x5f6737, 0x91f34f, 0x93c981,
	0x7c9764, 0x971fa1, 0x563bfe, 0x8d89eb, 0x4e7a16, 0x987180, 0x0504e4, 0x36f14e,
	0x2fdfb4, 0x4f7e60, 0x94e320, 0x9623d0, 0x7d12c4, 0x89c9cf, 0x7f9315, 0x988b81,
	0x8c3860, 0x505bce, 0x528c0d, 0x023fc2, 0x611dc9, 0x8a2ad7, 0x711659,
};

const char g_regMgrSysParamCategory[][REGMGR_WORD_SIZE] = {	/* names unknown */
	"/CONFIG/SYSTEM",
	"/CONFIG/DATE",
	"/CONFIG/ACCESSIBILITY",
	"/CONFIG/SECURITY/PARENTAL",
	"/CONFIG/NP",
	"/CONFIG/BROWSER",
	"/CONFIG/BROWSER/ADDIN/TRENDMICRO",
	"/CONFIG/BROWSER/DEBUG",
	"/DEVENV/TOOL",
	"/CONFIG/NP2",
	"/CONFIG/NP2/TWITTER",
	"/CONFIG/NP2/TELEPORT",
	"/CONFIG/NET",
	"/CONFIG/DISPLAY",
	"/CONFIG/MUSIC/MUSIC_APP",
	"/CONFIG/GAME",
	"/CONFIG/SPECIFIC",
	"/CONFIG/PSM",
	"/CONFIG/SHELL/",
	"/CONFIG/PS4LINK/",
	"/CONFIG/PSNOW/",
};

const char g_regMgrSysParamName[][REGMGR_CATEGORY_NAME] = {	/* names unknown */
	"button_assign", "key_pad", "language", "date_format", "time_format",
	"time_zone", "summer_time", "large_text", "bold_text", "contrast",
	"passcode", "username", "account_id", "web_security_status", "tm_service",
	"tm_ec_ttl", "tm_ec_ttl_update_time", "tm_service_sub_status", "net_dbg_config",
	"debug_ingame_commerce2", "env", "gpi_switch", "debug", "nav_only",
	"np_ad_clock_diff", "np_geo_filtering", "test_patch", "tpps_proxy_server",
	"tpps_proxy_port", "tpps_proxy_user_name", "tpps_proxy_password",
	"tpps_proxy_flag", "access_token", "access_token_secret",
	"wol_target_mac_address", "target_name", "initial_target", "passcode_client",
	"enable_media_transfer", "trophy_setup_dialog_debug", "country",
	"pspnet_adhoc_ssid_prefix", "hdmi_out_scaling_ratio", "impose_audio_balance",
	"show_debug_info", "fake_sdslot_broken", "idu_mode", "fake_free_space",
	"fake_free_space_quota", "revocation_check_req", "fake_plus", "voice_priority",
	"ssl_cert_ignorable", "counter", "keys", "fake_no_memory_card",
	"fake_contents_max", "store_start_control", "is_summer_time", "app_cached_url",
	"fake_ratelimit", "ignore_titleid", "content_start_control",
};

/* the arguments of a system call, copied in (names unknown) */
char g_regMgrSysKeyName[REGMGR_SYS_KEYS_MAX][REGMGR_CATEGORY_NAME];
const char *g_regMgrSysUserName[REGMGR_SYS_KEYS_MAX];	/* the user's, to give back */
char g_regMgrSysCategory[REGMGR_WORD_SIZE];
RegMgrKeyInfo g_regMgrSysKeys[REGMGR_SYS_KEYS_MAX];
const void *g_regMgrSysUserData[REGMGR_SYS_KEYS_MAX];	/* the user's, to give back */
char g_regMgrSysName[0x20];
char g_regMgrSysData[REGMGR_SYS_DATA_SIZE];

/* copy the keys out to the user: their values (mode REGMGR_COPY_KERNEL), and the
 * user's names and buffers back in them (name unknown) */
int regMgrSysCopyOutKeys(RegMgrKeyInfo *keys, int count, int mode)
{
	RegMgrKeyInfo *key;
	int i, ret;

	if ((SceUInt32)(count - 1) >= REGMGR_SYS_KEYS_MAX)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	if (mode == REGMGR_COPY_KERNEL) {
		for (i = 0, key = g_regMgrSysKeys; i != count; i++, key++) {
			if ((SceUInt16)(key->type - 1) < 2) {
				ret = ksceKernelCopyToUser((void *)g_regMgrSysUserData[i], key->data,
							   key->valueSize);
				if (ret != 0)
					return ret;
				key->data = g_regMgrSysUserData[i];
			} else if (key->type != REGMGR_TYPE_INT) {
				return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			}
			key->name = g_regMgrSysUserName[i];
		}
	} else {
		for (i = 0; i != count; i++) {
			g_regMgrSysKeys[i].data = g_regMgrSysUserData[i];
			g_regMgrSysKeys[i].name = g_regMgrSysUserName[i];
		}
	}
	return ksceKernelCopyToUser(keys, g_regMgrSysKeys, count * sizeof(RegMgrKeyInfo));
}

/* copy a category and keys in from the user: their names only (mode 2), or their
 * values too (name unknown) */
int regMgrSysCopyInKeys(const char *category, const RegMgrKeyInfo *keys, int count, int mode)
{
	RegMgrKeyInfo *key;
	const void *data;
	SceSize used;
	int i, ret;

	if (category == NULL || keys == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, category, sizeof(g_regMgrSysCategory));
	if (ret < 0)
		return ret;
	if (ret >= (int)sizeof(g_regMgrSysCategory))
		return SCE_REGMGR_ERROR_BAD_CATEGORY;
	if ((SceUInt32)(count - 1) >= REGMGR_SYS_KEYS_MAX)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	key = g_regMgrSysKeys;
	ret = ksceKernelCopyFromUser(key, keys, count * sizeof(RegMgrKeyInfo));
	if (ret != 0)
		return ret;
	if (key->size != sizeof(RegMgrKeyInfo))
		return SCE_REGMGR_ERROR_BAD_KEYS_INFO;
	if (key->name == NULL)
		return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	used = 0;
	for (i = 0; ; ) {
		ret = ksceKernelStrncpyFromUser(g_regMgrSysKeyName[i], key->name,
						REGMGR_SYS_NAME_SIZE);
		if (ret < 0)
			return ret;
		if (ret >= REGMGR_SYS_NAME_SIZE)
			return SCE_REGMGR_ERROR_BAD_NAME;
		g_regMgrSysUserName[i] = key->name;
		data = key->data;
		g_regMgrSysUserData[i] = data;
		key->name = g_regMgrSysKeyName[i];
		if (mode == 2) {
			key->type = REGMGR_TYPE_INT;
			key->valueSize = 0;
			key->value = 0;
		} else if ((SceUInt16)(key->type - 1) < 2) {
			key->data = &g_regMgrSysData[used];
			used += key->valueSize;
			if (used > sizeof(g_regMgrSysData))
				return SCE_REGMGR_ERROR_BAD_SIZE;
			if (data == NULL)
				return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			ret = ksceKernelCopyFromUser((void *)key->data, data, key->valueSize);
			if (ret != 0)
				return ret;
		} else if (key->type != REGMGR_TYPE_INT) {
			return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		}
		if (++i == count)
			return 0;
		key++;
		if (key->size != sizeof(RegMgrKeyInfo))
			return SCE_REGMGR_ERROR_BAD_KEYS_INFO;
		if (key->name == NULL)
			return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
}

/* the system parameter of an ID; NULL if none */
static inline __attribute__((always_inline)) const RegMgrSysParam *regMgrSysFindParam(int id)
{
	int i;

	for (i = 0; i < REGMGR_SYS_PARAM_CNT; i++) {
		if (id == g_regMgrSysParamId[i])
			return &g_regMgrSysParam[i];
	}
	return NULL;
}

/* get a system parameter, with the kernel's permission (names unknown) */
int regMgrSysParamGetInt(int id, int *value, int mode)
{
	const RegMgrSysParam *param;
	int lock = 0;
	int buf = 0;
	int ret, err, perm;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	param = regMgrSysFindParam(id);
	if (param == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (param->read != 1) {
		ret = SCE_REGMGR_ERROR_NO_ACCESS;
		goto out;
	}
	ret = perm = SceThreadmgrForDriver_332E127C();
	if (perm < 0)
		goto out;
	if (value == NULL) {
		err = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	} else {
		err = ksceKernelSetPermission(0x40);
		if (err >= 0) {
			regMgrSetStatus(REGMGR_STATUS_FLAGS);
			err = sceRegMgrGetKeyInt(g_regMgrSysParamCategory[param->categoryNo],
						 g_regMgrSysParamName[param->nameNo], &buf);
			regMgrClearStatus();
			if (err == 0) {
				if (mode == REGMGR_COPY_KERNEL)
					*value = buf;
				else
					err = ksceKernelCopyToUser(value, &buf, sizeof(buf));
			}
		}
	}
	ksceKernelSetPermission(perm);
	ret = err;
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* a string or binary system parameter (names unknown) */
static inline __attribute__((always_inline)) int regMgrSysParamGet(int id, void *buf, SceSize size, int mode, int bin)
{
	const RegMgrSysParam *param;
	int lock = 0;
	int ret, err, perm;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	param = regMgrSysFindParam(id);
	if (param == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (param->read != 1) {
		ret = SCE_REGMGR_ERROR_NO_ACCESS;
		goto out;
	}
	ret = perm = SceThreadmgrForDriver_332E127C();
	if (perm < 0)
		goto out;
	if (buf == NULL) {
		err = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	} else {
		err = ksceKernelSetPermission(0x40);
		if (err >= 0) {
			if (size >= REGMGR_SYS_DATA_SIZE)
				size = REGMGR_SYS_DATA_SIZE;
			regMgrSetStatus(REGMGR_STATUS_FLAGS);
			if (bin)
				err = sceRegMgrGetKeyBin(g_regMgrSysParamCategory[param->categoryNo],
							 g_regMgrSysParamName[param->nameNo],
							 g_regMgrSysData, size);
			else
				err = sceRegMgrGetKeyStr(g_regMgrSysParamCategory[param->categoryNo],
							 g_regMgrSysParamName[param->nameNo],
							 g_regMgrSysData, size);
			regMgrClearStatus();
			if (err == 0) {
				if (mode == REGMGR_COPY_KERNEL) {
					if (size != 0)
						memcpy(buf, g_regMgrSysData, size);
				} else {
					err = ksceKernelCopyToUser(buf, g_regMgrSysData, size);
				}
			}
		}
	}
	ksceKernelSetPermission(perm);
	ret = err;
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int regMgrSysParamGetStr(int id, char *buf, SceSize size, int mode)
{
	return regMgrSysParamGet(id, buf, size, mode, 0);
}

int regMgrSysParamGetBin(int id, void *buf, SceSize size, int mode)
{
	return regMgrSysParamGet(id, buf, size, mode, 1);
}

/* copy a category and a key's name in from the user */
static inline __attribute__((always_inline)) int regMgrSysCopyInName(const char *category, const char *name)
{
	int ret;

	ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, category, sizeof(g_regMgrSysCategory));
	if (ret < 0)
		return ret;
	if (ret >= (int)sizeof(g_regMgrSysCategory))
		return SCE_REGMGR_ERROR_BAD_CATEGORY;
	ret = ksceKernelStrncpyFromUser(g_regMgrSysName, name, REGMGR_SYS_NAME_SIZE);
	if (ret < 0)
		return ret;
	if (ret >= REGMGR_SYS_NAME_SIZE)
		return SCE_REGMGR_ERROR_BAD_NAME;
	return 0;
}

int sceRegMgrResetRegistryLvInternal2(const char *category, int level)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	if (category == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, category, sizeof(g_regMgrSysCategory));
	if (ret < 0)
		goto out;
	if (ret >= (int)sizeof(g_regMgrSysCategory)) {
		ret = SCE_REGMGR_ERROR_BAD_CATEGORY;
		goto out;
	}
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrResetRegistryLv(g_regMgrSysCategory, level);
	regMgrClearStatus();
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeyIntInternal2(const char *category, const char *name, int value)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			if (category == NULL || name == NULL) {
				ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
			} else {
				ret = regMgrSysCopyInName(category, name);
				if (ret == 0) {
					regMgrSetStatus(REGMGR_STATUS_SYSCALL);
					ret = sceRegMgrSetKeyInt(g_regMgrSysCategory, g_regMgrSysName,
								 value);
					regMgrClearStatus();
				}
			}
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeyIntInternal2(const char *category, const char *name, int *value)
{
	int lock = 0;
	int buf = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) != 1) {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		} else if (category == NULL || name == NULL || value == NULL) {
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else {
			ret = regMgrSysCopyInName(category, name);
			if (ret == 0) {
				regMgrSetStatus(REGMGR_STATUS_SYSCALL);
				ret = sceRegMgrGetKeyInt(g_regMgrSysCategory, g_regMgrSysName, &buf);
				regMgrClearStatus();
				if (ret == 0)
					ret = ksceKernelCopyToUser(value, &buf, sizeof(buf));
			}
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* set a string or binary key from the user (name unknown) */
static inline __attribute__((always_inline)) int regMgrSysSetKey(const char *category, const char *name,
					   const void *buf, SceSize size, int bin)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	if (category == NULL || name == NULL)
		goto invalid;
	ret = regMgrSysCopyInName(category, name);
	if (ret != 0)
		goto out;
	if (buf == NULL || size > REGMGR_SYS_STR_MAX)
		goto invalid;
	ret = ksceKernelCopyFromUser(g_regMgrSysData, buf, size);
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	if (bin)
		ret = sceRegMgrSetKeyBin(g_regMgrSysCategory, g_regMgrSysName, g_regMgrSysData, size);
	else
		ret = sceRegMgrSetKeyStr(g_regMgrSysCategory, g_regMgrSysName, g_regMgrSysData, size);
	regMgrClearStatus();
	goto out;
invalid:
	ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* get a string or binary key to the user (name unknown) */
static inline __attribute__((always_inline)) int regMgrSysGetKey(const char *category, const char *name, void *buf,
					   SceSize size, int bin)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	if (category == NULL || name == NULL || buf == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	ret = regMgrSysCopyInName(category, name);
	if (ret != 0)
		goto out;
	if (size >= REGMGR_SYS_DATA_SIZE)
		size = REGMGR_SYS_DATA_SIZE;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	if (bin)
		ret = sceRegMgrGetKeyBin(g_regMgrSysCategory, g_regMgrSysName, g_regMgrSysData, size);
	else
		ret = sceRegMgrGetKeyStr(g_regMgrSysCategory, g_regMgrSysName, g_regMgrSysData, size);
	regMgrClearStatus();
	if (ret == 0)
		ret = ksceKernelCopyToUser(buf, g_regMgrSysData, size);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeyStrInternal2(const char *category, const char *name, const char *buf,
				SceSize size)
{
	return regMgrSysSetKey(category, name, buf, size, 0);
}

int sceRegMgrGetKeyStrInternal2(const char *category, const char *name, char *buf, SceSize size)
{
	return regMgrSysGetKey(category, name, buf, size, 0);
}

int sceRegMgrSetKeyBinInternal2(const char *category, const char *name, const void *buf,
				SceSize size)
{
	return regMgrSysSetKey(category, name, buf, size, 1);
}

int sceRegMgrGetKeyBinInternal2(const char *category, const char *name, void *buf, SceSize size)
{
	return regMgrSysGetKey(category, name, buf, size, 1);
}

int sceRegMgrGetKeysInfoInternal2(const char *category, RegMgrKeyInfo *keys, int count)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	ret = regMgrSysCopyInKeys(category, keys, count, 2);
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrGetKeysInfo(g_regMgrSysCategory, g_regMgrSysKeys, count);
	regMgrClearStatus();
	if (ret == 0)
		ret = regMgrSysCopyOutKeys(keys, count, 0);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSetKeysInternal2(const char *category, const RegMgrKeyInfo *keys, int count)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	ret = regMgrSysCopyInKeys(category, keys, count, 0);
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrSetKeys(g_regMgrSysCategory, g_regMgrSysKeys, count);
	regMgrClearStatus();
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetKeysInternal2(const char *category, RegMgrKeyInfo *keys, int count)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	ret = regMgrSysCopyInKeys(category, keys, count, REGMGR_COPY_KERNEL);
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrGetKeys(g_regMgrSysCategory, g_regMgrSysKeys, count);
	regMgrClearStatus();
	if (ret == 0)
		ret = regMgrSysCopyOutKeys(keys, count, REGMGR_COPY_KERNEL);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetInitValsInternal2(const char *category, RegMgrKeyInfo *keys, int count)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	if (ksceSblACMgrIsSystemProgram(0) != 1) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	ret = regMgrSysCopyInKeys(category, keys, count, REGMGR_COPY_KERNEL);
	if (ret != 0)
		goto out;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrGetInitVals(g_regMgrSysCategory, g_regMgrSysKeys, count);
	regMgrClearStatus();
	if (ret == 0)
		ret = regMgrSysCopyOutKeys(keys, count, REGMGR_COPY_KERNEL);
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* a callback API on the kernel UID of the user's callback (name unknown) */
static inline __attribute__((always_inline)) int regMgrSysCallback(SceUID cbid, int (*func)(SceUID cbid))
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			ret = kscePUIDtoGUID(0, cbid);
			if (ret >= 0) {
				regMgrSetStatus(REGMGR_STATUS_SYSCALL);
				ret = func(ret);
				regMgrClearStatus();
			}
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrRegisterCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrRegisterCallback);
}

int sceRegMgrAddRegistryCallbackInternal2(SceUID cbid, const char *path)
{
	int lock = 0;
	int ret, guid;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			ret = guid = kscePUIDtoGUID(0, cbid);
			if (guid >= 0) {
				if (path == NULL) {
					ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
				} else {
					ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, path,
									sizeof(g_regMgrSysCategory));
					if (ret >= 0) {
						if (ret < (int)sizeof(g_regMgrSysCategory)) {
							regMgrSetStatus(REGMGR_STATUS_SYSCALL);
							ret = sceRegMgrAddRegistryCallback(guid,
										g_regMgrSysCategory);
							regMgrClearStatus();
						} else {
							ret = SCE_REGMGR_ERROR_BAD_CATEGORY;
						}
					}
				}
			}
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrStartCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrStartCallback);
}

int sceRegMgrStopCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrStopCallback);
}

int sceRegMgrUnregisterCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrUnregisterCallback);
}

int sceRegMgrRegisterDrvErrCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrRegisterDrvErrCallback);
}

int sceRegMgrUnregisterDrvErrCallbackInternal2(SceUID cbid)
{
	return regMgrSysCallback(cbid, sceRegMgrUnregisterDrvErrCallback);
}

int sceRegMgrDbBackupInternal2(const char *path, int level)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			if (path == NULL)
				goto invalid;
			ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, path,
							sizeof(g_regMgrSysCategory));
			if (ret < 0)
				goto out;
			if (ret >= (int)sizeof(g_regMgrSysCategory))
				goto invalid;
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrDbBackup(g_regMgrSysCategory, level);
			regMgrClearStatus();
			goto out;
invalid:
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrDbRestoreInternal2(const char *path)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			if (path == NULL)
				goto invalid;
			ret = ksceKernelStrncpyFromUser(g_regMgrSysCategory, path,
							sizeof(g_regMgrSysCategory));
			if (ret < 0)
				goto out;
			if (ret >= (int)sizeof(g_regMgrSysCategory))
				goto invalid;
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrDbRestore(g_regMgrSysCategory);
			regMgrClearStatus();
			goto out;
invalid:
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

/* whether an error is one for the blue screen; asked even when the call is not
 * allowed, of that error */
int sceRegMgrIsBlueScreenInternal2(int error)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		error = ret;
	if (ksceSblACMgrIsSystemProgram(0) != 1)
		error = SCE_ERROR_ERRNO_ENOSYS;
	regMgrSetStatus(REGMGR_STATUS_SYSCALL);
	ret = sceRegMgrIsBlueScreen(error);
	if ((SceUInt32)ret > 1)
		ret = 0;
	regMgrClearStatus();
	regMgrComApiEpilogue(lock);
	return ret;
}

/* an API of no arguments (name unknown) */
static inline __attribute__((always_inline)) int regMgrSysCall(int (*func)(void))
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = func();
			regMgrClearStatus();
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrGetRegVersionInternal2(void)
{
	return regMgrSysCall(sceRegMgrGetRegVersion);
}

int sceRegMgrSrvGetRegionInternal2(void)
{
	return regMgrSysCall(sceRegMgrSrvGetRegion);
}

int sceRegMgrSrvGetRegionStrInternal2(char *buf, SceSize size)
{
	char name[4];
	int lock = 0;
	SceSize len;
	int ret, err;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			memset(name, 0, sizeof(name));
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrSrvGetRegionStr(name, sizeof(name));
			regMgrClearStatus();
			if (ret > 0) {
				if (buf == NULL || (len = strnlen(name, size)) >= size) {
					ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
				} else {
					err = ksceKernelCopyToUser(buf, name, len + 1);
					if (err != 0)
						ret = err;
				}
			}
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSrvCnvRegionStrInternal2(int region, char *buf, SceSize size)
{
	char name[4];
	int lock = 0;
	SceSize len;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			memset(name, 0, sizeof(name));
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrSrvCnvRegionStr(region, name, sizeof(name));
			regMgrClearStatus();
			if (ret == 0) {
				if (buf == NULL || (len = strnlen(name, size)) >= size)
					ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
				else
					ret = ksceKernelCopyToUser(buf, name, len + 1);
			}
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSrvCnvRegionIntInternal2(const char *name)
{
	char buf[4];
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			if (name == NULL)
				goto invalid;
			ret = ksceKernelStrncpyFromUser(buf, name, sizeof(buf));
			if (ret < 0)
				goto out;
			if ((SceUInt32)ret >= sizeof(buf))
				goto invalid;
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrSrvCnvRegionInt(buf);
			regMgrClearStatus();
			goto out;
invalid:
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSrvCnvRegionPsCodeInternal2(int region)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (ksceSblACMgrIsSystemProgram(0) == 1) {
			regMgrSetStatus(REGMGR_STATUS_SYSCALL);
			ret = sceRegMgrSrvCnvRegionPsCode(region);
			regMgrClearStatus();
		} else {
			ret = SCE_ERROR_ERRNO_ENOSYS;
		}
	}
	regMgrComApiEpilogue(lock);
	return ret;
}

/* set a system parameter, with the kernel's permission; the value is copied in
 * from the user first (names unknown) */
static inline __attribute__((always_inline)) int regMgrSysParamSet(int id, int value, const void *buf, SceSize size,
					     int type)
{
	const RegMgrSysParam *param;
	int lock = 0;
	int ret, err, perm;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto out;
	param = regMgrSysFindParam(id);
	if (param == NULL) {
		ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (param->write != 1) {
		ret = SCE_REGMGR_ERROR_NO_ACCESS;
		goto out;
	}
	ret = perm = SceThreadmgrForDriver_332E127C();
	if (perm < 0)
		goto out;
	if (type == REGMGR_TYPE_INT) {
		err = ksceKernelSetPermission(0x40);
		if (err >= 0) {
			regMgrSetStatus(REGMGR_STATUS_FLAGS);
			err = sceRegMgrSetKeyInt(g_regMgrSysParamCategory[param->categoryNo],
						 g_regMgrSysParamName[param->nameNo], value);
			regMgrClearStatus();
		}
	} else if (buf == NULL || size > REGMGR_SYS_STR_MAX) {
		err = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	} else {
		err = ksceKernelCopyFromUser(g_regMgrSysData, buf, size);
		if (err == 0 && (err = ksceKernelSetPermission(0x40)) >= 0) {
			regMgrSetStatus(REGMGR_STATUS_FLAGS);
			if (type == REGMGR_TYPE_BIN)
				err = sceRegMgrSetKeyBin(g_regMgrSysParamCategory[param->categoryNo],
							 g_regMgrSysParamName[param->nameNo],
							 g_regMgrSysData, size);
			else
				err = sceRegMgrSetKeyStr(g_regMgrSysParamCategory[param->categoryNo],
							 g_regMgrSysParamName[param->nameNo],
							 g_regMgrSysData, size);
			regMgrClearStatus();
		}
	}
	ksceKernelSetPermission(perm);
	ret = err;
out:
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSystemParamSetInt(int id, int value)
{
	return regMgrSysParamSet(id, value, NULL, 0, REGMGR_TYPE_INT);
}

int sceRegMgrSystemParamGetInt(int id, int *value)
{
	return regMgrSysParamGetInt(id, value, 0);
}

int sceRegMgrSystemParamSetStr(int id, const char *buf, SceSize size)
{
	return regMgrSysParamSet(id, 0, buf, size, REGMGR_TYPE_STR);
}

int sceRegMgrSystemParamGetStr(int id, char *buf, SceSize size)
{
	return regMgrSysParamGetStr(id, buf, size, 0);
}

int sceRegMgrSystemParamSetBin(int id, const void *buf, SceSize size)
{
	return regMgrSysParamSet(id, 0, buf, size, REGMGR_TYPE_BIN);
}

int sceRegMgrSystemParamGetBin(int id, void *buf, SceSize size)
{
	return regMgrSysParamGetBin(id, buf, size, 0);
}

int sceRegMgrSystemIsBlueScreen(int error)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		error = ret;
	regMgrSetStatus(REGMGR_STATUS_FLAGS);
	ret = sceRegMgrIsBlueScreen(error);
	if ((SceUInt32)ret > 1)
		ret = 0;
	regMgrClearStatus();
	regMgrComApiEpilogue(lock);
	return ret;
}

int ksceRegMgrSystemParamGetInt(int id, int *value)
{
	return regMgrSysParamGetInt(id, value, REGMGR_COPY_KERNEL);
}

int ksceRegMgrSystemParamGetStr(int id, char *buf, SceSize size)
{
	return regMgrSysParamGetStr(id, buf, size, REGMGR_COPY_KERNEL);
}

int ksceRegMgrSystemParamGetBin(int id, void *buf, SceSize size)
{
	return regMgrSysParamGetBin(id, buf, size, REGMGR_COPY_KERNEL);
}

/* the utility's access to a system parameter: the alternative flags apply */
#define REGMGR_UTILITY(call)					\
	int lock = 0;						\
	int ret;						\
								\
	ret = regMgrComApiPrologue(&lock);			\
	if (ret >= 0) {						\
		regMgrSetStatus(REGMGR_STATUS_FLAGS_ALT);	\
		ret = call;					\
		regMgrClearStatus();				\
	}							\
	regMgrComApiEpilogue(lock);				\
	return ret

int sceRegMgrUtilitySetInt(int id, int value)
{
	REGMGR_UTILITY(sceRegMgrSystemParamSetInt(id, value));
}

int sceRegMgrUtilityGetInt(int id, int *value)
{
	REGMGR_UTILITY(regMgrSysParamGetInt(id, value, 0));
}

int sceRegMgrUtilitySetStr(int id, const char *buf, SceSize size)
{
	REGMGR_UTILITY(sceRegMgrSystemParamSetStr(id, buf, size));
}

int sceRegMgrUtilityGetStr(int id, char *buf, SceSize size)
{
	REGMGR_UTILITY(regMgrSysParamGetStr(id, buf, size, 0));
}

int sceRegMgrUtilitySetBin(int id, const void *buf, SceSize size)
{
	REGMGR_UTILITY(sceRegMgrSystemParamSetBin(id, buf, size));
}

int sceRegMgrUtilityGetBin(int id, void *buf, SceSize size)
{
	REGMGR_UTILITY(regMgrSysParamGetBin(id, buf, size, 0));
}
