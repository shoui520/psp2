/*
 * utoken_sm.c: PlayStation Vita User Token secure module (os0:sm/utoken_sm.self)
 * service entry point, AES-128-CBC token decryption, RSA-2048 PKCS#1 v1.5
 * signature verification, field validation, and hardware flag programming
 * (0x80b09c..0x80b65a, .rodata 0x80c978..0x80caf0).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "utoken_sm.h"

/* ============================================================================
 * .rodata (0x80c978..0x80caf0)
 * ============================================================================ */

/* 0x80c978: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80c980) */
static const uintptr_t s_utokenHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)utokenSmHookInvoke,
	0u
};

/* 0x80c988: C++ vtable for abstract hook base node */
static const uintptr_t s_utokenBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80c9a0..0x80c9b0: 16-byte all-zero wildcard Console ID */
static const uint8_t s_utokenZeroCid[16] __attribute__((aligned(16))) = {
	0u
};

/* 0x80c9b0..0x80cabc: RSA-2048 public key for User Token signature verification */
static const SceUtokenRsaPublicKey s_utokenRsaPublicKey __attribute__((aligned(16))) = {
	.modulus = {
#if FW_DEBUG
		0xeau, 0x91u, 0xd8u, 0x24u, 0x11u, 0x9fu, 0xaau, 0x50u,
		0xa2u, 0xb8u, 0x30u, 0x29u, 0xb4u, 0xf1u, 0xabu, 0x25u,
		0x0fu, 0x27u, 0x9du, 0xa0u, 0x03u, 0xffu, 0x73u, 0xbeu,
		0x20u, 0x56u, 0x23u, 0xdbu, 0x04u, 0x3eu, 0xc7u, 0x35u,
		0x71u, 0xedu, 0xf0u, 0x6fu, 0x60u, 0x1fu, 0xf6u, 0x9fu,
		0x86u, 0x0au, 0xf6u, 0x1fu, 0xbau, 0x51u, 0xc0u, 0x59u,
		0x5du, 0x0au, 0xbau, 0x9du, 0xf9u, 0x2cu, 0xacu, 0x4bu,
		0x27u, 0x6fu, 0x74u, 0xa9u, 0x4fu, 0x60u, 0x6au, 0x8fu,
		0x32u, 0x89u, 0x73u, 0x7bu, 0x60u, 0xeau, 0x8eu, 0x4eu,
		0x03u, 0x9bu, 0xa5u, 0xabu, 0xc9u, 0x5au, 0xf3u, 0xd4u,
		0xfdu, 0x2eu, 0xf6u, 0x3bu, 0xe2u, 0xf9u, 0xfbu, 0xadu,
		0x53u, 0x28u, 0xf7u, 0x0eu, 0x56u, 0xccu, 0xa0u, 0x49u,
		0xebu, 0x15u, 0x81u, 0xbcu, 0x5eu, 0x33u, 0x52u, 0x29u,
		0x07u, 0x61u, 0xcfu, 0x93u, 0x77u, 0x2eu, 0xd3u, 0x88u,
		0xacu, 0x14u, 0xa9u, 0xb1u, 0xb4u, 0xa0u, 0xe3u, 0x25u,
		0xd2u, 0x7bu, 0xc6u, 0xe9u, 0x31u, 0x8cu, 0xabu, 0x9fu,
		0xf0u, 0xebu, 0x12u, 0x98u, 0xf7u, 0xf8u, 0x44u, 0x9bu,
		0x63u, 0xc1u, 0x84u, 0xd5u, 0x1eu, 0x5fu, 0xc2u, 0xd5u,
		0x17u, 0x68u, 0xbeu, 0xabu, 0x4au, 0xfeu, 0xb5u, 0x9bu,
		0xc0u, 0x17u, 0x4cu, 0x63u, 0x5du, 0xa9u, 0x00u, 0xbcu,
		0x1fu, 0x14u, 0xf0u, 0x0fu, 0x4fu, 0xeeu, 0x5fu, 0xeeu,
		0xfau, 0x18u, 0xa3u, 0xa5u, 0x83u, 0x99u, 0x83u, 0x9eu,
		0x28u, 0xeau, 0x85u, 0xc3u, 0xb4u, 0x2au, 0xedu, 0xccu,
		0xb4u, 0x13u, 0x33u, 0xc6u, 0x63u, 0xe4u, 0x97u, 0x55u,
		0xceu, 0x2au, 0x15u, 0x0eu, 0x27u, 0xb0u, 0xaau, 0xcbu,
		0x9fu, 0xf7u, 0xdeu, 0x0eu, 0x5eu, 0xd8u, 0x67u, 0x2eu,
		0x74u, 0x24u, 0x6bu, 0x7eu, 0x19u, 0x03u, 0xd7u, 0x0bu,
		0xcbu, 0x99u, 0x91u, 0x14u, 0xe9u, 0xf6u, 0xc5u, 0xb4u,
		0x37u, 0x22u, 0x6fu, 0x6fu, 0xdau, 0xa1u, 0xebu, 0xfau,
		0x1du, 0xeau, 0x74u, 0x85u, 0x22u, 0x92u, 0x31u, 0xf9u,
		0xa1u, 0xbeu, 0x68u, 0x9du, 0x0bu, 0xb4u, 0x9fu, 0x5cu,
		0xaau, 0x85u, 0x4du, 0x8eu, 0x74u, 0xd2u, 0xaau, 0xadu,
#else
		0xb3u, 0x49u, 0xcbu, 0xe8u, 0xecu, 0xf2u, 0x41u, 0x4du,
		0x71u, 0x3du, 0x48u, 0x42u, 0x0du, 0xc3u, 0x0eu, 0xc1u,
		0x5bu, 0x04u, 0xd1u, 0xd3u, 0xfau, 0x27u, 0xfau, 0xfeu,
		0x61u, 0x26u, 0x1fu, 0x1cu, 0x1eu, 0x19u, 0xe6u, 0x78u,
		0x6bu, 0x97u, 0xe0u, 0x99u, 0x38u, 0x20u, 0x42u, 0x3bu,
		0x4bu, 0x8fu, 0x74u, 0xfdu, 0xcbu, 0x1au, 0xb1u, 0xecu,
		0xe8u, 0x91u, 0xd8u, 0xe5u, 0x84u, 0x08u, 0x5au, 0x09u,
		0x9du, 0xadu, 0x95u, 0x73u, 0xd0u, 0x54u, 0x4du, 0xc5u,
		0x8cu, 0x49u, 0x39u, 0x8du, 0xecu, 0xf1u, 0xa2u, 0x62u,
		0xcbu, 0x96u, 0xa9u, 0x6au, 0xb9u, 0x4fu, 0xceu, 0x8cu,
		0xedu, 0x9au, 0x2du, 0xefu, 0x07u, 0x01u, 0xbau, 0x4fu,
		0x98u, 0x4cu, 0x10u, 0x2du, 0x26u, 0xc0u, 0x32u, 0x70u,
		0x0bu, 0x20u, 0x16u, 0x51u, 0xcau, 0x67u, 0xb9u, 0xf6u,
		0x7cu, 0x8fu, 0x58u, 0x23u, 0x89u, 0xdau, 0xa1u, 0x72u,
		0x01u, 0x0cu, 0xfau, 0x4au, 0x9cu, 0x87u, 0xfeu, 0x45u,
		0xdbu, 0xfbu, 0x2au, 0xfau, 0x44u, 0x11u, 0x49u, 0x8au,
		0xe1u, 0xf6u, 0x01u, 0xf9u, 0x08u, 0x09u, 0xd1u, 0x92u,
		0x13u, 0x48u, 0x1du, 0x9au, 0x8cu, 0xe1u, 0xa0u, 0xd5u,
		0xf3u, 0x95u, 0x49u, 0xf0u, 0x61u, 0x8au, 0x2au, 0x8eu,
		0x80u, 0x4cu, 0x29u, 0x74u, 0x32u, 0x92u, 0x28u, 0x2du,
		0x08u, 0x64u, 0xcbu, 0x64u, 0x76u, 0xcbu, 0xd2u, 0x9cu,
		0x40u, 0x20u, 0x08u, 0xb5u, 0x09u, 0xc6u, 0xe8u, 0xbbu,
		0xc6u, 0x94u, 0x3au, 0x13u, 0x5du, 0xeeu, 0xe3u, 0x0du,
		0x23u, 0x0du, 0x2du, 0x51u, 0x1bu, 0x27u, 0xc8u, 0x94u,
		0xa1u, 0x7au, 0x51u, 0xe7u, 0x22u, 0xbcu, 0xecu, 0xe1u,
		0xf4u, 0x6eu, 0x53u, 0x7bu, 0x82u, 0xb2u, 0xa3u, 0x1cu,
		0xb3u, 0xfeu, 0xf9u, 0x53u, 0x2au, 0x20u, 0xb9u, 0x2au,
		0x2au, 0xa0u, 0xcdu, 0x35u, 0xb6u, 0x03u, 0x23u, 0x56u,
		0x4fu, 0xc2u, 0x23u, 0xafu, 0xb6u, 0x9au, 0x89u, 0x97u,
		0x87u, 0xbcu, 0x7cu, 0xcdu, 0x30u, 0x99u, 0xbdu, 0x29u,
		0x99u, 0x9du, 0xcdu, 0x81u, 0x6bu, 0xb5u, 0x1bu, 0x24u,
		0xb1u, 0x70u, 0x20u, 0xd4u, 0x92u, 0x6du, 0x7eu, 0xadu,
#endif
	},
	.modulusSize = 0x100u,
	.exponent = { 0x00u, 0x01u, 0x00u, 0x01u },
	.exponentSize = 0x04u,
};

/* 0x80cac0..0x80cad0: 16-byte AES-128-CBC key for User Token decryption */
static const uint8_t s_utokenAesKey[16] __attribute__((aligned(16))) = {
#if FW_DEBUG
	0x5cu, 0xa9u, 0x7bu, 0x8fu, 0xa1u, 0xc0u, 0x31u, 0x69u,
	0x71u, 0x71u, 0x3du, 0xe7u, 0xb0u, 0xffu, 0xfeu, 0x0bu,
#else
	0xabu, 0x37u, 0x75u, 0x9fu, 0x20u, 0xfau, 0xcau, 0xb6u,
	0x1au, 0x4cu, 0x51u, 0xb3u, 0x83u, 0x22u, 0x22u, 0x08u,
#endif
};

/* 0x80cad0..0x80cae0: 16-byte auxiliary constant */
static const uint8_t s_utokenAuxKey[16] __attribute__((used, aligned(16))) = {
	0xb8u, 0x0du, 0xfdu, 0x68u, 0x5cu, 0x83u, 0x00u, 0xdeu,
	0x6au, 0x84u, 0xe5u, 0x87u, 0xd2u, 0x3du, 0xc8u, 0x4fu,
};

/* 0x80cae0..0x80caf0: 16-byte AES-128-CBC IV for User Token decryption */
static const uint8_t s_utokenAesIv[16] __attribute__((aligned(16))) = {
#if FW_DEBUG
	0xe2u, 0xabu, 0xadu, 0x82u, 0x80u, 0xa2u, 0x66u, 0x4fu,
	0xaau, 0x70u, 0x09u, 0x48u, 0x05u, 0xe5u, 0xc3u, 0x4cu,
#else
	0x15u, 0x6fu, 0x2fu, 0xacu, 0xeau, 0xc5u, 0x75u, 0xf7u,
	0xb3u, 0xb9u, 0xc4u, 0x62u, 0x04u, 0xe5u, 0xfcu, 0xb9u,
#endif
};

/* ============================================================================
 * Functions (0x80b09c..0x80b65a)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *hookNode;
	uint8_t tokenWorkBuf[UTOKEN_BYTE_SIZE];
	int ret;

	smSrvInit(&srvCtx);

	fnPtr.fn = utokenCmd1VerifyOnly;
	fnPtr.adj = 0;
	ret = SCE_SBL_ERROR_UTOKEN_FAILED;
	if (utokenSmAddHook(&srvCtx, UTOKEN_CMD_VERIFY_ONLY, tokenWorkBuf, &fnPtr, &hookNode) == 0) {
		fnPtr.fn = utokenCmd2VerifyAndApply;
		fnPtr.adj = 0;
		if (utokenSmAddHook(&srvCtx, UTOKEN_CMD_VERIFY_AND_APPLY, tokenWorkBuf, &fnPtr, &hookNode) == 0) {
			smSrvRunLoop(&srvCtx);
			ret = 0x55555555;
		}
	}

	smSrvTeardown(&srvCtx);
	return ret;
}

/* 0x80b14e (name ours) */
int utokenSmHookInvoke(void *closure, void *packetBuf)
{
	uintptr_t *words = (uintptr_t *)closure;
	uintptr_t fn = words[2];
	uintptr_t self = words[1] + words[3];

	if ((fn & 1u) != 0u) {
		uintptr_t vtable = *(const uintptr_t *)self;

		fn = *(const uintptr_t *)(vtable + fn - 1u);
	}
	return ((SmCmdMethod)fn)((void *)self, packetBuf);
}

/* 0x80b182 (name ours) */
int utokenSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return (int)0x800f000cu;

	node->cmdId = cmdId;
	node->next = NULL;
	node->vtable = (const void *)&s_utokenHookVtable[2];
	node->handler = mgr;
	node->arg0 = (void *)fnPtr->fn;
	node->adj = fnPtr->adj;

	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b1fc (name ours) */
int utokenDecryptToken(void *mgr, const uint8_t *src, uint8_t *dst,
		uint32_t size)
{
	(void)mgr;
	memcpy(dst, src, UTOKEN_HEADER_CLEAR_SIZE);
	return bigmacAes128CbcDecryptSync(dst + UTOKEN_HEADER_CLEAR_SIZE,
			src + UTOKEN_HEADER_CLEAR_SIZE,
			size - UTOKEN_HEADER_CLEAR_SIZE,
			s_utokenAesKey, (void *)s_utokenAesIv);
}

/* 0x80b23c (name ours) */
int utokenComputeSha256(void *mgr, const uint8_t *buf, uint32_t size,
		uint8_t *outDigest, uint32_t digestBufSize)
{
	(void)mgr;
	if (digestBufSize < 32u)
		return SCE_SBL_ERROR_UTOKEN_INVALID_ARG;
	if (bigmacSha256(outDigest, buf, size - UTOKEN_SIGNATURE_SIZE) != 0)
		return (int)0x800f0027u;
	return 0;
}

/* 0x80b270 (name ours) */
int utokenVerifyRsaSignature(void *mgr, const void *sig, uint32_t sigSize,
		const void *digest, uint8_t digestSize)
{
	BignumPublicKey rsaKey;
	uint8_t workBuf[256];

	(void)mgr;
	memset(workBuf, 0, sizeof(workBuf));
	if (sig == NULL || sigSize != 256u || digest == NULL || digestSize != 32u)
		return SCE_SBL_ERROR_UTOKEN_INVALID_ARG;

	rsaKey.modulus = (const uint32_t *)s_utokenRsaPublicKey.modulus;
	rsaKey.modulusWords = 64u;
	rsaKey.exponent = (const uint32_t *)s_utokenRsaPublicKey.exponent;
	rsaKey.exponentWords = 1u;

	if (bignumVerifyRsaPkcs1v15(workBuf, sig, &rsaKey, digest, 4u) < 0)
		return SCE_SBL_ERROR_UTOKEN_BAD_SIGNATURE;
	return 0;
}

/* 0x80b312 (name ours) */
int utokenValidateFields(void *mgr, const uint8_t *tokenBuf, uint32_t tokenSize,
		uint32_t reqValue, const uint8_t *consoleId)
{
	uint32_t expField;
	uint32_t expStatus;
	uint32_t cidStatus;
	uint32_t flags;

	(void)mgr;
	if (tokenSize < 0x30u)
		return SCE_SBL_ERROR_UTOKEN_INVALID_SIZE;

	expField = *(const uint32_t *)(tokenBuf + 0x18);
	if (expField != 0u)
		expStatus = (expField < reqValue) ? 0u : 1u;
	else
		expStatus = 2u;

	if (timingsafeBcmp(tokenBuf + 0x20, s_utokenZeroCid, 16u) != 0)
		cidStatus = (timingsafeBcmp(tokenBuf + 0x20, consoleId, 16u) == 0) ? 1u : 0u;
	else
		cidStatus = 2u;

	flags = *(const uint32_t *)(tokenBuf + 0x10);
	if ((flags & 1u) != 0u) {
		if (cidStatus != 1u)
			return SCE_SBL_ERROR_UTOKEN_CID_MISMATCH;
	}

	if ((((flags >> 4) | (flags >> 1)) & 1u) != 0u ||
	    (((flags >> 9) | (flags >> 7)) & 1u) != 0u) {
		if (cidStatus != 1u)
			return SCE_SBL_ERROR_UTOKEN_CID_MISMATCH;
		if (expStatus != 1u)
			return SCE_SBL_ERROR_UTOKEN_EXPIRED;
	}

	if (((flags >> 6) & 1u) != 0u ||
	    (((flags >> 11) | (flags >> 8)) & 1u) != 0u ||
	    (((flags >> 5) | (flags >> 10)) & 1u) != 0u) {
		if (cidStatus != 1u)
			return SCE_SBL_ERROR_UTOKEN_CID_MISMATCH;
		if (expStatus - 1u > 1u)
			return SCE_SBL_ERROR_UTOKEN_EXPIRED;
	}

	return 0;
}

/* 0x80b408 (name ours) */
int utokenProcessCommand(uint8_t *tokenWorkBuf, SceUtokenCommPacket *pkt,
		uint32_t applyFlag)
{
	volatile const int8_t * const mmioBase =
		(volatile const int8_t *)(uintptr_t)0xe0062260u;
	volatile uint32_t * const utokenHwRegs =
		(volatile uint32_t *)(uintptr_t)0xe0030000u;
	union {
		int8_t b[4];
		uint32_t w;
	} cfgLimit;
	uint8_t cidCtx[16];
	uint8_t consoleId[16];
	uint8_t digest[32];
	uint32_t hwWords[8];
	uint32_t addr = pkt->tokenPhysAddr;
	uint32_t size = pkt->tokenSize;
	uint32_t reqValue = pkt->reqValue;
	int rangeOk = 0;
	int status;

	cfgLimit.b[0] = mmioBase[0];
	cfgLimit.b[1] = mmioBase[1];
	cfgLimit.b[2] = mmioBase[2];
	cfgLimit.b[3] = mmioBase[3];

	if (addr > 0x401fffffu) {
		uint64_t end64 = (uint64_t)addr + (uint64_t)size;
		uint64_t limit64 = (uint64_t)cfgLimit.w + 0x40000000ull;

		if (addr < limit64 && end64 <= limit64)
			rangeOk = 1;
	}
	if (!rangeOk && addr > 0x1effffffu) {
		if (addr <= 0x1f007fffu &&
		    addr + size >= addr &&
		    addr + size <= 0x1f008000u) {
			rangeOk = 1;
		} else if (addr > 0x1f83ffffu &&
			   addr > 0x1f85ffffu &&
			   addr + size >= addr &&
			   addr + size <= 0x1f860000u) {
			rangeOk = 1;
		}
	}

	if (!rangeOk) {
		pkt->result = SCE_SBL_ERROR_UTOKEN_INVALID_ARG;
		return 0;
	}

	aimgrReadConsoleIdAlias1(cidCtx);
	status = aimgrGetConsoleId(cidCtx, consoleId);
	if (status != 0) {
		pkt->result = status;
		goto cleanup;
	}

	if (size != UTOKEN_BYTE_SIZE) {
		pkt->result = SCE_SBL_ERROR_UTOKEN_INVALID_ARG;
		goto cleanup;
	}

	if (utokenDecryptToken(tokenWorkBuf, (const uint8_t *)(uintptr_t)addr,
			tokenWorkBuf, size) != 0) {
		pkt->result = SCE_SBL_ERROR_UTOKEN_FAILED;
		goto cleanup;
	}

	status = utokenComputeSha256(tokenWorkBuf, tokenWorkBuf, size, digest, 32u);
	if (status != 0) {
		pkt->result = status;
		goto cleanup;
	}

	status = utokenVerifyRsaSignature(tokenWorkBuf,
			tokenWorkBuf + UTOKEN_SIGNATURE_OFFSET,
			UTOKEN_SIGNATURE_SIZE, digest, 32u);
	if (status != 0) {
		pkt->result = status;
		goto cleanup;
	}

	status = utokenValidateFields(tokenWorkBuf, tokenWorkBuf, size,
			reqValue, consoleId);
	if (status != 0) {
		pkt->result = status;
		goto cleanup;
	}

	if (applyFlag != 0u) {
		memcpy((void *)(uintptr_t)addr, tokenWorkBuf, size);
		memset(hwWords, 0, 8u);
		memcpy(hwWords, tokenWorkBuf + 0x10, 8u);

		utokenHwRegs[0] = hwWords[0];
		utokenHwRegs[1] = hwWords[1];
		utokenHwRegs[2] = hwWords[2];
		utokenHwRegs[3] = hwWords[3];
		utokenHwRegs[4] = hwWords[4];
		utokenHwRegs[5] = hwWords[5];
		utokenHwRegs[6] = hwWords[6];
		utokenHwRegs[7] = hwWords[7];
		utokenHwRegs[8] = 0x519u;
	}

	pkt->result = 0;

cleanup:
	aimgrClearConsoleId(cidCtx);
	return 0;
}

/* 0x80b64e (name ours) */
int utokenCmd2VerifyAndApply(void *mgr, void *packetBuf)
{
	return utokenProcessCommand((uint8_t *)mgr, (SceUtokenCommPacket *)packetBuf, 1u);
}

/* 0x80b654 (name ours) */
int utokenCmd1VerifyOnly(void *mgr, void *packetBuf)
{
	return utokenProcessCommand((uint8_t *)mgr, (SceUtokenCommPacket *)packetBuf, 0u);
}
