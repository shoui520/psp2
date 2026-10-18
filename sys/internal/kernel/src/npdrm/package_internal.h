/* Package implementation; the original source filename is unavailable. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmem/aes.h>
#include <npdrm/npdrm_package.h>
#include <marlin_hci/rsa.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <acmgr/acmgr.h>
#include <npdrm/npdrm.h>
#include <npdrm/psmdrm.h>
#include <post_ss_mgr/sblpostssmgr.h>

/* Extended package file header; multibyte members retain big-endian order. */
struct NpDrmPackageExtendedHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t headerSize;
	uint32_t dataSize;
	uint32_t mainHeaderHmacOffset;
	uint32_t metadataHmacOffset;
	uint64_t tailOffset;
	uint32_t reserved;
	uint32_t keyId;
	uint32_t fullHeaderHmacOffset;
	uint8_t padding[20];
};

_Static_assert(sizeof(struct NpDrmPackageExtendedHeader) == 64,
	"package extended header ABI");
_Static_assert(offsetof(struct NpDrmPackageExtendedHeader,
	fullHeaderHmacOffset) == 40, "package full header HMAC offset ABI");

struct NpDrmPackageHeader {
	struct NpDrmPackageMainHeader main;
	struct NpDrmPackageExtendedHeader extended;
};

_Static_assert(sizeof(struct NpDrmPackageHeader) == 256,
	"package header ABI");
_Static_assert(offsetof(struct NpDrmPackageHeader, extended) == 192,
	"package extended header position ABI");

struct NpDrmPackageContext;

struct NpDrmPackageOperations {
	int (*verifySignature)(const unsigned char *signature,
		const unsigned char *digest);
	int (*verifyHeader)(unsigned char *data, unsigned int size,
		const struct NpDrmPackageExtendedHeader *header);
	int (*checkSignature)(const unsigned char *signature,
		unsigned int size, const unsigned char *digest);
	int (*initializeContext)(struct NpDrmPackageContext *context,
		const struct NpDrmPackageHeader *header, unsigned int size);
	int (*cryptData)(struct NpDrmPackageContext *context, void *data,
		unsigned int size, uint64_t offset, unsigned int flags);
};

_Static_assert(sizeof(struct NpDrmPackageOperations) == 20,
	"package operations ABI");

struct NpDrmPackageContext {
	const struct NpDrmPackageOperations *operations;
	uint8_t key[16];
	uint64_t counter[2];
	SceAesCompactContext *aesContext;
	uint32_t aesContextSize;
};

_Static_assert(sizeof(struct NpDrmPackageContext) == 48,
	"package context ABI");
_Static_assert(offsetof(struct NpDrmPackageContext, counter) == 24,
	"package counter ABI");
_Static_assert(offsetof(struct NpDrmPackageContext, aesContext) == 40,
	"package AES context ABI");

extern struct NpDrmPackageContext gNpDrmPackageContexts[6];

struct NpDrmPackageRsaCrtMaterial {
	uint32_t primeP[32];
	uint32_t primeQ[32];
	uint32_t exponentP[32];
	uint32_t exponentQ[32];
	uint32_t coefficient[32];
};

_Static_assert(sizeof(struct NpDrmPackageRsaCrtMaterial) == 640,
	"package RSA CRT material ABI");

/* Initial values remain unresolved; these are declarations of original data. */
extern const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageRetailRsaKey;
extern const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageDebugRsaKey;
extern const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageSigningRsaKey;
extern const struct NpDrmPackageRsaCrtMaterial gNpDrmPackageEncryptedKeyRsaKey;

struct NpDrmPackageMetadataRecord {
	uint32_t type;
	uint32_t size;
	unsigned char payload[];
};

struct NpDrmPackageEncryptedKeyMetadata {
	uint32_t offset;
	uint32_t size;
	unsigned char extension[];
};

_Static_assert(sizeof(struct NpDrmPackageMetadataRecord) == 8,
	"package metadata record header ABI");
_Static_assert(sizeof(struct NpDrmPackageEncryptedKeyMetadata) == 8,
	"package encrypted key metadata header ABI");

int NpDrmPackageInitializePrivateContext(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size);
int NpDrmPackageIsHardwareAllowed(unsigned int retail);

struct NpDrmPackageDebugSeed {
	uint64_t firstDigest[2];
	uint64_t secondDigest[2];
	uint8_t hashSuffix[24];
	uint64_t blockCounter;
};

_Static_assert(sizeof(struct NpDrmPackageDebugSeed) == 64,
	"package debug SHA1 seed ABI");
_Static_assert(offsetof(struct NpDrmPackageDebugSeed, blockCounter) == 56,
	"package debug block counter ABI");

int NpDrmPackageAesInitialize(SceAesCompactContext *context,
	const void *key, unsigned int keySize);
int NpDrmPackageAesEncrypt(SceAesCompactContext *context,
	void *output, const void *input);
int NpDrmPackageAesCtr(const void *input, void *output, unsigned int size,
	const void *key, unsigned int keyBits, void *counter);

int NpDrmRsaVerify2048(const MarlinRsaData *signature,
	const MarlinRsaData *digest, const MarlinRsaExponentKey *key,
	int algorithm);
int NpDrmRsaDecrypt2048(MarlinRsaData *output,
	const MarlinRsaData *input, const MarlinRsaPrivateKey *key);
int NpDrmRsaSign2048(MarlinRsaData *signature, const MarlinRsaData *digest,
	const MarlinRsaPrivateKey *key, int algorithm);

struct NpDrmPackageUnwrappedKey {
	unsigned char keyMaterial[96];
	char contentId[48];
};

struct NpDrmCheckActDataArgs {
	uint64_t *startTime;
	uint64_t *expirationTime;
	uint64_t reserved;
};

_Static_assert(sizeof(struct NpDrmCheckActDataArgs) == 16,
	"activation output argument ABI");

int NpDrmCheckActData(int *actType, int *versionFlag, uint64_t *accountId,
	uint64_t *startTime, uint64_t *expirationTime);
int NpDrmRemoveActData(uint64_t *accountId);

_Static_assert(sizeof(struct NpDrmPackageUnwrappedKey) == 144,
	"package unwrapped key ABI");

/* sceSblGcAuthMgrPkgVryForDriver; the import database retains its NID name. */
int SceSblGcAuthMgrPkgForDriver_E459A9A8(const char *pHash,
	const char *pSig);

int NpDrmPackageHashTransform(const void *input, void *digest, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags);

struct NpDrmGetRifInfoArgs {
	char *content_id;
	uint64_t *account_id;
	int *license_version;
	int *license_flags;
	int *flags;
	int *sku_flags;
	int64_t *lic_start_time;
	int64_t *lic_exp_time;
	uint64_t *flags2;
	uint32_t reserved;
};

_Static_assert(sizeof(struct NpDrmGetRifInfoArgs) == 40,
	"license information output argument ABI");

int NpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int license_size,
	int check_sign, char *content_id, uint64_t *account_id, int *license_version,
	int *license_flags, int *flags, int *sku_flags, int64_t *lic_start_time,
	int64_t *lic_exp_time, uint64_t *flags2);

int NpDrmGetRifName(char *name, uint64_t aid);
int NpDrmGetFixedRifName(char *name, uint64_t aid);

int NpDrmGetRifNameForInstall(char *rif_name,
	const SceNpDrmLicense *license, unsigned int is_fixed);

typedef struct SceNpDrmCheckDrmResetOpt {
	uint64_t account_id;
	int *pReset;
	unsigned int in_size;
	uint64_t reserved;
} SceNpDrmCheckDrmResetOpt;

_Static_assert(sizeof(SceNpDrmCheckDrmResetOpt) == 24,
	"DRM reset option ABI");

int NpDrmCheckDrmReset(const void *in_addr, unsigned int size,
	uint64_t account_id, int *pReset);

int NpDrmCheckDrmResetForDriver(const void *in_addr, unsigned int size,
	uint64_t account_id, int *pReset);

int NpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license);

int PsmDrmRemoveActData(uint64_t *accountId);

int PsmDrmGetActInfo(int *actType, int *versionFlag, uint64_t *accountId,
	uint64_t *startTime, uint64_t *expirationTime);
int PsmDrmGetRifInfo(const ScePsmDrmLicense *license, char *contentId,
	uint64_t *accountId, uint64_t *startTime, uint64_t *expirationTime);
int PsmDrmGetRifName(char *name);
int PsmDrmGetRifNameForInstall(char *name, const ScePsmDrmLicense *license);
int PsmDrmGetRifKey(const ScePsmDrmLicense *license, ScePsmDrmKeySet *keydata,
	int *flags, SceRtcTick *startTime, SceRtcTick *expirationTime);
int PsmDrmGetRifNameForDriver(char *name);
int PsmDrmGetRifNameForInstallForDriver(char *name,
	const ScePsmDrmLicense *license);
int NpDrmPackageCheckForUser(const void *buffer, unsigned int size,
	const unsigned char *digest, unsigned int identifier);

typedef struct _sceNpDrmPackageDecrypt {
	int64_t offset;
	unsigned int identifier;
	unsigned int reserved;
} _sceNpDrmPackageDecrypt_opt;

_Static_assert(sizeof(_sceNpDrmPackageDecrypt_opt) == 16,
	"package decrypt option ABI");

int NpDrmPackageDecryptForUser(void *buffer, unsigned int size,
	int64_t offset, unsigned int identifier);

int NpDrmPackageTransformForUser(void *buffer, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags);

struct NpDrmPackageInstallStartedArgs {
	int len;
	unsigned int size;
	uint64_t reserved;
};

_Static_assert(sizeof(struct NpDrmPackageInstallStartedArgs) == 16,
	"package install-started argument ABI");

int NpDrmPackageInstallStarted(int kind, unsigned int total,
	const char *msg, int len);

int NpDrmPackageInstallFinished(unsigned int result, const char *msg, int len);

int NpDrmPackageUninstallStarted(int kind, unsigned int total,
	const char *msg, int len);

int NpDrmPackageUninstallFinished(unsigned int result, const char *msg, int len);

int NpDrmSaveDataInstallStarted(int kind, unsigned int total,
	const char *msg, int len);

int NpDrmSaveDataInstallFinished(unsigned int result, const char *msg, int len);

int NpDrmSaveDataFormatStarted(int kind, unsigned int total,
	const char *msg, int len);

int NpDrmSaveDataFormatFinished(unsigned int result, const char *msg, int len);

struct NpDrmPackageCheckArgs {
	unsigned char *data;
	unsigned int size;
	const unsigned char *digest;
	unsigned int flags;
};

_Static_assert(sizeof(struct NpDrmPackageCheckArgs) == 16,
	"package check callback ABI");

struct NpDrmPackageDecryptArgs {
	unsigned char *data;
	unsigned int size;
	uint64_t offset;
	unsigned int flags;
	unsigned int reserved;
};

_Static_assert(sizeof(struct NpDrmPackageDecryptArgs) == 24,
	"package decrypt callback ABI");
