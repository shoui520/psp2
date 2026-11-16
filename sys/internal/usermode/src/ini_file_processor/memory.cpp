/*
 * SceIniFileProcessor: the allocator. A processor takes its memory from the application's functions, or
 * from a pool the library makes from a memory block (SceIniFileMemBlock) when it is first asked for memory.
 */

#include "ini_internal.h"

extern "C" void *ScePafStdc_FC5CD359(SceSize size);	/* allocate */
extern "C" void ScePafStdc_1B77082E(void *ptr);		/* free */

int g_poolsCreated;
int g_poolsDestroyed;

static void *iniPoolAlloc(SceSize size, void *context);
static void iniPoolFree(void *ptr, void *context);

/* the pool: its functions, and no memory until the first allocation */
struct IniPoolObject : IniPool {
	IniPoolObject()
	{
		alloc = iniPoolAlloc;
		free = iniPoolFree;
		memBlock = 0;
		mspace = NULL;
		size = 20480;
	}
	~IniPoolObject()
	{
		if (mspace != NULL) {
			sceClibMspaceDestroy(mspace);
			if (memBlock != 0)
				sceKernelFreeMemBlock(memBlock);
			++g_poolsDestroyed;
		}
	}
};

static void *iniPoolAlloc(SceSize size, void *context)
{
	IniPool *pool = (IniPool *)context;

	if (pool->mspace == NULL) {
		SceSize blockSize = ((pool->size >> 12) + 1) << 12;
		void *base = NULL;

		pool->memBlock = sceKernelAllocMemBlock("SceIniFileMemBlock", 0xC20D060, blockSize, NULL);
		if (pool->memBlock < 0 || sceKernelGetMemBlockBase(pool->memBlock, &base) < 0)
			return NULL;
		pool->mspace = sceClibMspaceCreate(base, blockSize);
		if (pool->mspace == NULL)
			return NULL;
		++g_poolsCreated;
	}
	return sceClibMspaceMalloc(pool->mspace, size);
}

static void iniPoolFree(void *ptr, void *context)
{
	IniPool *pool = (IniPool *)context;

	if (pool->mspace != NULL)
		sceClibMspaceFree(pool->mspace, ptr);
}

/* makes the pool's memory at once */
__attribute__((noinline)) static int iniPoolCreate(IniPool *pool, SceSize size)
{
	SceSize blockSize = ((size >> 12) + 1) << 12;
	void *base = NULL;

	pool->memBlock = sceKernelAllocMemBlock("SceIniFileMemBlock", 0xC20D060, blockSize, NULL);
	if (pool->memBlock < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	if (sceKernelGetMemBlockBase(pool->memBlock, &base) < 0)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	pool->mspace = sceClibMspaceCreate(base, blockSize);
	if (pool->mspace == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	++g_poolsCreated;
	return 0;
}

IniAllocatorRef *iniAllocatorRefInit(IniAllocatorRef *ref)
{
	ref->owned = false;
	ref->context = NULL;
	ref->ops = NULL;
	return ref;
}

/* gives back what the reference owns */
IniAllocatorRef *iniAllocatorRefRelease(IniAllocatorRef *ref)
{
	if (ref->ops != NULL) {
		if (ref->owned) {
			IniPool *pool = (IniPool *)ref->ops;

			if (pool->mspace != NULL) {
				sceClibMspaceDestroy(pool->mspace);
				if (pool->memBlock != 0)
					sceKernelFreeMemBlock(pool->memBlock);
				++g_poolsDestroyed;
			}
		}
		ScePafStdc_1B77082E(ref->ops);
	}
	ref->owned = false;
	ref->ops = NULL;
	return ref;
}

/* takes the application's allocator: a copy of its two functions, with its context */
int iniAllocatorRefSet(IniAllocatorRef *ref, const IniConfig *config)
{
	IniAllocatorOps *copy;

	if (config == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (ref->ops != NULL) {
		iniAllocatorRefRelease(ref);
	}
	copy = (IniAllocatorOps *)ScePafStdc_FC5CD359(sizeof(*copy));
	if (copy != NULL) {
		IniAllocatorOps *source = config->ops;

		copy->alloc = source->alloc;
		copy->free = source->free;
	}
	ref->ops = copy;
	if (copy == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	ref->context = config->context;
	return 0;
}

/* makes the reference its own pool, of the size given (20480 bytes when 0) */
int iniAllocatorRefCreatePool(IniAllocatorRef *ref, SceSize size)
{
	IniPool *pool;

	if (size > 0x6400000)
		return SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT;
	if (ref->ops != NULL)
		iniAllocatorRefRelease(ref);
	pool = (IniPool *)ScePafStdc_FC5CD359(sizeof(*pool));
	if (pool == NULL)
		return SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY;
	pool->alloc = iniPoolAlloc;
	pool->free = iniPoolFree;
	pool->memBlock = 0;
	pool->mspace = NULL;
	pool->size = 20480;
	if (size == 0)
		size = 20480;
	iniPoolCreate(pool, size);
	ref->ops = pool;
	ref->context = pool;
	ref->owned = true;
	return 0;
}

void *iniAlloc(IniAllocatorRef *ref, SceSize size)
{
	if (ref->ops != NULL)
		return ref->ops->alloc(size, ref->context);
	return NULL;
}

void iniFree(IniAllocatorRef *ref, void *ptr)
{
	if (ref->ops != NULL)
		ref->ops->free(ptr, ref->context);
}

/* the library's own pool, made when it is first needed and ended with the module */
IniAllocatorOps *iniDefaultAllocator(void)
{
	static IniPoolObject pool;

	return &pool;
}
