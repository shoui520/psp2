/*
 * resume.c: Warm-boot resume context decryption, CMAC verification, and
 * keyring restoration from DRAM at 0x4001ff00 (0x8026a8..0x8027df).
 *
 * Function and variable names here are ours (descriptive), except the magic
 * constant 0xacb4acb1 from SKBL / resume context.
 */

#include "second_loader.h"

/* ============================================================================
 * .data (0x80c87c..0x80c8bf)
 * ============================================================================ */

static uint8_t s_resumeContextIv[64] = {
	0xa1u, 0x32u, 0x5au, 0xd8u, 0xb9u, 0x21u, 0x2fu, 0xefu,
	0x72u, 0x16u, 0xefu, 0xfbu, 0x30u, 0xcbu, 0x4du, 0xfcu,
};

/* ============================================================================
 * Functions (0x8026a8..0x8027df)
 * ============================================================================ */

/* 0x8026a8 */
int verifyAndDecryptResumeContext(void *unusedCtx, const void *srcPa,
		const void *cipherKey, uint32_t cipherKeyLen,
		const void *cmacKey, uint32_t cmacKeyLen)
{
	DecryptedResumePayload decrypted;
	uint8_t computedCmac[16];
	BigmacContext bigmacCtx;
	int status;

	(void)unusedCtx;
	(void)cipherKeyLen;
	status = bigmacAes256CbcDecryptSync(&decrypted, srcPa, sizeof(decrypted),
			cipherKey, s_resumeContextIv);
	if (status != 0)
		return status;

	bigmacInitContext(&bigmacCtx);
	status = bigmacAesCmacWithKey(computedCmac, &decrypted, 0x90u,
			cmacKey, cmacKeyLen, &bigmacCtx);
	if (status != 0)
		return status;
	status = bigmacWaitChannel(&bigmacCtx);
	if (status != 0)
		return status;

	if (slTimingsafeBcmp(decrypted.cmac, computedCmac, sizeof(computedCmac)) != 0)
		return (int)0x800f0027;
	if (decrypted.magic != RESUME_PAYLOAD_MAGIC)
		return (int)0x800f0024;
	if (decrypted.version != 1u)
		return (int)0x800f0037;

	status = writeKeyringSlotBytes(0x511u, decrypted.slot511, 32u);
	if (status != 0)
		return status;
	status = writeKeyringSlotBytes(0x512u, decrypted.slot512, 32u);
	if (status != 0)
		return status;
	status = writeKeyringSlotBytes(0x517u, decrypted.slot517, 32u);
	if (status != 0)
		return status;
	return writeKeyringSlotBytes(0x519u, decrypted.slot519, 32u);
}

/* 0x802788 */
int loadAndSetupResumeOrColdBootKeys(void *ctx, int tryResume)
{
	uint32_t cmacKey[8];
	uint32_t cipherKey[8];
	int resumeValid;

	if (tryResume == 0) {
		resumeValid = 0;
	} else {
		readKeyringSlotWords(0x515u, cipherKey);
		readKeyringSlotWords(0x514u, cmacKey);
		resumeValid = (verifyAndDecryptResumeContext(ctx,
				(const void *)(uintptr_t)RESUME_CONTEXT_PA,
				cipherKey, 32u, cmacKey, 32u) == 0);
	}
	return initSnvsHandshakeOrFallbackKeys((uint32_t)resumeValid);
}

/* 0x8027dc */
int noopResumeHook(void)
{
	return 0;
}
