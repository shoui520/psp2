/*
 * pm_sm.c: PlayStation Vita Product Mode Secure Module (pm_sm.self)
 *
 * Implements the 10-command Product Mode / Assist Mode service:
 *   - Command 1: Read Product Mode flag from SNVS sector 0
 *   - Command 2: Modify Product Mode flag (0x04 enable, 0x05/0x07 disable)
 *   - Command 3: Initialize Auth1 header for key slot 6 or 14
 *   - Command 4: Build AES-128-CBC Auth1 challenge with TRNG nonce
 *   - Command 5: Verify Auth1 response and derive 16-byte session key
 *   - Command 6: Build Auth2 challenge (subcmd 4 or 6)
 *   - Command 7: Verify Auth2 response (establishes sub-session or authorizes PM)
 *   - Command 8: SNVS sector 0 challenge/response/commit service (ops 0..7)
 *   - Command 9: Build RSA-2048 challenge (subcmd 0x48 or 0x49)
 *   - Command 10: Verify RSA-2048 response (authorizes PM on subcmd 0x48)
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "pm_sm.h"

/* ============================================================================
 * Read-Only Tables & Constants (0x80ed90..0x80f188)
 * ============================================================================ */

/* 0x80ed90: Simple hook vtable */
static const void * const s_pmSimpleHookVtable[4] = {
	NULL,
	NULL,
	(const void *)pmSimpleHookInvoke,
	NULL,
};

/* 0x80edb0: Context hook vtable */
static const void * const s_pmCtxHookVtable[2] = {
	(const void *)pmCtxHookInvoke,
	NULL,
};

/* 0x80edd8: 16-byte zero IV */
static const uint8_t s_pmZeroIvAuth[16] = { 0 };

/* 0x80ede8: Key entry for slot 6 (64 bytes) */
static uint8_t s_pmKeyEntry6[64] = {
	0x10u, 0x53u, 0x14u, 0x3bu, 0xeau, 0xecu, 0xc5u, 0x9fu,
	0xcfu, 0xf1u, 0xa1u, 0x95u, 0xf8u, 0xf5u, 0xafu, 0xb0u,
	0xf9u, 0x0cu, 0xdcu, 0xbfu, 0x00u, 0x9bu, 0xa8u, 0x36u,
	0x7fu, 0x84u, 0x1bu, 0x25u, 0xe8u, 0xb1u, 0x03u, 0x06u,
	0x6fu, 0x63u, 0x74u, 0xfdu, 0x1au, 0x41u, 0xa7u, 0x52u,
	0x69u, 0xeeu, 0x15u, 0x83u, 0x24u, 0x51u, 0xdeu, 0xbfu,
	0xb6u, 0x80u, 0x6fu, 0x9fu, 0x58u, 0x70u, 0x6du, 0x72u,
	0xb0u, 0xe0u, 0x37u, 0x17u, 0x19u, 0x7du, 0x43u, 0x0cu,
};

/* 0x80ee28: Key entry for slot 14 (64 bytes, differs between DEX and TOOL/CEX) */
#if FW_DEBUG
static uint8_t s_pmKeyEntry14[64] = {
	0x2au, 0x60u, 0x62u, 0x55u, 0x32u, 0x84u, 0x38u, 0xcfu,
	0xebu, 0xf8u, 0x78u, 0x1eu, 0x4du, 0x55u, 0xcbu, 0x92u,
	0xbcu, 0x97u, 0xf9u, 0x7eu, 0xe0u, 0x37u, 0xebu, 0xfcu,
	0x0au, 0xe5u, 0x52u, 0x5eu, 0x57u, 0xbdu, 0x9cu, 0x35u,
	0xccu, 0xaeu, 0xffu, 0x36u, 0x23u, 0xceu, 0x97u, 0x1du,
	0x2cu, 0x8au, 0xbcu, 0xccu, 0x7au, 0xbdu, 0xbdu, 0xcfu,
	0xc7u, 0xecu, 0x37u, 0xb3u, 0x9au, 0x66u, 0x64u, 0x19u,
	0x4eu, 0xe3u, 0x99u, 0xfeu, 0x6eu, 0x86u, 0x4au, 0x30u,
};
#else
static uint8_t s_pmKeyEntry14[64] = {
	0xe5u, 0x53u, 0x64u, 0x61u, 0x6cu, 0xa8u, 0xc3u, 0xddu,
	0x28u, 0x59u, 0xb1u, 0xd9u, 0x13u, 0xe1u, 0xccu, 0x15u,
	0x02u, 0xdau, 0x87u, 0x0fu, 0xb5u, 0x98u, 0x3du, 0xe5u,
	0x68u, 0xe3u, 0x24u, 0xddu, 0xf5u, 0x33u, 0x0cu, 0x9du,
	0xd6u, 0x06u, 0xa9u, 0x9eu, 0x38u, 0x1au, 0x4bu, 0x44u,
	0x0du, 0xb9u, 0xa8u, 0xa8u, 0x7fu, 0x60u, 0x8fu, 0x1bu,
	0x80u, 0x82u, 0x30u, 0x7du, 0xa2u, 0x58u, 0x78u, 0x0du,
	0x50u, 0x88u, 0xc4u, 0x02u, 0x18u, 0xb9u, 0x8eu, 0xfcu,
};
#endif

/* 0x80ee68: 16-byte zero IV for SNVS */
static const uint8_t s_pmZeroIv16[16] = { 0 };

/* 0x80ee78: PmSnvsCipher vtable */
static const void * const s_pmSnvsCipherVtable[4] = {
	NULL,
	NULL,
	(const void *)pmSnvsCipherDtorD2,
	(const void *)pmSnvsCipherDtorD0,
};

/* 0x80ee88: Bitwise-inverted "SNVS" magic */
static const uint8_t s_pmInvMagicSnvs[4] = {
	0xacu, 0xb1u, 0xa9u, 0xacu,
};

/* 0x80eedc: Subcmd 6 constant for slot 6 */
static const uint8_t s_pmSubcmd6Const6[16] = {
	0x0eu, 0x08u, 0xa2u, 0x0cu, 0x87u, 0x18u, 0xbdu, 0x3bu,
	0x15u, 0x8eu, 0x2eu, 0x69u, 0x92u, 0x20u, 0x2du, 0xe7u,
};

/* 0x80eeec: Subcmd 6 constant for slot 14 */
#if FW_DEBUG
static const uint8_t s_pmSubcmd6Const14[16] = {
	0xc0u, 0x6bu, 0x00u, 0xc9u, 0x5bu, 0xe0u, 0x46u, 0x18u,
	0x59u, 0xb4u, 0x0fu, 0xdbu, 0xfau, 0x63u, 0xddu, 0x07u,
};
#else
static const uint8_t s_pmSubcmd6Const14[16] = {
	0xd1u, 0x4au, 0x64u, 0xdfu, 0x40u, 0x11u, 0xe1u, 0x10u,
	0x32u, 0x5eu, 0x15u, 0x96u, 0x02u, 0xf2u, 0x3bu, 0xa7u,
};
#endif

/* 0x80ee8c: Subcmd 6 constant lookup table */
static const uint8_t * const s_pmSubcmd6Table[16] = {
	[6] = s_pmSubcmd6Const6,
	[14] = s_pmSubcmd6Const14,
};

/* 0x80eecc: 16-byte zero IV for subcmd 4/6 */
static const uint8_t s_pmZeroIvSubcmd[16] = { 0 };

/* ============================================================================
 * Mutable Data & BSS Globals (0x80ef1c..0x811480)
 * ============================================================================ */

/* 0x80ef1c: 16-byte sub-session key established by subcmd 6 */
static uint8_t s_pmSessionKey[16] = { 0 };

/* 0x80ef2c: 16-byte RSA challenge nonce */
static uint8_t s_pmRsaNonce[16] = { 0 };

/* 0x80ef3c: Non-zero once subcmd 6 establishes s_pmSessionKey */
static uint32_t s_pmSessionEstablished = 0u;

/* 0x80ef40: Non-zero once Product Mode enable (mode 4) is authorized */
static uint32_t s_pmAuthAuthorized = 0u;

/* 0x80ef44: RSA-2048 modulus for subcmd 0x48 (72) */
static const uint32_t s_pmRsaModulus72[64] = {
	0x7246a18fu, 0x3585520cu, 0xa5e29805u, 0x7c1d55a8u,
	0x8566fa10u, 0x6ba9abfeu, 0x8e2213fau, 0xce12a70bu,
	0x4179869au, 0x7ef88f02u, 0x34ea59aau, 0xdcf6ff5du,
	0xc2c9f890u, 0x6f62b158u, 0x4777d308u, 0xfc533d79u,
	0x7ae24a57u, 0x34cc21eau, 0x889f23dfu, 0xcf279de3u,
	0xc2861f42u, 0x302a0d3cu, 0x60e5b3deu, 0x4b00670bu,
	0x468687bfu, 0x663f3141u, 0x66c44c99u, 0xff09120bu,
	0x104f97b4u, 0xbbf55f7eu, 0x94eb38b4u, 0x065bcf80u,
	0xdf750e1eu, 0x4d333956u, 0x91f24c2bu, 0x40885835u,
	0xfedb3e45u, 0xea3570b0u, 0x4eb093a5u, 0x6c855bccu,
	0x4553046au, 0x43485abbu, 0xfdcdcb3fu, 0x5090c161u,
	0x7ffb1907u, 0x8f2f13b3u, 0x514322f2u, 0x4e43e63eu,
	0x7ae62ec4u, 0x8f6bf1fdu, 0x3c936585u, 0x3c3d6d66u,
	0x812327fcu, 0x724c46c6u, 0xf7df11d2u, 0xf8873611u,
	0xa30955f3u, 0xfb335e0cu, 0x4879abcdu, 0x34c2cd99u,
	0xfc214bd7u, 0xd6e4cdc8u, 0x73e5728cu, 0x632e2cd1u,
};

/* 0x80f044: RSA-2048 exponent for subcmd 0x48 (72) */
static const uint32_t s_pmRsaExp72[1] = {
	0x01000100u,
};

/* 0x80f048: RSA-2048 modulus for subcmd 0x49 (73) */
static const uint32_t s_pmRsaModulus73[64] = {
	0xe550c589u, 0x913e2475u, 0x66aedbadu, 0x9711f289u,
	0xb525ce6fu, 0x27aa9f13u, 0x96d38bfcu, 0x95302fc7u,
	0xdfced155u, 0x38dd5646u, 0xdb1c3491u, 0xb1920d38u,
	0x398cce52u, 0x32b050fau, 0xcc44dd7cu, 0x0186b98du,
	0x7b293eaeu, 0x010952f5u, 0x793cf3ccu, 0xaaf5786cu,
	0x362fc9a9u, 0xd7458b45u, 0x6df67e22u, 0xfe4fd74eu,
	0xd5ace091u, 0xcf846e31u, 0x50faf158u, 0x20783022u,
	0xb8d35882u, 0x302ebd9fu, 0x99087607u, 0x14990429u,
	0x51d37f8eu, 0xd631460du, 0x75ae0d56u, 0x3862551cu,
	0xbf28008eu, 0xea6473d1u, 0xf43ffd29u, 0x90c52b75u,
	0x46fd0fa8u, 0xb8411f81u, 0x1a477cafu, 0xe02827ddu,
	0xa1c98c53u, 0x602f049au, 0xa2b5e86cu, 0x85b3be1cu,
	0x6a8b27f6u, 0x96eb98cau, 0x05fb49b9u, 0xf47448acu,
	0x6bc4f8a2u, 0x80d03d07u, 0x6ece1694u, 0xcc351ae1u,
	0x2fdb3850u, 0x84dfca91u, 0xe9808355u, 0x9f3444d6u,
	0x61fb7e19u, 0xe1ac1a7du, 0x8f5f89e3u, 0x150b3b5eu,
};

/* 0x80f148: RSA-2048 exponent for subcmd 0x49 (73) */
static const uint32_t s_pmRsaExp73[1] = {
	0x01000100u,
};

/* 0x80f14c: Key slot pointer table */
static uint8_t *s_pmKeyTable[16] = {
	[6] = s_pmKeyEntry6,
	[14] = s_pmKeyEntry14,
};

/* 0x80f18c: XTS AES-ECB function pointers */
typedef int (*PmAesEcbFn)(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
static PmAesEcbFn s_pmXtsEcbEncryptFn;
static PmAesEcbFn s_pmXtsEcbSecondFn;

/* 0x80f1a8: GP-relative zero IV pointers */
static const uint8_t *s_pmZeroIvPtr1 = s_pmZeroIvSubcmd;
static const uint8_t *s_pmZeroIvPtr2 = s_pmZeroIvSubcmd;

/* 0x811000: SNVS scratch buffer (992 bytes) */
static uint8_t s_pmSnvsScratch[992];

/* 0x811400: SNVS cipher instance and keyring buffers */
static PmSnvsCipher s_pmSnvsCipher;
static uint32_t s_pmSnvsInitialized;
static uint8_t s_pmSnvsKey1[32];
static uint8_t s_pmSnvsKey2[32];
static uint8_t s_pmSnvsKey3[32];

static void (* const s_pmInitArray[])(void)
	__attribute__((section(".ctors"), used)) = { pmStaticInitThunk };
static void (* const s_pmFiniArray[])(void)
	__attribute__((section(".dtors"), used)) = { pmStaticDtorThunk };

/* ============================================================================
 * Entry Point & Command Registration (0x80b09c..0x80b3d9)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	PmAuthContext authCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *hookNode;
	uint8_t dummyTarget;
	int status;

	smHeapInit((void *)0x00810000u, 0x1000u);
	pmAuthCtxInitC1(&authCtx);
	smSrvInit(&srvCtx);

	fnPtr.fn = pmCmd1GetProductMode;
	fnPtr.adj = 0;
	if (pmRegisterSimpleHook(&srvCtx, SCE_SBL_PM_CMD_GET_FLAG,
			&dummyTarget, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd2SetProductMode;
	fnPtr.adj = 0;
	if (pmRegisterSimpleHook(&srvCtx, SCE_SBL_PM_CMD_SET_FLAG,
			&dummyTarget, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd3Auth1Init;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_AUTH1_INIT,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd4Auth1Challenge;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_AUTH1_CHALLENGE,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd5Auth1Verify;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_AUTH1_VERIFY,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd6Auth2Challenge;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_AUTH2_CHALLENGE,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd7Auth2Verify;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_AUTH2_VERIFY,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd8SnvsService;
	fnPtr.adj = 0;
	if (pmRegisterSimpleHook(&srvCtx, SCE_SBL_PM_CMD_SNVS_SERVICE,
			&dummyTarget, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd9RsaChallenge;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_RSA_CHALLENGE,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	fnPtr.fn = pmCmd10RsaVerify;
	fnPtr.adj = 0;
	if (pmRegisterCtxHook(&srvCtx, SCE_SBL_PM_CMD_RSA_VERIFY,
			&authCtx, &fnPtr, &hookNode) != 0) {
		status = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		goto cleanup;
	}

	status = smSrvRunLoop(&srvCtx);
	if (status == 0)
		status = 0x55555555;

cleanup:
	smSrvTeardown(&srvCtx);
	pmAuthCtxDtorD1(&authCtx);
	return status;
}

/* 0x80b27e (name ours) */
void pmSimpleHookInvoke(void *self, void *commBuf)
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

/* 0x80b2b2 (name ours) */
void pmCtxHookInvoke(void *self, void *commBuf)
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

/* 0x80b2e6 (name ours) */
int pmRegisterSimpleHook(SmServiceContext *srvCtx, uint32_t cmdId,
		void *targetObj, const SmMemberFnPtr *fnPtr,
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
		node->vtable = &s_pmSimpleHookVtable[2];
		node->handler = targetObj;
		node->arg0 = (void *)fnPtr->fn;
		node->adj = fnPtr->adj;
	}

	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b360 (name ours) */
int pmRegisterCtxHook(SmServiceContext *srvCtx, uint32_t cmdId,
		PmAuthContext *authCtx, const SmMemberFnPtr *fnPtr,
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
		node->vtable = &s_pmCtxHookVtable[0];
		node->handler = authCtx;
		node->arg0 = (void *)fnPtr->fn;
		node->adj = fnPtr->adj;
	}

	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* ============================================================================
 * Mailbox Command Handlers (0x80b3da..0x80ba6b)
 * ============================================================================ */

/* 0x80b3da (name ours; Command 1) */
int pmCmd1GetProductMode(void *targetObj, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t word0[4];
	uint8_t word1[4];
	int status;

	(void)targetObj;
	if (pmEnsureSnvsCryptoInitialized() != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		return SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	}

	status = pmReadSnvsSector0(word0, word1, pkt + 0x48u);
	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
		return status;
	}

	*(int *)(void *)(pkt + 8u) = 0;
	*(uint32_t *)(void *)(pkt + 0x40u) = (uint32_t)word0[0];
	return 0;
}

/* 0x80b424 (name ours; Command 2) */
int pmCmd2SetProductMode(void *targetObj, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t mode = pkt[0x40u];
	uint8_t word0[4];
	uint8_t word1[4];
	int status;

	(void)targetObj;
	if (s_pmAuthAuthorized == 0u && mode == 4u) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
		return SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
	}

	if (pmEnsureSnvsCryptoInitialized() != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		return SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	}

	if (pmReadSnvsSector0(word0, word1, pkt + 0x48u) != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
		return SCE_SBL_ERROR_PM_VERIFY_FAILED;
	}

	if (mode == 5u) {
		word0[0] &= (uint8_t)~1u;
	} else if (mode == 7u) {
		word0[0] &= (uint8_t)~2u;
	} else if (mode == 4u) {
		word0[0] |= 1u;
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
		return SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	if (pmEnsureSnvsCryptoInitialized() != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		return SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	}

	status = pmWriteSnvsSector0(word0, word1, pkt + 0x48u);
	if (status != 0)
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	else
		*(int *)(void *)(pkt + 8u) = 0;

	return status;
}

/* 0x80b4d2 (name ours; Command 8) */
int pmCmd8SnvsService(void *targetObj, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t *payload = pkt + 0x40u;
	uint32_t counter = 0u;
	uint32_t sectorIdx = 0u;
	uint8_t sectorBuf[32];
	uint32_t subCmd;
	int status;

	(void)targetObj;
	if (pmEnsureSnvsCryptoInitialized() != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		return SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	}

	((uint8_t *)(void *)&counter)[0] = REG8(0xe0062240u);
	((uint8_t *)(void *)&counter)[1] = REG8(0xe0062241u);
	((uint8_t *)(void *)&counter)[2] = REG8(0xe0062242u);
	((uint8_t *)(void *)&counter)[3] = REG8(0xe0062243u);
	memset(s_pmSnvsScratch, 0, sizeof(s_pmSnvsScratch));

	subCmd = *(const uint32_t *)(const void *)payload;
	switch (subCmd) {
	case 0u:
		status = pmBuildSnvsMacChallenge(counter, 0u, payload + 0x10u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		break;

	case 1u:
		status = pmVerifyAndUnwrapSnvsSectorWrite(&counter, &sectorIdx,
				sectorBuf, 32u, payload + 0x40u);
		counter = pmIncrementSnvsCounter(counter);
		memset(payload + 0x40u, 0, 48u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		if (sectorIdx != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
			return SCE_SBL_ERROR_PM_INVALID_ARG;
		}
		status = pmReadSnvsSector0(payload + 8u, payload + 12u, sectorBuf);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		break;

	case 4u:
	case 5u:
	case 7u: {
		uint8_t mode = (uint8_t)subCmd;

		if (s_pmAuthAuthorized == 0u && mode == 4u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
			return SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
		}
		if (mode == 5u)
			payload[8] &= (uint8_t)~1u;
		else if (mode == 7u)
			payload[8] &= (uint8_t)~2u;
		else if (mode == 4u)
			payload[8] |= 1u;
		else {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
			return SCE_SBL_ERROR_PM_INVALID_ARG;
		}

		status = pmWriteSnvsSector0(payload + 8u, payload + 12u, sectorBuf);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		status = pmWrapSnvsSectorReadResponse(counter, 0u, sectorBuf,
				32u, payload + 0x10u);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		break;
	}

	case 3u:
		status = pmVerifySnvsCommitPacket(&counter, &sectorIdx,
				payload + 0x40u);
		counter = pmIncrementSnvsCounter(counter);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
			return status;
		}
		memset(payload + 0x40u, 0, 48u);
		if (sectorIdx != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
			return SCE_SBL_ERROR_PM_INVALID_ARG;
		}
		break;

	default:
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
		return SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	*(int *)(void *)(pkt + 8u) = 0;
	return 0;
}

/* 0x80b67a (name ours) */
void pmAuthCtxInitC2(PmAuthContext *ctx)
{
	ctx->state = 0u;
}

/* 0x80b680 (name ours) */
void pmAuthCtxInitC1(PmAuthContext *ctx)
{
	ctx->state = 0u;
}

/* 0x80b686 (name ours) */
void pmAuthCtxDtorD2(PmAuthContext *ctx)
{
	(void)ctx;
}

/* 0x80b688 (name ours) */
void pmAuthCtxDtorD1(PmAuthContext *ctx)
{
	(void)ctx;
}

/* 0x80b68a (name ours; Command 3) */
int pmCmd3Auth1Init(void *ctxPtr, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t keyId = *(const uint32_t *)(const void *)(pkt + 0x40u);
	int status;

	(void)ctxPtr;
	memset(pkt + 0x48u, 0, 40u);
	status = pmBuildAuth1InitHeader(pkt + 0x48u, keyId);
	if (status != 0)
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	else
		*(int *)(void *)(pkt + 8u) = 0;

	return status;
}

/* 0x80b6d2 (name ours; Command 4) */
int pmCmd4Auth1Challenge(void *ctxPtr, void *commBuf)
{
	PmAuthContext *ctx = (PmAuthContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t outBuf[40];
	uint32_t keyId;
	uint32_t variant;
	int status;

	memset(outBuf, 0, sizeof(outBuf));
	keyId = *(const uint32_t *)(const void *)(pkt + 0x40u);
	variant = *(const uint32_t *)(const void *)(pkt + 0x44u);
	memcpy(ctx->clientNonce, pkt + 0x50u, 8u);

	status = pmGetRandomBytes(ctx->serverNonce, 8u);
	if (status == 0) {
		status = pmBuildAuth1Challenge(outBuf, pkt + 0x48u,
				keyId, variant, ctx->serverNonce);
		if (status == 0) {
			memcpy(pkt + 0x48u, outBuf, 40u);
			*(int *)(void *)(pkt + 8u) = 0;
			return 0;
		}
	}

	*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	return status;
}

/* 0x80b772 (name ours; Command 5) */
int pmCmd5Auth1Verify(void *ctxPtr, void *commBuf)
{
	PmAuthContext *ctx = (PmAuthContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t keyId = *(const uint32_t *)(const void *)(pkt + 0x40u);
	uint32_t variant = *(const uint32_t *)(const void *)(pkt + 0x44u);
	int status;

	status = pmVerifyAuth1Response(pkt + 0x48u, keyId, variant,
			ctx->clientNonce, ctx->serverNonce, ctx->sessionKey);
	if (status != 0)
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
	else
		*(int *)(void *)(pkt + 8u) = 0;

	return status;
}

/* 0x80b7b8 (name ours; Command 6) */
int pmCmd6Auth2Challenge(void *ctxPtr, void *commBuf)
{
	PmAuthContext *ctx = (PmAuthContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t outBuf[40];
	uint32_t subcmd;
	uint32_t variant;
	int status;

	memset(outBuf, 0, sizeof(outBuf));
	subcmd = *(const uint32_t *)(const void *)(pkt + 0x40u);
	variant = *(const uint32_t *)(const void *)(pkt + 0x44u);

	if (subcmd == 4u) {
		if (s_pmSessionEstablished == 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
			return SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
		}
		status = pmBuildSubcmd4Challenge(outBuf, s_pmSessionKey,
				ctx->sessionKey);
	} else if (subcmd == 6u) {
		status = pmBuildSubcmd6Challenge(outBuf, variant, ctx->sessionKey);
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
		return SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
		return status;
	}

	memcpy(pkt + 0x48u, outBuf, 40u);
	*(int *)(void *)(pkt + 8u) = 0;
	return 0;
}

/* 0x80b860 (name ours; Command 7) */
int pmCmd7Auth2Verify(void *ctxPtr, void *commBuf)
{
	PmAuthContext *ctx = (PmAuthContext *)ctxPtr;
	uint8_t *pkt = (uint8_t *)commBuf;
	uint8_t payload[16];
	uint32_t subcmd = *(const uint32_t *)(const void *)(pkt + 0x40u);
	uint32_t variant = *(const uint32_t *)(const void *)(pkt + 0x44u);
	int status;

	if (subcmd == 4u) {
		status = pmVerifySubcmd4Response(pkt + 0x48u, ctx->sessionKey,
				payload);
		if (status != 0) {
			*(int *)(void *)(pkt + 8u) = status;
		} else if (payload[0] == 0u) {
			s_pmAuthAuthorized = 1u;
			*(int *)(void *)(pkt + 8u) = 0;
		} else {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
		}
	} else if (subcmd == 6u) {
		status = pmVerifySubcmd6Response(pkt + 0x48u, variant,
				ctx->sessionKey, payload);
		if (status == 0) {
			memcpy(s_pmSessionKey, payload, 16u);
			s_pmSessionEstablished = 1u;
		}
		*(int *)(void *)(pkt + 8u) = status;
	} else {
		status = SCE_SBL_ERROR_PM_INVALID_ARG;
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	return status;
}

/* 0x80b902 (name ours; Command 9) */
int pmCmd9RsaChallenge(void *ctxPtr, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t subcmd = *(const uint32_t *)(const void *)(pkt + 0x40u);
	BignumPublicKey rsaKey;
	int status;

	(void)ctxPtr;
	if (subcmd == 72u && s_pmSessionEstablished == 0u) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
		return SCE_SBL_ERROR_PM_NOT_AUTHORIZED;
	}

	memset(pkt + 0x48u, 0, 260u);
	if (subcmd == 72u) {
		memcpy(pkt + 0x11cu, s_pmSessionKey, 16u);
		rsaKey.modulus = s_pmRsaModulus72;
		rsaKey.modulusWords = 64u;
		rsaKey.exponent = s_pmRsaExp72;
		rsaKey.exponentWords = 1u;
	} else if (subcmd == 73u) {
		rsaKey.modulus = s_pmRsaModulus73;
		rsaKey.modulusWords = 64u;
		rsaKey.exponent = s_pmRsaExp73;
		rsaKey.exponentWords = 1u;
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
		return SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	status = pmBuildRsaChallenge((uint8_t)subcmd, pkt + 0x48u,
			s_pmRsaNonce, &rsaKey);
	if (status != 0)
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_CRYPTO_FAILED;
	else
		*(int *)(void *)(pkt + 8u) = 0;

	return status;
}

/* 0x80b9bc (name ours; Command 10) */
int pmCmd10RsaVerify(void *ctxPtr, void *commBuf)
{
	uint8_t *pkt = (uint8_t *)commBuf;
	uint32_t subcmd = *(const uint32_t *)(const void *)(pkt + 0x40u);
	BignumPublicKey rsaKey;
	uint8_t payload[16];
	int status;

	(void)ctxPtr;
	if (subcmd == 72u) {
		rsaKey.modulus = s_pmRsaModulus72;
		rsaKey.modulusWords = 64u;
		rsaKey.exponent = s_pmRsaExp72;
		rsaKey.exponentWords = 1u;
	} else if (subcmd == 73u) {
		rsaKey.modulus = s_pmRsaModulus73;
		rsaKey.modulusWords = 64u;
		rsaKey.exponent = s_pmRsaExp73;
		rsaKey.exponentWords = 1u;
	} else {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_INVALID_ARG;
		return SCE_SBL_ERROR_PM_INVALID_ARG;
	}

	status = pmVerifyRsaResponse((uint8_t)subcmd, pkt + 0x48u,
			s_pmRsaNonce, payload, &rsaKey);
	if (status != 0) {
		*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
		return status;
	}

	if (subcmd == 72u) {
		if (payload[0] != 0u) {
			*(int *)(void *)(pkt + 8u) = SCE_SBL_ERROR_PM_VERIFY_FAILED;
		} else {
			s_pmAuthAuthorized = 1u;
			*(int *)(void *)(pkt + 8u) = 0;
		}
	} else if (subcmd == 73u) {
		*(int *)(void *)(pkt + 8u) = 0;
	}

	return status;
}

/* ============================================================================
 * Key Slot Validation & SNVS Sector Engine (0x80ba6c..0x80cb8b)
 * ============================================================================ */

/* 0x80ba6c (name ours) */
int pmCheckKeyId(uint32_t keyId)
{
	if (keyId != 6u && keyId != 14u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (s_pmKeyTable[keyId] == NULL)
		return (int)0x800f0028u;
	return 0;
}

/* 0x80ba96 (name ours) */
void pmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	volatile const uint32_t *slot =
		(volatile const uint32_t *)(0xe0058000u + (slotIndex << 5));

	outWords8[0] = slot[0];
	outWords8[1] = slot[1];
	outWords8[2] = slot[2];
	outWords8[3] = slot[3];
	outWords8[4] = slot[4];
	outWords8[5] = slot[5];
	outWords8[6] = slot[6];
	outWords8[7] = slot[7];
}

/* 0x80bae0 (name ours) */
void pmStaticInitOrDtor(int isInit)
{
	if (isInit == 1)
		pmSnvsCipherCtorC1(&s_pmSnvsCipher);
	else if (isInit == 0)
		pmSnvsCipherDtorD2(&s_pmSnvsCipher);
}

/* 0x80bb00 (name ours) */
int pmEnsureSnvsCryptoInitialized(void)
{
	int status = 0;

	if (s_pmSnvsInitialized == 0u) {
		memset(s_pmSnvsKey1, 0, 32u);
		memset(s_pmSnvsKey2, 0, 32u);
		memset(s_pmSnvsKey3, 0, 32u);
		pmReadKeyringSlot32(0x502u, (uint32_t *)(void *)s_pmSnvsKey1);
		pmReadKeyringSlot32(0x503u, (uint32_t *)(void *)s_pmSnvsKey2);
		pmReadKeyringSlot32(0x504u, (uint32_t *)(void *)s_pmSnvsKey3);
		status = pmSnvsCipherInit(&s_pmSnvsCipher,
				s_pmSnvsKey1, s_pmSnvsKey2, s_pmSnvsKey3,
				128u, 128u);
		if (status == 0)
			s_pmSnvsInitialized = 1u;
	}
	return status;
}

/* 0x80bb7c (name ours) */
int pmReadSnvsSector0(void *outWord0, void *outWord1, const void *inSector32)
{
	uint8_t decBuf[32];
	uint8_t macCheck[32];
	uint8_t savedMac[16];
	int status;

	if (s_pmSnvsInitialized == 0u || outWord0 == NULL ||
	    outWord1 == NULL || inSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(decBuf, 0, sizeof(decBuf));
	status = pmSnvsDecryptSectors(&s_pmSnvsCipher, inSector32,
			decBuf, 32u, 0u);
	if (status != 0)
		return status;

	memcpy(macCheck, decBuf, 32u);
	memset(savedMac, 0, sizeof(savedMac));
	status = pmSnvsSignRecordCmac(&s_pmSnvsCipher, macCheck);
	if (status != 0)
		return status;

	if (timingsafeBcmp(savedMac, decBuf + 16u, 16u) != 0)
		return (int)0x800f0027u;

	memcpy(outWord0, macCheck + 8u, 4u);
	memcpy(outWord1, macCheck + 12u, 4u);
	return 0;
}

/* 0x80bc2c (name ours) */
int pmValidateSectorRange(uint32_t sectorCount, uint32_t startSector)
{
	if (sectorCount - 1u > 30u || startSector > 31u)
		return 0;
	return (startSector + sectorCount < 32u);
}

/* 0x80bc48 (name ours) */
int pmDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (s_pmSnvsInitialized == 0u || dst == NULL || src == NULL ||
	    pmValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return pmSnvsDecryptSectors(&s_pmSnvsCipher, src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80bc9e (name ours) */
uint32_t pmIsSnvsInitialized(void)
{
	return s_pmSnvsInitialized;
}

/* 0x80bca4 (name ours) */
PmSnvsCipher *pmGetSnvsCipher(void)
{
	return &s_pmSnvsCipher;
}

/* 0x80bcaa (name ours) */
const uint8_t *pmGetZeroIv16(void)
{
	return s_pmZeroIv16;
}

/* 0x80bcb0 (name ours) */
int pmBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
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

	pmReadKeyringSlot32(0x516u, slot516);
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

	pmReadKeyringSlot32(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_pmZeroIv16, 16u);
	status = bigmacStartAes128CbcEncrypt(outChallenge16, outChallenge16,
			16u, slot511, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80bdb2 (name ours) */
int pmVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
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

	pmReadKeyringSlot32(0x511u, slot511);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, s_pmZeroIv16, 16u);
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

/* 0x80bebe (name ours) */
uint32_t pmIncrementSnvsCounter(uint32_t counter)
{
	uint32_t nextCounter = counter + 1u;

	REG32(0xe0030000u) = nextCounter;
	REG32(0xe0030020u) = 0x512u;
	return nextCounter;
}

/* 0x80bef4 (name ours) */
void pmStaticInitThunk(void)
{
	pmStaticInitOrDtor(1);
}

/* 0x80befa (name ours) */
void pmStaticDtorThunk(void)
{
	pmStaticInitOrDtor(0);
}

/* 0x80bf00 (name ours) */
void pmSnvsCipherDtorD2(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
}

/* 0x80bf08 (name ours) */
void pmSnvsCipherDtorD0(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	operatorDelete(c);
}

/* 0x80bf12 (name ours) */
void pmSnvsCipherCtorC2(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80bf30 (name ours) */
void pmSnvsCipherCtorC1(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
	c->dataKey = NULL;
	c->tweakKey = NULL;
	c->cmacKey = NULL;
	c->dataKeyBits = 0u;
	c->cmacKeyBits = 0u;
}

/* 0x80bf4e (name ours) */
void pmSnvsCipherDtorD1(PmSnvsCipher *c)
{
	c->vptr = &s_pmSnvsCipherVtable[2];
}

/* 0x80bf56 (name ours) */
int pmSnvsCipherInit(PmSnvsCipher *c, const uint8_t *dataKey,
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

/* 0x80bfa8 (name ours) */
int pmSnvsSignRecordCmac(const PmSnvsCipher *c, uint8_t *inOutRec32)
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

/* 0x80c058 (name ours) */
int pmFormatSectorTweak(const PmSnvsCipher *c, uint32_t sectorIdx,
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

/* 0x80c0a4 (name ours) */
int pmSnvsDecryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    dst == NULL || src == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = pmFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (pmXtsDecryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = (int)0x800f0027u;
		}
	}
	return status;
}

/* 0x80c138 (name ours) */
void pmSnvsCipherNoop1(void)
{
}

/* 0x80c13a (name ours) */
int pmXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
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
		s_pmXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_pmXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_pmXtsEcbSecondFn(dst + offset, dst + offset,
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

		if (s_pmXtsEcbSecondFn(dst + offset - 16u, dst + offset - 16u,
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

		if (s_pmXtsEcbSecondFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ prevTweak[i];
	}

	return 0;
}

/* 0x80c3c4 (name ours) */
int pmIncrementTweak128(uint8_t *tweak, uint32_t len)
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

/* 0x80c3e8 (name ours) */
int pmXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
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

		pmXtsDecryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		pmIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* 0x80c4ae (name ours) */
void pmReadHardwareKey2220(uint32_t *outWords8)
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

/* 0x80c51e (name ours) */
int pmEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount)
{
	if (pmIsSnvsInitialized() == 0u || src == NULL || dst == NULL ||
	    pmValidateSectorRange(startSector, sectorCount) == 0) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}
	return pmSnvsEncryptSectors(pmGetSnvsCipher(), src, dst,
			sectorCount * 32u, startSector);
}

/* 0x80c57c (name ours) */
int pmWriteSnvsSector0(const void *inWord0, const void *inWord1,
		void *outSector32)
{
	uint8_t rec[32];
	uint8_t enc[32];
	uint32_t i;
	int status;

	if (pmIsSnvsInitialized() == 0u || inWord0 == NULL ||
	    inWord1 == NULL || outSector32 == NULL) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memset(rec, 0, sizeof(rec));
	memcpy(rec, s_pmInvMagicSnvs, 4u);
	for (i = 0u; i < 4u; i++)
		rec[i] = (uint8_t)~rec[i];
	rec[4] = 1u;
	memset(rec + 5u, 0, 3u);
	memcpy(rec + 8u, inWord0, 4u);
	memcpy(rec + 12u, inWord1, 4u);

	status = pmSnvsSignRecordCmac(pmGetSnvsCipher(), rec);
	if (status != 0)
		return status;

	memset(enc, 0, sizeof(enc));
	status = pmSnvsEncryptSectors(pmGetSnvsCipher(), rec, enc, 32u, 0u);
	if (status == 0)
		memcpy(outSector32, enc, 32u);
	return status;
}

/* 0x80c65a (name ours) */
int pmWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
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

	pmReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, pmGetZeroIv16(), 16u);
	status = bigmacStartAes128CbcEncrypt(outPacket48, outPacket48,
			48u, hwKey, iv, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80c758 (name ours) */
int pmVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
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

	pmReadHardwareKey2220(hwKey);
	bigmacInitChannel0(bmCtrl);
	memcpy(iv, pmGetZeroIv16(), 16u);
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

/* 0x80c842 (name ours) */
int pmSnvsEncryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector)
{
	uint64_t tweak[2];
	int status;

	if (c->dataKey == NULL || c->tweakKey == NULL ||
	    src == NULL || dst == NULL || (totalBytes & 0x0fu) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	status = pmFormatSectorTweak(c, startSector, tweak);
	if (status == 0) {
		if (pmXtsEncryptMultiSectors((const uint8_t *)(const void *)tweak,
				c->dataKey, c->tweakKey, c->dataKeyBits,
				totalBytes, 16u,
				(const uint8_t *)src, (uint8_t *)dst) != 0) {
			status = (int)0x800f0027u;
		}
	}
	return status;
}

/* 0x80c8d8 (name ours) */
int pmXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
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
		s_pmXtsEcbEncryptFn = bigmacStartAes128EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes128EcbDecrypt;
	} else if (keyBits == 256u) {
		s_pmXtsEcbEncryptFn = bigmacStartAes256EcbEncrypt;
		s_pmXtsEcbSecondFn = bigmacStartAes256EcbDecrypt;
	} else {
		return -1;
	}

	if (s_pmXtsEcbEncryptFn(tweak, tweakIv, 16u, tweakKey, bmCtrl) != 0)
		return -1;
	bigmacWaitChannelDone(bmCtrl);

	while (offset + 16u <= sectorBytes) {
		uint8_t carry = 0u;

		for (i = 0u; i < 16u; i++)
			dst[offset + i] = src[offset + i] ^ tweak[i];

		if (s_pmXtsEcbEncryptFn(dst + offset, dst + offset,
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

		if (s_pmXtsEcbEncryptFn(scratch, scratch, 16u, dataKey, bmCtrl) != 0)
			return -1;
		bigmacWaitChannelDone(bmCtrl);

		for (i = 0u; i < 16u; i++)
			dst[offset - 16u + i] = scratch[i] ^ tweak[i];
	}

	return 0;
}

/* 0x80cac4 (name ours) */
int pmXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
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

		pmXtsEncryptSector(tweakKey, dataKey, curTweak, keyBits,
				chunk, src + offset, dst + offset);
		offset += chunk;
		pmIncrementTweak128(curTweak, 16u);
		remBytes -= sectorBytes;
	}
	return 0;
}

/* ============================================================================
 * Endian Helpers (0x80cbd8..0x80ccdb)
 * ============================================================================ */

/* 0x80cbd8 */
uint16_t htole16Sm(uint16_t val)
{
	return val;
}

/* 0x80cbde */
uint16_t le16tohSm(uint16_t val)
{
	return val;
}

/* 0x80cbe4 */
uint32_t htole32Sm(uint32_t val)
{
	return val;
}

/* 0x80cbe8 */
uint32_t le32tohSm(uint32_t val)
{
	return val;
}

/* 0x80cbec */
uint64_t *htole64Sm(uint64_t *dst, const uint64_t *src)
{
	*dst = *src;
	return dst;
}

/* 0x80cbfc */
uint64_t *le64tohSm(uint64_t *dst, const uint64_t *src)
{
	*dst = *src;
	return dst;
}

/* 0x80cc0c */
int16_t bswap16Sm(uint16_t val)
{
	uint8_t bytes[2];

	bytes[0] = (uint8_t)(val >> 8);
	bytes[1] = (uint8_t)val;
	return *(int16_t *)(void *)bytes;
}

/* 0x80cc24 */
int16_t htobe16Sm(uint16_t val)
{
	return bswap16Sm(val);
}

/* 0x80cc28 */
int16_t be16tohSm(uint16_t val)
{
	return bswap16Sm(val);
}

/* 0x80cc2c */
uint32_t bswap32Sm(uint32_t val)
{
	uint8_t bytes[4];

	bytes[0] = (uint8_t)(val >> 24);
	bytes[1] = (uint8_t)(val >> 16);
	bytes[2] = (uint8_t)(val >> 8);
	bytes[3] = (uint8_t)val;
	return *(uint32_t *)(void *)bytes;
}

/* 0x80cc4e */
uint32_t htobe32Sm(uint32_t val)
{
	return bswap32Sm(val);
}

/* 0x80cc52 */
uint32_t be32tohSm(uint32_t val)
{
	return bswap32Sm(val);
}

/* 0x80cc56 */
uint64_t *bswap64Sm(uint64_t *dst, const uint64_t *src)
{
	uint8_t *p = (uint8_t *)dst;
	uint8_t *q = (uint8_t *)dst + 7u;
	uint32_t i;

	*dst = *src;
	for (i = 0u; i < 4u; i++, p++, q--) {
		uint8_t tmp = *p;

		*p = *q;
		*q = tmp;
	}
	return dst;
}

/* 0x80cc80 */
uint64_t *htobe64Sm(uint64_t *dst, const uint64_t *src)
{
	uint64_t tmp = *src;
	uint64_t swapped;

	bswap64Sm(&swapped, &tmp);
	*dst = swapped;
	return dst;
}

/* 0x80ccae */
uint64_t *be64tohSm(uint64_t *dst, const uint64_t *src)
{
	uint64_t tmp = *src;
	uint64_t swapped;

	bswap64Sm(&swapped, &tmp);
	*dst = swapped;
	return dst;
}

/* ============================================================================
 * Auth1, Auth2, and RSA Protocol Helpers (0x80ccdc..0x80d7d0)
 * ============================================================================ */

/* 0x80ccdc (name ours) */
int pmConstTimeCompare(const uint8_t *a, const uint8_t *b, uint32_t len)
{
	uint32_t i;
	int res = 0;

	for (i = 0u; i < len; i++) {
		if (a[i] != b[i])
			res = -1;
	}
	return res;
}

/* 0x80ccfa (name ours) */
int pmResolveKeySlot(uint32_t keyId, uint32_t variant, uint32_t *outSlot)
{
	int status = pmCheckKeyId(keyId);

	if (status == 0) {
		if (keyId == 6u && variant != 1u)
			*outSlot = 14u;
		else
			*outSlot = keyId;
	}
	return status;
}

/* 0x80cd2e (name ours) */
int pmBuildAuth1InitHeader(uint8_t *out40, uint32_t keyId)
{
	int status = pmCheckKeyId(keyId);

	if (status == 0) {
		out40[0] = 48u;
		out40[1] = 0u;
		out40[2] = 0u;
		out40[3] = (uint8_t)keyId;
		memset(out40 + 4u, 0, 4u);
		memset(out40 + 8u, 0, 32u);
	}
	return status;
}

/* 0x80cd7e (name ours) */
int pmBuildAuth1Challenge(uint8_t *out40, const uint8_t *in40,
		uint32_t keyId, uint32_t variant,
		const uint8_t *serverNonce8)
{
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	if (in40[0] != 48u || in40[1] != 1u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return (int)0x800f0027u;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return (int)0x800f0027u;

	if (in40[3] != keyId)
		return (int)0x800f0024u;

	out40[0] = 48u;
	out40[1] = 2u;
	out40[2] = 0u;
	out40[3] = (uint8_t)keyId;
	*(uint32_t *)(void *)(out40 + 4u) = variant;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	memcpy(out40 + 8u, serverNonce8, 8u);
	memcpy(out40 + 16u, in40 + 8u, 8u);
	memcpy(out40 + 24u, s_pmKeyTable[slot] + 48u, 16u);

	if (pmAes128CbcEncrypt(out40 + 8u, out40 + 8u, 32u,
			s_pmKeyTable[slot], s_pmZeroIvAuth) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80ce72 (name ours) */
int pmVerifyAuth1Response(const uint8_t *in40, uint32_t keyId,
		uint32_t variant, const uint8_t *expectedClientNonce8,
		const uint8_t *expectedServerNonce8, uint8_t *outSessionKey16)
{
	uint8_t decBuf[32];
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	if (in40[0] != 48u || in40[1] != 3u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return (int)0x800f0027u;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return (int)0x800f0027u;

	if (in40[3] != keyId)
		return (int)0x800f0024u;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	if (pmAes128CbcDecrypt(decBuf, in40 + 8u, 32u,
			s_pmKeyTable[slot] + 16u, s_pmZeroIvAuth) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmConstTimeCompare(decBuf, expectedClientNonce8, 8u) != 0 ||
	    pmConstTimeCompare(decBuf + 8u, expectedServerNonce8, 8u) != 0) {
		return (int)0x800f0027u;
	}

	if (outSessionKey16 != NULL) {
		memcpy(outSessionKey16, expectedClientNonce8, 8u);
		memcpy(outSessionKey16 + 8u, expectedServerNonce8, 8u);
		if (pmAes128CbcEncrypt(outSessionKey16, outSessionKey16, 16u,
				s_pmKeyTable[slot] + 32u, s_pmZeroIvAuth) != 0) {
			return SCE_SBL_ERROR_IO;
		}
	}
	return 0;
}

/* 0x80cfa6 (name ours) */
int pmClearKeySlot(uint32_t keyId, uint32_t variant)
{
	uint32_t slot;
	int status;

	status = pmCheckKeyId(keyId);
	if (status != 0)
		return status;

	slot = (keyId == 6u && variant != 1u) ? 14u : keyId;
	if (slot > 15u)
		return (int)0x800f0022u;
	if (s_pmKeyTable[slot] == NULL)
		return (int)0x800f0002u;

	memset(s_pmKeyTable[slot], 0, 64u);
	return 0;
}

/* 0x80d004 (name ours) */
int pmAes128CbcEncrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv)
{
	uint8_t localIv[16];

	memcpy(localIv, iv, 16u);
	if (bigmacAes128CbcEncryptSync(dst, src, len, key, localIv) != 0)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80d066 (name ours) */
int pmAes128CbcDecrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv)
{
	uint8_t localIv[16];

	memcpy(localIv, iv, 16u);
	if (bigmacAes128CbcDecryptSync(dst, src, len, key, localIv) != 0)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80d0c8 (name ours) */
int pmGetRandomBytes(uint8_t *dst, uint32_t len)
{
	uint32_t offset = 0u;

	while (len != 0u) {
		uint32_t chunk = (len < 5u) ? len : 4u;
		uint32_t word = REG32(0xe005003cu);

		memcpy(dst + offset, &word, chunk);
		len -= chunk;
		offset += chunk;
	}
	return 0;
}

/* 0x80d120 (name ours) */
int pmValidatePkcs1Padding(const uint8_t *buf260)
{
	uint8_t padByte;
	uint32_t i;

	if (buf260[4] != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (buf260[5] == 0u)
		padByte = 0u;
	else if (buf260[5] == 1u)
		padByte = 0xffu;
	else
		return SCE_SBL_ERROR_INVALID_VALUE;

	for (i = 0u; i < 189u; i++) {
		if (buf260[6u + i] != padByte)
			return SCE_SBL_ERROR_INVALID_VALUE;
	}
	if (buf260[195] != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	return 0;
}

/* 0x80d168 (name ours) */
int pmSha256HeaderAndNonce(const uint8_t *hdr4, const uint8_t *nonce32,
		uint8_t *outDigest32)
{
	CfHmacContext shaCtx;

	if (bigmacSha256Init(&shaCtx) == 0 &&
	    bigmacSha256Update(&shaCtx, hdr4, 4u, 0u) == 0 &&
	    bigmacSha256Update(&shaCtx, nonce32, 32u, 1u) == 0 &&
	    bigmacSha256Final(outDigest32, &shaCtx) == 0) {
		return 0;
	}
	return SCE_SBL_ERROR_IO;
}

/* 0x80d1e6 (name ours) */
int pmBuildRsaChallenge(uint8_t subcmd, uint8_t *out260,
		uint8_t *outServerNonce16, const BignumPublicKey *rsaKey)
{
	if (out260 == NULL || outServerNonce16 == NULL ||
	    rsaKey == NULL || rsaKey->modulusWords != 64u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	out260[0] = 64u;
	out260[1] = subcmd;
	out260[2] = 1u;
	out260[3] = 0u;

	if (pmGetRandomBytes(out260 + 196u, 16u) != 0)
		return SCE_SBL_ERROR_IO;

	if (subcmd != 72u) {
		if (pmGetRandomBytes(out260 + 212u, 16u) != 0)
			return SCE_SBL_ERROR_IO;
	}

	if (pmSha256HeaderAndNonce(out260, out260 + 196u, out260 + 228u) != 0)
		return SCE_SBL_ERROR_IO;

	memcpy(outServerNonce16, out260 + 196u, 16u);
	out260[4] = 0u;
	out260[5] = 2u;
	if (pmGetRandomNonZeroBytes(out260 + 6u, 189u) != 0)
		return SCE_SBL_ERROR_IO;
	out260[195] = 0u;

	if (bignumModExp2048((uint32_t *)(void *)(out260 + 4u),
			(const uint32_t *)(const void *)(out260 + 4u),
			rsaKey) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80d2b6 (name ours) */
int pmVerifyRsaResponseNoOut(uint8_t subcmd, const uint8_t *in260,
		const uint8_t *expectedNonce16, const BignumPublicKey *rsaKey)
{
	uint8_t buf[260];
	uint8_t calcDigest[32];

	if (in260 == NULL || expectedNonce16 == NULL ||
	    rsaKey == NULL || rsaKey->modulusWords != 64u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memcpy(buf, in260, sizeof(buf));
	if (buf[0] != 64u || buf[1] != subcmd || buf[2] != 2u || buf[3] != 0u)
		return (int)0x800f0029u;

	if (bignumModExp2048((uint32_t *)(void *)(buf + 4u),
			(const uint32_t *)(const void *)(buf + 4u),
			rsaKey) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmValidatePkcs1Padding(buf) != 0)
		return (int)0x800f0027u;

	if (timingsafeBcmp(buf + 196u, expectedNonce16, 16u) != 0)
		return (int)0x800f0027u;

	if (pmSha256HeaderAndNonce(buf, buf + 196u, calcDigest) != 0)
		return SCE_SBL_ERROR_IO;

	if (timingsafeBcmp(buf + 228u, calcDigest, 32u) != 0)
		return (int)0x800f0027u;

	return 0;
}

/* 0x80d3ba (name ours) */
int pmVerifyRsaResponse(uint8_t subcmd, const uint8_t *in260,
		const uint8_t *expectedNonce16, uint8_t *outPayload16,
		const BignumPublicKey *rsaKey)
{
	uint8_t buf[260];
	uint8_t calcDigest[32];

	if (in260 == NULL || expectedNonce16 == NULL ||
	    rsaKey == NULL || rsaKey->modulusWords != 64u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	memcpy(buf, in260, sizeof(buf));
	if (buf[0] != 64u || buf[1] != subcmd || buf[2] != 2u || buf[3] != 0u)
		return (int)0x800f0029u;

	if (bignumModExp2048((uint32_t *)(void *)(buf + 4u),
			(const uint32_t *)(const void *)(buf + 4u),
			rsaKey) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmValidatePkcs1Padding(buf) != 0)
		return (int)0x800f0027u;

	if (timingsafeBcmp(buf + 196u, expectedNonce16, 16u) != 0)
		return (int)0x800f0027u;

	if (pmSha256HeaderAndNonce(buf, buf + 196u, calcDigest) != 0)
		return SCE_SBL_ERROR_IO;

	if (timingsafeBcmp(buf + 228u, calcDigest, 32u) != 0)
		return (int)0x800f0027u;

	memcpy(outPayload16, buf + 212u, 16u);
	return 0;
}

/* 0x80d4cc (name ours) */
int pmBuildSubcmd6Challenge(uint8_t *out40, uint32_t variant,
		const uint8_t *sessionKey16)
{
	uint32_t slot = 0xffffffffu;

	out40[0] = 48u;
	out40[1] = 4u;
	out40[2] = 0u;
	out40[3] = 6u;
	if (pmResolveKeySlot(6u, variant, &slot) != 0)
		return (int)0x800f002fu;

	memset(out40 + 4u, 0, 4u);
	memcpy(out40 + 8u, s_pmSubcmd6Table[slot], 16u);
	memset(out40 + 24u, 0, 16u);
	if (pmAes128CbcEncrypt(out40 + 8u, out40 + 8u, 32u,
			sessionKey16, s_pmZeroIvPtr1) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80d568 (name ours) */
int pmVerifySubcmd6Response(const uint8_t *in40, uint32_t variant,
		const uint8_t *sessionKey16, uint8_t *outSessionKey16)
{
	uint8_t decBuf[32];
	uint32_t slot = 0xffffffffu;

	if (in40[0] != 48u || in40[1] != 5u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return (int)0x800f0027u;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return (int)0x800f0027u;

	if (in40[3] != 6u)
		return (int)0x800f0024u;

	if (pmResolveKeySlot(6u, variant, &slot) != 0)
		return (int)0x800f002fu;

	if (pmAes128CbcDecrypt(decBuf, in40 + 8u, 32u,
			sessionKey16, s_pmZeroIvPtr1) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmConstTimeCompare(decBuf, s_pmSubcmd6Table[slot], 16u) != 0)
		return (int)0x800f0027u;

	memcpy(outSessionKey16, decBuf + 16u, 16u);
	return 0;
}

/* 0x80d654 (name ours) */
int pmBuildSubcmd4Challenge(uint8_t *out40, const uint8_t *subSessionKey16,
		const uint8_t *sessionKey16)
{
	const void *refZeroPtr = NULL;

	out40[0] = 48u;
	out40[1] = 4u;
	out40[2] = 0u;
	out40[3] = 4u;
	memset(out40 + 4u, 0, 4u);
	memcpy(out40 + 8u, refZeroPtr, 16u);
	memcpy(out40 + 24u, subSessionKey16, 16u);
	if (pmAes128CbcEncrypt(out40 + 8u, out40 + 8u, 32u,
			sessionKey16, s_pmZeroIvPtr2) != 0) {
		return SCE_SBL_ERROR_IO;
	}
	return 0;
}

/* 0x80d6d0 (name ours) */
int pmVerifySubcmd4Response(const uint8_t *in40, const uint8_t *sessionKey16,
		uint8_t *outPayload16)
{
	uint8_t decBuf[32];
	const uint8_t *refZeroPtr = NULL;

	if (in40[0] != 48u || in40[1] != 5u)
		return (int)0x800f0024u;

	if (in40[2] >= 6u) {
		if (in40[2] == 16u)
			return (int)0x800f0027u;
		return (int)0x800f0024u;
	}
	if (in40[2] != 0u)
		return (int)0x800f0027u;

	if (in40[3] != 4u)
		return (int)0x800f0024u;

	if (pmAes128CbcDecrypt(decBuf, in40 + 8u, 32u,
			sessionKey16, s_pmZeroIvPtr2) != 0) {
		return SCE_SBL_ERROR_IO;
	}

	if (pmConstTimeCompare(decBuf, refZeroPtr, 16u) != 0)
		return (int)0x800f0027u;

	memcpy(outPayload16, decBuf + 16u, 16u);
	return 0;
}

/* 0x80d794 (name ours) */
int pmGetRandomNonZeroBytes(uint8_t *dst, uint32_t len)
{
	uint32_t outIdx = 0u;

	while (len != 0u) {
		uint32_t word = REG32(0xe005003cu);
		const uint8_t *bytes = (const uint8_t *)(const void *)&word;
		uint32_t i;

		for (i = 0u; i < 4u; i++) {
			if (bytes[i] != 0u) {
				dst[outIdx++] = bytes[i];
				len--;
				if (len == 0u)
					return 0;
			}
		}
	}
	return 0;
}
