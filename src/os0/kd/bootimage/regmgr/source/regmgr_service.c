/*
 * regmgr_service.c: the region services: the console's region, and its
 * conversions between the region number, its name and the PS code.
 */

#include <psp2kern/kernel/aimgr.h>
#include "regmgr_internal.h"

#define REGMGR_API_SRV_GET_REGION		21	/* names unknown */
#define REGMGR_API_SRV_GET_REGION_STR		22
#define REGMGR_API_SRV_CNV_REGION_STR		23
#define REGMGR_API_SRV_CNV_REGION_INT		24
#define REGMGR_API_SRV_CNV_REGION_PS_CODE	25

#define REGMGR_REGION_MAX	13	/* the regions are numbered from 1 (name unknown) */
#define REGMGR_REGION_NAME	4

int sceRegMgrSrvGetRegion(void)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrGetRegionNo();
	if (ret < 0 && regMgrGetErrEnable())
		regMgrRecordError(ret, REGMGR_API_SRV_GET_REGION, NULL, NULL, NULL, sizeof(int));
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSrvGetRegionStr(char *buf, SceSize size)
{
	int lock = 0;
	int ret, err;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		ret = regMgrGetRegionNo();
		if (ret > 0) {
			err = sceRegMgrSrvCnvRegionStr(ret, buf, size);
			if (err != 0)
				ret = err;
		}
	}
	if (ret < 0 && regMgrGetErrEnable())
		regMgrRecordError(ret, REGMGR_API_SRV_GET_REGION_STR, NULL, NULL, buf, size);
	regMgrComApiEpilogue(lock);
	return ret;
}

int sceRegMgrSrvCnvRegionStr(int region, char *buf, SceSize size)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0) {
		if (buf != NULL && size >= REGMGR_REGION_NAME &&
		    (SceUInt32)(region - 1) < REGMGR_REGION_MAX)
			strncpy(buf, regMgrGetRegionName(region), size);
		else
			ret = SCE_REGMGR_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0 && regMgrGetErrEnable())
		regMgrRecordError(ret, REGMGR_API_SRV_CNV_REGION_STR, NULL, NULL, &region,
				  sizeof(region));
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the number of a region's name (0.990 names it regMgrComGetRegionInt) */
static inline __attribute__((always_inline)) int regMgrComGetRegionInt(const char *name)
{
	int region;

	if (name != NULL && strnlen(name, REGMGR_REGION_NAME) < REGMGR_REGION_NAME) {
		for (region = 1; region <= REGMGR_REGION_MAX; region++) {
			if (strncmp(name, regMgrGetRegionName(region), REGMGR_REGION_NAME) == 0)
				return region;
		}
	}
	return SCE_REGMGR_ERROR_INVALID_ARGUMENT;
}

int sceRegMgrSrvCnvRegionInt(const char *name)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret >= 0)
		ret = regMgrComGetRegionInt(name);
	if (ret < 0 && regMgrGetErrEnable())
		regMgrRecordError(ret, REGMGR_API_SRV_CNV_REGION_INT, NULL, NULL, name,
				  REGMGR_REGION_NAME);
	regMgrComApiEpilogue(lock);
	return ret;
}

/* the product code of a region (0: the console's) */
int sceRegMgrSrvCnvRegionPsCode(int region)
{
	int lock = 0;
	int ret;

	ret = regMgrComApiPrologue(&lock);
	if (ret < 0)
		goto error;
	switch (region) {
	case 0:
	case 11:
		ret = ksceSblAimgrGetProductCode();
		if (ret < 0)
			goto error;
		if (region == 11 && (SceUInt32)(ret - 0x100) >= 3)
			goto bad_product;
		break;
	case 1:
		ret = 0x103;
		break;
	case 2:
		ret = 0x104;
		break;
	case 3:
		ret = 0x109;
		break;
	case 4:
		ret = 0x107;
		break;
	case 5:
		ret = 0x105;
		break;
	case 6:
		ret = 0x106;
		break;
	case 7:
		ret = 0x10a;
		break;
	case 8:
		ret = 0x10b;
		break;
	case 9:
		ret = 0x10c;
		break;
	case 10:
		ret = 0x108;
		break;
	case 13:
		ret = 0x10d;
		break;
	default:
	bad_product:
		ret = SCE_REGMGR_ERROR_BAD_PRODUCT;
	error:
		if (regMgrGetErrEnable())
			regMgrRecordError(ret, REGMGR_API_SRV_CNV_REGION_PS_CODE, NULL, NULL,
					  &region, sizeof(region));
		break;
	}
	regMgrComApiEpilogue(lock);
	return ret;
}
