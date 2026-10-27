/*
 * SceQafMgrForDriver (exported by os0:kd/sysmem.skprx), as used so far.
 */

#pragma once

#include <scetypes.h>

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
int sceSblQafMgrIsAllowForceUpdate(void);
int SceQafMgrForDriver_AE033133(void);
int SceQafMgrForDriver_F8BFEE48(void);	/* vitasdk: ksceSblQafMgrIsAllowControlIduAutoUpdate */
int SceQafMgrForDriver_883E9465(void);	/* vitasdk: ksceSblQafMgrIsAllowDecryptedBootConfigLoad */
int SceQafMgrForDriver_E8B8F31F(void);	/* vitasdk: ksceSblQafMgrIsAllowDtcpIpReset */
int SceQafMgrForDriver_082A4FC2(void);	/* vitasdk: ksceSblQafMgrIsAllowHost0Access */
int SceQafMgrForDriver_C1EA75C8(void);	/* vitasdk: ksceSblQafMgrIsAllowKeepCoreFile */
int SceQafMgrForDriver_36E5312E(void);	/* vitasdk: ksceSblQafMgrIsAllowLoadMagicGate */
int SceQafMgrForDriver_10283EB8(void);	/* vitasdk: ksceSblQafMgrIsAllowMarlinTest */
int SceQafMgrForDriver_9644171D(void);	/* vitasdk: ksceSblQafMgrIsAllowNearTest */
int SceQafMgrForDriver_B7B195B2(void);	/* vitasdk: ksceSblQafMgrIsAllowPSPEmuShowQAInfo */
int SceQafMgrForDriver_BFD5E463(void);	/* vitasdk: ksceSblQafMgrIsAllowRemotePlayDebug */
int SceQafMgrForDriver_CAD47130(void);	/* vitasdk: ksceSblQafMgrIsAllowSystemAppDebug */

int SceQafMgrForDriver_70A67A4B(void);
int SceQafMgrForDriver_7B14DC45(void);
int SceQafMgrForDriver_DEC6DF4E(void);
int SceQafMgrForDriver_E573F124(void);
