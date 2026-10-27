/*
 * act_sm.c: PlayStation Vita Activation secure module (os0:sm/act_sm.self)
 * service entry point, activation blob AES-256-CBC decryption, AES-256-CMAC
 * verification/signing, RSA-2048 PKCS#1 v1.5 signature verification, OpenPSID
 * binding check, and keyring slot 0x517 programming (0x80b09c..0x80ba2a,
 * .rodata 0x80cdf0..0x80cfa4, .bss 0x80d0c0..0x80d0f0).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "act_sm.h"

/* ============================================================================
 * .rodata (0x80cdf0..0x80cfa4) & .bss (0x80d0c0..0x80d0f0)
 * ============================================================================ */

/* 0x80cdf0: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80cdf8) */
static const uintptr_t s_actHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)actSmHookInvoke,
	0u
};

/* 0x80ce00: C++ vtable for abstract hook base node */
static const uintptr_t s_actBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80ce10: 4-byte activation record/blob magic ("act\0") */
static const uint8_t s_actMagic[4] __attribute__((aligned(8))) = {
	'a', 'c', 't', '\0'
};

/* 0x80ce18..0x80cf24: RSA-2048 public key for activation blob verification */
static const SceActRsaPublicKey s_actRsaPublicKey __attribute__((aligned(8))) = {
	.modulus = {
		0xa8u, 0x44u, 0xccu, 0x0eu, 0x30u, 0x8bu, 0x55u, 0xe9u,
		0x11u, 0x3fu, 0xa5u, 0x11u, 0xffu, 0x25u, 0x7fu, 0xa6u,
		0x43u, 0x1au, 0xfeu, 0xcfu, 0xc3u, 0x60u, 0x26u, 0x43u,
		0x55u, 0xdcu, 0xddu, 0x07u, 0xb5u, 0xd9u, 0x61u, 0x0eu,
		0x92u, 0xbbu, 0xa8u, 0x84u, 0x22u, 0x26u, 0xb0u, 0x27u,
		0xbbu, 0x18u, 0xb5u, 0xa5u, 0x96u, 0xb1u, 0xffu, 0xafu,
		0x41u, 0x46u, 0x6eu, 0x16u, 0x7bu, 0x6bu, 0xb7u, 0xceu,
		0xe8u, 0xb6u, 0xadu, 0x26u, 0x44u, 0x69u, 0xd5u, 0x8eu,
		0x9eu, 0x1cu, 0xc8u, 0xdcu, 0x86u, 0x77u, 0xa0u, 0xacu,
		0xffu, 0xc6u, 0xcdu, 0x6bu, 0x60u, 0x60u, 0xe9u, 0x6bu,
		0x97u, 0x1du, 0x54u, 0x06u, 0x3cu, 0x6cu, 0x38u, 0x48u,
		0x4cu, 0x2cu, 0x8du, 0x2fu, 0x9cu, 0xd1u, 0xecu, 0x70u,
		0xacu, 0xb8u, 0x08u, 0xc5u, 0x77u, 0xefu, 0xa7u, 0x07u,
		0x85u, 0xf8u, 0x6cu, 0x86u, 0x97u, 0x7du, 0x7au, 0x0cu,
		0x69u, 0x39u, 0xe7u, 0x22u, 0x19u, 0x39u, 0x51u, 0x24u,
		0x84u, 0xdcu, 0x39u, 0x99u, 0x66u, 0xeau, 0x6eu, 0xe2u,
		0x28u, 0xdbu, 0xcdu, 0x99u, 0xf7u, 0x4cu, 0xbcu, 0x17u,
		0x9bu, 0xceu, 0x59u, 0xfcu, 0x76u, 0xcau, 0xf6u, 0xfcu,
		0xd9u, 0xb6u, 0x5fu, 0xa6u, 0xb3u, 0xebu, 0x11u, 0x3au,
		0x58u, 0xf7u, 0x1fu, 0xddu, 0x16u, 0xb2u, 0x49u, 0xb6u,
		0x1cu, 0x8au, 0xf8u, 0x59u, 0x81u, 0x9fu, 0x5eu, 0x31u,
		0x1eu, 0x87u, 0x1eu, 0x47u, 0x5bu, 0x42u, 0x42u, 0x63u,
		0x5au, 0x79u, 0xb0u, 0xc8u, 0xdeu, 0x27u, 0x13u, 0xe6u,
		0xd5u, 0x84u, 0x72u, 0x15u, 0xb2u, 0x8eu, 0x03u, 0x7cu,
		0x51u, 0x81u, 0xe5u, 0x1cu, 0xc7u, 0xa3u, 0xe8u, 0xa6u,
		0x5bu, 0xd4u, 0x04u, 0x1au, 0x6fu, 0xd7u, 0x42u, 0x77u,
		0xfcu, 0x2au, 0x3eu, 0x63u, 0x43u, 0x4bu, 0x54u, 0x9bu,
		0xb6u, 0x2bu, 0xdcu, 0x64u, 0x9fu, 0xa2u, 0x12u, 0xa0u,
		0x3fu, 0xfeu, 0xa5u, 0xa3u, 0x89u, 0x3bu, 0x93u, 0xc6u,
		0x20u, 0xc2u, 0x64u, 0x2au, 0x71u, 0x79u, 0x28u, 0x9bu,
		0x98u, 0x1bu, 0x7cu, 0xfbu, 0x68u, 0x1eu, 0x05u, 0x77u,
		0xddu, 0x3eu, 0xbcu, 0x17u, 0x47u, 0x24u, 0x52u, 0x39u,
	},
	.modulusSize = 0x100u,
	.exponent = { 0x00u, 0x01u, 0x00u, 0x01u },
	.exponentSize = 0x04u,
};

/* 0x80cf24..0x80cf44: 32-byte AES-256-CBC/CMAC key for activation blob */
static const uint8_t s_actBlobKey32[32] = {
	0x84u, 0x6du, 0x2du, 0xfdu, 0x77u, 0xd3u, 0xc2u, 0xe5u,
	0xf0u, 0xe1u, 0x7eu, 0xb1u, 0x8cu, 0xc7u, 0x86u, 0x92u,
	0x8bu, 0x88u, 0x1eu, 0x2eu, 0x17u, 0xaeu, 0x0cu, 0xd8u,
	0xfdu, 0xe8u, 0x88u, 0x09u, 0xd0u, 0xd0u, 0x33u, 0xc5u,
};

/* 0x80cf44..0x80cf54: 16-byte AES-256-CBC IV for activation blob */
static const uint8_t s_actBlobIv16[16] = {
	0xc8u, 0xa0u, 0x40u, 0x66u, 0x2bu, 0x10u, 0xa1u, 0x98u,
	0x6au, 0x18u, 0x94u, 0xe9u, 0x4fu, 0xbeu, 0xfcu, 0xf0u,
};

/* 0x80cf54..0x80cf74: 32-byte AES-256-CMAC key for 32-byte activation record */
static const uint8_t s_actRecordCmacKey32[32] = {
	0x5au, 0x91u, 0xfcu, 0x74u, 0xa8u, 0x2bu, 0xe3u, 0xf2u,
	0xb8u, 0xf4u, 0xdbu, 0x60u, 0x70u, 0xa0u, 0x99u, 0xa2u,
	0xbdu, 0xf0u, 0x0eu, 0x7bu, 0xf0u, 0x0eu, 0x7bu, 0xf0u,
	0x8bu, 0x68u, 0x55u, 0x34u, 0xa0u, 0x64u, 0x6du, 0x87u,
};

/* 0x80cf74..0x80cf94: 32-byte legacy AES-256 key seed */
static const uint8_t s_actRodataKey32[32] = {
#if FW_DEBUG
	0x3du, 0x1eu, 0xfbu, 0x3cu, 0xfeu, 0xd4u, 0x2au, 0x5fu,
	0x87u, 0x12u, 0x13u, 0xd4u, 0xcfu, 0x1eu, 0x3bu, 0x7au,
	0x65u, 0xf9u, 0x27u, 0xe2u, 0x38u, 0x11u, 0xb5u, 0x94u,
	0x21u, 0x5du, 0x61u, 0x6bu, 0xe5u, 0x9du, 0xa4u, 0xd8u,
#else
	0x18u, 0xedu, 0xd9u, 0x6bu, 0xb0u, 0xe9u, 0x81u, 0x08u,
	0x31u, 0x4eu, 0xd0u, 0x6fu, 0x51u, 0x76u, 0x20u, 0x84u,
	0x55u, 0xe0u, 0xf5u, 0xdau, 0x59u, 0x21u, 0xebu, 0xf2u,
	0x9du, 0x07u, 0x9eu, 0x13u, 0xd3u, 0x0eu, 0x1eu, 0xc0u,
#endif
};

/* 0x80cf94..0x80cfa4: 16-byte legacy AES IV seed */
static const uint8_t s_actRodataIv16[16] = {
#if FW_DEBUG
	0xd5u, 0x7eu, 0x36u, 0x99u, 0x98u, 0x33u, 0x02u, 0x61u,
	0x16u, 0x32u, 0xdeu, 0x33u, 0xb1u, 0x97u, 0xa4u, 0x3cu,
#else
	0x15u, 0x25u, 0x7cu, 0x39u, 0x12u, 0x26u, 0x63u, 0x68u,
	0xc5u, 0xc2u, 0xdeu, 0x01u, 0xcfu, 0x27u, 0x00u, 0x55u,
#endif
};

/* 0x80d0c0..0x80d0e0: 32-byte BSS key buffer */
static uint8_t s_actBssKey32[32] __attribute__((aligned(32)));

/* 0x80d0e0..0x80d0f0: 16-byte BSS IV buffer */
static uint8_t s_actBssIv16[16] __attribute__((aligned(16)));

/* ============================================================================
 * Functions (0x80b09c..0x80ba2a)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *hookNode;
	uint8_t mgrDummy;
	int ret;

	smSrvInit(&srvCtx);

	fnPtr.fn = actCmd4VerifyRecord;
	fnPtr.adj = 0;
	ret = SCE_SBL_ERROR_ACT_FAILED;
	if (actSmAddHook(&srvCtx, ACT_CMD_VERIFY_RECORD, &mgrDummy, &fnPtr, &hookNode) == 0) {
		fnPtr.fn = actCmd11VerifyAndApplyActivation;
		fnPtr.adj = 0;
		if (actSmAddHook(&srvCtx, ACT_CMD_VERIFY_AND_APPLY, &mgrDummy, &fnPtr, &hookNode) == 0) {
			fnPtr.fn = actCmd10VerifyAndUpdateRenewal;
			fnPtr.adj = 0;
			if (actSmAddHook(&srvCtx, ACT_CMD_VERIFY_AND_RENEW, &mgrDummy, &fnPtr, &hookNode) == 0) {
				smSrvRunLoop(&srvCtx);
				ret = 0x55555555;
			}
		}
	}

	smSrvTeardown(&srvCtx);
	return ret;
}

/* 0x80b158 (name ours) */
int actSmHookInvoke(void *closure, void *packetBuf)
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

/* 0x80b18c (name ours) */
int actSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return (int)0x800f000cu;

	node->cmdId = cmdId;
	node->next = NULL;
	node->vtable = (const void *)&s_actHookVtable[2];
	node->handler = mgr;
	node->arg0 = (void *)fnPtr->fn;
	node->adj = fnPtr->adj;

	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b206 (name ours) */
void actSmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	volatile const uint32_t *slot =
		(volatile const uint32_t *)((uintptr_t)0xe0058000u + (slotIndex << 5));

	outWords8[0] = slot[0];
	outWords8[1] = slot[1];
	outWords8[2] = slot[2];
	outWords8[3] = slot[3];
	outWords8[4] = slot[4];
	outWords8[5] = slot[5];
	outWords8[6] = slot[6];
	outWords8[7] = slot[7];
}

/* 0x80b250 (name ours) */
void actInitLegacyKeys(void)
{
	memcpy(s_actBssKey32, s_actRodataKey32, 32u);
	memcpy(s_actBssIv16, s_actRodataIv16, 16u);
}

/* 0x80b278 (name ours) */
int actIsKeyringSlot50aNonZero(void *mgr)
{
	uint32_t words[8];

	(void)mgr;
	actSmReadKeyringSlot32(0x50au, words);
	return ((words[0] | words[1] | words[2] | words[3]) != 0u) ? 1 : 0;
}

/* 0x80b2a2 (name ours) */
int actGetKeyringSlot50bBit0(void *mgr)
{
	uint32_t words[8];

	(void)mgr;
	actSmReadKeyringSlot32(0x50bu, words);
	return (int)(((const uint8_t *)words)[0] & 1u);
}

/* 0x80b2be (name ours) */
void actUpdateKeyringSlot517Word(void *mgr, uint32_t value, uint32_t wordIndex)
{
	volatile uint32_t * const hwRegs =
		(volatile uint32_t *)(uintptr_t)0xe0030000u;
	uint32_t words[8];

	(void)mgr;
	actSmReadKeyringSlot32(0x517u, words);
	words[wordIndex] = value;

	hwRegs[0] = words[0];
	hwRegs[1] = words[1];
	hwRegs[2] = words[2];
	hwRegs[3] = words[3];
	hwRegs[4] = words[4];
	hwRegs[5] = words[5];
	hwRegs[6] = words[6];
	hwRegs[7] = words[7];
	hwRegs[8] = 0x517u;
}

/* 0x80b350 (name ours) */
int actVerifyRsa2048Sha256(void *mgr, const void *sig256, const void *data128)
{
	BignumPublicKey rsaKey;
	uint8_t workBuf[256];
	uint8_t digest[32];
	int status;

	(void)mgr;
	memset(workBuf, 0, sizeof(workBuf));
	if (bigmacSha256(digest, data128, ACT_BLOB_SIZE) != 0)
		return (int)0x800f0027u;

	rsaKey.modulus = (const uint32_t *)s_actRsaPublicKey.modulus;
	rsaKey.modulusWords = 64u;
	rsaKey.exponent = (const uint32_t *)s_actRsaPublicKey.exponent;
	rsaKey.exponentWords = 1u;

	status = bignumVerifyRsaPkcs1v15(workBuf, sig256, &rsaKey, digest, 4u);
	if (status > 0)
		status = 0;
	return status;
}

/* 0x80b3e4 (name ours) */
int actDecryptPayloadHalf(void *mgr, const void *src128, uint8_t *dst128,
		const void *aesKey32, const void *aesIv16)
{
	uint32_t bmCtrl[2];
	uint8_t ivLocal[16];
	int status;

	(void)mgr;
	memcpy(dst128, src128, ACT_BLOB_SIZE);
	memcpy(ivLocal, aesIv16, sizeof(ivLocal));

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAes256CbcDecrypt(dst128 + 64, dst128 + 64, 64u,
			aesKey32, ivLocal, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80b450 (name ours) */
int actVerifyRecordCmac(void *mgr, const void *record32, const void *cmacKey32)
{
	uint32_t bmCtrl[2];
	uint8_t localRec[32];
	uint8_t calcMac[16];
	int status;

	(void)mgr;
	memcpy(localRec, record32, sizeof(localRec));
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, localRec, 16u, cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0) {
			if (timingsafeBcmp(calcMac, localRec + 16, 16u) != 0)
				status = SCE_SBL_ERROR_ACT_BAD_CMAC;
		}
	}
	return status;
}

/* 0x80b4ce (name ours) */
int actCmd4VerifyRecord(void *mgr, void *packetBuf)
{
	SceActCommPacket *pkt = (SceActCommPacket *)packetBuf;

	pkt->result = actVerifyRecordCmac(mgr, pkt->payload, s_actRecordCmacKey32);
	return 0;
}

/* 0x80b4ee (name ours) */
int actSignRecordCmac(void *mgr, void *record32, const void *cmacKey32)
{
	uint32_t bmCtrl[2];
	uint8_t localRec[32];
	uint8_t calcMac[16];
	int status;

	(void)mgr;
	memcpy(localRec, record32, sizeof(localRec));
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, localRec, 16u, cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0) {
			memcpy(localRec + 16, calcMac, 16u);
			memcpy(record32, localRec, 32u);
		}
	}
	return status;
}

/* 0x80b56e (name ours) */
int actVerifyBlobCmac(void *mgr, const uint8_t *blob128, const void *cmacKey32)
{
	uint32_t bmCtrl[2];
	uint8_t calcMac[16];
	int status;

	(void)mgr;
	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCmac(calcMac, blob128, 112u, cmacKey32, 32u, bmCtrl);
	if (status == 0) {
		status = bigmacWaitChannelDone(bmCtrl);
		if (status == 0) {
			if (timingsafeBcmp(calcMac, blob128 + 112, 16u) != 0)
				status = SCE_SBL_ERROR_ACT_BAD_CMAC;
		}
	}
	return status;
}

/* 0x80b5e6 (name ours) */
int actDecryptAndValidateBlob(void *mgr, const void *blob128,
		uint32_t *outFlagByte)
{
	uint8_t cidCtx[16];
	uint8_t decBlob[128];
	uint8_t openPsId[16];
	int status;

	if (actDecryptPayloadHalf(mgr, blob128, decBlob,
			s_actBlobKey32, s_actBlobIv16) != 0) {
		return SCE_SBL_ERROR_ACT_FAILED;
	}

	if (timingsafeBcmp(decBlob, s_actMagic, 4u) != 0)
		return SCE_SBL_ERROR_ACT_BAD_CMAC;

	status = actVerifyBlobCmac(mgr, decBlob, s_actBlobKey32);
	if (status != 0)
		return status;

	aimgrReadConsoleIdAlias1(cidCtx);
	status = aimgrGetOpenPsId((uint32_t)(uintptr_t)cidCtx, openPsId);
	if (status == 0) {
		if (timingsafeBcmp(openPsId, decBlob + 0x54, 16u) != 0) {
			status = SCE_SBL_ERROR_ACT_PSID_MISMATCH;
		} else if (timingsafeBcmp(decBlob + 0x14, decBlob + 0x54, 16u) != 0 ||
			   *(const uint32_t *)(decBlob + 0x0c) != *(const uint32_t *)(decBlob + 0x4c)) {
			status = SCE_SBL_ERROR_ACT_BAD_CMAC;
		} else {
			*outFlagByte = decBlob[0x45];
		}
	}

	aimgrClearConsoleId2(cidCtx);
	return status;
}

/* 0x80b6c6 (name ours) */
int actCmd10VerifyAndUpdateRenewal(void *mgr, void *packetBuf)
{
	SceActCommPacket *pkt = (SceActCommPacket *)packetBuf;
	uint8_t *payload = pkt->payload;
	uint32_t flagByte = 0u;
	uint32_t valid2;
	uint32_t recTs;
	uint32_t blob2Ts;
	int status;

	status = actDecryptAndValidateBlob(mgr, payload, &flagByte);
	if (status != 0) {
		pkt->result = status;
		return 0;
	}

	status = actVerifyRsa2048Sha256(mgr, payload + 0x80, payload);
	if (status != 0) {
		if (flagByte != 0u) {
			pkt->result = status;
			return 0;
		}
		pkt->result = SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID;
		return 0;
	}

	status = actDecryptAndValidateBlob(mgr, payload + 0x180, &flagByte);
	if (actVerifyRsa2048Sha256(mgr, payload + 0x200, payload + 0x180) == 0)
		valid2 = (status == 0) ? 1u : 0u;
	else
		valid2 = 0u;

	recTs = *(const uint32_t *)(payload + 0x304);
	blob2Ts = *(const uint32_t *)(payload + 0x188);
	if (blob2Ts != recTs)
		valid2 = 0u;

	status = actVerifyRecordCmac(mgr, payload + 0x300, s_actRecordCmacKey32);
	if (status == 0 && valid2 != 0u) {
		uint32_t newTs = *(const uint32_t *)(payload + 0x08);
		uint32_t start = *(const uint32_t *)(payload + 0x0c);
		uint32_t end   = *(const uint32_t *)(payload + 0x10);

		if (newTs <= recTs && newTs != 0u) {
			pkt->result = SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID;
			return 0;
		}
		if (end - start > ACT_MAX_DURATION_SEC) {
			pkt->result = SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID;
			return 0;
		}
	}

	memset(payload + 0x300, 0, ACT_RECORD_SIZE);
	memcpy(payload + 0x300, s_actMagic, 4u);
	*(uint32_t *)(payload + 0x304) = *(const uint32_t *)(payload + 0x08);
	*(uint32_t *)(payload + 0x308) = *(const uint32_t *)(payload + 0x10);
	*(uint32_t *)(payload + 0x30c) = *(const uint32_t *)(payload + 0x0c);

	pkt->result = actSignRecordCmac(mgr, payload + 0x300, s_actRecordCmacKey32);
	return 0;
}

/* 0x80b7c6 (name ours) */
int actCmdUnusedUpdateRecord(void *mgr, void *packetBuf)
{
	SceActCommPacket *pkt = (SceActCommPacket *)packetBuf;
	uint8_t *payload = pkt->payload;
	uint32_t flagByte = 0u;
	uint32_t recTs;
	int status;

	status = actDecryptAndValidateBlob(mgr, payload, &flagByte);
	if (status != 0) {
		pkt->result = status;
		return 0;
	}

	status = actVerifyRsa2048Sha256(mgr, payload + 0x80, payload);
	if (status != 0) {
		if (flagByte != 0u) {
			pkt->result = status;
			return 0;
		}
		pkt->result = SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID;
		return 0;
	}

	recTs = *(const uint32_t *)(payload + 0x184);
	status = actVerifyRecordCmac(mgr, payload + 0x180, s_actRecordCmacKey32);
	if (status == 0) {
		uint32_t newTs = *(const uint32_t *)(payload + 0x08);

		if (newTs <= recTs && newTs != 0u) {
			pkt->result = SCE_SBL_ERROR_ACT_EXPIRED_OR_INVALID;
			return 0;
		}
	}

	memset(payload + 0x180, 0, ACT_RECORD_SIZE);
	memcpy(payload + 0x180, s_actMagic, 4u);
	*(uint32_t *)(payload + 0x184) = *(const uint32_t *)(payload + 0x08);
	*(uint32_t *)(payload + 0x188) = *(const uint32_t *)(payload + 0x10);
	*(uint32_t *)(payload + 0x18c) = *(const uint32_t *)(payload + 0x0c);

	pkt->result = actSignRecordCmac(mgr, payload + 0x180, s_actRecordCmacKey32);
	return 0;
}

/* 0x80b878 (name ours) */
int actCmd11VerifyAndApplyActivation(void *mgr, void *packetBuf)
{
	SceActCommPacket *pkt = (SceActCommPacket *)packetBuf;
	uint32_t *hdr = (uint32_t *)pkt->payload;
	uint32_t flagByte = 0u;
	uint32_t expTime = 0u;
	uint32_t outState = 1u;
	int status = 0;

	if (hdr[0] == (uint32_t)SCE_SBL_ERROR_ACT_NOT_ACTIVATED) {
		outState = 2u;
	} else if (hdr[0] == 0u) {
		status = actDecryptAndValidateBlob(mgr, pkt->payload + 0x38, &flagByte);
		if (status == 0) {
			status = actVerifyRsa2048Sha256(mgr, pkt->payload + 0xb8,
					pkt->payload + 0x38);
			if (status == 0) {
				status = actVerifyRecordCmac(mgr, pkt->payload + 0x18,
						s_actRecordCmacKey32);
				if (status != 0) {
					pkt->result = status;
				} else {
					uint32_t recEnd = hdr[8];
					uint32_t blobEnd = *(const uint32_t *)(pkt->payload + 0x48);
					uint32_t recTs = hdr[7];
					uint32_t blobTs = *(const uint32_t *)(pkt->payload + 0x40);

					if (recEnd != blobEnd || recTs != blobTs) {
						pkt->result = 0;
					} else {
						uint32_t blobStart = *(const uint32_t *)(pkt->payload + 0x44);

						expTime = recEnd;
						if (expTime - blobStart > ACT_MAX_DURATION_SEC)
							expTime = 0u;

						if (hdr[1] < expTime) {
							actUpdateKeyringSlot517Word(mgr, 3u, 0u);
							outState = 0u;
						} else {
							expTime = 0u;
						}
					}
				}
			}
		}
	}

	if (actIsKeyringSlot50aNonZero(mgr) != 0 ||
	    actGetKeyringSlot50bBit0(mgr) != 0) {
		actUpdateKeyringSlot517Word(mgr, 3u, 0u);
		outState = 0u;
	}

	hdr[3] = expTime;
	hdr[2] = outState;
	pkt->result = status;
	return 0;
}

/* 0x80b96e (name ours) */
int actCmdUnusedVerifyAndApply(void *mgr, void *packetBuf)
{
	SceActCommPacket *pkt = (SceActCommPacket *)packetBuf;
	uint32_t *hdr = (uint32_t *)pkt->payload;
	uint32_t flagByte = 0u;
	uint32_t expTime = 0u;
	uint32_t outState = 1u;
	int status = 0;

	if (hdr[0] == (uint32_t)SCE_SBL_ERROR_ACT_NOT_ACTIVATED) {
		outState = 2u;
	} else if (hdr[0] == 0u) {
		actDecryptAndValidateBlob(mgr, pkt->payload + 0x38, &flagByte);
		actVerifyRsa2048Sha256(mgr, pkt->payload + 0xb8, pkt->payload + 0x38);
		status = actVerifyRecordCmac(mgr, pkt->payload + 0x18,
				s_actRecordCmacKey32);
		if (status != 0) {
			pkt->result = status;
		} else {
			expTime = hdr[8];
			if (hdr[1] < expTime) {
				actUpdateKeyringSlot517Word(mgr, 3u, 0u);
				outState = 0u;
			} else {
				expTime = 0u;
			}
		}
	}

	if (actIsKeyringSlot50aNonZero(mgr) != 0 ||
	    actGetKeyringSlot50bBit0(mgr) != 0) {
		actUpdateKeyringSlot517Word(mgr, 3u, 0u);
		outState = 0u;
	}

	hdr[3] = expTime;
	hdr[2] = outState;
	pkt->result = status;
	return 0;
}
