#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <kernel/openpsid.h>

/* a context: the file, in a memory block */
typedef struct SceSblSpfsoCtx {
	SceUID uid;
	void *base;
	SceSize size;
} SceSblSpfsoCtx;

/* the activation key: the OpenPsId and a byte-wise sum of its words (names unknown) */
typedef struct SceActivationKey {
	SceKernelOpenPsId openPsId;
	SceUInt8 sum[4];
} SceActivationKey;

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
int sceSblPmMgrGetProductModeFromNVS(SceUInt8 *mode);
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

int SceSblPostSsMgrForDriver_2646DE64(const SceUInt32 *src, SceUInt32 *dst);	/* generate an app key */
/* vitasdk: ksceSblPostSsMgrInitializeSpfsoCtx */
int SceSblPostSsMgrForDriver_BDF18922(const char *path, SceSblSpfsoCtx *ctx);
/* vitasdk: ksceSblPostSsMgrVerifySpfsoCtx */
int SceSblPostSsMgrForDriver_686B9461(SceSblSpfsoCtx *ctx, void **data, SceSize *size);
/* vitasdk: ksceSblPostSsMgrReleaseSpfsoCtx */
int SceSblPostSsMgrForDriver_AD3B0078(SceSblSpfsoCtx *ctx);
/* vitasdk: ksceSblLicMgrGetActivationKey */
int SceSblPostSsMgrForDriver_F7F1015B(SceActivationKey *key);
int SceSblPostSsMgrForDriver_128FB35A(SceUID pid);	/* a utoken communication test process */
int sceSblSsDecryptSealedKey(const void *sealedKey, void *key);
