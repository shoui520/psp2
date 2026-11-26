/*
 * np_common_service.cpp: SceNpCommon service backend for account ID tracking,
 * NP test/debug settings, and privacy confirmation (0x81329E20..0x8132A067).
 */
#include "np_manager_core.h"

#include <libkernel/libkernel.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>

extern "C" {
extern int sceSblQafMgrIsAllowNpTest(void);
extern int sceRegMgrGetKeyBinForUser(const char *category, const char *name,
				     void *buf, SceSize size);
extern int sceRegMgrSetKeyIntForUser(const char *category, const char *name, int value);
extern int sceRegMgrGetKeysForUser(const char *category, RegMgrKeyInfo *keys, int count);
extern int sceRegMgrSystemParamGetIntForUser(int id, int *outVal);
extern int vshNpDrmUpdateAccountId(SceUInt64 accountId);
}

#define NP_COMMON_REG_KEY_TYPE_INT		0
#define NP_COMMON_REG_KEY_TYPE_STR		1
#define NP_COMMON_REG_ERR_KEY_NOT_FOUND		-2146631666	/* 0x800D000E */
#define NP_COMMON_SYSPARAM_FULL_TEST		750541
#define NP_COMMON_SYSPARAM_DEBUG_FLAG		1401993

/* Cached NP account ID (0x81546810) */
static SceUInt64 s_npCommonAccountId = 0;

/* 0x81329E20 */
int npCommonServiceIsSignedIn(int *outSignedIn)
{
	*outSignedIn = (s_npCommonAccountId != 0) ? 1 : 0;
	return 0;
}

/* 0x81329E4A */
int npCommonServiceCompareAccountId(SceUInt64 accountId)
{
	return (accountId != s_npCommonAccountId) ? 1 : 0;
}

/* 0x81329E64 */
int npCommonServiceGetSettings(void *outSettings)
{
	NpCommonSettings *settings = static_cast<NpCommonSettings *>(outSettings);
	sceClibMemset(settings, 0, sizeof(NpCommonSettings));

	if (sceSblQafMgrIsAllowNpTest() != 1 &&
	    vshSblAimgrIsCEX() == 1 &&
	    vshSblUtMgrHasStoreFlag() != 1) {
		return 0;
	}

	RegMgrKeyInfo keys[3];
	sceClibMemset(keys, 0, sizeof(keys));

	keys[0].size = sizeof(RegMgrKeyInfo);
	keys[0].name = "nav_only";
	keys[0].type = NP_COMMON_REG_KEY_TYPE_INT;
	keys[0].valueSize = 4;

	keys[1].size = sizeof(RegMgrKeyInfo);
	keys[1].name = "np_ad_clock_diff";
	keys[1].type = NP_COMMON_REG_KEY_TYPE_INT;
	keys[1].valueSize = 4;

	keys[2].size = sizeof(RegMgrKeyInfo);
	keys[2].name = "np_geo_filtering";
	keys[2].type = NP_COMMON_REG_KEY_TYPE_INT;
	keys[2].valueSize = 4;

	sceRegMgrGetKeysForUser("/CONFIG/NP", keys, 3);

	int fullTestVal = 0;
	sceRegMgrSystemParamGetIntForUser(NP_COMMON_SYSPARAM_FULL_TEST, &fullTestVal);

	settings->fullTest = (fullTestVal != 0) ? 1 : 0;
	settings->navOnly = (keys[0].value != 0) ? 1 : 0;
	settings->adClockDiff = keys[1].value;
	settings->geoFiltering = keys[2].value;

	sceClibMemset(keys, 0, sizeof(keys));
	keys[0].size = sizeof(RegMgrKeyInfo);
	keys[0].name = "title_id";
	keys[0].type = NP_COMMON_REG_KEY_TYPE_STR;
	keys[0].valueSize = 38;
	keys[0].data = settings->titleId;

	int ret = sceRegMgrGetKeysForUser("/CONFIG/NP", keys, 1);
	if (ret >= 0 || ret == NP_COMMON_REG_ERR_KEY_NOT_FOUND) {
		int debugVal = 0;
		ret = sceRegMgrSystemParamGetIntForUser(NP_COMMON_SYSPARAM_DEBUG_FLAG, &debugVal);
		if (ret < 0)
			ret = 0;
		settings->debugFlag = debugVal;
	}
	return ret;
}

/* 0x81329FAC */
int npCommonServiceGetPrivacyConfirm(int *outConfirmed)
{
	sceClibMemset(outConfirmed, 0, sizeof(*outConfirmed));

	RegMgrKeyInfo key;
	sceClibMemset(&key, 0, sizeof(key));
	key.size = sizeof(RegMgrKeyInfo);
	key.name = "privacy_confirmation";
	key.type = NP_COMMON_REG_KEY_TYPE_INT;
	key.valueSize = 4;

	int ret = sceRegMgrGetKeysForUser("/CONFIG/NP2", &key, 1);
	if (ret >= 0)
		*outConfirmed = key.value;
	return ret;
}

/* 0x8132A016 */
int npCommonServiceSetPrivacyConfirm(int confirmed)
{
	return sceRegMgrSetKeyIntForUser("/CONFIG/NP2", "privacy_confirmation", confirmed);
}

/* 0x8132A030 */
int npCommonServiceLoadAccountId(void)
{
	return sceRegMgrGetKeyBinForUser("/CONFIG/NP", "account_id",
					 &s_npCommonAccountId, sizeof(s_npCommonAccountId));
}

/* 0x8132A052 */
int npCommonServiceUpdateAccountId(SceUInt64 accountId)
{
	s_npCommonAccountId = accountId;
	return vshNpDrmUpdateAccountId(accountId);
}
