/*
 * SceSysmem: SceQafMgrForDriver, the QA flags of the KBL parameters. Each
 * query is one bit of SceKblParam.qa_flags.
 */

#include <psp2kern/kernel/kbl/kbl.h>

SceKblParam *ksceKernelSysrootGetKblParam(void);

/* bit `bit` of qa_flags[index] */
static inline int QafMgrFlag(unsigned int index, unsigned int bit)
{
	return (ksceKernelSysrootGetKblParam()->qa_flags[index] >> bit) & 1;
}

int SceQafMgrForDriver_382C71E8(void)
{
	return QafMgrFlag(0xd, 0);
}

int ksceSblQafMgrIsAllowSystemAppDebug(void)
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

int ksceSblQafMgrIsAllowLoadMagicGate(void)
{
	return QafMgrFlag(0xb, 4);
}

int SceQafMgrForDriver_3CB55F98(void)
{
	return QafMgrFlag(0xf, 0);
}

int SceQafMgrForDriver_8C423C18(void)
{
	return QafMgrFlag(0xf, 1);
}

int SceQafMgrForDriver_0E588747(void)
{
	return QafMgrFlag(0xf, 0);
}

int ksceSblQafMgrIsAllowHost0Access(void)
{
	return QafMgrFlag(0xe, 0);
}

int SceQafMgrForDriver_B9770A13(void)
{
	return QafMgrFlag(0xd, 1);
}

int ksceSblQafMgrIsAllowDecryptedBootConfigLoad(void)
{
	return QafMgrFlag(0xe, 0);
}

int SceQafMgrForDriver_52B4E164(void)
{
	return QafMgrFlag(0xd, 0);
}

int ksceSblQafMgrIsAllowRemotePlayDebug(void)
{
	return QafMgrFlag(0xc, 1);
}

int SceQafMgrForDriver_4BC1883F(void)
{
	return QafMgrFlag(0x6, 1);
}

int ksceSblQafMgrIsAllowPSPEmuShowQAInfo(void)
{
	return QafMgrFlag(0xf, 0);
}

int ksceSblQafMgrIsAllowNearTest(void)
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

int ksceSblQafMgrIsAllowMarlinTest(void)
{
	return QafMgrFlag(0xd, 1);
}

int SceQafMgrForDriver_70A67A4B(void)
{
	return QafMgrFlag(0xf, 0);
}

int ksceSblQafMgrIsAllowKeepCoreFile(void)
{
	return QafMgrFlag(0xf, 0);
}

int ksceSblQafMgrIsAllowDtcpIpReset(void)
{
	return QafMgrFlag(0x6, 1);
}

int SceQafMgrForDriver_DEC6DF4E(void)
{
	return QafMgrFlag(0xf, 0);
}

int ksceSblQafMgrIsAllowControlIduAutoUpdate(void)
{
	return QafMgrFlag(0x6, 1);
}
