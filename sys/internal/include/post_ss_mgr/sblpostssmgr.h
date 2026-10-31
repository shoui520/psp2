#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <kernel/openpsid.h>

/* the SPFSO file's errors (names unknown) */
#define SCE_SBL_ERROR_SPFSO_NO_MEMORY		0x800f1b0c
#define SCE_SBL_ERROR_SPFSO_INVALID_ARGUMENT	0x800f1b16
#define SCE_SBL_ERROR_SPFSO_INVALID_FORMAT	0x800f1b24

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

/* a keystone (names unknown) */
typedef struct ScePostSsKeystone {
	char magic[8];
	SceUInt16 version;
	SceUInt16 type;
	SceUInt8 reserved[4];
	SceUInt8 nonce[0x10];
	SceUInt8 passcodeHmac[0x20];
	SceUInt8 hmac[0x20];
} ScePostSsKeystone;

int sceSblLicMgrActivateDevkit(const char *path);
/* vitasdk: ksceSblRSA2048CreateSignature */
int SceSblPostSsMgrForDriver_07BAD056(void *signature, const void *hash, const void *key, int type);
int sceSblLicMgrGetLicenseStatus(void);
SceUInt8 SceSblPostSsMgrForDriver_1923D80D(void);
int SceSblPostSsMgrForDriver_1FF699DD(void);	/* vitasdk: ksceSblUtMgrResetUtokenFile */
SceUInt8 SceSblPostSsMgrForDriver_22599675(void);
int SceSblPostSsMgrForDriver_2BF04B8E(void);
int SceSblPostSsMgrForDriver_2C463AF1(SceUInt32 type, SceSize size, void *key);
/* vitasdk: ksceSblRSA2048VerifySignature */
int SceSblPostSsMgrForDriver_3DB69911(const void *signature, const void *hash, const void *key, int type);
int SceSblPostSsMgrForDriver_3F9BDEDF(SceUInt32 time);	/* vitasdk: ksceSblPostSsMgrSetCpRtc */
int SceSblPostSsMgrForDriver_4100B9E9(void);
/* vitasdk: ksceSblPostSsMgrDebugEncryptKeystone */
int SceSblPostSsMgrForDriver_42474C8B(const void *src, ScePostSsKeystone *keystone);
int sceSblPmMgrGetProductModeFromNVS(SceUInt8 *mode);
int sceSblLicMgrGetExpireDate(SceUInt32 *date, int fromNvs);
int SceSblPostSsMgrForDriver_739C981E(SceUInt32 version, SceUInt32 index, void *keys);
SceUInt8 SceSblPostSsMgrForDriver_7ACCAA50(void);	/* vitasdk: ksceSblUtMgrHasComTestFlag */
int SceSblPostSsMgrForDriver_942010A0(SceUInt32 *time);	/* vitasdk: ksceSblRtcMgrGetCpRtcPhysical */
SceUInt8 SceSblPostSsMgrForDriver_9B49C249(void);
SceUInt8 SceSblPostSsMgrForDriver_9D1D8EE5(void);
SceUInt8 SceSblPostSsMgrForDriver_9D2E2D39(void);	/* vitasdk: ksceSblUtMgrHasStoreFlag */
SceUInt8 SceSblPostSsMgrForDriver_9FD835B0(void);	/* vitasdk: ksceSblUtMgrHasNpTestFlag */
int sceSblPmMgrSetProductMode(int on);
/* vitasdk: ksceSblUtMgrExecuteUtokenSmCommand1 */
int SceSblPostSsMgrForDriver_C2E58CE3(void *utoken, SceSize size);
int SceSblPostSsMgrForDriver_C6684F7E(void *keys);
SceUInt8 SceSblPostSsMgrForDriver_C93C0A0D(void);
int SceSblPostSsMgrForDriver_C977601E(void);
int SceSblPostSsMgrForDriver_CB5436BD(SceUInt32 type, SceSize size, void *key);
/* vitasdk: ksceSblPostSsMgrDebugDecryptKeystone */
int SceSblPostSsMgrForDriver_CC5AA5A5(const ScePostSsKeystone *keystone, void *dst);
int sceSblRtcMgrSetCpRtcLogical(void);
/* vitasdk: ksceSblPostSsMgrVerifyKeystone */
int SceSblPostSsMgrForDriver_DDA6FA6D(const ScePostSsKeystone *keystone, SceUInt16 type);
int sceSblRtcMgrGetCpRtcLogical(SceUInt32 *time);
/* vitasdk: ksceSblPostSsMgrVerifyKeystoneWithPasscode */
int SceSblPostSsMgrForDriver_F86F1452(const ScePostSsKeystone *keystone, const void *passcode);
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
int sceSblLicMgrGetActivationKey(SceActivationKey *key);
int SceSblPostSsMgrForDriver_128FB35A(SceUID pid);	/* a utoken communication test process */
int sceSblSsGenerateSealedKey(void *sealedKey);
int sceSblSsDecryptSealedKey(const void *sealedKey, void *key);
