/*
 * aimgr_sm.c: PlayStation Vita SceSblAIMgr secure module (os0:sm/aimgr_sm.self)
 * service entry point, Console ID / OpenPSID / certificate verification, and
 * signed token generation handlers (0x80b09c..0x80b9c6, .rodata 0x80c258..0x80c2dc).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "aimgr_sm.h"

/* ============================================================================
 * .rodata (0x80c258..0x80c2dc)
 * ============================================================================ */

/* 0x80c258: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80c260) */
static const uintptr_t s_aimgrHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)aimgrSmHookInvoke,
	0u
};

/* 0x80c268: C++ vtable for abstract hook base node */
static const uintptr_t s_aimgrBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80c278..0x80c288: 16-byte AES-128-CBC key for token encryption */
static const uint8_t s_aimgrTokenAesKey[16] = {
#if FW_DEBUG
	0xeau, 0x35u, 0xfau, 0x34u, 0xb7u, 0x47u, 0x92u, 0x9au,
	0x54u, 0x02u, 0x19u, 0xdbu, 0xa2u, 0xdau, 0x00u, 0x1fu
#else
	0x88u, 0x60u, 0x73u, 0xdeu, 0x05u, 0x11u, 0xf0u, 0x58u,
	0x17u, 0x92u, 0xdcu, 0x66u, 0xfdu, 0x6cu, 0xa6u, 0xafu
#endif
};

/* 0x80c288..0x80c2c8: 64-byte HMAC-SHA-256 key for token authentication */
static const uint8_t s_aimgrTokenHmacKey[64] = {
#if FW_DEBUG
	0x12u, 0x7eu, 0x48u, 0x02u, 0x31u, 0x34u, 0x6au, 0x26u,
	0x05u, 0x2fu, 0xfcu, 0xf9u, 0xb5u, 0xf5u, 0x5fu, 0x45u,
	0xe9u, 0xe2u, 0xbfu, 0xaeu, 0x7cu, 0x9bu, 0xafu, 0xb2u,
	0x65u, 0x4du, 0x8eu, 0xfeu, 0x47u, 0x1du, 0x49u, 0x0eu,
	0x71u, 0x74u, 0x2fu, 0x19u, 0xd1u, 0x7cu, 0x3eu, 0x10u,
	0x8fu, 0x52u, 0xe1u, 0xa0u, 0x0eu, 0xf6u, 0xc0u, 0xb9u,
	0x02u, 0x7du, 0x77u, 0x90u, 0x29u, 0x61u, 0x8cu, 0x38u,
	0x9cu, 0xc9u, 0x45u, 0x97u, 0x25u, 0x47u, 0x83u, 0xc8u
#else
	0x4du, 0x3eu, 0x17u, 0x1cu, 0xfbu, 0x60u, 0xdfu, 0x96u,
	0xd1u, 0xafu, 0xa6u, 0xe7u, 0x6fu, 0xebu, 0xfbu, 0x5cu,
	0x07u, 0x9au, 0x5du, 0x17u, 0x79u, 0x19u, 0xc3u, 0xefu,
	0x41u, 0x7bu, 0xafu, 0xa2u, 0x3au, 0x0bu, 0x0du, 0xe2u,
	0x03u, 0x66u, 0x24u, 0xf0u, 0xc8u, 0x7au, 0x8du, 0x36u,
	0x59u, 0xdau, 0xe1u, 0x9eu, 0x77u, 0x19u, 0x51u, 0x46u,
	0xb1u, 0x1au, 0x76u, 0x7du, 0x8au, 0x35u, 0xa8u, 0x61u,
	0x0du, 0x30u, 0x1au, 0x79u, 0xbbu, 0xa9u, 0x34u, 0x2du
#endif
};

/* 0x80c2c8: Token header magic ("NPpp") */
static const uint32_t s_aimgrTokenMagic = 0x7070504eu;

/* 0x80c2cc..0x80c2dc: 16-byte encrypted certificate verification key seed */
static const uint8_t s_aimgrCertKeySeed[16] = {
	0x4eu, 0x50u, 0x70u, 0x70u, 0x82u, 0x1cu, 0x57u, 0x14u,
	0x41u, 0x5eu, 0x98u, 0x04u, 0xd6u, 0xaau, 0xe3u, 0x24u
};

/* ============================================================================
 * Service Entry & Command Handlers (0x80b09c..0x80b9c6)
 * ============================================================================ */

/* 0x80b09c */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *outNode;
	uint8_t mgrDummy;
	int ret;

	smSrvInit(&srvCtx);

	fnPtr.fn = aimgrCmd1GetConsoleId;
	fnPtr.adj = 0;
	ret = SCE_SBL_ERROR_AIMGR_FAILED;
	if (aimgrSmAddHook(&srvCtx, AIMGR_CMD_GET_CONSOLE_ID, &mgrDummy, &fnPtr, &outNode) == 0) {
		fnPtr.fn = aimgrCmd2GetOpenPsId;
		fnPtr.adj = 0;
		if (aimgrSmAddHook(&srvCtx, AIMGR_CMD_GET_OPEN_PSID, &mgrDummy, &fnPtr, &outNode) == 0) {
			fnPtr.fn = aimgrCmd3GetSlot600;
			fnPtr.adj = 0;
			if (aimgrSmAddHook(&srvCtx, AIMGR_CMD_GET_SLOT_600, &mgrDummy, &fnPtr, &outNode) == 0) {
				fnPtr.fn = aimgrCmd4ExtractCertTargetInfo;
				fnPtr.adj = 0;
				if (aimgrSmAddHook(&srvCtx, AIMGR_CMD_EXTRACT_CERT_INFO, &mgrDummy, &fnPtr, &outNode) == 0) {
					fnPtr.fn = aimgrCmd5GenerateSignedToken;
					fnPtr.adj = 0;
					if (aimgrSmAddHook(&srvCtx, AIMGR_CMD_GENERATE_TOKEN, &mgrDummy, &fnPtr, &outNode) == 0) {
						smSrvRunLoop(&srvCtx);
						ret = 0;
					}
				}
			}
		}
	}

	smSrvTeardown(&srvCtx);
	return ret;
}

/* 0x80b192 (name ours) */
int aimgrSmHookInvoke(void *closure, void *packetBuf)
{
	uintptr_t *words = (uintptr_t *)closure;
	uint8_t *obj = (uint8_t *)words[1];
	uintptr_t fnVal = words[2];
	int32_t adj = (int32_t)words[3];
	SmCmdMethod fn;

	if ((fnVal & 1u) != 0u) {
		uintptr_t vptr = *(const uintptr_t *)(const void *)(obj + adj);

		fn = *(const SmCmdMethod *)(const void *)(vptr + fnVal - 1u);
	} else {
		fn = (SmCmdMethod)fnVal;
	}
	return fn(obj + adj, packetBuf);
}

/* 0x80b1c6 (name ours) */
int aimgrSmAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_aimgrHookVtable[2];
		node->handler = mgr;
		node->arg0 = (void *)(uintptr_t)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b240 (name ours) */
void aimgrSmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
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

/* 0x80b28a (name ours; mailbox command 5) */
int aimgrCmd5GenerateSignedToken(void *mgr, void *packetBuf)
{
	SceAimgrCommPacket *pkt = (SceAimgrCommPacket *)packetBuf;
	uint32_t slot509[8];
	uint8_t tokenBuf[512];
	int ret;

	(void)mgr;
	memset(tokenBuf, 0, sizeof(tokenBuf));
	aimgrSmReadKeyringSlot32(0x509u, slot509);
	memcpy(tokenBuf + 32u, slot509, 16u);

	ret = aimgrVerifyCertAndExtractPayload(pkt->payload + 0x20u,
			tokenBuf + 48u, 80u);
	if (ret != 0) {
		pkt->result = ret;
		return ret;
	}

	memcpy(tokenBuf + 136u, pkt->payload + 0x10u, 16u);
	*(uint32_t *)(void *)(tokenBuf + 128u) =
		*(const uint32_t *)(const void *)(pkt->payload + 0x00u);
	*(uint32_t *)(void *)(tokenBuf + 132u) =
		*(const uint32_t *)(const void *)(pkt->payload + 0x04u);

	ret = aimgrBuildAndSignToken(tokenBuf);
	if (ret == 0)
		memcpy(pkt->payload + 0x20u, tokenBuf, sizeof(tokenBuf));

	pkt->result = ret;
	return ret;
}

/* 0x80b342 (name ours; mailbox command 1) */
int aimgrCmd1GetConsoleId(void *mgr, void *packetBuf)
{
	SceAimgrCommPacket *pkt = (SceAimgrCommPacket *)packetBuf;
	uint32_t slot509[8];

	(void)mgr;
	aimgrSmReadKeyringSlot32(0x509u, slot509);
	memcpy(pkt->payload, slot509, 16u);
	pkt->result = 0;
	return 0;
}

/* 0x80b374 (name ours; mailbox command 2) */
int aimgrCmd2GetOpenPsId(void *mgr, void *packetBuf)
{
	SceAimgrCommPacket *pkt = (SceAimgrCommPacket *)packetBuf;
	uint32_t slot50d[8];

	(void)mgr;
	aimgrSmReadKeyringSlot32(0x50du, slot50d);
	memcpy(pkt->payload, slot50d, 16u);
	pkt->result = 0;
	return 0;
}

/* 0x80b3a6 (name ours; mailbox command 3) */
int aimgrCmd3GetSlot600(void *mgr, void *packetBuf)
{
	SceAimgrCommPacket *pkt = (SceAimgrCommPacket *)packetBuf;
	uint32_t slot600[8];

	(void)mgr;
	aimgrSmReadKeyringSlot32(0x600u, slot600);
	memcpy(pkt->payload, slot600, 32u);
	pkt->result = 0;
	return 0;
}

/* 0x80b3d8 (name ours; mailbox command 4) */
int aimgrCmd4ExtractCertTargetInfo(void *mgr, void *packetBuf)
{
	SceAimgrCommPacket *pkt = (SceAimgrCommPacket *)packetBuf;
	uint16_t certHdr[8];
	uint16_t outInfo[4];
	int ret;

	(void)mgr;
	ret = aimgrVerifyCertAndExtract16(16u, certHdr, pkt->payload, 512u);
	if (ret == 0) {
		outInfo[0] = certHdr[1];
		outInfo[1] = certHdr[2];
		outInfo[2] = certHdr[3];
		outInfo[3] = (uint16_t)aimgrExtractSubCodeBe(
				*(const uint32_t *)(const void *)&certHdr[4]);
		memcpy(pkt->payload, outInfo, sizeof(outInfo));
	}
	pkt->result = ret;
	return ret;
}

/* 0x80b43a (name ours) */
int aimgrBuildAndSignToken(uint8_t *buf)
{
	uint32_t magic = s_aimgrTokenMagic;
	uint8_t zeroIv[16];
	uint64_t tsIn;
	uint64_t tsSwapped;
	uint32_t v1;
	uint16_t vLen;

	memcpy(buf, &magic, sizeof(magic));

	v1 = htobe32Sm(1u);
	buf[4] = (uint8_t)v1;
	buf[5] = (uint8_t)(v1 >> 8);
	buf[6] = (uint8_t)(v1 >> 16);
	buf[7] = (uint8_t)(v1 >> 24);

	vLen = (uint16_t)htobe16Sm(0x200u);
	buf[8] = (uint8_t)vLen;
	buf[9] = (uint8_t)(vLen >> 8);

	v1 = htobe32Sm(1u);
	buf[12] = (uint8_t)v1;
	buf[13] = (uint8_t)(v1 >> 8);
	buf[14] = (uint8_t)(v1 >> 16);
	buf[15] = (uint8_t)(v1 >> 24);

	tsIn = ((uint64_t)buf[128]) |
	       ((uint64_t)buf[129] << 8) |
	       ((uint64_t)buf[130] << 16) |
	       ((uint64_t)buf[131] << 24) |
	       ((uint64_t)buf[132] << 32) |
	       ((uint64_t)buf[133] << 40) |
	       ((uint64_t)buf[134] << 48) |
	       ((uint64_t)buf[135] << 56);
	bswap64Sm(&tsSwapped, &tsIn);
	buf[128] = (uint8_t)tsSwapped;
	buf[129] = (uint8_t)(tsSwapped >> 8);
	buf[130] = (uint8_t)(tsSwapped >> 16);
	buf[131] = (uint8_t)(tsSwapped >> 24);
	buf[132] = (uint8_t)(tsSwapped >> 32);
	buf[133] = (uint8_t)(tsSwapped >> 40);
	buf[134] = (uint8_t)(tsSwapped >> 48);
	buf[135] = (uint8_t)(tsSwapped >> 56);

	if (aimgrFillRandomBytes(buf + 16u, 16u) != 0)
		return SCE_SBL_ERROR_AIMGR_FAILED;

	memset(zeroIv, 0, sizeof(zeroIv));
	if (bigmacAes128CbcEncryptSync(buf + 16u, buf + 16u, 0x1d0u,
			s_aimgrTokenAesKey, zeroIv) != 0) {
		return SCE_SBL_ERROR_AIMGR_FAILED;
	}

	if (bigmacHmacSha256Sync(buf + 0x1e0u, buf, 0x1e0u,
			s_aimgrTokenHmacKey, sizeof(s_aimgrTokenHmacKey)) != 0) {
		return SCE_SBL_ERROR_AIMGR_FAILED;
	}
	return 0;
}

/* 0x80b59e (name ours) */
int aimgrVerifyCertAndExtract16(uint8_t outLen, void *out16,
		const uint8_t *certBuf, uint32_t certSize)
{
	uint8_t block[256];
	int ret;

	if (outLen != 16u || out16 == NULL || certSize != 512u || certBuf == NULL)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARG;

	memcpy(block, certBuf + 160u, sizeof(block));
	ret = aimgrDecryptAndVerifyCertBlock(block, sizeof(block), 0u);
	if (ret == 0)
		memcpy(out16, block, 16u);
	return ret;
}

/* 0x80b620 (name ours) */
int aimgrVerifyCertAndExtractPayload(const uint8_t *certBuf, void *out80,
		uint32_t outSize)
{
	uint8_t block[256];
	int ret;

	if (outSize < 80u)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARG;

	memcpy(block, certBuf + 160u, sizeof(block));
	ret = aimgrDecryptAndVerifyCertBlock(block, sizeof(block), 0u);
	if (ret == 0)
		memcpy(out80, certBuf + 176u, 80u);
	return ret;
}

/* 0x80b698 (name ours) */
int aimgrSetupKeyringSlot204(void)
{
	uint32_t status;

	REG32(0xe0050000u) = (uint32_t)(uintptr_t)s_aimgrCertKeySeed;
	REG32(0xe0050004u) = 0u;
	REG32(0xe0050008u) = 16u;
	REG32(0xe005000cu) = 0x10000001u;
	REG32(0xe0050010u) = 0x204u;
	REG32(0xe005001cu) = 1u;

	do {
		status = REG32(0xe0050024u);
	} while ((status & 1u) != 0u);

	REG32(0xe0030028u) = 0u;
	if ((REG32(0xe003002cu) & 2u) == 0u)
		return SCE_SBL_ERROR_AIMGR_KEY_NOT_READY;
	return (int)status;
}

/* 0x80b71a (name ours) */
int aimgrComputeCertCmac(void *outMac16, const void *src, uint32_t size)
{
	uint32_t status;

	REG32(0xe0050000u) = (uint32_t)(uintptr_t)src;
	REG32(0xe0050004u) = (uint32_t)(uintptr_t)outMac16;
	REG32(0xe0050008u) = size & 0x00ffffffu;
	REG32(0xe005000cu) = 0x33bu;
	REG32(0xe0050010u) = 0x212u;
	REG32(0xe005001cu) = 1u;

	do {
		status = REG32(0xe0050024u);
	} while ((status & 1u) != 0u);
	return (int)status;
}

/* 0x80b760 (name ours) */
int aimgrDecryptCertTag(void *outTag16, const void *inTag16, uint32_t size)
{
	uint32_t status;

	if (inTag16 == NULL || outTag16 == NULL || size != 16u)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARG;

	REG32(0xe0030028u) = 0u;
	if (((REG32(0xe003002cu) & 2u) >> 1) != 1u)
		return SCE_SBL_ERROR_AIMGR_KEY_NOT_READY;

	REG32(0xe0050000u) = (uint32_t)(uintptr_t)inTag16;
	REG32(0xe0050004u) = (uint32_t)(uintptr_t)outTag16;
	REG32(0xe0050008u) = size;
	REG32(0xe005000cu) = 0x102u;
	REG32(0xe0050010u) = 0u;
	REG32(0xe005001cu) = 1u;

	do {
		status = REG32(0xe0050024u);
	} while ((status & 1u) != 0u);
	return (int)status;
}

/* 0x80b7d4 (name ours) */
int aimgrDecryptAndVerifyCertBlock(const uint8_t *block256, uint32_t size,
		uint32_t unused)
{
	uint8_t decryptedTag[16];
	uint8_t computedMac[16];
	int ret;

	(void)unused;
	if (size != 256u)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARG;

	memset(computedMac, 0, sizeof(computedMac));
	memset(decryptedTag, 0, sizeof(decryptedTag));

	ret = aimgrSetupKeyringSlot204();
	if (ret != 0)
		return ret;

	ret = aimgrComputeCertCmac(computedMac, block256, 216u);
	if (ret != 0)
		return ret;

	ret = aimgrDecryptCertTag(decryptedTag, block256 + 216u, 16u);
	if (ret != 0)
		return ret;

	if (timingsafeBcmp(computedMac, decryptedTag, 16u) != 0)
		return SCE_SBL_ERROR_AIMGR_BAD_DIGEST;
	return 0;
}

/* 0x80b852 (name ours) */
int16_t aimgrExtractSubCodeBe(uint32_t rawWord2)
{
	return htobe16Sm((uint16_t)(be32tohSm(rawWord2) >> 26));
}

/* 0x80b86a (name ours) */
uint16_t htole16Sm(uint16_t v)
{
	return v;
}

/* 0x80b870 (name ours) */
uint16_t le16tohSm(uint16_t v)
{
	return v;
}

/* 0x80b876 (name ours) */
uint32_t htole32Sm(uint32_t v)
{
	return v;
}

/* 0x80b87a (name ours) */
uint32_t le32tohSm(uint32_t v)
{
	return v;
}

/* 0x80b87e (name ours) */
uint64_t *htole64Sm(uint64_t *out, const uint64_t *in)
{
	*out = *in;
	return out;
}

/* 0x80b88e (name ours) */
uint64_t *le64tohSm(uint64_t *out, const uint64_t *in)
{
	*out = *in;
	return out;
}

/* 0x80b89e (name ours) */
int16_t bswap16Sm(uint16_t v)
{
	union {
		uint8_t b[2];
		int16_t s;
	} u;

	u.b[0] = (uint8_t)(v >> 8);
	u.b[1] = (uint8_t)v;
	return u.s;
}

/* 0x80b8b6 (name ours) */
int16_t htobe16Sm(uint16_t v)
{
	return bswap16Sm(v);
}

/* 0x80b8ba (name ours) */
int16_t be16tohSm(uint16_t v)
{
	return bswap16Sm(v);
}

/* 0x80b8be (name ours) */
uint32_t bswap32Sm(uint32_t v)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} u;

	u.b[0] = (uint8_t)(v >> 24);
	u.b[1] = (uint8_t)(v >> 16);
	u.b[2] = (uint8_t)(v >> 8);
	u.b[3] = (uint8_t)v;
	return u.w;
}

/* 0x80b8e0 (name ours) */
uint32_t htobe32Sm(uint32_t v)
{
	return bswap32Sm(v);
}

/* 0x80b8e4 (name ours) */
uint32_t be32tohSm(uint32_t v)
{
	return bswap32Sm(v);
}

/* 0x80b8e8 (name ours) */
uint64_t *bswap64Sm(uint64_t *out, const uint64_t *in)
{
	uint8_t *b = (uint8_t *)out;
	uint32_t i;

	*out = *in;
	for (i = 0u; i < 4u; i++) {
		uint8_t tmp = b[i];

		b[i] = b[7u - i];
		b[7u - i] = tmp;
	}
	return out;
}

/* 0x80b912 (name ours) */
uint64_t *htobe64Sm(uint64_t *out, const uint64_t *in)
{
	uint64_t val = *in;
	uint64_t res;

	bswap64Sm(&res, &val);
	*out = res;
	return out;
}

/* 0x80b940 (name ours) */
uint64_t *be64tohSm(uint64_t *out, const uint64_t *in)
{
	uint64_t val = *in;
	uint64_t res;

	bswap64Sm(&res, &val);
	*out = res;
	return out;
}

/* 0x80b96e (name ours) */
int aimgrFillRandomBytes(uint8_t *dst, uint32_t size)
{
	uint32_t offset = 0u;

	while (size != 0u) {
		uint32_t chunk = (size < 5u) ? size : 4u;
		uint32_t rndWord = REG32(0xe005003cu);

		memcpy(dst + offset, &rndWord, chunk);
		size -= chunk;
		offset += chunk;
	}
	return 0;
}
