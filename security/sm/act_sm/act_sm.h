/*
 * act_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * Activation secure module (os0:sm/act_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef ACT_SM_H
#define ACT_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID	((int)0x800f1311u)
#define SCE_SBL_ERROR_ACT_BAD_CMAC		((int)0x800f1327u)
#define SCE_SBL_ERROR_ACT_FAILED		((int)0x800f1328u)
#define SCE_SBL_ERROR_ACT_NOT_ACTIVATED		((int)0x800f1329u)
#define SCE_SBL_ERROR_ACT_PSID_MISMATCH		((int)0x800f1339u)

#define ACT_CMD_VERIFY_RECORD			4u
#define ACT_CMD_VERIFY_AND_RENEW		10u
#define ACT_CMD_VERIFY_AND_APPLY		11u

#define ACT_BLOB_SIZE				128u
#define ACT_SIGNATURE_SIZE			256u
#define ACT_RECORD_SIZE				32u
#define ACT_MAX_DURATION_SEC			0x02ad8c00u

typedef struct SceActRsaPublicKey {
	uint8_t modulus[256];			/* 0x000..0x0ff */
	uint32_t modulusSize;			/* 0x100 */
	uint8_t exponent[4];			/* 0x104..0x107 */
	uint32_t exponentSize;			/* 0x108 */
} SceActRsaPublicKey;

/* Mailbox communication packet at 0x80d100 (size up to 0x360) */
typedef struct SceActCommPacket {
	uint32_t size;				/* 0x00 */
	uint32_t cmdId;				/* 0x04 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
	uint8_t payload[0x320];			/* 0x40..0x35f */
} SceActCommPacket;

_Static_assert(sizeof(SceActCommPacket) == 0x360, "SceActCommPacket size");

/* act_sm.c */
int actSmHookInvoke(void *closure, void *packetBuf);
int actSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
void actSmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
void actInitLegacyKeys(void);
int actIsKeyringSlot50aNonZero(void *mgr);
int actGetKeyringSlot50bBit0(void *mgr);
void actUpdateKeyringSlot517Word(void *mgr, uint32_t value, uint32_t wordIndex);
int actVerifyRsa2048Sha256(void *mgr, const void *sig256, const void *data128);
int actDecryptPayloadHalf(void *mgr, const void *src128, uint8_t *dst128,
		const void *aesKey32, const void *aesIv16);
int actVerifyRecordCmac(void *mgr, const void *record32, const void *cmacKey32);
int actCmd4VerifyRecord(void *mgr, void *packetBuf);
int actSignRecordCmac(void *mgr, void *record32, const void *cmacKey32);
int actVerifyBlobCmac(void *mgr, const uint8_t *blob128, const void *cmacKey32);
int actDecryptAndValidateBlob(void *mgr, const void *blob128,
		uint32_t *outFlagByte);
int actCmd10VerifyAndUpdateRenewal(void *mgr, void *packetBuf);
int actCmdUnusedUpdateRecord(void *mgr, void *packetBuf);
int actCmd11VerifyAndApplyActivation(void *mgr, void *packetBuf);
int actCmdUnusedVerifyAndApply(void *mgr, void *packetBuf);

/* crypto.c */
int bigmacInitChannel0(uint32_t *ctrl);
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus);
int bigmacWaitChannelDone(const uint32_t *ctrl);
int bigmacStartAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacSha256(void *dstDigest, const void *src, uint32_t size);
int bignumVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const BignumPublicKey *pubKey, const void *expectedDigest,
		uint32_t oidIndex);
int bigmacStartAesCmac(void *dstMac, const void *src, uint32_t size,
		const void *key, uint32_t keyByteLen, const uint32_t *ctrl);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, uint32_t dir, uint32_t chainMode,
		uint32_t keyBits, const uint32_t *ctrl);
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size,
		uint32_t hashSubMode, const uint32_t *bmCtrl);
int bignumCopyWordsReverseBswap(volatile uint32_t *dst,
		const uint32_t *src, uint32_t wordCount);
int bignumInitState(uint32_t *state);
int bignumModExpStep(void *dst, const void *base,
		const BignumPublicKey *key, uint32_t *state);
int bignumWaitInterrupt(void);
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig,
		const BignumPublicKey *key, const uint8_t *hash,
		uint32_t hashAlgo, uint32_t *state);
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl);

#endif /* ACT_SM_H */
