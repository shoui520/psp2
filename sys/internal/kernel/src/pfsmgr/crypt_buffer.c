/*
 * Crypt buffers: for each kind of device (game card, memory card, eMMC) a
 * pool of 8 blocks of 0x8000 bytes, mapped twice (plain and for the crypto
 * hardware), handed out as runs of blocks with an icv for each.
 */

#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <iofilemgr/iofilemgr.h>
#include "pfsmgr_internal.h"

#define PFS_CRYPT_BUF_POOLS		3
#define PFS_CRYPT_BUF_BLOCKS		8
#define PFS_CRYPT_BUF_BLOCK_SIZE	0x8000
#define PFS_CRYPT_BUF_ALL_FREE		0xff

typedef struct ScePfsCryptBufPool {		/* name unknown */
	SceUID block;
	SceUID sharedBlock;			/* "...ForShared": the same memory */
	void *base;
	void *sharedBase;
	SceUInt32 freeMask;			/* a bit for each free block */
} ScePfsCryptBufPool;

typedef struct ScePfsCryptBufDevice {		/* name unknown */
	SceUInt32 pool;
	SceUInt32 weight;			/* the I/O weight of the device's threads */
} ScePfsCryptBufDevice;

static const char *const g_pfsCryptBufNames[PFS_CRYPT_BUF_POOLS] = {
	"ScePfsCryptBufVC",
	"ScePfsCryptBufRM",
	"ScePfsCryptBufEMMC",
};

static const char *const g_pfsCryptBufCondNames[PFS_CRYPT_BUF_POOLS] = {
	"ScePfsCryptBufCondVarVC",
	"ScePfsCryptBufCondVarRM",
	"ScePfsCryptBufCondVarEMMC",
};

static const char *const g_pfsCryptBufMutexNames[PFS_CRYPT_BUF_POOLS] = {
	"ScePfsCryptBufMutexVC",
	"ScePfsCryptBufMutexRM",
	"ScePfsCryptBufMutexEMMC",
};

static const char *const g_pfsCryptBufSharedNames[PFS_CRYPT_BUF_POOLS] = {
	"ScePfsCryptBufVCForShared",
	"ScePfsCryptBufRMForShared",
	"ScePfsCryptBufEMMCForShared",
};

static const ScePfsCryptBufDevice g_pfsCryptBufDevices[PFS_CRYPT_BUF_POOLS] = {
	{ 0, 0x201 },				/* game card */
	{ 1, 0x202 },				/* memory card */
	{ 2, 0x100 },				/* eMMC */
};

static ScePfsHash g_pfsCryptBufIcvs[PFS_CRYPT_BUF_POOLS][PFS_CRYPT_BUF_BLOCKS];
static SceUID g_pfsCryptBufMutex[PFS_CRYPT_BUF_POOLS];
static ScePfsCryptBufPool g_pfsCryptBufPools[PFS_CRYPT_BUF_POOLS];
static SceUID g_pfsCryptBufCond[PFS_CRYPT_BUF_POOLS];

int pfsCryptBufferInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	ScePfsCryptBufPool *pool;
	int i, ret;

	for (i = 0; i < PFS_CRYPT_BUF_POOLS; i++) {
		pool = &g_pfsCryptBufPools[i];
		pool->sharedBase = NULL;
		pool->freeMask = 0;
		g_pfsCryptBufCond[i] = -1;
		pool->base = NULL;
		g_pfsCryptBufMutex[i] = -1;
		pool->block = -1;
		pool->sharedBlock = -1;
		ret = sceKernelCreateMutex(g_pfsCryptBufMutexNames[i], 0, 0, NULL);
		if (ret < 0)
			goto fail;
		g_pfsCryptBufMutex[i] = ret;
		ret = sceKernelCreateCond(g_pfsCryptBufCondNames[i], 0, ret, NULL);
		if (ret < 0)
			goto fail;
		g_pfsCryptBufCond[i] = ret;

		memset(&opt, 0, sizeof(opt));
		opt.alignment = 0x1000;
		opt.size = sizeof(opt);
		ret = SceSysmemForDriver_C94850C9(g_pfsCryptBufNames[i], 0x1020d006,
					      PFS_CRYPT_BUF_BLOCKS * PFS_CRYPT_BUF_BLOCK_SIZE, &opt);
		if (ret < 0)
			goto fail;
		pool->block = ret;
		ret = SceSysmemForDriver_A841EDDA(ret, &pool->base);
		if (ret != 0)
			goto fail;

		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x1000040;
		opt.mirror_blockid = pool->block;
		ret = SceSysmemForDriver_C94850C9(g_pfsCryptBufSharedNames[i], 0x10208006, 0, &opt);
		if (ret < 0)
			goto fail;
		pool->sharedBlock = ret;
		ret = SceSysmemForDriver_A841EDDA(ret, &pool->sharedBase);
		if (ret != 0)
			goto fail;

		memset(g_pfsCryptBufIcvs[i], 0, sizeof(g_pfsCryptBufIcvs[i]));
		pool->freeMask = PFS_CRYPT_BUF_ALL_FREE;
		SceIofilemgrForDriver_C722DF35(g_pfsCryptBufDevices[i].weight | 0x10000);
	}
	return ret;

fail:
	for (; i >= 0; i--) {
		pool = &g_pfsCryptBufPools[i];
		if (pool->sharedBlock != -1) {
			SceSysmemForDriver_009E1C61(pool->sharedBlock);
			pool->sharedBlock = -1;
			pool->sharedBase = NULL;
		}
		if (pool->block != -1) {
			SceSysmemForDriver_009E1C61(pool->block);
			pool->block = -1;
			pool->base = NULL;
		}
		if (g_pfsCryptBufCond[i] != -1) {
			sceKernelDeleteCond(g_pfsCryptBufCond[i]);
			g_pfsCryptBufCond[i] = -1;
		}
		if (g_pfsCryptBufMutex[i] != -1) {
			sceKernelDeleteMutex(g_pfsCryptBufMutex[i]);
			g_pfsCryptBufMutex[i] = -1;
		}
	}
	return ret;
}

void pfsCryptBufferTerm(void)
{
	int i;

	for (i = 0; i < PFS_CRYPT_BUF_POOLS; i++) {
		sceKernelDeleteCond(g_pfsCryptBufCond[i]);
		sceKernelDeleteMutex(g_pfsCryptBufMutex[i]);
		SceSysmemForDriver_009E1C61(g_pfsCryptBufPools[i].block);
		SceSysmemForDriver_009E1C61(g_pfsCryptBufPools[i].sharedBlock);
	}
}

static inline int pfsCryptBufPool(SceInt32 weight)	/* name unknown */
{
	int i;

	if (weight == 0x201)
		i = 0;
	else if (weight == 0x202)
		i = 1;
	else if (weight == 0x100)
		i = 2;
	else
		return -1;
	return g_pfsCryptBufDevices[i].pool;
}

/* the longest run of free blocks */
static inline int pfsCryptBufFreeRun(SceUInt32 freeMask)	/* name unknown */
{
	int run = 0, longest = 0, i;

	for (i = 0; i < PFS_CRYPT_BUF_BLOCKS; i++) {
		if ((freeMask & (1 << i)) == (1 << i)) {
			run++;
		} else {
			if (run > longest)
				longest = run;
			run = 0;
		}
	}
	if (run > longest)
		longest = run;
	return longest;
}

static inline SceUInt32 pfsCryptBufMask(SceUInt32 count)	/* name unknown */
{
	SceUInt32 mask = 1, i;

	for (i = 0; i < count - 1; i++)
		mask |= mask << 1;
	return mask;
}

int pfsCryptBufferAlloc(SceInt32 weight, SceInt32 priority, SceUInt32 count, void **plain,
			void **disk, ScePfsHash **icvs, int mode)
{
	ScePfsCryptBufPool *pool;
	SceUInt32 p, mask, shifted, freeMask, shift;
	int run, ret;

	if (plain == NULL || disk == NULL || count - 1 >= PFS_CRYPT_BUF_BLOCKS || icvs == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	p = pfsCryptBufPool(weight);
	if (p >= PFS_CRYPT_BUF_POOLS)
		return SCE_ERROR_ERRNO_EINVAL;
	pool = &g_pfsCryptBufPools[p];
	ret = sceKernelLockMutex(g_pfsCryptBufMutex[p], 1, NULL);
	if (ret != 0)
		return ret;
	for (;;) {
		run = pfsCryptBufFreeRun(pool->freeMask);
		if (run < 0) {
			ret = run;
			goto unlock;
		}
		if (run < count)
			SceIofilemgrForDriver_3675ECB9(weight | 0x10000);
		if (mode != PFS_CRYPT_BUF_WAIT)
			break;
		if (run >= count)
			goto take;
		ret = sceKernelWaitCond(g_pfsCryptBufCond[p], NULL);
		if (ret != 0)
			goto unlock;
	}
	if (mode == PFS_CRYPT_BUF_PARTIAL) {
		if (run < count)
			count = run;
	} else if (mode != PFS_CRYPT_BUF_EXACT) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto unlock;
	} else if (run < count) {
		ret = SCE_ERROR_ERRNO_ENOENT;
		goto unlock;
	}

take:
	mask = pfsCryptBufMask(count);
	freeMask = pool->freeMask;
	shift = 0;
	shifted = mask;
	if ((mask & freeMask) != mask) {
		do {
			shift++;
			shifted = mask << shift;
			if (shift == PFS_CRYPT_BUF_BLOCKS + 1 - count) {
				ret = SCE_ERROR_ERRNO_ENOENT;
				*plain = NULL;
				*disk = NULL;
				*icvs = NULL;
				goto unlock;
			}
		} while ((shifted & freeMask) != shifted);
	}
	pool->freeMask = freeMask & ~shifted;
	*plain = (char *)pool->base + shift * PFS_CRYPT_BUF_BLOCK_SIZE;
	*disk = (char *)pool->sharedBase + shift * PFS_CRYPT_BUF_BLOCK_SIZE;
	*icvs = &g_pfsCryptBufIcvs[p][shift];
	ret = 0;
unlock:
	sceKernelUnlockMutex(g_pfsCryptBufMutex[p], 1);
	return ret;
}

int pfsCryptBufferFree(SceInt32 weight, SceInt32 priority, SceUInt32 count, void *plain,
		       void *disk, ScePfsHash *icvs)
{
	ScePfsCryptBufPool *pool;
	SceUInt32 p, shift, mask;
	int ret;

	if (plain == NULL || disk == NULL || icvs == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	p = pfsCryptBufPool(weight);
	if (p >= PFS_CRYPT_BUF_POOLS)
		return SCE_ERROR_ERRNO_EINVAL;
	pool = &g_pfsCryptBufPools[p];
	ret = sceKernelLockMutex(g_pfsCryptBufMutex[p], 1, NULL);
	if (ret != 0)
		return ret;
	shift = ((char *)plain - (char *)pool->base) / PFS_CRYPT_BUF_BLOCK_SIZE;
	if (shift + count <= PFS_CRYPT_BUF_BLOCKS && count <= PFS_CRYPT_BUF_BLOCKS) {
		mask = pfsCryptBufMask(count) << shift;
		if ((mask & ~pool->freeMask) == mask) {
			pool->freeMask |= mask;
			SceIofilemgrForDriver_C722DF35(weight | 0x10000);
			ret = sceKernelSignalCond(g_pfsCryptBufCond[p]);
			if (ret != 0)
				return ret;
			ret = 0;
		} else {
			ret = SCE_ERROR_ERRNO_ENOENT;
		}
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceKernelUnlockMutex(g_pfsCryptBufMutex[p], 1);
	return ret;
}

SceUInt32 pfsCryptBufferMaxBlocks(SceInt32 weight, SceInt32 priority)
{
	return PFS_CRYPT_BUF_BLOCKS;
}
