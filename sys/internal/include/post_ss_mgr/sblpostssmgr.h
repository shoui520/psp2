#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

/* vitasdk: ksceSblPostSsMgrActivate */
int SceSblPostSsMgrForDriver_0298382B(const char *path);
/* vitasdk: ksceSblRSA2048CreateSignature */
int SceSblPostSsMgrForDriver_07BAD056(void *signature, const void *hash, const void *key, int type);
int SceSblPostSsMgrForDriver_15F37282(void);	/* vitasdk: ksceSblLicMgrGetLicenseStatus */
SceUInt8 SceSblPostSsMgrForDriver_1923D80D(void);
int SceSblPostSsMgrForDriver_1FF699DD(void);	/* vitasdk: ksceSblUtMgrResetUtokenFile */
SceUInt8 SceSblPostSsMgrForDriver_22599675(void);
int SceSblPostSsMgrForDriver_2BF04B8E(void);
int SceSblPostSsMgrForDriver_2C463AF1(SceUInt32 type, SceSize size, void *key);
/* vitasdk: ksceSblRSA2048VerifySignature */
int SceSblPostSsMgrForDriver_3DB69911(const void *signature, const void *hash, const void *key, int type);
int SceSblPostSsMgrForDriver_3F9BDEDF(SceUInt32 time);	/* vitasdk: ksceSblPostSsMgrSetCpRtc */
int SceSblPostSsMgrForDriver_4100B9E9(void);
/* vitasdk: ksceSblPostSsMgrExecutePmSmF00dCommand8 */
int SceSblPostSsMgrForDriver_4663C195(SceUInt8 *mode);
/* vitasdk: ksceSblPostSsMgrGetExpireDate */
int SceSblPostSsMgrForDriver_4FF2682F(SceUInt32 *date, int fromNvs);
int SceSblPostSsMgrForDriver_739C981E(SceUInt32 version, SceUInt32 index, void *keys);
SceUInt8 SceSblPostSsMgrForDriver_7ACCAA50(void);	/* vitasdk: ksceSblUtMgrHasComTestFlag */
int SceSblPostSsMgrForDriver_942010A0(SceUInt32 *time);	/* vitasdk: ksceSblRtcMgrGetCpRtcPhysical */
SceUInt8 SceSblPostSsMgrForDriver_9B49C249(void);
SceUInt8 SceSblPostSsMgrForDriver_9D1D8EE5(void);
SceUInt8 SceSblPostSsMgrForDriver_9D2E2D39(void);	/* vitasdk: ksceSblUtMgrHasStoreFlag */
SceUInt8 SceSblPostSsMgrForDriver_9FD835B0(void);	/* vitasdk: ksceSblUtMgrHasNpTestFlag */
/* vitasdk: ksceSblPostSsMgrExecutePmSmF00dCommand */
int SceSblPostSsMgrForDriver_ADF92824(int on);
/* vitasdk: ksceSblUtMgrExecuteUtokenSmCommand1 */
int SceSblPostSsMgrForDriver_C2E58CE3(void *utoken, SceSize size);
int SceSblPostSsMgrForDriver_C6684F7E(void *keys);
SceUInt8 SceSblPostSsMgrForDriver_C93C0A0D(void);
int SceSblPostSsMgrForDriver_C977601E(void);
int SceSblPostSsMgrForDriver_CB5436BD(SceUInt32 type, SceSize size, void *key);
int SceSblPostSsMgrForDriver_DC68EE5C(void);
int SceSblPostSsMgrForDriver_DE5150FE(SceUInt32 *time);	/* vitasdk: ksceSblRtcMgrGetCpRtcLogical */
/* vitasdk: _ksceSblPostSsMgrExecutePmSmF00dCommand */
int SceSblPostSsMgrForDriver_FE92A318(SceUInt32 flags);
int sceSblPmMgrAuthEtoI(void);
