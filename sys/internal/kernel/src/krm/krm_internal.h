#ifndef KRM_INTERNAL_H
#define KRM_INTERNAL_H

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <psp2kern/types.h>
#include <threadmgr/fast_mutex.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <sysmem/aes.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/utils.h>
#include <psp2kern/kernel/ssmgr.h>
#include <sysmem/procevent.h>
#include <psp2kern/kernel/rtc.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <threadmgr/threadmgr.h>

/* Name unknown: driver-owned CBC chaining state after the compact schedule. */
typedef struct KrmAesCbcContext {
	SceAesCompactContext aes;
	unsigned char chainingBlock[32];
} KrmAesCbcContext;

int KrmAesInitDecrypt(SceAesCompactContext *ctx, SceSize blockBits,
		      SceSize keyBits, const void *key) __asm__("SceKernelUtilsForDriver_72408E29");
int KrmAesInitEncrypt(SceAesCompactContext *ctx, SceSize blockBits,
		      SceSize keyBits, const void *key) __asm__("SceKernelUtilsForDriver_EDA97D6D");
int KrmAesDecryptBlock(SceAesCompactContext *ctx, const void *src,
		       void *dst) __asm__("SceKernelUtilsForDriver_E39CD272");
int KrmAesEncryptBlock(SceAesCompactContext *ctx, const void *src,
		       void *dst) __asm__("SceKernelUtilsForDriver_302947B6");

int KrmAesCbcDecrypt(const unsigned char *src, unsigned char *dst,
		     SceSize size, unsigned int keyBits, const void *key,
		     unsigned char *iv);
int KrmAesCbcEncrypt(const unsigned char *src, unsigned char *dst,
		     SceSize size, unsigned int keyBits, const void *key,
		     unsigned char *iv);
int KrmAesDecrypt(const void *src, void *dst, unsigned int keyBits,
		  const void *key);
int KrmAesEncrypt(const void *src, void *dst, unsigned int keyBits,
		  const void *key);
int KrmComputeMac(const void *src, SceSize size, void *mac,
		  SceSize keySize, const void *key);
int KrmVerifyMac(const void *src, SceSize size, const void *mac,
		 SceSize keySize, const void *key);

/* Names unknown: the driver copies each ECC component as a 256-bit value. */
typedef struct KrmEccPublicKey {
	uint32_t x[8];
	uint32_t y[8];
} KrmEccPublicKey;

typedef struct KrmEccSignature {
	uint32_t r[8];
	uint32_t s[8];
} KrmEccSignature;

typedef struct KrmEccPrivateKey {
	uint32_t scalar[8];
} KrmEccPrivateKey;

typedef struct KrmEccEncodedBlock {
	KrmEccPublicKey first;
	KrmEccPublicKey second;
} KrmEccEncodedBlock;

/* Names inferred: persistent key header and algorithm-dependent material. */
typedef struct KrmKeyInfo {
	uint16_t version;
	uint8_t keyWords;
	uint8_t algorithm;
	uint32_t permissions;
	uint64_t expirationTick;
	unsigned char id[16];
} KrmKeyInfo;

typedef struct KrmKey {
	KrmKeyInfo info;
	union {
		unsigned char symmetric[160];
		struct {
			KrmEccPrivateKey privateKey;
			KrmEccPublicKey publicKey;
		} ecc;
	} material;
} KrmKey;
_Static_assert(sizeof(KrmKey) == 192, "KrmKey size");

typedef struct KrmStateConfig {
	uint32_t version;
	uint32_t permissions;
	int32_t keyCount;
	SceSize maxDataSize;
	unsigned char id[16];
} KrmStateConfig;

typedef struct KrmSignedStateConfig {
	KrmStateConfig config;
	KrmEccSignature signature;
} KrmSignedStateConfig;

struct KrmState;
extern const KrmEccPublicKey g_krmConfigPublicKey;
int KrmInitializeState(struct KrmState *state, const KrmStateConfig *config);
int KrmDriverDeriveStateKeys(void);
int KrmDriverVerifySignature(const KrmEccSignature *signature,
			     const void *src, SceSize size,
			     const KrmEccPublicKey *publicKey);

typedef struct KrmState {
	unsigned char nonce[8];
	uint64_t tick;
	KrmStateConfig config;
	KrmKey keys[64];
} KrmState;

typedef struct KrmSignedState {
	KrmState state;
	KrmEccSignature signature;
} KrmSignedState;
_Static_assert(sizeof(KrmSignedState) == 12400, "KrmSignedState size");

typedef struct KrmCryptRequest {
	const void *src;
	SceSize srcSize;
	void *dst;
	SceSize dstSize;
	const void *keyId;
	void *iv;
} KrmCryptRequest;

typedef struct KrmSignatureRequest {
	const void *src;
	SceSize srcSize;
	void *signature;
	SceSize signatureSize;
	const void *keyId;
	int digest;
} KrmSignatureRequest;

int KrmSignData(KrmState *state, const void *keyId, const void *src,
		SceSize srcSize, void *signature, SceSize signatureSize, int digest);
int KrmVerifyData(KrmState *state, const void *keyId, const void *src,
		  SceSize srcSize, const void *signature, SceSize signatureSize, int digest);
extern KrmSignedState g_krmEncryptedState;
extern KrmEccPublicKey g_krmStatePublicKey;
extern KrmEccPrivateKey g_krmStatePrivateKey;
extern const SceKernelDebugInfo g_krmRandomDebug;
extern const SceKernelDebugInfo g_krmTickDebug;
int KrmDriverGetRandomBytes(unsigned char *dst, unsigned int size);
int KrmDriverGetNetworkTick(uint64_t *tick);
int KrmDriverLockProcess(void);
int KrmDriverSignMessage(KrmEccSignature *signature, const void *src,
			 SceSize size, const KrmEccPrivateKey *privateKey);
typedef struct KrmKeyCounts {
	int total;
	int active;
} KrmKeyCounts;
int KrmGetKeyInfo(KrmState *state, const void *keyId, KrmKey *key);
int KrmGetKeyInfoByIndex(KrmState *state, int index, KrmKey *key);
int KrmGetKeyCounts(KrmState *state, KrmKeyCounts *counts);

int KrmImportKey(KrmState *state, const void *wrappingKeyId, const KrmKey *key);
int KrmExportKey(KrmState *state, const void *wrappingKeyId, const void *keyId, KrmKey *key);
int sceKrmInternalGenerateKey(KrmState *state, const KrmKey *key);
int KrmDeleteKey(KrmState *state, const void *keyId);
int KrmSetDefaultKey(KrmState *state, const void *keyId);
int KrmGetDefaultKey(KrmState *state, void *keyId);
extern KrmSignedState g_krmSignedState;
extern unsigned char g_krmCryptBuffer[4096];
extern int g_krmKeysInitialized;
extern int g_krmDefaultKeyIndex;
extern int g_krmWrappingKeyInitialized;
extern KrmKey g_krmWrappingKey;
int KrmDriverEncrypt(const KrmKey *key, unsigned char *iv, const void *src, SceSize size, void *dst, SceSize dstSize);
int KrmDriverDecrypt(const KrmKey *key, unsigned char *iv, const void *src, SceSize size, void *dst, SceSize dstSize);
unsigned int KrmDriverGetVersion(void);
int KrmDriverCheckExpiration(const KrmKeyInfo *info);
int KrmDriverSign(const KrmKey *key, const void *src, SceSize size, void *signature, SceSize signatureSize);
int KrmDriverVerify(const KrmKey *key, const void *src, SceSize size, const void *signature, SceSize signatureSize);
int KrmDriverSignHash(const KrmKey *key, const void *src, SceSize size, void *signature, SceSize signatureSize);
int KrmDriverVerifyHash(const KrmKey *key, const void *src, SceSize size, const void *signature, SceSize signatureSize);
int KrmDriverGenerateKeyPair(KrmEccPublicKey *publicKey, KrmEccPrivateKey *privateKey);
int KrmDriverValidatePublicKey(const KrmEccPublicKey *publicKey);
extern const SceKernelDebugInfo g_krmGenerateEccDebug;
extern const SceKernelDebugInfo g_krmGenerateRandomDebug;
int KrmCryptData(KrmState *state, const void *keyId, const void *src,
		 SceSize srcSize, void *dst, SceSize dstSize, void *iv, int encrypt);
int KrmDriverLock(void);
int KrmDriverUnlock(void);
unsigned int KrmDriverGetState(void);
SceUID KrmDriverGetPID(void);
void sceKrmDriverSetPID(SceUID pid);
unsigned int KrmDriverGetAuthorization(SceUID pid);

typedef struct KrmSignedNonce {
	unsigned char nonce[16];
	KrmEccSignature signature;
} KrmSignedNonce;
int KrmLockChallenge(void);
int KrmUnlockChallenge(void);
int KrmCreateSignedNonce(KrmSignedNonce *nonce);

typedef struct KrmChallenge {
	unsigned char nonce[16];
	KrmEccPublicKey publicKey;
	KrmEccSignature certificate;
	KrmEccSignature signature;
} KrmChallenge;

typedef struct KrmChallengeResponse {
	KrmEccEncodedBlock encoded;
	KrmEccSignature signature;
} KrmChallengeResponse;

int KrmDriverRespondToChallenge(const KrmChallenge *challenge, KrmChallengeResponse *response);

typedef union KrmStateKey {
	uint32_t words[8];
	unsigned char bytes[32];
	struct {
		char label[16];
		SceOpenPsId id;
	} seed;
} KrmStateKey;

extern uint32_t g_krmStateIv[4];
extern KrmStateKey g_krmStateKey;
int KrmDriverGetOpenPsId(SceOpenPsId *id);
int KrmDriverSha256(void *digest, const void *src, SceSize size);
int KrmDriverIsAccessible(const void *addr, SceSize size, unsigned int flags);
int KrmDriverCopyFromUser(void *dst, const void *src, SceSize size);
int KrmDriverCopyToUser(void *dst, const void *src, SceSize size);
int KrmVerifyChallenge(const KrmChallenge *challenge);
int KrmCreateChallengeResponse(const KrmEccPublicKey *publicKey,
			       KrmChallengeResponse *response);
int KrmDriverEncodeEcc(const KrmEccPrivateKey *value,
			 KrmEccEncodedBlock *encoded, const KrmEccPublicKey *publicKey);

typedef struct KrmSha256Digest {
	uint32_t words[8];
} KrmSha256Digest;

extern KrmEccPrivateKey g_krmProvisionedPrimaryKey __attribute__((aligned(8)));
extern SceOpenPsId g_krmProvisionedPrimaryKeyIv __attribute__((aligned(8)));
extern SceOpenPsId g_krmProvisionedPrimaryDigestIv __attribute__((aligned(8)));
extern SceOpenPsId g_krmProvisionedAlternateKeyIv __attribute__((aligned(8)));
extern SceOpenPsId g_krmProvisionedAlternateDigestIv __attribute__((aligned(8)));
extern KrmSha256Digest g_krmProvisionedPrimaryDigest __attribute__((aligned(8)));
extern KrmSha256Digest g_krmProvisionedAlternateDigest __attribute__((aligned(8)));
extern KrmEccPrivateKey g_krmProvisionedAlternateKey __attribute__((aligned(8)));
extern const KrmEccPublicKey g_krmPrimaryStatePublicKey;
extern const KrmEccPublicKey g_krmAlternateStatePublicKey;
int KrmInitializeWrappingKey(void);
void KrmClearState(void);
int KrmDriverDecryptPortable(const void *iv, const void *src, void *dst);

typedef struct KrmEccAllocation {
	unsigned int used;
	unsigned int capacity;
} KrmEccAllocation;

/* Inferred owner layout: initialization exposes 0x8800 bytes of the workspace
 * to the stack allocator; the random adapter reads the final argument slot. */
typedef struct KrmEccContext {
	KrmEccAllocation allocation;
	unsigned char *base;
	int resetPending;
	unsigned char workspace[0x93dc];
	void *randomArgument;
} KrmEccContext;
extern KrmEccContext g_krmEccContext;

int KrmEccVerifyMessage(const void *src, SceSize size,
		       const KrmEccPublicKey *publicKey,
		       const KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccSignMessage(const void *src, SceSize size,
		     const KrmEccPrivateKey *privateKey,
		     KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccGenerateKeys(KrmEccPublicKey *publicKey,
		       KrmEccPrivateKey *privateKey, KrmEccContext *ctx);
int KrmEccEncode(const KrmEccPublicKey *publicKey,
		 const KrmEccPrivateKey *value, KrmEccEncodedBlock *encoded,
		 KrmEccContext *ctx);
int KrmEccDecode(const KrmEccPrivateKey *privateKey,
		 const KrmEccEncodedBlock *encoded, KrmEccPrivateKey *value,
		 KrmEccContext *ctx);
int KrmEccSignDigest(const void *digest, const KrmEccPrivateKey *privateKey,
		    KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccVerifyDigest(const void *digest, const KrmEccPublicKey *publicKey,
		      const KrmEccSignature *signature, KrmEccContext *ctx);
int KrmEccInitialize(KrmEccContext *ctx);
int KrmEccImportPoint(const KrmEccPublicKey *publicKey, KrmEccPublicKey *point);
int KrmEccValidatePoint(const KrmEccPublicKey *point, KrmEccContext *ctx);
int ksceSblRngGenuineRandomNumber(void *block);

extern struct ThreadmgrFastMutex g_krmDriverMutex;
extern SceUID g_krmDriverPid;
extern unsigned int g_krmDriverState;
extern const SceKernelDebugInfo g_krmSetPidDebug;
extern const char g_krmDriverMutexName[];

int KrmDriverLock(void);
int KrmDriverUnlock(void);
void sceKrmDriverSetPID(SceUID pid);
SceUID KrmDriverGetPID(void);
unsigned int KrmDriverGetAuthorization(SceUID pid);
void KrmDriverSetState(void);
unsigned int KrmDriverGetState(void);
void KrmDriverClearState(void);
int KrmDriverInitialize(void);

_Static_assert(sizeof(SceUID) == 4, "kernel identifier ABI");
_Static_assert(sizeof(uintptr_t) == 4, "guest address ABI");

extern const char g_krmSeedForIv[16] __attribute__((aligned(4)));
extern const char g_krmSeedForKey[20] __attribute__((aligned(4)));
extern const uint16_t g_krmPowerWindowLimits[6];
extern const uint32_t g_krmEccInverseCoefficientInfo;
extern const KrmEccPublicKey g_krmAlternateChallengePublicKey;
extern const KrmEccPublicKey g_krmPrimaryChallengePublicKey;
extern const char g_krmChallengeMutexName[16];
extern const char g_krmProcEventHandlerName[28];
#if FW_DEBUG
extern const char g_krmRandomError[48];
extern const char g_krmTickError[48];
extern const char g_krmGenerateEccError[56];
extern const char g_krmGenerateRandomError[48];
extern const char g_krmEncryptAndSignStateName[24];
extern const char g_krmGenerateKeyName[32];
#endif

extern KrmSha256Digest g_krmPrimaryWrappingKey;
extern KrmSha256Digest g_krmAlternateWrappingKey;
extern KrmSha256Digest g_krmPrimaryWrappingDigest;
extern KrmSha256Digest g_krmAlternateWrappingDigest;
extern unsigned int g_krmPrimaryWrappingKeyIv[4];
extern unsigned int g_krmAlternateWrappingKeyIv[4];
extern unsigned int g_krmPrimaryWrappingDigestIv[4];
extern unsigned int g_krmAlternateWrappingDigestIv[4];
extern KrmSha256Digest g_krmProvisionedChallengeAlternateDigest __attribute__((aligned(8)));
extern KrmEccPrivateKey g_krmProvisionedChallengePrimaryKey __attribute__((aligned(8)));
extern KrmEccPrivateKey g_krmProvisionedChallengeAlternateKey __attribute__((aligned(8)));
extern KrmSha256Digest g_krmProvisionedChallengePrimaryDigest __attribute__((aligned(8)));
extern unsigned int g_krmProvisionedChallengePrimaryDigestIv[4] __attribute__((aligned(8)));
extern unsigned int g_krmProvisionedChallengeAlternateDigestIv[4] __attribute__((aligned(8)));
extern unsigned int g_krmProvisionedChallengePrimaryKeyIv[4] __attribute__((aligned(8)));
extern unsigned int g_krmProvisionedChallengeAlternateKeyIv[4] __attribute__((aligned(8)));

/* Runtime work areas are independent C objects, as in their original owners. */
extern unsigned int g_krmChallengeNonce[4];
extern KrmEccPrivateKey g_krmChallengePrivateKey;
extern KrmEccPublicKey g_krmChallengePublicKey;
extern int g_krmChallengeInitialized;
extern struct ThreadmgrFastMutex g_krmChallengeMutex;
extern SceUID g_krmProcEventHandler;

#endif

#endif
