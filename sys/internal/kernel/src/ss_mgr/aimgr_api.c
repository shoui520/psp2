/* Sony's aimgr_api.c: console identity values. The PS code and OpenPSID come from the boot
 * parameters; the rest is queried from the secure processor under one mutex. */
#include "ssmgr_internal.h"
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/threadmgr.h>
#include <string.h>
#include <psp2kern/kernel/threadmgr/misc.h>

ScePsCode aimgrPscode;
SceOpenPsId aimgrOpenPsId;
SceUID aimgrMutex;

int kscePmMgrGetProductMode(uint8_t *mode);
int aimgrCreatePassPhrase(const uint32_t *parameters, void *passPhrase);

#define AIMGR_ERROR_INVALID_ARGUMENT	0x800F0C16
#define AIMGR_ERROR_PRODUCT_MODE	0x800F0C01
#define AIMGR_ERROR_NOT_PRODUCTION	0x800F0C29

static inline unsigned int swap16(unsigned int value)
{
	return (value >> 8) | (value << 8);
}

/* Internal name inferred: module_start's Aimgr setup. */
int aimgrInit(void)
{
	const SceKblParam *parameters = ksceKernelSysrootGetKblParam();
	SceUID mutex;

	if (parameters) {
		aimgrPscode = parameters->pscode;
		aimgrOpenPsId = parameters->openpsid;
	} else {
		aimgrPscode = (ScePsCode){ 0 };
		aimgrOpenPsId = (SceOpenPsId){ { 0 } };
	}
	mutex = ksceKernelCreateMutex("SceSblAimgrSm", 0, 0, NULL);
	aimgrMutex = mutex;
	return mutex >> 31;
}

int ksceSblAimgrGetPscode(ScePsCode *pscode)
{
	if (!pscode)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	*pscode = aimgrPscode;
	pscode->company_code = swap16(pscode->company_code);
	pscode->product_code = swap16(pscode->product_code);
	pscode->product_sub_code = swap16(pscode->product_sub_code);
	pscode->factory_code = swap16(pscode->factory_code);
	return 0;
}

int ksceSblAimgrGetPscode2(ScePsCode *pscode)
{
	uint8_t productMode = 0;
	ScePsCode value;
	int ret, permission;

	if (!pscode)
		return AIMGR_ERROR_INVALID_ARGUMENT;
	if (kscePmMgrGetProductMode(&productMode))
		return AIMGR_ERROR_PRODUCT_MODE;
	if (!productMode)
		return AIMGR_ERROR_NOT_PRODUCTION;

	ret = ksceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	permission = ksceKernelSetPermission(0x80);
	if (permission < 0) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return permission;
	}
	ret = aimgrReadPscode2(&value);
	if (ret) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	ret = ksceKernelSetPermission(permission);
	if (ret < 0) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	ret = ksceKernelUnlockMutex(aimgrMutex, 1);
	if (ret)
		return ret;

	pscode->company_code = swap16(value.company_code);
	pscode->product_code = swap16(value.product_code);
	pscode->product_sub_code = swap16(value.product_sub_code);
	pscode->factory_code = swap16(value.factory_code);
	return 0;
}

int ksceSblAimgrGetConsoleId(SceConsoleId *consoleId)
{
	int ret;

	if (!consoleId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	ret = ksceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrReadConsoleId(consoleId);
	if (ret) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return ksceKernelUnlockMutex(aimgrMutex, 1);
}

int ksceSblAimgrGetOpenPsId(SceOpenPsId *openPsId)
{
	if (!openPsId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	*openPsId = aimgrOpenPsId;
	return 0;
}

int ksceSblAimgrGetVisibleId(void *visibleId)
{
	int ret;

	if (!visibleId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	ret = ksceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrReadVisibleId(visibleId);
	if (ret) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return ksceKernelUnlockMutex(aimgrMutex, 1);
}

int ksceSblSsCreatePassPhrase(const uint32_t *parameters, void *passPhrase)
{
	int ret;

	if (!parameters || !passPhrase)
		return AIMGR_ERROR_INVALID_ARGUMENT;
	ret = ksceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrCreatePassPhrase(parameters, passPhrase);
	if (ret) {
		ksceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return ksceKernelUnlockMutex(aimgrMutex, 1);
}

