/* Sony's aimgr_api.c: console identity values. The PS code and OpenPSID come from the boot
 * parameters; the rest is queried from the secure processor under one mutex. */
#include "ssmgr_internal.h"
#include <sysmem/sysroot.h>
#include <sysmem/pmmgr.h>
#include <kernel_boot_loader/kbl_param.h>
#include <threadmgr/threadmgr.h>
#include <string.h>

ScePsCode aimgrPscode;
SceKernelOpenPsId aimgrOpenPsId;
SceUID aimgrMutex;

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
	const SceKblParam *parameters = SceSysrootForKernel_9DB56D1F();
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

int SceSblSsMgrForDriver_E0DC2587(ScePsCode *pscode)
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

int SceSblSsMgrForDriver_9A9676D0(ScePsCode *pscode)
{
	uint8_t productMode = 0;
	ScePsCode value;
	int ret, permission;

	if (!pscode)
		return AIMGR_ERROR_INVALID_ARGUMENT;
	if (ScePmMgrForDriver_2AC815A2(&productMode))
		return AIMGR_ERROR_PRODUCT_MODE;
	if (!productMode)
		return AIMGR_ERROR_NOT_PRODUCTION;

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

int SceSblSsMgrForDriver_FC6CDD68(SceConsoleId *consoleId)
{
	int ret;

	if (!consoleId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

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

int SceSblSsMgrForDriver_A5B5D269(SceKernelOpenPsId *openPsId)
{
	if (!openPsId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	*openPsId = aimgrOpenPsId;
	return 0;
}

int SceSblSsMgrForDriver_04843835(void *visibleId)
{
	int ret;

	if (!visibleId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

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
		return AIMGR_ERROR_INVALID_ARGUMENT;
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

