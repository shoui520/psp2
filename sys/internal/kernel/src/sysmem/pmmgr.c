/*
 * SceSysmem: SceSblPmMgr's queries of the boot type in the KBL parameters.
 */

#include <kernel_boot_loader/kbl_param.h>
#include <sysmem/pmmgr.h>

/* name unknown */
#define PMMGR_ERROR_INVALID_ARGUMENT	0x800f0a16
#define PMMGR_ERROR_NO_KBL_PARAM	0x800f0a29

#define BOOT_TYPE_EXTERNAL		0x1
#define BOOT_TYPE_PRODUCT_MODE		0x4

SceKblParam *SceSysrootForKernel_9DB56D1F(void);

int ScePmMgrForDriver_2AC815A2(SceProductMode *mode)
{
	SceKblParam *kbl;

	if (mode == NULL)
		return PMMGR_ERROR_INVALID_ARGUMENT;
	kbl = SceSysrootForKernel_9DB56D1F();
	if (kbl == NULL)
		return PMMGR_ERROR_NO_KBL_PARAM;
	*mode = (kbl->boot_type_indicator_1 & BOOT_TYPE_PRODUCT_MODE) != 0;
	return 0;
}

int ScePmMgrForDriver_BD1F193B(void)
{
	SceKblParam *kbl = SceSysrootForKernel_9DB56D1F();

	if (kbl == NULL)
		return PMMGR_ERROR_NO_KBL_PARAM;
	return kbl->boot_type_indicator_1 & BOOT_TYPE_EXTERNAL;
}
