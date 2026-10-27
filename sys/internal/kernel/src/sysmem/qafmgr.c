/*
 * SceSysmem: SceQafMgrForDriver, the QA flags of the KBL parameters. Each
 * query is one bit of SceKblParam.qa_flags.
 */

#include <kernel_boot_loader/kbl_param.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

/* bit `bit` of qa_flags[index] */
static inline int QafMgrFlag(unsigned int index, unsigned int bit)
{
	return (sceKernelSysrootGetKblParam()->qa_flags[index] >> bit) & 1;
}

int SceQafMgrForDriver_382C71E8(void)
{
	return QafMgrFlag(0xd, 0);
}

/* the boot loader (SYSMEM_NSKBL) keeps only 382C71E8 and 082A4FC2 */
#if !SYSMEM_NSKBL
int SceQafMgrForDriver_CAD47130(void)
{
	return QafMgrFlag(0xd, 1);
}

int SceQafMgrForDriver_694D1096(void)
{
	return QafMgrFlag(0xb, 2);
}

int SceQafMgrForDriver_41E04800(void)
{
	return QafMgrFlag(0xb, 2);
}

int SceQafMgrForDriver_36E5312E(void)
{
	return QafMgrFlag(0xb, 4);
}

int SceQafMgrForDriver_3CB55F98(void)
{
	return QafMgrFlag(0xf, 0);
}

int sceSblQafMgrIsAllowForceUpdate(void)
{
	return QafMgrFlag(0xf, 1);
}

int SceQafMgrForDriver_0E588747(void)
{
	return QafMgrFlag(0xf, 0);
}

#endif

int SceQafMgrForDriver_082A4FC2(void)
{
	return QafMgrFlag(0xe, 0);
}

#if !SYSMEM_NSKBL
int SceQafMgrForDriver_B9770A13(void)
{
	return QafMgrFlag(0xd, 1);
}

int SceQafMgrForDriver_883E9465(void)
{
	return QafMgrFlag(0xe, 0);
}

int SceQafMgrForDriver_52B4E164(void)
{
	return QafMgrFlag(0xd, 0);
}

int SceQafMgrForDriver_BFD5E463(void)
{
	return QafMgrFlag(0xc, 1);
}

int SceQafMgrForDriver_4BC1883F(void)
{
	return QafMgrFlag(0x6, 1);
}

int SceQafMgrForDriver_B7B195B2(void)
{
	return QafMgrFlag(0xf, 0);
}

int SceQafMgrForDriver_9644171D(void)
{
	return QafMgrFlag(0x6, 1);
}

int SceQafMgrForDriver_E573F124(void)
{
	return QafMgrFlag(0x0, 4);
}

int SceQafMgrForDriver_7B14DC45(void)
{
	return QafMgrFlag(0xd, 1);
}

int SceQafMgrForDriver_AE033133(void)
{
	return QafMgrFlag(0x0, 4);
}

int SceQafMgrForDriver_10283EB8(void)
{
	return QafMgrFlag(0xd, 1);
}

int SceQafMgrForDriver_70A67A4B(void)
{
	return QafMgrFlag(0xf, 0);
}

int SceQafMgrForDriver_C1EA75C8(void)
{
	return QafMgrFlag(0xf, 0);
}

int SceQafMgrForDriver_E8B8F31F(void)
{
	return QafMgrFlag(0x6, 1);
}

int SceQafMgrForDriver_DEC6DF4E(void)
{
	return QafMgrFlag(0xf, 0);
}

int SceQafMgrForDriver_F8BFEE48(void)
{
	return QafMgrFlag(0x6, 1);
}
#endif
