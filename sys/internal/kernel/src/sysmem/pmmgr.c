/*
 * SceSysmem: SceSblPmMgr's queries of the boot type in the KBL parameters.
 */

#include <psp2kern/kernel/kbl/kbl.h>

/* name unknown */
#define PMMGR_ERROR_INVALID_ARGUMENT	0x800f0a16
#define PMMGR_ERROR_NO_KBL_PARAM	0x800f0a29

#define BOOT_TYPE_EXTERNAL		0x1
#define BOOT_TYPE_PRODUCT_MODE		0x4

SceKblParam *ksceKernelSysrootGetKblParam(void);

int kscePmMgrGetProductMode(SceUInt8 *mode)
{
	SceKblParam *kbl;

	if (mode == NULL)
		return PMMGR_ERROR_INVALID_ARGUMENT;
	kbl = ksceKernelSysrootGetKblParam();
	if (kbl == NULL)
		return PMMGR_ERROR_NO_KBL_PARAM;
	*mode = (kbl->boot_type_indicator_1 & BOOT_TYPE_PRODUCT_MODE) != 0;
	return 0;
}

int kscePmMgrIsExternalBootMode(void)
{
	SceKblParam *kbl = ksceKernelSysrootGetKblParam();

	if (kbl == NULL)
		return PMMGR_ERROR_NO_KBL_PARAM;
	return kbl->boot_type_indicator_1 & BOOT_TYPE_EXTERNAL;
}
