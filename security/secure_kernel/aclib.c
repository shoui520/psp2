/*
 * aclib.c: Secure Module Access Control Library (SceSblACLib) verification
 * of target capabilities, shared secrets, QA/dipsw flags, revocation list,
 * and minimum system version (0x804c3e..0x8050e1).
 *
 * Sony function name "sceSblACLibIsLoadable" (0x804caa) is recovered from
 * 0.995.070-TOOL debug strings. Other internal names are ours.
 */

#include "secure_kernel.h"

typedef struct SceSkCapBitEntry {
	uint32_t id;
	uint32_t bitIndex;
} SceSkCapBitEntry;

/* ============================================================================
 * .rodata (0x807638..0x807868)
 * ============================================================================ */

/* 0x807640: capability bit index in target.cap3 for each hardware target class */
static const SceSkCapBitEntry s_aclibTargetCapBits[5] = {
	{ 0x00000000u, 0xffffffffu },
	{ 0x00000001u, 0x0000001fu },
	{ 0x00000002u, 0x0000001eu },
	{ 0x00000003u, 0x0000001du },
	{ 0x00000004u, 0x0000001fu },
};

/* 0x807668: capability bit index in target.cap3 for each module type (1..25) */
static const SceSkCapBitEntry s_aclibModuleCapBits[26] = {
	{ 0x00000000u, 0xffffffffu },
	{ 0x00000001u, 0x00000041u },
	{ 0x00000002u, 0x00000051u },
	{ 0x00000003u, 0x00000052u },
	{ 0x00000004u, 0x00000053u },
	{ 0x00000005u, 0x00000054u },
	{ 0x00000006u, 0x00000055u },
	{ 0x00000007u, 0x00000042u },
	{ 0x00000008u, 0x00000043u },
	{ 0x00000009u, 0x00000040u },
	{ 0x0000000au, 0x00000050u },
	{ 0x0000000bu, 0x00000056u },
	{ 0x0000000cu, 0x00000044u },
	{ 0x0000000du, 0x00000045u },
	{ 0x0000000eu, 0x00000046u },
	{ 0x0000000fu, 0x00000047u },
	{ 0x00000010u, 0x00000048u },
	{ 0x00000011u, 0x00000049u },
	{ 0x00000012u, 0x0000004au },
	{ 0x00000013u, 0x0000004bu },
	{ 0x00000014u, 0x0000004cu },
	{ 0x00000015u, 0x0000004du },
	{ 0x00000016u, 0x0000004eu },
	{ 0x00000017u, 0x0000005au },
	{ 0x00000018u, 0x0000005bu },
	{ 0x00000019u, 0x00000059u },
};

/* 0x807738: allowlist of 38 0x21000000-series auth IDs permitted on retail */
static const uint64_t s_allowed2100AuthIds[38] = {
	0x2100000e86262a6dULL,
	0x210000101cd36379ULL,
	0x210000101cd3637aULL,
	0x210000101cd3637dULL,
	0x210000101cd3637fULL,
	0x210000101cd36381ULL,
	0x210000101cd36382ULL,
	0x210000101cd36383ULL,
	0x210000101cd36384ULL,
	0x210000101cd36385ULL,
	0x210000101cd36386ULL,
	0x210000101cd36387ULL,
	0x210000101cd36388ULL,
	0x210000101cd36389ULL,
	0x210000101cd3638aULL,
	0x210000101cd3638bULL,
	0x210000101cd3638cULL,
	0x210000101cd3638dULL,
	0x210000101cd3638eULL,
	0x210000101cd3638fULL,
	0x210000101cd36390ULL,
	0x210000101cd36391ULL,
	0x210000101cd36392ULL,
	0x210000101cd36393ULL,
	0x210000101cd36394ULL,
	0x210000101cd36395ULL,
	0x210000101cd36396ULL,
	0x210000101cd36397ULL,
	0x210000101cd36398ULL,
	0x210000101cd36399ULL,
	0x210000101cd3639aULL,
	0x210000101cd3639bULL,
	0x210000101cd3639cULL,
	0x210000101cd3639dULL,
	0x210000101cd3639eULL,
	0x210000101cd3639fULL,
	0x210000101cd363a0ULL,
	0x210000101cd363a1ULL,
};

/* ============================================================================
 * Functions (0x804c3e..0x8050e1)
 * ============================================================================ */

/* 0x804c3e (name ours) */
int aclibLoadSrvk(const SceSkBufferDesc *srvkSrcDesc)
{
	uint32_t rangesPaddr = srvkSrcDesc->addr;
	uint32_t numRanges = srvkSrcDesc->size;
	SceSkBufferDesc dstRegion;
	SceSkBufferDesc loadedDesc;
	int status;

	dstRegion.addr = getSkSrvkSramBase();
	dstRegion.size = getSkSrvkSramSize();
	loadedDesc.addr = 0xffffffffu;
	loadedDesc.size = 0xffffffffu;

	status = srvkLoadAndVerify(rangesPaddr, numRanges, &dstRegion, &loadedDesc);
	if (status == 0) {
		setSrvkListAddr(loadedDesc.addr);
		setSrvkListSize(loadedDesc.size);
	}
	return status;
}

/* 0x804c8a (name ours) */
int aclibEqual16(const uint8_t *a, const uint8_t *b)
{
	uint8_t diff = 0u;
	uint32_t i;

	for (i = 0u; i < 16u; i++)
		diff |= a[i] ^ b[i];
	return (diff == 0u);
}

/* 0x804caa (Sony name from 0.995.070-TOOL) */
int sceSblACLibIsLoadable(const SceSkSmAuthContext *authCtx,
		const SceSkSelfAuthInfo *targetSelfInfo, const SceSkBufferDesc *srvkDesc)
{
	SceSkBufferDesc modulePair;
	uint64_t hwVer;
	uint32_t targetClass;
	uint32_t modType;
	uint32_t bitIdx;
	int modStatus;
	int qafBit0;
	int qaByte13Bit0;
	int qaByte15Bit0;
	uint8_t cap3Byte0;
	uint8_t cap3Byte4;
	uint8_t callerCap1Byte0;
	uint8_t callerAllow80;
	uint32_t addrMode;
	uint32_t attrFlags;
	uint32_t loadSrc;
	uint32_t keyring517;
	uint64_t targetAuthId;
	uint32_t authPrefix;
	uint64_t reqSysVer;
	int status;

	if (authCtx == NULL || targetSelfInfo == NULL || srvkDesc == NULL)
		return (int)0x800f0b16;

	if (checkRangeInSkSram((uint32_t)(uintptr_t)authCtx, sizeof(*authCtx)) == 0 ||
	    checkRangeInSkSram((uint32_t)(uintptr_t)targetSelfInfo, sizeof(*targetSelfInfo)) == 0 ||
	    checkRangeInSkSram((uint32_t)(uintptr_t)srvkDesc, sizeof(*srvkDesc)) == 0) {
		skPanic("%08x\n", 1u);
	}

	modulePair.addr = (uint32_t)(uintptr_t)authCtx;
	modulePair.size = (uint32_t)(uintptr_t)targetSelfInfo;
	status = srvkCheckModuleRevoked(&modulePair, srvkDesc);
	if (status != 0)
		return status;

	/* Target hardware class capability check */
	targetClass = (uint32_t)srvkGetTargetTypeClass();
	if (!(targetClass == 4u &&
	      ((authCtx->target.cap3[3] & 1u) != 0u ||
	       (authCtx->target.cap3[3] & 2u) != 0u))) {
		if (targetClass - 1u > 3u)
			return (int)0x800f0b31;
		bitIdx = s_aclibTargetCapBits[targetClass].bitIndex;
		if ((authCtx->target.cap3[bitIdx >> 3] & (1u << (~bitIdx & 7u))) == 0u)
			return (int)0x800f0b31;
	}

	/* Module type capability check */
	modType = authCtx->moduleType;
	if (modType - 1u > 24u)
		return (int)0x800f0b32;

	bitIdx = s_aclibModuleCapBits[modType].bitIndex;
	if ((authCtx->target.cap3[bitIdx >> 3] & (1u << (~bitIdx & 7u))) == 0u)
		modStatus = (int)0x800f0b32;
	else
		modStatus = 0;

	if (modType == 7u) {
		int qaAllow;

		loadSrc = authCtx->attrFlags & 0xfu;
		if (loadSrc == 1u) {
			qaAllow = srvkCheckQaFlagByte13Bit1();
		} else if (loadSrc == 0u || loadSrc == 2u) {
			qaAllow = srvkCheckQaFlagByte14Bit0();
		} else {
			return (int)0x800f0b38;
		}
		if (qaAllow == 0 && modStatus != 0)
			return modStatus;
	} else if (modStatus != 0) {
		return modStatus;
	}

	/* QAF / Dipsw / QA flag restrictions in target.cap3[4] */
	qafBit0 = srvkCheckQafByte0Bit0();
	cap3Byte4 = authCtx->target.cap3[4];
	if ((cap3Byte4 & 0x40u) != 0u && qafBit0 != 0)
		return (int)0x800f0b33;
	if ((cap3Byte4 & 0x80u) != 0u && qafBit0 == 0)
		return (int)0x800f0b33;
	if ((cap3Byte4 & 0x08u) != 0u && srvkCheckDipswBit197() == 0)
		return (int)0x800f0b33;

	qaByte13Bit0 = srvkCheckQaFlagByte13Bit0();
	if ((authCtx->target.cap3[4] & 0x10u) != 0u && qaByte13Bit0 != 0)
		return (int)0x800f0b33;
	if ((authCtx->target.cap3[4] & 0x20u) != 0u && qaByte13Bit0 == 0)
		return (int)0x800f0b33;

	qaByte15Bit0 = srvkCheckQaFlagByte15Bit0();
	if ((authCtx->target.cap3[4] & 0x04u) != 0u && qaByte15Bit0 == 0)
		return (int)0x800f0b33;

	/* Caller vs target authority & shared secret verification */
	cap3Byte0 = authCtx->target.cap3[0];
	callerCap1Byte0 = authCtx->caller.cap1[0];
	callerAllow80 = ((int8_t)callerCap1Byte0 < 0) ? 1u : ((callerCap1Byte0 >> 6) & 1u);

	if (((cap3Byte0 & 0x80u) == 0u || callerAllow80 == 0u) &&
	    ((cap3Byte0 & 0x40u) == 0u || (callerCap1Byte0 & 0x20u) == 0u) &&
	    ((cap3Byte0 & 0x20u) == 0u || (callerCap1Byte0 & 0x10u) == 0u) &&
	    (int8_t)authCtx->caller.cap1[4] >= 0 &&
	    authCtx->caller.authId != authCtx->target.authId) {
		uint8_t secretOr = 0u;
		uint32_t i;

		for (i = 0u; i < 16u; i++)
			secretOr |= authCtx->caller.sharedSecret0[i];
		if (secretOr == 0u)
			return (int)0x800f0b34;

		if (aclibEqual16(authCtx->caller.sharedSecret0, targetSelfInfo->sharedSecret[0]) == 0 &&
		    aclibEqual16(authCtx->caller.sharedSecret0, targetSelfInfo->sharedSecret[1]) == 0 &&
		    aclibEqual16(authCtx->caller.sharedSecret0, targetSelfInfo->sharedSecret[2]) == 0 &&
		    aclibEqual16(authCtx->caller.sharedSecret0, targetSelfInfo->sharedSecret[3]) == 0) {
			return (int)0x800f0b34;
		}
	}

	/* Fixed-exec / address space mode restrictions */
	addrMode = authCtx->attrFlags & 0x000f0000u;
	if (((authCtx->target.cap3[2] & 0x20u) != 0u && addrMode == 0x00010000u) ||
	    ((authCtx->attrFlags & 0x20u) != 0u &&
	     (authCtx->target.cap3[2] & 0x40u) != 0u &&
	     (authCtx->caller.cap1[4] & 0x40u) != 0u &&
	     (srvkIsRetailOrToolTarget() != 0 || addrMode != 0x00010000u))) {
		return (int)0x800f0b35;
	}

	/* Load media source check */
	attrFlags = authCtx->attrFlags;
	loadSrc = attrFlags & 0xfu;
	if (loadSrc == 1u) {
		if ((authCtx->target.cap3[12] & 0x40u) == 0u)
			return (int)0x800f0b38;
	} else if (loadSrc == 0u) {
		if ((int8_t)authCtx->target.cap3[12] >= 0)
			return (int)0x800f0b38;
	} else if (loadSrc == 2u) {
		if ((authCtx->target.cap3[12] & 0x20u) == 0u)
			return (int)0x800f0b38;
	} else {
		return (int)0x800f0b38;
	}

	/* Auth ID prefix & retail allowlist checks */
	keyring517 = srvkReadKeyring517Word0();
	targetAuthId = authCtx->target.authId;
	if (srvkIsDevelopmentTarget() == 0 && srvkIsRetailOrToolTarget() == 0) {
		if ((attrFlags & 0x10u) != 0u && (attrFlags & 0x0f00u) == 0x0100u)
			return (int)0x800f0b3a;

		authPrefix = (uint32_t)(targetAuthId >> 32) & 0xfff00000u;
		if (authPrefix == 0x21000000u) {
			if ((keyring517 & 2u) == 0u) {
				uint32_t k;
				int matched = 0;

				for (k = 0u; k < 38u; k++) {
					if (s_allowed2100AuthIds[k] == targetAuthId) {
						matched = 1;
						break;
					}
				}
				if (!matched)
					return (int)0x800f0b3a;
			}
		} else if (authPrefix == 0x2f000000u && (keyring517 & 1u) == 0u) {
			return (int)0x800f0b3a;
		}
	}

	if ((int8_t)authCtx->target.cap3[16] < 0 &&
	    srvkGetTargetTypeClass() != 1 &&
	    srvkGetTargetTypeClass() != 3 &&
	    srvkCheckQaFlagByte11Bit4() == 0) {
		return (int)0x800f0b33;
	}

	/* Required system version check */
	authPrefix = (uint32_t)(authCtx->target.authId >> 32) & 0xfff00000u;
	reqSysVer = targetSelfInfo->requiredSysVersion;
	getHardwareSocVersionPair((uint32_t *)&hwVer);
	if ((authPrefix != 0x21000000u && authPrefix != 0x2f000000u && reqSysVer == 0ULL) ||
	    reqSysVer <= hwVer) {
		uint32_t mType = authCtx->moduleType;
		uint64_t minVer = authCtx->target.minVersion;

		getHardwareSocVersionPair((uint32_t *)&hwVer);
		if (mType - 13u > 1u)
			return 0;
		if (minVer <= hwVer && minVer != 0ULL)
			return 0;
	}
	return (int)0x800f0b37;
}
