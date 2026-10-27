/*
 * pm_sm.h: Internal types, structures, and function declarations for
 * PlayStation Vita Product Mode Secure Module (pm_sm.self).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#ifndef PM_SM_H
#define PM_SM_H

#include "sm_common.h"

/* ============================================================================
 * PM Command IDs & Error Codes
 * ============================================================================ */

#define SCE_SBL_PM_CMD_GET_FLAG			1u
#define SCE_SBL_PM_CMD_SET_FLAG			2u
#define SCE_SBL_PM_CMD_AUTH1_INIT		3u
#define SCE_SBL_PM_CMD_AUTH1_CHALLENGE		4u
#define SCE_SBL_PM_CMD_AUTH1_VERIFY		5u
#define SCE_SBL_PM_CMD_AUTH2_CHALLENGE		6u
#define SCE_SBL_PM_CMD_AUTH2_VERIFY		7u
#define SCE_SBL_PM_CMD_SNVS_SERVICE		8u
#define SCE_SBL_PM_CMD_RSA_CHALLENGE		9u
#define SCE_SBL_PM_CMD_RSA_VERIFY		10u

#define SCE_SBL_ERROR_PM_INVALID_ARG		((int)0x800f0a16u)
#define SCE_SBL_ERROR_PM_NOT_AUTHORIZED		((int)0x800f0a26u)
#define SCE_SBL_ERROR_PM_VERIFY_FAILED		((int)0x800f0a27u)
#define SCE_SBL_ERROR_PM_CRYPTO_FAILED		((int)0x800f0a28u)

/* ============================================================================
 * PM Structures (names ours)
 * ============================================================================ */

typedef struct PmAuthContext {
	uint32_t state;				/* 0x00 */
	uint8_t clientNonce[8];			/* 0x04..0x0b */
	uint8_t serverNonce[8];			/* 0x0c..0x13 */
	uint8_t sessionKey[16];			/* 0x14..0x23 */
} PmAuthContext;

_Static_assert(sizeof(PmAuthContext) == 0x24, "PmAuthContext size");

typedef struct PmSnvsCipher {
	const void *vptr;			/* 0x00 */
	const uint8_t *dataKey;			/* 0x04 */
	const uint8_t *tweakKey;		/* 0x08 */
	const uint8_t *cmacKey;			/* 0x0c */
	uint32_t dataKeyBits;			/* 0x10 */
	uint32_t cmacKeyBits;			/* 0x14 */
} PmSnvsCipher;

_Static_assert(sizeof(PmSnvsCipher) == 0x18, "PmSnvsCipher size");

/* ============================================================================
 * Function Declarations (names ours except sm_main)
 * ============================================================================ */

int sm_main(void);
void pmSimpleHookInvoke(void *self, void *commBuf);
void pmCtxHookInvoke(void *self, void *commBuf);
int pmRegisterSimpleHook(SmServiceContext *srvCtx, uint32_t cmdId,
		void *targetObj, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode);
int pmRegisterCtxHook(SmServiceContext *srvCtx, uint32_t cmdId,
		PmAuthContext *authCtx, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode);
int pmCmd1GetProductMode(void *targetObj, void *commBuf);
int pmCmd2SetProductMode(void *targetObj, void *commBuf);
int pmCmd8SnvsService(void *targetObj, void *commBuf);
void pmAuthCtxInitC2(PmAuthContext *ctx);
void pmAuthCtxInitC1(PmAuthContext *ctx);
void pmAuthCtxDtorD2(PmAuthContext *ctx);
void pmAuthCtxDtorD1(PmAuthContext *ctx);
int pmCmd3Auth1Init(void *ctxPtr, void *commBuf);
int pmCmd4Auth1Challenge(void *ctxPtr, void *commBuf);
int pmCmd5Auth1Verify(void *ctxPtr, void *commBuf);
int pmCmd6Auth2Challenge(void *ctxPtr, void *commBuf);
int pmCmd7Auth2Verify(void *ctxPtr, void *commBuf);
int pmCmd9RsaChallenge(void *ctxPtr, void *commBuf);
int pmCmd10RsaVerify(void *ctxPtr, void *commBuf);
int pmCheckKeyId(uint32_t keyId);
void pmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
void pmStaticInitOrDtor(int isInit);
int pmEnsureSnvsCryptoInitialized(void);
int pmReadSnvsSector0(void *outWord0, void *outWord1, const void *inSector32);
int pmValidateSectorRange(uint32_t sectorCount, uint32_t startSector);
int pmDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
uint32_t pmIsSnvsInitialized(void);
PmSnvsCipher *pmGetSnvsCipher(void);
const uint8_t *pmGetZeroIv16(void);
int pmBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
		uint8_t *outChallenge16);
int pmVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
		uint32_t *outSectorIdx, uint8_t *outSectorData32,
		uint32_t sectorSize, uint8_t *inOutPacket48);
uint32_t pmIncrementSnvsCounter(uint32_t counter);
void pmStaticInitThunk(void);
void pmStaticDtorThunk(void);
void pmSnvsCipherDtorD2(PmSnvsCipher *c);
void pmSnvsCipherDtorD0(PmSnvsCipher *c);
void pmSnvsCipherCtorC2(PmSnvsCipher *c);
void pmSnvsCipherCtorC1(PmSnvsCipher *c);
void pmSnvsCipherDtorD1(PmSnvsCipher *c);
int pmSnvsCipherInit(PmSnvsCipher *c, const uint8_t *dataKey,
		const uint8_t *tweakKey, const uint8_t *cmacKey,
		uint32_t dataKeyBits, uint32_t cmacKeyBits);
int pmSnvsSignRecordCmac(const PmSnvsCipher *c, uint8_t *inOutRec32);
int pmFormatSectorTweak(const PmSnvsCipher *c, uint32_t sectorIdx,
		uint64_t *outTweak16);
int pmSnvsDecryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
void pmSnvsCipherNoop1(void);
int pmXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int pmIncrementTweak128(uint8_t *tweak, uint32_t len);
int pmXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);
void pmReadHardwareKey2220(uint32_t *outWords8);
int pmEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
int pmWriteSnvsSector0(const void *inWord0, const void *inWord1,
		void *outSector32);
int pmWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
		const void *sectorData32, uint32_t sectorSize,
		uint8_t *outPacket48);
int pmVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
		uint8_t *inOutPacket16);
int pmSnvsEncryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
int pmXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int pmXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);
int pmConstTimeCompare(const uint8_t *a, const uint8_t *b, uint32_t len);
int pmResolveKeySlot(uint32_t keyId, uint32_t variant, uint32_t *outSlot);
int pmBuildAuth1InitHeader(uint8_t *out40, uint32_t keyId);
int pmBuildAuth1Challenge(uint8_t *out40, const uint8_t *in40,
		uint32_t keyId, uint32_t variant,
		const uint8_t *serverNonce8);
int pmVerifyAuth1Response(const uint8_t *in40, uint32_t keyId,
		uint32_t variant, const uint8_t *expectedClientNonce8,
		const uint8_t *expectedServerNonce8, uint8_t *outSessionKey16);
int pmClearKeySlot(uint32_t keyId, uint32_t variant);
int pmAes128CbcEncrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv);
int pmAes128CbcDecrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv);
int pmGetRandomBytes(uint8_t *dst, uint32_t len);
int pmValidatePkcs1Padding(const uint8_t *buf260);
int pmSha256HeaderAndNonce(const uint8_t *hdr4, const uint8_t *nonce32,
		uint8_t *outDigest32);
int pmBuildRsaChallenge(uint8_t subcmd, uint8_t *out260,
		uint8_t *outServerNonce16, const BignumPublicKey *rsaKey);
int pmVerifyRsaResponseNoOut(uint8_t subcmd, const uint8_t *in260,
		const uint8_t *expectedNonce16, const BignumPublicKey *rsaKey);
int pmVerifyRsaResponse(uint8_t subcmd, const uint8_t *in260,
		const uint8_t *expectedNonce16, uint8_t *outPayload16,
		const BignumPublicKey *rsaKey);
int pmBuildSubcmd6Challenge(uint8_t *out40, uint32_t variant,
		const uint8_t *sessionKey16);
int pmVerifySubcmd6Response(const uint8_t *in40, uint32_t variant,
		const uint8_t *sessionKey16, uint8_t *outSessionKey16);
int pmBuildSubcmd4Challenge(uint8_t *out40, const uint8_t *subSessionKey16,
		const uint8_t *sessionKey16);
int pmVerifySubcmd4Response(const uint8_t *in40, const uint8_t *sessionKey16,
		uint8_t *outPayload16);
int pmGetRandomNonZeroBytes(uint8_t *dst, uint32_t len);

#endif /* PM_SM_H */
