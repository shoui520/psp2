/* Sony's aimgr_api.c: console identity values. The PS code and OpenPSID come from the boot
 * parameters; the rest is queried from the secure processor under one mutex. */
#include "ssmgr_internal.h"
#include <sysmem/sysroot.h>
#include <sysmem/pmmgr.h>
#include <sysmem/sblaimgr.h>
#include <kernel_boot_loader/kbl_param.h>
#include <threadmgr/threadmgr.h>
#include <string.h>

ScePsCode aimgrPscode;
SceKernelOpenPsId aimgrOpenPsId;
SceUID aimgrMutex;

static inline unsigned int swap16(unsigned int value)
{
	return (value >> 8) | (value << 8);
}

/* Internal name inferred: module_start's Aimgr setup. */
int aimgrInit(void)
{
	const SceKblParam *parameters = sceKernelSysrootGetKblParam();
	SceUID mutex;

	if (parameters) {
		aimgrPscode = parameters->pscode;
		aimgrOpenPsId = parameters->openpsid;
	} else {
		aimgrPscode = (ScePsCode){ 0 };
		aimgrOpenPsId = (SceKernelOpenPsId){ { 0 } };
	}
	mutex = sceKernelCreateMutex("SceSblAimgrSm", 0, 0, NULL);
	aimgrMutex = mutex;
	return mutex >> 31;
}

int sceSblAimgrGetPscode(ScePsCode *pscode)
{
	if (!pscode)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;

	*pscode = aimgrPscode;
	pscode->company_code = swap16(pscode->company_code);
	pscode->product_code = swap16(pscode->product_code);
	pscode->product_sub_code = swap16(pscode->product_sub_code);
	pscode->factory_code = swap16(pscode->factory_code);
	return 0;
}

int sceSblAimgrGetPscode2(ScePsCode *pscode)
{
	uint8_t productMode = 0;
	ScePsCode value;
	int ret, permission;

	if (!pscode)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
	if (sceSblPmMgrGetProductMode(&productMode))
		return SCE_SBL_ERROR_AIMGR_PRODUCT_MODE;
	if (!productMode)
		return SCE_SBL_ERROR_AIMGR_ILLEGAL_STATE;

	ret = sceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	permission = sceKernelSetThreadAccessLevel(0x80);
	if (permission < 0) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return permission;
	}
	ret = aimgrReadPscode2(&value);
	if (ret) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	ret = sceKernelSetThreadAccessLevel(permission);
	if (ret < 0) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	ret = sceKernelUnlockMutex(aimgrMutex, 1);
	if (ret)
		return ret;

	pscode->company_code = swap16(value.company_code);
	pscode->product_code = swap16(value.product_code);
	pscode->product_sub_code = swap16(value.product_sub_code);
	pscode->factory_code = swap16(value.factory_code);
	return 0;
}

int sceSblAimgrGetConsoleId(SceConsoleId *consoleId)
{
	int ret;

	if (!consoleId)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;

	ret = sceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrReadConsoleId(consoleId);
	if (ret) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return sceKernelUnlockMutex(aimgrMutex, 1);
}

int sceSblAimgrGetOpenPsId(SceKernelOpenPsId *openPsId)
{
	if (!openPsId)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;

	*openPsId = aimgrOpenPsId;
	return 0;
}

int sceSblAimgrGetVisibleId(void *visibleId)
{
	int ret;

	if (!visibleId)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;

	ret = sceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrReadVisibleId(visibleId);
	if (ret) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return sceKernelUnlockMutex(aimgrMutex, 1);
}

int SceSblSsMgrForDriver_B8B298FD(const uint32_t *parameters, void *passPhrase)
{
	int ret;

	if (!parameters || !passPhrase)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;
	ret = sceKernelLockMutex(aimgrMutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = aimgrCreatePassPhrase(parameters, passPhrase);
	if (ret) {
		sceKernelUnlockMutex(aimgrMutex, 1);
		return ret;
	}
	return sceKernelUnlockMutex(aimgrMutex, 1);
}

