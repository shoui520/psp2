/*
 * utoken_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * User Token secure module (os0:sm/utoken_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef UTOKEN_SM_H
#define UTOKEN_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_UTOKEN_INVALID_ARG	((int)0x800f1a16u)
#define SCE_SBL_ERROR_UTOKEN_INVALID_SIZE	((int)0x800f1a24u)
#define SCE_SBL_ERROR_UTOKEN_BAD_SIGNATURE	((int)0x800f1a27u)
#define SCE_SBL_ERROR_UTOKEN_FAILED		((int)0x800f1a28u)
#define SCE_SBL_ERROR_UTOKEN_CID_MISMATCH	((int)0x800f1a39u)
#define SCE_SBL_ERROR_UTOKEN_EXPIRED		((int)0x800f1a3au)

#define UTOKEN_CMD_VERIFY_ONLY			1u
#define UTOKEN_CMD_VERIFY_AND_APPLY		2u

#define UTOKEN_BYTE_SIZE			0x800u
#define UTOKEN_HEADER_CLEAR_SIZE		0x10u
#define UTOKEN_SIGNATURE_OFFSET			0x700u
#define UTOKEN_SIGNATURE_SIZE			0x100u

typedef struct SceUtokenRsaPublicKey {
	uint8_t modulus[256];			/* 0x000..0x0ff */
	uint32_t modulusSize;			/* 0x100 */
	uint8_t exponent[4];			/* 0x104..0x107 */
	uint32_t exponentSize;			/* 0x108 */
} SceUtokenRsaPublicKey;

/* Mailbox communication packet at 0x80cc00 (size up to 0x50) */
typedef struct SceUtokenCommPacket {
	uint32_t size;				/* 0x00 */
	uint32_t cmdId;				/* 0x04: 1 or 2 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
	uint32_t tokenPhysAddr;			/* 0x40 */
	uint32_t tokenSize;			/* 0x44: 0x800 */
	uint32_t reqValue;			/* 0x48 */
	uint32_t reserved4c;			/* 0x4c */
} SceUtokenCommPacket;

_Static_assert(sizeof(SceUtokenCommPacket) == 0x50, "SceUtokenCommPacket size");

/* utoken_sm.c */
int utokenSmHookInvoke(void *closure, void *packetBuf);
int utokenSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
int utokenDecryptToken(void *mgr, const uint8_t *src, uint8_t *dst,
		uint32_t size);
int utokenComputeSha256(void *mgr, const uint8_t *buf, uint32_t size,
		uint8_t *outDigest, uint32_t digestBufSize);
int utokenVerifyRsaSignature(void *mgr, const void *sig, uint32_t sigSize,
		const void *digest, uint8_t digestSize);
int utokenValidateFields(void *mgr, const uint8_t *tokenBuf, uint32_t tokenSize,
		uint32_t reqValue, const uint8_t *consoleId);
int utokenProcessCommand(uint8_t *tokenWorkBuf, SceUtokenCommPacket *pkt,
		uint32_t applyFlag);
int utokenCmd2VerifyAndApply(void *mgr, void *packetBuf);
int utokenCmd1VerifyOnly(void *mgr, void *packetBuf);

/* crypto.c */
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacAes128CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacSha256(void *dstDigest, const void *src, uint32_t size);
int bignumVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const BignumPublicKey *pubKey, const void *expectedDigest,
		uint32_t oidIndex);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);
int bigmacInitChannel0(uint32_t *ctrl);
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus);
int bigmacWaitChannelDone(const uint32_t *ctrl);
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

#endif /* UTOKEN_SM_H */
