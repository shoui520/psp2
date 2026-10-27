/*
 * self_loader_wrapper.c: SELF authority / capability / attribute / auth-info
 * extraction helpers for kprx_auth_sm.self (0x80eb9e..0x80ef02, .rodata
 * 0x810748..0x810808).
 *
 * Sony C++ method name recovered from 0.995.070-TOOL debug strings:
 *   - self_loader_wrapper::get_all_self_auth_info (0x80eec6)
 *
 * Other internal function and table names are ours.
 */

#include "kprx_auth_sm.h"
#include <modulemgr/elf.h>

/* ============================================================================
 * .rodata (0x810748..0x810808): Development SELF Default Capability Templates
 * ============================================================================ */

/* 0x810748: development capability template for authId 0x2f00000000000002 (normal) */
static const uint8_t s_devCapAuth2Normal[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x0fu, 0x00u,
	0x46u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
};

/* 0x810768: development capability template for authId 0x2f00000000000002 (QA) */
static const uint8_t s_devCapAuth2Qa[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x0fu, 0x00u,
	0x66u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
};

/* 0x810788: development attribute template for authId 0x2f00000000000002 */
static const uint8_t s_devAttrAuth2[32] = {
	0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xf0u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

/* 0x8107a8: development capability template for authId 0x2f00000000000001 (normal) */
static const uint8_t s_devCapAuth1Normal[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x33u, 0x00u,
	0x46u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
};

/* 0x8107c8: development capability template for authId 0x2f00000000000001 (QA) */
static const uint8_t s_devCapAuth1Qa[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x33u, 0x00u,
	0x66u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
};

/* 0x8107e8: development attribute template for authId 0x2f00000000000001 */
static const uint8_t s_devAttrAuth1[32] = {
	0x20u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0fu, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

/* ============================================================================
 * SELF Authority / AuthInfo Extraction Functions (0x80eb9e..0x80ef02)
 * ============================================================================ */

/* 0x80eb9e (name ours) */
int selfExtractKblParam(const SelfAuthContext *ctx,
		SceSelfKblParam *outParam, int useQaCap)
{
	const uint8_t *appInfo = NULL;
	const uint8_t *extHdr = NULL;
	const uint8_t *ctrlInfo7 = NULL;
	uint8_t devAttr[32];
	uint8_t devCap[32];
	uint64_t authId;
	const uint8_t *src;

	memset(devAttr, 0, sizeof(devAttr));
	memset(devCap, 0, sizeof(devCap));

	if (selfGetAppInfo(ctx, &appInfo) != 0)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	authId = *(const uint64_t *)appInfo;
	*(uint64_t *)&outParam->progAttr = authId;

	if ((int8_t)ctx->hdrBuf[8] < 0) {
		const uint8_t *capTemplate;

		if (authId == 0x2f00000000000001ULL) {
			memcpy(devAttr, s_devAttrAuth1, 32u);
			capTemplate = (useQaCap == 0) ?
				      s_devCapAuth1Normal : s_devCapAuth1Qa;
		} else if (authId == 0x2f00000000000002ULL) {
			memcpy(devAttr, s_devAttrAuth2, 32u);
			capTemplate = (useQaCap == 0) ?
				      s_devCapAuth2Normal : s_devCapAuth2Qa;
		} else {
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
		memcpy(devCap, capTemplate, 32u);
	}

	if ((int8_t)ctx->hdrBuf[8] < 0) {
		src = devAttr;
	} else {
		if (selfFindExtHeader(ctx, &extHdr, 1u) != 0)
			return SCE_SBL_ERROR_CF_INVALID_VALUE;
		src = extHdr + 0x10u;
	}
	memcpy(outParam->attr, src, 32u);

	if ((int8_t)ctx->hdrBuf[8] < 0) {
		src = devCap;
	} else {
		if (selfFindExtHeader(ctx, &extHdr, 3u) != 0)
			return SCE_SBL_ERROR_CF_INVALID_VALUE;
		src = extHdr + 0x10u;
	}
	memcpy(outParam->capability, src, 32u);

	if ((int8_t)ctx->hdrBuf[8] < 0 ||
	    selfFindControlInfo(ctx, &ctrlInfo7, 7u) != 0) {
		memset(outParam->sharedSecret0, 0, 16u);
	} else {
		memcpy(outParam->sharedSecret0, ctrlInfo7 + 0x10u, 16u);
	}
	return 0;
}

/* 0x80ed10 (name ours) */
int selfExtractSelfAuthInfo(const SelfAuthContext *ctx, SceSmSelfAuthInfo *outInfo)
{
	const uint8_t *appInfo = NULL;
	const uint8_t *ctrlInfo = NULL;

	outInfo->size = 0x80u;
	outInfo->version = 1u;

	if (selfGetAppInfo(ctx, &appInfo) != 0)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	outInfo->programAuthorityId = *(const uint64_t *)(appInfo + 0x10u);

	if ((int8_t)ctx->hdrBuf[8] < 0) {
		uint32_t socVer;

		memset(outInfo->digest, 0, 32u);
		socVer = (uint32_t)REG8(0xe00621c0u) |
			 ((uint32_t)REG8(0xe00621c1u) << 8) |
			 ((uint32_t)REG8(0xe00621c2u) << 16) |
			 ((uint32_t)REG8(0xe00621c3u) << 24);
		outInfo->minSysver = ((uint64_t)(socVer >> 16) << 32) |
				     (uint64_t)(socVer << 16);
	} else {
		if (selfFindControlInfo(ctx, &ctrlInfo, 4u) != 0)
			return SCE_SBL_ERROR_CF_INVALID_VALUE;
		memcpy(outInfo->digest, ctrlInfo + 0x24u, 32u);
		outInfo->minSysver = *(const uint64_t *)(ctrlInfo + 0x48u);
	}

	if ((int8_t)ctx->hdrBuf[8] < 0 ||
	    selfFindControlInfo(ctx, &ctrlInfo, 7u) != 0) {
		memset(outInfo->secret0, 0, 16u);
		memset(outInfo->secret1, 0, 16u);
		memset(outInfo->secret2, 0, 16u);
		memset(outInfo->secret3, 0, 16u);
	} else {
		memcpy(outInfo->secret0, ctrlInfo + 0x10u, 16u);
		memcpy(outInfo->secret1, ctrlInfo + 0x20u, 16u);
		memcpy(outInfo->secret2, ctrlInfo + 0x30u, 16u);
		memcpy(outInfo->secret3, ctrlInfo + 0x40u, 16u);
	}
	return 0;
}

/* 0x80ee76 (name ours) */
int selfExtractAttrFlags(const SelfAuthContext *ctx, uint32_t *inOutFlags)
{
	const Elf32_Ehdr *ehdr = NULL;
	int status;

	status = selfGetElfHeader(ctx, (const void **)&ehdr);
	if (status == 0) {
		uint32_t flags = (ehdr->e_type == ET_SCE_FIXEDEXEC) ? 0x20u : 0u;

		if ((int8_t)ctx->hdrBuf[8] < 0)
			flags |= 0x10u;
		*inOutFlags = (*inOutFlags & ~0xf0u) | flags;
	}
	return status;
}

/* 0x80eec6 (name ours; Sony method self_loader_wrapper::get_all_self_auth_info) */
int selfLoaderWrapperGetAllSelfAuthInfo(const SelfAuthContext *ctx,
		SceSelfKblParam *outKblParam, int useQaCap,
		SceSmSelfAuthInfo *outAuthInfo, uint32_t *inOutFlags)
{
	if (selfExtractKblParam(ctx, outKblParam, useQaCap) == 0 &&
	    selfExtractSelfAuthInfo(ctx, outAuthInfo) == 0 &&
	    selfExtractAttrFlags(ctx, inOutFlags) == 0) {
		return 0;
	}
	return SCE_SBL_ERROR_CF_INVALID_VALUE;
}
