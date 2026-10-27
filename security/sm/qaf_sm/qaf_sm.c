/*
 * qaf_sm.c: QA Flag Secure Module (qaf_sm.self) boot command dispatcher
 * (commands 4..8, 0xff), mailbox service handlers (commands 1, 2, 12, 13),
 * SNVS AES-XTS sector encryption/decryption, AES-CMAC record verification,
 * and QA token RSA-2048 + AES-256-CBC + AES-256-CMAC authentication
 * (0x80b09c..0x80d0e0, .rodata/.data 0x80ec5c..0x80eec8).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "qaf_sm.h"

/* ============================================================================
 * .rodata (0x80ec5c..0x80edc8)
 * ============================================================================ */

static const uintptr_t s_qafHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)qafHookInvoke,
	0u,
};

static const uintptr_t s_qafBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u,
};

/* 0x80ec80..0x80ed80: 256-byte RSA-2048 modulus for QA token signatures */
static const uint32_t s_qafRsaModulus[64] = {
#if FW_DEBUG
	0xbb7be6cfu, 0x91315fb3u, 0x1bb4f55cu, 0xa3a39557u,
	0xb3762c5bu, 0x4df38597u, 0x6772f4a8u, 0xbc0107a3u,
	0xa275bfcdu, 0xbd6794adu, 0x9a19c4e0u, 0x7fb82aa0u,
	0x7f9337afu, 0x57bc7d3bu, 0xd962caf6u, 0x6ab85568u,
	0x71a897f6u, 0x267ab378u, 0x47814f4eu, 0x10e94112u,
	0x199fe504u, 0xb92e311au, 0x902a1429u, 0x6b89ce79u,
	0xe9dffe3du, 0x9db18758u, 0x5900b1dfu, 0x00611fdau,
	0xefc4d555u, 0x187a92aeu, 0x5c81857eu, 0xa1d16f32u,
	0xb7562b2au, 0x2b71da84u, 0x01dcca54u, 0x16132b9cu,
	0x49d4d553u, 0xdec3f76cu, 0x8f9ee6f2u, 0x2d6e2697u,
	0x1a07d084u, 0x0a0f1580u, 0xd23b9e4fu, 0xd8794d7fu,
	0x73d78fe5u, 0xdd4d7b62u, 0x90abce93u, 0xb0eb9fd5u,
	0xba3f812eu, 0xa0db5a9du, 0x92ab7288u, 0xea5ab2c5u,
	0xddc2f096u, 0x82fd1faeu, 0x2a265f21u, 0x3bc71134u,
	0x1f163309u, 0xd3bafaabu, 0x4a291a29u, 0xa1ad5ed6u,
	0x306deda7u, 0x76d1173fu, 0x2e528e61u, 0xc3dc4b09u,
#else
	0x0f7dced2u, 0x88a7fc4bu, 0x658f66b0u, 0xc2564f6du,
	0x75cf95a9u, 0x36915202u, 0x4450a381u, 0x1e093a8fu,
	0x371202bau, 0xa8d7729fu, 0xe49ac2d7u, 0x625d8de2u,
	0x39d5fa84u, 0x12b170a0u, 0xf255fc51u, 0x0fd359b5u,
	0xc7297651u, 0x95b8cf38u, 0xba02b60du, 0x78921dd2u,
	0x4175ddd1u, 0xa627a2c0u, 0x261b2fe0u, 0x08dda71du,
	0x99432f8fu, 0x8dbb54aeu, 0x6397e51cu, 0x8c180705u,
	0x57efaba9u, 0x223d6426u, 0x0978cdf4u, 0x0b6fc5deu,
	0x24cc16ffu, 0xb6ef3544u, 0x2e19f619u, 0xfe5ffabfu,
	0x6676f5a6u, 0xbb183246u, 0xa878b07cu, 0x1686e9c4u,
	0xb7a6f901u, 0xe5dd41deu, 0x2d8da51cu, 0x544ff885u,
	0xa6ba40d2u, 0x3268709fu, 0xed13bd5bu, 0x4f346e2fu,
	0xf79e548du, 0x7c9ba5a9u, 0x21b3895fu, 0x002091b0u,
	0x020a5681u, 0xdc483a17u, 0x25ff6d9au, 0x3d56fcc4u,
	0x81e3b913u, 0xd6058337u, 0xc9873b22u, 0x9b8e3a72u,
	0x39feb47fu, 0xa89f16ffu, 0x8bcbeeedu, 0x778e62a9u,
#endif
};

/* 0x80ed80..0x80ed90: RSA-2048 modulus word count, exponent (0x10001), exponent word count */
static const uint32_t s_qafRsaParams[4] __attribute__((used)) = {
	0x00000100u,
	0x01000100u,
	0x00000004u,
	0x00000000u,
};

/* 0x80ed90..0x80eda0: 16-byte all-zero IV */
static const uint8_t s_qafZeroIv16[16] = {
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
};

/* 0x80eda0..0x80edb0: C++ vtable for QafSnvsCipher */
static const uintptr_t s_qafSnvsCipherVtable[4] = {
	0u,
	0u,
	(uintptr_t)qafSnvsCipherDtorD2,
	(uintptr_t)qafSnvsCipherDtorD0,
};

/* 0x80edb0..0x80edc8: bitwise-inverted magic strings (~"SNVS", ~"qafv", ~"qaf\0NO_FLAGS\0") */
static const uint8_t s_qafInvMagicSnvs[4] = {
	0xacu, 0xb1u, 0xa9u, 0xacu,
};
static const uint8_t s_qafInvMagicQafv[4] = {
	0x8eu, 0x9eu, 0x99u, 0x89u,
};
static const uint8_t s_qafInvMagicQafNoFlags[16] = {
	0x8eu, 0x9eu, 0x99u, 0xffu, 0xb1u, 0xb0u, 0xa0u, 0xb9u,
	0xb3u, 0xbeu, 0xb8u, 0xacu, 0xffu, 0x00u, 0x00u, 0x00u,
};

/* ============================================================================
 * .data & .bss (0x80eecc..0x80f840)
 * ============================================================================ */

typedef int (*BigmacEcbStepFn)(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);

static BigmacEcbStepFn s_qafXtsEcbEncryptFn;
static BigmacEcbStepFn s_qafXtsEcbSecondFn;

static uint8_t s_qafSlot506[32] __attribute__((aligned(16)));
static uint8_t s_qafSlot507[32] __attribute__((aligned(16)));
static uint8_t s_qafSnvsBufA[0x400] __attribute__((aligned(16)));
static uint8_t s_qafSnvsBufB[0x3e0] __attribute__((aligned(16)));
static uint8_t s_qafTokenWorkBuf[0xa0] __attribute__((aligned(16)));
static QafSnvsCipher s_qafSnvsCipher;
static uint32_t s_qafSnvsInitialized;
static uint8_t s_qafSnvsKey1[32] __attribute__((aligned(16)));
static uint8_t s_qafSnvsKey2[32] __attribute__((aligned(16)));
static uint8_t s_qafSnvsKey3[32] __attribute__((aligned(16)));

/* ============================================================================
 * Keyring Accessor & Main Entry Point (0x80b09c..0x80b380)
 * ============================================================================ */

/* 0x80b09c (name ours) */
void qafReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	uint32_t off = slotIndex * 0x20u;

	outWords8[0] = REG32(0xe0058000u + off);
	outWords8[1] = REG32(0xe0058004u + off);
	outWords8[2] = REG32(0xe0058008u + off);
	outWords8[3] = REG32(0xe005800cu + off);
	outWords8[4] = REG32(0xe0058010u + off);
	outWords8[5] = REG32(0xe0058014u + off);
	outWords8[6] = REG32(0xe0058018u + off);
	outWords8[7] = REG32(0xe005801cu + off);
}

/* 0x80b0e6 */
int sm_main(void)
{
	register int cmdReg __asm__("$1");
	register void *argReg __asm__("$2");
	int cmd = cmdReg;
	void *arg = argReg;
	SmServiceContext srvCtx;
	uint8_t rawCid[16];
	uint8_t decodedCid[16];
	QafKeyContext keyCtx;
	SmServiceHookNode *node = NULL;
	uintptr_t addr = (uintptr_t)arg;
	int status = SCE_SBL_ERROR_QAF_INVALID_VALUE;

	aimgrReadConsoleIdAlias1(rawCid);
	smSrvInit(&srvCtx);

	if (cmd != SCE_SBL_QAF_BOOT_CMD_SERVICE) {
		union {
			uint8_t b[4];
			uint32_t w;
		} dramSize;
		int rangeValid = 0;

		dramSize.b[0] = REG8(0xe0062260u);
		dramSize.b[1] = REG8(0xe0062261u);
		dramSize.b[2] = REG8(0xe0062262u);
		dramSize.b[3] = REG8(0xe0062263u);

		if (addr >= 0x40200000u) {
			uint64_t dramEnd = (uint64_t)dramSize.w + 0x40000000ULL;

			if ((uint64_t)addr < dramEnd &&
			    (uint64_t)addr + 128ULL <= dramEnd) {
				rangeValid = 1;
			}
		} else if (addr <= 0x1effffffu) {
			goto cleanup;
		}

		if (!rangeValid) {
			if (addr <= 0x1f007fffu) {
				uint32_t reqEnd = (uint32_t)addr + 128u;

				if (reqEnd >= (uint32_t)addr && reqEnd <= 0x1f008000u)
					rangeValid = 1;
			}
		}
		if (!rangeValid) {
			if (addr >= 0x1f840000u && addr <= 0x1f85ffffu) {
				uint32_t reqEnd = (uint32_t)addr + 128u;

				if (reqEnd >= (uint32_t)addr && reqEnd <= 0x1f860000u)
					rangeValid = 1;
			}
		}
		if (!rangeValid)
			goto cleanup;
	}

	qafReadKeyringSlot32(0x506u, (uint32_t *)(void *)s_qafSlot506);
	qafReadKeyringSlot32(0x507u, (uint32_t *)(void *)s_qafSlot507);

	if (aimgrGetConsoleId(rawCid, decodedCid) != 0) {
		status = SCE_SBL_ERROR_QAF_CID_ERROR;
		goto cleanup;
	}

	qafKeyCtxInitC1(&keyCtx, s_qafSlot506, s_qafSlot507, decodedCid);

	switch (cmd) {
	case SCE_SBL_QAF_BOOT_CMD_VERIFY_REC:
		status = qafBootCmd4VerifyRecord(&keyCtx, (uint8_t *)arg, s_qafSlot506);
		break;
	case SCE_SBL_QAF_BOOT_CMD_GEN_REC:
		status = qafBootCmd5GenerateRecord(&keyCtx, (uint8_t *)arg, s_qafSlot506);
		break;
	case SCE_SBL_QAF_BOOT_CMD_CHECK_BIT1:
		status = qafBootCmd6CheckFlagBit1(&keyCtx, (const uint8_t *)arg);
		break;
	case SCE_SBL_QAF_BOOT_CMD_CLEAR_BIT1:
		status = qafBootCmd7ClearFlagBit1(&keyCtx, (uint8_t *)arg);
		break;
	case SCE_SBL_QAF_BOOT_CMD_SET_BIT1:
		status = qafBootCmd8SetFlagBit1(&keyCtx, (uint8_t *)arg);
		break;
	case SCE_SBL_QAF_BOOT_CMD_SERVICE:
	{
		SmMemberFnPtr fn1 = { qafCmd1HandleSnvsSlot0, 0 };
		SmMemberFnPtr fn2 = { qafCmd2HandleSnvsAllSlots, 0 };
		SmMemberFnPtr fn12 = { qafCmd12VerifyToken, 0 };
		SmMemberFnPtr fn13 = { qafCmd13VerifyAndApplyToken, 0 };

		if (qafRegisterHook(&srvCtx, SCE_SBL_QAF_CMD_SNVS_SLOT0,
				&keyCtx, &fn1, &node) == 0 &&
		    qafRegisterHook(&srvCtx, SCE_SBL_QAF_CMD_SNVS_ALL_SLOTS,
				&keyCtx, &fn2, &node) == 0 &&
		    qafRegisterHook(&srvCtx, SCE_SBL_QAF_CMD_VERIFY_TOKEN,
				&keyCtx, &fn12, &node) == 0 &&
		    qafRegisterHook(&srvCtx, SCE_SBL_QAF_CMD_APPLY_TOKEN,
				&keyCtx, &fn13, &node) == 0) {
			smSrvRunLoop(&srvCtx);
		}
		status = SCE_SBL_ERROR_QAF_INVALID_VALUE;
		break;
	}
	default:
		status = SCE_SBL_ERROR_QAF_INVALID_VALUE;
		break;
	}

	qafKeyCtxDtorD1(&keyCtx);

cleanup:
	smSrvTeardown(&srvCtx);
	aimgrClearConsoleId(rawCid);
	return status;
}

/* 0x80b380 (name ours) */
void qafHookInvoke(void *self, void *commBuf)
{
	uint8_t *closure = (uint8_t *)self;
	uintptr_t fnVal = *(const uintptr_t *)(const void *)(closure + 8u);
	uintptr_t objBase = *(const uintptr_t *)(const void *)(closure + 4u);
	int32_t adj = *(const int32_t *)(const void *)(closure + 12u);
	SmCmdMethod fn;

	if ((fnVal & 1u) != 0u) {
		uintptr_t vptr = *(const uintptr_t *)(objBase + (uintptr_t)adj);

		fn = *(const SmCmdMethod *)(vptr + fnVal - 1u);
	} else {
		fn = (SmCmdMethod)fnVal;
	}
	fn((void *)(objBase + (uintptr_t)adj), commBuf);
}

/* 0x80b3b4 (name ours) */
int qafRegisterHook(SmServiceContext *srvCtx, uint32_t cmdId,
		QafKeyContext *keyCtx, const SmMemberFnPtr *fnPtr,
		SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_qafHookVtable[2];
		node->handler = keyCtx;
		node->arg0 = (void *)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b42e (name ours) */
void qafReadHardwareCounter2240(uint8_t *out4)
{
	out4[0] = REG8(0xe0062240u);
	out4[1] = REG8(0xe0062241u);
	out4[2] = REG8(0xe0062242u);
	out4[3] = REG8(0xe0062243u);
}

/* 0x80b466 (name ours) */
void qafKeyCtxInitC2(QafKeyContext *ctx, const void *slot506,
		const void *slot507, const void *cid)
{
	memcpy(ctx->slot506, slot506, 32u);
	memcpy(ctx->slot507, slot507, 16u);
	memcpy(ctx->consoleId, cid, 16u);
}

/* 0x80b4a2 (name ours) */
void qafKeyCtxInitC1(QafKeyContext *ctx, const void *slot506,
		const void *slot507, const void *cid)
{
	memcpy(ctx->slot506, slot506, 32u);
	memcpy(ctx->slot507, slot507, 16u);
	memcpy(ctx->consoleId, cid, 16u);
}

/* 0x80b4de (name ours) */
void qafKeyCtxDtorD2(QafKeyContext *ctx)
{
	(void)ctx;
}

/* 0x80b4e0 (name ours) */
void qafKeyCtxDtorD1(QafKeyContext *ctx)
{
	(void)ctx;
}

/* ============================================================================
 * QA Token & Boot Command Handlers (0x80b4e2..0x80bac6)
 * ============================================================================ */

/* 0x80b4e2 (name ours) */
int qafUnwrapAndVerifyToken(QafKeyContext *ctx, const uint8_t *inToken)
{
	uint8_t formattedCid[16];
	int status;

	status = qafDecryptQaToken(inToken, s_qafTokenWorkBuf,
			ctx->slot506, ctx->slot507);
	if (status != 0)
		return status;

	status = qafVerifyQaTokenCmac(s_qafTokenWorkBuf, ctx->slot506);
	if (status != 0)
		return status;

	qafFormatConsoleIdForToken(formattedCid, ctx->consoleId);
	return qafVerifyTokenConsoleId(s_qafTokenWorkBuf, formattedCid);
}

/* 0x80b52c (name ours; mailbox command 12) */
int qafCmd12VerifyToken(void *ctxPtr, void *commBuf)
{
	QafKeyContext *ctx = (QafKeyContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	const uint8_t *token = pkt + 0x40u;
	int status;

	status = qafUnwrapAndVerifyToken(ctx, token);
	if (status == 0) {
		BignumPublicKey pubKey;

		pubKey.modulus = s_qafRsaModulus;
		pubKey.modulusWords = 64u;
		pubKey.exponent = &s_qafRsaParams[1];
		pubKey.exponentWords = 1u;
		status = qafVerifyQaTokenRsaSignature(token, pkt + 0xc0u, &pubKey);
	}
	*(int *)(void *)(pkt + 8u) = status;
	return status;
}

/* 0x80b572 (name ours) */
int qafBuildNormalizedToken(const QafKeyContext *ctx, uint8_t *outToken128)
{
	int status;

	memset(s_qafTokenWorkBuf, 0, 128u);
	status = qafBuildDefaultNoFlagsToken(s_qafTokenWorkBuf,
			ctx->slot506, ctx->consoleId);
	if (status == 0) {
		status = qafEncryptDefaultQaToken(s_qafTokenWorkBuf,
				ctx->slot506, ctx->slot507);
		if (status == 0)
			memcpy(outToken128, s_qafTokenWorkBuf, 128u);
	}
	return status;
}

/* 0x80b5d0 (name ours; mailbox command 13) */
int qafCmd13VerifyAndApplyToken(void *ctxPtr, void *commBuf)
{
	QafKeyContext *ctx = (QafKeyContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t *token = pkt + 0x40u;
	int status;

	status = qafUnwrapAndVerifyToken(ctx, token);
	if (status == 0) {
		BignumPublicKey pubKey;

		pubKey.modulus = s_qafRsaModulus;
		pubKey.modulusWords = 64u;
		pubKey.exponent = &s_qafRsaParams[1];
		pubKey.exponentWords = 1u;
		status = qafVerifyQaTokenRsaSignature(token, pkt + 0xc0u, &pubKey);
		if (status == 0)
			goto done;
	}

	memset(token, 0, 384u);
	status = qafBuildNormalizedToken(ctx, token);

done:
	*(int *)(void *)(pkt + 8u) = status;
	return status;
}

/* 0x80b630 (name ours; boot command 4) */
int qafBootCmd4VerifyRecord(QafKeyContext *ctx, uint8_t *inOutRec32,
		const void *cmacKey)
{
	uint8_t encRec[32];
	uint8_t decRec[32];
	int status;

	(void)ctx;
	status = qafEnsureSnvsCryptoInitialized();
	if (status != 0)
		return status;

	memcpy(encRec, inOutRec32, 32u);
	status = qafDecryptSnvsSectors(21u, encRec, decRec, 1u);
	if (status != 0)
		return status;

	if (qafVerifySnvsRecordCmac(decRec, cmacKey) == 0)
		return SCE_SBL_ERROR_QAF_BAD_MAC;

	memcpy(inOutRec32, decRec, 32u);
	return 0;
}

/* 0x80b6ac (name ours; boot command 5) */
int qafBootCmd5GenerateRecord(QafKeyContext *ctx, uint8_t *inOutRec32,
		const void *cmacKey)
{
	uint8_t rec[32];
	int status;

	(void)ctx;
	status = qafBuildDefaultSnvsRecord(rec, inOutRec32[4], cmacKey);
	if (status == 0) {
		status = qafEnsureSnvsCryptoInitialized();
		if (status == 0) {
			status = qafEncryptSnvsSectors(21u, rec, rec, 1u);
			if (status == 0)
				memcpy(inOutRec32, rec, 32u);
		}
	}
	return status;
}

/* 0x80b70e (name ours; boot command 6) */
int qafBootCmd6CheckFlagBit1(QafKeyContext *ctx, const uint8_t *inRec32)
{
	uint8_t w0[4];
	uint8_t w1[4];
	int status;

	(void)ctx;
	status = qafEnsureSnvsCryptoInitialized();
	if (status == 0) {
		status = qafReadSnvsRecord21(w1, w0, inRec32);
		if (status == 0 && (w0[0] & 2u) != 0u)
			return SCE_SBL_ERROR_QAF_BIT1_SET;
	}
	return status;
}

/* 0x80b746 (name ours; boot command 7) */
int qafBootCmd7ClearFlagBit1(QafKeyContext *ctx, uint8_t *inOutRec32)
{
	uint8_t rec[32];
	uint8_t w0[4];
	uint8_t w1[4];
	int status;

	(void)ctx;
	memcpy(rec, inOutRec32, 32u);
	status = qafEnsureSnvsCryptoInitialized();
	if (status == 0) {
		status = qafReadSnvsRecord21(w1, w0, rec);
		if (status == 0) {
			w0[0] &= (uint8_t)~2u;
			status = qafWriteSnvsRecord21(w1, w0, rec);
			if (status == 0)
				memcpy(inOutRec32, rec, 32u);
		}
	}
	return status;
}

/* 0x80b7c4 (name ours; boot command 8) */
int qafBootCmd8SetFlagBit1(QafKeyContext *ctx, uint8_t *inOutRec32)
{
	uint8_t rec[32];
	uint8_t w0[4];
	uint8_t w1[4];
	int status;

	(void)ctx;
	memcpy(rec, inOutRec32, 32u);
	status = qafEnsureSnvsCryptoInitialized();
	if (status == 0) {
		status = qafReadSnvsRecord21(w1, w0, rec);
		if (status == 0) {
			w0[0] |= 2u;
			status = qafWriteSnvsRecord21(w1, w0, rec);
			if (status == 0)
				memcpy(inOutRec32, rec, 32u);
		}
	}
	return status;
}

/* 0x80b842 (name ours; mailbox command 1) */
int qafCmd1HandleSnvsSlot0(void *ctxPtr, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t subCmd;
	uint32_t counter = 0u;
	uint32_t sectorIdx = 0u;
	uint8_t sectorBuf[32];
	int status;

	(void)ctxPtr;
	status = qafEnsureSnvsCryptoInitialized();
	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = status;
		return status;
	}

	qafReadHardwareCounter2240((uint8_t *)(void *)&counter);
	memset(s_qafSnvsBufA, 0, 992u);
	subCmd = *(const uint32_t *)(const void *)(pkt + 0x40u);

	if (subCmd == 0u) {
		status = qafBuildSnvsMacChallenge(counter, 0u, pkt + 0x50u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
	} else if (subCmd == 1u) {
		status = qafVerifyAndUnwrapSnvsSectorWrite(&counter, &sectorIdx,
				sectorBuf, 32u, pkt + 0x80u);
		counter = qafIncrementSnvsCounter(counter);
		memset(pkt + 0x80u, 0, 48u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		if (sectorIdx != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
			return SCE_SBL_ERROR_QAF_INVALID_VALUE;
		}
		status = qafReadSnvsRecord21(pkt + 0x48u, pkt + 0x4cu, sectorBuf);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
	} else if (subCmd == 2u) {
		status = qafWriteSnvsRecord21(pkt + 0x48u, pkt + 0x4cu, sectorBuf);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		status = qafWrapSnvsSectorReadResponse(counter, 0u, sectorBuf,
				32u, pkt + 0x50u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
	} else if (subCmd == 3u) {
		status = qafVerifySnvsCommitPacket(&counter, &sectorIdx, pkt + 0x80u);
		counter = qafIncrementSnvsCounter(counter);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		memset(pkt + 0x80u, 0, 48u);
		if (sectorIdx != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
			return SCE_SBL_ERROR_QAF_INVALID_VALUE;
		}
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
		return SCE_SBL_ERROR_QAF_INVALID_VALUE;
	}

	*(int *)(void *)(pkt + 8u) = 0;
	return 0;
}

/* 0x80b966 (name ours; mailbox command 2) */
int qafCmd2HandleSnvsAllSlots(void *ctxPtr, void *commBuf)
{
	QafKeyContext *ctx = (QafKeyContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t subCmd;
	uint32_t reqSector;
	uint32_t counter = 0u;
	uint32_t sectorIdx = 0u;
	int status;

	status = qafEnsureSnvsCryptoInitialized();
	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = status;
		return status;
	}

	qafReadHardwareCounter2240((uint8_t *)(void *)&counter);
	memset(s_qafSnvsBufA, 0, 992u);
	subCmd = *(const uint32_t *)(const void *)(pkt + 0x40u);
	reqSector = *(const uint32_t *)(const void *)(pkt + 0x44u);

	if (subCmd == 0u) {
		status = qafBuildSnvsMacChallenge(counter, reqSector, pkt + 0x68u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
	} else if (subCmd == 1u) {
		status = qafVerifyAndUnwrapSnvsSectorWrite(&counter, &sectorIdx,
				s_qafSnvsBufA, 32u, pkt + 0x98u);
		counter = qafIncrementSnvsCounter(counter);
		memset(pkt + 0x98u, 0, 48u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		if (reqSector != sectorIdx) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
			return SCE_SBL_ERROR_QAF_INVALID_VALUE;
		}
		status = qafDecryptSnvsSectors(reqSector, s_qafSnvsBufA,
				s_qafSnvsBufB, 1u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		if (qafVerifySnvsRecordCmac(s_qafSnvsBufB, ctx->slot506) == 0) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_BAD_MAC;
			return SCE_SBL_ERROR_QAF_BAD_MAC;
		}
		memcpy(pkt + 0x48u, s_qafSnvsBufB, 992u);
	} else if (subCmd == 2u) {
		status = qafBuildDefaultSnvsRecord(pkt + 0x48u, pkt[0x4cu],
				ctx->slot506);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		status = qafEncryptSnvsSectors(reqSector, pkt + 0x48u,
				s_qafSnvsBufA, 1u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		status = qafWrapSnvsSectorReadResponse(counter, reqSector,
				s_qafSnvsBufA, 32u, pkt + 0x68u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
	} else if (subCmd == 3u) {
		status = qafVerifySnvsCommitPacket(&counter, &sectorIdx, pkt + 0x98u);
		counter = qafIncrementSnvsCounter(counter);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		memset(pkt + 0x98u, 0, 48u);
		if (reqSector != sectorIdx) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
			return SCE_SBL_ERROR_QAF_INVALID_VALUE;
		}
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_QAF_INVALID_VALUE;
		return SCE_SBL_ERROR_QAF_INVALID_VALUE;
	}

	*(int *)(void *)(pkt + 8u) = 0;
	return 0;
}

/* ============================================================================
 * SNVS Sector Crypto & Packet Protocol Helpers (0x80bac6..0x80bf30)
 * ============================================================================ */

/* 0x80bac6 (name ours) */
void qafReadKeyringSlot32Alias(uint32_t slotIndex, uint32_t *outWords8)
{
	qafReadKeyringSlot32(slotIndex, outWords8);
}

/* 0x80bb10 (name ours) */
void qafStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		qafSnvsCipherCtorC1(&s_qafSnvsCipher);
	else if (isInit == 0)
		qafSnvsCipherDtorD2(&s_qafSnvsCipher);
}

/* 0x80bb30 (name ours) */
int qafEnsureSnvsCryptoInitialized(void)
{
	int status = 0;

	if (s_qafSnvsInitialized == 0u) {
		memset(s_qafSnvsKey1, 0, 32u);
		memset(s_qafSnvsKey2, 0, 32u);
		memset(s_qafSnvsKey3, 0, 32u);
		qafReadKeyringSlot32Alias(0x502u, (uint32_t *)(void *)s_qafSnvsKey1);
		qafReadKeyringSlot32Alias(0x503u, (uint32_t *)(void *)s_qafSnvsKey2);
		qafReadKeyringSlot32Alias(0x504u, (uint32_t *)(void *)s_qafSnvsKey3);
		status = qafSnvsCipherInit(&s_qafSnvsCipher,
				s_qafSnvsKey1, s_qafSnvsKey2, s_qafSnvsKey3,
				128u, 128u);
		if (status == 0)
			s_qafSnvsInitialized = 1u;
	}
	return status;
}

/* 0x80bbac (name ours) */
int qafReadSnvsRecord21(void *outWord0, void *outWord1, const void *inSector32)
{
	uint8_t decBuf[32];
	uint8_t macCheck[32];
	uint8_t savedMac[16];
	int status;

	if (s_qafSnvsInitialized == 0u || outWord0 == NULL ||
	    outWord1 == NULL || inSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(decBuf, 0, sizeof(decBuf));
	status = qafSnvsDecryptSectors(&s_qafSnvsCipher, inSector32,
			decBuf, 32u, 0u);
	if (status != 0)
		return status;

	memcpy(macCheck, decBuf, 32u);
	memset(savedMac, 0, sizeof(savedMac));
	status = qafSnvsSignRecordCmac(&s_qafSnvsCipher, macCheck);
	if (status != 0)
		return status;

	if (timingsafeBcmp(savedMac, decBuf + 16u, 16u) != 0)
		return SCE_SBL_ERROR_CF_BAD_DIGEST & ~0x0600u;

	memcpy(outWord0, macCheck + 8u, 4u);
	memcpy(outWord1, macCheck + 12u, 4u);
	return 0;
}

/* 0x80bc5c (name ours) */
int qafValidateSectorRange(uint32_t sectorCount, uint32_t startSector)
{
	if (sectorCount - 1u > 30u || startSector > 31u)
		return 0;
	return (startSector + sectorCount < 32u);
}

/* 0x80bc78 (name ours) */
int qafDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (s_qafSnvsInitialized == 0u || dst == NULL || src == NULL ||
	    qafValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return qafSnvsDecryptSectors(&s_qafSnvsCipher, src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80bcce (name ours) */
uint32_t qafIsSnvsInitialized(void)
{
	return s_qafSnvsInitialized;
}

/* 0x80bcd4 (name ours) */
QafSnvsCipher *qafGetSnvsCipher(void)
{
	return &s_qafSnvsCipher;
}

/* 0x80bcda (name ours) */
const uint8_t *qafGetZeroIv16(void)
{
	return s_qafZeroIv16;
}

/* 0x80bce0 (name ours) */
int qafBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
		uint8_t *outChallenge16)
{
	uint32_t slot516[8];
	uint32_t slot511[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint32_t byteOff;
	uint16_t sum = 0u;
	uint32_t i;
	int status;

	qafReadKeyringSlot32Alias(0x516u, slot516);
	if (slot516[0] == 1u)
		return SCE_SBL_ERROR_NOT_INITIALIZED;

	byteOff = sectorIdx * 32u;
	if (outChallenge16 == NULL || byteOff > 992u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outChallenge16, 0, 16u);
	outChallenge16[0] = (uint8_t)byteOff;
	outChallenge16[1] = (uint8_t)(byteOff >> 8);
	outChallenge16[2] = 32u;
	outChallenge16[3] = 0u;
	outChallenge16[4] = (uint8_t)counter;
	outChallenge16[5] = (uint8_t)(counter >> 8);
	outChallenge16[6] = (uint8_t)(counter >> 16);
	outChallenge16[7] = (uint8_t)(counter >> 24);

	for (i = 0u; i < 13u; i++)
		sum = (uint16_t)(sum + outChallenge16[i]);
	sum = (uint16_t)~sum;
	outChallenge16[14] = (uint8_t)sum;
	outChallenge16[15] = (uint8_t)(sum >> 8);

	qafReadKeyringSlot32Alias(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_qafZeroIv16, 16u);
	status = bigmacStartAes128CbcEncrypt(outChallenge16, outChallenge16,
			16u, slot511, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80bde2 (name ours) */
int qafVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
		uint32_t *outSectorIdx, uint8_t *outSectorData32,
		uint32_t sectorSize, uint8_t *inOutPacket48)
{
	uint32_t slot511[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint16_t sum = 0u;
	uint16_t pktSum;
	uint32_t pktCounter;
	uint16_t pktByteOff;
	uint32_t i;
	int status;

	if (inOutCounter == NULL || outSectorIdx == NULL ||
	    outSectorData32 == NULL || inOutPacket48 == NULL ||
	    sectorSize != 32u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	qafReadKeyringSlot32Alias(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_qafZeroIv16, 16u);
	status = bigmacStartAes128CbcDecrypt(inOutPacket48, inOutPacket48,
			48u, slot511, iv, bmCtrl);
	if (status != 0)
		return status;
	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;

	for (i = 0u; i < 45u; i++)
		sum = (uint16_t)(sum + inOutPacket48[i]);
	sum = (uint16_t)~sum;
	pktSum = (uint16_t)inOutPacket48[46] | ((uint16_t)inOutPacket48[47] << 8);
	if (pktSum != sum)
		return (int)0x800f0027u;

	pktCounter = (uint32_t)inOutPacket48[4] |
		     ((uint32_t)inOutPacket48[5] << 8) |
		     ((uint32_t)inOutPacket48[6] << 16) |
		     ((uint32_t)inOutPacket48[7] << 24);
	if (pktCounter != *inOutCounter)
		return (int)0x800f0027u;

	*inOutCounter = pktCounter;
	pktByteOff = (uint16_t)inOutPacket48[0] | ((uint16_t)inOutPacket48[1] << 8);
	*outSectorIdx = (uint32_t)(pktByteOff >> 5);
	memcpy(outSectorData32, inOutPacket48 + 14u, 32u);
	return status;
}

/* 0x80beee (name ours) */
uint32_t qafIncrementSnvsCounter(uint32_t counter)
{
	uint32_t nextCounter = counter + 1u;

	REG32(0xe0030000u) = nextCounter;
	REG32(0xe0030020u) = 0x512u;
	return nextCounter;
}

/* 0x80bf24 (name ours) */
void qafStaticInitThunk(void)
{
	qafStaticInitOrDtor(1);
}

/* 0x80bf2a (name ours) */
void qafStaticDtorThunk(void)
{
	qafStaticInitOrDtor(0);
}

/* ============================================================================
 * QafSnvsCipher Class & AES-XTS Engine (0x80bf30..0x80cbbe)
 * ============================================================================ */

/* 0x80bf30 (name ours) */
void qafSnvsCipherDtorD2(QafSnvsCipher *c)
{
	c->vptr = &s_qafSnvsCipherVtable[2];
}

/* 0x80bf38 (name ours) */
void qafSnvsCipherDtorD0(QafSnvsCipher *c)
{
	c->vptr = &s_qafSnvsCipherVtable[2];
	operatorDelete(c);
}

/* 0x80bf42 (name ours) */
void qafSnvsCipherCtorC2(QafSnvsCipher *c)
{
	c->vptr = &s_qafSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80bf60 (name ours) */
void qafSnvsCipherCtorC1(QafSnvsCipher *c)
{
	c->vptr = &s_qafSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80bf7e (name ours) */
void qafSnvsCipherDtorD1(QafSnvsCipher *c)
{
	c->vptr = &s_qafSnvsCipherVtable[2];
}

/* 0x80bf86 (name ours) */
int qafSnvsCipherInit(QafSnvsCipher *c, const uint8_t *dataKey,
		const uint8_t *tweakKey, const uint8_t *cmacKey,
		uint32_t dataKeyBits, uint32_t cmacKeyBits)
{
	if (dataKey == NULL || tweakKey == NULL || cmacKey == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (dataKeyBits != 128u && dataKeyBits != 256u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (cmacKeyBits != 128u && cmacKeyBits != 256u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	c->dataKey = dataKey;
	c->tweakKey = tweakKey;
	c->cmacKey = cmacKey;
	c->dataKeyBits = dataKeyBits;
	c->cmacKeyBits = cmacKeyBits;
	return 0;
}

/* 0x80bfd8 (name ours) */
int qafSnvsSignRecordCmac(const QafSnvsCipher *c, uint8_t *inOutRec32)
{
	uint8_t mac[16];
	uint8_t msg[64];
	uint32_t bmCtrl[2];
	int status;

	if (inOutRec32 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(mac, 0, sizeof(mac));
	memset(msg, 0, sizeof(msg));
	memcpy(msg, inOutRec32, 32u);
	memset(msg + 16u, 0, 16u);

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(mac, msg, 64u, c->cmacKey,
			c->cmacKeyBits >> 3, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0)
			memcpy(inOutRec32 + 16u, mac, 16u);
	}
	return status;
}

/* 0x80c088 (name ours) */
int qafFormatSectorTweak(const QafSnvsCipher *c, uint32_t sectorIdx,
		uint64_t *outTweak16)
{
	uint64_t val;

	(void)c;
	if (outTweak16 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outTweak16, 0, 16u);
	val = (uint64_t)sectorIdx << 1;
	le64tohSm(outTweak16, &val);
	return 0;
}

/* 0x80c0d4 (name ours) */
int qafSnvsDecryptSectors(const QafSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    dst == NULL || src == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = qafFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (qafXtsDecryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = (int)0x800f0027u;
		}
	}
	return status;
}

/* 0x80c168 (name ours) */
void qafSnvsCipherNoop1(void)
{
}

/* 0x80c16a (name ours) */
int qafXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst)
{
	uint32_t bmCtrl[2];
	uint8_t tweak[16];
	uint8_t prevTweak[16];
	uint8_t scratch[16];
	uint32_t offset = 0u;
	uint32_t i;

	if (sectorBytes < 16u)
		return -1;

	bigmacInitChannel0(bmCtrl);
	if (keyBits == 128u) {
		s_qafXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_qafXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_qafXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_qafXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_qafXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_qafXtsEcbSecondFn(dst + offset, dst + offset,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset + i] ^= tweak[i];

		for (i = 0u; i < 16u; i++) {
			uint8_t nextCarry = tweak[i] >> 7;

			tweak[i] = (uint8_t)((tweak[i] << 1) + carry);
			carry = nextCarry;
		}
		if (carry != 0u)
			tweak[0] ^= 0x87u;

		offset += 16u;
	}

	if (offset < sectorBytes) {
		uint32_t rem = sectorBytes - offset;
		uint8_t carry = 0u;

		memcpy(prevTweak, tweak, 16u);
		if ((tweak[0] & 1u) != 0u) {
			tweak[0] ^= 0x87u;
			carry = 1u;
		}
		for (i = 16u; i > 0u; i--) {
			uint8_t b = prevTweak[i - 1u];
			uint8_t nextCarry = b & 1u;

			prevTweak[i - 1u] = (uint8_t)((carry << 7) | (b >> 1));
			carry = nextCarry;
		}

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = src[offset - 16u + i] ^ tweak[i];

		if (s_qafXtsEcbSecondFn(dst + offset - 16u, dst + offset - 16u,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] ^= tweak[i];

		for (i = 0u; i < rem; i++) {
			scratch[i] = src[offset + i] ^ prevTweak[i];
			dst[offset + i] = dst[offset - 16u + i];
		}
		for (; i < 16u; i++)
			scratch[i] = dst[offset - 16u + i] ^ prevTweak[i];

		if (s_qafXtsEcbSecondFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ prevTweak[i];
	}

	return 0;
}

/* 0x80c3f4 (name ours) */
int qafIncrementTweak128(uint8_t *tweak, uint32_t len)
{
	uint32_t i;

	for (i = 0u; i < len; i++) {
		if (tweak[i] != 0xffu) {
			tweak[i]++;
			break;
		}
		tweak[i] = 0u;
	}
	return 0;
}

/* 0x80c418 (name ours) */
int qafXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst)
{
	uint8_t curTweak[16];
	uint32_t sectorBytes = wordsPerSector << 4;
	uint32_t numSectors = (totalBytes / sectorBytes) +
			      ((totalBytes % sectorBytes) != 0u ? 1u : 0u);
	uint32_t remBytes = totalBytes;
	uint32_t offset = 0u;
	uint32_t i;

	memcpy(curTweak, tweakIv, 16u);
	for (i = 0u; i < numSectors; i++) {
		uint32_t chunk = (i == numSectors - 1u) ? remBytes : sectorBytes;

		qafXtsDecryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		qafIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* 0x80c4de (name ours) */
void qafReadHardwareKey2220(uint32_t *outWords8)
{
	outWords8[0] = REG32(0xe0062220u);
	outWords8[1] = REG32(0xe0062224u);
	outWords8[2] = REG32(0xe0062228u);
	outWords8[3] = REG32(0xe006222cu);
	outWords8[4] = REG32(0xe0062230u);
	outWords8[5] = REG32(0xe0062234u);
	outWords8[6] = REG32(0xe0062238u);
	outWords8[7] = REG32(0xe006223cu);
}

/* 0x80c54e (name ours) */
int qafEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (qafIsSnvsInitialized() == 0u || src == NULL || dst == NULL ||
	    qafValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return qafSnvsEncryptSectors(qafGetSnvsCipher(), src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80c5ac (name ours) */
int qafWriteSnvsRecord21(const void *inWord0, const void *inWord1,
		void *outSector32)
{
	uint8_t rec[32];
	uint8_t enc[32];
	uint32_t i;
	int status;

	if (qafIsSnvsInitialized() == 0u || inWord0 == NULL ||
	    inWord1 == NULL || outSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(rec, 0, sizeof(rec));
	memcpy(rec, s_qafInvMagicSnvs, 4u);
	for (i = 0u; i < 4u; i++)
		rec[i] = (uint8_t)~rec[i];
	rec[4] = 1u;
	memset(rec + 5u, 0, 3u);
	memcpy(rec + 8u, inWord0, 4u);
	memcpy(rec + 12u, inWord1, 4u);

	status = qafSnvsSignRecordCmac(qafGetSnvsCipher(), rec);
	if (status != 0)
		return status;

	memset(enc, 0, sizeof(enc));
	status = qafSnvsEncryptSectors(qafGetSnvsCipher(), rec, enc, 32u, 0u);
	if (status == 0)
		memcpy(outSector32, enc, 32u);
	return status;
}

/* 0x80c68a (name ours) */
int qafWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
		const void *sectorData32, uint32_t sectorSize,
		uint8_t *outPacket48)
{
	uint32_t hwKey[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint32_t byteOff = sectorIdx * 32u;
	uint16_t sum = 0u;
	uint32_t i;
	int status;

	if (sectorSize != 32u || outPacket48 == NULL || byteOff > 992u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	memset(outPacket48, 0, 48u);
	outPacket48[0] = (uint8_t)byteOff;
	outPacket48[1] = (uint8_t)(byteOff >> 8);
	outPacket48[2] = (uint8_t)sectorSize;
	outPacket48[3] = 0u;
	outPacket48[4] = (uint8_t)counter;
	outPacket48[5] = (uint8_t)(counter >> 8);
	outPacket48[6] = (uint8_t)(counter >> 16);
	outPacket48[7] = (uint8_t)(counter >> 24);
	memcpy(outPacket48 + 14u, sectorData32, 32u);

	for (i = 0u; i < 45u; i++)
		sum = (uint16_t)(sum + outPacket48[i]);
	sum = (uint16_t)~sum;
	outPacket48[46] = (uint8_t)sum;
	outPacket48[47] = (uint8_t)(sum >> 8);

	qafReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, qafGetZeroIv16(), 16u);
	status = bigmacStartAes128CbcEncrypt(outPacket48, outPacket48,
			48u, hwKey, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80c788 (name ours) */
int qafVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
		uint8_t *inOutPacket16)
{
	uint32_t hwKey[8];
	uint32_t bmCtrl[2];
	uint8_t iv[16];
	uint16_t sum = 0u;
	uint16_t pktSum;
	uint32_t pktCounter;
	uint16_t pktByteOff;
	uint32_t i;
	int status;

	if (inOutCounter == NULL || outSectorIdx == NULL || inOutPacket16 == NULL)
		return SCE_SBL_ERROR_INVALID_VALUE;

	qafReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, qafGetZeroIv16(), 16u);
	status = bigmacStartAes128CbcDecrypt(inOutPacket16, inOutPacket16,
			16u, hwKey, iv, bmCtrl);
	if (status != 0)
		return status;
	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;

	for (i = 0u; i < 13u; i++)
		sum = (uint16_t)(sum + inOutPacket16[i]);
	sum = (uint16_t)~sum;
	pktSum = (uint16_t)inOutPacket16[14] | ((uint16_t)inOutPacket16[15] << 8);
	if (pktSum != sum)
		return (int)0x800f0027u;

	pktCounter = (uint32_t)inOutPacket16[4] |
		     ((uint32_t)inOutPacket16[5] << 8) |
		     ((uint32_t)inOutPacket16[6] << 16) |
		     ((uint32_t)inOutPacket16[7] << 24);
	if (pktCounter != *inOutCounter)
		return (int)0x800f0027u;

	*inOutCounter = pktCounter;
	pktByteOff = (uint16_t)inOutPacket16[0] | ((uint16_t)inOutPacket16[1] << 8);
	*outSectorIdx = (uint32_t)(pktByteOff >> 5);
	return status;
}

/* 0x80c872 (name ours) */
int qafSnvsEncryptSectors(const QafSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    src == NULL || dst == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = qafFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (qafXtsEncryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = (int)0x800f0027u;
		}
	}
	return status;
}

/* 0x80c908 (name ours) */
int qafXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst)
{
	uint32_t bmCtrl[2];
	uint8_t tweak[16];
	uint8_t scratch[16];
	uint32_t offset = 0u;
	uint32_t i;

	if (sectorBytes < 16u)
		return -1;

	bigmacInitChannel0(bmCtrl);
	if (keyBits == 128u) {
		s_qafXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_qafXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_qafXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_qafXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_qafXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_qafXtsEcbEncryptFn(dst + offset, dst + offset,
				16u, dataKey, bmCtrl) != 0) {
			return -1;
		}
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset + i] ^= tweak[i];

		for (i = 0u; i < 16u; i++) {
			uint8_t nextCarry = tweak[i] >> 7;

			tweak[i] = (uint8_t)((tweak[i] << 1) + carry);
			carry = nextCarry;
		}
		if (carry != 0u)
			tweak[0] ^= 0x87u;

		offset += 16u;
	}

	if (offset < sectorBytes) {
		uint32_t rem = sectorBytes - offset;

		for (i = 0u; i < rem; i++) {
			scratch[i] = src[offset + i] ^ tweak[i];
			dst[offset + i] = dst[offset - 16u + i];
		}
		for (; i < 16u; i++)
			scratch[i] = dst[offset - 16u + i] ^ tweak[i];

		if (s_qafXtsEcbEncryptFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ tweak[i];
	}

	return 0;
}

/* 0x80caf4 (name ours) */
int qafXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst)
{
	uint8_t curTweak[16];
	uint32_t sectorBytes = wordsPerSector << 4;
	uint32_t numSectors = (totalBytes / sectorBytes) +
			      ((totalBytes % sectorBytes) != 0u ? 1u : 0u);
	uint32_t remBytes = totalBytes;
	uint32_t offset = 0u;
	uint32_t i;

	memcpy(curTweak, tweakIv, 16u);
	for (i = 0u; i < numSectors; i++) {
		uint32_t chunk = (i == numSectors - 1u) ? remBytes : sectorBytes;

		qafXtsEncryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		qafIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* 0x80cbbc (name ours) */
int qafSnvsCipherNoop2(void)
{
	return 0;
}

/* ============================================================================
 * QA Token & SNVS Record Crypto Primitives (0x80cbc0..0x80d0e0)
 * ============================================================================ */

/* 0x80cbc0 (name ours) */
int qafDecryptQaToken(const uint8_t *inToken128, uint8_t *outToken128,
		const void *aesKey32, const void *aesIv16)
{
	uint8_t iv[16];
	uint32_t bmCtrl[2];
	int status;

	memcpy(iv, aesIv16, 16u);
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAes256CbcDecrypt(outToken128 + 0x20u,
			inToken128 + 0x20u, 96u, aesKey32, iv, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0)
			memcpy(outToken128, inToken128, 32u);
	}
	return status;
}

/* 0x80cc3e (name ours) */
int qafVerifyQaTokenCmac(uint8_t *inOutToken128, const void *cmacKey32)
{
	uint8_t savedMac[16];
	uint8_t calcMac[16];
	uint32_t bmCtrl[2];
	int status;

	memcpy(savedMac, inOutToken128 + 0x70u, 16u);
	memset(inOutToken128 + 0x70u, 0, 16u);
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, inOutToken128, 128u,
			cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0 && timingsafeBcmp(calcMac, savedMac, 16u) != 0)
			status = SCE_SBL_ERROR_QAF_BAD_MAC;
	}
	return status;
}

/* 0x80ccd0 (name ours) */
int qafVerifyQaTokenRsaSignature(const void *token128, const void *sig256,
		const BignumPublicKey *pubKey)
{
	uint8_t workBuf[256];
	uint8_t digest[32];

	memset(workBuf, 0, sizeof(workBuf));
	if (bigmacSha256(digest, token128, 128u) != 0)
		return (int)0x800f0027u;
	if (bignumVerifyRsaPkcs1v15(workBuf, sig256, pubKey, digest, 4u) < 1)
		return (int)0x800f0027u;
	return 0;
}

/* 0x80cd56 (name ours) */
int qafVerifySnvsRecordCmac(const uint8_t *rec32, const void *cmacKey32)
{
	uint8_t calcMac[16];
	uint32_t bmCtrl[2];
	int status;

	memset(calcMac, 0xaa, sizeof(calcMac));
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, rec32, 16u, cmacKey32, 32u, bmCtrl);
	if (status != 0)
		return status;
	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;
	return (timingsafeBcmp(rec32 + 16u, calcMac, 16u) == 0);
}

/* 0x80cdcc (name ours) */
int qafCompareByte5WithByte4(const uint8_t *a, const uint8_t *b)
{
	return (a[5] == b[4]);
}

/* 0x80cdda (name ours) */
int qafVerifyTokenConsoleId(const uint8_t *token128, const uint8_t *cid16)
{
	if (timingsafeBcmp(token128 + 0x20u, cid16, 16u) != 0)
		return SCE_SBL_ERROR_QAF_CID_MISMATCH;
	return 0;
}

/* 0x80cdfe (name ours) */
int qafFormatConsoleIdForToken(uint8_t *outCid16, const uint8_t *inCid16)
{
	return aimgrGetConsoleId(inCid16, outCid16);
}

/* 0x80cee4 (name ours) */
int qafDecodeInvertedBytes(uint8_t *dst, const uint8_t *src, uint32_t len)
{
	uint32_t i;

	for (i = 0u; i < len; i++)
		dst[i] = (uint8_t)~src[i];
	return 1;
}

/* 0x80cefe (name ours) */
int qafEncryptDefaultQaToken(uint8_t *inOutToken128, const void *aesKey32,
		const void *aesIv16)
{
	uint8_t iv[16];
	uint32_t bmCtrl[2];
	uint32_t i;
	int status;

	for (i = 0u; i < 4u; i++) {
		if (inOutToken128[i] != (uint8_t)~s_qafInvMagicQafv[i])
			return SCE_SBL_ERROR_QAF_BAD_MAGIC;
	}

	memcpy(iv, aesIv16, 16u);
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAes256CbcEncrypt(inOutToken128 + 0x20u,
			inOutToken128 + 0x20u, 96u, aesKey32, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80cf92 (name ours) */
int qafBuildDefaultNoFlagsToken(uint8_t *outToken128, const void *cmacKey32,
		const uint8_t *cid16)
{
	uint8_t formattedCid[16];
	uint8_t calcMac[16];
	uint32_t bmCtrl[2];
	int status;

	memset(outToken128, 0, 128u);
	qafDecodeInvertedBytes(outToken128, s_qafInvMagicQafv, 4u);
	outToken128[5] = 0u;
	*(uint16_t *)(void *)(outToken128 + 6u) = 1u;
	qafDecodeInvertedBytes(outToken128 + 8u, s_qafInvMagicQafNoFlags, 9u);
	qafFormatConsoleIdForToken(formattedCid, cid16);
	memcpy(outToken128 + 0x20u, formattedCid, 16u);

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, outToken128, 128u,
			cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0)
			memcpy(outToken128 + 0x70u, calcMac, 16u);
	}
	return status;
}

/* 0x80d048 (name ours) */
int qafBuildDefaultSnvsRecord(uint8_t *outRec32, uint8_t flagByte,
		const void *cmacKey32)
{
	uint8_t calcMac[16];
	uint32_t bmCtrl[2];
	int status;

	memset(outRec32, 0xff, 32u);
	qafDecodeInvertedBytes(outRec32, s_qafInvMagicSnvs, 4u);
	outRec32[4] = flagByte;

	memset(calcMac, 0xaa, sizeof(calcMac));
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, outRec32, 16u,
			cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0)
			memcpy(outRec32 + 0x10u, calcMac, 16u);
	}
	return status;
}
