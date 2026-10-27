/*
 * self_loader.c: Secure Module (SM) Certified File (v3) / SELF header
 * authentication, metadata decryption, scatter-gather segment loading, and
 * authority / SELF auth info extraction (0x802926..0x804c3d).
 *
 * Sony C++ / C helper names recovered from 0.995.070-TOOL debug strings:
 *   - self_loader::open                       (0x803b18, selfInitAndVerifyHeader)
 *   - self_loader::verify_header              (0x803f9c, selfVerifySvnAndDecryptMetadata)
 *   - self_loader::get_elf_header             (0x804450, selfGetElfHeader)
 *   - self_loader::get_program_segment_info   (0x80446a / 0x804494)
 *   - self_loader::load_segment               (0x803d84, selfDecryptAndLoadSegment)
 *   - self_loader_wrapper::get_all_self_auth_info (0x8049aa)
 *
 * Uses cf_header, SceSelfHeader, SceSelfSegmentInfo, Elf32_Ehdr, and Elf32_Phdr
 * from sys/internal/include/modulemgr/{certified_file.h,self.h,elf.h}.
 * Other internal helper and structure names are ours.
 */

#include "secure_kernel.h"
#include <modulemgr/certified_file.h>
#include <modulemgr/self.h>
#include <modulemgr/elf.h>

typedef struct SceSelfSegmentInfo {
	uint64_t offset;			/* 0x00 */
	uint64_t length;			/* 0x08 */
	uint32_t compression;			/* 0x10 */
	uint32_t encryption;			/* 0x14 */
	uint64_t reserved;			/* 0x18 */
} SceSelfSegmentInfo;

_Static_assert(sizeof(SceSelfSegmentInfo) == 0x20, "SceSelfSegmentInfo");

/* ============================================================================
 * Certified File (CF) v3 Metadata & Internal Structures (names ours)
 * ============================================================================ */

typedef struct SceCfMetadataHeader {
	uint64_t signatureInputLength;		/* 0x00 */
	uint32_t signatureType;			/* 0x08: 5 = RSA-2048 */
	uint32_t sectionCount;			/* 0x0c */
	uint32_t keyCount;			/* 0x10: count of 16-byte keyPool slots */
	uint32_t optHeaderSize;			/* 0x14 */
	uint64_t reserved;			/* 0x18 */
} SceCfMetadataHeader;

_Static_assert(sizeof(SceCfMetadataHeader) == 0x20, "SceCfMetadataHeader");

typedef struct SceCfMetadataSection {
	uint64_t offset;			/* 0x00 */
	uint64_t size;				/* 0x08 */
	uint32_t type;				/* 0x10: 2 = ELF PT_LOAD segment */
	uint32_t index;				/* 0x14: program header index */
	uint32_t hashAlgorithm;			/* 0x18: 6 = HMAC-SHA256 */
	uint32_t hashKeyIndex;			/* 0x1c: index in 16-byte keyPool */
	uint32_t encryptionAlgorithm;		/* 0x20: 1 = none, 3 = AES-128-CTR */
	uint32_t keyIndex;			/* 0x24: index in 16-byte keyPool */
	uint32_t ivIndex;			/* 0x28: index in 16-byte keyPool */
	uint32_t compressionAlgorithm;		/* 0x2c: 1 = plain, 2 = compressed */
} SceCfMetadataSection;

_Static_assert(sizeof(SceCfMetadataSection) == 0x30, "SceCfMetadataSection");

typedef struct CfHmacContext {
	uint32_t state;				/* 0x00 */
	uint8_t iv[40];				/* 0x04 */
	uint8_t digest[32];			/* 0x2c */
	uint8_t blockBuf[64];			/* 0x4c */
	uint32_t bufUsed;			/* 0x8c */
	uint8_t hmacKey[64];			/* 0x90 */
	uint32_t keyLen;			/* 0xd0 */
} CfHmacContext;

_Static_assert(sizeof(CfHmacContext) == 0xd4, "CfHmacContext");

typedef struct CfStaticKeyParams {
	const void *metaKey;			/* 0x00 */
	const void *rootPubKey;			/* 0x04 */
	uint32_t sigType;			/* 0x08 */
} CfStaticKeyParams;

typedef int (*CfGetKeysCallback)(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg);

typedef struct CfSegmentDecryptState {
	const SelfAuthContext *selfCtx;		/* 0x00 */
	uint32_t reserved04[3];			/* 0x04 */
	SceCfMetadataSection section;		/* 0x10 */
	SelfSegmentReadCallback readCb;		/* 0x40 */
	void *readCbArg;			/* 0x44 */
	SelfSegmentDoneCallback doneCb;		/* 0x48 */
	void *doneCbArg;			/* 0x4c */
	uint64_t curOffset;			/* 0x50 */
	uint64_t remSize;			/* 0x58 */
	uint8_t headBlock[16];			/* 0x60 */
	uint32_t headLen;			/* 0x70 */
	const uint8_t *expectedDigest;		/* 0x74 */
	CfHmacContext hmacCtx;			/* 0x78 */
	const uint8_t *cipherKey;		/* 0x14c */
	uint8_t cipherIv[16];			/* 0x150 */
} CfSegmentDecryptState;

_Static_assert(offsetof(CfSegmentDecryptState, section) == 0x10, "CfSegmentDecryptState section");
_Static_assert(offsetof(CfSegmentDecryptState, readCb) == 0x40, "CfSegmentDecryptState readCb");
_Static_assert(offsetof(CfSegmentDecryptState, curOffset) == 0x50, "CfSegmentDecryptState curOffset");
_Static_assert(offsetof(CfSegmentDecryptState, headBlock) == 0x60, "CfSegmentDecryptState headBlock");
_Static_assert(offsetof(CfSegmentDecryptState, hmacCtx) == 0x78, "CfSegmentDecryptState hmacCtx");
_Static_assert(offsetof(CfSegmentDecryptState, cipherKey) == 0x14c, "CfSegmentDecryptState cipherKey");
_Static_assert(offsetof(CfSegmentDecryptState, cipherIv) == 0x150, "CfSegmentDecryptState cipherIv");

typedef struct SelfScatterSegmentCbArg {
	const SceSkLoadSmCmdArgs *cmdArgs;	/* 0x00 */
	uint32_t dstVaddr;			/* 0x04 */
	uint32_t filesz;			/* 0x08 */
	uint32_t pad0c;				/* 0x0c */
	uint64_t segOffset;			/* 0x10 */
	uint64_t segSize;			/* 0x18 */
} SelfScatterSegmentCbArg;

_Static_assert(sizeof(SelfScatterSegmentCbArg) == 0x20, "SelfScatterSegmentCbArg size");

/* ============================================================================
 * .rodata (0x8074dc..0x807638)
 * ============================================================================ */

/* 0x8074dc: signature size table indexed by signatureType */
static const uint32_t s_cfSigSizes[8] = {
	0xffffffffu, 0x30u, 0x20u, 0x20u, 0x10u, 0x100u, 0x20u, 0x20u
};

/* 0x8074fc: keyPool slot count required by hashAlgorithm */
static const uint32_t s_cfHashKeySlots[8] = {
	0xffffffffu, 7u, 6u, 2u, 2u, 0x21u, 4u, 2u
};

/* 0x80751c: keyPool slot count required by encryptionAlgorithm IV */
static const uint32_t s_cfEncIvSlots[6] = {
	0xffffffffu, 0u, 1u, 1u, 1u, 1u
};

/* 0x807534: keyPool slot count required by encryptionAlgorithm key */
static const uint32_t s_cfEncKeySlots[6] = {
	0xffffffffu, 0u, 1u, 1u, 2u, 2u
};

/* 0x80754c: standard 20-byte constant in SceSelfControlInfo type 4 */
static const uint8_t s_selfControlDigestConstant[20] = {
	0x62u, 0x7cu, 0xb1u, 0x80u, 0x8au, 0xb9u, 0x38u, 0xe3u,
	0x2cu, 0x8cu, 0x09u, 0x17u, 0x08u, 0x72u, 0x6au, 0x57u,
	0x9eu, 0x25u, 0x86u, 0xe4u,
};

/* 0x807560..0x807620: default capability blocks for development SM SELFs */
static const uint8_t s_devCap3Auth2NoFlag[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x0fu, 0x00u,
	0x46u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
};

static const uint8_t s_devCap3Auth2WithFlag[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x0fu, 0x00u,
	0x66u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
};

static const uint8_t s_devCap1Auth2[32] = {
	0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xf0u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

static const uint8_t s_devCap3Auth1NoFlag[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x33u, 0x00u,
	0x46u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
};

static const uint8_t s_devCap3Auth1WithFlag[32] = {
	0x60u, 0x07u, 0x40u, 0x06u, 0x40u, 0x00u, 0x33u, 0x00u,
	0x66u, 0x02u, 0x00u, 0x75u, 0x40u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
};

static const uint8_t s_devCap1Auth1[32] = {
	0x20u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0fu, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* ============================================================================
 * .data (0x8079d4..0x807b10): SM SELF Metadata Key/IV & RSA-2048 Public Key
 * ============================================================================ */

/* 0x8079d4: 32-byte AES-256 key + 16-byte IV for SM SELF metadata decryption */
static uint8_t s_smSelfMetadataKeyIv[48] = {
	0xaeu, 0xd9u, 0xd7u, 0x6eu, 0xe1u, 0xe2u, 0x92u, 0x90u,
	0x00u, 0x2bu, 0xffu, 0x32u, 0xd4u, 0xb0u, 0x65u, 0x6eu,
	0xeeu, 0x40u, 0xfbu, 0xdau, 0x4fu, 0x8bu, 0x55u, 0xbeu,
	0x5bu, 0xe0u, 0xedu, 0x83u, 0x53u, 0x0fu, 0x27u, 0xd2u,
	0xdbu, 0x50u, 0x91u, 0x2fu, 0x24u, 0x16u, 0xb5u, 0x4fu,
	0x7fu, 0x36u, 0x22u, 0x71u, 0x69u, 0xecu, 0xe5u, 0x00u,
};

/* 0x807a04: 256-byte RSA-2048 modulus for SM SELF signature verification */
static uint8_t s_smSelfRsaModulus[256] = {
	0xd9u, 0x70u, 0xe1u, 0xecu, 0xeeu, 0x27u, 0xcbu, 0xa9u,
	0xeau, 0xcau, 0x42u, 0x49u, 0x50u, 0xd7u, 0x4cu, 0x43u,
	0x90u, 0x02u, 0x24u, 0x29u, 0x26u, 0x5du, 0x38u, 0xfdu,
	0x0au, 0xc1u, 0xadu, 0xc2u, 0xfdu, 0xc9u, 0xc5u, 0x69u,
	0xf6u, 0x96u, 0xa8u, 0x57u, 0xf2u, 0xe8u, 0x6du, 0x60u,
	0x9fu, 0x44u, 0x86u, 0x11u, 0xc1u, 0xe2u, 0xffu, 0xabu,
	0x2cu, 0xefu, 0x57u, 0x5bu, 0x92u, 0xdfu, 0x78u, 0x18u,
	0x2bu, 0x2fu, 0x0bu, 0xbfu, 0x3eu, 0xfbu, 0xa6u, 0x2fu,
	0x58u, 0x00u, 0xb2u, 0x2eu, 0xc6u, 0x49u, 0xe2u, 0x87u,
	0x0eu, 0x6bu, 0x08u, 0x29u, 0xecu, 0x9bu, 0xb4u, 0xc9u,
	0x04u, 0xd2u, 0x4au, 0x99u, 0xf2u, 0x69u, 0xc9u, 0x60u,
	0xfau, 0x3eu, 0x12u, 0x31u, 0x4du, 0x8fu, 0x08u, 0xd5u,
	0x12u, 0x4cu, 0x67u, 0x1fu, 0x7cu, 0x92u, 0x90u, 0x0du,
	0xe0u, 0xefu, 0x45u, 0xcbu, 0xb4u, 0x41u, 0x5cu, 0x2du,
	0x1au, 0xfeu, 0x74u, 0xa4u, 0x06u, 0x33u, 0x80u, 0x4au,
	0x05u, 0xd9u, 0xa7u, 0x89u, 0x91u, 0x6eu, 0xdau, 0xb5u,
	0x2au, 0x71u, 0xb6u, 0xddu, 0xcbu, 0x0au, 0xe2u, 0x1bu,
	0x81u, 0x0cu, 0x4fu, 0xa6u, 0x9cu, 0xe5u, 0xaeu, 0x59u,
	0x02u, 0x50u, 0x74u, 0x73u, 0x94u, 0xf8u, 0x8eu, 0x3au,
	0x71u, 0x1au, 0x9au, 0x1du, 0x92u, 0xe6u, 0x58u, 0x57u,
	0x38u, 0x3eu, 0xb0u, 0xf6u, 0xd8u, 0x1du, 0x7au, 0xcdu,
	0x00u, 0x3eu, 0xb2u, 0x03u, 0x3bu, 0x70u, 0x29u, 0xaeu,
	0x44u, 0xe5u, 0x00u, 0xedu, 0x82u, 0x17u, 0x9fu, 0x3bu,
	0x1du, 0xc3u, 0x7eu, 0xe8u, 0x9eu, 0x2du, 0x6du, 0xf5u,
	0xa5u, 0x78u, 0x43u, 0x39u, 0xf7u, 0x14u, 0xc1u, 0x28u,
	0x13u, 0x92u, 0xbau, 0xdeu, 0xc7u, 0x80u, 0xf5u, 0x69u,
	0xcbu, 0x0du, 0x8au, 0xd4u, 0x1cu, 0xf1u, 0x56u, 0x8du,
	0x7bu, 0x7du, 0xfdu, 0x96u, 0xf9u, 0x75u, 0xc0u, 0x7cu,
	0xf5u, 0x36u, 0x15u, 0x76u, 0x6eu, 0x6au, 0xcbu, 0x7cu,
	0x70u, 0xc5u, 0x12u, 0x5bu, 0xfcu, 0xd9u, 0xe9u, 0xedu,
	0x21u, 0x9bu, 0xbcu, 0xdcu, 0x34u, 0xbfu, 0x4bu, 0x42u,
	0x0eu, 0xcfu, 0x4au, 0x62u, 0xaau, 0x18u, 0x5du, 0x47u,
};

/* 0x807b04: modulus size (256), exponent (65537 BE), exponent size (4) */
static uint32_t s_smSelfRsaModulusSize __attribute__((used)) = 0x100u;
static uint8_t s_smSelfRsaExponent[4] = { 0x00u, 0x01u, 0x00u, 0x01u };
static uint32_t s_smSelfRsaExponentSize __attribute__((used)) = 4u;

/* Forward declarations of internal helpers */
SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx);
SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx);
uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx);
int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount);
int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection);
int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint32_t **outExtHdr, uint32_t *outMetaOffset);
int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptHdrSize);
int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size,
		const void *key, const void *ivIn, void *ivOut, void *unusedDesc);
int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc);
int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen);
int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const uint32_t *pubKeyWords, const uint8_t *digest, uint32_t hashSubType);

/* ============================================================================
 * Scatter-Gather SM SELF Loader Wrapper (0x802926..0x802e8f)
 * ============================================================================ */

/* 0x802926 (name ours; Sony class self_loader_wrapper) */
int selfLoaderWrapperLoadSm(const SceSkLoadSmCmdArgs *cmdArgs, SceSkSmAuthContext *authCtx)
{
	uint32_t smSramEnd = getSmSramEnd();
	uint64_t expectedHeaderLen;
	SelfScatterSegmentCbArg segCbArg;
	SceSkBufferDesc srvkDesc;
	SelfAuthContext selfCtx;
	const Elf32_Phdr *phdrs = NULL;
	const Elf32_Ehdr *ehdr = NULL;
	const SceSelfSegmentInfo *segInfos = NULL;
	const SceSkLoadSmCmdArgs *hdrCbArg;
	struct {
		uint8_t modulus[256];
		uint8_t exponent[16];
	} rsaPubKey;
	SceSkSelfAuthInfo selfInfo;
	uint16_t pIdx;
	int status;

	if (cmdArgs->numRanges == 0u || cmdArgs->rangesPaddr == 0u)
		return 0x8016;

	selfCtx.scratchBuf = (void *)(uintptr_t)(smSramEnd - 0x800u);
	selfCtx.scratchSize = 0x800u;

	memcpy(rsaPubKey.modulus, s_smSelfRsaModulus, 0x100u);
	memcpy(rsaPubKey.exponent, s_smSelfRsaExponent, 4u);

	if ((authCtx->attrFlags & 0xf000u) == 0x1000u)
		BIGMAC->memMode = 3u;

	expectedHeaderLen = 0x800ULL;
	hdrCbArg = cmdArgs;
	status = selfInitAndVerifyHeader(&selfCtx, s_smSelfMetadataKeyIv, &rsaPubKey,
			5u, 0, 1u, selfScatterHeaderReadCb, &hdrCbArg,
			0x370, &expectedHeaderLen);
	BIGMAC->memMode = 0u;
	if (status != 0)
		return status;

	status = selfVerifySvnAndDecryptMetadata(&selfCtx);
	if (status != 0)
		return status;

	byteMemset(&selfInfo, 0, sizeof(selfInfo));
	status = selfLoaderWrapperGetAllSelfAuthInfo(&selfCtx, &authCtx->target,
			(uint32_t)srvkCheckFlagBit62(), &selfInfo, &authCtx->attrFlags);
	if (status != 0)
		return status;

	srvkDesc.addr = getSrvkListAddr();
	srvkDesc.size = getSrvkListSize();
	if (srvkDesc.addr == 0xffffffffu)
		return (int)0x800f0326;

	status = sceSblACLibIsLoadable(authCtx, &selfInfo, &srvkDesc);
	if (status != 0)
		return status;

	if (selfGetElfHeader(&selfCtx, (const void **)&ehdr) != 0 ||
	    ehdr->e_type != ET_EXEC ||
	    selfGetElfHeader(&selfCtx, (const void **)&ehdr) != 0 ||
	    selfGetProgramHeaders(&selfCtx, (const void **)&phdrs) != 0 ||
	    selfGetSegmentInfos(&selfCtx, (const void **)&segInfos) != 0) {
		return (int)0x800f0324;
	}

	for (pIdx = 0u; pIdx < ehdr->e_phnum; pIdx++) {
		const Elf32_Phdr *ph = &phdrs[pIdx];
		uint32_t vaddr = ph->p_vaddr;
		uint32_t filesz = ph->p_filesz;

		if (ph->p_type == PT_LOAD && filesz != 0u) {
			if (vaddr < getSmSramBase() ||
			    vaddr >= getSmSramEnd() - 0x800u ||
			    filesz >= getSmSramSize() - 0x800u ||
			    vaddr + filesz >= getSmSramEnd() - 0x800u) {
				return (int)0x800f0324;
			}

			segCbArg.segOffset = segInfos[pIdx].offset;
			segCbArg.segSize = segInfos[pIdx].length;
			if ((authCtx->attrFlags & 0xf000u) == 0x1000u)
				BIGMAC->memMode = 3u;

			segCbArg.cmdArgs = cmdArgs;
			segCbArg.dstVaddr = vaddr;
			segCbArg.filesz = filesz;
			status = selfDecryptAndLoadSegment(&selfCtx, 2u, pIdx,
					selfScatterSegmentReadCb, &segCbArg, NULL, NULL);
			BIGMAC->memMode = 0u;
			if (status != 0)
				return (int)0x800f0324;
		}
	}

	dmac5FillCh0((void *)(uintptr_t)(smSramEnd - 0x800u), 0u, 0x800u, 0u);
	return 1;
}

/* 0x802bca (name ours) */
void copyRangeEntry8(void *dst, const void *src)
{
	byteMemcpy(dst, src, 8u);
}

/* 0x802bd0 (name ours) */
int selfScatterHeaderReadCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outBytesRead, void *arg)
{
	const SceSkLoadSmCmdArgs *cmdArgs = *(const SceSkLoadSmCmdArgs *const *)arg;
	uint64_t targetOff = *offset;
	uint32_t numRanges = cmdArgs->numRanges;
	uint32_t rangesPaddr = cmdArgs->rangesPaddr;
	SceSkRangeEntry range;
	uint32_t rIdx = 0u;
	uint64_t curOff = 0ULL;
	uint64_t relOff = 0ULL;

	while (rIdx < numRanges) {
		uint64_t nextOff;

		copyRangeEntry8(&range, (const void *)(uintptr_t)(rangesPaddr + rIdx * 8u));
		nextOff = curOff + range.size;
		if (nextOff > targetOff) {
			relOff = targetOff - curOff;
			break;
		}
		rIdx++;
		curOff = nextOff;
	}

	if (rIdx == numRanges)
		return (int)0x800f0322;

	{
		uint32_t nextEntryAddr = rangesPaddr + (rIdx + 1u) * 8u;
		uint64_t remToRead = size;
		uint32_t dstOff = 0u;

		for (;;) {
			uint64_t availInRange = (uint64_t)range.size - relOff;
			uint32_t chunk = (remToRead < availInRange) ?
					 (uint32_t)remToRead : (uint32_t)availInRange;
			uint32_t srcPaddr = range.paddr + (uint32_t)relOff;

			if (checkRangeInAnyDram(srcPaddr, chunk) == 0)
				return (int)0x800f0322;

			dmac5MemcpyWithModeCh0((void *)(dstAddr + dstOff),
					(const void *)(uintptr_t)srcPaddr, chunk, 1u);
			remToRead -= chunk;
			if (remToRead == 0ULL) {
				*outBytesRead = size;
				return 0;
			}

			rIdx++;
			if (rIdx >= numRanges)
				return (int)0x800f0322;

			dstOff += chunk;
			copyRangeEntry8(&range, (const void *)(uintptr_t)nextEntryAddr);
			nextEntryAddr += 8u;
			relOff = 0ULL;
		}
	}
}

/* 0x802d24 (name ours) */
int selfScatterSegmentReadCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outBytesRead, uint32_t *outFlags, void *arg)
{
	const SelfScatterSegmentCbArg *cbArg = (const SelfScatterSegmentCbArg *)arg;
	uint64_t reqOff = *offset;
	uint64_t relSegOff;
	const SceSkLoadSmCmdArgs *cmdArgs;
	uint32_t numRanges;
	uint32_t rangesPaddr;
	SceSkRangeEntry range;
	uint32_t rIdx = 0u;
	uint64_t curOff = 0ULL;
	uint64_t relRangeOff = 0ULL;
	uint64_t availInRange;
	uint32_t chunk;

	if (reqOff < cbArg->segOffset)
		return (int)0x800f0322;
	if (cbArg->segSize < (uint64_t)size)
		return (int)0x800f0322;

	relSegOff = reqOff - cbArg->segOffset;
	if (relSegOff > cbArg->segSize - (uint64_t)size ||
	    (relSegOff >> 32) != 0ULL ||
	    (uint32_t)relSegOff > cbArg->filesz) {
		return (int)0x800f0322;
	}

	cmdArgs = cbArg->cmdArgs;
	rangesPaddr = cmdArgs->rangesPaddr;
	numRanges = cmdArgs->numRanges;

	while (rIdx < numRanges) {
		uint64_t nextOff;

		copyRangeEntry8(&range, (const void *)(uintptr_t)(rangesPaddr + rIdx * 8u));
		nextOff = curOff + range.size;
		if (nextOff > reqOff) {
			relRangeOff = reqOff - curOff;
			break;
		}
		rIdx++;
		curOff = nextOff;
	}

	if (rIdx == numRanges)
		return (int)0x800f0322;

	availInRange = (uint64_t)range.size - relRangeOff;
	chunk = ((uint64_t)size < availInRange) ? size : (uint32_t)availInRange;

	*outSrcAddr = relRangeOff + (uint64_t)range.paddr;
	*outDstAddr = ((uint64_t)cbArg->dstVaddr - cbArg->segOffset) + reqOff;
	*outBytesRead = chunk;
	*outFlags = 0u;
	return 0;
}

/* ============================================================================
 * Certified File (CF) / SELF Core Functions (0x802e90..0x804c3d)
 * ============================================================================ */

/* 0x802e90 (name ours) */
SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx)
{
	uint8_t *base = (uint8_t *)ctx->scratchBuf;
	uint32_t metaOffset = *(const uint32_t *)(base + 0x0cu);

	return (SceCfMetadataHeader *)(base + metaOffset + 0x70u);
}

/* 0x802e9e (name ours) */
SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx)
{
	return (SceCfMetadataSection *)((uint8_t *)cfGetMetadataHeader(ctx) + 0x20u);
}

/* 0x802eb0 (name ours) */
uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx)
{
	uint8_t *sections = (uint8_t *)cfGetMetadataSections(ctx);
	SceCfMetadataHeader *mh = cfGetMetadataHeader(ctx);

	return sections + mh->sectionCount * 0x30u;
}

/* 0x802edc (name ours) */
int cfDefaultGetKeysCallback(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg)
{
	const CfStaticKeyParams *params = (const CfStaticKeyParams *)arg;

	*outMetaKey = (const uint8_t *)params->metaKey;
	*outRootPubKey = (const uint8_t *)params->rootPubKey;
	*outSigType = params->sigType;
	*outRootKey = NULL;
	return 0;
}

/* 0x802ef6 (name ours) */
int cfGetSignatureSize(uint32_t sigType, uint32_t *outSize)
{
	uint32_t sigSizes[8];

	memcpy(sigSizes, s_cfSigSizes, sizeof(sigSizes));
	if (sigType - 1u > 6u)
		return (int)0x800f0616;

	*outSize = sigSizes[sigType];
	return 0;
}

/* 0x802f3e (name ours) */
int cfProcessUnalignedSegmentBlock(CfSegmentDecryptState *state,
		const uint64_t *reqLen, uint8_t *outBlock16, int updateHash)
{
	uint32_t lenLo = (uint32_t)*reqLen;
	uint32_t lenHi = (uint32_t)(*reqLen >> 32);
	uint64_t readOffset;
	uint64_t dstAddr = 0ULL;
	uint64_t srcAddr = 0ULL;
	uint32_t bytesRead = 0u;
	uint32_t cbFlags = 0u;
	uint32_t copyDesc[3];
	uint8_t localBlock[16];
	uint8_t *workBlock;
	int status;

	if (state == NULL || (lenLo & ~0xfu) != 0u || lenHi != 0u)
		return (int)0x800f0616;

	readOffset = state->curOffset;
	if (state->readCb(&readOffset, lenLo, &srcAddr, &dstAddr,
			&bytesRead, &cbFlags, state->readCbArg) != 0) {
		return (int)0x800f0605;
	}
	if (bytesRead != lenLo || lenHi != 0u)
		return (int)0x800f0605;

	workBlock = (outBlock16 != NULL) ? outBlock16 : localBlock;

	copyDesc[0] = 0u;
	copyDesc[1] = (uint32_t)(srcAddr >> 32);
	copyDesc[2] = cbFlags & 2u;
	status = cfCopyBytes(workBlock, (const void *)(uintptr_t)srcAddr, bytesRead, copyDesc);
	if (status != 0)
		return status;

	if (state->section.encryptionAlgorithm != 1u) {
		if (state->section.encryptionAlgorithm != 3u)
			return (int)0x800f0625;

		copyDesc[0] = 0u;
		copyDesc[1] = 0u;
		copyDesc[2] = 0u;
		status = cfAesCipher(4u, 1u, 0u, workBlock, workBlock, 16u,
				state->cipherKey, state->cipherIv, state->cipherIv, copyDesc);
		if (status != 0)
			return status;
	}

	copyDesc[0] = (uint32_t)(dstAddr >> 32);
	copyDesc[1] = 0u;
	copyDesc[2] = cbFlags & 1u;
	status = cfCopyBytes((void *)(uintptr_t)dstAddr, workBlock, bytesRead, copyDesc);
	if (status != 0)
		return status;

	if (updateHash != 0) {
		uint32_t isFinal = 0u;

		if (state->headLen == 0u &&
		    state->curOffset + bytesRead == state->section.offset + state->section.size) {
			isFinal = 0x80000000u;
		}
		status = cfHmacSha256Update(&state->hmacCtx, workBlock, bytesRead, isFinal);
		if (status != 0)
			return status;
	}

	if (state->doneCb != NULL) {
		uint64_t doneOff = state->curOffset;
		uint64_t doneSrc = srcAddr;
		uint64_t doneDst = dstAddr;

		if (state->doneCb(&doneOff, lenLo, &doneSrc, &doneDst,
				bytesRead, cbFlags, state->doneCbArg) != 0) {
			return (int)0x800f0605;
		}
	}

	state->curOffset += bytesRead;
	state->remSize -= bytesRead;
	return 0;
}

/* 0x803144 (name ours) */
int cfSegmentDecryptUpdate(CfSegmentDecryptState *state,
		const uint64_t *offset, const uint64_t *size,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint64_t startOff = *offset;
	uint64_t totalSize = *size;
	int status;

	if (state->section.size == 0ULL)
		return 0;

	state->curOffset = startOff;
	state->readCb = readCb;
	state->readCbArg = readCbArg;
	state->doneCb = doneCb;
	state->doneCbArg = doneCbArg;
	state->remSize = totalSize;

	if (((uint32_t)startOff & 0xfu) != 0u) {
		uint64_t headReq;

		state->headLen = 0x10u - ((uint32_t)startOff & 0xfu);
		if (totalSize < (uint64_t)state->headLen)
			state->headLen = (uint32_t)totalSize;
		headReq = (uint64_t)state->headLen;
		status = cfProcessUnalignedSegmentBlock(state, &headReq,
				state->headBlock, 0);
		if (status != 0)
			return status;
	}

	while ((state->remSize & ~0xfULL) != 0ULL) {
		uint64_t reqOff = state->curOffset;
		uint64_t srcAddr = 0ULL;
		uint64_t dstAddr = 0ULL;
		uint32_t bytesRead = 0u;
		uint32_t cbFlags = 0u;
		uint32_t copyDesc[3];
		uint32_t isFinal;

		if (state->readCb(&reqOff, (uint32_t)state->remSize & ~0xfu,
				&srcAddr, &dstAddr, &bytesRead,
				&cbFlags, state->readCbArg) != 0 ||
		    bytesRead == 0u || (bytesRead & 0xfu) != 0u) {
			return (int)0x800f0605;
		}

		copyDesc[0] = (uint32_t)(dstAddr >> 32);
		copyDesc[1] = (uint32_t)(srcAddr >> 32);
		copyDesc[2] = cbFlags;
		if (state->section.encryptionAlgorithm == 1u) {
			status = cfCopyBytes((void *)(uintptr_t)dstAddr,
					(const void *)(uintptr_t)srcAddr,
					bytesRead, copyDesc);
		} else if (state->section.encryptionAlgorithm == 3u) {
			status = cfAesCipher(4u, 1u, 0u,
					(void *)(uintptr_t)dstAddr,
					(const void *)(uintptr_t)srcAddr,
					bytesRead, state->cipherKey,
					state->cipherIv, state->cipherIv,
					copyDesc);
		} else {
			return (int)0x800f0625;
		}
		if (status != 0)
			return status;

		isFinal = (cbFlags & 1u) << 1;
		if (state->headLen == 0u &&
		    state->curOffset + bytesRead ==
		    state->section.offset + state->section.size) {
			isFinal |= 0x80000000u;
		}

		status = cfHmacSha256Update(&state->hmacCtx,
				(const void *)(uintptr_t)dstAddr,
				bytesRead, isFinal & 0x80000000u);
		if (status != 0)
			return status;

		if (state->doneCb != NULL) {
			uint64_t doneOff = state->curOffset;
			uint64_t doneSrc = srcAddr;
			uint64_t doneDst = dstAddr;

			if (state->doneCb(&doneOff, (uint32_t)state->remSize & ~0xfu,
					&doneSrc, &doneDst, bytesRead,
					cbFlags, state->doneCbArg) != 0) {
				return (int)0x800f0605;
			}
		}

		state->curOffset += bytesRead;
		state->remSize -= bytesRead;
	}

	if (state->remSize != 0ULL) {
		uint64_t tailReq = state->remSize;

		return cfProcessUnalignedSegmentBlock(state, &tailReq, NULL, 1);
	}
	return 0;
}

/* 0x803382 (name ours) */
int cfSegmentDecryptFinal(CfSegmentDecryptState *state)
{
	uint8_t calcDigest[32];
	int status;

	if (state->section.size == 0ULL)
		return 0;

	if (state->headLen != 0u) {
		status = cfHmacSha256Update(&state->hmacCtx, state->headBlock,
				state->headLen, 0x80000000u);
		if (status != 0)
			return status;
	}

	status = cfHmacSha256Final(calcDigest, &state->hmacCtx);
	if (status == 0 &&
	    timingsafeBcmp(calcDigest, state->expectedDigest, 32u) != 0) {
		status = (int)0x800f0627;
	}
	return status;
}

/* 0x803404 (name ours) */
int cfSegmentDecryptInit(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *secOverride)
{
	const SceCfMetadataSection *sec = secOverride;
	uint32_t copyDesc[3];
	int status;

	if (state == NULL || ctx == NULL)
		return (int)0x800f0616;

	if (sec == NULL) {
		if (sectionIndex >= cfGetMetadataHeader(ctx)->sectionCount)
			return (int)0x800f0616;
		sec = &cfGetMetadataSections(ctx)[sectionIndex];
	}

	state->selfCtx = ctx;
	memcpy(&state->section, sec, sizeof(SceCfMetadataSection));
	state->headLen = 0u;

	if (sec->size != 0ULL) {
		uint32_t hIdx;

		if (sec->hashAlgorithm != 6u)
			return (int)0x800f0625;

		hIdx = sec->hashKeyIndex;
		state->expectedDigest = cfGetMetadataKeyPool(ctx) + hIdx * 0x10u;
		if (cfHmacSha256InitShortKey(&state->hmacCtx,
				cfGetMetadataKeyPool(ctx) + (hIdx + 2u) * 0x10u,
				0x20u) != 0) {
			return (int)0x800f0605;
		}

		if (sec->encryptionAlgorithm == 3u) {
			copyDesc[0] = 0u;
			copyDesc[1] = 0u;
			copyDesc[2] = 0u;
			state->cipherKey = cfGetMetadataKeyPool(ctx) + sec->keyIndex * 0x10u;
			status = cfCopyBytes(state->cipherIv,
					cfGetMetadataKeyPool(ctx) + sec->ivIndex * 0x10u,
					0x10u, copyDesc);
			if (status != 0)
				return status;
		} else if (sec->encryptionAlgorithm == 1u) {
			state->cipherKey = NULL;
		} else {
			return (int)0x800f0625;
		}
	}
	return 0;
}

/* 0x80350a (name ours) */
int cfVerifyRsaSignature(const void *hdrBuf, uint32_t sigInputLen,
		const void *signature256, const uint8_t *pubKeyBuf)
{
	uint32_t pubKeyDesc[4];
	uint32_t zeroPad[3];
	uint8_t workBuf[256];
	uint8_t digest[32];
	int status;

	zeroPad[0] = 0u;
	zeroPad[1] = 0u;
	zeroPad[2] = 0u;
	(void)zeroPad;

	status = bigmacSha256(digest, hdrBuf, sigInputLen);
	if (status == 0) {
		pubKeyDesc[0] = (uint32_t)(uintptr_t)pubKeyBuf;
		pubKeyDesc[1] = 0x100u;
		pubKeyDesc[2] = (uint32_t)(uintptr_t)(pubKeyBuf + 0x100u);
		pubKeyDesc[3] = 4u;
		status = cfVerifyRsaPkcs1v15(workBuf, signature256, pubKeyDesc, digest, 1u);
	}
	return status;
}

/* 0x803586 (name ours) */
int cfAuthenticateAndDecryptHeaderWithKeyCb(const SelfAuthContext *ctx,
		CfGetKeysCallback getKeysCb, void *getKeysCbArg,
		int allowDevelopment, uint16_t expectedCategory,
		SelfHeaderReadCallback readCb, void *readCbArg,
		int32_t expectedExtHeaderSize, const uint64_t *expectedHeaderLen)
{
	uint64_t expHdrLen = *expectedHeaderLen;
	uint64_t readOffset;
	uint32_t bytesRead = 0u;
	cf_header *cfHdr;
	uint32_t scratchSize;
	uint32_t extHdrSize;
	uint32_t minHeaderEnd;
	uint32_t headerLenLo;
	const uint8_t *metaKey = NULL;
	const uint8_t *rootPubKey = NULL;
	uint32_t sigType = 0u;
	const uint8_t *rootKey = NULL;
	uint32_t cipherDesc[3];
	uint32_t dummySigSize = 0u;
	int32_t sigSize = 0;
	SceCfMetadataHeader *mh;
	SceCfMetadataSection *sections;
	uint8_t *metaKeyBlock;
	uint32_t i;
	int status;

	if (ctx == NULL || ctx->scratchBuf == NULL || getKeysCb == NULL ||
	    readCb == NULL || ctx->scratchSize < 0x30u) {
		return (int)0x800f0616;
	}

	cfHdr = (cf_header *)ctx->scratchBuf;
	readOffset = 0ULL;
	if (readCb((uintptr_t)cfHdr, &readOffset, 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != 0x30u) {
		return (int)0x800f0605;
	}

	scratchSize = ctx->scratchSize;
	if (cfHdr->magic != 0x00454353u || cfHdr->version != 3u ||
	    ((uint8_t)cfHdr->attribute & 0x40u) == 0u ||
	    ((int8_t)(uint8_t)cfHdr->attribute < 0 && allowDevelopment == 0) ||
	    cfHdr->category != expectedCategory) {
		return (int)0x800f0624;
	}

	extHdrSize = cfHdr->ext_header_size;
	if ((extHdrSize & 0xfu) != 0u ||
	    (expectedExtHeaderSize >= 0 && (uint32_t)expectedExtHeaderSize != extHdrSize)) {
		return (int)0x800f0624;
	}

	if ((int64_t)expHdrLen >= 0 && cfHdr->file_offset != expHdrLen)
		return (int)0x800f0624;

	minHeaderEnd = extHdrSize + 0x90u;
	if (minHeaderEnd < extHdrSize || minHeaderEnd > scratchSize)
		return (int)0x800f0624;

	headerLenLo = (uint32_t)cfHdr->file_offset;
	if ((headerLenLo & 0xfu) != 0u)
		return (int)0x800f0624;
	if ((cfHdr->file_offset >> 32) != 0u || headerLenLo < minHeaderEnd ||
	    headerLenLo > scratchSize) {
		return (int)0x800f0624;
	}
	if (*(const uint64_t *)cfHdr->versionExtension != 0ULL)
		return (int)0x800f0624;
	if (headerLenLo > scratchSize)
		return (int)0x800f0616;

	readOffset = 0x30ULL;
	if (readCb((uintptr_t)ctx->scratchBuf + 0x30u, &readOffset,
			headerLenLo - 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != headerLenLo - 0x30u) {
		return (int)0x800f0605;
	}

	status = getKeysCb(&metaKey, &rootPubKey, &sigType, &rootKey, getKeysCbArg);
	if (status != 0)
		return (int)0x800f0605;

	cipherDesc[0] = 0u;
	cipherDesc[1] = 0u;
	cipherDesc[2] = 0u;
	if (metaKey == NULL)
		return (int)0x800f0616;

	metaKeyBlock = (uint8_t *)ctx->scratchBuf + 0x30u + extHdrSize;
	if (rootKey != NULL) {
		status = cfAesCipher(1u, 0u, 0u, metaKeyBlock, metaKeyBlock, 0x40u,
				rootKey, rootKey + 0x10u, NULL, cipherDesc);
		if (status != 0)
			return status;
	}

	status = cfAesCipher(1u, 0u, 2u, metaKeyBlock, metaKeyBlock, 0x40u,
			metaKey, metaKey + 0x20u, NULL, cipherDesc);
	if (status != 0)
		return status;

	status = cfAesCipher(1u, 0u, 0u,
			(uint8_t *)ctx->scratchBuf + extHdrSize + 0x70u,
			(uint8_t *)ctx->scratchBuf + extHdrSize + 0x70u,
			headerLenLo - (extHdrSize + 0x70u),
			metaKeyBlock, metaKeyBlock + 0x20u, NULL, cipherDesc);
	if (status != 0)
		return status;

	if (rootPubKey == NULL)
		return (int)0x800f0616;
	if (cfGetSignatureSize(sigType, &dummySigSize) != 0)
		return (int)0x800f0616;
	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0)
		return (int)0x800f0625;

	mh = cfGetMetadataHeader(ctx);
	sections = cfGetMetadataSections(ctx);
	if (mh == NULL)
		return (int)0x800f0616;
	if (mh->signatureType != sigType)
		return (int)0x800f0616;
	if (cfGetSignatureSize(mh->signatureType, (uint32_t *)&sigSize) != 0)
		return (int)0x800f0616;

	if (mh->signatureInputLength == 0ULL ||
	    ((uint32_t)mh->signatureInputLength & 0xfu) != 0u ||
	    (mh->optHeaderSize & 0xfu) != 0u ||
	    mh->reserved != 0ULL) {
		return (int)0x800f0624;
	}

	{
		const cf_header *hdr = (const cf_header *)ctx->scratchBuf;
		uint64_t totalMetaEnd = (uint64_t)hdr->ext_header_size +
					(uint64_t)mh->optHeaderSize +
					0x10fULL +
					(uint64_t)(int64_t)sigSize +
					(uint64_t)mh->sectionCount * 0x30ULL +
					(uint64_t)mh->keyCount * 0x10ULL;

		if ((totalMetaEnd & ~0x7fULL) > hdr->file_offset)
			return (int)0x800f0624;
		if ((int64_t)expHdrLen >= 0 &&
		    mh->signatureInputLength != hdr->file_offset - (uint64_t)(int64_t)sigSize) {
			return (int)0x800f0624;
		}
		if (mh->signatureInputLength > hdr->file_offset - (uint64_t)(int64_t)sigSize)
			return (int)0x800f0624;
	}

	if (sections == NULL)
		return (int)0x800f0616;

	for (i = 0u; i < mh->sectionCount; i++) {
		const cf_header *hdr = (const cf_header *)ctx->scratchBuf;
		const SceCfMetadataSection *sec = &sections[i];
		uint32_t hashKeySlots[8];
		uint64_t secEnd;
		uint32_t hashNeed;

		if (sec->offset < hdr->file_offset)
			return (int)0x800f0624;
		secEnd = sec->offset + sec->size;
		if (secEnd < sec->offset || secEnd < sec->size || secEnd > hdr->cf_file_size)
			return (int)0x800f0624;

		memcpy(hashKeySlots, s_cfHashKeySlots, sizeof(hashKeySlots));
		if (sec->hashAlgorithm - 1u > 6u)
			return (int)0x800f0624;
		hashNeed = hashKeySlots[sec->hashAlgorithm];
		if ((hashNeed >> 31) != (uint32_t)(hashNeed + sec->hashKeyIndex < hashNeed) ||
		    hashNeed + sec->hashKeyIndex > mh->keyCount) {
			return (int)0x800f0624;
		}

		if (sec->encryptionAlgorithm == 1u) {
			if (sec->keyIndex != 0xffffffffu || sec->ivIndex != 0xffffffffu)
				return (int)0x800f0624;
		} else {
			uint32_t encKeySlots[6];
			uint32_t encIvSlots[6];
			uint32_t kNeed;
			uint32_t ivNeed;

			memcpy(encKeySlots, s_cfEncKeySlots, sizeof(encKeySlots));
			memcpy(encIvSlots, s_cfEncIvSlots, sizeof(encIvSlots));
			if (sec->encryptionAlgorithm - 1u > 4u)
				return (int)0x800f0624;
			kNeed = encKeySlots[sec->encryptionAlgorithm];
			ivNeed = encIvSlots[sec->encryptionAlgorithm];
			if ((kNeed >> 31) != (uint32_t)(kNeed + sec->keyIndex < kNeed) ||
			    kNeed + sec->keyIndex > mh->keyCount ||
			    (ivNeed >> 31) != (uint32_t)(ivNeed + sec->ivIndex < ivNeed) ||
			    ivNeed + sec->ivIndex > mh->keyCount) {
				return (int)0x800f0624;
			}
		}

		if (sec->compressionAlgorithm - 1u > 1u)
			return (int)0x800f0624;
	}

	if (mh->signatureType != 5u)
		return (int)0x800f0625;

	return cfVerifyRsaSignature(ctx->scratchBuf, (uint32_t)mh->signatureInputLength,
			(const uint8_t *)ctx->scratchBuf + (uint32_t)mh->signatureInputLength,
			rootPubKey);
}

/* 0x803b18 (name ours; Sony method self_loader::open) */
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowDevelopment,
		uint16_t expectedCategory, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedExtHeaderSize,
		const uint64_t *expectedHeaderLen)
{
	uint64_t expLen = *expectedHeaderLen;
	CfStaticKeyParams keyParams;

	keyParams.metaKey = metaKey;
	keyParams.rootPubKey = rootPubKey;
	keyParams.sigType = sigType;
	return cfAuthenticateAndDecryptHeaderWithKeyCb(ctx, cfDefaultGetKeysCallback,
			&keyParams, allowDevelopment, expectedCategory,
			readCb, readCbArg, expectedExtHeaderSize, &expLen);
}

/* 0x803b58 (name ours) */
int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount)
{
	if (ctx == NULL || outCount == NULL)
		return (int)0x800f0616;

	*outCount = cfGetMetadataHeader(ctx)->sectionCount;
	return 0;
}

/* 0x803b82 (name ours) */
int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection)
{
	if (ctx == NULL || outSection == NULL ||
	    index >= cfGetMetadataHeader(ctx)->sectionCount) {
		return (int)0x800f0616;
	}

	*outSection = &cfGetMetadataSections(ctx)[index];
	return 0;
}

/* 0x803bd2 (name ours) */
int cfFindSectionEntryByTypeAndIndex(const SelfAuthContext *ctx,
		uint32_t segType, uint32_t segIndex, const void **outSection)
{
	uint32_t secCount;
	uint32_t i;

	if (ctx == NULL || outSection == NULL)
		return (int)0x800f0616;

	secCount = cfGetMetadataHeader(ctx)->sectionCount;
	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex) {
			*outSection = sec;
			return 0;
		}
	}
	return (int)0x800f0602;
}

/* 0x803c48 (name ours) */
int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint32_t **outExtHdr, uint32_t *outMetaOffset)
{
	const uint8_t *base;

	if (ctx == NULL || outExtHdr == NULL || outMetaOffset == NULL)
		return (int)0x800f0616;

	base = (const uint8_t *)ctx->scratchBuf;
	*outExtHdr = (const uint32_t *)(base + 0x30u);
	*outMetaOffset = *(const uint32_t *)(base + 0x0cu);
	return 0;
}

/* 0x803c68 (name ours) */
int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptHdrSize)
{
	uint8_t *keyPool;

	if (ctx == NULL || outOptHdr == NULL || outOptHdrSize == NULL)
		return (int)0x800f0616;

	keyPool = cfGetMetadataKeyPool(ctx);
	*outOptHdr = keyPool + cfGetMetadataHeader(ctx)->keyCount * 0x10u;
	*outOptHdrSize = cfGetMetadataHeader(ctx)->optHeaderSize;
	return 0;
}

/* 0x803cbc (name ours) */
int cfDecryptAndVerifySection(const SelfAuthContext *ctx, uint32_t sectionIndex,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	CfSegmentDecryptState state;
	const SceCfMetadataSection *sec;
	uint64_t secOff;
	uint64_t secSize;
	int status;

	if (ctx == NULL || readCb == NULL ||
	    sectionIndex >= cfGetMetadataHeader(ctx)->sectionCount) {
		return (int)0x800f0616;
	}

	status = cfSegmentDecryptInit(&state, ctx, sectionIndex, NULL);
	if (status != 0)
		return status;

	sec = &cfGetMetadataSections(ctx)[sectionIndex];
	secOff = sec->offset;
	secSize = sec->size;
	status = cfSegmentDecryptUpdate(&state, &secOff, &secSize,
			readCb, readCbArg, doneCb, doneCbArg);
	if (status == 0)
		status = cfSegmentDecryptFinal(&state);
	return status;
}

/* 0x803d84 (name ours; Sony method self_loader::load_segment) */
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint32_t secCount = cfGetMetadataHeader(ctx)->sectionCount;
	uint32_t i;

	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex) {
			return cfDecryptAndVerifySection(ctx, i, readCb, readCbArg,
					doneCb, doneCbArg);
		}
	}
	return (int)0x800f0602;
}

/* 0x803e14 (name ours) */
int cfStreamSegmentInitThunk(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *secOverride)
{
	return cfSegmentDecryptInit(state, ctx, sectionIndex, secOverride);
}

/* 0x803e18 (name ours) */
int cfStreamSegmentInitByTypeAndIndex(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t segType, uint32_t segIndex)
{
	uint32_t secCount = cfGetMetadataHeader(ctx)->sectionCount;
	uint32_t i;

	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex)
			return cfSegmentDecryptInit(state, ctx, i, NULL);
	}
	return (int)0x800f0602;
}

/* 0x803e98 (name ours) */
int cfStreamSegmentFinal(CfSegmentDecryptState *state)
{
	if (state == NULL)
		return (int)0x800f0616;
	return cfSegmentDecryptFinal(state);
}

/* 0x803eae (name ours) */
int cfStreamSegmentUpdateRange(CfSegmentDecryptState *state,
		const uint64_t *offset, const uint64_t *size,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint64_t reqOff = *offset;
	uint64_t reqSize = *size;

	if (state == NULL || readCb == NULL)
		return (int)0x800f0616;
	if (reqOff < state->section.offset ||
	    reqOff >= state->section.offset + state->section.size ||
	    reqSize > state->section.size ||
	    reqOff - state->section.offset > state->section.size - reqSize) {
		return (int)0x800f0616;
	}

	return cfSegmentDecryptUpdate(state, &reqOff, &reqSize,
			readCb, readCbArg, doneCb, doneCbArg);
}

/* 0x803f9c (name ours; Sony method self_loader::verify_header) */
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx)
{
	const uint32_t *extHdr = NULL;
	uint32_t metaOffset = 0u;
	uint32_t maxExtEnd;
	uint32_t elfOff;
	uint32_t appInfoOff;
	uint32_t phdrOff;
	uint32_t segInfoOff;
	uint32_t sceVerOff;
	uint32_t ctrlInfoOff;
	uint32_t ctrlInfoSize;
	const uint8_t *base;
	const Elf32_Ehdr *ehdr;
	uint32_t phnum;
	uint32_t phdrTableSize;
	uint32_t fixedHdrOverhead;
	uint32_t secCount = 0u;
	const SceCfMetadataSection *sections = NULL;
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0u;
	uint32_t sIdx;
	int status;

	if (ctx == NULL)
		return (int)0x800f0616;
	if (cfGetExtHeaderAndMetaOffset(ctx, &extHdr, &metaOffset) != 0)
		return (int)0x800f0616;

	if (extHdr[0] != 4u || extHdr[1] != 0u)
		return (int)0x800f0624;

	elfOff = extHdr[4];
	if ((elfOff & 0xfu) != 0u)
		return (int)0x800f0624;

	maxExtEnd = metaOffset + 0x30u;
	if (extHdr[5] != 0u || elfOff > maxExtEnd || metaOffset < 0x40u ||
	    elfOff + 0x40u < elfOff || elfOff + 0x40u > maxExtEnd || elfOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	base = (const uint8_t *)ctx->scratchBuf;
	sceVerOff = extHdr[12];
	segInfoOff = extHdr[10];
	ehdr = (const Elf32_Ehdr *)(base + elfOff);

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != 0xfe00u &&
	    ehdr->e_type != ET_SCE_FIXEDEXEC) {
		return (int)0x800f0624;
	}

	appInfoOff = extHdr[2];
	if ((appInfoOff & 0xfu) != 0u || extHdr[3] != 0u || appInfoOff > maxExtEnd ||
	    appInfoOff + 0x20u < appInfoOff || appInfoOff + 0x20u > maxExtEnd ||
	    appInfoOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	if ((sceVerOff & 0xfu) != 0u || extHdr[13] != 0u || sceVerOff > maxExtEnd ||
	    sceVerOff + 0x10u < sceVerOff || sceVerOff + 0x10u > maxExtEnd ||
	    sceVerOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	ctrlInfoOff = extHdr[14];
	if (ctrlInfoOff != 0u || extHdr[15] != 0u) {
		if ((ctrlInfoOff & 0xfu) != 0u || extHdr[15] != 0u ||
		    ctrlInfoOff > maxExtEnd || ctrlInfoOff + 0x10u < ctrlInfoOff ||
		    ctrlInfoOff + 0x10u > maxExtEnd || ctrlInfoOff <= 0x7fu) {
			return (int)0x800f0624;
		}
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return (int)0x800f0624;

	if ((segInfoOff & 0xfu) != 0u || extHdr[11] != 0u || segInfoOff > maxExtEnd)
		return (int)0x800f0624;

	phnum = ehdr->e_phnum;
	phdrTableSize = phnum * 0x20u;
	if (phdrTableSize > metaOffset ||
	    phdrTableSize + segInfoOff < phdrTableSize ||
	    phdrTableSize + segInfoOff > maxExtEnd || segInfoOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	phdrOff = extHdr[6];
	fixedHdrOverhead = 0xb0u;
	if (phdrOff != 0u || extHdr[7] != 0u) {
		if ((phdrOff & 0xfu) != 0u || extHdr[7] != 0u || phdrOff > maxExtEnd ||
		    phdrTableSize + 0xfu < phdrTableSize || phdrTableSize > metaOffset ||
		    phdrTableSize + phdrOff < phdrTableSize ||
		    phdrTableSize + phdrOff > maxExtEnd || phdrOff <= 0x7fu) {
			return (int)0x800f0624;
		}
		fixedHdrOverhead = phdrTableSize + 0xb0u;
	}

	ctrlInfoSize = extHdr[16];
	{
		uint32_t verExtra = *(const uint32_t *)(base + sceVerOff + 4u);
		uint64_t totalExt = (uint64_t)ctrlInfoSize + 0x10ULL +
				    (uint64_t)phdrTableSize +
				    (uint64_t)verExtra * 0x30ULL +
				    (uint64_t)fixedHdrOverhead;

		if ((totalExt >> 32) != 0ULL || extHdr[17] != 0u ||
		    (uint32_t)totalExt > metaOffset || ctrlInfoSize > metaOffset) {
			return (int)0x800f0624;
		}
	}

	status = cfGetSectionCount(ctx, &secCount);
	if (status != 0)
		return status;
	status = cfGetSectionEntry(ctx, 0u, &sections);
	if (status != 0)
		return status;

	for (sIdx = 0u; sIdx < secCount; sIdx++) {
		const uint32_t *segInfo = (const uint32_t *)(base + segInfoOff);
		const uint32_t *secWords = (const uint32_t *)&sections[sIdx];
		uint32_t pIdx;

		for (pIdx = 0u; pIdx < phnum; pIdx++, segInfo += 8) {
			if (secWords[0] == segInfo[0] &&
			    secWords[1] == segInfo[1] &&
			    secWords[2] == segInfo[2] &&
			    secWords[3] == segInfo[3] &&
			    secWords[4] == 2u &&
			    secWords[5] == pIdx &&
			    secWords[11] == segInfo[4]) {
				break;
			}
			if (secWords[8] != 3u || secWords[6] != 6u)
				return (int)0x800f0624;
		}
		if (pIdx == phnum)
			return (int)0x800f0602;
	}

	{
		const SceSelfHeader *selfHdr = (const SceSelfHeader *)ctx->scratchBuf;

		if (selfHdr->controlInfoOffset != 0ULL) {
			const uint8_t *ctrlBase = (const uint8_t *)selfHdr +
						  (uint32_t)selfHdr->controlInfoOffset;
			uint64_t totalCtrlSize = selfHdr->controlInfoSize;
			uint64_t curCtrlOff = 0ULL;

			while (curCtrlOff < totalCtrlSize) {
				const uint32_t *ci = (const uint32_t *)(ctrlBase + (uint32_t)curCtrlOff);
				uint32_t ciType = ci[0];
				uint32_t entrySize;

				if (ciType == 5u) {
					if (ci[4] != 0x4d52447fu)
						return (int)0x800f0624;
					entrySize = 0x110u;
				} else if (ciType < 6u) {
					if (ciType != 4u)
						return (int)0x800f0624;
					if (timingsafeBcmp(&ci[4], s_selfControlDigestConstant, 20u) != 0)
						return (int)0x800f0624;
					if (ci[17] != 0u)
						return (int)0x800f0624;
					entrySize = 0x50u;
				} else if (ciType == 6u) {
					if (ci[4] != 1u)
						return (int)0x800f0624;
					entrySize = 0x110u;
				} else if (ciType == 7u) {
					entrySize = 0x50u;
				} else {
					return (int)0x800f0624;
				}

				curCtrlOff += entrySize;
				if (curCtrlOff > totalCtrlSize)
					return (int)0x800f0624;
				if (ci[2] == 0u && ci[3] == 0u)
					break;
			}
			if (curCtrlOff != totalCtrlSize)
				return (int)0x800f0624;
		}
	}

	status = cfGetOptHeaders(ctx, &optHdrBase, &optHdrSize);
	if (status != 0)
		return status;

	{
		uint32_t curOptOff = 0u;

		while (curOptOff < optHdrSize) {
			const uint32_t *oh = (const uint32_t *)(optHdrBase + curOptOff);
			uint32_t ohType = oh[0];
			uint32_t nextOptOff;

			if (ohType == 2u) {
				nextOptOff = curOptOff + 0x110u;
			} else if (ohType == 1u || ohType == 3u) {
				nextOptOff = curOptOff + 0x30u;
			} else {
				return (int)0x800f0624;
			}

			if (nextOptOff < curOptOff || nextOptOff > optHdrSize)
				return (int)0x800f0624;
			curOptOff = nextOptOff;
			if (oh[2] == 0u && oh[3] == 0u)
				break;
		}
		if (curOptOff != optHdrSize)
			return (int)0x800f0624;
	}

	return 0;
}

/* 0x804436 (name ours) */
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo)
{
	if (ctx == NULL || outAppInfo == NULL)
		return (int)0x800f0616;

	*outAppInfo = (const uint8_t *)ctx->scratchBuf +
		      *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x38u);
	return 0;
}

/* 0x804450 (name ours; Sony method self_loader::get_elf_header) */
int selfGetElfHeader(const SelfAuthContext *ctx, const void **outEhdr)
{
	if (ctx == NULL || outEhdr == NULL)
		return (int)0x800f0616;

	*outEhdr = (const uint8_t *)ctx->scratchBuf +
		   *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x40u);
	return 0;
}

/* 0x80446a (name ours; Sony method self_loader::get_program_segment_info) */
int selfGetProgramHeaders(const SelfAuthContext *ctx, const void **outPhdrs)
{
	const uint8_t *base;
	uint32_t phdrOffLo;
	uint32_t phdrOffHi;

	if (ctx == NULL || outPhdrs == NULL)
		return (int)0x800f0616;

	base = (const uint8_t *)ctx->scratchBuf;
	phdrOffLo = *(const uint32_t *)(base + 0x48u);
	phdrOffHi = *(const uint32_t *)(base + 0x4cu);
	if (phdrOffLo == 0u && phdrOffHi == 0u)
		return (int)0x800f0602;

	*outPhdrs = base + phdrOffLo;
	return 0;
}

/* 0x804494 (name ours; Sony method self_loader::get_program_segment_info) */
int selfGetSegmentInfos(const SelfAuthContext *ctx, const void **outSegInfos)
{
	if (ctx == NULL || outSegInfos == NULL)
		return (int)0x800f0616;

	*outSegInfos = (const uint8_t *)ctx->scratchBuf +
		       *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x58u);
	return 0;
}

/* 0x8044ae (name ours) */
int selfGetControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint64_t *outCtrlInfoSize)
{
	const SceSelfHeader *selfHdr;

	if (ctx == NULL || outCtrlInfo == NULL || outCtrlInfoSize == NULL)
		return (int)0x800f0616;

	selfHdr = (const SceSelfHeader *)ctx->scratchBuf;
	if (selfHdr->controlInfoOffset == 0ULL)
		return (int)0x800f0602;

	*outCtrlInfo = (const uint8_t *)selfHdr + (uint32_t)selfHdr->controlInfoOffset;
	*outCtrlInfoSize = selfHdr->controlInfoSize;
	return 0;
}

/* 0x8044ec (name ours) */
int selfFindControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint32_t targetType)
{
	const SceSelfHeader *selfHdr;
	const uint8_t *ctrlBase;
	uint64_t totalCtrlSize;
	uint64_t curOff = 0ULL;

	if (ctx == NULL || outCtrlInfo == NULL)
		return (int)0x800f0616;

	selfHdr = (const SceSelfHeader *)ctx->scratchBuf;
	if (selfHdr->controlInfoOffset == 0ULL)
		return (int)0x800f0602;

	ctrlBase = (const uint8_t *)selfHdr + (uint32_t)selfHdr->controlInfoOffset;
	totalCtrlSize = selfHdr->controlInfoSize;

	while (curOff < totalCtrlSize) {
		const uint32_t *ci = (const uint32_t *)(ctrlBase + (uint32_t)curOff);
		uint32_t ciType = ci[0];
		uint32_t entrySize;

		if (ciType == targetType) {
			*outCtrlInfo = ctrlBase + (uint32_t)curOff;
			return 0;
		}
		if (ciType == 5u || ciType == 6u)
			entrySize = 0x110u;
		else if (ciType == 4u || ciType == 7u)
			entrySize = 0x50u;
		else
			return (int)0x800f0624;

		curOff += entrySize;
		if (curOff > totalCtrlSize)
			return (int)0x800f0624;
		if (ci[2] == 0u && ci[3] == 0u)
			break;
	}

	if (curOff == totalCtrlSize)
		return (int)0x800f0602;
	return (int)0x800f0624;
}

/* 0x8045dc (name ours) */
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr,
		uint32_t targetType)
{
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0u;
	uint32_t curOff;

	if (cfGetOptHeaders(ctx, &optHdrBase, &optHdrSize) != 0)
		return (int)0x800f0602;

	curOff = 0u;
	while (curOff < optHdrSize) {
		const uint32_t *oh = (const uint32_t *)(optHdrBase + curOff);
		uint32_t ohType = oh[0];
		uint32_t nextOff;

		if (ohType == targetType) {
			*outExtHdr = optHdrBase + curOff;
			return 0;
		}
		if (ohType == 2u) {
			nextOff = curOff + 0x110u;
		} else if (ohType == 1u || ohType == 3u) {
			nextOff = curOff + 0x30u;
		} else {
			return (int)0x800f0624;
		}

		if (nextOff < curOff || nextOff > optHdrSize)
			return (int)0x800f0624;
		curOff = nextOff;
		if (oh[2] == 0u && oh[3] == 0u)
			break;
	}

	if (curOff == optHdrSize)
		return (int)0x800f0602;
	return (int)0x800f0624;
}

/* 0x804682 (name ours) */
int selfExtractKblParam(const SelfAuthContext *ctx,
		SceSkAuthorityInfo *outAuth, uint32_t flag62)
{
	const uint8_t *appInfo = NULL;
	const uint8_t *extHdr = NULL;
	const uint8_t *ctrlInfo7 = NULL;
	uint8_t devCap1[32];
	uint8_t devCap3[32];
	uint64_t authId;
	int isDevSelf;
	const uint8_t *capSrc;

	memset(devCap1, 0, sizeof(devCap1));
	memset(devCap3, 0, sizeof(devCap3));

	if (selfGetAppInfo(ctx, &appInfo) != 0)
		return (int)0x800f0616;

	authId = *(const uint64_t *)appInfo;
	isDevSelf = ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0);
	outAuth->authId = authId;

	if (isDevSelf) {
		const uint8_t *cap3Template;

		if (authId == 0x2f00000000000001ULL) {
			memcpy(devCap1, s_devCap1Auth1, 32u);
			cap3Template = (flag62 == 0u) ?
				       s_devCap3Auth1NoFlag : s_devCap3Auth1WithFlag;
		} else if (authId == 0x2f00000000000002ULL) {
			memcpy(devCap1, s_devCap1Auth2, 32u);
			cap3Template = (flag62 == 0u) ?
				       s_devCap3Auth2NoFlag : s_devCap3Auth2WithFlag;
		} else {
			return (int)0x800f0624;
		}
		memcpy(devCap3, cap3Template, 32u);
	}

	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0) {
		capSrc = devCap1;
	} else {
		if (selfFindExtHeader(ctx, &extHdr, 1u) != 0)
			return (int)0x800f0616;
		capSrc = extHdr + 0x10u;
	}
	memcpy(outAuth->cap1, capSrc, 32u);

	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0) {
		capSrc = devCap3;
	} else {
		if (selfFindExtHeader(ctx, &extHdr, 3u) != 0)
			return (int)0x800f0616;
		capSrc = extHdr + 0x10u;
	}
	memcpy(outAuth->cap3, capSrc, 32u);

	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0 ||
	    selfFindControlInfo(ctx, &ctrlInfo7, 7u) != 0) {
		memset(outAuth->sharedSecret0, 0, 16u);
	} else {
		memcpy(outAuth->sharedSecret0, ctrlInfo7 + 0x10u, 16u);
	}
	return 0;
}

/* 0x8047f4 (name ours) */
int selfExtractSelfAuthInfo(const SelfAuthContext *ctx, SceSkSelfAuthInfo *outInfo)
{
	const uint8_t *appInfo = NULL;
	const uint8_t *ctrlInfo = NULL;

	outInfo->size = 0x80u;
	outInfo->version = 1u;

	if (selfGetAppInfo(ctx, &appInfo) != 0)
		return (int)0x800f0616;

	outInfo->programVersion = *(const uint64_t *)(appInfo + 0x10u);

	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0) {
		uint32_t socVer;

		memset(outInfo->digest, 0, 32u);
		socVer = (uint32_t)REG8(0xe00621c0u) |
			 ((uint32_t)REG8(0xe00621c1u) << 8) |
			 ((uint32_t)REG8(0xe00621c2u) << 16) |
			 ((uint32_t)REG8(0xe00621c3u) << 24);
		outInfo->requiredSysVersion = ((uint64_t)(socVer >> 16) << 32) |
					      (uint64_t)(socVer << 16);
	} else {
		if (selfFindControlInfo(ctx, &ctrlInfo, 4u) != 0)
			return (int)0x800f0616;
		memcpy(outInfo->digest, ctrlInfo + 0x24u, 32u);
		outInfo->requiredSysVersion = *(const uint64_t *)(ctrlInfo + 0x48u);
	}

	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0 ||
	    selfFindControlInfo(ctx, &ctrlInfo, 7u) != 0) {
		memset(outInfo->sharedSecret[0], 0, 16u);
		memset(outInfo->sharedSecret[1], 0, 16u);
		memset(outInfo->sharedSecret[2], 0, 16u);
		memset(outInfo->sharedSecret[3], 0, 16u);
	} else {
		memcpy(outInfo->sharedSecret[0], ctrlInfo + 0x10u, 16u);
		memcpy(outInfo->sharedSecret[1], ctrlInfo + 0x20u, 16u);
		memcpy(outInfo->sharedSecret[2], ctrlInfo + 0x30u, 16u);
		memcpy(outInfo->sharedSecret[3], ctrlInfo + 0x40u, 16u);
	}
	return 0;
}

/* 0x80495a (name ours) */
int selfExtractAttrFlags(const SelfAuthContext *ctx, uint32_t *inOutFlags)
{
	const Elf32_Ehdr *ehdr = NULL;
	int status;

	status = selfGetElfHeader(ctx, (const void **)&ehdr);
	if (status == 0) {
		uint32_t flags = (ehdr->e_type == ET_SCE_FIXEDEXEC) ? 0x20u : 0u;

		if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0)
			flags |= 0x10u;
		*inOutFlags = (*inOutFlags & ~0xf0u) | flags;
	}
	return status;
}

/* 0x8049aa (name ours; Sony method self_loader_wrapper::get_all_self_auth_info) */
int selfLoaderWrapperGetAllSelfAuthInfo(const SelfAuthContext *ctx,
		SceSkAuthorityInfo *outKblParam, uint32_t flag62,
		SceSkSelfAuthInfo *outSelfInfo, uint32_t *outAttrFlags)
{
	if (selfExtractKblParam(ctx, outKblParam, flag62) == 0 &&
	    selfExtractSelfAuthInfo(ctx, outSelfInfo) == 0 &&
	    selfExtractAttrFlags(ctx, outAttrFlags) == 0) {
		return 0;
	}
	return (int)0x800f0616;
}

/* 0x8049e6 (name ours) */
int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size,
		const void *key, const void *ivIn, void *ivOut, void *unusedDesc)
{
	uint32_t bigmacChain;
	uint32_t bigmacDir;
	uint32_t bigmacKeySize;
	uint32_t localIv[4];
	uint32_t *ivPtr;
	BigmacContext bmCtx;
	int status;

	(void)unusedDesc;
	switch (chainSelect) {
	case 0u:
		bigmacChain = 0u;
		break;
	case 1u:
		bigmacChain = 1u;
		break;
	case 2u:
		bigmacChain = 2u;
		break;
	case 3u:
		bigmacChain = 3u;
		break;
	case 4u:
		bigmacChain = 4u;
		break;
	default:
		return (int)0x800f0616;
	}

	if (dirSelect == 0u)
		bigmacDir = 2u;
	else if (dirSelect == 1u)
		bigmacDir = 1u;
	else
		return (int)0x800f0616;

	if (keyBitsSelect == 1u)
		bigmacKeySize = 2u;
	else if (keyBitsSelect == 0u)
		bigmacKeySize = 1u;
	else if (keyBitsSelect == 2u)
		bigmacKeySize = 3u;
	else
		return (int)0x800f0616;

	if ((size & 0xfu) != 0u || (((uintptr_t)src | (uintptr_t)dst) & 3u) != 0u)
		return (int)0x800f0616;

	if (bigmacChain != 0u) {
		if (ivOut != NULL && ivIn == ivOut) {
			ivPtr = (uint32_t *)ivOut;
		} else {
			memcpy(localIv, ivIn, 16u);
			ivPtr = localIv;
		}
		if (bigmacChain == 4u)
			cfBswap128(ivPtr);
	} else {
		ivPtr = NULL;
	}

	bigmacInitContext(&bmCtx);
	status = bigmacStartAesCipher(dst, src, size, key, ivPtr,
			bigmacDir, bigmacChain, bigmacKeySize, &bmCtx);
	if (status != 0)
		return status;

	status = bigmacWaitChannel(&bmCtx);
	if (status != 0)
		return status;

	if (bigmacChain != 0u && ivOut != NULL) {
		if (bigmacChain == 4u)
			cfBswap128(ivPtr);
		if (ivPtr != (uint32_t *)ivOut)
			memcpy(ivOut, ivPtr, 16u);
	}
	return status;
}

/* 0x804b2e (name ours) */
int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc)
{
	uint32_t offset = 0u;
	int status;

	(void)unusedDesc;
	if (size <= 16u) {
		memcpy(dst, src, size);
		return 0;
	}

	for (;;) {
		uint32_t chunk = (size > 0x00800000u) ? 0x00800000u : size;

		status = bigmacMemcpy((uint8_t *)dst + offset,
				(const uint8_t *)src + offset, chunk);
		if (status != 0)
			break;
		size -= chunk;
		if (size == 0u)
			break;
		offset += chunk;
	}
	return status;
}

/* 0x804b88 (name ours) */
int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen)
{
	uint8_t paddedKey[64];
	const uint8_t *keyBytes = (const uint8_t *)key;
	uint32_t i;

	if (keyLen > 64u)
		return (int)0x800f0616;

	for (i = 0u; i < keyLen; i++)
		paddedKey[i] = keyBytes[i];
	for (; i < 64u; i++)
		paddedKey[i] = 0u;

	return cfHmacSha256InitBlockKey(ctx, paddedKey, 64u);
}

/* 0x804bec (name ours) */
int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const uint32_t *pubKeyWords, const uint8_t *digest, uint32_t hashSubType)
{
	RsaPublicKeyParam rsaKey;
	uint32_t hashAlgo;

	if (hashSubType == 1u)
		hashAlgo = 4u;
	else if (hashSubType == 0u)
		hashAlgo = 2u;
	else if (hashSubType == 2u)
		hashAlgo = 8u;
	else
		return (int)0x800f0616;

	rsaKey.modulus = (const uint32_t *)(uintptr_t)pubKeyWords[0];
	rsaKey.modulusWordCount = pubKeyWords[1] >> 2;
	rsaKey.exponent = (const uint32_t *)(uintptr_t)pubKeyWords[2];
	rsaKey.exponentWordCount = pubKeyWords[3] >> 2;
	return verifyRsaPkcs1v15Blocking(workBuf256, signature256, &rsaKey, digest, hashAlgo);
}
