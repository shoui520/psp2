#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <rtc.h>

/*
 * The NP DRM license (RIF), the PSM license and the activation data (act.dat). Layouts and
 * field names are the henkaku wiki's and vitasdk's (no Sony name found); fields nothing
 * reads are reserved.
 */
typedef struct SceNpDrmLicense {
	SceInt16 version;		/* -1 Vita (free), 0 PSP, 1 PSP on Vita */
	SceInt16 version_flags;
	SceInt16 license_type;
	SceInt16 license_flags;		/* 0x400: no ECDSA check */
	SceUInt64 account_id;		/* 0 for free content */
	char content_id[0x30];
	char key_table[0x10];
	char key1[0x10];
	SceRtcTick start_time;
	SceRtcTick expiration_time;
	char ecdsa_signature[0x28];
	SceInt64 flags;
	char key2[0x10];
	char reserved[0x10];
	char open_psid[0x10];
	char reserved2[0x10];
	char cmd56_handshake_part[0x14];
	int debug_upgradable;
	int reserved3;
	int sku_flag;
	char rsa_signature[0x100];
} SceNpDrmLicense;

_Static_assert(sizeof(SceNpDrmLicense) == 0x200, "SceNpDrmLicense");

typedef struct SceNpDrmActivationData {
	SceInt16 act_type;
	SceInt16 version_flag;
	SceInt32 version;
	SceUInt64 account_id;
	SceUInt8 primary_key_table[0x80][0x10];
	SceUInt8 reserved[0x40];
	SceUInt8 openpsid[0x10];
	SceUInt8 reserved2[0x20];
	SceUInt8 secondary_key_table[0x65][0x10];
	SceUInt8 rsa_signature[0x100];
	SceUInt8 reserved3[0x40];
	SceUInt8 ecdsa_signature[0x28];
} SceNpDrmActivationData;

_Static_assert(sizeof(SceNpDrmActivationData) == 0x1038, "SceNpDrmActivationData");

typedef struct ScePsmDrmKeySet {
	SceUInt8 hmac_key[0x20];
	SceUInt8 key[0x10];
	SceUInt8 signature[0x1d0];
} ScePsmDrmKeySet;

_Static_assert(sizeof(ScePsmDrmKeySet) == 0x200, "ScePsmDrmKeySet");

typedef struct ScePsmDrmExpireTime {
	SceRtcTick *start_date;
	SceRtcTick *end_date;
	int reserved[2];
} ScePsmDrmExpireTime;

_Static_assert(sizeof(ScePsmDrmExpireTime) == 0x10, "ScePsmDrmExpireTime");

typedef struct ScePsmDrmLicense {
	char magic[8];
	SceUInt32 reserved[2];
	SceUInt64 account_id;
	SceUInt32 reserved2[2];
	SceRtcTick start_time;
	SceRtcTick expiration_time;
	SceUInt8 activation_checksum[0x20];
	char content_id[0x30];
	SceUInt8 reserved3[0x80];
	ScePsmDrmKeySet keyset;
	SceUInt8 rsa_signature[0x100];
} ScePsmDrmLicense;

_Static_assert(sizeof(ScePsmDrmLicense) == 0x400, "ScePsmDrmLicense");

struct NpDrmActivationCache;
struct NpDrmCheckActDataArgs;
struct NpDrmGetRifInfoArgs;

int SceNpDrmForDriver_077926F5(void);	/* vitasdk: ksceNpDrmUpdateActData */
int SceNpDrmForDriver_3BFD2850(int value);	/* vitasdk: ksceNpDrmPackageSetGameExist */
int sceNpDrmGetFixedRifName(char *name, SceUInt64 accountId);
/* vitasdk: ksceNpDrmWriteActData */
int SceNpDrmForDriver_742EBAF4(const struct NpDrmActivationCache *activation, const void *key);
/* vitasdk: ksceNpDrmEbootSigVerify */
int SceNpDrmForDriver_7A319692(const char *ebootPath, const void *signature);
/* vitasdk: ksceNpDrmCheckActData */
int SceNpDrmForDriver_9265B350(int *actType, int *versionFlag, SceUInt64 *accountId,
			       SceUInt64 *startTime, SceUInt64 *expirationTime);
int SceNpDrmForDriver_8B85A509(SceUInt64 *accountId);	/* vitasdk: ksceNpDrmRemoveActData */
/* vitasdk: ksceNpDrmEbootSigConvert */
int SceNpDrmForDriver_A29B75F9(const char *ebootPath, const void *signature, void *newSignature);
int SceNpDrmForDriver_A91C7443(void);	/* vitasdk: ksceNpDrmUpdateDebugSettings */
int sceNpDrmGetRifName(char *name, SceUInt64 accountId);
/* vitasdk: ksceNpDrmPspEbootSigGen */
int SceNpDrmForDriver_EF387FC4(const char *ebootPath, const void *sha256, void *signature);
int _sceNpDrmGetFixedRifName(char *rif_name, uint64_t aid);
int _sceNpDrmGetRifName(char *rif_name, uint64_t aid);
int _sceNpDrmRemoveActData(uint64_t *accountId);

int _sceNpDrmCheckActData(int *actType, int *versionFlag, uint64_t *accountId, const struct NpDrmCheckActDataArgs *userArgs);

/* vitasdk: ksceNpDrmGetRifNameForInstall */
int SceNpDrmForDriver_17573133(char *name, const SceNpDrmLicense *license, unsigned int isFixed);
/* vitasdk: ksceNpDrmGetRifVitaKey */
int SceNpDrmForDriver_723322B5(const SceNpDrmLicense *license, void *klicense, int *flags, unsigned int *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime);
/* vitasdk: ksceNpDrmPresetRifProvisionalFlag */
int SceNpDrmForDriver_C070FE89(SceNpDrmLicense *license, unsigned int provisional);
/* vitasdk: ksceNpDrmReadActData */
int SceNpDrmForDriver_D91C3BCE(SceNpDrmActivationData *activation);
/* vitasdk: ksceNpDrmGetRifPspKey */
int SceNpDrmForDriver_DACB71F4(const SceNpDrmLicense *license, void *klicense, int *flags, SceUInt64 *startTime, SceUInt64 *expirationTime);
int sceNpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int licenseSize, int checkSign, char *contentId, SceUInt64 *accountId, int *licenseVersion, int *drmType, int *flags, unsigned int *skuFlags, SceUInt64 *startTime, SceUInt64 *expirationTime, SceUInt64 *flags2);
/* vitasdk: ksceNpDrmVerifyRifFull */
int SceNpDrmForDriver_FF63672D(SceNpDrmLicense *license);
int _sceNpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int license_size, int check_sign, const struct NpDrmGetRifInfoArgs *userArgs);
int _sceNpDrmGetRifNameForInstall(char *rif_name, const SceNpDrmLicense *license, unsigned int is_fixed);
int _sceNpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license);

/* vitasdk: ksceNpDrmEbootSigGenPsp */
int SceNpDrmForDriver_90B1A6D3(const char *ebootPath, const void *sha256, void *signature, int swVersion);
/* vitasdk: ksceNpDrmEbootSigGenPs1 */
int SceNpDrmForDriver_6D9223E1(const char *ebootPath, const void *sha256, void *signature, int swVersion);
/* vitasdk: ksceNpDrmEbootSigGenMultiDisc */
int SceNpDrmForDriver_39A7A666(const char *ebootPath, const void *discInfo, void *signature, int swVersion);
int SceNpDrmForDriver_116FC0D6(SceUInt64 accountId);	/* vitasdk: ksceNpDrmUpdateAccountId */

/* not written in C yet: the prototypes are vshbridge's, PspEbootVerify's vitasdk's */
/* vitasdk: ksceNpDrmGetLegacyDocKey */
int SceNpDrmForDriver_4E321BDE(void *a, void *b, void *c, void *d);
/* vitasdk: ksceNpDrmPspEbootVerify */
int SceNpDrmForDriver_B6CA3A2C(const char *ebootPath, const void *signature);
int SceNpDrmForDriver_FC84CA1A(void);	/* vitasdk: ksceNpDrmIsLooseAccountBind */
