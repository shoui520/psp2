/*
 * spkg_verifier_sm_w_key_2.h: Internal types, structures, and function
 * declarations for PlayStation Vita spkg_verifier_sm_w_key_2.self.
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#ifndef SPKG_VERIFIER_SM_W_KEY_2_H
#define SPKG_VERIFIER_SM_W_KEY_2_H

#include "sm_common.h"

/* ============================================================================
 * SPKG Error Codes & Command IDs
 * ============================================================================ */

#define SCE_SBL_SPKG_CMD_VERIFY			0x000e0002u

#define SCE_SBL_ERROR_SPKG_OUT_OF_RANGE		((int)0x800f0202u)
#define SCE_SBL_ERROR_SPKG_INVALID_VALUE	((int)0x800f0216u)
#define SCE_SBL_ERROR_SPKG_INVALID_ADDR		((int)0x800f0222u)
#define SCE_SBL_ERROR_SPKG_BAD_HEADER		((int)0x800f0224u)
#define SCE_SBL_ERROR_SPKG_PREPARE_FAILED	((int)0x800f0226u)
#define SCE_SBL_ERROR_SPKG_FACTORY_DENIED	((int)0x800f0227u)
#define SCE_SBL_ERROR_SPKG_TARGET_MISMATCH	((int)0x800f0231u)
#define SCE_SBL_ERROR_SPKG_VERSION_FLOOR	((int)0x800f0236u)
#define SCE_SBL_ERROR_SPKG_HW_REV_FLOOR		((int)0x800f0237u)

/* ============================================================================
 * SPKG Structures (names ours)
 * ============================================================================ */

typedef struct SpkgExtHeader {
	uint32_t magic;				/* 0x00: 4 */
	uint32_t pkgType;			/* 0x04: 1 or 9 */
	uint32_t flags08;			/* 0x08 */
	uint32_t rsvd0c;			/* 0x0c */
	uint32_t version;			/* 0x10 */
	uint32_t rsvd14[5];			/* 0x14..0x27 */
	uint32_t qaFlags;			/* 0x28 */
	uint32_t targetFlags;			/* 0x2c */
	uint32_t rsvd30[4];			/* 0x30..0x3f */
} SpkgExtHeader;

_Static_assert(sizeof(SpkgExtHeader) == 0x40, "SpkgExtHeader size");

typedef struct SpkgScatterListDesc {
	const SceSmRangeEntry *entries;		/* 0x00 */
	uint32_t byteSize;			/* 0x04 */
} SpkgScatterListDesc;

typedef struct SpkgVerifyRequest {
	uint32_t rsvd00[2];			/* 0x000..0x007 */
	uint64_t mode;				/* 0x008..0x00f: 1 = multi-list */
	uint32_t rsvd10[3];			/* 0x010..0x01b */
	uint32_t listCount;			/* 0x01c: <= 481 */
	uint32_t rsvd20[4];			/* 0x020..0x02f */
	uint32_t inlineEntryCount;		/* 0x030 */
	uint32_t rsvd34;			/* 0x034 */
	union {
		SpkgScatterListDesc lists[481];	/* 0x038..0xf3f */
		SceSmRangeEntry inlineEntries[481];
	} u;
	SpkgExtHeader decryptedExtHdr1;		/* 0xf40..0xf7f */
	uint8_t decryptedSeg2[64];		/* 0xf80..0xfbf */
} SpkgVerifyRequest;

_Static_assert(sizeof(SpkgVerifyRequest) == 0xfc0, "SpkgVerifyRequest size");

typedef struct SpkgStreamCbContext {
	SpkgVerifyRequest *req;			/* 0x00 */
	uint32_t rsvd04;			/* 0x04 */
	uint64_t listIdx;			/* 0x08..0x0f */
	uint32_t segType;			/* 0x10 */
	uint32_t segIdx;			/* 0x14 */
} SpkgStreamCbContext;

_Static_assert(sizeof(SpkgStreamCbContext) == 0x18, "SpkgStreamCbContext size");

typedef struct SpkgHwRevRule {
	uint32_t minVer;			/* 0x00 */
	uint32_t minVerAlt;			/* 0x04 */
	uint32_t attrMask;			/* 0x08 */
} SpkgHwRevRule;

_Static_assert(sizeof(SpkgHwRevRule) == 0x0c, "SpkgHwRevRule size");

typedef struct SpkgHwRevMapEntry {
	uint16_t minProdCode;			/* 0x00 */
	uint16_t maxProdCode;			/* 0x02 */
	uint16_t subCode;			/* 0x04 */
	uint16_t pad06;				/* 0x06 */
	uint32_t ruleIdx;			/* 0x08 */
} SpkgHwRevMapEntry;

_Static_assert(sizeof(SpkgHwRevMapEntry) == 0x0c, "SpkgHwRevMapEntry size");

typedef struct SpkgSegmentSpec {
	uint32_t segType;			/* 0x00 */
	uint32_t segIdx;			/* 0x04 */
} SpkgSegmentSpec;

typedef struct SpkgVerifier {
	const void *vptr;			/* 0x00 */
	SpkgVerifyRequest *req;			/* 0x04 */
	uint32_t isPrepared;			/* 0x08 */
} SpkgVerifier;

_Static_assert(sizeof(SpkgVerifier) == 0x0c, "SpkgVerifier size");

/* ============================================================================
 * Function Declarations (names ours except sm_main)
 * ============================================================================ */

int sm_main(void);
void spkgHookInvoke(void *self, void *commBuf);
void spkgStaticInitOrDtor(int isInit);
void spkgCmdVerifyHeaderAndSegments(void *commBuf);
void spkgStaticInitThunk(void);
void spkgStaticDtorThunk(void);
void spkgReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
int spkgValidateArmRange(uint32_t addr, uint32_t size);
void spkgVerifierDtorD2(SpkgVerifier *v);
void spkgVerifierDtorD0(SpkgVerifier *v);
void spkgVerifierCtorC2(SpkgVerifier *v);
void spkgVerifierCtorC1(SpkgVerifier *v);
void spkgVerifierDtorD1(SpkgVerifier *v);
void spkgVerifierCleanup(SpkgVerifier *v);
int spkgCheckCfHeader(SpkgVerifier *v, const void *cfHdr);
int spkgCheckExtHeaderMagic(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgCheckMinVersionFloor(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgReturnZero(void);
int spkgFindRangeEntry(const SceSmRangeEntry *entries, uint32_t count,
		const uint64_t *targetOffset, uint32_t *outIdx,
		uint32_t *outAccumLow, uint64_t *outInEntryOffset);
int spkgReadSegmentCallback(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg);
int spkgCopyFromScatterEntries(const SceSmRangeEntry *entries, uint32_t count,
		uintptr_t dstAddr, const uint64_t *offset, uint32_t size,
		uint32_t *outRead);
int spkgReadHeaderCallback(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg);
int spkgReadPlainHeaderBytes(void *dst, SpkgVerifyRequest *req,
		uint32_t offset, uint32_t size);
int spkgValidateUnencryptedHeaders(SpkgVerifier *v, SpkgVerifyRequest *req);
int spkgVerifierPrepare(SpkgVerifier *v, SpkgVerifyRequest *req);
int spkgCheckTargetCompatibility(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgLookupHardwareRevEntry(SpkgVerifier *v, uint32_t *outIdx);
uint32_t spkgGetHardwareAttrMask(SpkgVerifier *v, uint32_t idx);
int spkgIsVersionBelowHwMin(SpkgVerifier *v, uint32_t pkgType,
		uint32_t pkgVer, uint32_t defaultMinVer);
int spkgCheckPkgVersionAndChassis(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgCheckFactoryUpdatePolicy(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgVerifierExecute(SpkgVerifier *v);
uint32_t spkgIsQaFlagSet(void);

#endif /* SPKG_VERIFIER_SM_W_KEY_2_H */
