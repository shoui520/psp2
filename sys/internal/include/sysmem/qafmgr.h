/*
 * SceQafMgrForDriver (exported by os0:kd/sysmem.skprx), as used so far.
 */

#pragma once

#include <psp2kern/types.h>

/* name unknown: nonzero when the QA flag allowing debug consoles is set */
int SceQafMgrForDriver_382C71E8(void);

/* name unknown: nonzero when host0: may be used for system modules (QA setting) */
int SceQafMgrForDriver_B9770A13(void);

int SceQafMgrForDriver_0E588747(void);
int SceQafMgrForDriver_3CB55F98(void);
int SceQafMgrForDriver_41E04800(void);
int SceQafMgrForDriver_4BC1883F(void);
int SceQafMgrForDriver_52B4E164(void);
int SceQafMgrForDriver_694D1096(void);
int SceQafMgrForDriver_8C423C18(void);
int SceQafMgrForDriver_AE033133(void);
int ksceSblQafMgrIsAllowControlIduAutoUpdate(void);
int ksceSblQafMgrIsAllowDecryptedBootConfigLoad(void);
int ksceSblQafMgrIsAllowDtcpIpReset(void);
int ksceSblQafMgrIsAllowHost0Access(void);
int ksceSblQafMgrIsAllowKeepCoreFile(void);
int ksceSblQafMgrIsAllowLoadMagicGate(void);
int ksceSblQafMgrIsAllowMarlinTest(void);
int ksceSblQafMgrIsAllowNearTest(void);
int ksceSblQafMgrIsAllowPSPEmuShowQAInfo(void);
int ksceSblQafMgrIsAllowRemotePlayDebug(void);
int ksceSblQafMgrIsAllowSystemAppDebug(void);
