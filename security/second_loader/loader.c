/*
 * loader.c: SLB2 boot module loading, SELF segment address validation,
 * kernel_boot_loader.self authentication, and asynchronous boot module staging
 * (0x801162..0x801b37).
 *
 * File names in SLB2 ("kernel_boot_loader.self", "secure_kernel.enp",
 * "context_auth_sm.self", "kprx_auth_sm.self", "prog_rvk.srvk") are stored
 * bitwise-inverted (~ch) in .rodata and deobfuscated by partitionStorageOpen.
 * Function and variable names here are ours (descriptive).
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bd1c..0x80bd87)
 * ============================================================================ */

/* Bitwise-inverted (~ch) "kernel_boot_loader.self" */
static const uint8_t s_invKernelBootLoaderSelf[24] = {
	0x94u, 0x9au, 0x8du, 0x91u, 0x9au, 0x93u, 0xa0u, 0x9du,
	0x90u, 0x90u, 0x8bu, 0xa0u, 0x93u, 0x90u, 0x9eu, 0x9bu,
	0x9au, 0x8du, 0xd1u, 0x8cu, 0x9au, 0x93u, 0x99u, 0x00u,
};

static const uint32_t s_selfHeaderScratchSize = 0x00001000u;

/* Bitwise-inverted (~ch) "secure_kernel.enp" */
static const uint8_t s_invSecureKernelEnp[20] = {
	0x8cu, 0x9au, 0x9cu, 0x8au, 0x8du, 0x9au, 0xa0u, 0x94u,
	0x9au, 0x8du, 0x91u, 0x9au, 0x93u, 0xd1u, 0x9au, 0x91u,
	0x8fu, 0x00u, 0x00u, 0x00u,
};

/* Bitwise-inverted (~ch) "context_auth_sm.self" */
static const uint8_t s_invContextAuthSmSelf[24] = {
	0x9cu, 0x90u, 0x91u, 0x8bu, 0x9au, 0x87u, 0x8bu, 0xa0u,
	0x9eu, 0x8au, 0x8bu, 0x97u, 0xa0u, 0x8cu, 0x92u, 0xd1u,
	0x8cu, 0x9au, 0x93u, 0x99u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* Bitwise-inverted (~ch) "kprx_auth_sm.self" */
static const uint8_t s_invKprxAuthSmSelf[20] = {
	0x94u, 0x8fu, 0x8du, 0x87u, 0xa0u, 0x9eu, 0x8au, 0x8bu,
	0x97u, 0xa0u, 0x8cu, 0x92u, 0xd1u, 0x8cu, 0x9au, 0x93u,
	0x99u, 0x00u, 0x00u, 0x00u,
};

/* Bitwise-inverted (~ch) "prog_rvk.srvk" */
static const uint8_t s_invProgRvkSrvk[16] = {
	0x8fu, 0x8du, 0x90u, 0x98u, 0xa0u, 0x8du, 0x89u, 0x94u,
	0xd1u, 0x8cu, 0x8du, 0x89u, 0x94u, 0x00u, 0x00u, 0x00u,
};

/* ============================================================================
 * .data (0x80c6a0..0x80c873)
 * ============================================================================ */

PartitionStorageOps g_partitionStorageOps = {
	partitionStorageOpen,
	partitionStorageClose,
	partitionStorageLseek,
	partitionStorageRead,
	partitionStorageGetFileSize,
	{ 0u, 0u, 0u }
};

/* KBL SELF AES/HMAC key material at 0x80c6c0 (cleared after use) */
static uint8_t s_kblSelfKeyMaterial[64] = {
	0x9du, 0x3fu, 0x28u, 0xdeu, 0x30u, 0xdeu, 0xd1u, 0xd5u,
	0x03u, 0xdbu, 0x6fu, 0xa7u, 0x62u, 0xa5u, 0x71u, 0xc4u,
	0x22u, 0xa8u, 0x8du, 0x0fu, 0x36u, 0x18u, 0x99u, 0xefu,
	0x36u, 0xd3u, 0x57u, 0x05u, 0x9cu, 0x72u, 0xecu, 0x43u,
	0x30u, 0xe4u, 0x3cu, 0xfbu, 0x57u, 0xd4u, 0x18u, 0xa5u,
	0xa0u, 0xd3u, 0x2au, 0x99u, 0x39u, 0xd2u, 0x35u, 0x01u,
};

/* KBL SELF RSA-2048 modulus at 0x80c700 */
static uint8_t s_kblSelfRsaModulus[256] = {
	0xc8u, 0x19u, 0x51u, 0x72u, 0xcbu, 0x70u, 0xb0u, 0x6au,
	0x29u, 0xfau, 0xefu, 0x05u, 0x92u, 0x51u, 0x75u, 0x4bu,
	0x87u, 0xdeu, 0xe2u, 0x10u, 0xa6u, 0xc3u, 0x5cu, 0xbau,
	0x53u, 0x67u, 0xd5u, 0x96u, 0x5bu, 0xadu, 0x4au, 0x43u,
	0x06u, 0xf7u, 0xe5u, 0xb1u, 0xcbu, 0x2bu, 0x13u, 0x5cu,
	0xb2u, 0x40u, 0x34u, 0x9cu, 0xabu, 0x66u, 0xb5u, 0xf2u,
	0x20u, 0x8fu, 0x3fu, 0xbbu, 0xfbu, 0x3bu, 0x2du, 0x01u,
	0xe0u, 0x05u, 0x9bu, 0xf6u, 0x4cu, 0x4du, 0xe8u, 0x94u,
	0xf7u, 0x16u, 0xf8u, 0xabu, 0x18u, 0x05u, 0x7fu, 0x52u,
	0xacu, 0xe4u, 0x59u, 0x40u, 0xa7u, 0xd6u, 0x76u, 0x04u,
	0x80u, 0xb4u, 0xe9u, 0xe9u, 0x93u, 0xe5u, 0x01u, 0x0au,
	0xfdu, 0x54u, 0x92u, 0x02u, 0x8bu, 0xbcu, 0xa6u, 0x63u,
	0xd3u, 0xe5u, 0x90u, 0x32u, 0xadu, 0x52u, 0xd0u, 0x71u,
	0x2cu, 0xdfu, 0x16u, 0x92u, 0x74u, 0x79u, 0x83u, 0x23u,
	0x52u, 0x85u, 0x5fu, 0x46u, 0x88u, 0x42u, 0x01u, 0x57u,
	0xd0u, 0x2bu, 0x28u, 0xdbu, 0x22u, 0xe7u, 0x60u, 0xcdu,
	0x81u, 0xdeu, 0xaau, 0x80u, 0x2bu, 0xe2u, 0xd1u, 0xe5u,
	0xa3u, 0x3fu, 0xb5u, 0x1fu, 0x6bu, 0x37u, 0x13u, 0xe8u,
	0xf5u, 0x24u, 0xf3u, 0x4cu, 0xfeu, 0xabu, 0x50u, 0xc5u,
	0x59u, 0x33u, 0x66u, 0x65u, 0x7cu, 0x3cu, 0x70u, 0x90u,
	0x75u, 0x16u, 0xb0u, 0x22u, 0x7eu, 0x93u, 0x67u, 0x2fu,
	0xb4u, 0xd5u, 0xe3u, 0xa3u, 0xd6u, 0x4au, 0x07u, 0x43u,
	0x6bu, 0x5au, 0x6cu, 0xc5u, 0x14u, 0xf2u, 0xd7u, 0x6fu,
	0x3au, 0xc5u, 0xbdu, 0xf3u, 0x8du, 0xc0u, 0x8du, 0x6au,
	0x00u, 0x73u, 0x36u, 0xa8u, 0x86u, 0x90u, 0xefu, 0xf5u,
	0xbcu, 0xa9u, 0x8du, 0xcbu, 0x13u, 0xb5u, 0x2eu, 0x2bu,
	0x71u, 0x3cu, 0xbfu, 0xecu, 0xdau, 0x98u, 0x25u, 0xaau,
	0xd6u, 0xbbu, 0xe4u, 0x1eu, 0xe4u, 0xbbu, 0x1au, 0x5au,
	0xaau, 0x05u, 0x27u, 0x41u, 0xe7u, 0x15u, 0x86u, 0xcbu,
	0xf5u, 0xbau, 0xb7u, 0x67u, 0x0fu, 0x99u, 0x7bu, 0x0du,
	0x23u, 0xe4u, 0x3cu, 0x08u, 0xc6u, 0xa1u, 0x31u, 0x3eu,
	0x6au, 0x3eu, 0x52u, 0xa5u, 0xe7u, 0x14u, 0xd0u, 0x1bu,
};

static uint32_t s_kblSelfRsaMeta[16] = {
	0x00000100u, 0x01000100u, 0x00000004u, 0u,
	0u, 0u, 0u, 0u,
	0u, 0u, 0u, 0u,
	0u, 0u, 0u, 0u,
};

static uint32_t s_bootModuleLoadAddrs[4] = { 0u, 0u, 0u, 0u };
static uint32_t s_bootModuleLoadSizes[4] = { 0u, 0u, 0u, 0u };
static const uint8_t *s_bootModuleFileNames[4] = { NULL, NULL, NULL, NULL };
static uint32_t s_bootModulesFlags = 0u;

/* ============================================================================
 * Functions (0x801162..0x801b37)
 * ============================================================================ */

/* 0x801162 */
int loadKernelBootLoaderSelf(uint32_t *outMinRequiredVer)
{
	return loadAndAuthenticateSelfFromStorage(&g_partitionStorageOps, 1u,
			s_invKernelBootLoaderSelf, outMinRequiredVer);
}

/* 0x801172 */
int isKernelBootLoaderDebugMode(void)
{
	return 0;
}

/* 0x801176 */
int validateSelfLoadRange(uintptr_t startAddr, uintptr_t endAddr)
{
	uint32_t dramSize;

	if (endAddr < startAddr)
		return (int)0x800f001b;

	dramSize = ddrcGetTotalDramSize();
	if (startAddr < 0x40000000u) {
		if (startAddr < 0x1f000000u)
			return (int)0x800f0022;
		if (endAddr > 0x1f008000u &&
		    (startAddr < 0x1f840000u || endAddr > 0x1f860000u)) {
			return (int)0x800f0022;
		}
	} else if (endAddr > 0x40000000u + dramSize) {
		if (endAddr > 0x1f008000u &&
		    (startAddr < 0x1f840000u || endAddr > 0x1f860000u)) {
			return (int)0x800f0022;
		}
	}

	/* Ensure the segment does not overlap reserved boot/loader regions */
	if (startAddr < 0x40020000u) {
		if (endAddr > 0x40000200u)
			return (int)0x800f0038;
		if (startAddr <= 0x1f0002ffu) {
			if (endAddr > 0x1f000100u)
				return (int)0x800f0038;
			if (startAddr <= 0x0005ffffu) {
				if (endAddr > 0x00040000u)
					return (int)0x800f0038;
				goto check_cry2sram;
			}
		}
	}
	if (startAddr < 0x00820000u) {
check_cry2sram:
		if (endAddr > 0x00800000u)
			return (int)0x800f0038;
		if (startAddr < 0x00400000u) {
			if (endAddr > 0x00300000u)
				return (int)0x800f0038;
			goto check_mmio;
		}
	}
	if (startAddr > 0xdfffffffu)
		return (int)0x800f0038;

check_mmio:
	if (endAddr >= 0xe0000000u)
		return (int)0x800f0038;
	return 0;
}

/* 0x8012a0: segment read callback invoked by selfDecryptAndLoadSegment (0x80ab5a) */
static int readSelfSegmentCallback(const uint64_t *fileOffset, uint32_t readSize,
		uint64_t *outDstAddr, uint64_t *outSrcAddr, uint32_t *outBytesRead,
		uint32_t *outFlags, void *arg)
{
	SelfSegmentLoadContext *ctx = (SelfSegmentLoadContext *)arg;
	uint64_t reqOffset = *fileOffset;
	uint64_t segOffset = ctx->segFileOffset;
	uint64_t segFilesz = ctx->segFileSize;
	uintptr_t dstAddr;
	int status;
	int bytesRead;

	(void)s_kblSelfRsaMeta;
	if (reqOffset < segOffset)
		return (int)0x800f0016;
	if (segFilesz < readSize)
		return (int)0x800f0016;
	if (reqOffset - segOffset > segFilesz - readSize)
		return (int)0x800f0016;
	if (reqOffset - segOffset > (uint64_t)(ctx->segEndAddr - ctx->segStartAddr))
		return (int)0x800f0016;

	if (ctx->ops->lseek(ctx->devHandle, (int64_t)reqOffset, 0) < 0)
		return (int)0x800f0016;

	dstAddr = ctx->segStartAddr + (uintptr_t)(reqOffset - segOffset);
	status = validateSelfLoadRange(dstAddr, dstAddr + readSize);
	if (status != 0)
		return status;

	bytesRead = ctx->ops->read(ctx->devHandle, (void *)dstAddr, readSize);
	if (bytesRead < 1 || (uint32_t)bytesRead > readSize)
		return (int)0x800f0005;

	*outDstAddr = (uint64_t)dstAddr;
	*outSrcAddr = (uint64_t)dstAddr;
	*outBytesRead = (uint32_t)bytesRead;
	*outFlags = 0u;
	return 0;
}

/* 0x8013bc: header read callback invoked by selfInitAndVerifyHeader (0x80a672) */
static int readSelfHeaderCallback(uintptr_t dstBuf, const uint64_t *fileOffset,
		uint32_t totalSize, uint32_t *outBytesRead, void *arg)
{
	SelfHeaderLoadContext *ctx = (SelfHeaderLoadContext *)arg;
	uintptr_t maxScratchEnd;
	uint32_t done = 0;

	if (ctx->bounceBufAddr < 0x1f840000u ||
	    ctx->bounceBufAddr + ctx->bounceBufSize > 0x1f860000u) {
		return (int)0x800f0022;
	}
	if (ctx->ops->lseek(ctx->devHandle, (int64_t)*fileOffset, 0) < 0)
		return (int)0x800f0016;

	maxScratchEnd = 0x0081e800u + s_selfHeaderScratchSize;
	while (totalSize != 0) {
		uint32_t chunk = (totalSize > ctx->bounceBufSize) ? ctx->bounceBufSize : totalSize;
		int n = ctx->ops->read(ctx->devHandle, (void *)ctx->bounceBufAddr, chunk);
		uintptr_t curDst;

		if (n < 1 || (uint32_t)n > chunk)
			return (int)0x800f0005;
		curDst = dstBuf + done;
		if (curDst < 0x0081e800u || curDst + (uint32_t)n > maxScratchEnd)
			return (int)0x800f0022;
		bigmacMemcpy((void *)curDst, (const void *)ctx->bounceBufAddr, (uint32_t)n);
		done += (uint32_t)n;
		totalSize -= (uint32_t)n;
	}
	*outBytesRead = done;
	return 0;
}

/* 0x8014ae */
int loadAndAuthenticateSelfFromStorage(const PartitionStorageOps *ops, uint32_t devIndex,
		const uint8_t *invFileName, uint32_t *outMinRequiredVer)
{
	uint8_t rsaKeyBuf[272];
	SelfAuthContext selfCtx;
	SelfHeaderLoadContext hdrLoadCtx;
	SelfSegmentLoadContext segLoadCtx;
	uint64_t maxHeaderReadSize;
	uint64_t fileSize = 0;
	const SceElf32Ehdr *ehdr;
	const SceElf32Phdr *phdrs;
	const SceSelfSegmentInfo *segInfos;
	const uint8_t *authInfo;
	const uint8_t *appInfo;
	volatile uint32_t *canaryPtr;
	uint32_t canaryVal;
	int handle;
	int status;
	uint32_t i;

	if (devIndex > 1u)
		return (int)0x800f0006;

	handle = ops->open((int)devIndex, (const char *)invFileName, 0);
	if (handle < 0)
		return (int)0x800f0002;

	canaryVal = BIGMAC->ch[0].rng;
	canaryPtr = (volatile uint32_t *)(uintptr_t)(0x0081e800u + s_selfHeaderScratchSize);
	*canaryPtr = canaryVal;

	if (ops->getFileSize(handle, &fileSize) != 0) {
		status = (int)0x800f0029;
		goto cleanup;
	}

	selfCtx.scratchBuf = (void *)0x0081e800u;
	selfCtx.scratchSize = 0x1000u;
	slMemcpy(rsaKeyBuf, s_kblSelfRsaModulus, 256u);
	slMemcpy(rsaKeyBuf + 256u, &s_kblSelfRsaMeta[1], 4u);

	hdrLoadCtx.ops = ops;
	hdrLoadCtx.devHandle = handle;
	hdrLoadCtx.bounceBufAddr = 0x1f840000u;
	hdrLoadCtx.bounceBufSize = 0x1000u;
	maxHeaderReadSize = 0x1000u;

	status = selfInitAndVerifyHeader(&selfCtx, s_kblSelfKeyMaterial, rsaKeyBuf,
			5u, 0u, 1u, readSelfHeaderCallback, &hdrLoadCtx,
			0x600u, &maxHeaderReadSize);
	slMemset(s_kblSelfKeyMaterial, 0, sizeof(s_kblSelfKeyMaterial));
	if (status != 0)
		goto cleanup;

	status = selfVerifySvnAndDecryptMetadata(&selfCtx);
	if (status != 0)
		goto cleanup;

	ehdr = NULL;
	if (selfGetElfHeader(&selfCtx, &ehdr) != 0 || ehdr->e_type != 2u) {
		status = (int)0x800f0024;
		goto cleanup;
	}

	authInfo = NULL;
	status = selfFindExtHeader(&selfCtx, &authInfo, 3u);
	if (status != 0)
		goto cleanup;

	/* Verify capability bits in SceSelfAuthInfo (+0x10 from ext header start) */
	if (((int8_t)authInfo[0x28] < 0) ||
	    (authInfo[0x28] & 0x40u) != 0 ||
	    (authInfo[0x28] & 0x30u) != 0 ||
	    ((int8_t)authInfo[0x2c] >= 0) ||
	    (authInfo[0x2c] & 0x40u) == 0 ||
	    (authInfo[0x2c] & 0x20u) == 0 ||
	    (authInfo[0x2c] & 0x10u) == 0) {
		status = (int)0x800f0030;
		goto cleanup;
	}

	if (devIndex == 0u) {
		if ((authInfo[0x18] & 0x40u) == 0) {
			status = (int)0x800f0032;
			goto cleanup;
		}
	} else {
		if ((int8_t)authInfo[0x1a] >= 0) {
			status = (int)0x800f0032;
			goto cleanup;
		}
	}

	if ((authInfo[0x14] & 0x40u) != 0 && !isSnvsSd0FlagEnabled()) {
		status = (int)0x800f0033;
		goto cleanup;
	}
	if ((authInfo[0x14] & 0x08u) != 0 && !isSpecialSelfAuthAllowed()) {
		status = (int)0x800f0035;
		goto cleanup;
	}

	status = selfGetElfHeader(&selfCtx, &ehdr);
	if (status != 0)
		goto cleanup;
	status = selfGetProgramHeaders(&selfCtx, &phdrs);
	if (status != 0)
		goto cleanup;
	status = selfGetSegmentInfos(&selfCtx, &segInfos);
	if (status != 0)
		goto cleanup;

	for (i = 0; (i & 0xffffu) < ehdr->e_phnum; i++) {
		uint32_t idx = i & 0xffffu;
		const SceElf32Phdr *phdr = &phdrs[idx];
		uintptr_t loadAddr;

		if (phdr->p_type != 1u)
			continue;

		loadAddr = phdr->p_paddr;
		if (loadAddr < 0x8000u && phdr->p_filesz < 0x8000u &&
		    loadAddr + phdr->p_filesz < 0x8000u) {
			loadAddr += 0x1f000000u;
		}
		if (phdr->p_filesz == 0u) {
			status = (int)0x800f0021;
			break;
		}
		status = validateSelfLoadRange(loadAddr, loadAddr + phdr->p_filesz);
		if (status != 0)
			break;
		if (phdr->p_align > 1u &&
		    (((phdr->p_align & (phdr->p_align - 1u)) != 0) ||
		     (((phdr->p_offset ^ phdr->p_paddr) & (phdr->p_align - 1u)) != 0))) {
			status = (int)0x800f0016;
			break;
		}

		segLoadCtx.ops = ops;
		segLoadCtx.devHandle = handle;
		segLoadCtx.segStartAddr = loadAddr;
		segLoadCtx.segEndAddr = loadAddr + phdr->p_filesz;
		segLoadCtx.segFileOffset = segInfos[idx].offset;
		segLoadCtx.segFileSize = segInfos[idx].length;

		status = selfDecryptAndLoadSegment(&selfCtx, 2u, idx,
				readSelfSegmentCallback, &segLoadCtx, NULL, 0);
		if (status != 0)
			break;
	}

	if ((i & 0xffffu) == ehdr->e_phnum) {
		appInfo = NULL;
		status = selfGetAppInfo(&selfCtx, &appInfo);
		if (status == 0) {
			*outMinRequiredVer = ((uint32_t)appInfo[0x15] << 24) |
					     ((uint32_t)appInfo[0x14] << 16) |
					     ((uint32_t)appInfo[0x13] << 8) |
					     (uint32_t)appInfo[0x12];
		}
	}

cleanup:
	ops->close(handle);
	if (*canaryPtr != canaryVal)
		status = (int)0x800f0007;
	bigmacMemset((void *)0x0081e800u, 0x1000u, 0);
	bigmacMemset((void *)0x1f840000u, 0x1000u, 0);
	return status;
}

/* 0x801888 */
int asyncLoadBootModulesCallback(void)
{
	uint32_t modSize = 0;
	int status;
	uint32_t curDramAddr;
	unsigned int idx;

	status = loadRawBootModuleFromStorage(&g_partitionStorageOps, 1u,
			getBootModuleFileName(0), 0x1f850000u, 0x1f860000u, &modSize);
	if (status != 0)
		return status;

	setBootModuleLoadAddr(0, 0x1f850000u);
	setBootModuleLoadSize(0, modSize);

	curDramAddr = 0x40000500u;
	for (idx = 1u; idx != 4u; idx = (idx + 1u) & 0xffu) {
		int err = loadRawBootModuleFromStorage(&g_partitionStorageOps, 1u,
				getBootModuleFileName(idx), curDramAddr,
				0x4001fd00u - curDramAddr, &modSize);
		if (err == 0) {
			setBootModuleLoadAddr(idx, curDramAddr);
			setBootModuleLoadSize(idx, modSize);
			(void)getBootModuleFileName(idx);
			curDramAddr += (modSize + 0x1ffu) & ~0x1ffu;
		}
	}
	return status;
}

/* 0x80193e */
void initBootModulesTable(void)
{
	s_bootModulesFlags = 0u;
	slMemset(s_bootModuleLoadAddrs, 0, sizeof(s_bootModuleLoadAddrs));
	slMemset(s_bootModuleLoadSizes, 0, sizeof(s_bootModuleLoadSizes));
	s_bootModuleFileNames[0] = s_invSecureKernelEnp;
	s_bootModuleFileNames[1] = s_invContextAuthSmSelf;
	s_bootModuleFileNames[2] = s_invKprxAuthSmSelf;
	s_bootModuleFileNames[3] = s_invProgRvkSrvk;
}

/* 0x801988 */
uint32_t getBootModuleLoadAddr(unsigned int index)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		return s_bootModuleLoadAddrs[idx];
	return 0u;
}

/* 0x8019a0 */
uint32_t getBootModuleLoadSize(unsigned int index)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		return s_bootModuleLoadSizes[idx];
	return 0u;
}

/* 0x8019b8 */
const uint8_t *getBootModuleFileName(unsigned int index)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		return s_bootModuleFileNames[idx];
	return NULL;
}

/* 0x8019d0 */
void setBootModuleLoadAddr(unsigned int index, uint32_t addr)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		s_bootModuleLoadAddrs[idx] = addr;
}

/* 0x8019e4 */
void setBootModuleLoadSize(unsigned int index, uint32_t size)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		s_bootModuleLoadSizes[idx] = size;
}

/* 0x8019f8 */
void setBootModuleFileName(unsigned int index, const uint8_t *invFileName)
{
	unsigned int idx = index & 0xffu;

	if (idx <= 3u)
		s_bootModuleFileNames[idx] = invFileName;
}

/* 0x801a0c */
uint32_t getBootModulesFlags(void)
{
	return s_bootModulesFlags;
}

/* 0x801a12 */
int loadRawBootModuleFromStorage(const PartitionStorageOps *ops, uint32_t devIndex,
		const uint8_t *invFileName, uint32_t loadAddr, uint32_t maxLen,
		uint32_t *outSize)
{
	uint64_t fileSize = 0;
	uint32_t alignedSize;
	uint32_t endAddr;
	uint32_t remaining;
	uint32_t offset;
	int handle;
	int status;

	handle = ops->open((int)devIndex, (const char *)invFileName, 0);
	if (handle < 0)
		return (int)0x800f0002;

	if (ops->getFileSize(handle, &fileSize) != 0) {
		ops->close(handle);
		return (int)0x800f0029;
	}

	alignedSize = ((uint32_t)fileSize + 0x1ffu) & ~0x1ffu;
	if (maxLen < alignedSize) {
		ops->close(handle);
		return (int)0x800f001c;
	}

	endAddr = loadAddr + alignedSize;
	if (loadAddr < 0x40000500u) {
		if (loadAddr < 0x1f840000u || endAddr > 0x1f860000u) {
			ops->close(handle);
			return (int)0x800f0022;
		}
	} else if (endAddr > 0x4001fd00u && endAddr > 0x1f860000u) {
		ops->close(handle);
		return (int)0x800f0022;
	}

	*outSize = (uint32_t)fileSize;
	offset = 0;
	remaining = (uint32_t)fileSize;
	status = 0;
	while ((int32_t)remaining > 0) {
		int n = ops->read(handle, (void *)(uintptr_t)(loadAddr + offset), remaining);

		if (n < 1) {
			status = (int)0x800f0005;
			break;
		}
		offset += (uint32_t)n;
		remaining -= (uint32_t)n;
	}
	ops->close(handle);
	return status;
}

/* 0x801b0e */
int verifyStepFHook(const uint8_t *qaFlags)
{
	(void)qaFlags;
	return 0;
}

/* 0x801b12 */
int isSpecialSelfAuthAllowed(void)
{
	return 0;
}

/* 0x801b16 */
int isBootFlagBit0Enabled(void)
{
	return 0;
}

/* 0x801b1a */
int isBootFlagBit16Enabled(void)
{
	return 0;
}

/* 0x801b1e */
int isBootFlagBit3Enabled(void)
{
	return 0;
}

/* 0x801b22 */
void clearCmepSboot(void)
{
}

/* 0x801b24 */
int verifyStepGHook(void)
{
	return 0;
}

/* 0x801b28 */
int isBootFlagBit3KblEnabled(void)
{
	return 0;
}

/* 0x801b2c */
uint32_t getTargetArmClockStep(void)
{
	return 7u;
}

/* 0x801b30 */
uint32_t getTargetCmepClockStep(void)
{
	return 6u;
}

/* 0x801b34 */
uint32_t getTargetBootModeFlag(void)
{
	return 0u;
}
